#include "Base64Util.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// =======================================================
// Base64编码
// 参数:
//   input  - 原始数据指针
//   length - 原始数据长度
// 返回:
//   Base64编码后的字符串
// =======================================================
std::string Base64Util::encode(const unsigned char* input, int length)
{
    // 创建内存 BIO，用于保存最终的编码结果
    BIO* bmem = BIO_new(BIO_s_mem());

    // 创建 Base64 BIO
    BIO* b64 = BIO_new(BIO_f_base64());

    // 设置不自动换行
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // 把内存BIO接到Base64 BIO后面
    // 数据流: 写入 b64 -> 编码 -> 保存到 bmem
    b64 = BIO_push(b64, bmem);

    // 写入原始数据
    BIO_write(b64, input, length);

    // 刷新BIO，确保数据写入完成
    BIO_flush(b64);

    // 获取内存BIO中的数据指针
    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);

    // 构造结果字符串
    std::string result(bptr->data, bptr->length);

    // 释放整条BIO链
    BIO_free_all(b64);

    return result;
}

// =======================================================
// Base64解码
// 参数:
//   input - Base64字符串
// 返回:
//   解码后的原始字符串
// =======================================================
std::string Base64Util::decode(const std::string& input)
{
    // 创建Base64解码BIO
    BIO* b64 = BIO_new(BIO_f_base64());

    // 创建内存BIO，读取输入字符串
    BIO* mem = BIO_new_mem_buf(input.data(), input.size());

    // 设置不换行
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // 建立BIO链
    mem = BIO_push(b64, mem);

    // 申请输出缓冲区
    std::string output(input.size(), '\0');

    // 执行解码
    int len = BIO_read(mem, &output[0], output.size());

    // 释放BIO链
    BIO_free_all(mem);

    // 调整最终长度
    if (len > 0)
    {
        output.resize(len);
    }
    else
    {
        output.clear();
    }

    return output;
}