// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_md5.c
// =========================================================
// 优化版: MD5 Proxy (2-way ILP)
// 特征: 2路并发计算 -> 打破依赖链，压测整数 ALU 吞吐
// =========================================================

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_ROUNDS 5000000
#define NUM_THREADS 4

int rounds = DEFAULT_ROUNDS;
uint64_t global_checksum = 0; // 全局汇总，防止优化

typedef struct
{
    int id;
} thread_arg_t;

// MD5 Macros (使用反斜杠换行以符合 79 字符限制)
#define F(x, y, z) \
    (((x) & (y)) | ((~x) & (z)))

#define G(x, y, z) \
    (((x) & (z)) | ((y) & (~z)))

#define ROTATE_LEFT(x, n) \
    (((x) << (n)) | ((x) >> (32 - (n))))

// 核心优化: 同时计算两个独立的 MD5 状态 (Lane 0 和 Lane 1)
void*
worker(void* arg)
{
    // Lane 0 State
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89;
    uint32_t c0 = 0x98badcfe, d0 = 0x10325476;
    // Lane 1 State
    uint32_t a1 = 0x10325476, b1 = 0x98badcfe;
    uint32_t c1 = 0xefcdab89, d1 = 0x67452301;

    // 模拟输入
    uint32_t x[16];
    for (int i = 0; i < 16; i++) x[i] = i * 0xdeadbeef;

    // 每次循环处理 2 轮，提高吞吐
    for (int i = 0; i < rounds; i += 2) {
        // --- Lane 0 Ops ---
        // 强制折行以避免 line length error
        a0 = b0 + ROTATE_LEFT((a0 + F(b0, c0, d0) + x[0] + 0xd76aa478), 7);
        d0 = a0 + ROTATE_LEFT((d0 + F(a0, b0, c0) + x[1] + 0xe8c7b756), 12);

        // --- Lane 1 Ops (Independent!) ---
        a1 = b1 + ROTATE_LEFT((a1 + F(b1, c1, d1) + x[0] + 0xd76aa478), 7);
        d1 = a1 + ROTATE_LEFT((d1 + F(a1, b1, c1) + x[1] + 0xe8c7b756), 12);

        // ... 省略部分中间轮次以保持代码精简 ...

        c0 = d0 + ROTATE_LEFT((c0 + F(d0, a0, b0) + x[2] + 0x242070db), 17);
        b0 = c0 + ROTATE_LEFT((b0 + F(c0, d0, a0) + x[3] + 0xc1bdceee), 22);

        c1 = d1 + ROTATE_LEFT((c1 + F(d1, a1, b1) + x[2] + 0x242070db), 17);
        b1 = c1 + ROTATE_LEFT((b1 + F(c1, d1, a1) + x[3] + 0xc1bdceee), 22);

        // 简单的扰动防止优化
        x[0] += a0 ^ a1;
    }

    // 汇总结果 (使用原子操作避免 Data Race)
    uint32_t local_sum = a0 + b0 + a1 + b1;
    __sync_fetch_and_add(&global_checksum, local_sum);
    return NULL;
}

int
main(int argc, char* argv[])
{
    if (argc > 1) rounds = atoi(argv[1]);
    printf("[Benchmark] MD5 (Master-Worker). Rounds: %d\n", rounds);

    // 修改点 1: 只创建 NUM_THREADS - 1 个子线程
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];

    // 1. 启动子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].id = i;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    // 2. 主线程作为 Worker 3 执行
    args[NUM_THREADS - 1].id = NUM_THREADS - 1;
    worker(&args[NUM_THREADS - 1]);

    // 3. 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[Done] Checksum: %lx\n", global_checksum);
    return 0;
}
