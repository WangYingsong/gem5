#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// 你的架构配置
#define NUM_CPUS 4
// 我们设置任务数为核心数的 3 倍，确保必须排队
#define TOTAL_TASKS 12

// 模拟计算负载的强度 (防止任务瞬间跑完，看不出排队效果)
#define WORK_INTENSITY 10000

// 线程参数
typedef struct
{
    long task_id;
} thread_arg_t;

thread_arg_t task_args[TOTAL_TASKS];
pthread_t threads[TOTAL_TASKS];

// === 工作线程函数 ===
void*
worker_thread(void* arg)
{
    thread_arg_t* my_arg = (thread_arg_t*)arg;
    long tid = my_arg->task_id;

    // 1. 获取当前“占用”的线程ID (对应硬件Context)
    pthread_t sys_id = pthread_self();

    printf("   [Worker] Task %02ld STARTED on HW Context 0x%lx\n",
           tid, (unsigned long)sys_id);

    // 2. 模拟耗时计算 (忙等待)
    volatile int counter = 0;
    for (int i = 0; i < WORK_INTENSITY; i++) {
        counter++;
    }

    printf("   [Worker] Task %02ld FINISHED. Releasing Core.\n", tid);
    return NULL;
}

int
main()
{
    int rc;
    long t;

    printf("\n[MAIN] Scheduler started.\n");
    printf("[MAIN] Hardware Cores: %d\n", NUM_CPUS);
    printf("[MAIN] Total Tasks:    %d\n", TOTAL_TASKS);
    printf("------------------------------------------------------------\n");

    // === 任务分发循环 ===
    for (t = 0; t < TOTAL_TASKS; t++) {
        task_args[t].task_id = t;

        // 【关键逻辑】 失败重试循环 (Busy Retry Loop)
        while (1) {
            rc = pthread_create(&threads[t], NULL, worker_thread,
                                (void*)&task_args[t]);

            if (rc == 0) {
                // 成功：任务被放入了某个空闲核心
                break;
            } else if (rc == EAGAIN) { // EAGAIN 通常是 11
                // 失败：核心已满，原地空转等待
                volatile int dummy = 0;
                dummy++;
            } else {
                // 其他致命错误
                printf("[FATAL] pthread_create failed with code %d\n", rc);
                exit(-1);
            }
        }
    }

    printf("------------------------------------------------------------\n");
    printf("[MAIN] All tasks dispatched. Waiting for completion...\n");

    // === 等待所有任务完成 ===
    for (t = 0; t < TOTAL_TASKS; t++) {
        pthread_join(threads[t], NULL);
    }

    printf("[MAIN] All %d tasks completed successfully.\n", TOTAL_TASKS);
    return 0;
}
