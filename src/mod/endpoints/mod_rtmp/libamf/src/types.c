/*
    $Id: types.c 1 2009-11-13 00:04:24Z noirotm $

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

#include "types.h"

#ifndef WORDS_BIGENDIAN

/* swap 64 bits doubles */
typedef union __convert_u {
    uint64_t i;
    number64_t f;
} convert_u;

number64_t swap_number64(number64_t n) {
    convert_u c;
    c.f = n;
    c.i = (((c.i & 0x00000000000000FFULL) << 56) |
           ((c.i & 0x000000000000FF00ULL) << 40) |
           ((c.i & 0x0000000000FF0000ULL) << 24) |
           ((c.i & 0x00000000FF000000ULL) << 8)  |
           ((c.i & 0x000000FF00000000ULL) >> 8)  |
           ((c.i & 0x0000FF0000000000ULL) >> 24) |
           ((c.i & 0x00FF000000000000ULL) >> 40) |
           ((c.i & 0xFF00000000000000ULL) >> 56));
    return c.f;
}
#endif /* !WORDS_BIGENDIAN */
