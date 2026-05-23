#include "MessageService.h"
#include "AesCrypto.h"
#include "Base64Util.h"
#include "Logger.h"
#include "AuditService.h"
#include "MessageRepository.h"
#include "AesGcmCrypto.h"
#include <ctime>
#include <sstream>
#include <iomanip>

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

	// 第 4 步：使用取到的 key 解密消息
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

	// 如果 B 的密钥状态不是 1，说明密钥不可用。
	// 例如已经注销、过期，或者状态被服务端置为无效。
	if (receiverNode.status != 1)
	{
		respInfo.code = secmng::v2::RESULT_KEY_INVALID;
		respInfo.message = "receiver key is invalid";

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_DELIVER",
			header.receiver_id(),
			0,
			"receiver key invalid",
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 6 步：用 B-Server key 重新加密明文。
	EncryptMessageResult receiverEncResult = encryptMessage(
		receiverNode.seckey,
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
	msgRecord.receiverKeyId = receiverNode.seckeyID;

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

	// 当前调试阶段可以打印明文，便于确认流程。
	// 后续正式展示时，建议不要在服务端日志里打印明文。
	Logger::info("消息解密成功，明文内容: " + decResult.plaintext);
	Logger::info("消息已使用接收方密钥重新加密，receiver_id: " + header.receiver_id());

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
	V2QueryMessageResponseInfo respInfo;

	// 第 1 步：先准备默认失败响应
	respInfo.header = buildResponseHeader(packet, secmng::v2::CMD_QUERY_MSG_RESP);
	respInfo.code = secmng::v2::RESULT_FAILED;
	respInfo.message = "query message failed";
	respInfo.serverMessageId = "";
	respInfo.senderId = "";
	respInfo.receiverId = "";
	respInfo.keyId = 0;
	respInfo.msgType = "";
	respInfo.sendTime = 0;
	respInfo.status = 0;

	// 查询业务同样会写审计日志，所以这里先准备服务对象
	AuditService auditSvc(m_db);
	MessageRepository msgRepo(m_db);

	// 第 2 步：校验请求是否合法
	std::string validateErr;
	if (!validateQueryRequest(packet, validateErr))
	{
		respInfo.code = secmng::v2::RESULT_INVALID_REQUEST;
		respInfo.message = validateErr;

		// 非法查询请求也应留痕
		auditSvc.logAction(
			generateAuditLogId(),
			packet.has_header() ? packet.header().sender_id() : "",
			"MSG_QUERY",
			"",
			0,
			"invalid query request: " + validateErr,
			m_db->getCurTime()
		);

		return respInfo;
	}

	const secmng::v2::Header& header = packet.header();
	const secmng::v2::QueryMessageRequest& req = packet.query_msg_req();

	// 第 3 步：调用 Repository 查询消息
	MessageQueryResult queryResult;
	bool ret = msgRepo.queryMessageById(req.server_message_id(),queryResult);

	if (!ret || !queryResult.found)
	{
		respInfo.code = secmng::v2::RESULT_MSG_NOT_FOUND;
		respInfo.message = "message not found";
		respInfo.serverMessageId = req.server_message_id();

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_QUERY",
			req.server_message_id(),
			0,
			"message not found",
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 4 步：组装成功响应
	respInfo.code = secmng::v2::RESULT_SUCCESS;
	respInfo.message = "query message success";
	respInfo.serverMessageId = queryResult.msgId;
	respInfo.senderId = queryResult.senderId;
	respInfo.receiverId = queryResult.receiverId;
	respInfo.keyId = queryResult.keyId;
	respInfo.msgType = queryResult.msgType;
	respInfo.sendTime = parseDateTimeToTimestamp(queryResult.sendTime);
	respInfo.status = queryResult.status;

	// 第 5 步：写成功审计日志
	auditSvc.logAction(
		generateAuditLogId(),
		header.sender_id(),
		"MSG_QUERY",
		req.server_message_id(),
		1,
		"query message success",
		m_db->getCurTime()
	);

	return respInfo;
}

V2QueryMessageListResponseInfo MessageService::handleQueryMessageList(const secmng::v2::RequestPacket& packet)
{
	V2QueryMessageListResponseInfo respInfo;

	// 第 1 步：准备默认失败响应
	// 注意这里的命令类型是 CMD_QUERY_MSG_LIST_RESP
	respInfo.header = buildResponseHeader(packet, secmng::v2::CMD_QUERY_MSG_LIST_RESP);
	respInfo.code = secmng::v2::RESULT_FAILED;
	respInfo.message = "query message list failed";
	respInfo.messages.clear();

	AuditService auditSvc(m_db);
	MessageRepository msgRepo(m_db);

	// 第 2 步：校验请求是否合法
	std::string validateErr;
	if(!validateQueryListRequest(packet, validateErr))
	{
		respInfo.code = secmng::v2::RESULT_INVALID_REQUEST;
		respInfo.message = validateErr;

		auditSvc.logAction(
			generateAuditLogId(),
			packet.has_header() ? packet.header().sender_id() : "",
			"MSG_LIST_QUERY",
			"",
			0,
			"invalid query list request: " + validateErr,
			m_db->getCurTime()
		);

		return respInfo;
	}

	const secmng::v2::Header& header = packet.header();
	const secmng::v2::QueryMessageListRequest& req = packet.query_msg_list_req();

	// 第 3 步：调用 Repository 查询最近 N 条消息
	std::vector<MessageSummaryInfo> dbMessages;
	bool ret = msgRepo.queryRecentMessagesBySender(req.sender_id(), req.limit(), dbMessages);

	if (!ret)
	{
		respInfo.code = secmng::v2::RESULT_FAILED;
		respInfo.message = "query recent messages failed";

		auditSvc.logAction(
			generateAuditLogId(),
			header.sender_id(),
			"MSG_LIST_QUERY",
			req.sender_id(),
			0,
			"query recent messages failed",
			m_db->getCurTime()
		);

		return respInfo;
	}

	// 第 4 步：把 Repository 层结构转换成 V2 响应结构
	for (const auto& item : dbMessages)
	{
		V2MessageSummaryInfo summary;
		summary.serverMessageId = item.msgId;
		summary.senderId = item.senderId;
		summary.receiverId = item.receiverId;
		summary.keyId = item.keyId;
		summary.msgType = item.msgType;
		summary.sendTime = parseDateTimeToTimestamp(item.sendTime);
		summary.status = item.status;

		respInfo.messages.push_back(summary);
	}

	// 第 5 步：写成功审计日志
	auditSvc.logAction(
		generateAuditLogId(),
		header.sender_id(),
		"MSG_LIST_QUERY",
		req.sender_id(),
		1,
		"query message list success, count = " +
		std::to_string(respInfo.messages.size()),
		m_db->getCurTime()
	);

	// 第 6 步：组装成功响应
	respInfo.code = secmng::v2::RESULT_SUCCESS;
	respInfo.message = "query message list success";

	return respInfo;

}

bool MessageService::validateQueryRequest(const secmng::v2::RequestPacket& packet, std::string& errorMsg)
{
	// 第 1 步：必须有 header
	if(!packet.has_header())
	{
		errorMsg = "header is empty";
		return false;
	}

	// 第 2 步：必须有 query_msg_req
	if(!packet.has_query_msg_req())
	{
		errorMsg = "query_msg_req is empty";
		return false;
	}

	const secmng::v2::QueryMessageRequest& req = packet.query_msg_req();

	// 第 3 步：server_message_id 不能为空
	if(req.server_message_id().empty())
	{
		errorMsg = "server_message_id is empty";
		return false;
	}
	return true;
}

bool MessageService::validateQueryListRequest(
	const secmng::v2::RequestPacket& packet,
	std::string& errorMsg
)
{
	// 第 1 步：必须有 header
	if (!packet.has_header())
	{
		errorMsg = "header is empty";
		return false;
	}

	// 第 2 步：必须有 query_msg_list_req
	if (!packet.has_query_msg_list_req())
	{
		errorMsg = "query_msg_list_req is empty";
		return false;
	}

	const secmng::v2::Header& header = packet.header();
	const secmng::v2::QueryMessageListRequest& req =
		packet.query_msg_list_req();

	// 第 3 步：请求头 sender_id 不能为空
	// 这个 sender_id 表示“谁发起了查询”
	if (header.sender_id().empty())
	{
		errorMsg = "header sender_id is empty";
		return false;
	}

	// 第 4 步：查询条件 sender_id 不能为空
	// 这个 sender_id 表示“要查谁发送过的消息”
	if (req.sender_id().empty())
	{
		errorMsg = "query sender_id is empty";
		return false;
	}

	// 第 5 步：limit 必须合法
	if (req.limit() <= 0)
	{
		errorMsg = "limit must be greater than 0";
		return false;
	}

	// 第 6 步：限制最大查询数量，避免一次查太多
	if (req.limit() > 100)
	{
		errorMsg = "limit must not exceed 100";
		return false;
	}

	return true;
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

long long MessageService::parseDateTimeToTimestamp(const std::string& dateTimeStr)
{
	if (dateTimeStr.empty())
	{
		return 0;
	}

	std::tm tmTime = {};
	std::istringstream ss(dateTimeStr);

	// 按固定格式解析
	ss >> std::get_time(&tmTime, "%Y-%m-%d %H:%M:%S");

	// 如果解析失败，直接返回 0，表示当前时间不可用
	if (ss.fail())
	{
		Logger::error("parseDateTimeToTimestamp failed: " + dateTimeStr);
		return 0;
	}

	// mktime 会把本地时间 struct tm 转成 time_t
	std::time_t timeValue = std::mktime(&tmTime);
	if(timeValue == -1)
	{
		Logger::error("mktime failed for: " + dateTimeStr);
		return 0;
	}

	return static_cast<long long>(timeValue);
}
