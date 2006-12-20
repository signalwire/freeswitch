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
 * $Id: mboxdriver.c,v 1.43 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mboxdriver.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifndef _MSC_VER
#	include <dirent.h>
#	include <unistd.h>
#endif
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <sys/times.h>
#endif

#include "mail.h"
#include "maildriver_tools.h"
#include "mailmbox.h"
#include "mboxdriver_tools.h"
#include "maildriver.h"
#include "carray.h"
#include "mboxdriver_message.h"
#include "mailmessage.h"

static int mboxdriver_initialize(mailsession * session);

static void mboxdriver_uninitialize(mailsession * session);

static int mboxdriver_parameters(mailsession * session,
				 int id, void * value);

static int mboxdriver_connect_path(mailsession * session, const char * path);

static int mboxdriver_logout(mailsession * session);

static int mboxdriver_expunge_folder(mailsession * session);

static int mboxdriver_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);

static int mboxdriver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result);

static int mboxdriver_append_message(mailsession * session,
				     const char * message, size_t size);

static int mboxdriver_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);

static int mboxdriver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result);

static int
mboxdriver_get_envelopes_list(mailsession * session,
			      struct mailmessage_list * env_list);

static int mboxdriver_remove_message(mailsession * session, uint32_t num);

static int mboxdriver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result);

static int mboxdriver_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result);

static mailsession_driver local_mbox_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "mbox",

  /* sess_initialize */ mboxdriver_initialize,
  /* sess_uninitialize */ mboxdriver_uninitialize,

  /* sess_parameters */ mboxdriver_parameters,

  /* sess_connect_stream */ NULL,

  /* sess_connect_path */ mboxdriver_connect_path,
  /* sess_starttls */ NULL,
  /* sess_login */ NULL,
  /* sess_logout */ mboxdriver_logout,
  /* sess_noop */ NULL,

  /* sess_build_folder_name */ NULL,
  /* sess_create_folder */ NULL,
  /* sess_delete_folder */ NULL,
  /* sess_rename_folder */ NULL,
  /* sess_check_folder */ NULL,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ NULL,
  /* sess_expunge_folder */ mboxdriver_expunge_folder,
  /* sess_status_folder */ mboxdriver_status_folder,
  /* sess_messages_number */ mboxdriver_messages_number,
  /* sess_recent_number */ mboxdriver_messages_number,
  /* sess_unseen_number */ mboxdriver_messages_number,
  /* sess_list_folders */ NULL,
  /* sess_lsub_folders */ NULL,
  /* sess_subscribe_folder */ NULL,
  /* sess_unsubscribe_folder */ NULL,

  /* sess_append_message */ mboxdriver_append_message,
  /* sess_append_message_flags */ mboxdriver_append_message_flags,
  /* sess_copy_message */ NULL,
  /* sess_move_message */ NULL,

  /* sess_get_message */ mboxdriver_get_message,
  /* sess_get_message_by_uid */ mboxdriver_get_message_by_uid,

  /* sess_get_messages_list */ mboxdriver_get_messages_list,
  /* sess_get_envelopes_list */ mboxdriver_get_envelopes_list,
  /* sess_remove_message */ mboxdriver_remove_message,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "mbox",

  .sess_initialize = mboxdriver_initialize,
  .sess_uninitialize = mboxdriver_uninitialize,

  .sess_parameters = mboxdriver_parameters,

  .sess_connect_path = mboxdriver_connect_path,
  .sess_connect_stream = NULL,
  .sess_starttls = NULL,
  .sess_login = NULL,
  .sess_logout = mboxdriver_logout,
  .sess_noop = NULL,

  .sess_build_folder_name = NULL,
  .sess_create_folder = NULL,
  .sess_delete_folder = NULL,
  .sess_rename_folder = NULL,
  .sess_check_folder = NULL,
  .sess_examine_folder = NULL,
  .sess_select_folder = NULL,
  .sess_expunge_folder = mboxdriver_expunge_folder,
  .sess_status_folder = mboxdriver_status_folder,
  .sess_messages_number = mboxdriver_messages_number,
  .sess_recent_number = mboxdriver_messages_number,
  .sess_unseen_number = mboxdriver_messages_number,
  .sess_list_folders = NULL,
  .sess_lsub_folders = NULL,
  .sess_subscribe_folder = NULL,
  .sess_unsubscribe_folder = NULL,

  .sess_append_message = mboxdriver_append_message,
  .sess_append_message_flags = mboxdriver_append_message_flags,
  .sess_copy_message = NULL,
  .sess_move_message = NULL,

  .sess_get_messages_list = mboxdriver_get_messages_list,
  .sess_get_envelopes_list = mboxdriver_get_envelopes_list,
  .sess_remove_message = mboxdriver_remove_message,
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = mboxdriver_get_message,
  .sess_get_message_by_uid = mboxdriver_get_message_by_uid,
  .sess_login_sasl = NULL,
