# created by Wang Yingsong on 2025-12-17
#!/bin/bash

# =========================================================
# Parallel Workload Runner (General Purpose)
# Usage: ./run_parallel_workloads.sh <ARCH> <BENCH1> [BENCH2] ...
#    or: ./run_parallel_workloads.sh <ARCH> all
# =========================================================

# 中断处理
trap 'echo -e "\n\033[1;31m[ABORT] User interrupted execution (Ctrl+C). Stopping all benchmarks.\033[0m"; exit 1' SIGINT

# 1. 路径配置
SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
GEM5_ROOT=$(dirname "$SCRIPT_LOC")
FAST_RUN="$SCRIPT_LOC/fast_run.sh"
COMPARE_SCRIPT="$SCRIPT_LOC/compare_results.py"

# =========================================================
# 2. 参数数据库 (Parameter Database) - 论文实验优化版
# 策略：
# 1. 计算型：保证指令数足够预热流水线，体现 Master-Worker 高 IPC。
# 2. 访存型：数据量必须 > LLC (假设 2MB)，强制 DRAM 访问，体现 Bandwidth/Latency 瓶颈。
# 3. 均衡型：减少迭代次数，只保留必要的锁竞争和混合特征。
# =========================================================
declare -A DEFAULT_PARAMS

# ---------------------------------------------------------
# [类别 1] 计算密集型 (Compute-Intensive)
# 预期特征：IPC > 1.5，L1 Miss < 5%，多核加速比接近线性
# ---------------------------------------------------------
# MonteCarlo: 纯浮点计算，无依赖。
DEFAULT_PARAMS["spme_montecarlo"]="1000000"

# Matrix: O(N^3)。N=256 时，数据总量约 1.5MB (放入 LLC)，主要压测 ALU。
DEFAULT_PARAMS["spme_matrix"]="256"            # 原 512 -> 256 (计算量减小 8 倍)

# NQueens: 指数级复杂度。N=12 是秒级完成，N=13 是分钟级。
DEFAULT_PARAMS["spme_nqueens"]="12"            # 原 13 -> 12

# MD5: 纯整数位运算。1M 轮足以压测流水线发射宽度。
DEFAULT_PARAMS["spme_md5"]="1000000"           # 原 5M -> 1M


# ---------------------------------------------------------
# [类别 2] 访存密集型 (Memory-Intensive)
# 预期特征：IPC < 0.8，LLC Miss 高 (带宽受限) 或 L1 Miss 高 (延迟受限)
# ---------------------------------------------------------
# VecAdd: 流式访问 (Bandwidth Bound)。
# 数据量: 1M double = 8MB > 2MB LLC。保证 100% 穿透 Cache，直击 DRAM。
DEFAULT_PARAMS["spme_vec_add"]="1000000"       # 原 3M -> 1M

# Stream: 同上，标准的内存带宽测试。
DEFAULT_PARAMS["spme_stream"]="1000000"        # 原 2M -> 1M

# CC (连通分量): 随机指针追踪 (Latency Bound)。
# 难点: 极高的 Cache Miss。5万个点足够制造大量 Miss，不用跑 100万。
DEFAULT_PARAMS["spme_cc"]="50000"              # 原 1M -> 5万 (大幅缩短时间)

# BFS: 图遍历。64k 节点适中，能体现随机访问延迟。
DEFAULT_PARAMS["spme_bfs"]="65536"             # 保持 64k 或降至 32768

# HashTable: 随机哈希访问。50万次查找足以填满 MSHR 导致停顿。
DEFAULT_PARAMS["spme_hashtable"]="500000"      # 原 2M -> 50万

# Grep: 文本扫描。10MB 文本足以压测预取器和分支预测。
DEFAULT_PARAMS["spme_grep"]="10000000"         # 原 50MB -> 10MB


# ---------------------------------------------------------
# [类别 3] 均衡型 / 同步敏感 (Balanced / Synchronization)
# 预期特征：IPC 中等 (0.8-1.2)，受锁 (Lock) 或混合因素影响
# ---------------------------------------------------------
# TaskQueue: 锁竞争 (Lock Contention)。
# 关键: 2万次锁操作足以导致核间通信拥堵，无需 20万次。
DEFAULT_PARAMS["spme_task_queue"]="20000"      # 原 200k -> 2万 (解决运行慢的核心)

# FFT: 计算与访存混合，蝴蝶运算。32k 点 (2^15) 刚好溢出 L1 但在 L2/LLC 内。
DEFAULT_PARAMS["spme_fft"]="32768"             # 原 64k -> 32k

