#include "MessageClient.h"
#include "Base64Util.h"
#include "Logger.h"
#include "AesGcmCrypto.h"


MessageClient::MessageClient(const ClientInfo& info, SecKeyShm* shm)
	: m_info(info), m_shm(shm)
{
}

MessageClient::~MessageClient()
{
}

// 这是客户端发送文本消息的主流程。
// 可以把它理解成“新业务的一条完整链路入口”。
SendMessageResult  MessageClient::sendTextMessage(const SendTextMessageInfo& msgInfo)
{
	SendMessageResult sendResult;
	sendResult.success = false;
	sendResult.serverMessageId.clear();
	sendResult.errorMsg.clear();

	// 第 1 步：读取本地有效密钥
	NodeSHMInfo KeyNode;
	if (!getActiveKey(KeyNode))
	{
		sendResult.errorMsg = "没有可用的密钥，请先完成密钥协商。";
		Logger::error(sendResult.errorMsg);
		return sendResult;
	}

	// 第 2 步：使用密钥加密明文
	// 注意：当前共享内存里保存的是 base64 编码后的 key，
	// 所以这里先直接把它作为字符串传给加密函数。
	// 后面加密函数内部会按需要做 base64 解码。
	EncryptTextResult encResult = encryptText(KeyNode.seckey, msgInfo.plaintext);
	if (!encResult.success)
	{
		sendResult.errorMsg = "文本加密失败: " + encResult.errorMsg;
		Logger::error(sendResult.errorMsg);
		return sendResult;
	}

	// 第 3 步：组装 v2 请求对象
	V2SendMessageRequestInfo reqInfo;
	//构造请求头
	reqInfo.header = buildHeader(msgInfo.receiverId);

	//构造消息体
	reqInfo.message.keyId = KeyNode.seckeyID;
	reqInfo.message.msgType = secmng::v2::MSG_TYPE_TEXT;
	reqInfo.message.ciphertext = encResult.ciphertext;
	reqInfo.message.nonce = encResult.nonce;
	reqInfo.message.tag = encResult.tag;
	reqInfo.message.algorithm = encResult.algorithm;

	// 第 4 步：编码请求
	V2RequestCodec reqCodec(&reqInfo);
	std::string encStr = reqCodec.encodeMsg();

	// 第 5 步：建立连接并发送请求
	TcpSocket tcp;
	int ret = tcp.connectToHost(m_info.IP, m_info.port, 10);
	if (ret != 0)
	{
		sendResult.errorMsg = "连接服务器失败。";
		Logger::error(sendResult.errorMsg);
		return sendResult;
	}
	ret = tcp.sendMsg(encStr, 10);
	if (ret != 0)
	{
		sendResult.errorMsg = "发送加密消息请求失败。";
		Logger::error(sendResult.errorMsg);
		return sendResult;
	}

	// 第 6 步：接收服务端响应
	std::string recvData;
	ret = tcp.recvMsg(recvData, 10);
	if (ret != 0)
	{
		sendResult.errorMsg = "接收服务器响应失败。";
		Logger::error(sendResult.errorMsg);
		return sendResult;
	}

	// 第 7 步：解码响应
	V2RespondCodec rspCodec(recvData);
	secmng::v2::ResponsePacket* rspMsg = static_cast<secmng::v2::ResponsePacket*>(rspCodec.decodeMsg());
	if (rspMsg == nullptr)
	{
		sendResult.errorMsg = "响应解码失败。";
		Logger::error(sendResult.errorMsg);
		return sendResult;
	}

	// 判断响应体里是否真的是 send_msg_resp
	if (!rspMsg->has_send_msg_resp())
	{
		sendResult.errorMsg = "服务端响应体类型错误。";
		Logger::error(sendResult.errorMsg);
		return sendResult;
	}

	const secmng::v2::SendMessageResponse& resp = rspMsg->send_msg_resp();

	// 第 8 步：根据响应结果给出反馈
	if (resp.code() == secmng::v2::RESULT_SUCCESS)
	{
		sendResult.success = true;
		sendResult.serverMessageId = resp.server_message_id();

		Logger::info("消息发送成功！");
		Logger::info("server message id: " + resp.server_message_id());
		Logger::info("delivery_status: " + deliveryStatusToString(resp.delivery_status()));
		return sendResult;
	}
	else
	{
		sendResult.success = false;
		sendResult.errorMsg = resp.message();

		Logger::error("消息发送失败: " + resp.message());
		Logger::error("delivery_status: " + deliveryStatusToString(resp.delivery_status()));
		return sendResult;
	}
}

