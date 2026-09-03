#pragma once
#include <string>
#include <vector>

class mysqlOP;

// 这个结构体表示一条准备写入 message_log 的消息记录。
// 它是“服务层”和“存储层”之间传递的数据对象。
//
// 新版本为了支持 A -> Server -> B 的多客户端投递，保存两份密文：
// 1. senderCiphertext：A 使用 A-Server 会话密钥加密后发给服务端的密文。
// 2. receiverCiphertext：服务端解密后，再使用 B-Server 会话密钥重加密的密文。
// 这样 B 后续拉取消息时，就能用自己的 B-Server 密钥解密 receiverCiphertext。
struct MessageLogRecord
{
    std::string msgId;      // 服务端生成的消息唯一 ID
    std::string senderId;   // 原始发送方节点 ID
    std::string receiverId; // 原始接收方节点 ID

    int senderKeyId;   // 发送方与服务端之间的会话密钥 ID
    int receiverKeyId; // 接收方与服务端之间的会话密钥 ID

    std::string msgType; // 消息类型，例如 text

    std::string senderCiphertext; // 发送方发给服务端的 base64 密文
    std::string senderNonce;      // 发送方加密时使用的 base64 nonce
    std::string senderTag;        // 发送方加密时生成的 base64 GCM tag

    std::string receiverCiphertext; // 服务端给接收方重加密后的 base64 密文
    std::string receiverNonce;      // 服务端给接收方重加密时使用的 base64 nonce
    std::string receiverTag;        // 服务端给接收方重加密时生成的 base64 GCM tag

    std::string algorithm; // 加密算法，例如 AES-128-GCM
    std::string sendTime;  // 发送时间，字符串形式，便于直接拼 SQL
    int status;            // 状态：0处理中 1成功 2失败
};

// 这个结构体表示“查询单条消息”的结果。
// 它服务于 MessageService 的查询业务，返回的是消息元数据。
struct MessageQueryResult
{
    bool found;             // 是否查到记录
    std::string msgId;      // 服务端消息 ID
    std::string senderId;   // 原发送方
    std::string receiverId; // 原接收方
    int keyId;              // 使用的密钥编号
    std::string msgType;    // 消息类型
    std::string ciphertext; // 密文（当前阶段先查出来，是否返回给客户端由服务层决定）
    std::string nonce;      // nonce
    std::string tag;        // tag
    std::string sendTime;   // 发送时间
    int status;             // 状态
    std::string errorMsg;   // 查询失败时的错误信息
};

// 单条消息摘要
// 这是“列表查询”时每一条记录的结构化表示。
struct MessageSummaryInfo
{
    std::string msgId;      // 服务端消息 ID
    std::string senderId;   // 发送方
    std::string receiverId; // 接收方
    int keyId;              // 使用的密钥编号
    std::string msgType;    // 消息类型
    std::string sendTime;   // 数据库里的时间字符串
    int status;             // 消息状态
};

// 负责 message_log 表的数据库操作。
// Repository 层只做数据访问和结构转换，不做权限判断。
// 这样权限规则可以集中放在 MessageService，避免存储层混入业务上下文。
// 1. 把消息写入 message_log
// 2. 从 message_log 查询消息
// 消息仓储接口是查询业务与持久化实现之间的接缝。
// 生产环境使用 MySQL 适配器，自动化测试使用内存适配器。
class IMessageRepository
{
  public:
    virtual ~IMessageRepository() = default;

    virtual bool insertMessage(const MessageLogRecord& record) = 0;
    virtual bool queryMessageById(const std::string& msgId, MessageQueryResult& result) = 0;
    virtual bool queryRecentMessagesBySender(const std::string& senderId, int limit,
                                             std::vector<MessageSummaryInfo>& results) = 0;
};

class MessageRepository final : public IMessageRepository
{
  public:
    // 构造时注入数据库对象。
    // 生命周期仍由外部管理，这样和你现在 mysqlOP 的使用风格一致。
    MessageRepository(mysqlOP* db);

    ~MessageRepository() override;

    // 向 message_log 插入一条消息记录。
    bool insertMessage(const MessageLogRecord& record) override;

    // 根据 server_message_id 查询单条消息
    bool queryMessageById(const std::string& msgId, MessageQueryResult& result) override;

    // 根据 sender_id 查询最近 N 条消息
    bool queryRecentMessagesBySender(const std::string& senderId, int limit,
                                     std::vector<MessageSummaryInfo>& results) override;

  private:
    mysqlOP* m_db; // 数据库对象，不负责释放
};
