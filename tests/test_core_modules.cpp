#include <iostream>
#include <string>

#include "AesGcmCrypto.h"
#include "Base64Util.h"
#ifdef DEMO_CMAKE_PROTO
#include <MessageV2.pb.h>
#else
#include "MessageV2.pb.h"
#endif
#include "V2RequestCodec.h"
#include "V2RespondCodec.h"

namespace
{
int g_failedCount = 0;

void expectTrue(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[失败] " << message << std::endl;
        ++g_failedCount;
    }
}

V2HeaderInfo makeHeader(int command)
{
    return { "test-message-001", command, "0001", "0002", 1725000000 };
}

void testBase64RoundTrip()
{
    const std::string input("\x00\x01hello\xff", 8);
    const std::string encoded = Base64Util::encode(
        reinterpret_cast<const unsigned char*>(input.data()),
        static_cast<int>(input.size())
    );

    expectTrue(!encoded.empty(), "Base64 编码结果不应为空。");
    expectTrue(Base64Util::decode(encoded) == input, "Base64 应保留二进制数据并完成往返还原。");
}

void testAesGcmRoundTrip()
{
    const std::string key = "1234567890abcdef";
    const std::string plaintext = "confidential message";
    AesGcmCrypto crypto(key);

    const GcmEncryptResult encrypted = crypto.encrypt(plaintext);
    expectTrue(encrypted.success, "AES-GCM 加密应成功。");
    expectTrue(encrypted.nonce.size() == 12, "AES-GCM nonce 长度应为 12 字节。");
    expectTrue(encrypted.tag.size() == 16, "AES-GCM tag 长度应为 16 字节。");

    const GcmDecryptResult decrypted = crypto.decrypt(
        encrypted.nonce,
        encrypted.ciphertext,
        encrypted.tag
    );
    expectTrue(decrypted.success, "AES-GCM 解密应成功。");
    expectTrue(decrypted.plaintext == plaintext, "AES-GCM 解密结果应等于原始明文。");

    std::string tamperedTag = encrypted.tag;
    tamperedTag[0] = static_cast<char>(tamperedTag[0] ^ 0x01);
    const GcmDecryptResult tampered = crypto.decrypt(
        encrypted.nonce,
        encrypted.ciphertext,
        tamperedTag
    );
    expectTrue(!tampered.success, "AES-GCM 应拒绝被篡改的认证标签。");
}

void testAesGcmRejectsInvalidKey()
{
    AesGcmCrypto crypto("invalid-key");
    const GcmEncryptResult encrypted = crypto.encrypt("message");

    expectTrue(!encrypted.success, "AES-GCM 应拒绝不支持的密钥长度。");
    expectTrue(encrypted.errorMsg == "unsupported AES key length", "无效密钥长度应返回明确错误信息。");
}

void testRequestCodecRoundTrip()
{
    V2SendMessageRequestInfo request{};
    request.header = makeHeader(secmng::v2::CMD_SEND_MSG_REQ);
    request.message = { 42, secmng::v2::MSG_TYPE_TEXT, "cipher", "nonce", "tag", "AES-128-GCM" };

    V2RequestCodec encoder(&request);
    V2RequestCodec decoder(encoder.encodeMsg());
    auto* packet = static_cast<secmng::v2::RequestPacket*>(decoder.decodeMsg());

    expectTrue(packet->has_header(), "请求解码后应包含公共头。");
    expectTrue(packet->header().message_id() == request.header.messageId, "请求头消息 ID 应保持一致。");
    expectTrue(packet->header().command() == secmng::v2::CMD_SEND_MSG_REQ, "请求命令类型应保持一致。");
    expectTrue(packet->has_send_msg_req(), "请求解码后应包含发送消息体。");
    expectTrue(packet->send_msg_req().message().key_id() == 42, "请求密钥 ID 应保持一致。");
    expectTrue(packet->send_msg_req().message().algorithm() == "AES-128-GCM", "请求算法字段应保持一致。");
}

void testResponseCodecRoundTrip()
{
    V2SendMessageResponseInfo response{};
    response.header = makeHeader(secmng::v2::CMD_SEND_MSG_RESP);
    response.code = secmng::v2::RESULT_SUCCESS;
    response.message = "accepted";
    response.serverMessageId = "server-msg-001";
    response.serverTime = 1725000001;
    response.deliveryStatus = secmng::v2::DELIVERY_ACCEPTED;

    V2RespondCodec encoder(&response);
    V2RespondCodec decoder(encoder.encodeMsg());
    auto* packet = static_cast<secmng::v2::ResponsePacket*>(decoder.decodeMsg());

    expectTrue(packet->has_send_msg_resp(), "响应解码后应包含发送消息响应体。");
    expectTrue(packet->send_msg_resp().code() == secmng::v2::RESULT_SUCCESS, "响应结果码应保持一致。");
    expectTrue(packet->send_msg_resp().server_message_id() == response.serverMessageId, "服务端消息 ID 应保持一致。");
    expectTrue(packet->send_msg_resp().delivery_status() == secmng::v2::DELIVERY_ACCEPTED, "投递状态应保持一致。");
}
}

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    testBase64RoundTrip();
    testAesGcmRoundTrip();
    testAesGcmRejectsInvalidKey();
    testRequestCodecRoundTrip();
    testResponseCodecRoundTrip();

    google::protobuf::ShutdownProtobufLibrary();

    if (g_failedCount == 0)
    {
        std::cout << "全部核心模块测试通过。" << std::endl;
        return 0;
    }

    std::cerr << "核心模块测试失败数量: " << g_failedCount << std::endl;
    return 1;
}
