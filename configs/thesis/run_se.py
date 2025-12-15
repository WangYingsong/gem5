# created by Wang Yingsong on 2025-12-12
# configs/thesis/run_se.py
# =========================================================
# SE 模式仿真运行脚本 (run_se.py)
# =========================================================

import argparse
import os
import sys

import Conventional_Baseline_Arch

# 导入环境与架构定义
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

args = parser.parse_args()

# =========================================================
# 系统构建 (统一接口)
# =========================================================
print(f"[SE BUILD] Architecture: {args.arch.upper()}")
print(f"[SE BUILD] Cores: {args.num_cpus}")
print(f"[SE BUILD] Clock: {args.sys_clock}")

system = None

# 统一使用 build_system(args) 接口
if args.arch == "standard":
    system = Standard_MultiCore_Arch.build_system(args)
elif args.arch == "conventional":
    system = Conventional_Baseline_Arch.build_system(args)
else:
    print(f"Error: Unknown architecture {args.arch}")
    sys.exit(1)

# =========================================================
# 负载加载
# =========================================================
binary = env.SE_BENCHMARKS.get(args.cmd, args.cmd)
print(f"[SE RUN] Workload Binary: {binary}")

if not os.path.exists(binary):
    print(f"Error: Binary not found at {binary}")
    sys.exit(1)

env.setup_se_workload(system, binary)

# =========================================================
# 运行仿真
# =========================================================
root = Root(full_system=False, system=system)

print("info: Instantiating C++ objects...")
m5.instantiate()

print("--- Simulation Start (SE Mode) ---")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
