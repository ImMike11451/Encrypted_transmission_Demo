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

// 职责：封装 RSA 密钥生成、公钥加密、私钥解密、签名和验签。
// 边界：RSA 只用于密钥协商和身份证明，不用于加密正文消息；正文消息使用 AES-GCM。
class RsaCrypto
{
public:
	// 初始化空 RSA 公钥和私钥结构。
	RsaCrypto();
	// 根据文件初始化公钥或私钥。
	RsaCrypto(std::string filename, bool isPrivate = true);
	~RsaCrypto();

	// 生成 RSA 密钥对，并写入 PEM 文件。
	void generateKeyPair(std::string pubfile, std::string prifile, int bits = 2048);

	bool loadKey(std::string pubfile, std::string prifile);
	// 公钥加密：服务端用客户端公钥加密会话密钥。
	std::string rsaPublicEncrypt(std::string data);
	// 私钥解密：客户端用自己的私钥解开会话密钥。
	std::string rsaPrivateDecrypt(std::string data);
	// 私钥签名：客户端对摘要签名，证明自己持有私钥。
	std::string rsaSign(std::string data, SignLevel level = Level_4);
	// 公钥验签：服务端验证签名是否来自对应私钥。
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

