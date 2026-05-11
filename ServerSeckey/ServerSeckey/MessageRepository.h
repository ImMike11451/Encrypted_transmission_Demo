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

// 这个结构体表示“查询单条消息”的结果。
// 它服务于 MessageService 的查询业务，返回的是消息元数据。
struct MessageQueryResult
{
    bool found;                // 是否查到记录
    std::string msgId;         // 服务端消息 ID
    std::string senderId;      // 原发送方
    std::string receiverId;    // 原接收方
    int keyId;                 // 使用的密钥编号
    std::string msgType;       // 消息类型
    std::string ciphertext;    // 密文（当前阶段先查出来，是否返回给客户端由服务层决定）
    std::string nonce;         // nonce
    std::string tag;           // tag
    std::string sendTime;      // 发送时间
    int status;                // 状态
    std::string errorMsg;      // 查询失败时的错误信息
};

// 负责 message_log 表的数据库操作。
// 1. 把消息写入 message_log
// 2. 从 message_log 查询消息
class MessageRepository
{
public:
    // 构造时注入数据库对象。
    // 生命周期仍由外部管理，这样和你现在 mysqlOP 的使用风格一致。
    MessageRepository(mysqlOP* db);

    ~MessageRepository();

    // 向 message_log 插入一条消息记录。
    bool insertMessage(const MessageLogRecord& record);

    // 根据 server_message_id 查询单条消息
	bool queryMessageById(const std::string& msgId, MessageQueryResult& result);

private:
    mysqlOP* m_db;  // 数据库对象，不负责释放
};

