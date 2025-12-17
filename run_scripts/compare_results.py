# created by Wang Yingsong on 2025-12-16
# /compare_results.py
# 用法: python3 compare_results.py <目录路径1> <目录路径2> ... [-f] [-p]
# =========================================================
# 比较多个 Gem5 仿真结果目录下的 stats.txt 文件，
# 提取关键性能指标并生成对比报告。
# 支持将报告保存为文件，便于归档和分享。
# =========================================================

import argparse
import os
import re
import statistics
import sys


# =========================================================
# 1. 数据提取模块
# =========================================================
def extract_dir_info(dir_path):
    abs_path = os.path.abspath(dir_path)
    parent_dir = os.path.dirname(abs_path)
    folder_name = os.path.basename(abs_path)

    # --- 智能提取 Benchmark 名称和核心数 ---
    cores = "N/A"
    core_match = re.search(r"_(\d+)c", folder_name)
    if core_match:
        cores = core_match.group(1)

    if core_match:
        bench_raw = folder_name[: core_match.start()]
    else:
        date_match = re.search(r"_\d{4}-\d{2}-\d{2}", folder_name)
        if date_match:
            bench_raw = folder_name[: date_match.start()]
        else:
            bench_raw = folder_name

    workload_name = bench_raw.rstrip("_")

    stats_path = os.path.join(dir_path, "stats.txt")

    data = {
        "Arch": os.path.basename(parent_dir),
        "Benchmark": workload_name,
        "Cores": cores,
        "Folder": folder_name,
        "Status": "OK",
        "Time": 0.0,
        "BW": 0.0,
        "LLC Miss": 0.0,
        "Total IPC": 0.0,
        "Avg IPC": 0.0,
        "Avg L1D Miss": 0.0,
        "Avg L1I Miss": 0.0,
        "Core IPCs": [],
        "Core L1D Misses": [],
        "Core L1I Misses": [],
    }

    if not os.path.exists(stats_path):
        data["Status"] = "Stats Missing"
        return data

    try:
        with open(stats_path) as f:
            content = f.read()

        # System Metrics
        m_time = re.search(r"simSeconds\s+([0-9\.]+)", content)
        if m_time:
            data["Time"] = float(m_time.group(1))

        m_miss = re.search(
            r"system\.(?:llc|l2cache)\.overallMissRate::total\s+([0-9\.]+)",
            content,
        )
        if m_miss:
            data["LLC Miss"] = float(m_miss.group(1)) * 100

        m_bw = re.search(
            r"system\.mem_ctrl.*\.bwTotal::total\s+([0-9\.]+)", content
        )
        if m_bw:
            data["BW"] = float(m_bw.group(1)) / (1024**3)

        # Per-Core Helper
        def get_per_core_stats(pattern):
            matches = re.findall(pattern, content)
            val_map = {int(k): float(v) for k, v in matches}
            sorted_keys = sorted(val_map.keys())
            return [val_map[k] for k in sorted_keys]

        data["Core IPCs"] = get_per_core_stats(
            r"system\.(?:core_list|cpu)(\d+)(?:\.cpu)?\.ipc\s+([0-9\.]+)"
        )

        l1d = get_per_core_stats(
            r"system\.(?:core_list|cpu)(\d+)(?:\.cpu)?\.dcache\.overallMissRate::total\s+([0-9\.]+)"
        )
        data["Core L1D Misses"] = [x * 100 for x in l1d]

        l1i = get_per_core_stats(
            r"system\.(?:core_list|cpu)(\d+)(?:\.cpu)?\.icache\.overallMissRate::total\s+([0-9\.]+)"
        )
        data["Core L1I Misses"] = [x * 100 for x in l1i]

        # Aggregates
        if data["Core IPCs"]:
            data["Total IPC"] = sum(data["Core IPCs"])
            data["Avg IPC"] = statistics.mean(data["Core IPCs"])
        if data["Core L1D Misses"]:
            data["Avg L1D Miss"] = statistics.mean(data["Core L1D Misses"])
        if data["Core L1I Misses"]:
            data["Avg L1I Miss"] = statistics.mean(data["Core L1I Misses"])

    except Exception as e:
        data["Status"] = f"Error: {e}"

    return data


