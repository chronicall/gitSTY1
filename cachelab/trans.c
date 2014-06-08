/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 * Sandra Ros Hrefnu Jonsdottir - sandrarj13@ru.is
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, ii, jj, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;

    if(M == 32 && N == 32)
    {
        for(j = 0; j < N; j += 8)
        {
            for(i = 0; i < M; i += 8)
            {
                for(ii = i; ii < i + 8; ii++)
                {
                    for(jj = j; jj < j + 8; jj += 8)
                    {
                        tmp0 = A[ii][jj + 0];
                        tmp1 = A[ii][jj + 1];
                        tmp2 = A[ii][jj + 2];
                        tmp3 = A[ii][jj + 3];
                        tmp4 = A[ii][jj + 4];
                        tmp5 = A[ii][jj + 5];
                        tmp6 = A[ii][jj + 6];
                        tmp7 = A[ii][jj + 7];
                        B[jj + 0][ii] = tmp0;
                        B[jj + 1][ii] = tmp1;
                        B[jj + 2][ii] = tmp2;
                        B[jj + 3][ii] = tmp3;
                        B[jj + 4][ii] = tmp4;
                        B[jj + 5][ii] = tmp5;
                        B[jj + 6][ii] = tmp6;
                        B[jj + 7][ii] = tmp7;
                    }
                }
            }
        }
    }
    if(M == 64 && N == 64)
    {
        for(j = 0; j < N; j += 4)
        {
            for(i = 0; i < M; i += 4)
            {
                for(jj = j; jj < j + 4; jj += 4)
                {
                    for(ii = i; ii < i + 4; ii++)
                    {
                        tmp0 = A[ii][jj + 0];
                        tmp1 = A[ii][jj + 1];
                        tmp2 = A[ii][jj + 2];
                        tmp3 = A[ii][jj + 3];
                        B[jj + 0][ii] = tmp0;
                        B[jj + 1][ii] = tmp1;
                        B[jj + 2][ii] = tmp2;
                        B[jj + 3][ii] = tmp3;
                    }
                }
            }
        }
    }
    if(M == 61 && N == 67)
    {
        for (i = 0; i < N; i++) {
            for (j = 0; j < M; j++) {
                B[j][i] = A[i][j];
            }
        }    
    }
}

/*
 *
 */
char transpose_64M_desc[] = "64x64 blocking";
void transpose_64M(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, ii, jj, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;

    for(j = 0; j < N; j += 8)
    {
        for(i = 0; i < M; i += 8)
        {
            for(jj = j; jj < j + 8; jj += 4)
            {
                for(ii = i; ii < i + 4; ii++)
                {
                    tmp0 = A[ii][jj + 0];
                    tmp1 = A[ii][jj + 1];
                    tmp2 = A[ii][jj + 2];
                    tmp3 = A[ii][jj + 3];
                    B[jj + 0][ii] = tmp0;
                    B[jj + 1][ii] = tmp1;
                    B[jj + 2][ii] = tmp2;
                    B[jj + 3][ii] = tmp3;
                }
                for(ii = i + 4; ii < i + 8; ii++)
                {
                    tmp4 = A[ii][jj + 4];
                    tmp5 = A[ii][jj + 5];
                    tmp6 = A[ii][jj + 6];
                    tmp7 = A[ii][jj + 7];
                    B[jj + 4][ii] = tmp4;
                    B[jj + 5][ii] = tmp5;
                    B[jj + 6][ii] = tmp6;
                    B[jj + 7][ii] = tmp7;
                }
            }
        }
    }
}

/* 
 * transpose_blocking - A transpose function that uses blocking.
 */
char transpose_blocking_desc[] = "Transpose with blocking";
void transpose_blocking(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, ii, jj;

    for(i = 0; i < N; i += 4)
    {
        for(j = 0; j < M; j += 4)
        {
            for(ii = i; ii < i + 4; ii++)
            {
                for(jj = j; jj < j + 4; jj++)
                {
                    B[jj][ii] = A[ii][jj];
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 


    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 
    registerTransFunction(transpose_blocking, transpose_blocking_desc); 
    registerTransFunction(transpose_64M, transpose_64M_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

