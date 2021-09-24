/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@file sofia-sip/utf8.h
 * Encoding/Decoding Functions for UCS Transformation Format UTF-8.
 *
 * UTF-8 encoding codes the ISO 10646 (Unicode, UCS2 and UCS4) characters as
 * variable length (1 - 6 bytes) strings of 8-bit characters.
 *
 * @author Pekka Pessi <pekka.pessi@nokia.com>
 *
 * @date Created: Tue Apr 21 15:32:38 1998 pessi

 * @sa <a href="ftp://ftp.ietf.org/rfc/rfc2279.txt">RFC 2279</a>,
 * <i>"UTF-8, a transformation format of ISO 10646"</i>,
 * F. Yergeau. January 1998.
 *
 */

#ifndef UTF8_H
/** Defined when <sofia-sip/utf8.h> has been included */
#define	UTF8_H

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

typedef unsigned char  utf8;
typedef unsigned short utf16;
typedef unsigned char  ucs1;
typedef unsigned short ucs2;
typedef unsigned int   ucs4;

SOFIAPUBFUN size_t utf8_width(const utf8 *);

/* Latin-1 encoding/decoding */
SOFIAPUBFUN size_t ucs18decode(char *dst, size_t dst_size, const utf8 *s);
SOFIAPUBFUN size_t ucs1encode(utf8 *dst, const ucs1 *s, size_t n,
			      const char quote[128]);
SOFIAPUBFUN size_t ucs1declen(const utf8 *s);
SOFIAPUBFUN size_t ucs1enclen(const ucs1 *s, size_t n, const char quote[128]);

/* UCS2 (BMP) encoding/decoding */
size_t ucs2decode(ucs2 *dst, size_t dst_size, const utf8 *s);
size_t ucs2encode(utf8 *dst, const ucs2 *s, size_t n, const char quote[128]);
size_t ucs2declen(const utf8 *s);
size_t ucs2enclen(const ucs2 *s, size_t n, const char quote[128]);

size_t ucs4decode(ucs4 *dst, size_t dst_size, const utf8 *s);
size_t ucs4encode(utf8 *dst, const ucs4 *s, size_t n, const char quote[128]);
size_t ucs4declen(const utf8 *s);
size_t ucs4enclen(const ucs4 *s, size_t n, const char quote[128]);

size_t ucs2len(ucs2 const *s);
int ucs2cmp(ucs2 const *s1, ucs2 const *s2);
int ucs2ncmp(ucs2 const *s1, ucs2 const *s2, size_t n);

size_t ucs4len(ucs4 const *s);
int ucs4cmp(ucs4 const *s1, ucs4 const *s2);
int ucs4ncmp(ucs4 const *s1, ucs4 const *s2, size_t n);

/*
 * IS_UCS4_n tests whether UCS4 character should be represented
 * with 'n' byte utf8 string
 */
#define IS_UCS4_1(x) ((ucs4)(x) <= 0x7fu)
#define IS_UCS4_2(x) (0x80u <= (ucs4)(x) && (ucs4)(x) <= 0x7ffu)
#define IS_UCS4_3(x) (0x800u <= (ucs4)(x) && (ucs4)(x) <= 0xffffu)
#define IS_UCS4_4(x) (0x10000u <= (ucs4)(x) && (ucs4)(x) <= 0x1fFFFFu)
#define IS_UCS4_5(x) (0x200000u <= (ucs4)(x) && (ucs4)(x) <= 0x3ffFFFFu)
#define IS_UCS4_6(x) (0x4000000u <= (ucs4)(x) && (ucs4)(x) <= 0x7fffFFFFu)

/* Special test for ISO-8859-1 characters */
#define IS_UCS4_I(x) (0x80u <= (ucs4)(x) && (ucs4)(x) <= 0xffu)

/* Length of an UCS4 character in UTF8 encoding */
#define UTF8_LEN4(x) (IS_UCS4_1(x) || IS_UCS4_2(x) && 2 || \
		      IS_UCS4_3(x) && 3 || IS_UCS4_4(x) && 4 || \
		      IS_UCS4_5(x) && 5 || IS_UCS4_6(x) && 6)

/* Length of an UCS2 character in UTF8 encoding */
#define UTF8_LEN2(x) (IS_UCS4_1(x) || IS_UCS4_2(x) && 2 || IS_UCS4_3(x) && 3)

/*
 * IS_UTF8_n tests the length of the next wide character
 */
