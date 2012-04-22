/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * broadvoice.h - 
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from code which is
 * Copyright 2000-2009 Broadcom Corporation
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
 * $Id: broadvoice.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

#if !defined(_BROADVOICE_BROADVOICE_H_)
#define _BROADVOICE_BROADVOICE_H_

#if defined(_M_IX86)  ||  defined(_M_X64)
#if defined(BROADVOICE_EXPORTS)
#define BV_DECLARE(type)                __declspec(dllexport) type __stdcall
#define BV_DECLARE_NONSTD(type)          __declspec(dllexport) type __cdecl
#define BV_DECLARE_DATA                 __declspec(dllexport)
#else
#define BV_DECLARE(type)                __declspec(dllimport) type __stdcall
#define BV_DECLARE_NONSTD(type)         __declspec(dllimport) type __cdecl
#define BV_DECLARE_DATA                 __declspec(dllimport)
#endif
#elif defined(BROADVOICE_USE_EXPORT_CAPABILITY)  &&  (defined(__GNUC__)  ||  defined(__SUNCC__))
#define BV_DECLARE(type)                __attribute__((visibility("default"))) type
#define BV_DECLARE_NONSTD(type)         __attribute__((visibility("default"))) type
#define BV_DECLARE_DATA                 __attribute__((visibility("default")))
#else
#define BV_DECLARE(type)                /**/ type
#define BV_DECLARE_NONSTD(type)         /**/ type
#define BV_DECLARE_DATA                 /**/
#endif

typedef struct bv16_encode_state_s bv16_encode_state_t;
typedef struct bv16_decode_state_s bv16_decode_state_t;
typedef struct bv32_encode_state_s bv32_encode_state_t;
typedef struct bv32_decode_state_s bv32_decode_state_t;

#define BV16_FRAME_LEN      40
#define BV32_FRAME_LEN      80

#if defined(__cplusplus)
extern "C"
{
#endif

BV_DECLARE(bv16_encode_state_t *) bv16_encode_init(bv16_encode_state_t *s);

BV_DECLARE(int) bv16_encode(bv16_encode_state_t *cs,
                            uint8_t *out,
                            const int16_t amp[],
                            int len);

BV_DECLARE(int) bv16_encode_release(bv16_encode_state_t *s);

BV_DECLARE(int) bv16_encode_free(bv16_encode_state_t *s);

    
BV_DECLARE(bv16_decode_state_t *) bv16_decode_init(bv16_decode_state_t *s);

BV_DECLARE(int) bv16_decode(bv16_decode_state_t *s,
                            int16_t amp[],
                            const uint8_t *in,
                            int len);

BV_DECLARE(int) bv16_fillin(bv16_decode_state_t *s,
                            int16_t amp[],
                            int len);

BV_DECLARE(int) bv16_decode_release(bv16_decode_state_t *s);

BV_DECLARE(int) bv16_decode_free(bv16_decode_state_t *s);


BV_DECLARE(bv32_encode_state_t *) bv32_encode_init(bv32_encode_state_t *s);

BV_DECLARE(int) bv32_encode(bv32_encode_state_t *s,
                            uint8_t *out,
                            const int16_t amp[],
                            int len);

BV_DECLARE(int) bv32_encode_release(bv32_encode_state_t *s);

BV_DECLARE(int) bv32_encode_free(bv32_encode_state_t *s);


BV_DECLARE(bv32_decode_state_t *) bv32_decode_init(bv32_decode_state_t *s);

BV_DECLARE(int) bv32_decode(bv32_decode_state_t *s,
                            int16_t amp[],
                            const uint8_t *in,
                            int len);

BV_DECLARE(int) bv32_fillin(bv32_decode_state_t *ds, 
                            int16_t amp[],
                            int len);

BV_DECLARE(int) bv32_decode_release(bv32_decode_state_t *s);

BV_DECLARE(int) bv32_decode_free(bv32_decode_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
