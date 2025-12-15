# created by Wang Yingsong on 2025-12-12
# configs/thesis/Standard_MultiCore_Arch.py
# =========================================================
# 标准多核架构配置脚本 (Standard Multi-Core Architecture)
# 特征:
# 1. 标准乱序核心 (3-wide, 64 ROB)
# 2. 二级缓存 (2MB, 8-way)
# =========================================================

import env
from components import (
    L1XBar,
    L2Cache,
    StandardCore,
)

import m5
from m5.objects import *


def build_system(options):
    """
    构建标准多核架构 (Standard Multi-Core Architecture)
    特征:
    1. 标准乱序核心 (3-wide, 64 ROB)
    2. 二级缓存 (2MB, 8-way)
    """
    print(
        f"[ARCH] Building: Standard Multi-Core Architecture ({options.num_cpus} Cores)"
    )

    system = System()

    clk = getattr(options, "sys_clock", env.SYS_CLOCK)
    system.clk_domain = SrcClockDomain()
    system.clk_domain.clock = clk
    system.clk_domain.voltage_domain = VoltageDomain()

    system.mem_mode = "timing"
    system.mem_ranges = [AddrRange(env.MEM_SIZE)]

    system.membus = SystemXBar()

    system.mem_ctrl = MemCtrl()
    system.mem_ctrl.dram = env.MEM_TYPE()
    system.mem_ctrl.dram.range = system.mem_ranges[0]
    system.mem_ctrl.port = system.membus.mem_side_ports

    system.system_port = system.membus.cpu_side_ports

    # --- L2 ---
    system.l2bus = L2XBar()
    system.l2cache = L2Cache(size="2MiB")
    system.l2cache.cpu_side = system.l2bus.mem_side_ports
    system.l2cache.mem_side = system.membus.cpu_side_ports

    # --- Cores ---
    system.core_list = [
        StandardCore(X86O3CPU, l1i_size="32KiB", l1d_size="32KiB")
        for _ in range(options.num_cpus)
    ]

    for core in system.core_list:
        core.connect_to_l2bus(system.l2bus, interrupt_bus=system.membus)

    return system
