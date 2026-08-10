#include "AesCrypto.h"
#include <openssl/rand.h>

AesCrypto::AesCrypto(std::string key)
{
	if( key.size() == 16 || key.size() == 24 || key.size() == 32)
	{
		const unsigned char* keyData = (const unsigned char*)key.data();
		AES_set_encrypt_key(keyData, key.size() * 8, &m_encryptKey);
		AES_set_decrypt_key(keyData, key.size() * 8, &m_decryptKey);
		m_key = key;
	}
}

AesCrypto::~AesCrypto()
{
}

std::string AesCrypto::aesEncrypt(std::string data)
{
	return aesCrypto(data, AES_ENCRYPT);
}

std::string AesCrypto::aesDecrypt(std::string data)
{
	return aesCrypto(data, AES_DECRYPT);
}

std::string AesCrypto::aesCrypto(std::string data, size_t crypto)
{
	// 根据 crypto 判断当前是加密还是解密。
	AES_KEY* key = (crypto == AES_ENCRYPT) ? &m_encryptKey : &m_decryptKey;
	unsigned char ivec[AES_BLOCK_SIZE];
	generateIvec(ivec);

	// 输入数据（后续可能会添加 padding）
    std::string input = data;

    if (crypto == AES_ENCRYPT)
    {
		// CBC 模式要求输入长度是分组大小的整数倍，这里使用 PKCS#7 风格 padding。
        int padding = AES_BLOCK_SIZE - (data.size() % AES_BLOCK_SIZE);
        for (int i = 0; i < padding; i++)
        {
            input.push_back(padding);
        }
    }

    std::string outData(input.size(), '\0');

    AES_cbc_encrypt((const unsigned char*)input.data(),(unsigned char*)outData.data(),input.size(),key,ivec,crypto);

    if (crypto == AES_DECRYPT)
    {
        int padding = outData[outData.size() - 1];
        outData = outData.substr(0, outData.size() - padding);
    }

    return outData;
}

void AesCrypto::generateIvec(unsigned char* ivec)
{
	for (int i = 0; i < AES_BLOCK_SIZE; ++i)
	{
		ivec[i] = m_key.at(AES_BLOCK_SIZE - i - 1);
	}
}
