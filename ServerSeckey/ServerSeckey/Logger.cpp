#include "Logger.h"

void Logger::info(const std::string& msg)
{
	std::cout << "[" << getCurrentTime() << "] [INFO] " << msg << std::endl;
}

void Logger::warn(const std::string& msg)
{
	std::cout << "[" << getCurrentTime() << "] [WARN] " << msg << std::endl;
}

void Logger::error(const std::string& msg)
{
	std::cout << "[" << getCurrentTime() << "] [ERROR] " << msg << std::endl;
}

std::string Logger::getCurrentTime()
{
	// 获取当前时间戳
	time_t now = time(nullptr);
	char buf[64]{ 0 };

	// 将时间戳转换为本地时间，并格式化为字符串
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
	return std::string(buf);
}
