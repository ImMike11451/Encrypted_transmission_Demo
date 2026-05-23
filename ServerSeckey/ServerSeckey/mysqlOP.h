#pragma once
#include <string>
#include <iostream>
#include <vector>
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
	// 向 message_log 插入消息记录。
	// 新版本同时保存发送方密文和接收方密文，用于支持多客户端消息投递。
	bool insertMessageLog(const std::string& msgId,
		const std::string& senderId,
		const std::string& receiverId,
		int senderKeyId,
		int receiverKeyId,
		const std::string& msgType,
		const std::string& senderCiphertext,
		const std::string& senderNonce,
		const std::string& senderTag,
		const std::string& receiverCiphertext,
		const std::string& receiverNonce,
		const std::string& receiverTag,
		const std::string& algorithm,
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

	// 根据 msg_id 查询单条消息
	bool queryMessageLogById(const std::string& msgId,
		std::string& senderId,
		std::string& receiverId,
		int& keyId,
		std::string& msgType,
		std::string& ciphertext,
		std::string& nonce,
		std::string& tag,
		std::string& sendTime,
		int& status);

	// 根据 sender_id 查询最近 N 条消息
	bool queryRecentMessagesBySender(const std::string& senderId,
		int limit,
		std::vector<std::vector<std::string>>& rows);

private:
	MYSQL* m_conn;
};

