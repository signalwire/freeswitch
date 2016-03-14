/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP10_DECODER_DECODEFRAME_H_
#define VP10_DECODER_DECODEFRAME_H_

#ifdef __cplusplus
extern "C" {
#endif

struct VP10Decoder;
struct vpx_read_bit_buffer;

int vp10_read_sync_code(struct vpx_read_bit_buffer *const rb);
void vp10_read_frame_size(struct vpx_read_bit_buffer *rb,
                         int *width, int *height);
BITSTREAM_PROFILE vp10_read_profile(struct vpx_read_bit_buffer *rb);

void vp10_decode_frame(struct VP10Decoder *pbi,
                      const uint8_t *data, const uint8_t *data_end,
                      const uint8_t **p_data_end);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_DECODER_DECODEFRAME_H_
