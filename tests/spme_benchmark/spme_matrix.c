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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// [设定说明]
// 默认 N=450
// 矩阵总内存占用: 3 * 450^2 * 8 bytes ≈ 4.6 MB
// 你的架构: L1 (64KB) < Data (4.6MB) < LLC (8MB)
// 预期现象:
// 1. L1 Miss Rate 高 (正常)
// 2. LLC Miss Rate 极低 (数据全在 LLC 里)
// 3. IPC 很高 (因为不用访问慢速 DRAM)
#define DEFAULT_N 450
#define DEFAULT_THREADS 4

double **A, **B, **C;
int N = DEFAULT_N;
int num_threads = DEFAULT_THREADS;

// [Gem5 Style Fix] 结构体大括号必须另起一行
typedef struct
{
    int thread_id;
    int start_row;
    int end_row;
} thread_arg_t;

// 初始化矩阵
void
init_matrix()
{
    // 连续内存分配，模拟真实的密集计算场景
    double* dataA = (double*)malloc(N * N * sizeof(double));
    double* dataB = (double*)malloc(N * N * sizeof(double));
    double* dataC = (double*)malloc(N * N * sizeof(double));

    // 指针数组用于二维访问语法 A[i][j]
    A = (double**)malloc(N * sizeof(double*));
    B = (double**)malloc(N * sizeof(double*));
    C = (double**)malloc(N * sizeof(double*));

    for (int i = 0; i < N; i++) {
        A[i] = &dataA[i * N];
        B[i] = &dataB[i * N];
        C[i] = &dataC[i * N];
        for (int j = 0; j < N; j++) {
            A[i][j] = (double)(i + j) * 0.001;
            B[i][j] = (double)(i - j) * 0.001;
            C[i][j] = 0.0;
        }
    }
}

void*
matrix_mul_worker(void* arg)
{
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int start = t_arg->start_row;
    int end = t_arg->end_row;

    // 经典的三重循环矩阵乘法
    // 对于 O3 乱序核心，这里的指令级并行度 (ILP) 极高
    for (int i = start; i < end; i++) {
        for (int k = 0; k < N; k++) {
            double r = A[i][k];
            for (int j = 0; j < N; j++) {
                C[i][j] += r * B[k][j];
            }
        }
    }
    return NULL;
}

int
main(int argc, char* argv[])
{
    // =========================================================
    // 智能参数解析 (适配 fast_run.sh)
    // =========================================================
    if (argc > 1) {
        int val = atoi(argv[1]);
        // 如果参数很小 (< 100)，认为是核心数，N 使用默认 450
        if (val < 100) {
            num_threads = val;
            N = DEFAULT_N;
            printf("[Info] Detected Core Count Arg (%d), "
                   "using Default N=%d\n", val, N);
        } else {
            // 如果参数很大，认为是 N
            N = val;
            if (argc > 2) num_threads = atoi(argv[2]);
        }
    }

    if (num_threads < 1) num_threads = 1;

    double total_mem_mb = (double)N * N * 8 * 3 / (1024.0 * 1024.0);
    printf("[Benchmark] Matrix Mul (N=%d)\n", N);
    printf("[Config] Memory Footprint: %.2f MB\n", total_mem_mb);
    // [Gem5 Style Fix] 长字符串换行
    printf("[Config] Cache Check: Should be < 8.0 MB "
           "(Your LLC Size)\n");
    printf("[Config] Threads: %d\n", num_threads);

    init_matrix();

    pthread_t* threads = (pthread_t*)malloc(
        (num_threads - 1) * sizeof(pthread_t));
    thread_arg_t* args = (thread_arg_t*)malloc(
        num_threads * sizeof(thread_arg_t));

    int rows_per_thread = N / num_threads;

    for (int i = 0; i < num_threads - 1; i++) {
        args[i].thread_id = i;
        args[i].start_row = i * rows_per_thread;
        args[i].end_row = (i + 1) * rows_per_thread;
        // [Gem5 Style Fix] 参数过长需换行
        pthread_create(&threads[i], NULL, matrix_mul_worker,
                       &args[i]);
    }

    // 主线程参与计算
    int last_id = num_threads - 1;
    args[last_id].thread_id = last_id;
    args[last_id].start_row = last_id * rows_per_thread;
    args[last_id].end_row = N;

    printf("Main Thread working...\n");
    matrix_mul_worker(&args[last_id]);

    for (int i = 0; i < num_threads - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[SUCCESS] C[0][0] = %f\n", C[0][0]);

    // 释放内存 (虽然操作系统会回收，但这是好习惯)
    free(A[0]); free(B[0]); free(C[0]); // free data block
    free(A); free(B); free(C);
    free(threads); free(args);

    return 0;
}
