#include "ClientOP.h"
#include "RequestFactory.h"
#include "RsaCrypto.h"
#include "TcpSocket.h"
#include "RespondCodec.h"
#include "RespondFactory.h"
#include "Message.pb.h"
#include "MessageClient.h"
#include "Hash.h"
#include "Base64Util.h"
#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <iostream>
#include <sstream>


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

	//初始化序列化数据
	//序列化对象，工厂类创建对象，调用对象的序列化方法，得到序列化数据
	RequestInfo reqInfo;
	reqInfo.clientID = m_info.clientId;
	reqInfo.serverID = m_info.serverId;
	reqInfo.cmd = t_keyAgreement;   //秘钥协商
	reqInfo.data = str.str();  //非对称加密的公钥

	//创建哈希对象
	Hash sha1(T_SHA256);
	sha1.addData(str.str());
	reqInfo.sign = rsa.rsaSign(sha1.result());//公钥的哈希运算后的签名

	Logger::info("签名完成....");

	std::unique_ptr<CodecFactory> fac = std::make_unique<RequestFactory>(&reqInfo);
	std::unique_ptr<Codec> c(fac->createCodec());
	//得到序列化数据
	string encstr = c->encodeMsg();

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

	//解码，反序列化，验签
	//数据还原到RespondMsg
	std::unique_ptr<CodecFactory> rspFactory = std::make_unique<RespondFactory>(recvData);
	std::unique_ptr<Codec> rspCodec(rspFactory->createCodec());
	RespondMsg* resData = (RespondMsg*)rspCodec->decodeMsg();
	//判断状态
	if (!resData->status())
	{
		Logger::error("秘钥协商失败！");
		return false;
	}

	Logger::info("秘钥协商成功！");

	//将得到的密文解密
	std::string key = rsa.rsaPrivateDecrypt(resData->data());

	//base64编码后存储
	std::string base64Key = Base64Util::encode((const unsigned char*)key.data(), key.size());
	Logger::info("协商得到的对称加密的密钥:" + base64Key);

	//秘钥写入共享内存中
	NodeSHMInfo info;
	strcpy(info.clientID, m_info.clientId.data());
	strcpy(info.serverID, m_info.serverId.data());
	strcpy(info.seckey, base64Key.data());
	info.seckeyID = resData->seckeyid();
	info.status = 1;

	m_shm->shmWrite(&info);

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

	RequestInfo reqInfo;
	reqInfo.clientID = m_info.clientId;
	reqInfo.serverID = m_info.serverId;
	reqInfo.cmd = t_keyVerification;   //秘钥校验
	reqInfo.data = std::to_string(readInfo.seckeyID);  //秘钥ID
	reqInfo.sign = "";

	std::unique_ptr<CodecFactory> factory = std::make_unique<RequestFactory>(&reqInfo);
	std::unique_ptr<Codec> c(factory->createCodec());
	std::string resStr = c->encodeMsg();
	
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

	//解析响应
	std::unique_ptr<CodecFactory> rspFactory = std::make_unique<RespondFactory>(recvData);
	std::unique_ptr<Codec> rspCodec(rspFactory->createCodec());
	RespondMsg* resData = (RespondMsg*)rspCodec->decodeMsg();

	if (resData->status())
	{
		Logger::info("密钥校验成功，当前密钥有效。");
		Logger::info("server msg: " + resData->data());
	}
	else
	{
		Logger::error("密钥校验失败，当前密钥无效。");
		Logger::error("server msg: " + resData->data());

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
	RequestInfo reqInfo;
	reqInfo.clientID = m_info.clientId;
	reqInfo.serverID = m_info.serverId;
	reqInfo.cmd = t_keyLogout;
	reqInfo.data = std::to_string(node.seckeyID);
	reqInfo.sign = "";

	std::unique_ptr<CodecFactory> factory = std::make_unique<RequestFactory>(&reqInfo);
	std::unique_ptr<Codec> c(factory->createCodec());
	std::string encstr = c->encodeMsg();

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

	std::unique_ptr<CodecFactory> rspFactory = std::make_unique<RespondFactory>(recvData);
	std::unique_ptr<Codec> rspCodec(rspFactory->createCodec());
	RespondMsg* resData = (RespondMsg*)rspCodec->decodeMsg();

	if (resData->status())
	{
		Logger::info("密钥注销成功！");
		Logger::info("server msg: " + resData->data());

		// 本地共享内存同步置失效
		m_shm->shmUpdateStatus(m_info.clientId, m_info.serverId, 0);
	}
	else
	{
		Logger::error("密钥注销失败！");
		Logger::error("server msg: " + resData->data());
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
	bool ret = msgClient.sendTextMessage(msgInfo);

	if (!ret)
	{
		Logger::error("发送加密消息失败。");
		return;
	}
	Logger::info("发送加密消息完成。");
}
