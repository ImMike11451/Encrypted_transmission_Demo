#pragma once
#include "SecKeyShm.h"
#include "mysqlOP.h"
#include "V2RespondCodec.h"
#include "MessageV2.pb.h"
#include <string>

// 这个结构体表示服务端查 key 之后得到的结果。
// 为什么要单独封装？
// 因为“查 key”不是只返回成功/失败，它还会带出：
// 1. 密钥是否存在
// 2. 密钥是否有效
// 3. 密钥原文（当前项目中以 base64 形式存于共享内存）
// 4. 错误信息
struct ActiveKeyResult
{
	bool found; // 是否找到了对应 key
	bool valid;  // 密钥是否有效
    std::string base64Key;     // 共享内存/数据库中保存的 base64 编码 key
    int keyId;                 // key 编号
    std::string errorMsg;      // 错误信息
};

// 这个结构体表示服务端解密消息后的结果。
// 单独封装能让解密逻辑和业务流程分开，代码会更清楚。
struct DecryptMessageResult
{
    bool success;              // 解密是否成功
    std::string plaintext;     // 解密后的明文
    std::string errorMsg;      // 错误信息
};

// MessageService 的职责是处理“发送加密消息”这个新业务。
// 它不负责 socket 收发，也不负责 protobuf 编解码细节。
// 它只关心业务本身：
// 1. 校验请求是否合法
// 2. 查找当前 key
// 3. 解密消息
// 4. 生成结构化响应
class MessageService
{
public:
    // 构造函数需要依赖：
    // 1. 服务端 ID：用于组装响应头
    // 2. mysqlOP：用于后续查库、写库
    // 3. SecKeyShm：用于快速读取当前 key
    MessageService(const std::string& serverId, mysqlOP* db, SecKeyShm* shm);
    ~MessageService();

    // 处理“发送消息请求”的主入口。
    // 这里先返回 V2SendMessageResponseInfo，后面由外层 codec 编码。
	V2SendMessageResponseInfo handleSendMessage(const secmng::v2::RequestPacket& packet);

private:
    // 校验请求包字段是否合法。
    // 合法返回 true，不合法时同时填写 errorMsg。
    bool validateRequest(const secmng::v2::RequestPacket& packet,std::string& errorMsg);

    // 根据 sender / receiver / keyId 查找当前活跃 key。
    // 第一阶段先采用：
    // 1. 优先共享内存
    // 2. 再通过数据库确认状态
    ActiveKeyResult getActiveKey(const std::string& senderId,const std::string& receiverId,int keyId);

    // 使用当前项目已有的 AesCrypto 解密密文。
    // 注意：当前 AesCrypto 并不使用外部 nonce，而是内部用 key 派生固定 IV。
    DecryptMessageResult decryptMessage(const std::string& base64Key,const std::string& base64Ciphertext,const std::string& algorithm);

    // 生成服务端响应头
    V2HeaderInfo buildResponseHeader(const secmng::v2::RequestPacket& packet,int command);

    // 生成服务端消息记录 ID。
    // 第一阶段先用简单规则，后面可以替换成更规范的 UUID。
    std::string generateServerMessageId();

    // 生成审计日志 ID
	std::string generateAuditLogId();

private:
    std::string m_serverId;   // 当前服务端 ID
    mysqlOP* m_db;      // 数据库对象，生命周期由外部管理
	SecKeyShm* m_shm;   // 共享内存对象，生命周期由外部管理

};

