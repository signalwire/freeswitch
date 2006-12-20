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
 * $Id: mailmime_content.c,v 1.41 2006/05/22 13:39:42 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimf.h"

#include <string.h>
#include <stdlib.h>

#include "mailmime.h"
#include "mailmime_types.h"
#include "mmapstring.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*
  RFC 2045
  RFC 2046
  RFC 2047

  RFC 2231
*/


static int mailmime_parse_with_default(const char * message, size_t length,
    size_t * index, int default_type,
    struct mailmime_content * content_type,
    struct mailmime_fields * mime_fields,
    struct mailmime ** result);



LIBETPAN_EXPORT
char * mailmime_content_charset_get(struct mailmime_content * content)
{
  char * charset;

  charset = mailmime_content_param_get(content, "charset");
  if (charset == NULL)
    return "us-ascii";
  else
    return charset;
}

LIBETPAN_EXPORT
char * mailmime_content_param_get(struct mailmime_content * content,
				  char * name)
{
  clistiter * cur;

  for(cur = clist_begin(content->ct_parameters) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_parameter * param;
    
    param = clist_content(cur);

    if (strcasecmp(param->pa_name, name) == 0)
      return param->pa_value;
  }

  return NULL;
}


/*
     boundary := 0*69<bchars> bcharsnospace
*/

/*
     bchars := bcharsnospace / " "
*/

/*
     bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
                      "+" / "_" / "," / "-" / "." /
                      "/" / ":" / "=" / "?"
*/

/*
     body-part := <"message" as defined in RFC 822, with all
                   header fields optional, not starting with the
                   specified dash-boundary, and with the
                   delimiter not occurring anywhere in the
                   body part.  Note that the semantics of a
                   part differ from the semantics of a message,
                   as described in the text.>
*/

/*
     close-delimiter := delimiter "--"
*/

/*
     dash-boundary := "--" boundary
                      ; boundary taken from the value of
                      ; boundary parameter of the
                      ; Content-Type field.
*/

/*
     delimiter := CRLF dash-boundary
*/

/*
     discard-text := *(*text CRLF)
                     ; May be ignored or discarded.
*/

/*
     encapsulation := delimiter transport-padding
                      CRLF body-part
*/

/*
     epilogue := discard-text
*/

/*
     multipart-body := [preamble CRLF]
                       dash-boundary transport-padding CRLF
                       body-part *encapsulation
                       close-delimiter transport-padding
                       [CRLF epilogue]
*/

/*
     preamble := discard-text
*/

/*
     transport-padding := *LWSP-char
                          ; Composers MUST NOT generate
                          ; non-zero length transport
                          ; padding, but receivers MUST
                          ; be able to handle padding
                          ; added by message transports.
*/


/*
  ACCESS-TYPE
  EXPIRATION
  SIZE
  PERMISSION
*/

/*
  5.2.3.2.  The 'ftp' and 'tftp' Access-Types
  NAME
  SITE
  
      (3)   Before any data are retrieved, using FTP, the user will
          generally need to be asked to provide a login id and a
          password for the machine named by the site parameter.
          For security reasons, such an id and password are not
          specified as content-type parameters, but must be
          obtained from the user.

  optional :
  DIRECTORY
  MODE
*/

/*
5.2.3.3.  The 'anon-ftp' Access-Type
*/

/*
5.2.3.4.  The 'local-file' Access-Type
NAME
SITE
*/

/*
5.2.3.5.  The 'mail-server' Access-Type
SERVER
SUBJECT
*/


enum {
  PREAMBLE_STATE_A0,
  PREAMBLE_STATE_A,
  PREAMBLE_STATE_A1,
  PREAMBLE_STATE_B,
  PREAMBLE_STATE_C,
  PREAMBLE_STATE_D,
  PREAMBLE_STATE_E
};

