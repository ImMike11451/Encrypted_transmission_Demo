#pragma once
#include <string>

class mysqlOP;

// 这个结构体表示一条审计日志记录。
// 审计日志和消息记录不同，它更关注“发生了什么动作、结果如何”。
struct AuditLogRecord
{
    std::string logId;      // 审计日志唯一 ID
    std::string nodeId;     // 操作节点 ID，例如发送方 clientId
    std::string action;     // 动作类型，例如 MSG_SEND / MSG_DECRYPT
    std::string targetId;   // 目标 ID，例如 message_id 或 key_id
    int result;             // 结果：0失败 1成功
    std::string detail;     // 详细说明
    std::string createTime; // 记录时间
};

// AuditService 负责把审计信息写入 audit_log。
// 审计日志用于回答“谁在什么时候对什么目标做了什么，结果如何”。
// 失败请求也应该记录，这对排查非法请求、权限拒绝和解密失败很重要。
// 消息审计接口隐藏数据库时间和持久化细节。
// 查询业务只描述需要记录的事件，不直接依赖 mysqlOP。
class IMessageAudit
{
  public:
    virtual ~IMessageAudit() = default;

    virtual bool logAction(const std::string& logId, const std::string& nodeId, const std::string& action,
                           const std::string& targetId, int result, const std::string& detail) = 0;
};

class AuditService final : public IMessageAudit
{
  public:
    AuditService(mysqlOP* db);
    ~AuditService() override;

    // 写入一条完整的审计记录
    bool logAction(const AuditLogRecord& record);

    // 查询模块使用此接口，由生产适配器负责补充数据库时间。
    bool logAction(const std::string& logId, const std::string& nodeId, const std::string& action,
                   const std::string& targetId, int result, const std::string& detail) override;

    // 一个方便调用的辅助接口：
    // 直接传关键字段，让调用方不用每次手动组装完整结构体。
    bool logAction(const std::string& logId, const std::string& nodeId, const std::string& action,
                   const std::string& targetId, int result, const std::string& detail, const std::string& createTime);

  private:
    mysqlOP* m_db; // 数据库对象，不负责释放
};
