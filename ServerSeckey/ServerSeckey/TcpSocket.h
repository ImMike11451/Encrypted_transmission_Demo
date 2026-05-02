#pragma once
#include<iostream>
#include<string>

#ifdef _WIN32
#include<WinSock2.h>
#else
#include<netinet/in.h>
#endif 


static const int TIMEOUT = 3;  //秒

class TcpSocket
{
public:
	// 错误码
	enum ErrorType {
		ParamError = 3001,      // 参数错误
		TimeoutError,           // 超时
		PeerCloseError,         // 对端关闭
		MallocError,            // 内存错误（如需要）
		SyscallError            // 系统调用错误（补一个更通用）
	};
	TcpSocket();
	//使用已经创建的套接字描述符来构造TcpSocket对象
	TcpSocket(int sockfd);
	//连接到指定的IP地址和端口号服务器
	int connectToHost(std::string ip,unsigned short port,int timeout = TIMEOUT);
	//发送数据
	int sendMsg(const std::string& sendData, int timeout = TIMEOUT);
	//接收数据
	int recvMsg(std::string& recvData, int timeout = TIMEOUT);
	//断开连接
	int disconnect();
	~TcpSocket();
	
private:
	//设置套接字为非阻塞模式
	int setNonBlock(int fd);
	//设置套接字为阻塞模式
	int setBlock(int fd);
	//使用select函数 读超时检测函数，不含读操作
	int readWithTimeout(unsigned int wait_time);
	//使用select函数 写超时检测函数, 不包含写操作
	int writeWithTimeout(unsigned int wait_time);
	//使用select函数 连接超时的connect函数
	int connectTimeout(struct sockaddr_in* addr,unsigned int wait_time);
	//每次从缓冲区中读取n个字符
	int readn(std::string& recvData, int len);
	//每次向缓冲区中写入n个字符
	int writen(const std::string& sendData, int len);

private:
	int m_sockfd;  //套接字描述符
};