static int mailmime_preamble_parse(const char * message, size_t length,
    size_t * index, int beol)
{
  int state;
  size_t cur_token;

  cur_token = * index;
  if (beol)
    state = PREAMBLE_STATE_A0;
  else
    state = PREAMBLE_STATE_A;

  while (state != PREAMBLE_STATE_E) {

    if (cur_token >= length)
      return MAILIMF_ERROR_PARSE;

    switch (state) {
    case PREAMBLE_STATE_A0:
      switch (message[cur_token]) {
      case '-':
	state = PREAMBLE_STATE_A1;
	break;
      case '\r':
	state = PREAMBLE_STATE_B;
	break;
      case '\n':
	state = PREAMBLE_STATE_C;
	break;
      default:
	state = PREAMBLE_STATE_A;
	break;
      }
      break;

    case PREAMBLE_STATE_A:
      switch (message[cur_token]) {
      case '\r':
	state = PREAMBLE_STATE_B;
	break;
      case '\n':
	state = PREAMBLE_STATE_C;
	break;
      default:
	state = PREAMBLE_STATE_A;
	break;
      }
      break;

    case PREAMBLE_STATE_A1:
      switch (message[cur_token]) {
      case '-':
	state = PREAMBLE_STATE_E;
	break;
      case '\r':
	state = PREAMBLE_STATE_B;
	break;
      case '\n':
	state = PREAMBLE_STATE_C;
	break;
      default:
	state = PREAMBLE_STATE_A;
	break;
      }
      break;

    case PREAMBLE_STATE_B:
      switch (message[cur_token]) {
      case '\r':
	state = PREAMBLE_STATE_B;
	break;
      case '\n':
	state = PREAMBLE_STATE_C;
	break;
      case '-':
	state = PREAMBLE_STATE_D;
	break;
      default:
	state = PREAMBLE_STATE_A0;
	break;
      }
      break;

    case PREAMBLE_STATE_C:
      switch (message[cur_token]) {
      case '-':
	state = PREAMBLE_STATE_D;
	break;
      case '\r':
	state = PREAMBLE_STATE_B;
	break;
      case '\n':
	state = PREAMBLE_STATE_C;
	break;
      default:
	state = PREAMBLE_STATE_A0;
	break;
      }
      break;

    case PREAMBLE_STATE_D:
      switch (message[cur_token]) {
      case '-':
	state = PREAMBLE_STATE_E;
	break;
      default:
	state = PREAMBLE_STATE_A;
	break;
      }
      break;
    }
    
    cur_token ++;
  }

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

static int mailmime_boundary_parse(const char * message, size_t length,
				   size_t * index, char * boundary)
{
  size_t cur_token;
  size_t len;

  cur_token = * index;

  len = strlen(boundary);

  if (cur_token + len >= length)
    return MAILIMF_ERROR_PARSE;

  if (strncmp(message + cur_token, boundary, len) != 0)
    return MAILIMF_ERROR_PARSE;

  cur_token += len;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

static int is_wsp(char ch)
{
  if ((ch == ' ') || (ch == '\t'))
    return TRUE;

  return FALSE;
}

static int mailmime_lwsp_parse(const char * message, size_t length,
			       size_t * index)
{
  size_t cur_token;

  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  while (is_wsp(message[cur_token])) {
    cur_token ++;
    if (cur_token >= length)
      break;
  }

  if (cur_token == * index)
    return MAILIMF_ERROR_PARSE;
  
  * index = cur_token;
  
  return MAILIMF_NO_ERROR;
}

/*
gboolean mailimf_crlf_parse(gchar * message, guint32 length, guint32 * index)
*/

enum {
  BODY_PART_DASH2_STATE_0,
  BODY_PART_DASH2_STATE_1,
  BODY_PART_DASH2_STATE_2,
  BODY_PART_DASH2_STATE_3,
  BODY_PART_DASH2_STATE_4,
  BODY_PART_DASH2_STATE_5,
  BODY_PART_DASH2_STATE_6
};

static int
mailmime_body_part_dash2_parse(const char * message, size_t length,
			       size_t * index, char * boundary,
			       const char ** result, size_t * result_size)
{
  int state;
  size_t cur_token;
  size_t size;
  size_t begin_text;
  size_t end_text;
  int r;

  cur_token = * index;
  state = BODY_PART_DASH2_STATE_0;

  begin_text = cur_token;
  end_text = length;

  while (state != BODY_PART_DASH2_STATE_5) {

    if (cur_token >= length)
      break;
    
    switch(state) {

    case BODY_PART_DASH2_STATE_0:
      switch (message[cur_token]) {
      case '\r':
	state = BODY_PART_DASH2_STATE_1;
	break;
      case '\n':
	state = BODY_PART_DASH2_STATE_2;
	break;
      default:
	state = BODY_PART_DASH2_STATE_0;
	break;
      }
      break;

    case BODY_PART_DASH2_STATE_1:
      switch (message[cur_token]) {
      case '\n':
	state = BODY_PART_DASH2_STATE_2;
	break;
      default:
	state = BODY_PART_DASH2_STATE_0;
	break;
      }
      break;

    case BODY_PART_DASH2_STATE_2:
      switch (message[cur_token]) {
      case '-':
        end_text = cur_token;
	state = BODY_PART_DASH2_STATE_3;
	break;
      case '\r':
	state = BODY_PART_DASH2_STATE_1;
	break;
      case '\n':
	state = BODY_PART_DASH2_STATE_2;
	break;
      default:
	state = BODY_PART_DASH2_STATE_0;
	break;
      }
      break;

    case BODY_PART_DASH2_STATE_3:
      switch (message[cur_token]) {
      case '\r':
	state = BODY_PART_DASH2_STATE_1;
	break;
      case '\n':
	state = BODY_PART_DASH2_STATE_2;
	break;
      case '-':
	state = BODY_PART_DASH2_STATE_4;
	break;
      default:
	state = BODY_PART_DASH2_STATE_0;
	break;
      }
      break;

    case BODY_PART_DASH2_STATE_4:
      r = mailmime_boundary_parse(message, length, &cur_token, boundary);
      if (r == MAILIMF_NO_ERROR)
	state = BODY_PART_DASH2_STATE_5;
      else
	state = BODY_PART_DASH2_STATE_6;

      break;
    }

    if ((state != BODY_PART_DASH2_STATE_5) &&
	(state != BODY_PART_DASH2_STATE_6))
      cur_token ++;

    if (state == BODY_PART_DASH2_STATE_6)
      state = BODY_PART_DASH2_STATE_0;
  }
  
  size = end_text - begin_text;
  
#if 0  
  if (size > 0) {
    end_text --;
    size --;
  }
#endif

  if (size >= 1) {
    if (message[end_text - 1] == '\r') {
      end_text --;
      size --;
    }
    else if (size >= 1) {
      if (message[end_text - 1] == '\n') {
        end_text --;
        size --;
        if (size >= 1) {
          if (message[end_text - 1] == '\r') {
            end_text --;
            size --;
          }
        }
      }
    }
  }
  
  size = end_text - begin_text;
  if (size == 0)
    return MAILIMF_ERROR_PARSE;

#if 0
  body_part = mailimf_body_new(message + begin_text, size);
  if (body_part == NULL)
    goto err;
#endif

  * result = message + begin_text;
  * result_size = size;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
#if 0
 err:
  return MAILIMF_ERROR_PARSE;
#endif
}

static int
mailmime_body_part_dash2_transport_crlf_parse(const char * message,
    size_t length,
    size_t * index, char * boundary,
    const char ** result, size_t * result_size)
{
  size_t cur_token;
  int r;
  const char * data_str;
  size_t data_size;
  const char * begin_text;
  const char * end_text;
  
  cur_token = * index;
  
  begin_text = message + cur_token;
  end_text = message + cur_token;
  
  while (1) {
    r = mailmime_body_part_dash2_parse(message, length, &cur_token,
        boundary, &data_str, &data_size);
    if (r == MAILIMF_NO_ERROR) {
      end_text = data_str + data_size;
    }
    else {
      return r;
    }
    
    /* parse transport-padding */
    while (1) {
      r = mailmime_lwsp_parse(message, length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
        /* do nothing */
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        break;
      }
      else {
        return r;
      }
    }
    
    r = mailimf_crlf_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      break;
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      return r;
    }
  }
  
  * index = cur_token;
  * result = begin_text;
  * result_size = end_text - begin_text;
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_multipart_close_parse(const char * message, size_t length,
    size_t * index);

static int
mailmime_body_part_dash2_close_parse(const char * message,
    size_t length,
    size_t * index, char * boundary,
    const char ** result, size_t * result_size)
{
  size_t cur_token;
  int r;
  const char * data_str;
  size_t data_size;
  const char * begin_text;
  const char * end_text;
  
  cur_token = * index;
  
  begin_text = message + cur_token;
  end_text = message + cur_token;
  
  while (1) {
    r = mailmime_body_part_dash2_parse(message, length,
        &cur_token, boundary, &data_str, &data_size);
    if (r == MAILIMF_NO_ERROR) {
      end_text = data_str + data_size;
    }
    else {
      return r;
    }
    
    r = mailmime_multipart_close_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      break;
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      return r;
    }
  }
  
  * index = cur_token;
  * result = data_str;
  * result_size = data_size;
  
  return MAILIMF_NO_ERROR;
}

enum {
  MULTIPART_CLOSE_STATE_0,
  MULTIPART_CLOSE_STATE_1,
  MULTIPART_CLOSE_STATE_2,
  MULTIPART_CLOSE_STATE_3,
  MULTIPART_CLOSE_STATE_4
};

static int mailmime_multipart_close_parse(const char * message, size_t length,
    size_t * index)
{
  int state;
  size_t cur_token;

  cur_token = * index;
  state = MULTIPART_CLOSE_STATE_0;

  while (state != MULTIPART_CLOSE_STATE_4) {

    switch(state) {

    case MULTIPART_CLOSE_STATE_0:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch (message[cur_token]) {
      case '-':
	state = MULTIPART_CLOSE_STATE_1;
	break;
      default:
	return MAILIMF_ERROR_PARSE;
      }
      break;

    case MULTIPART_CLOSE_STATE_1:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch (message[cur_token]) {
      case '-':
	state = MULTIPART_CLOSE_STATE_2;
	break;
      default:
	return MAILIMF_ERROR_PARSE;
      }
      break;

    case MULTIPART_CLOSE_STATE_2:
      if (cur_token >= length) {
	state = MULTIPART_CLOSE_STATE_4;
	break;
      }

      switch (message[cur_token]) {
      case ' ':
	state = MULTIPART_CLOSE_STATE_2;
	break;
      case '\t':
	state = MULTIPART_CLOSE_STATE_2;
	break;
      case '\r':
	state = MULTIPART_CLOSE_STATE_3;
	break;
      case '\n':
	state = MULTIPART_CLOSE_STATE_4;
	break;
      default:
	state = MULTIPART_CLOSE_STATE_4;
	break;
      }
      break;

    case MULTIPART_CLOSE_STATE_3:
      if (cur_token >= length) {
	state = MULTIPART_CLOSE_STATE_4;
	break;
      }

      switch (message[cur_token]) {
      case '\n':
	state = MULTIPART_CLOSE_STATE_4;
	break;
      default:
	state = MULTIPART_CLOSE_STATE_4;
	break;
      }
      break;
    }

    cur_token ++;
  }

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

enum {
  MULTIPART_NEXT_STATE_0,
  MULTIPART_NEXT_STATE_1,
  MULTIPART_NEXT_STATE_2
};

LIBETPAN_EXPORT
int mailmime_multipart_next_parse(const char * message, size_t length,
				  size_t * index)
{
  int state;
  size_t cur_token;

  cur_token = * index;
  state = MULTIPART_NEXT_STATE_0;

  while (state != MULTIPART_NEXT_STATE_2) {

    if (cur_token >= length)
      return MAILIMF_ERROR_PARSE;
    
    switch(state) {

    case MULTIPART_NEXT_STATE_0:
      switch (message[cur_token]) {
      case ' ':
	state = MULTIPART_NEXT_STATE_0;
	break;
      case '\t':
	state = MULTIPART_NEXT_STATE_0;
	break;
      case '\r':
	state = MULTIPART_NEXT_STATE_1;
	break;
      case '\n':
	state = MULTIPART_NEXT_STATE_2;
	break;
      default:
	return MAILIMF_ERROR_PARSE;
      }
      break;

    case MULTIPART_NEXT_STATE_1:
      switch (message[cur_token]) {
      case '\n':
	state = MULTIPART_NEXT_STATE_2;
	break;
      default:
	return MAILIMF_ERROR_PARSE;
      }
      break;
    }

    cur_token ++;
  }

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

static int
mailmime_multipart_body_parse(const char * message, size_t length,
    size_t * index, char * boundary,
    int default_subtype,
    clist ** result,
    struct mailmime_data ** p_preamble,
    struct mailmime_data ** p_epilogue)
{
  size_t cur_token;
  clist * list;
  int r;
  int res;
#if 0
  size_t begin;
#endif
  size_t preamble_begin;
  size_t preamble_length;
  size_t preamble_end;
#if 0
  int no_preamble;
  size_t before_crlf;
#endif
  size_t epilogue_begin;
  size_t epilogue_length;
  struct mailmime_data * preamble;
  struct mailmime_data * epilogue;
  size_t part_begin;
  int final_part;
  
  preamble = NULL;
  epilogue = NULL;
  
  cur_token = * index;
  preamble_begin = cur_token;

#if 0
  no_preamble = FALSE;
#endif
  preamble_end = preamble_begin;
  
#if 0
  r = mailmime_preamble_parse(message, length, &cur_token);
  if (r == MAILIMF_NO_ERROR) {
    /* do nothing */
#if 0
    preamble_end = cur_token - 2;
#endif
  }
  else if (r == MAILIMF_ERROR_PARSE) {
    /* do nothing */
    no_preamble = TRUE;
  }
  else {
    res = r;
    goto err;
  }
  
  while (1) {

    preamble_end = cur_token;
    r = mailmime_boundary_parse(message, length, &cur_token, boundary);
    if (r == MAILIMF_NO_ERROR) {
      break;
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }

    r = mailmime_preamble_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
#if 0
      preamble_end = cur_token - 2;
#endif
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      no_preamble = TRUE;
      break;
    }
    else {
      res = r;
      goto err;
    }
  }
  
  if (no_preamble) {
#if 0
    preamble_end = cur_token;
#endif
  }
  else {
    
    r = mailmime_lwsp_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto err;
    }
    
    before_crlf = cur_token;
    r = mailimf_crlf_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
#if 0
      preamble_end = before_crlf;
#endif
      /* remove the CR LF at the end of preamble if any */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
  }
  preamble_length = preamble_end - begin;
#endif
  
  r = mailmime_preamble_parse(message, length, &cur_token, 1);
  if (r == MAILIMF_NO_ERROR) {
    while (1) {
      
      preamble_end = cur_token;
      r = mailmime_boundary_parse(message, length, &cur_token, boundary);
      if (r == MAILIMF_NO_ERROR) {
        break;
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        /* do nothing */
      }
      else {
        res = r;
        goto err;
      }
    
      r = mailmime_preamble_parse(message, length, &cur_token, 0);
      if (r == MAILIMF_NO_ERROR) {
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        break;
      }
      else {
        res = r;
        goto err;
      }
    }
  }
  
  preamble_end -= 2;
  if (preamble_end != preamble_begin) {
    /* try to find the real end of the preamble (strip CR LF) */
    if (message[preamble_end - 1] == '\n') {
      preamble_end --;
      if (preamble_end - 1 >= preamble_begin) {
        if (message[preamble_end - 1] == '\r')
          preamble_end --;
      }
    }
    else if (message[preamble_end - 1] == '\r') {
      preamble_end --;
    }
  }
  preamble_length = preamble_end - preamble_begin;
  
  part_begin = cur_token;
  while (1) {
    r = mailmime_lwsp_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto err;
    }
#if 0
    if (r == MAILIMF_ERROR_PARSE)
      break;
#endif
    
    r = mailimf_crlf_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      part_begin = cur_token;
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
      break;
    }
    else {
      res = r;
      goto err;
    }
  }
  
  cur_token = part_begin;
  
  list = clist_new();
  if (list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }
  
  final_part = 0;
  
  while (!final_part) {
    size_t bp_token;
    struct mailmime * mime_bp;
    const char * data_str;
    size_t data_size;
    struct mailimf_fields * fields;
    struct mailmime_fields * mime_fields;

#if 0    
    int got_crlf;
    size_t after_boundary;
    
    /* XXX - begin */
    r = mailmime_body_part_dash2_parse(message, length, &cur_token,
				       boundary, &data_str, &data_size);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      break;
    }
    else {
      res = r;
      goto free;
    }
    
    after_boundary = cur_token;
    got_crlf = 0;
    /* parse transport-padding */
    while (1) {
      r = mailmime_lwsp_parse(message, length, &cur_token);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
        res = r;
        goto free;
      }
      
      r = mailimf_crlf_parse(message, length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
        got_crlf = 1;
        break;
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        /* do nothing */
        break;
      }
      else {
        res = r;
        goto free;
      }
    }
    if (after_boundary != cur_token) {
      if (!got_crlf) {
        r = mailimf_crlf_parse(message, length, &cur_token);
        if (r == MAILIMF_NO_ERROR) {
          got_crlf = 1;
          break;
        }
      }
    }
    /* XXX - end */
