#pragma once
#include <string>
#include <openssl/evp.h>
// GCM 加密结果
// 之所以单独封装，是因为 GCM 加密不是只返回一个密文，
// 而是会同时返回：
// 1. ciphertext - 密文
// 2. nonce      - 随机数
// 3. tag        - 认证标签
struct GcmEncryptResult
{
    bool success;              // 是否加密成功
    std::string ciphertext;    // 二进制密文
    std::string nonce;         // 二进制 nonce
    std::string tag;           // 二进制 tag
    std::string errorMsg;      // 错误信息
};

// GCM 解密结果
struct GcmDecryptResult
{
    bool success;              // 是否解密成功
    std::string plaintext;     // 二进制明文
    std::string errorMsg;      // 错误信息
};  

// AesGcmCrypto：基于 OpenSSL EVP 接口封装 AES-GCM。
// EVP 是 OpenSSL 推荐的高层接口：
// - 更统一
// - 更安全
// - 更适合 GCM 这类现代模式
class AesGcmCrypto
{
public:
    // 构造时传入原始二进制 key
	explicit AesGcmCrypto(const std::string& key);
    ~AesGcmCrypto();

    //加密
    // 输入明文，输出 ciphertext + nonce + tag
	GcmEncryptResult encrypt(const std::string& plaintext);

    // 解密：
    // 输入 nonce + ciphertext + tag，输出明文
	GcmDecryptResult decrypt(const std::string& nonce, const std::string& ciphertext, const std::string& tag);

private:
    // 根据 key 长度选择具体的 GCM 算法
    // 16 字节 -> AES-128-GCM
    // 24 字节 -> AES-192-GCM
    // 32 字节 -> AES-256-GCM
	const EVP_CIPHER* getCipherByKeyLen() const; 

private:
	std::string m_key; // 原始二进制 key

};

