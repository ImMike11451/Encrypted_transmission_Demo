#pragma once

#include <string>
#include "Codec.h"
#include "MessageV2.pb.h"
#include "V2RequestCodec.h"

// 这个结构体表示“发送消息响应”的业务层数据。
struct V2SendMessageResponseInfo
{
    V2HeaderInfo header;          // 公共头
    int code;                     // 结果码，对应 proto 中的 ResultCode
    std::string message;          // 响应描述信息
    std::string serverMessageId;  // 服务端生成的消息记录 ID
    long long serverTime;         // 服务端处理时间
    int deliveryStatus;           // DeliveryStatus
};

// 查询消息响应
struct V2QueryMessageResponseInfo
{
    V2HeaderInfo header;
    int code;                     // ResultCode
    std::string message;          // 响应描述
    std::string serverMessageId;  // 服务端消息 ID
    std::string senderId;         // 原发送方
    std::string receiverId;       // 原接收方
    int keyId;                    // 使用的 key_id
    std::string msgType;          // 消息类型
    long long sendTime;           // 发送时间
    int status;                   // 消息状态
};

// 响应编解码器：
// 负责把服务端业务层的响应结构体编码成 protobuf 字节流，
// 以及把收到的响应字节流还原成 protobuf 对象。
class V2RespondCodec : public Codec
{
public:
    V2RespondCodec();
    V2RespondCodec(const std::string& encStr);
    V2RespondCodec(V2SendMessageResponseInfo* info);

    // 查询消息响应编码
    V2RespondCodec(V2QueryMessageResponseInfo* info);

    void initMessage(const std::string& encStr);
    void initMessage(V2SendMessageResponseInfo* info);
    void initMessage(V2QueryMessageResponseInfo* info);

    std::string encodeMsg() override;
    void* decodeMsg() override;

    ~V2RespondCodec();

private:
    // protobuf 响应对象
    secmng::v2::ResponsePacket m_msg;

    // 保存待解码的原始字符串
    std::string m_encStr;
};
