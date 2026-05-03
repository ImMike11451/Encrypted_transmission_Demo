#pragma once
#include <string>
#include <ctime>
#include <sstream>
#include "ClientOP.h"
#include "SecKeyShm.h"
#include "TcpSocket.h"
#include "AesCrypto.h"
#include "V2RequestCodec.h"
#include "V2RespondCodec.h"

// 这个结构体表示“发送文本消息”时，业务层需要提供的最小输入。
// 第一阶段我们只支持文本消息，所以字段比较少。
struct SendTextMessageInfo
{
    std::string receiverId;   // 接收方节点 ID
    std::string plaintext;    // 待发送的明文文本
};

// 这个结构体表示一次加密后的结果。
// 之所以单独封装，是因为现代加密通常不只返回密文，
// 还会同时产生 nonce / IV、tag、算法名等信息。
struct EncryptTextResult
{
    bool success;             // 加密是否成功
    std::string ciphertext;   // 密文，建议最终保存为 base64 字符串
    std::string nonce;        // 随机数 / IV，建议也保存为 base64 字符串
    std::string tag;          // 认证标签；如果当前算法暂时不支持，可先置空
    std::string algorithm;    // 算法名称，例如 AES-128-CBC
    std::string errorMsg;     // 如果失败，记录错误原因
};

// MessageClient 是客户端“发送加密消息”业务类。
// 它不负责密钥协商，也不负责菜单显示；
// 它只负责：
// 1. 读取本地有效密钥
// 2. 加密文本
// 3. 组装 v2 请求
// 4. 通过 socket 发给服务端
// 5. 接收并解析响应
class MessageClient
{
public:
    // 构造函数接收两部分依赖：
    // 1. ClientInfo：当前客户端的基础配置信息（clientId、serverId、IP、端口）
    // 2. SecKeyShm：本地共享内存对象，用于读取当前密钥
    MessageClient(const ClientInfo& info, SecKeyShm* shm);

	~MessageClient();

    // 对外暴露的主业务接口：
    // 发送一条文本消息到 receiverId
	bool sendTextMessage(const SendTextMessageInfo& msgInfo);

private:

    // 从共享内存读取当前可用密钥。
    // 如果读取失败，返回 false。
	bool getActiveKey(NodeSHMInfo& keyInfo);

    // 使用当前密钥对文本进行加密。
    // 这里先输出一个结构化结果，方便后面协议层直接使用。
	EncryptTextResult encryptText(const std::string& base64Key,const std::string& plaintext);

    // 组装请求头信息
	V2HeaderInfo buildHeader(const std::string& receiverId);

    // 生成消息唯一 ID。
    // 第一阶段我们可以先用“时间戳 + 发送方ID”这种简单方案。
	std::string generateMessageId();

private:
    ClientInfo m_info;   // 客户端基础配置
	SecKeyShm* m_shm;    // 共享内存对象指针，注意它的生命周期由外部管理

};

