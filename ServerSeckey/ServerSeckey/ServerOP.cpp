#include "ServerOP.h"
#include "RespondCodec.h"
#include "RequestCodec.h"
#include "RespondFactory.h"
#include "RequestFactory.h"
#include "RsaCrypto.h"
#include "Hash.h"
#include "Base64Util.h"
#include "Config.h"
#include "ErrorCode.h"
#include "Logger.h"
#include <string>
#include <fstream> 
#include <unistd.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>


using namespace Json;

ServerOP::ServerOP(std::string json) :mySQL(nullptr), m_server(nullptr),m_shm(nullptr)
{
	//从json文件中读取服务器配置
	Config config;
	if(!config.load(json))
	{
		Logger::error("加载配置文件失败: " + json);
		// 处理失败（例如抛异常或设置标志），避免继续使用未初始化的配置
		return;
	}
	//将root中的端口号赋值给Port成员变量
	m_port = config.getInt("Port");
	m_serverID = config.getString("ServerID");
	m_userDB = config.getString("UserDB");
	m_passDB = config.getString("PassDB");
	m_connectDB = config.getString("ConnectDB");
	m_host = config.getString("Host");

	//实例化数据库对象
	mySQL = std::make_unique<mysqlOP>();
	if (!mySQL || !mySQL->connectDb(m_host, m_userDB, m_passDB, m_connectDB)) {
		Logger::error("连接数据库失败: " + m_host);
		// 处理失败（例如抛异常或设置标志），避免继续使用 mySQL
	}

	//实例化共享内存
	//从配置文件读取path/name   客户端一个秘钥
	std::string shmKey = config.getString("ShmKey");
	int maxNode = config.getInt("ShmMaxNode");
	m_shm = std::make_unique<SecKeyShm>(shmKey, maxNode);
	//m_shm = new SecKeyShm(shmKey, maxNode);
	// 
	// 创建线程池，线程数可按机器核心数或配置指定
	m_pool = std::make_unique<ThreadPool>(4);
}

ServerOP::~ServerOP() = default;

void ServerOP::startServer()
{
	m_server = std::make_unique<EpollServer>();

	if(!m_server->init(m_port, 128))
	{
		Logger::error("初始化epoll服务器失败");
		return;
	}

	Logger::info("服务器启动成功! 等待客户端连接...");

	const int maxEvents = 128;
	epoll_event events[maxEvents];

	while (true)
	{
		// 等待事件，返回就绪的事件数量
		int nready = m_server->wait(events, maxEvents, -1);
		if(nready < 0)
		{
			if(errno == EINTR)
			{
				continue;
			}
			else
			{
				Logger::error("等待事件失败");
				break;
			}
		}
		for(int i = 0;i < nready; ++i)
		{
			int fd = events[i].data.fd;
			if(fd == m_server->getListenFd())
			{
				while (true)
				{
					sockaddr_in clientAddr;
					socklen_t len = sizeof(clientAddr);

					int cfd = accept(fd,(sockaddr*)&clientAddr,&len);
					if(cfd < 0)
					{
						if(errno == EAGAIN || errno == EWOULDBLOCK)
						{
							// 已经处理完所有就绪的连接，退出循环
							break;
						}
						else
						{
							Logger::error("接受客户端连接失败");
							break;
						}
					}
					//新的客户端连接，设置为非阻塞，并添加到epoll监听
					if(!m_server->addFd(cfd, EPOLLIN | EPOLLET))
					{
						Logger::error("添加客户端fd到epoll失败" + std::to_string(cfd));
						close(cfd);
						continue;
					}

					Logger::info("新的客户端连接: " + std::to_string(cfd));
				}
			}
			else
			{
				// 处理就绪的客户端fd
				if (events[i].events & EPOLLIN)
				{
					if(!markFdProcessing(fd))
					{
						continue;
					}
					m_pool->enqueue([this, fd]() {
						this->handleClientFd(fd);
						this->unmarkFdProcessing(fd);
						});
				}
				else
				{
					m_server->delFd(fd);
					close(fd);
				}
			}
		}
	}

	////设置服务器监听端口
	//m_server->setListen(m_port);

	//Logger::info("等待客户端连接...");

	//while (1)
	//{
	//	TcpSocket* tcp = m_server->acceptClient(10);
	//	if(tcp == nullptr)
	//	{
	//		//Logger::warn("暂时无客户端连接，等待...");
	//		continue;
	//	}
	//	//通信,创建子线程
	//	//pthread_t tid;
	//	//pthread_create(&tid, NULL, working, this);
	//	////线程回收
	//	//pthread_detach(tid);
	//	//// 插入 map 时加锁
	//	//{
	//	//	//将map的访问放在互斥锁保护的范围内，确保线程安全
	//	//	std::lock_guard<std::mutex> lock(m_mutex);
	//	//	m_list.insert(std::make_pair(tid, tcp));
	//	//}

	//	// 把连接处理任务提交给线程池
	//	m_pool->enqueue([this, tcp]() {
	//		this->handleClient(tcp);

	//		// 处理完成后断开连接并清理资源
	//		tcp->disconnect();
	//		delete tcp;
	//		});

	//}

}

