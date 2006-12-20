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
 * $Id: mboxdriver_cached.c,v 1.53 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mboxdriver_cached.h"

#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#	include "win_etpan.h"
#else
#	include <dirent.h>
#	include <unistd.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "mail.h"
#include "mail_cache_db.h"
#include "mboxdriver.h"
#include "mboxdriver_tools.h"
#include "maildriver_tools.h"
#include "mailmbox.h"
#include "maildriver.h"
#include "carray.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "mboxdriver_cached_message.h"
#include "libetpan-config.h"

static int mboxdriver_cached_initialize(mailsession * session);

static void mboxdriver_cached_uninitialize(mailsession * session);

static int mboxdriver_cached_parameters(mailsession * session,
					int id, void * value);

static int mboxdriver_cached_connect_path(mailsession * session, const char * path);

static int mboxdriver_cached_logout(mailsession * session);

static int mboxdriver_cached_check_folder(mailsession * session);

static int mboxdriver_cached_expunge_folder(mailsession * session);

static int mboxdriver_cached_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);
static int mboxdriver_cached_messages_number(mailsession * session, const char * mb,
					     uint32_t * result);
static int mboxdriver_cached_recent_number(mailsession * session, const char * mb,
					   uint32_t * result);
static int mboxdriver_cached_unseen_number(mailsession * session, const char * mb,
					   uint32_t * result);

static int mboxdriver_cached_append_message(mailsession * session,
					    const char * message, size_t size);

static int mboxdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);

static int
mboxdriver_cached_get_messages_list(mailsession * session,
				    struct mailmessage_list ** result);

static int
mboxdriver_cached_get_envelopes_list(mailsession * session,
			      struct mailmessage_list * env_list);

static int mboxdriver_cached_remove_message(mailsession * session,
					    uint32_t num);

static int mboxdriver_cached_get_message(mailsession * session,
					 uint32_t num, mailmessage ** result);

static int mboxdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result);

static mailsession_driver local_mbox_cached_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "mbox-cached",

  /* sess_initialize */ mboxdriver_cached_initialize,
  /* sess_uninitialize */ mboxdriver_cached_uninitialize,

  /* sess_parameters */ mboxdriver_cached_parameters,

  /* sess_connect_stream */ NULL,
  /* sess_connect_path */ mboxdriver_cached_connect_path,
  /* sess_starttls */ NULL,
  /* sess_login */ NULL,
  /* sess_logout */ mboxdriver_cached_logout,
  /* sess_noop */ NULL,

  /* sess_build_folder_name */ NULL,
  /* sess_create_folder */ NULL,
  /* sess_delete_folder */ NULL,
  /* sess_rename_folder */ NULL,
  /* sess_check_folder */ mboxdriver_cached_check_folder,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ NULL,
  /* sess_expunge_folder */ mboxdriver_cached_expunge_folder,
  /* sess_status_folder */ mboxdriver_cached_status_folder,
  /* sess_messages_number */ mboxdriver_cached_messages_number,
  /* sess_recent_number */ mboxdriver_cached_recent_number,
  /* sess_unseen_number */ mboxdriver_cached_unseen_number,
  /* sess_list_folders */ NULL,
  /* sess_lsub_folders */ NULL,
  /* sess_subscribe_folder */ NULL,
  /* sess_unsubscribe_folder */ NULL,

  /* sess_append_message */ mboxdriver_cached_append_message,
  /* sess_append_message_flags */ mboxdriver_cached_append_message_flags,
  
  /* sess_copy_message */ NULL,
  /* sess_move_message */ NULL,

  /* sess_get_message */ mboxdriver_cached_get_message,
  /* sess_get_message_by_uid */ mboxdriver_cached_get_message_by_uid,

  /* sess_get_messages_list */ mboxdriver_cached_get_messages_list,
  /* sess_get_envelopes_list */ mboxdriver_cached_get_envelopes_list,
  /* sess_remove_message */ mboxdriver_cached_remove_message,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "mbox-cached",

  .sess_initialize = mboxdriver_cached_initialize,
  .sess_uninitialize = mboxdriver_cached_uninitialize,

  .sess_parameters = mboxdriver_cached_parameters,

  .sess_connect_path = mboxdriver_cached_connect_path,
  .sess_connect_stream = NULL,
  .sess_starttls = NULL,
  .sess_login = NULL,
  .sess_logout = mboxdriver_cached_logout,
  .sess_noop = NULL,

  .sess_build_folder_name = NULL,
  .sess_create_folder = NULL,
  .sess_delete_folder = NULL,
  .sess_rename_folder = NULL,
  .sess_check_folder = mboxdriver_cached_check_folder,
  .sess_examine_folder = NULL,
  .sess_select_folder = NULL,
  .sess_expunge_folder = mboxdriver_cached_expunge_folder,
  .sess_status_folder = mboxdriver_cached_status_folder,
  .sess_messages_number = mboxdriver_cached_messages_number,
  .sess_recent_number = mboxdriver_cached_recent_number,
  .sess_unseen_number = mboxdriver_cached_unseen_number,
  .sess_list_folders = NULL,
  .sess_lsub_folders = NULL,
  .sess_subscribe_folder = NULL,
  .sess_unsubscribe_folder = NULL,

  .sess_append_message = mboxdriver_cached_append_message,
  .sess_append_message_flags = mboxdriver_cached_append_message_flags,
  
  .sess_copy_message = NULL,
  .sess_move_message = NULL,

  .sess_get_messages_list = mboxdriver_cached_get_messages_list,
  .sess_get_envelopes_list = mboxdriver_cached_get_envelopes_list,
  .sess_remove_message = mboxdriver_cached_remove_message,
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = mboxdriver_cached_get_message,
  .sess_get_message_by_uid = mboxdriver_cached_get_message_by_uid,
  .sess_login_sasl = NULL,