# =========================================================
# 2. 格式化工具
# =========================================================
def format_cell(
    base_val,
    curr_val,
    metric_type,
    is_pct_val=False,
    is_baseline_row=False,
    show_diff=True,
):
    if is_pct_val:
        val_str = f"{curr_val:.2f}"
    else:
        val_str = f"{curr_val:.4f}"

    if not show_diff or is_baseline_row:
        return val_str

    if base_val == 0:
        return val_str

    if metric_type == "lower_is_better":
        if is_pct_val:
            diff = curr_val - base_val
            return f"{val_str} ({diff:+.2f})"
        else:
            if curr_val == 0:
                return val_str
            speedup = base_val / curr_val
            return f"{val_str} (x{speedup:.2f})"
    else:
        diff_pct = (curr_val - base_val) / base_val * 100
        return f"{val_str} ({diff_pct:+.1f}%)"


# =========================================================
# 3. 主程序
# =========================================================
def main():
    parser = argparse.ArgumentParser(description="Gem5 Comparison Tool")
    parser.add_argument(
        "directories", nargs="+", help="Result directories (1st is Baseline)"
    )
    parser.add_argument(
        "-f", "--file", action="store_true", help="Save report to file"
    )
    parser.add_argument(
        "-p",
        "--parallel",
        action="store_true",
        help="Parallel Mode: Show raw values only",
    )
    args = parser.parse_args()

    results = []
    for d in args.directories:
        res = extract_dir_info(d)
        # [预处理] 合并 Arch 和 Cores，方便 Part 1 显示
        if res["Cores"] != "N/A":
            res["Arch_Full"] = f"{res['Arch']} ({res['Cores']})"
        else:
            res["Arch_Full"] = res["Arch"]
        results.append(res)

    if not results:
        sys.exit(1)

    base = results[0]
    output_lines = []

    # ================= PART 1: SUMMARY TABLE =================
    output_lines.append("=" * 150)
    if args.parallel:
        output_lines.append(
            "PART 1: WORKLOAD CHARACTERIZATION (Parallel View - No Baseline)"
        )
    else:
        output_lines.append(
            f"PART 1: PERFORMANCE COMPARISON (Baseline: {base['Arch']} - {base['Benchmark']})"
        )
    output_lines.append("=" * 150)

    col_w = 18
    col_w_wide = 24  # 加宽以容纳 Arch (Cores) 和长名字

    # [优化] 将 Cores 合并到 Arch 列
    summary_cols = [
        ("Arch (Cores)", "Arch_Full", "text", False, col_w_wide),
        ("Benchmark", "Benchmark", "text", False, col_w_wide),
        # ("Cores"...) 已删除
        ("Time (s)", "Time", "lower_is_better", False, col_w),
        ("Avg IPC", "Avg IPC", "higher_is_better", False, col_w),
        ("Total IPC", "Total IPC", "higher_is_better", False, col_w),
        ("BW (GB/s)", "BW", "higher_is_better", False, col_w),
        ("LLC Miss(%)", "LLC Miss", "lower_is_better", True, col_w),
    ]

    header = ""
    for col in summary_cols:
        name, _, _, _, width = col
        header += f"{name:<{width}} "
    header += "Source Folder"

    output_lines.append(header)
    output_lines.append("-" * 150)

    for idx, res in enumerate(results):
        row_str = ""
        is_base = idx == 0
        for col in summary_cols:
            head, key, mtype, is_pct, width = col
            val = res.get(key, 0)
            base_val = base.get(key, 0)

            if mtype == "text":
                val_str = str(val)
                if len(val_str) > width - 1:
                    val_str = val_str[: width - 2] + ".."
                row_str += f"{val_str:<{width}} "
            else:
                cell = format_cell(
                    base_val,
                    val,
                    mtype,
                    is_pct,
                    is_baseline_row=is_base,
                    show_diff=(not args.parallel),
                )
                row_str += f"{cell:<{width}} "

        row_str += f"{res['Folder']}"
        output_lines.append(row_str)

    output_lines.append("")
    output_lines.append("")

    # ================= PART 2: DETAILED VIEW =================
    output_lines.append("=" * 90)
    output_lines.append(
        f"PART 2: INDIVIDUAL RUN DETAILS (System & Per-Core Data)"
    )
    output_lines.append("=" * 90)

    sys_label_width = 35

    for idx, res in enumerate(results):
        cores_info = (
            f"{res['Cores']} Cores" if res["Cores"] != "N/A" else "N/A"
        )
        output_lines.append(
            f"[{idx}] Arch: {res['Arch']} ({cores_info}) | Workload: {res['Benchmark']}"
        )
        output_lines.append(f"    Folder: {res['Folder']}")
        output_lines.append("-" * 60)

        output_lines.append(
            f"{'[System] Execution Time':<{sys_label_width}} | {res['Time']:.6f} s"
        )
        output_lines.append(
            f"{'[System] Total IPC':<{sys_label_width}} | {res['Total IPC']:.4f}"
        )
        output_lines.append(
            f"{'[System] Avg IPC':<{sys_label_width}} | {res['Avg IPC']:.4f}"
        )
        output_lines.append(
            f"{'[System] LLC Miss Rate':<{sys_label_width}} | {res['LLC Miss']:.2f} %"
        )
        output_lines.append(
            f"{'[System] DRAM Bandwidth':<{sys_label_width}} | {res['BW']:.2f} GB/s"
        )
        output_lines.append(
            f"{'[System] Avg L1D Miss':<{sys_label_width}} | {res['Avg L1D Miss']:.2f} %"
        )
        output_lines.append(
            f"{'[System] Avg L1I Miss':<{sys_label_width}} | {res['Avg L1I Miss']:.2f} %"
        )

        output_lines.append("")

        ipcs = res.get("Core IPCs", [])
        l1d = res.get("Core L1D Misses", [])
        l1i = res.get("Core L1I Misses", [])

        if ipcs:
            cw_id = 8
            cw_val = 15
            table_header = f"    {'Core ID':<{cw_id}} | {'IPC':<{cw_val}} | {'L1D Miss (%)':<{cw_val}} | {'L1I Miss (%)':<{cw_val}}"
            output_lines.append("    " + "." * (len(table_header)))
            output_lines.append(table_header)
            output_lines.append("    " + "." * (len(table_header)))

            for i in range(len(ipcs)):
                ipc_val = ipcs[i] if i < len(ipcs) else 0
                l1d_val = l1d[i] if i < len(l1d) else 0
                l1i_val = l1i[i] if i < len(l1i) else 0
                row_str = f"    {i:<{cw_id}} | {ipc_val:<{cw_val}.4f} | {l1d_val:<{cw_val}.2f} | {l1i_val:<{cw_val}.2f}"
                output_lines.append(row_str)
        else:
            output_lines.append("  (No Per-Core Data Available)")

        output_lines.append("")
        output_lines.append("=" * 90)
        output_lines.append("")

    final_output = "\n".join(output_lines)

    # ================= FILE OUTPUT LOGIC (VS Split) =================
    if args.file:
        unique_archs = sorted(list({r["Arch"] for r in results}))
        unique_benchs = sorted(list({r["Benchmark"] for r in results}))

        script_dir = os.path.dirname(os.path.abspath(__file__))
        gem5_root = os.path.dirname(script_dir)
        base_result_dir = os.path.join(gem5_root, "tests", "result")

        filename = "Compare_Report.txt"
        save_dir = base_result_dir

        # 逻辑 A: 同一架构，不同负载 (Parallel Mode)
        if len(unique_archs) == 1:
            target_arch = unique_archs[0]
            save_dir = os.path.join(base_result_dir, target_arch)
            # 样例名之间用 _vs_ 分割
            bench_str = "_vs_".join(unique_benchs)
            filename = f"Compare_Report_{target_arch}_{bench_str}.txt"

        # 逻辑 B: 不同架构 (Cross-Arch Mode)
        else:
            save_dir = base_result_dir
            # 架构名之间用 _vs_ 分割
            arch_str = "_vs_".join(unique_archs)
            # 负载名之间用 _vs_ 分割 (如果也是多个)
            bench_str = "_vs_".join(unique_benchs)
            filename = f"Compare_Report_{arch_str}_{bench_str}.txt"

        filename = re.sub(r'[\\/*?:"<>|]', "", filename)[:250]
        if not filename.endswith(".txt"):
            filename += ".txt"

        if not os.path.exists(save_dir):
            try:
                os.makedirs(save_dir)
            except OSError:
                pass

        full_save_path = os.path.join(save_dir, filename)

        try:
            with open(full_save_path, "w") as f:
                f.write(final_output)
            print(f"\033[1;32m[SUCCESS] Report saved: {full_save_path}\033[0m")
        except Exception as e:
            print(f"Error saving file: {e}")
            print(final_output)
    else:
        print(final_output)


if __name__ == "__main__":
    main()
