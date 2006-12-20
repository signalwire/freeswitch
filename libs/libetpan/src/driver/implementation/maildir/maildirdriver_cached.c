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
 * $Id: maildirdriver_cached.c,v 1.17 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildirdriver.h"

#include <stdio.h>
#include <sys/types.h>
#ifndef _MSC_VER
#	include <dirent.h>
#	include <unistd.h>
#	include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "mail.h"
#include "maildir.h"
#include "maildriver_tools.h"
#include "maildirdriver_tools.h"
#include "maildirdriver_cached_message.h"
#include "mailmessage.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "mail_cache_db.h"
#include "libetpan-config.h"

static int initialize(mailsession * session);

static void uninitialize(mailsession * session);

static int parameters(mailsession * session,
    int id, void * value);

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

static mailsession_driver local_maildir_cached_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
   /* sess_name */ "maildir-cached",

  /* sess_initialize */ initialize,
  /* sess_uninitialize */ uninitialize,

  /* sess_parameters */ parameters,

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
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "maildir-cached",

  .sess_initialize = initialize,
  .sess_uninitialize = uninitialize,

  .sess_parameters = parameters,

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
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = get_message,
  .sess_get_message_by_uid = get_message_by_uid,
  .sess_login_sasl = NULL,
#endif
};

mailsession_driver * maildir_cached_session_driver =
&local_maildir_cached_session_driver;


static inline struct maildir_cached_session_state_data *
get_cached_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession * get_ancestor(mailsession * session)
{
  return get_cached_data(session)->md_ancestor;
}

static inline struct maildir_session_state_data *
get_ancestor_data(mailsession * session)
{
  return get_ancestor(session)->sess_data;
}


static struct maildir * get_maildir_session(mailsession * session)
{
  return get_ancestor_data(session)->md_session;
}

static int initialize(mailsession * session)
{
  struct maildir_cached_session_state_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;

  data->md_ancestor = mailsession_new(maildir_session_driver);
  if (data->md_ancestor == NULL)
    goto free;
  
  data->md_flags_store = mail_flags_store_new();
  if (data->md_flags_store == NULL)
    goto free_session;
  
  data->md_quoted_mb = NULL;
  data->md_cache_directory[0] = '\0';
  data->md_flags_directory[0] = '\0';

  session->sess_data = data;
  
  return MAIL_NO_ERROR;
  
 free_session:
  mailsession_free(data->md_ancestor);
 free:
  free(data);
 err:
  return MAIL_ERROR_MEMORY;
}

static void
free_quoted_mb(struct maildir_cached_session_state_data * maildir_cached_data)
{
  if (maildir_cached_data->md_quoted_mb != NULL) {
    free(maildir_cached_data->md_quoted_mb);
    maildir_cached_data->md_quoted_mb = NULL;
  }
}

static int
write_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * uid, struct mail_flags * flags);

#define ENV_NAME "env.db"
#define FLAGS_NAME "flags.db"

static int flags_store_process(char * flags_directory, char * quoted_mb,
    struct mail_flags_store * flags_store)
{
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  unsigned int i;
  int r;
  int res;

  if (carray_count(flags_store->fls_tab) == 0)
    return MAIL_NO_ERROR;

  if (quoted_mb == NULL)
    return MAIL_NO_ERROR;

  snprintf(filename_flags, PATH_MAX, "%s%c%s%c%s",
      flags_directory, MAIL_DIR_SEPARATOR, quoted_mb,
      MAIL_DIR_SEPARATOR, FLAGS_NAME);

  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db_flags;
  }

  for(i = 0 ; i < carray_count(flags_store->fls_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(flags_store->fls_tab, i);

    r = write_cached_flags(cache_db_flags, mmapstr,
        msg->msg_uid, msg->msg_flags);
    if (r != MAIL_NO_ERROR) {
      /* ignore errors */
    }
  }

  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);

  mail_flags_store_clear(flags_store);

  return MAIL_NO_ERROR;

 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}

static void uninitialize(mailsession * session)
{
  struct maildir_cached_session_state_data * data;
  
  data = get_cached_data(session);
  
  flags_store_process(data->md_flags_directory,
      data->md_quoted_mb,
      data->md_flags_store);
  
  mail_flags_store_free(data->md_flags_store);
  mailsession_free(data->md_ancestor);
  free_quoted_mb(data);
  free(data);
  
  session->sess_data = data;
}


