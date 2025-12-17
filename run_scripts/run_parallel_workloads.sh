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
# 2. 参数数据库 (Parameter Database)
# =========================================================
declare -A DEFAULT_PARAMS

# --- 计算密集型 ---
DEFAULT_PARAMS["spme_montecarlo"]="80000000"
DEFAULT_PARAMS["spme_matrix"]="512"
DEFAULT_PARAMS["spme_nqueens"]="13"

# --- 访存密集型 ---
DEFAULT_PARAMS["spme_vec_add"]="3000000"
DEFAULT_PARAMS["spme_stream"]="2000000"
DEFAULT_PARAMS["spme_conv"]="2048"

# --- 随机访问/延迟敏感 ---
DEFAULT_PARAMS["spme_bfs"]="65536"
DEFAULT_PARAMS["spme_hashtable"]="2000000"

# --- 特殊类型 ---
DEFAULT_PARAMS["spme_task_queue"]="200000"
DEFAULT_PARAMS["spme_string_search"]="50000000"

# --- BigDataBench Proxies ---
DEFAULT_PARAMS["spme_fft"]="65536"
DEFAULT_PARAMS["spme_sort"]="2000000"
DEFAULT_PARAMS["spme_md5"]="5000000"
DEFAULT_PARAMS["spme_cc"]="1000000"
DEFAULT_PARAMS["spme_grep"]="50000000"
DEFAULT_PARAMS["spme_randsample"]="10000000"

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
    fi
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
