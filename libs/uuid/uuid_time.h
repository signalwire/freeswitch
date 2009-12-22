/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP uuid, a library for the generation
**  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
**
**  uuid_time.h: Time Management API
*/

#ifndef __UUID_TIME_H__
#define __UUID_TIME_H__

#include "uuid_ac.h"

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#define TIME_PREFIX uuid_

/* embedding support */
#ifdef TIME_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __TIME_CONCAT(x,y) x ## y
#define TIME_CONCAT(x,y) __TIME_CONCAT(x,y)
#else
#define __TIME_CONCAT(x) x
#define TIME_CONCAT(x,y) __TIME_CONCAT(x)y
#endif
#define time_gettimeofday TIME_CONCAT(TIME_PREFIX,time_gettimeofday)
#define time_usleep       TIME_CONCAT(TIME_PREFIX,time_usleep)
#endif

/* minimum C++ support */
#ifdef __cplusplus
#define DECLARATION_BEGIN extern "C" {
#define DECLARATION_END   }
#else
#define DECLARATION_BEGIN
#define DECLARATION_END
#endif

DECLARATION_BEGIN

#ifndef HAVE_STRUCT_TIMEVAL
struct timeval { long tv_sec; long tv_usec; };
#endif

extern int time_gettimeofday(struct timeval *);
extern int time_usleep(long usec);

DECLARATION_END

#endif /* __UUID_TIME_H__ */

