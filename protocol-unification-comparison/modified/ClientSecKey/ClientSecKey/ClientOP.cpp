#include "ClientOP.h"
#include "RsaCrypto.h"
#include "TcpSocket.h"
#include "MessageClient.h"
#include "Hash.h"
#include "Base64Util.h"
#include "Config.h"
#include "Logger.h"
#include "V2RequestCodec.h"
#include "V2RespondCodec.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <cstring>
#include <ctime>

namespace
{
V2HeaderInfo buildKeyHeader(const ClientInfo& info, int command)
{
	V2HeaderInfo header;
	header.messageId = info.clientId + "_" + std::to_string(static_cast<long long>(time(nullptr)));
	header.command = command;
	header.senderId = info.clientId;
	header.receiverId = info.serverId;
	header.timestamp = static_cast<long long>(time(nullptr));
	return header;
}
}


ClientOP::ClientOP(std::string fileName)
{
	//解析json文件，读文件-->解析json文件
	Config config;
	if(!config.load(fileName))
	{
		Logger::error("加载配置文件失败！请检查文件路径和内容格式是否正确！");
		exit(1);
	}
	//将root中的数据成员取出
	m_info.serverId = config.getString("ServerID");
	m_info.clientId = config.getString("ClientID");
	m_info.IP = config.getString("ServerIP");
	m_info.port = config.getInt("Port");

	//实例化共享内存
	//从配置文件读取path/name   客户端一个秘钥
	std::string shmKey  = config.getString("ShmKey");
	int maxNode = config.getInt("ShmMaxNode");
	m_shm = make_unique<SecKeyShm>(shmKey, maxNode);

}


ClientOP::~ClientOP() = default;


bool ClientOP::keyAgreement()
{
	//生成秘钥对，将公钥读取
	RsaCrypto rsa{};
	rsa.generateKeyPair("pub.pem", "pri.pem",2048);

	//读取公钥文件
	ifstream ifs("pub.pem");
	if(!ifs.is_open())
	{
		Logger::error("读取公钥文件失败！请检查文件路径和内容格式是否正确！");
		return false;
	}

	stringstream str;
	str << ifs.rdbuf();

	//创建哈希对象
	Hash sha1(T_SHA256);
	sha1.addData(str.str());

	V2KeyAgreementRequestInfo reqInfo;
	reqInfo.header = buildKeyHeader(m_info, secmng::v2::CMD_KEY_AGREE_REQ);
	reqInfo.publicKey = str.str();
	reqInfo.sign = rsa.rsaSign(sha1.result());//公钥的哈希运算后的签名

	Logger::info("签名完成....");

	V2RequestCodec reqCodec(&reqInfo);
	string encstr = reqCodec.encodeMsg();

	//套接字通信，连接服务器，发送数据，接收数据
	TcpSocket tcp;
	//连接服务器
	int ret = tcp.connectToHost(m_info.IP, m_info.port,10);
	if (ret != 0)
	{
		Logger::error("连接服务器失败.....");
		return false;
	}
	//发送序列化数据
	ret = tcp.sendMsg(encstr, 10);
	if (ret != 0)
	{
		Logger::error("发送请求失败");
		return false;
	}
	std::string recvData;
	//接收数据
	ret = tcp.recvMsg(recvData,10);
	if(ret != 0)
	{
		Logger::error("接收数据失败.....");
		return false;
	}

	V2RespondCodec rspCodec(recvData);
	secmng::v2::ResponsePacket* resData = static_cast<secmng::v2::ResponsePacket*>(rspCodec.decodeMsg());
	if (resData == nullptr || !resData->has_key_op_resp())
	{
		Logger::error("密钥协商响应格式错误！");
		return false;
	}

	const secmng::v2::KeyOperationResponse& keyResp = resData->key_op_resp();
	//判断状态
	if (keyResp.code() != secmng::v2::RESULT_SUCCESS)
	{
		Logger::error("秘钥协商失败！");
		Logger::error("server msg: " + keyResp.message());
		return false;
	}

	Logger::info("秘钥协商成功！");

	//将得到的密文解密
	std::string key = rsa.rsaPrivateDecrypt(keyResp.data());

	//base64编码后存储
	std::string base64Key = Base64Util::encode((const unsigned char*)key.data(), key.size());
	Logger::info("协商得到的对称加密的密钥:" + base64Key);

	////秘钥写入共享内存中
	//NodeSHMInfo info;
	//strcpy(info.clientID, m_info.clientId.data());
	//strcpy(info.serverID, m_info.serverId.data());
	//strcpy(info.seckey, base64Key.data());
	//info.seckeyID = resData->seckeyid();
	//info.status = 1;

	//m_shm->shmWrite(&info);

	//return true;
	// 秘钥写入共享内存中
	NodeSHMInfo info{};
	memset(&info, 0, sizeof(NodeSHMInfo));

	strncpy(info.clientID, m_info.clientId.c_str(), sizeof(info.clientID) - 1);
	strncpy(info.serverID, m_info.serverId.c_str(), sizeof(info.serverID) - 1);
	strncpy(info.seckey, base64Key.c_str(), sizeof(info.seckey) - 1);

	info.seckeyID = keyResp.key_id();
	info.status = 1;

	m_shm->shmWrite(&info);

	// 写完立刻回读，确认是否真的写进去了
	NodeSHMInfo checkInfo = m_shm->shmRead(m_info.clientId, m_info.serverId);

	if (strlen(checkInfo.clientID) == 0 || strlen(checkInfo.serverID) == 0)
	{
		Logger::error("密钥协商成功，但写入共享内存后回读失败！");
		Logger::error("请检查 ShmKey 路径是否存在、ftok 是否成功、ShmMaxNode 是否太小、shmWrite 是否真的写入。");
		return false;
	}

	Logger::info("密钥已成功写入本地共享内存。");
	Logger::info("clientID: " + std::string(checkInfo.clientID));
	Logger::info("serverID: " + std::string(checkInfo.serverID));
	Logger::info("seckeyID: " + std::to_string(checkInfo.seckeyID));

	return true;
}

