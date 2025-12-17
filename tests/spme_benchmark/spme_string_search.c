// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_string_search.c
// =========================================================
// 优化版: MapReduce WordCount Proxy (Histogram)
// 特征: 线性内存扫描 + 密集 Load/Store -> 压测 L1 带宽与流水线
// =========================================================

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 默认 50MB 数据
#define DEFAULT_SIZE 50000000
#define NUM_THREADS 4

long data_size = DEFAULT_SIZE;
unsigned char* big_data;
long global_hist[256] = {0};
pthread_mutex_t lock;

typedef struct
{
    long start;
    long end;
} thread_arg_t;

void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    long local_hist[256] = {0}; // 线程私有直方图，避免 False Sharing
    unsigned char* ptr = big_data + t->start;
    unsigned char* end = big_data + t->end;

    // 关键循环: 极其紧凑的 Load-Add-Store
    while (ptr < end) {
        local_hist[*ptr]++;
        ptr++;
    }

    // 合并结果
    pthread_mutex_lock(&lock);
    for (int i=0; i<256; i++) global_hist[i] += local_hist[i];
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc > 1) data_size = atol(argv[1]);

    printf("[Benchmark] Histogram (Master-Worker). Size: %ld\n", data_size);

    big_data = (unsigned char*)malloc(data_size);
    // 随机填充数据
    for (long i=0; i<data_size; i++) big_data[i] = rand() % 256;

    pthread_mutex_init(&lock, NULL);

    // 修改点 1: 线程句柄减少 1
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    long chunk = data_size / NUM_THREADS;

    // 修改点 2: 启动子线程 (Worker 0, 1, 2)
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].start = i * chunk;
        args[i].end = (i + 1) * chunk;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    // 修改点 3: 主线程执行最后一份任务 (Worker 3)
    int last = NUM_THREADS - 1;
    args[last].start = last * chunk;
    args[last].end = data_size; // 覆盖剩余
    worker(&args[last]);

    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[Done] Checksum (Byte 0): %ld\n", global_hist[0]);
    free(big_data);
    return 0;
}
