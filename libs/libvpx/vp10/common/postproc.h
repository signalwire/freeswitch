/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_COMMON_POSTPROC_H_
#define VP10_COMMON_POSTPROC_H_

#include "vpx_ports/mem.h"
#include "vpx_scale/yv12config.h"
#include "vp10/common/blockd.h"
#include "vp10/common/mfqe.h"
#include "vp10/common/ppflags.h"

#ifdef __cplusplus
extern "C" {
#endif

struct postproc_state {
  int last_q;
  int last_noise;
  char noise[3072];
  int last_base_qindex;
  int last_frame_valid;
  MODE_INFO *prev_mip;
  MODE_INFO *prev_mi;
  DECLARE_ALIGNED(16, char, blackclamp[16]);
  DECLARE_ALIGNED(16, char, whiteclamp[16]);
  DECLARE_ALIGNED(16, char, bothclamp[16]);
};

struct VP10Common;

#define MFQE_PRECISION 4

int vp10_post_proc_frame(struct VP10Common *cm,
                        YV12_BUFFER_CONFIG *dest, vp10_ppflags_t *flags);

void vp10_denoise(const YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst, int q);

void vp10_deblock(const YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst, int q);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_POSTPROC_H_