#endif
};

mailsession_driver * mbox_cached_session_driver =
&local_mbox_cached_session_driver;


#define ENV_NAME "env.db"
#define FLAGS_NAME "flags.db"



static int mbox_error_to_mail_error(int error)
{
  switch (error) {
  case MAILMBOX_NO_ERROR:
    return MAIL_NO_ERROR;

  case MAILMBOX_ERROR_PARSE:
    return MAIL_ERROR_PARSE;

  case MAILMBOX_ERROR_INVAL:
    return MAIL_ERROR_INVAL;

  case MAILMBOX_ERROR_FILE_NOT_FOUND:
    return MAIL_ERROR_PARSE;

  case MAILMBOX_ERROR_MEMORY:
    return MAIL_ERROR_MEMORY;

  case MAILMBOX_ERROR_TEMPORARY_FILE:
    return MAIL_ERROR_PARSE;

  case MAILMBOX_ERROR_FILE:
    return MAIL_ERROR_FILE;

  case MAILMBOX_ERROR_MSG_NOT_FOUND:
    return MAIL_ERROR_MSG_NOT_FOUND;

  case MAILMBOX_ERROR_READONLY:
    return MAIL_ERROR_READONLY;

  default:
    return MAIL_ERROR_INVAL;
  }
}




static inline struct mbox_cached_session_state_data *
get_cached_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession * get_ancestor(mailsession * session)
{
  return get_cached_data(session)->mbox_ancestor;
}

static inline struct mbox_session_state_data *
get_ancestor_data(mailsession * session)
{
  return get_ancestor(session)->sess_data;
}

static inline struct mailmbox_folder *
get_mbox_session(mailsession * session)
{
  return get_ancestor_data(session)->mbox_folder;
}