bool MessageClient::queryMessageById(const std::string& serverMessageId)
{
	// 第 1 步：先做最基本的参数校验
	if (serverMessageId.empty())
	{
		Logger::error("serverMessageId is empty.");
		return false;
	}

	// 第 2 步：组装查询请求对象
	V2QueryMessageRequestInfo reqInfo;

	reqInfo.header.messageId = generateMessageId();
	reqInfo.header.command = secmng::v2::CMD_QUERY_MSG_REQ;
	reqInfo.header.senderId = m_info.clientId;
	reqInfo.header.receiverId = m_info.serverId;
	reqInfo.header.timestamp = static_cast<long long>(time(nullptr));

	reqInfo.serverMessageId = serverMessageId;

	// 第 3 步：编码查询请求
	V2RequestCodec reqCodec(&reqInfo);
	std::string encStr = reqCodec.encodeMsg();

	// 第 4 步：建立连接并发送请求
	TcpSocket tcp;
	int ret = tcp.connectToHost(m_info.IP, m_info.port, 10);
	if (ret != 0)
	{
		Logger::error("连接服务器失败。");
		return false;
	}

	ret = tcp.sendMsg(encStr, 10);
	if (ret != 0)
	{
		Logger::error("发送查询消息请求失败。");
		return false;
	}

	// 第 5 步：接收服务端响应
	std::string recvData;
	ret = tcp.recvMsg(recvData, 10);
	if (ret != 0)
	{
		Logger::error("接收服务器查询响应失败。");
		return false;
	}

	// 第 6 步：解码响应
	V2RespondCodec rspCodec(recvData);
	secmng::v2::ResponsePacket* rspMsg =
		static_cast<secmng::v2::ResponsePacket*>(rspCodec.decodeMsg());

	if (rspMsg == nullptr)
	{
		Logger::error("查询响应解码失败。");
		return false;
	}

	// 查询请求必须收到 query_msg_resp，
	// 否则说明服务端响应体类型不匹配。
	if (!rspMsg->has_query_msg_resp())
	{
		Logger::error("服务端查询响应体类型错误。");
		return false;
	}

	const secmng::v2::QueryMessageResponse& resp = rspMsg->query_msg_resp();

	// 第 7 步：根据结果码判断查询是否成功
	if (resp.code() != secmng::v2::RESULT_SUCCESS)
	{
		Logger::error("查询消息失败: " + resp.message());
		return false;
	}

	// 第 8 步：输出查询结果
	// 当前阶段先把消息元数据打印出来，
	// 不直接返回明文，也不直接展示密文内容。
	Logger::info("查询消息成功。");
	Logger::info("server_message_id: " + resp.server_message_id());
	Logger::info("sender_id: " + resp.sender_id());
	Logger::info("receiver_id: " + resp.receiver_id());
	Logger::info("key_id: " + std::to_string(resp.key_id()));
	Logger::info("msg_type: " + resp.msg_type());
	Logger::info("status: " + std::to_string(resp.status()));
	Logger::info("send_time: " + std::to_string(resp.send_time()));

	return true;

}

bool MessageClient::queryRecentMessagesBySender(const std::string& senderId, int limit)
{
	// 第 1 步：参数校验
	if (senderId.empty())
	{
		Logger::error("senderId is empty.");
		return false;
	}

	if (limit <= 0)
	{
		Logger::error("limit must be greater than 0.");
		return false;
	}

	if (limit > 100)
	{
		Logger::error("limit must not exceed 100.");
		return false;
	}

	// 第 2 步：组装列表查询请求
	V2QueryMessageListRequestInfo reqInfo;
	reqInfo.header.messageId = generateMessageId();
	reqInfo.header.command = secmng::v2::CMD_QUERY_MSG_LIST_REQ;
	reqInfo.header.senderId = m_info.clientId;
	reqInfo.header.receiverId = m_info.serverId;
	reqInfo.header.timestamp = static_cast<long long>(time(nullptr));

	reqInfo.senderId = senderId;
	reqInfo.limit = limit;

	// 第 3 步：编码请求
	V2RequestCodec reqCodec(&reqInfo);
	std::string encStr = reqCodec.encodeMsg();

	// 第 4 步：连接服务端
	TcpSocket tcp;
	int ret = tcp.connectToHost(m_info.IP, m_info.port, 10);
	if (ret != 0)
	{
		Logger::error("连接服务器失败。");
		return false;
	}

	// 第 5 步：发送请求
	ret = tcp.sendMsg(encStr, 10);
	if (ret != 0)
	{
		Logger::error("发送查询消息列表请求失败。");
		return false;
	}

	// 第 6 步：接收响应
	std::string recvData;
	ret = tcp.recvMsg(recvData, 10);
	if (ret != 0)
	{
		Logger::error("接收服务器查询列表响应失败。");
		return false;
	}

	// 第 7 步：解码响应
	V2RespondCodec rspCodec(recvData);
	secmng::v2::ResponsePacket* rspMsg =
		static_cast<secmng::v2::ResponsePacket*>(rspCodec.decodeMsg());

	if (rspMsg == nullptr)
	{
		Logger::error("查询列表响应解码失败。");
		return false;
	}

	if (!rspMsg->has_query_msg_list_resp())
	{
		Logger::error("服务端查询列表响应体类型错误。");
		return false;
	}

	const secmng::v2::QueryMessageListResponse& resp =
		rspMsg->query_msg_list_resp();

	// 第 8 步：判断服务端处理结果
	if (resp.code() != secmng::v2::RESULT_SUCCESS)
	{
		Logger::error("查询消息列表失败: " + resp.message());
		return false;
	}

	// 第 9 步：打印消息列表
	Logger::info("查询消息列表成功。");
	Logger::info("message count: " + std::to_string(resp.messages_size()));

	for (int i = 0; i < resp.messages_size(); ++i)
	{
		const secmng::v2::MessageSummary& item = resp.messages(i);

		Logger::info("----------------------------------------");
		Logger::info("server_message_id: " + item.server_message_id());
		Logger::info("sender_id: " + item.sender_id());
		Logger::info("receiver_id: " + item.receiver_id());
		Logger::info("key_id: " + std::to_string(item.key_id()));
		Logger::info("msg_type: " + item.msg_type());
		Logger::info("send_time: " + std::to_string(item.send_time()));
		Logger::info("status: " + std::to_string(item.status()));
	}

	return true;
}

