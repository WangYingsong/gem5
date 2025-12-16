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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// 默认数组大小: 200万元素
// 内存占用: 2,000,000 * 8 bytes * 3 arrays = ~48 MB
// 这远超 8MB LLC，迫使 CPU 访问 DRAM，从而测出带宽瓶颈
#define DEFAULT_SIZE 2000000
#define DEFAULT_THREADS 4

double *A, *B, *C;
int array_size = DEFAULT_SIZE;
int num_threads = DEFAULT_THREADS;

// [Gem5 Style Fix] 结构体大括号另起一行
typedef struct
{
    int thread_id;
    int start_idx;
    int end_idx;
} thread_arg_t;

// 线程工作函数: 简单的线性访问，最大化带宽压力
void*
stream_worker(void* arg)
{
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int start = t_arg->start_idx;
    int end = t_arg->end_idx;

    for (int i = start; i < end; i++) {
        C[i] = A[i] + B[i];
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

        // 启发式判断：
        // 如果传入的值小于 1000，肯定不是为了测带宽的数组大小，而是核心数
        // (这是为了适配 fast_run.sh 自动传入的 --options="$core_count")
        if (val < 1000) {
            num_threads = val;
            array_size = DEFAULT_SIZE; // 强制使用大数组以击穿缓存
            printf("[Info] Detected Small Arg (%d), "
                   "treating as Core Count.\n", val);
        } else {
            // 如果传入的值很大，说明是手动运行指定的数组大小
            array_size = val;
            if (argc > 2) {
                num_threads = atoi(argv[2]);
            }
        }
    }

    // 安全检查
    if (num_threads < 1) num_threads = 1;
    if (array_size < 1000) array_size = 1000; // 防止太小没意义

    // 计算总内存占用 (MB)
    double total_mem_mb = (double)array_size * sizeof(double) * 3.0 /
                          (1024.0 * 1024.0);

    printf("[Benchmark] STREAM Copy+Add (Memory Bandwidth Test)\n");
    printf("[Config] Array Size    : %d elements\n", array_size);
    // [Gem5 Style Fix] 长字符串换行
    printf("[Config] Memory Footprint: %.2f MB "
           "(Should be > 8.0 MB for LLC Thrashing)\n", total_mem_mb);
    printf("[Config] Active Threads  : %d\n", num_threads);

    // 2. 动态分配内存
    A = (double*)malloc(array_size * sizeof(double));
    B = (double*)malloc(array_size * sizeof(double));
    C = (double*)malloc(array_size * sizeof(double));

    if (!A || !B || !C) {
        fprintf(stderr, "[Error] Memory allocation failed!\n");
        return 1;
    }

    // 3. 初始化 (Write Traffic)
    for (int i = 0; i < array_size; i++) {
        A[i] = 1.0;
        B[i] = 2.0;
        C[i] = 0.0;
    }

    // 4. 创建线程
    pthread_t* threads = (pthread_t*)malloc(
        (num_threads - 1) * sizeof(pthread_t));
    thread_arg_t* args = (thread_arg_t*)malloc(
        num_threads * sizeof(thread_arg_t));

    int chunk_size = array_size / num_threads;

    for (int i = 0; i < num_threads - 1; i++) {
        args[i].thread_id = i;
        args[i].start_idx = i * chunk_size;
        args[i].end_idx = (i + 1) * chunk_size;

        // [Gem5 Style Fix] 参数过长换行
        if (pthread_create(&threads[i], NULL, stream_worker,
                           &args[i])) {
            perror("Thread creation failed");
            return 1;
        }
    }

    // 主线程也参与工作
    int last_id = num_threads - 1;
    args[last_id].thread_id = last_id;
    args[last_id].start_idx = last_id * chunk_size;
    args[last_id].end_idx = array_size; // 处理剩余尾部

    printf("Main Thread working as Worker %d...\n", last_id);
    stream_worker(&args[last_id]);

    // 5. 等待结束
    for (int i = 0; i < num_threads - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    // 简单验证
    printf("[SUCCESS] Done. C[0]=%.1f\n", C[0]);

    free(A); free(B); free(C);
    free(threads); free(args);
    return 0;
}
