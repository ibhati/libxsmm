/******************************************************************************
** Copyright (c) 2015, Intel Corporation                                     **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Alexander Heinecke (Intel Corp.)
******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#include <immintrin.h>

/* This will be our future incldue */
/*#include <libxsmm_generator.h>*/
/*@TODO remove:*/
#include <generator_extern_typedefs.h>
#include <generator_dense.h>
#include <generator_sparse.h>

#define REPS 10000

void print_help() {
  printf("Usage (dense*dense=dense):\n");
  printf("    M\n");
  printf("    N\n");
  printf("    K\n");
  printf("    LDA\n");
  printf("    LDB\n");
  printf("    LDC\n");
  printf("    alpha: -1 or 1\n");
  printf("    beta: 0 or 1\n");
  printf("    0: unaligned A, otherwise aligned\n");
  printf("    0: unaligned C, otherwise aligned\n");
  printf("    ARCH: snb, hsw, knl, skx\n");
  printf("    PREFETCH: nopf (none), pfsigonly, BL2viaC, AL2, curAL2, AL2jpst, AL2_BL2viaC, curAL2_BL2viaC, AL2jpst_BL2viaC\n");
  printf("    PRECISION: SP, DP\n");
  printf("\n\n\n\n");
}

inline double sec(struct timeval start, struct timeval end) {
  return ((double)(((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)))) / 1.0e6;
}

void init_double( double*                         io_a,
                  double*                         io_b,
                  double*                         io_c,
                  double*                         io_c_gold,
                  const libxsmm_xgemm_descriptor* i_xgemm_desc ) {
  unsigned int l_i, l_j;

  // touch A
  for ( l_i = 0; l_i < i_xgemm_desc->lda; l_i++) {
    for ( l_j = 0; l_j < i_xgemm_desc->k; l_j++) {
      io_a[(l_j * i_xgemm_desc->lda) + l_i] = (double)drand48();
    }
  }
  // touch B
  for ( l_i = 0; l_i < i_xgemm_desc->ldb; l_i++ ) {
    for ( l_j = 0; l_j < i_xgemm_desc->n; l_j++ ) {
      io_b[(l_j * i_xgemm_desc->ldb) + l_i] = (double)drand48();
    }
  }
  // touch C
  for ( l_i = 0; l_i < i_xgemm_desc->ldc; l_i++) {
    for ( l_j = 0; l_j < i_xgemm_desc->n; l_j++) {
      io_c[(l_j * i_xgemm_desc->ldc) + l_i] = (double)0.0;
      io_c_gold[(l_j * i_xgemm_desc->ldc) + l_i] = (double)0.0;
    }
  }
}

void init_float( float*                          io_a,
                 float*                          io_b,
                 float*                          io_c,
                 float*                          io_c_gold,
                 const libxsmm_xgemm_descriptor* i_xgemm_desc ) {
  unsigned int l_i, l_j;

  // touch A
  for ( l_i = 0; l_i < i_xgemm_desc->lda; l_i++) {
    for ( l_j = 0; l_j < i_xgemm_desc->k; l_j++) {
      io_a[(l_j * i_xgemm_desc->lda) + l_i] = (float)drand48();
    }
  }
  // touch B
  for ( l_i = 0; l_i < i_xgemm_desc->ldb; l_i++ ) {
    for ( l_j = 0; l_j < i_xgemm_desc->n; l_j++ ) {
      io_b[(l_j * i_xgemm_desc->ldb) + l_i] = (float)drand48();
    }
  }
  // touch C
  for ( l_i = 0; l_i < i_xgemm_desc->ldc; l_i++) {
    for ( l_j = 0; l_j < i_xgemm_desc->n; l_j++) {
      io_c[(l_j * i_xgemm_desc->ldc) + l_i] = (float)0.0;
      io_c_gold[(l_j * i_xgemm_desc->ldc) + l_i] = (float)0.0;
    }
  }
}