#endif
};

mailsession_driver * mbox_session_driver = &local_mbox_session_driver;

static inline struct mbox_session_state_data * get_data(mailsession * session)
{
  return session->sess_data;
}

static inline struct mailmbox_folder * get_mbox_session(mailsession * session)
{
  return get_data(session)->mbox_folder;
}

static int mboxdriver_initialize(mailsession * session)
{
  struct mbox_session_state_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;

  data->mbox_folder = NULL;

  data->mbox_force_read_only = FALSE;
  data->mbox_force_no_uid = TRUE;

  session->sess_data = data;
  
  return MAIL_NO_ERROR;

 err:
  return MAIL_ERROR_MEMORY;
}

static void free_state(struct mbox_session_state_data * mbox_data)
{
  if (mbox_data->mbox_folder != NULL) {
    mailmbox_done(mbox_data->mbox_folder);
    mbox_data->mbox_folder = NULL;
  }
}

static void mboxdriver_uninitialize(mailsession * session)
{
  struct mbox_session_state_data * data;

  data = get_data(session);

  free_state(data);

  free(data);
}

static int mboxdriver_parameters(mailsession * session,
				 int id, void * value)
{
  struct mbox_session_state_data * data;

  data = get_data(session);

  switch (id) {
  case MBOXDRIVER_SET_READ_ONLY:
    {
      int * param;

      param = value;

      data->mbox_force_read_only = * param;
      return MAIL_NO_ERROR;
    }

  case MBOXDRIVER_SET_NO_UID:
    {
      int * param;

      param = value;

      data->mbox_force_no_uid = * param;
      return MAIL_NO_ERROR;
    }
  }

  return MAIL_ERROR_INVAL;
}


static int mboxdriver_connect_path(mailsession * session, const char * path)
{
  struct mbox_session_state_data * mbox_data;
  struct mailmbox_folder * folder;
  int r;

  mbox_data = get_data(session);

  if (mbox_data->mbox_folder != NULL)
    return MAIL_ERROR_BAD_STATE;

  r = mailmbox_init(path,
		    mbox_data->mbox_force_read_only,
		    mbox_data->mbox_force_no_uid,
		    0,
		    &folder);
  
  if (r != MAILMBOX_NO_ERROR)
    return mboxdriver_mbox_error_to_mail_error(r);

  mbox_data->mbox_folder = folder;

  return MAIL_NO_ERROR;
}

static int mboxdriver_logout(mailsession * session)
{
  struct mbox_session_state_data * mbox_data;

  mbox_data = get_data(session);

  if (mbox_data->mbox_folder == NULL)
    return MAIL_ERROR_BAD_STATE;

  free_state(mbox_data);

  mbox_data->mbox_folder = NULL;

  return MAIL_NO_ERROR;
}

