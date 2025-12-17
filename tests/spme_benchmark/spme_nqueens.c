// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_nqueens.c
// =========================================================
// 优化版: SAT Proxy (Bitwise Optimized)
// 特征: 纯位运算 + 深度递归 -> 极致压测分支预测与ALU
// =========================================================

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_N 13
#define NUM_THREADS 4
int N = DEFAULT_N;
long total_solutions = 0;
pthread_mutex_t lock;
typedef struct
{
    int id;
    int start_col;
    int end_col;
} thread_arg_t;
// 核心优化: 位运算解法
// ld: left diagonal, col: column, rd: right diagonal
void solve_bitwise(int row, int ld, int col, int rd, int n_mask, long* count) {
    if (row == N) {
        (*count)++;
        return;
    }
    // 所有可放的位置 (mask)
    int available = ~(ld | col | rd) & n_mask;
    while (available) {
        int p = available & -available;
        available -= p;
        solve_bitwise(row + 1, (ld | p) << 1, col | p, (rd | p) >> 1,
                      n_mask, count);
    }
}
void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    long local_count = 0;
    int n_mask = (1 << N) - 1;
    // 第一行的处理 (手动展开以分配任务)
    for (int i = t->start_col; i < t->end_col; i++) {
        int p = (1 << i);
        solve_bitwise(1, p << 1, p, p >> 1, n_mask, &local_count);
    }
    pthread_mutex_lock(&lock);
    total_solutions += local_count;
    pthread_mutex_unlock(&lock);
    return NULL;
}
int main(int argc, char* argv[]) {
    if (argc > 1) N = atoi(argv[1]);
    printf("[Benchmark] N-Queens (Master-Worker + Bitwise). Board: %d\n", N);
    pthread_mutex_init(&lock, NULL);
    // 修改点 1: 只创建 NUM_THREADS - 1 个线程句柄
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    int chunk = (N + NUM_THREADS - 1) / NUM_THREADS;
    // 修改点 2: 启动子线程 (Worker 0, 1, 2)
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].start_col = i * chunk;
        args[i].end_col = (i + 1) * chunk;
        if (args[i].end_col > N) args[i].end_col = N;
        // 只有当任务有效时才创建 (防止 N 很小时出错)
        if (args[i].start_col < N) {
            pthread_create(&threads[i], NULL, worker, &args[i]);
        }
    }
    // 修改点 3: 主线程作为 Worker 3
    int last = NUM_THREADS - 1;
    args[last].start_col = last * chunk;
    args[last].end_col = N;
    if (args[last].start_col < N) {
        worker(&args[last]);
    }
    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        if (i * chunk < N) {
             pthread_join(threads[i], NULL);
        }
    }
    printf("[Done] Solutions: %ld\n", total_solutions);
    return 0;
}
