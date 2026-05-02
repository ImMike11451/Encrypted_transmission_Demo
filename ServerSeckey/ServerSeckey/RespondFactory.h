#pragma once
#include "CodecFactory.h"
#include "RespondCodec.h" 
#include<iostream>

// 响应消息的编解码器工厂类，继承自CodecFactory类，实现了创建RespondCodec对象的接口
using namespace std;
class RespondFactory :
    public CodecFactory
{
public:
    RespondFactory();
    ~RespondFactory();
    RespondFactory(string enc);
    RespondFactory(RespondInfo* info);
	Codec* createCodec();

private:
    bool m_flag;
	string m_encStr;
    RespondInfo m_info;
};

