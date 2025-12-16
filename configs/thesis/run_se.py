# created by Wang Yingsong on 2025-12-12
# configs/thesis/run_se.py
# =========================================================
# SE 模式仿真运行脚本 (run_se.py)
# =========================================================

import argparse
import os
import sys

# 导入架构定义
import Conventional_Baseline_Arch

# 导入环境定义
import env
import Standard_MultiCore_Arch

import m5
from m5.objects import *

# =========================================================
# 参数解析
# =========================================================
parser = argparse.ArgumentParser(description="SE Simulation Runner")

parser.add_argument(
    "--arch", type=str, default="standard", choices=env.VALID_ARCHS
)
parser.add_argument("--cmd", type=str, default="hello")
parser.add_argument("--num-cpus", type=int, default=4)
parser.add_argument("--sys-clock", type=str, default=env.SYS_CLOCK)
parser.add_argument(
    "--options", type=str, default="", help="Arguments passed to the binary"
)

args = parser.parse_args()

# =========================================================
# 系统构建 (统一接口)
# =========================================================
print(f"[SE BUILD] Architecture: {args.arch.upper()}")
print(f"[SE BUILD] Cores: {args.num_cpus}")
print(f"[SE BUILD] Clock: {args.sys_clock}")

system = None

if args.arch == "standard":
    system = Standard_MultiCore_Arch.build_system(args)
elif args.arch == "conventional":
    system = Conventional_Baseline_Arch.build_system(args)
else:
    print(f"Error: Unknown architecture {args.arch}")
    sys.exit(1)

# =========================================================
# 负载加载 (路径修复逻辑)
# =========================================================
binary = env.SE_BENCHMARKS.get(args.cmd, args.cmd)

if not os.path.exists(binary):
    fallback_path = os.path.join("tests/spme_benchmark/bin", args.cmd)
    if os.path.exists(fallback_path):
        binary = fallback_path
    else:
        print(f"Error: Binary not found at '{binary}' nor '{fallback_path}'")
        sys.exit(1)

print(f"[SE RUN] Workload Binary: {binary}")

# 3. 设置负载
env.setup_se_workload(system, binary)

# 4. 追加参数 (options) - [修复] 支持 StandardCore 包装类
if args.options:
    print(f"[SE RUN] Appending Options: {args.options}")

    cpus = []
    if hasattr(system, "cpu"):
        cpus = system.cpu
    elif hasattr(system, "core_list"):
        cpus = system.core_list

    if cpus and len(cpus) > 0:
        # 获取第一个核心对象
        target_obj = cpus[0]

        # [关键修复] 探测是否存在包装器 (Wrapper)
        # 如果 target_obj 本身没有 workload 属性，尝试查找其内部的 core 或 cpu 成员
        if not hasattr(target_obj, "workload"):
            if hasattr(target_obj, "core") and hasattr(
                target_obj.core, "workload"
            ):
                target_obj = target_obj.core  # Standard 架构通常在这里
            elif hasattr(target_obj, "cpu") and hasattr(
                target_obj.cpu, "workload"
            ):
                target_obj = target_obj.cpu

        # 现在尝试获取 workload
        if hasattr(target_obj, "workload"):
            workload_obj = target_obj.workload

            process = None
            if isinstance(workload_obj, list) or isinstance(
                workload_obj, tuple
            ):
                process = workload_obj[0]
            else:
                process = workload_obj

            if process:
                original_binary = process.cmd[0]
                process.cmd = [original_binary] + args.options.split()
                print(f"[SE RUN] Final Command: {' '.join(process.cmd)}")
        else:
            print(
                "[WARN] Could not find 'workload' attribute in CPU object. Options might not be set."
            )

# =========================================================
# 运行仿真
# =========================================================
root = Root(full_system=False, system=system)

print("info: Instantiating C++ objects...")
m5.instantiate()

print("--- Simulation Start (SE Mode) ---")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
