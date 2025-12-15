// created by Wang Yingsong on 2025-12-12
// tests/spme_benchmark/spme_micro.c
// =============================================================
// SPME 微基准测试
// 说明：测试 SPME 替换算法对热点数据与流式数据
// 的不同处理效果
// =============================================================

#include <stdio.h>
#include <stdlib.h>

// 热点数据大小：4KB (远小于 32KB L1，应该被替换算法保护)
#define HOT_SIZE (4 * 1024)
// 流式数据大小：64KB (大于 32KB L1，如果不旁路会污染 Cache)
#define STREAM_SIZE (64 * 1024)

int hot_array[HOT_SIZE];
int stream_array[STREAM_SIZE];

int main() {
    printf("[SPME MICRO] Initializing Arrays...\n");

    for (int i = 0; i < HOT_SIZE; i++) hot_array[i] = i;
    for (int i = 0; i < STREAM_SIZE; i++) stream_array[i] = i;

    printf("[SPME MICRO] Starting Mixed Workload...\n");

    long long sum = 0;

    // 模拟混合负载
    // 逻辑：每线性访问 1 次流式数据，就反复访问 10 次热点数据
    for (int i = 0; i < STREAM_SIZE; i++) {

        // --- 模式 A: 流式访问 ---
        // 预期行为：Cache.cc 逻辑应检测到并在 Debug Log 中显示 "Bypassing"
        sum += stream_array[i];

        // --- 模式 B: 热点访问 ---
        // 预期行为：SPMERP 替换算法应给予高优先级，Log 中显示 "Touching Hot Block"
        for (int k = 0; k < 10; k++) {
            int idx = (i + k) % HOT_SIZE;
            sum += hot_array[idx];
        }
    }

    printf("[SPME MICRO] Workload Finished. CheckSum: %lld\n", sum);
    printf("[INFO] Check your stats.txt or debug.log for 'Bypass' events.\n");

    return 0;
}
