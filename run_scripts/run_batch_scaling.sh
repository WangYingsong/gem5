# created by Wang Yingsong on 2025-12-16
#!/bin/bash

# =========================================================
# Matrix Scaling Batch Runner
# Purpose: Run N=128, 256, 384, 450 sequentially & Compare
# =========================================================

# 1. 自动定位路径
SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
GEM5_ROOT=$(dirname "$SCRIPT_LOC")

# 关键文件路径
SRC_FILE="$GEM5_ROOT/tests/spme_benchmark/spme_matrix.c"
FAST_RUN="$SCRIPT_LOC/fast_run.sh"
# 都在 run_scripts 目录下，直接引用
COMPARE_SCRIPT="$SCRIPT_LOC/compare_results.py"

# 定义要测试的矩阵尺寸列表
SIZES=(128 256 384 450)
RESULT_DIRS=()

echo -e "\033[1;33m[BATCH] Starting Matrix Scaling Test (N=${SIZES[*]})...\033[0m"

# 2. 循环运行
for N in "${SIZES[@]}"; do
    echo ""
    echo -e "\033[1;34m>>> Processing N=$N ...\033[0m"

    # 2.1 修改 C 代码中的 DEFAULT_N (使用 sed 替换)
    sed -i "s/#define DEFAULT_N [0-9]\+/#define DEFAULT_N $N/" "$SRC_FILE"

    # 2.2 重新编译 (必须 -O3)
    echo "  > Compiling..."
    # 切换到目录编译，然后切换回来，或者使用完整路径编译
    cd "$GEM5_ROOT/tests/spme_benchmark"
    gcc -static -pthread -O3 spme_matrix.c -o bin/spme_matrix
    cd - > /dev/null # 返回之前目录，保持脚本逻辑清晰

    # 2.3 运行仿真
    TAG="Step1_Scaling_N${N}"
    echo "  > Running Gem5..."
    # 调用同一目录下的 fast_run.sh
    "$FAST_RUN" se conventional spme_matrix new "$TAG" > /dev/null

    # 2.4 捕获结果目录名 (查找最新结果)
    LATEST_DIR=$(ls -td "$GEM5_ROOT"/tests/result/conventional/spme_matrix_4c_*_"$TAG" | head -1)
    if [ -d "$LATEST_DIR" ]; then
        RESULT_DIRS+=("$LATEST_DIR")
        echo -e "\033[1;32m  > Finished N=$N. Result: $LATEST_DIR\033[0m"
    else
        echo -e "\033[1;31m  > Error: Result directory for N=$N not found!\033[0m"
        exit 1
    fi
done

# 3. 恢复 C 代码默认值 (设回 450)
sed -i "s/#define DEFAULT_N [0-9]\+/#define DEFAULT_N 450/" "$SRC_FILE"

# 4. 生成对比报告
echo ""
echo -e "\033[1;33m[BATCH] Generating Comparison Report...\033[0m"

# 构建 python 命令参数
CMD="python3 $COMPARE_SCRIPT"
for dir in "${RESULT_DIRS[@]}"; do
    CMD="$CMD $dir"
done
CMD="$CMD -f"

# 执行对比
echo "  > Executing Comparison..."
eval $CMD

echo -e "\033[1;32m[BATCH] All Done!\033[0m"
