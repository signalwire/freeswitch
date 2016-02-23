/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_ENCODER_ENCODEFRAME_H_
#define VP10_ENCODER_ENCODEFRAME_H_

#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct macroblock;
struct yv12_buffer_config;
struct VP10_COMP;
struct ThreadData;

// Constants used in SOURCE_VAR_BASED_PARTITION
#define VAR_HIST_MAX_BG_VAR 1000
#define VAR_HIST_FACTOR 10
#define VAR_HIST_BINS (VAR_HIST_MAX_BG_VAR / VAR_HIST_FACTOR + 1)
#define VAR_HIST_LARGE_CUT_OFF 75
#define VAR_HIST_SMALL_CUT_OFF 45

void vp10_setup_src_planes(struct macroblock *x,
                          const struct yv12_buffer_config *src,
                          int mi_row, int mi_col);

void vp10_encode_frame(struct VP10_COMP *cpi);

void vp10_init_tile_data(struct VP10_COMP *cpi);
void vp10_encode_tile(struct VP10_COMP *cpi, struct ThreadData *td,
                     int tile_row, int tile_col);

void vp10_set_variance_partition_thresholds(struct VP10_COMP *cpi, int q);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_ENCODEFRAME_H_
