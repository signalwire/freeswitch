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
 * $Id: nntpdriver_cached.c,v 1.53 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "nntpdriver_cached.h"

#include "libetpan-config.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <stdlib.h>
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif

#include "mail_cache_db.h"

#include "mail.h"
#include "mailmessage.h"
#include "maildriver_tools.h"
#include "nntpdriver.h"
#include "maildriver.h"
#include "newsnntp.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "maillock.h"
#include "nntpdriver_cached_message.h"
#include "nntpdriver_tools.h"

static int nntpdriver_cached_initialize(mailsession * session);

static void nntpdriver_cached_uninitialize(mailsession * session);

static int nntpdriver_cached_parameters(mailsession * session,
					int id, void * value);

static int nntpdriver_cached_connect_stream(mailsession * session,
					    mailstream * s);

static int nntpdriver_cached_login(mailsession * session,
				   const char * userid, const char * password);

static int nntpdriver_cached_logout(mailsession * session);

static int nntpdriver_cached_check_folder(mailsession * session);

static int nntpdriver_cached_select_folder(mailsession * session, const char * mb);

static int nntpdriver_cached_status_folder(mailsession * session,
					   const char * mb,
					   uint32_t * result_messages,
					   uint32_t * result_recent,
					   uint32_t * result_unseen);

static int nntpdriver_cached_messages_number(mailsession * session, const char * mb,
					     uint32_t * result);

static int nntpdriver_cached_recent_number(mailsession * session, const char * mb,
					   uint32_t * result);

static int nntpdriver_cached_unseen_number(mailsession * session, const char * mb,
					   uint32_t * result);

static int nntpdriver_cached_append_message(mailsession * session,
					    const char * message, size_t size);

static int nntpdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);

static int
nntpdriver_cached_get_envelopes_list(mailsession * session,
				     struct mailmessage_list * env_list);


static int
nntpdriver_cached_get_messages_list(mailsession * session,
				    struct mailmessage_list ** result);

static int nntpdriver_cached_list_folders(mailsession * session, const char * mb,
					  struct mail_list ** result);

static int nntpdriver_cached_lsub_folders(mailsession * session, const char * mb,
					  struct mail_list ** result);

static int nntpdriver_cached_subscribe_folder(mailsession * session,
					      const char * mb);

static int nntpdriver_cached_unsubscribe_folder(mailsession * session,
						const char * mb);

static int nntpdriver_cached_get_message(mailsession * session,
					 uint32_t num, mailmessage ** result);

static int nntpdriver_cached_noop(mailsession * session);

static int nntpdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result);

