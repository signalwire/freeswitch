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
 * $Id: mhdriver_cached.c,v 1.46 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mhdriver_cached.h"

#include <stdio.h>
#include <sys/types.h>
#ifdef _MSC_VER
#	include "win_etpan.h"
#else
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
#include "mail_cache_db.h"

#include "generic_cache.h"
#include "imfcache.h"
#include "mhdriver.h"

#include "mhdriver_cached_message.h"
#include "mailmh.h"
#include "maildriver_tools.h"
#include "mhdriver_tools.h"
#include "mailmessage.h"

static int mhdriver_cached_initialize(mailsession * session);

static void mhdriver_cached_uninitialize(mailsession * session);

static int mhdriver_cached_parameters(mailsession * session,
				      int id, void * value);

static int mhdriver_cached_connect_path(mailsession * session, const char * path);
static int mhdriver_cached_logout(mailsession * session);

static int mhdriver_cached_build_folder_name(mailsession * session, const char * mb,
					     const char * name, char ** result);
static int mhdriver_cached_create_folder(mailsession * session, const char * mb);

static int mhdriver_cached_delete_folder(mailsession * session, const char * mb);

static int mhdriver_cached_rename_folder(mailsession * session, const char * mb,
					 const char * new_name);

static int mhdriver_cached_check_folder(mailsession * session);

static int mhdriver_cached_select_folder(mailsession * session, const char * mb);

static int mhdriver_cached_expunge_folder(mailsession * session);

static int mhdriver_cached_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);

static int mhdriver_cached_messages_number(mailsession * session, const char * mb,
					   uint32_t * result);
static int mhdriver_cached_recent_number(mailsession * session, const char * mb,
					   uint32_t * result);
static int mhdriver_cached_unseen_number(mailsession * session, const char * mb,
					   uint32_t * result);

static int mhdriver_cached_list_folders(mailsession * session, const char * mb,
					struct mail_list ** result);

static int mhdriver_cached_lsub_folders(mailsession * session, const char * mb,
					struct mail_list ** result);

static int mhdriver_cached_subscribe_folder(mailsession * session, const char * mb);

static int mhdriver_cached_unsubscribe_folder(mailsession * session,
					      const char * mb);

static int mhdriver_cached_append_message(mailsession * session,
					  const char * message, size_t size);
static int mhdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);
static int mhdriver_cached_copy_message(mailsession * session,
					uint32_t num, const char * mb);

static int mhdriver_cached_remove_message(mailsession * session,
					  uint32_t num);

static int mhdriver_cached_move_message(mailsession * session,
					uint32_t num, const char * mb);

static int
mhdriver_cached_get_messages_list(mailsession * session,
				  struct mailmessage_list ** result);

static int
mhdriver_cached_get_envelopes_list(mailsession * session,
				   struct mailmessage_list * env_list);

static int mhdriver_cached_get_message(mailsession * session,
				       uint32_t num, mailmessage ** result);

static int mhdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result);

