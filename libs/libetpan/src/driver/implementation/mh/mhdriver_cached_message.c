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
 * $Id: mhdriver_cached_message.c,v 1.24 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mhdriver_message.h"

#include "mailmessage_tools.h"
#include "mhdriver_tools.h"
#include "mhdriver_cached.h"
#include "mailmh.h"
#include "generic_cache.h"

#include "mail_cache_db.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#	include <sys/mman.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

static int mh_prefetch(mailmessage * msg_info);

static void mh_prefetch_free(struct generic_message_t * msg);

static int mh_initialize(mailmessage * msg_info);

static int mh_fetch_size(mailmessage * msg_info,
			 size_t * result);

static int mh_get_flags(mailmessage * msg_info,
			struct mail_flags ** result);

static void mh_uninitialize(mailmessage * msg_info);

static void mh_flush(mailmessage * msg_info);

static void mh_check(mailmessage * msg_info);

static int mh_fetch_header(mailmessage * msg_info,
			   char ** result,
			   size_t * result_len);

static mailmessage_driver local_mh_cached_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* msg_name */ "mh-cached",

  /* msg_initialize */ mh_initialize,
  /* msg_uninitialize */ mh_uninitialize,

  /* msg_flush */ mh_flush,
  /* msg_check */ mh_check,

  /* msg_fetch_result_free */ mailmessage_generic_fetch_result_free,

  /* msg_fetch */ mailmessage_generic_fetch,
  /* msg_fetch_header */ mh_fetch_header,
  /* msg_fetch_body */ mailmessage_generic_fetch_body,
  /* msg_fetch_size */ mh_fetch_size,
  /* msg_get_bodystructure */ mailmessage_generic_get_bodystructure,
  /* msg_fetch_section */ mailmessage_generic_fetch_section,
  /* msg_fetch_section_header */ mailmessage_generic_fetch_section_header,
  /* msg_fetch_section_mime */ mailmessage_generic_fetch_section_mime,
  /* msg_fetch_section_body */ mailmessage_generic_fetch_section_body,
  /* msg_fetch_envelope */ mailmessage_generic_fetch_envelope,

  /* msg_get_flags */ mh_get_flags,
#else
  .msg_name = "mh-cached",

  .msg_initialize = mh_initialize,
  .msg_uninitialize = mh_uninitialize,

  .msg_flush = mh_flush,
  .msg_check = mh_check,

  .msg_fetch_result_free = mailmessage_generic_fetch_result_free,

  .msg_fetch = mailmessage_generic_fetch,
  .msg_fetch_header = mh_fetch_header,
  .msg_fetch_body = mailmessage_generic_fetch_body,
  .msg_fetch_size = mh_fetch_size,
  .msg_get_bodystructure = mailmessage_generic_get_bodystructure,
  .msg_fetch_section = mailmessage_generic_fetch_section,
  .msg_fetch_section_header = mailmessage_generic_fetch_section_header,
  .msg_fetch_section_mime = mailmessage_generic_fetch_section_mime,
  .msg_fetch_section_body = mailmessage_generic_fetch_section_body,
  .msg_fetch_envelope = mailmessage_generic_fetch_envelope,

  .msg_get_flags = mh_get_flags,
#endif
};

mailmessage_driver * mh_cached_message_driver =
&local_mh_cached_message_driver;

static inline struct mh_cached_session_state_data *
get_cached_session_data(mailmessage * msg)
{
  return msg->msg_session->sess_data;
}

static inline mailsession * get_ancestor_session(mailmessage * msg)
{
  return get_cached_session_data(msg)->mh_ancestor;
}

static inline struct mh_session_state_data *
get_ancestor_session_data(mailmessage * msg)
{
  return get_ancestor_session(msg)->sess_data;
}

static inline struct mailmh *
get_mh_session(mailmessage * msg)
{
  return get_ancestor_session_data(msg)->mh_session;
}

static inline struct mailmh_folder *
get_mh_cur_folder(mailmessage * msg)
{
  return get_ancestor_session_data(msg)->mh_cur_folder;
}

