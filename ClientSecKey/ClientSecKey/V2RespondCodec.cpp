#include "V2RespondCodec.h"

V2RespondCodec::V2RespondCodec()
{
    // 空构造，适合先创建对象再初始化的场景
}

V2RespondCodec::V2RespondCodec(const std::string& encStr)
{
    // 解码路径：先保存字节流
    initMessage(encStr);
}

V2RespondCodec::V2RespondCodec(V2SendMessageResponseInfo* info)
{
    // 编码路径：先把业务层响应结构填充到 protobuf 对象中
    initMessage(info);
}

V2RespondCodec::V2RespondCodec(V2QueryMessageListResponseInfo* info)
{
	initMessage(info);
}

V2RespondCodec::V2RespondCodec(V2QueryMessageResponseInfo* info)
{
    initMessage(info);
}

void V2RespondCodec::initMessage(const std::string& encStr)
{
    // 保存待解码的原始响应数据
    m_encStr = encStr;
}

void V2RespondCodec::initMessage(V2SendMessageResponseInfo* info)
{
    // =========================
    // 1. 填充公共头 Header
    // =========================
    secmng::v2::Header* header = m_msg.mutable_header();

    header->set_message_id(info->header.messageId);
    header->set_command(
        static_cast<secmng::v2::CommandType>(info->header.command)
    );
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    // =========================
    // 2. 填充发送消息响应体
    // =========================
    secmng::v2::SendMessageResponse* resp = m_msg.mutable_send_msg_resp();

    // 设置结果码
    resp->set_code(static_cast<secmng::v2::ResultCode>(info->code));

    // 设置提示信息
    resp->set_message(info->message);

    // 设置服务端记录的消息 ID
    resp->set_server_message_id(info->serverMessageId);

    // 设置服务端处理时间
    resp->set_server_time(info->serverTime);

    resp->set_delivery_status(
        static_cast<secmng::v2::DeliveryStatus>(info->deliveryStatus)
    );
}

void V2RespondCodec::initMessage(V2QueryMessageResponseInfo* info)
{
    // 公共头
    secmng::v2::Header* header = m_msg.mutable_header();
    header->set_message_id(info->header.messageId);
    header->set_command(static_cast<secmng::v2::CommandType>(info->header.command));
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    // 响应体：query_msg_resp
    secmng::v2::QueryMessageResponse* resp = m_msg.mutable_query_msg_resp();
    resp->set_code(static_cast<secmng::v2::ResultCode>(info->code));
    resp->set_message(info->message);
    resp->set_server_message_id(info->serverMessageId);
    resp->set_sender_id(info->senderId);
    resp->set_receiver_id(info->receiverId);
    resp->set_key_id(info->keyId);
    resp->set_msg_type(info->msgType);
    resp->set_send_time(info->sendTime);
    resp->set_status(info->status);
}

void V2RespondCodec::initMessage(V2QueryMessageListResponseInfo* info)
{
    // 第 1 步：填充公共头
    secmng::v2::Header* header = m_msg.mutable_header();
    header->set_message_id(info->header.messageId);
    header->set_command(static_cast<secmng::v2::CommandType>(info->header.command));
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    // 第 2 步：填充列表查询响应体
    secmng::v2::QueryMessageListResponse* resp = m_msg.mutable_query_msg_list_resp();
    resp->set_code(static_cast<secmng::v2::ResultCode>(info->code));
    resp->set_message(info->message);

    // 第 3 步：逐条填充 MessageSummary 列表
    for (const auto& item : info->messages)
    {
        secmng::v2::MessageSummary* summary = resp->add_messages();
        summary->set_server_message_id(item.serverMessageId);
        summary->set_sender_id(item.senderId);
        summary->set_receiver_id(item.receiverId);
        summary->set_key_id(item.keyId);
        summary->set_msg_type(item.msgType);
        summary->set_send_time(item.sendTime);
        summary->set_status(item.status);
    }
}

std::string V2RespondCodec::encodeMsg()
{
    // 序列化 protobuf 响应对象，得到网络可传输的字节流
    std::string outStr;
    m_msg.SerializeToString(&outStr);
    return outStr;
}

void* V2RespondCodec::decodeMsg()
{
    // 把收到的响应字节流解析为 protobuf 对象
    m_msg.ParseFromString(m_encStr);

    // 返回内部 protobuf 对象地址
    return &m_msg;
}

V2RespondCodec::~V2RespondCodec()
{
    // 当前没有手动释放的资源
}
