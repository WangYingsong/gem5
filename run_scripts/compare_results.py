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

    # 尝试提取 workload 名称，去除日期后缀
    match = re.match(r"^(.*?)_\d{4}-\d{2}-\d{2}", folder_name)
    workload_name = match.group(1) if match else folder_name

    stats_path = os.path.join(dir_path, "stats.txt")

    data = {
        "Arch": os.path.basename(parent_dir),
        "Benchmark": workload_name,
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
    show_diff=True,  # [新增] 控制是否显示差异
):
    # 1. 基础数值格式化
    if is_pct_val:
        val_str = f"{curr_val:.2f}"
    else:
        val_str = f"{curr_val:.4f}"

    # 2. 如果不显示差异，或者这是基准行，直接返回数值
    if not show_diff or is_baseline_row:
        return val_str

    if base_val == 0:
        return val_str

    # 3. 差异计算逻辑
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
        # higher is better
        diff_pct = (curr_val - base_val) / base_val * 100
        return f"{val_str} ({diff_pct:+.1f}%)"


# =========================================================
# 3. 主程序
# =========================================================
def main():
    parser = argparse.ArgumentParser(description="Split View Gem5 Comparison")
    parser.add_argument(
        "directories", nargs="+", help="Result directories (1st is Baseline)"
    )
    parser.add_argument(
        "-f",
        "--file",
        action="store_true",
        help="Save report to file (suppress console output)",
    )
    # [新增] 平行对比模式开关
    parser.add_argument(
        "-p",
        "--parallel",
        action="store_true",
        help="Parallel Mode: Show raw values only (Workload Characterization), no baseline comparison",
    )
    args = parser.parse_args()

    results = []
    for d in args.directories:
        res = extract_dir_info(d)
        results.append(res)

    if not results:
        sys.exit(1)

    base = results[0]
    output_lines = []

    # ================= PART 1: SUMMARY TABLE =================
    output_lines.append("=" * 140)
    if args.parallel:
        output_lines.append(
            "PART 1: WORKLOAD CHARACTERIZATION (Parallel View - No Baseline)"
        )
    else:
        output_lines.append(
            f"PART 1: PERFORMANCE COMPARISON (Baseline: {base['Arch']} - {base['Benchmark']})"
        )
    output_lines.append("=" * 140)

    # 定义列宽
    col_w = 18

    summary_cols = [
        ("Arch", "Arch", "text", False),
        ("Benchmark", "Benchmark", "text", False),
        ("Time (s)", "Time", "lower_is_better", False),
        ("Avg IPC", "Avg IPC", "higher_is_better", False),
        ("Total IPC", "Total IPC", "higher_is_better", False),
        ("BW (GB/s)", "BW", "higher_is_better", False),
        ("LLC Miss(%)", "LLC Miss", "lower_is_better", True),
    ]

    header = ""
    for col in summary_cols:
        header += f"{col[0]:<{col_w}} "
    header += "Source Folder"  # 最后一列不限制宽度

    output_lines.append(header)
    output_lines.append("-" * 140)

    for idx, res in enumerate(results):
        row_str = ""
        is_base = idx == 0
        for col in summary_cols:
            head, key, mtype, is_pct = col
            val = res.get(key, 0)
            base_val = base.get(key, 0)

            if mtype == "text":
                val_str = str(val)
                # 截断太长的名字
                if len(val_str) > col_w - 2:
                    val_str = val_str[: col_w - 3] + ".."
                row_str += f"{val_str:<{col_w}} "
            else:
                # [关键] 传入 show_diff 参数
                # 如果是 Parallel 模式，show_diff = False
                # 如果是普通模式，只有非 Base 行才显示 Diff
                cell = format_cell(
                    base_val,
                    val,
                    mtype,
                    is_pct,
                    is_baseline_row=is_base,
                    show_diff=(not args.parallel),
                )
                row_str += f"{cell:<{col_w}} "

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
        output_lines.append(
            f"[{idx}] Arch: {res['Arch']} | Workload: {res['Benchmark']}"
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
            # 简化了 per-core 表格的表头
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

    # ================= FILE OUTPUT =================
    if args.file:
        dir_names = [r["Folder"] for r in results]
        combined_name = "_vs_".join(dir_names)
        # 限制文件名长度，防止溢出
        combined_name = re.sub(r'[\\/*?:"<>|]', "", combined_name)[:150]

        # [Auto-Path Logic]
        script_dir = os.path.dirname(os.path.abspath(__file__))
        gem5_root = os.path.dirname(script_dir)
        result_dir = os.path.join(gem5_root, "tests", "result")

        if not os.path.exists(result_dir):
            try:
                os.makedirs(result_dir)
            except OSError:
                pass

        save_path = os.path.join(result_dir, f"Report_{combined_name}.txt")
        try:
            with open(save_path, "w") as f:
                f.write(final_output)
            print(f"\033[1;32m[SUCCESS] Report saved: {save_path}\033[0m")
        except Exception as e:
            print(f"Error saving file: {e}")
            print(final_output)
    else:
        print(final_output)


if __name__ == "__main__":
    main()
