#include "MessageQueryService.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "Logger.h"

namespace
{
std::string generateQueryAuditId(const std::string& serverId)
{
    static std::atomic<unsigned long long> sequence{0};
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

    return serverId + "_audit_" + std::to_string(micros) + "_" + std::to_string(++sequence);
}
} // namespace

MessageQueryService::MessageQueryService(const std::string& serverId, IMessageRepository* repository,
                                         IMessageAudit* audit)
    : m_serverId(serverId), m_repository(repository), m_audit(audit)
{
}

V2QueryMessageResponseInfo MessageQueryService::handleQueryMessage(const secmng::v2::RequestPacket& packet)
{
    V2QueryMessageResponseInfo response{};
    response.header = buildResponseHeader(packet, secmng::v2::CMD_QUERY_MSG_RESP);
    response.code = secmng::v2::RESULT_FAILED;
    response.message = "query message failed";

    std::string validationError;
    if (!validateQueryRequest(packet, validationError))
    {
        response.code = secmng::v2::RESULT_INVALID_REQUEST;
        response.message = validationError;
        m_audit->logAction(generateAuditLogId(), packet.has_header() ? packet.header().sender_id() : "", "MSG_QUERY",
                           "", 0, "invalid query request: " + validationError);
        return response;
    }

    const secmng::v2::Header& header = packet.header();
    const secmng::v2::QueryMessageRequest& request = packet.query_msg_req();

    MessageQueryResult queryResult{};
    const bool querySucceeded = m_repository->queryMessageById(request.server_message_id(), queryResult);
    if (!querySucceeded || !queryResult.found)
    {
        response.code = secmng::v2::RESULT_MSG_NOT_FOUND;
        response.message = "message not found";
        response.serverMessageId = request.server_message_id();
        m_audit->logAction(generateAuditLogId(), header.sender_id(), "MSG_QUERY", request.server_message_id(), 0,
                           "message not found");
        return response;
    }

    if (header.sender_id() != queryResult.senderId && header.sender_id() != queryResult.receiverId)
    {
        response.code = secmng::v2::RESULT_INVALID_REQUEST;
        response.message = "permission denied";
        response.serverMessageId = request.server_message_id();
        m_audit->logAction(generateAuditLogId(), header.sender_id(), "MSG_QUERY", request.server_message_id(), 0,
                           "permission denied");
        return response;
    }

    response.code = secmng::v2::RESULT_SUCCESS;
    response.message = "query message success";
    response.serverMessageId = queryResult.msgId;
    response.senderId = queryResult.senderId;
    response.receiverId = queryResult.receiverId;
    response.keyId = queryResult.keyId;
    response.msgType = queryResult.msgType;
    response.sendTime = parseDateTimeToTimestamp(queryResult.sendTime);
    response.status = queryResult.status;

    m_audit->logAction(generateAuditLogId(), header.sender_id(), "MSG_QUERY", request.server_message_id(), 1,
                       "query message success");
    return response;
}

