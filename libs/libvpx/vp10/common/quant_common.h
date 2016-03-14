/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_COMMON_QUANT_COMMON_H_
#define VP10_COMMON_QUANT_COMMON_H_

#include "vpx/vpx_codec.h"
#include "vp10/common/seg_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MINQ 0
#define MAXQ 255
#define QINDEX_RANGE (MAXQ - MINQ + 1)
#define QINDEX_BITS 8

int16_t vp10_dc_quant(int qindex, int delta, vpx_bit_depth_t bit_depth);
int16_t vp10_ac_quant(int qindex, int delta, vpx_bit_depth_t bit_depth);

int vp10_get_qindex(const struct segmentation *seg, int segment_id,
                   int base_qindex);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_QUANT_COMMON_H_
