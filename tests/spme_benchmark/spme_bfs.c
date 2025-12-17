// created by Wang Yingsong on 2025-12-16
// tests/spme_benchmark/spme_bfs.c
// =========================================================
// 图计算 BFS (Memory Bound / Low IPC / Random Access)
// =========================================================
/*
 * spme_bfs.c (Smart Arg Version)
 * Purpose: Graph BFS Benchmark (Memory Bound / Low IPC / Random Access)
 * Logic: Breadth-First Search on Random Graph
 * * Auto-Adaptation for fast_run.sh:
 * - If argv[1] is small (< 100), it's treated as Num_Threads.
 * - If argv[1] is large (>= 100), it's treated as Number of Nodes.
*/

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_NODES 65536
#define AVG_EDGES 16
#define NUM_THREADS 4  // 默认定死为 4，配合 Gem5 4核脚本

typedef struct
{
    int id;
    int num_edges;
    int* edges;
    int distance;
    bool visited;
} Node;

Node* graph;
int num_nodes = DEFAULT_NODES;
int num_threads = NUM_THREADS;

// 全局队列
typedef struct
{
    int* data;
    int head;
    int tail;
} Queue;

Queue global_q;

void init_queue(int capacity) {
    global_q.data = (int*)malloc(capacity * sizeof(int));
    global_q.head = 0;
    global_q.tail = 0;
}

typedef struct
{
    int thread_id;
    int q_start;
    int q_end;
    int* next_level_nodes; // 线程私有缓冲区，避免锁竞争
    int next_count;
    int current_level;
} thread_arg_t;

void* bfs_worker(void* arg) {
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int start = t_arg->q_start;
    int end = t_arg->q_end;
    int current_dist = t_arg->current_level;

    t_arg->next_count = 0;

    // 如果没有任务分配给该线程，直接返回 (保持核心热度)
    if (start >= end) return NULL;

    for (int i = start; i < end; i++) {
        int u_id = global_q.data[i];
        // 指针追逐 A: 访问节点结构
        Node* u = &graph[u_id];

        for (int j = 0; j < u->num_edges; j++) {
            // 指针追逐 B: 访问边表
            int v_id = u->edges[j];
            Node* v = &graph[v_id];

            // 核心竞争点: CAS 原子操作
            if (__sync_bool_compare_and_swap(&v->visited, false, true)) {
                v->distance = current_dist + 1;
                // 写入私有缓冲区，完全无锁
                t_arg->next_level_nodes[t_arg->next_count++] = v_id;
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    // 灵活参数: ./spme_bfs <NODES>
    if (argc > 1) {
        int val = atoi(argv[1]);
        if (val > 0) num_nodes = val;
    }

    printf("[Benchmark] Graph BFS (Master-Worker). Nodes: %d\n", num_nodes);

    // 1. 图生成 (CSR-like Pointer Chasing)
    graph = (Node*)malloc(num_nodes * sizeof(Node));
    srand(42);

    for (int i = 0; i < num_nodes; i++) {
        graph[i].id = i;
        graph[i].visited = false;
        graph[i].distance = -1;
        int edges = AVG_EDGES + (rand() % AVG_EDGES); // 16~31 edges
        graph[i].num_edges = edges;
        graph[i].edges = (int*)malloc(edges * sizeof(int));

        for (int j = 0; j < edges; j++) {
            graph[i].edges[j] = rand() % num_nodes;
        }
    }

    init_queue(num_nodes * 10); // 足够大的队列

    // 2. 线程资源准备 (栈分配，避免 malloc)
    // 注意: Master-Worker 模式下只创建 N-1 个子线程
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];

    // 为每个线程预分配私有缓冲区
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].next_level_nodes = (int*)malloc(num_nodes * sizeof(int));
    }

    // 3. BFS 初始化
    graph[0].visited = true;
    graph[0].distance = 0;
    global_q.data[0] = 0;
    global_q.tail = 1;

    int level = 0;
    int visited_count = 1;

    // 4. 层级遍历 Loop
    while (global_q.head < global_q.tail) {
        int q_size = global_q.tail - global_q.head;
        if (q_size == 0) break;

        int level_start = global_q.head;

        // 均匀划分任务
        int chunk = q_size / NUM_THREADS;
        int remainder = q_size % NUM_THREADS;

        int current_idx = level_start;

        // --- 启动 Worker 0, 1, 2 (子线程) ---
        for (int i = 0; i < NUM_THREADS - 1; i++) {
            args[i].thread_id = i;
            args[i].current_level = level;

            // 计算每个线程的起止点 (处理余数)
            int my_chunk = chunk + (i < remainder ? 1 : 0);
            args[i].q_start = current_idx;
            args[i].q_end = current_idx + my_chunk;
            current_idx += my_chunk;

            pthread_create(&threads[i], NULL, bfs_worker, &args[i]);
        }

        // --- 运行 Worker 3 (主线程) ---
        int last = NUM_THREADS - 1;
        args[last].thread_id = last;
        args[last].current_level = level;
        args[last].q_start = current_idx;
        args[last].q_end = global_q.tail; // 剩下的全包

        bfs_worker(&args[last]); // 主线程干活！

        // --- 等待子线程 ---
        for (int i = 0; i < NUM_THREADS - 1; i++) {
            pthread_join(threads[i], NULL);
        }

        // 5. 串行合并 (Serial Merge)
        // 这一步虽然是串行的，但在 4 线程规模下比复杂的无锁队列更高效且稳定
        global_q.head = global_q.tail;

        for (int i = 0; i < NUM_THREADS; i++) {
            for (int k = 0; k < args[i].next_count; k++) {
                global_q.data[global_q.tail++] = args[i].next_level_nodes[k];
                visited_count++;
            }
        }
        level++;
    }

    printf("[Done] BFS Traversal. Visited: %d\n", visited_count);

    // 简单的清理 (Benchmark结束直接退出OS会回收，这里象征性释放)
    free(graph);
    free(global_q.data);
    return 0;
}
