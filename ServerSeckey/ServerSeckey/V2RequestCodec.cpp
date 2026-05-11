#include "V2RequestCodec.h"

V2RequestCodec::V2RequestCodec()
{
    // 空实现即可。
    // 这种构造方式适合“先创建对象，后调用 initMessage()”的场景。
}

V2RequestCodec::V2RequestCodec(const std::string& encStr)
{
    // 这是解码路径：
    // 调用者手里已经有网络收到的字节流，
    // 所以我们先把字节流保存起来，后面 decodeMsg() 时再解析。
    initMessage(encStr);
}

V2RequestCodec::V2RequestCodec(V2SendMessageRequestInfo* info)
{
    // 这是编码路径：
    // 调用者先准备好了业务层结构体，
    // 然后我们把这些业务字段填充到 protobuf 对象里。
    initMessage(info);
}

V2RequestCodec::V2RequestCodec(V2QueryMessageRequestInfo* info)
{
    initMessage(info);
}

void V2RequestCodec::initMessage(const std::string& encStr)
{
    // 这里只是简单保存原始字节流，不做解析。
    // 真正的解析发生在 decodeMsg() 中。
    m_encStr = encStr;
}

void V2RequestCodec::initMessage(V2SendMessageRequestInfo* info)
{
    // =========================
    // 1. 填充公共头 Header
    // =========================

    // mutable_header() 的含义是：
    // 如果当前 RequestPacket 里还没有 header，就创建一个；
    // 然后返回这个 header 的可写指针。
    secmng::v2::Header* header = m_msg.mutable_header();

    // 设置消息唯一编号
    header->set_message_id(info->header.messageId);

    // 命令类型是枚举，业务层先用 int 存，
    // 这里再显式转换成 proto 定义的枚举类型。
    header->set_command(
        static_cast<secmng::v2::CommandType>(info->header.command)
    );

    // 设置发送方 ID
    header->set_sender_id(info->header.senderId);

    // 设置接收方 ID
    header->set_receiver_id(info->header.receiverId);

    // 设置时间戳
    header->set_timestamp(info->header.timestamp);

    // =========================
    // 2. 填充发送消息请求体
    // =========================

    // 取到 RequestPacket 里的 send_msg_req 业务体
    secmng::v2::SendMessageRequest* req = m_msg.mutable_send_msg_req();

    // 再取到 send_msg_req 里的真正消息对象 message
    secmng::v2::EncryptedMessage* msg = req->mutable_message();

    // 设置本次使用的密钥编号
    msg->set_key_id(info->message.keyId);

    // 设置消息类型，同样需要从 int 转换为 proto 枚举
    msg->set_msg_type(
        static_cast<secmng::v2::MessageType>(info->message.msgType)
    );

    // 设置密文内容
    msg->set_ciphertext(info->message.ciphertext);

    // 设置 nonce / IV
    msg->set_nonce(info->message.nonce);

    // 设置认证标签
    msg->set_tag(info->message.tag);

    // 设置算法名称
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

std::string V2RequestCodec::encodeMsg()
{
    // 这个函数做的事情很单纯：
    // 把前面已经填充好的 protobuf 对象序列化成字符串。
    std::string outStr;

    // SerializeToString() 是 protobuf 提供的序列化函数。
    // 成功后，outStr 里就是可以直接通过 socket 发送的字节流。
    m_msg.SerializeToString(&outStr);

    return outStr;
}

void* V2RequestCodec::decodeMsg()
{
    // 把 m_encStr 中保存的原始字节流反序列化成 protobuf 对象。
    m_msg.ParseFromString(m_encStr);

    // 返回 protobuf 对象地址。
    // 注意：这里返回的是成员对象地址，外部不能 delete。
    return &m_msg;
}

V2RequestCodec::~V2RequestCodec()
{
    // 当前没有手动管理的堆资源，所以析构函数为空即可。
}
