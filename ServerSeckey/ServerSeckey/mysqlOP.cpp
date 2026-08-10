#include "mysqlOP.h"

#include "Logger.h"

#include <ctime>
#include <sstream>

mysqlOP::mysqlOP()
	: m_conn(nullptr)
{
}

mysqlOP::~mysqlOP()
{
	if (m_conn != nullptr)
	{
		mysql_close(m_conn);
		m_conn = nullptr;
	}
}

bool mysqlOP::connectDb(const std::string& host,
	const std::string& user,
	const std::string& password,
	const std::string& dbName)
{
	m_conn = mysql_init(nullptr);
	if (m_conn == nullptr)
	{
		Logger::error("初始化数据库连接失败");
		return false;
	}

	if (mysql_real_connect(m_conn, host.c_str(), user.c_str(), password.c_str(), dbName.c_str(), 3306, nullptr, 0) == nullptr)
	{
		Logger::error("连接数据库失败: " + std::string(mysql_error(m_conn)));
		mysql_close(m_conn);
		m_conn = nullptr;
		return false;
	}

	return true;
}

std::string mysqlOP::escapeSqlString(const std::string& value) const
{
	if (m_conn == nullptr)
	{
		return "";
	}

	std::string escaped(value.size() * 2 + 1, '\0');
	const unsigned long escapedLength = mysql_real_escape_string(
		m_conn,
		&escaped[0],
		value.data(),
		static_cast<unsigned long>(value.size()));
	escaped.resize(escapedLength);
	return escaped;
}

bool mysqlOP::writeSecKey(NodeSHMInfo* pNode)
{
	if (pNode == nullptr)
	{
		Logger::error("写入密钥失败: 密钥节点为空");
		return false;
	}

	std::ostringstream sql;
	sql << "insert into seckeyinfo(clientid,serverid,keyid,createtime,state,seckey) values('"
		<< escapeSqlString(pNode->clientID) << "','"
		<< escapeSqlString(pNode->serverID) << "',"
		<< pNode->seckeyID << ",'"
		<< escapeSqlString(getCurTime()) << "',1,'"
		<< escapeSqlString(pNode->seckey) << "')";

	if (mysql_query(m_conn, sql.str().c_str()))
	{
		Logger::error("执行查询失败: " + std::string(mysql_error(m_conn)));
		return false;
	}

	return true;
}

int mysqlOP::getKeyId()
{
	std::string sql = "select ikeysn from keysn";
	//执行sql语句
	if (mysql_query(m_conn,sql.c_str())) return -1;
	//获取结果集,保存到内存中，返回一个结果集指针
	MYSQL_RES* res = mysql_store_result(m_conn);
	//从结果集中获取下一行，返回一个字符串数组
	MYSQL_ROW row = mysql_fetch_row(res);
	int keyID = -1;
	//如果row不为空，说明查询到了数据，将第一列的值转换为整数，赋值给keyID
	if(row)
		keyID = atoi(row[0]);
	//释放结果集占用的内存
	mysql_free_result(res);

	return keyID;
}

