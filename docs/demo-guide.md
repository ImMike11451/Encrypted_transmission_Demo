# 部署与运行

[返回项目首页](../README.md)

本文介绍首次部署的配置和操作流程。请先按[构建说明](构建说明.md)安装依赖并编译项目。以下命令均在仓库根目录的 Linux / WSL2 Bash 中执行。

## 1. 初始化数据库

准备可访问的 MySQL 服务端和数据库账户。服务端默认使用 MySQL 的 `3306` 端口，数据库名为 `secmng`。

使用有建库、建表权限的账户执行：

```bash
mysql -h 127.0.0.1 -u root -p < scripts/init_db.sql
```

将主机和用户名替换为实际连接信息，并按提示输入密码。脚本创建以下表：

| 表 | 用途 |
| --- | --- |
| `keysn` | 分配密钥编号 |
| `seckeyinfo` | 保存会话密钥及生命周期状态 |
| `message_log` | 保存两侧密文和消息元数据 |
| `audit_log` | 保存操作审计记录 |

确认初始化结果：

```bash
mysql -h 127.0.0.1 -u root -p -D secmng -e "SHOW TABLES;"
```

## 2. 生成运行配置

```bash
bash scripts/setup-demo.sh
```

脚本创建三个独立运行目录，并为缺失的配置复制模板：

| 角色 | ID | 配置文件 |
| --- | --- | --- |
| Server | `6789` | `runtime/server/server.json` |
| Client A | `0001` | `runtime/client-a/clientA.json` |
| Client B | `0002` | `runtime/client-b/clientB.json` |

编辑 `runtime/server/server.json`：

| 字段 | 配置说明 |
| --- | --- |
| `Host` | MySQL 地址，本机数据库填写 `127.0.0.1` |
| `UserDB` | 可登录数据库并读写业务表的账户 |
| `PassDB` | 该账户的实际密码，替换 `change_me` |
| `ConnectDB` | 数据库名 `secmng` |
| `ServerID` | 服务端标识，默认 `6789`，与客户端配置一致 |
| `Port` | 业务监听端口，默认 `9898`，与客户端配置一致 |
| `ShmKey` | 保持 `./shm`，脚本已在各运行目录中创建对应路径 |

客户端默认连接 `127.0.0.1:9898`。跨机器运行时，将两份客户端配置的 `ServerIP` 改为业务服务端地址，并确认端口可达。数据库地址 `Host` 与业务服务端地址 `ServerIP` 分别配置；`127.0.0.1` 始终指当前运行程序所在的机器。

配置字段和密钥文件说明见[配置与密钥说明](敏感配置与密钥说明.md)。

## 3. 启动服务端和客户端

分别打开三个终端，每个终端都先进入仓库根目录。每个角色启动一个实例。

终端一：

```bash
bash scripts/start-server.sh
```

服务端启动成功后，在终端二启动 Client B，并选择菜单 **1：密钥协商**：

```bash
bash scripts/start-client-b.sh
```

在终端三启动 Client A，同样选择菜单 **1：密钥协商**：

```bash
bash scripts/start-client-a.sh
```

两端均完成协商后即可发送消息。脚本默认使用 `build/bin` 中的程序。自定义构建目录时，为三个启动命令设置对应的 `BUILD_DIR`，例如：

```bash
BUILD_DIR=build-release bash scripts/start-client-a.sh
```

## 4. 发送与查询消息

在 Client A 中选择菜单 **3：发送加密消息**，接收方输入 `0002`，再输入文本。

处理流程为：Client A 加密 → 服务端校验密钥并解密 → 使用 Client B 会话密钥重加密 → 保存消息和审计记录 → 返回消息 ID。

| 操作 | 预期结果 |
| --- | --- |
| A 选择菜单 4，输入消息 ID | 返回该消息的参与方、时间、状态等元数据；可使用最近一次消息 ID |
| A 选择菜单 5 | 返回 A 最近发送的消息记录 |
| B 选择菜单 4，输入同一消息 ID | 返回 B 作为接收方的消息元数据 |

## 5. 验证密钥管理

1. A 选择 **2：密钥校验**，当前密钥应有效。
2. A 再次选择 **1：密钥协商**，生成新密钥，原活跃密钥变为已轮换。
3. A 选择 **2**，校验新密钥。
4. A 选择 **6：密钥注销**，再次校验应提示密钥不可用。
5. 再次协商后可继续发送消息。

新密钥默认有效 24 小时，在校验或使用时检查有效期。

可在数据库中核对操作结果，以下查询不输出密钥和消息正文：

```sql
USE secmng;

SELECT clientid, serverid, keyid, state, createtime, expiretime,
       invalidatetime, rotated_from_keyid
FROM seckeyinfo ORDER BY keyid DESC;

SELECT msg_id, sender_id, receiver_id, sender_key_id, receiver_key_id,
       algorithm, send_time, status
FROM message_log ORDER BY send_time DESC;

SELECT log_id, node_id, action, target_id, result, create_time
FROM audit_log ORDER BY create_time DESC;
```

## 6. 退出与常见问题

客户端选择 **7：退出**，服务端使用 `Ctrl+C` 停止。

| 现象 | 处理 |
| --- | --- |
| 数据库登录失败 | 检查 `runtime/server/server.json`，用相同主机和账户在命令行验证连接 |
| 数据库或表不存在 | 确认初始化脚本执行成功，且 `ConnectDB` 指向 `secmng` |
| `write db failed` | 查看服务端输出中的 MySQL 错误码，检查账户权限和数据库连接 |
| 客户端无法连接 | 先启动服务端，核对 `ServerIP`、`Port` 和网络连通性 |
| 找不到可用接收方密钥 | B 先完成协商，再由 A 发送；过期或注销后需重新协商 |
| `shmget` 失败 | 确认各运行目录的 `shm` 路径存在，且角色使用独立目录 |
| `bad interpreter` / 出现 `\r` | 将启动脚本保存为 LF 换行 |
| 找不到程序 | 确认已构建成功，并核对 `BUILD_DIR` 和 `bin` 目录 |

System V 共享内存可能在进程退出后保留。需要清理时，只处理本项目对应的共享内存段。
