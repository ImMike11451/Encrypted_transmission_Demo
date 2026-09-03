#pragma once

#include <string>

#include "AuditService.h"
#include "MessageRepository.h"
#ifdef DEMO_CMAKE_PROTO
#include <MessageV2.pb.h>
#else
#include "MessageV2.pb.h"
#endif
#include "V2RespondCodec.h"

// MessageQueryService 集中处理消息查询、权限校验和查询审计。
// 它只依赖抽象接口，因此自动化测试不需要连接 MySQL。
class MessageQueryService
{
  public:
    MessageQueryService(const std::string& serverId, IMessageRepository* repository, IMessageAudit* audit);

    V2QueryMessageResponseInfo handleQueryMessage(const secmng::v2::RequestPacket& packet);
    V2QueryMessageListResponseInfo handleQueryMessageList(const secmng::v2::RequestPacket& packet);

  private:
    bool validateQueryRequest(const secmng::v2::RequestPacket& packet, std::string& errorMsg) const;
    bool validateQueryListRequest(const secmng::v2::RequestPacket& packet, std::string& errorMsg) const;
    V2HeaderInfo buildResponseHeader(const secmng::v2::RequestPacket& packet, int command) const;
    std::string generateAuditLogId() const;
    long long parseDateTimeToTimestamp(const std::string& dateTimeStr) const;

    std::string m_serverId;
    IMessageRepository* m_repository;
    IMessageAudit* m_audit;
};
