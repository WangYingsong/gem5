# created by Wang Yingsong on 2025-12-12
#!/bin/bash

# =========================================================
# 快速运行脚本 (fast_run.sh) - Topology Option Added
# Usage: [CORES=N] ./run_scripts/fast_run.sh <se/fs> <arch> <workload> [new] [remarks...] [--topo]
# =========================================================

# 1. 自动定位 GEM5 根目录
SCRIPT_LOC=$(cd "$(dirname "$0")" && pwd)
GEM5_ROOT=$(dirname "$SCRIPT_LOC")

# 2. 定义绝对路径
BUILD_BIN="$GEM5_ROOT/build/X86/gem5.opt"
SCRIPT_DIR="$GEM5_ROOT/configs/thesis"
TEST_BIN_DIR="$GEM5_ROOT/tests/spme_benchmark/bin"
RESULT_ROOT="$GEM5_ROOT/tests/result"

# Python 入口
SE_SCRIPT="$SCRIPT_DIR/run_se.py"
FS_SCRIPT="$SCRIPT_DIR/run_fs.py"

# [Config] 默认不生成拓扑图
GEN_TOPOLOGY=false

# =========================================================
# 辅助函数: 生成拓扑图
# =========================================================
generate_topology_images() {
    if [ "$GEN_TOPOLOGY" != "true" ]; then
        return
    fi
    local out_dir=$1
    local dot_file="$out_dir/config.dot"
    if [ -f "$dot_file" ] && command -v dot &> /dev/null; then
        echo -e "  > Generating topology image..."
        dot -Tsvg "$dot_file" -o "$out_dir/topology.svg"
    fi
}

