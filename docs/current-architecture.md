# 当前架构

[返回项目首页](../README.md)

## 架构概览

项目采用客户端—服务端结构。客户端负责密钥协商、文本加密和查询；服务端负责请求分发、密钥校验、消息解密与重加密、数据库存储和审计。

## 模块与职责

| 区域 | 主要模块 | 职责 |
| --- | --- | --- |
| 客户端 | `ClientOP`、`MessageClient` | 菜单编排、密钥操作、文本加密、请求发送和响应解析 |
| 通信与协议 | `TcpSocket`、V2 编解码器、`MessageV2.proto` | 短连接收发、长度帧与 Protobuf 请求/响应编解码 |
| 服务端入口 | `ServerOP`、`EpollServer`、`ThreadPool` | 监听连接、协议路由、向线程池投递请求任务 |
| 密钥管理 | `RsaCrypto`、`SecKeyShm`、`mysqlOP` | RSA 协商、共享内存缓存、密钥生命周期持久化 |
| 消息服务 | `MessageService`、`MessageQueryService` | 解密并重加密、消息存储、查询授权与审计 |
| 数据访问 | `MessageRepository`、`AuditService` 及接口 | 将查询服务与 MySQL 实现解耦，支持内存替身测试 |

## 消息处理流程

```text
Client A
  └─ 读取 A-Server 活跃会话密钥，AES-128-GCM 加密文本
       └─ RequestPacket / SendMessageRequest
            └─ TCP 短连接（4 字节网络序长度 + Protobuf payload）
                 └─ ServerOP → MessageService
                      ├─ 回源确认 A-Server 密钥状态并解密
                      ├─ 回源确认 B-Server 密钥状态并重新加密
                      ├─ 写入 message_log（发送侧与接收侧密文）
                      └─ 写入 audit_log，返回 server_message_id
```

消息查询返回消息 ID、发送方、接收方、时间和状态等元数据。

## 协议、状态与数据

协议源为 [`MessageV2.proto`](../ServerSeckey/ServerSeckey/MessageV2.proto)。CMake 使用本机 `protoc` 在构建目录中生成协议代码。

| 项目 | 说明 |
| --- | --- |
| 请求类型 | 密钥协商、校验、注销；消息发送；单条与最近消息列表查询 |
| 加密封装 | `EncryptedMessage` 包含密文、nonce、tag、算法和密钥编号；当前消息路径使用 AES-128-GCM |
| 密钥状态 | 活跃 `1`、已注销 `2`、已过期 `3`、已轮换 `4` |
| 有效期与轮换 | 新密钥默认 24 小时；访问时推进过期状态；同一客户端再次协商时轮换原活跃密钥 |
| 密钥存储 | MySQL 是生命周期权威源，System V 共享内存只缓存当前可用密钥 |
| `message_log` | 保存发送方/接收方、两侧密文及其 nonce、tag、算法、时间和状态 |
| `audit_log` | 记录消息发送、查询及失败等审计事件 |

单条查询核对声明的查询者是否为发送方或接收方；列表查询限制为该声明身份的发送记录。空请求者会在访问仓储前被拒绝，并记录失败审计。

## 本地运行布局

运行环境为 Linux/WSL2：先执行 `bash scripts/setup-demo.sh`，再按 `start-server.sh`、`start-client-b.sh`、`start-client-a.sh` 的顺序启动。默认构建目录为仓库 `build/`，可通过 `BUILD_DIR` 指定相对仓库路径或绝对路径。

| 角色 | 目录 | 配置 |
| --- | --- | --- |
| 服务端 | `runtime/server/` | `server.json` |
| Client A | `runtime/client-a/` | `clientA.json` |
| Client B | `runtime/client-b/` | `clientB.json` |

各配置的 `ShmKey` 使用相对本运行目录的 `./shm`。配置脚本只创建缺失的目录和配置，保留已有配置。运行目录和构建目录由本机生成。

## 实现说明

- 服务端使用发送方会话密钥解密，再使用接收方会话密钥加密并保存消息。
- 查询服务通过仓储和审计接口访问数据，测试中使用内存替身。
- 查询归属依据请求中的节点 ID 与消息参与方进行比较。
- 线程池中的任务共用 MySQL 连接，部署操作按顺序完成密钥协商和消息发送。

## 源码阅读顺序

1. [`MessageV2.proto`](../ServerSeckey/ServerSeckey/MessageV2.proto)：请求、响应和字段边界。
2. [`ClientOP.h`](../ClientSecKey/ClientSecKey/ClientOP.h) 与 [`MessageClient.h`](../ClientSecKey/ClientSecKey/MessageClient.h)：客户端交互和发送职责。
3. [`ServerOP.h`](../ServerSeckey/ServerSeckey/ServerOP.h) 与 [`MessageService.h`](../ServerSeckey/ServerSeckey/MessageService.h)：服务端路由和消息主链路。
4. [`MessageQueryService.h`](../ServerSeckey/ServerSeckey/MessageQueryService.h) 与 [`mysqlOP.h`](../ServerSeckey/ServerSeckey/mysqlOP.h)：查询授权和数据访问边界。
