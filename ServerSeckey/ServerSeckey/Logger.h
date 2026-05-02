#pragma once
#include <iostream>
// =======================================================
// 文件名: Logger.h
// 作用:   简单日志工具类
// 功能:
//   1. 输出普通信息日志
//   2. 输出警告日志
//   3. 输出错误日志
// =======================================================
class Logger
{
public:
	// 输出普通信息日志
	static void info(const std::string& msg);
	// 输出警告日志
	static void warn(const std::string& msg);
	// 输出错误日志
	static void error(const std::string& msg);

private:
	// 获取当前时间的字符串表示，格式为 "YYYY-MM-DD HH:MM:SS"
	static std::string getCurrentTime();
};

