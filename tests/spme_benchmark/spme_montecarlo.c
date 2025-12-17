// created by Wang Yingsong on 2025-12-16
// tests/spme_benchmark/spme_montecarlo.c
// =========================================================
// 蒙特卡洛 Pi 近似计算 (Compute Bound / High IPC)
// 特征: 4路独立 RNG + 循环展开 -> 旨在打满 4-Wide 架构 IPC
// =========================================================
/*
 * spme_montecarlo.c (Smart Arg Version)
 * Purpose: Monte Carlo Pi Approximation (Compute Bound / High IPC)
 * Logic: Estimate Pi using random sampling
 * * Auto-Adaptation for fast_run.sh:
 * - If argv[1] is small (< 100), it's treated as Num_Threads.
 * - If argv[1] is large (>= 100), it's treated as Total Iterations.
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// 默认 2亿次迭代 (足够热身)
#define DEFAULT_LOOPS 200000000
#define NUM_THREADS 4
long total_loops = DEFAULT_LOOPS;
long loops_per_thread;
long hits[NUM_THREADS];
typedef struct
{
    int id;
    long num_iters;
} thread_arg_t;
// 简单的线性同余生成器 (LCG) 参数
#define A 1664525
#define C 1013904223
#define M 4294967296.0
// 预计算倒数，把除法变乘法 (IPC 提升关键)
#define M_RECIP (1.0 / M)
// 核心工作函数 (深度优化)
void* worker(void* arg) {
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int tid = t_arg->id;
    long iters = t_arg->num_iters;
    long local_hits = 0;
    // 关键优化: 4路独立种子 (ILP-4)
    unsigned int s0 = tid * 1000 + 123;
    unsigned int s1 = tid * 1000 + 456;
    unsigned int s2 = tid * 1000 + 789;
    unsigned int s3 = tid * 1000 + 999;
    long i;
    // 每次循环处理 4 个点
    for (i = 0; i < iters; i += 4) {
        // --- Lane 0 ---
        s0 = s0 * 1664525 + 1013904223;
        double x0 = (double)s0 * M_RECIP; // 乘法代替除法
        s0 = s0 * 1664525 + 1013904223;
        double y0 = (double)s0 * M_RECIP;
        if (x0*x0 + y0*y0 <= 1.0) local_hits++;
        // --- Lane 1 ---
        s1 = s1 * 1664525 + 1013904223;
        double x1 = (double)s1 * M_RECIP;
        s1 = s1 * 1664525 + 1013904223;
        double y1 = (double)s1 * M_RECIP;
        if (x1*x1 + y1*y1 <= 1.0) local_hits++;
        // --- Lane 2 ---
        s2 = s2 * 1664525 + 1013904223;
        double x2 = (double)s2 * M_RECIP;
        s2 = s2 * 1664525 + 1013904223;
        double y2 = (double)s2 * M_RECIP;
        if (x2*x2 + y2*y2 <= 1.0) local_hits++;
        // --- Lane 3 ---
        s3 = s3 * 1664525 + 1013904223;
        double x3 = (double)s3 * M_RECIP;
        s3 = s3 * 1664525 + 1013904223;
        double y3 = (double)s3 * M_RECIP;
        if (x3*x3 + y3*y3 <= 1.0) local_hits++;
    }
    hits[tid] = local_hits;
    return NULL;
}
int main(int argc, char* argv[]) {
    if (argc > 1) total_loops = atol(argv[1]);
    loops_per_thread = total_loops / NUM_THREADS;
    // 确保是 4 的倍数以便循环展开
    loops_per_thread = (loops_per_thread / 4) * 4;
    printf("[Benchmark] MonteCarlo (Master-Worker + ILP). Total: %ld\n",
           total_loops);
    // 修改点 1: 线程句柄减少 1
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    // 修改点 2: 启动 NUM_THREADS - 1 个子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].id = i;
        args[i].num_iters = loops_per_thread;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    // 修改点 3: 主线程执行最后一份任务 (Worker 3)
    int last = NUM_THREADS - 1;
    args[last].id = last;
    args[last].num_iters = loops_per_thread;
    worker(&args[last]);
    long total_hits = 0;
    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }
    // 汇总所有结果 (包括主线程计算的 hits[3])
    for (int i = 0; i < NUM_THREADS; i++) {
        total_hits += hits[i];
    }
    double pi = 4.0 * (double)total_hits /
                (double)(loops_per_thread * NUM_THREADS);
    printf("[Done] Pi = %f\n", pi);
    return 0;
}
