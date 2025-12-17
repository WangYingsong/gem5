// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_grep.c
// =========================================================
// Proxy: BigDataBench Grep
// 特征: 模式匹配, 分支预测敏感, 流式扫描
// =========================================================

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SIZE 50000000 // 50MB
#define NUM_THREADS 4
// 搜索模式: "gem5"
const char* PATTERN = "gem5";

long data_size = DEFAULT_SIZE;
char* text_data;
long total_matches = 0;
pthread_mutex_t lock;

typedef struct
{
    long start;
    long end;
} thread_arg_t;

void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    long local_matches = 0;
    const char* pat = PATTERN;
    int pat_len = strlen(pat);

    // 优化版搜索: 这里的逻辑是为了制造分支压力
    // 每次比较第一个字符，如果匹配才进入内层循环
    for (long i = t->start; i < t->end - pat_len; i++) {
        // Branch 1: 检查首字符 (高频分支，容易预测)
        if (text_data[i] == pat[0]) {
            // Branch 2: 检查完整字符串 (低频分支，如果数据有很多 'g' 但不是 'gem5'，则难预测)
            int match = 1;
            for (int j = 1; j < pat_len; j++) {
                if (text_data[i+j] != pat[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) local_matches++;
        }
    }

    pthread_mutex_lock(&lock);
    total_matches += local_matches;
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc > 1) data_size = atol(argv[1]);

    printf("[Benchmark] Grep (Master-Worker). Size: %ld Bytes\n", data_size);

    text_data = (char*)malloc(data_size + 1);

    // 填充数据: 大量 'g' 但不一定是 "gem5"，制造分支压力
    srand(42);
    for (long i=0; i<data_size; i++) {
        int r = rand() % 100;
        if (r < 10) text_data[i] = 'g';      // 10% 概率是 'g' (诱导进入内层比较)
        else if (r < 15) text_data[i] = 'e';
        else if (r < 20) text_data[i] = 'm';
        else if (r < 25) text_data[i] = '5';
        else text_data[i] = 'a' + (r % 26);
    }
    text_data[data_size] = '\0';

    pthread_mutex_init(&lock, NULL);

    // 修改点 1: 线程句柄数组减少 1
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    long chunk = data_size / NUM_THREADS;

    // 修改点 2: 创建前 NUM_THREADS-1 个任务作为子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].start = i * chunk;
        args[i].end = (i + 1) * chunk;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    // 修改点 3: 主线程处理最后一份任务 (Worker 3)
    int last = NUM_THREADS - 1;
    args[last].start = last * chunk;
    args[last].end = data_size; // 确保覆盖所有剩余数据
    worker(&args[last]);

    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[Done] Found %ld matches.\n", total_matches);
    free(text_data);
    return 0;
}