#endif

    r = mailmime_body_part_dash2_transport_crlf_parse(message, length,
        &cur_token, boundary, &data_str, &data_size);
    if (r == MAILIMF_ERROR_PARSE) {
      r = mailmime_body_part_dash2_close_parse(message, length,
          &cur_token, boundary, &data_str, &data_size);
      if (r == MAILIMF_NO_ERROR) {
        final_part = 1;
      }
    }
    
    if (r == MAILIMF_NO_ERROR) {
      bp_token = 0;
      
      r = mailimf_optional_fields_parse(data_str, data_size,
          &bp_token, &fields);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
        res = r;
        goto free;
      }
      
      r = mailimf_crlf_parse(data_str, data_size, &bp_token);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
        mailimf_fields_free(fields);
        res = r;
        goto free;
      }
      
      mime_fields = NULL;
      r = mailmime_fields_parse(fields, &mime_fields);
      mailimf_fields_free(fields);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
        res = r;
        goto free;
      }
      
      r = mailmime_parse_with_default(data_str, data_size,
          &bp_token, default_subtype, NULL,
          mime_fields, &mime_bp);
      if (r == MAILIMF_NO_ERROR) {
        r = clist_append(list, mime_bp);
        if (r < 0) {
          mailmime_free(mime_bp);
          res = MAILIMF_ERROR_MEMORY;
          goto free;
        }
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        mailmime_fields_free(mime_fields);
        break;
      }
      else {
        mailmime_fields_free(mime_fields);
        res = r;
        goto free;
      }
      
      r = mailmime_multipart_next_parse(message, length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
        /* do nothing */
      }
    }
    else {
      res = r;
      goto free;
    }
    
