/*
 * SpanDSP - a series of DSP components for telephony
 *
 * inttypes.h - a fudge for MSVC, which lacks this header
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Michael Jerris
 *
 *
 * This file is released in the public domain.
 *
 */

#if !defined(_INTTYPES_H_)
#define _INTTYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#define open _open
#define write _write

extern int gethostname (char *name, size_t len);

#ifdef __cplusplus
}
#endif

#endif
