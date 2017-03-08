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
/* Rajkishore Barik (Intel Corp.)
******************************************************************************/
int imgifm1, img, ofm1, ifm1, oj, ij, kj, ki, ifm2, ofm2, ifm1ofm1, ifh;
#ifndef INPUT_PADDING
int ii;
#endif
/* computing first logical thread */
const int ltid = tid - start_thread;
/* number of tasks that could be run in parallel */
const int work = handle->desc.N * handle->blocksifm;
/* compute chunck size */
const int chunksize = (work % handle->desc.threads == 0) ? (work / handle->desc.threads) : ((work / handle->desc.threads) + 1);
/* compute thr_begin and thr_end */
const int thr_begin = (ltid * chunksize < work) ? (ltid * chunksize) : work;
const int thr_end = ((ltid + 1) * chunksize < work) ? ((ltid + 1) * chunksize) : work;


/* number of tasks that could be run in parallel */
const int transpose_work = handle->blocksofm * handle->blocksifm;
/* compute chunck size */
const int transpose_chunksize = (transpose_work % handle->desc.threads == 0) ? (transpose_work / handle->desc.threads) : ((transpose_work / handle->desc.threads) + 1);
/* compute thr_begin and thr_end */
const int transpose_thr_begin = (ltid * transpose_chunksize < transpose_work) ? (ltid * transpose_chunksize) : transpose_work;
const int transpose_thr_end = ((ltid + 1) * transpose_chunksize < transpose_work) ? ((ltid + 1) * transpose_chunksize) : transpose_work;

element_output_type *const out = ((element_output_type*)handle->grad_output->data) + (handle->desc.pad_h_out * handle->ofwp + handle->desc.pad_w_out) * handle->blocksofm * handle->ofmblock;
LIBXSMM_VLA_DECL(5, element_output_type, del_out, out, handle->ofhp, handle->ofwp, handle->blocksofm, handle->ofmblock);
LIBXSMM_VLA_DECL(5, element_input_type, del_input, (element_input_type*)handle->grad_input->data, handle->ifhp, handle->ifwp, handle->blocksifm, handle->ifmblock);
LIBXSMM_VLA_DECL(6, element_filter_type, wt, (element_filter_type*)handle->reg_filter->data, handle->desc.S, handle->blocksifm, handle->ifmblock, handle->blocksofm, handle->ofmblock);
LIBXSMM_VLA_DECL(6, element_filter_type, tr_wt, (element_filter_type*)handle->scratch1, handle->desc.S, handle->blocksofm, handle->ofmblock, handle->blocksifm, handle->ifmblock);

/* avoid warning by using the xconv.sconv sequence to get some fn. ptr. to act as source of the type-cast */
libxsmm_convfunction jitted_conv_bp_no_pf = (libxsmm_convfunction)handle->code_bwd[0].xconv.sconv;

element_input_type *l_input;
element_filter_type *l_wt;
element_output_type* l_output;

#if defined(INPUT_PADDING)
element_input_type (* LIBXSMM_RESTRICT input_ptr);
element_input_type (* LIBXSMM_RESTRICT copy_ptr);
element_input_type *prefetch_ptr;
const int padded_h = handle->ifhp + 2 * handle->desc.pad_h;
const int padded_w = handle->ifwp + 2 * handle->desc.pad_w;
libxsmm_matcopyfunction jitted_matcopy = (libxsmm_matcopyfunction)handle->matcopy_bwd[0].xmatcopy.smatcopy;
libxsmm_matcopybackfunction jitted_matcopyback = (libxsmm_matcopybackfunction)handle->matcopy_bwd[1].xmatcopy.smatcopy;
LIBXSMM_VLA_DECL(4, element_input_type, input_buffer, ((element_input_type*)handle->scratch5) + ltid * padded_h * padded_w * handle->blocksifm * handle->ifmblock, padded_w, handle->blocksifm, handle->ifmblock);
memset(&LIBXSMM_VLA_ACCESS(4, input_buffer, 0, 0, 0, 0, padded_w, handle->blocksifm, handle->ifmblock), 0,
       padded_w * padded_h * handle->blocksifm * handle->ifmblock * sizeof(element_input_type));
ifh = handle->ifhp + 2 * handle->desc.pad_h;
#else
ifh = handle->ifhp;
#endif