# Sort: 排序包含大量分支预测和数据交换。
DEFAULT_PARAMS["spme_sort"]="500000"           # 原 2M -> 50万

# RandSample: 生成随机数(计算) + 写内存(访存)。
DEFAULT_PARAMS["spme_randsample"]="2000000"    # 原 10M -> 2M

# Conv: 卷积运算，有空间局部性。
DEFAULT_PARAMS["spme_conv"]="1024"             # 降至 1024x1024
DEFAULT_PARAMS["spme_string_search"]="10000000" # 降至 10MB

# =========================================================
# 3. 输入解析
# =========================================================
if [ $# -lt 2 ]; then
    echo "Usage: $0 <ARCH> <BENCHMARK_LIST...>"
    echo "Examples:"
    echo "  $0 conventional all"
    exit 1
fi

TARGET_ARCH=$1
shift
INPUT_BENCHMARKS=("$@")
target_list=()

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

# --- [新增功能 1.1] 记录开始时间 ---
START_TIME_STR=$(date "+%Y-%m-%d_%H-%M-%S")
echo ""
echo -e "\033[1;33m[RUNNER] Batch Start Time: $START_TIME_STR\033[0m"
echo -e "\033[1;33m[RUNNER] Starting execution on Arch: $TARGET_ARCH\033[0m"

for bench in "${target_list[@]}"; do
    param="${DEFAULT_PARAMS[$bench]}"

    if [ -z "$param" ]; then
        echo -e "\033[1;31m[SKIP] Unknown benchmark: $bench\033[0m"
        continue
    fi

    echo ""
    echo -e "\033[1;34m>>> Running $bench (Param: $param) ...\033[0m"

    # 执行仿真
    # 注意：这里如果用户按 Ctrl+C，顶部的 trap 会捕获并退出脚本
    "$FAST_RUN" se "$TARGET_ARCH" "$bench" new "$TAG" --options="$param" > /dev/null

    # 捕获结果路径
    LATEST_DIR=$(ls -td "$GEM5_ROOT"/tests/result/"$TARGET_ARCH"/"${bench}_"*_"$TAG" 2>/dev/null | head -1)

    if [ -d "$LATEST_DIR" ]; then
        RESULT_DIRS+=("$LATEST_DIR")
        echo -e "\033[1;32m  > Finished. Result: $(basename "$LATEST_DIR")\033[0m"
    else
        echo -e "\033[1;31m  > Error: Simulation failed for $bench\033[0m"
    fi
done

# =========================================================
# 6. 结果归档 (Result Archiving)
# =========================================================
if [ ${#RESULT_DIRS[@]} -eq 0 ]; then
    echo "No results collected. Exiting."
    exit 0
fi

# --- [新增功能 1.2] 记录结束时间并创建归档目录 ---
END_TIME_STR=$(date "+%Y-%m-%d_%H-%M-%S")
BATCH_DIR_NAME="${START_TIME_STR}------${END_TIME_STR}"
ARCH_RESULT_ROOT="$GEM5_ROOT/tests/result/$TARGET_ARCH"
FINAL_BATCH_DIR="$ARCH_RESULT_ROOT/$BATCH_DIR_NAME"

echo ""
echo -e "\033[1;33m[ARCHIVE] Archiving results to: $BATCH_DIR_NAME\033[0m"

mkdir -p "$FINAL_BATCH_DIR"

# 移动所有生成的文件夹到新目录，并更新列表以供后续脚本使用
MOVED_DIRS=()
for dir in "${RESULT_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        mv "$dir" "$FINAL_BATCH_DIR/"
        # 更新路径为移动后的新路径
        DIR_NAME=$(basename "$dir")
        MOVED_DIRS+=("$FINAL_BATCH_DIR/$DIR_NAME")
    fi
done

# =========================================================
# 7. 生成报告
# =========================================================
echo ""
echo -e "\033[1;33m[REPORT] Generating Parallel Comparison Table...\033[0m"

# 构造 python 命令 (使用移动后的新路径)
CMD="python3 $COMPARE_SCRIPT"
for dir in "${MOVED_DIRS[@]}"; do
    CMD="$CMD $dir"
done
CMD="$CMD -f -p"

echo "  > Executing report generator..."
eval $CMD

echo -e "\033[1;32m[DONE] Batch Completed. Results saved in: tests/result/$TARGET_ARCH/$BATCH_DIR_NAME\033[0m"
