#include "AuditService.h"
#include "Logger.h"

AuditService::AuditService(mysqlOP* db)
    : m_db(db)
{
}

AuditService::~AuditService()
{
}

// 写入一条完整的审计记录
bool AuditService::logAction(const AuditLogRecord& record)
{
    if (m_db == nullptr)
    {
        Logger::error("AuditService logAction failed: m_db is nullptr");
        return false;
    }

    // 和 MessageRepository 一样，这里建议继续走 mysqlOP 提供的专用接口，
    // 而不是让业务模块直接操作底层 MYSQL*。
    return m_db->insertAuditLog(
        record.logId,
        record.nodeId,
        record.action,
        record.targetId,
        record.result,
        record.detail,
        record.createTime
    );
}

// 辅助重载版本：方便 MessageService 直接按字段写审计记录
bool AuditService::logAction(const std::string& logId,
    const std::string& nodeId,
    const std::string& action,
    const std::string& targetId,
    int result,
    const std::string& detail,
    const std::string& createTime)
{
    AuditLogRecord record;
    record.logId = logId;
    record.nodeId = nodeId;
    record.action = action;
    record.targetId = targetId;
    record.result = result;
    record.detail = detail;
    record.createTime = createTime;

    return logAction(record);
}
