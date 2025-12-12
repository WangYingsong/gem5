# created by Wang Yingsong on 2025-12-12
# configs/thesis/components.py

# =========================================================
# 缓存组件定义脚本 (components.py)
# 定义各种缓存组件及其参数
# =========================================================

from m5.objects import Cache

# =========================================================
# 缓存参数定义 (Cache Specifications)
# 所有架构共用这一套参数，保证对照实验的公平性
# =========================================================


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
