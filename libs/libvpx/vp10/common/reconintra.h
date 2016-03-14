/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_COMMON_RECONINTRA_H_
#define VP10_COMMON_RECONINTRA_H_

#include "vpx/vpx_integer.h"
#include "vp10/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

void vp10_init_intra_predictors(void);

void vp10_predict_intra_block(const MACROBLOCKD *xd, int bwl_in, int bhl_in,
                             TX_SIZE tx_size, PREDICTION_MODE mode,
                             const uint8_t *ref, int ref_stride,
                             uint8_t *dst, int dst_stride,
                             int aoff, int loff, int plane);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_RECONINTRA_H_