// 从共享内存中读取当前客户端与服务端之间的有效密钥。
bool MessageClient::getActiveKey(NodeSHMInfo& keyInfo)
{
	keyInfo = m_shm->shmRead(m_info.clientId, m_info.serverId);

	if (strlen(keyInfo.clientID) == 0 || strlen(keyInfo.serverID) == 0)
	{
		Logger::error("共享内存中没有找到密钥记录。");
		return false;
	}

	if(keyInfo.status != 1)
	{
		Logger::error("共享内存中的密钥记录不可用。");
		return false;
	}

	return true;
}

// 使用共享密钥对文本进行加密。
// 第一阶段这里先采用“兼容现有工程”的思路：
// 1. 共享内存中的 key 是 base64 字符串
// 2. 先 base64 解码得到原始二进制 key
// 3. 再调用现有 AesCrypto 完成加密
EncryptTextResult MessageClient::encryptText(const std::string& base64Key, const std::string& plaintext)
{
	EncryptTextResult result;
	result.success = false; // 默认失败，后续成功时再改为 true

	// 第 1 步：把 base64 key 解码成原始 key
	std::string rawKey = Base64Util::decode(base64Key);
	if(rawKey.empty())
	{
		result.errorMsg = "Base64 解码密钥失败。";
		return result;
	}

	// 第 2 步：使用 AesGcmCrypto 执行 AES-GCM 加密
	AesGcmCrypto aes(rawKey);
	GcmEncryptResult gcmResult = aes.encrypt(plaintext);
	if (!gcmResult.success)
	{
		result.errorMsg = gcmResult.errorMsg;
		return result;
	}

	// 第 3 步：把二进制结果转成 base64 字符串，便于协议传输
	// 因为 protobuf 里我们当前定义的是 string 字段，
	// 为了避免直接传输不可见二进制带来的调试困难，
	// 这里统一转成 base64。
	result.ciphertext = Base64Util::encode(
		reinterpret_cast<const unsigned char*>(gcmResult.ciphertext.data()), 
		gcmResult.ciphertext.size()
	);
	result.nonce = Base64Util::encode(
		reinterpret_cast<const unsigned char*>(gcmResult.nonce.data()),
		gcmResult.nonce.size()
	);

	result.tag = Base64Util::encode(
		reinterpret_cast<const unsigned char*>(gcmResult.tag.data()),
		gcmResult.tag.size()
	);

	// 第 4 步：标记当前使用的算法
	result.algorithm = "AES-128-GCM";

	result.success = true;
	return result;

}

// 构造请求头。
// 这一层把“公共头字段的生成规则”集中起来，
// 避免 sendTextMessage() 里塞太多细节。
V2HeaderInfo MessageClient::buildHeader(const std::string& receiverId)
{
	V2HeaderInfo header;
	header.messageId = generateMessageId();
	header.command = secmng::v2::CMD_SEND_MSG_REQ;
	header.senderId = m_info.clientId;
	header.receiverId = receiverId;
	header.timestamp = static_cast<long long>(time(nullptr));

	return header;
}

// 生成消息 ID。
// 第一阶段先采用简单方案：
// senderId + 当前时间戳
// 后面如果你想更规范，可以换成 UUID。
std::string MessageClient::generateMessageId()
{
	std::stringstream ss;
	ss << m_info.clientId << "_" << static_cast<long long>(time(nullptr));
	return ss.str();
}

std::string MessageClient::deliveryStatusToString(int status)
{
	switch (status)
	{
	case secmng::v2::DELIVERY_ACCEPTED:
		return "ACCEPTED";
	case secmng::v2::DELIVERY_REJECTED:
		return "REJECTED";
	default:
		return "UNKNOWN";
	}
}

