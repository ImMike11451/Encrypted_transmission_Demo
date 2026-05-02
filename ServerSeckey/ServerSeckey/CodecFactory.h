#pragma once
#include"Codec.h"
#include<iostream>
using namespace std;

// 编解码器工厂类，定义了创建编解码器对象的接口
class CodecFactory
{
public:
	CodecFactory() {};
	~CodecFactory() {};
	virtual Codec* createCodec()
	{
		return nullptr;
	}
};

