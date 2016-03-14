/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_ENCODER_ENCODEMB_H_
#define VP10_ENCODER_ENCODEMB_H_

#include "./vpx_config.h"
#include "vp10/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct encode_b_args {
  MACROBLOCK *x;
  struct optimize_ctx *ctx;
  int8_t *skip;
};
void vp10_encode_sb(MACROBLOCK *x, BLOCK_SIZE bsize);
void vp10_encode_sby_pass1(MACROBLOCK *x, BLOCK_SIZE bsize);
void vp10_xform_quant_fp(MACROBLOCK *x, int plane, int block,
                         int blk_row, int blk_col,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size);
void vp10_xform_quant_dc(MACROBLOCK *x, int plane, int block,
                         int blk_row, int blk_col,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size);
void vp10_xform_quant(MACROBLOCK *x, int plane, int block,
                      int blk_row, int blk_col,
                      BLOCK_SIZE plane_bsize, TX_SIZE tx_size);

void vp10_subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane);

void vp10_encode_block_intra(int plane, int block, int blk_row, int blk_col,
                             BLOCK_SIZE plane_bsize,
                             TX_SIZE tx_size, void *arg);

void vp10_encode_intra_block_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane);

void vp10_fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                       int diff_stride, TX_TYPE tx_type, int lossless);

#if CONFIG_VP9_HIGHBITDEPTH
void vp10_highbd_fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                              int diff_stride, TX_TYPE tx_type, int lossless);
#endif  // CONFIG_VP9_HIGHBITDEPTH

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_ENCODEMB_H_
