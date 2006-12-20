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
 * $Id: maildirdriver.c,v 1.15 2006/06/28 06:13:47 skunk Exp $
 */


/*
  flags directory MUST be kept so that we can have other flags
  than standards
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildirdriver.h"

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

#include "maildir.h"
#include "maildriver_tools.h"
#include "maildirdriver_message.h"
#include "maildirdriver_tools.h"
#include "mailmessage.h"
#include "generic_cache.h"

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

static int get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result);

static mailsession_driver local_maildir_session_driver = {

#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
	
   /* sess_name */ "maildir",

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

  /* sess_get_message */ NULL,
  /* sess_get_message_by_uid */ get_message_by_uid,

  /* sess_get_messages_list */ get_messages_list,
  /* sess_get_envelopes_list */ get_envelopes_list,
  /* sess_remove_message */ NULL,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "maildir",

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
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = NULL,
  .sess_get_message_by_uid = get_message_by_uid,
  .sess_login_sasl = NULL,

#endif
};

mailsession_driver * maildir_session_driver = &local_maildir_session_driver;


static int flags_store_process(struct maildir * md,
    struct mail_flags_store * flags_store);


static inline struct maildir_session_state_data * get_data(mailsession * session)
{
  return session->sess_data;
}

static struct maildir * get_maildir_session(mailsession * session)
{
  return get_data(session)->md_session;
}

static int initialize(mailsession * session)
{
  struct maildir_session_state_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;

  data->md_session = NULL;
  
  data->md_flags_store = mail_flags_store_new();
  if (data->md_flags_store == NULL)
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
  struct maildir_session_state_data * data;
  
  data = get_data(session);

  if (data->md_session != NULL)
    flags_store_process(data->md_session, data->md_flags_store);
  
  mail_flags_store_free(data->md_flags_store);
  if (data->md_session != NULL)
    maildir_free(data->md_session);
  
  free(data);
  
  session->sess_data = NULL;
}


static int connect_path(mailsession * session, const char * path)
{
  struct maildir * md;
  int res;
  int r;
  
  if (get_maildir_session(session) != NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  md = maildir_new(path);
  if (md == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = maildirdriver_maildir_error_to_mail_error(r);
    goto free;
  }

  get_data(session)->md_session = md;
  
  return MAIL_NO_ERROR;
  
 free:
  maildir_free(md);
 err:
  return res;
}

static int logout(mailsession * session)
{
  struct maildir * md;
  
  check_folder(session);
  
  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  maildir_free(md);
  get_data(session)->md_session = NULL;

  return MAIL_NO_ERROR;
}

/* folders operations */

static int status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  int r;
  struct maildir * md;
  unsigned int i;
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;

  check_folder(session);
  
  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR)
    return maildirdriver_maildir_error_to_mail_error(r);
  
  messages = 0;
  recent = 0;
  unseen = 0;
  for(i = 0 ; i < carray_count(md->mdir_msg_list) ; i ++) {
    struct maildir_msg * msg;
    
    msg = carray_get(md->mdir_msg_list, i);
    if ((msg->msg_flags & MAILDIR_FLAG_NEW) != 0)
      recent ++;
    if ((msg->msg_flags & MAILDIR_FLAG_SEEN) == 0)
      unseen ++;
    messages ++;
  }
  
  * result_messages = messages;
  * result_recent = recent;
  * result_unseen = unseen;

  return MAIL_NO_ERROR;
}

static int messages_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  struct maildir * md;
  int r;

  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR)
    return maildirdriver_maildir_error_to_mail_error(r);
  
  * result = carray_count(md->mdir_msg_list);
  
  return MAIL_NO_ERROR;
}

static int unseen_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = unseen;
 
  return MAIL_NO_ERROR;
}

static int recent_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = recent;
 
  return MAIL_NO_ERROR;
}


/* messages operations */

static int append_message(mailsession * session,
    const char * message, size_t size)
{
#if 0
  struct maildir * md;
  int r;

  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = maildir_message_add(md, message, size);
  if (r != MAILDIR_NO_ERROR)
    return maildirdriver_maildir_error_to_mail_error(r);
  
  return MAIL_NO_ERROR;
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
  
  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = maildir_message_add_uid(md, message, size,
      uid, sizeof(uid));
  if (r != MAILDIR_NO_ERROR)
    return maildirdriver_maildir_error_to_mail_error(r);
  
  if (flags == NULL)
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
  
 exit:
  return MAIL_NO_ERROR;
}

