/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_COMMON_VP10_FWD_TXFM_H_
#define VP10_COMMON_VP10_FWD_TXFM_H_

#include "vpx_dsp/txfm_common.h"
#include "vpx_dsp/fwd_txfm.h"

void vp10_fdct32(const tran_high_t *input, tran_high_t *output, int round);
#endif  // VP10_COMMON_VP10_FWD_TXFM_H_
