// created by Wang Yingsong on 2025-12-16
// tests/spme_benchmark/spme_stream.c
// =========================================================
// 流式内存带宽基准测试
// =========================================================
/*
 * spme_stream.c (Smart Arg Version)
 * Purpose: Memory Bandwidth Benchmark (STREAM Triad-like)
 * Logic: C[i] = A[i] + B[i]
 * * Auto-Adaptation for fast_run.sh:
 * - If argv[1] is small (< 1000), it's treated as Num_Threads.
 * - If argv[1] is large (>= 1000), it's treated as Array_Size.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_N 2000000
#define NUM_THREADS 4

long N = DEFAULT_N;
double *a, *b, *c;
double scalar = 3.0;

typedef struct
{
    long start;
    long end;
} thread_arg_t;

void*
worker(void* arg)
{
    thread_arg_t* t = (thread_arg_t*)arg;
    for (long i = t->start; i < t->end; i++) {
        c[i] = a[i];                  // Copy
        b[i] = scalar * c[i];         // Scale
        c[i] = a[i] + b[i];           // Add
        a[i] = b[i] + scalar * c[i];  // Triad
    }
    return NULL;
}

int
main(int argc, char* argv[])
{
    if (argc > 1)
        N = atol(argv[1]);

    printf("[Benchmark] Stream (Master-Worker). N=%ld\n", N);

    a = (double*)malloc(N * sizeof(double));
    b = (double*)malloc(N * sizeof(double));
    c = (double*)malloc(N * sizeof(double));

    for (long i = 0; i < N; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 0.0;
    }

    // 修改点 1: 线程句柄减少 1
    pthread_t th[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    long chunk = N / NUM_THREADS;

    // 修改点 2: 启动子线程 (Worker 0, 1, 2)
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].start = i * chunk;
        args[i].end = (i + 1) * chunk;
        pthread_create(&th[i], NULL, worker, &args[i]);
    }

    // 修改点 3: 主线程执行最后一份任务 (Worker 3)
    int last = NUM_THREADS - 1;
    args[last].start = last * chunk;
    args[last].end = N;
    worker(&args[last]);

    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(th[i], NULL);
    }

    printf("[Done] a[0]=%f\n", a[0]);

    free(a);
    free(b);
    free(c);

    return 0;
}
