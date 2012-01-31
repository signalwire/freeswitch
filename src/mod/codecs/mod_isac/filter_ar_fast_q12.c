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
 * This file contains the function WebRtcSpl_FilterARFastQ12().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_FilterARFastQ12(WebRtc_Word16 *in, WebRtc_Word16 *out, WebRtc_Word16 *A,
                               WebRtc_Word16 A_length, WebRtc_Word16 length)
{
    WebRtc_Word32 o;
    int i, j;

    WebRtc_Word16 *x_ptr = &in[0];
    WebRtc_Word16 *filtered_ptr = &out[0];

    for (i = 0; i < length; i++)
    {
        // Calculate filtered[i]
        G_CONST WebRtc_Word16 *a_ptr = &A[0];
        WebRtc_Word16 *state_ptr = &out[i - 1];

        o = WEBRTC_SPL_MUL_16_16(*x_ptr++, *a_ptr++);

        for (j = 1; j < A_length; j++)
        {
            o -= WEBRTC_SPL_MUL_16_16(*a_ptr++,*state_ptr--);
        }

        // Saturate the output
        o = WEBRTC_SPL_SAT((WebRtc_Word32)134215679, o, (WebRtc_Word32)-134217728);

        *filtered_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12);
    }

    return;
}
