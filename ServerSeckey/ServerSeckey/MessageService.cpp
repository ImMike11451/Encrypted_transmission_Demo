#include "MessageService.h"
#include "AesCrypto.h"
#include "Base64Util.h"
#include "Logger.h"
#include "AuditService.h"
#include "MessageRepository.h"
#include <ctime>
#include <sstream>

MessageService::MessageService(const std::string& serverId, mysqlOP* db, SecKeyShm* shm)
	:m_serverId(serverId), m_db(db), m_shm(shm)
{
}

MessageService::~MessageService()
{
}

// 这是服务端处理“发送加密消息”请求的主入口。
// 你可以把它理解成服务端新业务链的核心函数。
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
	ActiveKeyResult keyResult = getActiveKey(header.sender_id(), header.receiver_id(), encMsg.key_id());

	if (!keyResult.found)
	{
		respInfo.code = secmng::v2::RESULT_KEY_NOT_FOUND;
		respInfo.message = keyResult.errorMsg;

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_SEND",
			std::to_string(encMsg.key_id()),
			0,
			"key not found: " + keyResult.errorMsg,
			m_db->getCurTime()
		);

		return respInfo;
	}

	if (!keyResult.valid)
	{
		respInfo.code = secmng::v2::RESULT_KEY_INVALID;
		respInfo.message = keyResult.errorMsg;

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_SEND",
			std::to_string(encMsg.key_id()),
			0,
			"key invalid: " + keyResult.errorMsg,
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 4 步：使用取到的 key 解密消息
	DecryptMessageResult decResult = decryptMessage(keyResult.base64Key, encMsg.ciphertext(), encMsg.algorithm());
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

	// 第 5 步：生成服务端消息 ID
	std::string serverMsgId = generateServerMessageId();

	// 第 6 步：写 message_log
	MessageLogRecord msgRecord;
	msgRecord.msgId = serverMsgId;
	msgRecord.senderId = header.sender_id();
	msgRecord.receiverId = header.receiver_id();
	msgRecord.keyId = encMsg.key_id();
	msgRecord.msgType = "text";
	msgRecord.ciphertext = encMsg.ciphertext();
	msgRecord.nonce = encMsg.nonce();
	msgRecord.tag = encMsg.tag();
	msgRecord.sendTime = m_db->getCurTime();
	msgRecord.status = 1;

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

	// 第 7 步：写成功审计日志
	auditSvc.logAction(
		generateAuditLogId(),
		header.sender_id(),
		"MSG_SEND",
		serverMsgId,
		1,
		"message send success",
		m_db->getCurTime()
	);

	Logger::info("消息解密成功，明文内容: " + decResult.plaintext);

	// 第 8 步：组装成功响应
	respInfo.code = secmng::v2::RESULT_SUCCESS;
	respInfo.message = "消息发送成功";
	respInfo.serverMessageId = generateServerMessageId();
	respInfo.serverTime = static_cast<long long>(std::time(nullptr));

	return respInfo;
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
// 第一阶段优先复用你现有的共享内存 + 数据库校验逻辑。
ActiveKeyResult MessageService::getActiveKey(const std::string& senderId, const std::string& receiverId, int keyId)
{
	ActiveKeyResult result;
	result.found = false;
	result.valid = false;
	result.keyId = keyId;

	// 第 1 步：先从共享内存读取
	NodeSHMInfo node = m_shm->shmRead(senderId, receiverId);

	// 如果共享内存命中，并且 keyId/status 都匹配，
	if(strlen(node.clientID) != 0 && node.serverID != 0 && node.seckeyID == keyId)
	{
		result.found = true;
		if (node.status == 1)
		{
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

// 使用当前项目已有的 AesCrypto 解密消息。
// 当前 AesCrypto 特点：
// 1. 只接收 key，不接收外部 IV
// 2. IV 由 key 内部派生
// 所以这里暂时不使用 nonce 参数来参与解密
DecryptMessageResult MessageService::decryptMessage(const std::string& base64Key, const std::string& base64Ciphertext, const std::string& algorithm)
{
	DecryptMessageResult result;
	result.success = false;

	if (algorithm != "AES-128-CBC")
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

	// 第 3 步：使用现有 AesCrypto 解密
	try
	{
		AesCrypto aes(rawKey);
		std::string plaintext = aes.aesDecrypt(cipherBin);

		result.success = true;
		result.plaintext = plaintext;
		return result;
	}
	catch (...)
	{
		result.errorMsg = "exception occurred during aes decrypt";
		return result;
	}

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
	std::stringstream ss;
	ss << m_serverId << "_" << static_cast<long long>(time(nullptr));
	return ss.str();
}

std::string MessageService::generateAuditLogId()
{
	std::stringstream ss;
	ss << m_serverId << "_audit_" << static_cast<long long>(time(nullptr));
	return ss.str();
}
