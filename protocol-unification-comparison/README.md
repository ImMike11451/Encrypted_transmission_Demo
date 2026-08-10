# Protocol Unification Comparison

这个目录用于学习对照“统一协议”改造前后的代码。

## 目录说明

```text
protocol-unification-comparison/
  original/   改造前代码
  modified/   改造后代码
```

## 推荐阅读顺序

1. `ClientSecKey/ClientSecKey/MessageV2.proto`
   - 先看协议从“只管消息业务”变成“密钥业务 + 消息业务统一协议”。

2. `ClientSecKey/ClientSecKey/ClientOP.cpp`
   - 对照密钥协商、密钥校验、密钥注销如何从老协议改成 `V2RequestCodec/V2RespondCodec`。

3. `ServerSeckey/ServerSeckey/ServerOP.cpp`
   - 对照服务端如何删除 `V2PK` 分流和老协议处理，统一走 `processV2Request()`。

4. `ClientSecKey/ClientSecKey/V2RequestCodec.*`
   - 看客户端如何新增密钥请求编码结构。

5. `ClientSecKey/ClientSecKey/V2RespondCodec.*`
   - 看客户端如何新增密钥响应解析结构。

6. `ServerSeckey/ServerSeckey/V2RequestCodec.*`
   - 服务端和客户端保持同样协议结构。

7. `ServerSeckey/ServerSeckey/V2RespondCodec.*`
   - 服务端生成 `key_op_resp` 响应。

8. `.vcxproj` 和 `.vcxproj.filters`
   - 看老协议文件如何从 Visual Studio 工程中移除。

## 已删除文件怎么看

老协议文件只存在于 `original/`，`modified/` 中没有对应文件，表示它们已经删除：

- `Message.proto`
- `Message.pb.h`
- `Message.pb.cc`
- `RequestCodec.h/.cpp`
- `RequestFactory.h/.cpp`
- `RespondCodec.h/.cpp`
- `RespondFactory.h/.cpp`
- `CodecFactory.h/.cpp`

`Codec.h/.cpp` 没删除，因为 `V2RequestCodec` 和 `V2RespondCodec` 仍然继承它。

## 生成代码说明

`MessageV2.pb.h/.cc` 是 protobuf 生成代码，不建议作为主要学习对象。真正值得学习的是：

- `MessageV2.proto`
- `ClientOP.cpp`
- `ServerOP.cpp`
- `V2RequestCodec.*`
- `V2RespondCodec.*`
- `MessageClient.cpp`

如果你继续使用 protobuf 3.8，请在项目原来的 Linux 环境中重新生成 `MessageV2.pb.h/.cc`。
