// created by Wang Yingsong on 2025-12-12
// tests/spme_benchmark/spme_vec_add.c
// =============================================================
// SPME 向量加法示例代码
// 说明：模拟多核环境下的向量加法计算，展示 SPME 模式下的线程行为
// =============================================================

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_N 2000000
#define NUM_THREADS 4

long N = DEFAULT_N;
double *a, *b, *c;

typedef struct
{
    int id;
    long start;
    long end;
} thread_arg_t;

void*
worker(void* arg)
{
    thread_arg_t* t = (thread_arg_t*)arg;
    for (long i = t->start; i < t->end; i++) {
        c[i] = a[i] + b[i];
    }
    return NULL;
}

int
main(int argc, char* argv[])
{
    if (argc > 1)
        N = atol(argv[1]);

    printf("[Benchmark] Vector Add. N=%ld\n", N);

    a = (double*)malloc(N * sizeof(double));
    b = (double*)malloc(N * sizeof(double));
    c = (double*)malloc(N * sizeof(double));

    for (long i = 0; i < N; i++) {
        a[i] = i * 1.0;
        b[i] = i * 0.5;
    }

    pthread_t th[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    long chunk = N / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].start = i * chunk;
        args[i].end = (i == NUM_THREADS - 1) ? N : (i + 1) * chunk;
        pthread_create(&th[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(th[i], NULL);
    }

    printf("[Done] c[N-1] = %f\n", c[N-1]);
    return 0;
}
