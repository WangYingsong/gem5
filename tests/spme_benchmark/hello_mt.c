// created by Wang Yingsong on 2025-12-12
// tests/spme_benchmark/hello_mt.c

#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#define NUM_CPUS 4
#define TOTAL_TASKS 10
#define WORKLOAD_INTENSITY 100

void* thread_task(void* thread_arg) {
    long task_id = (long)thread_arg;
    pthread_t sys_tid = pthread_self(); // 获取线程唯一ID (代表硬件上下文)

    // 1. 模拟一点点工作 (避免太快导致全是 Core 1 在抢任务)
    volatile int counter = 0;
    for (int i = 0; i < WORKLOAD_INTENSITY; i++) {
        counter++;
    }

    // 2. 打印核心信息
    // 注意：GEM5 SE 模式下，不同的 Pthread ID (SysID) 就对应不同的硬件 Core
    printf("   [Core Context: 0x%lx] executing Task %ld: Hello World!\n", 
           (unsigned long)sys_tid, task_id);

    return NULL;
}

int main() {
    pthread_t threads[TOTAL_TASKS];
    int rc;
    long t;

    printf("[MAIN] System: 4 Cores. Workload: %d Hello World Tasks.\n", TOTAL_TASKS);
    printf("----------------------------------------------------------------\n");
    printf("   [Core Context]          | [Task info]\n");
    printf("----------------------------------------------------------------\n");

    for(t = 0; t < TOTAL_TASKS; t++) {
        // === 排队重试机制 ===
        while(1) {
            rc = pthread_create(&threads[t], NULL, thread_task, (void *)t);
            
            if (rc == 0) {
                break; // 成功放入核心
            } else if (rc == 11) { 
                // 核心满了 (EAGAIN)，原地重试
                // 因为计算量很小，瞬间就会有核心释放
            } else {
                printf("FATAL ERROR %d\n", rc);
                return -1;
            }
        }
    }

    // 等待所有任务
    for(t = 0; t < TOTAL_TASKS; t++) {
        pthread_join(threads[t], NULL);
    }

    printf("----------------------------------------------------------------\n");
    printf("[MAIN] All tasks finished.\n");
    return 0;
}