# =========================================================
# 辅助函数: 自动提取关键指标 (包含配置信息提取)
# =========================================================
extract_key_metrics() {
    local out_dir=$1
    local start_time=$2
    local end_time=$3
    local duration=$4

    local stats_file="$out_dir/stats.txt"
    local config_file="$out_dir/config.ini"
    local summary_file="$out_dir/summary.txt"

    # --- 1. 定义单位转换函数 (Bytes -> KB/MB) ---
    to_human() {
        local size=$1
        if [ -z "$size" ]; then echo "N/A"; return; fi
        awk -v val="$size" 'BEGIN {
            if(val >= 1073741824) printf "%.2f GB", val/1073741824;
            else if(val >= 1048576) printf "%.2f MB", val/1048576;
            else if(val >= 1024) printf "%.2f kB", val/1024;
            else printf "%d B", val;
        }'
    }

    # --- 2. 针对 config.ini 进行精准提取 ---
    local raw_l1d=""
    local raw_l1i=""
    local raw_llc=""

    if [ -f "$config_file" ]; then
        # 提取 L1 D-Cache (定位 [system.cpu0.dcache] 下的 size)
        raw_l1d=$(grep -A 20 "\[system.cpu0.dcache\]" "$config_file" | grep "^size=" | head -n 1 | cut -d'=' -f2 | tr -d '\r')

        # 提取 L1 I-Cache (定位 [system.cpu0.icache] 下的 size)
        raw_l1i=$(grep -A 20 "\[system.cpu0.icache\]" "$config_file" | grep "^size=" | head -n 1 | cut -d'=' -f2 | tr -d '\r')

        # 提取 LLC (定位 [system.llc] 下的 size)
        raw_llc=$(grep -A 20 "\[system.llc\]" "$config_file" | grep "^size=" | head -n 1 | cut -d'=' -f2 | tr -d '\r')

        # 兼容性兜底：如果没找到 [system.llc]，再试 [system.l2]
        if [ -z "$raw_llc" ]; then
            raw_llc=$(grep -A 20 "\[system.l2" "$config_file" | grep "^size=" | head -n 1 | cut -d'=' -f2 | tr -d '\r')
        fi
    fi

    # 转换为易读格式
    local l1d_size=$(to_human "$raw_l1d")
    local l1i_size=$(to_human "$raw_l1i")
    local llc_size=$(to_human "$raw_llc")

    echo "========================================" > "$summary_file"
    echo "       Automatic Simulation Summary     " >> "$summary_file"
    echo "========================================" >> "$summary_file"
    echo "Simulation Start : $start_time" >> "$summary_file"
    echo "Simulation End   : $end_time" >> "$summary_file"
    echo "Total Duration   : ${duration} s" >> "$summary_file"
    echo "" >> "$summary_file"

    if [ -f "$stats_file" ]; then
        echo "--- 1. Simulated Time (Virtual) ---" >> "$summary_file"
        grep "simSeconds" "$stats_file" | awk '{printf "Time: %.6f s\n", $2}' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 2. IPC Stats (Per Core & Aggregate) ---" >> "$summary_file"
        grep -E "system\.(cpu|core_list)[0-9]+(\.cpu)?\.ipc" "$stats_file" | awk '
            {
                val=$2;
                sum+=val;
                count++;
                split($1, a, ".");
                name=a[2];
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
        echo "--- 3. L1 D-Cache Miss Rate (Data) | Size: ${l1d_size} ---" >> "$summary_file"
        grep -E "system\.(cpu|core_list)[0-9]+(\.cpu)?\.dcache\.overallMissRate::total" "$stats_file" | awk '
            {
                val=$2 * 100;
                sum+=val;
                count++;
                split($1, a, ".");
                name=a[2];
                printf "%-15s: %.2f%%\n", name, val;
            }
            END {
                if(count>0) {
                    print "--------------------------";
                    printf "Avg L1D Miss   : %.2f%%\n", sum/count;
                } else print "N/A";
            }' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 4. L1 I-Cache Miss Rate (Instruction) | Size: ${l1i_size} ---" >> "$summary_file"
        grep -E "system\.(cpu|core_list)[0-9]+(\.cpu)?\.icache\.overallMissRate::total" "$stats_file" | awk '
            {
                val=$2 * 100;
                sum+=val;
                count++;
                split($1, a, ".");
                name=a[2];
                printf "%-15s: %.2f%%\n", name, val;
            }
            END {
                if(count>0) {
                    print "--------------------------";
                    printf "Avg L1I Miss   : %.2f%%\n", sum/count;
                } else print "N/A";
            }' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 5. Memory Subsystem | LLC Size: ${llc_size} ---" >> "$summary_file"
        grep -E "(l2cache|llc)\.overallMissRate::total" "$stats_file" | awk '{printf "LLC Miss Rate  : %.2f%%\n", $2 * 100}' >> "$summary_file"
        grep "system.mem_ctrl.*\.bwTotal::total" "$stats_file" | awk '{printf "DRAM Bandwidth : %.2f GB/s\n", $2 / 1024 / 1024 / 1024}' >> "$summary_file"

        echo "" >> "$summary_file"
        echo "--- 6. Full Stats Path ---" >> "$summary_file"
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
    local is_new_mode=$4
    local core_count=$5
    local extra_options=$6

    local dir_name="${test_name}_${core_count}c${dir_suffix}"
    local out_dir="$RESULT_ROOT/$arch/$dir_name"
    local log_file="sim_console.log"

    if [ -d "$out_dir" ]; then
        if [ "$is_new_mode" == "true" ]; then
            echo -e "\033[1;33m[WARN] Directory exists in NEW mode: $out_dir\033[0m"
        else
            echo -e "\033[1;33m[INFO] Overwriting existing result...\033[0m"
            rm -rf "$out_dir"
        fi
    fi
    mkdir -p "$out_dir"

    echo -e "\033[1;32m============================================\033[0m"
    echo -e "          SPME THESIS RUNNER (SE MODE)"
    echo -e "\033[1;32m============================================\033[0m"
    echo -e "  > Arch      : \033[1;35m$arch\033[0m"
    echo -e "  > Workload  : \033[1;33m$test_name\033[0m"
    echo -e "  > Cores     : \033[1;36m$core_count\033[0m"
    echo -e "  > Output Dir: $out_dir"
    if [ ! -z "$extra_options" ]; then
        echo -e "  > Options   : $extra_options"
    fi
    echo -e "\033[1;32m============================================\033[0m"

    local opts="$core_count"
    if [ ! -z "$extra_options" ]; then
        opts="$extra_options"
    fi

    # [Requirement 1] 捕获开始时间
    local start_ts=$(date +%s)
    local start_fmt=$(date "+%Y-%m-%d %H:%M:%S")

    "$BUILD_BIN" \
        --outdir="$out_dir" \
        "$SE_SCRIPT" \
        --arch=$arch \
        --cmd=$test_name \
        --num-cpus=$core_count \
        --options="$opts" \
        > "$out_dir/$log_file" 2>&1

    local exit_code=$?

    # [Requirement 1] 捕获结束时间并计算耗时
    local end_ts=$(date +%s)
    local end_fmt=$(date "+%Y-%m-%d %H:%M:%S")
    local duration=$((end_ts - start_ts))

    local abs_log_path=$(readlink -f "$out_dir/$log_file")

    if [ $exit_code -eq 0 ]; then
        generate_topology_images "$out_dir"
        # 传递时间参数给提取函数
        extract_key_metrics "$out_dir" "$start_fmt" "$end_fmt" "$duration"
        local abs_summary_path=$(readlink -f "$out_dir/summary.txt")

        echo -e "\033[1;32m[SUCCESS] Simulation Finished in ${duration}s.\033[0m"
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

    # [Requirement 1] FS 模式同样添加时间捕获
    local start_ts=$(date +%s)
    local start_fmt=$(date "+%Y-%m-%d %H:%M:%S")

    "$BUILD_BIN" --outdir="$out_dir" "$FS_SCRIPT" --arch=$arch --restore

    local exit_code=$?

    local end_ts=$(date +%s)
    local end_fmt=$(date "+%Y-%m-%d %H:%M:%S")
    local duration=$((end_ts - start_ts))

    if [ $exit_code -eq 0 ]; then
        generate_topology_images "$out_dir"
        # 传递时间参数
        extract_key_metrics "$out_dir" "$start_fmt" "$end_fmt" "$duration"
        local abs_out_path=$(readlink -f "$out_dir")
        echo -e "  > Result Dir: \033[4;36mfile://$abs_out_path\033[0m"
    fi
}

