# created by Wang Yingsong on 2025-12-17
#!/bin/bash

# =========================================================
# 批量参数调优调度器 (Batch Parameter Tuning Scheduler)
# 功能：调用 run_param_tuning.sh，一次性执行多个测试样例的参数扫描。
# 格式：./run_batch_tuning.sh <ARCH> "BENCH1:MIN:MAX" "BENCH2:MIN:MAX" ...
# 示例：./run_batch_tuning.sh conventional "spme_matrix:128:512" "spme_vec_add:100000:500000"
# =========================================================

# 1. 基础配置与中断处理
trap 'echo -e "\n\033[1;31m[ABORT] Batch execution interrupted by user.\033[0m"; exit 1' SIGINT

SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
TUNING_SCRIPT="$SCRIPT_LOC/run_param_tuning.sh"

# =========================================================
# 2. 输入解析与帮助
# =========================================================
if [ $# -lt 2 ]; then
    echo "Usage: $0 <ARCH> <BENCH_CONFIG_1> [BENCH_CONFIG_2] ..."
    echo ""
    echo "Config Format:  BENCHMARK_NAME:MIN_VALUE:MAX_VALUE"
    echo ""
    echo "Examples:"
    echo "  $0 conventional \"spme_matrix:128:1024\""
    echo "  $0 conventional \"spme_matrix:128:512\" \"spme_vec_add:100000:1000000\""
    exit 1
fi

ARCH=$1
shift # 移除第一个参数(ARCH)，剩下的都是任务配置
CONFIG_LIST=("$@")

# 检查调优脚本是否存在
if [ ! -f "$TUNING_SCRIPT" ]; then
    echo "Error: Tuning script not found at $TUNING_SCRIPT"
    exit 1
fi

# =========================================================
# 3. 批量执行循环
# =========================================================
TOTAL_TASKS=${#CONFIG_LIST[@]}
CURRENT_TASK=0
START_TIME=$(date +%s)

echo -e "\033[1;33m[BATCH] Starting Batch Tuning Session\033[0m"
echo -e "  > Target Arch : $ARCH"
echo -e "  > Total Tasks : $TOTAL_TASKS"
echo "---------------------------------------------------------"

for config in "${CONFIG_LIST[@]}"; do
    ((CURRENT_TASK++))

    # --- 解析 "BENCH:MIN:MAX" 字符串 ---
    # 使用 IFS (Internal Field Separator) 将冒号设为分隔符
    IFS=':' read -r bench min max <<< "$config"

    # --- 参数校验 ---
    if [ -z "$bench" ] || [ -z "$min" ] || [ -z "$max" ]; then
        echo -e "\033[1;31m[SKIP] Invalid config format: $config (Expected Name:Min:Max)\033[0m"
        continue
    fi

    echo ""
    echo -e "\033[1;34m>>> [Task $CURRENT_TASK/$TOTAL_TASKS] Tuning $bench ($min -> $max) ...\033[0m"

    # --- 调用核心调优脚本 ---
    "$TUNING_SCRIPT" "$ARCH" "$bench" "$min" "$max"

    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo -e "\033[1;32m>>> [Task $CURRENT_TASK] Completed Successfully.\033[0m"
    else
        echo -e "\033[1;31m>>> [Task $CURRENT_TASK] Failed with error code $EXIT_CODE.\033[0m"
        # 即使单个失败，我们也继续执行下一个任务，而不是直接退出整个批处理
    fi

    echo "---------------------------------------------------------"
done

# =========================================================
# 4. 结束汇总
# =========================================================
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""
echo -e "\033[1;33m[BATCH] All tasks finished.\033[0m"
echo -e "  > Total Duration: ${DURATION} seconds"
echo -e "  > Results are stored in: tests/result/$ARCH/"
