// created by Wang Yingsong on 2025-12-14
// tests/spme_benchmark/spme_task_queue.c
// =============================================================
// SPME 任务排队示例代码
// 说明：模拟多核环境下的任务调度与排队
// =============================================================

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_TASKS 200000
#define NUM_THREADS 4
// 模拟每个任务的计算量 (不要太大，否则测不出锁竞争；也不要太小，否则全是锁开销)
#define WORK_INTENSITY 100
int total_tasks = DEFAULT_TASKS;
int tasks_processed = 0; // 全局共享计数器 (任务队列)
pthread_mutex_t lock;
typedef struct
{
    int id;
} thread_arg_t;
void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    int my_id = t->id;
    int processed_locally = 0;
    while (1) {
        int task_id = -1;
        // --- 临界区开始 (Critical Section) ---
        pthread_mutex_lock(&lock);
        if (tasks_processed < total_tasks) {
            task_id = tasks_processed++;
        }
        pthread_mutex_unlock(&lock);
        // --- 临界区结束 ---
        if (task_id == -1) {
            // 任务队列已空，退出
            break;
        }
        // 模拟任务处理 (Payload)
        volatile int dummy = 0;
        for (int i = 0; i < WORK_INTENSITY; i++) {
            dummy += i;
        }
        processed_locally++;
    }
    // 可以在这里打印每个线程抢到了多少任务，观察负载均衡
    // printf("Worker %d processed %d tasks\n", my_id, processed_locally);
    return NULL;
}
int main(int argc, char* argv[]) {
    if (argc > 1) total_tasks = atoi(argv[1]);
    printf("[Benchmark] TaskQueue (Lock Contention). Tasks: %d\n",
           total_tasks);
    pthread_mutex_init(&lock, NULL);
    // 修改点 1: 线程句柄减少 1
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    // 修改点 2: 启动子线程 (Worker 0, 1, 2)
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].id = i;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    // 修改点 3: 主线程加入抢任务 (Worker 3)
    args[NUM_THREADS - 1].id = NUM_THREADS - 1;
    worker(&args[NUM_THREADS - 1]);
    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("[Done] Processed: %d\n", tasks_processed);
    pthread_mutex_destroy(&lock);
    return 0;
}
