# 加密传输与密钥管理演示系统

基于 C++ 的安全消息传输与密钥管理演示系统。

本项目实现了客户端与服务端之间的会话密钥协商、密钥校验、密钥注销、AES-GCM 加密消息发送、服务端消息转发存储、消息元数据查询和审计日志记录。项目重点展示 C++ 网络编程、OpenSSL 加密、Protobuf 协议设计、MySQL 持久化、共享内存缓存、epoll 与线程池并发处理等能力。

> 当前版本是可信服务端转发模型：服务端会解密发送方消息，再使用接收方与服务端之间的会话密钥重新加密后存储。它不是端到端加密模型。

## 功能特性

- RSA 公私钥生成、签名验签与会话密钥加密传输
- AES-128-GCM 认证加密，支持 ciphertext、nonce、tag 传输
- Protobuf 统一二进制协议，覆盖密钥业务和消息业务
- TCP 长度帧协议：4 字节网络序长度 + protobuf payload
- 服务端基于 epoll + 线程池处理客户端短连接请求
- MySQL 持久化密钥、消息记录和审计日志
- System V 共享内存缓存活跃会话密钥
- 双客户端演示配置：Client A 向 Client B 发送加密消息

## 架构概览

```text
Client A
  | 1. 密钥协商
  | 2. AES-GCM 加密消息
  v
Server
  | 校验密钥
  | 使用 A-Server 密钥解密
  | 使用 B-Server 密钥重加密
  | 写入 message_log / audit_log
  v
Client B
```

主要模块：

```text
ClientSecKey/ClientSecKey/
  ClientOP            菜单业务协调与密钥操作
  MessageClient       加密消息发送与查询客户端
  TcpSocket           长度帧 TCP 客户端
  SecKeyShm           本地会话密钥缓存
  RsaCrypto           RSA 加解密与签名验签
  AesGcmCrypto        AES-GCM 加解密
  V2RequestCodec      protobuf 请求编码
  V2RespondCodec      protobuf 响应解码

ServerSeckey/ServerSeckey/
  ServerOP            服务端启动、协议路由与密钥操作
  EpollServer         epoll 事件循环封装
  ThreadPool          工作线程池
  MessageService      消息业务逻辑
  MessageRepository   message_log 访问层
  AuditService        audit_log 访问层
  mysqlOP             MySQL 底层操作
  SecKeyShm           服务端活跃密钥缓存
```

更多当前架构说明见 [docs/current-architecture.md](docs/current-architecture.md)。

## 协议设计

当前协议文件：

```text
ClientSecKey/ClientSecKey/MessageV2.proto
ServerSeckey/ServerSeckey/MessageV2.proto
```

统一请求与响应：

```text
RequestPacket
ResponsePacket
Header
KeyAgreementRequest
KeyCheckRequest
KeyLogoutRequest
SendMessageRequest
QueryMessageRequest
QueryMessageListRequest
```

## 环境要求

- Linux 构建和运行环境
- 支持 C++17 或更高版本的 C++ 编译器
- OpenSSL 开发库
- Protobuf 编译器和 C++ 运行库
- MySQL 客户端开发库
- MySQL 服务端

当前仓库保留 Visual Studio Linux 工程文件，同时提供顶层 CMake 构建入口。推荐使用 CMake 进行跨平台构建，具体命令见 `docs/构建说明.md`。

## 快速开始

1. 初始化数据库。

```bash
mysql -u root -p < scripts/init_db.sql
```

2. 准备配置文件。

```bash
cp ServerSeckey/ServerSeckey/server.example.json ServerSeckey/ServerSeckey/server.json
cp ClientSecKey/ClientSecKey/clientA.example.json ClientSecKey/ClientSecKey/clientA.json
cp ClientSecKey/ClientSecKey/clientB.example.json ClientSecKey/ClientSecKey/clientB.json
```

根据本机 MySQL 用户名、密码、共享内存路径等修改配置。

## 敏感配置与密钥文件

仓库只提交 `server.example.json`、`clientA.example.json` 和 `clientB.example.json` 这类公开模板。复制模板后生成的真实配置文件仅用于本机运行，不能提交到版本控制。

客户端完成密钥协商时会在运行目录生成 `pri.pem` 和 `pub.pem`；服务端会按客户端 ID 生成或缓存 `客户端ID_pub.pem`。这些文件属于运行时密钥材料，不能用于生产环境，也不能提交到公开仓库。若私钥意外泄露，应删除旧密钥、重新执行密钥协商，并更新本机配置。

3. 编译服务端和客户端。

当前工程主要通过 Visual Studio Linux 项目编译：

```text
ServerSeckey/ServerSeckey.sln
ClientSecKey/ClientSecKey.sln
```

4. 启动服务端。

```bash
./ServerSeckey
```

5. 分别启动两个客户端。

```bash
./ClientSecKey clientA.json
./ClientSecKey clientB.json
```

6. 按演示流程操作。

详见 [docs/demo-guide.md](docs/demo-guide.md)。

## 演示流程

推荐演示顺序：

1. Client B 执行密钥协商。
2. Client A 执行密钥协商。
3. Client A 发送加密消息给 Client B，接收方节点 ID 输入 `0002`。
4. Client A 查询最近一次消息。
5. Client A 查询最近消息列表。
6. Client A 执行密钥校验。
7. Client A 执行密钥注销，再次校验密钥状态。

## 存储模型

数据库 `secmng` 包含：

- `keysn`：分配递增密钥 ID
- `seckeyinfo`：保存客户端与服务端之间的会话密钥和状态
- `message_log`：保存发送方密文、接收方重加密密文和消息元数据
- `audit_log`：保存密钥与消息相关审计记录

初始化脚本见 [scripts/init_db.sql](scripts/init_db.sql)。

## 安全说明

当前版本仍有一些适合继续打磨的安全点：

- 服务端属于可信节点，当前能看到消息明文。
- 演示密钥与配置只应用于本地测试环境。
- 当前代码已避免在客户端日志输出协商后的会话密钥。
- 当前代码已避免在服务端日志输出解密后的消息明文。
- 当前消息 ID 和审计 ID 已改为微秒时间戳 + 进程内递增序号，降低高频请求碰撞概率。
- 当前消息查询已增加基础权限控制：单条消息只允许发送方或接收方查询，消息列表只允许查询当前客户端自己的发送记录。
- 生产级实现仍应避免日志输出完整 SQL 和敏感配置。
- 动态 SQL 已统一通过 MySQL 长度感知转义处理；生产级实现仍建议进一步升级为 `mysql_stmt` 参数化查询。
- 后续可以进一步将消息 ID 与审计 ID 升级为 UUID。

## 后续路线

- 顶层 CMake 工程与公共模块拆分
- 参数化 SQL 与敏感配置治理
- 单元测试：crypto、codec、repository、message service
- 一键集成测试与并发压测脚本
- 接收方拉取密文并本地解密
- 端到端加密模式
- 长连接实时推送