static int parameters(mailsession * session,
    int id, void * value)
{
  struct maildir_cached_session_state_data * data;
  int r;

  data = get_cached_data(session);

  switch (id) {
  case MAILDIRDRIVER_CACHED_SET_CACHE_DIRECTORY:
    strncpy(data->md_cache_directory, value, PATH_MAX);
    data->md_cache_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(data->md_cache_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  case MAILDIRDRIVER_CACHED_SET_FLAGS_DIRECTORY:
    strncpy(data->md_flags_directory, value, PATH_MAX);
    data->md_flags_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(data->md_flags_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  default:
    return mailsession_parameters(data->md_ancestor, id, value);
  }
}


static int get_cache_folder(mailsession * session, char ** result)
{
  struct maildir * md;
  char * quoted_mb;
  int res;
  int r;
  char key[PATH_MAX];
  struct maildir_cached_session_state_data * data;
  
  md = get_maildir_session(session);
  data = get_cached_data(session);
  
  quoted_mb = maildriver_quote_mailbox(md->mdir_path);
  if (quoted_mb == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(key, PATH_MAX, "%s/%s", data->md_cache_directory, quoted_mb);
  r = generic_cache_create_dir(key);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_quoted_mb;
  }

  snprintf(key, PATH_MAX, "%s/%s", data->md_flags_directory, quoted_mb);
  r = generic_cache_create_dir(key);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_quoted_mb;
  }
  
  * result = quoted_mb;
  
  return MAIL_NO_ERROR;

 free_quoted_mb:
  free(quoted_mb);
 err:
  return res;
}


static int connect_path(mailsession * session, const char * path)
{
  int r;
  int res;
  char * quoted_mb;

  r = mailsession_connect_path(get_ancestor(session), path);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  quoted_mb = NULL;
  r = get_cache_folder(session, &quoted_mb);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto logout;
  }
  
  get_cached_data(session)->md_quoted_mb = quoted_mb;
  
  return MAILDIR_NO_ERROR;
  
 logout:
  mailsession_logout(get_ancestor(session));
 err:
  return res;
}

static int logout(mailsession * session)
{
  struct maildir_cached_session_state_data * data;
  int r;
  
  data = get_cached_data(session);

  flags_store_process(data->md_flags_directory,
      data->md_quoted_mb, data->md_flags_store);
  
  r = mailsession_logout(get_ancestor(session));
  if (r != MAIL_NO_ERROR)
    return r;
  
  free_quoted_mb(get_cached_data(session));
  
  return MAIL_NO_ERROR;
}

static int status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  return mailsession_status_folder(get_ancestor(session), mb,
      result_messages, result_recent, result_unseen);
}

static int messages_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  return mailsession_messages_number(get_ancestor(session), mb, result);
}

static int unseen_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  return mailsession_unseen_number(get_ancestor(session), mb, result);
}

static int recent_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  return mailsession_recent_number(get_ancestor(session), mb, result);
}


static int append_message(mailsession * session,
    const char * message, size_t size)
{
#if 0
  return mailsession_append_message(get_ancestor(session), message, size);
#endif
  return append_message_flags(session, message, size, NULL);
}

static int append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  struct maildir * md;
  int r;
  char uid[PATH_MAX];
  struct maildir_msg * md_msg;
  chashdatum key;
  chashdatum value;
  uint32_t md_flags;
  struct mail_cache_db * cache_db_flags;
  char filename_flags[PATH_MAX];
  MMAPString * mmapstr;
  struct maildir_cached_session_state_data * data;
  
  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = maildir_message_add_uid(md, message, size,
      uid, sizeof(uid));
  if (r != MAILDIR_NO_ERROR)
    return maildirdriver_maildir_error_to_mail_error(r);
  
  if (flags == NULL)
    goto exit;
  
  data = get_cached_data(session);
  
  snprintf(filename_flags, PATH_MAX, "%s%c%s%c%s",
      data->md_flags_directory, MAIL_DIR_SEPARATOR, data->md_quoted_mb,
      MAIL_DIR_SEPARATOR, FLAGS_NAME);
  
  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0)
    goto exit;
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL)
    goto close_db_flags;
  
  r = write_cached_flags(cache_db_flags, mmapstr,
      uid, flags);
  
  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  
  if (r != MAIL_NO_ERROR)
    goto exit;
  
  key.data = uid;
  key.len = strlen(uid);
  r = chash_get(md->mdir_msg_hash, &key, &value);
  if (r < 0)
    goto exit;
  
  md_msg = value.data;
  
  md_flags = maildirdriver_flags_to_maildir_flags(flags->fl_flags);
  
  r = maildir_message_change_flags(md, uid, md_flags);
  if (r != MAILDIR_NO_ERROR)
    goto exit;
  
  return MAIL_NO_ERROR;
  
 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 exit:
  return MAIL_NO_ERROR;
}

