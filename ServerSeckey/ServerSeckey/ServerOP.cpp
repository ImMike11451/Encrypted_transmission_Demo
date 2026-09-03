#include "ServerOP.h"
#include "RsaCrypto.h"
#include "Hash.h"
#include "Base64Util.h"
#include "Config.h"
#include "ErrorCode.h"
#include "Logger.h"
#include "MessageService.h"
#include "V2RequestCodec.h"
#include "V2RespondCodec.h"
#include <string>
#include <fstream> 
#include <unistd.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>


using namespace Json;

namespace
{
// 密钥业务响应头沿用请求 message_id，便于客户端和审计日志关联请求/响应。
V2HeaderInfo buildKeyResponseHeader(
	const secmng::v2::RequestPacket& packet,
	const std::string& serverId,
	int command)
{
	V2HeaderInfo header;
	header.command = command;
	header.senderId = serverId;
	header.timestamp = static_cast<long long>(time(nullptr));

	if (packet.has_header())
	{
		header.messageId = packet.header().message_id();
		header.receiverId = packet.header().sender_id();
	}

	return header;
}

V2KeyOperationResponseInfo makeKeyResponse(
	const secmng::v2::RequestPacket& packet,
	const std::string& serverId,
	int command,
	int code,
	const std::string& message,
	int keyId = 0,
	const std::string& data = "")
{
	V2KeyOperationResponseInfo info;
	info.header = buildKeyResponseHeader(packet, serverId, command);
	info.code = code;
	info.message = message;
	info.keyId = keyId;
	info.data = data;
	return info;
}
}

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
}

V2KeyOperationResponseInfo ServerOP::seckeyAgree(const secmng::v2::RequestPacket& packet)
{
	if (!packet.has_header() || !packet.has_key_agree_req())
	{
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_AGREE_RESP,
			secmng::v2::RESULT_INVALID_REQUEST, "invalid key agreement request");
	}

	const secmng::v2::Header& header = packet.header();
	const secmng::v2::KeyAgreementRequest& req = packet.key_agree_req();

	std::string pubFile = header.sender_id() + "_pub.pem";
	// 当前 demo 把客户端公钥落盘后交给 RsaCrypto 读取。
	// 后续可以改成内存 BIO，避免协议处理依赖临时文件。
	std::ofstream ofs(pubFile);
	ofs << req.public_key();
	ofs.flush();
	ofs.close();

	//验证签名
	RsaCrypto rsa(pubFile,false);

	//创建哈希对象
	Hash sha(T_SHA256);
	sha.addData(req.public_key());

	bool flage =  rsa.rsaVerify(sha.result(), req.sign());
	if (flage == false)
	{
		Logger::error("签名校验失败....");
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_AGREE_RESP,
			secmng::v2::RESULT_KEY_AGREE_FAILED, "verify sign failed");
	}

	// 生成 A-Server 会话密钥。Len16 对应 AES-128。
	std::string aesKey = getRandKey(Len16);

	// 只有持有客户端私钥的一方才能解开这段会话密钥。
	std::string secKey = rsa.rsaPublicEncrypt(aesKey);

	//将生成的随机字符串写入数据库
	NodeSHMInfo node;
	strcpy(node.clientID, header.sender_id().data());
	strcpy(node.serverID, header.receiver_id().data());
	// 数据库和共享内存当前以字符串保存 key，所以统一 Base64 编码。
	std::string base64Key = Base64Util::encode((const unsigned char*)aesKey.data(), aesKey.size());
	strcpy(node.seckey, base64Key.data());
	node.status = 1;

	//初始化node对象
	bool bl = mySQL->writeSecKeyWithTrans(&node);

	if (!bl)
	{
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_AGREE_RESP,
			secmng::v2::RESULT_KEY_AGREE_FAILED, "write db failed");
	}

	m_shm->shmWrite(&node);

	return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_AGREE_RESP,
		secmng::v2::RESULT_SUCCESS, "key agreement success", node.seckeyID, secKey);

}