V2QueryMessageListResponseInfo MessageQueryService::handleQueryMessageList(const secmng::v2::RequestPacket& packet)
{
    V2QueryMessageListResponseInfo response{};
    response.header = buildResponseHeader(packet, secmng::v2::CMD_QUERY_MSG_LIST_RESP);
    response.code = secmng::v2::RESULT_FAILED;
    response.message = "query message list failed";

    std::string validationError;
    if (!validateQueryListRequest(packet, validationError))
    {
        response.code = secmng::v2::RESULT_INVALID_REQUEST;
        response.message = validationError;
        m_audit->logAction(generateAuditLogId(), packet.has_header() ? packet.header().sender_id() : "",
                           "MSG_LIST_QUERY", "", 0, "invalid query list request: " + validationError);
        return response;
    }

    const secmng::v2::Header& header = packet.header();
    const secmng::v2::QueryMessageListRequest& request = packet.query_msg_list_req();
    if (request.sender_id() != header.sender_id())
    {
        response.code = secmng::v2::RESULT_INVALID_REQUEST;
        response.message = "permission denied";
        m_audit->logAction(generateAuditLogId(), header.sender_id(), "MSG_LIST_QUERY", request.sender_id(), 0,
                           "permission denied");
        return response;
    }

    std::vector<MessageSummaryInfo> storedMessages;
    if (!m_repository->queryRecentMessagesBySender(request.sender_id(), request.limit(), storedMessages))
    {
        response.code = secmng::v2::RESULT_FAILED;
        response.message = "query recent messages failed";
        m_audit->logAction(generateAuditLogId(), header.sender_id(), "MSG_LIST_QUERY", request.sender_id(), 0,
                           "query recent messages failed");
        return response;
    }

    for (const MessageSummaryInfo& item : storedMessages)
    {
        V2MessageSummaryInfo summary{};
        summary.serverMessageId = item.msgId;
        summary.senderId = item.senderId;
        summary.receiverId = item.receiverId;
        summary.keyId = item.keyId;
        summary.msgType = item.msgType;
        summary.sendTime = parseDateTimeToTimestamp(item.sendTime);
        summary.status = item.status;
        response.messages.push_back(summary);
    }

    m_audit->logAction(generateAuditLogId(), header.sender_id(), "MSG_LIST_QUERY", request.sender_id(), 1,
                       "query message list success, count = " + std::to_string(response.messages.size()));
    response.code = secmng::v2::RESULT_SUCCESS;
    response.message = "query message list success";
    return response;
}

bool MessageQueryService::validateQueryRequest(const secmng::v2::RequestPacket& packet, std::string& errorMsg) const
{
    if (!packet.has_header())
    {
        errorMsg = "header is empty";
        return false;
    }

    if (packet.header().sender_id().empty())
    {
        errorMsg = "header sender_id is empty";
        return false;
    }

    if (!packet.has_query_msg_req())
    {
        errorMsg = "query_msg_req is empty";
        return false;
    }

    if (packet.query_msg_req().server_message_id().empty())
    {
        errorMsg = "server_message_id is empty";
        return false;
    }

    return true;
}

bool MessageQueryService::validateQueryListRequest(const secmng::v2::RequestPacket& packet, std::string& errorMsg) const
{
    if (!packet.has_header())
    {
        errorMsg = "header is empty";
        return false;
    }

    if (!packet.has_query_msg_list_req())
    {
        errorMsg = "query_msg_list_req is empty";
        return false;
    }

    const secmng::v2::Header& header = packet.header();
    const secmng::v2::QueryMessageListRequest& request = packet.query_msg_list_req();
    if (header.sender_id().empty())
    {
        errorMsg = "header sender_id is empty";
        return false;
    }

    if (request.sender_id().empty())
    {
        errorMsg = "query sender_id is empty";
        return false;
    }

    if (request.limit() <= 0)
    {
        errorMsg = "limit must be greater than 0";
        return false;
    }

    if (request.limit() > 100)
    {
        errorMsg = "limit must not exceed 100";
        return false;
    }

    return true;
}

V2HeaderInfo MessageQueryService::buildResponseHeader(const secmng::v2::RequestPacket& packet, int command) const
{
    V2HeaderInfo header{};
    header.command = command;
    header.senderId = m_serverId;
    header.timestamp = static_cast<long long>(std::time(nullptr));

    if (packet.has_header())
    {
        header.messageId = packet.header().message_id();
        header.receiverId = packet.header().sender_id();
    }

    return header;
}

std::string MessageQueryService::generateAuditLogId() const
{
    return generateQueryAuditId(m_serverId);
}

long long MessageQueryService::parseDateTimeToTimestamp(const std::string& dateTimeStr) const
{
    if (dateTimeStr.empty())
    {
        return 0;
    }

    std::tm parsedTime = {};
    std::istringstream stream(dateTimeStr);
    stream >> std::get_time(&parsedTime, "%Y-%m-%d %H:%M:%S");
    if (stream.fail())
    {
        Logger::error("parseDateTimeToTimestamp failed: " + dateTimeStr);
        return 0;
    }

    const std::time_t timestamp = std::mktime(&parsedTime);
    if (timestamp == -1)
    {
        Logger::error("mktime failed for: " + dateTimeStr);
        return 0;
    }

    return static_cast<long long>(timestamp);
}
