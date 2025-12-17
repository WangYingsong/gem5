# created by Wang Yingsong on 2025-12-17
#!/bin/bash

# =========================================================
# 参数调优自动化脚本 (Parameter Tuning Runner)
# 功能：自动在 Min 和 Max 之间取 5 个采样点，运行测试并生成对比报告。
# Usage: ./run_param_tuning.sh <ARCH> <BENCHMARK> <MIN> <MAX>
# Example: ./run_param_tuning.sh conventional spme_matrix 128 1024
# =========================================================

# 中断处理：按 Ctrl+C 停止所有任务
trap 'echo -e "\n\033[1;31m[ABORT] User interrupted execution. Stopping...\033[0m"; exit 1' SIGINT

# 1. 路径配置
SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
GEM5_ROOT=$(dirname "$SCRIPT_LOC")
FAST_RUN="$SCRIPT_LOC/fast_run.sh"
COMPARE_SCRIPT="$SCRIPT_LOC/compare_results.py"

# =========================================================
# 2. 输入解析
# =========================================================
if [ $# -lt 4 ]; then
    echo "Usage: $0 <ARCH> <BENCHMARK> <MIN_PARAM> <MAX_PARAM>"
    echo "Example: $0 conventional spme_matrix 128 512"
    exit 1
fi

ARCH=$1
BENCHMARK=$2
MIN_VAL=$3
MAX_VAL=$4

# 简单检查参数合法性
if [ "$MIN_VAL" -gt "$MAX_VAL" ]; then
    echo "Error: MIN ($MIN_VAL) cannot be greater than MAX ($MAX_VAL)."
    exit 1
fi

# =========================================================
# 3. 计算 5 个采样点 (使用 Python 确保整数运算准确)
# =========================================================
echo -e "\033[1;34m>>> Calculating Step Sizes...\033[0m"

# 生成 5 个线性分布的整数点，去重并排序
PARAM_LIST=($(python3 -c "
min_v = int($MIN_VAL)
max_v = int($MAX_VAL)
if min_v == max_v:
    print(min_v)
else:
    # 生成 5 个点：0%, 25%, 50%, 75%, 100%
    steps = [int(min_v + i * (max_v - min_v) / 4.0) for i in range(5)]
    # 确保 max_v 一定在列表里 (防止取整误差)
    steps[-1] = max_v
    # 去重并排序
    unique_steps = sorted(list(set(steps)))
    print(' '.join(map(str, unique_steps)))
"))

echo -e "  > Target Parameters: [ ${PARAM_LIST[*]} ]"

# =========================================================
# 4. 编译阶段 (确保二进制是最新的)
# =========================================================
echo ""
echo -e "\033[1;34m>>> Checking Benchmark Binary...\033[0m"
cd "$GEM5_ROOT/tests/spme_benchmark"
if [ ! -f "${BENCHMARK}.c" ]; then
    echo -e "\033[1;31m[ERROR] Source file ${BENCHMARK}.c not found!\033[0m"
    exit 1
fi
echo "  > Compiling $BENCHMARK ..."
gcc -static -pthread -O3 "${BENCHMARK}.c" -o "bin/${BENCHMARK}" -lm
cd - > /dev/null

# =========================================================
# 5. 执行循环
# =========================================================
RESULT_DIRS=()
# 使用当前时间戳防止多次运行冲突
TIMESTAMP=$(date "+%H%M%S")

echo ""
echo -e "\033[1;33m[RUNNER] Starting Parameter Sweep for: $BENCHMARK\033[0m"

for param in "${PARAM_LIST[@]}"; do
    echo ""
    echo -e "\033[1;34m>>> Running with Parameter: $param ...\033[0m"

    # 构造唯一的 Tag，方便 fast_run 识别
    # Tag 格式: Sweep_P<参数值>_<时间戳>
    RUN_TAG="Sweep_P${param}_${TIMESTAMP}"

    # 调用 fast_run.sh
    # 传递 new 标志以确保不删除旧目录（虽然 fast_run 默认逻辑是覆盖同名，但我们要产生不同名的）
    # --options="$param" 传递给 SE 模式
    "$FAST_RUN" se "$ARCH" "$BENCHMARK" new "$RUN_TAG" --options="$param" > /dev/null

    # 自动捕获最新生成的结果路径
    # 路径模式：tests/result/<ARCH>/<BENCHMARK>_<CORES>c_<DATE>_<TIME>_<TAG>
    # 我们利用 TAG 来精准定位
    LATEST_DIR=$(ls -td "$GEM5_ROOT"/tests/result/"$ARCH"/"${BENCHMARK}_"*_"$RUN_TAG" 2>/dev/null | head -1)

    if [ -d "$LATEST_DIR" ]; then
        RESULT_DIRS+=("$LATEST_DIR")
        echo -e "\033[1;32m  > Finished. Result: $(basename "$LATEST_DIR")\033[0m"
    else
        echo -e "\033[1;31m  > Error: Simulation failed for param $param\033[0m"
        exit 1
    fi
done

# =========================================================
# 6. 结果归档 (Result Archiving)
# =========================================================
if [ ${#RESULT_DIRS[@]} -eq 0 ]; then
    echo "No results collected. Exiting."
    exit 0
fi

# 定义归档目录名称：Benchmark_Sweep_Min_Max
ARCH_ROOT="$GEM5_ROOT/tests/result/$ARCH"
SWEEP_DIR_NAME="${BENCHMARK}_Sweep_${MIN_VAL}_to_${MAX_VAL}_$(date +%Y%m%d_%H%M)"
FINAL_SWEEP_DIR="$ARCH_ROOT/$SWEEP_DIR_NAME"

echo ""
echo -e "\033[1;33m[ARCHIVE] Moving results to subfolder: $SWEEP_DIR_NAME\033[0m"
mkdir -p "$FINAL_SWEEP_DIR"

MOVED_DIRS=()
for dir in "${RESULT_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        mv "$dir" "$FINAL_SWEEP_DIR/"
        DIR_NAME=$(basename "$dir")
        MOVED_DIRS+=("$FINAL_SWEEP_DIR/$DIR_NAME")
    fi
done

# =========================================================
# 7. 生成对比报告
# =========================================================
echo ""
echo -e "\033[1;33m[REPORT] Generating Parameter Sensitivity Report...\033[0m"

# 构造 python 命令
# 这里的逻辑是：第一个参数（最小值）将作为 baseline，
# 后面的参数结果会显示相对于最小值的变化幅度（如 IPC 增长了多少）
CMD="python3 $COMPARE_SCRIPT"
for dir in "${MOVED_DIRS[@]}"; do
    CMD="$CMD $dir"
done

# -f: 保存为文件
# 我们不加 -p (parallel)，因为我们希望看到 diff。
# 比如：参数从 128 变到 256，IPC 变成了多少，相对于 128 的变化幅度。
CMD="$CMD -f"

echo "  > Executing report generator..."
eval $CMD

echo -e "\033[1;32m[DONE] Parameter Sweep Completed.\033[0m"
echo -e "  > Data Location: tests/result/$ARCH/$SWEEP_DIR_NAME"
