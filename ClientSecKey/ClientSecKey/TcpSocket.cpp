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

TcpSocket::TcpSocket()
{
	m_sockfd = -1;
#ifdef _WIN32
	//在Windows平台上，使用套接字之前需要先调用WSAStartup函数进行初始化，并且只需要初始化一次即可。
	static bool is_winsock_initialized = false;
	if (!is_winsock_initialized)
	{
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);
		is_winsock_initialized = true;
	}
#endif 
}

TcpSocket::TcpSocket(int sockfd)
{
	m_sockfd = sockfd;
#ifdef _WIN32
	//在Windows平台上，使用套接字之前需要先调用WSAStartup函数进行初始化，并且只需要初始化一次即可。
	static bool is_winsock_initialized = false;
	if (!is_winsock_initialized)
	{
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);
		is_winsock_initialized = true;
	}
#endif 
}

int TcpSocket::connectToHost(std::string ip, unsigned short port, int timeout)
{
	int ret = 0;
	if (port <= 0 || port > 65535 || timeout < 0 )
	{
		ret = ParamError;
		return ret;
	}

	// 创建套接字
	m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sockfd < 0)
	{
		ret = errno;
		std::cout << "socket fun err:" << ret << std::endl;
		return ret;
	}

	// 设置服务器地址结构
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));  //将服务器地址结构清零
	server_addr.sin_family = AF_INET;  //设置地址族为IPv4
	server_addr.sin_port = htons(port);  //设置端口号，使用htons函数将主机字节序转换为网络字节序
	//server_addr.sin_addr.s_addr = inet_addr(ip.c_str());  //设置IP地址，使用inet_addr函数将点分十进制字符串转换为网络字节序的二进制形式
	//inet_pton函数将点分十进制字符串转换为网络字节序的二进制形式，并将结果存储在server_addr.sin_addr中
	if(inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0)
	{
		ret = ParamError;
		std::cout << "inet_pton fun err:" << ret << std::endl;
		return ret;
	}

	ret = connectTimeout(&server_addr, timeout);
	if (ret < 0)
	{
		//超时
		if(ret == -1 && errno == ETIMEDOUT)
		{
			ret = TimeoutError;
			std::cout << "connect timeout" << std::endl;
		}
		else
		{
			ret = errno;
			std::cout << "connect fun err:" << ret << std::endl;
		}
	}

	return ret;
}
// 发送一帧数据：4 字节网络序长度 + payload。
// TCP 本身没有消息边界，长度头用于接收方准确拆出一条 protobuf 消息。
int TcpSocket::sendMsg(const std::string& sendData, int timeout)
{
	// 参数检查
	if (m_sockfd < 0 || timeout < 0)
	{
		return ParamError;
	}

	int dataLen = sendData.size();//数据长度
	int netLen = htonl(dataLen); //将数据长度转换为网络字节序
	// 创建发送缓冲区
	// 总长度 = 4字节长度头 + 数据长度
	std::string sendBuf;
	sendBuf.resize(4 + dataLen);
	memcpy(&sendBuf[0], &netLen, 4); // 前4字节存放数据长度
	if (dataLen > 0)
	{
		memcpy(&sendBuf[4], sendData.data(), dataLen); // 后面存放数据内容
	} 
	// 返回0->没超时, 返回-1->异常 返回3002->超时
	int ret = writeWithTimeout(timeout);
	if (ret == 0)
	{
		ret = writen(sendBuf, sendBuf.size());
		if (ret != 0)
		{
#ifdef _WIN32
			ret = WSAGetLastError();
#else
			ret = errno;
#endif 
			std::cout << "writen fun err:" << ret << std::endl;
			return SyscallError;
		}
		return 0;
	}
	else if(ret == -1)
	{
#ifdef _WIN32
		ret = WSAGetLastError();
#else
		ret = errno;
#endif
		std::cout << "writeWithTimeout fun err:" << ret << std::endl;
		return SyscallError;
	}
	else if (ret == TimeoutError)
	{
		std::cout << "TimeoutError" << std::endl;
		return TimeoutError;
	}

	return SyscallError;
}

