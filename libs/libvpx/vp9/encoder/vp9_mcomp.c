/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_reconinter.h"

#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_mcomp.h"

// #define NEW_DIAMOND_SEARCH

void vp9_set_mv_search_range(MvLimits *mv_limits, const MV *mv) {
  int col_min = (mv->col >> 3) - MAX_FULL_PEL_VAL + (mv->col & 7 ? 1 : 0);
  int row_min = (mv->row >> 3) - MAX_FULL_PEL_VAL + (mv->row & 7 ? 1 : 0);
  int col_max = (mv->col >> 3) + MAX_FULL_PEL_VAL;
  int row_max = (mv->row >> 3) + MAX_FULL_PEL_VAL;

  col_min = VPXMAX(col_min, (MV_LOW >> 3) + 1);
  row_min = VPXMAX(row_min, (MV_LOW >> 3) + 1);
  col_max = VPXMIN(col_max, (MV_UPP >> 3) - 1);
  row_max = VPXMIN(row_max, (MV_UPP >> 3) - 1);

  // Get intersection of UMV window and valid MV window to reduce # of checks
  // in diamond search.
  if (mv_limits->col_min < col_min) mv_limits->col_min = col_min;
  if (mv_limits->col_max > col_max) mv_limits->col_max = col_max;
  if (mv_limits->row_min < row_min) mv_limits->row_min = row_min;
  if (mv_limits->row_max > row_max) mv_limits->row_max = row_max;
}

void vp9_set_subpel_mv_search_range(MvLimits *subpel_mv_limits,
                                    const MvLimits *umv_window_limits,
                                    const MV *ref_mv) {
  subpel_mv_limits->col_min = VPXMAX(umv_window_limits->col_min * 8,
                                     ref_mv->col - MAX_FULL_PEL_VAL * 8);
  subpel_mv_limits->col_max = VPXMIN(umv_window_limits->col_max * 8,
                                     ref_mv->col + MAX_FULL_PEL_VAL * 8);
  subpel_mv_limits->row_min = VPXMAX(umv_window_limits->row_min * 8,
                                     ref_mv->row - MAX_FULL_PEL_VAL * 8);
  subpel_mv_limits->row_max = VPXMIN(umv_window_limits->row_max * 8,
                                     ref_mv->row + MAX_FULL_PEL_VAL * 8);

  subpel_mv_limits->col_min = VPXMAX(MV_LOW + 1, subpel_mv_limits->col_min);
  subpel_mv_limits->col_max = VPXMIN(MV_UPP - 1, subpel_mv_limits->col_max);
  subpel_mv_limits->row_min = VPXMAX(MV_LOW + 1, subpel_mv_limits->row_min);
  subpel_mv_limits->row_max = VPXMIN(MV_UPP - 1, subpel_mv_limits->row_max);
}

int vp9_init_search_range(int size) {
  int sr = 0;
  // Minimum search size no matter what the passed in value.
  size = VPXMAX(16, size);

  while ((size << sr) < MAX_FULL_PEL_VAL) sr++;

  sr = VPXMIN(sr, MAX_MVSEARCH_STEPS - 2);
  return sr;
}

static INLINE int mv_cost(const MV *mv, const int *joint_cost,
                          int *const comp_cost[2]) {
  assert(mv->row >= -MV_MAX && mv->row < MV_MAX);
  assert(mv->col >= -MV_MAX && mv->col < MV_MAX);
  return joint_cost[vp9_get_mv_joint(mv)] + comp_cost[0][mv->row] +
         comp_cost[1][mv->col];
}

int vp9_mv_bit_cost(const MV *mv, const MV *ref, const int *mvjcost,
                    int *mvcost[2], int weight) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  return ROUND_POWER_OF_TWO(mv_cost(&diff, mvjcost, mvcost) * weight, 7);
}

#define PIXEL_TRANSFORM_ERROR_SCALE 4
static int mv_err_cost(const MV *mv, const MV *ref, const int *mvjcost,
                       int *mvcost[2], int error_per_bit) {
  if (mvcost) {
    const MV diff = { mv->row - ref->row, mv->col - ref->col };
    return (int)ROUND64_POWER_OF_TWO(
        (int64_t)mv_cost(&diff, mvjcost, mvcost) * error_per_bit,
        RDDIV_BITS + VP9_PROB_COST_SHIFT - RD_EPB_SHIFT +
            PIXEL_TRANSFORM_ERROR_SCALE);
  }
  return 0;
}

static int mvsad_err_cost(const MACROBLOCK *x, const MV *mv, const MV *ref,
                          int sad_per_bit) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  return ROUND_POWER_OF_TWO(
      (unsigned)mv_cost(&diff, x->nmvjointsadcost, x->nmvsadcost) * sad_per_bit,
      VP9_PROB_COST_SHIFT);
}

void vp9_init_dsmotion_compensation(search_site_config *cfg, int stride) {
  int len;
  int ss_count = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 4 search sites per step.
    const MV ss_mvs[] = { { -len, 0 }, { len, 0 }, { 0, -len }, { 0, len } };
    int i;
    for (i = 0; i < 4; ++i, ++ss_count) {
      cfg->ss_mv[ss_count] = ss_mvs[i];
      cfg->ss_os[ss_count] = ss_mvs[i].row * stride + ss_mvs[i].col;
    }
  }

  cfg->searches_per_step = 4;
  cfg->total_steps = ss_count / cfg->searches_per_step;
}

void vp9_init3smotion_compensation(search_site_config *cfg, int stride) {
  int len;
  int ss_count = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 8 search sites per step.
    const MV ss_mvs[8] = { { -len, 0 },   { len, 0 },     { 0, -len },
                           { 0, len },    { -len, -len }, { -len, len },
                           { len, -len }, { len, len } };
    int i;
    for (i = 0; i < 8; ++i, ++ss_count) {
      cfg->ss_mv[ss_count] = ss_mvs[i];
      cfg->ss_os[ss_count] = ss_mvs[i].row * stride + ss_mvs[i].col;
    }
  }

  cfg->searches_per_step = 8;
  cfg->total_steps = ss_count / cfg->searches_per_step;
}

// convert motion vector component to offset for sv[a]f calc
static INLINE int sp(int x) { return x & 7; }

static INLINE const uint8_t *pre(const uint8_t *buf, int stride, int r, int c) {
  return &buf[(r >> 3) * stride + (c >> 3)];
}

#if CONFIG_VP9_HIGHBITDEPTH
/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER(v, r, c)                                                \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {                    \
    int64_t tmpmse;                                                          \
    const MV mv = { r, c };                                                  \
    const MV ref_mv = { rr, rc };                                            \
    if (second_pred == NULL) {                                               \
      thismse = vfp->svf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), z,  \
                         src_stride, &sse);                                  \
    } else {                                                                 \
      thismse = vfp->svaf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), z, \
                          src_stride, &sse, second_pred);                    \
    }                                                                        \
    tmpmse = thismse;                                                        \
    tmpmse += mv_err_cost(&mv, &ref_mv, mvjcost, mvcost, error_per_bit);     \
    if (tmpmse >= INT_MAX) {                                                 \
      v = INT_MAX;                                                           \
    } else if ((v = (uint32_t)tmpmse) < besterr) {                           \
      besterr = v;                                                           \
      br = r;                                                                \
      bc = c;                                                                \
      *distortion = thismse;                                                 \
      *sse1 = sse;                                                           \
    }                                                                        \
  } else {                                                                   \
    v = INT_MAX;                                                             \
  }
#else
/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER(v, r, c)                                                \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {                    \
    const MV mv = { r, c };                                                  \
    const MV ref_mv = { rr, rc };                                            \
    if (second_pred == NULL)                                                 \
      thismse = vfp->svf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), z,  \
                         src_stride, &sse);                                  \
    else                                                                     \
      thismse = vfp->svaf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), z, \
                          src_stride, &sse, second_pred);                    \
    if ((v = mv_err_cost(&mv, &ref_mv, mvjcost, mvcost, error_per_bit) +     \
             thismse) < besterr) {                                           \
      besterr = v;                                                           \
      br = r;                                                                \
      bc = c;                                                                \
      *distortion = thismse;                                                 \
      *sse1 = sse;                                                           \
    }                                                                        \
  } else {                                                                   \
    v = INT_MAX;                                                             \
  }

#endif
#define FIRST_LEVEL_CHECKS                                       \
  {                                                              \
    unsigned int left, right, up, down, diag;                    \
    CHECK_BETTER(left, tr, tc - hstep);                          \
    CHECK_BETTER(right, tr, tc + hstep);                         \
    CHECK_BETTER(up, tr - hstep, tc);                            \
    CHECK_BETTER(down, tr + hstep, tc);                          \
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);     \
    switch (whichdir) {                                          \
      case 0: CHECK_BETTER(diag, tr - hstep, tc - hstep); break; \
      case 1: CHECK_BETTER(diag, tr - hstep, tc + hstep); break; \
      case 2: CHECK_BETTER(diag, tr + hstep, tc - hstep); break; \
      case 3: CHECK_BETTER(diag, tr + hstep, tc + hstep); break; \
    }                                                            \
  }

#define SECOND_LEVEL_CHECKS                                       \
  {                                                               \
    int kr, kc;                                                   \
    unsigned int second;                                          \
    if (tr != br && tc != bc) {                                   \
      kr = br - tr;                                               \
      kc = bc - tc;                                               \
      CHECK_BETTER(second, tr + kr, tc + 2 * kc);                 \
      CHECK_BETTER(second, tr + 2 * kr, tc + kc);                 \
    } else if (tr == br && tc != bc) {                            \
      kc = bc - tc;                                               \
      CHECK_BETTER(second, tr + hstep, tc + 2 * kc);              \
      CHECK_BETTER(second, tr - hstep, tc + 2 * kc);              \
      switch (whichdir) {                                         \
        case 0:                                                   \
        case 1: CHECK_BETTER(second, tr + hstep, tc + kc); break; \
        case 2:                                                   \
        case 3: CHECK_BETTER(second, tr - hstep, tc + kc); break; \
      }                                                           \
    } else if (tr != br && tc == bc) {                            \
      kr = br - tr;                                               \
      CHECK_BETTER(second, tr + 2 * kr, tc + hstep);              \
      CHECK_BETTER(second, tr + 2 * kr, tc - hstep);              \
      switch (whichdir) {                                         \
        case 0:                                                   \
        case 2: CHECK_BETTER(second, tr + kr, tc + hstep); break; \
        case 1:                                                   \
        case 3: CHECK_BETTER(second, tr + kr, tc - hstep); break; \
      }                                                           \
    }                                                             \
  }

#define SETUP_SUBPEL_SEARCH                                                 \
  const uint8_t *const z = x->plane[0].src.buf;                             \
  const int src_stride = x->plane[0].src.stride;                            \
  const MACROBLOCKD *xd = &x->e_mbd;                                        \
  unsigned int besterr = UINT_MAX;                                          \
  unsigned int sse;                                                         \
  unsigned int whichdir;                                                    \
  int thismse;                                                              \
  const unsigned int halfiters = iters_per_step;                            \
  const unsigned int quarteriters = iters_per_step;                         \
  const unsigned int eighthiters = iters_per_step;                          \
  const int y_stride = xd->plane[0].pre[0].stride;                          \
  const int offset = bestmv->row * y_stride + bestmv->col;                  \
  const uint8_t *const y = xd->plane[0].pre[0].buf;                         \
                                                                            \
  int rr = ref_mv->row;                                                     \
  int rc = ref_mv->col;                                                     \
  int br = bestmv->row * 8;                                                 \
  int bc = bestmv->col * 8;                                                 \
  int hstep = 4;                                                            \
  int minc, maxc, minr, maxr;                                               \
  int tr = br;                                                              \
  int tc = bc;                                                              \
  MvLimits subpel_mv_limits;                                                \
                                                                            \
  vp9_set_subpel_mv_search_range(&subpel_mv_limits, &x->mv_limits, ref_mv); \
  minc = subpel_mv_limits.col_min;                                          \
  maxc = subpel_mv_limits.col_max;                                          \
  minr = subpel_mv_limits.row_min;                                          \
  maxr = subpel_mv_limits.row_max;                                          \
                                                                            \
  bestmv->row *= 8;                                                         \
  bestmv->col *= 8;

static unsigned int setup_center_error(
    const MACROBLOCKD *xd, const MV *bestmv, const MV *ref_mv,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp,
    const uint8_t *const src, const int src_stride, const uint8_t *const y,
    int y_stride, const uint8_t *second_pred, int w, int h, int offset,
    int *mvjcost, int *mvcost[2], uint32_t *sse1, uint32_t *distortion) {
#if CONFIG_VP9_HIGHBITDEPTH
  uint64_t besterr;
  if (second_pred != NULL) {
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      DECLARE_ALIGNED(16, uint16_t, comp_pred16[64 * 64]);
      vpx_highbd_comp_avg_pred(comp_pred16, CONVERT_TO_SHORTPTR(second_pred), w,
                               h, CONVERT_TO_SHORTPTR(y + offset), y_stride);
      besterr =
          vfp->vf(CONVERT_TO_BYTEPTR(comp_pred16), w, src, src_stride, sse1);
    } else {
      DECLARE_ALIGNED(16, uint8_t, comp_pred[64 * 64]);
      vpx_comp_avg_pred(comp_pred, second_pred, w, h, y + offset, y_stride);
      besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
    }
  } else {
    besterr = vfp->vf(y + offset, y_stride, src, src_stride, sse1);
  }
  *distortion = (uint32_t)besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);
  if (besterr >= UINT_MAX) return UINT_MAX;
  return (uint32_t)besterr;
