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
 * $Id: pop3driver_cached_message.c,v 1.20 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "pop3driver_cached_message.h"

#include <string.h>
#include <stdlib.h>

#include "mail_cache_db.h"

#include "mailmessage.h"
#include "mailmessage_tools.h"
#include "pop3driver.h"
#include "pop3driver_tools.h"
#include "pop3driver_cached.h"
#include "pop3driver_message.h"
#include "generic_cache.h"

static int pop3_prefetch(mailmessage * msg_info);

static void pop3_prefetch_free(struct generic_message_t * msg);

static int pop3_initialize(mailmessage * msg_info);

static void pop3_flush(mailmessage * msg_info);

static void pop3_check(mailmessage * msg_info);

static int pop3_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len);

static int pop3_fetch_size(mailmessage * msg_info,
			   size_t * result);

static int pop3_get_flags(mailmessage * msg_info,
			  struct mail_flags ** result);

static void pop3_uninitialize(mailmessage * msg_info);

static mailmessage_driver local_pop3_cached_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* msg_name */ "pop3-cached",

  /* msg_initialize */ pop3_initialize,
  /* msg_uninitialize */ pop3_uninitialize,

  /* msg_flush */ pop3_flush,
  /* msg_check */ pop3_check,

  /* msg_fetch_result_free */ mailmessage_generic_fetch_result_free,

  /* msg_fetch */ mailmessage_generic_fetch,
  /* msg_fetch_header */ pop3_fetch_header,
  /* msg_fetch_body */ mailmessage_generic_fetch_body,
  /* msg_fetch_size */ pop3_fetch_size,
  /* msg_get_bodystructure */ mailmessage_generic_get_bodystructure,
  /* msg_fetch_section */ mailmessage_generic_fetch_section,
  /* msg_fetch_section_header */ mailmessage_generic_fetch_section_header,
  /* msg_fetch_section_mime */ mailmessage_generic_fetch_section_mime,
  /* msg_fetch_section_body */ mailmessage_generic_fetch_section_body,
  /* msg_fetch_envelope */ mailmessage_generic_fetch_envelope,

  /* msg_get_flags */ pop3_get_flags,
#else
  .msg_name = "pop3-cached",

  .msg_initialize = pop3_initialize,
  .msg_uninitialize = pop3_uninitialize,

  .msg_flush = pop3_flush,
  .msg_check = pop3_check,

  .msg_fetch_result_free = mailmessage_generic_fetch_result_free,

  .msg_fetch = mailmessage_generic_fetch,
  .msg_fetch_header = pop3_fetch_header,
  .msg_fetch_body = mailmessage_generic_fetch_body,
  .msg_fetch_size = pop3_fetch_size,
  .msg_get_bodystructure = mailmessage_generic_get_bodystructure,
  .msg_fetch_section = mailmessage_generic_fetch_section,
  .msg_fetch_section_header = mailmessage_generic_fetch_section_header,
  .msg_fetch_section_mime = mailmessage_generic_fetch_section_mime,
  .msg_fetch_section_body = mailmessage_generic_fetch_section_body,
  .msg_fetch_envelope = mailmessage_generic_fetch_envelope,

  .msg_get_flags = pop3_get_flags,
#endif
};

mailmessage_driver * pop3_cached_message_driver =
&local_pop3_cached_message_driver;


static inline struct pop3_cached_session_state_data *
get_cached_session_data(mailmessage * msg)
{
  return msg->msg_session->sess_data;
}

static inline mailsession * get_ancestor_session(mailmessage * msg)
{
  return get_cached_session_data(msg)->pop3_ancestor;
}

static inline struct pop3_session_state_data *
get_ancestor_session_data(mailmessage * msg)
{
  return get_ancestor_session(msg)->sess_data;
}

static inline mailpop3 * get_pop3_session(mailmessage * msg)
{
  return get_ancestor_session_data(msg)->pop3_session;
}


static int pop3_prefetch(mailmessage * msg_info)
{
  char * msg_content;
  size_t msg_length;
  struct generic_message_t * msg;
  int r;
  struct pop3_cached_session_state_data * cached_data;
  char filename[PATH_MAX];

  /* we try the cached message */

  cached_data = get_cached_session_data(msg_info);

  snprintf(filename, PATH_MAX, "%s/%s",
	   cached_data->pop3_cache_directory, msg_info->msg_uid);

  r = generic_cache_read(filename, &msg_content, &msg_length);
  if (r == MAIL_NO_ERROR) {
    msg = msg_info->msg_data;

    msg->msg_message = msg_content;
    msg->msg_length = msg_length;

    return MAIL_NO_ERROR;
  }

  /* we get the message through the network */

  r = pop3driver_retr(get_ancestor_session(msg_info), msg_info->msg_index,
		      &msg_content, &msg_length);
  if (r != MAIL_NO_ERROR)
    return r;

  /* we write the message cache */

  generic_cache_store(filename, msg_content, msg_length);

  msg = msg_info->msg_data;

  msg->msg_message = msg_content;
  msg->msg_length = msg_length;

  return MAIL_NO_ERROR;
}