static int mboxdriver_cached_initialize(mailsession * session)
{
  struct mbox_cached_session_state_data * cached_data;
  struct mbox_session_state_data * mbox_data;

  cached_data = malloc(sizeof(* cached_data));
  if (cached_data == NULL)
    goto err;

  cached_data->mbox_flags_store = mail_flags_store_new();
  if (cached_data->mbox_flags_store == NULL)
    goto free;

  cached_data->mbox_ancestor = mailsession_new(mbox_session_driver);
  if (cached_data->mbox_ancestor == NULL)
    goto free_store;

  cached_data->mbox_quoted_mb = NULL;
  /*
    UID must be enabled to take advantage of the cache
  */
  mbox_data = cached_data->mbox_ancestor->sess_data;
  mbox_data->mbox_force_no_uid = FALSE;

  session->sess_data = cached_data;

  return MAIL_NO_ERROR;

 free_store:
  mail_flags_store_free(cached_data->mbox_flags_store);
 free:
  free(cached_data);
 err:
  return MAIL_ERROR_MEMORY;
}

static void free_state(struct mbox_cached_session_state_data * mbox_data)
{
  if (mbox_data->mbox_quoted_mb) {
    free(mbox_data->mbox_quoted_mb);
    mbox_data->mbox_quoted_mb = NULL;
  }
}

static int mbox_flags_store_process(char * flags_directory, char * quoted_mb,
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

    r = mboxdriver_write_cached_flags(cache_db_flags, mmapstr,
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

static void mboxdriver_cached_uninitialize(mailsession * session)
{
  struct mbox_cached_session_state_data * data;

  data = get_cached_data(session);

  mbox_flags_store_process(data->mbox_flags_directory,
      data->mbox_quoted_mb,
      data->mbox_flags_store);

  mail_flags_store_free(data->mbox_flags_store); 

  free_state(data);
  mailsession_free(data->mbox_ancestor);
  free(data);
  
  session->sess_data = NULL;
}

static int mboxdriver_cached_parameters(mailsession * session,
					int id, void * value)
{
  struct mbox_cached_session_state_data * data;
  int r;

  data = get_cached_data(session);

  switch (id) {
  case MBOXDRIVER_CACHED_SET_CACHE_DIRECTORY:
    strncpy(data->mbox_cache_directory, value, PATH_MAX);
    data->mbox_cache_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(data->mbox_cache_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  case MBOXDRIVER_CACHED_SET_FLAGS_DIRECTORY:
    strncpy(data->mbox_flags_directory, value, PATH_MAX);
    data->mbox_flags_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(data->mbox_flags_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  case MBOXDRIVER_SET_NO_UID:
    return MAIL_ERROR_INVAL;

  default:
    return mailsession_parameters(data->mbox_ancestor, id, value);
  }
}


static int get_cache_directory(mailsession * session,
			       const char * path, char ** result)
{
  char * quoted_mb;
  char dirname[PATH_MAX];
  int res;
  int r;
  struct mbox_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);

  quoted_mb = maildriver_quote_mailbox(path);
  if (quoted_mb == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  snprintf(dirname, PATH_MAX, "%s%c%s",
      cached_data->mbox_cache_directory, MAIL_DIR_SEPARATOR, quoted_mb);

  r = generic_cache_create_dir(dirname);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  snprintf(dirname, PATH_MAX, "%s%c%s",
      cached_data->mbox_flags_directory, MAIL_DIR_SEPARATOR, quoted_mb);

  r = generic_cache_create_dir(dirname);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  * result = quoted_mb;

  return MAIL_NO_ERROR;

 free:
  free(quoted_mb);
 err:
  return res;
}




#define FILENAME_MAX_UID "max-uid"

/* write max uid current value */

static int write_max_uid_value(mailsession * session)
{
  int r;
  char filename[PATH_MAX];
  FILE * f;
  int res;

#if 0
  struct mbox_session_state_data * mbox_data;
#endif
  struct mbox_cached_session_state_data * cached_data;
  int fd;

  MMAPString * mmapstr;
  size_t cur_token;
  struct mailmbox_folder * folder;

  /* expunge the mailbox */

#if 0
  mbox_data = get_ancestor(session)->data;
#endif
  folder = get_mbox_session(session);

  r = mailmbox_validate_write_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = mbox_error_to_mail_error(r);
    goto err;
  }

  r = mailmbox_expunge_no_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  cached_data = get_cached_data(session);

  snprintf(filename, PATH_MAX, "%s%c%s%c%s",
	   cached_data->mbox_flags_directory, MAIL_DIR_SEPARATOR,
	   cached_data->mbox_quoted_mb, MAIL_DIR_SEPARATOR, FILENAME_MAX_UID);

  fd = creat(filename, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  f = fdopen(fd, "w");
  if (f == NULL) {
    close(fd);
    res = MAIL_ERROR_FILE;
    goto unlock;
  }

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close;
  }

  r = mail_serialize_clear(mmapstr, &cur_token);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }

  r = mailimf_cache_int_write(mmapstr, &cur_token,
      folder->mb_written_uid);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }

  fwrite(mmapstr->str, 1, mmapstr->len, f);

  mmap_string_free(mmapstr);
  fclose(f);
  mailmbox_write_unlock(folder);

  return MAIL_NO_ERROR;

 free_mmapstr:
  mmap_string_free(mmapstr);
 close:
  fclose(f);
 unlock:
  mailmbox_read_unlock(folder);
 err:
  return res;
}

static int read_max_uid_value(mailsession * session, uint32_t * result)
{
  int r;
  char filename[PATH_MAX];
  FILE * f;
  uint32_t written_uid;
  int res;

  struct mbox_cached_session_state_data * cached_data;

  MMAPString * mmapstr;
  size_t cur_token;
  char buf[sizeof(uint32_t)];
  size_t read_size;

  cached_data = get_cached_data(session);

  snprintf(filename, PATH_MAX, "%s%c%s%c%s",
	   cached_data->mbox_flags_directory, MAIL_DIR_SEPARATOR,
	   cached_data->mbox_quoted_mb, MAIL_DIR_SEPARATOR, FILENAME_MAX_UID);

  f = fopen(filename, "r");
  if (f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }

  read_size = fread(buf, 1, sizeof(uint32_t), f);

  mmapstr = mmap_string_new_len(buf, read_size);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close;
  }

  cur_token = 0;
  
  r = mailimf_cache_int_read(mmapstr, &cur_token, &written_uid);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }

  mmap_string_free(mmapstr);
  fclose(f);

  * result = written_uid;

  return MAIL_NO_ERROR;

 free_mmapstr:
  mmap_string_free(mmapstr);
 close:
  fclose(f);
 err:
  return res;
}

