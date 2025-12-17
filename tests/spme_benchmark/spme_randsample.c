// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_randsample.c
// =========================================================
// Proxy: BigDataBench RandSample
// 特征: 随机数生成 (RNG), 条件写入 (Sparse Write)
// =========================================================

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_SIZE 10000000 // 10M integers
#define NUM_THREADS 4
#define SAMPLE_RATE 10 // 10% 采样率
int N = DEFAULT_SIZE;
int *input_data;
int *global_counts;
typedef struct
{
    int id;
    int start;
    int end;
    int *local_buf;
} thread_arg_t;
// 快速伪随机数生成器 (Xorshift)
uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}
void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    int count = 0;
    uint32_t rng_state = t->id * 1234567 + 1; // 独立种子
    // 核心循环: Load -> RNG -> Branch -> Store
    for (int i = t->start; i < t->end; i++) {
        int val = input_data[i];
        // 只有当随机数满足条件时才写入
        if ((xorshift32(&rng_state) % 100) < SAMPLE_RATE) {
            t->local_buf[count++] = val;
        }
    }
    global_counts[t->id] = count;
    return NULL;
}
int main(int argc, char* argv[]) {
    if (argc > 1) N = atoi(argv[1]);
    printf("[Benchmark] Random Sampling (Master-Worker). "
           "Size: %d, Rate: %d%%\n", N, SAMPLE_RATE);
    input_data = (int*)malloc(N * sizeof(int));
    global_counts = (int*)malloc(NUM_THREADS * sizeof(int));
    // 初始化数据
    for (int i = 0; i < N; i++) input_data[i] = i;
    // 修改点 1: 线程句柄减少 1
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    int chunk = N / NUM_THREADS;
    // 修改点 2: 启动子线程 (Worker 0, 1, 2)
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].id = i;
        args[i].start = i * chunk;
        args[i].end = (i + 1) * chunk;
        args[i].local_buf = (int*)malloc(chunk * sizeof(int)); // 私有Buff
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    // 修改点 3: 主线程执行最后一份任务 (Worker 3)
    int last = NUM_THREADS - 1;
    args[last].id = last;
    args[last].start = last * chunk;
    args[last].end = N; // 包含剩余所有
    args[last].local_buf = (int*)malloc(chunk * sizeof(int));
    worker(&args[last]);
    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }
    int total_sampled = 0;
    for (int i = 0; i < NUM_THREADS; i++) total_sampled += global_counts[i];
    printf("[Done] Sampled %d items.\n", total_sampled);
    // 清理
    for (int i = 0; i < NUM_THREADS; i++) free(args[i].local_buf);
    free(input_data);
    free(global_counts);
    return 0;
}
