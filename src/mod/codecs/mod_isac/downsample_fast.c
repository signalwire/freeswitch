/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file contains the function WebRtcSpl_DownsampleFast().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_DownsampleFast(WebRtc_Word16 *in_ptr, WebRtc_Word16 in_length,
                             WebRtc_Word16 *out_ptr, WebRtc_Word16 out_length,
                             WebRtc_Word16 *B, WebRtc_Word16 B_length, WebRtc_Word16 factor,
                             WebRtc_Word16 delay)
{
    WebRtc_Word32 o;
    int i, j;

    WebRtc_Word16 *downsampled_ptr = out_ptr;
    WebRtc_Word16 *b_ptr;
    WebRtc_Word16 *x_ptr;
    WebRtc_Word16 endpos = delay
            + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(factor, (out_length - 1)) + 1;

    if (in_length < endpos)
    {
        return -1;
    }

    for (i = delay; i < endpos; i += factor)
    {
        b_ptr = &B[0];
        x_ptr = &in_ptr[i];

        o = (WebRtc_Word32)2048; // Round val

        for (j = 0; j < B_length; j++)
        {
            o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
        }

        o = WEBRTC_SPL_RSHIFT_W32(o, 12);

        // If output is higher than 32768, saturate it. Same with negative side

        *downsampled_ptr++ = WebRtcSpl_SatW32ToW16(o);
    }

    return 0;
}
