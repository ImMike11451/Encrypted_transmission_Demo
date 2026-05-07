#include "AesGcmCrypto.h"
#include <openssl/rand.h>

// GCM 模式下，推荐使用 12 字节 nonce
static const size_t GCM_NONCE_SIZE = 12;

// GCM 模式下，tag 常用 16 字节
static const size_t GCM_TAG_SIZE = 16;

AesGcmCrypto::AesGcmCrypto(const std::string& key)
    : m_key(key)
{
}

AesGcmCrypto::~AesGcmCrypto()
{
}

GcmEncryptResult AesGcmCrypto::encrypt(const std::string& plaintext)
{
	GcmEncryptResult result;
	result.success = false;

	const EVP_CIPHER* cipher = getCipherByKeyLen();
	if (cipher == nullptr)
	{
		result.errorMsg = "unsupported AES key length";
		return result;
	}

	// 随机生成 nonce
	std::string nonce(GCM_NONCE_SIZE, '\0');
	if (RAND_bytes(reinterpret_cast<unsigned char*>(&nonce[0]), GCM_NONCE_SIZE) != 1)
	{
		result.errorMsg = "generate nonce failed";
		return result;
	}

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if(ctx == nullptr)
	{
		result.errorMsg = "create EVP_CIPHER_CTX failed";
		return result;
	}

	int len = 0;
	int ciphertextLen = 0;

	// GCM 加密后的密文长度通常与明文长度相同，
	// 所以这里先按明文长度分配缓冲区即可。
	std::string ciphertext(plaintext.size(), '\0');
	std::string tag(GCM_TAG_SIZE, '\0'); // tag 长度固定

	do
	{
		// 初始化 GCM 算法
		if(EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1)
		{
			result.errorMsg = "EVP_EncryptInit_ex failed";
			break;
		}

		// 设置 nonce 长度
		if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_SIZE, nullptr) != 1)
		{
			result.errorMsg = "EVP_CTRL_GCM_SET_IVLEN failed";
			break;
		}

		// 设置 key 和 nonce
		if(EVP_EncryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(m_key.data()), 
			reinterpret_cast<const unsigned char*>(nonce.data())) != 1)
		{
			result.errorMsg = "EVP_EncryptInit_ex set key/nonce failed";
			break;
		}

		//执行加密
		if(EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(&ciphertext[0]), 
			&len, reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()) != 1)
		{
			result.errorMsg = "EVP_EncryptUpdate failed";
			break;
		}

		ciphertextLen += len;
		ciphertext.resize(ciphertextLen); //调整密文长度

		// 取出认证标签 tag
		if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE,&tag[0]) != 1)
		{
			result.errorMsg = "get GCM tag failed";
			break;
		}

		result.success = true;
		result.ciphertext = ciphertext;
		result.nonce = nonce;
		result.tag = tag;

	} while (false);

	EVP_CIPHER_CTX_free(ctx);
	return result;

}

GcmDecryptResult AesGcmCrypto::decrypt(const std::string& nonce, const std::string& ciphertext, const std::string& tag)
{
	GcmDecryptResult result;
	result.success = false;

	const EVP_CIPHER* cipher = getCipherByKeyLen();
	if(cipher == nullptr)
	{
		result.errorMsg = "unsupported AES key length";
		return result;
	}

	if(nonce.size() != GCM_NONCE_SIZE)
	{
		result.errorMsg = "invalid nonce size";
		return result;
	}

	if(tag.size() != GCM_TAG_SIZE)
	{
		result.errorMsg = "invalid tag size";
		return result;
	}

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if(ctx == nullptr)
	{
		result.errorMsg = "create EVP_CIPHER_CTX failed";
		return result;
	}

	int len = 0;
	int plaintextLen = 0;

	std::string plaintext(ciphertext.size(), '\0'); //解密后的明文长度与密文相同

	do
	{
		//初始解密上下文
		if(EVP_DecryptInit_ex(ctx,cipher,nullptr,nullptr,nullptr) != 1)
		{
			result.errorMsg = "EVP_DecryptInit_ex failed";
			break;
		}

		// 设置 nonce 长度
		if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_SIZE, nullptr) != 1)
		{
			result.errorMsg = "set GCM IV len failed";
			break;
		}

		// 设置 key 和 nonce
		if (EVP_DecryptInit_ex(
			ctx,
			nullptr,
			nullptr,
			reinterpret_cast<const unsigned char*>(m_key.data()),
			reinterpret_cast<const unsigned char*>(nonce.data())) != 1)
		{
			result.errorMsg = "set key and nonce failed";
			break;
		}

		// 先解密主体
		if (EVP_DecryptUpdate(
			ctx,
			reinterpret_cast<unsigned char*>(&plaintext[0]),
			&len,
			reinterpret_cast<const unsigned char*>(ciphertext.data()),
			ciphertext.size()) != 1)
		{
			result.errorMsg = "EVP_DecryptUpdate failed";
			break;
		}

		plaintextLen = len;

		// 在 Final 前设置 tag
		if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 
			GCM_TAG_SIZE, const_cast<char*>(tag.data())) != 1)
		{
			result.errorMsg = "set GCM tag failed";
			break;
		}

		// 这里是 GCM 最关键的一步：
		// 如果 tag 校验失败，返回值就不是 1。
		// 这说明消息可能被篡改、nonce/tag 不匹配，或者 key 不对
		int ret = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&plaintext[0]) + plaintextLen, &len);

		if(ret != 1)
		{
			result.errorMsg = "GCM authentication failed";
			break;
		}

		plaintextLen += len;
		plaintext.resize(plaintextLen); //调整明文长度

		result.success = true;
		result.plaintext = plaintext;

	} while (false);

	EVP_CIPHER_CTX_free(ctx);
	return result;

}

const EVP_CIPHER* AesGcmCrypto::getCipherByKeyLen() const
{
	if (m_key.size() == 16)
	{
		return EVP_aes_128_gcm();  //返回的是一个指向 EVP_CIPHER 结构的指针，表示 AES-128-GCM 算法
	}
	else if(m_key.size() == 24)
	{
		return EVP_aes_192_gcm();  //返回的是一个指向 EVP_CIPHER 结构的指针，表示 AES-192-GCM 算法
	}
	else if(m_key.size() == 32)
	{
		return EVP_aes_256_gcm();  //返回的是一个指向 EVP_CIPHER 结构的指针，表示 AES-256-GCM 算法
	}
	else
	{
		return nullptr; //不支持的 key 长度
	}
}
