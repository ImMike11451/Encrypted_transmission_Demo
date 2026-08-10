#include "V2RequestCodec.h"

V2RequestCodec::V2RequestCodec()
{
    // 空构造适合“先创建对象，后调用 initMessage()”的场景。
}

V2RequestCodec::V2RequestCodec(const std::string& encStr)
{
    // 解码路径：先保存网络收到的 protobuf 字节流，decodeMsg() 时再解析。
    initMessage(encStr);
}

V2RequestCodec::V2RequestCodec(V2SendMessageRequestInfo* info)
{
    // 编码路径：把业务层结构体填充到 RequestPacket。
    // Codec 只做字段映射，不做业务校验。
    initMessage(info);
}

V2RequestCodec::V2RequestCodec(V2QueryMessageRequestInfo* info)
{
    initMessage(info);
}

V2RequestCodec::V2RequestCodec(V2QueryMessageListRequestInfo* info)
{
    initMessage(info);
}

V2RequestCodec::V2RequestCodec(V2KeyAgreementRequestInfo* info)
{
    initMessage(info);
}

V2RequestCodec::V2RequestCodec(V2KeyCheckRequestInfo* info)
{
    initMessage(info);
}

V2RequestCodec::V2RequestCodec(V2KeyLogoutRequestInfo* info)
{
    initMessage(info);
}

void V2RequestCodec::initMessage(const std::string& encStr)
{
    // 这里只保存原始字节流，真正解析发生在 decodeMsg()。
    m_encStr = encStr;
}

void V2RequestCodec::initMessage(V2SendMessageRequestInfo* info)
{
    // 1. 填充公共头 Header。
    // Header 承载路由和追踪信息，业务体只保留各自领域字段。
    secmng::v2::Header* header = m_msg.mutable_header();

    header->set_message_id(info->header.messageId);
    header->set_command(
        static_cast<secmng::v2::CommandType>(info->header.command)
    );
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    // 2. 填充发送消息请求体。
    // oneof 保证同一个 RequestPacket 中只出现一种业务请求。
    secmng::v2::SendMessageRequest* req = m_msg.mutable_send_msg_req();
    secmng::v2::EncryptedMessage* msg = req->mutable_message();

    msg->set_key_id(info->message.keyId);
    msg->set_msg_type(
        static_cast<secmng::v2::MessageType>(info->message.msgType)
    );
    msg->set_ciphertext(info->message.ciphertext);
    msg->set_nonce(info->message.nonce);
    msg->set_tag(info->message.tag);
    msg->set_algorithm(info->message.algorithm);
}

void V2RequestCodec::initMessage(V2QueryMessageRequestInfo* info)
{
    // 公共头
    secmng::v2::Header* header = m_msg.mutable_header();
    header->set_message_id(info->header.messageId);
    header->set_command(static_cast<secmng::v2::CommandType>(info->header.command));
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    // 请求体：query_msg_req
    secmng::v2::QueryMessageRequest* req = m_msg.mutable_query_msg_req();
    req->set_server_message_id(info->serverMessageId);

}

void V2RequestCodec::initMessage(V2QueryMessageListRequestInfo* info)
{
    // 第 1 步：填充公共头
    secmng::v2::Header* header = m_msg.mutable_header();
    header->set_message_id(info->header.messageId);
    header->set_command(static_cast<secmng::v2::CommandType>(info->header.command));
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    // 第 2 步：填充列表查询请求体
	secmng::v2::QueryMessageListRequest* req = m_msg.mutable_query_msg_list_req();
    req->set_sender_id(info->senderId);
    req->set_limit(info->limit);
}

void V2RequestCodec::initMessage(V2KeyAgreementRequestInfo* info)
{
    secmng::v2::Header* header = m_msg.mutable_header();
    header->set_message_id(info->header.messageId);
    header->set_command(static_cast<secmng::v2::CommandType>(info->header.command));
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    secmng::v2::KeyAgreementRequest* req = m_msg.mutable_key_agree_req();
    req->set_public_key(info->publicKey);
    req->set_sign(info->sign);
}

void V2RequestCodec::initMessage(V2KeyCheckRequestInfo* info)
{
    secmng::v2::Header* header = m_msg.mutable_header();
    header->set_message_id(info->header.messageId);
    header->set_command(static_cast<secmng::v2::CommandType>(info->header.command));
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    secmng::v2::KeyCheckRequest* req = m_msg.mutable_key_check_req();
    req->set_key_id(info->keyId);
}

void V2RequestCodec::initMessage(V2KeyLogoutRequestInfo* info)
{
    secmng::v2::Header* header = m_msg.mutable_header();
    header->set_message_id(info->header.messageId);
    header->set_command(static_cast<secmng::v2::CommandType>(info->header.command));
    header->set_sender_id(info->header.senderId);
    header->set_receiver_id(info->header.receiverId);
    header->set_timestamp(info->header.timestamp);

    secmng::v2::KeyLogoutRequest* req = m_msg.mutable_key_logout_req();
    req->set_key_id(info->keyId);
}

std::string V2RequestCodec::encodeMsg()
{
    // 把已经填充好的 RequestPacket 序列化成网络可传输的字节流。
    std::string outStr;
    m_msg.SerializeToString(&outStr);

    return outStr;
}

void* V2RequestCodec::decodeMsg()
{
    // 把原始字节流解析成 RequestPacket。
    // 返回成员对象地址，调用方不能 delete。
    m_msg.ParseFromString(m_encStr);

    return &m_msg;
}

V2RequestCodec::~V2RequestCodec()
{
    // 没有手动管理的堆资源。
}