static int get_messages_list(mailsession * session,
    struct mailmessage_list ** result)
{
  struct maildir * md;
  int r;
  struct mailmessage_list * env_list;
  int res;
  
  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = maildirdriver_maildir_error_to_mail_error(r);
    goto err;
  }
  
  r = maildir_get_messages_list(session, md,
      maildir_message_driver, &env_list);
  if (r != MAILDIR_NO_ERROR) {
    res = r;
    goto free_list;
  }
  
  * result = env_list;
  
  return MAIL_NO_ERROR;
  
 free_list:
  mailmessage_list_free(env_list);
 err:
  return res;
}

static int get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list)
{
  int r;
  struct maildir * md;
  unsigned int i;
  int res;
  
  check_folder(session);
  
  md = get_maildir_session(session);
  if (md == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = maildirdriver_maildir_error_to_mail_error(r);
    goto err;
  }
  
  r = maildriver_generic_get_envelopes_list(session, env_list);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i++) {
    struct maildir_msg * md_msg;
    mailmessage * msg;
    uint32_t driver_flags;
    clist * ext;
    chashdatum key;
    chashdatum value;
    
    msg = carray_get(env_list->msg_tab, i);
    
    key.data = msg->msg_uid;
    key.len = strlen(msg->msg_uid);
    r = chash_get(md->mdir_msg_hash, &key, &value);
    if (r < 0)
      continue;
    
    md_msg = value.data;
    
    driver_flags = maildirdriver_maildir_flags_to_flags(md_msg->msg_flags);
    
    if (msg->msg_flags == NULL) {
      ext = clist_new();
      if (ext == NULL) {
        res = MAIL_ERROR_MEMORY;
        continue;
      }
      
      msg->msg_flags = mail_flags_new(driver_flags, ext);
      if (msg->msg_flags == NULL) {
        clist_free(ext);
        res = MAIL_ERROR_MEMORY;
        continue;
      }
      
      if ((md_msg->msg_flags & MAILDIR_FLAG_NEW) != 0) {
        mail_flags_store_set(get_data(session)->md_flags_store, msg);
      }
    }
    else {
      msg->msg_flags->fl_flags &= MAIL_FLAG_FORWARDED;
      msg->msg_flags->fl_flags |= driver_flags;
    }
  }
  
  return MAIL_NO_ERROR;
  
 err:
  return res;
}


static int expunge_folder(mailsession * session)
{
  unsigned int i;
  int r;
  int res;
  struct maildir * md;

  check_folder(session);
  
  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = maildirdriver_maildir_error_to_mail_error(r);
    goto err;
  }
  
  for(i = 0 ; i < carray_count(md->mdir_msg_list) ; i++) {
    struct maildir_msg * md_msg;
    
    md_msg = carray_get(md->mdir_msg_list, i);
    
    if ((md_msg->msg_flags & MAILDIR_FLAG_TRASHED) != 0)
      maildir_message_remove(md, md_msg->msg_uid);
  }
  
  return MAIL_NO_ERROR;

 err:
  return res;
}


static int flags_store_process(struct maildir * md,
    struct mail_flags_store * flags_store)
{
  unsigned int i;
  
  if (carray_count(flags_store->fls_tab) == 0)
    return MAIL_NO_ERROR;
  
  for(i = 0 ; i < carray_count(flags_store->fls_tab) ; i ++) {
    mailmessage * msg;
    uint32_t md_flags;
    
    msg = carray_get(flags_store->fls_tab, i);
    md_flags = maildirdriver_flags_to_maildir_flags(msg->msg_flags->fl_flags);
    md_flags &= ~MAILDIR_FLAG_NEW;
    
    maildir_message_change_flags(md, msg->msg_uid, md_flags);
  }
  
  mail_flags_store_clear(flags_store);
  
  return MAIL_NO_ERROR;
}



static int check_folder(mailsession * session)
{
  struct mail_flags_store * flags_store;
  struct maildir_session_state_data * data;  
  struct maildir * md;
  
  md = get_maildir_session(session);
  if (md == NULL)
    return MAIL_ERROR_BAD_STATE;
  
  data = get_data(session);
  flags_store = data->md_flags_store;

  return flags_store_process(md, flags_store);
}

static int get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result)
{
  int r;
  struct maildir * md;
  int res;
  mailmessage * msg;
  char * msg_filename;
  struct stat stat_info;
  
  md = get_maildir_session(session);
  
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
  
  r = mailmessage_init(msg, session, maildir_message_driver,
      0, stat_info.st_size);
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

 err:
  return res;
}
