#pragma once
#include <iostream>
#include <string>
#include "jsoncpp/json/json.h"
#include "SecKeyShm.h"
using namespace Json;

struct ClientInfo
{
	std::string serverId;
	std::string clientId;
	std::string IP;
	unsigned short port;
};

enum keyType
{
	t_keyAgreement,
	t_keyVerification,
	t_keyLogout
};

class ClientOP
{
public:
	ClientOP(std::string fileName);
	~ClientOP();
	// 秘钥协商
	bool keyAgreement();
	// 秘钥校验
	void keyVerification();
	// 秘钥注销
	void keyLogout();


private:
	ClientInfo m_info;
	std::unique_ptr<SecKeyShm> m_shm;
};