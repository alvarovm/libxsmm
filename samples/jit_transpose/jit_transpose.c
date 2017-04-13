/******************************************************************************
** Copyright (c) 2017, Intel Corporation                                     **
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
/* Alexander Heinecke, Greg Henry (Intel Corp.)
******************************************************************************/
#include <libxsmm.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#if defined(_OPENMP)
# include <omp.h>
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
/* note: later on, this leads to (correct but) different than expected norm-values */
# define drand48() ((double)rand() / RAND_MAX)
# define srand48 srand
#endif

#ifndef MAX
#define MAX(x,y) ((x)<(y)?(y):(x))
#endif

void dfill_matrix ( double *matrix, int ld, int m, int n )
{
  extern double drand48();
  int i, j;
  double dtmp;

  if ( ld < m )
  {
     fprintf(stderr,"Error is dfill_matrix: ld=%d m=%d mismatched!\n",ld,m);
     exit(-1);
  }
  for ( j = 1 ; j <= n ; j++ )
  {
     /* Fill through the leading dimension */
     for ( i = 1; i <= ld; i++ )
     {
        dtmp = 1.0 - 2.0*drand48();
        matrix [ (j-1)*ld + (i-1) ] = dtmp;
     }
  }
}

void sfill_matrix ( float *matrix, int ld, int m, int n )
{
  extern double drand48();
  int i, j;
  double dtmp;

  if ( ld < m )
  {
     fprintf(stderr,"Error is sfill_matrix: ld=%d m=%d mismatched!\n",ld,m);
     exit(-1);
  }
  for ( j = 1 ; j <= n ; j++ )
  {
     /* Fill through the leading dimension */
     for ( i = 1; i <= ld; i++ )
     {
        dtmp = 1.0 - 2.0*drand48();
        matrix [ (j-1)*ld + (i-1) ] = (float) dtmp;
     }
  }
}

double residual_stranspose ( float *A, int lda, int m, int n, float *out,
                             int ld_out, int *nerrs )
{
  int i, j;
  double dtmp, derror;

  *nerrs = 0;
  derror = 0.0;
  for ( j = 1 ; j <= n ; j++ )
  {
     for ( i = 1 ; i <= m ; i++ )
     {
         dtmp = A[ (j-1)*lda + (i-1) ] - out [ (i-1)*ld_out + (j-1) ];
         if ( dtmp < 0.0 ) dtmp = -dtmp;
         if ( dtmp > 0.0 )
         {
            *nerrs = *nerrs + 1;
            if ( *nerrs < 10 ) printf("Err #%d: A(%d,%d)=%g B(%d,%d)=%g Diff=%g\n",*nerrs,i,j,A[(j-1)*lda+(i-1)],j,i,out[(i-1)*ld_out+(j-1)],dtmp);
         }
         derror += (double) dtmp;
     }
  }
  return ( derror );
}

double residual_dtranspose ( double *A, int lda, int m, int n, double *out,
                             int ld_out, int *nerrs )
{
  int i, j;
  double dtmp, derror;

  *nerrs = 0;
  derror = 0.0;
  for ( j = 1 ; j <= n ; j++ )
  {
     for ( i = 1 ; i <= m ; i++ )
     {
         dtmp = A[ (j-1)*lda + (i-1) ] - out [ (i-1)*ld_out + (j-1) ];
         if ( dtmp < 0.0 ) dtmp = -dtmp;
         if ( dtmp > 0.0 ) *nerrs = *nerrs + 1;
         derror += dtmp;
     }
  }
  return ( derror );
}

/* Comment 1 of the following lines to compare to an ass. code byte-for-byte */
/* #define COMPARE_TO_A_R32_ASSEMBLY_CODE */
/* #define COMPARE_TO_A_R64_ASSEMBLY_CODE */

#if defined(COMPARE_TO_A_R32_ASSEMBLY_CODE) || defined(COMPARE_TO_A_R64_ASSEMBLY_CODE)
  #ifndef COMPARE_TO_AN_ASSEMBLY_CODE
    #define COMPARE_TO_AN_ASSEMBLY_CODE
  #endif
#endif
#if defined(COMPARE_TO_A_R32_ASSEMBLY_CODE) && defined(COMPARE_TO_A_R64_ASSEMBLY_CODE)
  #error Define a comparison to either R32 or R64 code, not both at once
#endif

