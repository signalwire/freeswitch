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
 * $Id: mboxdriver_tools.c,v 1.16 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mboxdriver_tools.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#include "maildriver_types.h"
#include "mailmbox.h"
#include "mboxdriver_cached.h"
#include "mboxdriver.h"
#include "generic_cache.h"
#include "mailmessage.h"
#include "imfcache.h"
#include "mail_cache_db.h"

static inline struct mbox_session_state_data *
session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline struct mailmbox_folder *
session_get_mbox_session(mailsession * session)
{
  return session_get_data(session)->mbox_folder;
}

static inline struct mbox_cached_session_state_data *
cached_session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession *
cached_session_get_ancestor(mailsession * session)
{
  return cached_session_get_data(session)->mbox_ancestor;
}

static inline struct mbox_session_state_data *
cached_session_get_ancestor_data(mailsession * session)
{
  return cached_session_get_ancestor(session)->sess_data;
}

static inline struct mailmbox_folder *
cached_session_get_mbox_session(mailsession * session)
{
  return session_get_mbox_session(cached_session_get_ancestor(session));
}


int mboxdriver_mbox_error_to_mail_error(int error)
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

int mboxdriver_fetch_msg(mailsession * session, uint32_t index,
			 char ** result, size_t * result_len)
{
  int r;
  char * msg_content;
  size_t msg_length;
  struct mailmbox_folder * folder;

  folder = session_get_mbox_session(session);
  if (folder == NULL)
    return MAIL_ERROR_BAD_STATE;

  r = mailmbox_fetch_msg(folder, index, &msg_content, &msg_length);
  if (r != MAILMBOX_NO_ERROR)
    return mboxdriver_mbox_error_to_mail_error(r);

  * result = msg_content;
  * result_len = msg_length;

  return MAIL_NO_ERROR;
}


int mboxdriver_fetch_size(mailsession * session, uint32_t index,
			  size_t * result)
{
  struct mailmbox_folder * folder;
  int r;
  char * data;
  size_t len;
  int res;

  folder = session_get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_FETCH;
    goto err;
  }

  r = mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = mboxdriver_mbox_error_to_mail_error(r);
    goto err;
  }

  r = mailmbox_fetch_msg_no_lock(folder, index, &data, &len);
  if (r != MAILMBOX_NO_ERROR) {
    res = mboxdriver_mbox_error_to_mail_error(r);
    goto unlock;
  }

  mailmbox_read_unlock(folder);

  * result = len;

  return MAIL_NO_ERROR;

 unlock:
  mailmbox_read_unlock(folder);
 err:
  return res;
}

int
mboxdriver_get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session,
    uint32_t num,
    struct mail_flags ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mail_flags * flags;
  int res;
  struct mailmbox_msg_info * info;
  struct mailmbox_folder * folder;
  chashdatum key;
  chashdatum data;

  folder = cached_session_get_mbox_session(session);
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

  snprintf(keyname, PATH_MAX, "%u-%lu-flags", num,
      (unsigned long) info->msg_body_len);

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

int
mboxdriver_write_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * uid,
    struct mail_flags * flags)
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

int mboxdriver_fetch_header(mailsession * session, uint32_t index,
			    char ** result, size_t * result_len)
{
  int r;
  char * msg_content;
  size_t msg_length;
  struct mailmbox_folder * folder;

  folder = session_get_mbox_session(session);
  if (folder == NULL)
    return MAIL_ERROR_BAD_STATE;

  r = mailmbox_fetch_msg_headers(folder, index, &msg_content, &msg_length);
  if (r != MAILMBOX_NO_ERROR)
    return mboxdriver_mbox_error_to_mail_error(r);

  * result = msg_content;
  * result_len = msg_length;

  return MAIL_NO_ERROR;
}

int mbox_get_locked_messages_list(struct mailmbox_folder * folder,
    mailsession * session, 
    mailmessage_driver * driver,
    int (* lock)(struct mailmbox_folder *),
    int (* unlock)(struct mailmbox_folder *),
    struct mailmessage_list ** result)
{
  struct mailmessage_list * env_list;
  unsigned int i;
  int r;
  int res;
  carray * tab;

  tab = carray_new(128);
  if (tab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = lock(folder);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  for(i = 0 ; i < carray_count(folder->mb_tab) ; i ++) {
    struct mailmbox_msg_info * msg_info;
    mailmessage * msg;

    msg_info = carray_get(folder->mb_tab, i);
    if (msg_info == NULL)
      continue;

    if (msg_info->msg_deleted)
      continue;

    msg = mailmessage_new();
    if (msg == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto unlock;
    }

    r = mailmessage_init(msg, session, driver, msg_info->msg_uid,
        msg_info->msg_size - msg_info->msg_start_len);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto unlock;
    }

    r = carray_add(tab, msg, NULL);
    if (r < 0) {
      mailmessage_free(msg);
      res = MAIL_ERROR_MEMORY;
      goto unlock;
    }
  }
  
  env_list = mailmessage_list_new(tab);
  if (env_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto unlock;
  }

  unlock(folder);

  * result = env_list;

  return MAIL_NO_ERROR;

 unlock:
  unlock(folder);
 free:
  for(i = 0 ; i < carray_count(tab) ; i ++)
    mailmessage_free(carray_get(tab, i));
  carray_free(tab);
 err:
  return res;
}

static int release_read_mbox(struct mailmbox_folder * folder)
{
  int r;
  
  r = mailmbox_read_unlock(folder);
  return mboxdriver_mbox_error_to_mail_error(r);
}

static int acquire_read_mbox(struct mailmbox_folder * folder)
{
  int r;

  r = mailmbox_validate_read_lock(folder);
  return mboxdriver_mbox_error_to_mail_error(r);
}

static int release_write_mbox(struct mailmbox_folder * folder)
{
  int r;
  
  r = mailmbox_write_unlock(folder);
  return mboxdriver_mbox_error_to_mail_error(r);
}

static int acquire_write_mbox(struct mailmbox_folder * folder)
{
  int r;
  int res;

  r = mailmbox_validate_write_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = mboxdriver_mbox_error_to_mail_error(r);
    goto err;
  }
  
  if (folder->mb_written_uid < folder->mb_max_uid) {
    r = mailmbox_expunge_no_lock(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = mboxdriver_mbox_error_to_mail_error(r);
      goto unlock;
    }
  }
  
  return MAIL_NO_ERROR;

 unlock:
  mailmbox_write_unlock(folder);
 err:
  return res;
}

/* get message list with all valid written UID */

int mbox_get_uid_messages_list(struct mailmbox_folder * folder,
    mailsession * session, 
    mailmessage_driver * driver,
    struct mailmessage_list ** result)
{
  return mbox_get_locked_messages_list(folder, session, driver,
      acquire_write_mbox, release_write_mbox, result);
}


/* get message list */

int mbox_get_messages_list(struct mailmbox_folder * folder,
    mailsession * session, 
    mailmessage_driver * driver,
    struct mailmessage_list ** result)
{
  return mbox_get_locked_messages_list(folder, session, driver,
      acquire_read_mbox, release_read_mbox, result);
}
