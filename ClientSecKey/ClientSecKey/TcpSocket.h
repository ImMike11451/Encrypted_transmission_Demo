#pragma once
#include<iostream>
#include<string>

#ifdef _WIN32
#include<WinSock2.h>
#else
#include<netinet/in.h>
#endif 


static const int TIMEOUT = 3;  //秒

// 职责：封装 TCP 连接、超时控制和长度帧收发。
// 协议：每个应用层消息都以 4 字节网络序长度开头，后面跟 protobuf payload。
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
	// 使用已经 accept 出来的套接字构造对象，服务端处理客户端 fd 时使用。
	TcpSocket(int sockfd);
	// 连接到指定 IP 和端口，timeout 表示连接超时时间。
	int connectToHost(std::string ip,unsigned short port,int timeout = TIMEOUT);
	// 发送一帧完整应用层消息。
	int sendMsg(const std::string& sendData, int timeout = TIMEOUT);
	// 接收一帧完整应用层消息。
	int recvMsg(std::string& recvData, int timeout = TIMEOUT);
	// 关闭当前 socket。
	int disconnect();
	~TcpSocket();
	
private:
	//设置套接字为非阻塞模式
	int setNonBlock(int fd);
	//设置套接字为阻塞模式
	int setBlock(int fd);
	// 使用 select 做读超时检测，不执行实际读操作。
	int readWithTimeout(unsigned int wait_time);
	// 使用 select 做写超时检测，不执行实际写操作。
	int writeWithTimeout(unsigned int wait_time);
	// 非阻塞 connect + select，用于实现连接超时。
	int connectTimeout(struct sockaddr_in* addr,unsigned int wait_time);
	// 循环读取指定字节数，解决 TCP 半包问题。
	int readn(std::string& recvData, int len);
	// 循环写入指定字节数，解决 send 一次写不完的问题。
	int writen(const std::string& sendData, int len);

private:
	int m_sockfd;  //套接字描述符
};
