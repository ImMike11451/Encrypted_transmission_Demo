#pragma once
#include <vector>
#include <string>

#include <mysql/mysql.h>

#include "SeckKeyNodeInfo.h"

class mysqlOP
{

public:
	mysqlOP();
	~mysqlOP();

	// 建立 MySQL 连接；连接对象由 mysqlOP 统一释放。
	bool connectDb(const std::string& host,
		const std::string& user,
		const std::string& password,
		const std::string& dbName);
	//将秘钥信息写入数据库
	bool writeSecKey(NodeSHMInfo* pNode);
	//从数据库中获取秘钥编号
	int getKeyId();
	//根据秘钥ID获取秘钥状态，返回-1表示查询失败，其他为state值
	//int getKeyStatus(int keyID);
	//更新秘钥编号
	bool updateKeyId(int keyID);
	//将秘钥信息写入数据库, 使用事务
	bool writeSecKeyWithTrans(NodeSHMInfo* pNode);
	//根据clientID和serverID和keyID查询秘钥状态，返回true表示秘钥有效，false表示无效
	bool checkSecKey(const std::string& clientID, const std::string& serverID, int keyID);
	//根据clientID和serverID和keyID更新秘钥状态，返回true表示更新成功，false表示更新失败
	bool logoutSecKey(const std::string& clientID, const std::string& serverID, int keyID);
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
	// 将任意长度字符串转为可安全放入单引号 SQL 字面量的内容。
	// 使用长度参数，保证密文中的 '\0' 不会截断或绕过转义。
	std::string escapeSqlString(const std::string& value) const;

	MYSQL* m_conn;
};

