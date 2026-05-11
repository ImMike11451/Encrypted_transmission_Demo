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

bool MessageRepository::queryMessageById(const std::string& msgId, MessageQueryResult& result)
{
    // 先把结果对象初始化成“未找到”状态，
    // 避免上层误读到旧值。
    result.found = false;
    result.msgId = msgId;
    result.senderId.clear();
    result.receiverId.clear();
    result.keyId = 0;
    result.msgType.clear();
    result.ciphertext.clear();
    result.nonce.clear();
    result.tag.clear();
    result.sendTime.clear();
    result.status = 0;
    result.errorMsg.clear();

    if(m_db == nullptr)
    {
        result.errorMsg = "m_db is nullptr";
        Logger::error("MessageRepository query failed: m_db is nullptr");
        return false;
	}

    bool ret = m_db->queryMessageLogById(
        msgId,
        result.senderId,
        result.receiverId,
        result.keyId,
        result.msgType,
        result.ciphertext,
        result.nonce,
        result.tag,
        result.sendTime,
        result.status
	);

    if (!ret)
    {
        result.found = false;
        result.errorMsg = "message not found";
        return false;
    }

    result.found = true;
    return true;
}