static int mboxdriver_cached_connect_path(mailsession * session, const char * path)
{
  int r;
  int res;
  char * quoted_mb;
  struct mbox_cached_session_state_data * cached_data;
  struct mbox_session_state_data * ancestor_data;
  struct mailmbox_folder * folder;
  uint32_t written_uid;
  
  folder = get_mbox_session(session);
  if (folder != NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }
  
  quoted_mb = NULL;
  r = get_cache_directory(session, path, &quoted_mb);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  cached_data = get_cached_data(session);
  free_state(cached_data);
  
  cached_data->mbox_quoted_mb = quoted_mb;

  written_uid = 0;
  r = read_max_uid_value(session, &written_uid);
  /* ignore errors */

  ancestor_data = get_ancestor_data(session);

  r = mailmbox_init(path,
		    ancestor_data->mbox_force_read_only,
		    ancestor_data->mbox_force_no_uid,
		    written_uid,
		    &folder);

  if (r != MAILMBOX_NO_ERROR) {
    cached_data->mbox_quoted_mb = NULL;

    res = mboxdriver_mbox_error_to_mail_error(r);
    goto free;
  }

  ancestor_data->mbox_folder = folder;

  return MAIL_NO_ERROR;
  
 free:
  free(quoted_mb);
 err:
  return res;
}


static int mboxdriver_cached_logout(mailsession * session)
{
  struct mbox_cached_session_state_data * cached_data;
  int r;

  r = write_max_uid_value(session);

  cached_data = get_cached_data(session);

  mbox_flags_store_process(cached_data->mbox_flags_directory,
			   cached_data->mbox_quoted_mb,
			   cached_data->mbox_flags_store);

  r = mailsession_logout(get_ancestor(session));
  if (r != MAIL_NO_ERROR)
    return r;

  free_state(cached_data);

  return MAIL_NO_ERROR;
}

