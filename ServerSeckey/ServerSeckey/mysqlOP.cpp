#include "mysqlOP.h"
#include "Logger.h"

mysqlOP::mysqlOP()
{

}

mysqlOP::~mysqlOP()
{
}

bool mysqlOP::connectDb(std::string host,std::string user, std::string password, std::string dbName)
{
	m_conn = mysql_init(NULL);

	if(!mysql_real_connect(m_conn, host.c_str(), user.c_str(), password.c_str(), dbName.c_str(), 3306, NULL, 0))
	{
		Logger::error("连接数据库失败: " + std::string(mysql_error(m_conn)));
		return false;
	}
	Logger::info("连接数据库成功!");
	return true;
}

bool mysqlOP::writeSecKey(NodeSHMInfo* pNode)
{
	char sql[1024]{ 0 };
	sprintf(sql, "insert into seckeyinfo(clientid,serverid,keyid,createtime,state,seckey)"
		"values('%s','%s',%d,'%s',%d,'%s')",
		pNode->clientID,
		pNode->serverID,
		pNode->seckeyID,
		getCurTime().c_str(),
		1,
		pNode->seckey);

	if(mysql_query(m_conn,sql))
	{
		Logger::info("插入 SQL: " + std::string(sql));
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
	// 1️⃣ 开启事务
	mysql_query(m_conn, "start transaction");

	// 2️⃣ 加锁读取 keyID
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

	// 3️⃣ 设置到节点
	pNode->seckeyID = keyID;

	// 4️⃣ 插入 seckeyinfo
	char sqlInsert[1024]{ 0 };
	sprintf(sqlInsert,
		"insert into seckeyinfo(clientid,serverid,keyid,createtime,state,seckey) "
		"values('%s','%s',%d,'%s',%d,'%s')",
		pNode->clientID,
		pNode->serverID,
		pNode->seckeyID,
		getCurTime().c_str(),
		1,
		pNode->seckey);

	if (mysql_query(m_conn, sqlInsert))
	{
		Logger::info("插入 SQL: " + std::string(sqlInsert));
		Logger::error("执行查询失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}

	// 5️⃣ keyID 自增
	std::string updateSql = "update keysn set ikeysn = ikeysn + 1";
	if (mysql_query(m_conn, updateSql.c_str()))
	{
		Logger::error("执行更新失败: " + std::string(mysql_error(m_conn)));
		mysql_query(m_conn, "rollback");
		return false;
	}

	// 6️⃣ 提交事务
	mysql_query(m_conn, "commit");

	return true;
}

bool mysqlOP::checkSecKey(std::string clientID, std::string serverID, int keyID)
{
	char sql[1024]{ 0 };
	sprintf(sql, "select keyid, state from seckeyinfo "
		"where clientid = '%s' and serverid = '%s' and keyid = % d",
		clientID.data(), serverID.data(), keyID);
	if(mysql_query(m_conn, sql))
	{
		Logger::info("checkSecKey sql : " + std::string(sql));
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
		Logger::info("No matching key found.");
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
		Logger::info("Key ID or state does not match.");
		return false;
	}

}

bool mysqlOP::logoutSecKey(std::string clientID, std::string serverID, int keyID)
{
	char sql[1024]{ 0 };
	sprintf(sql, "update seckeyinfo set state = 0 "
		"where clientid = '%s' and serverid = '%s' and keyid = %d and state = 1",
		clientID.data(), serverID.data(), keyID);
	if (mysql_query(m_conn,sql))
	{
		Logger::info("logoutSecKey sql : " + std::string(sql));
		Logger::error("执行更新失败: " + std::string(mysql_error(m_conn)));
		return false;
	}
	// mysql_affected_rows == 0 说明没有更新到记录
	my_ulonglong rows = mysql_affected_rows(m_conn);
	if (rows == 0)
	{
		Logger::info("没有匹配到可注销的密钥记录...");
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
