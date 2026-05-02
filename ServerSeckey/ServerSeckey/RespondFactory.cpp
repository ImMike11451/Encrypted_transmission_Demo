#include "RespondFactory.h"

RespondFactory::RespondFactory() {};

RespondFactory::RespondFactory(string enc) {
	m_flag = true;
	m_encStr = enc;
};

RespondFactory::RespondFactory(RespondInfo* info) {
	m_flag = false;
	m_info = *info;
};

Codec* RespondFactory::createCodec() {
	if (m_flag) {
		return new RespondCodec(m_encStr);
	}
	else {
		return new RespondCodec(&m_info);
	}
};

RespondFactory::~RespondFactory() {};
