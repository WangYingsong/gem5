#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 4
// 减少迭代次数以适应 O3CPU 仿真速度
#define ITERATIONS 100

// 共享变量
volatile int shared_counter = 0;
pthread_mutex_t lock;

void*
increment_counter(void* threadid)
{
    long tid = (long)threadid;
    int i;

    printf("Thread %ld starting...\n", tid);

    for (i = 0; i < ITERATIONS; i++) {
        // 加锁
        pthread_mutex_lock(&lock);

        // 临界区
        int temp = shared_counter;
        // 忙等待一小会儿，增加冲突概率
        volatile int delay = 0;
        for (int k = 0; k < 10; k++) {
            delay++;
        }
        shared_counter = temp + 1;

        // 解锁
        pthread_mutex_unlock(&lock);
    }

    printf("Thread %ld finished.\n", tid);
    return NULL;
}

int
main()
{
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;

    // 初始化互斥锁
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }

    printf("[MAIN] Spawning %d threads to contend for lock...\n", NUM_THREADS);

    for (t = 0; t < NUM_THREADS; t++) {
        // 主线程充当 Worker 0 逻辑 (可选，这里简单起见还是创建4个子线程)
        // 注意：如果你运行这代码遇到 EAGAIN，请使用 hello_mt.c 中的排队逻辑
        // 这里为了简单，假设你已经修复了这个问题或不在意这一点的等待
        rc = pthread_create(&threads[t], NULL, increment_counter, (void*)t);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            return -1;
        }
    }

    for (t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    pthread_mutex_destroy(&lock);

    printf("[MAIN] Final Counter Value: %d (Expected: %d)\n", shared_counter,
           NUM_THREADS * ITERATIONS);

    if (shared_counter == NUM_THREADS * ITERATIONS) {
        printf("[SUCCESS] Cache Coherency Verified!\n");
    } else {
        printf("[FAILED] Race Condition Detected!\n");
    }

    return 0;
}
