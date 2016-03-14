/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_DECODER_DETOKENIZE_H_
#define VP10_DECODER_DETOKENIZE_H_

#include "vpx_dsp/bitreader.h"
#include "vp10/decoder/decoder.h"
#include "vp10/common/scan.h"

#ifdef __cplusplus
extern "C" {
#endif

int vp10_decode_block_tokens(MACROBLOCKD *xd,
                            int plane, const scan_order *sc,
                            int x, int y,
                            TX_SIZE tx_size, vpx_reader *r,
                            int seg_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_DECODER_DETOKENIZE_H_