static int mboxdriver_cached_check_folder(mailsession * session)
{
  struct mbox_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);

  mbox_flags_store_process(cached_data->mbox_flags_directory,
                           cached_data->mbox_quoted_mb,
			   cached_data->mbox_flags_store);

  return MAIL_NO_ERROR;
}

static int mboxdriver_cached_expunge_folder(mailsession * session)
{
  struct mailmbox_folder * folder;
  int res;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  struct mbox_cached_session_state_data * data;
  int r;
  unsigned int i;

  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  data = get_cached_data(session);
  if (data->mbox_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  mbox_flags_store_process(data->mbox_flags_directory,
			   data->mbox_quoted_mb,
			   data->mbox_flags_store);

  snprintf(filename_flags, PATH_MAX, "%s%c%s%c%s",
      data->mbox_flags_directory, MAIL_DIR_SEPARATOR, data->mbox_quoted_mb,
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

  for(i = 0 ; i < carray_count(folder->mb_tab) ; i ++) {
    struct mailmbox_msg_info * msg_info;
    struct mail_flags * flags;

    msg_info = carray_get(folder->mb_tab, i);
    if (msg_info == NULL)
      continue;

    if (msg_info->msg_deleted)
      continue;

    r = mboxdriver_get_cached_flags(cache_db_flags, mmapstr,
        session, msg_info->msg_uid, &flags);
    if (r != MAIL_NO_ERROR)
      continue;

    if (flags->fl_flags & MAIL_FLAG_DELETED) {
      r = mailmbox_delete_msg(folder, msg_info->msg_uid);
    }

    mail_flags_free(flags);
  }
  
  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  
  r = mailmbox_expunge(folder);

  return MAIL_NO_ERROR;

 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}

static int mboxdriver_cached_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  struct mailmbox_folder * folder;
  int res;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  struct mbox_cached_session_state_data * data;
  int r;
  unsigned int i;
  uint32_t recent;
  uint32_t unseen;
  uint32_t num;
  
  num = 0;
  recent = 0;
  unseen = 0;
  
  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  data = get_cached_data(session);
  if (data->mbox_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  r = mailmbox_validate_read_lock(folder);
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  mailmbox_read_unlock(folder);

  mbox_flags_store_process(data->mbox_flags_directory, data->mbox_quoted_mb,
			   data->mbox_flags_store);

  snprintf(filename_flags, PATH_MAX, "%s%c%s%c%s",
      data->mbox_flags_directory, MAIL_DIR_SEPARATOR, data->mbox_quoted_mb,
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

  for(i = 0 ; i < carray_count(folder->mb_tab) ; i ++) {
    struct mailmbox_msg_info * msg_info;
    struct mail_flags * flags;

    msg_info = carray_get(folder->mb_tab, i);
    if (msg_info == NULL)
      continue;

    if (msg_info->msg_deleted)
      continue;

    r = mboxdriver_get_cached_flags(cache_db_flags, mmapstr,
        session, msg_info->msg_uid, &flags);
    if (r != MAIL_NO_ERROR) {
      recent ++;
      unseen ++;
      num ++;
      continue;
    }

    if ((flags->fl_flags & MAIL_FLAG_NEW) != 0) {
      recent ++;
    }
    if ((flags->fl_flags & MAIL_FLAG_SEEN) == 0) {
      unseen ++;
    }

    num ++;

    mail_flags_free(flags);
  }

  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);

  * result_messages = num;
  * result_recent = recent;
  * result_unseen = unseen;
  
  return MAIL_NO_ERROR;

 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}

static int mboxdriver_cached_messages_number(mailsession * session, const char * mb,
					     uint32_t * result)
{
  return mailsession_messages_number(get_ancestor(session), mb, result);
}


static int mboxdriver_cached_recent_number(mailsession * session, const char * mb,
					   uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = mboxdriver_cached_status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = recent;
  
  return MAIL_NO_ERROR;  
}

static int mboxdriver_cached_unseen_number(mailsession * session, const char * mb,
					   uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = mboxdriver_cached_status_folder(session, mb,
      &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = unseen;
  
  return MAIL_NO_ERROR;
}

/* messages operations */

static int mboxdriver_cached_append_message(mailsession * session,
					    const char * message, size_t size)
{
  return mboxdriver_cached_append_message_flags(session,
      message, size, NULL);
}

static int mboxdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  int r;
  struct mailmbox_folder * folder;
  struct mbox_cached_session_state_data * data;
  unsigned int uid;
  struct mailmbox_msg_info * msg_info;
  chashdatum key;
  chashdatum value;
  struct mail_cache_db * cache_db_flags;
  char filename_flags[PATH_MAX];
  MMAPString * mmapstr;
  char keyname[PATH_MAX];
  
  folder = get_mbox_session(session);
  if (folder == NULL)
    return MAIL_ERROR_APPEND;
  
  r = mailmbox_append_message_uid(folder, message, size, &uid);
  
  switch (r) {
  case MAILMBOX_ERROR_FILE:
    return MAIL_ERROR_DISKSPACE;
  case MAILMBOX_NO_ERROR:
    break;
  default:
    return mboxdriver_mbox_error_to_mail_error(r);
  }
  
  /* could store in flags store instead */
  
  if (flags == NULL)
    goto exit;
  
  key.data = &uid;
  key.len = sizeof(uid); 
  r = chash_get(folder->mb_hash, &key, &value);
  if (r < 0)
    goto exit;
  
  msg_info = value.data;
  
  data = get_cached_data(session);
  
  snprintf(filename_flags, PATH_MAX, "%s%c%s%c%s",
      data->mbox_flags_directory, MAIL_DIR_SEPARATOR, data->mbox_quoted_mb,
      MAIL_DIR_SEPARATOR, FLAGS_NAME);
  
  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0)
    goto exit;
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL)
    goto close_db_flags;
  
  snprintf(keyname, PATH_MAX, "%u-%lu", uid,
      (unsigned long) msg_info->msg_body_len);
  
  r = mboxdriver_write_cached_flags(cache_db_flags, mmapstr, keyname, flags);
  
  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  
  if (r != MAIL_NO_ERROR)
    goto exit;
  
  return MAIL_NO_ERROR;
  
 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 exit:
  return MAIL_NO_ERROR;
}

