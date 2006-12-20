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
 * $Id: dbdriver_message.c,v 1.6 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "dbdriver_message.h"
#include "dbdriver.h"
#include "mail_cache_db.h"

#include "mailmessage_tools.h"
#include "generic_cache.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

static int fetch_envelope(mailmessage * msg_info,
    struct mailimf_fields ** result);

static int get_flags(mailmessage * msg_info,
    struct mail_flags ** result);

static int prefetch(mailmessage * msg_info);

static void prefetch_free(struct generic_message_t * msg);

static int initialize(mailmessage * msg_info);

static void check(mailmessage * msg_info);

static mailmessage_driver local_db_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* msg_name */ "db",

  /* msg_initialize */ initialize,
  /* msg_uninitialize */ mailmessage_generic_uninitialize,

  /* msg_flush */ mailmessage_generic_flush,
  /* msg_check */ check,

  /* msg_fetch_result_free */ mailmessage_generic_fetch_result_free,

  /* msg_fetch */ mailmessage_generic_fetch,
  /* msg_fetch_header */ mailmessage_generic_fetch_header,
  /* msg_fetch_body */ mailmessage_generic_fetch_header,
  /* msg_fetch_size */ NULL,
  /* msg_get_bodystructure */ mailmessage_generic_get_bodystructure,
  /* msg_fetch_section */ mailmessage_generic_fetch_section,
  /* msg_fetch_section_header */ mailmessage_generic_fetch_section_header,
  /* msg_fetch_section_mime */ mailmessage_generic_fetch_section_mime,
  /* msg_fetch_section_body */ mailmessage_generic_fetch_section_body,
  /* msg_fetch_envelope */ fetch_envelope,

  /* msg_get_flags */ get_flags,
#else
  .msg_name = "db",

  .msg_initialize = initialize,
  .msg_uninitialize = mailmessage_generic_uninitialize,

  .msg_flush = mailmessage_generic_flush,
  .msg_check = check,

  .msg_fetch_result_free = mailmessage_generic_fetch_result_free,

  .msg_fetch = mailmessage_generic_fetch,
  .msg_fetch_header = mailmessage_generic_fetch_header,
  .msg_fetch_body = mailmessage_generic_fetch_header,
  .msg_fetch_size = NULL,
  .msg_get_bodystructure = mailmessage_generic_get_bodystructure,
  .msg_fetch_section = mailmessage_generic_fetch_section,
  .msg_fetch_section_header = mailmessage_generic_fetch_section_header,
  .msg_fetch_section_mime = mailmessage_generic_fetch_section_mime,
  .msg_fetch_section_body = mailmessage_generic_fetch_section_body,
  .msg_fetch_envelope = fetch_envelope,

  .msg_get_flags = get_flags,
#endif
};

mailmessage_driver * db_message_driver = &local_db_message_driver;

struct db_msg_data {
  MMAPString * msg_content;
};

static inline struct db_session_state_data *
get_session_data(mailmessage * msg)
{
  return msg->msg_session->sess_data;
}

