#pragma once
#include "CodecFactory.h"
#include"RequestCodec.h"

// 请求消息的编解码器工厂类，继承自CodecFactory类，实现了创建RequestCodec对象的接口
class RequestFactory :
    public CodecFactory
{
public:
    RequestFactory();
    ~RequestFactory();
    RequestFactory(string enc);
	RequestFactory(RequestInfo* info);
    Codec* createCodec();

private:
    bool m_flag;
    string m_encStr;
	RequestInfo* m_info;
};