#else
  uint32_t besterr;
  (void)xd;
  if (second_pred != NULL) {
    DECLARE_ALIGNED(16, uint8_t, comp_pred[64 * 64]);
    vpx_comp_avg_pred(comp_pred, second_pred, w, h, y + offset, y_stride);
    besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
  } else {
    besterr = vfp->vf(y + offset, y_stride, src, src_stride, sse1);
  }
  *distortion = besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);
  return besterr;
#endif  // CONFIG_VP9_HIGHBITDEPTH
}

static INLINE int64_t divide_and_round(const int64_t n, const int64_t d) {
  return ((n < 0) ^ (d < 0)) ? ((n - d / 2) / d) : ((n + d / 2) / d);
}

static INLINE int is_cost_list_wellbehaved(int *cost_list) {
  return cost_list[0] < cost_list[1] && cost_list[0] < cost_list[2] &&
         cost_list[0] < cost_list[3] && cost_list[0] < cost_list[4];
}

// Returns surface minima estimate at given precision in 1/2^n bits.
// Assume a model for the cost surface: S = A(x - x0)^2 + B(y - y0)^2 + C
// For a given set of costs S0, S1, S2, S3, S4 at points
// (y, x) = (0, 0), (0, -1), (1, 0), (0, 1) and (-1, 0) respectively,
// the solution for the location of the minima (x0, y0) is given by:
// x0 = 1/2 (S1 - S3)/(S1 + S3 - 2*S0),
// y0 = 1/2 (S4 - S2)/(S4 + S2 - 2*S0).
// The code below is an integerized version of that.
static void get_cost_surf_min(int *cost_list, int *ir, int *ic, int bits) {
  const int64_t x0 = (int64_t)cost_list[1] - cost_list[3];
  const int64_t y0 = cost_list[1] - 2 * (int64_t)cost_list[0] + cost_list[3];
  const int64_t x1 = (int64_t)cost_list[4] - cost_list[2];
  const int64_t y1 = cost_list[4] - 2 * (int64_t)cost_list[0] + cost_list[2];
  const int b = 1 << (bits - 1);
  *ic = (int)divide_and_round(x0 * b, y0);
  *ir = (int)divide_and_round(x1 * b, y1);
}

uint32_t vp9_skip_sub_pixel_tree(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp, int forced_stop,
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h, int use_accurate_subpel_search) {
  SETUP_SUBPEL_SEARCH;
  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp, z,
                               src_stride, y, y_stride, second_pred, w, h,
                               offset, mvjcost, mvcost, sse1, distortion);
  (void)halfiters;
  (void)quarteriters;
  (void)eighthiters;
  (void)whichdir;
  (void)allow_hp;
  (void)forced_stop;
  (void)hstep;
  (void)rr;
  (void)rc;
  (void)minr;
  (void)minc;
  (void)maxr;
  (void)maxc;
  (void)tr;
  (void)tc;
  (void)sse;
  (void)thismse;
  (void)cost_list;
  (void)use_accurate_subpel_search;

  return besterr;
}

