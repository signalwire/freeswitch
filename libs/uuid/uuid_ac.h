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
**  uuid_ac.c: auto-configuration
*/

#ifndef __UUID_AC_H__
#define __UUID_AC_H__

/* include GNU autoconf results */
#include "config.h"           /* HAVE_xxx */

/* include standard system headers */
#include <stdio.h>            /* NULL, etc. */
#include <stdlib.h>           /* malloc, NULL, etc. */
#include <stdarg.h>           /* va_list, etc. */
#include <string.h>           /* size_t, strlen, etc. */
#include <unistd.h>           /* dmalloc pre-loading */

/* enable optional "dmalloc" support */
#ifdef WITH_DMALLOC
#include <dmalloc.h>          /* malloc override, etc */
#endif

/* define boolean values */
#define UUID_FALSE 0
#define UUID_TRUE  (/*lint -save -e506*/ !UUID_FALSE /*lint -restore*/)

/* determine types of 8-bit size */
#if SIZEOF_CHAR == 1
typedef char uuid_int8_t;
#else
#error unexpected: sizeof(char) != 1 !?
#endif
#if SIZEOF_UNSIGNED_CHAR == 1
typedef unsigned char uuid_uint8_t;
#else
#error unexpected: sizeof(unsigned char) != 1 !?
#endif

/* determine types of 16-bit size */
#if SIZEOF_SHORT == 2
typedef short uuid_int16_t;
#elif SIZEOF_INT == 2
typedef int uuid_int16_t;
#elif SIZEOF_LONG == 2
typedef long uuid_int16_t;
#else
#error unexpected: no type found for uuid_int16_t
#endif
#if SIZEOF_UNSIGNED_SHORT == 2
typedef unsigned short uuid_uint16_t;
#elif SIZEOF_UNSIGNED_INT == 2
typedef unsigned int uuid_uint16_t;
#elif SIZEOF_UNSIGNED_LONG == 2
typedef unsigned long uuid_uint16_t;
#else
#error unexpected: no type found for uuid_uint16_t
#endif

/* determine types of 32-bit size */
#if SIZEOF_SHORT == 4
typedef short uuid_int32_t;
#elif SIZEOF_INT == 4
typedef int uuid_int32_t;
#elif SIZEOF_LONG == 4
typedef long uuid_int32_t;
#elif SIZEOF_LONG_LONG == 4
typedef long long uuid_int32_t;
#else
#error unexpected: no type found for uuid_int32_t
#endif
#if SIZEOF_UNSIGNED_SHORT == 4
typedef unsigned short uuid_uint32_t;
#elif SIZEOF_UNSIGNED_INT == 4
typedef unsigned int uuid_uint32_t;
#elif SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long uuid_uint32_t;
#elif SIZEOF_UNSIGNED_LONG_LONG == 4
typedef unsigned long long uuid_uint32_t;
#else
#error unexpected: no type found for uuid_uint32_t
#endif

#endif /* __UUID_AC_H__ */

