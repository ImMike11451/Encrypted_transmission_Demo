#pragma once
#include <iostream>
#include <string>
#include <openssl/aes.h>

class AesCrypto
{
public:

	// 可使用 16byte, 24byte, 32byte 的秘钥
	AesCrypto(std::string key);
	~AesCrypto();
	std::string aesEncrypt(std::string data);
	std::string aesDecrypt(std::string data);

private:
	std::string aesCrypto(std::string data, size_t crypto);
	void generateIvec(unsigned char* ivec);

	AES_KEY m_encryptKey;  //加密密钥
	AES_KEY m_decryptKey;  //解密密钥
	std::string m_key;   //秘钥
};

