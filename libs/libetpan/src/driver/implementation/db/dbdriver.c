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
 * $Id: dbdriver.c,v 1.11 2006/10/24 21:57:51 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "dbdriver.h"
#include "imfcache.h"
#include "generic_cache.h"
#include "libetpan-config.h"
#include "dbdriver_message.h"
#include "mail_cache_db.h"
#include <string.h>
#include <stdlib.h>
#include "mailmessage.h"

static int initialize(mailsession * session);

static void uninitialize(mailsession * session);

static int connect_path(mailsession * session, const char * path);

static int logout(mailsession * session);

static int expunge_folder(mailsession * session);

static int status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);

static int recent_number(mailsession * session, const char * mb,
    uint32_t * result);

static int unseen_number(mailsession * session, const char * mb,
    uint32_t * result);

static int messages_number(mailsession * session, const char * mb,
    uint32_t * result);

static int append_message(mailsession * session,
    const char * message, size_t size);

static int append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);

static int get_messages_list(mailsession * session,
    struct mailmessage_list ** result);

static int get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list);

static int check_folder(mailsession * session);

static int get_message(mailsession * session,
    uint32_t num, mailmessage ** result);

static int get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result);

static mailsession_driver local_db_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "db",

  /* sess_initialize */ initialize,
  /* sess_uninitialize */ uninitialize,

  /* sess_parameters */ NULL,

  /* sess_connect_stream */ NULL,
  /* sess_connect_path */ connect_path,
  /* sess_starttls */ NULL,
  /* sess_login */ NULL,
  /* sess_logout */ logout,
  /* sess_noop */ NULL,

  /* sess_build_folder_name */ NULL,
  /* sess_create_folder */ NULL,
  /* sess_delete_folder */ NULL,
  /* sess_rename_folder */ NULL,
  /* sess_check_folder */ check_folder,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ NULL,
  /* sess_expunge_folder */ expunge_folder,
  /* sess_status_folder */ status_folder,
  /* sess_messages_number */ messages_number,
  /* sess_recent_number */ recent_number,
  /* sess_unseen_number */ unseen_number,
  /* sess_list_folders */ NULL,
  /* sess_lsub_folders */ NULL,
  /* sess_subscribe_folder */ NULL,
  /* sess_unsubscribe_folder */ NULL,

  /* sess_append_message */ append_message,
  /* sess_append_message_flags */ append_message_flags,
  /* sess_copy_message */ NULL,
  /* sess_move_message */ NULL,

  /* sess_get_message */ get_message,
  /* sess_get_message_by_uid */ get_message_by_uid,

  /* sess_get_messages_list */ get_messages_list,
  /* sess_get_envelopes_list */ get_envelopes_list,
  /* sess_remove_message */ NULL,
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "db",

  .sess_initialize = initialize,
  .sess_uninitialize = uninitialize,

  .sess_parameters = NULL,

  .sess_connect_stream = NULL,
  .sess_connect_path = connect_path,
  .sess_starttls = NULL,
  .sess_login = NULL,
  .sess_logout = logout,
  .sess_noop = NULL,

  .sess_build_folder_name = NULL,
  .sess_create_folder = NULL,
  .sess_delete_folder = NULL,
  .sess_rename_folder = NULL,
  .sess_check_folder = check_folder,
  .sess_examine_folder = NULL,
  .sess_select_folder = NULL,
  .sess_expunge_folder = expunge_folder,
  .sess_status_folder = status_folder,
  .sess_messages_number = messages_number,
  .sess_recent_number = recent_number,
  .sess_unseen_number = unseen_number,
  .sess_list_folders = NULL,
  .sess_lsub_folders = NULL,
  .sess_subscribe_folder = NULL,
  .sess_unsubscribe_folder = NULL,

  .sess_append_message = append_message,
  .sess_append_message_flags = append_message_flags,
  .sess_copy_message = NULL,
  .sess_move_message = NULL,

  .sess_get_messages_list = get_messages_list,
  .sess_get_envelopes_list = get_envelopes_list,
  .sess_remove_message = NULL,

  .sess_get_message = get_message,
  .sess_get_message_by_uid = get_message_by_uid,
  .sess_login_sasl = NULL,
