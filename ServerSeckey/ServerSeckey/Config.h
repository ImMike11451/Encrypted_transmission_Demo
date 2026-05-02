#pragma once
#include <string>
#include <jsoncpp/json/json.h>

// =======================================================
// 文件名: Config.h
// 作用:   配置文件读取工具类
// 功能:
//   1. 加载json配置文件
//   2. 获取字符串配置项
//   3. 获取整数配置项
// =======================================================

class Config
{
public:
    // 加载配置文件
    bool load(const std::string& fileName);

    // 获取字符串配置
    std::string getString(const std::string& key) const;

    // 获取整数配置
    int getInt(const std::string& key) const;

private:
    // 保存解析后的json根节点
    Json::Value m_root;
};