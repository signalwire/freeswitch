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

#ifndef SOFIA_RESOLV_SRES_CONFIG_H
/** Defined when <sofia-resolv/sres_config.h> has been included. */
#define SOFIA_RESOLV_SRES_CONFIG_H

/**
 * @file sofia-resolv/sres_config.h
 *
 * Configuration for Sofia DNS Resolver.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

/* ---------------------------------------------------------------------- */
/* Macros required by Win32 linkage */

/** SRESPUBFUN declares an exported function */
#define SRESPUBFUN

#if defined(_WIN32) && !defined(LIBSRES_STATIC) && \
  (defined(_MSC_VER) || defined(__BORLANDC__) ||  \
   defined(__CYGWIN__) || defined(__MINGW32__))
  /* Windows platform with MS/Borland/Cygwin/MinGW32 compiler */
  #undef SRESPUBFUN
  #if defined(IN_LIBSOFIA_SRES)
    #define SRESPUBFUN __declspec(dllexport)
  #else
    #define SRESPUBFUN __declspec(dllimport)
  #endif
#elif defined(SYMBIAN) && !defined(LIBSRES_STATIC)
  /* Open C platform */
  #undef SRESPUBFUN
  #if defined(IN_LIBSOFIA_SRES)
    #define SRESPUBFUN __declspec(dllexport)
  #else
    #define SRESPUBFUN __declspec(dllimport)
  #endif
#endif

/* ---------------------------------------------------------------------- */

/* Types required by Win32/64 */

#if defined (SYMBIAN)
/** Socket descriptor. @since New in @VERSION_1_12_2. */
typedef int sres_socket_t;
#elif defined(_WIN32)
typedef SOCKET sres_socket_t;
#else
/** Socket descriptor. @since New in @VERSION_1_12_2. */
typedef int sres_socket_t;
#endif

#endif /* SOFIA_RESOLV_SRES_CONFIG_H */