//void* ServerOP::working(void* arg)
//{
//	usleep(500000);
//	std::string data = string();
//	//通过参数将传递的this对象转换
//	ServerOP* op = static_cast<ServerOP*>(arg);
//	//从op中将通信的套接字取出
//	TcpSocket* tcp = nullptr;
//	//读取m_list时加锁
//	{
//		//访问共享资源m_list时加锁，确保线程安全
//		std::lock_guard<std::mutex> lock(op->m_mutex);
//		//根据当前线程ID从map中获取对应的TcpSocket对象
//		auto it = op->m_list.find(pthread_self());
//		if (it != op->m_list.end()) {
//			tcp = it->second;
//		}
//	}
//	if (tcp == nullptr)
//	{
//		Logger::error("未找到当前线程对应的TcpSocket对象");
//		return NULL;
//	}
//
//	op->handleClient(tcp);
//
//	tcp->disconnect();
//
//	//从map中删除当前线程ID对应的条目
//	{
//		std::lock_guard<std::mutex> lock(op->m_mutex);
//		op->m_list.erase(pthread_self());
//	}
//
//	delete tcp;
//	return NULL;
//}

std::string ServerOP::seckeyAgree(RequestMsg* reqMsg)
{
	std::string pubFile = reqMsg->clientid() + "_pub.pem";
	//将客户端的公钥写入文件
	std::ofstream ofs(pubFile);
	ofs << reqMsg->data();
	ofs.flush();
	ofs.close();

	//验证签名
	RsaCrypto rsa(pubFile,false);

	//创建哈希对象
	Hash sha(T_SHA256);
	sha.addData(reqMsg->data());

	RespondInfo info;
	bool flage =  rsa.rsaVerify(sha.result(), reqMsg->sign());
	if (flage == false)
	{
		Logger::error("签名校验失败....");
		info.status = false;
	}
	else
	{
		//生成随机字符串
		//对称加密的秘钥，aes，密钥长度为16,24,32字节
		std::string aesKey = getRandKey(Len16);

		//通过公钥加密
		std::string secKey = rsa.rsaPublicEncrypt(aesKey);
		//初始化回复的数据
		info.clientID = reqMsg->clientid();
		info.data = secKey;
		//info.seckeyID = 12;
		//info.seckeyID = mySQL->getKeyId();
		info.serverID = m_serverID;
		info.status = true;

		//将生成的随机字符串写入数据库
		NodeSHMInfo node;
		strcpy(node.clientID, reqMsg->clientid().data());
		strcpy(node.serverID, reqMsg->serverid().data());
		//base64编码后存储
		std::string base64Key = Base64Util::encode((const unsigned char*)aesKey.data(), aesKey.size());
		strcpy(node.seckey, base64Key.data());
		//node.seckeyID = mySQL->getKeyId();
		node.status = 1;

		//初始化node对象
		bool bl = mySQL->writeSecKeyWithTrans(&node);

		if (!bl)
		{
			info.status = false;
			info.data = "write db failed";
		}
		else
		{
			info.seckeyID = node.seckeyID;
			m_shm->shmWrite(&node);
		}
	}

	//序列化
	std::unique_ptr<CodecFactory> fac = std::make_unique<RespondFactory>(&info);
	std::unique_ptr<Codec> c(fac->createCodec());
	std::string ecnMsg = c->encodeMsg();
	//发送
	return ecnMsg;

}

