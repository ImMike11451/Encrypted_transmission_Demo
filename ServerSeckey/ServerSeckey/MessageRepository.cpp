#include "MessageRepository.h"
#include "Logger.h"
#include <mysql/mysql.h>
#include <cstdio>

MessageRepository::MessageRepository(mysqlOP* db)
	:m_db(db)
{
}

MessageRepository::~MessageRepository()
{
}

// 插入一条消息记录到 message_log。
bool MessageRepository::insertMessage(const MessageLogRecord& record)
{

	// 这里先做一个基本空指针保护，避免数据库对象未初始化时直接崩溃
	if (m_db == nullptr)
	{
		Logger::error("MessageRepository insert failed: m_db is nullptr");
		return false;
	}

    return m_db->insertMessageLog(
        record.msgId,
        record.senderId,
        record.receiverId,
        record.keyId,
        record.msgType,
        record.ciphertext,
        record.nonce,
        record.tag,
        record.sendTime,
        record.status
    );
}
