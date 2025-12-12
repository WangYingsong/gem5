# created by Wang Yingsong on 2025-12-12
# configs/thesis/env.py

# ========================================================
# GEM5 环境配置脚本 (env.py)
# 负责路径管理、全局参数、基础系统创建及负载加载
# ========================================================

import os
import sys

import m5
from m5.objects import *

# =========================================================
# 1. 路径资源管理
# =========================================================
_CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
GEM5_ROOT = os.path.abspath(os.path.join(_CURRENT_DIR, "../../"))

# FS 资源 (内核与磁盘)
KERNEL_BIN = os.path.join(GEM5_ROOT, "dist/m5/system/binaries/vmlinux")
DISK_IMG = os.path.join(GEM5_ROOT, "dist/m5/system/disks/x86-ubuntu.img")
CHECKPOINT_DIR = os.path.join(GEM5_ROOT, "m5out_base")

# SE 资源 (微基准测试二进制目录)
TESTS_ROOT = os.path.join(GEM5_ROOT, "tests/spme_benchmark/bin")

# 常用测试程序映射
SE_BENCHMARKS = {
    "hello": os.path.join(TESTS_ROOT, "hello"),
    "matmul": os.path.join(TESTS_ROOT, "matmul"),
    "test_sharing": os.path.join(TESTS_ROOT, "test_sharing"),
    "spme_micro": os.path.join(TESTS_ROOT, "spme_micro"),
}

# =========================================================
# 2. 全局物理参数
# =========================================================
SYS_CLOCK = "3GHz"
MEM_SIZE = "4GB"
MEM_TYPE = DDR4_2400_8x8


# =========================================================
# 3. 基础系统工厂
# =========================================================
def create_base_system(full_system=True):
    """创建包含时钟、内存、总线的基础系统"""
    system = System()
    system.clk_domain = SrcClockDomain(
        clock=SYS_CLOCK, voltage_domain=VoltageDomain()
    )
    system.mem_mode = "timing"
    system.mem_ranges = [AddrRange(MEM_SIZE)]
    system.membus = SystemXBar()

    system.mem_ctrl = MemCtrl()
    system.mem_ctrl.dram = MEM_TYPE()
    system.mem_ctrl.dram.range = system.mem_ranges[0]
    system.mem_ctrl.port = system.membus.mem_side_ports
    system.system_port = system.membus.cpu_side_ports

    if full_system:
        system.kernel = KERNEL_BIN
        system.workload = KernelWorkload(object_file=KERNEL_BIN)
        system.pc = Pc()
        system.pc.south_bridge = SouthBridge()
        system.pc.south_bridge.attachIO(system.membus)
        system.pc.south_bridge.ide.disks = []

    return system


# =========================================================
# 4. 负载加载助手
# =========================================================
def setup_fs_workload(system):
    if not isinstance(system.pc.south_bridge.ide.disks, list):
        system.pc.south_bridge.ide.disks = []
    system.pc.south_bridge.ide.disks.append(
        IdeDisk(driveID="master", image=DiskImage(image_file=DISK_IMG))
    )


def setup_se_workload(system, binary_path, cmd_args=""):
    import shlex

    process = Process()
    process.cmd = [binary_path] + shlex.split(cmd_args)
    process.cwd = os.getcwd()

    # -------------------------------------------------------------
    # [关键修正] 初始化系统级 SE Workload
    # 这一步是必须的，告诉 System 如何处理 X86 的系统调用
    # -------------------------------------------------------------
    system.workload = SEWorkload.init_compatible(binary_path)

    # 查找并分配 CPU
    cpu_list = []
    if hasattr(system, "core_list"):
        cpu_list = [core.cpu for core in system.core_list]
    elif hasattr(system, "cluster_list"):
        for cluster in system.cluster_list:
            if hasattr(cluster, "cpus"):
                cpu_list.extend(cluster.cpus)
            elif hasattr(cluster, "_cpus"):
                cpu_list.extend(cluster._cpus)
    elif hasattr(system, "cpu"):
        cpu_list = system.cpu

    if not cpu_list:
        fatal("Env Helper: Could not find any CPUs to assign workload!")

    print(f"Info: Assigning workload to {len(cpu_list)} CPUs")
    for cpu in cpu_list:
        cpu.workload = process
        cpu.createThreads()
