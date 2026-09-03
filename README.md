# 加密传输与密钥管理演示系统

一个基于 **C++17 / Linux** 的命令行网络项目，将会话密钥管理、AES-GCM 文本加密、Protobuf 通信、MySQL 持久化和共享内存缓存串成完整的服务端处理链路。

## 主要功能

| 能力 | 当前实现 |
| --- | --- |
| 密钥管理 | RSA 公钥、签名验签与会话密钥传输；协商、校验、注销 |
| 密钥生命周期 | 默认 24 小时有效期，访问时检查过期，再次协商时轮换旧活跃密钥 |
| 消息保护 | AES-128-GCM，携带密文、nonce 和认证标签 |
| 网络与协议 | TCP 短连接、4 字节网络序长度帧、统一 Protobuf 请求/响应 |
| 服务端处理 | epoll + 线程池；按接收方会话密钥重加密后写库 |
| 查询与审计 | 单条元数据与最近发送列表查询、基础归属检查、审计记录 |
| 工程组织 | CMake 构建、构建时生成协议、隔离运行目录、可选 CTest 目标 |

密钥状态、存储模型和模块职责见[当前架构](docs/current-architecture.md)。

## 架构概览

```text
Client A ── A-Server 密钥 / AES-GCM ──> Server
                                        ├─ 校验生命周期、解密
Client B ── 预先协商 B-Server 密钥 ────────>├─ 使用 B-Server 密钥重加密
                                        ├─ MySQL：两侧密文、元数据、审计
                                        └─ 返回消息 ID，供客户端查询元数据
```

服务端使用 System V 共享内存缓存活跃密钥，并回源 MySQL 检查生命周期。消息查询返回消息 ID、参与方、时间和状态等元数据。

## 快速开始

以下命令在 **Linux 或 WSL2 的 Bash** 中执行，并以仓库根目录为当前目录。原生 Windows 不支持项目使用的 epoll 和 System V IPC。

### 1. 下载项目

```bash
git clone https://github.com/ImMike11451/Encrypted_transmission_Demo.git
cd Encrypted_transmission_Demo
```

### 2. 安装依赖并构建

Ubuntu 22.04 / 24.04 可使用系统软件包准备依赖：

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libssl-dev \
  protobuf-compiler libprotobuf-dev libjsoncpp-dev \
  default-libmysqlclient-dev mysql-client

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

产物为 `build/bin/encrypted-server` 和 `build/bin/encrypted-client`。已有自定义 Protobuf 安装时，见[构建说明](docs/构建说明.md)，不要混用不同版本的编译器、头文件和运行库。

### 3. 初始化数据库

需要一个可访问的 MySQL 服务端；上述客户端开发包不会替你部署数据库。本机需要时可另行安装 `mysql-server`，也可使用现有 Linux 数据库。

在 MySQL 中创建项目数据库和数据表：

```bash
mysql -h 127.0.0.1 -u root -p < scripts/init_db.sql
```

将命令中的主机、用户名替换为实际数据库连接信息。脚本创建 `secmng` 数据库及四张业务表，数据库账户需自行准备。详细步骤见[部署与运行](docs/demo-guide.md)。

### 4. 准备运行配置

```bash
bash scripts/setup-demo.sh
```

编辑 `runtime/server/server.json` 中的 `Host`、`UserDB`、`PassDB`、`ConnectDB`，填写可用的数据库连接信息。

三个角色分别使用独立的运行目录，配置和共享内存路径已经分开。脚本保留已存在的配置。跨机器运行时还要修改客户端配置里的 `ServerIP`。

### 5. 在三个终端启动

```bash
# 终端一
bash scripts/start-server.sh

# 终端二：启动后选择 1，先完成 B 的密钥协商
bash scripts/start-client-b.sh

# 终端三：选择 1 协商，再选择 3，接收方输入 0002
bash scripts/start-client-a.sh
```

同一角色只启动一个实例。使用其他构建目录时，例如：

```bash
BUILD_DIR=build-release bash scripts/start-server.sh
```

完成消息发送后，使用菜单 4 查询元数据、菜单 5 查看最近发送列表；菜单 2 校验密钥，菜单 6 注销密钥。完整操作和预期结果见[演示指南](docs/demo-guide.md)。

## 项目目录

```text
ClientSecKey/ClientSecKey/   客户端、协议与加密封装
ServerSeckey/ServerSeckey/   网络服务、密钥和消息业务、数据访问
tests/                     核心模块与查询归属规则测试
scripts/                   数据库脚本与启动入口
docs/                      架构、构建、部署及项目导览
build/                     构建生成，不提交
runtime/                   各角色配置和运行文件，不提交
```

## 运行测试

默认不构建测试。需要运行已有离线测试时：

```bash
cmake -S . -B build-tests -DBUILD_TESTING=ON
cmake --build build-tests --parallel
(cd build-tests && ctest --output-on-failure)
```

两个 CTest 目标覆盖加密、协议编解码和消息查询归属规则，使用离线数据运行。

## 阅读导航

- [项目导览](docs/项目导览.md)：技术栈、源码阅读顺序与功能说明。
- [当前架构](docs/current-architecture.md)：模块职责、消息流、协议和存储模型。
- [构建说明](docs/构建说明.md)：依赖、构建选项、Protobuf、WSL 与 Visual Studio。
- [部署与运行](docs/demo-guide.md)：数据库初始化、配置、操作和排错。
- [配置与密钥说明](docs/敏感配置与密钥说明.md)：本机文件与公开模板的区别。
- [文档索引](docs/README.md)：项目使用文档。
