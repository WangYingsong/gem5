# created by Wang Yingsong on 2025-12-12
#!/bin/bash

# =========================================================
# 快速运行脚本 (fast_run.sh) - Enhanced Version with Links
# =========================================================

GEM5_ROOT=$(pwd)
BUILD_BIN="build/X86/gem5.opt"
SCRIPT_DIR="configs/thesis"
TEST_BIN_DIR="tests/spme_benchmark/bin"
RESULT_ROOT="tests/result"

# 定义 Python 入口
SE_SCRIPT="$SCRIPT_DIR/run_se.py"
FS_SCRIPT="$SCRIPT_DIR/run_fs.py"

# =========================================================
# 辅助函数: 生成拓扑图
# =========================================================
generate_topology_images() {
    local out_dir=$1
    local dot_file="$out_dir/config.dot"
    if [ -f "$dot_file" ] && command -v dot &> /dev/null; then
        dot -Tsvg "$dot_file" -o "$out_dir/topology.svg"
    fi
}

# =========================================================
# 核心功能: SE 模式运行
# =========================================================
do_se() {
    local arch=$1
    local test_name=$2
    local dir_suffix=$3
    local is_new_mode=$4  # true 或 false

    # 1. 构建输出路径
    local dir_name="${test_name}${dir_suffix}"
    local out_dir="$RESULT_ROOT/$arch/$dir_name"
    local log_file="sim_console.log"

    # 2. 覆盖逻辑检查
    if [ -d "$out_dir" ]; then
        if [ "$is_new_mode" == "true" ]; then
            echo -e "\033[1;33m[WARN] Directory exists in NEW mode (rare): $out_dir\033[0m"
        else
            echo -e "\033[1;33m[INFO] Overwriting existing daily result...\033[0m"
            rm -rf "$out_dir"
        fi
    fi
    mkdir -p "$out_dir"

    # 3. 打印运行信息
    echo -e "\033[1;32m============================================\033[0m"
    echo -e "          SPME THESIS RUNNER (SE MODE)"
    echo -e "\033[1;32m============================================\033[0m"
    echo -e "  > Arch      : \033[1;35m$arch\033[0m"
    echo -e "  > Workload  : \033[1;33m$test_name\033[0m"
    echo -e "  > Output Dir: $out_dir"
    echo -e "\033[1;32m============================================\033[0m"

    # 4. 执行 GEM5
    ./$BUILD_BIN \
        --outdir="$out_dir" \
        $SE_SCRIPT \
        --arch=$arch \
        --cmd=$test_name \
        --num-cpus=4 \
        > "$out_dir/$log_file" 2>&1

    # =========================================================
    # [修改] 获取绝对路径并生成可点击链接
    # =========================================================
    local exit_code=$?
    local abs_log_path=$(readlink -f "$out_dir/$log_file")

    if [ $exit_code -eq 0 ]; then
        generate_topology_images "$out_dir"
        echo -e "\033[1;32m[SUCCESS] Simulation Finished.\033[0m"
        # 这里的 file:// 协议配合绝对路径，在 VS Code / Terminal 中可 Ctrl+点击
        echo -e "  > View Log : \033[4;36mfile://$abs_log_path\033[0m"
    else
        echo -e "\033[1;31m[ERROR] Simulation Failed!\033[0m"
        echo -e "  > Check Log: \033[4;31mfile://$abs_log_path\033[0m"
    fi
}

# =========================================================
# 核心功能: FS 模式运行
# =========================================================
do_fs() {
    local arch=$1
    local dir_suffix=$2

    local dir_name="fs_baseline${dir_suffix}"
    local out_dir="$RESULT_ROOT/$arch/$dir_name"

    mkdir -p "$out_dir"
    echo -e "\033[1;33m[INFO] Running FS Simulation...\033[0m"

    ./$BUILD_BIN --outdir="$out_dir" $FS_SCRIPT --arch=$arch --restore
    generate_topology_images "$out_dir"

    # 同样为 FS 模式生成链接 (虽然没有 log重定向，但有 system log)
    # 这里假设 GEM5 默认生成 system.pc.com_1.device
    local abs_out_path=$(readlink -f "$out_dir")
    echo -e "  > Result Dir: \033[4;36mfile://$abs_out_path\033[0m"
}

# =========================================================
# 参数解析逻辑
# =========================================================

if [ $# -lt 2 ]; then
    echo "Usage: ./fast_run.sh <se/fs> <arch> <workload> [new] [remarks...]"
    exit 1
fi

MODE=$1
ARCH=$2
WORKLOAD=""

if [ "$MODE" == "fs" ]; then
    if [ "$3" == "new" ]; then
        SUFFIX="_$(date +%Y-%m-%d_%H-%M-%S)"
    else
        SUFFIX="_$(date +%Y-%m-%d)"
    fi
    do_fs $ARCH "$SUFFIX"
    exit 0
fi

if [ "$MODE" == "se" ]; then
    if [ -z "$3" ]; then
        echo "Error: SE mode requires a workload name."
        exit 1
    fi
    WORKLOAD=$3
    shift 3

    IS_NEW=false
    NOTE=""

    for arg in "$@"; do
        if [ "$arg" == "new" ]; then
            IS_NEW=true
        else
            if [ -z "$NOTE" ]; then
                NOTE="${arg}"
            else
                NOTE="${NOTE}_${arg}"
            fi
        fi
    done

    if [ "$IS_NEW" == "true" ]; then
        TIMESTAMP="_$(date +%Y-%m-%d_%H-%M-%S)"
    else
        TIMESTAMP="_$(date +%Y-%m-%d)"
    fi

    FULL_SUFFIX="${TIMESTAMP}"
    if [ ! -z "$NOTE" ]; then
        FULL_SUFFIX="${FULL_SUFFIX}_${NOTE}"
    fi

    do_se $ARCH $WORKLOAD "$FULL_SUFFIX" "$IS_NEW"

else
    echo "Invalid mode: $MODE. Use 'se' or 'fs'."
    exit 1
fi
