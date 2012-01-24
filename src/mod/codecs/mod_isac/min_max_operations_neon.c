/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if (defined(WEBRTC_ANDROID) && defined(WEBRTC_ARCH_ARM_NEON))

#include <arm_neon.h>

#include "signal_processing_library.h"

// Maximum absolute value of word16 vector.
WebRtc_Word16 WebRtcSpl_MaxAbsValueW16(const WebRtc_Word16* vector,
                                       WebRtc_Word16 length) {
  WebRtc_Word32 temp_max = 0;
  WebRtc_Word32 abs_val;
  WebRtc_Word16 tot_max;
  int i;

  __asm__("vmov.i16 d25, #0" : : : "d25");

  for (i = 0; i < length - 7; i += 8) {
    __asm__("vld1.16 {d26, d27}, [%0]" : : "r"(&vector[i]) : "q13");
    __asm__("vabs.s16 q13, q13" : : : "q13");
    __asm__("vpmax.s16 d26, d27" : : : "q13");
    __asm__("vpmax.s16 d25, d26" : : : "d25", "d26");
  }
  __asm__("vpmax.s16 d25, d25" : : : "d25");
  __asm__("vpmax.s16 d25, d25" : : : "d25");
  __asm__("vmov.s16 %0, d25[0]" : "=r"(temp_max): : "d25");

  for (; i < length; i++) {
    abs_val = WEBRTC_SPL_ABS_W32((vector[i]));
    if (abs_val > temp_max) {
      temp_max = abs_val;
    }
  }
  tot_max = (WebRtc_Word16)WEBRTC_SPL_MIN(temp_max, WEBRTC_SPL_WORD16_MAX);
  return tot_max;
}

#endif