//秘钥校验
std::string ServerOP::secKeyCheck(RequestMsg* msg)
{
    RespondInfo info;

	info.clientID = msg->clientid();
	info.serverID = msg->serverid();
	info.seckeyID = atoi(msg->data().data());
	info.status = false;
	info.data = "key invalid";

	int reqKeyID = atoi(msg->data().data());
	//先检查共享内存中是否存在有效的秘钥
	NodeSHMInfo node = m_shm->shmRead(info.clientID, info.serverID);
	if(strcmp(node.clientID, msg->clientid().data()) == 0 &&
		strcmp(node.serverID, msg->serverid().data()) == 0 &&
		node.seckeyID == reqKeyID &&
		node.status == 1)
	{
		info.status = true;
		info.data = "key valid in shm";
	}
	else
	{
		//共享内存中没有，就查找数据库
		bool ret = mySQL->checkSecKey(info.clientID, info.serverID, reqKeyID);
		if (ret)
		{
			info.status = true;
			info.data = "key valid in db";
		}
		else
		{
			info.status = false;
			info.data = "key invalid";
		}
	}

	std::unique_ptr<CodecFactory> fac = std::make_unique<RespondFactory>(&info);
	std::unique_ptr<Codec> c(fac->createCodec());
    string n = c->encodeMsg();

	Logger::info("秘钥校验成功");

    return n;
}

std::string ServerOP::SeckeyLogout(RequestMsg* msg)
{
	RespondInfo info;
	info.clientID = msg->clientid();
	info.serverID = msg->serverid();
	info.seckeyID = atoi(msg->data().data());
	info.status = false;
	info.data = "logout failed";

	int reqKeyID = atoi(msg->data().data());

	//先更新数据库
	bool dbRet = mySQL->logoutSecKey(msg->clientid(), msg->serverid(), reqKeyID);
	if (!dbRet)
	{
		info.status = false;
		info.data = "logout failed in db";
	}
	else
	{
		// 2. 再同步更新共享内存状态
		int shmRet = m_shm->shmUpdateStatus(msg->clientid(), msg->serverid(), 0);
		if(shmRet == 0)
		{
			info.status = true;
			info.data = "logout success";
		}
		else
		{
			// 数据库成功，共享内存失败，也可以认为整体成功，提示
			info.status = true;
			info.data = "db logout success, shm update failed";
		}
	}

	std::unique_ptr<CodecFactory> fac = std::make_unique<RespondFactory>(&info);
	std::unique_ptr<Codec> c(fac->createCodec());
	std::string ecnMsg = c->encodeMsg();

	Logger::info("秘钥注销成功");

	return ecnMsg;
}

//包含a-z，A-Z，0-9，特殊字符
std::string ServerOP::getRandKey(keyLen len)
{
	std::string key(len, '\0');
	RAND_bytes((unsigned char*)key.data(), len);
	return key;
	//生成随机数种子
	//static bool seed = false;
	//if (!seed)
	//{
	//	srand(time(NULL));
	//	seed = true;
	//}

	//int flage = 0;
	//string randStr = string();
	//char* cs = "^)$#}{|>?@!";
	//for (int i = 0; i < len; i++)
	//{

	//	//4种情况
	//	flage = rand() % 4;
	//	switch (flage)
	//	{
	//	case 0: //a-z
	//		randStr.append(1,'a' + rand() % 26);
	//		break;
	//	case 1://A-Z
	//		randStr.append(1, 'A' + rand() % 26);
	//		break;
	//	case 2://0-9
	//		randStr.append(1, '0' + rand() % 10);
	//		break;
	//	case 3:
	//		randStr.append(1,cs[rand() % strlen(cs)]);
	//		break;
	//	default:
	//		break;
	//	}
	//}
	/*return randStr;*/
}