int TcpSocket::recvMsg(std::string& recvData, int timeout)
{
	// 参数检查
	if (m_sockfd < 0 || timeout < 0)
	{
		return ParamError;
	}
	int ret = readWithTimeout(timeout);
	if (ret != 0)  return ret;

	// 先读取 4 字节长度头，再按长度读取 payload，避免 TCP 粘包/半包影响上层协议。
	std::string lenBuf;
	ret = readn(lenBuf, 4);
	if (ret != 0) return ret;

	int netLen = 0;
	memcpy(&netLen, lenBuf.data(), 4); // 从前4字节获取数据长度（网络字节序）
	netLen = ntohl(netLen); // 转换为主机字节序

	ret = readn(recvData, netLen); // 读取数据内容
	if (ret != 0) return ret;

	return 0;
}

TcpSocket::~TcpSocket()
{
	disconnect();
}

// 使用非阻塞 connect + select 实现连接超时。
// 连接成功后恢复阻塞模式，让后续 readn/writen 的循环逻辑更直接。
int TcpSocket::connectTimeout(struct sockaddr_in* addr, unsigned int wait_time)
{
	int ret = 0;
	if (setNonBlock(m_sockfd) != 0)
	{
		return -1;
	}
	
	socklen_t addrlen = sizeof(struct sockaddr_in);
	ret = connect(m_sockfd, (struct sockaddr*)addr, addrlen);

	// 如果 ret == 0，说明连接立即成功
	// 比如本地网络很快，或者目标就在本机 / 局域网内
	if (ret == 0)
	{
		// 恢复回阻塞模式，保持后续收发逻辑简单
		setBlock(m_sockfd);
		return 0;
	}

#ifdef _WIN32
	// Windows 下，非阻塞 connect 失败后
	// 需要用 WSAGetLastError() 取错误码
	int err = WSAGetLastError();

	// 如果错误码不是“正在连接中”
	// 而是别的错误，说明 connect 已经真正失败了
	if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS)
	{
		setBlock(m_sockfd);
		return -1;
	}
#else
	// Linux 下，非阻塞 connect 返回 -1 时
	// 如果 errno == EINPROGRESS，表示连接正在进行中，这是正常现象
	// 之后应该继续用 select 检测连接结果
	if (errno != EINPROGRESS)
	{
		setBlock(m_sockfd);
		return -1;
	}