static mailsession_driver local_mh_cached_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "mh-cached",

  /* sess_initialize */ mhdriver_cached_initialize,
  /* sess_uninitialize */ mhdriver_cached_uninitialize,

  /* sess_parameters */ mhdriver_cached_parameters,

  /* sess_connect_stream */ NULL,
  /* sess_connect_path */ mhdriver_cached_connect_path,
  /* sess_starttls */ NULL,
  /* sess_login */ NULL,
  /* sess_logout */ mhdriver_cached_logout,
  /* sess_noop */ NULL,

  /* sess_build_folder_name */ mhdriver_cached_build_folder_name,
  /* sess_create_folder */ mhdriver_cached_create_folder,
  /* sess_delete_folder */ mhdriver_cached_delete_folder,
  /* sess_rename_folder */ mhdriver_cached_rename_folder,
  /* sess_check_folder */ mhdriver_cached_check_folder,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ mhdriver_cached_select_folder,
  /* sess_expunge_folder */ mhdriver_cached_expunge_folder,
  /* sess_status_folder */ mhdriver_cached_status_folder,
  /* sess_messages_number */ mhdriver_cached_messages_number,
  /* sess_recent_number */ mhdriver_cached_recent_number,
  /* sess_unseen_number */ mhdriver_cached_unseen_number,
  /* sess_list_folders */ mhdriver_cached_list_folders,
  /* sess_lsub_folders */ mhdriver_cached_lsub_folders,
  /* sess_subscribe_folder */ mhdriver_cached_subscribe_folder,
  /* sess_unsubscribe_folder */ mhdriver_cached_unsubscribe_folder,

  /* sess_append_message */ mhdriver_cached_append_message,
  /* sess_append_message_flags */ mhdriver_cached_append_message_flags,
  /* sess_copy_message */ mhdriver_cached_copy_message,
  /* sess_move_message */ mhdriver_cached_move_message,

  /* sess_get_message */ mhdriver_cached_get_message,
  /* sess_get_message_by_uid */ mhdriver_cached_get_message_by_uid,

  /* sess_get_messages_list */ mhdriver_cached_get_messages_list,
  /* sess_get_envelopes_list */ mhdriver_cached_get_envelopes_list,
  /* sess_remove_message */ mhdriver_cached_remove_message,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "mh-cached",

  .sess_initialize = mhdriver_cached_initialize,
  .sess_uninitialize = mhdriver_cached_uninitialize,

  .sess_parameters = mhdriver_cached_parameters,

  .sess_connect_stream = NULL,
  .sess_connect_path = mhdriver_cached_connect_path,
  .sess_starttls = NULL,
  .sess_login = NULL,
  .sess_logout = mhdriver_cached_logout,
  .sess_noop = NULL,

  .sess_build_folder_name = mhdriver_cached_build_folder_name,
  .sess_create_folder = mhdriver_cached_create_folder,
  .sess_delete_folder = mhdriver_cached_delete_folder,
  .sess_rename_folder = mhdriver_cached_rename_folder,
  .sess_check_folder = mhdriver_cached_check_folder,
  .sess_examine_folder = NULL,
  .sess_select_folder = mhdriver_cached_select_folder,
  .sess_expunge_folder = mhdriver_cached_expunge_folder,
  .sess_status_folder = mhdriver_cached_status_folder,
  .sess_messages_number = mhdriver_cached_messages_number,
  .sess_recent_number = mhdriver_cached_recent_number,
  .sess_unseen_number = mhdriver_cached_unseen_number,
  .sess_list_folders = mhdriver_cached_list_folders,
  .sess_lsub_folders = mhdriver_cached_lsub_folders,
  .sess_subscribe_folder = mhdriver_cached_subscribe_folder,
  .sess_unsubscribe_folder = mhdriver_cached_unsubscribe_folder,

  .sess_append_message = mhdriver_cached_append_message,
  .sess_append_message_flags = mhdriver_cached_append_message_flags,
  .sess_copy_message = mhdriver_cached_copy_message,
  .sess_move_message = mhdriver_cached_move_message,

  .sess_get_messages_list = mhdriver_cached_get_messages_list,
  .sess_get_envelopes_list = mhdriver_cached_get_envelopes_list,
  .sess_remove_message = mhdriver_cached_remove_message,
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = mhdriver_cached_get_message,
  .sess_get_message_by_uid = mhdriver_cached_get_message_by_uid,
  .sess_login_sasl = NULL,
#endif
};

mailsession_driver * mh_cached_session_driver =
&local_mh_cached_session_driver;

#define ENV_NAME "env.db"
#define FLAGS_NAME "flags.db"


static inline struct mh_cached_session_state_data *
get_cached_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession * get_ancestor(mailsession * session)
{
  return get_cached_data(session)->mh_ancestor;
}

static inline struct mh_session_state_data *
get_ancestor_data(mailsession * session)
{
  return get_ancestor(session)->sess_data;
}

static inline struct mailmh *
get_mh_session(mailsession * session)
{
  return get_ancestor_data(session)->mh_session;
}

static inline struct mailmh_folder *
get_mh_cur_folder(mailsession * session)
{
  return get_ancestor_data(session)->mh_cur_folder;
}


#define FILENAME_MAX_UID "max-uid"

/* write max uid current value */

static int write_max_uid_value(mailsession * session)
{
  int r;
  char filename[PATH_MAX];
  FILE * f;
  int res;
  struct mh_cached_session_state_data * cached_data;
  struct mh_session_state_data * ancestor_data;
  int fd;
  
  MMAPString * mmapstr;
  size_t cur_token;

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  if (cached_data->mh_quoted_mb == NULL)
    return MAIL_ERROR_BAD_STATE;

  snprintf(filename, PATH_MAX, "%s/%s/%s",
	   cached_data->mh_cache_directory,
	   cached_data->mh_quoted_mb, FILENAME_MAX_UID);

  fd = creat(filename, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  f = fdopen(fd, "w");
  if (f == NULL) {
    close(fd);
    res = MAIL_ERROR_FILE;
    goto err;
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
      ancestor_data->mh_cur_folder->fl_max_index);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }

  fwrite(mmapstr->str, 1, mmapstr->len, f);

  mmap_string_free(mmapstr);
  fclose(f);

  return MAIL_NO_ERROR;

 free_mmapstr:
  mmap_string_free(mmapstr);
 close:
  fclose(f);
 err:
  return res;
}

