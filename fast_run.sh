# created by Wang Yingsong on 2025-12-12
#!/bin/bash

# =========================================================
# 快速运行脚本 (fast_run.sh)
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
# 辅助函数: 自动提取关键指标 (支持 Avg/Total 计算)
# =========================================================
extract_key_metrics() {
    local out_dir=$1
    local stats_file="$out_dir/stats.txt"
    local summary_file="$out_dir/summary.txt"

    echo "========================================" > "$summary_file"
    echo "       Automatic Simulation Summary     " >> "$summary_file"
    echo "========================================" >> "$summary_file"
    echo "Generated at: $(date)" >> "$summary_file"
    echo "" >> "$summary_file"

    if [ -f "$stats_file" ]; then
        echo "--- 1. Execution Time ---" >> "$summary_file"
        grep "simSeconds" "$stats_file" | awk '{printf "Time: %.6f s\n", $2}' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 2. IPC Stats (Per Core & Aggregate) ---" >> "$summary_file"

        # 使用 awk 自动计算 Average IPC 和 Total IPC
        # 兼容 system.cpuX.ipc (Conventional) 和 system.core_listX.ipc (Standard)
        grep -E "system\.(cpu|core_list)[0-9]+(\.cpu)?\.ipc" "$stats_file" | awk '
            {
                val=$2;
                sum+=val;
                count++;
                # 尝试从 $1 中提取核心编号
                split($1, a, ".");
                name=a[2]; # cpu0 or core_list0
                printf "%-15s: %.4f\n", name, val;
            }
            END {
                if(count>0) {
                    print "--------------------------";
                    printf "Total IPC      : %.4f (System Throughput)\n", sum;
                    printf "Avg   IPC      : %.4f (Per-Core Efficiency)\n", sum/count;
                }
            }' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 3. L1 D-Cache Miss Rate (Avg) ---" >> "$summary_file"
        grep -E "system\.(cpu|core_list)[0-9]+(\.cpu)?\.dcache\.overallMissRate::total" "$stats_file" | awk '
            { sum+=$2; count++; }
            END {
                if(count>0) printf "Avg L1D Miss   : %.2f%%\n", (sum/count)*100;
                else print "N/A";
            }' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 4. Memory Subsystem ---" >> "$summary_file"
        # 抓取 LLC Miss Rate
        grep -E "(l2cache|llc)\.overallMissRate::total" "$stats_file" | awk '{printf "LLC Miss Rate  : %.2f%%\n", $2 * 100}' >> "$summary_file"
        # 抓取 带宽 (如果存在)
        grep "system.mem_ctrl.*\.bwTotal::total" "$stats_file" | awk '{printf "DRAM Bandwidth : %.2f GB/s\n", $2 / 1024 / 1024 / 1024}' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 5. Full Stats Path ---" >> "$summary_file"
        echo "$stats_file" >> "$summary_file"
    else
        echo "Error: stats.txt not found!" >> "$summary_file"
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
    local core_count=$5   # [新增] 接收核心数参数

    # 1. 构建输出路径
    local dir_name="${test_name}_${core_count}c${dir_suffix}"
    local out_dir="$RESULT_ROOT/$arch/$dir_name"
    local log_file="sim_console.log"

    # 2. 覆盖逻辑检查
    if [ -d "$out_dir" ]; then
        if [ "$is_new_mode" == "true" ]; then
            echo -e "\033[1;33m[WARN] Directory exists in NEW mode (rare): $out_dir\033[0m"
        else
            echo -e "\033[1;33m[INFO] Overwriting existing result...\033[0m"
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
    echo -e "  > Cores     : \033[1;36m$core_count\033[0m"
    echo -e "  > Output Dir: $out_dir"
    echo -e "\033[1;32m============================================\033[0m"

    # 4. 执行 GEM5
    ./$BUILD_BIN \
        --outdir="$out_dir" \
        $SE_SCRIPT \
        --arch=$arch \
        --cmd=$test_name \
        --num-cpus=$core_count \
        --options="$core_count" \
        > "$out_dir/$log_file" 2>&1

    # =========================================================
    # 结果处理
    # =========================================================
    local exit_code=$?
    local abs_log_path=$(readlink -f "$out_dir/$log_file")

    if [ $exit_code -eq 0 ]; then
        generate_topology_images "$out_dir"

        # 提取关键数据 (含平均值)
        extract_key_metrics "$out_dir"
        local abs_summary_path=$(readlink -f "$out_dir/summary.txt")

        echo -e "\033[1;32m[SUCCESS] Simulation Finished.\033[0m"
        echo -e "  > View Summary: \033[4;36mfile://$abs_summary_path\033[0m"
        echo -e "  > View Full Log: \033[4;36mfile://$abs_log_path\033[0m"
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
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        generate_topology_images "$out_dir"
        extract_key_metrics "$out_dir"
        local abs_out_path=$(readlink -f "$out_dir")
        echo -e "  > Result Dir: \033[4;36mfile://$abs_out_path\033[0m"
    fi
}

# =========================================================
# 参数解析逻辑
# =========================================================

if [ $# -lt 2 ]; then
    echo "Usage: [CORES=N] ./fast_run.sh <se/fs> <arch> <workload> [new] [remarks...]"
    exit 1
fi

if [ -z "$CORES" ]; then CORES=4; fi

MODE=$1
ARCH=$2
WORKLOAD=""

if [ "$MODE" == "fs" ]; then
    if [ "$3" == "new" ]; then SUFFIX="_$(date +%Y-%m-%d_%H-%M-%S)"; else SUFFIX="_$(date +%Y-%m-%d)"; fi
    do_fs $ARCH "$SUFFIX"
    exit 0
fi

if [ "$MODE" == "se" ]; then
    if [ -z "$3" ]; then echo "Error: SE mode requires a workload name."; exit 1; fi
    WORKLOAD=$3
    shift 3
    IS_NEW=false
    NOTE=""
    for arg in "$@"; do
        if [ "$arg" == "new" ]; then IS_NEW=true; else if [ -z "$NOTE" ]; then NOTE="${arg}"; else NOTE="${NOTE}_${arg}"; fi; fi
    done
    if [ "$IS_NEW" == "true" ]; then TIMESTAMP="_$(date +%Y-%m-%d_%H-%M-%S)"; else TIMESTAMP="_$(date +%Y-%m-%d)"; fi
    FULL_SUFFIX="${TIMESTAMP}"
    if [ ! -z "$NOTE" ]; then FULL_SUFFIX="${FULL_SUFFIX}_${NOTE}"; fi

    do_se $ARCH $WORKLOAD "$FULL_SUFFIX" "$IS_NEW" "$CORES"
else
    echo "Invalid mode: $MODE. Use 'se' or 'fs'."
    exit 1
fi
