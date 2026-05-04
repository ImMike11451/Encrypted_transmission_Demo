#pragma once
#include "mysqlOP.h"
#include <string>

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
class AuditService
{
public:
    AuditService(mysqlOP* db);
    ~AuditService();

    // 写入一条完整的审计记录
    bool logAction(const AuditLogRecord& record);

    // 一个方便调用的辅助接口：
// 直接传关键字段，让调用方不用每次手动组装完整结构体。
    bool logAction(const std::string& logId,
        const std::string& nodeId,
        const std::string& action,
        const std::string& targetId,
        int result,
        const std::string& detail,
        const std::string& createTime);

private:
    mysqlOP* m_db;   // 数据库对象，不负责释放
};