//void ServerOP::handleClient(TcpSocket* tcp)
//{
//	std::string recvData;
//	
//	//接收客户端数据
//	int ret = tcp->recvMsg(recvData, 10);
//	if(ret != 0)
//	{
//		Logger::error("接收客户端数据失败, ret = " + std::to_string(ret));
//		return;
//	}
//
//	//解码
//	std::unique_ptr<CodecFactory> fac = std::make_unique<RequestFactory>(recvData);
//	std::unique_ptr<Codec> c(fac->createCodec());
//	
//	RequestMsg* recvReq = (RequestMsg*)c->decodeMsg();
//	if (recvReq == nullptr)
//	{
//		Logger::error("请求解码失败");
//		return;
//	}
//
//	Logger::info("客户端：" + recvReq->clientid() + "  连接成功");
//
//	// 3. 处理业务
//	std::string rspData = processRequest(recvReq);
//
//	//发送响应
//	ret = tcp->sendMsg(rspData, 10);
//	if(ret != 0)
//	{
//		Logger::error("发送响应数据失败, ret = " + std::to_string(ret));
//	}
//}

void ServerOP::handleClientFd(int clientfd)
{
    TcpSocket tcp(clientfd);

    std::string recvData;
    int ret = tcp.recvMsg(recvData, 10);
    if (ret != 0)
    {
        Logger::error("接收客户端数据失败, fd = " + std::to_string(clientfd));
        m_server->delFd(clientfd);
        tcp.disconnect();
        return;
    }

    // 解码请求
    std::unique_ptr<CodecFactory> fac = std::make_unique<RequestFactory>(recvData);
    std::unique_ptr<Codec> c(fac->createCodec());

    // 注意：这里返回的是Codec内部成员地址，不能delete
    RequestMsg* recvReq = static_cast<RequestMsg*>(c->decodeMsg());
    if (recvReq == nullptr)
    {
        Logger::error("请求解码失败, fd = " + std::to_string(clientfd));
        m_server->delFd(clientfd);
        tcp.disconnect();
        return;
    }

    Logger::info("收到客户端请求: " + recvReq->clientid());

    // 处理业务
    std::string rspData = processRequest(recvReq);

    // 发送响应
    ret = tcp.sendMsg(rspData, 10);
    if (ret != 0)
    {
        Logger::error("发送响应失败, fd = " + std::to_string(clientfd));
    }

    // 短连接：本次请求处理完成就关闭
    m_server->delFd(clientfd);
    tcp.disconnect();
}

std::string ServerOP::processRequest(RequestMsg* msg)
{
	switch (msg->cmdtype())
	{
	case t_keyAgreement:
		return seckeyAgree(msg);
	case t_keyVerification:
		return secKeyCheck(msg);
	case  t_keyLogout:
		return SeckeyLogout(msg);
	default:
	{
		Logger::error("未知的请求类型: " + std::to_string(msg->cmdtype()));

		RespondInfo info;
		
		info.status = false;
		info.data = "unknown cmd type";
		info.clientID = msg->clientid();
		info.serverID = m_serverID;
		info.seckeyID = 0;

		std::unique_ptr<CodecFactory> fac = std::make_unique<RespondFactory>(&info);
		std::unique_ptr<Codec> c(fac->createCodec());
		return c->encodeMsg();
	}
	}
}

bool ServerOP::markFdProcessing(int fd)
{
	// 通过互斥锁保护对 m_processingFds 的访问，确保线程安全
	std::lock_guard<std::mutex> lock(m_fdMutex);

	if(m_processingFds.count(fd) > 0)
	{
		// 已经有线程在处理这个fd，返回false
		return false;
	}

	// 没有线程在处理，标记为正在处理
	m_processingFds.insert(fd);
	return true;
}

void ServerOP::unmarkFdProcessing(int fd)
{
	std::lock_guard<std::mutex> lock(m_fdMutex);
	m_processingFds.erase(fd);
}