void run_gold_double( const double*                   i_a,
                      const double*                   i_b,
                      double*                         o_c,
                      const libxsmm_xgemm_descriptor* i_xgemm_desc ) {
  unsigned int l_m, l_n, l_k, l_t;
  double l_runtime = 0.0;

  struct timeval l_start, l_end;
  gettimeofday(&l_start, NULL);

  for ( l_t = 0; l_t < REPS; l_t++  ) {
    for ( l_n = 0; l_n < i_xgemm_desc->n; l_n++  ) {
      for ( l_k = 0; l_k < i_xgemm_desc->k; l_k++  ) {
        for ( l_m = 0; l_m < i_xgemm_desc->m; l_m++ ) {
          o_c[(l_n * i_xgemm_desc->ldc) + l_m] += i_a[(l_k * i_xgemm_desc->lda) + l_m] * i_b[(l_n * i_xgemm_desc->ldb) + l_k];
        }
      }
    }
  }

  gettimeofday(&l_end, NULL);  
  l_runtime = sec(l_start, l_end);

  printf("%fs for C\n", l_runtime);
  printf("%f GFLOPS for C\n", ((double)((double)REPS * (double)i_xgemm_desc->m * (double)i_xgemm_desc->n * (double)i_xgemm_desc->k) * 2.0) / (l_runtime * 1.0e9));
}

void run_gold_float( const float*                   i_a,
                     const float*                   i_b,
                     float*                         o_c,
                     const libxsmm_xgemm_descriptor* i_xgemm_desc ) {
  unsigned int l_m, l_n, l_k, l_t;
  double l_runtime = 0.0;

  struct timeval l_start, l_end;
  gettimeofday(&l_start, NULL);

  for ( l_t = 0; l_t < REPS; l_t++  ) {
    for ( l_n = 0; l_n < i_xgemm_desc->n; l_n++  ) {
      for ( l_k = 0; l_k < i_xgemm_desc->k; l_k++  ) {
        for ( l_m = 0; l_m < i_xgemm_desc->m; l_m++ ) {
          o_c[(l_n * i_xgemm_desc->ldc) + l_m] += i_a[(l_k * i_xgemm_desc->lda) + l_m] * i_b[(l_n * i_xgemm_desc->ldb) + l_k];
        }
      }
    }
  }

  gettimeofday(&l_end, NULL);  
  l_runtime = sec(l_start, l_end);

  printf("%fs for C\n", l_runtime);
  printf("%f GFLOPS for C\n", ((double)((double)REPS * (double)i_xgemm_desc->m * (double)i_xgemm_desc->n * (double)i_xgemm_desc->k) * 2.0) / (l_runtime * 1.0e9));
}

void max_error_double( const double*                   i_c,
                       const double*                   i_c_gold,
                       const libxsmm_xgemm_descriptor* i_xgemm_desc ) {
  unsigned int l_i, l_j;
  double l_max_error = 0.0;

  for ( l_i = 0; l_i < i_xgemm_desc->m; l_i++) {
    for ( l_j = 0; l_j < i_xgemm_desc->n; l_j++) {
#if 0
      printf("Entries in row %i, column %i, gold: %f, jit: %f\n", l_i+1, l_j+1, i_c_gold[(l_j*i_xgemm_desc->ldc)+l_i], i_c[(l_j*i_xgemm_desc->ldc)+l_i]);
#endif
      if (l_max_error < fabs( i_c_gold[(l_j * i_xgemm_desc->ldc) + l_i] - i_c[(l_j * i_xgemm_desc->ldc) + l_i]))
        l_max_error = fabs( i_c_gold[(l_j * i_xgemm_desc->ldc) + l_i] - i_c[(l_j * i_xgemm_desc->ldc) + l_i]);
    }
  }

  printf("max. error: %f\n", l_max_error);
}

void max_error_float( const float*                    i_c,
                      const float*                    i_c_gold,
                      const libxsmm_xgemm_descriptor* i_xgemm_desc ) {
  unsigned int l_i, l_j;
  double l_max_error = 0.0;

  for ( l_i = 0; l_i < i_xgemm_desc->m; l_i++) {
    for ( l_j = 0; l_j < i_xgemm_desc->n; l_j++) {
#if 0
      printf("Entries in row %i, column %i, gold: %f, jit: %f\n", l_i+1, l_j+1, i_c_gold[(l_j*i_xgemm_desc->ldc)+l_i], i_c[(l_j*i_xgemm_desc->ldc)+l_i]);
#endif
      if (l_max_error < fabs( (double)i_c_gold[(l_j * i_xgemm_desc->ldc) + l_i] - (double)i_c[(l_j * i_xgemm_desc->ldc) + l_i]))
        l_max_error = fabs( (double)i_c_gold[(l_j * i_xgemm_desc->ldc) + l_i] - (double)i_c[(l_j * i_xgemm_desc->ldc) + l_i]);
    }
  }

  printf("max. error: %f\n", l_max_error);
}

