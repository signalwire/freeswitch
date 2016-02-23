/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_COMMON_RECONINTER_H_
#define VP10_COMMON_RECONINTER_H_

#include "vp10/common/filter.h"
#include "vp10/common/onyxc_int.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/vpx_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE void inter_predictor(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride,
                                   const int subpel_x,
                                   const int subpel_y,
                                   const struct scale_factors *sf,
                                   int w, int h, int ref,
                                   const InterpKernel *kernel,
                                   int xs, int ys) {
  sf->predict[subpel_x != 0][subpel_y != 0][ref](
      src, src_stride, dst, dst_stride,
      kernel[subpel_x], xs, kernel[subpel_y], ys, w, h);
}

#if CONFIG_VP9_HIGHBITDEPTH
static INLINE void high_inter_predictor(const uint8_t *src, int src_stride,
                                        uint8_t *dst, int dst_stride,
                                        const int subpel_x,
                                        const int subpel_y,
                                        const struct scale_factors *sf,
                                        int w, int h, int ref,
                                        const InterpKernel *kernel,
                                        int xs, int ys, int bd) {
  sf->highbd_predict[subpel_x != 0][subpel_y != 0][ref](
      src, src_stride, dst, dst_stride,
      kernel[subpel_x], xs, kernel[subpel_y], ys, w, h, bd);
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

static INLINE int round_mv_comp_q4(int value) {
  return (value < 0 ? value - 2 : value + 2) / 4;
}

static MV mi_mv_pred_q4(const MODE_INFO *mi, int idx) {
  MV res = { round_mv_comp_q4(mi->bmi[0].as_mv[idx].as_mv.row +
                              mi->bmi[1].as_mv[idx].as_mv.row +
                              mi->bmi[2].as_mv[idx].as_mv.row +
                              mi->bmi[3].as_mv[idx].as_mv.row),
             round_mv_comp_q4(mi->bmi[0].as_mv[idx].as_mv.col +
                              mi->bmi[1].as_mv[idx].as_mv.col +
                              mi->bmi[2].as_mv[idx].as_mv.col +
                              mi->bmi[3].as_mv[idx].as_mv.col) };
  return res;
}

static INLINE int round_mv_comp_q2(int value) {
  return (value < 0 ? value - 1 : value + 1) / 2;
}

static MV mi_mv_pred_q2(const MODE_INFO *mi, int idx, int block0, int block1) {
  MV res = { round_mv_comp_q2(mi->bmi[block0].as_mv[idx].as_mv.row +
                              mi->bmi[block1].as_mv[idx].as_mv.row),
             round_mv_comp_q2(mi->bmi[block0].as_mv[idx].as_mv.col +
                              mi->bmi[block1].as_mv[idx].as_mv.col) };
  return res;
}

// TODO(jkoleszar): yet another mv clamping function :-(
static INLINE MV clamp_mv_to_umv_border_sb(const MACROBLOCKD *xd,
                                           const MV *src_mv,
                                           int bw, int bh, int ss_x, int ss_y) {
  // If the MV points so far into the UMV border that no visible pixels
  // are used for reconstruction, the subpel part of the MV can be
  // discarded and the MV limited to 16 pixels with equivalent results.
  const int spel_left = (VP9_INTERP_EXTEND + bw) << SUBPEL_BITS;
  const int spel_right = spel_left - SUBPEL_SHIFTS;
  const int spel_top = (VP9_INTERP_EXTEND + bh) << SUBPEL_BITS;
  const int spel_bottom = spel_top - SUBPEL_SHIFTS;
  MV clamped_mv = {
    src_mv->row * (1 << (1 - ss_y)),
    src_mv->col * (1 << (1 - ss_x))
  };
  assert(ss_x <= 1);
  assert(ss_y <= 1);

  clamp_mv(&clamped_mv,
           xd->mb_to_left_edge * (1 << (1 - ss_x)) - spel_left,
           xd->mb_to_right_edge * (1 << (1 - ss_x)) + spel_right,
           xd->mb_to_top_edge * (1 << (1 - ss_y)) - spel_top,
           xd->mb_to_bottom_edge * (1 << (1 - ss_y)) + spel_bottom);

  return clamped_mv;
}

static INLINE MV average_split_mvs(const struct macroblockd_plane *pd,
                                   const MODE_INFO *mi, int ref, int block) {
  const int ss_idx = ((pd->subsampling_x > 0) << 1) | (pd->subsampling_y > 0);
  MV res = {0, 0};
  switch (ss_idx) {
    case 0:
      res = mi->bmi[block].as_mv[ref].as_mv;
      break;
    case 1:
      res = mi_mv_pred_q2(mi, ref, block, block + 2);
      break;
    case 2:
      res = mi_mv_pred_q2(mi, ref, block, block + 1);
      break;
    case 3:
      res = mi_mv_pred_q4(mi, ref);
      break;
    default:
      assert(ss_idx <= 3 && ss_idx >= 0);
  }
  return res;
}

void build_inter_predictors(MACROBLOCKD *xd, int plane, int block,
                                   int bw, int bh,
                                   int x, int y, int w, int h,
                                   int mi_x, int mi_y);

void vp10_build_inter_predictor_sub8x8(MACROBLOCKD *xd, int plane,
                                       int i, int ir, int ic,
                                       int mi_row, int mi_col);

void vp10_build_inter_predictors_sby(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize);

void vp10_build_inter_predictors_sbp(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize, int plane);

void vp10_build_inter_predictors_sbuv(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     BLOCK_SIZE bsize);

void vp10_build_inter_predictors_sb(MACROBLOCKD *xd, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize);

void vp10_build_inter_predictor(const uint8_t *src, int src_stride,
                               uint8_t *dst, int dst_stride,
                               const MV *mv_q3,
                               const struct scale_factors *sf,
                               int w, int h, int do_avg,
                               const InterpKernel *kernel,
                               enum mv_precision precision,
                               int x, int y);

#if CONFIG_VP9_HIGHBITDEPTH
void vp10_highbd_build_inter_predictor(const uint8_t *src, int src_stride,
                                      uint8_t *dst, int dst_stride,
                                      const MV *mv_q3,
                                      const struct scale_factors *sf,
                                      int w, int h, int do_avg,
                                      const InterpKernel *kernel,
                                      enum mv_precision precision,
                                      int x, int y, int bd);
#endif

static INLINE int scaled_buffer_offset(int x_offset, int y_offset, int stride,
                                       const struct scale_factors *sf) {
  const int x = sf ? sf->scale_value_x(x_offset, sf) : x_offset;
  const int y = sf ? sf->scale_value_y(y_offset, sf) : y_offset;
  return y * stride + x;
}

static INLINE void setup_pred_plane(struct buf_2d *dst,
                                    uint8_t *src, int stride,
                                    int mi_row, int mi_col,
                                    const struct scale_factors *scale,
                                    int subsampling_x, int subsampling_y) {
  const int x = (MI_SIZE * mi_col) >> subsampling_x;
  const int y = (MI_SIZE * mi_row) >> subsampling_y;
  dst->buf = src + scaled_buffer_offset(x, y, stride, scale);
  dst->stride = stride;
}

void vp10_setup_dst_planes(struct macroblockd_plane planes[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col);

void vp10_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_RECONINTER_H_
