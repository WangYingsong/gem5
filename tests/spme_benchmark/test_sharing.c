// created by Wang Yingsong on 2025-12-12
// tests/spme_benchmark/spme_micro.c

#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 2
#define ITERATIONS 10000

volatile int shared_counter = 0;
pthread_mutex_t lock;

void*
worker(void* arg)
{
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&lock);
        shared_counter++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int
main()
{
    pthread_t threads[NUM_THREADS];
    pthread_mutex_init(&lock, NULL);

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    if (shared_counter == NUM_THREADS * ITERATIONS) {
        printf("[SHARING] PASSED\n");
    } else {
        printf("FAILED\n");
    }
    return 0;
}
