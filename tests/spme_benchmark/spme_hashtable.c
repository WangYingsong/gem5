// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_hashtable.c
// =========================================================
// 优化版: Data Serving Proxy (High-MLP Optimized)
// 特征: 4路独立并发查找 -> 压测 MSHR 和非阻塞缓存能力
// =========================================================

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_TABLE_SIZE 2000000
#define NUM_THREADS 4
#define LOOKUPS_PER_THREAD 1000000

typedef struct Node
{
    uint32_t key;
    uint32_t val;
    struct Node* next;
} Node;

Node** table;
uint32_t table_size = DEFAULT_TABLE_SIZE;
long global_sum = 0; // 用于汇总结果，防止优化

// 内联哈希函数
static inline uint32_t
hash_func(uint32_t key)
{
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    return key % table_size;
}

void
insert(uint32_t key, uint32_t val)
{
    uint32_t idx = hash_func(key);
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->key = key;
    new_node->val = val;
    new_node->next = table[idx];
    table[idx] = new_node;
}

// 查找逻辑内联
static inline uint32_t
lookup_inline(uint32_t key)
{
    uint32_t idx = hash_func(key);
    Node* curr = table[idx];
    while (curr) {
        if (curr->key == key) return curr->val;
        curr = curr->next;
    }
    return 0;
}

typedef struct
{
    int id;
    uint32_t num_lookups;
    uint32_t seed;
} thread_arg_t;

void*
worker(void* arg)
{
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    uint32_t seed1 = t_arg->seed;
    uint32_t seed2 = seed1 + 1000;
    uint32_t seed3 = seed1 + 2000;
    uint32_t seed4 = seed1 + 3000;

    uint32_t dummy = 0;

    // 关键优化: 4路并发查找 (ILP)
    for (uint32_t i = 0; i < t_arg->num_lookups; i += 4) {
        // 拆分长行以符合 Gem5 79字符限制
        seed1 = seed1 * 1103515245 + 12345;
        uint32_t k1 = (seed1 / 65536) % (table_size * 2);

        seed2 = seed2 * 1103515245 + 12345;
        uint32_t k2 = (seed2 / 65536) % (table_size * 2);

        seed3 = seed3 * 1103515245 + 12345;
        uint32_t k3 = (seed3 / 65536) % (table_size * 2);

        seed4 = seed4 * 1103515245 + 12345;
        uint32_t k4 = (seed4 / 65536) % (table_size * 2);

        dummy += lookup_inline(k1);
        dummy += lookup_inline(k2);
        dummy += lookup_inline(k3);
        dummy += lookup_inline(k4);
    }

    // 原子汇总结果
    __sync_fetch_and_add(&global_sum, dummy);
    return NULL;
}

int
main(int argc, char* argv[])
{
    if (argc > 1) table_size = atoi(argv[1]);

    printf("[Benchmark] HashTable (Master-Worker). Size: %d\n", table_size);

    table = (Node**)calloc(table_size, sizeof(Node*));

    // 预热数据
    for (uint32_t i = 0; i < table_size; i++) insert(i, i * 2);

    // 修改点: 只创建 NUM_THREADS - 1 个线程
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];

    // 1. 启动 Worker 0, 1, 2
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].id = i;
        args[i].num_lookups = LOOKUPS_PER_THREAD;
        args[i].seed = i * 9999;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    // 2. 主线程作为 Worker 3
    args[NUM_THREADS - 1].id = NUM_THREADS - 1;
    args[NUM_THREADS - 1].num_lookups = LOOKUPS_PER_THREAD;
    args[NUM_THREADS - 1].seed = (NUM_THREADS - 1) * 9999;
    worker(&args[NUM_THREADS - 1]);

    // 3. 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[Done] Checksum: %ld\n", global_sum);
    return 0;
}