static int read_max_uid_value(mailsession * session)
{
  int r;
  char filename[PATH_MAX];
  FILE * f;
  uint32_t written_uid;
  int res;
  struct mh_cached_session_state_data * cached_data;
  struct mh_session_state_data * ancestor_data;

  MMAPString * mmapstr;
  size_t cur_token;
  char buf[sizeof(uint32_t)];
  size_t read_size;

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  snprintf(filename, PATH_MAX, "%s/%s/%s",
	   cached_data->mh_cache_directory,
	   cached_data->mh_quoted_mb, FILENAME_MAX_UID);

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
    fclose(f);
    res = r;
    goto free_mmapstr;
  }

  mmap_string_free(mmapstr);
  fclose(f);

  if (written_uid > ancestor_data->mh_cur_folder->fl_max_index)
    ancestor_data->mh_cur_folder->fl_max_index = written_uid;

  return MAIL_NO_ERROR;

 free_mmapstr:
  mmap_string_free(mmapstr);
 close:
  fclose(f);
 err:
  return res;
}


static int mhdriver_cached_initialize(mailsession * session)
{
  struct mh_cached_session_state_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;

  data->mh_flags_store = mail_flags_store_new();
  if (data->mh_flags_store == NULL)
    goto free;

  data->mh_ancestor = mailsession_new(mh_session_driver);
  if (data->mh_ancestor == NULL)
    goto free_store;

  data->mh_quoted_mb = NULL;
  
  session->sess_data = data;
  
  return MAIL_NO_ERROR;

 free_store:
  mail_flags_store_free(data->mh_flags_store);
 free:
  free(data);
 err:
  return MAIL_ERROR_MEMORY;
}

static void free_state(struct mh_cached_session_state_data * mh_data)
{
  if (mh_data->mh_quoted_mb) {
    free(mh_data->mh_quoted_mb);
    mh_data->mh_quoted_mb = NULL;
  }
}

