# created by Wang Yingsong on 2025-12-16
#!/bin/bash

# =========================================================
# Comprehensive Benchmark Suite Runner
# Purpose: Run different types of workloads to characterize architecture
# 1. Matrix (Balanced)
# 2. MonteCarlo (Compute Bound / High IPC)
# 3. Convolution (Spatial Locality / Balanced)
# 4. BFS (Memory Bound / Low IPC)
# 5. VecAdd (Simple Memory Streaming)
# 6. Stream (High Bandwidth Saturation)
# 7. TaskQueue (Synchronization / Lock Contention)
# =========================================================

# 1. 自动定位路径
SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
GEM5_ROOT=$(dirname "$SCRIPT_LOC")

FAST_RUN="$SCRIPT_LOC/fast_run.sh"
COMPARE_SCRIPT="$SCRIPT_LOC/compare_results.py"

# =========================================================
# 2. 负载定义 & 参数配置 (Balanced for Characterization)
# =========================================================
BENCHMARKS=(
    "spme_montecarlo"   # Compute Bound (High IPC)
    "spme_matrix"       # Balanced / Compute Intense
    "spme_vec_add"      # Simple Memory Streaming
    "spme_stream"       # High Bandwidth Saturation
    "spme_conv"         # Spatial Locality / Large Footprint
    "spme_bfs"          # Random Access / Latency Bound
    "spme_task_queue"   # Synchronization / Lock Contention
    "spme_hashtable"      # Data Serving / Random Access
    "spme_nqueens"        # SAT / Branch Heavy
    "spme_string_search"  # MapReduce / String Ops
)

# [参数说明]
# 旨在运行时间约 0.1s ~ 0.3s，既消除启动噪音，又不浪费时间。
# 数据规模均设定为超过 8MB LLC (Conventional Arch)，以触发真实的访存行为。
PARAMS=(
    "80000000"          # MonteCarlo: 80M iters (稳定 IPC)
    "512"               # Matrix: 512x512 (经典尺寸)
    "3000000"           # VecAdd: 3M elements (24MB > 8MB LLC)
    "2000000"           # Stream: 2M elements (Total 48MB > 8MB LLC)
    "2048"              # Conv: 2048x2048 (32MB Input > 8MB LLC)
    "16384"             # BFS: 65k nodes (随机范围大)
    "10000"            # TaskQueue: 100k tasks (竞争密集)
    "2000000"     # Hashtable: 2M buckets (~32MB > LLC)
    "13"          # N-Queens: 13x13 board
    "50000000"    # String Search: 50MB text
)

RESULT_DIRS=()

echo -e "\033[1;33m[SUITE] Starting Benchmark Suite (7 Workloads / Balanced Parameters)...\033[0m"

# 3. 批量编译 (全部重新编译)
echo -e "\033[1;34m>>> Compiling all benchmarks...\033[0m"
cd "$GEM5_ROOT/tests/spme_benchmark"

gcc -static -pthread -O3 spme_montecarlo.c -o bin/spme_montecarlo
gcc -static -pthread -O3 spme_matrix.c -o bin/spme_matrix
gcc -static -pthread -O3 spme_conv.c -o bin/spme_conv
gcc -static -pthread -O3 spme_bfs.c -o bin/spme_bfs
gcc -static -pthread -O3 spme_vec_add.c -o bin/spme_vec_add
gcc -static -pthread -O3 spme_stream.c -o bin/spme_stream
gcc -static -pthread -O3 spme_task_queue.c -o bin/spme_task_queue
gcc -static -pthread -O3 spme_hashtable.c -o bin/spme_hashtable
gcc -static -pthread -O3 spme_nqueens.c -o bin/spme_nqueens
gcc -static -pthread -O3 spme_string_search.c -o bin/spme_string_search

cd - > /dev/null

# 4. 循环运行
for i in "${!BENCHMARKS[@]}"; do
    BENCH=${BENCHMARKS[$i]}
    PARAM=${PARAMS[$i]}

    echo ""
    echo -e "\033[1;34m>>> Running $BENCH (Input: $PARAM) ...\033[0m"

    TAG="Characterization_Balanced"

    # 运行仿真
    "$FAST_RUN" se conventional "$BENCH" new "$TAG" --options="$PARAM" > /dev/null

    # 捕获结果
    LATEST_DIR=$(ls -td "$GEM5_ROOT"/tests/result/conventional/"${BENCH}_4c_"*"$TAG" | head -1)

    if [ -d "$LATEST_DIR" ]; then
        RESULT_DIRS+=("$LATEST_DIR")
        echo -e "\033[1;32m  > Finished $BENCH. Result: $LATEST_DIR\033[0m"
    else
        echo -e "\033[1;31m  > Error: Result directory for $BENCH not found!\033[0m"
    fi
done

# 5. 生成报告 (-p 平行对比)
echo ""
echo -e "\033[1;33m[SUITE] Generating Characterization Report...\033[0m"

CMD="python3 $COMPARE_SCRIPT"
for dir in "${RESULT_DIRS[@]}"; do
    CMD="$CMD $dir"
done
CMD="$CMD -f -p"  # 使用平行模式

echo "  > Executing: $CMD"
eval $CMD

echo -e "\033[1;32m[SUITE] All Done! Check tests/result/ for the report.\033[0m"
