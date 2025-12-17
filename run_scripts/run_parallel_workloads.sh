# created by Wang Yingsong on 2025-12-17
#!/bin/bash

# =========================================================
# Parallel Workload Runner (General Purpose)
# Usage: ./run_parallel_workloads.sh <ARCH> <BENCH1> [BENCH2] ...
#    or: ./run_parallel_workloads.sh <ARCH> all
# =========================================================

# 1. 路径配置
SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
GEM5_ROOT=$(dirname "$SCRIPT_LOC")
FAST_RUN="$SCRIPT_LOC/fast_run.sh"
COMPARE_SCRIPT="$SCRIPT_LOC/compare_results.py"

# =========================================================
# 2. 参数数据库 (Parameter Database)
# 定义所有已知 Benchmark 的最佳稳态参数
# =========================================================
declare -A DEFAULT_PARAMS

# --- 计算密集型 ---
DEFAULT_PARAMS["spme_montecarlo"]="80000000"   # 80M iters (Stable IPC)
DEFAULT_PARAMS["spme_matrix"]="512"            # 512x512 (Cache/ALU Balance)
DEFAULT_PARAMS["spme_nqueens"]="13"            # 13x13 Board (Branch/ALU Heavy)

# --- 访存密集型 ---
DEFAULT_PARAMS["spme_vec_add"]="3000000"       # 3M elems (~24MB, Streaming)
DEFAULT_PARAMS["spme_stream"]="2000000"        # 2M elems (~48MB, Bandwidth)
DEFAULT_PARAMS["spme_conv"]="2048"             # 2048x2048 (Spatial Locality)

# --- 随机访问/延迟敏感 ---
DEFAULT_PARAMS["spme_bfs"]="65536"             # 64k Nodes (Latency Bound)
DEFAULT_PARAMS["spme_hashtable"]="2000000"     # 2M buckets (Random Access)

# --- 特殊类型 ---
DEFAULT_PARAMS["spme_task_queue"]="200000"     # 200k tasks (Lock Contention)
DEFAULT_PARAMS["spme_string_search"]="50000000" # 50MB text (L1 Data Stream)

# ========================
# BigDataBench Proxies
# ========================
# FFT: 65536点 (2^16), O(N log N) 浮点运算
DEFAULT_PARAMS["spme_fft"]="65536"

# Sort: 2M 整数 (8MB), 刚好填满或溢出 LLC
DEFAULT_PARAMS["spme_sort"]="2000000"

# MD5: 500万轮, 纯 ALU 计算
DEFAULT_PARAMS["spme_md5"]="5000000"

# CC: 100万节点, 1000万边 (随机内存访问压力大)
DEFAULT_PARAMS["spme_cc"]="1000000"

# Grep: 50MB 文本 (分支预测压力大)
DEFAULT_PARAMS["spme_grep"]="50000000"

# RandSample: 1000万整数 (40MB), 混合计算与访存
DEFAULT_PARAMS["spme_randsample"]="10000000"

# =========================================================
# 3. 输入解析
# =========================================================
if [ $# -lt 2 ]; then
    echo "Usage: $0 <ARCH> <BENCHMARK_LIST...>"
    echo "Examples:"
    echo "  $0 conventional spme_matrix spme_bfs"
    echo "  $0 conventional all"
    echo "Available Benchmarks:"
    for key in "${!DEFAULT_PARAMS[@]}"; do echo "  - $key"; done
    exit 1
fi

TARGET_ARCH=$1
shift
INPUT_BENCHMARKS=("$@")
target_list=()

# 处理 "all" 关键字
if [ "${INPUT_BENCHMARKS[0]}" == "all" ]; then
    echo -e "\033[1;33m[INFO] 'all' selected. Queueing entire suite.\033[0m"
    for key in "${!DEFAULT_PARAMS[@]}"; do
        target_list+=("$key")
    done
else
    target_list=("${INPUT_BENCHMARKS[@]}")
fi

# =========================================================
# 4. 编译阶段
# =========================================================
echo -e "\033[1;34m>>> Preparing Benchmarks...\033[0m"
cd "$GEM5_ROOT/tests/spme_benchmark"

for bench in "${target_list[@]}"; do
    # 检查源码是否存在
    if [ ! -f "${bench}.c" ]; then
        echo -e "\033[1;31m[ERROR] Source file ${bench}.c not found!\033[0m"
        continue
    fi
    # 编译
    echo "  > Compiling $bench ..."
    gcc -static -pthread -O3 "${bench}.c" -o "bin/${bench}" -lm
done
cd - > /dev/null

# =========================================================
# 5. 执行阶段
# =========================================================
RESULT_DIRS=()
TAG="Parallel_Run"

echo ""
echo -e "\033[1;33m[RUNNER] Starting execution on Arch: $TARGET_ARCH\033[0m"

for bench in "${target_list[@]}"; do
    # 获取参数
    param="${DEFAULT_PARAMS[$bench]}"

    if [ -z "$param" ]; then
        echo -e "\033[1;31m[SKIP] Unknown benchmark: $bench (No default param found)\033[0m"
        continue
    fi

    echo ""
    echo -e "\033[1;34m>>> Running $bench (Param: $param) ...\033[0m"

    # 调用 fast_run.sh
    # 格式: fast_run.sh se <arch> <workload> new <tag> --options=<param>
    "$FAST_RUN" se "$TARGET_ARCH" "$bench" new "$TAG" --options="$param" > /dev/null

    # 自动捕获结果路径 (利用 fast_run 的命名规则)
    # 查找最新的匹配文件夹
    LATEST_DIR=$(ls -td "$GEM5_ROOT"/tests/result/"$TARGET_ARCH"/"${bench}_"*_"$TAG" 2>/dev/null | head -1)

    if [ -d "$LATEST_DIR" ]; then
        RESULT_DIRS+=("$LATEST_DIR")
        echo -e "\033[1;32m  > Finished. Result: $(basename "$LATEST_DIR")\033[0m"
    else
        echo -e "\033[1;31m  > Error: Simulation failed or result not found for $bench\033[0m"
    fi
done

# =========================================================
# 6. 生成报告
# =========================================================
if [ ${#RESULT_DIRS[@]} -eq 0 ]; then
    echo "No results collected. Exiting."
    exit 0
fi

echo ""
echo -e "\033[1;33m[REPORT] Generating Parallel Comparison Table...\033[0m"

# 构造 python 命令
CMD="python3 $COMPARE_SCRIPT"
for dir in "${RESULT_DIRS[@]}"; do
    CMD="$CMD $dir"
done
# 关键: 加上 -f (保存文件) 和 -p (平行模式, 存到 Arch 目录下)
CMD="$CMD -f -p"

echo "  > Executing report generator..."
eval $CMD

echo -e "\033[1;32m[DONE] All tasks completed.\033[0m"
