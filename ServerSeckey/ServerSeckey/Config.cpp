#include "Config.h"
#include <iostream>
#include <fstream>

bool Config::load(const std::string& fileName)
{
	std::ifstream in(fileName);
	if (!in)
	{
		std::cerr << "配置文件打开失败: " << fileName << std::endl;
		return false;
	}
	
	Json::Reader reader;
	if(!reader.parse(in, m_root))
	{
		std::cerr << "配置文件解析失败: " << reader.getFormattedErrorMessages() << std::endl;
		return false;
	}
	return true;
}

std::string Config::getString(const std::string& key) const
{
	return m_root[key].asString();
}

int Config::getInt(const std::string& key) const
{
	return m_root[key].asInt();
}