#endif
};

mailsession_driver * db_session_driver = &local_db_session_driver;

static inline struct db_session_state_data * get_data(mailsession * session)
{
  return session->sess_data;
}

static int flags_store_process(mailsession * session)
{
  unsigned int i;
  MMAPString * mmapstr;
  int r;
  int res;
  struct mail_cache_db * maildb;
  struct db_session_state_data * data;
  struct mail_flags_store * flags_store;
  
  data = get_data(session);
  
  flags_store = data->db_flags_store;
  
  if (carray_count(flags_store->fls_tab) == 0)
    return MAIL_NO_ERROR;
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mail_cache_db_open_lock(data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto free_mmapstr;
  }
  
  for(i = 0 ; i < carray_count(flags_store->fls_tab) ; i ++) {
    mailmessage * msg;
    char key[PATH_MAX];
    
    msg = carray_get(flags_store->fls_tab, i);
    
    snprintf(key, sizeof(key), "%lu-flags", (unsigned long) msg->msg_index);
    
    r = generic_cache_flags_write(maildb, mmapstr,
        key, msg->msg_flags);
  }
  
  mail_flags_store_clear(flags_store);
  
  mail_cache_db_close_unlock(data->db_filename, maildb);
  mmap_string_free(mmapstr);
  
  return MAIL_NO_ERROR;
  
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int db_get_next_validity(struct mail_cache_db * maildb,
    uint32_t * p_validity)
{
  int r;
  char key_value[PATH_MAX];
  uint32_t validity;
  void * serialized;
  size_t serialized_len;
  int res;
  MMAPString * mmapstr;
  size_t cur_token;
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(key_value, sizeof(key_value), "next-validity");
  
  r = mail_cache_db_get(maildb, key_value, strlen(key_value),
      &serialized, &serialized_len);
  
  if (r >= 0) {
    size_t cur_token;
    
    if (mmap_string_append_len(mmapstr, serialized, serialized_len) == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    
    cur_token = 0;
    r = mailimf_cache_int_read(mmapstr, &cur_token, &validity);
    if (r < 0)
      validity = 0;
  }
  else {
    validity = 0;
  }
  
  mmap_string_set_size(mmapstr, 0);
  cur_token = 0;
  r = mailimf_cache_int_write(mmapstr, &cur_token, validity + 1);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }
  
  r = mail_cache_db_put(maildb, key_value, strlen(key_value),
      mmapstr->str, mmapstr->len);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto free_mmapstr;
  }
  
  mmap_string_free(mmapstr);
  
  * p_validity = validity;
  
  return MAIL_NO_ERROR;
  
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int db_get_next_msg_number(struct mail_cache_db * maildb,
    uint32_t * p_num)
{
  int r;
  char key_value[PATH_MAX];
  uint32_t num;
  void * serialized;
  size_t serialized_len;
  int res;
  MMAPString * mmapstr;
  size_t cur_token;
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(key_value, sizeof(key_value), "next-msg");
  
  r = mail_cache_db_get(maildb, key_value, strlen(key_value),
      &serialized, &serialized_len);
  
  if (r >= 0) {
    size_t cur_token;
    
    if (mmap_string_append_len(mmapstr, serialized, serialized_len) == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    
    cur_token = 0;
    r = mailimf_cache_int_read(mmapstr, &cur_token, &num);
    if (r < 0)
      num = 1;
  }
  else {
    num = 1;
  }
  
  mmap_string_set_size(mmapstr, 0);
  cur_token = 0;
  r = mailimf_cache_int_write(mmapstr, &cur_token, num + 1);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }
  
  r = mail_cache_db_put(maildb, key_value, strlen(key_value),
      mmapstr->str, mmapstr->len);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto free_mmapstr;
  }
  
  mmap_string_free(mmapstr);
  
  * p_num = num;
  
  return MAIL_NO_ERROR;
  
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int db_set_message_list(struct mail_cache_db * maildb,
    carray * msglist)
{
  MMAPString * mmapstr;
  char key_value[PATH_MAX];
  int r;
  unsigned int i;
  size_t cur_token;
  int res;
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  cur_token = 0;
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    r = mailimf_cache_int_write(mmapstr, &cur_token, * msg);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_mmapstr;
    }
  }
  
  snprintf(key_value, sizeof(key_value), "message-list");
  r = mail_cache_db_put(maildb, key_value, strlen(key_value),
      mmapstr->str, mmapstr->len);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  mmap_string_free(mmapstr);
  
  return MAIL_NO_ERROR;
  
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int db_get_message_list(struct mail_cache_db * maildb,
    carray ** p_msglist)
{
  carray * msglist;
  void * serialized;
  size_t serialized_len;
  int r;
  char key_value[PATH_MAX];
  int res;
  unsigned int i;

  msglist = carray_new(16);
  if (msglist == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(key_value, sizeof(key_value), "message-list");
  r = mail_cache_db_get(maildb, key_value, strlen(key_value),
      &serialized, &serialized_len);
  if (r >= 0) {
    MMAPString * mmapstr;
    size_t cur_token;
    
    /* collect message list */
    
    mmapstr = mmap_string_new_len(serialized, serialized_len);
    if (mmapstr == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_msglist;
    }
    
    cur_token = 0;
    do {
      uint32_t num;
      uint32_t * msg;
      
      r = mailimf_cache_int_read(mmapstr, &cur_token, &num);
      if (r != MAIL_NO_ERROR)
        break;
      
      msg = malloc(sizeof(* msg));
      if (msg == NULL) {
        res = MAIL_ERROR_MEMORY;
        mmap_string_free(mmapstr);
        goto free_msglist;
      }
      * msg = num;
      
      r = carray_add(msglist, msg, NULL);
      if (r < 0) {
        res = MAIL_ERROR_MEMORY;
        free(msg);
        mmap_string_free(mmapstr);
        goto free_msglist;
      }
    } while (1);
    
    mmap_string_free(mmapstr);
  }
  
  * p_msglist = msglist;
  
  return MAIL_NO_ERROR;
  
 free_msglist:
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    free(msg);
  }
  carray_free(msglist);
 err:
  return res;
}

