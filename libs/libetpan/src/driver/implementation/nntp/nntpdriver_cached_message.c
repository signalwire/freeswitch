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
 * $Id: nntpdriver_cached_message.c,v 1.19 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "nntpdriver_cached_message.h"

#include <string.h>
#include <stdlib.h>

#include "mail_cache_db.h"

#include "mailmessage.h"
#include "mailmessage_tools.h"
#include "nntpdriver.h"
#include "nntpdriver_tools.h"
#include "nntpdriver_cached.h"
#include "nntpdriver_message.h"
#include "generic_cache.h"

static int nntp_prefetch(mailmessage * msg_info);

static void nntp_prefetch_free(struct generic_message_t * msg);

static int nntp_initialize(mailmessage * msg_info);

static int nntp_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len);

static int nntp_fetch_size(mailmessage * msg_info,
			   size_t * result);

static void nntp_uninitialize(mailmessage * msg_info);

static void nntp_flush(mailmessage * msg_info);

static void nntp_check(mailmessage * msg_info);

static int nntp_get_flags(mailmessage * msg_info,
			  struct mail_flags ** result);

static mailmessage_driver local_nntp_cached_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* msg_name */ "nntp-cached",

  /* msg_initialize */ nntp_initialize,
  /* msg_uninitialize */ nntp_uninitialize,

  /* msg_flush */ nntp_flush,
  /* msg_check */ nntp_check,

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

  /* msg_get_flags */ nntp_get_flags,
#else
  .msg_name = "nntp-cached",

  .msg_initialize = nntp_initialize,
  .msg_uninitialize = nntp_uninitialize,

  .msg_flush = nntp_flush,
  .msg_check = nntp_check,

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

  .msg_get_flags = nntp_get_flags,
#endif
};

mailmessage_driver * nntp_cached_message_driver =
&local_nntp_cached_message_driver;

static inline struct nntp_cached_session_state_data *
get_cached_session_data(mailmessage * msg)
{
  return msg->msg_session->sess_data;
}

static inline mailsession * get_ancestor_session(mailmessage * msg)
{
  return get_cached_session_data(msg)->nntp_ancestor;
}

static inline struct nntp_session_state_data *
get_ancestor_session_data(mailmessage * msg)
{
  return get_ancestor_session(msg)->sess_data;
}

static inline newsnntp *
get_nntp_session(mailmessage * msg)
{
  return get_ancestor_session_data(msg)->nntp_session;
}

static int nntp_prefetch(mailmessage * msg_info)
{
  char * msg_content;
  size_t msg_length;
  struct generic_message_t * msg;
  int r;
  struct nntp_cached_session_state_data * cached_data;
  struct nntp_session_state_data * ancestor_data;
  char filename[PATH_MAX];

  /* we try the cached message */

  cached_data = get_cached_session_data(msg_info);

  ancestor_data = get_ancestor_session_data(msg_info);

  snprintf(filename, PATH_MAX, "%s/%s/%i", cached_data->nntp_cache_directory,
      ancestor_data->nntp_group_name, msg_info->msg_index);
  
  r = generic_cache_read(filename, &msg_content, &msg_length);
  if (r == MAIL_NO_ERROR) {
    msg = msg_info->msg_data;

    msg->msg_message = msg_content;
    msg->msg_length = msg_length;

    return MAIL_NO_ERROR;
  }

  /* we get the message through the network */

  r = nntpdriver_article(get_ancestor_session(msg_info),
      msg_info->msg_index, &msg_content,
      &msg_length);

  if (r != MAIL_NO_ERROR)
    return r;

  /* we write the message cache */

  generic_cache_store(filename, msg_content, msg_length);

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


static void nntp_uninitialize(mailmessage * msg_info)
{
  mailmessage_generic_uninitialize(msg_info);
}

#define FLAGS_NAME "flags.db"

static void nntp_flush(mailmessage * msg_info)
{
  mailmessage_generic_flush(msg_info);
}


static void nntp_check(mailmessage * msg_info)
{
  int r;

  if (msg_info->msg_flags != NULL) {
    r = mail_flags_store_set(get_cached_session_data(msg_info)->nntp_flags_store,
        msg_info);
    /* ignore errors */
  }
}
     
static int nntp_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len)
{
  struct generic_message_t * msg;
  char * headers;
  size_t headers_length;
  struct nntp_cached_session_state_data * cached_data;
  struct nntp_session_state_data * ancestor_data;
  int r;
  char filename[PATH_MAX];

  msg = msg_info->msg_data;

  if (msg->msg_message != NULL)
    return mailmessage_generic_fetch_header(msg_info,
        result, result_len);
  
  /* we try the cached message */
  
  cached_data = get_cached_session_data(msg_info);

  ancestor_data = get_ancestor_session_data(msg_info);

  snprintf(filename, PATH_MAX, "%s/%s/%i-header",
      cached_data->nntp_cache_directory,
      ancestor_data->nntp_group_name, msg_info->msg_index);

  r = generic_cache_read(filename, &headers, &headers_length);
  if (r == MAIL_NO_ERROR) {
    * result = headers;
    * result_len = headers_length;

    return MAIL_NO_ERROR;
  }

  /* we get the message through the network */

  r = nntpdriver_head(get_ancestor_session(msg_info), msg_info->msg_index,
		      &headers, &headers_length);
  if (r != MAIL_NO_ERROR)
    return r;

  /* we write the message cache */

  generic_cache_store(filename, headers, headers_length);

  * result = headers;
  * result_len = headers_length;

  return MAIL_NO_ERROR;
}

static int nntp_fetch_size(mailmessage * msg_info,
			   size_t * result)
{
  return nntpdriver_size(get_ancestor_session(msg_info),
      msg_info->msg_index, result);
}

static int nntp_get_flags(mailmessage * msg_info,
    struct mail_flags ** result)
{
  int r;
  struct mail_flags * flags;
  struct mail_cache_db * cache_db_flags;
  char filename_flags[PATH_MAX];
  int res;
  MMAPString * mmapstr;

  if (msg_info->msg_flags != NULL) {
    * result = msg_info->msg_flags;
    
    return MAIL_NO_ERROR;
  }

  flags = mail_flags_store_get(get_cached_session_data(msg_info)->nntp_flags_store, msg_info->msg_index);

  if (flags == NULL) {
    struct nntp_cached_session_state_data * cached_data;
    struct nntp_session_state_data * ancestor_data;
    
    cached_data = get_cached_session_data(msg_info);
    
    ancestor_data = get_ancestor_session_data(msg_info);
    if (ancestor_data->nntp_group_name == NULL) {
      res = MAIL_ERROR_BAD_STATE;
      goto err;
    }
    
    snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
        cached_data->nntp_flags_directory,
        ancestor_data->nntp_group_name, FLAGS_NAME);
    
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
    
    r = nntpdriver_get_cached_flags(cache_db_flags, mmapstr,
        msg_info->msg_index, &flags);
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
