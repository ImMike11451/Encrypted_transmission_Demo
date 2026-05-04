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
	// 获得当前时间
	std::string getCurTime();
	// 向 message_log 插入消息记录
	bool insertMessageLog(const std::string& msgId,
		const std::string& senderId,
		const std::string& receiverId,
		int keyId,
		const std::string& msgType,
		const std::string& ciphertext,
		const std::string& nonce,
		const std::string& tag,
		const std::string& sendTime,
		int status);
	// 向 audit_log 插入审计记录
	bool insertAuditLog(const std::string& logId,
		const std::string& nodeId,
		const std::string& action,
		const std::string& targetId,
		int result,
		const std::string& detail,
		const std::string& createTime);

private:
	MYSQL* m_conn;
};

