#pragma once
#include <iostream>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <string>

enum HashType
{
	T_MD5,
	T_SHA1,
	T_SHA224,
	T_SHA256,
	T_SHA384,
	T_SHA512
};

class Hash
{
public:
	Hash(HashType type);
	~Hash();

	void addData(std::string data);
	std::string result();

private:

	//md5
	inline void initMD5() { MD5_Init(&md5); }
	inline void updateMD5(const void* data, size_t len) { MD5_Update(&md5, data, len); }
	std::string md5Result();

	//sha1
	inline void initSHA1() { SHA1_Init(&sha1); }
	inline void updateSHA1(const void* data, size_t len) { SHA1_Update(&sha1, data, len); }
	std::string sha1Result();

	//sha224
	inline void initSHA224() { SHA224_Init(&sha224); }
	inline void updateSHA224(const void* data, size_t len) { SHA224_Update(&sha224, data, len); }
	std::string sha224Result();

	//sha256
	inline void initSHA256() { SHA256_Init(&sha256); }
	inline void updateSHA256(const void* data, size_t len) { SHA256_Update(&sha256, data, len); }
	std::string sha256Result();

	//sha384
	inline void initSHA384() { SHA384_Init(&sha384); }
	inline void updateSHA384(const void* data, size_t len) { SHA384_Update(&sha384, data, len); }
	std::string sha384Result();

	//sha512
	inline void initSHA512() { SHA512_Init(&sha512); }
	inline void updateSHA512(const void* data, size_t len) { SHA512_Update(&sha512, data, len); }
	std::string sha512Result();

	HashType my_type;
	MD5_CTX md5;
	SHA_CTX sha1;
	SHA256_CTX sha224;
	SHA256_CTX sha256;
	SHA512_CTX sha384;
	SHA512_CTX sha512;
};	

