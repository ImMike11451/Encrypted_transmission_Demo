# 加密传输与密钥管理演示系统：当前架构基线

## 1. 项目目标

本项目是一个基于 C++、Linux 网络编程、OpenSSL、Protobuf 与 MySQL 的安全消息通信演示系统。

当前系统支持两个客户端节点通过服务端完成：

- 会话密钥协商
- 密钥状态校验
- 密钥注销
- AES-GCM 加密文本消息发送
- 服务端消息转发存储
- 单条消息元数据查询
- 最近消息列表查询

当前实现属于服务端可解密的消息转发模型，不属于真正的端到端加密模型。

## 2. 当前可执行程序

### 客户端

客户端工程目录：

```text
ClientSecKey/ClientSecKey/
```

入口文件：

```text
ClientSecKey/ClientSecKey/main.cpp
```

当前交互菜单：

1. 密钥协商
2. 密钥校验
3. 发送加密消息
4. 查询消息
5. 查询消息列表
6. 密钥注销
7. 退出

### 服务端

服务端工程目录：

```text
ServerSeckey/ServerSeckey/
```

入口文件：

```text
ServerSeckey/ServerSeckey/main.cpp
```

服务端启动后监听配置文件指定端口，使用 epoll 和线程池处理客户端短连接请求。

## 3. 当前客户端身份

当前联调配置包含两个客户端：

| 客户端 | 客户端 ID | 配置文件 | 用途 |
|---|---|---|---|
| Client A | 0001 | `clientA.json` | 发送消息测试 |
| Client B | 0002 | `clientB.json` | 接收方密钥准备测试 |

服务端 ID：

```text
6789
```

默认服务端端口：

```text
9898
```

## 4. 当前模块职责

### 客户端侧

| 模块 | 职责 |
|---|---|
| `ClientOP` | 菜单业务协调、密钥协商、校验、注销、调用消息业务 |
| `MessageClient` | V2 消息加密、消息发送、单条查询、列表查询 |
| `TcpSocket` | TCP 连接和长度帧收发 |
| `SecKeyShm` | 本地会话密钥共享内存读写 |
| `RsaCrypto` | RSA 密钥生成、签名、加解密 |
| `AesGcmCrypto` | AES-GCM 消息加解密 |
| `V2RequestCodec` / `V2RespondCodec` | 统一 protobuf 协议编解码，覆盖密钥业务和消息业务 |

### 服务端侧

| 模块 | 职责 |
|---|---|
| `ServerOP` | 配置初始化、网络监听、统一协议路由、密钥业务 |
| `MessageService` | 发送消息、查询消息、消息列表查询业务 |
| `MessageRepository` | 消息记录写入和查询封装 |
| `AuditService` | 审计日志业务封装 |
| `mysqlOP` | MySQL 连接与底层 SQL 操作 |
| `SecKeyShm` | 服务端活跃会话密钥缓存 |
| `EpollServer` | epoll 网络事件管理 |
| `ThreadPool` | 并发任务执行 |

## 5. 当前协议设计

当前项目统一使用一套 protobuf 协议。

### 统一消息协议

协议文件：

```text
MessageV2.proto
```

用途：

- 密钥协商
- 密钥校验
- 密钥注销
- 发送加密消息
- 查询单条消息
- 查询最近消息列表

核心消息：

```text
RequestPacket
ResponsePacket
Header
EncryptedMessage
SendMessageRequest
QueryMessageRequest
QueryMessageListRequest
KeyAgreementRequest
KeyCheckRequest
KeyLogoutRequest
KeyOperationResponse
```

老协议文件 `Message.proto`、`Message.pb.*`、`RequestCodec`、`RespondCodec`、`RequestFactory`、`RespondFactory`、`CodecFactory` 已删除。

网络层不再使用 `V2PK` 前缀区分协议，长度帧中的 payload 直接是 `MessageV2` 的 protobuf 字节流。

## 6. 当前密钥协商流程

当前密钥协商流程如下：

1. 客户端生成 RSA 公私钥对。
2. 客户端通过 `KeyAgreementRequest` 发送公钥及签名给服务端。
3. 服务端验证公钥签名。
4. 服务端生成 16 字节随机 AES 会话密钥。
5. 服务端使用客户端公钥加密 AES 密钥。
6. 服务端将 Base64 编码后的会话密钥写入 MySQL 和共享内存。
7. 客户端使用私钥解密 AES 密钥。
8. 客户端将 Base64 编码后的会话密钥写入本地共享内存。

当前会话密钥缓存结构包含：

```text
status
seckeyID
clientID
serverID
seckey
```

## 7. 当前消息发送流程

当前 A 向 B 发送消息的流程如下：

1. Client A 从本地共享内存读取 A-Server 会话密钥。
2. Client A 使用 AES-GCM 加密明文消息。
3. Client A 将密文、nonce、tag 和算法信息封装为 `SendMessageRequest`。
4. Server 从共享内存读取 A-Server 会话密钥。
5. Server 解密 Client A 发来的密文。
6. Server 从共享内存读取 B-Server 会话密钥。
7. Server 使用 B-Server 会话密钥重新加密明文。
8. Server 将发送方密文和接收方密文一并写入 `message_log`。
9. Server 写入审计日志。
10. Server 返回 `server_message_id` 给 Client A。