#define UID_NAME "uid.db"

static int uid_clean_up(struct mail_cache_db * uid_db,
    struct mailmessage_list * env_list)
{
  chash * hash_exist;
  int res;
  int r;
  unsigned int i;
  chashdatum key;
  chashdatum value;
  char key_str[PATH_MAX];
  
  /* flush cache */
  
  hash_exist = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  if (hash_exist == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  value.data = NULL;
  value.len = 0;
  
  key.data = "max-uid";
  key.len = strlen("max-uid");
  r = chash_set(hash_exist, &key, &value, NULL);
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);
    
    value.data = NULL;
    value.len = 0;
    
    key.data = msg->msg_uid;
    key.len = strlen(msg->msg_uid);
    r = chash_set(hash_exist, &key, &value, NULL);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto free;
    }
    
    snprintf(key_str, sizeof(key_str), "uid-%lu",
        (unsigned long) msg->msg_index);
    key.data = key_str;
    key.len = strlen(key_str);
    r = chash_set(hash_exist, &key, &value, NULL);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto free;
    }
  }
  
  mail_cache_db_clean_up(uid_db, hash_exist);
  
  chash_free(hash_exist);
  
  return MAIL_NO_ERROR;

 free:
  chash_free(hash_exist);
 err:
  return res;
}

static int get_messages_list(mailsession * session,
    struct mailmessage_list ** result)
{
  struct maildir * md;
  int r;
  struct mailmessage_list * env_list;
  int res;
  uint32_t max_uid;
  char filename[PATH_MAX];
  struct mail_cache_db * uid_db;
  void * value;
  size_t value_len;
  unsigned long i;
  struct maildir_cached_session_state_data * data;
  char key[PATH_MAX];
  
  data = get_cached_data(session);
  
  md = get_maildir_session(session);
  if (md == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }
  
  check_folder(session);
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = maildirdriver_maildir_error_to_mail_error(r);
    goto err;
  }
  
  r = maildir_get_messages_list(session, md,
      maildir_cached_message_driver, &env_list);
  if (r != MAILDIR_NO_ERROR) {
    res = r;
    goto err;
  }
  
  /* read/write DB */
  
  snprintf(filename, sizeof(filename), "%s%c%s%c%s",
      data->md_flags_directory, MAIL_DIR_SEPARATOR, data->md_quoted_mb,
      MAIL_DIR_SEPARATOR, UID_NAME);
  
  r = mail_cache_db_open_lock(filename, &uid_db);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }
  
  max_uid = 0;
  r = mail_cache_db_get(uid_db, "max-uid", sizeof("max-uid") - 1,
      &value, &value_len);
  if (r == 0) {
    memcpy(&max_uid, value, sizeof(max_uid));
  }
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    uint32_t index;
    
    msg = carray_get(env_list->msg_tab, i);
    
    r = mail_cache_db_get(uid_db, msg->msg_uid,
        strlen(msg->msg_uid), &value, &value_len);
    if (r < 0) {
      max_uid ++;
      msg->msg_index = max_uid;
      mail_cache_db_put(uid_db, msg->msg_uid,
          strlen(msg->msg_uid), &msg->msg_index, sizeof(msg->msg_index));
      
      snprintf(key, sizeof(key), "uid-%lu", (unsigned long) msg->msg_index);
      mail_cache_db_put(uid_db, key, strlen(key),
          msg->msg_uid, strlen(msg->msg_uid));
    }
    else {
      memcpy(&index, value, sizeof(index));
      msg->msg_index = index;
    }
  }
  
  mail_cache_db_put(uid_db, "max-uid", sizeof("max-uid") - 1,
      &max_uid, sizeof(max_uid));
  
  uid_clean_up(uid_db, env_list);
  
  mail_cache_db_close_unlock(filename, uid_db);
  
  * result = env_list;
  
  return MAIL_NO_ERROR;

 free_list:
  mailmessage_list_free(env_list);
 err:
  return res;
}

