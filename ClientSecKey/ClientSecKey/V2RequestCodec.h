#pragma once

#include <string>
#include "Codec.h"
#include "MessageV2.pb.h"

// 这个结构体表示“业务层眼里的公共头信息”。
// 也就是说，客户端在准备发送消息时，先把这些字段填进来，
// 编解码器再负责把它们搬运到 protobuf 的 Header 中。
struct V2HeaderInfo
{
    std::string messageId;   // 消息唯一编号
    int command;             // 命令类型，对应 proto 里的 CommandType
    std::string senderId;    // 发送方 ID
    std::string receiverId;  // 接收方 ID
    long long timestamp;     // 时间戳
};

// 这个结构体表示“业务层眼里的加密消息内容”。
// 注意：这里放的是已经加密好的结果，而不是明文。
// 也就是说，真正的 AES 加密逻辑不应该写在 codec 里，
// 而应该先由 MessageClient 之类的业务类完成。
struct V2EncryptedMessageInfo
{
    int keyId;                  // 使用的密钥编号
    int msgType;                // 消息类型，对应 proto 里的 MessageType
    std::string ciphertext;     // base64 编码后的密文
    std::string nonce;          // base64 编码后的 nonce / IV
    std::string tag;            // base64 编码后的认证标签，若暂时不用可为空
    std::string algorithm;      // 算法名称，例如 AES-128-CBC
};

// 这个结构体表示“发送消息请求”的完整业务结构。
// 它由两部分组成：
// 1. 公共头信息
// 2. 加密消息内容
struct V2SendMessageRequestInfo
{
    V2HeaderInfo header;
    V2EncryptedMessageInfo message;
};

// V2RequestCodec 的职责只有一个：
// 把“C++ 业务结构” <-> “protobuf 请求对象/字符串”做转换。
// 它不关心消息从哪来，也不关心消息往哪发。
class V2RequestCodec : public Codec
{
public:
    // 空构造：用于先创建对象，后续再初始化
    V2RequestCodec();

    // 这个构造函数用于“解码”场景：
    // 调用者拿到网络字节流后，用它初始化 codec
    V2RequestCodec(const std::string& encStr);

    // 这个构造函数用于“编码”场景：
    // 调用者先准备业务结构，再交给 codec 生成 protobuf 字节流
    V2RequestCodec(V2SendMessageRequestInfo* info);

    // 解码初始化：把收到的字节流保存起来，后面 decodeMsg() 再解析
    void initMessage(const std::string& encStr);

    // 编码初始化：把业务结构填充进 protobuf 对象
    void initMessage(V2SendMessageRequestInfo* info);

    // 把 protobuf 请求对象序列化成字符串，供网络发送
    std::string encodeMsg() override;

    // 把字符串反序列化回 protobuf 对象
    // 返回值仍然沿用旧工程的设计，使用 void* 以兼容当前 Codec 抽象
    void* decodeMsg() override;

    ~V2RequestCodec();

private:
    // protobuf 请求对象，真正负责序列化/反序列化
    secmng::v2::RequestPacket m_msg;

    // 保存待解码的原始字符串
    std::string m_encStr;
};
