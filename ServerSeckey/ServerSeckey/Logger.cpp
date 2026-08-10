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
	time_t now = time(nullptr);
	char buf[64]{ 0 };

	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
	return std::string(buf);
}
