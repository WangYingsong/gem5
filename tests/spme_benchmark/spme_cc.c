// created by Wang Yingsong on 2025-12-17
// tests/spme_benchmark/spme_cc.c
// =========================================================
// Proxy: BigDataBench Connected Components (MPI_Connect)
// 特征: Union-Find, 指针追逐, 随机内存访问
// =========================================================

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// 默认 100万节点 (4MB parent array) + 1000万条边
#define DEFAULT_NODES 1000000
#define DEFAULT_EDGES 10000000
#define NUM_THREADS 4
int num_nodes = DEFAULT_NODES;
int num_edges = DEFAULT_EDGES;
int *parent;
// 边结构体
typedef struct
{
    int u;
    int v;
} Edge;
Edge *edges;
// 查找操作 (带路径压缩)
int find_root(int i) {
    int root = i;
    while (root != parent[root]) {
        root = parent[root];
    }
    // 路径压缩
    while (i != root) {
        int next = parent[i];
        parent[i] = root;
        i = next;
    }
    return root;
}
// 合并操作 (Benign Race for Benchmark Pressure)
void union_sets(int i, int j) {
    int root_i = find_root(i);
    int root_j = find_root(j);
    if (root_i != root_j) {
        // 简化版无锁写入，制造内存竞争压力
        if (root_i < root_j) parent[root_j] = root_i;
        else parent[root_i] = root_j;
    }
}
typedef struct
{
    int start_edge;
    int end_edge;
} thread_arg_t;
void* worker(void* arg) {
    thread_arg_t* t = (thread_arg_t*)arg;
    for (int i = t->start_edge; i < t->end_edge; i++) {
        union_sets(edges[i].u, edges[i].v);
    }
    return NULL;
}
int main(int argc, char* argv[]) {
    if (argc > 1) num_nodes = atoi(argv[1]);
    // 边数默认是节点数的 10 倍
    num_edges = num_nodes * 10;
    printf("[Benchmark] CC (Master-Worker). Nodes: %d, Edges: %d\n",
           num_nodes, num_edges);
    parent = (int*)malloc(num_nodes * sizeof(int));
    edges = (Edge*)malloc(num_edges * sizeof(Edge));
    // 初始化
    for (int i = 0; i < num_nodes; i++) parent[i] = i;
    // 生成随机图
    srand(42);
    for (int i = 0; i < num_edges; i++) {
        edges[i].u = rand() % num_nodes;
        edges[i].v = rand() % num_nodes;
    }
    // 修改点 1: 线程数组减 1
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];
    int chunk = num_edges / NUM_THREADS;
    // 修改点 2: 启动 NUM_THREADS - 1 个子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].start_edge = i * chunk;
        args[i].end_edge = (i + 1) * chunk;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    // 修改点 3: 主线程处理最后一块 (Worker 3)
    int last = NUM_THREADS - 1;
    args[last].start_edge = last * chunk;
    args[last].end_edge = num_edges; // 包含剩余所有
    worker(&args[last]);
    // 修改点 4: 等待子线程
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }
    // 验证
    int components = 0;
    for (int i = 0; i < num_nodes; i++) {
        if (parent[i] == i) components++;
    }
    printf("[Done] Found %d components.\n", components);
    free(parent);
    free(edges);
    return 0;
}
