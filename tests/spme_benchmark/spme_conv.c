// created by Wang Yingsong on 2025-12-16
// tests/spme_benchmark/spme_conv.c
// =========================================================
// 2D 卷积 (Balanced Compute/Memory)
// =========================================================
/*
 * spme_conv.c (Smart Arg Version)
 * Purpose: 2D Convolution Benchmark (3x3 Kernel)
 * Logic: Apply 3x3 convolution kernel on 2D image
 * * Auto-Adaptation for fast_run.sh:
 * - If argv[1] is small (< 100), it's treated as Num_Threads.
 * - If argv[1] is large (>= 100), it's treated as Image Dimension.
*/

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_DIM 512
#define DEFAULT_THREADS 4
#define KERNEL_SIZE 3

int dim = DEFAULT_DIM;
int num_threads = DEFAULT_THREADS;

double *input_img;
double *output_img;
double kernel[3][3] = {
    {-1, -1, -1},
    {-1,  8, -1},
    {-1, -1, -1}
};

typedef struct
{
    int thread_id;
    int start_row;
    int end_row;
} thread_arg_t;

void*
conv_worker(void* arg)
{
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int start = t_arg->start_row;
    int end = t_arg->end_row;
    int k_offset = KERNEL_SIZE / 2;

    if (start < k_offset) start = k_offset;
    if (end > dim - k_offset) end = dim - k_offset;

    for (int i = start; i < end; i++) {
        for (int j = k_offset; j < dim - k_offset; j++) {
            double sum = 0.0;
            for (int ki = 0; ki < KERNEL_SIZE; ki++) {
                for (int kj = 0; kj < KERNEL_SIZE; kj++) {
                    int ii = i + ki - k_offset;
                    int jj = j + kj - k_offset;
                    sum += input_img[ii * dim + jj] * kernel[ki][kj];
                }
            }
            output_img[i * dim + j] = sum;
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
            dim = val;
            if (argc > 2) num_threads = atoi(argv[2]);
        }
    }
    if (num_threads < 1) num_threads = 1;

    printf("[Benchmark] 2D Convolution (3x3 Kernel)\n");
    printf("[Config] Image Size: %dx%d\n", dim, dim);
    printf("[Config] Threads: %d\n", num_threads);

    input_img = (double*)malloc(dim * dim * sizeof(double));
    output_img = (double*)malloc(dim * dim * sizeof(double));

    for (int i = 0; i < dim * dim; i++) {
        input_img[i] = (double)(i % 255);
    }

    pthread_t* threads = (pthread_t*)malloc(
        (num_threads - 1) * sizeof(pthread_t));
    thread_arg_t* args = (thread_arg_t*)malloc(
        num_threads * sizeof(thread_arg_t));

    int chunk = dim / num_threads;

    for (int i = 0; i < num_threads - 1; i++) {
        args[i].thread_id = i;
        args[i].start_row = i * chunk;
        args[i].end_row = (i + 1) * chunk;
        if (pthread_create(&threads[i], NULL, conv_worker,
                           &args[i])) {
             return 1;
        }
    }

    int last = num_threads - 1;
    args[last].thread_id = last;
    args[last].start_row = last * chunk;
    args[last].end_row = dim;

    printf("Main Thread working...\n");
    conv_worker(&args[last]);

    for (int i = 0; i < num_threads - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[SUCCESS] Done. Pixel[100][100] = %.1f\n",
           output_img[100 * dim + 100]);

    free(input_img); free(output_img);
    free(threads); free(args);
    return 0;
}
