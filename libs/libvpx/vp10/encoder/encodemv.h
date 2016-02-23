/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_ENCODER_ENCODEMV_H_
#define VP10_ENCODER_ENCODEMV_H_

#include "vp10/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void vp10_entropy_mv_init(void);

void vp10_write_nmv_probs(VP10_COMMON *cm, int usehp, vpx_writer *w,
                         nmv_context_counts *const counts);

void vp10_encode_mv(VP10_COMP *cpi, vpx_writer* w, const MV* mv, const MV* ref,
                   const nmv_context* mvctx, int usehp);

void vp10_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context* mvctx, int usehp);

void vp10_update_mv_count(ThreadData *td);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_ENCODEMV_H_
