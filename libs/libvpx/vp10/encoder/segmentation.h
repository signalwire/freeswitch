/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_ENCODER_SEGMENTATION_H_
#define VP10_ENCODER_SEGMENTATION_H_

#include "vp10/common/blockd.h"
#include "vp10/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void vp10_enable_segmentation(struct segmentation *seg);
void vp10_disable_segmentation(struct segmentation *seg);

void vp10_disable_segfeature(struct segmentation *seg,
                            int segment_id,
                            SEG_LVL_FEATURES feature_id);
void vp10_clear_segdata(struct segmentation *seg,
                       int segment_id,
                       SEG_LVL_FEATURES feature_id);

// The values given for each segment can be either deltas (from the default
// value chosen for the frame) or absolute values.
//
// Valid range for abs values is (0-127 for MB_LVL_ALT_Q), (0-63 for
// SEGMENT_ALT_LF)
// Valid range for delta values are (+/-127 for MB_LVL_ALT_Q), (+/-63 for
// SEGMENT_ALT_LF)
//
// abs_delta = SEGMENT_DELTADATA (deltas) abs_delta = SEGMENT_ABSDATA (use
// the absolute values given).
void vp10_set_segment_data(struct segmentation *seg, signed char *feature_data,
                          unsigned char abs_delta);

void vp10_choose_segmap_coding_method(VP10_COMMON *cm, MACROBLOCKD *xd);

void vp10_reset_segment_features(VP10_COMMON *cm);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_SEGMENTATION_H_
