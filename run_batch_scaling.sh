#!/bin/bash

# =========================================================
# Matrix Scaling Batch Runner
# Purpose: Run N=128, 256, 384, 450 sequentially & Compare
# =========================================================

# 1. 定义要测试的矩阵尺寸列表
SIZES=(128 256 384 450)
SRC_FILE="tests/spme_benchmark/spme_matrix.c"
RESULT_DIRS=()

echo -e "\033[1;33m[BATCH] Starting Matrix Scaling Test (N=${SIZES[*]})...\033[0m"

# 2. 循环运行
for N in "${SIZES[@]}"; do
    echo ""
    echo -e "\033[1;34m>>> Processing N=$N ...\033[0m"

    # 2.1 修改 C 代码中的 DEFAULT_N (使用 sed 替换)
    # 寻找 "#define DEFAULT_N 数字" 并替换为新的 N
    sed -i "s/#define DEFAULT_N [0-9]\+/#define DEFAULT_N $N/" "$SRC_FILE"

    # 2.2 重新编译 (必须 -O3)
    echo "  > Compiling..."
    cd tests/spme_benchmark
    gcc -static -pthread -O3 spme_matrix.c -o bin/spme_matrix
    cd ../../

    # 2.3 运行仿真
    # 使用 Step1_Scaling_N{N} 作为后缀，方便识别
    TAG="Step1_Scaling_N${N}"
    echo "  > Running Gem5..."
    ./fast_run.sh se standard spme_matrix new "$TAG" > /dev/null

    # 2.4 捕获结果目录名 (用于最后的对比)
    # 查找刚刚生成的最新目录
    LATEST_DIR=$(ls -td tests/result/standard/spme_matrix_4c_*_"$TAG" | head -1)
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
CMD="python3 compare_results.py"
for dir in "${RESULT_DIRS[@]}"; do
    CMD="$CMD $dir"
done
CMD="$CMD -f"

# 执行对比
echo "  > Executing: $CMD"
eval $CMD

echo -e "\033[1;32m[BATCH] All Done!\033[0m"
