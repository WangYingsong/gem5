// created by Wang Yingsong on 2025-12-16
// tests/spme_benchmark/spme_montecarlo.c
// =========================================================
// 蒙特卡洛 Pi 近似计算 (Compute Bound / High IPC)
// =========================================================
/*
 * spme_montecarlo.c (Smart Arg Version)
 * Purpose: Monte Carlo Pi Approximation (Compute Bound / High IPC)
 * Logic: Estimate Pi using random sampling
 * * Auto-Adaptation for fast_run.sh:
 * - If argv[1] is small (< 100), it's treated as Num_Threads.
 * - If argv[1] is large (>= 100), it's treated as Total Iterations.
*/

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_ITERATIONS 10000000
#define DEFAULT_THREADS 4

long long total_iterations = DEFAULT_ITERATIONS;
int num_threads = DEFAULT_THREADS;

typedef struct
{
    long long hits;
    long long iterations;
    int thread_id;
    unsigned int seed;
    char padding[48]; // Padding to avoid false sharing
} thread_result_t;

thread_result_t* results;

// 快速伪随机数生成器
static inline unsigned int
fast_rand(unsigned int* seed)
{
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

void*
mc_worker(void* arg)
{
    thread_result_t* res = (thread_result_t*)arg;
    long long local_hits = 0;
    long long loops = res->iterations;
    unsigned int seed = res->seed;

    for (long long i = 0; i < loops; i++) {
        unsigned int rx = fast_rand(&seed);
        unsigned int ry = fast_rand(&seed);

        double x = (double)rx / (double)0x7fffffff;
        double y = (double)ry / (double)0x7fffffff;

        if ((x * x + y * y) <= 1.0) {
            local_hits++;
        }
    }

    res->hits = local_hits;
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
            total_iterations = val;
            if (argc > 2) num_threads = atoi(argv[2]);
        }
    }
    if (num_threads < 1) num_threads = 1;

    printf("[Benchmark] Monte Carlo Pi (High IPC)\n");
    printf("[Config] Total Iterations: %lld\n", total_iterations);
    printf("[Config] Threads: %d\n", num_threads);

    pthread_t* threads = (pthread_t*)malloc(
        (num_threads - 1) * sizeof(pthread_t));
    results = (thread_result_t*)malloc(
        num_threads * sizeof(thread_result_t));

    long long iter_per_thread = total_iterations / num_threads;

    for (int i = 0; i < num_threads; i++) {
        results[i].thread_id = i;
        results[i].iterations = iter_per_thread;
        results[i].hits = 0;
        results[i].seed = 12345 + i * 997;
    }

    for (int i = 0; i < num_threads - 1; i++) {
        if (pthread_create(&threads[i], NULL, mc_worker,
                           &results[i])) {
            perror("Thread creation failed");
            return 1;
        }
    }

    printf("Main Thread working...\n");
    mc_worker(&results[num_threads - 1]);

    for (int i = 0; i < num_threads - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    long long total_hits = 0;
    for (int i = 0; i < num_threads; i++) {
        total_hits += results[i].hits;
    }

    double pi = 4.0 * (double)total_hits / (double)total_iterations;
    printf("[SUCCESS] Estimated Pi = %f\n", pi);

    free(threads);
    free(results);
    return 0;
}
