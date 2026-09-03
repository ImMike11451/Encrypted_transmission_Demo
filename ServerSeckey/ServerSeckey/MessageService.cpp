#include "MessageService.h"
#include "AesCrypto.h"
#include "Base64Util.h"
#include "Logger.h"
#include "AuditService.h"
#include "MessageRepository.h"
#include "MessageQueryService.h"
#include "AesGcmCrypto.h"
#include <ctime>
#include <atomic>
#include <chrono>

namespace
{
// 服务端消息 ID 和审计 ID 都走同一套生成规则。
// 这里先用“作用域前缀 + 微秒时间戳 + 进程内递增序号”，避免引入额外 UUID 依赖。
std::string generateScopedId(const std::string& prefix)
{
	static std::atomic<unsigned long long> sequence{ 0 };
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

	return prefix + "_" + std::to_string(micros) + "_" + std::to_string(++sequence);
}
}

MessageService::MessageService(const std::string& serverId, mysqlOP* db, SecKeyShm* shm)
	:m_serverId(serverId), m_db(db), m_shm(shm)
{
}

MessageService::~MessageService()
{
}

// 这是服务端处理“发送加密消息”的主入口。
// 当前链路是可信服务端转发模型：服务端先用 A-Server key 解密，
// 再用 B-Server key 重新加密后存储，便于 B 后续拉取。
V2SendMessageResponseInfo MessageService::handleSendMessage(const secmng::v2::RequestPacket& packet)
{

	V2SendMessageResponseInfo respInfo;

	// 第 1 步：先准备响应头
	respInfo.header = buildResponseHeader(packet, secmng::v2::CMD_SEND_MSG_RESP);

	// 先给响应体设置一个兜底失败值，
	// 这样即便后面中途 return，也能保证返回结构是完整的
	respInfo.code = secmng::v2::RESULT_FAILED;
	respInfo.message = "未知错误";
	respInfo.serverMessageId = "";
	respInfo.serverTime = static_cast<long long>(std::time(nullptr));
	respInfo.deliveryStatus = secmng::v2::DELIVERY_REJECTED;

	// 这里先创建两个轻量服务对象。
	// 第一阶段这样写改动最小，不需要额外改 MessageService 构造函数签名。
	MessageRepository msgRepo(m_db);
	AuditService auditSvc(m_db);

	// 第 2 步：校验请求字段是否合法
	std::string validateErr;
	if(!validateRequest(packet, validateErr))
	{
		respInfo.code = secmng::v2::RESULT_INVALID_REQUEST;
		respInfo.message = validateErr;

		// 请求非法也应该写审计日志，方便排查恶意请求或调用错误
		auditSvc.logAction(
			generateAuditLogId(),
			packet.has_header()?packet.header().sender_id():"",
			"MSG_SEND",
			"", // 同上，receiverId 也取不到
			0,
			"invalid request: " + validateErr,
			m_db->getCurTime()
		);

		return respInfo;
	}
	// 取出请求头和请求体，后面会频繁使用
	const secmng::v2::Header& header = packet.header();
	const secmng::v2::SendMessageRequest& req = packet.send_msg_req();
	const secmng::v2::EncryptedMessage& encMsg = req.message();

	// 第 3 步：根据 sender / receiver / keyId 查找当前可用 key
	ActiveKeyResult senderKeyResult = getActiveKey(header.sender_id(), m_serverId, encMsg.key_id());

	if (!senderKeyResult.found)
	{
		respInfo.code = secmng::v2::RESULT_KEY_NOT_FOUND;
		respInfo.message = senderKeyResult.errorMsg;

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_SEND",
			std::to_string(encMsg.key_id()),
			0,
			"key not found: " + senderKeyResult.errorMsg,
			m_db->getCurTime()
		);

		return respInfo;
	}

	if (!senderKeyResult.valid)
	{
		respInfo.code = secmng::v2::RESULT_KEY_INVALID;
		respInfo.message = senderKeyResult.errorMsg;

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_SEND",
			std::to_string(encMsg.key_id()),
			0,
			"key invalid: " + senderKeyResult.errorMsg,
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 4 步：使用取到的 key 解密消息。
	// 解密得到的明文只在内存中短暂存在，用于给接收方重加密，不写入日志。
	DecryptMessageResult decResult = decryptMessage(senderKeyResult.base64Key, encMsg.ciphertext(), encMsg.nonce(), encMsg.tag(),encMsg.algorithm());
	if (!decResult.success)
	{
		respInfo.code = secmng::v2::RESULT_DECRYPT_FAILED;
		respInfo.message = decResult.errorMsg;

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_DECRYPT",
			header.message_id(),
			0,
			"decrypt failed: " + decResult.errorMsg,
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 5 步：查接收方 B 与服务端之间的活跃密钥。
	NodeSHMInfo receiverNode = m_shm->shmRead(header.receiver_id(), m_serverId);

	// 如果共享内存里没有 B 的密钥记录，说明 B 还没有协商密钥，
	// 或者服务端共享内存中不存在这条记录。
	if (strlen(receiverNode.clientID) == 0 ||
		strlen(receiverNode.serverID) == 0)
	{
		respInfo.code = secmng::v2::RESULT_KEY_NOT_FOUND;
		respInfo.message =
			"receiver key not found, receiver must do key agreement first";

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_DELIVER",
			header.receiver_id(),
			0,
			"receiver key not found",
			m_db->getCurTime()
		);

		return respInfo;
	}

    // 接收方与发送方复用同一个生命周期入口，不能仅凭缓存状态继续使用旧密钥。
    const ActiveKeyResult receiverKeyResult =
        getActiveKey(header.receiver_id(), m_serverId, receiverNode.seckeyID);
    if (!receiverKeyResult.found || !receiverKeyResult.valid)
    {
        respInfo.code = receiverKeyResult.found ? secmng::v2::RESULT_KEY_INVALID
                                              : secmng::v2::RESULT_KEY_NOT_FOUND;
        respInfo.message = "receiver key unavailable: " + receiverKeyResult.errorMsg;

        auditSvc.logAction(
            generateAuditLogId(),
            header.sender_id(),
            "MSG_DELIVER",
            header.receiver_id(),
            0,
            "receiver key unavailable: " + receiverKeyResult.errorMsg,
            m_db->getCurTime()
        );

        return respInfo;
    }

    // 第 6 步：使用经过生命周期校验的密钥快照重加密，不再使用原始缓存中的密钥。
    EncryptMessageResult receiverEncResult = encryptMessage(
        receiverKeyResult.base64Key,
        decResult.plaintext
    );

	if (!receiverEncResult.success)
	{
		respInfo.code = secmng::v2::RESULT_FAILED;
		respInfo.message =
			"encrypt message for receiver failed: " +
			receiverEncResult.errorMsg;

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_DELIVER",
			header.receiver_id(),
			0,
			"encrypt for receiver failed: " + receiverEncResult.errorMsg,
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 7 步：生成服务端消息 ID。
	std::string serverMsgId = generateServerMessageId();

	// 第 8 步：组装 message_log 记录
	MessageLogRecord msgRecord;

	msgRecord.msgId = serverMsgId;
	msgRecord.senderId = header.sender_id();
	msgRecord.receiverId = header.receiver_id();

	// senderKeyId 表示 A-Server key。
	msgRecord.senderKeyId = encMsg.key_id();

	// receiverKeyId 表示 B-Server key。
    msgRecord.receiverKeyId = receiverKeyResult.keyId;

	msgRecord.msgType = "text";

	// sender 侧保存 A 原始发来的密文。
	// 这部分用于审计、排查和保留原始入站数据。
	msgRecord.senderCiphertext = encMsg.ciphertext();
	msgRecord.senderNonce = encMsg.nonce();
	msgRecord.senderTag = encMsg.tag();

	// receiver 侧保存服务端用 B 的密钥重新加密后的密文。
	// B 后续读取消息时，只需要这一组数据。
	msgRecord.receiverCiphertext = receiverEncResult.ciphertext;
	msgRecord.receiverNonce = receiverEncResult.nonce;
	msgRecord.receiverTag = receiverEncResult.tag;

	msgRecord.algorithm = receiverEncResult.algorithm;
	msgRecord.sendTime = m_db->getCurTime();
	msgRecord.status = 1;

	// 第 9 步：写入 message_log
	bool msgInsertRet = msgRepo.insertMessage(msgRecord);
	if (!msgInsertRet)
	{
		respInfo.code = secmng::v2::RESULT_FAILED;
		respInfo.message = "insert message_log failed";

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_STORE",
			serverMsgId,
			0,
			"insert message_log failed",
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 10 步：写成功审计日志。
	auditSvc.logAction(
		generateAuditLogId(),
		header.sender_id(),
		"MSG_SEND",
		serverMsgId,
		1,
		"message send success and re-encrypted for receiver",
		m_db->getCurTime()
	);

	// 第 11 步：组装成功响应。
	respInfo.code = secmng::v2::RESULT_SUCCESS;
	respInfo.message = "消息发送成功";
	respInfo.serverMessageId = serverMsgId;
	respInfo.serverTime = static_cast<long long>(std::time(nullptr));
	respInfo.deliveryStatus = secmng::v2::DELIVERY_ACCEPTED;

	return respInfo;
}

V2QueryMessageResponseInfo MessageService::handleQueryMessage(const secmng::v2::RequestPacket& packet)
{
    MessageRepository repository(m_db);
    AuditService audit(m_db);
    MessageQueryService queryService(m_serverId, &repository, &audit);
    return queryService.handleQueryMessage(packet);
}

V2QueryMessageListResponseInfo MessageService::handleQueryMessageList(const secmng::v2::RequestPacket& packet)
{
    MessageRepository repository(m_db);
    AuditService audit(m_db);
    MessageQueryService queryService(m_serverId, &repository, &audit);
    return queryService.handleQueryMessageList(packet);
}

// 校验请求包是否合法。
// 这一步的目标是尽早挡掉格式错误的请求，避免后面业务处理混乱。
bool MessageService::validateRequest(const secmng::v2::RequestPacket& packet, std::string& errorMsg)
{
	// 1. 必须有 header
	if (!packet.has_header())
	{
		errorMsg = "header is empty";
		return false;
	}

	const::secmng::v2::Header& header = packet.header();

	// 2. 必须有 send_msg_req
	if(!packet.has_send_msg_req())
	{
		errorMsg = "send_msg_req is empty";
		return false;
	}

	const::secmng::v2::SendMessageRequest& req = packet.send_msg_req();

	// 3. 请求体里必须有 message
	if(!req.has_message())
	{
		errorMsg = "message is empty";
		return false;
	}

	const secmng::v2::EncryptedMessage& msg = req.message();

	// 4. sender_id 不能为空
	if (header.sender_id().empty())
	{
		errorMsg = "sender_id is empty";
		return false;
	}

	// 5. receiver_id 不能为空
	if (header.receiver_id().empty())
	{
		errorMsg = "receiver_id is empty";
		return false;
	}

	// 6. key_id 必须大于 0
	if (msg.key_id() <= 0)
	{
		errorMsg = "invalid key_id";
		return false;
	}

	// 7. 密文不能为空
	if (msg.ciphertext().empty())
	{
		errorMsg = "ciphertext is empty";
		return false;
	}

	// 8. 算法字段不能为空
	if (msg.algorithm().empty())
	{
		errorMsg = "algorithm is empty";
		return false;
	}

	return true;
}

// 查找当前活跃 key。
// 当前消息解密必须拿到真实 key，因此共享内存命中是必要条件。
// 数据库是生命周期权威源，共享内存只负责缓存真实 key。
ActiveKeyResult MessageService::getActiveKey(const std::string& senderId, const std::string& receiverId, int keyId)
{
	ActiveKeyResult result;
	result.found = false;
	result.valid = false;
	result.keyId = keyId;

	// 第 1 步：先从共享内存读取
	NodeSHMInfo node = m_shm->shmRead(senderId, receiverId);

	// 如果共享内存命中，并且 keyId/status 都匹配，仍需回源确认生命周期状态。
	if(strlen(node.clientID) != 0 && strlen(node.serverID) != 0 && node.seckeyID == keyId)
	{
		result.found = true;
		if (node.status == 1)
		{
			if (!m_db->checkSecKey(senderId, receiverId, keyId))
			{
				m_shm->shmUpdateStatus(senderId, receiverId, 0);
				result.errorMsg = "key expired, revoked or rotated";
				return result;
			}

			result.valid = true;
			result.base64Key = node.seckey;
			return result;
		}
		else
		{
			result.valid = false;
			result.errorMsg = "key exists in shm but status is invalid";
			return result;
		}
	}

	// 第 2 步：共享内存未命中时，回源数据库确认状态
	bool dbRet = m_db->checkSecKey(senderId, receiverId, keyId);
	if (!dbRet)
	{
		result.found = false;
		result.valid = false;
		result.errorMsg = "key not found or invalid in db";
		return result;
	}

	// 注意：
	// 你当前 mysqlOP::checkSecKey() 只能确认“存在且有效”，
	// 但无法把 seckey 本体查询出来。
	// 而消息解密需要真正的 key。
	//
	// 所以按照你当前项目结构，第一阶段最现实的方案是：
	// 只有共享内存里命中时，才能真正完成消息解密。
	// 数据库当前更多是做状态校验的辅助。
	result.found = true;
	result.valid = false;
	result.errorMsg = "key valid in db but missing in shm, cannot decrypt";

	return result;

}

// 使用 AES-GCM 解密消息。
// nonce 和 tag 必须来自发送方请求；tag 校验失败时说明密文、nonce、tag 或 key 任一项不匹配。
DecryptMessageResult MessageService::decryptMessage(const std::string& base64Key,
	const std::string& base64Ciphertext,
	const std::string& base64Nonce,
	const std::string& base64Tag,
	const std::string& algorithm)
{
	DecryptMessageResult result;
	result.success = false;

	if (algorithm != "AES-128-GCM")
	{
		result.errorMsg = "unsupported algorithm: " + algorithm;
		return result;
	}

	// 第 1 步：base64 解码 key
	std::string rawKey = Base64Util::decode(base64Key);
	if(rawKey.empty())
	{
		result.errorMsg = "failed to decode base64 key";
		return result;
	}

	// 第 2 步：base64 解码密文
	std::string cipherBin = Base64Util::decode(base64Ciphertext);
	if(cipherBin.empty())
	{
		result.errorMsg = "failed to decode base64 ciphertext";
		return result;
	}

	std::string nonceBin = Base64Util::decode(base64Nonce);
	if(nonceBin.empty())
	{
		result.errorMsg = "failed to decode base64 nonce";
		return result;
	}

	std::string tagBin = Base64Util::decode(base64Tag);
	if (tagBin.empty())
	{
		result.errorMsg = "base64 tag decode failed";
		return result;
	}

	AesGcmCrypto aes(rawKey);
	GcmDecryptResult gcmResult = aes.decrypt(nonceBin, cipherBin, tagBin);

	if (!gcmResult.success)
	{
		result.errorMsg = gcmResult.errorMsg;
		return result;
	}

	result.success = true;
	result.plaintext = gcmResult.plaintext;
	return result;
}

EncryptMessageResult MessageService::encryptMessage(const std::string& base64Key, const std::string& plaintext)
{
	EncryptMessageResult result;
	result.success = false;
	result.ciphertext.clear();
	result.nonce.clear();
	result.tag.clear();
	result.algorithm = "AES-128-GCM";
	result.errorMsg.clear();

	// 第 1 步：明文不能为空。
	if (plaintext.empty())
	{
		result.errorMsg = "plaintext is empty";
		return result;
	}

	// 第 2 步：base64 解码 key。
	// 共享内存里保存的是 base64 key，AesGcmCrypto 需要原始二进制 key。
	std::string rawKey = Base64Util::decode(base64Key);
	if (rawKey.empty())
	{
		result.errorMsg = "failed to decode receiver base64 key";
		return result;
	}

	// 第 3 步：使用 AES-GCM 加密明文。
	// AesGcmCrypto::encrypt 会返回：
	// ciphertext：二进制密文
	// nonce：二进制随机数
	// tag：二进制认证标签
	AesGcmCrypto aes(rawKey);
	GcmEncryptResult gcmResult = aes.encrypt(plaintext);
	if (!gcmResult.success)
	{
		result.errorMsg = gcmResult.errorMsg;
		return result;
	}

	// 第 4 步：把二进制结果转成 base64。
	// 这样可以安全放进 protobuf string 和 MySQL text/varchar 字段。
	result.ciphertext = Base64Util::encode(reinterpret_cast<const unsigned char*>(gcmResult.ciphertext.data()), gcmResult.ciphertext.size());
	result.nonce = Base64Util::encode(reinterpret_cast<const unsigned char*>(gcmResult.nonce.data()), gcmResult.nonce.size());
	result.tag = Base64Util::encode(reinterpret_cast<const unsigned char*>(gcmResult.tag.data()), gcmResult.tag.size());

	result.success = true;

	return result;
}

// 根据请求包构造响应头。
// 响应头里通常保留原 message_id，并交换 sender / receiver。
V2HeaderInfo MessageService::buildResponseHeader(const secmng::v2::RequestPacket& packet,int command)
{
	V2HeaderInfo header;

	if (packet.has_header())
	{
		const secmng::v2::Header& reqHeader = packet.header();

		// 响应沿用请求的 message_id，方便请求响应配对
		header.messageId = reqHeader.message_id();

		// 响应命令
		header.command = command;

		// 响应的 sender 变成服务端
		header.senderId = m_serverId;

		// 响应的 receiver 变成原请求发送方
		header.receiverId = reqHeader.sender_id();

		// 响应时间戳
		header.timestamp = static_cast<long long>(time(nullptr));
	}
	else
	{
		// 理论上不应该走到这里，因为 validateRequest 已经做了 header 校验。
		// 这里保留兜底逻辑，避免函数内部依赖隐式前提。
		header.messageId = generateServerMessageId();
		header.command = command;
		header.senderId = m_serverId;
		header.receiverId = "";
		header.timestamp = static_cast<long long>(time(nullptr));
	}

	return header;
}

// 生成服务端消息记录 ID。
// 第一阶段先采用简单规则：serverId + timestamp。
// 后续如果你想做得更规范，可以换成 UUID。
std::string MessageService::generateServerMessageId()
{
	return generateScopedId(m_serverId + "_msg");
}

std::string MessageService::generateAuditLogId()
{
	return generateScopedId(m_serverId + "_audit");
}