uint32_t vp9_find_best_sub_pixel_tree_pruned_evenmore(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp, int forced_stop,
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h, int use_accurate_subpel_search) {
  SETUP_SUBPEL_SEARCH;
  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp, z,
                               src_stride, y, y_stride, second_pred, w, h,
                               offset, mvjcost, mvcost, sse1, distortion);
  (void)halfiters;
  (void)quarteriters;
  (void)eighthiters;
  (void)whichdir;
  (void)allow_hp;
  (void)forced_stop;
  (void)hstep;
  (void)use_accurate_subpel_search;

  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX && is_cost_list_wellbehaved(cost_list)) {
    int ir, ic;
    unsigned int minpt = INT_MAX;
    get_cost_surf_min(cost_list, &ir, &ic, 2);
    if (ir != 0 || ic != 0) {
      CHECK_BETTER(minpt, tr + 2 * ir, tc + 2 * ic);
    }
  } else {
    FIRST_LEVEL_CHECKS;
    if (halfiters > 1) {
      SECOND_LEVEL_CHECKS;
    }

    tr = br;
    tc = bc;

    // Each subsequent iteration checks at least one point in common with
    // the last iteration could be 2 ( if diag selected) 1/4 pel
    // Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
    if (forced_stop != 2) {
      hstep >>= 1;
      FIRST_LEVEL_CHECKS;
      if (quarteriters > 1) {
        SECOND_LEVEL_CHECKS;
      }
    }
  }

  tr = br;
  tc = bc;

  if (allow_hp && use_mv_hp(ref_mv) && forced_stop == 0) {
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (eighthiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

uint32_t vp9_find_best_sub_pixel_tree_pruned_more(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp, int forced_stop,
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h, int use_accurate_subpel_search) {
  SETUP_SUBPEL_SEARCH;
  (void)use_accurate_subpel_search;

  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp, z,
                               src_stride, y, y_stride, second_pred, w, h,
                               offset, mvjcost, mvcost, sse1, distortion);
  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX && is_cost_list_wellbehaved(cost_list)) {
    unsigned int minpt;
    int ir, ic;
    get_cost_surf_min(cost_list, &ir, &ic, 1);
    if (ir != 0 || ic != 0) {
      CHECK_BETTER(minpt, tr + ir * hstep, tc + ic * hstep);
    }
  } else {
    FIRST_LEVEL_CHECKS;
    if (halfiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel

  // Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
  if (forced_stop != 2) {
    tr = br;
    tc = bc;
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (quarteriters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  if (allow_hp && use_mv_hp(ref_mv) && forced_stop == 0) {
    tr = br;
    tc = bc;
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (eighthiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }
  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

uint32_t vp9_find_best_sub_pixel_tree_pruned(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp, int forced_stop,
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h, int use_accurate_subpel_search) {
  SETUP_SUBPEL_SEARCH;
  (void)use_accurate_subpel_search;

  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp, z,
                               src_stride, y, y_stride, second_pred, w, h,
                               offset, mvjcost, mvcost, sse1, distortion);
  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX) {
    unsigned int left, right, up, down, diag;
    whichdir = (cost_list[1] < cost_list[3] ? 0 : 1) +
               (cost_list[2] < cost_list[4] ? 0 : 2);
    switch (whichdir) {
      case 0:
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(down, tr + hstep, tc);
        CHECK_BETTER(diag, tr + hstep, tc - hstep);
        break;
      case 1:
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(down, tr + hstep, tc);
        CHECK_BETTER(diag, tr + hstep, tc + hstep);
        break;
      case 2:
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(diag, tr - hstep, tc - hstep);
        break;
      case 3:
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(diag, tr - hstep, tc + hstep);
        break;
    }
  } else {
    FIRST_LEVEL_CHECKS;
    if (halfiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  tr = br;
  tc = bc;

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel

  // Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
  if (forced_stop != 2) {
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (quarteriters > 1) {
      SECOND_LEVEL_CHECKS;
    }
    tr = br;
    tc = bc;
  }

  if (allow_hp && use_mv_hp(ref_mv) && forced_stop == 0) {
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (eighthiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
    tr = br;
    tc = bc;
  }
  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

/* clang-format off */
static const MV search_step_table[12] = {
  // left, right, up, down
  { 0, -4 }, { 0, 4 }, { -4, 0 }, { 4, 0 },
  { 0, -2 }, { 0, 2 }, { -2, 0 }, { 2, 0 },
  { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 }
};
/* clang-format on */

static int accurate_sub_pel_search(
    const MACROBLOCKD *xd, const MV *this_mv, const struct scale_factors *sf,
    const InterpKernel *kernel, const vp9_variance_fn_ptr_t *vfp,
    const uint8_t *const src_address, const int src_stride,
    const uint8_t *const pre_address, int y_stride, const uint8_t *second_pred,
    int w, int h, uint32_t *sse) {
#if CONFIG_VP9_HIGHBITDEPTH
  uint64_t besterr;
  assert(sf->x_step_q4 == 16 && sf->y_step_q4 == 16);
  assert(w != 0 && h != 0);
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    DECLARE_ALIGNED(16, uint16_t, pred16[64 * 64]);
    vp9_highbd_build_inter_predictor(CONVERT_TO_SHORTPTR(pre_address), y_stride,
                                     pred16, w, this_mv, sf, w, h, 0, kernel,
                                     MV_PRECISION_Q3, 0, 0, xd->bd);
    if (second_pred != NULL) {
      DECLARE_ALIGNED(16, uint16_t, comp_pred16[64 * 64]);
      vpx_highbd_comp_avg_pred(comp_pred16, CONVERT_TO_SHORTPTR(second_pred), w,
                               h, pred16, w);
      besterr = vfp->vf(CONVERT_TO_BYTEPTR(comp_pred16), w, src_address,
                        src_stride, sse);
    } else {
      besterr =
          vfp->vf(CONVERT_TO_BYTEPTR(pred16), w, src_address, src_stride, sse);
    }
  } else {
    DECLARE_ALIGNED(16, uint8_t, pred[64 * 64]);
    vp9_build_inter_predictor(pre_address, y_stride, pred, w, this_mv, sf, w, h,
                              0, kernel, MV_PRECISION_Q3, 0, 0);
    if (second_pred != NULL) {
      DECLARE_ALIGNED(16, uint8_t, comp_pred[64 * 64]);
      vpx_comp_avg_pred(comp_pred, second_pred, w, h, pred, w);
      besterr = vfp->vf(comp_pred, w, src_address, src_stride, sse);
    } else {
      besterr = vfp->vf(pred, w, src_address, src_stride, sse);
    }
  }
  if (besterr >= UINT_MAX) return UINT_MAX;
  return (int)besterr;
#else
  int besterr;
  DECLARE_ALIGNED(16, uint8_t, pred[64 * 64]);
  assert(sf->x_step_q4 == 16 && sf->y_step_q4 == 16);
  assert(w != 0 && h != 0);
  (void)xd;

  vp9_build_inter_predictor(pre_address, y_stride, pred, w, this_mv, sf, w, h,
                            0, kernel, MV_PRECISION_Q3, 0, 0);
  if (second_pred != NULL) {
    DECLARE_ALIGNED(16, uint8_t, comp_pred[64 * 64]);
    vpx_comp_avg_pred(comp_pred, second_pred, w, h, pred, w);
    besterr = vfp->vf(comp_pred, w, src_address, src_stride, sse);
  } else {
    besterr = vfp->vf(pred, w, src_address, src_stride, sse);
  }
  return besterr;
#endif  // CONFIG_VP9_HIGHBITDEPTH
}

// TODO(yunqing): this part can be further refactored.
#if CONFIG_VP9_HIGHBITDEPTH
/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER1(v, r, c)                                                 \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {                      \
    int64_t tmpmse;                                                            \
    const MV mv = { r, c };                                                    \
    const MV ref_mv = { rr, rc };                                              \
    thismse =                                                                  \
        accurate_sub_pel_search(xd, &mv, x->me_sf, kernel, vfp, z, src_stride, \
                                y, y_stride, second_pred, w, h, &sse);         \
    tmpmse = thismse;                                                          \
    tmpmse += mv_err_cost(&mv, &ref_mv, mvjcost, mvcost, error_per_bit);       \
    if (tmpmse >= INT_MAX) {                                                   \
      v = INT_MAX;                                                             \
    } else if ((v = (uint32_t)tmpmse) < besterr) {                             \
      besterr = v;                                                             \
      br = r;                                                                  \
      bc = c;                                                                  \
      *distortion = thismse;                                                   \
      *sse1 = sse;                                                             \
    }                                                                          \
  } else {                                                                     \
    v = INT_MAX;                                                               \
  }
#else
/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER1(v, r, c)                                                 \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {                      \
    const MV mv = { r, c };                                                    \
    const MV ref_mv = { rr, rc };                                              \
    thismse =                                                                  \
        accurate_sub_pel_search(xd, &mv, x->me_sf, kernel, vfp, z, src_stride, \
                                y, y_stride, second_pred, w, h, &sse);         \
    if ((v = mv_err_cost(&mv, &ref_mv, mvjcost, mvcost, error_per_bit) +       \
             thismse) < besterr) {                                             \
      besterr = v;                                                             \
      br = r;                                                                  \
      bc = c;                                                                  \
      *distortion = thismse;                                                   \
      *sse1 = sse;                                                             \
    }                                                                          \
  } else {                                                                     \
    v = INT_MAX;                                                               \
  }

#endif

uint32_t vp9_find_best_sub_pixel_tree(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp, int forced_stop,
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h, int use_accurate_subpel_search) {
  const uint8_t *const z = x->plane[0].src.buf;
  const uint8_t *const src_address = z;
  const int src_stride = x->plane[0].src.stride;
  const MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = UINT_MAX;
  unsigned int sse;
  int thismse;
  const int y_stride = xd->plane[0].pre[0].stride;
  const int offset = bestmv->row * y_stride + bestmv->col;
  const uint8_t *const y = xd->plane[0].pre[0].buf;

  int rr = ref_mv->row;
  int rc = ref_mv->col;
  int br = bestmv->row * 8;
  int bc = bestmv->col * 8;
  int hstep = 4;
  int iter, round = 3 - forced_stop;

  int minc, maxc, minr, maxr;
  int tr = br;
  int tc = bc;
  const MV *search_step = search_step_table;
  int idx, best_idx = -1;
  unsigned int cost_array[5];
  int kr, kc;
  MvLimits subpel_mv_limits;

  // TODO(yunqing): need to add 4-tap filter optimization to speed up the
  // encoder.
  const InterpKernel *kernel =
      (use_accurate_subpel_search > 0)
          ? ((use_accurate_subpel_search == USE_4_TAPS)
                 ? vp9_filter_kernels[FOURTAP]
                 : ((use_accurate_subpel_search == USE_8_TAPS)
                        ? vp9_filter_kernels[EIGHTTAP]
                        : vp9_filter_kernels[EIGHTTAP_SHARP]))
          : vp9_filter_kernels[BILINEAR];

  vp9_set_subpel_mv_search_range(&subpel_mv_limits, &x->mv_limits, ref_mv);
  minc = subpel_mv_limits.col_min;
  maxc = subpel_mv_limits.col_max;
  minr = subpel_mv_limits.row_min;
  maxr = subpel_mv_limits.row_max;

  if (!(allow_hp && use_mv_hp(ref_mv)))
    if (round == 3) round = 2;

  bestmv->row *= 8;
  bestmv->col *= 8;

  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp, z,
                               src_stride, y, y_stride, second_pred, w, h,
                               offset, mvjcost, mvcost, sse1, distortion);

  (void)cost_list;  // to silence compiler warning

  for (iter = 0; iter < round; ++iter) {
    // Check vertical and horizontal sub-pixel positions.
    for (idx = 0; idx < 4; ++idx) {
      tr = br + search_step[idx].row;
      tc = bc + search_step[idx].col;
      if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
        MV this_mv;
        this_mv.row = tr;
        this_mv.col = tc;

        if (use_accurate_subpel_search) {
          thismse = accurate_sub_pel_search(xd, &this_mv, x->me_sf, kernel, vfp,
                                            src_address, src_stride, y,
                                            y_stride, second_pred, w, h, &sse);
        } else {
          const uint8_t *const pre_address =
              y + (tr >> 3) * y_stride + (tc >> 3);
          if (second_pred == NULL)
            thismse = vfp->svf(pre_address, y_stride, sp(tc), sp(tr),
                               src_address, src_stride, &sse);
          else
            thismse = vfp->svaf(pre_address, y_stride, sp(tc), sp(tr),
                                src_address, src_stride, &sse, second_pred);
        }

        cost_array[idx] = thismse + mv_err_cost(&this_mv, ref_mv, mvjcost,
                                                mvcost, error_per_bit);

        if (cost_array[idx] < besterr) {
          best_idx = idx;
          besterr = cost_array[idx];
          *distortion = thismse;
          *sse1 = sse;
        }
      } else {
        cost_array[idx] = UINT_MAX;
      }
    }

    // Check diagonal sub-pixel position
    kc = (cost_array[0] <= cost_array[1] ? -hstep : hstep);
    kr = (cost_array[2] <= cost_array[3] ? -hstep : hstep);

    tc = bc + kc;
    tr = br + kr;
    if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
      MV this_mv = { tr, tc };
      if (use_accurate_subpel_search) {
        thismse = accurate_sub_pel_search(xd, &this_mv, x->me_sf, kernel, vfp,
                                          src_address, src_stride, y, y_stride,
                                          second_pred, w, h, &sse);
      } else {
        const uint8_t *const pre_address = y + (tr >> 3) * y_stride + (tc >> 3);
        if (second_pred == NULL)
          thismse = vfp->svf(pre_address, y_stride, sp(tc), sp(tr), src_address,
                             src_stride, &sse);
        else
          thismse = vfp->svaf(pre_address, y_stride, sp(tc), sp(tr),
                              src_address, src_stride, &sse, second_pred);
      }

      cost_array[4] = thismse + mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost,
                                            error_per_bit);

      if (cost_array[4] < besterr) {
        best_idx = 4;
        besterr = cost_array[4];
        *distortion = thismse;
        *sse1 = sse;
      }
    } else {
      cost_array[idx] = UINT_MAX;
    }

    if (best_idx < 4 && best_idx >= 0) {
      br += search_step[best_idx].row;
      bc += search_step[best_idx].col;
    } else if (best_idx == 4) {
      br = tr;
      bc = tc;
    }

    if (iters_per_step > 0 && best_idx != -1) {
      unsigned int second;
      const int br0 = br;
      const int bc0 = bc;
      assert(tr == br || tc == bc);

      if (tr == br && tc != bc) {
        kc = bc - tc;
        if (iters_per_step == 1) {
          if (use_accurate_subpel_search) {
            CHECK_BETTER1(second, br0, bc0 + kc);
          } else {
            CHECK_BETTER(second, br0, bc0 + kc);
          }
        }
      } else if (tr != br && tc == bc) {
        kr = br - tr;
        if (iters_per_step == 1) {
          if (use_accurate_subpel_search) {
            CHECK_BETTER1(second, br0 + kr, bc0);
          } else {
            CHECK_BETTER(second, br0 + kr, bc0);
          }
        }
      }

      if (iters_per_step > 1) {
        if (use_accurate_subpel_search) {
          CHECK_BETTER1(second, br0 + kr, bc0);
          CHECK_BETTER1(second, br0, bc0 + kc);
          if (br0 != br || bc0 != bc) {
            CHECK_BETTER1(second, br0 + kr, bc0 + kc);
          }
        } else {
          CHECK_BETTER(second, br0 + kr, bc0);
          CHECK_BETTER(second, br0, bc0 + kc);
          if (br0 != br || bc0 != bc) {
            CHECK_BETTER(second, br0 + kr, bc0 + kc);
          }
        }
      }
    }

    search_step += 4;
    hstep >>= 1;
    best_idx = -1;
  }

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel

  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

#undef CHECK_BETTER
#undef CHECK_BETTER1

static INLINE int check_bounds(const MvLimits *mv_limits, int row, int col,
                               int range) {
  return ((row - range) >= mv_limits->row_min) &
         ((row + range) <= mv_limits->row_max) &
         ((col - range) >= mv_limits->col_min) &
         ((col + range) <= mv_limits->col_max);
}

static INLINE int is_mv_in(const MvLimits *mv_limits, const MV *mv) {
  return (mv->col >= mv_limits->col_min) && (mv->col <= mv_limits->col_max) &&
         (mv->row >= mv_limits->row_min) && (mv->row <= mv_limits->row_max);
}

#define CHECK_BETTER                                                      \
  {                                                                       \
    if (thissad < bestsad) {                                              \
      if (use_mvcost)                                                     \
        thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit); \
      if (thissad < bestsad) {                                            \
        bestsad = thissad;                                                \
        best_site = i;                                                    \
      }                                                                   \
    }                                                                     \
  }

#define MAX_PATTERN_SCALES 11
#define MAX_PATTERN_CANDIDATES 8  // max number of canddiates per scale
#define PATTERN_CANDIDATES_REF 3  // number of refinement candidates

// Calculate and return a sad+mvcost list around an integer best pel.
static INLINE void calc_int_cost_list(const MACROBLOCK *x, const MV *ref_mv,
                                      int sadpb,
                                      const vp9_variance_fn_ptr_t *fn_ptr,
                                      const MV *best_mv, int *cost_list) {
  static const MV neighbors[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->e_mbd.plane[0].pre[0];
  const MV fcenter_mv = { ref_mv->row >> 3, ref_mv->col >> 3 };
  int br = best_mv->row;
  int bc = best_mv->col;
  MV this_mv;
  int i;
  unsigned int sse;

  this_mv.row = br;
  this_mv.col = bc;
  cost_list[0] =
      fn_ptr->vf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv),
                 in_what->stride, &sse) +
      mvsad_err_cost(x, &this_mv, &fcenter_mv, sadpb);
  if (check_bounds(&x->mv_limits, br, bc, 1)) {
    for (i = 0; i < 4; i++) {
      const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
      cost_list[i + 1] = fn_ptr->vf(what->buf, what->stride,
                                    get_buf_from_mv(in_what, &this_mv),
                                    in_what->stride, &sse) +
                         mv_err_cost(&this_mv, &fcenter_mv, x->nmvjointcost,
                                     x->mvcost, x->errorperbit);
    }
  } else {
    for (i = 0; i < 4; i++) {
      const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
      if (!is_mv_in(&x->mv_limits, &this_mv))
        cost_list[i + 1] = INT_MAX;
      else
        cost_list[i + 1] = fn_ptr->vf(what->buf, what->stride,
                                      get_buf_from_mv(in_what, &this_mv),
                                      in_what->stride, &sse) +
                           mv_err_cost(&this_mv, &fcenter_mv, x->nmvjointcost,
                                       x->mvcost, x->errorperbit);
    }
  }
}

// Generic pattern search function that searches over multiple scales.
// Each scale can have a different number of candidates and shape of
// candidates as indicated in the num_candidates and candidates arrays
// passed into this function
//
static int vp9_pattern_search(
    const MACROBLOCK *x, MV *ref_mv, int search_param, int sad_per_bit,
    int do_init_search, int *cost_list, const vp9_variance_fn_ptr_t *vfp,
    int use_mvcost, const MV *center_mv, MV *best_mv,
    const int num_candidates[MAX_PATTERN_SCALES],
    const MV candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES]) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = {
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  int i, s, t;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  int br, bc;
  int bestsad = INT_MAX;
  int thissad;
  int k = -1;
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  int best_init_s = search_param_to_steps[search_param];
  // adjust ref_mv to make sure it is within MV range
  clamp_mv(ref_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  br = ref_mv->row;
  bc = ref_mv->col;

  // Work out the start point for the search
  bestsad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv),
                     in_what->stride) +
            mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);

  // Search all possible scales upto the search param around the center point
  // pick the scale of the point that is best as the starting scale of
  // further steps around it.
  if (do_init_search) {
    s = best_init_s;
    best_init_s = -1;
    for (t = 0; t <= s; ++t) {
      int best_site = -1;
      if (check_bounds(&x->mv_limits, br, bc, 1 << t)) {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = { br + candidates[t][i].row,
                               bc + candidates[t][i].col };
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      } else {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = { br + candidates[t][i].row,
                               bc + candidates[t][i].col };
          if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      }
      if (best_site == -1) {
        continue;
      } else {
        best_init_s = t;
        k = best_site;
      }
    }
    if (best_init_s != -1) {
      br += candidates[best_init_s][k].row;
      bc += candidates[best_init_s][k].col;
    }
  }

  // If the center point is still the best, just skip this and move to
  // the refinement step.
  if (best_init_s != -1) {
    int best_site = -1;
    s = best_init_s;

    do {
      // No need to search all 6 points the 1st time if initial search was used
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site == -1) {
          continue;
        } else {
          br += candidates[s][best_site].row;
          bc += candidates[s][best_site].col;
          k = best_site;
        }
      }

      do {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;

        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      } while (best_site != -1);
    } while (s--);
  }

  // Returns the one-away integer pel sad values around the best as follows:
  // cost_list[0]: cost at the best integer pel
  // cost_list[1]: cost at delta {0, -1} (left)   from the best integer pel
  // cost_list[2]: cost at delta { 1, 0} (bottom) from the best integer pel
  // cost_list[3]: cost at delta { 0, 1} (right)  from the best integer pel
  // cost_list[4]: cost at delta {-1, 0} (top)    from the best integer pel
  if (cost_list) {
    const MV best_mv = { br, bc };
    calc_int_cost_list(x, &fcenter_mv, sad_per_bit, vfp, &best_mv, cost_list);
  }
  best_mv->row = br;
  best_mv->col = bc;
  return bestsad;
}