static int mh_flags_store_process(char * flags_directory, char * quoted_mb,
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

  snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
	   flags_directory, quoted_mb, FLAGS_NAME);

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

    r = mhdriver_write_cached_flags(cache_db_flags, mmapstr,
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

static void mhdriver_cached_uninitialize(mailsession * session)
{
  struct mh_cached_session_state_data * data;

  data = get_cached_data(session);

  mh_flags_store_process(data->mh_flags_directory, data->mh_quoted_mb,
			 data->mh_flags_store);

  mail_flags_store_free(data->mh_flags_store); 

  free_state(data);
  mailsession_free(data->mh_ancestor);
  free(data);
  
  session->sess_data = NULL;
}

static int mhdriver_cached_parameters(mailsession * session,
				      int id, void * value)
{
  struct mh_cached_session_state_data * cached_data;
  int r;

  cached_data = get_cached_data(session);

  switch (id) {
  case MHDRIVER_CACHED_SET_CACHE_DIRECTORY:
    strncpy(cached_data->mh_cache_directory, value, PATH_MAX);
    cached_data->mh_cache_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(cached_data->mh_cache_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  case MHDRIVER_CACHED_SET_FLAGS_DIRECTORY:
    strncpy(cached_data->mh_flags_directory, value, PATH_MAX);
    cached_data->mh_flags_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(cached_data->mh_flags_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;
  }

  return MAIL_ERROR_INVAL;
}

static int mhdriver_cached_connect_path(mailsession * session, const char * path)
{
  return mailsession_connect_path(get_ancestor(session), path);
}

static int mhdriver_cached_logout(mailsession * session)
{
  int r;
  struct mh_cached_session_state_data * cached_data;

  r = write_max_uid_value(session);

  cached_data = get_cached_data(session);

  mh_flags_store_process(cached_data->mh_flags_directory,
			 cached_data->mh_quoted_mb,
			 cached_data->mh_flags_store);
  
  return mailsession_logout(get_ancestor(session));
}

static int mhdriver_cached_check_folder(mailsession * session)
{
  struct mh_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);

  mh_flags_store_process(cached_data->mh_flags_directory,
                         cached_data->mh_quoted_mb,
                         cached_data->mh_flags_store);

  return MAIL_NO_ERROR;
}

/* folders operations */

static int mhdriver_cached_build_folder_name(mailsession * session, const char * mb,
					     const char * name, char ** result)
{
  return mailsession_build_folder_name(get_ancestor(session),
      mb, name, result);
}

static int mhdriver_cached_create_folder(mailsession * session, const char * mb)
{
  return mailsession_create_folder(get_ancestor(session), mb);
}

static int mhdriver_cached_delete_folder(mailsession * session, const char * mb)
{
  return mailsession_delete_folder(get_ancestor(session), mb);
}

static int mhdriver_cached_rename_folder(mailsession * session, const char * mb,
					 const char * new_name)
{
  return mailsession_rename_folder(get_ancestor(session), mb, new_name);
}

static int get_cache_directory(mailsession * session,
			       const char * path, char ** result)
{
  char * quoted_mb;
  char dirname[PATH_MAX];
  int res;
  int r;
  struct mh_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);

  quoted_mb = maildriver_quote_mailbox(path);
  if (quoted_mb == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  snprintf(dirname, PATH_MAX, "%s/%s",
	   cached_data->mh_cache_directory, quoted_mb);

  r = generic_cache_create_dir(dirname);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  snprintf(dirname, PATH_MAX, "%s/%s",
	   cached_data->mh_flags_directory, quoted_mb);

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

static int mhdriver_cached_select_folder(mailsession * session, const char * mb)
{
  int r;
  int res;
  char * quoted_mb;
  struct mh_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);

  mh_flags_store_process(cached_data->mh_flags_directory,
      cached_data->mh_quoted_mb,
      cached_data->mh_flags_store);
  
  quoted_mb = NULL;
  r = get_cache_directory(session, mb, &quoted_mb);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailsession_select_folder(get_ancestor(session), mb);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  r = write_max_uid_value(session);

  free_state(cached_data);
  cached_data->mh_quoted_mb = quoted_mb;

  r = read_max_uid_value(session);

  return MAIL_NO_ERROR;

 free:
  free(quoted_mb);
 err:
  return res;
}

static int mhdriver_cached_expunge_folder(mailsession * session)
{
  struct mailmh_folder * folder;
  int res;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  struct mh_cached_session_state_data * cached_data;
  unsigned int i;
  int r;

  cached_data = get_cached_data(session);
  if (cached_data->mh_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  mh_flags_store_process(cached_data->mh_flags_directory,
      cached_data->mh_quoted_mb,
      cached_data->mh_flags_store);

  folder = get_mh_cur_folder(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
      cached_data->mh_flags_directory, cached_data->mh_quoted_mb, FLAGS_NAME);

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

  for(i = 0 ; i < carray_count(folder->fl_msgs_tab) ; i++) {
    struct mailmh_msg_info * mh_info;
    struct mail_flags * flags;
    
    mh_info = carray_get(folder->fl_msgs_tab, i);
    if (mh_info == NULL)
      continue;

    r = mhdriver_get_cached_flags(cache_db_flags, mmapstr,
        session, mh_info->msg_index, &flags);
    if (r != MAIL_NO_ERROR)
      continue;

    if (flags->fl_flags & MAIL_FLAG_DELETED) {
      r = mailmh_folder_remove_message(folder, mh_info->msg_index);
    }

    mail_flags_free(flags);
  }

  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);

  mailmh_folder_update(folder);
  
  return MAIL_NO_ERROR;

 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}


static int mhdriver_cached_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages,
    uint32_t * result_recent,
    uint32_t * result_unseen)
{
  struct mailmh_folder * folder;
  int res;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  struct mh_cached_session_state_data * cached_data;
  unsigned int i;
  int r;
  uint32_t count;
  uint32_t recent;
  uint32_t unseen;

  r = mhdriver_cached_select_folder(session, mb);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  count = 0;
  recent = 0;
  unseen = 0;
  
  folder = get_mh_cur_folder(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  cached_data = get_cached_data(session);
  if (cached_data->mh_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
      cached_data->mh_flags_directory,
      cached_data->mh_quoted_mb, FLAGS_NAME);

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

  for(i = 0 ; i < carray_count(folder->fl_msgs_tab) ; i++) {
    struct mailmh_msg_info * mh_info;
    struct mail_flags * flags;
    
    mh_info = carray_get(folder->fl_msgs_tab, i);
    if (mh_info == NULL)
      continue;

    count ++;

    r = mhdriver_get_cached_flags(cache_db_flags, mmapstr,
        session, mh_info->msg_index, 
        &flags);

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

  * result_messages = count;
  * result_recent = recent;
  * result_unseen = unseen;
  
  return MAIL_NO_ERROR;

 close_db_flags:
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
 err:
  return res;
}

static int mhdriver_cached_messages_number(mailsession * session, const char * mb,
					   uint32_t * result)
{
  return mailsession_messages_number(get_ancestor(session), mb, result);
}

static int mhdriver_cached_recent_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = mhdriver_cached_status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = recent;
  
  return MAIL_NO_ERROR;  
}


static int mhdriver_cached_unseen_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = mhdriver_cached_status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = recent;
  
  return MAIL_NO_ERROR;  
}

  
static int mhdriver_cached_list_folders(mailsession * session, const char * mb,
					struct mail_list ** result)
{
  return mailsession_list_folders(get_ancestor(session), mb, result);
}

static int mhdriver_cached_lsub_folders(mailsession * session, const char * mb,
					struct mail_list ** result)
{
  return mailsession_lsub_folders(get_ancestor(session), mb, result);
}

static int mhdriver_cached_subscribe_folder(mailsession * session, const char * mb)
{
  return mailsession_subscribe_folder(get_ancestor(session), mb);
}

static int mhdriver_cached_unsubscribe_folder(mailsession * session,
					      const char * mb)
{
  return mailsession_unsubscribe_folder(get_ancestor(session), mb);
}

/* messages operations */

static int mhdriver_cached_append_message(mailsession * session,
					  const char * message, size_t size)
{
  return mhdriver_cached_append_message_flags(session,
      message, size, NULL);
}

static int mhdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{ 
  int r;
  struct mailmh_folder * folder;
  struct mailmh_msg_info * msg_info;
  chashdatum key;
  chashdatum value;
  uint32_t uid;
  struct mh_cached_session_state_data * data;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  char keyname[PATH_MAX];
  
  folder = get_mh_cur_folder(session);
  if (folder == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = mailmh_folder_add_message_uid(folder,
      message, size, &uid);
  
  switch (r) {
  case MAILMH_ERROR_FILE:
    return MAIL_ERROR_DISKSPACE;
    
  case MAILMH_NO_ERROR:
    break;
    
  default:
    return mhdriver_mh_error_to_mail_error(r);
  }
  
  if (flags == NULL)
    goto exit;
  
  key.data = &uid;
  key.len = sizeof(uid);
  r = chash_get(folder->fl_msgs_hash, &key, &value);
  if (r < 0)
    return MAIL_ERROR_CACHE_MISS;
  
  msg_info = value.data;
  
  data = get_cached_data(session);
  
  snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
      data->mh_flags_directory, data->mh_quoted_mb, FLAGS_NAME);
  
  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0)
    goto exit;
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL)
    goto close_db_flags;
  
  snprintf(keyname, PATH_MAX, "%u-%lu-%lu-flags",
      uid, (unsigned long) msg_info->msg_mtime,
      (unsigned long) msg_info->msg_size);
  
  r = mhdriver_write_cached_flags(cache_db_flags, mmapstr, keyname, flags);
  
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

