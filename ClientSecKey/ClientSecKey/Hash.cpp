#include "Hash.h"

# define MD5_DIGEST_LENGTH 16	// md5哈希值长度
# define SHA_DIGEST_LENGTH		 20
# define SHA224_DIGEST_LENGTH    28
# define SHA256_DIGEST_LENGTH    32
# define SHA384_DIGEST_LENGTH    48
# define SHA512_DIGEST_LENGTH    64

Hash::Hash(HashType type)
{
	my_type = type;		
	switch (type)
	{
	case T_MD5:
		initMD5();
		break;
	case T_SHA1:
		initSHA1();
		break;
	case T_SHA224:
		initSHA224();
		break;
	case T_SHA256:
		initSHA256();
		break;
	case T_SHA384:
		initSHA384();
		break;
	case T_SHA512:
		initSHA512();
		break;
	default:
		initMD5();
		break;
	}
}

Hash::~Hash()
{
}

void Hash::addData(std::string data)
{
	size_t len = data.size();
	switch (my_type)
	{
	case T_MD5:
		updateMD5(data.c_str(), len);
		break;
	case T_SHA1:
		updateSHA1(data.c_str(), len);
		break;
	case T_SHA224:
		updateSHA224(data.c_str(), len);
		break;
	case T_SHA256:
		updateSHA256(data.c_str(), len);
		break;
	case T_SHA384:
		updateSHA384(data.c_str(), len);
		break;
	case T_SHA512:
		updateSHA512(data.c_str(), len);
		break;
	default:
		updateMD5(data.c_str(), len);
		break;
	}
}

std::string Hash::result()
{
	switch (my_type)
	{
	case T_MD5:
		return md5Result();
	case T_SHA1:
		return sha1Result();
	case T_SHA224:
		return sha224Result();
	case T_SHA256:
		return sha256Result();
	case T_SHA384:
		return sha384Result();
	case T_SHA512:
		return sha512Result();
	default:
		return md5Result();
	}
}

std::string Hash::md5Result()
{
	//md5哈希值长度为16字节，每个字节用两位十六进制表示，所以结果字符串长度为32+ '/0'
	unsigned char digest[MD5_DIGEST_LENGTH];
	char res[MD5_DIGEST_LENGTH * 2 + 1];
	MD5_Final(digest, &md5);
	for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
	{
		//将每个字节转换为两位十六进制字符串，并存储在res中
		sprintf(res + i * 2, "%02x", digest[i]);
	}
	return std::string(res,MD5_DIGEST_LENGTH * 2);
}

std::string Hash::sha1Result()
{
	unsigned char digest[SHA_DIGEST_LENGTH];
	char res[SHA_DIGEST_LENGTH * 2 + 1];
	SHA1_Final(digest, &sha1);
	for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
	{
		sprintf(res + i * 2, "%02x", digest[i]);
	}
	return std::string(res, SHA_DIGEST_LENGTH * 2);
}

std::string Hash::sha224Result()
{
	unsigned char digest[SHA224_DIGEST_LENGTH];
	char res[SHA224_DIGEST_LENGTH * 2 + 1];
	SHA224_Final(digest, &sha224);
	for (int i = 0; i < SHA224_DIGEST_LENGTH; i++)
	{
		sprintf(res + i * 2, "%02x", digest[i]);
	}
	return std::string(res, SHA224_DIGEST_LENGTH * 2);
}

std::string Hash::sha256Result()
{
	unsigned char digest[SHA256_DIGEST_LENGTH];
	char res[SHA256_DIGEST_LENGTH * 2 + 1];
	SHA256_Final(digest, &sha256);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sprintf(res + i * 2, "%02x", digest[i]);
	}
	return std::string(res, SHA256_DIGEST_LENGTH * 2);
}

std::string Hash::sha384Result()
{
	unsigned char digest[SHA384_DIGEST_LENGTH];
	char res[SHA384_DIGEST_LENGTH * 2 + 1];
	SHA384_Final(digest, &sha384);
	for (int i = 0; i < SHA384_DIGEST_LENGTH; i++)
	{
		sprintf(res + i * 2, "%02x", digest[i]);
	}
	return std::string(res, SHA384_DIGEST_LENGTH * 2);
}

std::string Hash::sha512Result()
{
	unsigned char digest[SHA512_DIGEST_LENGTH];
	char res[SHA512_DIGEST_LENGTH * 2 + 1];
	SHA512_Final(digest, &sha512);
	for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
	{
		sprintf(res + i * 2, "%02x", digest[i]);
	}
	return std::string(res, SHA512_DIGEST_LENGTH * 2);
}