void ClientOP::keyVerification()
{
	//从共享内存中读取秘钥信息
	NodeSHMInfo readInfo =  m_shm->shmRead(m_info.clientId, m_info.serverId);
	//cout << "从共享内存中读取的秘钥信息: " << endl;
	if (strlen(readInfo.clientID) == 0 || strlen(readInfo.serverID) == 0)
	{
		Logger::error("本地没有密钥记录！");
		return;
	}
	if (readInfo.status == 0)
	{
		Logger::error("本地密钥已失效！");
		return;
	}

	//Logger::info("SecKeyID: " + std::to_string(readInfo.seckeyID));

	V2KeyCheckRequestInfo reqInfo;
	reqInfo.header = buildKeyHeader(m_info, secmng::v2::CMD_KEY_CHECK_REQ);
	reqInfo.keyId = readInfo.seckeyID;

	V2RequestCodec reqCodec(&reqInfo);
	std::string resStr = reqCodec.encodeMsg();
	
	//套接字通信，连接服务器，发送数据，接收数据
	TcpSocket tcp;
	int ret = tcp.connectToHost(m_info.IP, m_info.port, 10);
	if (ret != 0)
	{
		Logger::error("连接服务器失败");
		return;
	}
	//发送数据
	ret = tcp.sendMsg(resStr, 10);
	if(ret != 0)
	{
		Logger::error("发送校验请求失败");
		return;
	}

	//接收数据
	std::string recvData;
	ret = tcp.recvMsg(recvData, 10);
	if(ret != 0)
	{
		Logger::error("接收校验响应失败");
		return;
	}

	V2RespondCodec rspCodec(recvData);
	secmng::v2::ResponsePacket* resData = static_cast<secmng::v2::ResponsePacket*>(rspCodec.decodeMsg());
	if (resData == nullptr || !resData->has_key_op_resp())
	{
		Logger::error("密钥校验响应格式错误。");
		return;
	}

	const secmng::v2::KeyOperationResponse& keyResp = resData->key_op_resp();

	if (keyResp.code() == secmng::v2::RESULT_SUCCESS)
	{
		Logger::info("密钥校验成功，当前密钥有效。");
		Logger::info("server msg: " + keyResp.message());
	}
	else
	{
		Logger::error("密钥校验失败，当前密钥无效。");
		Logger::error("server msg: " + keyResp.message());

		// 服务端说无效，则本地也同步置为失效
		m_shm->shmUpdateStatus(m_info.clientId, m_info.serverId, 0);
	}
}

