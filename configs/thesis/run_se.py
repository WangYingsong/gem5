# created by Wang Yingsong on 2025-12-12
# configs/thesis/run_se.py

# =========================================================
# SE 模式仿真运行脚本 (run_se.py)
# =========================================================

import argparse
import os
import sys

# 导入环境与架构定义
import env
import Standard_MultiCore_Arch

import m5
from m5.objects import *

# 未来导入: import SPME_Cluster_Arch

# =========================================================
# 参数解析 (SE 专属)
# =========================================================
parser = argparse.ArgumentParser(description="SE Simulation Runner")

# 1. 架构选择
parser.add_argument(
    "--arch",
    type=str,
    required=True,
    choices=["standard_multicore", "spme"],  # 后续可添加 spme
    help="Select Architecture Design",
)

# 2. 测试样例选择
parser.add_argument(
    "--cmd",
    type=str,
    default="hello",
    help="Benchmark name (key in env.SE_BENCHMARKS) or binary path",
)

# 3. 可选参数 (用于灵活调试，默认值为 Baseline 1 配置)
parser.add_argument("--num-cpus", type=int, default=4, help="Number of CPUs")

args = parser.parse_args()

# =========================================================
# 系统构建
# =========================================================
# 1. 创建基础 SE 环境
system = env.create_base_system(full_system=False)

# 2. 注入架构
if args.arch == "standard_multicore":
    Standard_MultiCore_Arch.build(
        system, num_cpus=args.num_cpus, l1_size="32kB", l2_size="2MB"
    )
elif args.arch == "spme":
    # 预留给未来的 SPME 架构
    print("SPME Architecture not yet implemented in run_se.py")
    sys.exit(1)

# 3. 加载 SE 负载
# 从 env.py 的映射表中查找路径，找不到则认为是绝对路径
binary = env.SE_BENCHMARKS.get(args.cmd, args.cmd)
print(f"[SE RUN] Arch: {args.arch} | Binary: {binary}")

env.setup_se_workload(system, binary)

# =========================================================
# 运行仿真
# =========================================================
root = Root(full_system=False, system=system)
m5.instantiate()

print("--- Simulation Start (SE Mode) ---")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