重要说明：

当前服务端能够看到消息明文，因此该流程不是端到端加密。

## 8. 当前存储模型

### `seckeyinfo`

用途：

- 保存客户端与服务端之间的会话密钥；
- 保存密钥状态。

### `keysn`

用途：

- 分配递增密钥 ID。

### `message_log`

用途：

- 保存消息发送方、接收方；
- 保存发送侧密文；
- 保存接收侧重加密后的密文；
- 保存 nonce、tag、算法、时间和状态。

当前消息记录主要字段：

```text
msg_id
sender_id
receiver_id
sender_key_id
receiver_key_id
msg_type
sender_ciphertext
sender_nonce
sender_tag
receiver_ciphertext
receiver_nonce
receiver_tag
algorithm
send_time
status
```

### `audit_log`

用途：

- 记录消息发送、消息查询、解密失败、投递失败等行为。

## 9. 当前网络模型

服务端当前使用：

```text
epoll + thread pool + short TCP connection
```

每次请求的网络数据采用长度帧：

```text
4-byte network-order payload length + protobuf payload
```

当前请求处理大致流程：

```text
accept connection
-> receive one request
-> decode MessageV2 RequestPacket
-> process business request
-> send one response
-> close connection
```

## 10. 当前功能完成情况

| 功能 | 状态 |
|---|---|
| 密钥协商 | 已完成 |
| 密钥校验 | 已完成 |
| 密钥注销 | 已完成 |
| AES-GCM 文本消息发送 | 已完成 |
| 服务端解密与接收方重加密存储 | 已完成 |
| 单条消息元数据查询 | 已完成 |
| 最近消息列表查询 | 已完成 |
| 双客户端联调配置 | 已完成 |
| 敏感密钥日志治理 | 已完成 |
| 服务端明文日志治理 | 已完成 |
| 查询权限基础控制 | 已完成 |
| 更低碰撞概率的消息/审计 ID | 已完成 |
| 接收方获取密文并本地解密 | 未实现 |
| 消息已读回执 | 未实现 |
| 端到端加密 | 未实现 |
| 长连接实时推送 | 未实现 |

## 11. 已知结构问题

当前识别出的结构问题包括：

1. 客户端和服务端存在大量重复源码，例如 crypto、socket、logger、codec 与 protobuf 生成文件。
2. `ServerOP` 同时负责网络、初始化、协议分发和密钥业务，职责过重。
3. `ClientOP` 同时承担交互和业务编排，后续接入 GUI 时迁移成本较高。
4. 当前错误处理依赖 `bool`、`int` 和日志输出，无法稳定表达失败原因。
5. 当前自定义 `Logger` 不支持结构化日志、日志文件轮转和统一安全策略。
6. 数据库访问集中在 `mysqlOP`，且业务领域边界不清晰。
7. System V 共享内存与业务层直接耦合，未来替换 Redis 成本较高。
8. 当前构建依赖 Visual Studio Linux 工程与机器相关绝对路径，缺少顶层 CMake 结构。

## 12. 已知安全问题

当前识别出的安全问题包括：

1. 服务端可解密所有消息明文。
2. 动态 SQL 已使用 MySQL 长度感知转义，但尚未迁移到 `mysql_stmt` 参数化查询。
3. 完整 SQL 日志可能包含密钥或密文。
4. 数据库账号密码保存在版本控制中的配置文件里。
5. 测试私钥文件已经进入版本控制，需要明确其只用于演示环境。
6. 消息 ID 和审计 ID 已降低碰撞概率，但后续仍建议升级为 UUID。
7. Protobuf 解析结果检查和异常输入处理仍不完整。

已完成的安全治理：

1. 客户端不再输出协商后的会话密钥。
2. 服务端不再输出解密后的消息明文。
3. 单条消息查询只允许发送方或接收方查询。
4. 最近消息列表查询只允许查询当前客户端自己的发送记录。
5. 消息 ID 和审计 ID 不再只依赖秒级时间戳。

## 13. 重构约束

后续重构过程中应保持以下已有行为：

- 客户端仍可执行密钥协商、校验和注销。
- Client A 仍可发送加密消息给 Client B。
- 服务端仍能保存消息记录和审计记录。
- 客户端仍可查询单条消息元数据和最近消息列表。
- 每个重构阶段结束时工程应可以编译和手工验证。

后续重构过程中暂不同时引入：

- Redis
- 端到端加密
- Qt UI
- 长连接推送
- 群聊或文件传输

这些能力应在现有结构完成治理后再继续开发。

## 14. 后续重构顺序

后续计划按以下顺序推进：

1. 建立当前架构与安全基线文档。
2. 整理敏感配置、测试密钥与敏感日志策略。
3. 建立顶层 CMake 工程并提取公共模块。
4. 将自定义 `Logger` 迁移为 `spdlog`。
5. 引入统一的 `Error` 与 `Result<T>` 错误模型。
6. 拆分 `ServerOP`、`ClientOP` 与存储接口。
7. 清理已删除老协议文件后的构建配置与文档。
8. 增加测试、部署文档与持续集成。
9. 在结构稳定后，引入 Redis、端到端加密和实时推送。
