# created by Wang Yingsong on 2025-12-12
# configs/thesis/components.py

# =========================================================
# 组件定义脚本 (components.py)
# 定义多核架构中常用的缓存和核心组件
# =========================================================

from m5.objects import *


class L1Cache(Cache):
    """L1 Cache 基类: 低延迟，高吞吐"""

    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20


class L1ICache(L1Cache):
    """L1 指令缓存"""

    is_read_only = True
    # writeback_clean = True # 视 GEM5 版本而定，通常不需要


class L1DCache(L1Cache):
    """L1 数据缓存"""

    pass


class L2Cache(Cache):
    """L2 Cache: 容量大，延迟稍高"""

    size = "2MB"
    assoc = 8
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12
    write_buffers = 8


class L1XBar(CoherentXBar):
    """
    L1 Crossbar:
    用于 CPU 内部汇聚 I-Port/D-Port 和 MMU Walker。
    特点：
    1. 极低延迟 (1 cycle)
    2. 无 Snoop Filter (因为是点对点私有连接，不需要过滤)
    3. 不是一致性或统一对其点 (只是个简单的复用器)
    """

    width = 32
    frontend_latency = 1
    forward_latency = 0
    response_latency = 1
    snoop_response_latency = 1

    # [关键优化] 显式禁用 Snoop Filter，减轻仿真负担
    snoop_filter = NULL

    # 显式声明这不是系统级的一致性点
    point_of_coherency = False
    point_of_unification = False


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
        # 引入 L1 总线来汇聚 CPU 和 MMU 的请求
        # ---------------------------------------------------------
        # 创建两个微型总线，分别用于指令侧和数据侧
        self.l1i_bus = L1XBar()
        self.l1d_bus = L1XBar()
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
# 激进多核架构组件定义 (Conventional Aggressive Components)
# =========================================================
class ConventionalO3CPU(X86O3CPU):
    #  Conventional: 4-wide dispatch/retirement
    fetchWidth = 4
    decodeWidth = 4
    renameWidth = 4
    issueWidth = 4
    dispatchWidth = 4
    commitWidth = 4

    #  128-entry ROB
    numROBEntries = 128

    #  32-entry LSQ (GEM5 分为 LQ 和 SQ，通常拆分为 16/16 或 32/32)
    # 为了保证激进性能，建议设为 Load=32, Store=32 或总和匹配
    LQEntries = 32
    SQEntries = 32

    # 物理寄存器文件 (需比 ROB 大以支持重命名)
    numPhysIntRegs = 256
    numPhysFloatRegs = 256

    # 分支预测 (论文未详述，但"激进核心"通常配备竞赛型预测器)
    branchPred = TournamentBP()


class ConventionalL1ICache(Cache):
    size = "64KiB"  #
    assoc = 4  #
    tag_latency = 3  #
    data_latency = 3
    response_latency = 1  # 这里的 1+2=3 cycles total
    mshrs = 32  #
    tgts_per_mshr = 20


class ConventionalL1DCache(Cache):
    size = "64KiB"  #
    assoc = 8  #
    tag_latency = 3
    data_latency = 3
    response_latency = 1
    mshrs = 32  #
    tgts_per_mshr = 20
    write_buffers = 32


class ConventionalLLC(Cache):
    #  Conventional: 2MB of LLC per core.
    # 如果你的实验是 4 核，请设为 '8MB'
    size = "8MiB"
    assoc = 16  #
    tag_latency = 15  # 大容量缓存延迟较高，通常 10-20 cycles
    data_latency = 15
    response_latency = 5
    mshrs = 64  #
    tgts_per_mshr = 20

    # 论文提到 Crossbar 互联
    # 在 GEM5 中，确保这个 Cache 连在 SystemXBar 上
