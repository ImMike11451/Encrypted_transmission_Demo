#pragma once
#include "TcpSocket.h"
#include "TcpServer.h"
#include "Message.pb.h"
#include "mysqlOP.h"
#include "SecKeyShm.h"
#include "ThreadPool.h"
#include "EpollServer.h"
#include <unordered_set>
#include <map>
#include <pthread.h>
#include <iostream>
#include <mutex>
#include <memory>

enum keyType
{
	t_keyAgreement,
	t_keyVerification,
	t_keyLogout
};

enum keyLen
{
	Len16 = 16,
	Len24 = 24,
	Len32 = 32
};

class ServerOP
{
public:
	ServerOP(std::string json = "server.json");
	~ServerOP();
	//线程回调函数,可以是类静态函数，类的友元函数，普通函数
	//static void* working(void* arg);
	void startServer();
	//秘钥协商
	std::string seckeyAgree(RequestMsg* msg);
	//秘钥校验
	std::string secKeyCheck(RequestMsg* msg);
	//秘钥注销
	std::string SeckeyLogout(RequestMsg* msg);

private:
	//生成随机字符串
	std::string getRandKey(keyLen len);

	//统一处理客户端请求的函数，根据请求类型调用不同的处理函数
	//void handleClient(TcpSocket* tcp);

	//处理一个已经就绪的客户端fd
	void handleClientFd(int clientfd);

	// 根据请求类型分发业务处理
	std::string processRequest(RequestMsg* msg);

	// 标记fd正在处理，避免重复投递
	bool markFdProcessing(int fd);

	// 取消fd处理标记
	void unmarkFdProcessing(int fd);

	//根据请求类型调用不同的处理函数
	//std::string processRequest(RequestMsg* msg);


private:

	unsigned short m_port;
	//线程ID和通信套接字的映射表
	//std::map<pthread_t,TcpSocket*> m_list;

	std::string m_serverID;  //当前服务器ID
	std::string m_userDB;
	std::string m_passDB;
	std::string m_connectDB;
	std::string m_host;

	std::unique_ptr<EpollServer>m_server;
	//数据库实例
	std::unique_ptr<mysqlOP>mySQL;

	std::unique_ptr<SecKeyShm>m_shm;
	//线程池实例
	std::unique_ptr<ThreadPool> m_pool;

	// 保护“正在处理的fd集合”
	std::mutex m_fdMutex;
	// 使用unordered_set来存储正在处理的fd，提供快速查找和插入
	std::unordered_set<int> m_processingFds;
};

