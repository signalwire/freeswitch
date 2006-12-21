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

#ifndef UTF8INTERNAL_H
#define UTF8INTERNAL_H 

/**@IFILE utf8internal.h 
 * UTF-8 macros.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Apr 21 15:32:02 1998 pessi
 */

#define UTF8_ANALYZE(s, ascii, latin1, ucs2, ucs4, errors) \
do {  \
  if (s) while (*s) { \
    utf8 c = *s++; \
    if (IS_UTF8_1(c)) \
      ascii++; \
    else if (IS_UTF8_I(c)) { \
      if (IS_UTF8_X(s[0])) \
	latin1++, s++; \
      else \
	errors++; \
    } \
    else if (IS_UTF8_2(c)) { \
      if (IS_UTF8_X(s[0])) \
	ucs2++, s++; \
      else \
	errors++; \
    } \
    else if (IS_UTF8_3(c)) { \
      if (IS_UTF8_X(s[0]) && IS_UTF8_X(s[1])) \
	ucs2++, s++, s++; \
      else \
	errors++; \
    } \
    else if (IS_UTF8_4(c)) { \
      if (IS_UTF8_X(s[0]) && IS_UTF8_X(s[1]) && IS_UTF8_X(s[2])) \
	ucs4++, s++, s++, s++; \
      else \
	errors++; \
    } \
    else if (IS_UTF8_5(c)) { \
      if (IS_UTF8_X(s[0]) && IS_UTF8_X(s[1]) &&  \
	  IS_UTF8_X(s[2]) && IS_UTF8_X(s[3])) \
	ucs4++, s++, s++, s++, s++; \
      else \
	errors++; \
    } \
    else if (IS_UTF8_6(c)) { \
      if (IS_UTF8_X(s[0]) && IS_UTF8_X(s[1]) &&  \
	  IS_UTF8_X(s[2]) && IS_UTF8_X(s[3]) && IS_UTF8_X(s[4])) \
	ucs4++, s++, s++, s++, s++, s++; \
      else \
	errors++; \
    } \
    else \
	errors++; \
  } \
} while(0)

#endif /* UTF8INTERNAL_H */
