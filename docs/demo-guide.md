# 本地部署与演示指南

[返回项目首页](../README.md)

本文从已完成[构建](构建说明.md)开始，介绍服务端重加密存储链路的操作步骤与预期结果。

## 1. 数据库：区分新建与升级

服务端需要可访问的 MySQL，默认端口为 3306，数据库为 `secmng`。服务器配置里的 `Host` 是数据库地址；客户端 `ServerIP` 才是业务服务端地址。服务端运行在远程 Linux 时，其 `127.0.0.1` 指远程 Linux，不是 Windows。

### 全新数据库

确认目标实例后，在仓库根目录执行：

```bash
mysql -h 127.0.0.1 -u root -p < scripts/init_db.sql
```

替换主机与用户为实际可用连接。脚本创建 `keysn`、`seckeyinfo`、`message_log`、`audit_log`，不负责创建数据库账户。原有同名表不会因 `CREATE TABLE IF NOT EXISTS` 自动升级。

### 已有旧库

先停止使用该库的业务服务、备份，确认旧状态含义及表结构，再执行迁移。命令行备份示例（备份文件仅保存在本地）：

```bash
mkdir -p backups
mysqldump -h 127.0.0.1 -u root -p --single-transaction --no-tablespaces \
  --databases secmng --result-file="backups/secmng-$(date +%Y%m%d-%H%M%S).sql"
```

确认备份命令成功、文件存在且非空后，执行：

```bash
mysql -h 127.0.0.1 -u root -p < scripts/migrate_key_lifecycle.sql
```

图形工具中则连接目标实例，执行整份 SQL 文件。不要用删除表、重建库的方式代替迁移；遇到错误停止，不使用强制忽略错误选项。表结构变更不能当作可一次回滚的完整事务。

迁移规则：

- 添加缺失的 `expiretime`、`invalidatetime`、`rotated_from_keyid`。
- 保留已有到期时间，否则继承旧 `expires_at`，再否则以创建时间加 24 小时补齐；创建时间也为空则迁移时到期。
- 旧失效状态 `0` 转为已注销 `2`，其他状态不因此重新启用。
- 保留旧字段，不同步维护旧 `expires_at`、`rotated_at`；不要新旧服务混跑。
- 不自动修正密钥计数器或改变表引擎。运行前确保 `keysn` 只有一行，且编号大于当前最大 `keyid`。

检查结构与编号（不输出密钥内容）：

```sql
USE secmng;
SHOW COLUMNS FROM seckeyinfo;
SELECT COUNT(*) AS counter_rows, MIN(ikeysn) AS next_key_id FROM keysn;
SELECT MAX(keyid) AS largest_key_id FROM seckeyinfo;
SELECT COUNT(*) AS missing_expiretime_count
FROM seckeyinfo WHERE expiretime IS NULL;
```

三个新字段应存在，缺失有效期数量应为 0。首次空表的最大编号为 NULL 是正常的。

## 2. 准备隔离运行目录

```bash
bash scripts/setup-demo.sh
```

脚本只创建缺失配置和目录，不覆盖、不启动服务、不连接数据库：

| 角色 | ID | 配置 | 共享内存路径 |
| --- | --- | --- | --- |
| Server | `6789` | `runtime/server/server.json` | 相对其工作目录的 `./shm` |
| Client A | `0001` | `runtime/client-a/clientA.json` | 相对其工作目录的 `./shm` |
| Client B | `0002` | `runtime/client-b/clientB.json` | 相对其工作目录的 `./shm` |

编辑服务端配置的数据库连接信息，不能直接使用密码占位值 `change_me`。旧源码目录下的真实配置不受影响，但新脚本也不会读取它们；已有用户需要自行将正确连接信息填入新配置。

两个客户端默认连接 `127.0.0.1:9898`。跨机器部署需修改 `ServerIP` 并检查网络和端口可达性。每个角色只启动一个实例，不要同时启动同一套旧服务端和新服务端。

## 3. 启动和协商

分别打开三个 Linux 终端：

```bash
bash scripts/start-server.sh
```

服务端启动成功并等待连接后：

```bash
bash scripts/start-client-b.sh
```

Client B 选择菜单 **1：密钥协商**，成功后保持终端。随后：

```bash
bash scripts/start-client-a.sh
```

Client A 也选择 **1：密钥协商**。两端都完成协商后才能演示 A 向 B 发送消息。

快捷脚本默认读取 `build/bin`；使用其他构建目录时，三个启动命令均加上对应 `BUILD_DIR`：

```bash
BUILD_DIR=build-release bash scripts/start-client-a.sh
```

## 4. 发送与查询

在 A 中选择菜单 **3：发送加密消息**，接收方填 `0002`，输入一段演示文本。

预期链路：A 加密 → 服务端检查 A 密钥并解密 → 检查 B 密钥并重加密 → 写入消息和审计 → 返回消息 ID。

- 菜单 **4：查询消息**：按消息 ID 查询元数据；可使用客户端保存的最近一次消息 ID。
- 菜单 **5：查询消息列表**：查询当前客户端的最近发送记录。
- B 可用 A 返回的消息 ID 查询对应元数据；这不等于 B 下载或解密消息正文。
- 列表归属由声明身份检查，不能据此认定协议已有完整身份认证。

## 5. 生命周期演示

1. A 选择 **2：密钥校验**，当前密钥应有效。
2. A 再次选择 **1：密钥协商**，旧活跃记录应进入 `4`（已轮换），新记录为 `1`（活跃）。
3. A 选择 **2** 校验新密钥。
4. A 选择 **6：密钥注销**，再选 **2**，应提示密钥不可用。
5. 恢复发送前，再次协商新密钥。

可读查询核对记录：

```sql
SELECT clientid, serverid, keyid, state, createtime, expiretime,
       invalidatetime, rotated_from_keyid
FROM seckeyinfo ORDER BY keyid DESC;

SELECT msg_id, sender_id, receiver_id, sender_key_id, receiver_key_id,
       algorithm, send_time, status
FROM message_log ORDER BY send_time DESC;

SELECT log_id, node_id, action, target_id, result, create_time
FROM audit_log ORDER BY create_time DESC;
```

新密钥默认有效 24 小时，访问时才检查过期；没有后台计时轮换。菜单只管理当前密钥。

## 6. 退出与排错

客户端选择 **7：退出**，服务端用 `Ctrl+C` 停止。System V 共享内存可能在进程退出后保留；不要直接执行清理所有共享内存的命令。新运行目录与旧运行目录可能对应不同缓存，迁移运行方式后应重新协商。

| 现象 | 处理 |
| --- | --- |
| `write db failed` | 看服务端操作阶段、MySQL errno 和 SQLSTATE，不要只看客户端汇总错误 |
| 错误 1054 / 缺字段 | 确认迁移在服务端实际连接的实例和库执行；仅编译不能补数据库字段 |
| 错误 1062 / 编号冲突 | 停止业务，核对 `keysn` 与已有最大编号，不盲目重置或删除历史记录 |
| 数据库登录失败 | 检查新 `runtime/server/server.json`，先用相同主机和用户在命令行验证 |
| 找不到可用接收方密钥 | B 先协商，A 后协商；检查密钥是否过期或注销 |
| `shmget` 失败 | 确认 `ShmKey` 指向已存在路径，角色不能误用同一缓存路径 |
| `bad interpreter` / 出现 `\\r` | 保持脚本 LF 换行；仓库已提供 `.gitattributes` |
| 找不到程序 | 使用新 CMake 构建，并核对 `BUILD_DIR` 和 `bin` 目录 |
