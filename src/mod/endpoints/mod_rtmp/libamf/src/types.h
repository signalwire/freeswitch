/*
    $Id: types.h 1 2009-11-13 00:04:24Z noirotm $

    FLV Metadata updater

    Copyright (C) 2007, 2008 Marc Noirot <marc.noirot AT gmail.com>

    This file is part of FLVMeta.

    FLVMeta is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    FLVMeta is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FLVMeta; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef __TYPES_H__
#define __TYPES_H__

#include "amf.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef WORDS_BIGENDIAN

#  define swap_uint16(x) (x)
#  define swap_int16(x) (x)
#  define swap_uint32(x) (x)
#  define swap_number64(x) (x)

#else /* WORDS_BIGENDIAN */
/* swap 16 bits integers */
#  define swap_uint16(x) ((((x) & 0x00FFU) << 8) | (((x) & 0xFF00U) >> 8))
#  define swap_sint16(x) ((((x) & 0x00FF) << 8) | (((x) & 0xFF00) >> 8))

/* swap 32 bits integers */
#  define swap_uint32(x) ((((x) & 0x000000FFU) << 24) | \
    (((x) & 0x0000FF00U) << 8)  | \
    (((x) & 0x00FF0000U) >> 8)  | \
    (((x) & 0xFF000000U) >> 24))

/* swap 64 bits doubles */
number64_t swap_number64(number64_t);

#endif /* WORDS_BIGENDIAN */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TYPES_H__ */
