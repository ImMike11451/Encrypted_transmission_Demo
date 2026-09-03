#!/usr/bin/env bash
set -euo pipefail

usage()
{
    echo "用法：bash scripts/run-demo.sh server|client-a|client-b"
    echo "BUILD_DIR 可指定构建目录（相对项目根目录或绝对路径），默认 build。"
}
if [[ $# -eq 1 && "$1" == "--help" ]]; then
    usage
    exit 0
fi
if [[ $# -ne 1 ]]; then
    usage >&2
    exit 1
fi
role="$1"
case "${role}" in
    server) binary_name="encrypted-server"; config_name="server.json" ;;
    client-a) binary_name="encrypted-client"; config_name="clientA.json" ;;
    client-b) binary_name="encrypted-client"; config_name="clientB.json" ;;
    *) usage >&2; exit 1 ;;
esac
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${BUILD_DIR:-${project_root}/build}"
if [[ "${build_dir}" != /* ]]; then
    build_dir="${project_root}/${build_dir}"
fi
binary="${build_dir}/bin/${binary_name}"
role_dir="${project_root}/runtime/${role}"

if [[ ! -x "${binary}" ]]; then
    echo "错误：未找到可执行文件 ${binary}" >&2
    echo "请先运行 cmake -S . -B build，再运行 cmake --build build --parallel。" >&2
    exit 1
fi
if [[ ! -f "${role_dir}/${config_name}" || ! -d "${role_dir}/shm" ]]; then
    echo "错误：演示目录尚未准备好，请运行 bash scripts/setup-demo.sh。" >&2
    exit 1
fi
# 相对配置、共享内存路径和生成的 PEM 文件均限定在当前角色目录。
cd -- "${role_dir}"
if [[ "${role}" == "server" ]]; then
    exec "${binary}"
else
    exec "${binary}" "${config_name}"
fi
