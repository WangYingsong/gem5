# created by Wang Yingsong on 2025-12-15
# configs/thesis/Conventional_Baseline_Arch.py
# =========================================================
# 'Scale-Out Processors' 论文中的 Conventional Baseline 架构配置脚本
# 特征:
# 1. 激进的乱序核心 (4-wide, 128 ROB)
# 2. 巨大的共享 LLC (2MB per core)
# 3. Crossbar 互联结构
# =========================================================

import env
from components import (
    ConventionalL1DCache,
    ConventionalL1ICache,
    ConventionalLLC,
    ConventionalO3CPU,
)

import m5
from m5.objects import *


def build_system(options):
    """
    构建符合 'Scale-Out Processors' 论文定义的 Conventional Baseline 系统。
    特征:
    1. 激进的乱序核心 (4-wide, 128 ROB)
    2. 巨大的共享 LLC (2MB per core)
    3. Crossbar 互联结构
    """

    # 1. 基础系统初始化
    system = System()

    # 不再硬编码 '2GHz'，而是使用 options 中的参数
    # options.sys_clock 默认值在 run_se.py 中来自于 env.SYS_CLOCK (即 2GHz)
    # 这样既保证了默认符合论文，又允许你用 --sys-clock="3GHz" 临时超频测试
    system.clk_domain = SrcClockDomain()
    system.clk_domain.clock = options.sys_clock
    system.clk_domain.voltage_domain = VoltageDomain()

    system.mem_mode = "timing"

    # 使用 env.MEM_SIZE 统一管理内存大小
    system.mem_ranges = [AddrRange(env.MEM_SIZE)]

    # 2. 创建 CPU (Conventional O3)
    system.cpu = [ConventionalO3CPU() for i in range(options.num_cpus)]

    # 初始化 x86 中断控制器
    for cpu in system.cpu:
        cpu.createInterruptController()

    # 3. 创建互联结构 (System Crossbar)
    # 论文 Table 3: "Crossbar: 1-8 cores: 4 cycles"
    system.membus = SystemXBar()
    system.membus.frontend_latency = 4
    system.membus.forward_latency = 4
    system.membus.response_latency = 4
    system.membus.snoop_response_latency = 4
    system.membus.width = 64

    # 4. 创建并连接 L1 Cache
    for cpu in system.cpu:
        cpu.icache = ConventionalL1ICache()
        cpu.dcache = ConventionalL1DCache()

        # 连接 CPU <-> L1
        cpu.icache.cpu_side = cpu.icache_port
        cpu.dcache.cpu_side = cpu.dcache_port

        # 连接 L1 <-> System Crossbar
        cpu.icache.mem_side = system.membus.cpu_side_ports
        cpu.dcache.mem_side = system.membus.cpu_side_ports

        # 连接 TLB Walker
        cpu.mmu.itb.walker.port = system.membus.cpu_side_ports
        cpu.mmu.dtb.walker.port = system.membus.cpu_side_ports

        # 连接中断端口
        cpu.interrupts[0].pio = system.membus.mem_side_ports
        cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
        cpu.interrupts[0].int_responder = system.membus.mem_side_ports

    # 5. 创建并连接 LLC
    llc_capacity = 2 * options.num_cpus
    system.llc = ConventionalLLC(size=f"{llc_capacity}MiB")
    # 告诉 LLC 只响应系统内存范围 (0~4GB)，不要覆盖 IO/中断范围
    system.llc.addr_ranges = system.mem_ranges

    # LLC 的 CPU 侧连接到 System Crossbar 的内存侧
    system.llc.cpu_side = system.membus.mem_side_ports

    # 6. 创建内存总线与控制器
    system.membus_dram = SystemXBar()
    system.llc.mem_side = system.membus_dram.cpu_side_ports

    system.mem_ctrl = MemCtrl()

    # 使用 env.MEM_TYPE 统一管理内存类型 (如 DDR3_1600_8x8)
    system.mem_ctrl.dram = env.MEM_TYPE()
    system.mem_ctrl.dram.range = system.mem_ranges[0]

    system.mem_ctrl.port = system.membus_dram.mem_side_ports
    system.system_port = system.membus.cpu_side_ports

    return system
