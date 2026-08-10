#pragma once
#include <string>
#include <vector>

// 职责：提供 Base64 编码和解码。
// 边界：Base64 只是二进制到文本的编码，不提供任何保密能力。
class Base64Util
{
public:
    // 把任意二进制数据编码成可放入 protobuf string / MySQL 文本字段的字符串。
    static std::string encode(const unsigned char* input, int length);

    // 把 Base64 字符串还原成原始二进制数据。
    static std::string decode(const std::string& input);
};