// A specialized function where the smallest scale search candidates
// are 4 1-away neighbors, and cost_list is non-null
// TODO(debargha): Merge this function with the one above. Also remove
// use_mvcost option since it is always 1, to save unnecessary branches.
static int vp9_pattern_search_sad(
    const MACROBLOCK *x, MV *ref_mv, int search_param, int sad_per_bit,
    int do_init_search, int *cost_list, const vp9_variance_fn_ptr_t *vfp,
    int use_mvcost, const MV *center_mv, MV *best_mv,
    const int num_candidates[MAX_PATTERN_SCALES],
    const MV candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES]) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = {
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  int i, s, t;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  int br, bc;
  int bestsad = INT_MAX;
  int thissad;
  int k = -1;
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  int best_init_s = search_param_to_steps[search_param];
  // adjust ref_mv to make sure it is within MV range
  clamp_mv(ref_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  br = ref_mv->row;
  bc = ref_mv->col;
  if (cost_list != NULL) {
    cost_list[0] = cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] =
        INT_MAX;
  }

  // Work out the start point for the search
  bestsad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv),
                     in_what->stride) +
            mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);

  // Search all possible scales upto the search param around the center point
  // pick the scale of the point that is best as the starting scale of
  // further steps around it.
  if (do_init_search) {
    s = best_init_s;
    best_init_s = -1;
    for (t = 0; t <= s; ++t) {
      int best_site = -1;
      if (check_bounds(&x->mv_limits, br, bc, 1 << t)) {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = { br + candidates[t][i].row,
                               bc + candidates[t][i].col };
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      } else {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = { br + candidates[t][i].row,
                               bc + candidates[t][i].col };
          if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      }
      if (best_site == -1) {
        continue;
      } else {
        best_init_s = t;
        k = best_site;
      }
    }
    if (best_init_s != -1) {
      br += candidates[best_init_s][k].row;
      bc += candidates[best_init_s][k].col;
    }
  }

  // If the center point is still the best, just skip this and move to
  // the refinement step.
  if (best_init_s != -1) {
    int do_sad = (num_candidates[0] == 4 && cost_list != NULL);
    int best_site = -1;
    s = best_init_s;

    for (; s >= do_sad; s--) {
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site == -1) {
          continue;
        } else {
          br += candidates[s][best_site].row;
          bc += candidates[s][best_site].col;
          k = best_site;
        }
      }

      do {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;

        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      } while (best_site != -1);
    }

    // Note: If we enter the if below, then cost_list must be non-NULL.
    if (s == 0) {
      cost_list[0] = bestsad;
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            cost_list[i + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            cost_list[i + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          br += candidates[s][best_site].row;
          bc += candidates[s][best_site].col;
          k = best_site;
        }
      }
      while (best_site != -1) {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;
        cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;
        cost_list[((k + 2) % 4) + 1] = cost_list[0];
        cost_list[0] = bestsad;

        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            cost_list[next_chkpts_indices[i] + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            if (!is_mv_in(&x->mv_limits, &this_mv)) {
              cost_list[next_chkpts_indices[i] + 1] = INT_MAX;
              continue;
            }
            cost_list[next_chkpts_indices[i] + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      }
    }
  }

  // Returns the one-away integer pel sad values around the best as follows:
  // cost_list[0]: sad at the best integer pel
  // cost_list[1]: sad at delta {0, -1} (left)   from the best integer pel
  // cost_list[2]: sad at delta { 1, 0} (bottom) from the best integer pel
  // cost_list[3]: sad at delta { 0, 1} (right)  from the best integer pel
  // cost_list[4]: sad at delta {-1, 0} (top)    from the best integer pel
  if (cost_list) {
    static const MV neighbors[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    if (cost_list[0] == INT_MAX) {
      cost_list[0] = bestsad;
      if (check_bounds(&x->mv_limits, br, bc, 1)) {
        for (i = 0; i < 4; i++) {
          const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
          cost_list[i + 1] =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
        }
      } else {
        for (i = 0; i < 4; i++) {
          const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
          if (!is_mv_in(&x->mv_limits, &this_mv))
            cost_list[i + 1] = INT_MAX;
          else
            cost_list[i + 1] =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
        }
      }
    } else {
      if (use_mvcost) {
        for (i = 0; i < 4; i++) {
          const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
          if (cost_list[i + 1] != INT_MAX) {
            cost_list[i + 1] +=
                mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
          }
        }
      }
    }
  }
  best_mv->row = br;
  best_mv->col = bc;
  return bestsad;
}

int vp9_get_mvpred_var(const MACROBLOCK *x, const MV *best_mv,
                       const MV *center_mv, const vp9_variance_fn_ptr_t *vfp,
                       int use_mvcost) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = { best_mv->row * 8, best_mv->col * 8 };
  uint32_t unused;
#if CONFIG_VP9_HIGHBITDEPTH
  uint64_t err =
      vfp->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
              in_what->stride, &unused);
  err += (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                   x->errorperbit)
                     : 0);
  if (err >= INT_MAX) return INT_MAX;
  return (int)err;
#else
  return vfp->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
                 in_what->stride, &unused) +
         (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                   x->errorperbit)
                     : 0);
#endif
}

int vp9_get_mvpred_av_var(const MACROBLOCK *x, const MV *best_mv,
                          const MV *center_mv, const uint8_t *second_pred,
                          const vp9_variance_fn_ptr_t *vfp, int use_mvcost) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = { best_mv->row * 8, best_mv->col * 8 };
  unsigned int unused;

  return vfp->svaf(get_buf_from_mv(in_what, best_mv), in_what->stride, 0, 0,
                   what->buf, what->stride, &unused, second_pred) +
         (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                   x->errorperbit)
                     : 0);
}

static int hex_search(const MACROBLOCK *x, MV *ref_mv, int search_param,
                      int sad_per_bit, int do_init_search, int *cost_list,
                      const vp9_variance_fn_ptr_t *vfp, int use_mvcost,
                      const MV *center_mv, MV *best_mv) {
  // First scale has 8-closest points, the rest have 6 points in hex shape
  // at increasing scales
  static const int hex_num_candidates[MAX_PATTERN_SCALES] = { 8, 6, 6, 6, 6, 6,
                                                              6, 6, 6, 6, 6 };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV hex_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
    { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 },
      { -1, 0 } },
    { { -1, -2 }, { 1, -2 }, { 2, 0 }, { 1, 2 }, { -1, 2 }, { -2, 0 } },
    { { -2, -4 }, { 2, -4 }, { 4, 0 }, { 2, 4 }, { -2, 4 }, { -4, 0 } },
    { { -4, -8 }, { 4, -8 }, { 8, 0 }, { 4, 8 }, { -4, 8 }, { -8, 0 } },
    { { -8, -16 }, { 8, -16 }, { 16, 0 }, { 8, 16 }, { -8, 16 }, { -16, 0 } },
    { { -16, -32 }, { 16, -32 }, { 32, 0 }, { 16, 32 }, { -16, 32 },
      { -32, 0 } },
    { { -32, -64 }, { 32, -64 }, { 64, 0 }, { 32, 64 }, { -32, 64 },
      { -64, 0 } },
    { { -64, -128 }, { 64, -128 }, { 128, 0 }, { 64, 128 }, { -64, 128 },
      { -128, 0 } },
    { { -128, -256 }, { 128, -256 }, { 256, 0 }, { 128, 256 }, { -128, 256 },
      { -256, 0 } },
    { { -256, -512 }, { 256, -512 }, { 512, 0 }, { 256, 512 }, { -256, 512 },
      { -512, 0 } },
    { { -512, -1024 }, { 512, -1024 }, { 1024, 0 }, { 512, 1024 },
      { -512, 1024 }, { -1024, 0 } }
  };
  /* clang-format on */
  return vp9_pattern_search(
      x, ref_mv, search_param, sad_per_bit, do_init_search, cost_list, vfp,
      use_mvcost, center_mv, best_mv, hex_num_candidates, hex_candidates);
}

static int bigdia_search(const MACROBLOCK *x, MV *ref_mv, int search_param,
                         int sad_per_bit, int do_init_search, int *cost_list,
                         const vp9_variance_fn_ptr_t *vfp, int use_mvcost,
                         const MV *center_mv, MV *best_mv) {
  // First scale has 4-closest points, the rest have 8 points in diamond
  // shape at increasing scales
  static const int bigdia_num_candidates[MAX_PATTERN_SCALES] = {
    4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV
      bigdia_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
        { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } },
        { { -1, -1 }, { 0, -2 }, { 1, -1 }, { 2, 0 }, { 1, 1 }, { 0, 2 },
          { -1, 1 }, { -2, 0 } },
        { { -2, -2 }, { 0, -4 }, { 2, -2 }, { 4, 0 }, { 2, 2 }, { 0, 4 },
          { -2, 2 }, { -4, 0 } },
        { { -4, -4 }, { 0, -8 }, { 4, -4 }, { 8, 0 }, { 4, 4 }, { 0, 8 },
          { -4, 4 }, { -8, 0 } },
        { { -8, -8 }, { 0, -16 }, { 8, -8 }, { 16, 0 }, { 8, 8 }, { 0, 16 },
          { -8, 8 }, { -16, 0 } },
        { { -16, -16 }, { 0, -32 }, { 16, -16 }, { 32, 0 }, { 16, 16 },
          { 0, 32 }, { -16, 16 }, { -32, 0 } },
        { { -32, -32 }, { 0, -64 }, { 32, -32 }, { 64, 0 }, { 32, 32 },
          { 0, 64 }, { -32, 32 }, { -64, 0 } },
        { { -64, -64 }, { 0, -128 }, { 64, -64 }, { 128, 0 }, { 64, 64 },
          { 0, 128 }, { -64, 64 }, { -128, 0 } },
        { { -128, -128 }, { 0, -256 }, { 128, -128 }, { 256, 0 }, { 128, 128 },
          { 0, 256 }, { -128, 128 }, { -256, 0 } },
        { { -256, -256 }, { 0, -512 }, { 256, -256 }, { 512, 0 }, { 256, 256 },
          { 0, 512 }, { -256, 256 }, { -512, 0 } },
        { { -512, -512 }, { 0, -1024 }, { 512, -512 }, { 1024, 0 },
          { 512, 512 }, { 0, 1024 }, { -512, 512 }, { -1024, 0 } }
      };
  /* clang-format on */
  return vp9_pattern_search_sad(
      x, ref_mv, search_param, sad_per_bit, do_init_search, cost_list, vfp,
      use_mvcost, center_mv, best_mv, bigdia_num_candidates, bigdia_candidates);
}

static int square_search(const MACROBLOCK *x, MV *ref_mv, int search_param,
                         int sad_per_bit, int do_init_search, int *cost_list,
                         const vp9_variance_fn_ptr_t *vfp, int use_mvcost,
                         const MV *center_mv, MV *best_mv) {
  // All scales have 8 closest points in square shape
  static const int square_num_candidates[MAX_PATTERN_SCALES] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV
      square_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
        { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 },
          { -1, 1 }, { -1, 0 } },
        { { -2, -2 }, { 0, -2 }, { 2, -2 }, { 2, 0 }, { 2, 2 }, { 0, 2 },
          { -2, 2 }, { -2, 0 } },
        { { -4, -4 }, { 0, -4 }, { 4, -4 }, { 4, 0 }, { 4, 4 }, { 0, 4 },
          { -4, 4 }, { -4, 0 } },
        { { -8, -8 }, { 0, -8 }, { 8, -8 }, { 8, 0 }, { 8, 8 }, { 0, 8 },
          { -8, 8 }, { -8, 0 } },
        { { -16, -16 }, { 0, -16 }, { 16, -16 }, { 16, 0 }, { 16, 16 },
          { 0, 16 }, { -16, 16 }, { -16, 0 } },
        { { -32, -32 }, { 0, -32 }, { 32, -32 }, { 32, 0 }, { 32, 32 },
          { 0, 32 }, { -32, 32 }, { -32, 0 } },
        { { -64, -64 }, { 0, -64 }, { 64, -64 }, { 64, 0 }, { 64, 64 },
          { 0, 64 }, { -64, 64 }, { -64, 0 } },
        { { -128, -128 }, { 0, -128 }, { 128, -128 }, { 128, 0 }, { 128, 128 },
          { 0, 128 }, { -128, 128 }, { -128, 0 } },
        { { -256, -256 }, { 0, -256 }, { 256, -256 }, { 256, 0 }, { 256, 256 },
          { 0, 256 }, { -256, 256 }, { -256, 0 } },
        { { -512, -512 }, { 0, -512 }, { 512, -512 }, { 512, 0 }, { 512, 512 },
          { 0, 512 }, { -512, 512 }, { -512, 0 } },
        { { -1024, -1024 }, { 0, -1024 }, { 1024, -1024 }, { 1024, 0 },
          { 1024, 1024 }, { 0, 1024 }, { -1024, 1024 }, { -1024, 0 } }
      };
  /* clang-format on */
  return vp9_pattern_search(
      x, ref_mv, search_param, sad_per_bit, do_init_search, cost_list, vfp,
      use_mvcost, center_mv, best_mv, square_num_candidates, square_candidates);
}

static int fast_hex_search(const MACROBLOCK *x, MV *ref_mv, int search_param,
                           int sad_per_bit,
                           int do_init_search,  // must be zero for fast_hex
                           int *cost_list, const vp9_variance_fn_ptr_t *vfp,
                           int use_mvcost, const MV *center_mv, MV *best_mv) {
  return hex_search(x, ref_mv, VPXMAX(MAX_MVSEARCH_STEPS - 2, search_param),
                    sad_per_bit, do_init_search, cost_list, vfp, use_mvcost,
                    center_mv, best_mv);
}

static int fast_dia_search(const MACROBLOCK *x, MV *ref_mv, int search_param,
                           int sad_per_bit, int do_init_search, int *cost_list,
                           const vp9_variance_fn_ptr_t *vfp, int use_mvcost,
                           const MV *center_mv, MV *best_mv) {
  return bigdia_search(x, ref_mv, VPXMAX(MAX_MVSEARCH_STEPS - 2, search_param),
                       sad_per_bit, do_init_search, cost_list, vfp, use_mvcost,
                       center_mv, best_mv);
}

#undef CHECK_BETTER

// Exhuastive motion search around a given centre position with a given
// step size.
static int exhaustive_mesh_search(const MACROBLOCK *x, MV *ref_mv, MV *best_mv,
                                  int range, int step, int sad_per_bit,
                                  const vp9_variance_fn_ptr_t *fn_ptr,
                                  const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  MV fcenter_mv = { center_mv->row, center_mv->col };
  unsigned int best_sad = INT_MAX;
  int r, c, i;
  int start_col, end_col, start_row, end_row;
  int col_step = (step > 1) ? step : 4;

  assert(step >= 1);

  clamp_mv(&fcenter_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  *best_mv = fcenter_mv;
  best_sad =
      fn_ptr->sdf(what->buf, what->stride,
                  get_buf_from_mv(in_what, &fcenter_mv), in_what->stride) +
      mvsad_err_cost(x, &fcenter_mv, ref_mv, sad_per_bit);
  start_row = VPXMAX(-range, x->mv_limits.row_min - fcenter_mv.row);
  start_col = VPXMAX(-range, x->mv_limits.col_min - fcenter_mv.col);
  end_row = VPXMIN(range, x->mv_limits.row_max - fcenter_mv.row);
  end_col = VPXMIN(range, x->mv_limits.col_max - fcenter_mv.col);

  for (r = start_row; r <= end_row; r += step) {
    for (c = start_col; c <= end_col; c += col_step) {
      // Step > 1 means we are not checking every location in this pass.
      if (step > 1) {
        const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c };
        unsigned int sad =
            fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv),
                        in_what->stride);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            *best_mv = mv;
          }
        }
      } else {
        // 4 sads in a single call if we are checking every location
        if (c + 3 <= end_col) {
          unsigned int sads[4];
          const uint8_t *addrs[4];
          for (i = 0; i < 4; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            addrs[i] = get_buf_from_mv(in_what, &mv);
          }
          fn_ptr->sdx4df(what->buf, what->stride, addrs, in_what->stride, sads);

          for (i = 0; i < 4; ++i) {
            if (sads[i] < best_sad) {
              const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
              const unsigned int sad =
                  sads[i] + mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                *best_mv = mv;
              }
            }
          }
        } else {
          for (i = 0; i < end_col - c; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            unsigned int sad =
                fn_ptr->sdf(what->buf, what->stride,
                            get_buf_from_mv(in_what, &mv), in_what->stride);
            if (sad < best_sad) {
              sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                *best_mv = mv;
              }
            }
          }
        }
      }
    }
  }

  return best_sad;
}

