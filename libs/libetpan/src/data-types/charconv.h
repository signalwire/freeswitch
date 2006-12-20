/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: charconv.h,v 1.13 2006/06/16 09:25:23 smarinier Exp $
 */

#ifndef CHARCONV_H

#define CHARCONV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#ifndef LIBETPAN_CONFIG_H
#	include <libetpan/libetpan-config.h>
#endif

enum {
  MAIL_CHARCONV_NO_ERROR = 0,
  MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET,
  MAIL_CHARCONV_ERROR_MEMORY,
  MAIL_CHARCONV_ERROR_CONV
};

/**
*	define your own conversion. 
*		- result is big enough to contain your converted string 
*		- result_len contain the maximum size available (out value must contain the final converted size)
*		- your conversion return an error code based on upper enum values
*/
LIBETPAN_EXPORT
extern int (*extended_charconv)(const char * tocode, const char * fromcode, const char * str, size_t length,
    char * result, size_t* result_len);

LIBETPAN_EXPORT
int charconv(const char * tocode, const char * fromcode,
    const char * str, size_t length,
    char ** result);

LIBETPAN_EXPORT
int charconv_buffer(const char * tocode, const char * fromcode,
		    const char * str, size_t length,
		    char ** result, size_t * result_len);

LIBETPAN_EXPORT
void charconv_buffer_free(char * str);

#ifdef __cplusplus
}
#endif

#endif