void ClientOP::keyLogout()
{
	//读取本地共享内存中的秘钥信息
	NodeSHMInfo node = m_shm->shmRead(m_info.clientId, m_info.serverId);

	if(node.status != 1)
	{
		Logger::info("本地密钥已经失效，无需重复注销...");
		return;
	}

	//请求初始化
	V2KeyLogoutRequestInfo reqInfo;
	reqInfo.header = buildKeyHeader(m_info, secmng::v2::CMD_KEY_LOGOUT_REQ);
	reqInfo.keyId = node.seckeyID;

	V2RequestCodec reqCodec(&reqInfo);
	std::string encstr = reqCodec.encodeMsg();

	//通信
	TcpSocket tcp;
	int ret = tcp.connectToHost(m_info.IP, m_info.port, 10);
	if (ret != 0)
	{
		Logger::error("连接服务器失败");
		return;
	}

	ret = tcp.sendMsg(encstr, 10);
	if(ret != 0)
	{
		Logger::error("发送注销请求失败");
		return;
	}

	std::string recvData;
	ret = tcp.recvMsg(recvData, 10);
	if(ret != 0)
	{
		Logger::error("接收注销响应失败");
		return;
	}

	V2RespondCodec rspCodec(recvData);
	secmng::v2::ResponsePacket* resData = static_cast<secmng::v2::ResponsePacket*>(rspCodec.decodeMsg());
	if (resData == nullptr || !resData->has_key_op_resp())
	{
		Logger::error("密钥注销响应格式错误。");
		return;
	}

	const secmng::v2::KeyOperationResponse& keyResp = resData->key_op_resp();

	if (keyResp.code() == secmng::v2::RESULT_SUCCESS)
	{
		Logger::info("密钥注销成功！");
		Logger::info("server msg: " + keyResp.message());

		// 本地共享内存同步置失效
		m_shm->shmUpdateStatus(m_info.clientId, m_info.serverId, 0);
	}
	else
	{
		Logger::error("密钥注销失败！");
		Logger::error("server msg: " + keyResp.message());
	}

}

void ClientOP::sendEncryptedMessage()
{

	// 第 1 步：让用户输入接收方和明文
	SendTextMessageInfo msgInfo;

	std::cout << "请输入接收方节点 ID: ";
	std::cin >> msgInfo.receiverId;

	// 清理缓冲区换行
	std::cin.ignore();

	std::cout << "请输入要发送的明文消息: ";
	std::getline(std::cin, msgInfo.plaintext);
	if (msgInfo.plaintext.empty())
	{
		Logger::error("消息内容不能为空。");
		return;
	}

	// 第 2 步：创建 MessageClient 并发送
	MessageClient msgClient(m_info,m_shm.get());
	SendMessageResult result = msgClient.sendTextMessage(msgInfo);

	if (!result.success)
	{
		Logger::error("发送加密消息失败。");
		return;
	}

	// 第 3 步：缓存最近一次 server_message_id
	m_lastServerMessageId = result.serverMessageId;

	Logger::info("发送加密消息完成。");
	Logger::info("已缓存最近一次 server_message_id: " + m_lastServerMessageId);
}

void ClientOP::queryMessage()
{

	std::string serverMessageId;

	if (!m_lastServerMessageId.empty())
	{
		std::cout << "最近一次 server_message_id: " << m_lastServerMessageId << "\n";
		std::cout << "请输入要查询的 server_message_id（直接回车使用最近一次）: ";

		std::cin.ignore();
		std::getline(std::cin, serverMessageId);

		if(serverMessageId.empty())
		{
			serverMessageId = m_lastServerMessageId;
		}
	}
	else
	{
		std::cout << "请输入 server_message_id: ";
		std::cin >> serverMessageId;
	}

	if (serverMessageId.empty())
	{
		Logger::error("server_message_id 不能为空。");
		return;
	}


	// 第 2 步：创建 MessageClient 并发起查询
	MessageClient msgClient(m_info, m_shm.get());

	bool ret = msgClient.queryMessageById(serverMessageId);
	if (!ret)
	{
		Logger::error("查询消息失败。");
		return;
	}
	Logger::info("查询消息完成。");
}

void ClientOP::queryRecentMessages()
{
	std::string senderId;
	int limit = 10;

	std::cout << "请输入要查询的发送方节点 ID（直接回车使用当前客户端）: ";

	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::getline(std::cin, senderId);

	if (senderId.empty())
	{
		senderId = m_info.clientId;
	}

	std::cout << "请输入最多查询多少条消息（建议 10）: ";
	std::cin >> limit;

	if (limit <= 0)
	{
		Logger::error("查询数量必须大于 0。");
		return;
	}

	if (limit > 100)
	{
		Logger::error("查询数量不能超过 100。");
		return;
	}

	MessageClient msgClient(m_info, m_shm.get());

	bool ret = msgClient.queryRecentMessagesBySender(senderId, limit);
	if (!ret)
	{
		Logger::error("查询消息列表失败。");
		return;
	}

	Logger::info("查询消息列表完成。");
}
