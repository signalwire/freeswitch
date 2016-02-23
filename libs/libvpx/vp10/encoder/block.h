/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_ENCODER_BLOCK_H_
#define VP10_ENCODER_BLOCK_H_

#include "vp10/common/entropymv.h"
#include "vp10/common/entropy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int sse;
  int sum;
  unsigned int var;
} diff;

struct macroblock_plane {
  DECLARE_ALIGNED(16, int16_t, src_diff[64 * 64]);
  tran_low_t *qcoeff;
  tran_low_t *coeff;
  uint16_t *eobs;
  struct buf_2d src;

  // Quantizer setings
  int16_t *quant_fp;
  int16_t *round_fp;
  int16_t *quant;
  int16_t *quant_shift;
  int16_t *zbin;
  int16_t *round;

  int64_t quant_thred[2];
};

/* The [2] dimension is for whether we skip the EOB node (i.e. if previous
 * coefficient in this block was zero) or not. */
typedef unsigned int vp10_coeff_cost[PLANE_TYPES][REF_TYPES][COEF_BANDS][2]
                                   [COEFF_CONTEXTS][ENTROPY_TOKENS];

typedef struct {
  int_mv ref_mvs[MAX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
  uint8_t mode_context[MAX_REF_FRAMES];
} MB_MODE_INFO_EXT;

typedef struct macroblock MACROBLOCK;
struct macroblock {
  struct macroblock_plane plane[MAX_MB_PLANE];

  MACROBLOCKD e_mbd;
  MB_MODE_INFO_EXT *mbmi_ext;
  int skip_block;
  int select_tx_size;
  int skip_recode;
  int skip_optimize;
  int q_index;

  int errorperbit;
  int sadperbit16;
  int sadperbit4;
  int rddiv;
  int rdmult;
  int mb_energy;
  int * m_search_count_ptr;
  int * ex_search_count_ptr;

  // These are set to their default values at the beginning, and then adjusted
  // further in the encoding process.
  BLOCK_SIZE min_partition_size;
  BLOCK_SIZE max_partition_size;

  int mv_best_ref_index[MAX_REF_FRAMES];
  unsigned int max_mv_context[MAX_REF_FRAMES];
  unsigned int source_variance;
  unsigned int pred_sse[MAX_REF_FRAMES];
  int pred_mv_sad[MAX_REF_FRAMES];

  int nmvjointcost[MV_JOINTS];
  int *nmvcost[2];
  int *nmvcost_hp[2];
  int **mvcost;

  int nmvjointsadcost[MV_JOINTS];
  int *nmvsadcost[2];
  int *nmvsadcost_hp[2];
  int **mvsadcost;

  // These define limits to motion vector components to prevent them
  // from extending outside the UMV borders
  int mv_col_min;
  int mv_col_max;
  int mv_row_min;
  int mv_row_max;

  // Notes transform blocks where no coefficents are coded.
  // Set during mode selection. Read during block encoding.
  uint8_t zcoeff_blk[TX_SIZES][256];

  int skip;

  int encode_breakout;

  // note that token_costs is the cost when eob node is skipped
  vp10_coeff_cost token_costs[TX_SIZES];

  int optimize;

  // indicate if it is in the rd search loop or encoding process
  int use_lp32x32fdct;

  // use fast quantization process
  int quant_fp;

  // skip forward transform and quantization
  uint8_t skip_txfm[MAX_MB_PLANE << 2];
  #define SKIP_TXFM_NONE 0
  #define SKIP_TXFM_AC_DC 1
  #define SKIP_TXFM_AC_ONLY 2

  int64_t bsse[MAX_MB_PLANE << 2];

  // Used to store sub partition's choices.
  MV pred_mv[MAX_REF_FRAMES];

  // Strong color activity detection. Used in RTC coding mode to enhance
  // the visual quality at the boundary of moving color objects.
  uint8_t color_sensitivity[2];
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_BLOCK_H_
