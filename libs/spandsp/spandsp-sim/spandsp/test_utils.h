/*
 * SpanDSP - a series of DSP components for telephony
 *
 * test_utils.h - Utility routines for module tests.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: test_utils.h,v 1.5 2008/08/29 09:28:13 steveu Exp $
 */

/*! \file */

#if !defined(_TEST_UTILS_H_)
#define _TEST_UTILS_H_

enum
{
    MUNGE_CODEC_NONE = 0,
    MUNGE_CODEC_ALAW,
    MUNGE_CODEC_ULAW,
    MUNGE_CODEC_G726_40K,
    MUNGE_CODEC_G726_32K,
    MUNGE_CODEC_G726_24K,
    MUNGE_CODEC_G726_16K,
};

typedef struct codec_munge_state_s codec_munge_state_t;

typedef struct complexify_state_s complexify_state_t;

#ifdef __cplusplus
extern "C" {
#endif

complexify_state_t *complexify_init(void);

void complexify_release(complexify_state_t *s);

complexf_t complexify(complexify_state_t *s, int16_t amp);

void fft(complex_t data[], int len);

void ifft(complex_t data[], int len);

codec_munge_state_t *codec_munge_init(int codec, int info);

void codec_munge_release(codec_munge_state_t *s);

void codec_munge(codec_munge_state_t *s, int16_t amp[], int len);

AFfilehandle afOpenFile_telephony_read(const char *name, int channels);

AFfilehandle afOpenFile_telephony_write(const char *name, int channels);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
