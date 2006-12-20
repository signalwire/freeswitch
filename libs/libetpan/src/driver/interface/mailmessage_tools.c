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
 * $Id: mailmessage_tools.c,v 1.24 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmessage_tools.h"
#include "mailmessage.h"

#include <stdlib.h>

#include "maildriver.h"
#include "maildriver_tools.h"

int
mailmessage_generic_initialize(mailmessage * msg_info)
{
  struct generic_message_t * msg;

  msg = malloc(sizeof(* msg));

  if (msg == NULL) {
    return MAIL_ERROR_MEMORY;
  }
  
  msg->msg_fetched = 0;
  msg->msg_message = NULL;
  msg->msg_length = 0;

  msg->msg_prefetch = NULL;
  msg->msg_prefetch_free = NULL;
  msg->msg_data = NULL;

  msg_info->msg_data = msg;

  return MAIL_NO_ERROR;
}

void mailmessage_generic_flush(mailmessage * msg_info)
{
  struct generic_message_t * msg;

  if (msg_info->msg_mime != NULL) {
    mailmime_free(msg_info->msg_mime);
    msg_info->msg_mime = NULL;
  }
  msg = msg_info->msg_data;
  if (msg != NULL) {
    if (msg->msg_prefetch_free != NULL)
      msg->msg_prefetch_free(msg);
    msg->msg_fetched = 0;
  }
}

void mailmessage_generic_uninitialize(mailmessage * msg_info)
{
  struct generic_message_t * msg;

  mailmessage_generic_flush(msg_info);

  msg = msg_info->msg_data;
  msg_info->msg_data = NULL;
  free(msg);
}

static inline int
mailmessage_generic_prefetch(mailmessage * msg_info)
{
  struct generic_message_t * msg;
  int r;
  
  msg = msg_info->msg_data;
  
  if (msg->msg_fetched)
    return MAIL_NO_ERROR;
  
#if 0
  if (msg->message != NULL)
    return MAIL_NO_ERROR;
#endif
  
  r = msg->msg_prefetch(msg_info);
  if (r != MAIL_NO_ERROR)
    return r;
  
  msg->msg_fetched = 1;
  
  return MAIL_NO_ERROR;
}

static int
mailmessage_generic_prefetch_bodystructure(mailmessage * msg_info)
{
  size_t length;
  char * message;
  size_t cur_token;
  struct mailmime * mime;
  int r;
  int res;
  struct generic_message_t * msg;

  if (msg_info->msg_mime != NULL) {
    /* it has already been fetched */
    return MAIL_NO_ERROR;
  }

#if 0
  msg = msg_info->data;
  if (msg->message == NULL) {
    r = mailmessage_generic_prefetch(msg_info);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
  }
#endif
  r = mailmessage_generic_prefetch(msg_info);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  msg = msg_info->msg_data;
  message = msg->msg_message;
  length = msg->msg_length;
  cur_token = 0;
  r = mailmime_parse(message, length, &cur_token, &mime);
  if (r != MAILIMF_NO_ERROR) {
    res = MAIL_ERROR_PARSE;
    goto err;
  }

  msg_info->msg_mime = mime;

  return MAIL_NO_ERROR;

 err:
  return res;
}

void
mailmessage_generic_fetch_result_free(mailmessage * msg_info, char * msg)
{
  int r;
  
  r = mmap_string_unref(msg);
}

int mailmessage_generic_fetch(mailmessage * msg_info,
			      char ** result,
			      size_t * result_len)
{
  int r;
  char * message;
  size_t cur_token;
  size_t length;
  MMAPString * mmapstr;
  int res;
  struct generic_message_t * msg;

  msg = msg_info->msg_data;
  r = mailmessage_generic_prefetch(msg_info);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  message = msg->msg_message;
  length = msg->msg_length;
  cur_token = 0;
  
  mmapstr = mmap_string_new_len(message, length);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmap;
  }
  
  * result = mmapstr->str;
  * result_len = length;

  return MAIL_NO_ERROR;

 free_mmap:
  mmap_string_free(mmapstr);
 err:
  return res;
}

int mailmessage_generic_fetch_header(mailmessage * msg_info,
				     char ** result,
				     size_t * result_len)
{
  int r;
  char * message;
  size_t cur_token;
  size_t length;
  MMAPString * mmapstr;
  char * headers;
  int res;
  struct generic_message_t * msg;

  msg = msg_info->msg_data;
  r = mailmessage_generic_prefetch(msg_info);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  message = msg->msg_message;
  length = msg->msg_length;
  cur_token = 0;
  
  while (1) {
    r = mailimf_ignore_field_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else
      break;
  }
  mailimf_crlf_parse(message, length, &cur_token);
  
  mmapstr = mmap_string_new_len(message, cur_token);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmap;
  }
  
  headers = mmapstr->str;

  * result = headers;
  * result_len = cur_token;

  return MAIL_NO_ERROR;

 free_mmap:
  mmap_string_free(mmapstr);
 err:
  return res;
}

