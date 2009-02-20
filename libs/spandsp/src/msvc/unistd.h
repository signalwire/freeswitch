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

#if !defined(_UNISTD_H_)
#define _UNISTD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define open _open
#define write _write

extern int getopt(int argc, char *argv[], char *opstring);

extern char *optarg;

#ifdef __cplusplus
}
#endif

#endif
