#pragma once
#include<iostream>
#include<string>
// 请求和响应的编解码器的基类，定义了编解码器的接口
class Codec
{
public:
	Codec();
	virtual std::string encodeMsg() = 0;
	virtual void* decodeMsg() = 0;
	//虚析构  （在使用多态的时候使用虚析构）
	virtual ~Codec();
};