static int
get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session,
    char * uid,
    struct mail_flags ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mail_flags * flags;
  int res;

  snprintf(keyname, PATH_MAX, "%s-flags", uid);
  
  r = generic_cache_flags_read(cache_db, mmapstr, keyname, &flags);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  * result = flags;
  
  return MAIL_NO_ERROR;

 err:
  return res;
}

static int
get_cached_envelope(struct mail_cache_db * cache_db, MMAPString * mmapstr,
    mailsession * session, char * uid,
    struct mailimf_fields ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mailimf_fields * fields;
  int res;

  snprintf(keyname, PATH_MAX, "%s-envelope", uid);
  
  r = generic_cache_fields_read(cache_db, mmapstr, keyname, &fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  * result = fields;

  return MAIL_NO_ERROR;

 err:
  return res;
}

static int
write_cached_envelope(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session, char * uid,
    struct mailimf_fields * fields)
{
  int r;
  char keyname[PATH_MAX];
  int res;

  snprintf(keyname, PATH_MAX, "%s-envelope", uid);

  r = generic_cache_fields_write(cache_db, mmapstr, keyname, fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}

static int
write_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * uid, struct mail_flags * flags)
{
  int r;
  char keyname[PATH_MAX];
  int res;

  snprintf(keyname, PATH_MAX, "%s-flags", uid);

  r = generic_cache_flags_write(cache_db, mmapstr, keyname, flags);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}


static int get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list)
{
  int r;
  unsigned int i;
  int res;
  struct maildir_cached_session_state_data * data;
  char filename_env[PATH_MAX];
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_env;
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  
  data = get_cached_data(session);
  
  flags_store_process(data->md_flags_directory,
      data->md_quoted_mb, data->md_flags_store);
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(filename_env, PATH_MAX, "%s%c%s%c%s",
      data->md_cache_directory, MAIL_DIR_SEPARATOR, data->md_quoted_mb,
      MAIL_DIR_SEPARATOR, ENV_NAME);
  
  r = mail_cache_db_open_lock(filename_env, &cache_db_env);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }
  
  snprintf(filename_flags, PATH_MAX, "%s%c%s%c%s",
      data->md_flags_directory, MAIL_DIR_SEPARATOR, data->md_quoted_mb,
      MAIL_DIR_SEPARATOR, FLAGS_NAME);
  
  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto close_db_env;
  }
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i++) {
    mailmessage * msg;
    struct mailimf_fields * fields;
    struct mail_flags * flags;
    
    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields == NULL) {
      r = get_cached_envelope(cache_db_env, mmapstr, session,
          msg->msg_uid, &fields);
      if (r == MAIL_NO_ERROR) {
        msg->msg_cached = TRUE;
        msg->msg_fields = fields;
      }
    }

    if (msg->msg_flags == NULL) {
      r = get_cached_flags(cache_db_flags, mmapstr,
          session, msg->msg_uid, &flags);
      if (r == MAIL_NO_ERROR) {
        msg->msg_flags = flags;
      }
    }
  }
  
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  mail_cache_db_close_unlock(filename_env, cache_db_env);
  
  r = mailsession_get_envelopes_list(get_ancestor(session), env_list);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }
  
  r = mail_cache_db_open_lock(filename_env, &cache_db_env);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }

  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto close_db_env;
  }

  /* must write cache */

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields != NULL) {
      if (!msg->msg_cached) {
        /* msg->index is the numerical UID of the message */
	r = write_cached_envelope(cache_db_env, mmapstr,
            session, msg->msg_uid, msg->msg_fields);
      }
    }

    if (msg->msg_flags != NULL) {
      r = write_cached_flags(cache_db_flags, mmapstr,
          msg->msg_uid, msg->msg_flags);
    }
  }
  
  /* flush cache */
  
  maildriver_cache_clean_up(cache_db_env, cache_db_flags, env_list);
  
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  mail_cache_db_close_unlock(filename_env, cache_db_env);
  
  mmap_string_free(mmapstr);
  
  return MAIL_NO_ERROR;
  
 close_db_env:
  mail_cache_db_close_unlock(filename_env, cache_db_env);
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int expunge_folder(mailsession * session)
{
  return mailsession_expunge_folder(get_ancestor(session));
}

static int check_folder(mailsession * session)
{
  struct maildir_cached_session_state_data * data;
  
  data = get_cached_data(session);

  flags_store_process(data->md_flags_directory,
      data->md_quoted_mb, data->md_flags_store);
  
  return mailsession_check_folder(get_ancestor(session));
}

