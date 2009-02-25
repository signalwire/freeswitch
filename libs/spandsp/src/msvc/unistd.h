/*
 * SpanDSP - a series of DSP components for telephony
 *
 * unistd.h - a fudge for MSVC, which lacks this header
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

// Declare this so we don't have to include winsock.h, it causes numerous conflicts.
extern int __stdcall gethostname(char * name, int namelen);
#pragma comment(lib, "ws2_32.lib")

extern int getopt(int argc, char *argv[], char *opstring);

extern char *optarg;

#ifdef __cplusplus
}
#endif

#endif