#endif

	//设置超时时间
	struct timeval val;
	val.tv_sec = wait_time;  //秒
	val.tv_usec = 0;  //微秒


	fd_set wrset;
	//初始化文件描述符集合
	FD_ZERO(&wrset);
	//加入套接字描述符到集合中
	FD_SET(m_sockfd, &wrset);
	//函数返回值小于0表示出错，等于0表示超时，大于0表示成功
	ret = select(m_sockfd + 1, NULL, &wrset, NULL, &val);
	if (ret < 0)
	{
		setBlock(m_sockfd);
		return -1;
	}
	else if (ret == 0)
	{
		setBlock(m_sockfd);
#ifdef _WIN32
		WSASetLastError(WSAETIMEDOUT);
#else
		errno = ETIMEDOUT;
#endif
		return TimeoutError;
	}
	else
	{
		/* ret返回为1（表示套接字可写），可能有两种情况，一种是连接建立成功，一种是套接字产生错误，*/
		/* 此时错误信息不会保存至errno变量中，因此，需要调用getsockopt来获取。 */
		int opt = 0; 
		socklen_t optlen = sizeof(opt);
		getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, (char*) & opt, &optlen);
		if (opt != 0)
		{
			setBlock(m_sockfd);
#ifdef _WIN32
			WSASetLastError(opt);
#else
			errno = opt;
#endif
			return -1;
		}
	}
	//设置套接字为阻塞模式
	setBlock(m_sockfd);
	return 0;
}
// 循环读取指定长度。recv 一次返回多少字节不可控，必须累积到目标长度。
int TcpSocket::readn(std::string& recvData, int len)
{
	int left = len; //剩余要读取的数据长度
	int nread = 0; //每次读取的数据长度
	char buf[1024] = {0}; //临时缓冲区

	recvData.clear(); //清空接收数据的字符串

	while (left > 0)
	{
		int readLen = left > sizeof(buf) ? sizeof(buf) : left; //每次读取的长度不能超过缓冲区的剩余空间

		nread = recv(m_sockfd, buf, readLen, 0); //调用recv函数读取数据，返回实际读取的字节数
		if (nread > 0)
		{
			recvData.append(buf, nread); //将读取到的数据追加到recvData字符串中
			left -= nread; //更新剩余要读取的数据长度
		}
		else if(nread == 0)
		{
			// recv返回0一般说明对端关闭了连接
			return PeerCloseError;
		}
		else
		{
			return -1; //出错
		}
	}

	return 0;
}
// 写超时检测，只判断 socket 是否可写，不真正发送数据。
int TcpSocket::writeWithTimeout(unsigned int wait_time)
{
	int ret = 0;
	struct timeval val;
	val.tv_sec = wait_time;  //秒
	val.tv_usec = 0;  //微秒

	// 将通信的m_sockfd放到写集合中进行检测
	fd_set wrset;
	FD_ZERO(&wrset);
	FD_SET(m_sockfd, &wrset);
	
	ret = select(m_sockfd + 1, NULL, &wrset, NULL, &val);

	if (ret == 0)
		return TimeoutError;
	else if (ret > 0)
		return 0;
	else
		return -1;
}
// 循环写入指定长度。send 一次可能只写出部分数据，所以要持续推进指针。
int TcpSocket::writen(const std::string& sendData, int len)
{
	int left = len; //剩余要发送的数据长度
	int written = 0; //每次发送的数据长度
	const char* buf = sendData.c_str(); //发送数据的指针
	while(left > 0)
	{
		written = send(m_sockfd, buf, left, 0);  //调用send函数发送数据，返回实际发送的字节数
 		if (written < 0)
		{
#ifdef _WIN32
			int err = WSAGetLastError();
			// 被信号打断，继续发送
			if (err == WSAEINTR)
			{
				continue;
			}
#else
			if(errno == EINTR)
			{
				continue;
			}
#endif 
			return -1; //发送出错，返回-1
		}
		else if (written == 0)
		{
			// send返回0一般说明连接异常
			return -1;
		}

		buf += written; //移动发送数据的指针
		left -= written; //更新剩余要发送的数据长度
	}
	return 0;
}
/*
* setNonBlock - 设置I/O为非阻塞模式
* @fd: 文件描符符
*/
int TcpSocket::setNonBlock(int fd)
{
#ifdef _WIN32
	u_long mode = 1;
	return ioctlsocket(fd, FIONBIO, &mode);
#else
	int flags = fcntl(fd, F_GETFL,0);
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

/*
* setBlock - 设置I/O为阻塞模式
* @fd: 文件描符符
*/
int TcpSocket::setBlock(int fd)
{
#ifdef _WIN32
	u_long mode = 0;
	return ioctlsocket(fd, FIONBIO, &mode);
#else
	int flags = fcntl(fd, F_GETFL,0);
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
}
// 读超时检测，只判断 socket 是否可读，不真正读取数据。
int TcpSocket::readWithTimeout(unsigned int wait_time)
{
	int ret = 0;
	struct timeval val;
	val.tv_sec = wait_time;  //秒
	val.tv_usec = 0;  //微秒

	fd_set rdset;
	FD_ZERO(&rdset);
	FD_SET(m_sockfd, &rdset);

	ret = select(m_sockfd + 1, &rdset, NULL, NULL, &val);

	if(ret == 0)
		return TimeoutError;
	else if (ret > 0)
		return 0;
	else
		return -1;
}

int TcpSocket::disconnect()
{
	if (m_sockfd == -1)
	{
		return 0;
	}
#ifdef _WIN32
	closesocket(m_sockfd);
#else
	close(m_sockfd);
#endif 
	m_sockfd = -1;
	return 0;
}
