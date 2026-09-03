#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "MessageQueryService.h"

namespace
{
int g_failedCount = 0;

void expectTrue(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[失败] " << message << std::endl;
        ++g_failedCount;
    }
}

struct RecordedAuditEvent
{
    std::string nodeId;
    std::string action;
    std::string targetId;
    int result;
    std::string detail;
};

class InMemoryMessageRepository final : public IMessageRepository
{
  public:
    bool insertMessage(const MessageLogRecord& record) override
    {
        MessageQueryResult result{};
        result.found = true;
        result.msgId = record.msgId;
        result.senderId = record.senderId;
        result.receiverId = record.receiverId;
        result.keyId = record.senderKeyId;
        result.msgType = record.msgType;
        result.sendTime = record.sendTime;
        result.status = record.status;
        messages[result.msgId] = result;
        return true;
    }

    bool queryMessageById(const std::string& msgId, MessageQueryResult& result) override
    {
        ++singleQueryCount;
        const auto item = messages.find(msgId);
        if (item == messages.end())
        {
            result = {};
            result.msgId = msgId;
            result.errorMsg = "message not found";
            return false;
        }

        result = item->second;
        return true;
    }

    bool queryRecentMessagesBySender(const std::string& senderId, int limit,
                                     std::vector<MessageSummaryInfo>& results) override
    {
        ++listQueryCount;
        lastListSenderId = senderId;
        results.clear();

        for (const MessageSummaryInfo& message : summaries)
        {
            if (message.senderId == senderId && static_cast<int>(results.size()) < limit)
            {
                results.push_back(message);
            }
        }

        return true;
    }

    std::unordered_map<std::string, MessageQueryResult> messages;
    std::vector<MessageSummaryInfo> summaries;
    int singleQueryCount = 0;
    int listQueryCount = 0;
    std::string lastListSenderId;
};

class RecordingMessageAudit final : public IMessageAudit
{
  public:
    bool logAction(const std::string& logId, const std::string& nodeId, const std::string& action,
                   const std::string& targetId, int result, const std::string& detail) override
    {
        (void)logId;
        events.push_back({nodeId, action, targetId, result, detail});
        return true;
    }

    std::vector<RecordedAuditEvent> events;
};

secmng::v2::RequestPacket makeSingleQueryRequest(const std::string& requesterId, const std::string& serverMessageId)
{
    secmng::v2::RequestPacket packet;
    secmng::v2::Header* header = packet.mutable_header();
    header->set_message_id("query-request-001");
    header->set_command(secmng::v2::CMD_QUERY_MSG_REQ);
    header->set_sender_id(requesterId);
    header->set_receiver_id("6789");
    header->set_timestamp(1725000000);
    packet.mutable_query_msg_req()->set_server_message_id(serverMessageId);
    return packet;
}

secmng::v2::RequestPacket makeListQueryRequest(const std::string& requesterId, const std::string& queriedSenderId,
                                               int limit)
{
    secmng::v2::RequestPacket packet;
    secmng::v2::Header* header = packet.mutable_header();
    header->set_message_id("list-request-001");
    header->set_command(secmng::v2::CMD_QUERY_MSG_LIST_REQ);
    header->set_sender_id(requesterId);
    header->set_receiver_id("6789");
    header->set_timestamp(1725000000);
    secmng::v2::QueryMessageListRequest* request = packet.mutable_query_msg_list_req();
    request->set_sender_id(queriedSenderId);
    request->set_limit(limit);
    return packet;
}

MessageQueryResult makeStoredMessage()
{
    MessageQueryResult message{};
    message.found = true;
    message.msgId = "server-message-001";
    message.senderId = "0001";
    message.receiverId = "0002";
    message.keyId = 42;
    message.msgType = "text";
    message.sendTime = "2026-09-02 12:00:00";
    message.status = 1;
    return message;
}

void testSenderCanQuerySingleMessage()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    repository.messages.emplace("server-message-001", makeStoredMessage());
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageResponseInfo response =
        service.handleQueryMessage(makeSingleQueryRequest("0001", "server-message-001"));

    expectTrue(response.code == secmng::v2::RESULT_SUCCESS, "发送方应能查询自己的单条消息。");
    expectTrue(response.serverMessageId == "server-message-001", "成功响应应包含服务端消息 ID。");
    expectTrue(response.senderId == "0001" && response.receiverId == "0002", "成功响应应包含真实参与方。");
    expectTrue(audit.events.size() == 1 && audit.events[0].result == 1, "成功查询应记录成功审计事件。");
}

