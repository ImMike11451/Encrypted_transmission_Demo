#include "TcpServer.h"
#include "TcpSocket.h"
#include<iostream>
#include<string>
#include<cstring>

#ifdef _WIN32
#include<winsock2.h>
#include<WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#else
#include<unistd.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/select.h>
#include<errno.h>
#endif
TcpServer::TcpServer()
{
	m_lfd = -1;
}

TcpServer::~TcpServer()
{
	closeListen();
}

int TcpServer::setListen(unsigned short port)
{
	int ret = 0;
	//创建套接字
	m_lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_lfd == -1)
	{
		perror("m_lfd ERROR");
		exit(0);
	}

	int on = 1;
	//设置套接字选项，允许地址重用
	ret = setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));

	if (ret == -1)
	{
		perror("setsockopt ERROR");
		closeListen();
		exit(0);
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr)); //将服务器地址结构清零
	server_addr.sin_family = AF_INET; //地址族为IPv4
	server_addr.sin_port = htons(port); //设置端口号，使用htons函数将主机字节序转换为网络字节序
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //设置IP地址，使用htonl函数将主机字节序转换为网络字节序的二进制形式，INADDR_ANY表示监听所有可用的网络接口
	//绑定套接字到指定的IP地址和端口号
	ret = bind(m_lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret == -1)
	{
		perror("bind ERROR");
		closeListen();
		exit(0);
	}
	//将套接字设置为监听状态，等待客户端连接请求
	ret = listen(m_lfd, 128);
	if (ret == -1)
	{
		perror("listen ERROR");
		closeListen();
		exit(0);
	}

	return ret;

}

TcpSocket* TcpServer::acceptClient(int timeout)
{
	int ret = 0;
	struct timeval val;
	val.tv_sec = timeout; //秒
	val.tv_usec = 0; //微秒

	fd_set rdset;
	FD_ZERO(&rdset);
	FD_SET(m_lfd, &rdset);

	ret = select(m_lfd + 1, &rdset, NULL, NULL, &val);

	if (ret < 0)
	{
		perror("select ERROR");
		return nullptr;
	}
	else if (ret == 0)
	{
		return nullptr; //等待超时，没有检测到连接请求
	}

	struct sockaddr_in client_addr;
#ifdef _WIN32
	int len = sizeof(client_addr);
#else
	socklen_t len = sizeof(client_addr);
#endif // _WIN32
	int cfd = accept(m_lfd, (struct sockaddr*)&client_addr, &len);
	if (cfd == -1)
	{
		perror("accept ERROR");
		return nullptr;
	}
	return new TcpSocket(cfd);
}

void TcpServer::closeListen()
{
	if (m_lfd != -1)
	{
#ifdef _WIN32
		closesocket(m_lfd);
#else
		close(m_lfd);
#endif
	}
}