static int
mboxdriver_cached_get_messages_list(mailsession * session,
				    struct mailmessage_list ** result)
{
  struct mailmbox_folder * folder;
  int res;

  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  return mbox_get_uid_messages_list(folder,
      session, mbox_cached_message_driver, result);

 err:
  return res;
}

static int
get_cached_envelope(struct mail_cache_db * cache_db, MMAPString * mmapstr,
    mailsession * session, uint32_t num,
    struct mailimf_fields ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mailimf_fields * fields;
  int res;
  struct mailmbox_msg_info * info;
  struct mailmbox_folder * folder;
  chashdatum key;
  chashdatum data;
  
  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }
  
  key.data = &num;
  key.len = sizeof(num);
  
  r = chash_get(folder->mb_hash, &key, &data);
  if (r < 0) {
    res = MAIL_ERROR_MSG_NOT_FOUND;
    goto err;
  }
  
  info = data.data;
  
  snprintf(keyname, PATH_MAX, "%u-%lu-envelope", num,
      (unsigned long) info->msg_body_len);

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
write_cached_envelope(struct mail_cache_db * cache_db, MMAPString * mmapstr,
    mailsession * session, uint32_t num,
    struct mailimf_fields * fields)
{
  int r;
  char keyname[PATH_MAX];
  int res;
  struct mailmbox_msg_info * info;
  struct mailmbox_folder * folder;
  chashdatum key;
  chashdatum data;
  
  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  key.data = &num;
  key.len = sizeof(num);
  