static mailsession_driver local_nntp_cached_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "nntp-cached",

  /* sess_initialize */ nntpdriver_cached_initialize,
  /* sess_uninitialize */ nntpdriver_cached_uninitialize,

  /* sess_parameters */ nntpdriver_cached_parameters,

  /* sess_connect_stream */ nntpdriver_cached_connect_stream,
  /* sess_connect_path */ NULL,
  /* sess_starttls */ NULL,
  /* sess_login */ nntpdriver_cached_login,
  /* sess_logout */ nntpdriver_cached_logout,
  /* sess_noop */ nntpdriver_cached_noop,

  /* sess_build_folder_name */ NULL,
  /* sess_create_folder */ NULL,
  /* sess_delete_folder */ NULL,
  /* sess_rename_folder */ NULL,
  /* sess_check_folder */ nntpdriver_cached_check_folder,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ nntpdriver_cached_select_folder,
  /* sess_expunge_folder */ NULL,
  /* sess_status_folder */ nntpdriver_cached_status_folder,
  /* sess_messages_number */ nntpdriver_cached_messages_number,
  /* sess_recent_number */ nntpdriver_cached_recent_number,
  /* sess_unseen_number */ nntpdriver_cached_unseen_number,
  /* sess_list_folders */ nntpdriver_cached_list_folders,
  /* sess_lsub_folders */ nntpdriver_cached_lsub_folders,
  /* sess_subscribe_folder */ nntpdriver_cached_subscribe_folder,
  /* sess_unsubscribe_folder */ nntpdriver_cached_unsubscribe_folder,

  /* sess_append_message */ nntpdriver_cached_append_message,
  /* sess_append_message_flags */ nntpdriver_cached_append_message_flags,
  /* sess_copy_message */ NULL,
  /* sess_move_message */ NULL,

  /* sess_get_message */ nntpdriver_cached_get_message,
  /* sess_get_message_by_uid */ nntpdriver_cached_get_message_by_uid,

  /* sess_get_messages_list */ nntpdriver_cached_get_messages_list,
  /* sess_get_envelopes_list */ nntpdriver_cached_get_envelopes_list,
  /* sess_remove_message */ NULL,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "nntp-cached",

  .sess_initialize = nntpdriver_cached_initialize,
  .sess_uninitialize = nntpdriver_cached_uninitialize,

  .sess_parameters = nntpdriver_cached_parameters,

  .sess_connect_stream = nntpdriver_cached_connect_stream,
  .sess_connect_path = NULL,
  .sess_starttls = NULL,
  .sess_login = nntpdriver_cached_login,
  .sess_logout = nntpdriver_cached_logout,
  .sess_noop = nntpdriver_cached_noop,

  .sess_build_folder_name = NULL,
  .sess_create_folder = NULL,
  .sess_delete_folder = NULL,
  .sess_rename_folder = NULL,
  .sess_check_folder = nntpdriver_cached_check_folder,
  .sess_examine_folder = NULL,
  .sess_select_folder = nntpdriver_cached_select_folder,
  .sess_expunge_folder = NULL,
  .sess_status_folder = nntpdriver_cached_status_folder,
  .sess_messages_number = nntpdriver_cached_messages_number,
  .sess_recent_number = nntpdriver_cached_recent_number,
  .sess_unseen_number = nntpdriver_cached_unseen_number,
  .sess_list_folders = nntpdriver_cached_list_folders,
  .sess_lsub_folders = nntpdriver_cached_lsub_folders,
  .sess_subscribe_folder = nntpdriver_cached_subscribe_folder,
  .sess_unsubscribe_folder = nntpdriver_cached_unsubscribe_folder,

  .sess_append_message = nntpdriver_cached_append_message,
  .sess_append_message_flags = nntpdriver_cached_append_message_flags,
  .sess_copy_message = NULL,
  .sess_move_message = NULL,

  .sess_get_messages_list = nntpdriver_cached_get_messages_list,
  .sess_get_envelopes_list = nntpdriver_cached_get_envelopes_list,
  .sess_remove_message = NULL,
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = nntpdriver_cached_get_message,
  .sess_get_message_by_uid = nntpdriver_cached_get_message_by_uid,
  .sess_login_sasl = NULL,
#endif
};


mailsession_driver * nntp_cached_session_driver =
&local_nntp_cached_session_driver;

#define ENV_NAME "env.db"
#define FLAGS_NAME "flags.db"



static void read_article_seq(mailsession * session,
			     uint32_t * pfirst, uint32_t * plast);

static void write_article_seq(mailsession * session,
			      uint32_t first,  uint32_t last);


static inline struct nntp_cached_session_state_data *
get_cached_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession * get_ancestor(mailsession * session)
{
  return get_cached_data(session)->nntp_ancestor;
}

static inline struct nntp_session_state_data *
get_ancestor_data(mailsession * session)
{
  return get_ancestor(session)->sess_data;
}

static inline newsnntp * get_nntp_session(mailsession * session)
{
  return get_ancestor_data(session)->nntp_session;
}

static int nntpdriver_cached_initialize(mailsession * session)
{
  struct nntp_cached_session_state_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;

  data->nntp_flags_store = mail_flags_store_new();
  if (data->nntp_flags_store == NULL)
    goto free;

  data->nntp_ancestor = mailsession_new(nntp_session_driver);
  if (data->nntp_ancestor == NULL)
    goto free_store;

  session->sess_data = data;

  return MAIL_NO_ERROR;

 free_store:
  mail_flags_store_free(data->nntp_flags_store);
 free:
  free(data);
 err:
  return MAIL_ERROR_MEMORY;
}

