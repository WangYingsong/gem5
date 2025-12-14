#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// 定义对应你的 4 核架构
#define NUM_CPUS 4
// 定义向量长度
#define VECTOR_SIZE 1024

// 全局数据 (模拟共享内存)
int A[VECTOR_SIZE];
int B[VECTOR_SIZE];
int C[VECTOR_SIZE];

// 线程参数结构体
typedef struct
{
    long logic_core_id;
    int start_idx;
    int end_idx;
} ThreadArgs;

ThreadArgs args[NUM_CPUS];
pthread_t threads[NUM_CPUS];

// === SPME 核心逻辑 (所有核都跑这段完全一样的代码) ===
void*
vector_add_kernel(void* arg)
{
    ThreadArgs* my_args = (ThreadArgs*)arg;
    int start = my_args->start_idx;
    int end = my_args->end_idx;
    long tid = my_args->logic_core_id;

    printf("   [Core %ld] Activated. Processing Data Chunk [%d - %d]\n",
           tid, start, end - 1);

    // 核心计算循环
    for (int i = start; i < end; i++) {
        C[i] = A[i] + B[i];
    }

    return NULL;
}

int
main()
{
    int i;
    int chunk_size = VECTOR_SIZE / NUM_CPUS;

    printf("[MAIN] Initializing Vectors (Size: %d)...\n", VECTOR_SIZE);
    // 1. 初始化数据
    for (i = 0; i < VECTOR_SIZE; i++) {
        A[i] = i;        // 0, 1, 2...
        B[i] = i * 2;    // 0, 2, 4...
        C[i] = 0;
    }

    printf("[MAIN] Simulating SPME Mode: Forking %d threads...\n", NUM_CPUS);
    printf("------------------------------------------------------------\n");

    // 2. 模拟 SPME 发射 (Launch)

    // (A) 创建 3 个子线程 (Core 1, 2, 3)
    for (long t = 1; t < NUM_CPUS; t++) {
        args[t].logic_core_id = t;
        args[t].start_idx = t * chunk_size;
        args[t].end_idx = (t == NUM_CPUS - 1) ? VECTOR_SIZE :
                                                (t + 1) * chunk_size;

        if (pthread_create(&threads[t], NULL, vector_add_kernel,
                           (void*)&args[t])) {
            printf("Error creating thread %ld\n", t);
            return -1;
        }
    }

    // (B) 主线程自己充当 Worker 0 (Core 0)
    args[0].logic_core_id = 0;
    args[0].start_idx = 0;
    args[0].end_idx = chunk_size;
    vector_add_kernel((void*)&args[0]);

    // 3. 模拟 SPME 汇聚 (Join/Barrier)
    for (long t = 1; t < NUM_CPUS; t++) {
        pthread_join(threads[t], NULL);
    }

    printf("------------------------------------------------------------\n");
    printf("[MAIN] Computation Complete. Verifying results...\n");

    // 4. 结果验证 (抽样检查)
    int error = 0;
    for (i = 0; i < VECTOR_SIZE; i++) {
        int expected = i + (i * 2); // 3*i
        if (C[i] != expected) {
            printf("Error at index %d: Expected %d, Got %d\n",
                   i, expected, C[i]);
            error = 1;
            break;
        }
    }

    if (!error) {
        printf("[SUCCESS] SPME Vector Addition Verified!\n");
    } else {
        printf("[FAILED] Calculation errors found.\n");
    }

    return 0;
}
