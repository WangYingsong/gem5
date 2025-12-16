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
#define DEFAULT_THREADS 4

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
int num_threads = DEFAULT_THREADS;

typedef struct
{
    int* data;
    int head;
    int tail;
    pthread_mutex_t lock;
} Queue;

Queue global_q;

void
init_queue(int capacity)
{
    global_q.data = (int*)malloc(capacity * sizeof(int));
    global_q.head = 0;
    global_q.tail = 0;
    pthread_mutex_init(&global_q.lock, NULL);
}

typedef struct
{
    int thread_id;
    int q_start;
    int q_end;
    int* next_level_nodes;
    int next_count;
    int current_level;
} thread_arg_t;

void*
bfs_worker(void* arg)
{
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int start = t_arg->q_start;
    int end = t_arg->q_end;
    int current_dist = t_arg->current_level;

    t_arg->next_count = 0;

    for (int i = start; i < end; i++) {
        int u_id = global_q.data[i];
        Node* u = &graph[u_id];

        for (int j = 0; j < u->num_edges; j++) {
            int v_id = u->edges[j];
            Node* v = &graph[v_id];

            if (__sync_bool_compare_and_swap(&v->visited, false, true)) {
                v->distance = current_dist + 1;
                t_arg->next_level_nodes[t_arg->next_count++] = v_id;
            }
        }
    }
    return NULL;
}

int
main(int argc, char* argv[])
{
    if (argc > 1) {
        int val = atoi(argv[1]);
        if (val < 100) {
            num_threads = val;
        } else {
            num_nodes = val;
            if (argc > 2) num_threads = atoi(argv[2]);
        }
    }
    if (num_threads < 1) num_threads = 1;

    printf("[Benchmark] Graph BFS (Low IPC)\n");
    printf("[Config] Nodes: %d (Avg Edges: %d)\n", num_nodes, AVG_EDGES);
    printf("[Config] Threads: %d\n", num_threads);

    graph = (Node*)malloc(num_nodes * sizeof(Node));
    srand(42);

    for (int i = 0; i < num_nodes; i++) {
        graph[i].id = i;
        graph[i].visited = false;
        graph[i].distance = -1;
        int edges = AVG_EDGES + (rand() % AVG_EDGES);
        graph[i].num_edges = edges;
        graph[i].edges = (int*)malloc(edges * sizeof(int));

        for (int j = 0; j < edges; j++) {
            graph[i].edges[j] = rand() % num_nodes;
        }
    }

    init_queue(num_nodes * 10);

    pthread_t* threads = (pthread_t*)malloc(
        (num_threads - 1) * sizeof(pthread_t));
    thread_arg_t* args = (thread_arg_t*)malloc(
        num_threads * sizeof(thread_arg_t));

    for (int i = 0; i < num_threads; i++) {
        args[i].next_level_nodes = (int*)malloc(num_nodes * sizeof(int));
    }

    graph[0].visited = true;
    graph[0].distance = 0;
    global_q.data[0] = 0;
    global_q.tail = 1;

    int level = 0;
    int visited_count = 1;

    while (global_q.head < global_q.tail) {
        int q_size = global_q.tail - global_q.head;
        if (q_size == 0) break;

        int level_start = global_q.head;
        int level_end = global_q.tail;
        int chunk = q_size / num_threads;
        if (chunk == 0) chunk = 1;

        int active_threads = (q_size < num_threads) ? 1 : num_threads;

        for (int i = 0; i < active_threads - 1; i++) {
            args[i].thread_id = i;
            args[i].current_level = level;
            args[i].q_start = level_start + i * chunk;
            args[i].q_end = level_start + (i + 1) * chunk;

            pthread_create(&threads[i], NULL, bfs_worker, &args[i]);
        }

        int last = active_threads - 1;
        args[last].thread_id = last;
        args[last].current_level = level;
        args[last].q_start = level_start + last * chunk;
        args[last].q_end = level_end;
        bfs_worker(&args[last]);

        for (int i = 0; i < active_threads - 1; i++) {
            pthread_join(threads[i], NULL);
        }

        global_q.head = level_end;

        for (int i = 0; i < active_threads; i++) {
            for (int k = 0; k < args[i].next_count; k++) {
                global_q.data[global_q.tail++] = args[i].next_level_nodes[k];
                visited_count++;
            }
        }
        level++;
    }

    printf("[SUCCESS] BFS Traversal Done. Visited %d nodes.\n", visited_count);

    return 0;
}