static int nntp_flags_store_process(char * flags_directory, char * group_name,
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

  if (group_name == NULL)
    return MAIL_NO_ERROR;

  snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
	   flags_directory, group_name, FLAGS_NAME);

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

    r = nntpdriver_write_cached_flags(cache_db_flags, mmapstr,
        msg->msg_index, msg->msg_flags);
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

static void nntpdriver_cached_uninitialize(mailsession * session)
{
  struct nntp_cached_session_state_data * cached_data;
  struct nntp_session_state_data * ancestor_data;

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  nntp_flags_store_process(cached_data->nntp_flags_directory,
      ancestor_data->nntp_group_name,
      cached_data->nntp_flags_store);

  mail_flags_store_free(cached_data->nntp_flags_store); 

  mailsession_free(cached_data->nntp_ancestor);
  free(cached_data);
  
  session->sess_data = NULL;
}

static int nntpdriver_cached_parameters(mailsession * session,
					int id, void * value)
{
  struct nntp_cached_session_state_data * cached_data;
  int r;

  cached_data = get_cached_data(session);

  switch (id) {
  case NNTPDRIVER_CACHED_SET_CACHE_DIRECTORY:
    strncpy(cached_data->nntp_cache_directory, value, PATH_MAX);
    cached_data->nntp_cache_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(cached_data->nntp_cache_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  case NNTPDRIVER_CACHED_SET_FLAGS_DIRECTORY:
    strncpy(cached_data->nntp_flags_directory, value, PATH_MAX);
    cached_data->nntp_flags_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(cached_data->nntp_flags_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;

  default:
    return mailsession_parameters(get_ancestor(session), id, value);
  }
}

static int nntpdriver_cached_connect_stream(mailsession * session,
					    mailstream * s)
{
  return mailsession_connect_stream(get_ancestor(session), s);
}

static int nntpdriver_cached_login(mailsession * session,
				   const char * userid, const char * password)
{
  return mailsession_login(get_ancestor(session), userid, password);
}

static int nntpdriver_cached_logout(mailsession * session)
{
  struct nntp_cached_session_state_data * cached_data;
  struct nntp_session_state_data * ancestor_data;

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  nntp_flags_store_process(cached_data->nntp_flags_directory,
      ancestor_data->nntp_group_name,
      cached_data->nntp_flags_store);

  return mailsession_logout(get_ancestor(session));
}

static int nntpdriver_cached_select_folder(mailsession * session, const char * mb)
{
  int r;
  struct nntp_session_state_data * ancestor_data;
  struct nntp_cached_session_state_data * cached_data;
  int res;
  char key[PATH_MAX];

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  nntp_flags_store_process(cached_data->nntp_flags_directory,
      ancestor_data->nntp_group_name,
      cached_data->nntp_flags_store);

  r = mailsession_select_folder(get_ancestor(session), mb);
  if (r != MAIL_NO_ERROR)
    return r;

  if (ancestor_data->nntp_group_name == NULL)
    return MAIL_ERROR_BAD_STATE;

  snprintf(key, PATH_MAX, "%s/%s", cached_data->nntp_cache_directory,
      ancestor_data->nntp_group_name);

  r = generic_cache_create_dir(key);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  snprintf(key, PATH_MAX, "%s/%s", cached_data->nntp_flags_directory,
      ancestor_data->nntp_group_name);

  r = generic_cache_create_dir(key);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}

static int nntpdriver_cached_check_folder(mailsession * session)
{
  struct nntp_session_state_data * ancestor_data;
  struct nntp_cached_session_state_data * cached_data;

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  nntp_flags_store_process(cached_data->nntp_flags_directory,
      ancestor_data->nntp_group_name,
      cached_data->nntp_flags_store);

  return MAIL_NO_ERROR;
}


static int nntpdriver_cached_status_folder(mailsession * session,
    const char * mb, uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  int res;
  struct nntp_cached_session_state_data * cached_data;
  struct nntp_session_state_data * ancestor_data;
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  uint32_t i;
  int r;
  uint32_t recent;
  uint32_t unseen;
  uint32_t first;
  uint32_t last;
  uint32_t count;
  uint32_t additionnal;

  r = nntpdriver_cached_select_folder(session, mb);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  read_article_seq(session, &first, &last);

  count = 0;
  recent = 0;
  unseen = 0;
  
  ancestor_data = get_ancestor_data(session);
  cached_data = get_cached_data(session);
  if (ancestor_data->nntp_group_name == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  if (ancestor_data->nntp_group_info->grp_first > first)
    first = ancestor_data->nntp_group_info->grp_first;
  if (last < first)
    last = ancestor_data->nntp_group_info->grp_last;

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

  for(i = first ; i <= last ; i++) {
    struct mail_flags * flags;
    
    r = nntpdriver_get_cached_flags(cache_db_flags, mmapstr,
        i, &flags);
    if (r == MAIL_NO_ERROR) {
      if ((flags->fl_flags & MAIL_FLAG_CANCELLED) != 0) {
        mail_flags_free(flags);
        continue;
      }
      
      count ++;
      if ((flags->fl_flags & MAIL_FLAG_NEW) != 0) {
	recent ++;
      }
      if ((flags->fl_flags & MAIL_FLAG_SEEN) == 0) {
	unseen ++;
      }
      mail_flags_free(flags);
    }
  }

  if ((count == 0) && (first != last)) {
    count = last - first + 1;
    recent = count;
    unseen = count;
  }
  
  additionnal = ancestor_data->nntp_group_info->grp_last - last;
  recent += additionnal;
  unseen += additionnal;
  
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

static int nntpdriver_cached_messages_number(mailsession * session,
					     const char * mb,
					     uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = nntpdriver_cached_status_folder(session, mb,
      &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = messages;
  
  return MAIL_NO_ERROR;  
}

static int nntpdriver_cached_recent_number(mailsession * session,
					   const char * mb,
					   uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = nntpdriver_cached_status_folder(session, mb,
      &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = recent;
  
  return MAIL_NO_ERROR;  
}

static int nntpdriver_cached_unseen_number(mailsession * session,
					   const char * mb,
					   uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = nntpdriver_cached_status_folder(session, mb,
      &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = unseen;
  
  return MAIL_NO_ERROR;  
}

static int nntpdriver_cached_list_folders(mailsession * session, const char * mb,
					  struct mail_list ** result)
{
  return mailsession_list_folders(get_ancestor(session), mb, result);
}

static int nntpdriver_cached_lsub_folders(mailsession * session, const char * mb,
					  struct mail_list ** result)
{
  return mailsession_lsub_folders(get_ancestor(session), mb, result);
}

static int nntpdriver_cached_subscribe_folder(mailsession * session,
					      const char * mb)
{
  return mailsession_subscribe_folder(get_ancestor(session), mb);
}

static int nntpdriver_cached_unsubscribe_folder(mailsession * session,
						const char * mb)
{
  return mailsession_unsubscribe_folder(get_ancestor(session), mb);
}



/* messages operations */

static int nntpdriver_cached_append_message(mailsession * session,
					    const char * message, size_t size)
{
  return mailsession_append_message(get_ancestor(session), message, size);
}

static int nntpdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  return nntpdriver_cached_append_message(session, message, size);
}



static int
get_cached_envelope(struct mail_cache_db * cache_db, MMAPString * mmapstr,
    mailsession * session, uint32_t num,
    struct mailimf_fields ** result)
{
  char keyname[PATH_MAX];
  int r;
  struct mailimf_fields * fields;
  int res;

  snprintf(keyname, PATH_MAX, "%i-envelope", num);

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
  int res;
  char keyname[PATH_MAX];

  snprintf(keyname, PATH_MAX, "%i-envelope", num);

  r = generic_cache_fields_write(cache_db, mmapstr, keyname, fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  return MAIL_NO_ERROR;

 err:
  return res;
}

#define SEQ_FILENAME "articles-seq"

static void read_article_seq(mailsession * session,
			     uint32_t * pfirst, uint32_t * plast)
{
  FILE * f;
  struct nntp_session_state_data * ancestor_data;
  uint32_t first;
  uint32_t last;
  char seq_filename[PATH_MAX];
  struct nntp_cached_session_state_data * cached_data;
  int r;

  first = 0;
  last = 0;

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  if (ancestor_data->nntp_group_name == NULL)
    return;

  snprintf(seq_filename, PATH_MAX, "%s/%s/%s",
      cached_data->nntp_cache_directory,
      ancestor_data->nntp_group_name, SEQ_FILENAME);
  f = fopen(seq_filename, "r");

  if (f != NULL) {
    int fd;

    fd = fileno(f);
    
    r = maillock_read_lock(seq_filename, fd);
    if (r == 0) {
      MMAPString * mmapstr;
      size_t cur_token;
      char buf[sizeof(uint32_t) * 2];
      size_t read_size;
      
      read_size = fread(buf, 1, sizeof(uint32_t) * 2, f);
      mmapstr = mmap_string_new_len(buf, read_size);
      if (mmapstr != NULL) {
	cur_token = 0;
	r = mailimf_cache_int_read(mmapstr, &cur_token, &first);
	r = mailimf_cache_int_read(mmapstr, &cur_token, &last);
	
	mmap_string_free(mmapstr);
      }
      
      maillock_read_unlock(seq_filename, fd);
    }
    fclose(f);
  }

  * pfirst = first;
  * plast = last;
}

static void write_article_seq(mailsession * session,
			      uint32_t first,  uint32_t last)
{
  FILE * f;
  struct nntp_session_state_data * ancestor_data;
  char seq_filename[PATH_MAX];
  struct nntp_cached_session_state_data * cached_data;
  int r;
  int fd;

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  if (ancestor_data->nntp_group_name == NULL)
    return;

  snprintf(seq_filename, PATH_MAX, "%s/%s/%s",
      cached_data->nntp_cache_directory,
      ancestor_data->nntp_group_name, SEQ_FILENAME);

  fd = creat(seq_filename, S_IRUSR | S_IWUSR);
  if (fd < 0)
    return;
  
  f = fdopen(fd, "w");
  if (f != NULL) {
    r = maillock_write_lock(seq_filename, fd);
    if (r == 0) {
      MMAPString * mmapstr;
      size_t cur_token;

      mmapstr = mmap_string_new("");
      if (mmapstr != NULL) {
	r = mail_serialize_clear(mmapstr, &cur_token);
	if (r == MAIL_NO_ERROR) {
	  r = mailimf_cache_int_write(mmapstr, &cur_token, first);
	  r = mailimf_cache_int_write(mmapstr, &cur_token, last);
	  
	  fwrite(mmapstr->str, 1, mmapstr->len, f);
	}

	mmap_string_free(mmapstr);
      }
	  
      maillock_write_unlock(seq_filename, fd);
    }
    fclose(f);
  }
  else
    close(fd);
}


static void get_uid_from_filename(char * filename)
{
  char * p;
  
  if (strcmp(filename, SEQ_FILENAME) == 0)
    * filename = 0;
  
  p = strstr(filename, "-header");
  if (p != NULL)
    * p = 0;
}  

static int
nntpdriver_cached_get_envelopes_list(mailsession * session,
				     struct mailmessage_list * env_list)
{
  int r;
  unsigned int i;
  struct nntp_cached_session_state_data * cached_data;
  uint32_t first;
  uint32_t last;
  struct nntp_session_state_data * ancestor_data;
  char filename_env[PATH_MAX];
  char filename_flags[PATH_MAX];
  struct mail_cache_db * cache_db_env;
  struct mail_cache_db * cache_db_flags;
  MMAPString * mmapstr;
  int res;
  char cache_dir[PATH_MAX];

  cached_data = get_cached_data(session);
  ancestor_data = get_ancestor_data(session);

  nntp_flags_store_process(cached_data->nntp_flags_directory,
      ancestor_data->nntp_group_name,
      cached_data->nntp_flags_store);

  if (ancestor_data->nntp_group_name == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  /* read articles sequence */

  read_article_seq(session, &first, &last);

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  snprintf(filename_env, PATH_MAX, "%s/%s/%s",
      cached_data->nntp_cache_directory,
      ancestor_data->nntp_group_name, ENV_NAME);

  r = mail_cache_db_open_lock(filename_env, &cache_db_env);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }

  snprintf(filename_flags, PATH_MAX, "%s/%s/%s",
      cached_data->nntp_flags_directory,
      ancestor_data->nntp_group_name, FLAGS_NAME);

  /* fill with cached */
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {   
    mailmessage * msg;
    struct mailimf_fields * fields;

    msg = carray_get(env_list->msg_tab, i);

    if ((msg->msg_index < first) || (msg->msg_index > last))
      continue;

    if (msg->msg_fields == NULL) {
      r = get_cached_envelope(cache_db_env, mmapstr,
          session, msg->msg_index, &fields);
      if (r == MAIL_NO_ERROR) {
        msg->msg_fields = fields;
        msg->msg_cached = TRUE;
      }
    }
  }
  
  mail_cache_db_close_unlock(filename_env, cache_db_env);

  r = mailsession_get_envelopes_list(get_ancestor(session), env_list);

  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }

  r = mail_cache_db_open_lock(filename_flags, &cache_db_flags);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }
  
  /* add flags */
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);
    
    if (msg->msg_flags == NULL) {
      struct mail_flags * flags;
      
      r = nntpdriver_get_cached_flags(cache_db_flags, mmapstr,
          msg->msg_index, &flags);
      if (r == MAIL_NO_ERROR) {
	msg->msg_flags = flags;
      }
      else {
        msg->msg_flags = mail_flags_new_empty();
        if (msg->msg_fields == NULL) {
          msg->msg_flags->fl_flags |= MAIL_FLAG_CANCELLED;
          mailmessage_check(msg);
        }
      }
    }
  }
  
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  
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
      r = nntpdriver_write_cached_flags(cache_db_flags, mmapstr,
          msg->msg_index, msg->msg_flags);
    }
  }

  first = 0;
  last = 0;
  if (carray_count(env_list->msg_tab) > 0) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, 0);
    first = msg->msg_index;

    msg = carray_get(env_list->msg_tab, carray_count(env_list->msg_tab) - 1);
    last = msg->msg_index;
  }

  /* write articles sequence */

  write_article_seq(session, first, last);

  /* flush cache */
  
  maildriver_cache_clean_up(cache_db_env, cache_db_flags, env_list);
  
  /* remove cache files */
  
  snprintf(cache_dir, PATH_MAX, "%s/%s",
      cached_data->nntp_cache_directory, ancestor_data->nntp_group_name);
  
  mail_cache_db_close_unlock(filename_flags, cache_db_flags);
  mail_cache_db_close_unlock(filename_env, cache_db_env);
  mmap_string_free(mmapstr);

  maildriver_message_cache_clean_up(cache_dir, env_list,
      get_uid_from_filename);

  return MAIL_NO_ERROR;

 close_db_env:
  mail_cache_db_close_unlock(filename_env, cache_db_env);
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int
nntpdriver_cached_get_messages_list(mailsession * session,
				    struct mailmessage_list ** result)
{
  return nntp_get_messages_list(get_ancestor(session), session,
      nntp_cached_message_driver, result);
}

static int nntpdriver_cached_get_message(mailsession * session,
					 uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, nntp_cached_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

static int nntpdriver_cached_noop(mailsession * session)
{
  return mailsession_noop(get_ancestor(session));
}

static int nntpdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result)
{
  uint32_t num;
  char * p;
  
  if (uid == NULL)
    return MAIL_ERROR_INVAL;
  
  num = strtoul(uid, &p, 10);
  if ((p == uid) || (* p != '\0'))
    return MAIL_ERROR_INVAL;
  
  return nntpdriver_cached_get_message(session, num, result);
}
