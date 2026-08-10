#pragma once
#include <sys/epoll.h>
#include <iostream>

// 职责：封装监听 socket 和 epoll 实例。
// 边界：只管理 fd 的监听，不处理具体业务请求；业务处理由 ServerOP 和线程池完成。
class EpollServer
{
public:
	EpollServer();
	~EpollServer();

	// 初始化监听 socket、设置非阻塞、创建 epoll，并把监听 fd 加入 epoll。
	bool init(unsigned short port, int maxEvents = 1024);

	// 等待 epoll 事件，timeout 与 epoll_wait 语义一致。
	int wait(epoll_event* events, int maxEvents, int timeout);

	// 获取监听 socket 的文件描述符。
	int getListenFd() const;

	// 添加文件描述符到 epoll 实例。
	bool addFd(int fd, uint32_t events);

	// 修改文件描述符的监听事件。
	bool modFd(int fd, uint32_t events);

	// 从 epoll 实例中删除文件描述符。
	bool delFd(int fd);

private:
	// 设置文件描述符为非阻塞，配合 epoll 边缘触发避免阻塞事件循环。
	bool setNonBlocking(int fd);

private:

	int m_lfd;  //监听socket的文件描述符
	int m_epfd; //epoll实例的文件描述符

};

