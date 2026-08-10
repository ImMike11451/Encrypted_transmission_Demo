#pragma once
#include <iostream>
// 职责：提供轻量控制台日志输出。
// 边界：当前 Logger 只适合 demo，不支持结构化字段、日志轮转和统一脱敏。
// 安全约束：不要输出会话密钥、明文消息、完整 SQL 或数据库密码。
class Logger
{
public:
	// 输出普通信息日志。
	static void info(const std::string& msg);
	// 输出警告日志。
	static void warn(const std::string& msg);
	// 输出错误日志。
	static void error(const std::string& msg);

private:
	// 获取当前时间的字符串表示，格式为 "YYYY-MM-DD HH:MM:SS"
	static std::string getCurrentTime();
};

