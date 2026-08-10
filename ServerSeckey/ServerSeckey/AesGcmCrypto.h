#pragma once
#include <string>
#include <openssl/evp.h>
// AES-GCM 是认证加密模式：一次加密会同时产生密文、nonce 和 tag。
// 调用方必须把三者一起保存或传输，接收方才能完成解密和完整性校验。
struct GcmEncryptResult
{
    bool success;              // 是否加密成功
    std::string ciphertext;    // 二进制密文
    std::string nonce;         // 二进制 nonce
    std::string tag;           // 二进制 tag
    std::string errorMsg;      // 错误信息
};

// GCM 解密结果。
// success=false 通常表示 key、nonce、tag、密文任一项不匹配，或密文被篡改。
struct GcmDecryptResult
{
    bool success;              // 是否解密成功
    std::string plaintext;     // 二进制明文
    std::string errorMsg;      // 错误信息
};  

// 职责：基于 OpenSSL EVP 接口封装 AES-GCM 加解密。
// 边界：只处理原始二进制 key / plaintext / ciphertext，不负责 Base64、协议或存储。
class AesGcmCrypto
{
public:
    // 构造时传入原始二进制 key，长度必须是 16 / 24 / 32 字节之一。
	explicit AesGcmCrypto(const std::string& key);
    ~AesGcmCrypto();

    // 输入明文，输出 ciphertext + nonce + tag。
	GcmEncryptResult encrypt(const std::string& plaintext);

    // 输入 nonce + ciphertext + tag，校验通过后输出明文。
	GcmDecryptResult decrypt(const std::string& nonce, const std::string& ciphertext, const std::string& tag);

private:
    // 根据 key 长度选择具体的 GCM 算法。
    // 16 字节 -> AES-128-GCM
    // 24 字节 -> AES-192-GCM
    // 32 字节 -> AES-256-GCM
	const EVP_CIPHER* getCipherByKeyLen() const; 

private:
	std::string m_key; // 原始二进制 key

};

