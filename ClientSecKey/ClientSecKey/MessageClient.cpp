#include "MessageClient.h"
#include "Base64Util.h"
#include "Logger.h"


MessageClient::MessageClient(const ClientInfo& info, SecKeyShm* shm)
	: m_info(info), m_shm(shm)
{
}

MessageClient::~MessageClient()
{
}

// 这是客户端发送文本消息的主流程。
// 可以把它理解成“新业务的一条完整链路入口”。
bool MessageClient::sendTextMessage(const SendTextMessageInfo& msgInfo)
{
	// 第 1 步：读取本地有效密钥
	NodeSHMInfo KeyNode;
	if (!getActiveKey(KeyNode))
	{
		Logger::error("没有可用的密钥，请先完成密钥协商。");
		return false;
	}

	// 第 2 步：使用密钥加密明文
	// 注意：当前共享内存里保存的是 base64 编码后的 key，
	// 所以这里先直接把它作为字符串传给加密函数。
	// 后面加密函数内部会按需要做 base64 解码。
	EncryptTextResult encResult = encryptText(KeyNode.seckey, msgInfo.plaintext);
	if (!encResult.success)
	{
		Logger::error("文本加密失败: " + encResult.errorMsg);
		return false;
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
		Logger::error("连接服务器失败");
		return false;
	}

	ret = tcp.sendMsg(encStr, 10);
	if (ret != 0)
	{
		Logger::error("发送加密消息请求失败。");
		return false;
	}

	// 第 6 步：接收服务端响应
	std::string recvData;
	ret = tcp.recvMsg(recvData, 10);
	if (ret != 0)
	{
		Logger::error("接收服务器响应失败。");
		return false;
	}

	// 第 7 步：解码响应
	V2RespondCodec rspCodec(recvData);
	secmng::v2::ResponsePacket* rspMsg = static_cast<secmng::v2::ResponsePacket*>(rspCodec.decodeMsg());
	if (rspMsg == nullptr)
	{
		Logger::error("响应解码失败。");
		return false;
	}

	// 判断响应体里是否真的是 send_msg_resp
	if (!rspMsg->has_send_msg_resp())
	{
		Logger::error("服务端响应体类型错误。");
		return false;
	}

	const secmng::v2::SendMessageResponse& resp = rspMsg->send_msg_resp();

	// 第 8 步：根据响应结果给出反馈
	if (resp.code() == secmng::v2::RESULT_SUCCESS)
	{
		Logger::info("消息发送成功！");
		Logger::info("server message id: " + resp.server_message_id());
		return true;
	}
	else
	{
		Logger::error("消息发送失败: " + resp.message());
		return false;
	}
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
	result.tag = "";     // 第一阶段如果未使用 GCM，可先置空
	result.algorithm = "AES-128-CBC"; // 先写死一个算法标识，后面可升级

	// 第 1 步：把 base64 key 解码成原始 key
	std::string rawKey = Base64Util::decode(base64Key);
	if(rawKey.empty())
	{
		result.errorMsg = "Base64 解码密钥失败。";
		return result;
	}

	// 第 2 步：准备 IV / nonce
	// 第一阶段先简化处理：
	// 用一个固定长度的字符串充当 IV。
	// 以后升级到更现代的实现时，应改为随机生成并随消息发送。
	std::string iv = "";

	// 第 3 步：执行 AES 加密
	try 
	{
		// 这里假设你现有 AesCrypto 的构造方式和加密接口
		// 支持“key + iv”的模式。
		// 如果你现有 AesCrypto 接口不同，后面我们再按真实代码适配。
		AesCrypto aes(rawKey);

		// 将明文加密成二进制密文
		std::string cipherBin = aes.aesEncrypt(plaintext); 
		if(cipherBin.empty())
		{
			result.errorMsg = "AES 加密失败。";
			return result;
		}

		// 为了便于协议传输和日志调试，这里将二进制密文转成 base64 字符串。
		result.ciphertext = Base64Util::encode(reinterpret_cast<const unsigned char*>(cipherBin.data()), cipherBin.size());

		// IV 也一并编码后发给服务端，方便服务端按相同参数解密。
		//result.nonce = Base64Util::encode(reinterpret_cast<const unsigned char*>(iv.data()), iv.size());
		result.nonce = "";

		result.success = true;
		return result;

	}
	catch(...)
	{
		result.errorMsg = "exception occurred during aes encryption";
		return result;
	}

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
