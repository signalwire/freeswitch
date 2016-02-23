/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_ENCODER_PICKLPF_H_
#define VP10_ENCODER_PICKLPF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vp10/encoder/encoder.h"

struct yv12_buffer_config;
struct VP10_COMP;

void vp10_pick_filter_level(const struct yv12_buffer_config *sd,
                           struct VP10_COMP *cpi, LPF_PICK_METHOD method);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_PICKLPF_H_
