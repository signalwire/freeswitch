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
 * $Id: pop3driver_cached.c,v 1.47 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "pop3driver_cached.h"

#include "libetpan-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#include "mail.h"
#include "mail_cache_db.h"

#include "maildriver.h"
#include "mailmessage.h"
#include "pop3driver.h"
#include "mailpop3.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "pop3driver_cached_message.h"
#include "pop3driver_tools.h"
#include "maildriver_tools.h"

static int pop3driver_cached_initialize(mailsession * session);

static void pop3driver_cached_uninitialize(mailsession * session);

static int pop3driver_cached_parameters(mailsession * session,
    int id, void * value);

static int pop3driver_cached_connect_stream(mailsession * session,
    mailstream * s);

static int pop3driver_cached_starttls(mailsession * session);

static int pop3driver_cached_login(mailsession * session,
    const char * userid, const char * password);

static int pop3driver_cached_logout(mailsession * session);

static int pop3driver_cached_check_folder(mailsession * session);

static int pop3driver_cached_noop(mailsession * session);

static int pop3driver_cached_expunge_folder(mailsession * session);

static int pop3driver_cached_status_folder(mailsession * session,
    const char * mb, uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);

static int pop3driver_cached_messages_number(mailsession * session,
    const char * mb,
    uint32_t * result);

static int pop3driver_cached_recent_number(mailsession * session,
    const char * mb,
    uint32_t * result);

static int pop3driver_cached_unseen_number(mailsession * session,
    const char * mb,
    uint32_t * result);

static int pop3driver_cached_remove_message(mailsession * session,
    uint32_t num);

static int
pop3driver_cached_get_messages_list(mailsession * session,
    struct mailmessage_list ** result);

static int
pop3driver_cached_get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list);

static int pop3driver_cached_get_message(mailsession * session,
    uint32_t num, mailmessage ** result);

static int pop3driver_cached_get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result);

static int pop3driver_cached_login_sasl(mailsession * session,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);

static mailsession_driver local_pop3_cached_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "pop3-cached",

  /* sess_initialize */ pop3driver_cached_initialize,
  /* sess_uninitialize */ pop3driver_cached_uninitialize,

  /* sess_parameters */ pop3driver_cached_parameters,

  /* sess_connect_stream */ pop3driver_cached_connect_stream,
  /* sess_connect_path */ NULL,
  /* sess_starttls */ pop3driver_cached_starttls,
  /* sess_login */ pop3driver_cached_login,
  /* sess_logout */ pop3driver_cached_logout,
  /* sess_noop */ pop3driver_cached_noop,

  /* sess_build_folder_name */ NULL,
  /* sess_create_folder */ NULL,
  /* sess_delete_folder */ NULL,
  /* sess_rename_folder */ NULL,
  /* sess_check_folder */ pop3driver_cached_check_folder,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ NULL,
  /* sess_expunge_folder */ pop3driver_cached_expunge_folder,
  /* sess_status_folder */ pop3driver_cached_status_folder,
  /* sess_messages_number */ pop3driver_cached_messages_number,
  /* sess_recent_number */ pop3driver_cached_recent_number,
  /* sess_unseen_number */ pop3driver_cached_unseen_number,
  /* sess_list_folders */ NULL,
  /* sess_lsub_folders */ NULL,
  /* sess_subscribe_folder */ NULL,
  /* sess_unsubscribe_folder */ NULL,

  /* sess_append_message */ NULL,
  /* sess_append_message_flags */ NULL,
  /* sess_copy_message */ NULL,
  /* sess_move_message */ NULL,

  /* sess_get_message */ pop3driver_cached_get_message,
  /* sess_get_message_by_uid */ pop3driver_cached_get_message_by_uid,

  /* sess_get_messages_list */ pop3driver_cached_get_messages_list,
  /* sess_get_envelopes_list */ pop3driver_cached_get_envelopes_list,
  /* sess_remove_message */ pop3driver_cached_remove_message,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ pop3driver_cached_login_sasl,
  
