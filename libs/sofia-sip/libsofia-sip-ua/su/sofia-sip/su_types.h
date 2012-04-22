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

#ifndef SU_TYPES_H
/** Defined when <sofia-sip/su_types.h> has been included */
#define SU_TYPES_H
/**@file sofia-sip/su_types.h Basic integer types for @b su library.
 *
 * This include file provides <stdint.h> or <inttypes.h> types.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 */

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif

#if SU_HAVE_STDINT
#include <stdint.h>
#elif SU_HAVE_INTTYPES
#include <inttypes.h>
#endif

#if SU_HAVE_SYS_TYPES
#include <sys/types.h>
#endif

#include <stddef.h>

SOFIA_BEGIN_DECLS

#if SU_HAVE_STDINT || SU_HAVE_INTTYPES
#define SU_S64_T int64_t
#define SU_U64_T uint64_t
#define SU_S32_T int32_t
#define SU_U32_T uint32_t
#define SU_S16_T int16_t
#define SU_U16_T uint16_t
#define SU_S8_T  int8_t
#define SU_U8_T  uint8_t
#define SU_LEAST64_T int_least64_t
#define SU_LEAST32_T int_least32_t
#define SU_LEAST16_T int_least16_t
#define SU_LEAST8_T int_least8_t
#endif

#if DOXYGEN_ONLY || (!SU_HAVE_STDINT && !SU_HAVE_INTTYPES && SU_HAVE_WIN32)

/* Use macros defined in <sofia-sip/su_configure_win32.h> */

#ifndef _INTPTR_T_DEFINED
/** Integer type large enough to store pointers */
typedef SU_INTPTR_T intptr_t;
#endif
#ifndef _UINTPTR_T_DEFINED
/** Unsigned integer type large enough to store pointers */
typedef unsigned SU_INTPTR_T uintptr_t;
#endif

/** 64-bit unsigned integer */
typedef SU_U64_T uint64_t;
/** 64-bit signed integer */
typedef SU_S64_T int64_t;
/** 32-bit unsigned integer */
typedef SU_U32_T uint32_t;
/** 32-bit signed integer */
typedef SU_S32_T int32_t;
/** 16-bit unsigned integer */
typedef SU_U16_T uint16_t;
/** 16-bit signed integer */
typedef SU_S16_T int16_t;
/** 8-bit unsigned integer */
typedef SU_U8_T  uint8_t;
/** 8-bit signed integer */
typedef SU_S8_T  int8_t;

/** At least 64-bit integer */
typedef SU_LEAST64_T int_least64_t;
/** At least 32-bit integer */
typedef SU_LEAST32_T int_least32_t;
/** At least 16-bit integer */
typedef SU_LEAST16_T int_least16_t;
/** At least 8-bit integer */
typedef SU_LEAST8_T int_least8_t;
#endif

#if !SU_HAVE_STDINT && !SU_HAVE_INTTYPES && !SU_HAVE_WIN32
#error "no integer types available."
#endif

/* ---------------------------------------------------------------------- */
/* size_t types for binary compatibility */

#ifdef SOFIA_SSIZE_T
/** POSIX type used for a count of bytes or an error indication. */
typedef SOFIA_SSIZE_T ssize_t;
#endif

#ifdef SOFIA_ISIZE_T
/** Compatibility type.
 *
 * sofia-sip <= 1.12.1 often used int for count of bytes.
 * When configured for compatibility with sofia-sip 1.12.0, this is defined
 * as int, otherwise as size_t. Note that int is signed and size_t is
 * unsigned.
 *
 * @since New in @VERSION_1_12_2.
 */
typedef SOFIA_ISIZE_T isize_t;
#else
typedef size_t isize_t;
#endif

#ifdef SOFIA_ISSIZE_T
/**Compatibility type.
 *
 * sofia-sip <= 1.12.1 used int for count of bytes.
 * When configured for compatibility with sofia-sip 1.12.0, this is defined
 * as int, otherwise as ssize_t. (-1 is used for error indication).
 *
 * @since New in @VERSION_1_12_2.
 */
typedef SOFIA_ISSIZE_T issize_t;
#else
typedef ssize_t issize_t;
#endif

#ifdef SOFIA_USIZE_T
/**Compatibility type.
 *
 * sofia-sip <= 1.12.1 sometimes used unsigned int for count of bytes.
 * When configured for compatibility with sofia-sip 1.12.0, this is defined
 * as unsigned int, otherwise as size_t.
 *
 * @since New in @VERSION_1_12_2.
 */
typedef SOFIA_USIZE_T usize_t;
#else
typedef size_t usize_t;
#endif

SOFIA_END_DECLS

#endif /* SU_TYPES_H */