static int prefetch(mailmessage * msg_info)
{
  struct generic_message_t * msg;
  int res;
  struct db_msg_data * data;
  struct db_session_state_data * sess_data;
  MMAPString * msg_content;
  struct mail_cache_db * maildb;
  int r;
  char key[PATH_MAX];
  void * msg_data;
  size_t msg_data_len;
  
  sess_data = get_session_data(msg_info);
  
  r = mail_cache_db_open_lock(sess_data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  snprintf(key, sizeof(key), "%lu", (unsigned long) msg_info->msg_index);
  r = mail_cache_db_get(maildb, key, strlen(key), &msg_data, &msg_data_len);
  if (r < 0) {
    res = MAIL_ERROR_MSG_NOT_FOUND;
    goto close_db;
  }
  
  msg_content = mmap_string_new_len(msg_data, msg_data_len);
  if (msg_content == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  data = malloc(sizeof(* data));
  if (data == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }
  
  data->msg_content = msg_content;
  
  msg = msg_info->msg_data;
  
  msg->msg_data = data;
  msg->msg_message = msg_content->str;
  msg->msg_length = msg_content->len;
  
  mail_cache_db_close_unlock(sess_data->db_filename, maildb);
  
  return MAIL_NO_ERROR;
  
 free_mmapstr:
  mmap_string_free(msg_content);
 close_db:
  mail_cache_db_close_unlock(sess_data->db_filename, maildb);
 err:
  return res;
}

static void prefetch_free(struct generic_message_t * msg)
{
  if (msg->msg_message != NULL) {
    struct db_msg_data * data;
    
    data = msg->msg_data;
    mmap_string_free(data->msg_content);
    data->msg_content = NULL;
    free(data);
    msg->msg_message = NULL;
  }
}

static int initialize(mailmessage * msg_info)
{
  struct generic_message_t * msg;
  int r;
  char key[PATH_MAX];

  snprintf(key, sizeof(key), "%lu", (unsigned long) msg_info->msg_index);
  msg_info->msg_uid = strdup(key);
  if (msg_info->msg_uid == NULL)
    return MAIL_ERROR_MEMORY;
  
  r = mailmessage_generic_initialize(msg_info);
  if (r != MAIL_NO_ERROR)
    return r;
  
  msg = msg_info->msg_data;
  msg->msg_prefetch = prefetch;
  msg->msg_prefetch_free = prefetch_free;
  
  return MAIL_NO_ERROR;
}

static void check(mailmessage * msg_info)
{
  int r;

  if (msg_info->msg_flags != NULL) {
    r = mail_flags_store_set(get_session_data(msg_info)->db_flags_store,
        msg_info);
    /* ignore errors */
  }
}

static int fetch_envelope(mailmessage * msg_info,
    struct mailimf_fields ** result)
{
  char key[PATH_MAX];
  int r;
  struct db_session_state_data * sess_data;
  struct mail_cache_db * maildb;
  int res;
  struct mailimf_fields * fields;
  MMAPString * mmapstr;
  
  sess_data = get_session_data(msg_info);
  
  r = mail_cache_db_open_lock(sess_data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  snprintf(key, sizeof(key), "%lu-envelope",
      (unsigned long) msg_info->msg_index);
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  r = generic_cache_fields_read(maildb, mmapstr,
    key, &fields);
  
  mmap_string_free(mmapstr);
  
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_MSG_NOT_FOUND;
    goto close_db;
  }
  
  mail_cache_db_close_unlock(sess_data->db_filename, maildb);
  
  * result = fields;
  
  return MAIL_NO_ERROR;
  
 close_db:
  mail_cache_db_close_unlock(sess_data->db_filename, maildb);
 err:
  return res;
}

static int get_flags(mailmessage * msg_info,
    struct mail_flags ** result)
{
  char key[PATH_MAX];
  int r;
  struct db_session_state_data * sess_data;
  struct mail_cache_db * maildb;
  int res;
  MMAPString * mmapstr;
  
  sess_data = get_session_data(msg_info);
  
  r = mail_cache_db_open_lock(sess_data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  snprintf(key, sizeof(key), "%lu-flags", (unsigned long) msg_info->msg_index);
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  r = generic_cache_flags_read(maildb, mmapstr,
    key, &msg_info->msg_flags);
  
  mmap_string_free(mmapstr);
  
  if (r != MAIL_NO_ERROR) {
    msg_info->msg_flags = mail_flags_new_empty();
    if (msg_info->msg_flags == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto close_db;
    }
  }
  
  mail_cache_db_close_unlock(sess_data->db_filename, maildb);
  
  * result = msg_info->msg_flags;
  
  return MAIL_NO_ERROR;
  
 close_db:
  mail_cache_db_close_unlock(sess_data->db_filename, maildb);
 err:
  return res;
}