static int initialize(mailsession * session)
{
  struct db_session_state_data * data;
  
  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;
  
  data->db_filename[0] = '\0';
  
  data->db_flags_store = mail_flags_store_new();
  if (data->db_flags_store == NULL)
    goto free;
  
  session->sess_data = data;
  
  return MAIL_NO_ERROR;
  
 free:
  free(data);
 err:
  return MAIL_ERROR_MEMORY;
}

static void uninitialize(mailsession * session)
{
  struct db_session_state_data * data;
  
  data = get_data(session);

  flags_store_process(session);
  
  mail_flags_store_free(data->db_flags_store);
  
  free(data);
  
  session->sess_data = NULL;
}

static int connect_path(mailsession * session, const char * path)
{
  struct db_session_state_data * data;
  
  data = get_data(session);
  
  strncpy(data->db_filename, path, sizeof(data->db_filename));
  
  return MAIL_NO_ERROR;
}

static int logout(mailsession * session)
{
  return MAIL_NO_ERROR;
}

static int expunge_folder(mailsession * session)
{
  int r;
  char key_value[PATH_MAX];
  struct mail_cache_db * maildb;
  carray * msglist;
  unsigned int i;
  struct db_session_state_data * data;
  int res;
  chash * msg_table;
  MMAPString * mmapstr;
  
  data = get_data(session);
  
  flags_store_process(session);
  
  r = mail_cache_db_open_lock(data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  r = db_get_message_list(maildb, &msglist);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto close_db;
  }
  
  msg_table = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (msg_table == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_msglist;
  }
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_msgtable;
  }
  
  i = 0;
  while (i < carray_count(msglist)) {
    uint32_t num;
    uint32_t * msg;
    chashdatum key;
    chashdatum value;
    struct mail_flags * flags;
    int deleted;
    
    msg = carray_get(msglist, i);
    num = * msg;
    
    deleted = 0;
    snprintf(key_value, sizeof(key_value), "%lu-flags",
        (unsigned long) num);
    r = generic_cache_flags_read(maildb, mmapstr, key_value, &flags);
    if (r == MAIL_NO_ERROR) {
      if ((flags->fl_flags & MAIL_FLAG_DELETED) != 0)
        deleted = 1;
    }
    
    if (!deleted) {
      snprintf(key_value, sizeof(key_value), "%lu", (unsigned long) num);
      key.data = key_value;
      key.len = strlen(key_value);
      chash_set(msg_table, &key, &value, NULL);
      
      snprintf(key_value, sizeof(key_value), "%lu-envelope",
          (unsigned long) num);
      key.data = key_value;
      key.len = strlen(key_value);
      chash_set(msg_table, &key, &value, NULL);
      
      snprintf(key_value, sizeof(key_value), "%lu-flags",
          (unsigned long) num);
      key.data = key_value;
      key.len = strlen(key_value);
      chash_set(msg_table, &key, &value, NULL);
      
      i ++;
    }
    else {
      free(msg);
      carray_delete(msglist, i);
    }
  }
  
  mmap_string_free(mmapstr);
  
  r = mail_cache_db_clean_up(maildb, msg_table);
  
  chash_free(msg_table);
  
  r = db_set_message_list(maildb, msglist);
  
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    free(msg);
  }
  carray_free(msglist);
  
  mail_cache_db_close_unlock(data->db_filename, maildb);
  
  return MAIL_NO_ERROR;

 free_msgtable:
  chash_free(msg_table);
 free_msglist:
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    free(msg);
  }
 close_db:
  mail_cache_db_close_unlock(data->db_filename, maildb);
 err:
  return res;
}

