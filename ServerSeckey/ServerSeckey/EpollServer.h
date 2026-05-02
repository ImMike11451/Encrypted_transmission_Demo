#pragma once
#include <sys/epoll.h>
#include <iostream>

// =======================================================
// 文件名: EpollServer.h
// 作用:   封装监听socket和epoll实例
// 功能:
//   1. 创建监听socket
//   2. 设置监听socket为非阻塞
//   3. 创建epoll
//   4. 把fd加入/修改/删除epoll
//   5. 等待事件
// =======================================================

class EpollServer
{
public:
	EpollServer();
	~EpollServer();

	// 初始化服务器，创建监听socket和epoll实例
	bool init(unsigned short port, int maxEvents = 1024);

	//等待事件
	int wait(epoll_event* events, int maxEvents, int timeout);

	//获取监听socket的文件描述符
	int getListenFd() const;

	//添加文件描述符到epoll实例，events是监听的事件类型
	bool addFd(int fd, uint32_t events);

	//修改文件描述符的监听事件
	bool modFd(int fd, uint32_t events);

	//删除文件描述符
	bool delFd(int fd);

private:
	//设置文件描述符为非阻塞
	bool setNonBlocking(int fd);

private:

	int m_lfd;  //监听socket的文件描述符
	int m_epfd; //epoll实例的文件描述符

};

