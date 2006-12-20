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
 * $Id: mailmime_decode.c,v 1.30 2006/10/07 17:04:18 hoa Exp $
 */

/*
  RFC 2047 : MIME (Multipurpose Internet Mail Extensions) Part Three:
             Message Header Extensions for Non-ASCII Text
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime_decode.h"

#include <ctype.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#	include <sys/mman.h>
#endif
#include <string.h>
#include <stdlib.h>

#include "mailmime_content.h"

#include "charconv.h"
#include "mmapstring.h"
#include "mailimf.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static int mailmime_charset_parse(const char * message, size_t length,
				  size_t * index, char ** charset);

enum {
  MAILMIME_ENCODING_B,
  MAILMIME_ENCODING_Q
};

static int mailmime_encoding_parse(const char * message, size_t length,
				   size_t * index, int * result);

static int mailmime_etoken_parse(const char * message, size_t length,
				 size_t * index, char ** result);

static int
mailmime_non_encoded_word_parse(const char * message, size_t length,
				size_t * index,
				char ** result);

static int
mailmime_encoded_word_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailmime_encoded_word ** result);
     

enum {
  TYPE_ERROR,
  TYPE_WORD,
  TYPE_ENCODED_WORD
};

LIBETPAN_EXPORT
int mailmime_encoded_phrase_parse(const char * default_fromcode,
    const char * message, size_t length,
    size_t * index, const char * tocode,
    char ** result)
{
  MMAPString * gphrase;
  struct mailmime_encoded_word * word;
  int first;
  size_t cur_token;
  int r;
  int res;
  char * str;
  char * wordutf8;
  int type;

  cur_token = * index;

  gphrase = mmap_string_new("");
  if (gphrase == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  first = TRUE;

  type = TYPE_ERROR; /* XXX - removes a gcc warning */

  while (1) {

    word = NULL;
    r = mailmime_encoded_word_parse(message, length, &cur_token, &word);
    if (r == MAILIMF_NO_ERROR) {
      if (!first) {
	if (type != TYPE_ENCODED_WORD) {
	  if (mmap_string_append_c(gphrase, ' ') == NULL) {
	    mailmime_encoded_word_free(word);
	    res = MAILIMF_ERROR_MEMORY;
	    goto free;
	  }
	}
      }
      type = TYPE_ENCODED_WORD;
      wordutf8 = NULL;
      if (strcasecmp(word->wd_charset, "unknown") == 0) {
        r = charconv(tocode, "iso-8859-1", word->wd_text,
            strlen(word->wd_text), &wordutf8);
      }
      else {
        r = charconv(tocode, word->wd_charset, word->wd_text,
            strlen(word->wd_text), &wordutf8);
      }
      switch (r) {
      case MAIL_CHARCONV_ERROR_MEMORY:
	mailmime_encoded_word_free(word);
	res = MAILIMF_ERROR_MEMORY;
	goto free;

      case MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET:
      case MAIL_CHARCONV_ERROR_CONV:
	mailmime_encoded_word_free(word);
	res = MAILIMF_ERROR_PARSE;
	goto free;
      }
      
      if (wordutf8 != NULL) {
        if (mmap_string_append(gphrase, wordutf8) == NULL) {
          mailmime_encoded_word_free(word);
          free(wordutf8);
          res = MAILIMF_ERROR_MEMORY;
          goto free;
        }
        free(wordutf8);
      }
      mailmime_encoded_word_free(word);
      first = FALSE;
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto free;
    }

    if (r == MAILIMF_ERROR_PARSE) {
      char * raw_word;

      raw_word = NULL;
      r = mailmime_non_encoded_word_parse(message, length,
					  &cur_token, &raw_word);
      if (r == MAILIMF_NO_ERROR) {
	if (!first) {
	  if (mmap_string_append_c(gphrase, ' ') == NULL) {
	    free(raw_word);
	    res = MAILIMF_ERROR_MEMORY;
	    goto free;
	  }
	}
	type = TYPE_WORD;
        
        wordutf8 = NULL;
        r = charconv(tocode, default_fromcode, raw_word,
            strlen(raw_word), &wordutf8);
        
        switch (r) {
        case MAIL_CHARCONV_ERROR_MEMORY:
          free(raw_word);
          res = MAILIMF_ERROR_MEMORY;
          goto free;
          
        case MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET:
        case MAIL_CHARCONV_ERROR_CONV:
          free(raw_word);
          res = MAILIMF_ERROR_PARSE;
          goto free;
        }
        
        if (mmap_string_append(gphrase, wordutf8) == NULL) {
          free(wordutf8);
          free(raw_word);
          res = MAILIMF_ERROR_MEMORY;
          goto free;
        }
        
        free(wordutf8);
	free(raw_word);
	first = FALSE;
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        r = mailimf_fws_parse(message, length, &cur_token);
        if (r != MAILIMF_NO_ERROR) {
          break;
        }
        
        if (mmap_string_append_c(gphrase, ' ') == NULL) {
          res = MAILIMF_ERROR_MEMORY;
          goto free;
        }
	first = FALSE;
        break;
      }
      else {
	res = r;
	goto free;
      }
    }
  }

  if (first) {
    res = MAILIMF_ERROR_PARSE;
    goto free;
  }

  str = strdup(gphrase->str);
  if (str == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }
  mmap_string_free(gphrase);

  * result = str;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  mmap_string_free(gphrase);
 err:
  return res;
}

