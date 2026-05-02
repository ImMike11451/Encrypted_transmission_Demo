#pragma once
#include <iostream>
#include <string>
#include <openssl/rsa.h>
#include <openssl/pem.h>

enum SignLevel
{
	Level_1 = NID_md5,
	Level_2 = NID_sha1,
	Level_3 = NID_sha224,
	Level_4 = NID_sha256,
	Level_5 = NID_sha384,
	Level_6 = NID_sha512
};

class RsaCrypto
{
public:
	// 构造函数：初始化公钥和私钥结构体
	RsaCrypto();
	// 构造函数（带参数）：根据文件初始化密钥
	RsaCrypto(std::string filename, bool isPrivate = true);
	~RsaCrypto();

	// 生成 RSA 密钥对（公钥 + 私钥）
	void generateKeyPair(std::string pubfile, std::string prifile, int bits = 2048);

	bool loadKey(std::string pubfile, std::string prifile);
	// 公钥加密
	std::string rsaPublicEncrypt(std::string data);
	// 私钥解密
	std::string rsaPrivateDecrypt(std::string data);
	// 私钥签名
	std::string rsaSign(std::string data, SignLevel level = Level_4);
	// 公钥验签
	bool rsaVerify(std::string data, std::string signData, SignLevel level = Level_4);


private:
	//base64编码
	std::string toBase64(const char* str, int len);
	//base64解码
	std::string fromBase64(std::string str);
	// 初始化私钥（从文件读取）
	bool initPrivateKey(std::string prifile);
	// 初始化公钥
	bool initPublicKey(std::string pubfile);

	RSA* m_publicKey;
	RSA* m_privateKey;
};