int main(int argc, char* argv[])
{
  int m=16, n=16, ld_in=16, ld_out=16, nerrs;
  int i, nbest, istop;
  float  *sin, *sout;
  double *din, *dout, dtmp;
  unsigned char *cptr, *cptr2;
#ifdef COMPARE_TO_AN_ASSEMBLY_CODE
  extern void myro_();
#endif
  libxsmm_stransfunction skernel;
  libxsmm_dtransfunction dkernel;

  if ( argc <= 3 )
  {
     printf("\nUSAGE: %s m n ld_in\n",argv[0]);
     printf("Out-of-place transpose a mxn matrix of leading dimension ld_in\n");
     printf("Defaults: m=n=ld_in=16\n");
     printf("Note: ld_in is not needed for dispatching. Code works for any valid (>=m) ld_in\n");
     printf("Note: ld_out is NOT a real parameter. For now, we assume ld_out=n\n");
  }
  if ( argc > 1 ) m = atoi(argv[1]);
  if ( argc > 2 ) n = atoi(argv[2]);
  if ( argc > 3 ) ld_in = atoi(argv[3]);
  m = MAX(m,1);
  n = MAX(n,1);
  ld_in = MAX(ld_in,m);
  ld_out = n;

  printf("This is a tester for JIT transpose kernels! (m=%d n=%d ld_in=%d ld_out=%d)\n",m,n,ld_in,ld_out);

  /* test dispatch call */
  skernel = libxsmm_stransdispatch( m, n );
  dkernel = libxsmm_dtransdispatch( m, n );

  printf("address of F32 kernel: %lld\n", (size_t)skernel);
  printf("address of F64 kernel: %lld\n", (size_t)dkernel);

  cptr = (unsigned char *) dkernel;
  printf("First few bytes/opcodes: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",cptr[0],cptr[1],cptr[2],cptr[3],cptr[4],cptr[5]);
  printf("cptr[9:11]=0x%02x 0x%02x 0x%02x\n",cptr[9],cptr[10],cptr[11]);
  printf("cptr[12:14]=0x%02x 0x%02x 0x%02x\n",cptr[12],cptr[13],cptr[14]);

#ifdef COMPARE_TO_AN_ASSEMBLY_CODE
  cptr2 = (unsigned char *) &myro_;
  i = 0 ;
  nbest = 0;
  istop = 0;
  while ( istop == 0 )
  {
     if ( cptr[i] != cptr2[i] )
     {
        printf("Byte: %d=0x%x differs. We generated: 0x%02x. Should be: 0x%02x\n",
               i,i,cptr[i],cptr2[i]);
     } else {
        ++nbest;
     }
     if ( i >= 208 ) istop = 1;
     if ( i >= 2 )
     {
        if ( (cptr2[i]==0xc3) && (cptr2[i-1]==0x5b) && (cptr2[i-2]==0x5d) )
           istop = 1;
        //if ( i == 114 ) printf("cptr2=0x%02x 0x%02x 0x%02x istop=%d\n",cptr2[i],cptr2[i-1],cptr2[i-2],istop);
     }
     ++i;
  }
  printf("Bytes agree: %d\n",nbest);
#endif

  sin  = (float  *) malloc ( ld_in*n*sizeof(float) );
  din  = (double *) malloc ( ld_in*n*sizeof(double) );
  sout = (float  *) malloc ( ld_out*m*sizeof(float) );
  dout = (double *) malloc ( ld_out*m*sizeof(double) );

  /* Fill matrices with random data: */
  sfill_matrix ( sin, ld_in, m, n );
  dfill_matrix ( din, ld_in, m, n );
  sfill_matrix ( sout, ld_out, n, m );
  dfill_matrix ( dout, ld_out, n, m );

  if ( ld_out != n )
  {
     fprintf(stderr,"Final warning: This code only works for ld_out=n (n=%d,ld_out=%d)\n",n,ld_out);
     exit(-1);
  }

#ifdef COMPARE_TO_A_R64_ASSEMBLY_CODE
  printf("Calling myro_: \n");
  myro_ ( din, &ld_in, dout, &ld_out );
  dtmp = residual_dtranspose ( din, ld_in, m, n, dout, ld_out, &nerrs );
  printf("Myro_ R64 error: %g number of errors: %d\n",dtmp,nerrs);
  dfill_matrix ( dout, ld_out, n, m );
#endif
#ifdef COMPARE_TO_A_R32_ASSEMBLY_CODE
  printf("Calling myro_: \n");
  myro_ ( sin, &ld_in, sout, &ld_out );
  dtmp = residual_stranspose ( sin, ld_in, m, n, sout, ld_out, &nerrs );
  printf("Myro_ R32 error: %g number of errors: %d\n",dtmp,nerrs);
  sfill_matrix ( sout, ld_out, n, m );
#endif


  /* let's call */
#if 1
  printf("calling skernel\n");
  skernel( sin, &ld_in, sout, &ld_out );
  printf("calling dkernel\n");
  dkernel( din, &ld_in, dout, &ld_out );
#endif

  /* Did it transpose correctly? */
  dtmp = residual_stranspose ( sin, ld_in, m, n, sout, ld_out, &nerrs );
  printf("Single precision m=%d n=%d ld_in=%d ld_out=%d error: %g number of errors: %d",m,n,ld_in,ld_out,dtmp,nerrs);
  if ( nerrs > 0 ) printf(" ->FAILED at %dx%d real*4 case",m,n);
  printf("\n");

  dtmp = residual_dtranspose ( din, ld_in, m, n, dout, ld_out, &nerrs );
  printf("Double precision m=%d n=%d ld_in=%d ld_out=%d error: %g number of errors: %d\n",m,n,ld_in,ld_out,dtmp,nerrs);
  if ( nerrs > 0 ) printf(" ->FAILED at %dx%d real*8 case",m,n);
  printf("\n");

  free(dout);
  free(sout);
  free(din);
  free(sin);
  return 0;
}