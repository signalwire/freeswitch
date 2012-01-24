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
 * This file contains the implementation of functions
 * WebRtcSpl_MaxAbsValueW16()
 * WebRtcSpl_MaxAbsIndexW16()
 * WebRtcSpl_MaxAbsValueW32()
 * WebRtcSpl_MaxValueW16()
 * WebRtcSpl_MaxIndexW16()
 * WebRtcSpl_MaxValueW32()
 * WebRtcSpl_MaxIndexW32()
 * WebRtcSpl_MinValueW16()
 * WebRtcSpl_MinIndexW16()
 * WebRtcSpl_MinValueW32()
 * WebRtcSpl_MinIndexW32()
 *
 * The description header can be found in signal_processing_library.h.
 *
 */

#include "signal_processing_library.h"

#if !(defined(WEBRTC_ANDROID) && defined(WEBRTC_ARCH_ARM_NEON))

// Maximum absolute value of word16 vector.
WebRtc_Word16 WebRtcSpl_MaxAbsValueW16(const WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtc_Word32 tempMax = 0;
    WebRtc_Word32 absVal;
    WebRtc_Word16 totMax;
    int i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    for (i = 0; i < length; i++)
    {
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
    }
    totMax = (WebRtc_Word16)WEBRTC_SPL_MIN(tempMax, WEBRTC_SPL_WORD16_MAX);
    return totMax;
}

#endif

// Index of maximum absolute value in a  word16 vector.
WebRtc_Word16 WebRtcSpl_MaxAbsIndexW16(G_CONST WebRtc_Word16* vector, WebRtc_Word16 length)
{
    WebRtc_Word16 tempMax;
    WebRtc_Word16 absTemp;
    WebRtc_Word16 tempMaxIndex = 0;
    WebRtc_Word16 i = 0;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    tempMax = WEBRTC_SPL_ABS_W16(*tmpvector);
    tmpvector++;
    for (i = 1; i < length; i++)
    {
        absTemp = WEBRTC_SPL_ABS_W16(*tmpvector);
        tmpvector++;
        if (absTemp > tempMax)
        {
            tempMax = absTemp;
            tempMaxIndex = i;
        }
    }
    return tempMaxIndex;
}

// Maximum absolute value of word32 vector.
WebRtc_Word32 WebRtcSpl_MaxAbsValueW32(G_CONST WebRtc_Word32 *vector, WebRtc_Word16 length)
{
    WebRtc_UWord32 tempMax = 0;
    WebRtc_UWord32 absVal;
    WebRtc_Word32 retval;
    int i;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    for (i = 0; i < length; i++)
    {
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
    }
    retval = (WebRtc_Word32)(WEBRTC_SPL_MIN(tempMax, WEBRTC_SPL_WORD32_MAX));
    return retval;
}

// Maximum value of word16 vector.
#ifndef XSCALE_OPT
WebRtc_Word16 WebRtcSpl_MaxValueW16(G_CONST WebRtc_Word16* vector, WebRtc_Word16 length)
{
    WebRtc_Word16 tempMax;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ > tempMax)
            tempMax = vector[i];
    }
    return tempMax;
}
#else
#pragma message(">> WebRtcSpl_MaxValueW16 is excluded from this build")
#endif

// Index of maximum value in a word16 vector.
WebRtc_Word16 WebRtcSpl_MaxIndexW16(G_CONST WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtc_Word16 tempMax;
    WebRtc_Word16 tempMaxIndex = 0;
    WebRtc_Word16 i = 0;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ > tempMax)
        {
            tempMax = vector[i];
            tempMaxIndex = i;
        }
    }
    return tempMaxIndex;
}

// Maximum value of word32 vector.
#ifndef XSCALE_OPT
WebRtc_Word32 WebRtcSpl_MaxValueW32(G_CONST WebRtc_Word32* vector, WebRtc_Word16 length)
{
    WebRtc_Word32 tempMax;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ > tempMax)
            tempMax = vector[i];
    }
    return tempMax;
}
#else
#pragma message(">> WebRtcSpl_MaxValueW32 is excluded from this build")
#endif

// Index of maximum value in a word32 vector.
WebRtc_Word16 WebRtcSpl_MaxIndexW32(G_CONST WebRtc_Word32* vector, WebRtc_Word16 length)
{
    WebRtc_Word32 tempMax;
    WebRtc_Word16 tempMaxIndex = 0;
    WebRtc_Word16 i = 0;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ > tempMax)
        {
            tempMax = vector[i];
            tempMaxIndex = i;
        }
    }
    return tempMaxIndex;
}

// Minimum value of word16 vector.
WebRtc_Word16 WebRtcSpl_MinValueW16(G_CONST WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtc_Word16 tempMin;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    // Find the minimum value
    tempMin = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ < tempMin)
            tempMin = (vector[i]);
    }
    return tempMin;
}

// Index of minimum value in a word16 vector.
#ifndef XSCALE_OPT
WebRtc_Word16 WebRtcSpl_MinIndexW16(G_CONST WebRtc_Word16* vector, WebRtc_Word16 length)
{
    WebRtc_Word16 tempMin;
    WebRtc_Word16 tempMinIndex = 0;
    WebRtc_Word16 i = 0;
    G_CONST WebRtc_Word16* tmpvector = vector;

    // Find index of smallest value
    tempMin = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ < tempMin)
        {
            tempMin = vector[i];
            tempMinIndex = i;
        }
    }
    return tempMinIndex;
}
#else
#pragma message(">> WebRtcSpl_MinIndexW16 is excluded from this build")
#endif

// Minimum value of word32 vector.
WebRtc_Word32 WebRtcSpl_MinValueW32(G_CONST WebRtc_Word32 *vector, WebRtc_Word16 length)
{
    WebRtc_Word32 tempMin;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    // Find the minimum value
    tempMin = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ < tempMin)
            tempMin = (vector[i]);
    }
    return tempMin;
}

// Index of minimum value in a word32 vector.
#ifndef XSCALE_OPT
WebRtc_Word16 WebRtcSpl_MinIndexW32(G_CONST WebRtc_Word32* vector, WebRtc_Word16 length)
{
    WebRtc_Word32 tempMin;
    WebRtc_Word16 tempMinIndex = 0;
    WebRtc_Word16 i = 0;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    // Find index of smallest value
    tempMin = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ < tempMin)
        {
            tempMin = vector[i];
            tempMinIndex = i;
        }
    }
    return tempMinIndex;
}
#else
#pragma message(">> WebRtcSpl_MinIndexW32 is excluded from this build")
#endif