int main(int argc, char* argv []) {
  /* check argument count for a valid range */
  if ( argc != 14 ) {
    print_help();
    return -1;
  }

  char* l_arch;
  char* l_prefetch;
  char* l_precision;
  int l_m = 0;
  int l_n = 0;
  int l_k = 0;
  int l_lda = 0;
  int l_ldb = 0;
  int l_ldc = 0;
  int l_aligned_a = 0;
  int l_aligned_c = 0;
  int l_alpha = 0;
  int l_beta = 0;
  int l_single_precision = 0;
    
  /* xgemm sizes */
  l_m = atoi(argv[1]);
  l_n = atoi(argv[2]);
  l_k = atoi(argv[3]);
  l_lda = atoi(argv[4]);
  l_ldb = atoi(argv[5]);
  l_ldc = atoi(argv[6]);

  /* some sugar */
  l_alpha = atoi(argv[7]);
  l_beta = atoi(argv[8]);
  l_aligned_a = atoi(argv[9]);
  l_aligned_c = atoi(argv[10]);

  /* arch specific stuff */
  l_arch = argv[11];
  l_prefetch = argv[12];
  l_precision = argv[13];

  /* check value of prefetch flag */
  if ( (strcmp(l_prefetch, "nopf") != 0 )           &&
       (strcmp(l_prefetch, "pfsigonly") != 0 )      &&
       (strcmp(l_prefetch, "BL2viaC") != 0 )        &&
       (strcmp(l_prefetch, "curAL2") != 0 )         &&
       (strcmp(l_prefetch, "curAL2_BL2viaC") != 0 ) &&
       (strcmp(l_prefetch, "AL2") != 0 )            &&
       (strcmp(l_prefetch, "AL2_BL2viaC") != 0 )    &&
       (strcmp(l_prefetch, "AL2jpst") !=0 )         &&
       (strcmp(l_prefetch, "AL2jpst_BL2viaC") !=0 )    ) {
    print_help();
    return -1;
  }  

  /* check value of arch flag */
  if ( (strcmp(l_arch, "snb") != 0)    && 
       (strcmp(l_arch, "hsw") != 0)    && 
       (strcmp(l_arch, "knl") != 0)    && 
       (strcmp(l_arch, "skx") != 0)       ) {
    print_help();
    return -1;
  }

  /* check and evaluate precison flag */
  if ( strcmp(l_precision, "SP") == 0 ) {
    l_single_precision = 1;
  } else if ( strcmp(l_precision, "DP") == 0 ) {
    l_single_precision = 0;
  } else {
    print_help();
    return -1;
  }

  /* check alpha */
  if ((l_alpha != -1) && (l_alpha != 1)) {
    print_help();
    return -1;
  }

  /* check beta */
  if ((l_beta != 0) && (l_beta != 1)) {
    print_help();
    return -1;
  }

  libxsmm_xgemm_descriptor l_xgemm_desc;
  if ( l_m < 0 ) { l_xgemm_desc.m = 0; } else {  l_xgemm_desc.m = l_m; }
  if ( l_n < 0 ) { l_xgemm_desc.n = 0; } else {  l_xgemm_desc.n = l_n; }
  if ( l_k < 0 ) { l_xgemm_desc.k = 0; } else {  l_xgemm_desc.k = l_k; }
  if ( l_lda < 0 ) { l_xgemm_desc.lda = 0; } else {  l_xgemm_desc.lda = l_lda; }
  if ( l_ldb < 0 ) { l_xgemm_desc.ldb = 0; } else {  l_xgemm_desc.ldb = l_ldb; }
  if ( l_ldc < 0 ) { l_xgemm_desc.ldc = 0; } else {  l_xgemm_desc.ldc = l_ldc; }
  l_xgemm_desc.alpha = l_alpha;
  l_xgemm_desc.beta = l_beta;
  l_xgemm_desc.trans_a = 'n';
  l_xgemm_desc.trans_b = 'n';
  if (l_aligned_a == 0) {
    l_xgemm_desc.aligned_a = 0;
  } else {
    l_xgemm_desc.aligned_a = 1;
  }
  if (l_aligned_c == 0) {
    l_xgemm_desc.aligned_c = 0;
  } else {
    l_xgemm_desc.aligned_c = 1;
  }
  l_xgemm_desc.single_precision = l_single_precision;
  strcpy ( l_xgemm_desc.prefetch, l_prefetch );

  /* init data structures */
  double* l_a_d; 
  double* l_b_d; 
  double* l_c_d; 
  double* l_c_gold_d;
  float* l_a_f;
  float* l_b_f; 
  float* l_c_f; 
  float* l_c_gold_f;

  if ( l_xgemm_desc.single_precision == 0 ) {
    l_a_d = (double*)_mm_malloc(l_xgemm_desc.lda * l_xgemm_desc.k * sizeof(double), 64);
    l_b_d = (double*)_mm_malloc(l_xgemm_desc.ldb * l_xgemm_desc.n * sizeof(double), 64);
    l_c_d = (double*)_mm_malloc(l_xgemm_desc.ldc * l_xgemm_desc.n * sizeof(double), 64);
    l_c_gold_d = (double*)_mm_malloc(l_xgemm_desc.ldc * l_xgemm_desc.n * sizeof(double), 64);
    init_double(l_a_d, l_b_d, l_c_d, l_c_gold_d, &l_xgemm_desc);
  } else {
    l_a_f = (float*)_mm_malloc(l_xgemm_desc.lda * l_xgemm_desc.k * sizeof(float), 64);
    l_b_f = (float*)_mm_malloc(l_xgemm_desc.ldb * l_xgemm_desc.n * sizeof(float), 64);
    l_c_f = (float*)_mm_malloc(l_xgemm_desc.ldc * l_xgemm_desc.n * sizeof(float), 64);
    l_c_gold_f = (float*)_mm_malloc(l_xgemm_desc.ldc * l_xgemm_desc.n * sizeof(float), 64);
    init_float(l_a_f, l_b_f, l_c_f, l_c_gold_f, &l_xgemm_desc);
  }

  /* print some output... */
  printf("------------------------------------------------\n");
  printf("RUNNING (%ix%i) X (%ix%i) = (%ix%i)", l_xgemm_desc.m, l_xgemm_desc.k, l_xgemm_desc.k, l_xgemm_desc.n, l_xgemm_desc.m, l_xgemm_desc.n);
  if ( l_xgemm_desc.single_precision == 0 ) {
    printf(", DP\n");
  } else {
    printf(", SP\n");
  }
  printf("------------------------------------------------\n");

  /* run C */
  if ( l_xgemm_desc.single_precision == 0 ) {
    run_gold_double( l_a_d, l_b_d, l_c_gold_d, &l_xgemm_desc );
  } else {
    run_gold_float( l_a_f, l_b_f, l_c_gold_f, &l_xgemm_desc );
  }  

  /* run jit */
  if ( l_xgemm_desc.single_precision == 0 ) {
    run_gold_double( l_a_d, l_b_d, l_c_d, &l_xgemm_desc );
  } else {
    run_gold_float( l_a_f, l_b_f, l_c_f, &l_xgemm_desc );
  }  
  
  /* test result */
  if ( l_xgemm_desc.single_precision == 0 ) {
    max_error_double( l_c_d, l_c_gold_d, &l_xgemm_desc );
  } else {
    max_error_float( l_c_f, l_c_gold_f, &l_xgemm_desc );
  }

  /* free */
  if ( l_xgemm_desc.single_precision == 0 ) {
    _mm_free(l_a_d);
    _mm_free(l_b_d);
    _mm_free(l_c_d);
    _mm_free(l_c_gold_d);
  } else {
    _mm_free(l_a_f);
    _mm_free(l_b_f);
    _mm_free(l_c_f);
    _mm_free(l_c_gold_f);
  }

  printf("------------------------------------------------\n");
  return 0;
}

