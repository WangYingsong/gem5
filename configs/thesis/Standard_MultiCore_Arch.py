# created by Wang Yingsong on 2025-12-12
# configs/thesis/Standard_MultiCore_Arch.py

# =========================================================
# 标准多核架构配置脚本 (Standard_MultiCore_Arch.py)
# 定义一个包含 N 个标准核心的多核系统
# 拓扑结构: 扁平结构 (Flat), 私有 L1, 共享 L2
# =========================================================

# 从 components 中导入缓存定义
from components import (
    StandardCore,
    L2Cache,
)

from m5.objects import *

# =========================================================
# 标准多核架构构建函数
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
