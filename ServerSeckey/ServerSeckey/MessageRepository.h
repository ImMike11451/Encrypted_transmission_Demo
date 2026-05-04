#pragma once
#include "mysqlOP.h"
#include <string>

// 这个结构体表示一条准备写入 message_log 的消息记录。
// 它是“服务层”和“存储层”之间传递的数据对象。
struct MessageLogRecord
{
    std::string msgId;         // 服务端生成的消息唯一 ID
    std::string senderId;      // 发送方节点 ID
    std::string receiverId;    // 接收方节点 ID
    int keyId;                 // 使用的密钥编号
    std::string msgType;       // 消息类型，例如 text
    std::string ciphertext;    // base64 编码后的密文
    std::string nonce;         // base64 编码后的 nonce / IV
    std::string tag;           // base64 编码后的认证标签，第一阶段可能为空
    std::string sendTime;      // 发送时间，字符串形式，便于直接拼 SQL
    int status;                // 状态：0处理中 1成功 2失败
};

// MessageRepository 的职责非常单一：
// 负责 message_log 表的数据库操作。
// 第一阶段先只做“插入消息记录”这一件事。
class MessageRepository
{
public:
    // 构造时注入数据库对象。
    // 生命周期仍由外部管理，这样和你现在 mysqlOP 的使用风格一致。
    MessageRepository(mysqlOP* db);

    ~MessageRepository();

    // 向 message_log 插入一条消息记录。
    bool insertMessage(const MessageLogRecord& record);

private:
    mysqlOP* m_db;  // 数据库对象，不负责释放
};

