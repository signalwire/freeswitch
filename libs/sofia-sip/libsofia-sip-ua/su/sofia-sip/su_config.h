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
#ifndef SU_CONFIG_H
/** Defined when <sofia-sip/su_config.h> has been included. */
#define SU_CONFIG_H
/**@file sofia-sip/su_config.h
 *
 * @b su library configuration
 *
 * This file includes an appropriate <sofia-sip/su_configure*.h> include file.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 */

#ifndef SU_CONFIGURE_H
#include <sofia-sip/su_configure.h>
#endif

#if defined(__GNUC__)
/* Special attributes for GNU C */
#if __GNUC__ < 3 && (!defined(__GNUC_MINOR__) || __GNUC_MINOR__ < 96)
#define __malloc__		/* avoid spurious warnigns */
#endif
#elif !defined(__attribute__)
#  define __attribute__(x)
#endif

/* C++ linkage needs to know that types and declarations are C, not C++.  */
#if defined(__cplusplus)
/** Begin declarations in Sofia header files */
# define SOFIA_BEGIN_DECLS	extern "C" {
/** End declarations in Sofia header files */
# define SOFIA_END_DECLS	}
#else
# define SOFIA_BEGIN_DECLS
# define SOFIA_END_DECLS
#endif

/* ---------------------------------------------------------------------- */
/* Macros required by Win32 linkage */

/** SOFIAPUBFUN declares an exported function */
#define SOFIAPUBFUN
/** SOFIAPUBVAR declares an exported variable */
#define SOFIAPUBVAR extern
/** SOFIACALL declares the calling convention for exported functions */
#define SOFIACALL

/* Win32 linkage */

/* Windows platform with MS/Borland/Cygwin/MinGW32 compiler */
#if defined(_WIN32) && \
  (defined(_MSC_VER) || defined(__BORLANDC__) ||  \
   defined(__CYGWIN__) || defined(__MINGW32__))
  #undef SOFIACALL
  #define SOFIACALL __cdecl

  #if defined(LIBSOFIA_SIP_UA_STATIC)
  #else
    #undef SOFIAPUBFUN
    #undef SOFIAPUBVAR
    #if defined(IN_LIBSOFIA_SIP_UA)
      #define SOFIAPUBFUN __declspec(dllexport)
      #define SOFIAPUBVAR __declspec(dllexport) extern
    #else
      #define SOFIAPUBFUN __declspec(dllimport)
      #define SOFIAPUBVAR __declspec(dllimport) extern
    #endif
  #endif

  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#elif defined (SYMBIAN)
  #undef SOFIACALL
  #define SOFIACALL __cdecl

  #if defined(LIBSOFIA_SIP_UA_STATIC)
  #else
    #undef SOFIAPUBFUN
    #undef SOFIAPUBVAR
    #if defined(IN_LIBSOFIA_SIP_UA)
      #define SOFIAPUBFUN __declspec(dllexport)
      #define SOFIAPUBVAR __declspec(dllexport) extern
    #else
      #define SOFIAPUBFUN __declspec(dllimport)
      #define SOFIAPUBVAR __declspec(dllimport)
    #endif
  #endif

  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#endif


#define BNF_DLL   SOFIAPUBFUN
#define HTTP_DLL  SOFIAPUBFUN
#define IPT_DLL   SOFIAPUBFUN
#define AUTH_DLL  SOFIAPUBFUN
#define MSG_DLL   SOFIAPUBFUN
#define NEA_DLL   SOFIAPUBFUN
#define NTA_DLL   SOFIAPUBFUN
#define NTH_DLL   SOFIAPUBFUN
#define SDP_DLL   SOFIAPUBFUN
#define SIP_DLL   SOFIAPUBFUN
#define SU_DLL    SOFIAPUBFUN
#define TPORT_DLL SOFIAPUBFUN
#define URL_DLL   SOFIAPUBFUN
#define MSG_TEST_DLL SOFIAPUBFUN

#endif /* SU_CONFIG_H */
