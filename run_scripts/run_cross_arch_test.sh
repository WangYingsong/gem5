# created by Wang Yingsong on 2025-12-17
#!/bin/bash

# =========================================================
# Cross-Architecture Comparator
# Usage: ./run_cross_arch_test.sh <BENCHMARK> <ARCH1> [ARCH2] ...
# Example: ./run_cross_arch_test.sh spme_matrix conventional spme
# Note: The FIRST architecture provided will be used as the BASELINE.
# =========================================================

# --- [新增功能] 信号捕获：按 Ctrl+C 时终止整个脚本 ---
trap 'echo -e "\n\033[1;31m[ABORT] User interrupted execution (Ctrl+C). Stopping all simulations.\033[0m"; exit 1' SIGINT

# 1. 路径配置
SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
GEM5_ROOT=$(dirname "$SCRIPT_LOC")
FAST_RUN="$SCRIPT_LOC/fast_run.sh"
COMPARE_SCRIPT="$SCRIPT_LOC/compare_results.py"

# =========================================================
# 2. 参数数据库 (Parameter Database)
# 保持与 Parallel Runner 一致，确保对比公平性
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
    echo "Usage: $0 <BENCHMARK> <ARCH1 (Baseline)> [ARCH2] ..."
    echo "Examples:"
    echo "  $0 spme_matrix conventional spme"
    echo "  $0 spme_bfs conventional spme_l1_opt spme_l2_opt"
    exit 1
fi

BENCHMARK=$1
shift
ARCH_LIST=("$@")

# 检查 Benchmark 是否有效
PARAM="${DEFAULT_PARAMS[$BENCHMARK]}"
if [ -z "$PARAM" ]; then
    echo -e "\033[1;31m[ERROR] Unknown benchmark: $BENCHMARK\033[0m"
    echo "Available: ${!DEFAULT_PARAMS[@]}"
    exit 1
fi

# =========================================================
# 4. 编译阶段 (只编译指定的这一个 Benchmark)
# =========================================================
echo -e "\033[1;34m>>> Checking Benchmark Binary...\033[0m"
cd "$GEM5_ROOT/tests/spme_benchmark"

if [ ! -f "${BENCHMARK}.c" ]; then
    echo -e "\033[1;31m[ERROR] Source file ${BENCHMARK}.c not found!\033[0m"
    exit 1
fi
    #编译
    echo "  > Compiling $BENCHMARK ..."
    gcc -static -pthread -O3 "${BENCHMARK}.c" -o "bin/${BENCHMARK}" -lm
cd - > /dev/null

# =========================================================
# 5. 执行阶段
# =========================================================
RESULT_DIRS=()
# 这里的 Tag 可以区分这是一次跨架构对比测试
TAG="CrossArch_Test"

echo ""
echo -e "\033[1;33m[RUNNER] Starting Cross-Architecture Test for: $BENCHMARK\033[0m"
echo -e "\033[1;33m[CONFIG] Baseline Arch: ${ARCH_LIST[0]}\033[0m"

for arch in "${ARCH_LIST[@]}"; do
    echo ""
    echo -e "\033[1;34m>>> Running on Architecture: $arch ...\033[0m"

    # 调用 fast_run.sh
    # 注意：这里如果用户按 Ctrl+C，顶部的 trap 会捕获并退出脚本
    "$FAST_RUN" se "$arch" "$BENCHMARK" new "$TAG" --options="$PARAM" > /dev/null

    # 自动捕获结果路径
    LATEST_DIR=$(ls -td "$GEM5_ROOT"/tests/result/"$arch"/"${BENCHMARK}_"*_"$TAG" 2>/dev/null | head -1)

    if [ -d "$LATEST_DIR" ]; then
        RESULT_DIRS+=("$LATEST_DIR")
        echo -e "\033[1;32m  > Finished. Result: $(basename "$LATEST_DIR")\033[0m"
    else
        echo -e "\033[1;31m  > Error: Simulation failed for arch $arch\033[0m"
    fi
done

# =========================================================
# 6. 生成报告
# =========================================================
if [ ${#RESULT_DIRS[@]} -lt 1 ]; then
    echo "No valid results. Exiting."
    exit 0
fi

echo ""
echo -e "\033[1;33m[REPORT] Generating Comparison Table (Baseline vs Others)...\033[0m"

CMD="python3 $COMPARE_SCRIPT"
for dir in "${RESULT_DIRS[@]}"; do
    CMD="$CMD $dir"
done

# 注意: 这里不加 -p 参数，因为跨架构对比通常需要看 Diff/Speedup
# 只加 -f 保存文件
CMD="$CMD -f"

echo "  > Executing report generator..."
eval $CMD

echo -e "\033[1;32m[DONE] Check the generated report in tests/result/\033[0m"
