#pragma once
#include <iostream>
#include <string>
#include <openssl/aes.h>

// 职责：历史 AES-CBC 封装，保留用于兼容旧代码。
// 边界：当前消息发送主链路使用 AesGcmCrypto；新功能优先使用 AES-GCM。
// 注意：该类从 key 派生 IV，不适合作为生产级新加密方案。
class AesCrypto
{
public:

	// 可使用 16 / 24 / 32 字节密钥。
	AesCrypto(std::string key);
	~AesCrypto();
	std::string aesEncrypt(std::string data);
	std::string aesDecrypt(std::string data);

private:
	std::string aesCrypto(std::string data, size_t crypto);
	// 历史实现：从 key 派生 IV。新代码应使用随机 nonce/IV 的认证加密方案。
	void generateIvec(unsigned char* ivec);

	AES_KEY m_encryptKey;  //加密密钥
	AES_KEY m_decryptKey;  //解密密钥
	std::string m_key;   //秘钥
};

