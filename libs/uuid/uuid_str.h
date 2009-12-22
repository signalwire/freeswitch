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
**  uuid_str.h: string formatting functions
*/

#ifndef __UUID_STR_H__
#define __UUID_STR_H__

#include <stdarg.h>
#include <string.h>

#define STR_PREFIX uuid_

/* embedding support */
#ifdef STR_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __STR_CONCAT(x,y) x ## y
#define STR_CONCAT(x,y) __STR_CONCAT(x,y)
#else
#define __STR_CONCAT(x) x
#define STR_CONCAT(x,y) __STR_CONCAT(x)y
#endif
#define str_vsnprintf  STR_CONCAT(STR_PREFIX,str_vsnprintf)
#define str_snprintf   STR_CONCAT(STR_PREFIX,str_snprintf)
#define str_vrsprintf  STR_CONCAT(STR_PREFIX,str_vrsprintf)
#define str_rsprintf   STR_CONCAT(STR_PREFIX,str_rsprintf)
#define str_vasprintf  STR_CONCAT(STR_PREFIX,str_vasprintf)
#define str_asprintf   STR_CONCAT(STR_PREFIX,str_asprintf)
#endif

extern int   str_vsnprintf (char  *, size_t, const char *, va_list);
extern int   str_snprintf  (char  *, size_t, const char *, ...);
extern int   str_vrsprintf (char **,         const char *, va_list);
extern int   str_rsprintf  (char **,         const char *, ...);
extern char *str_vasprintf (                 const char *, va_list);
extern char *str_asprintf  (                 const char *, ...);

#endif /* __UUID_STR_H__ */