#define MIN_RANGE 7
#define MAX_RANGE 256
#define MIN_INTERVAL 1
#if CONFIG_NON_GREEDY_MV

#define LOG2_TABLE_SIZE 1024
static const int log2_table[LOG2_TABLE_SIZE] = {
  0,  // This is a dummy value
  0,        1048576,  1661954,  2097152,  2434718,  2710530,  2943725,
  3145728,  3323907,  3483294,  3627477,  3759106,  3880192,  3992301,
  4096672,  4194304,  4286015,  4372483,  4454275,  4531870,  4605679,
  4676053,  4743299,  4807682,  4869436,  4928768,  4985861,  5040877,
  5093962,  5145248,  5194851,  5242880,  5289431,  5334591,  5378443,
  5421059,  5462508,  5502851,  5542146,  5580446,  5617800,  5654255,
  5689851,  5724629,  5758625,  5791875,  5824409,  5856258,  5887450,
  5918012,  5947969,  5977344,  6006160,  6034437,  6062195,  6089453,
  6116228,  6142538,  6168398,  6193824,  6218829,  6243427,  6267632,
  6291456,  6314910,  6338007,  6360756,  6383167,  6405252,  6427019,
  6448477,  6469635,  6490501,  6511084,  6531390,  6551427,  6571202,
  6590722,  6609993,  6629022,  6647815,  6666376,  6684713,  6702831,
  6720734,  6738427,  6755916,  6773205,  6790299,  6807201,  6823917,
  6840451,  6856805,  6872985,  6888993,  6904834,  6920510,  6936026,
  6951384,  6966588,  6981641,  6996545,  7011304,  7025920,  7040397,
  7054736,  7068940,  7083013,  7096956,  7110771,  7124461,  7138029,
  7151476,  7164804,  7178017,  7191114,  7204100,  7216974,  7229740,
  7242400,  7254954,  7267405,  7279754,  7292003,  7304154,  7316208,
  7328167,  7340032,  7351805,  7363486,  7375079,  7386583,  7398000,
  7409332,  7420579,  7431743,  7442826,  7453828,  7464751,  7475595,
  7486362,  7497053,  7507669,  7518211,  7528680,  7539077,  7549404,
  7559660,  7569847,  7579966,  7590017,  7600003,  7609923,  7619778,
  7629569,  7639298,  7648964,  7658569,  7668114,  7677598,  7687023,
  7696391,  7705700,  7714952,  7724149,  7733289,  7742375,  7751407,
  7760385,  7769310,  7778182,  7787003,  7795773,  7804492,  7813161,
  7821781,  7830352,  7838875,  7847350,  7855777,  7864158,  7872493,
  7880782,  7889027,  7897226,  7905381,  7913492,  7921561,  7929586,
  7937569,  7945510,  7953410,  7961268,  7969086,  7976864,  7984602,
  7992301,  7999960,  8007581,  8015164,  8022709,  8030217,  8037687,
  8045121,  8052519,  8059880,  8067206,  8074496,  8081752,  8088973,
  8096159,  8103312,  8110431,  8117516,  8124569,  8131589,  8138576,
  8145532,  8152455,  8159347,  8166208,  8173037,  8179836,  8186605,
  8193343,  8200052,  8206731,  8213380,  8220001,  8226593,  8233156,
  8239690,  8246197,  8252676,  8259127,  8265550,  8271947,  8278316,
  8284659,  8290976,  8297266,  8303530,  8309768,  8315981,  8322168,
  8328330,  8334467,  8340579,  8346667,  8352730,  8358769,  8364784,
  8370775,  8376743,  8382687,  8388608,  8394506,  8400381,  8406233,
  8412062,  8417870,  8423655,  8429418,  8435159,  8440878,  8446576,
  8452252,  8457908,  8463542,  8469155,  8474748,  8480319,  8485871,
  8491402,  8496913,  8502404,  8507875,  8513327,  8518759,  8524171,
  8529564,  8534938,  8540293,  8545629,  8550947,  8556245,  8561525,
  8566787,  8572031,  8577256,  8582464,  8587653,  8592825,  8597980,
  8603116,  8608236,  8613338,  8618423,  8623491,  8628542,  8633576,
  8638593,  8643594,  8648579,  8653547,  8658499,  8663434,  8668354,
  8673258,  8678145,  8683017,  8687874,  8692715,  8697540,  8702350,
  8707145,  8711925,  8716690,  8721439,  8726174,  8730894,  8735599,
  8740290,  8744967,  8749628,  8754276,  8758909,  8763528,  8768134,
  8772725,  8777302,  8781865,  8786415,  8790951,  8795474,  8799983,
  8804478,  8808961,  8813430,  8817886,  8822328,  8826758,  8831175,
  8835579,  8839970,  8844349,  8848715,  8853068,  8857409,  8861737,
  8866053,  8870357,  8874649,  8878928,  8883195,  8887451,  8891694,
  8895926,  8900145,  8904353,  8908550,  8912734,  8916908,  8921069,
  8925220,  8929358,  8933486,  8937603,  8941708,  8945802,  8949885,
  8953957,  8958018,  8962068,  8966108,  8970137,  8974155,  8978162,
  8982159,  8986145,  8990121,  8994086,  8998041,  9001986,  9005920,
  9009844,  9013758,  9017662,  9021556,  9025440,  9029314,  9033178,
  9037032,  9040877,  9044711,  9048536,  9052352,  9056157,  9059953,
  9063740,  9067517,  9071285,  9075044,  9078793,  9082533,  9086263,
  9089985,  9093697,  9097400,  9101095,  9104780,  9108456,  9112123,
  9115782,  9119431,  9123072,  9126704,  9130328,  9133943,  9137549,
  9141146,  9144735,  9148316,  9151888,  9155452,  9159007,  9162554,
  9166092,  9169623,  9173145,  9176659,  9180165,  9183663,  9187152,
  9190634,  9194108,  9197573,  9201031,  9204481,  9207923,  9211357,
  9214784,  9218202,  9221613,  9225017,  9228412,  9231800,  9235181,
  9238554,  9241919,  9245277,  9248628,  9251971,  9255307,  9258635,
  9261956,  9265270,  9268577,  9271876,  9275169,  9278454,  9281732,
  9285002,  9288266,  9291523,  9294773,  9298016,  9301252,  9304481,
  9307703,  9310918,  9314126,  9317328,  9320523,  9323711,  9326892,
  9330067,  9333235,  9336397,  9339552,  9342700,  9345842,  9348977,
  9352106,  9355228,  9358344,  9361454,  9364557,  9367654,  9370744,
  9373828,  9376906,  9379978,  9383043,  9386102,  9389155,  9392202,
  9395243,  9398278,  9401306,  9404329,  9407345,  9410356,  9413360,
  9416359,  9419351,  9422338,  9425319,  9428294,  9431263,  9434226,
  9437184,  9440136,  9443082,  9446022,  9448957,  9451886,  9454809,
  9457726,  9460638,  9463545,  9466446,  9469341,  9472231,  9475115,
  9477994,  9480867,  9483735,  9486597,  9489454,  9492306,  9495152,
  9497993,  9500828,  9503659,  9506484,  9509303,  9512118,  9514927,
  9517731,  9520530,  9523324,  9526112,  9528895,  9531674,  9534447,
  9537215,  9539978,  9542736,  9545489,  9548237,  9550980,  9553718,
  9556451,  9559179,  9561903,  9564621,  9567335,  9570043,  9572747,
  9575446,  9578140,  9580830,  9583514,  9586194,  9588869,  9591540,
  9594205,  9596866,  9599523,  9602174,  9604821,  9607464,  9610101,
  9612735,  9615363,  9617987,  9620607,  9623222,  9625832,  9628438,
  9631040,  9633637,  9636229,  9638818,  9641401,  9643981,  9646556,
  9649126,  9651692,  9654254,  9656812,  9659365,  9661914,  9664459,
  9666999,  9669535,  9672067,  9674594,  9677118,  9679637,  9682152,
  9684663,  9687169,  9689672,  9692170,  9694665,  9697155,  9699641,
  9702123,  9704601,  9707075,  9709545,  9712010,  9714472,  9716930,
  9719384,  9721834,  9724279,  9726721,  9729159,  9731593,  9734024,
  9736450,  9738872,  9741291,  9743705,  9746116,  9748523,  9750926,
  9753326,  9755721,  9758113,  9760501,  9762885,  9765266,  9767642,
  9770015,  9772385,  9774750,  9777112,  9779470,  9781825,  9784175,
  9786523,  9788866,  9791206,  9793543,  9795875,  9798204,  9800530,
  9802852,  9805170,  9807485,  9809797,  9812104,  9814409,  9816710,
  9819007,  9821301,  9823591,  9825878,  9828161,  9830441,  9832718,
  9834991,  9837261,  9839527,  9841790,  9844050,  9846306,  9848559,
  9850808,  9853054,  9855297,  9857537,  9859773,  9862006,  9864235,
  9866462,  9868685,  9870904,  9873121,  9875334,  9877544,  9879751,
  9881955,  9884155,  9886352,  9888546,  9890737,  9892925,  9895109,
  9897291,  9899469,  9901644,  9903816,  9905985,  9908150,  9910313,
  9912473,  9914629,  9916783,  9918933,  9921080,  9923225,  9925366,
  9927504,  9929639,  9931771,  9933900,  9936027,  9938150,  9940270,
  9942387,  9944502,  9946613,  9948721,  9950827,  9952929,  9955029,
  9957126,  9959219,  9961310,  9963398,  9965484,  9967566,  9969645,
  9971722,  9973796,  9975866,  9977934,  9980000,  9982062,  9984122,
  9986179,  9988233,  9990284,  9992332,  9994378,  9996421,  9998461,
  10000498, 10002533, 10004565, 10006594, 10008621, 10010644, 10012665,
  10014684, 10016700, 10018713, 10020723, 10022731, 10024736, 10026738,
  10028738, 10030735, 10032729, 10034721, 10036710, 10038697, 10040681,
  10042662, 10044641, 10046617, 10048591, 10050562, 10052530, 10054496,
  10056459, 10058420, 10060379, 10062334, 10064287, 10066238, 10068186,
  10070132, 10072075, 10074016, 10075954, 10077890, 10079823, 10081754,
  10083682, 10085608, 10087532, 10089453, 10091371, 10093287, 10095201,
  10097112, 10099021, 10100928, 10102832, 10104733, 10106633, 10108529,
  10110424, 10112316, 10114206, 10116093, 10117978, 10119861, 10121742,
  10123620, 10125495, 10127369, 10129240, 10131109, 10132975, 10134839,
  10136701, 10138561, 10140418, 10142273, 10144126, 10145976, 10147825,
  10149671, 10151514, 10153356, 10155195, 10157032, 10158867, 10160699,
  10162530, 10164358, 10166184, 10168007, 10169829, 10171648, 10173465,
  10175280, 10177093, 10178904, 10180712, 10182519, 10184323, 10186125,
  10187925, 10189722, 10191518, 10193311, 10195103, 10196892, 10198679,
  10200464, 10202247, 10204028, 10205806, 10207583, 10209357, 10211130,
  10212900, 10214668, 10216435, 10218199, 10219961, 10221721, 10223479,
  10225235, 10226989, 10228741, 10230491, 10232239, 10233985, 10235728,
  10237470, 10239210, 10240948, 10242684, 10244417, 10246149, 10247879,
  10249607, 10251333, 10253057, 10254779, 10256499, 10258217, 10259933,
  10261647, 10263360, 10265070, 10266778, 10268485, 10270189, 10271892,
  10273593, 10275292, 10276988, 10278683, 10280376, 10282068, 10283757,
  10285444, 10287130, 10288814, 10290495, 10292175, 10293853, 10295530,
  10297204, 10298876, 10300547, 10302216, 10303883, 10305548, 10307211,
  10308873, 10310532, 10312190, 10313846, 10315501, 10317153, 10318804,
  10320452, 10322099, 10323745, 10325388, 10327030, 10328670, 10330308,
  10331944, 10333578, 10335211, 10336842, 10338472, 10340099, 10341725,
  10343349, 10344971, 10346592, 10348210, 10349828, 10351443, 10353057,
  10354668, 10356279, 10357887, 10359494, 10361099, 10362702, 10364304,
  10365904, 10367502, 10369099, 10370694, 10372287, 10373879, 10375468,
  10377057, 10378643, 10380228, 10381811, 10383393, 10384973, 10386551,
  10388128, 10389703, 10391276, 10392848, 10394418, 10395986, 10397553,
  10399118, 10400682, 10402244, 10403804, 10405363, 10406920, 10408476,
  10410030, 10411582, 10413133, 10414682, 10416230, 10417776, 10419320,
  10420863, 10422404, 10423944, 10425482, 10427019, 10428554, 10430087,
  10431619, 10433149, 10434678, 10436206, 10437731, 10439256, 10440778,
  10442299, 10443819, 10445337, 10446854, 10448369, 10449882, 10451394,
  10452905, 10454414, 10455921, 10457427, 10458932, 10460435, 10461936,
  10463436, 10464935, 10466432, 10467927, 10469422, 10470914, 10472405,
  10473895, 10475383, 10476870, 10478355, 10479839, 10481322, 10482802,
  10484282,
};

