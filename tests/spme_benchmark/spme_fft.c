// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_fft.c
// =========================================================
// 优化版: FFT Proxy (Precomputed Twiddle)
// 修复: 手动定义 M_PI，确保兼容性
// =========================================================

#include <complex.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// [Fix] 如果 math.h 没有定义 M_PI，手动定义它
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEFAULT_N 65536
#define NUM_THREADS 4
int N = DEFAULT_N;
double complex *data;
double complex *twiddles; // 预计算表
typedef struct
{ int id; int n; } thread_arg_t;
void precompute_twiddles() {
    twiddles = (double complex*)malloc(sizeof(double complex) * (N / 2));
    for (int i = 0; i < N / 2; i++) {
        // 使用欧拉公式预计算: e^(-2*pi*i/N)
        // cexp 是 C99 complex.h 标准函数
        twiddles[i] = cexp(-2.0 * M_PI * I * i / N);
    }
}
void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    int local_n = t->n;
    int start = t->id * (N / NUM_THREADS);
    // 模拟 FFT 核心级数
    for (int len = 2; len <= local_n; len <<= 1) {
        int half_len = len / 2;
        // 步长为 len
        for (int i = 0; i < local_n; i += len) {
            for (int j = 0; j < half_len; j++) {
                // 模拟访问模式: u 和 v 间隔 half_len
                int idx_u = start + i + j;
                int idx_v = idx_u + half_len;
                // 查表获取旋转因子 (模拟跨步访存)
                int twiddle_idx = (j * (N / len)) % (N/2);
                double complex w = twiddles[twiddle_idx];
                double complex u = data[idx_u];
                double complex v = data[idx_v] * w; // 核心 FPU 负载
                data[idx_u] = u + v;
                data[idx_v] = u - v;
            }
        }
    }
    return NULL;
}
int main(int argc, char* argv[]) {
    if (argc > 1) N = atoi(argv[1]);
    printf("[Benchmark] FFT (Precomputed). Size: %d\n", N);
    data = (double complex*)malloc(sizeof(double complex) * N);
    for (int i=0; i<N; i++) data[i] = i + 0 * I;
    precompute_twiddles();
    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    for (int i=0; i<NUM_THREADS; i++) {
        args[i].id = i;
        args[i].n = N / NUM_THREADS;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    for (int i=0; i<NUM_THREADS; i++) pthread_join(threads[i], NULL);
    printf("[Done] data[0]=%f\n", creal(data[0]));
    free(data);
    free(twiddles);
    return 0;
}
