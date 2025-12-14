#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

#define NUM_CPUS 4
#define TOTAL_TASKS 8
#define WORKLOAD_INTENSITY 100

void*
thread_task(void* thread_arg)
{
    long task_id = (long)thread_arg;

    // 1. 获取软件层面的线程 ID (内存地址)
    pthread_t sys_tid = pthread_self();

    // 2. 获取硬件层面的 CPU 核心编号 (SE模式下通常无效)
    int cpu_id = sched_getcpu();

    // 模拟一点工作
    volatile int counter = 0;
    for (int i = 0; i < WORKLOAD_INTENSITY; i++) {
        counter++;
    }

    // 3. 打印双重身份验证 (折行以符合规范)
    printf("   [HW Core: %d] | [SW Thread: 0x%lx] | Executing Task %ld\n",
           cpu_id, (unsigned long)sys_tid, task_id);

    return NULL;
}

int
main()
{
    pthread_t threads[TOTAL_TASKS];
    int rc;
    long t;

    printf("[MAIN] System: 4 Cores. Workload: %d Tasks.\n", TOTAL_TASKS);
    // 缩短分割线，确保不超过 79 字符
    printf("----------------------------------------------------------\n");
    printf("   [Hardware]      | [Software]          | [Task]\n");
    printf("----------------------------------------------------------\n");

    for (t = 0; t < TOTAL_TASKS; t++) {
        while (1) {
            rc = pthread_create(&threads[t], NULL, thread_task, (void*)t);
            if (rc == 0) {
                break;
            } else if (rc == 11) { // EAGAIN
                // 忙等待：原地空转，等待其他线程释放资源
                volatile int dummy = 0;
                dummy++;
            } else {
                printf("FATAL ERROR %d\n", rc);
                return -1;
            }
        }
    }

    for (t = 0; t < TOTAL_TASKS; t++) {
        pthread_join(threads[t], NULL);
    }

    printf("----------------------------------------------------------\n");
    return 0;
}
