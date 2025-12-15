// created by Wang Yingsong on 2025-12-15
// tests/spme_benchmark/spme_matrix.c
/*
 * spme_matrix.c
 * Purpose: Demonstrate the benefit of Large LLC and Aggressive O3 Cores.
 * Matrix Size: 384 x 384
 * Memory Footprint: ~3.4 MB
 * - Will thrash Standard L2 (2MB)
 * - Will fit comfortably in Conventional LLC (8MB)
 */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Matrix Dimension N=384
// Total Memory = 3 * 384 * 384 * 8 bytes approx 3.37 MB
#define N 384
#define NUM_THREADS 4

// Use static global arrays to ensure continuous memory layout
// This helps hardware prefetchers work efficiently
double A[N][N];
double B[N][N];
double C[N][N];

typedef struct
{
    int thread_id;
    int start_row;
    int end_row;
} thread_arg_t;

// Initialize Matrix
void
init_matrix()
{
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = (double)(i + j) * 0.001;
            B[i][j] = (double)(i - j) * 0.001;
            C[i][j] = 0.0;
        }
    }
}

// Thread Worker Function
void*
matrix_mul_worker(void* arg)
{
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int start = t_arg->start_row;
    int end = t_arg->end_row;

    // Standard Triple Loop
    // Conventional O3 core (4-wide) executes this efficiently
    for (int i = start; i < end; i++) {
        for (int k = 0; k < N; k++) {
            double r = A[i][k];
            // Inner loop: sequential memory access, cache friendly
            for (int j = 0; j < N; j++) {
                C[i][j] += r * B[k][j];
            }
        }
    }
    return NULL;
}

int
main()
{
    printf("[Benchmark] Matrix Multiplication (%dx%d)\n", N, N);
    printf("[Info] Estimated Memory Footprint: ~3.4 MB\n");

    init_matrix();

    // Create 3 extra threads, main thread works as the 4th
    pthread_t threads[NUM_THREADS - 1];
    thread_arg_t args[NUM_THREADS];

    // Task Distribution
    int rows_per_thread = N / NUM_THREADS;

    // Create (NUM_THREADS - 1) worker threads
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        args[i].thread_id = i;
        args[i].start_row = i * rows_per_thread;
        // Handle remainder for the last logical block
        if (i == NUM_THREADS - 1) {
             args[i].end_row = N;
        } else {
             args[i].end_row = (i + 1) * rows_per_thread;
        }

        if (pthread_create(&threads[i], NULL, matrix_mul_worker, &args[i])) {
            fprintf(stderr, "Error creating thread %d\n", i);
            return 1;
        }
    }

    // Main thread acts as the last worker
    int last_id = NUM_THREADS - 1;
    args[last_id].thread_id = last_id;
    args[last_id].start_row = last_id * rows_per_thread;
    args[last_id].end_row = N;

    printf("Main Thread joining work as Core %d...\n", last_id);
    matrix_mul_worker(&args[last_id]);

    // Wait for others
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    // Verify
    printf("[SUCCESS] Calculation Done. C[0][0] = %f\n", C[0][0]);
    return 0;
}