#else
  .sess_name = "pop3-cached",

  .sess_initialize = pop3driver_cached_initialize,
  .sess_uninitialize = pop3driver_cached_uninitialize,

  .sess_parameters = pop3driver_cached_parameters,

  .sess_connect_stream = pop3driver_cached_connect_stream,
  .sess_connect_path = NULL,
  .sess_starttls = pop3driver_cached_starttls,
  .sess_login = pop3driver_cached_login,
  .sess_logout = pop3driver_cached_logout,
  .sess_noop = pop3driver_cached_noop,

  .sess_build_folder_name = NULL,
  .sess_create_folder = NULL,
  .sess_delete_folder = NULL,
  .sess_rename_folder = NULL,
  .sess_check_folder = pop3driver_cached_check_folder,
  .sess_examine_folder = NULL,
  .sess_select_folder = NULL,
  .sess_expunge_folder = pop3driver_cached_expunge_folder,
  .sess_status_folder = pop3driver_cached_status_folder,
  .sess_messages_number = pop3driver_cached_messages_number,
  .sess_recent_number = pop3driver_cached_recent_number,
  .sess_unseen_number = pop3driver_cached_unseen_number,
  .sess_list_folders = NULL,
  .sess_lsub_folders = NULL,
  .sess_subscribe_folder = NULL,
  .sess_unsubscribe_folder = NULL,

  .sess_append_message = NULL,
  .sess_append_message_flags = NULL,
  .sess_copy_message = NULL,
  .sess_move_message = NULL,

  .sess_get_messages_list = pop3driver_cached_get_messages_list,
  .sess_get_envelopes_list = pop3driver_cached_get_envelopes_list,
  .sess_remove_message = pop3driver_cached_remove_message,
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = pop3driver_cached_get_message,
  .sess_get_message_by_uid = pop3driver_cached_get_message_by_uid,
  .sess_login_sasl = pop3driver_cached_login_sasl,
#endif
};

mailsession_driver * pop3_cached_session_driver =
&local_pop3_cached_session_driver;

#define ENV_NAME "env.db"
#define FLAGS_NAME "flags.db"


static inline struct pop3_cached_session_state_data *
get_cached_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession * get_ancestor(mailsession * session)
{
  return get_cached_data(session)->pop3_ancestor;
}

static inline struct pop3_session_state_data *
get_ancestor_data(mailsession * session)
{
  return get_ancestor(session)->sess_data;
}

static inline mailpop3 * get_pop3_session(mailsession * session)
{
  return get_ancestor_data(session)->pop3_session;
}

static int pop3driver_cached_initialize(mailsession * session)
{
  struct pop3_cached_session_state_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;

  data->pop3_flags_store = mail_flags_store_new();
  if (data->pop3_flags_store == NULL)
    goto free_data;

  data->pop3_ancestor = mailsession_new(pop3_session_driver);
  if (data->pop3_ancestor == NULL)
    goto free_store;

  data->pop3_flags_hash = chash_new(128, CHASH_COPYNONE);
  if (data->pop3_flags_hash == NULL)
    goto free_session;

  session->sess_data = data;

  return MAIL_NO_ERROR;

 free_session:
  mailsession_free(data->pop3_ancestor);
 free_store:
  mail_flags_store_free(data->pop3_flags_store);
 free_data:
  free(data);
 err:
  return MAIL_ERROR_MEMORY;
}