static int status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  struct mail_cache_db * maildb;
  char key_value[PATH_MAX];
  MMAPString * mmapstr;
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  struct db_session_state_data * data;
  int r;
  int res;
  carray * msglist;
  unsigned int i;
  
  data = get_data(session);
  
  flags_store_process(session);
  
  r = mail_cache_db_open_lock(data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }

  r = db_get_message_list(maildb, &msglist);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto close_db;
  }

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }
  
  messages = 0;
  recent = 0;
  unseen = 0;
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t num;
    uint32_t * msg;
    int r;
    struct mail_flags * flags;
    
    msg = carray_get(msglist, i);
    num = * msg;
    free(msg);
    carray_set(msglist, i, NULL);
    
    messages ++;
    
    snprintf(key_value, sizeof(key_value), "%lu-flags", (unsigned long) num);
    
    r = generic_cache_flags_read(maildb, mmapstr, key_value, &flags);
    if (r == MAIL_NO_ERROR) {
      if ((flags->fl_flags & MAIL_FLAG_NEW) != 0) {
        recent ++;
      }
      if ((flags->fl_flags & MAIL_FLAG_SEEN) == 0) {
        unseen ++;
      }
      mail_flags_free(flags);
    }
  }

  mmap_string_free(mmapstr);
  
  carray_free(msglist);
  
  mail_cache_db_close_unlock(data->db_filename, maildb);

  * result_messages = messages;
  * result_unseen = unseen;
  * result_recent = recent;
  
  return MAIL_NO_ERROR;
  
 free_list:
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    if (msg != NULL)
      free(msg);
  }
  carray_free(msglist);
 close_db:
  mail_cache_db_close_unlock(data->db_filename, maildb);
 err:
  return res;
}

static int recent_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  uint32_t dummy_messages;
  uint32_t dummy_unseen;
  
  return status_folder(session, mb,
      &dummy_messages, result, &dummy_unseen);
}