void testReceiverCanQuerySingleMessage()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    repository.messages.emplace("server-message-001", makeStoredMessage());
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageResponseInfo response =
        service.handleQueryMessage(makeSingleQueryRequest("0002", "server-message-001"));

    expectTrue(response.code == secmng::v2::RESULT_SUCCESS, "接收方应能查询收到的单条消息。");
}

void testUnrelatedClientCannotQuerySingleMessage()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    repository.messages.emplace("server-message-001", makeStoredMessage());
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageResponseInfo response =
        service.handleQueryMessage(makeSingleQueryRequest("0003", "server-message-001"));

    expectTrue(response.code == secmng::v2::RESULT_INVALID_REQUEST, "无关客户端查询单条消息时应被拒绝。");
    expectTrue(response.message == "permission denied", "越权查询应返回明确的权限错误。");
    expectTrue(audit.events.size() == 1 && audit.events[0].detail == "permission denied",
               "越权查询应记录失败审计事件。");
}

void testMissingMessageReturnsNotFound()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageResponseInfo response =
        service.handleQueryMessage(makeSingleQueryRequest("0001", "missing-message"));

    expectTrue(response.code == secmng::v2::RESULT_MSG_NOT_FOUND, "不存在的消息应返回未找到结果。");
    expectTrue(audit.events.size() == 1 && audit.events[0].result == 0, "未找到消息应记录失败审计事件。");
}

void testEmptyRequesterDoesNotAccessRepository()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    repository.messages.emplace("server-message-001", makeStoredMessage());
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageResponseInfo response =
        service.handleQueryMessage(makeSingleQueryRequest("", "server-message-001"));

    expectTrue(response.code == secmng::v2::RESULT_INVALID_REQUEST, "请求者为空的单条查询应被拒绝。");
    expectTrue(response.message == "header sender_id is empty", "请求者为空时应返回明确的字段错误。");
    expectTrue(repository.singleQueryCount == 0, "请求者为空时不应访问消息仓储。");
    expectTrue(audit.events.size() == 1 && audit.events[0].result == 0, "请求者为空时应记录失败审计事件。");
}

void testClientCanQueryOwnSentMessageList()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    repository.summaries.push_back({"server-message-001", "0001", "0002", 42, "text", "2026-09-02 12:00:00", 1});
    repository.summaries.push_back({"server-message-002", "0003", "0001", 43, "text", "2026-09-02 12:01:00", 1});
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageListResponseInfo response =
        service.handleQueryMessageList(makeListQueryRequest("0001", "0001", 10));

    expectTrue(response.code == secmng::v2::RESULT_SUCCESS, "客户端应能查询自己的发送记录。");
    expectTrue(response.messages.size() == 1, "列表响应应只包含当前发送方的记录。");
    expectTrue(response.messages[0].serverMessageId == "server-message-001", "列表响应应保留消息 ID。");
    expectTrue(repository.lastListSenderId == "0001", "查询模块应使用当前请求者作为发送方条件。");
}

void testClientCannotQueryAnotherSendersList()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageListResponseInfo response =
        service.handleQueryMessageList(makeListQueryRequest("0001", "0002", 10));

    expectTrue(response.code == secmng::v2::RESULT_INVALID_REQUEST, "客户端查询他人发送记录时应被拒绝。");
    expectTrue(response.message == "permission denied", "越权列表查询应返回明确的权限错误。");
    expectTrue(repository.listQueryCount == 0, "越权列表查询不应访问仓储。");
    expectTrue(audit.events.size() == 1 && audit.events[0].result == 0, "越权列表查询应记录失败审计事件。");
}

void testInvalidLimitDoesNotAccessRepository()
{
    InMemoryMessageRepository repository;
    RecordingMessageAudit audit;
    MessageQueryService service("6789", &repository, &audit);

    const V2QueryMessageListResponseInfo response =
        service.handleQueryMessageList(makeListQueryRequest("0001", "0001", 0));

    expectTrue(response.code == secmng::v2::RESULT_INVALID_REQUEST, "无效查询数量应被拒绝。");
    expectTrue(repository.listQueryCount == 0, "无效查询数量不应访问仓储。");
}
} // namespace

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    testSenderCanQuerySingleMessage();
    testReceiverCanQuerySingleMessage();
    testUnrelatedClientCannotQuerySingleMessage();
    testMissingMessageReturnsNotFound();
    testEmptyRequesterDoesNotAccessRepository();
    testClientCanQueryOwnSentMessageList();
    testClientCannotQueryAnotherSendersList();
    testInvalidLimitDoesNotAccessRepository();

    google::protobuf::ShutdownProtobufLibrary();

    if (g_failedCount == 0)
    {
        std::cout << "全部消息权限测试通过。" << std::endl;
        return 0;
    }

    std::cerr << "消息权限测试失败数量: " << g_failedCount << std::endl;
    return 1;
}