static int pop3_flags_store_process(char * flags_directory,
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

  snprintf(filename_flags, PATH_MAX, "%s/%s",
      flags_directory, FLAGS_NAME);

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

    r = pop3driver_write_cached_flags(cache_db_flags, mmapstr,
        msg->msg_uid, msg->msg_flags);
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

static void pop3driver_cached_uninitialize(mailsession * session)
{
  struct pop3_cached_session_state_data * data;

  data = get_cached_data(session);

  pop3_flags_store_process(data->pop3_flags_directory,
      data->pop3_flags_store);

  mail_flags_store_free(data->pop3_flags_store); 

  chash_free(data->pop3_flags_hash);
  mailsession_free(data->pop3_ancestor);
  free(data);
  
  session->sess_data = data;
}

static int pop3driver_cached_check_folder(mailsession * session)
{
  struct pop3_cached_session_state_data * pop3_data;

  pop3_data = get_cached_data(session);

  pop3_flags_store_process(pop3_data->pop3_flags_directory,
      pop3_data->pop3_flags_store);

  return MAIL_NO_ERROR;
}

static int pop3driver_cached_parameters(mailsession * session,
    int id, void * value)
{
  struct pop3_cached_session_state_data * data;
  int r;

  data = get_cached_data(session);

  switch (id) {
  case POP3DRIVER_CACHED_SET_CACHE_DIRECTORY:
    strncpy(data->pop3_cache_directory, value, PATH_MAX);
    data->pop3_cache_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(data->pop3_cache_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  case POP3DRIVER_CACHED_SET_FLAGS_DIRECTORY:
    strncpy(data->pop3_flags_directory, value, PATH_MAX);
    data->pop3_flags_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(data->pop3_flags_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  default:
    return mailsession_parameters(data->pop3_ancestor, id, value);
  }
}

static int pop3driver_cached_connect_stream(mailsession * session,
    mailstream * s)
{
  int r;

  r = mailsession_connect_stream(get_ancestor(session), s);
  if (r != MAIL_NO_ERROR)
    return r;

  return MAIL_NO_ERROR;
}

static int pop3driver_cached_starttls(mailsession * session)
{
  return mailsession_starttls(get_ancestor(session));
}


static int pop3driver_cached_login(mailsession * session,
    const char * userid, const char * password)
{
  return mailsession_login(get_ancestor(session), userid, password);
}

static int pop3driver_cached_logout(mailsession * session)
{
  struct pop3_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);

  pop3_flags_store_process(cached_data->pop3_flags_directory,
      cached_data->pop3_flags_store);

  return mailsession_logout(get_ancestor(session));
}

static int pop3driver_cached_noop(mailsession * session)
{
  return mailsession_noop(get_ancestor(session));
}

static int pop3driver_cached_expunge_folder(mailsession * session)
{
  int res;
  struct pop3_cached_session_state_data * cached_data;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  unsigned int i;
  int r;
  carray * msg_tab;
  mailpop3 * pop3;

  pop3 = get_pop3_session(session);

  cached_data = get_cached_data(session);

  pop3_flags_store_process(cached_data->pop3_flags_directory,
      cached_data->pop3_flags_store);

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

  mailpop3_list(pop3, &msg_tab);

  for(i = 0 ; i < carray_count(msg_tab) ; i++) {
    struct mailpop3_msg_info * pop3_info;
    struct mail_flags * flags;
    
    pop3_info = carray_get(msg_tab, i);
    if (pop3_info == NULL)
      continue;

    if (pop3_info->msg_deleted)
      continue;

    r = pop3driver_get_cached_flags(cache_db_flags, mmapstr,
        session, pop3_info->msg_index, &flags);
    if (r != MAIL_NO_ERROR)
      continue;

    if (flags->fl_flags & MAIL_FLAG_DELETED) {
      r = mailpop3_dele(pop3, pop3_info->msg_index);
    }

    mail_flags_free(flags);
  }
  
  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);

  return MAIL_NO_ERROR;

 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}

static int pop3driver_cached_status_folder(mailsession * session,
    const char * mb, uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  int res;
  struct pop3_cached_session_state_data * cached_data;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  unsigned int i;
  int r;
  carray * msg_tab;
  mailpop3 * pop3;
  uint32_t recent;
  uint32_t unseen;
  
  recent = 0;
  unseen = 0;
  
  pop3 = get_pop3_session(session);

  cached_data = get_cached_data(session);

  pop3_flags_store_process(cached_data->pop3_flags_directory,
      cached_data->pop3_flags_store);

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

  mailpop3_list(pop3, &msg_tab);

  for(i = 0 ; i < carray_count(msg_tab) ; i++) {
    struct mailpop3_msg_info * pop3_info;
    struct mail_flags * flags;
    
    pop3_info = carray_get(msg_tab, i);
    if (pop3_info == NULL)
      continue;

    if (pop3_info->msg_deleted)
      continue;

    r = pop3driver_get_cached_flags(cache_db_flags, mmapstr,
        session, pop3_info->msg_index, &flags);
    if (r != MAIL_NO_ERROR) {
      recent ++;
      unseen ++;
      continue;
    }

    if ((flags->fl_flags & MAIL_FLAG_NEW) != 0) {
      recent ++;
    }
    if ((flags->fl_flags & MAIL_FLAG_SEEN) == 0) {
      unseen ++;
    }
    mail_flags_free(flags);

  }
  
  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);

  * result_messages = carray_count(msg_tab) - pop3->pop3_deleted_count;
  * result_recent = recent;
  * result_unseen = unseen;
  
  return MAIL_NO_ERROR;

 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}

