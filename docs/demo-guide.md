# 演示指南

本文档用于快速完成一次可展示的双客户端加密消息传输演示。

## 1. 准备数据库

执行初始化脚本：

```bash
mysql -u root -p < scripts/init_db.sql
```

确认数据库已创建：

```sql
use secmng;
show tables;
```

应看到：

```text
keysn
seckeyinfo
message_log
audit_log
```

## 2. 准备配置文件

服务端配置：

```bash
cp ServerSeckey/ServerSeckey/server.example.json ServerSeckey/ServerSeckey/server.json
```

客户端配置：

```bash
cp ClientSecKey/ClientSecKey/clientA.example.json ClientSecKey/ClientSecKey/clientA.json
cp ClientSecKey/ClientSecKey/clientB.example.json ClientSecKey/ClientSecKey/clientB.json
```

默认身份：

| 节点 | ID | 配置文件 |
|---|---|---|
| Server | 6789 | `server.json` |
| Client A | 0001 | `clientA.json` |
| Client B | 0002 | `clientB.json` |

默认监听地址：

```text
127.0.0.1:9898
```

### 配置安全提示

`server.json`、`client.json`、`clientA.json` 和 `clientB.json` 是本机运行文件。请从示例配置复制生成，再填写自己的数据库信息；不要将真实密码写回示例文件或提交到仓库。

客户端执行密钥协商后会生成 `pri.pem` 和 `pub.pem`，服务端会生成或缓存 `客户端ID_pub.pem`。这些均为本机运行时文件，丢失或泄露后应重新协商密钥，不要通过 Git、聊天工具或截图传递私钥内容。

## 3. 启动服务端

进入服务端可执行文件目录，启动服务端：

```bash
./ServerSeckey
```

看到类似日志即可：

```text
服务器启动成功! 等待客户端连接...
```

## 4. 启动 Client B 并协商密钥

启动 Client B：

```bash
./ClientSecKey clientB.json
```

选择：

```text
1. 秘钥协商
```

成功后，服务端共享内存与数据库中会出现 `0002` 与 `6789` 之间的可用会话密钥。

## 5. 启动 Client A 并协商密钥

启动 Client A：

```bash
./ClientSecKey clientA.json
```

选择：

```text
1. 秘钥协商
```

成功后，Client A 可以使用自己的 A-Server 会话密钥发送加密消息。

## 6. 发送加密消息

在 Client A 菜单中选择：

```text
3. 发送加密消息
```

接收方节点 ID 输入：

```text
0002
```

然后输入任意文本消息。

预期结果：

- Client A 使用 A-Server 会话密钥 AES-GCM 加密明文。
- Server 使用 A-Server 会话密钥解密。
- Server 使用 B-Server 会话密钥重新加密。
- Server 写入 `message_log` 和 `audit_log`。
- Client A 收到 `server_message_id`。

## 7. 查询单条消息元数据

在 Client A 菜单中选择：

```text
4. 查询消息
```

如果客户端已缓存最近一次 `server_message_id`，可以直接回车使用最近一次消息 ID。

查询成功后应显示：

```text
server_message_id
sender_id
receiver_id
key_id
msg_type
status
send_time
```

## 8. 查询最近消息列表

在 Client A 菜单中选择：

```text
5. 查询消息列表
```

发送方节点 ID 可以直接回车使用当前客户端 `0001`。

建议查询数量输入：

```text
10
```

## 9. 校验并注销密钥

密钥校验：

```text
2. 秘钥校验
```

密钥注销：

```text
6. 秘钥注销
```

注销后再次校验，预期密钥不可用。

## 10. 查看数据库记录

查看密钥：

```sql
select clientid, serverid, keyid, state, createtime from seckeyinfo order by keyid desc;
```

查看消息：

```sql
select msg_id, sender_id, receiver_id, sender_key_id, receiver_key_id, msg_type, algorithm, send_time, status
from message_log
order by send_time desc;
```

查看审计日志：

```sql
select log_id, node_id, action, target_id, result, create_time
from audit_log
order by create_time desc;
```

## 常见问题

如果客户端提示找不到密钥，请先确认发送方和接收方都完成密钥协商。

如果服务端无法连接数据库，请检查 `server.json` 中的 `UserDB`、`PassDB`、`Host` 和 `ConnectDB`。

如果共享内存写入失败，请确认配置中的 `ShmKey` 路径在 Linux 环境中存在。
