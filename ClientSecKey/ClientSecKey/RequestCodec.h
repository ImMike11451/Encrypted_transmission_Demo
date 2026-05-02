#pragma once
#include "Codec.h"
#include "Message.pb.h"
#include<iostream>
using namespace std;

struct RequestInfo
{
    int cmd;
    string clientID;
    string serverID;
    string sign;
    string data;
};

// 请求消息的编解码器，继承自Codec类，实现了Codec类的接口
class RequestCodec :
    public Codec
{
public:
    //空对象
    RequestCodec();
    //构造的对象用于 解码 
    RequestCodec(string encstr);
    //构造的对象用于 编码 
    RequestCodec(RequestInfo* info);
    //解码时使用
	void initMessage(string encstr);
    //编码时使用
    void initMessage(RequestInfo* info);
	//重写父类的纯虚函数->序列化函数，返回编码后（序列化）的字符串
    string encodeMsg();
	//重写父类的纯虚函数->反序列化函数，返回解码后（反序列化）的对象指针
    void* decodeMsg();

	~RequestCodec();

    private:
	//要系列化的数据在这个类中，通过这个类进行序列化和反序列化
    RequestMsg m_msg;
	//存储编码后字符串
    string m_encStr;
};