int mailmessage_generic_fetch_body(mailmessage * msg_info,
				   char ** result, size_t * result_len)
{
  int r;
  char * message;
  size_t cur_token;
  MMAPString * mmapstr;
  size_t length;
  int res;
  struct generic_message_t * msg;

  msg = msg_info->msg_data;
  r = mailmessage_generic_prefetch(msg_info);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  message = msg->msg_message;
  length = msg->msg_length;
  cur_token = 0;

  while (1) {
    r = mailimf_ignore_field_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else
      break;
  }
  mailimf_crlf_parse(message, length, &cur_token);

  mmapstr = mmap_string_new_len(message + cur_token, length - cur_token);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmap;
  }

  * result = mmapstr->str;
  * result_len = length - cur_token;

  return MAIL_NO_ERROR;

 free_mmap:
  mmap_string_free(mmapstr);
 err:
  return res;
}




int
mailmessage_generic_get_bodystructure(mailmessage * msg_info,
				      struct mailmime ** result)
{
  int r;

  r = mailmessage_generic_prefetch_bodystructure(msg_info);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = msg_info->msg_mime;

  return MAIL_NO_ERROR;
}




int
mailmessage_generic_fetch_section(mailmessage * msg_info,
				  struct mailmime * mime,
				  char ** result, size_t * result_len)
{
  MMAPString * mmapstr;
  int r;
  int res;

  mmapstr = mmap_string_new_len(mime->mm_body->dt_data.dt_text.dt_data,
				mime->mm_body->dt_data.dt_text.dt_length);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmap;
  }

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  return MAIL_NO_ERROR;

 free_mmap:
  mmap_string_free(mmapstr);
 err:
  return res;
}

int
mailmessage_generic_fetch_section_header(mailmessage * msg_info,
					 struct mailmime * mime,
					 char ** result,
					 size_t * result_len)
{
  MMAPString * mmapstr;
  int r;
  int res;
  size_t cur_token;

  /* skip mime */

  cur_token = 0;

  if (mime->mm_type == MAILMIME_MESSAGE) {

    while (1) {
      r = mailimf_ignore_field_parse(mime->mm_body->dt_data.dt_text.dt_data,
          mime->mm_body->dt_data.dt_text.dt_length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
	/* do nothing */
      }
      else
	break;
    }
    
    r = mailimf_crlf_parse(mime->mm_body->dt_data.dt_text.dt_data,
        mime->mm_body->dt_data.dt_text.dt_length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = maildriver_imf_error_to_mail_error(r);
      goto err;
    }
  }

  mmapstr = mmap_string_new_len(mime->mm_body->dt_data.dt_text.dt_data,
      cur_token);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmap;
  }

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  return MAIL_NO_ERROR;

 free_mmap:
  mmap_string_free(mmapstr);
 err:
  return res;
}

int
mailmessage_generic_fetch_section_mime(mailmessage * msg_info,
				       struct mailmime * mime,
				       char ** result,
				       size_t * result_len)
{
  MMAPString * mmapstr;
  int r;
  int res;
  size_t cur_token;

  cur_token = 0;

  /* skip header */
  
  while (1) {
    r = mailimf_ignore_field_parse(mime->mm_mime_start,
				   mime->mm_length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else
      break;
  }

  r = mailimf_crlf_parse(mime->mm_mime_start, mime->mm_length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = maildriver_imf_error_to_mail_error(r);
    goto err;
  }

  mmapstr = mmap_string_new_len(mime->mm_mime_start, cur_token);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmap;
  }

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  return MAIL_NO_ERROR;

 free_mmap:
  mmap_string_free(mmapstr);
 err:
  return res;
}

int
mailmessage_generic_fetch_section_body(mailmessage * msg_info,
				       struct mailmime * mime,
				       char ** result,
				       size_t * result_len)
{
  MMAPString * mmapstr;
  int r;
  int res;
  size_t cur_token;

  cur_token = 0;

  if (mime->mm_type == MAILMIME_MESSAGE) {

    /* skip header */

    while (1) {
      r = mailimf_ignore_field_parse(mime->mm_body->dt_data.dt_text.dt_data,
          mime->mm_body->dt_data.dt_text.dt_length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
	/* do nothing */
      }
      else
	break;
    }
    
    r = mailimf_crlf_parse(mime->mm_body->dt_data.dt_text.dt_data,
        mime->mm_body->dt_data.dt_text.dt_length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = maildriver_imf_error_to_mail_error(r);
      goto err;
    }
  }

  mmapstr = mmap_string_new_len(mime->mm_body->dt_data.dt_text.dt_data +
      cur_token, mime->mm_body->dt_data.dt_text.dt_length - cur_token);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmap;
  }

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  return MAIL_NO_ERROR;

 free_mmap:
  mmap_string_free(mmapstr);
 err:
  return res;
}

int mailmessage_generic_fetch_envelope(mailmessage * msg_info,
				       struct mailimf_fields ** result)
{
  int r;
  int res;
  size_t cur_token;
  char * header;
  size_t length;
  struct mailimf_fields * fields;

  r = mailmessage_fetch_header(msg_info, &header, &length);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  cur_token = 0;
  
  r = mailimf_envelope_fields_parse(header, length, &cur_token,
				    &fields);
  if (r != MAILIMF_NO_ERROR) {
    res = maildriver_imf_error_to_mail_error(r);
    goto free;
    /* do nothing */
  }

  mailmessage_fetch_result_free(msg_info, header);
  
  * result = fields;

  return MAIL_NO_ERROR;

 free:
  mailmessage_fetch_result_free(msg_info, header);
 err:
  return res;
}
