/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_ENCODER_BITSTREAM_H_
#define VP10_ENCODER_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vp10/encoder/encoder.h"

void vp10_encode_token_init();
void vp10_pack_bitstream(VP10_COMP *const cpi, uint8_t *dest, size_t *size);

static INLINE int vp10_preserve_existing_gf(VP10_COMP *cpi) {
  return !cpi->multi_arf_allowed && cpi->refresh_golden_frame &&
         cpi->rc.is_src_frame_alt_ref;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_BITSTREAM_H_
