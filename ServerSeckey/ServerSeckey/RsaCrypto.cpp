#include "RsaCrypto.h"
#include "Logger.h"
#include <openssl/err.h>

//extern "C"
//{
//#include <openssl/applink.c>
//};

// 构造函数：初始化公钥和私钥结构体
RsaCrypto::RsaCrypto()
{
	m_publicKey = RSA_new();
	m_privateKey = RSA_new();
}

// 构造函数（带参数）：根据文件初始化密钥
RsaCrypto::RsaCrypto(std::string filename, bool isPrivate)
{
	m_publicKey = RSA_new();
	m_privateKey = RSA_new();
	// 根据标志选择初始化公钥或私钥
	if (isPrivate)
	{
		initPrivateKey(filename);
	}
	else
	{
		initPublicKey(filename);
	}
}

RsaCrypto::~RsaCrypto()
{
	RSA_free(m_publicKey);
	RSA_free(m_privateKey);
}

void RsaCrypto::generateKeyPair(std::string pubfile, std::string prifile, int bits)
{
	RSA* rsa = RSA_new();
	BIGNUM* bn = BN_new();
	BN_set_word(bn, RSA_F4);
	// 生成密钥对
	if (RSA_generate_key_ex(rsa, bits, bn, NULL) != 1)
	{
		ERR_print_errors_fp(stdout);
		return;
	}

	// 当前 demo 把密钥写入 PEM 文件，便于复用现有文件读取逻辑。
	BIO* pubbio = BIO_new_file(pubfile.data(), "w");

	if (PEM_write_bio_RSAPublicKey(pubbio, rsa) != 1)
	{
		ERR_print_errors_fp(stdout);
		return;
	}
	// 缓存中的数据刷到文件中
	BIO_flush(pubbio);

	BIO_free(pubbio);

	//将生成的私钥对写入文件
	BIO* pribio = BIO_new_file(prifile.data(), "w");
	if (PEM_write_bio_RSAPrivateKey(pribio, rsa, NULL, NULL, 0, NULL, NULL) != 1)
	{
		ERR_print_errors_fp(stdout);
		return;
	}

	BIO_flush(pribio);
	BIO_free(pribio);

	if (m_privateKey) RSA_free(m_privateKey);
	m_privateKey = RSAPrivateKey_dup(rsa);

	if (m_publicKey) RSA_free(m_publicKey);
	m_publicKey = RSAPublicKey_dup(rsa);

	RSA_free(rsa);
	BN_free(bn);

}

bool RsaCrypto::loadKey(std::string pubfile, std::string prifile)
{
	bool ret1 = initPublicKey(pubfile);
	bool ret2 = initPrivateKey(prifile);

	return ret1 && ret2;
}

std::string RsaCrypto::rsaPublicEncrypt(std::string data)
{
	// RSA 可加密的数据长度有限，本项目只用它加密短会话密钥。
	int keyLen = RSA_size(m_publicKey);
	std::string toData(keyLen, '\0');
	int retlen = RSA_public_encrypt(data.size(), (const unsigned char*)data.data(), (unsigned char*)toData.data(), m_publicKey, RSA_PKCS1_PADDING);
	if (retlen < 0)
	{
		ERR_print_errors_fp(stdout);
		return "";
	}
	std::string retStr = toBase64(toData.data(), retlen);
	// 返回加密后的数据
	return retStr;
}

std::string RsaCrypto::rsaPrivateDecrypt(std::string data)
{
	std::string text = fromBase64(data);
	int keyLen = RSA_size(m_privateKey);
	std::string toData(keyLen, '\0');
	int retlen = RSA_private_decrypt(text.size(), (const unsigned char*)text.data(), (unsigned char*)toData.data(), m_privateKey, RSA_PKCS1_PADDING);
	if (retlen < 0)
	{
		ERR_print_errors_fp(stdout);
		return "";
	}
	return std::string(toData.data(), retlen);
}

std::string RsaCrypto::rsaSign(std::string data, SignLevel level)
{
	size_t len = data.size();
	// RSA_sign 接收摘要数据。本项目先在业务层计算 SHA-256，再在这里签名。
	std::string signBuf(RSA_size(m_privateKey), '\0');
	unsigned int signBufLen = 0;
	int flag = RSA_sign(level, (const unsigned char*)data.data(), len, (unsigned char*)signBuf.data(), &signBufLen, m_privateKey);
	if (flag != 1)
	{
		ERR_print_errors_fp(stdout);
		return "";
	}
	std::string retStr = toBase64(signBuf.data(), signBufLen);
	return retStr;
}

// 公钥验签
bool RsaCrypto::rsaVerify(std::string data, std::string signData, SignLevel level)
{
	std::string sign = fromBase64(signData);
	int flag = RSA_verify(level, (const unsigned char*)data.data(), data.size(), (const unsigned char*)sign.data(), sign.size(), m_publicKey);
	if (flag != 1)
	{
		ERR_print_errors_fp(stdout);
		return false;
	}
	return true;
}

//base64编码
std::string RsaCrypto::toBase64(const char* str, int len)
{
	BIO* mem = BIO_new(BIO_s_mem());
	BIO* base64 = BIO_new(BIO_f_base64());
	//禁止换行
	BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);
	//mem添加到base64
	base64 = BIO_push(base64, mem);
	//写数据
	BIO_write(base64, str, len);
	BIO_flush(base64);

	BUF_MEM* memPtr;
	BIO_get_mem_ptr(base64, &memPtr);
	std::string retStr = std::string(memPtr->data, memPtr->length);
	BIO_free(base64);
	return retStr;
}

//base64解码
std::string RsaCrypto::fromBase64(std::string str)
{
	BIO* base64 = BIO_new(BIO_f_base64());
	BIO* mem = BIO_new_mem_buf(str.data(), str.size());

	BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);

	mem = BIO_push(base64, mem);
	std::string buffer(str.size(), '\0');

	int len = BIO_read(mem, &buffer[0], buffer.size());

	BIO_free_all(mem);

	buffer.resize(len);
	return buffer;
}

bool RsaCrypto::initPrivateKey(std::string prifile)
{
	// OpenSSL BIO 负责读取 PEM 文件并解析出 RSA 私钥结构。
	BIO* bio = BIO_new_file(prifile.data(), "r");
	if (PEM_read_bio_RSAPrivateKey(bio, &m_privateKey, NULL, NULL) == NULL)
	{
		ERR_print_errors_fp(stdout);
		return false;
	}

	BIO_free(bio);
	return true;
}

bool RsaCrypto::initPublicKey(std::string pubfile)
{
	// OpenSSL BIO 负责读取 PEM 文件并解析出 RSA 公钥结构。
	BIO* bio = BIO_new_file(pubfile.data(), "r");
	if (PEM_read_bio_RSAPublicKey(bio, &m_publicKey, NULL, NULL) == NULL)
	{
		ERR_print_errors_fp(stdout);
		return false;
	}

	BIO_free(bio);
	return true;
}