static int unseen_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  uint32_t dummy_messages;
  uint32_t dummy_recent;
  
  return status_folder(session, mb,
      &dummy_messages, &dummy_recent, result);
}

static int messages_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  uint32_t dummy_unseen;
  uint32_t dummy_recent;
  
  return status_folder(session, mb,
      result, &dummy_recent, &dummy_unseen);
}

static int append_message(mailsession * session,
    const char * message, size_t size)
{
  return append_message_flags(session, message, size, NULL);
}

static int append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  carray * msglist;
  unsigned int i;
  uint32_t * msg;
  uint32_t num;
  char key_value[PATH_MAX];
  MMAPString * mmapstr;
  struct mail_cache_db * maildb;
  struct db_session_state_data * data;
  size_t cur_token;
  struct mailimf_fields * fields;
  int r;
  int res;
  
  data = get_data(session);
  
  r = mail_cache_db_open_lock(data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  num = 0;
  r = db_get_next_msg_number(maildb, &num);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = db_get_message_list(maildb, &msglist);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto close_db;
  }
  
  msg = malloc(sizeof(* msg));
  if (msg == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_msglist;
  }
  
  * msg = num;
  
  r = carray_add(msglist, msg, NULL);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    free(msg);
    goto free_msglist;
  }
  
  r = db_set_message_list(maildb, msglist);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_msglist;
  }
  
  /* free msglist */
  
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    free(msg);
  }
  carray_free(msglist);
  
  snprintf(key_value, sizeof(key_value), "%lu", (unsigned long) num);
  
  r = mail_cache_db_put(maildb, key_value, strlen(key_value),
      message, size);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto close_db;
  }
  
  /* write envelope */
  
  cur_token = 0;
  r = mailimf_envelope_fields_parse(message, size, &cur_token, &fields);
  if (r != MAILIMF_NO_ERROR) {
    res = MAIL_ERROR_PARSE;
    goto close_db;
  }
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  cur_token = 0;
  r = mailimf_cache_fields_write(mmapstr, &cur_token, fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    mmap_string_free(mmapstr);
    goto close_db;
  }
  
  snprintf(key_value, sizeof(key_value), "%lu-envelope", (unsigned long) num);
  
  r = mail_cache_db_put(maildb, key_value, strlen(key_value),
      mmapstr->str, mmapstr->len);
  
  mmap_string_free(mmapstr);
  
  mailimf_fields_free(fields);
  
  /* write flags */
  
  if (flags != NULL) {
    snprintf(key_value, sizeof(key_value), "%lu-flags", (unsigned long) num);
    
    mmapstr = mmap_string_new("");
    if (mmapstr == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto close_db;
    }
    
    r = generic_cache_flags_write(maildb, mmapstr,
        key_value, flags);
    
    mmap_string_free(mmapstr);
    
    if (r != MAIL_NO_ERROR) {
      res = MAIL_ERROR_FILE;
      goto close_db;
    }
  }
  
  mail_cache_db_close_unlock(data->db_filename, maildb);
  
  return MAIL_NO_ERROR;
  
 free_msglist:
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    free(msg);
  }
  carray_free(msglist);
 close_db:
  mail_cache_db_close_unlock(data->db_filename, maildb);
 err:
  return res;
}

static int get_messages_list(mailsession * session,
    struct mailmessage_list ** result)
{
  int r;
  char key[PATH_MAX];
  struct mail_cache_db * maildb;
  struct db_session_state_data * data;
  int res;
  carray * msglist;
  unsigned int i;
  carray * msgtab;
  struct mailmessage_list * driver_msglist;
  
  data = get_data(session);
  
  r = mail_cache_db_open_lock(data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  r = db_get_message_list(maildb, &msglist);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto close_db;
  }
  