#define IS_UTF8_1(c) (0x00 == ((c) & 0x80))
#define IS_UTF8_2(c) (0xc0 == ((c) & 0xe0))
#define IS_UTF8_3(c) (0xe0 == ((c) & 0xf0))
#define IS_UTF8_4(c) (0xf0 == ((c) & 0xf8))
#define IS_UTF8_5(c) (0xf8 == ((c) & 0xfc))
#define IS_UTF8_6(c) (0xfc == ((c) & 0xfe))

/* Extension byte? */
#define IS_UTF8_X(c) (0x80 == ((c) & 0xc0))
/* ISO-8859-1 character? */
#define IS_UTF8_I(c) (0xc0 == ((c) & 0xfc))

#define IS_UTF8_S1(s) \
(IS_UTF8_1(s[0]))
#define IS_UTF8_S2(s) \
(IS_UTF8_2(s[0])&&((s)[1]&192)==128)
#define IS_UTF8_SI(s) \
(IS_UTF8_I(s[0])&&((s)[1]&192)==128)
#define IS_UTF8_S3(s) \
(IS_UTF8_3(s[0])&& ((s)[1]&192)==128&&((s)[2]&192)==128)
#define IS_UTF8_S4(s) \
(IS_UTF8_4(s[0])&& ((s)[1]&192)==128&&((s)[2]&192)==128&&((s)[3]&192)==128)
#define IS_UTF8_S5(s) \
(IS_UTF8_5(s[0])&& ((s)[1]&192)==128&&((s)[2]&192)==128&&\
 ((s)[3]&192)==128&&((s)[4]&192)==128)
#define IS_UTF8_S6(s) \
(IS_UTF8_6(s[0])&& ((s)[1]&192)==128&&((s)[2]&192)==128&&((s)[3]&192)==128&&\
 ((s)[4]&192)==128&&((s)[5]&192)==128)

#define UCS4_S1(s) ((ucs4)(s[0]))
#define UCS4_S2(s) ((ucs4)\
		    (((s[0])&31)<<6)|((s[1])&63))
#define UCS4_S3(s) ((ucs4)\
	            (((s[0])&15)<<12)|(((s[1])&63)<<6)|((s[2])&63))
#define UCS4_S4(s) ((ucs4)\
	            (((s[0])&7)<<18)|(((s[1])&63)<<12)|(((s[2])&63)<<6)|\
	            ((s[3])&63))
#define UCS4_S5(s) ((ucs4)\
	            (((s[0])&3)<<24)|(((s[1])&63)<<18)|(((s[2])&63)<<12)|\
	            (((s[3])&63)<<6)|((s[4])&63))
#define UCS4_S6(s) ((ucs4)\
		    (((s[0])&1)<<30)|(((s[1])&63)<<24)|(((s[2])&63)<<18)|\
		    (((s[3])&63)<<12)|(((s[4])&63)<<6)|((s[5])&63))

#define UTF8_S1(s,c) ((s)[0]=(c))
#define UTF8_S2(s,c) ((s)[0]=(((c)>>6)&31)|0xc0,\
		      (s)[1]=((c)&63)|128)
#define UTF8_S3(s,c) ((s)[0]=(((c)>>12)&15)|0xe0,\
		      (s)[1]=((c>>6)&63)|128,\
		      (s)[2]=((c)&63)|128)
#define UTF8_S4(s,c) ((s)[0]=(((c)>>18)&7)|0xf0,\
		      (s)[1]=((c>>12)&63)|128,\
		      (s)[2]=((c>>6)&63)|128,\
		      (s)[3]=((c)&63)|128)
#define UTF8_S5(s,c) ((s)[0]=(((c)>>24)&3)|0xf8,\
		      (s)[1]=((c>>18)&63)|128,\
		      (s)[2]=((c>>12)&63)|128,\
		      (s)[3]=((c>>6)&63)|128,\
		      (s)[4]=((c)&63)|128)
#define UTF8_S6(s,c) ((s)[0]=(((c)>>30)&1)|0xfc,\
		      (s)[1]=((c>>24)&63)|128,\
		      (s)[2]=((c>>18)&63)|128,\
		      (s)[3]=((c>>12)&63)|128,\
		      (s)[4]=((c>>6)&63)|128,\
		      (s)[5]=((c)&63)|128)

SOFIA_END_DECLS

#endif /* UTF8_H */