//秘钥校验
V2KeyOperationResponseInfo ServerOP::secKeyCheck(const secmng::v2::RequestPacket& packet)
{
	if (!packet.has_header() || !packet.has_key_check_req())
	{
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_CHECK_RESP,
			secmng::v2::RESULT_INVALID_REQUEST, "invalid key check request");
	}

	const secmng::v2::Header& header = packet.header();
	const int reqKeyID = packet.key_check_req().key_id();
	NodeSHMInfo node = m_shm->shmRead(header.sender_id(), header.receiver_id());

	// 数据库是生命周期权威源，先确认密钥未过期、未注销且未被轮换。
	bool dbRet = mySQL->checkSecKey(header.sender_id(), header.receiver_id(), reqKeyID);
	if (!dbRet)
	{
		// 查询旧密钥失败时，不能把已经轮换出的新密钥缓存误置为失效。
		if (node.seckeyID == reqKeyID)
		{
			m_shm->shmUpdateStatus(header.sender_id(), header.receiver_id(), 0);
		}
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_CHECK_RESP,
			secmng::v2::RESULT_KEY_INVALID, "key expired, revoked, rotated or not found", reqKeyID);
	}

	// 数据库确认有效后，再检查共享内存缓存是否同步。
	if(strcmp(node.clientID, header.sender_id().data()) == 0 &&
		strcmp(node.serverID, header.receiver_id().data()) == 0 &&
		node.seckeyID == reqKeyID &&
		node.status == 1)
	{
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_CHECK_RESP,
			secmng::v2::RESULT_SUCCESS, "key active", reqKeyID);
	}

	return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_CHECK_RESP,
		secmng::v2::RESULT_SUCCESS, "key active in db, local cache missing", reqKeyID);
}

V2KeyOperationResponseInfo ServerOP::SeckeyLogout(const secmng::v2::RequestPacket& packet)
{
	if (!packet.has_header() || !packet.has_key_logout_req())
	{
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_LOGOUT_RESP,
			secmng::v2::RESULT_INVALID_REQUEST, "invalid key logout request");
	}

	const secmng::v2::Header& header = packet.header();
	int reqKeyID = packet.key_logout_req().key_id();

	//先更新数据库
	bool dbRet = mySQL->logoutSecKey(header.sender_id(), header.receiver_id(), reqKeyID);
	if (!dbRet)
	{
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_LOGOUT_RESP,
			secmng::v2::RESULT_KEY_LOGOUT_FAILED, "logout failed in db", reqKeyID);
	}

	// 2. 再同步更新共享内存状态
	int shmRet = m_shm->shmUpdateStatus(header.sender_id(), header.receiver_id(), 0);

	if(shmRet == 0)
	{
		return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_LOGOUT_RESP,
			secmng::v2::RESULT_SUCCESS, "logout success", reqKeyID);
	}

	// 数据库成功，共享内存失败，也可以认为整体成功，提示
	return makeKeyResponse(packet, m_serverID, secmng::v2::CMD_KEY_LOGOUT_RESP,
		secmng::v2::RESULT_SUCCESS, "db logout success, shm update failed", reqKeyID);
}

//包含a-z，A-Z，0-9，特殊字符
std::string ServerOP::getRandKey(keyLen len)
{
	std::string key(len, '\0');
	RAND_bytes((unsigned char*)key.data(), len);
	return key;
}


