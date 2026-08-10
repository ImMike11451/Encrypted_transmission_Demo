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
// 这里不直接拼 SQL，而是继续走 mysqlOP，保持当前项目的数据访问入口集中。
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
        record.senderKeyId,
        record.receiverKeyId,
        record.msgType,
        record.senderCiphertext,
        record.senderNonce,
        record.senderTag,
        record.receiverCiphertext,
        record.receiverNonce,
        record.receiverTag,
        record.algorithm,
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
bool MessageRepository::queryRecentMessagesBySender(const std::string& senderId,
    int limit,
    std::vector<MessageSummaryInfo>& results)
{
    results.clear();

    if (m_db == nullptr)
    {
        Logger::error("MessageRepository queryRecentMessagesBySender failed: m_db is nullptr");
        return false;
    }

    // 先从 mysqlOP 拿到底层原始行数据，再转换成服务层更容易使用的结构体。
    // 这里不做权限过滤，因为 Repository 不知道当前请求者是谁。
    std::vector<std::vector<std::string>> rows;
    bool ret = m_db->queryRecentMessagesBySender(senderId, limit, rows);
    if (!ret)
    {
        return false;
    }

    // 把原始行数据转换成结构化结果
    for (const auto& row : rows)
    {
        // 每一行应该有 7 列：
        // msg_id, sender_id, receiver_id, sender_key_id, msg_type, send_time, status
        if (row.size() != 7)
        {
            Logger::error("MessageRepository queryRecentMessagesBySender: unexpected row size");
            continue;
        }

        MessageSummaryInfo info;
        info.msgId = row[0];
        info.senderId = row[1];
        info.receiverId = row[2];
        info.keyId = row[3].empty() ? 0 : atoi(row[3].c_str());
        info.msgType = row[4];
        info.sendTime = row[5];
        info.status = row[6].empty() ? 0 : atoi(row[6].c_str());

        results.push_back(info);
    }

    return true;
}

