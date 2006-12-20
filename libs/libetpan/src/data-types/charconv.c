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
 * $Id: charconv.c,v 1.22 2006/07/03 16:36:08 skunk Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "charconv.h"

#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "mmapstring.h"

int (*extended_charconv)(const char * tocode, const char * fromcode, const char * str, size_t length,
    char * result, size_t* result_len) = NULL;

#ifdef HAVE_ICONV
static size_t mail_iconv (iconv_t cd, const char **inbuf, size_t *inbytesleft,
    char **outbuf, size_t *outbytesleft,
    char **inrepls, char *outrepl)
{
  size_t ret = 0, ret1;
  /* XXX - force const to mutable */
  char *ib = (char *) *inbuf;
  size_t ibl = *inbytesleft;
  char *ob = *outbuf;
  size_t obl = *outbytesleft;

  for (;;)
  {
#ifdef HAVE_ICONV_PROTO_CONST
    ret1 = iconv (cd, (const char **) &ib, &ibl, &ob, &obl);
#else
    ret1 = iconv (cd, &ib, &ibl, &ob, &obl);
#endif
    if (ret1 != (size_t)-1)
      ret += ret1;
    if (ibl && obl && errno == EILSEQ)
    {
      if (inrepls)
      {
	/* Try replacing the input */
	char **t;
	for (t = inrepls; *t; t++)
	{
	  char *ib1 = *t;
	  size_t ibl1 = strlen (*t);
	  char *ob1 = ob;
	  size_t obl1 = obl;
#ifdef HAVE_ICONV_PROTO_CONST
	  iconv (cd, (const char **) &ib1, &ibl1, &ob1, &obl1);
#else
	  iconv (cd, &ib1, &ibl1, &ob1, &obl1);
#endif
	  if (!ibl1)
	  {
	    ++ib, --ibl;
	    ob = ob1, obl = obl1;
	    ++ret;
	    break;
	  }
	}
	if (*t)
	  continue;
      }
      if (outrepl)
      {
	/* Try replacing the output */
	size_t n = strlen (outrepl);
	if (n <= obl)
	{
	  memcpy (ob, outrepl, n);
	  ++ib, --ibl;
	  ob += n, obl -= n;
	  ++ret;
	  continue;
	}
      }
    }
    *inbuf = ib, *inbytesleft = ibl;
    *outbuf = ob, *outbytesleft = obl;
    return ret;
  }
}
#endif

LIBETPAN_EXPORT
int charconv(const char * tocode, const char * fromcode,
    const char * str, size_t length,
    char ** result)
{
#ifdef HAVE_ICONV
	iconv_t conv;
	size_t r;
	char * pout;
	size_t out_size;
	size_t old_out_size;
	size_t count;
#endif
	char * out;
	int res;

	if (extended_charconv != NULL) {
		size_t		result_length;
		result_length = length * 6;
		*result = malloc( length * 6 + 1);
		if (*result == NULL) {
			res = MAIL_CHARCONV_ERROR_MEMORY;
		} else {
			res = (*extended_charconv)( tocode, fromcode, str, length, *result, &result_length);
			if (res != MAIL_CHARCONV_NO_ERROR) {
				free( *result);
			} else {
				out = realloc( *result, result_length + 1);
				if (out != NULL) *result = out;
				/* also a cstring, just in case */
				(*result)[result_length] = '\0';
			}
		}
		if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
			return res;
		/* else, let's try with iconv, if available */
	}

#ifndef HAVE_ICONV
  return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
#else

  conv = iconv_open(tocode, fromcode);
  if (conv == (iconv_t) -1) {
    res = MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    goto err;
  }

  out_size = 6 * length; /* UTF-8 can be encoded up to 6 bytes */

  out = malloc(out_size + 1);
  if (out == NULL) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto close_iconv;
  }

  pout = out;
  old_out_size = out_size;

  r = mail_iconv(conv, &str, &length, &pout, &out_size, NULL, "?");

  if (r == (size_t) -1) {
    res = MAIL_CHARCONV_ERROR_CONV;
    goto free;
  }

  iconv_close(conv);

  * pout = '\0';
  count = old_out_size - out_size;
  pout = realloc(out, count + 1);
  if (pout != NULL)
    out = pout;

  * result = out;

  return MAIL_CHARCONV_NO_ERROR;

 free:
  free(out);
 close_iconv:
  iconv_close(conv);
 err:
  return res;
#endif
}

LIBETPAN_EXPORT
int charconv_buffer(const char * tocode, const char * fromcode,
		    const char * str, size_t length,
		    char ** result, size_t * result_len)
{
#ifdef HAVE_ICONV
	iconv_t conv;
	size_t iconv_r;
	int r;
	char * out;
	char * pout;
	size_t out_size;
	size_t old_out_size;
	size_t count;
#endif
	int res;
	MMAPString * mmapstr;

	if (extended_charconv != NULL) {
		size_t		result_length;
		result_length = length * 6;
		mmapstr = mmap_string_sized_new( result_length + 1);
		*result_len = 0;
		if (mmapstr == NULL) {
			res = MAIL_CHARCONV_ERROR_MEMORY;
		} else {
			res = (*extended_charconv)( tocode, fromcode, str, length, mmapstr->str, &result_length);
			if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET) {
				if (res == MAIL_CHARCONV_NO_ERROR) {
					*result = mmapstr->str;
					res = mmap_string_ref(mmapstr);
					if (res < 0) {
						res = MAIL_CHARCONV_ERROR_MEMORY;
						mmap_string_free(mmapstr);
					} else {
						mmap_string_set_size( mmapstr, result_length);	/* can't fail */
						*result_len = result_length;
					}
				}
				free( *result);
			}
			return res;
		}
		/* else, let's try with iconv, if available */
	}

#ifndef HAVE_ICONV
  return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
#else

  conv = iconv_open(tocode, fromcode);
  if (conv == (iconv_t) -1) {
    res = MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    goto err;
  }

  out_size = 6 * length; /* UTF-8 can be encoded up to 6 bytes */

  mmapstr = mmap_string_sized_new(out_size + 1);
  if (mmapstr == NULL) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto err;
  }

  out = mmapstr->str;

  pout = out;
  old_out_size = out_size;

  iconv_r = mail_iconv(conv, &str, &length, &pout, &out_size, NULL, "?");

  if (iconv_r == (size_t) -1) {
    res = MAIL_CHARCONV_ERROR_CONV;
    goto free;
  }

  iconv_close(conv);

  * pout = '\0';

  count = old_out_size - out_size;

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto free;
  }

  * result = out;
  * result_len = count;

  return MAIL_CHARCONV_NO_ERROR;

 free:
  mmap_string_free(mmapstr);
 err:
  return res;
#endif
}

LIBETPAN_EXPORT
void charconv_buffer_free(char * str)
{
  mmap_string_unref(str);
}