  msgtab = carray_new(16);
  if (msgtab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t msg_num;
    uint32_t * pmsg_num;
    mailmessage * msg;
    size_t size;
    
    pmsg_num = carray_get(msglist, i);
    msg_num = * pmsg_num;
    free(pmsg_num);
    carray_set(msglist, i, NULL);
    
    snprintf(key, sizeof(key), "%lu", (unsigned long) msg_num);
    r = mail_cache_db_get_size(maildb, key, strlen(key), &size);
    if (r < 0) {
      continue;
    }
    
    msg = mailmessage_new();
    if (msg == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
    
    r = mailmessage_init(msg, session, db_message_driver,
        msg_num, size);
    if (r != MAIL_NO_ERROR) {
      mailmessage_free(msg);
      res = r;
      goto free_list;
    }
    
    r = carray_add(msgtab, msg, NULL);
    if (r < 0) {
      mailmessage_free(msg);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }
  carray_free(msglist);
  
  driver_msglist = mailmessage_list_new(msgtab);
  if (driver_msglist == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }
  
  mail_cache_db_close_unlock(data->db_filename, maildb);
  
  * result = driver_msglist;
  
  return MAIL_NO_ERROR;
  
 free_list:
  for(i = 0 ; i < carray_count(msgtab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(msgtab, i);
    mailmessage_free(msg);
  }
  carray_free(msgtab);
  
  for(i = 0 ; i < carray_count(msglist) ; i ++) {
    uint32_t * msg;
    
    msg = carray_get(msglist, i);
    if (msg != NULL)
      free(msg);
  }
  carray_free(msglist);
 close_db:
  mail_cache_db_close_unlock(data->db_filename, maildb);
 err:
  return res;
}

static int get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list)
{
  unsigned int i;
  char key[PATH_MAX];
  int r;
  struct mail_cache_db * maildb;
  int res;
  struct db_session_state_data * data;
  MMAPString * mmapstr;
  
  data = get_data(session);
  
  flags_store_process(session);
  
  r = mail_cache_db_open_lock(data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(env_list->msg_tab, i);
    if (msg->msg_fields == NULL) {
      snprintf(key, sizeof(key), "%lu-envelope",
          (unsigned long) msg->msg_index);
      
      r = generic_cache_fields_read(maildb, mmapstr,
        key, &msg->msg_fields);
    }
    
    if (msg->msg_flags == NULL) {
      snprintf(key, sizeof(key), "%lu-flags",
          (unsigned long) msg->msg_index);
      
      r = generic_cache_flags_read(maildb, mmapstr,
          key, &msg->msg_flags);
    }
  }
  
  mmap_string_free(mmapstr);
  
  mail_cache_db_close_unlock(data->db_filename, maildb);
  
  return MAIL_NO_ERROR;
  
 close_db:
  mail_cache_db_close_unlock(data->db_filename, maildb);
 err:
  return res;
}

static int check_folder(mailsession * session)
{
  flags_store_process(session);
  
  return MAIL_NO_ERROR;
}

static int get_message(mailsession * session,
    uint32_t num, mailmessage ** result)
{
  mailmessage * msg;
  int r;
  size_t size;
  char key[PATH_MAX];
  struct db_session_state_data * data;
  struct mail_cache_db * maildb;
  int res;
  
  data = get_data(session);
  
  r = mail_cache_db_open_lock(data->db_filename, &maildb);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  msg = mailmessage_new();
  if (msg == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  size = 0;
  snprintf(key, sizeof(key), "%lu", (unsigned long) num);
  r = mail_cache_db_get_size(maildb, key, strlen(key), &size);
  /* ignore error */
  
  r = mailmessage_init(msg, session, db_message_driver,
      num, size);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg);
    res = r;
    goto close_db;
  }
  
  mail_cache_db_close_unlock(data->db_filename, maildb);
  
  return MAIL_NO_ERROR;
  
 close_db:
  mail_cache_db_close_unlock(data->db_filename, maildb);
 err:
  return res;
}

static int get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result)
{
  uint32_t msg_num;
  
  msg_num = strtoul(uid, NULL, 10);
  
  return get_message(session, msg_num, result);
}