static void pop3_prefetch_free(struct generic_message_t * msg)
{
  if (msg->msg_message != NULL) {
    mmap_string_unref(msg->msg_message);
    msg->msg_message = NULL;
  }
}

static int pop3_initialize(mailmessage * msg_info)
{
  struct generic_message_t * msg;
  int r;
  char * uid;
  struct mailpop3_msg_info * info;
  mailpop3 * pop3;

  pop3 = get_pop3_session(msg_info);

  r = mailpop3_get_msg_info(pop3, msg_info->msg_index, &info);
  switch (r) {
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return pop3driver_pop3_error_to_mail_error(r);
  }

  uid = strdup(info->msg_uidl);
  if (uid == NULL)
    return MAIL_ERROR_MEMORY;
  
  r = mailmessage_generic_initialize(msg_info);
  if (r != MAIL_NO_ERROR) {
    free(uid);
    return r;
  }

  msg = msg_info->msg_data;
  msg->msg_prefetch = pop3_prefetch;
  msg->msg_prefetch_free = pop3_prefetch_free;
  msg_info->msg_uid = uid;

  return MAIL_NO_ERROR;
}

static void pop3_uninitialize(mailmessage * msg_info)
{
  mailmessage_generic_uninitialize(msg_info);
}

#define FLAGS_NAME "flags.db"

static void pop3_flush(mailmessage * msg_info)
{
  mailmessage_generic_flush(msg_info);
}

static void pop3_check(mailmessage * msg_info)
{
  int r;

  if (msg_info->msg_flags != NULL) {
    r = mail_flags_store_set(get_cached_session_data(msg_info)->pop3_flags_store,
        msg_info);
  }
}

     
static int pop3_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len)
{
  struct generic_message_t * msg;
  char * headers;
  size_t headers_length;
  int r;
  struct pop3_cached_session_state_data * cached_data;
  char filename[PATH_MAX];

  msg = msg_info->msg_data;

  if (msg->msg_message != NULL)
    return mailmessage_generic_fetch_header(msg_info,
        result, result_len);

  /* we try the cached message */

  cached_data = get_cached_session_data(msg_info);

  snprintf(filename, PATH_MAX, "%s/%s-header",
	   cached_data->pop3_cache_directory, msg_info->msg_uid);

  r = generic_cache_read(filename, &headers, &headers_length);
  if (r == MAIL_NO_ERROR) {
    * result = headers;
    * result_len = headers_length;

    return MAIL_NO_ERROR;
  }

  /* we get the message trough the network */
  
  r = pop3driver_header(get_ancestor_session(msg_info), msg_info->msg_index,
      &headers, &headers_length);
  if (r != MAIL_NO_ERROR)
    return r;
  
  generic_cache_store(filename, headers, headers_length);

  * result = headers;
  * result_len = headers_length;

  return MAIL_NO_ERROR;
}

static int pop3_fetch_size(mailmessage * msg_info,
			   size_t * result)
{
  return pop3driver_size(get_ancestor_session(msg_info),
      msg_info->msg_index, result);
}

static int pop3_get_flags(mailmessage * msg_info,
    struct mail_flags ** result)
{
  int r;
  struct mail_flags * flags;
  struct mail_cache_db * cache_db_flags;
  char filename_flags[PATH_MAX];
  int res;
  struct pop3_cached_session_state_data * cached_data;
  MMAPString * mmapstr;

  if (msg_info->msg_flags != NULL) {
    * result = msg_info->msg_flags;
    
    return MAIL_NO_ERROR;
  }

  cached_data = get_cached_session_data(msg_info);

  flags = mail_flags_store_get(cached_data->pop3_flags_store,
      msg_info->msg_index);
  
  if (flags == NULL) {
    snprintf(filename_flags, PATH_MAX, "%s/%s",
        cached_data->pop3_flags_directory, FLAGS_NAME);
    
    r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    
    mmapstr = mmap_string_new("");
    if (mmapstr == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto close_db_flags;
    }
    
    r = pop3driver_get_cached_flags(cache_db_flags, mmapstr,
        msg_info->msg_session, msg_info->msg_index, &flags);
    if (r != MAIL_NO_ERROR) {
      flags = mail_flags_new_empty();
      if (flags == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_mmapstr;
      }
    }

    mmap_string_free(mmapstr);
    mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  }

  msg_info->msg_flags = flags;

  * result = flags;

  return MAIL_NO_ERROR;

 free_mmapstr:
  mmap_string_free(mmapstr);
 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}
