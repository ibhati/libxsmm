/******************************************************************************
** Copyright (c) 2016-2017, Intel Corporation                                **
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
#include <libxsmm.h>
#include "libxsmm_main.h"

#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(push,target(LIBXSMM_OFFLOAD_TARGET))
#endif
#include <stdlib.h>
#include <string.h>
#if !defined(NDEBUG)
# include <stdio.h>
#endif
#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(pop)
#endif


LIBXSMM_API_DEFINITION libxsmm_dfsspmdm* libxsmm_dfsspmdm_create( const int M,   const int N,   const int K, 
                                                                  const int lda, const int ldb, const int ldc,
                                                                  const double alpha, const double beta,
                                                                  const double* a_dense )
{
  double* a_csr_values = 0;
  unsigned int* a_csr_rowptr = 0;
  unsigned int* a_csr_colidx = 0;
  libxsmm_gemm_descriptor* xgemm_desc = 0;
  libxsmm_dfsspmdm* new_handle = 0;
  int a_nnz;
  int i = 0;
  int j = 0;
  int n = 0;
  int vlen = 0;

  /* some checks... */
  assert( N%8 == 0 );
  assert( N >= 8 );
  assert( alpha == 1.0 );
  assert( beta == 1.0 );

  /* allocate handle */
  new_handle = (libxsmm_dfsspmdm*)malloc(sizeof(libxsmm_dfsspmdm));
  memset((void*)new_handle, 0, sizeof(libxsmm_dfsspmdm));

  /* init stuff of the handle */
  new_handle->N = N;
  new_handle->M = M;
  new_handle->K = K;
  new_handle->ldb = ldb;
  new_handle->ldc = ldc;
  
  /* get number of non-zeros */
  a_nnz = 0;
  for ( i = 0; i < M*K; ++i) {
    if (a_dense[i] != 0.0) {
      a_nnz++;
    }
  }

  /* allocate CSR structure */
  a_csr_values = (double*)malloc(a_nnz*sizeof(double));
  a_csr_rowptr = (unsigned int*)malloc((M+1)*sizeof(unsigned int));
  a_csr_colidx = (unsigned int*)malloc(a_nnz*sizeof(unsigned int));
    
  /* populate CSR structure */
  n = 0;
  for ( i = 0; i < M; i++ ) {
    a_csr_rowptr[i] = n;
    for ( j = 0; j < K; j++) {
      if ( a_dense[(i*lda)+j] != 0.0 ) {
        a_csr_values[n] = a_dense[(i*lda)+j];
        a_csr_colidx[n] = j;
        n++;
      }
    }
  }
  a_csr_rowptr[M] = a_nnz; 

  /* attempt to JIT a sparse_reg */
  vlen = 8;
  xgemm_desc = libxsmm_create_dgemm_descriptor('n', 'n', M, vlen, K, 0, ldb, ldc, alpha, beta, LIBXSMM_PREFETCH_NONE);
  new_handle->kernel = libxsmm_create_dcsr_reg( xgemm_desc, a_csr_rowptr, a_csr_colidx, a_csr_values ).dmm;

  /* continue with sparse A */
  if (new_handle->kernel != 0) {
    new_handle->N_chunksize = vlen;
  /* attempt to JIT dense kernel as sparse_reg failed */  
  } else {
    new_handle->kernel = libxsmm_dmmdispatch(N, M, K, &ldb, &K, &ldc, &alpha, &beta, 0, (const int*)LIBXSMM_PREFETCH_NONE);
    new_handle->N_chunksize = N;
    /* copy A over */
    new_handle->a_dense = (double*)libxsmm_aligned_malloc(M*K*sizeof(double), 64);
    for ( i = 0; i < M; i++ ) {
      for ( j = 0; j < K; j++) {
        new_handle->a_dense[(i*K)+j] = a_dense[(i*lda)+j];
      }
    }
  }

  /* free CSR */
  free( a_csr_values );
  free( a_csr_rowptr );
  free( a_csr_colidx );
  
  return new_handle;
}


LIBXSMM_API_DEFINITION void libxsmm_dfsspmdm_execute( const libxsmm_dfsspmdm* handle, const double* B, double* C )
{
  int i;

  assert( handle != 0 );

  if ( handle->a_dense == 0 ) {
    for ( i = 0; i < handle->N; i+=handle->N_chunksize ) {
      handle->kernel( handle->a_dense, B+i, C+i );
    }
  } else {
    for ( i = 0; i < handle->N; i+=handle->N_chunksize ) {
      handle->kernel( B+i, handle->a_dense, C+i );
    }
  }
}


LIBXSMM_API_DEFINITION void libxsmm_dfsspmdm_destroy( libxsmm_dfsspmdm* handle )
{
  assert( handle != 0 );

  if (handle->a_dense != 0) {
    libxsmm_free(handle->a_dense);
  } else {
    libxsmm_release_kernel((const void*)handle->kernel);
  }

  free(handle);
}

