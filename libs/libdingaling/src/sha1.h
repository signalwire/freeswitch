/* 
 * libDingaLing XMPP Jingle Library
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is libDingaLing XMPP Jingle Library
 *
 * The Initial Developer of the Original Code is
 * Steve Reid <steve@edmweb.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Steve Reid <steve@edmweb.com>
 *
 * sha1.h
 *
 */
/*! \file sha1.h
    \brief SHA1
*/

#ifndef __SHA1_H
#define __SHA1_H
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __STUPIDFORMATBUG__
}
#endif

#undef inline
#define inline __inline

#ifndef uint32_t
#ifdef WIN32
typedef unsigned __int8		uint8_t;
typedef unsigned __int16	uint16_t;
typedef unsigned __int32	uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8		int8_t;
typedef __int16		int16_t;
typedef __int32		int32_t;
typedef __int64		int64_t;
typedef unsigned long	in_addr_t;
#else
#include <limits.h>
#include <inttypes.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#endif
#endif

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} sha_context_t;

void SHA1Transform(uint32_t state[5], unsigned char buffer[64]);
void SHA1Init(sha_context_t* context);
void SHA1Update(sha_context_t* context, unsigned char* data, uint32_t len);
void SHA1Final(unsigned char digest[20], sha_context_t* context);

#ifdef __cplusplus
}
#endif
#endif
