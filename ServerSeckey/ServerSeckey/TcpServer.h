#pragma once
#include "TcpSocket.h"

class TcpServer
{
public:
	TcpServer();
	~TcpServer();

	int setListen(unsigned short port);
	TcpSocket* acceptClient(int timeout = 5);
	void closeListen();
private:
	int m_lfd; //监听套接字描述符
};