#if 0
    else if (r == MAILIMF_ERROR_PARSE) {
      r = mailmime_body_part_dash2_parse(message, length,
          &cur_token, boundary, &data_str, &data_size);
      if (r != MAILIMF_NO_ERROR) {
        res = r;
        goto free;
      }
      
      r = mailmime_multipart_close_parse(message, length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
        break;
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        res = r;
        goto free;
#if 0
        fprintf(stderr, "close not found, reparse %s\n", boundary);
        /* reparse */
        continue;
#endif
      }
      else {
        res = r;
        goto free;
      }
    }
    else {
      res = r;
      goto free;
    }
#endif
  }
  
  epilogue_begin = length;
  /* parse transport-padding */
  while (1) {
    r = mailmime_lwsp_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto free;
    }
    
    if (r == MAILIMF_ERROR_PARSE)
      break;
    
#if 0
    if (r == MAILIMF_ERROR_PARSE)
      break;
#endif
    
#if 0
    before_crlf = cur_token;
#endif
  }
  
  r = mailimf_crlf_parse(message, length, &cur_token);
  if (r == MAILIMF_NO_ERROR) {
    epilogue_begin = cur_token;
  }
  else if (r != MAILIMF_ERROR_PARSE) {
    res = r;
    goto free;
  }
  
  /* add preamble and epilogue */
  
  epilogue_length = length - epilogue_begin;
  
  if (preamble_length != 0) {
    preamble = mailmime_data_new(MAILMIME_DATA_TEXT,
        MAILMIME_MECHANISM_8BIT, 1,
        message + preamble_begin, preamble_length,
        NULL);
    if (preamble == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto free;
    }
  }
  
  if (epilogue_length != 0) {
    epilogue = mailmime_data_new(MAILMIME_DATA_TEXT,
        MAILMIME_MECHANISM_8BIT, 1,
        message + epilogue_begin, epilogue_length,
        NULL);
    if (epilogue == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto free;
    }
  }
  
  /* end of preamble and epilogue */
  
  cur_token = length;

  * result = list;
  * p_preamble = preamble;
  * p_epilogue = epilogue;
  * index = cur_token;
  
  return MAILIMF_NO_ERROR;

 free:
  if (epilogue != NULL)
    mailmime_data_free(epilogue);
  if (preamble != NULL)
    mailmime_data_free(preamble);
  clist_foreach(list, (clist_func) mailmime_free, NULL);
  clist_free(list);
 err:
  return res;
}