/* lazy barrier init */
if (handle->filter_transposed == 0) {
  libxsmm_barrier_init(handle->barrier, ltid);

  for (ifm1ofm1 = transpose_thr_begin; ifm1ofm1 < transpose_thr_end; ++ifm1ofm1) {
    ofm1 = ifm1ofm1/handle->blocksifm;
    ifm1 = ifm1ofm1%handle->blocksifm;
    for (kj=0; kj < handle->desc.R; ++kj) {
      for (ki=0; ki < handle->desc.S; ++ki) {
        /* TODO: enable this later */
        /*transpose<VLEN,VLEN>(&wt[ofm1][ifm1][kj][ki][0][0],&tr_wt[ofm1][ifm1][kj][ki][0][0]);*/
        for (ofm2 = 0; ofm2 < handle->ofmblock; ++ofm2) {
          for (ifm2 = 0; ifm2 < handle->ifmblock; ++ifm2) {
            LIBXSMM_VLA_ACCESS(6, tr_wt, kj, ki, ofm1, ofm2, ifm1, ifm2, handle->desc.S, handle->blocksofm, handle->ofmblock, handle->blocksifm, handle->ifmblock) =
              LIBXSMM_VLA_ACCESS(6, wt,  kj, ki, ifm1, ifm2, ofm1, ofm2, handle->desc.S, handle->blocksifm, handle->ifmblock, handle->blocksofm, handle->ofmblock);
          }
        }
      }
    }
  }
  libxsmm_barrier_wait(handle->barrier, ltid);
}

if ( libxsmm_target_archid == LIBXSMM_X86_AVX512_MIC  ||
     libxsmm_target_archid == LIBXSMM_X86_AVX512_CORE ||
     libxsmm_target_archid == LIBXSMM_X86_AVX512_KNM  || /* ) {
  status = LIBXSMM_DNN_ERR_UNSUPPORTED_ARCH;
} else if (*/ libxsmm_target_archid == LIBXSMM_X86_AVX2 ) {
  for (imgifm1 = thr_begin; imgifm1 < thr_end; ++imgifm1) {
    img = imgifm1/handle->blocksifm;
    ifm1 = imgifm1%handle->blocksifm;

#if defined(INPUT_PADDING)
    for (oj = 0; oj < handle->ifhp; oj++) {
      for (ij = 0; ij < handle->ifwp; ij++) {
        input_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img, oj, ij, ifm1, 0, handle->ifhp, handle->ifwp, handle->blocksifm, handle->ifmblock);
        copy_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(4, input_buffer, oj+handle->desc.pad_h, ij+handle->desc.pad_w, ifm1, 0, padded_w, handle->blocksifm, handle->ifmblock);
        prefetch_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img+(ifm1+1)/handle->blocksifm, oj, ij, (ifm1+1)%handle->blocksifm, 0, handle->ifhp, handle->ifwp, handle->blocksifm, handle->ifmblock);
        jitted_matcopy(input_ptr, NULL, copy_ptr, NULL, prefetch_ptr);
      }
    }
#endif

    for (ofm1 = 0; ofm1 < handle->blocksofm; ++ofm1) {
      for (ij= 0 ; ij < ifh; ++ij) {
        for (kj=0; kj < handle->desc.R; ++kj) {
          oj = ij - handle->desc.R + kj + 1;
          if (oj >= 0 && oj < handle->ofh) {
#if defined(INPUT_PADDING)
            l_input =  &LIBXSMM_VLA_ACCESS(4, input_buffer, ij, 0, ifm1, 0, padded_w, handle->blocksifm, handle->ifmblock);
#else
            l_input =  &LIBXSMM_VLA_ACCESS(5, del_input, img, ij, 0, ifm1, 0, handle->ifhp, handle->ifwp, handle->blocksifm, handle->ifmblock);
#endif
            l_wt = &LIBXSMM_VLA_ACCESS(6, tr_wt, handle->desc.R-kj-1, 0, ofm1, 0, ifm1, 0, handle->desc.S, handle->blocksofm, handle->ofmblock, handle->blocksifm, handle->ifmblock);
            l_output = &LIBXSMM_VLA_ACCESS(5, del_out, img, oj, 0, ofm1, 0, handle->ofhp, handle->ofwp, handle->blocksofm, handle->ofmblock);
            jitted_conv_bp_no_pf(l_input, l_wt, l_output, NULL, NULL, NULL );
          }
        }
      }
    }
#if defined(INPUT_PADDING)
    /* Write back input buffer */
    for (oj = 0; oj < handle->ifhp; oj++) {
      for (ij = 0; ij < handle->ifwp; ij++) {
        input_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img, oj, ij, ifm1, 0, handle->ifhp, handle->ifwp, handle->blocksifm, handle->ifmblock);
        copy_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(4, input_buffer, oj+handle->desc.pad_h, ij+handle->desc.pad_w, ifm1, 0, padded_w, handle->blocksifm, handle->ifmblock);
        jitted_matcopyback(copy_ptr, NULL, input_ptr, NULL, NULL);
      }
    }
#else
#include "libxsmm_dnn_zero_rim_st_input_nhwc.tpl.c"
#endif
  }
/* should never happen, this is just an additional check */
} else {
  status = LIBXSMM_DNN_ERR_UNSUPPORTED_ARCH;
}