static int mhdriver_cached_copy_message(mailsession * session,
					uint32_t num, const char * mb)
{
  return mailsession_copy_message(get_ancestor(session), num, mb);
}

static int mhdriver_cached_remove_message(mailsession * session, uint32_t num)
{
  return mailsession_remove_message(get_ancestor(session), num);
}

static int mhdriver_cached_move_message(mailsession * session,
					uint32_t num, const char * mb)
{
  return mailsession_move_message(get_ancestor(session), num, mb);
}

static int
mhdriver_cached_get_messages_list(mailsession * session,
				  struct mailmessage_list ** result)
{
  struct mailmh_folder * folder;
  int res;

  folder = get_mh_cur_folder(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  return mh_get_messages_list(folder, session,
      mh_cached_message_driver, result);

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
  struct mailmh_folder * folder;
  struct mailmh_msg_info * msg_info;
  chashdatum key;
  chashdatum data;
  
  folder = get_mh_cur_folder(session);

#if 0
  msg_info = cinthash_find(mh_data->mh_cur_folder->fl_msgs_hash, num);
  if (msg_info == NULL)
    return MAIL_ERROR_CACHE_MISS;
#endif
  key.data = &num;
  key.len = sizeof(num);
  r = chash_get(folder->fl_msgs_hash, &key, &data);
  if (r < 0)
    return MAIL_ERROR_CACHE_MISS;
  msg_info = data.data;
  
  snprintf(keyname, PATH_MAX, "%u-%lu-%lu-envelope",
	   num, (unsigned long) msg_info->msg_mtime,
      (unsigned long) msg_info->msg_size);

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
  struct mailmh_folder * folder;
  chashdatum key;
  chashdatum data;
  struct mailmh_msg_info * msg_info;

  folder = get_mh_cur_folder(session);
#if 0
  msg_info = cinthash_find(mh_data->mh_cur_folder->fl_msgs_hash, num);
  if (msg_info == NULL) {
    res = MAIL_ERROR_CACHE_MISS;
    goto err;
  }
#endif
  key.data = &num;
  key.len = sizeof(num);
  r = chash_get(folder->fl_msgs_hash, &key, &data);
  if (r < 0)
    return MAIL_ERROR_CACHE_MISS;
  msg_info = data.data;

  snprintf(keyname, PATH_MAX, "%u-%lu-%lu-envelope",
	   num, (unsigned long) msg_info->msg_mtime,
      (unsigned long) msg_info->msg_size);

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
mhdriver_cached_get_envelopes_list(mailsession * session,
				   struct mailmessage_list * env_list)
{
  int r;
  unsigned int i;
  char filename_env[PATH_MAX];
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_env;
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  int res;
  struct mh_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);
  if (cached_data->mh_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  mh_flags_store_process(cached_data->mh_flags_directory,
      cached_data->mh_quoted_mb,
      cached_data->mh_flags_store);
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(filename_env, PATH_MAX, "%s/%s/%s",
      cached_data->mh_cache_directory,
      cached_data->mh_quoted_mb, ENV_NAME);
  
  r = mail_cache_db_open_lock(filename_env, &cache_db_env);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }

  snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
      cached_data->mh_flags_directory, cached_data->mh_quoted_mb, FLAGS_NAME);

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
          msg->msg_session, msg->msg_index, &fields);
      if (r == MAIL_NO_ERROR) {
	msg->msg_cached = TRUE;
	msg->msg_fields = fields;
      }
    }
      
    if (msg->msg_flags == NULL) {
      r = mhdriver_get_cached_flags(cache_db_flags, mmapstr,
          session, msg->msg_index, &flags);
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
    res = MAIL_ERROR_MEMORY;
    goto close_db_env;
  }
  
  /* add flags */
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_flags == NULL)
      msg->msg_flags = mail_flags_new_empty();
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
      r = mhdriver_write_cached_flags(cache_db_flags, mmapstr,
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

static int mhdriver_cached_get_message(mailsession * session,
				       uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, mh_cached_message_driver, num, 0);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = msg_info;

  return MAIL_NO_ERROR;
}