enum {
  MAILMIME_DEFAULT_TYPE_TEXT_PLAIN,
  MAILMIME_DEFAULT_TYPE_MESSAGE
};


LIBETPAN_EXPORT
int mailmime_parse(const char * message, size_t length,
		   size_t * index, struct mailmime ** result)
{
  struct mailmime * mime;
  int r;
  int res;
  struct mailmime_content * content_message;
  size_t cur_token;
  struct mailmime_fields * mime_fields;
  const char * data_str;
  size_t data_size;
  size_t bp_token;

  cur_token = * index;

  content_message = mailmime_get_content_message();
  if (content_message == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

#if 0
  mime_fields = mailmime_fields_new_with_data(content_message,
					      NULL,
					      NULL,
					      NULL,
					      NULL,
					      NULL);
  if (mime_fields == NULL) {
    mailmime_content_free(content_message);
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }
#endif
  mime_fields = mailmime_fields_new_empty();
  if (mime_fields == NULL) {
    mailmime_content_free(content_message);
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  data_str = message + cur_token;
  data_size = length - cur_token;

  bp_token = 0;
  r = mailmime_parse_with_default(data_str, data_size,
      &bp_token, MAILMIME_DEFAULT_TYPE_TEXT_PLAIN,
      content_message, mime_fields, &mime);
  cur_token += bp_token;
  if (r != MAILIMF_NO_ERROR) {
    mailmime_fields_free(mime_fields);
    res = r;
    goto free;
  }
  
  * index = cur_token;
  * result = mime;

  return MAILIMF_NO_ERROR;

 free:
  mailmime_fields_free(mime_fields);
 err:
  return res;
}


LIBETPAN_EXPORT
char * mailmime_extract_boundary(struct mailmime_content * content_type)
{
  char * boundary;

  boundary = mailmime_content_param_get(content_type, "boundary");

  if (boundary != NULL) {
    int len;
    char * new_boundary;

    len = strlen(boundary);
    new_boundary = malloc(len + 1);
    if (new_boundary == NULL)
      return NULL;

    if (boundary[0] == '"') {
      strncpy(new_boundary, boundary + 1, len - 2);
      new_boundary[len - 2] = 0;
    }
    else
      strcpy(new_boundary, boundary);

    boundary = new_boundary;
  }

  return boundary;
}

static void remove_unparsed_mime_headers(struct mailimf_fields * fields)
{
  clistiter * cur;
  
  cur = clist_begin(fields->fld_list);
  while (cur != NULL) {
    struct mailimf_field * field;
    int delete;
    
    field = clist_content(cur);
    
    switch (field->fld_type) {
    case MAILIMF_FIELD_OPTIONAL_FIELD:
      delete = 0;
      if (strncasecmp(field->fld_data.fld_optional_field->fld_name,
              "Content-", 8) == 0) {
        char * name;
        
        name = field->fld_data.fld_optional_field->fld_name + 8;
        if ((strcasecmp(name, "Type") == 0)
            || (strcasecmp(name, "Transfer-Encoding") == 0)
            || (strcasecmp(name, "ID") == 0)
            || (strcasecmp(name, "Description") == 0)
            || (strcasecmp(name, "Disposition") == 0)
            || (strcasecmp(name, "Language") == 0)) {
          delete = 1;
        }
      }
      else if (strcasecmp(field->fld_data.fld_optional_field->fld_name,
                   "MIME-Version") == 0) {
        delete = 1;
      }
      
      if (delete) {
        cur = clist_delete(fields->fld_list, cur);
        mailimf_field_free(field);
      }
      else {
        cur = clist_next(cur);
      }
      break;

    default:
      cur = clist_next(cur);
    }
  }
}

static int mailmime_parse_with_default(const char * message, size_t length,
    size_t * index, int default_type,
    struct mailmime_content * content_type,
    struct mailmime_fields * mime_fields,
    struct mailmime ** result)
{
  size_t cur_token;

  int body_type;

  int encoding;
  struct mailmime_data * body;
  char * boundary;
  struct mailimf_fields * fields;
  clist * list;
  struct mailmime * msg_mime;

  struct mailmime * mime;

  int r;
  int res;
  struct mailmime_data * preamble;
  struct mailmime_data * epilogue;

  /*
    note that when this function is called, content type is always detached,
    even if the function fails
  */

  preamble = NULL;
  epilogue = NULL;
  
  cur_token = * index;

  /* get content type */

  if (content_type == NULL) {
    if (mime_fields != NULL) {
      clistiter * cur;
      
      for(cur = clist_begin(mime_fields->fld_list) ; cur != NULL ;
          cur = clist_next(cur)) {
        struct mailmime_field * field;
        
        field = clist_content(cur);
        if (field->fld_type == MAILMIME_FIELD_TYPE) {
          content_type = field->fld_data.fld_content;
          
          /* detach content type from list */
          field->fld_data.fld_content = NULL;
          clist_delete(mime_fields->fld_list, cur);
          mailmime_field_free(field);
          /*
            there may be a leak due to the detached content type
            in case the function fails
          */
          break;
        }
      }
    }
  }

  /* set default type if no content type */

  if (content_type == NULL) {
    /* content_type is detached, in any case, we will have to free it */
    if (default_type == MAILMIME_DEFAULT_TYPE_TEXT_PLAIN) {
      content_type = mailmime_get_content_text();
      if (content_type == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto err;
      }
    }
    else /* message */ {
      body_type = MAILMIME_MESSAGE;
      
      content_type = mailmime_get_content_message();
      if (content_type == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto err;
      }
    }
  }

  /* get the body type */

  boundary = NULL; /* XXX - removes a gcc warning */

  switch (content_type->ct_type->tp_type) {
  case MAILMIME_TYPE_COMPOSITE_TYPE:
    switch (content_type->ct_type->tp_data.tp_composite_type->ct_type) {
    case MAILMIME_COMPOSITE_TYPE_MULTIPART:
      boundary = mailmime_extract_boundary(content_type);
      
      if (boundary == NULL)
	body_type = MAILMIME_SINGLE;
      else
	body_type = MAILMIME_MULTIPLE;
      break;
      
    case MAILMIME_COMPOSITE_TYPE_MESSAGE:

      if (strcasecmp(content_type->ct_subtype, "rfc822") == 0)
	body_type = MAILMIME_MESSAGE;
      else
	body_type = MAILMIME_SINGLE;
      break;

    default:
      res = MAILIMF_ERROR_INVAL;
      goto free_content;
    }
    break;

  default: /* MAILMIME_TYPE_DISCRETE_TYPE */
    body_type = MAILMIME_SINGLE;
    break;
  }

  /* set body */

  if (mime_fields != NULL)
    encoding = mailmime_transfer_encoding_get(mime_fields);
  else
    encoding = MAILMIME_MECHANISM_8BIT;
  
  cur_token = * index;
  body = mailmime_data_new(MAILMIME_DATA_TEXT, encoding, 1,
      message + cur_token, length - cur_token,
      NULL);
  if (body == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_content;
  }

  /* in case of composite, parse the sub-part(s) */

  list = NULL;
  msg_mime = NULL;
  fields = NULL;

  switch (body_type) {
  case MAILMIME_MESSAGE:
    {
      struct mailmime_fields * submime_fields;
     
      r = mailimf_envelope_and_optional_fields_parse(message, length,
          &cur_token, &fields);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
        res = r;
        goto free_content;
      }
      
      r = mailimf_crlf_parse(message, length, &cur_token);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
        mailimf_fields_free(fields);
        res = r;
        goto free_content;
      }
      
      submime_fields = NULL;
      r = mailmime_fields_parse(fields, &submime_fields);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
        mailimf_fields_free(fields);
        res = r;
        goto free_content;
      }
      
      remove_unparsed_mime_headers(fields);
      
      r = mailmime_parse_with_default(message, length,
          &cur_token, MAILMIME_DEFAULT_TYPE_TEXT_PLAIN,
          NULL, submime_fields, &msg_mime);
      if (r == MAILIMF_NO_ERROR) {
        /* do nothing */
      }
      else if (r == MAILIMF_ERROR_PARSE) {
        mailmime_fields_free(mime_fields);
        msg_mime = NULL;
      }
      else {
        mailmime_fields_free(mime_fields);
        res = r;
        goto free_content;
      }
    }
    
    break;

  case MAILMIME_MULTIPLE:
    {
      int default_subtype;

      default_subtype = MAILMIME_DEFAULT_TYPE_TEXT_PLAIN;
      if (content_type != NULL)
	if (strcasecmp(content_type->ct_subtype, "digest") == 0)
	  default_subtype = MAILMIME_DEFAULT_TYPE_MESSAGE;

      cur_token = * index;
      r = mailmime_multipart_body_parse(message, length,
          &cur_token, boundary,
          default_subtype,
          &list, &preamble, &epilogue);
      if (r == MAILIMF_NO_ERROR) {
	/* do nothing */
      }
      else if (r == MAILIMF_ERROR_PARSE) {
	list = clist_new();
        if (list == NULL) {
          res = MAILIMF_ERROR_MEMORY;
          goto free_content;
        }
      }
      else {
	res = r;
	goto free_content;
      }

      free(boundary);
    }
    break;
    
  default: /* MAILMIME_SINGLE */
    /* do nothing */
    break;
  }

  mime = mailmime_new(body_type, message, length,
      mime_fields, content_type,
      body, preamble, /* preamble */
      epilogue, /* epilogue */
      list, fields, msg_mime);
  if (mime == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = mime;
  * index = length;

  return MAILIMF_NO_ERROR;

 free:
  if (epilogue != NULL)
    mailmime_data_free(epilogue);
  if (preamble != NULL)
    mailmime_data_free(preamble);
  if (msg_mime != NULL)
    mailmime_free(msg_mime);
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailmime_free, NULL);
    clist_free(list);
  }
 free_content:
  mailmime_content_free(content_type);
 err:
  return res;
}

