/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_ENCODER_AQ_VARIANCE_H_
#define VP10_ENCODER_AQ_VARIANCE_H_

#include "vp10/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned int vp10_vaq_segment_id(int energy);
void vp10_vaq_frame_setup(VP10_COMP *cpi);

int vp10_block_energy(VP10_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs);
double vp10_log_block_var(VP10_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_AQ_VARIANCE_H_
