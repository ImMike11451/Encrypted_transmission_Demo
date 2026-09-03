#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 0 ]]; then
    echo "用法：bash scripts/setup-demo.sh"
    echo "创建 runtime 下的独立演示目录，只复制缺失配置，不覆盖已有文件。"
    [[ $# -eq 1 && "$1" == "--help" ]] && exit 0
    exit 1
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"

# 不读取或迁移源码目录中的真实配置；新文件仅对当前用户开放。
umask 077
for role in server client-a client-b; do
    case "${role}" in
        server)
            template="${project_root}/ServerSeckey/ServerSeckey/server.example.json"
            config_name="server.json"
            ;;
        client-a)
            template="${project_root}/ClientSecKey/ClientSecKey/clientA.example.json"
            config_name="clientA.json"
            ;;
        client-b)
            template="${project_root}/ClientSecKey/ClientSecKey/clientB.example.json"
            config_name="clientB.json"
            ;;
    esac
    role_dir="${project_root}/runtime/${role}"
    # 各角色通过不同的已存在路径生成共享内存标识。
    mkdir -p -- "${role_dir}/shm"
    if [[ -e "${role_dir}/${config_name}" || -L "${role_dir}/${config_name}" ]]; then
        echo "保留已有配置：runtime/${role}/${config_name}"
    else
        cp -n -- "${template}" "${role_dir}/${config_name}"
        echo "创建示例配置：runtime/${role}/${config_name}"
    fi
done
echo "目录准备完成。请编辑 runtime/server/server.json 的数据库连接配置。"
echo "跨机器运行时，请同时修改客户端配置中的 ServerIP。"
echo "本脚本不创建数据库、不执行迁移，也不启动任何业务进程。"