static int mailmime_get_section_list(struct mailmime * mime,
    clistiter * list, struct mailmime ** result)
{
  uint32_t id;
  struct mailmime * data;
  struct mailmime * submime;

  if (list == NULL) {
    * result = mime;
    return MAILIMF_NO_ERROR;
  }

  id = * ((uint32_t *) clist_content(list));

  data = NULL;
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    return MAILIMF_ERROR_INVAL;

  case MAILMIME_MULTIPLE:
    data = clist_nth_data(mime->mm_data.mm_multipart.mm_mp_list, id - 1);
    if (data == NULL)
      return MAILIMF_ERROR_INVAL;

    if (clist_next(list) != NULL)
      return mailmime_get_section_list(data, clist_next(list), result);
    else {
      * result = data;
      return MAILIMF_NO_ERROR;
    }

  case MAILMIME_MESSAGE:
    submime = mime->mm_data.mm_message.mm_msg_mime;
    switch (submime->mm_type) {
    case MAILMIME_MULTIPLE:
      data = clist_nth_data(submime->mm_data.mm_multipart.mm_mp_list, id - 1);
      if (data == NULL)
	return MAILIMF_ERROR_INVAL;
      return mailmime_get_section_list(data, clist_next(list), result);

    default:
      if (id != 1)
	return MAILIMF_ERROR_INVAL;
      
      data = submime;
      if (data == NULL)
	return MAILIMF_ERROR_INVAL;

      return mailmime_get_section_list(data, clist_next(list), result);
    }
    break;

  default:
    return MAILIMF_ERROR_INVAL;
  }
}

LIBETPAN_EXPORT
int mailmime_get_section(struct mailmime * mime,
			 struct mailmime_section * section,
			 struct mailmime ** result)
{
  return mailmime_get_section_list(mime,
      clist_begin(section->sec_list), result);
}















/* ************************************************************************* */
/* MIME part decoding */

