#pragma once
// =======================================================
// 文件名: Base64Util.h
// 作用:   提供Base64编码和解码功能
// 说明:
//   1. 用于替代 ClientOP / ServerOP 中重复的base64函数
// =======================================================
#include <string>
#include <vector>

class Base64Util
{
public:
    // Base64编码
    static std::string encode(const unsigned char* input, int length);

    // Base64解码
    static std::string decode(const std::string& input);
};