bool mysqlOP::updateKeyId(int keyID)
{
	//开启事务
	mysql_query(m_conn, "start transaction");
	//执行查询语句，锁定行
	std::string sql = "select ikeysn from keysn for update";
	if(mysql_query(m_conn, sql.c_str()))
	{
		Logger::error("执行查询失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}

	//获取结果集
	MYSQL_RES* res = mysql_store_result(m_conn);
	MYSQL_ROW row = mysql_fetch_row(res);
	if(!row)
	{
		Logger::error("获取行失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}
	//当前keyID
	keyID = atoi(row[0]);
	mysql_free_result(res);
	std::string updateSql = "update keysn set ikeysn = ikeysn + 1";
	if(mysql_query(m_conn, updateSql.c_str()))
	{
		Logger::error("执行更新失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}
	//提交事务
	mysql_query(m_conn, "commit");

	return true;
}

bool mysqlOP::writeSecKeyWithTrans(NodeSHMInfo* pNode)
{
	if (pNode == nullptr)
	{
		Logger::error("写入密钥失败: 密钥节点为空");
		return false;
	}

	// 开启事务后用 select ... for update 锁住 keysn。
	// 这样多个客户端同时协商密钥时，不会拿到相同的 key_id。
	mysql_query(m_conn, "start transaction");

	std::string sql = "select ikeysn from keysn for update";
	if (mysql_query(m_conn, sql.c_str()))
	{
		Logger::error("执行查询失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}

	MYSQL_RES* res = mysql_store_result(m_conn);
	MYSQL_ROW row = mysql_fetch_row(res);

	if (!row)
	{
		mysql_free_result(res);
		mysql_query(m_conn, "rollback");
		return false;
	}

	int keyID = atoi(row[0]);
	mysql_free_result(res);

	pNode->seckeyID = keyID;

	std::ostringstream sqlInsert;
	sqlInsert << "insert into seckeyinfo(clientid,serverid,keyid,createtime,state,seckey) values('"
		<< escapeSqlString(pNode->clientID) << "','"
		<< escapeSqlString(pNode->serverID) << "',"
		<< pNode->seckeyID << ",'"
		<< escapeSqlString(getCurTime()) << "',1,'"
		<< escapeSqlString(pNode->seckey) << "')";

	if (mysql_query(m_conn, sqlInsert.str().c_str()))
	{
		Logger::error("执行查询失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}

	std::string updateSql = "update keysn set ikeysn = ikeysn + 1";
	if (mysql_query(m_conn, updateSql.c_str()))
	{
		Logger::error("执行更新失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}

	mysql_query(m_conn, "commit");

	return true;
}

bool mysqlOP::checkSecKey(const std::string& clientID, const std::string& serverID, int keyID)
{
	// 只校验 key 是否存在且有效，不把 seckey 本体返回给上层。
	// 消息解密仍依赖共享内存中的活跃 key。
	std::ostringstream sql;
	sql << "select keyid, state from seckeyinfo where clientid = '"
		<< escapeSqlString(clientID) << "' and serverid = '"
		<< escapeSqlString(serverID) << "' and keyid = " << keyID;
	if (mysql_query(m_conn, sql.str().c_str()))
	{
		Logger::error("执行查询失败: " + std::string(mysql_error(m_conn)));
		return false;
	}

	MYSQL_RES* res = mysql_store_result(m_conn);
	if(res == NULL)
	{
		Logger::error("获取结果集失败: " + std::string(mysql_error(m_conn)));
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if(row == NULL)
	{
		mysql_free_result(res);
		return false;
	}
	int dbKeyID = atoi(row[0]);
	int state = atoi(row[1]);

	mysql_free_result(res);

	if(dbKeyID == keyID && state == 1)
	{
		return true;
	}
	else
	{
		return false;
	}

}

bool mysqlOP::logoutSecKey(const std::string& clientID, const std::string& serverID, int keyID)
{
	// 注销并不删除历史密钥记录，而是把 state 置为 0。
	// 这样可以保留审计和排查所需的历史状态。
	std::ostringstream sql;
	sql << "update seckeyinfo set state = 0 where clientid = '"
		<< escapeSqlString(clientID) << "' and serverid = '"
		<< escapeSqlString(serverID) << "' and keyid = " << keyID << " and state = 1";
	if (mysql_query(m_conn, sql.str().c_str()))
	{
		Logger::error("执行更新失败: " + std::string(mysql_error(m_conn)));
		return false;
	}
	// mysql_affected_rows == 0 说明没有更新到记录
	my_ulonglong rows = mysql_affected_rows(m_conn);
	if (rows == 0)
	{
		return false;
	}

	return true;

}

std::string mysqlOP::getCurTime()
{
	time_t now = time(0);
	char buf[64]{ 0 };
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
	return buf;
}

bool mysqlOP::insertMessageLog(const std::string& msgId,
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
	int status)
{
	// message_log 同时保存发送方原始密文和接收方重加密密文。
	// 这样既能保留入站记录，也能支持接收方后续按自己的会话密钥解密。
	std::ostringstream sql;
	sql << "insert into message_log("
		"msg_id,sender_id,receiver_id,sender_key_id,receiver_key_id,msg_type,"
		"sender_ciphertext,sender_nonce,sender_tag,receiver_ciphertext,receiver_nonce,receiver_tag,"
		"algorithm,send_time,status) values('"
		<< escapeSqlString(msgId) << "','"
		<< escapeSqlString(senderId) << "','"
		<< escapeSqlString(receiverId) << "',"
		<< senderKeyId << ',' << receiverKeyId << ",'"
		<< escapeSqlString(msgType) << "','"
		<< escapeSqlString(senderCiphertext) << "','"
		<< escapeSqlString(senderNonce) << "','"
		<< escapeSqlString(senderTag) << "','"
		<< escapeSqlString(receiverCiphertext) << "','"
		<< escapeSqlString(receiverNonce) << "','"
		<< escapeSqlString(receiverTag) << "','"
		<< escapeSqlString(algorithm) << "','"
		<< escapeSqlString(sendTime) << "'," << status << ')';
	if (mysql_query(m_conn, sql.str().c_str()))
	{
		Logger::error("insertMessageLog failed: " + std::string(mysql_error(m_conn)));
		return false;
	}
	return true;
}

bool mysqlOP::insertAuditLog(const std::string& logId,
	const std::string& nodeId,
	const std::string& action,
	const std::string& targetId,
	int result,
	const std::string& detail,
	const std::string& createTime)
{
	// audit_log 记录业务行为结果，不应该包含密钥、明文或完整 SQL。
	std::ostringstream sql;
	sql << "insert into audit_log(log_id,node_id,action,target_id,result,detail,create_time) values('"
		<< escapeSqlString(logId) << "',";
	if (nodeId.empty())
	{
		sql << "NULL";
	}
	else
	{
		sql << '\'' << escapeSqlString(nodeId) << '\'';
	}
	sql << ",'" << escapeSqlString(action) << "','"
		<< escapeSqlString(targetId) << "'," << result << ",'"
		<< escapeSqlString(detail) << "','"
		<< escapeSqlString(createTime) << "')";

	if (mysql_query(m_conn, sql.str().c_str()))
	{
		Logger::error("insertAuditLog failed: " + std::string(mysql_error(m_conn)));
		return false;
	}

	return true;
}

bool mysqlOP::queryMessageLogById(const std::string& msgId, std::string& senderId, std::string& receiverId, int& keyId, std::string& msgType, std::string& ciphertext, std::string& nonce, std::string& tag, std::string& sendTime, int& status)
{
	// 按 msg_id 查询单条消息记录。
	// 权限校验不在这里做，由 MessageService 根据请求方身份统一判断。
	std::ostringstream sql;
	sql << "select sender_id, receiver_id, sender_key_id, msg_type, "
		"sender_ciphertext, sender_nonce, sender_tag, send_time, status "
		"from message_log where msg_id = '" << escapeSqlString(msgId) << "'";

	if (mysql_query(m_conn, sql.str().c_str()))
	{
		Logger::error("queryMessageLogById failed: " + std::string(mysql_error(m_conn)));
		return false;
	}

	//将查询结果保存到内存中，返回一个结果集指针
	MYSQL_RES* res = mysql_store_result(m_conn);
	if (res == nullptr)
	{
		Logger::error("queryMessageLogById get result failed: " + std::string(mysql_error(m_conn)));
		return false;
	}

	//将结果集中获取下一行，返回一个字符串数组
	MYSQL_ROW row = mysql_fetch_row(res);
	if(row == nullptr)
	{
		// 没查到数据不算 SQL 执行失败，但本函数先统一返回 false，
		// 由上层 Repository 再决定 errorMsg 如何描述。
		mysql_free_result(res);
		return false;
	}

	// 结果列顺序要和 SQL select 顺序严格一致
	senderId = row[0] ? row[0] : "";
	receiverId = row[1] ? row[1] : "";
	keyId = row[2] ? atoi(row[2]) : 0;
	msgType = row[3] ? row[3] : "";
	ciphertext = row[4] ? row[4] : "";
	nonce = row[5] ? row[5] : "";
	tag = row[6] ? row[6] : "";
	sendTime = row[7] ? row[7] : "";
	status = row[8] ? atoi(row[8]) : 0;

	mysql_free_result(res);
	
	return true;
}

bool mysqlOP::queryRecentMessagesBySender(const std::string& senderId, int limit, std::vector<std::vector<std::string>>& rows)
{
	rows.clear();

	// limit 不合法时直接失败，避免查出异常数量数据
	if (limit <= 0)
	{
		Logger::error("queryRecentMessagesBySender failed: invalid limit");
		return false;
	}

	// 这里按 send_time 倒序查最近 N 条消息。
	// 当前阶段只查消息摘要相关字段，不查 ciphertext / nonce / tag，
	// 因为列表页主要用于展示元数据。
	std::ostringstream sql;
	sql << "select msg_id, sender_id, receiver_id, sender_key_id, msg_type, send_time, status "
		"from message_log where sender_id = '" << escapeSqlString(senderId)
		<< "' order by send_time desc limit " << limit;

	if (mysql_query(m_conn, sql.str().c_str()))
	{
		Logger::error("queryRecentMessagesBySender failed: " + std::string(mysql_error(m_conn)));
		return false;
	}

	MYSQL_RES* res = mysql_store_result(m_conn);
	if (res == nullptr)
	{
		Logger::error("queryRecentMessagesBySender get result failed: " + std::string(mysql_error(m_conn)));
		return false;
	}

	MYSQL_ROW row = nullptr;

	// 逐行读取结果集
	while ((row = mysql_fetch_row(res)) != nullptr)
	{
		std::vector<std::string> oneRow;

		// select 出来的列顺序必须和下面读取顺序一致
		oneRow.push_back(row[0] ? row[0] : ""); // msg_id
		oneRow.push_back(row[1] ? row[1] : ""); // sender_id
		oneRow.push_back(row[2] ? row[2] : ""); // receiver_id
		oneRow.push_back(row[3] ? row[3] : ""); // key_id
		oneRow.push_back(row[4] ? row[4] : ""); // msg_type
		oneRow.push_back(row[5] ? row[5] : ""); // send_time
		oneRow.push_back(row[6] ? row[6] : ""); // status

		rows.push_back(oneRow);
	}

	mysql_free_result(res);
	return true;
}

