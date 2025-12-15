# created by Wang Yingsong on 2025-12-12
# configs/thesis/run_fs.py
# =========================================================
# FS 模式仿真运行脚本 (run_fs.py)
# =========================================================

import argparse
import os
import sys

import env
import Standard_MultiCore_Arch

import m5
from m5.objects import *

# =========================================================
# 参数解析
# =========================================================
parser = argparse.ArgumentParser(description="FS Simulation Runner")
parser.add_argument("--arch", type=str, required=True, choices=["standard"])
parser.add_argument("--restore", action="store_true")
# FS 模式通常固定核心数，或者也可以通过参数传入
parser.add_argument("--num-cpus", type=int, default=4)

args = parser.parse_args()

# =========================================================
# 系统构建
# =========================================================
system = None

if args.arch == "standard":
    # 之前是 env.create_base_system + build
    # 现在直接调用 build_system
    system = Standard_MultiCore_Arch.build_system(args)

    # 注意：FS 模式需要内核和磁盘配置，这些之前在 env.create_base_system 里
    # 现在需要在 system 创建后手动挂载，或者让 Arch 脚本感知 full_system 选项
    # 这里我们采用简单的"后处理"方式补全 FS 需求:

    system.kernel = env.KERNEL_BIN
    system.workload = KernelWorkload(object_file=env.KERNEL_BIN)

    # 添加 PC 平台组件 (Standard Arch 默认没有创建 PC 组件)
    # 这部分逻辑其实最好封装在 env 里，或者让 Arch 脚本支持 FS 模式
    # 简单起见，我们在这里补充南桥和 PC 结构
    system.pc = Pc()
    system.pc.south_bridge = SouthBridge()
    # 连接 IO 总线 (Standard Arch 里是 membus)
    system.pc.south_bridge.attachIO(system.membus)
    system.pc.south_bridge.ide.disks = []

# =========================================================
# 加载 FS 负载
# =========================================================
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