static inline signed char get_base64_value(char ch)
{
  if ((ch >= 'A') && (ch <= 'Z'))
    return ch - 'A';
  if ((ch >= 'a') && (ch <= 'z'))
    return ch - 'a' + 26;
  if ((ch >= '0') && (ch <= '9'))
    return ch - '0' + 52;
  switch (ch) {
  case '+':
    return 62;
  case '/':
    return 63;
  case '=': /* base64 padding */
    return -1;
  default:
    return -1;
  }
}

int mailmime_base64_body_parse(const char * message, size_t length,
			       size_t * index, char ** result,
			       size_t * result_len)
{
  size_t cur_token;
  size_t i;
  char chunk[4];
  int chunk_index;
  char out[3];
  MMAPString * mmapstr;
  int res;
  int r;
  size_t written;

  cur_token = * index;
  chunk_index = 0;
  written = 0;

  mmapstr = mmap_string_sized_new((length - cur_token) * 3 / 4);
  if (mmapstr == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  i = 0;
  while (1) {
    signed char value;

    value = -1;
    while (value == -1) {

      if (cur_token >= length)
	break;

      value = get_base64_value(message[cur_token]);
      cur_token ++;
    }

    if (value == -1)
      break;

    chunk[chunk_index] = value;
    chunk_index ++;

    if (chunk_index == 4) {
      out[0] = (chunk[0] << 2) | (chunk[1] >> 4);
      out[1] = (chunk[1] << 4) | (chunk[2] >> 2);
      out[2] = (chunk[2] << 6) | (chunk[3]);

      chunk[0] = 0;
      chunk[1] = 0;
      chunk[2] = 0;
      chunk[3] = 0;
      
      chunk_index = 0;

      if (mmap_string_append_len(mmapstr, out, 3) == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto free;
      }
      written += 3;
    }
  }

  if (chunk_index != 0) {
    size_t len;

    len = 0;
    out[0] = (chunk[0] << 2) | (chunk[1] >> 4);
    len ++;

    if (chunk_index >= 3) {
      out[1] = (chunk[1] << 4) | (chunk[2] >> 2);
      len ++;
    }
	
    if (mmap_string_append_len(mmapstr, out, len) == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto free;
    }
    written += len;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = mmapstr->str;
  * result_len = written;

  return MAILIMF_NO_ERROR;

 free:
  mmap_string_free(mmapstr);
 err:
  return res;
}



static inline int hexa_to_char(char hexdigit)
{
  if ((hexdigit >= '0') && (hexdigit <= '9'))
    return hexdigit - '0';
  if ((hexdigit >= 'a') && (hexdigit <= 'f'))
    return hexdigit - 'a' + 10;
  if ((hexdigit >= 'A') && (hexdigit <= 'F'))
    return hexdigit - 'A' + 10;
  return 0;
}

static inline char to_char(const char * hexa)
{
  return (hexa_to_char(hexa[0]) << 4) | hexa_to_char(hexa[1]);
}

enum {
  STATE_NORMAL,
  STATE_CODED,
  STATE_OUT,
  STATE_CR
};


static int write_decoded_qp(MMAPString * mmapstr,
    const char * start, size_t count)
{
  if (mmap_string_append_len(mmapstr, start, count) == NULL)
    return MAILIMF_ERROR_MEMORY;

  return MAILIMF_NO_ERROR;
}


#define WRITE_MAX_QP 512

int mailmime_quoted_printable_body_parse(const char * message, size_t length,
					 size_t * index, char ** result,
					 size_t * result_len, int in_header)
{
  size_t cur_token;
  int state;
  int r;
  char ch;
  size_t count;
  const char * start;
  MMAPString * mmapstr;
  int res;
  size_t written;

  state = STATE_NORMAL;
  cur_token = * index;

  count = 0;
  start = message + cur_token;
  written = 0;

  mmapstr = mmap_string_sized_new(length - cur_token);
  if (mmapstr == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

#if 0
  if (length >= 1) {
    if (message[length - 1] == '\n') {
      length --;
      if (length >= 1)
	if (message[length - 1] == '\r') {
	  length --;
	}
    }
  }
#endif

  while (state != STATE_OUT) {

    if (cur_token >= length) {
      state = STATE_OUT;
      break;
    }

    switch (state) {
    
    case STATE_CODED:

      if (count > 0) {
	r = write_decoded_qp(mmapstr, start, count);
	if (r != MAILIMF_NO_ERROR) {
	  res = r;
	  goto free;
	}
	written += count;
	count = 0;
      }
      
      switch (message[cur_token]) {
      case '=':
	if (cur_token + 1 >= length) {
          /* error but ignore it */
	  state = STATE_NORMAL;
          start = message + cur_token;
          cur_token ++;
          count ++;
	  break;
	}

	switch (message[cur_token + 1]) {

	case '\n':
	  cur_token += 2;

          start = message + cur_token;

	  state = STATE_NORMAL;
	  break;

	case '\r':
	  if (cur_token + 2 >= length) {
	    state = STATE_OUT;
	    break;
	  }
          
	  if (message[cur_token + 2] == '\n')
	    cur_token += 3;
	  else
	    cur_token += 2;

          start = message + cur_token;

	  state = STATE_NORMAL;

	  break;

	default:
	  if (cur_token + 2 >= length) {
            /* error but ignore it */
            cur_token ++;
            
            start = message + cur_token;
            
            count ++;
	    state = STATE_NORMAL;
	    break;
	  }
          
#if 0
          /* flush before writing additionnal information */
          r = write_decoded_qp(mmapstr, start, count);
          if (r != MAILIMF_NO_ERROR) {
            res = r;
            goto free;
          }
          written += count;
          count = 0;
#endif
          
	  ch = to_char(message + cur_token + 1);
          
	  if (mmap_string_append_c(mmapstr, ch) == NULL) {
	    res = MAILIMF_ERROR_MEMORY;
	    goto free;
	  }
          
	  cur_token += 3;
          written ++;
          
          start = message + cur_token;
          
	  state = STATE_NORMAL;
	  break;
	}
	break;
      }
      break; /* end of STATE_ENCODED */

    case STATE_NORMAL:

      switch (message[cur_token]) {

      case '=':
	state = STATE_CODED;
	break;

      case '\n':
        /* flush before writing additionnal information */
        if (count > 0) {
          r = write_decoded_qp(mmapstr, start, count);
          if (r != MAILIMF_NO_ERROR) {
            res = r;
            goto free;
          }
          written += count;
          
          count = 0;
        }
        
        r = write_decoded_qp(mmapstr, "\r\n", 2);
        if (r != MAILIMF_NO_ERROR) {
          res = r;
          goto free;
        }
        written += 2;
        cur_token ++;
        start = message + cur_token;
        break;
        
      case '\r':
        state = STATE_CR;
        cur_token ++;
        break;

      case '_':
	if (in_header) {
	  if (count > 0) {
	    r = write_decoded_qp(mmapstr, start, count);
	    if (r != MAILIMF_NO_ERROR) {
	      res = r;
	      goto free;
	    }
	    written += count;
	    count = 0;
	  }

	  if (mmap_string_append_c(mmapstr, ' ') == NULL) {
	    res = MAILIMF_ERROR_MEMORY;
	    goto free;
	  }

	  written ++;
	  cur_token ++;
          start = message + cur_token;
	  
	  break;
	}
        /* WARINING : must be followed by switch default action */

      default:
	if (count >= WRITE_MAX_QP) {
	  r = write_decoded_qp(mmapstr, start, count);
	  if (r != MAILIMF_NO_ERROR) {
	    res = r;
	    goto free;
	  }
	  written += count;
	  count = 0;
          start = message + cur_token;
	}
        
	count ++;
	cur_token ++;
	break;
      }
      break; /* end of STATE_NORMAL */

    case STATE_CR:
      switch (message[cur_token]) {
        
      case '\n':
        /* flush before writing additionnal information */
        if (count > 0) {
          r = write_decoded_qp(mmapstr, start, count);
          if (r != MAILIMF_NO_ERROR) {
            res = r;
            goto free;
          }
          written += count;
          count = 0;
        }
        
        r = write_decoded_qp(mmapstr, "\r\n", 2);
        if (r != MAILIMF_NO_ERROR) {
          res = r;
          goto free;
        }
        written += 2;
        cur_token ++;
        start = message + cur_token;
        state = STATE_NORMAL;
        break;
        
      default:
        /* flush before writing additionnal information */
        if (count > 0) {
          r = write_decoded_qp(mmapstr, start, count);
          if (r != MAILIMF_NO_ERROR) {
            res = r;
            goto free;
          }
          written += count;
          count = 0;
        }
        
        start = message + cur_token;
        
        r = write_decoded_qp(mmapstr, "\r\n", 2);
        if (r != MAILIMF_NO_ERROR) {
          res = r;
          goto free;
        }
        written += 2;
        state = STATE_NORMAL;
      }
      break;  /* end of STATE_CR */
    }
  }

  if (count > 0) {
    r = write_decoded_qp(mmapstr, start, count);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto free;
    }
    written += count;
    count = 0;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = mmapstr->str;
  * result_len = written;

  return MAILIMF_NO_ERROR;

 free:
  mmap_string_free(mmapstr);
 err:
  return res;
}

int mailmime_binary_body_parse(const char * message, size_t length,
			       size_t * index, char ** result,
			       size_t * result_len)
{
  MMAPString * mmapstr;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  if (length >= 1) {
    if (message[length - 1] == '\n') {
      length --;
      if (length >= 1)
	if (message[length - 1] == '\r')
	  length --;
    }
  }

  mmapstr = mmap_string_new_len(message + cur_token, length - cur_token);
  if (mmapstr == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * index = length;
  * result = mmapstr->str;
  * result_len = length - cur_token;

  return MAILIMF_NO_ERROR;

 free:
  mmap_string_free(mmapstr);
 err:
  return res;
}


int mailmime_part_parse(const char * message, size_t length,
			size_t * index,
			int encoding, char ** result, size_t * result_len)
{
  switch (encoding) {
  case MAILMIME_MECHANISM_BASE64:
    return mailmime_base64_body_parse(message, length, index,
				      result, result_len);
    
  case MAILMIME_MECHANISM_QUOTED_PRINTABLE:
    return mailmime_quoted_printable_body_parse(message, length, index,
						result, result_len, FALSE);

  case MAILMIME_MECHANISM_7BIT:
  case MAILMIME_MECHANISM_8BIT:
  case MAILMIME_MECHANISM_BINARY:
  default:
    return mailmime_binary_body_parse(message, length, index,
				      result, result_len);
  }
}

int mailmime_get_section_id(struct mailmime * mime,
			    struct mailmime_section ** result)
{
  clist * list;
  int res;
  struct mailmime_section * section_id;
  int r;

  if (mime->mm_parent == NULL) {
    list = clist_new();
    if (list == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }
    
    section_id = mailmime_section_new(list);
    if (section_id == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }
  }
  else {
    uint32_t id;
    uint32_t * p_id;
    clistiter * cur;
    struct mailmime * parent;
    
    r = mailmime_get_section_id(mime->mm_parent, &section_id);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }

    parent = mime->mm_parent;
    switch (parent->mm_type) {
    case MAILMIME_MULTIPLE:
      id = 1;
      for(cur = clist_begin(parent->mm_data.mm_multipart.mm_mp_list) ;
          cur != NULL ; cur = clist_next(cur)) {
	if (clist_content(cur) == mime)
	  break;
	id ++;
      }
      
      p_id = malloc(sizeof(* p_id));
      if (p_id == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto free;
      }
      * p_id = id;
      
      r = clist_append(section_id->sec_list, p_id);
      if (r < 0) {
        free(p_id);
	res = MAILIMF_ERROR_MEMORY;
	goto free;
      }
      break;

    case MAILMIME_MESSAGE:
      if ((mime->mm_type == MAILMIME_SINGLE) ||
          (mime->mm_type == MAILMIME_MESSAGE)) {
	p_id = malloc(sizeof(* p_id));
	if (p_id == NULL) {
	  res = MAILIMF_ERROR_MEMORY;
	  goto free;
	}
	* p_id = 1;
	
	r = clist_append(section_id->sec_list, p_id);
	if (r < 0) {
          free(p_id);
	  res = MAILIMF_ERROR_MEMORY;
	  goto free;
	}
      }
    }
  }

  * result = section_id;

  return MAILIMF_NO_ERROR;

 free:
  mailmime_section_free(section_id);
 err:
  return res;
}
