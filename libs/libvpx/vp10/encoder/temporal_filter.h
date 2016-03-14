/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_ENCODER_TEMPORAL_FILTER_H_
#define VP10_ENCODER_TEMPORAL_FILTER_H_

#ifdef __cplusplus
extern "C" {
#endif

void vp10_temporal_filter_init(void);
void vp10_temporal_filter(VP10_COMP *cpi, int distance);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_TEMPORAL_FILTER_H_