#define LOG2_PRECISION 20
static int64_t log2_approximation(int64_t v) {
  assert(v > 0);
  if (v < LOG2_TABLE_SIZE) {
    return log2_table[v];
  } else {
    // use linear approximation when v >= 2^10
    const int slope =
        1477;  // slope = 1 / (log(2) * 1024) * (1 << LOG2_PRECISION)
    assert(LOG2_TABLE_SIZE == 1 << 10);

    return slope * (v - LOG2_TABLE_SIZE) + (10 << LOG2_PRECISION);
  }
}

int64_t vp9_nb_mvs_inconsistency(const MV *mv, const int_mv *nb_mvs,
                                 int mv_num) {
  int i;
  int update = 0;
  int64_t best_cost = 0;
  vpx_clear_system_state();
  for (i = 0; i < mv_num; ++i) {
    if (nb_mvs[i].as_int != INVALID_MV) {
      MV nb_mv = nb_mvs[i].as_mv;
      const int64_t row_diff = abs(mv->row - nb_mv.row);
      const int64_t col_diff = abs(mv->col - nb_mv.col);
      const int64_t cost =
          log2_approximation(1 + row_diff * row_diff + col_diff * col_diff);
      if (update == 0) {
        best_cost = cost;
        update = 1;
      } else {
        best_cost = cost < best_cost ? cost : best_cost;
      }
    }
  }
  return best_cost;
}

static int64_t exhaustive_mesh_search_new(const MACROBLOCK *x, MV *best_mv,
                                          int range, int step,
                                          const vp9_variance_fn_ptr_t *fn_ptr,
                                          const MV *center_mv, int lambda,
                                          const int_mv *nb_full_mvs,
                                          int full_mv_num) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  MV fcenter_mv = { center_mv->row, center_mv->col };
  int64_t best_sad;
  int r, c, i;
  int start_col, end_col, start_row, end_row;
  int col_step = (step > 1) ? step : 4;

  assert(step >= 1);

  clamp_mv(&fcenter_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  *best_mv = fcenter_mv;
  best_sad =
      ((int64_t)fn_ptr->sdf(what->buf, what->stride,
                            get_buf_from_mv(in_what, &fcenter_mv),
                            in_what->stride)
       << LOG2_PRECISION) +
      lambda * vp9_nb_mvs_inconsistency(&fcenter_mv, nb_full_mvs, full_mv_num);
  start_row = VPXMAX(-range, x->mv_limits.row_min - fcenter_mv.row);
  start_col = VPXMAX(-range, x->mv_limits.col_min - fcenter_mv.col);
  end_row = VPXMIN(range, x->mv_limits.row_max - fcenter_mv.row);
  end_col = VPXMIN(range, x->mv_limits.col_max - fcenter_mv.col);

  for (r = start_row; r <= end_row; r += step) {
    for (c = start_col; c <= end_col; c += col_step) {
      // Step > 1 means we are not checking every location in this pass.
      if (step > 1) {
        const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c };
        int64_t sad =
            (int64_t)fn_ptr->sdf(what->buf, what->stride,
                                 get_buf_from_mv(in_what, &mv), in_what->stride)
            << LOG2_PRECISION;
        if (sad < best_sad) {
          sad +=
              lambda * vp9_nb_mvs_inconsistency(&mv, nb_full_mvs, full_mv_num);
          if (sad < best_sad) {
            best_sad = sad;
            *best_mv = mv;
          }
        }
      } else {
        // 4 sads in a single call if we are checking every location
        if (c + 3 <= end_col) {
          unsigned int sads[4];
          const uint8_t *addrs[4];
          for (i = 0; i < 4; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            addrs[i] = get_buf_from_mv(in_what, &mv);
          }
          fn_ptr->sdx4df(what->buf, what->stride, addrs, in_what->stride, sads);

          for (i = 0; i < 4; ++i) {
            int64_t sad = (int64_t)sads[i] << LOG2_PRECISION;
            if (sad < best_sad) {
              const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
              sad += lambda *
                     vp9_nb_mvs_inconsistency(&mv, nb_full_mvs, full_mv_num);
              if (sad < best_sad) {
                best_sad = sad;
                *best_mv = mv;
              }
            }
          }
        } else {
          for (i = 0; i < end_col - c; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            int64_t sad = (int64_t)fn_ptr->sdf(what->buf, what->stride,
                                               get_buf_from_mv(in_what, &mv),
                                               in_what->stride)
                          << LOG2_PRECISION;
            if (sad < best_sad) {
              sad += lambda *
                     vp9_nb_mvs_inconsistency(&mv, nb_full_mvs, full_mv_num);
              if (sad < best_sad) {
                best_sad = sad;
                *best_mv = mv;
              }
            }
          }
        }
      }
    }
  }

  return best_sad;
}

static int64_t full_pixel_exhaustive_new(const VP9_COMP *cpi, MACROBLOCK *x,
                                         MV *centre_mv_full,
                                         const vp9_variance_fn_ptr_t *fn_ptr,
                                         MV *dst_mv, int lambda,
                                         const int_mv *nb_full_mvs,
                                         int full_mv_num) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  MV temp_mv = { centre_mv_full->row, centre_mv_full->col };
  int64_t bestsme;
  int i;
  int interval = sf->mesh_patterns[0].interval;
  int range = sf->mesh_patterns[0].range;
  int baseline_interval_divisor;
  const MV dummy_mv = { 0, 0 };

  // Trap illegal values for interval and range for this function.
  if ((range < MIN_RANGE) || (range > MAX_RANGE) || (interval < MIN_INTERVAL) ||
      (interval > range)) {
    printf("ERROR: invalid range\n");
    assert(0);
  }

  baseline_interval_divisor = range / interval;

  // Check size of proposed first range against magnitude of the centre
  // value used as a starting point.
  range = VPXMAX(range, (5 * VPXMAX(abs(temp_mv.row), abs(temp_mv.col))) / 4);
  range = VPXMIN(range, MAX_RANGE);
  interval = VPXMAX(interval, range / baseline_interval_divisor);

  // initial search
  bestsme =
      exhaustive_mesh_search_new(x, &temp_mv, range, interval, fn_ptr, &temp_mv,
                                 lambda, nb_full_mvs, full_mv_num);

  if ((interval > MIN_INTERVAL) && (range > MIN_RANGE)) {
    // Progressive searches with range and step size decreasing each time
    // till we reach a step size of 1. Then break out.
    for (i = 1; i < MAX_MESH_STEP; ++i) {
      // First pass with coarser step and longer range
      bestsme = exhaustive_mesh_search_new(
          x, &temp_mv, sf->mesh_patterns[i].range,
          sf->mesh_patterns[i].interval, fn_ptr, &temp_mv, lambda, nb_full_mvs,
          full_mv_num);

      if (sf->mesh_patterns[i].interval == 1) break;
    }
  }

  bestsme = vp9_get_mvpred_var(x, &temp_mv, &dummy_mv, fn_ptr, 0);
  *dst_mv = temp_mv;

  return bestsme;
}

static double diamond_search_sad_new(const MACROBLOCK *x,
                                     const search_site_config *cfg,
                                     const MV *init_full_mv, MV *best_full_mv,
                                     int search_param, int lambda, int *num00,
                                     const vp9_variance_fn_ptr_t *fn_ptr,
                                     const int_mv *nb_full_mvs,
                                     int full_mv_num) {
  int i, j, step;

  const MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address;

  double bestsad;
  int best_site = -1;
  int last_site = -1;

  // search_param determines the length of the initial step and hence the number
  // of iterations.
  // 0 = initial step (MAX_FIRST_STEP) pel
  // 1 = (MAX_FIRST_STEP/2) pel,
  // 2 = (MAX_FIRST_STEP/4) pel...
  //  const search_site *ss = &cfg->ss[search_param * cfg->searches_per_step];
  const MV *ss_mv = &cfg->ss_mv[search_param * cfg->searches_per_step];
  const intptr_t *ss_os = &cfg->ss_os[search_param * cfg->searches_per_step];
  const int tot_steps = cfg->total_steps - search_param;
  vpx_clear_system_state();

  *best_full_mv = *init_full_mv;
  clamp_mv(best_full_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  *num00 = 0;

  // Work out the start point for the search
  in_what = xd->plane[0].pre[0].buf + best_full_mv->row * in_what_stride +
            best_full_mv->col;
  best_address = in_what;

  // Check the starting position
  {
    const double mv_dist =
        fn_ptr->sdf(what, what_stride, in_what, in_what_stride);
    const double mv_cost =
        vp9_nb_mvs_inconsistency(best_full_mv, nb_full_mvs, full_mv_num) /
        (double)(1 << LOG2_PRECISION);
    bestsad = mv_dist + lambda * mv_cost;
  }

  i = 0;

  for (step = 0; step < tot_steps; step++) {
    int all_in = 1, t;

    // All_in is true if every one of the points we are checking are within
    // the bounds of the image.
    all_in &= ((best_full_mv->row + ss_mv[i].row) > x->mv_limits.row_min);
    all_in &= ((best_full_mv->row + ss_mv[i + 1].row) < x->mv_limits.row_max);
    all_in &= ((best_full_mv->col + ss_mv[i + 2].col) > x->mv_limits.col_min);
    all_in &= ((best_full_mv->col + ss_mv[i + 3].col) < x->mv_limits.col_max);

    // If all the pixels are within the bounds we don't check whether the
    // search point is valid in this loop,  otherwise we check each point
    // for validity..
    if (all_in) {
      unsigned int sad_array[4];

      for (j = 0; j < cfg->searches_per_step; j += 4) {
        unsigned char const *block_offset[4];

        for (t = 0; t < 4; t++) block_offset[t] = ss_os[i + t] + best_address;

        fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride,
                       sad_array);

        for (t = 0; t < 4; t++, i++) {
          if (sad_array[t] < bestsad) {
            const MV this_mv = { best_full_mv->row + ss_mv[i].row,
                                 best_full_mv->col + ss_mv[i].col };
            const double mv_dist = sad_array[t];
            const double mv_cost =
                vp9_nb_mvs_inconsistency(&this_mv, nb_full_mvs, full_mv_num) /
                (double)(1 << LOG2_PRECISION);
            double thissad = mv_dist + lambda * mv_cost;
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = i;
            }
          }
        }
      }
    } else {
      for (j = 0; j < cfg->searches_per_step; j++) {
        // Trap illegal vectors
        const MV this_mv = { best_full_mv->row + ss_mv[i].row,
                             best_full_mv->col + ss_mv[i].col };

        if (is_mv_in(&x->mv_limits, &this_mv)) {
          const uint8_t *const check_here = ss_os[i] + best_address;
          const double mv_dist =
              fn_ptr->sdf(what, what_stride, check_here, in_what_stride);
          if (mv_dist < bestsad) {
            const double mv_cost =
                vp9_nb_mvs_inconsistency(&this_mv, nb_full_mvs, full_mv_num) /
                (double)(1 << LOG2_PRECISION);
            double thissad = mv_dist + lambda * mv_cost;
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = i;
            }
          }
        }
        i++;
      }
    }
    if (best_site != last_site) {
      best_full_mv->row += ss_mv[best_site].row;
      best_full_mv->col += ss_mv[best_site].col;
      best_address += ss_os[best_site];
      last_site = best_site;
    } else if (best_address == in_what) {
      (*num00)++;
    }
  }
  return bestsad;
}

void vp9_prepare_nb_full_mvs(const TplDepFrame *tpl_frame, int mi_row,
                             int mi_col, int rf_idx, BLOCK_SIZE bsize,
                             int_mv *nb_full_mvs) {
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int dirs[NB_MVS_NUM][2] = { { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 } };
  int i;
  for (i = 0; i < NB_MVS_NUM; ++i) {
    int r = dirs[i][0] * mi_height;
    int c = dirs[i][1] * mi_width;
    if (mi_row + r >= 0 && mi_row + r < tpl_frame->mi_rows && mi_col + c >= 0 &&
        mi_col + c < tpl_frame->mi_cols) {
      const TplDepStats *tpl_ptr =
          &tpl_frame
               ->tpl_stats_ptr[(mi_row + r) * tpl_frame->stride + mi_col + c];
      int_mv *mv =
          get_pyramid_mv(tpl_frame, rf_idx, bsize, mi_row + r, mi_col + c);
      if (tpl_ptr->ready[rf_idx]) {
        nb_full_mvs[i].as_mv = get_full_mv(&mv->as_mv);
      } else {
        nb_full_mvs[i].as_int = INVALID_MV;
      }
    } else {
      nb_full_mvs[i].as_int = INVALID_MV;
    }
  }
}
#endif  // CONFIG_NON_GREEDY_MV

