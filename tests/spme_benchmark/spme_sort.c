// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_sort.c
// =========================================================
// 优化版: BigDataBench Sort Proxy (Zero-Malloc)
// 特征: 预分配辅助空间 -> 纯粹压测内存读写与分支预测
// =========================================================

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SIZE 2000000
#define NUM_THREADS 4

int N = DEFAULT_SIZE;
int *array;
int *temp_global; // 全局辅助空间

typedef struct
{
    int id;
    int left;
    int right;
} thread_arg_t;

// 优化后的合并: 不分配内存，使用预分配的 temp
void merge(int *arr, int *temp, int left, int mid, int right) {
    int i = left;
    int j = mid + 1;
    int k = left;

    // 数据拷贝到 temp (测试写带宽)
    for (int x = left; x <= right; x++) temp[x] = arr[x];

    while (i <= mid && j <= right) {
        if (temp[i] <= temp[j]) {
            arr[k++] = temp[i++];
        } else {
            arr[k++] = temp[j++];
        }
    }

    while (i <= mid) arr[k++] = temp[i++];
}

void merge_sort_recursive(int *arr, int *temp, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        merge_sort_recursive(arr, temp, left, mid);
        merge_sort_recursive(arr, temp, mid + 1, right);
        merge(arr, temp, left, mid, right);
    }
}

void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    // 每个线程使用全局 temp 的对应部分，互不干扰
    merge_sort_recursive(array, temp_global, t->left, t->right);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc > 1) N = atoi(argv[1]);

    printf("[Benchmark] Parallel Sort (Master-Worker). Size: %d\n", N);

    array = (int*)malloc(N * sizeof(int));
    temp_global = (int*)malloc(N * sizeof(int)); // 关键优化: 一次性分配

    srand(42);
    for (int i = 0; i < N; i++) array[i] = rand();

    // 修改点 1: 线程句柄减少 1
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    int chunk = N / NUM_THREADS;

    // 修改点 2: 启动子线程 (Worker 0, 1, 2)
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].id = i;
        args[i].left = i * chunk;
        args[i].right = (i + 1) * chunk - 1;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    // 修改点 3: 主线程执行最后一份任务 (Worker 3)
    int last = NUM_THREADS - 1;
    args[last].id = last;
    args[last].left = last * chunk;
    args[last].right = N - 1; // 包含剩余所有
    worker(&args[last]);

    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[Done] Sorted[0]=%d\n", array[0]);
    free(array);
    free(temp_global);
    return 0;
}
