/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_ENCODER_ETHREAD_H_
#define VP10_ENCODER_ETHREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

struct VP10_COMP;
struct ThreadData;

typedef struct EncWorkerData {
  struct VP10_COMP *cpi;
  struct ThreadData *td;
  int start;
} EncWorkerData;

void vp10_encode_tiles_mt(struct VP10_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_ETHREAD_H_
