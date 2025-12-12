# created by Wang Yingsong on 2025-12-12
#!/bin/bash

# =========================================================
# 快速运行脚本 (fast_run.sh)
# 用于简化 SE 和 FS 模式下的仿真执行
# =========================================================

GEM5_ROOT=$(pwd)
BUILD_BIN="build/X86/gem5.opt"
SCRIPT_DIR="configs/thesis"
TEST_BIN_DIR="tests/spme_benchmark/bin"
RESULT_ROOT="tests/result"

# 定义两个独立的 Python 入口
SE_SCRIPT="$SCRIPT_DIR/run_se.py"
FS_SCRIPT="$SCRIPT_DIR/run_fs.py"

# =========================================================
# 辅助函数
# =========================================================
generate_topology_images() {
    local out_dir=$1
    local dot_file="$out_dir/config.dot"
    if [ -f "$dot_file" ] && command -v dot &> /dev/null; then
        dot -Tsvg "$dot_file" -o "$out_dir/topology.svg"
        dot -Tpdf "$dot_file" -o "$out_dir/topology.pdf"
    fi
}

# =========================================================
# 功能 A: SE 模式运行 (Verification)
# 参数: 1.架构名 2.测试名 3.后缀
# =========================================================
do_se() {
    local arch=$1
    local test_name=$2
    local mode_suffix=$3

    # 检查二进制
    if [ ! -f "$TEST_BIN_DIR/$test_name" ]; then
        echo -e "\033[1;31m[ERROR] Binary '$test_name' not found!\033[0m"
        exit 1
    fi

    # 路径: tests/result/<架构名>/<测试名><后缀>
    local dir_name="${test_name}${mode_suffix}"
    local out_dir="$RESULT_ROOT/$arch/$dir_name"
    local log_file="sim_console.log"

    if [ -z "$mode_suffix" ] && [ -d "$out_dir" ]; then rm -rf "$out_dir"; fi
    mkdir -p "$out_dir"

    echo -e "\033[1;33m[INFO] Running SE Simulation...\033[0m"
    echo -e "  > Arch: $arch"
    echo -e "  > Workload: $test_name"
    echo -e "  > Output: \033[1;36m$out_dir\033[0m"

    # 调用 run_se.py
    ./$BUILD_BIN \
        --outdir="$out_dir" \
        $SE_SCRIPT \
        --arch=$arch \
        --cmd=$test_name \
        > "$out_dir/$log_file" 2>&1

    generate_topology_images "$out_dir"
    echo -e "\033[1;32m[INFO] Finished.\033[0m Check log: $out_dir/$log_file"
}

# =========================================================
# 功能 B: FS 模式运行 (Production)
# 参数: 1.架构名 2.后缀
# =========================================================
do_fs() {
    local arch=$1
    local mode_suffix=$2

    local dir_name="fs_baseline${mode_suffix}"
    local out_dir="$RESULT_ROOT/$arch/$dir_name"

    if [ -z "$mode_suffix" ] && [ -d "$out_dir" ]; then rm -rf "$out_dir"; fi
    mkdir -p "$out_dir"

    echo -e "\033[1;33m[INFO] Running FS Simulation...\033[0m"
    echo -e "  > Arch: $arch"
    echo -e "  > Output: \033[1;36m$out_dir\033[0m"

    # 调用 run_fs.py
    ./$BUILD_BIN \
        --outdir="$out_dir" \
        $FS_SCRIPT \
        --arch=$arch \
        --restore

    generate_topology_images "$out_dir"
}

# =========================================================
# 入口处理
# =========================================================
# 用法: ./fast_run.sh [se/fs] [arch] [test/args] [opt]
CMD=$1

if [ $# -lt 2 ]; then
    echo "Usage:"
    echo "  SE Mode: ./fast_run.sh se <arch> <test_name> [new]"
    echo "  FS Mode: ./fast_run.sh fs <arch> [new]"
    echo ""
    echo "Example:"
    echo "  ./fast_run.sh se standard_multicore hello"
    echo "  ./fast_run.sh fs standard_multicore"
    exit 1
fi

ARCH=$2
TARGET=$3 # 仅 SE 模式有效
OPT=$4    # 仅 SE 模式有效

# 调整参数位置 (FS 模式没有 TARGET，第三个参数直接是 OPT)
if [ "$CMD" == "fs" ] || [ "$CMD" == "run" ]; then
    OPT=$3
fi

# 处理后缀
SUFFIX=""
if [ "$OPT" == "new" ]; then
    SUFFIX="_$(date +%Y%m%d_%H%M%S)"
fi

# 核心分发逻辑
case $CMD in
    se) # [修改] 命令改为 se
        if [ -z "$TARGET" ]; then TARGET="hello"; fi
        do_se $ARCH $TARGET "$SUFFIX"
        ;;
    fs|run) # [修改] 推荐用 fs，但也兼容 run
        do_fs $ARCH "$SUFFIX"
        ;;
    *)
        echo "Invalid command: $CMD. Use 'se' or 'fs'."
        exit 1
        ;;
esac
