// created by Wang Yingsong on 2025-12-15
// tests/spme_benchmark/spme_matrix.c
// =========================================================
// 矩阵乘法基准测试
// =========================================================
/*
* spme_matrix.c (Smart Arg Version)
* Purpose: Matrix Multiplication Benchmark
* Logic: C = A * B
* - Matrix Size N x N
* - Memory Footprint designed to fit in LLC but exceed L1/Data Cache
* * Auto-Adaptation for fast_run.sh:
* - If argv[1] is small (< 100), it's treated as Num_Threads.
* - If argv[1] is large (>= 100), it's treated as Matrix Size N.
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_SIZE 512
#define NUM_THREADS 4

int N = DEFAULT_SIZE;
double *A, *B, *C;

typedef struct
{ int id; int start_row; int end_row; } thread_arg_t;

void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    // 优化后的 ikj 顺序
    for (int i = t->start_row; i < t->end_row; i++) {
        for (int k = 0; k < N; k++) {
            double r = A[i * N + k];
            for (int j = 0; j < N; j++) {
                C[i * N + j] += r * B[k * N + j];
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc > 1) N = atoi(argv[1]);
    printf("[Benchmark] Matrix Mul (Master-Worker). Size: %d\n", N);

    A = (double*)malloc(N*N*sizeof(double));
    B = (double*)malloc(N*N*sizeof(double));
    C = (double*)calloc(N*N, sizeof(double));
    for (int i=0; i<N*N; i++) { A[i]=1.0; B[i]=1.0; }

    pthread_t threads[NUM_THREADS-1];
    thread_arg_t args[NUM_THREADS];
    int chunk = N / NUM_THREADS;

    // 启动 NUM_THREADS-1 个子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].start_row = i * chunk;
        args[i].end_row = (i + 1) * chunk;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    // 主线程做最后一份
    args[NUM_THREADS-1].start_row = (NUM_THREADS-1) * chunk;
    args[NUM_THREADS-1].end_row = N;
    worker(&args[NUM_THREADS-1]);

    // 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) pthread_join(threads[i], NULL);

    printf("[Done] C[0]=%f\n", C[0]);
    return 0;
}