int vp9_diamond_search_sad_c(const MACROBLOCK *x, const search_site_config *cfg,
                             MV *ref_mv, MV *best_mv, int search_param,
                             int sad_per_bit, int *num00,
                             const vp9_variance_fn_ptr_t *fn_ptr,
                             const MV *center_mv) {
  int i, j, step;

  const MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address;

  unsigned int bestsad = INT_MAX;
  int best_site = -1;
  int last_site = -1;

  int ref_row;
  int ref_col;

  // search_param determines the length of the initial step and hence the number
  // of iterations.
  // 0 = initial step (MAX_FIRST_STEP) pel
  // 1 = (MAX_FIRST_STEP/2) pel,
  // 2 = (MAX_FIRST_STEP/4) pel...
  //  const search_site *ss = &cfg->ss[search_param * cfg->searches_per_step];
  const MV *ss_mv = &cfg->ss_mv[search_param * cfg->searches_per_step];
  const intptr_t *ss_os = &cfg->ss_os[search_param * cfg->searches_per_step];
  const int tot_steps = cfg->total_steps - search_param;

  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  clamp_mv(ref_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  ref_row = ref_mv->row;
  ref_col = ref_mv->col;
  *num00 = 0;
  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Work out the start point for the search
  in_what = xd->plane[0].pre[0].buf + ref_row * in_what_stride + ref_col;
  best_address = in_what;

  // Check the starting position
  bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride) +
            mvsad_err_cost(x, best_mv, &fcenter_mv, sad_per_bit);

  i = 0;

  for (step = 0; step < tot_steps; step++) {
    int all_in = 1, t;

    // All_in is true if every one of the points we are checking are within
    // the bounds of the image.
    all_in &= ((best_mv->row + ss_mv[i].row) > x->mv_limits.row_min);
    all_in &= ((best_mv->row + ss_mv[i + 1].row) < x->mv_limits.row_max);
    all_in &= ((best_mv->col + ss_mv[i + 2].col) > x->mv_limits.col_min);
    all_in &= ((best_mv->col + ss_mv[i + 3].col) < x->mv_limits.col_max);

    // If all the pixels are within the bounds we don't check whether the
    // search point is valid in this loop,  otherwise we check each point
    // for validity..
    if (all_in) {
      unsigned int sad_array[4];

      for (j = 0; j < cfg->searches_per_step; j += 4) {
        unsigned char const *block_offset[4];

        for (t = 0; t < 4; t++) block_offset[t] = ss_os[i + t] + best_address;

        fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride,
                       sad_array);

        for (t = 0; t < 4; t++, i++) {
          if (sad_array[t] < bestsad) {
            const MV this_mv = { best_mv->row + ss_mv[i].row,
                                 best_mv->col + ss_mv[i].col };
            sad_array[t] +=
                mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (sad_array[t] < bestsad) {
              bestsad = sad_array[t];
              best_site = i;
            }
          }
        }
      }
    } else {
      for (j = 0; j < cfg->searches_per_step; j++) {
        // Trap illegal vectors
        const MV this_mv = { best_mv->row + ss_mv[i].row,
                             best_mv->col + ss_mv[i].col };

        if (is_mv_in(&x->mv_limits, &this_mv)) {
          const uint8_t *const check_here = ss_os[i] + best_address;
          unsigned int thissad =
              fn_ptr->sdf(what, what_stride, check_here, in_what_stride);

          if (thissad < bestsad) {
            thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = i;
            }
          }
        }
        i++;
      }
    }
    if (best_site != last_site) {
      best_mv->row += ss_mv[best_site].row;
      best_mv->col += ss_mv[best_site].col;
      best_address += ss_os[best_site];
      last_site = best_site;
#if defined(NEW_DIAMOND_SEARCH)
      while (1) {
        const MV this_mv = { best_mv->row + ss_mv[best_site].row,
                             best_mv->col + ss_mv[best_site].col };
        if (is_mv_in(&x->mv_limits, &this_mv)) {
          const uint8_t *const check_here = ss_os[best_site] + best_address;
          unsigned int thissad =
              fn_ptr->sdf(what, what_stride, check_here, in_what_stride);
          if (thissad < bestsad) {
            thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_mv->row += ss_mv[best_site].row;
              best_mv->col += ss_mv[best_site].col;
              best_address += ss_os[best_site];
              continue;
            }
          }
        }
        break;
      }
#endif
    } else if (best_address == in_what) {
      (*num00)++;
    }
  }
  return bestsad;
}

static int vector_match(int16_t *ref, int16_t *src, int bwl) {
  int best_sad = INT_MAX;
  int this_sad;
  int d;
  int center, offset = 0;
  int bw = 4 << bwl;  // redundant variable, to be changed in the experiments.
  for (d = 0; d <= bw; d += 16) {
    this_sad = vpx_vector_var(&ref[d], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      offset = d;
    }
  }
  center = offset;

  for (d = -8; d <= 8; d += 16) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -4; d <= 4; d += 8) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -2; d <= 2; d += 4) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -1; d <= 1; d += 2) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }

  return (center - (bw >> 1));
}

static const MV search_pos[4] = {
  { -1, 0 },
  { 0, -1 },
  { 0, 1 },
  { 1, 0 },
};

unsigned int vp9_int_pro_motion_estimation(const VP9_COMP *cpi, MACROBLOCK *x,
                                           BLOCK_SIZE bsize, int mi_row,
                                           int mi_col, const MV *ref_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi[0];
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0 } };
  DECLARE_ALIGNED(16, int16_t, hbuf[128]);
  DECLARE_ALIGNED(16, int16_t, vbuf[128]);
  DECLARE_ALIGNED(16, int16_t, src_hbuf[64]);
  DECLARE_ALIGNED(16, int16_t, src_vbuf[64]);
  int idx;
  const int bw = 4 << b_width_log2_lookup[bsize];
  const int bh = 4 << b_height_log2_lookup[bsize];
  const int search_width = bw << 1;
  const int search_height = bh << 1;
  const int src_stride = x->plane[0].src.stride;
  const int ref_stride = xd->plane[0].pre[0].stride;
  uint8_t const *ref_buf, *src_buf;
  MV *tmp_mv = &xd->mi[0]->mv[0].as_mv;
  unsigned int best_sad, tmp_sad, this_sad[4];
  MV this_mv;
  const int norm_factor = 3 + (bw >> 5);
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      vp9_get_scaled_ref_frame(cpi, mi->ref_frame[0]);
  MvLimits subpel_mv_limits;

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++) backup_yv12[i] = xd->plane[i].pre[0];
    vp9_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
  }

#if CONFIG_VP9_HIGHBITDEPTH
  // TODO(jingning): Implement integral projection functions for high bit-depth
  // setting and remove this part of code.
  if (xd->bd != 8) {
    unsigned int this_sad;
    tmp_mv->row = 0;
    tmp_mv->col = 0;
    this_sad = cpi->fn_ptr[bsize].sdf(x->plane[0].src.buf, src_stride,
                                      xd->plane[0].pre[0].buf, ref_stride);

    if (scaled_ref_frame) {
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++) xd->plane[i].pre[0] = backup_yv12[i];
    }
    return this_sad;
  }
#endif

  // Set up prediction 1-D reference set
  ref_buf = xd->plane[0].pre[0].buf - (bw >> 1);
  for (idx = 0; idx < search_width; idx += 16) {
    vpx_int_pro_row(&hbuf[idx], ref_buf, ref_stride, bh);
    ref_buf += 16;
  }

  ref_buf = xd->plane[0].pre[0].buf - (bh >> 1) * ref_stride;
  for (idx = 0; idx < search_height; ++idx) {
    vbuf[idx] = vpx_int_pro_col(ref_buf, bw) >> norm_factor;
    ref_buf += ref_stride;
  }

  // Set up src 1-D reference set
  for (idx = 0; idx < bw; idx += 16) {
    src_buf = x->plane[0].src.buf + idx;
    vpx_int_pro_row(&src_hbuf[idx], src_buf, src_stride, bh);
  }

  src_buf = x->plane[0].src.buf;
  for (idx = 0; idx < bh; ++idx) {
    src_vbuf[idx] = vpx_int_pro_col(src_buf, bw) >> norm_factor;
    src_buf += src_stride;
  }

  // Find the best match per 1-D search
  tmp_mv->col = vector_match(hbuf, src_hbuf, b_width_log2_lookup[bsize]);
  tmp_mv->row = vector_match(vbuf, src_vbuf, b_height_log2_lookup[bsize]);

  this_mv = *tmp_mv;
  src_buf = x->plane[0].src.buf;
  ref_buf = xd->plane[0].pre[0].buf + this_mv.row * ref_stride + this_mv.col;
  best_sad = cpi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);

  {
    const uint8_t *const pos[4] = {
      ref_buf - ref_stride,
      ref_buf - 1,
      ref_buf + 1,
      ref_buf + ref_stride,
    };

    cpi->fn_ptr[bsize].sdx4df(src_buf, src_stride, pos, ref_stride, this_sad);
  }

  for (idx = 0; idx < 4; ++idx) {
    if (this_sad[idx] < best_sad) {
      best_sad = this_sad[idx];
      tmp_mv->row = search_pos[idx].row + this_mv.row;
      tmp_mv->col = search_pos[idx].col + this_mv.col;
    }
  }

  if (this_sad[0] < this_sad[3])
    this_mv.row -= 1;
  else
    this_mv.row += 1;

  if (this_sad[1] < this_sad[2])
    this_mv.col -= 1;
  else
    this_mv.col += 1;

  ref_buf = xd->plane[0].pre[0].buf + this_mv.row * ref_stride + this_mv.col;

  tmp_sad = cpi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);
  if (best_sad > tmp_sad) {
    *tmp_mv = this_mv;
    best_sad = tmp_sad;
  }

  tmp_mv->row *= 8;
  tmp_mv->col *= 8;

  vp9_set_subpel_mv_search_range(&subpel_mv_limits, &x->mv_limits, ref_mv);
  clamp_mv(tmp_mv, subpel_mv_limits.col_min, subpel_mv_limits.col_max,
           subpel_mv_limits.row_min, subpel_mv_limits.row_max);

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++) xd->plane[i].pre[0] = backup_yv12[i];
  }

  return best_sad;
}

#if CONFIG_NON_GREEDY_MV
// Runs sequence of diamond searches in smaller steps for RD.
/* do_refine: If last step (1-away) of n-step search doesn't pick the center
              point as the best match, we will do a final 1-away diamond
              refining search  */
double vp9_full_pixel_diamond_new(const VP9_COMP *cpi, MACROBLOCK *x,
                                  MV *mvp_full, int step_param, int lambda,
                                  int do_refine,
                                  const vp9_variance_fn_ptr_t *fn_ptr,
                                  const int_mv *nb_full_mvs, int full_mv_num,
                                  MV *best_mv) {
  int n, num00 = 0;
  double thissme;
  double bestsme;
  const int further_steps = MAX_MVSEARCH_STEPS - 1 - step_param;
  const MV center_mv = { 0, 0 };
  vpx_clear_system_state();
  bestsme =
      diamond_search_sad_new(x, &cpi->ss_cfg, mvp_full, best_mv, step_param,
                             lambda, &n, fn_ptr, nb_full_mvs, full_mv_num);

  bestsme = vp9_get_mvpred_var(x, best_mv, &center_mv, fn_ptr, 0);

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  if (n > further_steps) do_refine = 0;

  while (n < further_steps) {
    ++n;
    if (num00) {
      num00--;
    } else {
      MV temp_mv;
      thissme = diamond_search_sad_new(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                       step_param + n, lambda, &num00, fn_ptr,
                                       nb_full_mvs, full_mv_num);
      thissme = vp9_get_mvpred_var(x, &temp_mv, &center_mv, fn_ptr, 0);
      // check to see if refining search is needed.
      if (num00 > further_steps - n) do_refine = 0;

      if (thissme < bestsme) {
        bestsme = thissme;
        *best_mv = temp_mv;
      }
    }
  }

  // final 1-away diamond refining search
  if (do_refine) {
    const int search_range = 8;
    MV temp_mv = *best_mv;
    thissme = vp9_refining_search_sad_new(x, &temp_mv, lambda, search_range,
                                          fn_ptr, nb_full_mvs, full_mv_num);
    thissme = vp9_get_mvpred_var(x, &temp_mv, &center_mv, fn_ptr, 0);
    if (thissme < bestsme) {
      bestsme = thissme;
      *best_mv = temp_mv;
    }
  }

  bestsme = (double)full_pixel_exhaustive_new(cpi, x, best_mv, fn_ptr, best_mv,
                                              lambda, nb_full_mvs, full_mv_num);
  return bestsme;
}
#endif  // CONFIG_NON_GREEDY_MV

// Runs sequence of diamond searches in smaller steps for RD.
/* do_refine: If last step (1-away) of n-step search doesn't pick the center
              point as the best match, we will do a final 1-away diamond
              refining search  */
static int full_pixel_diamond(const VP9_COMP *const cpi,
                              const MACROBLOCK *const x, MV *mvp_full,
                              int step_param, int sadpb, int further_steps,
                              int do_refine, int *cost_list,
                              const vp9_variance_fn_ptr_t *fn_ptr,
                              const MV *ref_mv, MV *dst_mv) {
  MV temp_mv;
  int thissme, n, num00 = 0;
  int bestsme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param, sadpb, &n, fn_ptr, ref_mv);
  if (bestsme < INT_MAX)
    bestsme = vp9_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);
  *dst_mv = temp_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  if (n > further_steps) do_refine = 0;

  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      thissme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param + n, sadpb, &num00, fn_ptr,
                                        ref_mv);
      if (thissme < INT_MAX)
        thissme = vp9_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);

      // check to see if refining search is needed.
      if (num00 > further_steps - n) do_refine = 0;

      if (thissme < bestsme) {
        bestsme = thissme;
        *dst_mv = temp_mv;
      }
    }
  }

  // final 1-away diamond refining search
  if (do_refine) {
    const int search_range = 8;
    MV best_mv = *dst_mv;
    thissme = vp9_refining_search_sad(x, &best_mv, sadpb, search_range, fn_ptr,
                                      ref_mv);
    if (thissme < INT_MAX)
      thissme = vp9_get_mvpred_var(x, &best_mv, ref_mv, fn_ptr, 1);
    if (thissme < bestsme) {
      bestsme = thissme;
      *dst_mv = best_mv;
    }
  }

  // Return cost list.
  if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, dst_mv, cost_list);
  }
  return bestsme;
}

