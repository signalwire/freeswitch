/*
 * SpanDSP - a series of DSP components for telephony
 *
 * config.h - a fudge for MSVC, which lacks this header
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Michael Jerris
 *
 *
 * This file is released in the public domain.
 *
 * $Id: config.h,v 1.4 2009/02/25 15:30:21 steveu Exp $
 */

#if !defined(_MSVC_CONFIG_H_)
#define _MSVC_CONFIG_H_

#define HAVE_SINF
#define HAVE_COSF
#define HAVE_TANF
#define HAVE_ASINF
#define HAVE_ACOSF
#define HAVE_ATANF
#define HAVE_ATAN2F
#define HAVE_CEILF
#define HAVE_FLOORF
#define HAVE_POWF
#define HAVE_EXPF
#define HAVE_LOGF
#define HAVE_LOG10F
#define HAVE_MATH_H
#define HAVE_TGMATH_H

#define HAVE_LONG_DOUBLE
#define HAVE_LIBTIFF

#define SPANDSP_USE_EXPORT_CAPABILITY 1

#define PACKAGE "spandsp"
#define VERSION "0.0.6"

/* Win32/DevStudio compatibility stuff */

#ifdef _MSC_VER

  #if (_MSC_VER >= 1400) // VC8+
    #ifndef _CRT_SECURE_NO_DEPRECATE
      #define _CRT_SECURE_NO_DEPRECATE
    #endif
    #ifndef _CRT_NONSTDC_NO_DEPRECATE
      #define _CRT_NONSTDC_NO_DEPRECATE
    #endif
    #ifndef _CRT_SECURE_NO_WARNINGS
      #define _CRT_SECURE_NO_WARNINGS
    #endif
  #endif // VC8+

  // disable the following warnings 
  #pragma warning(disable:4100) // The formal parameter is not referenced in the body of the function. The unreferenced parameter is ignored. 
  #pragma warning(disable:4200) // Non standard extension C zero sized array
  #pragma warning(disable:4706) // assignment within conditional expression
  #pragma warning(disable:4244) // conversion from 'type1' to 'type2', possible loss of data
  #pragma warning(disable:4295) // array is too small to include a terminating null character
  #pragma warning(disable:4125) // decimal digit terminates octal escape sequence
  #pragma warning(disable:4305) // 'function' : truncation from 'double' to 'float'
  #pragma warning(disable:4018) // '<' : signed/unsigned mismatch
  #pragma warning(disable:4389) // '==' : signed/unsigned mismatch
  #pragma warning(disable:4245) // 'return' : conversion from 'int' to 'size_t', signed/unsigned mismatch

  #define strncasecmp _strnicmp
  #define strcasecmp _stricmp
  #define snprintf _snprintf
  #define inline __inline
  #define __inline__ __inline

  #define _MMX_H_

  #include <malloc.h> // To get alloca

#endif

#endif
