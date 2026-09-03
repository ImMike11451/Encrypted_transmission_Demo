-- 执行前备份目标库、停止所有使用该库的服务端，并确认下方库名。
-- 在同一数据库会话中按顺序执行完整脚本；任何语句失败应停止，不要忽略错误继续。
-- ALTER TABLE 会隐式提交，不能依赖事务一次性回滚整份迁移。
-- 兼容以 keyid 为主键、没有 id 的旧表，也兼容仓库初始化脚本创建的表。
-- 不删除旧字段，不读取或输出 seckey，不修改密钥材料或密钥编号。
-- 状态约定：旧版 0 失效、1 活跃；新版 2 已注销、3 已过期、4 已轮换。
-- 若实际旧系统使用其他状态含义，应先核对，不能直接运行状态转换步骤。
USE secmng;

-- 逐列检查，允许补齐缺失列，也允许在排除失败原因后重新执行。
SET @lifecycle_sql = IF(
  EXISTS (SELECT 1 FROM information_schema.COLUMNS
          WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'seckeyinfo'
            AND COLUMN_NAME = 'expiretime'),
  'DO 0',
  'ALTER TABLE seckeyinfo ADD COLUMN expiretime DATETIME NULL AFTER createtime');
PREPARE lifecycle_stmt FROM @lifecycle_sql;
EXECUTE lifecycle_stmt;
DEALLOCATE PREPARE lifecycle_stmt;

SET @lifecycle_sql = IF(
  EXISTS (SELECT 1 FROM information_schema.COLUMNS
          WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'seckeyinfo'
            AND COLUMN_NAME = 'invalidatetime'),
  'DO 0',
  'ALTER TABLE seckeyinfo ADD COLUMN invalidatetime DATETIME NULL AFTER expiretime');
PREPARE lifecycle_stmt FROM @lifecycle_sql;
EXECUTE lifecycle_stmt;
DEALLOCATE PREPARE lifecycle_stmt;

SET @lifecycle_sql = IF(
  EXISTS (SELECT 1 FROM information_schema.COLUMNS
          WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'seckeyinfo'
            AND COLUMN_NAME = 'rotated_from_keyid'),
  'DO 0',
  'ALTER TABLE seckeyinfo ADD COLUMN rotated_from_keyid INT NULL AFTER state');
PREPARE lifecycle_stmt FROM @lifecycle_sql;
EXECUTE lifecycle_stmt;
DEALLOCATE PREPARE lifecycle_stmt;

-- 优先保留已有 expiretime；缺失时继承旧 expires_at（包括已过期时间）。
-- 旧表没有 expires_at 时不引用该列，避免再次出现未知字段错误。
-- 两种有效期均缺失时按创建时间加 24 小时；创建时间也缺失则立即到期。
SET @lifecycle_sql = IF(
  EXISTS (SELECT 1 FROM information_schema.COLUMNS
          WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'seckeyinfo'
            AND COLUMN_NAME = 'expires_at'),
  'UPDATE seckeyinfo SET expiretime = COALESCE(expires_at, DATE_ADD(createtime, INTERVAL 24 HOUR), NOW()) WHERE expiretime IS NULL',
  'UPDATE seckeyinfo SET expiretime = COALESCE(DATE_ADD(createtime, INTERVAL 24 HOUR), NOW()) WHERE expiretime IS NULL');
PREPARE lifecycle_stmt FROM @lifecycle_sql;
EXECUTE lifecycle_stmt;
DEALLOCATE PREPARE lifecycle_stmt;

-- 旧版本使用 0 表示失效，迁移后统一归类为“已注销”。
UPDATE seckeyinfo
SET state = 2,
    invalidatetime = COALESCE(invalidatetime, NOW())
WHERE state = 0;

ALTER TABLE seckeyinfo
  MODIFY COLUMN expiretime DATETIME NOT NULL;

-- expires_at、rotated_at 等旧字段原样保留；新程序仅使用新生命周期字段。
-- 不推测历史轮换来源或 rotated_at 的语义，旧记录的 rotated_from_keyid 保持 NULL。
-- 重复执行不覆盖已有 expiretime，也不重新启用失效密钥。
-- 仅返回结构和计数，避免输出密钥材料。
SHOW COLUMNS FROM seckeyinfo;
SELECT COUNT(*) AS missing_expiretime_count
FROM seckeyinfo WHERE expiretime IS NULL;
