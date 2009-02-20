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
 * $Id: config.h,v 1.3 2009/02/10 13:06:47 steveu Exp $
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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
