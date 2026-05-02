#pragma once
#include "Codec.h"
#include "Message.pb.h"
#include<iostream>
using namespace std;

struct RespondInfo
{
	int status;
	int seckeyID;
	string clientID;
	string serverID;
	string data;
};

// 响应消息的编解码器，继承自Codec类，实现了Codec类的接口
class RespondCodec :
    public Codec
{
public:

	RespondCodec();
	RespondCodec(string encstr);
	RespondCodec(RespondInfo* info);
	void initMessage(string encstr);
	void initMessage(RespondInfo* info);
	string encodeMsg();
	void* decodeMsg();
	~RespondCodec();


private:
	string m_encStr;
	RespondMsg m_msg;

};