void ServerOP::handleClientFd(int clientfd)
{
	// 当前服务端使用短连接：一个连接只收一个请求、回一个响应，然后关闭。
	// 这样实现简单，适合演示协议和业务链路；后续做实时推送时再升级成长连接。
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

	std::string rspData = processV2Request(recvData);
	if(rspData.empty())
	{
		Logger::error("处理 v2 请求失败, fd = " + std::to_string(clientfd));
		m_server->delFd(clientfd);
		tcp.disconnect();
		return;
	}

    // 发送响应
    ret = tcp.sendMsg(rspData, 10);
    if (ret != 0)
    {
        Logger::error("发送响应失败, fd = " + std::to_string(clientfd));
    }

    // 短连接：本次请求处理完成就关闭
    m_server->delFd(clientfd);
    tcp.disconnect();

	std::cout << "------------------------------------------------------" << std::endl;
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

// 处理客户端发来的 v2 请求，并返回编码后的响应
std::string ServerOP::processV2Request(const std::string& recvData)
{
    // 解码客户端请求
    V2RequestCodec codec(recvData);
    secmng::v2::RequestPacket* packet = static_cast<secmng::v2::RequestPacket*>(codec.decodeMsg());

    if (packet == nullptr)
    {
        Logger::error("v2 请求解码失败。");
        return "";
    }

    // 消息业务处理对象
    MessageService msgService(m_serverID, mySQL.get(), m_shm.get());

    // 根据请求体类型分发业务
    switch (packet->body_case())
    {
    case secmng::v2::RequestPacket::kSendMsgReq:
    {
        // 发送加密消息
        V2SendMessageResponseInfo respInfo = msgService.handleSendMessage(*packet);

        V2RespondCodec rspCodec(&respInfo);
        return rspCodec.encodeMsg();
    }

    case secmng::v2::RequestPacket::kQueryMsgReq:
    {
        // 查询单条消息
        V2QueryMessageResponseInfo respInfo = msgService.handleQueryMessage(*packet);

        V2RespondCodec rspCodec(&respInfo);
        return rspCodec.encodeMsg();
    }

    case secmng::v2::RequestPacket::kQueryMsgListReq:
    {
        // 查询消息列表
        V2QueryMessageListResponseInfo respInfo = msgService.handleQueryMessageList(*packet);

        V2RespondCodec rspCodec(&respInfo);
        return rspCodec.encodeMsg();
    }

    case secmng::v2::RequestPacket::kKeyAgreeReq:
    {
        // 密钥协商
        V2KeyOperationResponseInfo respInfo = seckeyAgree(*packet);

        V2RespondCodec rspCodec(&respInfo);
        return rspCodec.encodeMsg();
    }

    case secmng::v2::RequestPacket::kKeyCheckReq:
    {
        // 密钥校验
        V2KeyOperationResponseInfo respInfo = secKeyCheck(*packet);

        V2RespondCodec rspCodec(&respInfo);
        return rspCodec.encodeMsg();
    }

    case secmng::v2::RequestPacket::kKeyLogoutReq:
    {
        // 密钥注销
        V2KeyOperationResponseInfo respInfo = SeckeyLogout(*packet);

        V2RespondCodec rspCodec(&respInfo);
        return rspCodec.encodeMsg();
    }

    default:
    {
        // 返回不支持请求类型的错误响应
        Logger::error("不支持的 v2 请求类型。");

        V2SendMessageResponseInfo respInfo;
        respInfo.header = {};

        if (packet->has_header())
        {
            // 保留请求 ID，并把响应发回原发送方
            respInfo.header.messageId = packet->header().message_id();
            respInfo.header.receiverId = packet->header().sender_id();
        }

        respInfo.header.command = secmng::v2::CMD_SEND_MSG_RESP;
        respInfo.header.senderId = m_serverID;
        respInfo.header.timestamp = static_cast<long long>(time(nullptr));

        respInfo.code = secmng::v2::RESULT_INVALID_REQUEST;
        respInfo.message = "unsupported v2 request type";
        respInfo.serverMessageId = "";
        respInfo.serverTime = static_cast<long long>(time(nullptr));
        respInfo.deliveryStatus = secmng::v2::DELIVERY_REJECTED;

        V2RespondCodec rspCodec(&respInfo);
        return rspCodec.encodeMsg();
    }
    }
}