// Runs an limited range exhaustive mesh search using a pattern set
// according to the encode speed profile.
static int full_pixel_exhaustive(const VP9_COMP *const cpi,
                                 const MACROBLOCK *const x, MV *centre_mv_full,
                                 int sadpb, int *cost_list,
                                 const vp9_variance_fn_ptr_t *fn_ptr,
                                 const MV *ref_mv, MV *dst_mv) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  MV temp_mv = { centre_mv_full->row, centre_mv_full->col };
  MV f_ref_mv = { ref_mv->row >> 3, ref_mv->col >> 3 };
  int bestsme;
  int i;
  int interval = sf->mesh_patterns[0].interval;
  int range = sf->mesh_patterns[0].range;
  int baseline_interval_divisor;

  // Trap illegal values for interval and range for this function.
  if ((range < MIN_RANGE) || (range > MAX_RANGE) || (interval < MIN_INTERVAL) ||
      (interval > range))
    return INT_MAX;

  baseline_interval_divisor = range / interval;

  // Check size of proposed first range against magnitude of the centre
  // value used as a starting point.
  range = VPXMAX(range, (5 * VPXMAX(abs(temp_mv.row), abs(temp_mv.col))) / 4);
  range = VPXMIN(range, MAX_RANGE);
  interval = VPXMAX(interval, range / baseline_interval_divisor);

  // initial search
  bestsme = exhaustive_mesh_search(x, &f_ref_mv, &temp_mv, range, interval,
                                   sadpb, fn_ptr, &temp_mv);

  if ((interval > MIN_INTERVAL) && (range > MIN_RANGE)) {
    // Progressive searches with range and step size decreasing each time
    // till we reach a step size of 1. Then break out.
    for (i = 1; i < MAX_MESH_STEP; ++i) {
      // First pass with coarser step and longer range
      bestsme = exhaustive_mesh_search(
          x, &f_ref_mv, &temp_mv, sf->mesh_patterns[i].range,
          sf->mesh_patterns[i].interval, sadpb, fn_ptr, &temp_mv);

      if (sf->mesh_patterns[i].interval == 1) break;
    }
  }

  if (bestsme < INT_MAX)
    bestsme = vp9_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);
  *dst_mv = temp_mv;

  // Return cost list.
  if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, dst_mv, cost_list);
  }
  return bestsme;
}

#if CONFIG_NON_GREEDY_MV
double vp9_refining_search_sad_new(const MACROBLOCK *x, MV *best_full_mv,
                                   int lambda, int search_range,
                                   const vp9_variance_fn_ptr_t *fn_ptr,
                                   const int_mv *nb_full_mvs, int full_mv_num) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MV neighbors[4] = { { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const uint8_t *best_address = get_buf_from_mv(in_what, best_full_mv);
  double best_sad;
  int i, j;
  vpx_clear_system_state();
  {
    const double mv_dist =
        fn_ptr->sdf(what->buf, what->stride, best_address, in_what->stride);
    const double mv_cost =
        vp9_nb_mvs_inconsistency(best_full_mv, nb_full_mvs, full_mv_num) /
        (double)(1 << LOG2_PRECISION);
    best_sad = mv_dist + lambda * mv_cost;
  }

  for (i = 0; i < search_range; i++) {
    int best_site = -1;
    const int all_in = ((best_full_mv->row - 1) > x->mv_limits.row_min) &
                       ((best_full_mv->row + 1) < x->mv_limits.row_max) &
                       ((best_full_mv->col - 1) > x->mv_limits.col_min) &
                       ((best_full_mv->col + 1) < x->mv_limits.col_max);

    if (all_in) {
      unsigned int sads[4];
      const uint8_t *const positions[4] = { best_address - in_what->stride,
                                            best_address - 1, best_address + 1,
                                            best_address + in_what->stride };

      fn_ptr->sdx4df(what->buf, what->stride, positions, in_what->stride, sads);

      for (j = 0; j < 4; ++j) {
        const MV mv = { best_full_mv->row + neighbors[j].row,
                        best_full_mv->col + neighbors[j].col };
        const double mv_dist = sads[j];
        const double mv_cost =
            vp9_nb_mvs_inconsistency(&mv, nb_full_mvs, full_mv_num) /
            (double)(1 << LOG2_PRECISION);
        const double thissad = mv_dist + lambda * mv_cost;
        if (thissad < best_sad) {
          best_sad = thissad;
          best_site = j;
        }
      }
    } else {
      for (j = 0; j < 4; ++j) {
        const MV mv = { best_full_mv->row + neighbors[j].row,
                        best_full_mv->col + neighbors[j].col };

        if (is_mv_in(&x->mv_limits, &mv)) {
          const double mv_dist =
              fn_ptr->sdf(what->buf, what->stride,
                          get_buf_from_mv(in_what, &mv), in_what->stride);
          const double mv_cost =
              vp9_nb_mvs_inconsistency(&mv, nb_full_mvs, full_mv_num) /
              (double)(1 << LOG2_PRECISION);
          const double thissad = mv_dist + lambda * mv_cost;
          if (thissad < best_sad) {
            best_sad = thissad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      best_full_mv->row += neighbors[best_site].row;
      best_full_mv->col += neighbors[best_site].col;
      best_address = get_buf_from_mv(in_what, best_full_mv);
    }
  }

  return best_sad;
}
#endif  // CONFIG_NON_GREEDY_MV

int vp9_refining_search_sad(const MACROBLOCK *x, MV *ref_mv, int error_per_bit,
                            int search_range,
                            const vp9_variance_fn_ptr_t *fn_ptr,
                            const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MV neighbors[4] = { { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  const uint8_t *best_address = get_buf_from_mv(in_what, ref_mv);
  unsigned int best_sad =
      fn_ptr->sdf(what->buf, what->stride, best_address, in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
  int i, j;

  for (i = 0; i < search_range; i++) {
    int best_site = -1;
    const int all_in = ((ref_mv->row - 1) > x->mv_limits.row_min) &
                       ((ref_mv->row + 1) < x->mv_limits.row_max) &
                       ((ref_mv->col - 1) > x->mv_limits.col_min) &
                       ((ref_mv->col + 1) < x->mv_limits.col_max);

    if (all_in) {
      unsigned int sads[4];
      const uint8_t *const positions[4] = { best_address - in_what->stride,
                                            best_address - 1, best_address + 1,
                                            best_address + in_what->stride };

      fn_ptr->sdx4df(what->buf, what->stride, positions, in_what->stride, sads);

      for (j = 0; j < 4; ++j) {
        if (sads[j] < best_sad) {
          const MV mv = { ref_mv->row + neighbors[j].row,
                          ref_mv->col + neighbors[j].col };
          sads[j] += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
          if (sads[j] < best_sad) {
            best_sad = sads[j];
            best_site = j;
          }
        }
      }
    } else {
      for (j = 0; j < 4; ++j) {
        const MV mv = { ref_mv->row + neighbors[j].row,
                        ref_mv->col + neighbors[j].col };

        if (is_mv_in(&x->mv_limits, &mv)) {
          unsigned int sad =
              fn_ptr->sdf(what->buf, what->stride,
                          get_buf_from_mv(in_what, &mv), in_what->stride);
          if (sad < best_sad) {
            sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              best_site = j;
            }
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      ref_mv->row += neighbors[best_site].row;
      ref_mv->col += neighbors[best_site].col;
      best_address = get_buf_from_mv(in_what, ref_mv);
    }
  }

  return best_sad;
}

// This function is called when we do joint motion search in comp_inter_inter
// mode.
int vp9_refining_search_8p_c(const MACROBLOCK *x, MV *ref_mv, int error_per_bit,
                             int search_range,
                             const vp9_variance_fn_ptr_t *fn_ptr,
                             const MV *center_mv, const uint8_t *second_pred) {
  const MV neighbors[8] = { { -1, 0 },  { 0, -1 }, { 0, 1 },  { 1, 0 },
                            { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  unsigned int best_sad = INT_MAX;
  int i, j;
  clamp_mv(ref_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  best_sad =
      fn_ptr->sdaf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv),
                   in_what->stride, second_pred) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);

  for (i = 0; i < search_range; ++i) {
    int best_site = -1;

    for (j = 0; j < 8; ++j) {
      const MV mv = { ref_mv->row + neighbors[j].row,
                      ref_mv->col + neighbors[j].col };

      if (is_mv_in(&x->mv_limits, &mv)) {
        unsigned int sad =
            fn_ptr->sdaf(what->buf, what->stride, get_buf_from_mv(in_what, &mv),
                         in_what->stride, second_pred);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      ref_mv->row += neighbors[best_site].row;
      ref_mv->col += neighbors[best_site].col;
    }
  }
  return best_sad;
}

int vp9_full_pixel_search(const VP9_COMP *const cpi, const MACROBLOCK *const x,
                          BLOCK_SIZE bsize, MV *mvp_full, int step_param,
                          int search_method, int error_per_bit, int *cost_list,
                          const MV *ref_mv, MV *tmp_mv, int var_max, int rd) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const SEARCH_METHODS method = (SEARCH_METHODS)search_method;
  const vp9_variance_fn_ptr_t *fn_ptr = &cpi->fn_ptr[bsize];
  int var = 0;
  int run_exhaustive_search = 0;

  if (cost_list) {
    cost_list[0] = INT_MAX;
    cost_list[1] = INT_MAX;
    cost_list[2] = INT_MAX;
    cost_list[3] = INT_MAX;
    cost_list[4] = INT_MAX;
  }

  switch (method) {
    case FAST_DIAMOND:
      var = fast_dia_search(x, mvp_full, step_param, error_per_bit, 0,
                            cost_list, fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case FAST_HEX:
      var = fast_hex_search(x, mvp_full, step_param, error_per_bit, 0,
                            cost_list, fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case HEX:
      var = hex_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
                       fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case SQUARE:
      var = square_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
                          fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case BIGDIA:
      var = bigdia_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
                          fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case NSTEP:
    case MESH:
      var = full_pixel_diamond(cpi, x, mvp_full, step_param, error_per_bit,
                               MAX_MVSEARCH_STEPS - 1 - step_param, 1,
                               cost_list, fn_ptr, ref_mv, tmp_mv);
      break;
    default: assert(0 && "Unknown search method");
  }

  if (method == NSTEP) {
    if (sf->exhaustive_searches_thresh < INT_MAX &&
        !cpi->rc.is_src_frame_alt_ref) {
      const int64_t exhaustive_thr =
          sf->exhaustive_searches_thresh >>
          (8 - (b_width_log2_lookup[bsize] + b_height_log2_lookup[bsize]));
      if (var > exhaustive_thr) run_exhaustive_search = 1;
    }
  } else if (method == MESH) {
    run_exhaustive_search = 1;
  }

  if (run_exhaustive_search) {
    int var_ex;
    MV tmp_mv_ex;
    var_ex = full_pixel_exhaustive(cpi, x, tmp_mv, error_per_bit, cost_list,
                                   fn_ptr, ref_mv, &tmp_mv_ex);
    if (var_ex < var) {
      var = var_ex;
      *tmp_mv = tmp_mv_ex;
    }
  }

  if (method != NSTEP && method != MESH && rd && var < var_max)
    var = vp9_get_mvpred_var(x, tmp_mv, ref_mv, fn_ptr, 1);

  return var;
}

// Note(yunqingwang): The following 2 functions are only used in the motion
// vector unit test, which return extreme motion vectors allowed by the MV
// limits.
#define COMMON_MV_TEST \
  SETUP_SUBPEL_SEARCH; \
                       \
  (void)error_per_bit; \
  (void)vfp;           \
  (void)z;             \
  (void)src_stride;    \
  (void)y;             \
  (void)y_stride;      \
  (void)second_pred;   \
  (void)w;             \
  (void)h;             \
  (void)offset;        \
  (void)mvjcost;       \
  (void)mvcost;        \
  (void)sse1;          \
  (void)distortion;    \
                       \
  (void)halfiters;     \
  (void)quarteriters;  \
  (void)eighthiters;   \
  (void)whichdir;      \
  (void)allow_hp;      \
  (void)forced_stop;   \
  (void)hstep;         \
  (void)rr;            \
  (void)rc;            \
                       \
  (void)tr;            \
  (void)tc;            \
  (void)sse;           \
  (void)thismse;       \
  (void)cost_list;     \
  (void)use_accurate_subpel_search;

// Return the maximum MV.
uint32_t vp9_return_max_sub_pixel_mv(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp, int forced_stop,
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h, int use_accurate_subpel_search) {
  COMMON_MV_TEST;

  (void)minr;
  (void)minc;

  bestmv->row = maxr;
  bestmv->col = maxc;
  besterr = 0;

  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp && use_mv_hp(ref_mv));

  return besterr;
}
// Return the minimum MV.
uint32_t vp9_return_min_sub_pixel_mv(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp, int forced_stop,
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h, int use_accurate_subpel_search) {
  COMMON_MV_TEST;

  (void)maxr;
  (void)maxc;

  bestmv->row = minr;
  bestmv->col = minc;
  besterr = 0;

  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp && use_mv_hp(ref_mv));

  return besterr;
}