static int pop3driver_cached_messages_number(mailsession * session,
    const char * mb,
    uint32_t * result)
{
  return mailsession_messages_number(get_ancestor(session), mb, result);
}

static int pop3driver_cached_recent_number(mailsession * session,
    const char * mb,
    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = pop3driver_cached_status_folder(session, mb,
      &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = recent;
  
  return MAIL_NO_ERROR;  
}

static int pop3driver_cached_unseen_number(mailsession * session,
    const char * mb,
    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = pop3driver_cached_status_folder(session, mb,
      &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = unseen;
  
  return MAIL_NO_ERROR;  
}

/* messages operations */

static int pop3driver_cached_remove_message(mailsession * session,
    uint32_t num)
{
  return mailsession_remove_message(get_ancestor(session), num);
}

static int
pop3driver_cached_get_messages_list(mailsession * session,
    struct mailmessage_list ** result)
{
  mailpop3 * pop3;

  pop3 = get_pop3_session(session);

  return pop3_get_messages_list(pop3, session,
      pop3_cached_message_driver, result);
}


static int
get_cached_envelope(struct mail_cache_db * cache_db, MMAPString * mmapstr,
    mailsession * session, uint32_t num,
    struct mailimf_fields ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mailpop3_msg_info * info;
  struct mailimf_fields * fields;
  int res;
  mailpop3 * pop3;

  pop3 = get_pop3_session(session);

  r = mailpop3_get_msg_info(pop3, num, &info);
  switch (r) {
  case MAILPOP3_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;
  case MAILPOP3_ERROR_NO_SUCH_MESSAGE:
    return MAIL_ERROR_MSG_NOT_FOUND;
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return MAIL_ERROR_FETCH;
  }

  snprintf(keyname, PATH_MAX, "%s-envelope", info->msg_uidl);

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
    mailsession * session, uint32_t num,
    struct mailimf_fields * fields)
{
  int r;
  char keyname[PATH_MAX];
  int res;
  struct mailpop3_msg_info * info;
  mailpop3 * pop3;

  pop3 = get_pop3_session(session);

  r = mailpop3_get_msg_info(pop3, num, &info);
  switch (r) {
  case MAILPOP3_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;
  case MAILPOP3_ERROR_NO_SUCH_MESSAGE:
    return MAIL_ERROR_MSG_NOT_FOUND;
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return MAIL_ERROR_FETCH;
  }

  snprintf(keyname, PATH_MAX, "%s-envelope", info->msg_uidl);

  r = generic_cache_fields_write(cache_db, mmapstr, keyname, fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}

static void get_uid_from_filename(char * filename)
{
  char * p;
  
  p = strstr(filename, "-header");
  if (p != NULL)
    * p = 0;
}  

static int
pop3driver_cached_get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list)
{
  int r;
  unsigned int i;
  struct pop3_cached_session_state_data * cached_data;
  char filename_env[PATH_MAX];
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_env;
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  int res;

  cached_data = get_cached_data(session);

  pop3_flags_store_process(cached_data->pop3_flags_directory,
      cached_data->pop3_flags_store);

  snprintf(filename_env, PATH_MAX, "%s/%s",
      cached_data->pop3_cache_directory, ENV_NAME);

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mail_cache_db_open_lock(filename_env, &cache_db_env);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }

  snprintf(filename_flags, PATH_MAX, "%s/%s",
      cached_data->pop3_flags_directory, FLAGS_NAME);

  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto close_db_env;
  }

  /* fill with cached */

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {   
    mailmessage * msg;
    struct mailimf_fields * fields;
    struct mail_flags * flags;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields == NULL) {
      r = get_cached_envelope(cache_db_env, mmapstr,
          session, msg->msg_index, &fields);
      if (r == MAIL_NO_ERROR) {
	msg->msg_cached = TRUE;
	msg->msg_fields = fields;
      }
    }

    if (msg->msg_flags == NULL) {
      r = pop3driver_get_cached_flags(cache_db_flags, mmapstr,
          session, msg->msg_index, &flags);
      if (r == MAIL_NO_ERROR) {
	msg->msg_flags = flags;
      }
    }
  }
  
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  mail_cache_db_close_unlock(filename_env, cache_db_env);

  r = maildriver_generic_get_envelopes_list(session, env_list);

  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }

  /* add flags */
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_flags == NULL)
      msg->msg_flags = mail_flags_new_empty();
  }
  
  r = mail_cache_db_open_lock(filename_env, &cache_db_env);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }
  
  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto close_db_env;
  }

  /* must write cache */

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields != NULL) {
      if (!msg->msg_cached) {
	r = write_cached_envelope(cache_db_env, mmapstr,
            session, msg->msg_index, msg->msg_fields);
      }
    }
    
    if (msg->msg_flags != NULL) {
      r = pop3driver_write_cached_flags(cache_db_flags, mmapstr,
          msg->msg_uid, msg->msg_flags);
    }
  }

  /* flush cache */
  
  maildriver_cache_clean_up(cache_db_env, cache_db_flags, env_list);
  
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  mail_cache_db_close_unlock(filename_env, cache_db_env);
  mmap_string_free(mmapstr);

  /* remove cache files */
  
  maildriver_message_cache_clean_up(cached_data->pop3_cache_directory,
      env_list, get_uid_from_filename);
  
  return MAIL_NO_ERROR;

 close_db_env:
  mail_cache_db_close_unlock(filename_env, cache_db_env);
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int pop3driver_cached_get_message(mailsession * session,
    uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, pop3_cached_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

static int pop3driver_cached_get_message_by_uid(mailsession * session,
				  const char * uid, mailmessage ** result)
{
  mailpop3 * pop3;
  struct mailpop3_msg_info * msg_info;
  int found;
  unsigned int i;
  
  if (uid == NULL)
    return MAIL_ERROR_INVAL;
  
  pop3 = get_pop3_session(session);
  
  found = 0;
  
  /* iterate all messages and look for uid */
  for(i = 0 ; i < carray_count(pop3->pop3_msg_tab) ; i++) {
    msg_info = carray_get(pop3->pop3_msg_tab, i);
    
    if (msg_info == NULL)
      continue;
    
    if (msg_info->msg_deleted)
      continue;
    
    /* uid found, stop looking */
    if (strcmp(msg_info->msg_uidl, uid) == 0) {
      found = 1;
      break;
    }
  }
  
  if (!found)
    return MAIL_ERROR_MSG_NOT_FOUND;
  
  return pop3driver_cached_get_message(session, msg_info->msg_index, result);
}

static int pop3driver_cached_login_sasl(mailsession * session,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
  return mailsession_login_sasl(get_ancestor(session), auth_type,
      server_fqdn,
      local_ip_port,
      remote_ip_port,
      login, auth_name,
      password, realm);
}
