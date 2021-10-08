/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sys/time.h - a fudge for MSVC, which lacks this header
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Michael Jerris
 *
 *
 * This file is released in the public domain.
 *
 */

struct timeval
{
    long int tv_sec;
    long int tv_usec;
};

extern void gettimeofday(struct timeval *tv, void *tz);
