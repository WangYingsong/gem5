// created by Wang Yingsong on 2025-12-12
// tests/spme_benchmark/matmul.c

#include <stdio.h>

#define N 64  // 矩阵大小 64x64

int A[N][N], B[N][N], C[N][N];

void init_matrix() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = 1;
            B[i][j] = 2;
            C[i][j] = 0;
        }
    }
}

int main() {
    printf("[MATMUL] Initializing %dx%d matrices...\n", N, N);
    init_matrix();

    printf("[MATMUL] Starting computation...\n");
    // 标准矩阵乘法 C = A * B
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    // 验证结果: 1 * 2 * N = 2N = 128
    int expected = 1 * 2 * N;
    if (C[0][0] == expected && C[N-1][N-1] == expected) {
        printf("[MATMUL] PASSED! Result C[0][0] = %d\n", C[0][0]);
    } else {
        printf("[MATMUL] FAILED! Expected %d, got %d\n", expected, C[0][0]);
    }

    return 0;
}
