import os

import m5
from m5.objects import *
from m5.objects import Cache


class L1Cache(Cache):
    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20

    def __init__(self, options=None):
        super().__init__()

    def connectBus(self, bus):
        self.mem_side = bus.cpu_side_ports

    def connectCPU(self, cpu):
        raise NotImplementedError


class L1ICache(L1Cache):
    size = "16KiB"

    def __init__(self, opts=None):
        super().__init__(opts)

    def connectCPU(self, cpu):
        self.cpu_side = cpu.icache_port


class L1DCache(L1Cache):
    size = "64KiB"

    def __init__(self, opts=None):
        super().__init__(opts)

    def connectCPU(self, cpu):
        self.cpu_side = cpu.dcache_port


class L2Cache(Cache):
    size = "256KiB"
    assoc = 8
    tag_latency = 10
    data_latency = 10
    response_latency = 10
    mshrs = 20
    tgts_per_mshr = 12

    def __init__(self, options=None):
        super().__init__()

    def connectCPUSideBus(self, bus):
        self.cpu_side = bus.mem_side_ports

    def connectMemSideBus(self, bus):
        self.mem_side = bus.cpu_side_ports


class L3Cache(Cache):
    size = "8MiB"
    assoc = 16
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 32
    tgts_per_mshr = 16

    def __init__(self, options=None):
        super().__init__()

    def connectCPUSideBus(self, bus):
        self.cpu_side = bus.mem_side_ports

    def connectMemSideBus(self, bus):
        self.mem_side = bus.cpu_side_ports


system = System()
system.clk_domain = SrcClockDomain(
    clock="1GHz", voltage_domain=VoltageDomain()
)
system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MiB")]
system.membus = SystemXBar()
system.mem_ctrl = MemCtrl(
    dram=DDR3_1600_8x8(range=system.mem_ranges[0]),
    port=system.membus.mem_side_ports,
)
system.system_port = system.membus.cpu_side_ports


num_cpus = 4

binary = os.path.join("tests/test-progs/hello/bin/x86/linux/hello")
processes = []
for i in range(num_cpus):
    process = Process(pid=100 + i)
    process.cmd = [binary]
    processes.append(process)

system.workload = SEWorkload.init_compatible(binary)

system.cpu = [X86O3CPU(cpu_id=i) for i in range(num_cpus)]
system.l3bus = L2XBar()
system.l3cache = L3Cache()
system.l3cache.connectCPUSideBus(system.l3bus)
system.l3cache.connectMemSideBus(system.membus)

for i in range(num_cpus):
    cpu = system.cpu[i]
    cpu.icache = L1ICache()
    cpu.dcache = L1DCache()
    cpu.icache.connectCPU(cpu)
    cpu.dcache.connectCPU(cpu)
    cpu.l2bus = L2XBar()
    cpu.icache.connectBus(cpu.l2bus)
    cpu.dcache.connectBus(cpu.l2bus)
    cpu.l2cache = L2Cache()
    cpu.l2cache.connectCPUSideBus(cpu.l2bus)
    cpu.l2cache.connectMemSideBus(system.l3bus)

    cpu.workload = processes[i]

    cpu.createThreads()
    cpu.createInterruptController()
    cpu.interrupts[0].pio = system.membus.mem_side_ports
    cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
    cpu.interrupts[0].int_responder = system.membus.mem_side_ports

root = Root(full_system=False, system=system)
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