static int get_message(mailsession * session,
    uint32_t num, mailmessage ** result)
{
  struct maildir * md;
  int res;
  mailmessage * msg;
  char filename[PATH_MAX];
  struct mail_cache_db * uid_db;
  char * msg_filename;
  struct stat stat_info;
  char key_str[PATH_MAX];
  void * value;
  size_t value_len;
  char uid[PATH_MAX];
  struct maildir_cached_session_state_data * data;
  int r;

  data = get_cached_data(session);
  
  md = get_maildir_session(session);
  
  /* a get_messages_list() should have been done once before */
  
  /* read DB */
  
  snprintf(filename, sizeof(filename), "%s%c%s%c%s",
      data->md_flags_directory, MAIL_DIR_SEPARATOR, data->md_quoted_mb,
      MAIL_DIR_SEPARATOR, UID_NAME);
  
  r = mail_cache_db_open_lock(filename, &uid_db);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(key_str, sizeof(key_str), "uid-%lu", (unsigned long) num);
  
  r = mail_cache_db_get(uid_db, key_str, strlen(key_str), &value, &value_len);
  if (r < 0) {
    res = MAIL_ERROR_INVAL;
    goto close_db;
  }
  
  if (value_len >= PATH_MAX) {
    res = MAIL_ERROR_INVAL;
    goto close_db;
  }
  
  memcpy(uid, value, value_len);
  uid[value_len] = '\0';
  
  mail_cache_db_close_unlock(filename, uid_db);
  
  /* update maildir data */
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = maildirdriver_maildir_error_to_mail_error(r);
    goto err;
  }
  
  msg_filename = maildir_message_get(md, uid);
  if (msg_filename == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  r = stat(msg_filename, &stat_info);
  free(msg_filename);
  if (r < 0) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  /* create message */
  
  msg = mailmessage_new();
  if (msg == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mailmessage_init(msg, session, maildir_cached_message_driver,
      num, stat_info.st_size);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg);
    res = r;
    goto err;
  }
  
  msg->msg_uid = strdup(uid);
  if (msg->msg_uid == NULL) {
    mailmessage_free(msg);
    res = r;
    goto err;
  }
  
  * result = msg;
  
  return MAIL_NO_ERROR;
  
 close_db:
  mail_cache_db_close_unlock(filename, uid_db);
 err:
  return res;
}


static int get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result)
{
  int r;
  struct maildir * md;
  int res;
  mailmessage * msg;
  char filename[PATH_MAX];
  struct mail_cache_db * uid_db;
  char * msg_filename;
  struct stat stat_info;
  void * value;
  size_t value_len;
  struct maildir_cached_session_state_data * data;
  uint32_t index;
  
  data = get_cached_data(session);
  
  md = get_maildir_session(session);
  
  /* a get_messages_list() should have been done once before */
  
  /* read DB */
  
  snprintf(filename, sizeof(filename), "%s%c%s%c%s",
      data->md_flags_directory, MAIL_DIR_SEPARATOR, data->md_quoted_mb,
      MAIL_DIR_SEPARATOR, UID_NAME);
  
  r = mail_cache_db_open_lock(filename, &uid_db);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mail_cache_db_get(uid_db, uid, strlen(uid), &value, &value_len);
  if (r < 0) {
    res = MAIL_ERROR_INVAL;
    goto close_db;
  }
  
  memcpy(&index, value, sizeof(index));
  
  mail_cache_db_close_unlock(filename, uid_db);

  /* update maildir data */
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = maildirdriver_maildir_error_to_mail_error(r);
    goto err;
  }
  
  msg_filename = maildir_message_get(md, uid);
  if (msg_filename == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  r = stat(msg_filename, &stat_info);
  free(msg_filename);
  if (r < 0) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  /* create message */
  
  msg = mailmessage_new();
  if (msg == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mailmessage_init(msg, session, maildir_cached_message_driver,
      index, stat_info.st_size);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg);
    res = r;
    goto err;
  }
  
  msg->msg_uid = strdup(uid);
  if (msg->msg_uid == NULL) {
    mailmessage_free(msg);
    res = r;
    goto err;
  }
  
  * result = msg;
  
  return MAIL_NO_ERROR;
  
 close_db:
  mail_cache_db_close_unlock(filename, uid_db);
 err:
  return res;
}
