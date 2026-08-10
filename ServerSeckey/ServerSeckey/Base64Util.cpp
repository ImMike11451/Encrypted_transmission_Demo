#include "Base64Util.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

std::string Base64Util::encode(const unsigned char* input, int length)
{
    // OpenSSL BIO 链：写入 b64 后完成编码，结果落到内存 BIO。
    BIO* bmem = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());

    // 不自动换行，避免协议字段和数据库字段中出现额外换行符。
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);

    std::string result(bptr->data, bptr->length);

    BIO_free_all(b64);

    return result;
}

std::string Base64Util::decode(const std::string& input)
{
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new_mem_buf(input.data(), input.size());

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    mem = BIO_push(b64, mem);

    // Base64 解码后的长度不会超过输入长度。
    std::string output(input.size(), '\0');

    int len = BIO_read(mem, &output[0], output.size());

    BIO_free_all(mem);

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
