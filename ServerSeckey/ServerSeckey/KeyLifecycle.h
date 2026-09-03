#pragma once

// 数据库中的密钥生命周期状态。
// 状态只允许从“活跃”进入某个终态，终态密钥不能再次恢复使用。
enum class KeyLifecycleState : int
{
    Active = 1,
    Revoked = 2,
    Expired = 3,
    Rotated = 4
};

constexpr int KEY_DEFAULT_VALIDITY_HOURS = 24;