# =========================================================
# 参数解析逻辑 (保持不变)
# =========================================================

if [ $# -lt 2 ]; then
    echo "Usage: [CORES=N] ./run_scripts/fast_run.sh <se/fs> <arch> <workload> [new] [remarks...] [--topo] [--options=...]"
    exit 1
fi

if [ -z "$CORES" ]; then CORES=4; fi

MODE=$1
ARCH=$2
WORKLOAD=""
EXTRA_OPTIONS=""

shift 2

if [ "$MODE" == "fs" ]; then
    IS_NEW=false
    SUFFIX="_$(date +%Y-%m-%d)"
    for arg in "$@"; do
        if [ "$arg" == "new" ]; then
            SUFFIX="_$(date +%Y-%m-%d_%H-%M-%S)"
        elif [ "$arg" == "--topo" ]; then
            GEN_TOPOLOGY=true
        fi
    done
    do_fs $ARCH "$SUFFIX"
    exit 0
fi

if [ "$MODE" == "se" ]; then
    if [ -z "$1" ]; then echo "Error: SE mode requires a workload name."; exit 1; fi
    WORKLOAD=$1
    shift 1

    IS_NEW=false
    NOTE=""

    for arg in "$@"; do
        if [ "$arg" == "new" ]; then
            IS_NEW=true
        elif [ "$arg" == "--topo" ]; then
            GEN_TOPOLOGY=true
        elif [[ "$arg" == --options=* ]]; then
            EXTRA_OPTIONS="${arg#*=}"
        else
            if [ -z "$NOTE" ]; then NOTE="${arg}"; else NOTE="${NOTE}_${arg}"; fi
        fi
    done

    if [ "$IS_NEW" == "true" ]; then TIMESTAMP="_$(date +%Y-%m-%d_%H-%M-%S)"; else TIMESTAMP="_$(date +%Y-%m-%d)"; fi
    FULL_SUFFIX="${TIMESTAMP}"
    if [ ! -z "$NOTE" ]; then FULL_SUFFIX="${FULL_SUFFIX}_${NOTE}"; fi

    do_se $ARCH $WORKLOAD "$FULL_SUFFIX" "$IS_NEW" "$CORES" "$EXTRA_OPTIONS"
else
    echo "Invalid mode: $MODE. Use 'se' or 'fs'."
    exit 1
fi
