#pragma once
#include <string>
#include <jsoncpp/json/json.h>

// 职责：加载 json 配置文件，并提供基础类型读取接口。
// 边界：只做配置解析，不负责校验业务语义，例如端口是否可用、路径是否存在。
class Config
{
public:
    // 加载并解析配置文件。
    bool load(const std::string& fileName);

    // 获取字符串配置项。
    std::string getString(const std::string& key) const;

    // 获取整数配置项。
    int getInt(const std::string& key) const;

private:
    // 保存解析后的 json 根节点。
    Json::Value m_root;
};
