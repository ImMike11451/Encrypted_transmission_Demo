# 当前架构

## 定位与边界

这是一个面向学习、演示和面试交流的 C++17 Linux 加密传输 Demo，覆盖会话密钥协商、AES-GCM 文本消息发送、服务端存储和消息元数据查询；并非生产级即时通信系统。

服务端会解密发送方密文，再以接收方与服务端之间的会话密钥重新加密后存储。因此它是“服务端可信、可见明文”的转发模型，**不是端到端加密**。

## 模块与职责

| 区域 | 主要模块 | 职责 |
| --- | --- | --- |
| 客户端 | `ClientOP`、`MessageClient` | 菜单编排、密钥操作、文本加密、请求发送和响应解析 |
| 通信与协议 | `TcpSocket`、V2 编解码器、`MessageV2.proto` | 短连接收发、长度帧与 Protobuf 请求/响应编解码 |
| 服务端入口 | `ServerOP`、`EpollServer`、`ThreadPool` | 监听连接、协议路由、向线程池投递请求任务 |
| 密钥管理 | `RsaCrypto`、`SecKeyShm`、`mysqlOP` | RSA 协商、共享内存缓存、密钥生命周期持久化 |
| 消息服务 | `MessageService`、`MessageQueryService` | 解密并重加密、消息存储、查询授权与审计 |
| 持久化接缝 | `MessageRepository`、`AuditService` 及接口 | 将查询服务与 MySQL 实现解耦，支持内存替身测试 |

## 真实消息流

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

客户端 B 当前没有“下载接收侧密文并本地解密”的实现；消息查询返回元数据而非消息正文或密文。

## 协议、状态与数据

协议源为 [`MessageV2.proto`](../ServerSeckey/ServerSeckey/MessageV2.proto)。构建入口会以本机 `protoc` 生成构建目录中的代码；源码树保留的旧生成文件仅为兼容用途，字段定义不变。

| 项目 | 当前事实 |
| --- | --- |
| 请求类型 | 密钥协商、校验、注销；消息发送；单条与最近消息列表查询 |
| 加密封装 | `EncryptedMessage` 包含密文、nonce、tag、算法和密钥编号；当前消息路径使用 AES-128-GCM |
| 密钥状态 | 活跃 `1`、已注销 `2`、已过期 `3`、已轮换 `4` |
| 有效期与轮换 | 新密钥默认 24 小时；访问时推进过期状态；同一客户端再次协商才轮换旧活跃密钥，不是后台定时或周期轮换 |
| 密钥存储 | MySQL 是生命周期权威源，System V 共享内存只缓存当前可用密钥 |
| `message_log` | 保存发送方/接收方、两侧密文及其 nonce、tag、算法、时间和状态 |
| `audit_log` | 记录消息发送、查询及失败等审计事件 |

单条查询核对声明的查询者是否为发送方或接收方；列表查询限制为该声明身份的发送记录。这是基础归属校验，不能等同于完整身份认证。

## 本地运行布局

发布入口面向 Linux/WSL2：先执行 `bash scripts/setup-demo.sh`，再按 `start-server.sh`、`start-client-b.sh`、`start-client-a.sh` 的顺序启动。默认构建目录为仓库 `build/`，可通过 `BUILD_DIR` 指定相对仓库路径或绝对路径。

| 角色 | 目录 | 配置 |
| --- | --- | --- |
| 服务端 | `runtime/server/` | `server.json` |
| Client A | `runtime/client-a/` | `clientA.json` |
| Client B | `runtime/client-b/` | `clientB.json` |

各配置的 `ShmKey` 使用相对本运行目录的 `./shm`。初始化脚本只在缺失时创建目录和配置，不覆盖本机配置、不会迁移数据库、也不会复制历史真实配置。运行目录和构建目录不应提交。

## 安全与并发边界

- 服务端可短暂访问明文；当前没有端到端加密。
- 私钥、真实 JSON 配置、数据库凭据和运行时生成公钥均不应提交或写入日志。
- SQL 使用长度感知转义，但尚未全面切换到 `mysql_stmt` 参数化查询。
- 协议当前没有完整的防重放设计。
- 单条与列表查询均在访问仓储前拒绝空请求者，并记录失败审计。
- `ThreadPool` 的任务会共用一个 `mysqlOP` 连接；密钥轮换与共享内存缓存之间也没有整体并发同步。当前不能据此宣称并发轮换一致性或线程安全。
- 没有已读回执、长连接推送、接收方下载解密、性能基准或生产级可用性承诺。

## 建议阅读顺序

1. [`MessageV2.proto`](../ServerSeckey/ServerSeckey/MessageV2.proto)：请求、响应和字段边界。
2. [`ClientOP.h`](../ClientSecKey/ClientSecKey/ClientOP.h) 与 [`MessageClient.h`](../ClientSecKey/ClientSecKey/MessageClient.h)：客户端交互和发送职责。
3. [`ServerOP.h`](../ServerSeckey/ServerSeckey/ServerOP.h) 与 [`MessageService.h`](../ServerSeckey/ServerSeckey/MessageService.h)：服务端路由和消息主链路。
4. [`MessageQueryService.h`](../ServerSeckey/ServerSeckey/MessageQueryService.h) 与 [`mysqlOP.h`](../ServerSeckey/ServerSeckey/mysqlOP.h)：查询授权和数据访问边界。
