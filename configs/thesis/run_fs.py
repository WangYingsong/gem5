# created by Wang Yingsong on 2025-12-12
# configs/thesis/run_fs.py

# =========================================================
# FS 模式仿真运行脚本 (run_fs.py)
# =========================================================

import argparse
import os
import sys

# 导入环境与架构定义
import env
import Standard_MultiCore_Arch

import m5
from m5.objects import *

# =========================================================
# 参数解析 (FS 专属)
# =========================================================
parser = argparse.ArgumentParser(description="FS Simulation Runner")

# 1. 架构选择
parser.add_argument(
    "--arch",
    type=str,
    required=True,
    choices=["standard_multicore"],
    help="Select Architecture Design",
)

# 2. Checkpoint 选项
parser.add_argument(
    "--restore",
    action="store_true",
    help="Restore from standard checkpoint dir",
)

args = parser.parse_args()

# =========================================================
# 系统构建
# =========================================================
# 1. 创建基础 FS 环境 (带内核、磁盘)
system = env.create_base_system(full_system=True)

# 2. 注入架构
if args.arch == "standard_multicore":
    Standard_MultiCore_Arch.build(
        system,
        num_cpus=4,  # FS 模式下严格固定为 4 核，保证对照公平
        l1_size="32kB",
        l2_size="2MB",
    )

# 3. 加载 FS 负载 (挂载磁盘)
print(f"[FS RUN] Arch: {args.arch} | OS: Ubuntu")
env.setup_fs_workload(system)

# =========================================================
# 运行仿真
# =========================================================
root = Root(full_system=True, system=system)

if args.restore:
    if os.path.exists(env.CHECKPOINT_DIR):
        print(f"[FS RUN] Restoring Checkpoint: {env.CHECKPOINT_DIR}")
        m5.instantiate(checkpoint_dir=env.CHECKPOINT_DIR)
    else:
        print("[WARN] Checkpoint not found, starting fresh boot...")
        m5.instantiate()
else:
    print("[FS RUN] Starting fresh instantiation...")
    m5.instantiate()

print("--- Simulation Start (FS Mode) ---")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