static int mhdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result)
{
  uint32_t index;
  char *p;
  struct mailmh_msg_info * mh_msg_info;
  struct mailmh_folder * folder;
  time_t mtime;
  char * mtime_p;
  chashdatum key;
  chashdatum data;
  int r;
  
  if (uid == NULL)
    return MAIL_ERROR_INVAL;

  index = strtoul(uid, &p, 10);
  if (p == uid || * p != '-')
    return MAIL_ERROR_INVAL;
  
  folder = get_mh_cur_folder(session);
  
  mh_msg_info = NULL;
  key.data = &index;
  key.len = sizeof(index);
  r = chash_get(folder->fl_msgs_hash, &key, &data);
  if (r < 0)
    return MAIL_ERROR_MSG_NOT_FOUND;
  
  mh_msg_info = data.data;

  mtime_p = p + 1;

  mtime = strtoul(mtime_p, &p, 10);
  if ((* p == '-') && (mtime == mh_msg_info->msg_mtime)) {
    size_t size;
    char *size_p;
    
    size_p = p + 1;
    size = strtoul(size_p, &p, 10);
    if ((* p == '\0') && (size == mh_msg_info->msg_size))
      return mhdriver_cached_get_message(session, index, result);
  }
  else if (*p != '-') {
    return MAIL_ERROR_INVAL;
  }
  
  return MAIL_ERROR_MSG_NOT_FOUND;
}