  r = chash_get(folder->mb_hash, &key, &data);
  if (r < 0) {
    res = MAIL_ERROR_MSG_NOT_FOUND;
    goto err;
  }
  
  info = data.data;
  
  snprintf(keyname, PATH_MAX, "%u-%lu-envelope", num,
      (unsigned long) info->msg_body_len);

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
mboxdriver_cached_get_envelopes_list(mailsession * session,
				     struct mailmessage_list * env_list)
{
  int r;
  unsigned int i;
  struct mbox_cached_session_state_data * cached_data;
  char filename_env[PATH_MAX];
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_env;
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  int res;
  struct mailmbox_folder * folder;

  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  cached_data = get_cached_data(session);
  if (cached_data->mbox_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  mbox_flags_store_process(cached_data->mbox_flags_directory,
			   cached_data->mbox_quoted_mb,
			   cached_data->mbox_flags_store);

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  snprintf(filename_env, PATH_MAX, "%s%c%s%c%s",
      cached_data->mbox_cache_directory, MAIL_DIR_SEPARATOR,
      cached_data->mbox_quoted_mb,
      MAIL_DIR_SEPARATOR, ENV_NAME);

  r = mail_cache_db_open_lock(filename_env, &cache_db_env);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }

  snprintf(filename_flags, PATH_MAX, "%s%c%s%c%s",
      cached_data->mbox_flags_directory, MAIL_DIR_SEPARATOR,
      cached_data->mbox_quoted_mb,
      MAIL_DIR_SEPARATOR, FLAGS_NAME);

  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto close_db_env;
  }

  /* fill with cached */

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {   
    mailmessage * msg;
    struct mailimf_fields * fields;
    struct mail_flags * flags;
    
    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields == NULL) {
      r = get_cached_envelope(cache_db_env, mmapstr, session,
          msg->msg_index, &fields);
      if (r == MAIL_NO_ERROR) {
        msg->msg_cached = TRUE;
        msg->msg_fields = fields;
      }
    }

    if (msg->msg_flags == NULL) {
      r = mboxdriver_get_cached_flags(cache_db_flags, mmapstr,
          session, msg->msg_index,
          &flags);
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
    res = MAIL_ERROR_FILE;
    goto close_db_env;
  }

  /* must write cache */

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields != NULL) {
      if (!msg->msg_cached) {
        /* msg->msg_index is the numerical UID of the message */
	r = write_cached_envelope(cache_db_env, mmapstr,
            session, msg->msg_index, msg->msg_fields);
      }
    }
    
    if (msg->msg_flags != NULL) {
      r = mboxdriver_write_cached_flags(cache_db_flags, mmapstr,
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


static int
mboxdriver_cached_remove_message(mailsession * session, uint32_t num)
{
  return mailsession_remove_message(get_ancestor(session), num);
}

static int mboxdriver_cached_get_message(mailsession * session,
					 uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, mbox_cached_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

static int mboxdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result)
{
  uint32_t num;
  char * p;
  chashdatum key;
  chashdatum data;
  struct mailmbox_msg_info * info;
  struct mailmbox_folder * folder;
  int r;

  if (uid == NULL)
    return MAIL_ERROR_INVAL;

  num = strtoul(uid, &p, 10);
  if (p == uid || * p != '-')
    return MAIL_ERROR_INVAL;

  folder = get_mbox_session(session);
  if (folder == NULL)
    return MAIL_ERROR_BAD_STATE;

  key.data = &num;
  key.len = sizeof(num);

  r = chash_get(folder->mb_hash, &key, &data);
  if (r == 0) {
    char * body_len_p = p + 1;
    size_t body_len;
    
    info = data.data;
    /* Check if the cached message has the same UID */
    body_len = strtoul(body_len_p, &p, 10);
    if (p == body_len_p || * p != '\0')
      return MAIL_ERROR_INVAL;

    if (body_len == info->msg_body_len)
      return mboxdriver_cached_get_message(session, num, result);
  }

  return MAIL_ERROR_MSG_NOT_FOUND;
}
