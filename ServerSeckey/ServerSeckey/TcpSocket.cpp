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
/*
* sendMsg - 发送数据（带4字节长度头）
* @sendData: 要发送的数据内容
* @timeout:  写超时时间（秒）
*
* 发送数据时采用“长度 + 数据”的协议格式：
*
* 数据结构：
* +------------+--------------+
* | 4字节长度   | 数据内容      |
* +------------+--------------+
*
* 其中：
* - 前4字节为数据长度（网络字节序）
* - 后续为真实数据内容
*
* 发送流程：
* 1. 计算数据长度
* 2. 将长度转换为网络字节序
* 3. 构造发送缓冲区（长度头 + 数据）
* 4. 检测写超时
* 5. 调用 writen() 发送完整数据
*
* 返回值：
* 0            -> 发送成功
* ParamError   -> 参数错误
* TimeoutError -> 发送超时
* SyscallError -> 系统调用错误
*/
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

	// 先读取4字节长度头
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

/*
* connectTimeout - connect
* @addr: 要连接的对方地址
* @wait_seconds: 等待超时秒数，如果为0表示正常模式
* 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
*/
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
/*
* readn - 从套接字循环读取指定长度的数据
* @recvData: 接收数据的字符串引用
* @len:      需要读取的字节数
*
* TCP 的 recv() 一次读取的数据长度是不确定的，
* 可能小于请求的长度，因此需要循环调用 recv()
* 直到读取到指定长度的数据。
* 返回值：
* 0                     -> 读取成功
* PeerCloseError        -> 对端关闭连接
* -1                    -> recv 调用失败
*/
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
/*
* writeWithTimeout - 写超时检测函数，不执行实际写操作
* @wait_time: 等待超时时间（秒），如果为0表示不检测超时
*
* 使用 select 监测套接字是否可写，在指定时间内等待写事件发生。
*
* 返回值：
* 0             -> 套接字可写，未超时
* -1            -> 检测失败（select 调用错误）
* TimeoutError  -> 等待超时
*/
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
/*
* writen - 向套接字循环发送指定长度的数据
* @sendData: 要发送的数据
* @len:      需要发送的字节数
*
* TCP 的 send() 一次发送的数据长度不确定，
* 因此需要循环调用 send()，直到发送完指定长度的数据。
*
* 返回值：
* 0   -> 所有数据发送成功
* -1  -> 发送失败（send 系统调用错误或连接异常）
*/
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
/*
* readWithTimeout - 读事件超时检测函数（不执行读操作）
* @wait_time: 等待超时的秒数，如果为0表示不检测超时
*
* 该函数通过 select() 监视套接字的读事件，
* 判断套接字在指定时间内是否变为可读状态。
* 这里只是检测是否可以读取数据，并不真正执行 read/recv 操作。
*
* 返回值：
* 0              -> 套接字可读（未超时）
* TimeoutError   -> 等待超时，没有检测到读事件
* -1             -> select 调用失败
*/
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