static int mboxdriver_expunge_folder(mailsession * session)
{
  int r;
  struct mbox_session_state_data * mbox_data;

  mbox_data = get_data(session);

  if (mbox_data->mbox_folder == NULL)
    return MAIL_ERROR_BAD_STATE;

  r = mailmbox_expunge(mbox_data->mbox_folder);
  if (r != MAILMBOX_NO_ERROR)
    return mboxdriver_mbox_error_to_mail_error(r);

  return MAIL_NO_ERROR;
}

static int mboxdriver_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  uint32_t count;
  int r; 
  
  r = mboxdriver_messages_number(session, mb, &count);
  if (r != MAIL_NO_ERROR)
    return r; 
  
  * result_messages = count;
  * result_recent = count;
  * result_unseen = count;
  
  return MAIL_NO_ERROR;
}

static int mboxdriver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result)
{
  struct mailmbox_folder * folder;
  int r;

  folder = get_mbox_session(session);
  if (folder == NULL)
    return MAIL_ERROR_STATUS;

  r = mailmbox_validate_read_lock(folder);
  if (r != MAIL_NO_ERROR)
    return r;

  mailmbox_read_unlock(folder);

  * result = carray_count(folder->mb_tab) - folder->mb_deleted_count;
  
  return MAILMBOX_NO_ERROR;
}

/* messages operations */

static int mboxdriver_append_message(mailsession * session,
				     const char * message, size_t size)
{
  int r;
  struct mailmbox_folder * folder;

  folder = get_mbox_session(session);
  if (folder == NULL)
    return MAIL_ERROR_APPEND;

  r = mailmbox_append_message(folder, message, size);

  switch (r) {
  case MAILMBOX_ERROR_FILE:
    return MAIL_ERROR_DISKSPACE;
  default:
    return mboxdriver_mbox_error_to_mail_error(r);
  }
}

static int mboxdriver_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  return mboxdriver_append_message(session, message, size);
}

static int mboxdriver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result)
{
  struct mailmbox_folder * folder;
  int res;

  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  return mbox_get_messages_list(folder, session, mbox_message_driver, result);

 err:
  return res;
}

static int
mboxdriver_get_envelopes_list(mailsession * session,
			      struct mailmessage_list * env_list)
{
  struct mailmbox_folder * folder;
  unsigned int i;
  int r;
  int res;

  folder = get_mbox_session(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  r = mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = mboxdriver_mbox_error_to_mail_error(r);
    goto err;
  }

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    struct mailimf_fields * fields;
    char * headers;
    size_t headers_len;
    size_t cur_token;

    msg = carray_get(env_list->msg_tab, i);
    if (msg == NULL)
      continue;

    if (msg->msg_fields != NULL)
      continue;

    r = mailmbox_fetch_msg_headers_no_lock(folder,
        msg->msg_index, &headers, &headers_len);
    if (r != MAILMBOX_NO_ERROR) {
      res = mboxdriver_mbox_error_to_mail_error(r);
      goto unlock;
    }

    cur_token = 0;
    r = mailimf_envelope_fields_parse(headers, headers_len,
				      &cur_token, &fields);
    
    if (r != MAILIMF_NO_ERROR)
      continue;

    msg->msg_fields = fields;
  }

  mailmbox_read_unlock(folder);

  return MAIL_NO_ERROR;

 unlock:
  mailmbox_read_unlock(folder);
 err:
  return res;
}


static int mboxdriver_remove_message(mailsession * session, uint32_t num)
{
  int r;
  struct mailmbox_folder * folder;

  folder = get_mbox_session(session);
  if (folder == NULL)
    return MAIL_ERROR_DELETE;

  r = mailmbox_delete_msg(folder, num);

  return mboxdriver_mbox_error_to_mail_error(r);
}

static int mboxdriver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, mbox_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

static int mboxdriver_get_message_by_uid(mailsession * session,
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
      return mboxdriver_get_message(session, num, result);
  }

  return MAIL_ERROR_MSG_NOT_FOUND;
}
