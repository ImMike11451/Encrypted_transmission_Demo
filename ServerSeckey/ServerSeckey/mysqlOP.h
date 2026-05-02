#pragma once
#include <string>
#include <iostream>
#include <mysql/mysql.h>
#include "SeckKeyNodeInfo.h"

class mysqlOP
{

public:
	mysqlOP();
	~mysqlOP();

	//连接数据库
	bool connectDb(std::string host, std::string user, std::string password, std::string dbName);
	//将秘钥信息写入数据库
	bool writeSecKey(NodeSHMInfo* pNode);
	//从数据库中获取秘钥编号
	int getKeyId();
	//根据秘钥ID获取秘钥状态，返回-1表示查询失败，其他为state值
	int getKeyStatus(int keyID);
	//更新秘钥编号
	bool updateKeyId(int keyID);
	//将秘钥信息写入数据库, 使用事务
	bool writeSecKeyWithTrans(NodeSHMInfo* pNode);
	//根据clientID和serverID和keyID查询秘钥状态，返回true表示秘钥有效，false表示无效
	bool checkSecKey(std::string clientID, std::string serverID, int keyID);
	//根据clientID和serverID和keyID更新秘钥状态，返回true表示更新成功，false表示更新失败
	bool logoutSecKey(std::string clientID, std::string serverID, int keyID);
	std::string getCurTime();

private:
	MYSQL* m_conn;
};

