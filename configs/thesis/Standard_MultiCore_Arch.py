# created by Wang Yingsong on 2025-12-12
# configs/thesis/Standard_MultiCore_Arch.py

# =========================================================
# 标准多核架构配置脚本 (Standard_MultiCore_Arch.py)
# 定义一个包含 N 个标准核心的多核系统
# 拓扑结构: 扁平结构 (Flat), 私有 L1, 共享 L2
# =========================================================

# 从 components 中导入缓存定义
from components import (
    L1DCache,
    L1ICache,
    L2Cache,
)

# configs/thesis/Standard_MultiCore_Arch.py
from m5.objects import *

# =========================================================
# 1. 架构专属组件定义 (Architecture Specific Components)
# =========================================================


class StandardCore(SubSystem):
    """
    StandardCore: 普通多核架构的基本单元
    定义：一个 CPU 挂载私有的 L1 I/D Cache 和 MMU
    """

    def __init__(self, cpu_type, l1i_size, l1d_size):
        super().__init__()

        # 1. 实例化 CPU
        self.cpu = cpu_type()

        # 2. 实例化私有 L1 (使用 components 中的类)
        self.l1i = L1ICache(size=l1i_size)
        self.l1d = L1DCache(size=l1d_size)

        # 3. 实例化 MMU
        self.mmu = X86MMU()

        # ---------------------------------------------------------
        # [修正点] 引入 L1 总线来汇聚 CPU 和 MMU 的请求
        # ---------------------------------------------------------
        # 创建两个微型总线，分别用于指令侧和数据侧
        # L2XBar 是一个通用的连贯性交叉开关，适合在这里做汇聚
        self.l1i_bus = L2XBar()
        self.l1d_bus = L2XBar()

        # ---------------------------------------------------------
        # 4. 内部连接 (Internal Wiring)
        # ---------------------------------------------------------

        # --- 指令侧连接 (Instruction Side) ---
        # 汇聚：CPU I-Port -> Bus
        self.cpu.icache_port = self.l1i_bus.cpu_side_ports
        # 汇聚：MMU ITB Walker -> Bus
        self.cpu.mmu.itb.walker.port = self.l1i_bus.cpu_side_ports
        # 输出：Bus -> L1 I-Cache
        self.l1i_bus.mem_side_ports = self.l1i.cpu_side

        # --- 数据侧连接 (Data Side) ---
        # 汇聚：CPU D-Port -> Bus
        self.cpu.dcache_port = self.l1d_bus.cpu_side_ports
        # 汇聚：MMU DTB Walker -> Bus
        self.cpu.mmu.dtb.walker.port = self.l1d_bus.cpu_side_ports
        # 输出：Bus -> L1 D-Cache
        self.l1d_bus.mem_side_ports = self.l1d.cpu_side

        # 挂载 MMU 到 CPU
        self.cpu.mmu = self.mmu

    def connect_to_l2bus(self, l2bus, interrupt_bus):
        """将私有 L1 挂载到外部 L2 总线"""
        # L1 Mem Side -> L2 Bus
        self.l1i.mem_side = l2bus.cpu_side_ports
        self.l1d.mem_side = l2bus.cpu_side_ports

        # 中断控制器连接 (X86 APIC)
        self.cpu.createInterruptController()
        self.cpu.interrupts[0].pio = interrupt_bus.mem_side_ports
        self.cpu.interrupts[0].int_requestor = interrupt_bus.cpu_side_ports
        self.cpu.interrupts[0].int_responder = interrupt_bus.mem_side_ports


# =========================================================
# 2. 架构构建函数 (Build Function)
# =========================================================


def build(system, num_cpus, l1_size, l2_size):
    """
    构建标准多核架构 (Standard Multi-Core Architecture)
    拓扑特征: 扁平结构 (Flat), N 个核心, 私有 L1, 共享 L2
    """
    print(
        f"[ARCH] Building: Standard Multi-Core Architecture ({num_cpus} Cores)"
    )

    # --- 创建共享 L2 子系统 ---
    system.l2bus = L2XBar()
    # 使用 components 中定义的 L2Cache 类
    system.l2cache = L2Cache(size=l2_size)

    # L2 连接: L2Bus <-> L2 <-> MemBus
    system.l2cache.cpu_side = system.l2bus.mem_side_ports
    system.l2cache.mem_side = system.membus.cpu_side_ports

    # --- 实例化 N 个标准核心 ---
    # 这里使用的是本文件上方定义的 StandardCore
    system.core_list = [
        StandardCore(X86O3CPU, l1_size, l1_size) for _ in range(num_cpus)
    ]

    # --- 连接核心 ---
    for core in system.core_list:
        # 将每个核心的私有 L1 连到共享 L2 总线
        core.connect_to_l2bus(system.l2bus, system.membus)

    # --- 注册 CPU ---
    # system.cpu = [core.cpu for core in system.core_list]
