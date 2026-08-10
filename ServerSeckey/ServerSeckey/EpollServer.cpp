#include "EpollServer.h"
#include "Logger.h"
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

EpollServer::EpollServer()
	: m_lfd(-1), m_epfd(-1)
{
}

EpollServer::~EpollServer()
{

	if(m_lfd != -1)
	{
		close(m_lfd);
		m_lfd = -1;
	}

	if(m_epfd != -1)
	{
		close(m_epfd);
		m_epfd = -1;
	}

}

bool EpollServer::init(unsigned short port, int maxEvents)
{
	// 创建监听 socket。EpollServer 只负责网络事件，不负责业务协议。
	m_lfd = socket(AF_INET, SOCK_STREAM, 0);
	if(m_lfd < 0)
	{
		Logger::error("创建监听socket失败: " + std::string(strerror(errno)));
		return false;
	}

	int on = 1;
	// 允许地址复用，避免服务重启时端口短时间不可用。
	setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if(!setNonBlocking(m_lfd))
	{
		Logger::error("设置监听socket为非阻塞失败: " + std::string(strerror(errno)));
		return false;
	}

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(m_lfd, (sockaddr*)&addr, sizeof(addr)) != 0)
	{
		Logger::error("绑定监听socket失败: " + std::string(strerror(errno)));
		return false;
	}

	if(listen(m_lfd, 128) != 0)
	{
		Logger::error("监听socket失败: " + std::string(strerror(errno)));
		return false;
	}

	// 创建 epoll 实例，并用 EPOLL_CLOEXEC 避免子进程继承该 fd。
	m_epfd = epoll_create1(EPOLL_CLOEXEC);
	if(m_epfd < 0)
	{
		Logger::error("创建epoll实例失败: " + std::string(strerror(errno)));
		return false;
	}

	return addFd(m_lfd, EPOLLIN);
}

int EpollServer::wait(epoll_event* events, int maxEvents, int timeout)
{
	return epoll_wait(m_epfd, events, maxEvents, timeout);
}

int EpollServer::getListenFd() const
{
	return m_lfd;
}

bool EpollServer::addFd(int fd, uint32_t events)
{
	// 调用方决定监听哪些事件，例如监听 fd 用 EPOLLIN，客户端 fd 可加 EPOLLET。
	epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;

	return epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool EpollServer::modFd(int fd, uint32_t events)
{
	epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;

	return epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool EpollServer::delFd(int fd)
{
	return epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

bool EpollServer::setNonBlocking(int fd)
{
	// 获取当前文件描述符标志，再追加 O_NONBLOCK。
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		return false;
	}
	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		return false;
	}

	return true;
}