static int
mailmime_non_encoded_word_parse(const char * message, size_t length,
				size_t * index,
				char ** result)
{
  int end;
  size_t cur_token;
  int res;
  char * text;
  int r;
  size_t begin;

  cur_token = * index;

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  begin = cur_token;

  end = FALSE;
  while (1) {
    if (cur_token >= length)
      break;

    switch (message[cur_token]) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
	end = TRUE;
	break;
    }

    if (end)
      break;

    cur_token ++;
  }

  if (cur_token - begin == 0) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  text = malloc(cur_token - begin + 1);
  if (text == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  memcpy(text, message + begin, cur_token - begin);
  text[cur_token - begin] = '\0';

  * index = cur_token;
  * result = text;

  return MAILIMF_NO_ERROR;

 err:
  return res;
}

static int mailmime_encoded_word_parse(const char * message, size_t length,
				       size_t * index,
				       struct mailmime_encoded_word ** result)
{
  size_t cur_token;
  char * charset;
  int encoding;
  char * text;
  size_t end_encoding;
  char * decoded;
  size_t decoded_len;
  struct mailmime_encoded_word * ew;
  int r;
  int res;
  int opening_quote;
  int end;

  cur_token = * index;

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  opening_quote = FALSE;
  r = mailimf_char_parse(message, length, &cur_token, '\"');
  if (r == MAILIMF_NO_ERROR) {
    opening_quote = TRUE;
  }
  else if (r == MAILIMF_ERROR_PARSE) {
    /* do nothing */  
  }
  else {
    res = r;
    goto err;
  }

  r = mailimf_token_case_insensitive_parse(message, length, &cur_token, "=?");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailmime_charset_parse(message, length, &cur_token, &charset);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_char_parse(message, length, &cur_token, '?');
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_charset;
  }

  r = mailmime_encoding_parse(message, length, &cur_token, &encoding);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_charset;
  }

  r = mailimf_char_parse(message, length, &cur_token, '?');
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_charset;
  }

  end = FALSE;
  end_encoding = cur_token;
  while (1) {
    if (end_encoding >= length)
      break;

    switch (message[end_encoding]) {
      case '?':
#if 0
      case ' ':
#endif
	end = TRUE;
	break;
    }

    if (end)
      break;

    end_encoding ++;
  }

  decoded_len = 0;
  decoded = NULL;
  switch (encoding) {
  case MAILMIME_ENCODING_B:
    r = mailmime_base64_body_parse(message, end_encoding,
				   &cur_token, &decoded,
				   &decoded_len);
      
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto free_charset;
    }
    break;
  case MAILMIME_ENCODING_Q:
    r = mailmime_quoted_printable_body_parse(message, end_encoding,
					     &cur_token, &decoded,
					     &decoded_len, TRUE);

    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto free_charset;
    }

    break;
  }

  text = malloc(decoded_len + 1);
  if (text == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_charset;
  }

  if (decoded_len > 0)
    memcpy(text, decoded, decoded_len);
  text[decoded_len] = '\0';

  mailmime_decoded_part_free(decoded);

  r = mailimf_token_case_insensitive_parse(message, length, &cur_token, "?=");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_encoded_text;
  }

  if (opening_quote) {
    r = mailimf_char_parse(message, length, &cur_token, '\"');
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto free_encoded_text;
    }
  }

  ew = mailmime_encoded_word_new(charset, text);
  if (ew == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_encoded_text;
  }

  * result = ew;
  * index = cur_token;
  
  return MAILIMF_NO_ERROR;

 free_encoded_text:
  mailmime_encoded_text_free(text);
 free_charset:
  mailmime_charset_free(charset);
 err:
  return res;
}

static int mailmime_charset_parse(const char * message, size_t length,
				  size_t * index, char ** charset)
{
  return mailmime_etoken_parse(message, length, index, charset);
}

static int mailmime_encoding_parse(const char * message, size_t length,
				   size_t * index, int * result)
{
  size_t cur_token;
  int encoding;

  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  switch ((char) toupper((unsigned char) message[cur_token])) {
  case 'Q':
    encoding = MAILMIME_ENCODING_Q;
    break;
  case 'B':
    encoding = MAILMIME_ENCODING_B;
    break;
  default:
    return MAILIMF_ERROR_INVAL;
  }

  cur_token ++;

  * result = encoding;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

int is_etoken_char(char ch)
{
  unsigned char uch = ch;

  if (uch < 31)
    return FALSE;

  switch (uch) {
  case ' ':
  case '(':
  case ')':
  case '<':
  case '>':
  case '@':
  case ',':
  case ';':
  case ':':
  case '"':
  case '/':
  case '[':
  case ']':
  case '?':
  case '.':
  case '=':
    return FALSE;
  }

  return TRUE;
}

static int mailmime_etoken_parse(const char * message, size_t length,
				 size_t * index, char ** result)
{
  return mailimf_custom_string_parse(message, length,
				     index, result,
				     is_etoken_char);
}
