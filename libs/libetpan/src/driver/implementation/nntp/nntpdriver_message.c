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
 * $Id: nntpdriver_message.c,v 1.15 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "nntpdriver_message.h"

#include "mailmessage_tools.h"
#include "nntpdriver_tools.h"
#include "nntpdriver.h"
#include "newsnntp.h"
#include <string.h>
#include <stdlib.h>

static int nntp_prefetch(mailmessage * msg_info);

static void nntp_prefetch_free(struct generic_message_t * msg);

static int nntp_initialize(mailmessage * msg_info);

static int nntp_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len);

static int nntp_fetch_size(mailmessage * msg_info,
			   size_t * result);

static mailmessage_driver local_nntp_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L

  /* msg_name */ "nntp",

  /* msg_initialize */ nntp_initialize,
  /* msg_uninitialize */ mailmessage_generic_uninitialize,

  /* msg_flush */ mailmessage_generic_flush,
  /* msg_check */ NULL,

  /* msg_fetch_result_free */ mailmessage_generic_fetch_result_free,

  /* msg_fetch */ mailmessage_generic_fetch,
  /* msg_fetch_header */ nntp_fetch_header,
  /* msg_fetch_body */ mailmessage_generic_fetch_body,
  /* msg_fetch_size */ nntp_fetch_size,
  /* msg_get_bodystructure */ mailmessage_generic_get_bodystructure,
  /* msg_fetch_section */ mailmessage_generic_fetch_section,
  /* msg_fetch_section_header */ mailmessage_generic_fetch_section_header,
  /* msg_fetch_section_mime */ mailmessage_generic_fetch_section_mime,
  /* msg_fetch_section_body */ mailmessage_generic_fetch_section_body,
  /* msg_fetch_envelope */ mailmessage_generic_fetch_envelope,

  /* msg_get_flags */ NULL,
#else
  .msg_name = "nntp",

  .msg_initialize = nntp_initialize,
  .msg_uninitialize = mailmessage_generic_uninitialize,

  .msg_flush = mailmessage_generic_flush,
  .msg_check = NULL,

  .msg_fetch_result_free = mailmessage_generic_fetch_result_free,

  .msg_fetch = mailmessage_generic_fetch,
  .msg_fetch_header = nntp_fetch_header,
  .msg_fetch_body = mailmessage_generic_fetch_body,
  .msg_fetch_size = nntp_fetch_size,
  .msg_get_bodystructure = mailmessage_generic_get_bodystructure,
  .msg_fetch_section = mailmessage_generic_fetch_section,
  .msg_fetch_section_header = mailmessage_generic_fetch_section_header,
  .msg_fetch_section_mime = mailmessage_generic_fetch_section_mime,
  .msg_fetch_section_body = mailmessage_generic_fetch_section_body,
  .msg_fetch_envelope = mailmessage_generic_fetch_envelope,

  .msg_get_flags = NULL,
#endif
};

mailmessage_driver * nntp_message_driver = &local_nntp_message_driver;

static int nntp_prefetch(mailmessage * msg_info)
{
  char * msg_content;
  size_t msg_length;
  struct generic_message_t * msg;
  int r;

  r = nntpdriver_article(msg_info->msg_session, msg_info->msg_index,
      &msg_content, &msg_length);
  if (r != MAIL_NO_ERROR)
    return r;

  msg = msg_info->msg_data;

  msg->msg_message = msg_content;
  msg->msg_length = msg_length;

  return MAIL_NO_ERROR;
}

static void nntp_prefetch_free(struct generic_message_t * msg)
{
  if (msg->msg_message != NULL) {
    mmap_string_unref(msg->msg_message);
    msg->msg_message = NULL;
  }
}

static int nntp_initialize(mailmessage * msg_info)
{
  struct generic_message_t * msg;
  int r;
  char * uid;
  char static_uid[20];

  snprintf(static_uid, 20, "%u", msg_info->msg_index);
  uid = strdup(static_uid);
  if (uid == NULL)
    return MAIL_ERROR_MEMORY;
  
  r = mailmessage_generic_initialize(msg_info);
  if (r != MAIL_NO_ERROR) {
    free(uid);
    return r;
  }

  msg = msg_info->msg_data;
  msg->msg_prefetch = nntp_prefetch;
  msg->msg_prefetch_free = nntp_prefetch_free;
  msg_info->msg_uid = uid;

  return MAIL_NO_ERROR;
}

static int nntp_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len)
{
  struct generic_message_t * msg;
  char * headers;
  size_t headers_length;
  int r;
  
  msg = msg_info->msg_data;

  if (msg->msg_message != NULL)
    return mailmessage_generic_fetch_header(msg_info,
        result, result_len);
  
  r = nntpdriver_head(msg_info->msg_session, msg_info->msg_index,
      &headers, &headers_length);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = headers;
  * result_len = headers_length;

  return MAIL_NO_ERROR;
}

static int nntp_fetch_size(mailmessage * msg_info,
			   size_t * result)
{
  return nntpdriver_size(msg_info->msg_session, msg_info->msg_index, result);
}