static int mh_prefetch(mailmessage * msg_info)
{
  struct generic_message_t * msg;
  int r;
  char * msg_content;
  size_t msg_length;

  r = mhdriver_fetch_message(get_ancestor_session(msg_info),
      msg_info->msg_index, &msg_content, &msg_length);
  if (r != MAIL_NO_ERROR)
    return r;

  msg = msg_info->msg_data;

  msg->msg_message = msg_content;
  msg->msg_length = msg_length;

  return MAIL_NO_ERROR;
}

static void mh_prefetch_free(struct generic_message_t * msg)
{
  if (msg->msg_message != NULL) {
    mmap_string_unref(msg->msg_message);
    msg->msg_message = NULL;
  }
}

static int mh_initialize(mailmessage * msg_info)
{
  struct generic_message_t * msg;
  int r;
  char * uid;
  char static_uid[PATH_MAX];
  struct mailmh_msg_info * mh_msg_info;
  chashdatum key;
  chashdatum data;
  struct mailmh_folder * folder;

  folder = get_mh_cur_folder(msg_info);
  
  key.data = &msg_info->msg_index;
  key.len = sizeof(msg_info->msg_index);
  r = chash_get(folder->fl_msgs_hash, &key, &data);
  if (r < 0)
    return MAIL_ERROR_INVAL;
  
  mh_msg_info = data.data;

  snprintf(static_uid, PATH_MAX, "%u-%lu-%lu", msg_info->msg_index,
	   mh_msg_info->msg_mtime, (unsigned long) mh_msg_info->msg_size);
  uid = strdup(static_uid);
  if (uid == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_generic_initialize(msg_info);
  if (r != MAIL_NO_ERROR) {
    free(uid);
    return r;
  }

  msg = msg_info->msg_data;
  msg->msg_prefetch = mh_prefetch;
  msg->msg_prefetch_free = mh_prefetch_free;
  msg_info->msg_uid = uid;

  return MAIL_NO_ERROR;
}

static void mh_uninitialize(mailmessage * msg_info)
{
  mailmessage_generic_uninitialize(msg_info);
}


#define FLAGS_NAME "flags.db"

static void mh_flush(mailmessage * msg_info)
{
  mailmessage_generic_flush(msg_info);
}

static void mh_check(mailmessage * msg_info)
{
  int r;

  if (msg_info->msg_flags != NULL) {
    r = mail_flags_store_set(get_cached_session_data(msg_info)->mh_flags_store,
        msg_info);
    /* ignore errors */
  }
}
     
static int mh_fetch_size(mailmessage * msg_info,
			 size_t * result)
{
  int r;
  size_t size;

  r = mhdriver_fetch_size(get_ancestor_session(msg_info),
      msg_info->msg_index, &size);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = size;

  return MAIL_NO_ERROR;
}

static int mh_get_flags(mailmessage * msg_info,
    struct mail_flags ** result)
{
  int r;
  struct mail_flags * flags;
  struct mail_cache_db * cache_db_flags;
  char filename_flags[PATH_MAX];
  int res;
  struct mh_cached_session_state_data * cached_data;
  MMAPString * mmapstr;

  if (msg_info->msg_flags != NULL) {
    * result = msg_info->msg_flags;
    
    return MAIL_NO_ERROR;
  }

  cached_data = get_cached_session_data(msg_info);
  
  flags = mail_flags_store_get(cached_data->mh_flags_store,
      msg_info->msg_index);

  if (flags == NULL) {
    if (cached_data->mh_quoted_mb == NULL) {
      res = MAIL_ERROR_BAD_STATE;
      goto err;
    }
    
    snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
        cached_data->mh_flags_directory,
        cached_data->mh_quoted_mb, FLAGS_NAME);
    
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
    
    r = mhdriver_get_cached_flags(cache_db_flags, mmapstr,
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

static int mh_fetch_header(mailmessage * msg_info,
			   char ** result,
			   size_t * result_len)
{
  struct generic_message_t * msg;
  int r;
  char * msg_content;
  size_t msg_length;

  msg = msg_info->msg_data;
  if (msg->msg_message != NULL) {
    return mailmessage_generic_fetch_header(msg_info, result, result_len);
  }
  else {
    r = mhdriver_fetch_header(get_ancestor_session(msg_info),
        msg_info->msg_index, &msg_content, &msg_length);
    if (r != MAIL_NO_ERROR)
      return r;
    
    * result = msg_content;
    * result_len = msg_length;
    
    return MAIL_NO_ERROR;
  }
}
