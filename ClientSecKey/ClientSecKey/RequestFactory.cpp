#include "RequestFactory.h"

RequestFactory::RequestFactory() {};

RequestFactory::RequestFactory (string enc)
{
	m_flag = true;
	m_encStr = enc;
}

RequestFactory::RequestFactory(RequestInfo* info)
{
	m_flag = false;
	m_info = info;
}

Codec* RequestFactory::createCodec()
{
	if (m_flag)
	{
		return new RequestCodec(m_encStr);
	}
	else
	{
		return new RequestCodec(m_info);
	}
}

RequestFactory::~RequestFactory() {};

