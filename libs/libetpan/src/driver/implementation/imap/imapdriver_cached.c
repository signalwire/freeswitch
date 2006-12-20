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
 * $Id: imapdriver_cached.c,v 1.55 2006/08/05 02:34:06 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imapdriver_cached.h"

#include "libetpan-config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <stdlib.h>

#include "mail.h"
#include "imapdriver_tools.h"
#include "mail_cache_db.h"
#include "mailmessage.h"
#include "imapdriver_cached_message.h"
#include "maildriver.h"
#include "imapdriver_types.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "maildriver_tools.h"
#include "imapdriver.h"

static int imapdriver_cached_initialize(mailsession * session);
static void imapdriver_cached_uninitialize(mailsession * session);

static int imapdriver_cached_parameters(mailsession * session,
					int id, void * value);

static int imapdriver_cached_connect_stream(mailsession * session,
					    mailstream * s);

static int imapdriver_cached_starttls(mailsession * session);

static int imapdriver_cached_login(mailsession * session,
				   const char * userid, const char * password);
static int imapdriver_cached_logout(mailsession * session);
static int imapdriver_cached_noop(mailsession * session);
static int imapdriver_cached_build_folder_name(mailsession * session,
					       const char * mb,
					       const char * name, char ** result);
static int imapdriver_cached_create_folder(mailsession * session, const char * mb);
static int imapdriver_cached_delete_folder(mailsession * session, const char * mb);
static int imapdriver_cached_rename_folder(mailsession * session, const char * mb,
					   const char * new_name);
static int imapdriver_cached_check_folder(mailsession * session);
static int imapdriver_cached_examine_folder(mailsession * session,
					    const char * mb);
static int imapdriver_cached_select_folder(mailsession * session, const char * mb);
static int imapdriver_cached_expunge_folder(mailsession * session);
static int imapdriver_cached_status_folder(mailsession * session, const char * mb,
					   uint32_t * result_messages,
					   uint32_t * result_recent,
					   uint32_t * result_unseen);
static int imapdriver_cached_messages_number(mailsession * session,
					     const char * mb,
					     uint32_t * result);
static int imapdriver_cached_recent_number(mailsession * session, const char * mb,
					   uint32_t * result);
static int imapdriver_cached_unseen_number(mailsession * session, const char * mb,
					   uint32_t * result);
static int imapdriver_cached_list_folders(mailsession * session, const char * mb,
					  struct mail_list ** result);
static int imapdriver_cached_lsub_folders(mailsession * session, const char * mb,
					  struct mail_list ** result);
static int imapdriver_cached_subscribe_folder(mailsession * session,
					      const char * mb);
static int imapdriver_cached_unsubscribe_folder(mailsession * session,
						const char * mb);
static int imapdriver_cached_append_message(mailsession * session,
					    const char * message, size_t size);
static int imapdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);
static int imapdriver_cached_copy_message(mailsession * session,
					  uint32_t num, const char * mb);

static int imapdriver_cached_get_messages_list(mailsession * session,
						struct mailmessage_list **
						result);
static int
imapdriver_cached_get_envelopes_list(mailsession * session,
				     struct mailmessage_list * env_list);
static int imapdriver_cached_remove_message(mailsession * session,
					    uint32_t num);

#if 0
static int imapdriver_cached_search_messages(mailsession * session,
					      const char * charset,
					      struct mail_search_key * key,
					      struct mail_search_result **
					      result);
#endif

static int imapdriver_cached_get_message(mailsession * session,
					 uint32_t num, mailmessage ** result);

static int imapdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result);

static int imapdriver_cached_login_sasl(mailsession * session,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);

static mailsession_driver local_imap_cached_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "imap-cached",

  /* sess_initialize */ imapdriver_cached_initialize,
  /* sess_uninitialize */ imapdriver_cached_uninitialize,

  /* sess_parameters */ imapdriver_cached_parameters,

  /* sess_connect_stream */ imapdriver_cached_connect_stream,
  /* sess_connect_path */ NULL,
  /* sess_starttls */ imapdriver_cached_starttls,
  /* sess_login */ imapdriver_cached_login,
  /* sess_logout */ imapdriver_cached_logout,
  /* sess_noop */ imapdriver_cached_noop,

  /* sess_build_folder_name */ imapdriver_cached_build_folder_name,
  /* sess_create_folder */ imapdriver_cached_create_folder,
  /* sess_delete_folder */ imapdriver_cached_delete_folder,
  /* sess_rename_folder */ imapdriver_cached_rename_folder,
  /* sess_check_folder */ imapdriver_cached_check_folder,
  /* sess_examine_folder */ imapdriver_cached_examine_folder,
  /* sess_select_folder */ imapdriver_cached_select_folder,
  /* sess_expunge_folder */ imapdriver_cached_expunge_folder,
  /* sess_status_folder */ imapdriver_cached_status_folder,
  /* sess_messages_number */ imapdriver_cached_messages_number,
  /* sess_recent_number */ imapdriver_cached_recent_number,
  /* sess_unseen_number */ imapdriver_cached_unseen_number,
  /* sess_list_folders */ imapdriver_cached_list_folders,
  /* sess_lsub_folders */ imapdriver_cached_lsub_folders,
  /* sess_subscribe_folder */ imapdriver_cached_subscribe_folder,
  /* sess_unsubscribe_folder */ imapdriver_cached_unsubscribe_folder,

  /* sess_append_message */ imapdriver_cached_append_message,
  /* sess_append_message_flags */ imapdriver_cached_append_message_flags,
  /* sess_copy_message */ imapdriver_cached_copy_message,
  /* sess_move_message */ NULL,

  /* sess_get_message */ imapdriver_cached_get_message,
  /* sess_get_message_by_uid */ imapdriver_cached_get_message_by_uid,

  /* sess_get_messages_list */ imapdriver_cached_get_messages_list,
  /* sess_get_envelopes_list */ imapdriver_cached_get_envelopes_list,
  /* sess_remove_message */ imapdriver_cached_remove_message,
#if 0
  /* sess_search_messages */ imapdriver_cached_search_messages,
#endif
  /* sess_cached_login_sasl */ imapdriver_cached_login_sasl,

#else
  .sess_name = "imap-cached",

  .sess_initialize = imapdriver_cached_initialize,
  .sess_uninitialize = imapdriver_cached_uninitialize,

  .sess_parameters = imapdriver_cached_parameters,

  .sess_connect_stream = imapdriver_cached_connect_stream,
  .sess_connect_path = NULL,
  .sess_starttls = imapdriver_cached_starttls,
  .sess_login = imapdriver_cached_login,
  .sess_logout = imapdriver_cached_logout,
  .sess_noop = imapdriver_cached_noop,

  .sess_build_folder_name = imapdriver_cached_build_folder_name,
  .sess_create_folder = imapdriver_cached_create_folder,
  .sess_delete_folder = imapdriver_cached_delete_folder,
  .sess_rename_folder = imapdriver_cached_rename_folder,
  .sess_check_folder = imapdriver_cached_check_folder,
  .sess_examine_folder = imapdriver_cached_examine_folder,
  .sess_select_folder = imapdriver_cached_select_folder,
  .sess_expunge_folder = imapdriver_cached_expunge_folder,
  .sess_status_folder = imapdriver_cached_status_folder,
  .sess_messages_number = imapdriver_cached_messages_number,
  .sess_recent_number = imapdriver_cached_recent_number,
  .sess_unseen_number = imapdriver_cached_unseen_number,
  .sess_list_folders = imapdriver_cached_list_folders,
  .sess_lsub_folders = imapdriver_cached_lsub_folders,
  .sess_subscribe_folder = imapdriver_cached_subscribe_folder,
  .sess_unsubscribe_folder = imapdriver_cached_unsubscribe_folder,

  .sess_append_message = imapdriver_cached_append_message,
  .sess_append_message_flags = imapdriver_cached_append_message_flags,
  .sess_copy_message = imapdriver_cached_copy_message,
  .sess_move_message = NULL,

  .sess_get_messages_list = imapdriver_cached_get_messages_list,
  .sess_get_envelopes_list = imapdriver_cached_get_envelopes_list,
  .sess_remove_message = imapdriver_cached_remove_message,
#if 0
  .sess_search_messages = imapdriver_cached_search_messages,
#endif

  .sess_get_message = imapdriver_cached_get_message,
  .sess_get_message_by_uid = imapdriver_cached_get_message_by_uid,
  .sess_login_sasl = imapdriver_cached_login_sasl,
#endif
};

mailsession_driver * imap_cached_session_driver =
&local_imap_cached_session_driver;

#define CACHE_MESSAGE_LIST

static inline struct imap_cached_session_state_data *
get_cached_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession * get_ancestor(mailsession * s)
{
  return get_cached_data(s)->imap_ancestor;
}

static inline 
struct imap_session_state_data * get_ancestor_data(mailsession * s)
{
  return get_ancestor(s)->sess_data;
}

static inline mailimap * get_imap_session(mailsession * session)
{
  return get_ancestor_data(session)->imap_session;
}

static int imapdriver_cached_initialize(mailsession * session)
{
  struct imap_cached_session_state_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto err;

  data->imap_ancestor = mailsession_new(imap_session_driver);
  if (data->imap_ancestor == NULL)
    goto free_data;
  data->imap_quoted_mb = NULL;
  data->imap_cache_directory[0] = '\0';
  data->imap_uid_list = carray_new(128);
  if (data->imap_uid_list == NULL)
    goto free_session;
  data->imap_uidvalidity = 0;
  
  session->sess_data = data;
  
  return MAIL_NO_ERROR;

 free_session:
  mailsession_free(data->imap_ancestor);
 free_data:
  free(data);
 err:
  return MAIL_ERROR_MEMORY;
}

static void
free_quoted_mb(struct imap_cached_session_state_data * imap_cached_data)
{
  if (imap_cached_data->imap_quoted_mb != NULL) {
    free(imap_cached_data->imap_quoted_mb);
    imap_cached_data->imap_quoted_mb = NULL;
  }
}

struct uid_cache_item {
  uint32_t uid;
  uint32_t size;
};

static int update_uid_cache(mailsession * session,
    struct mailmessage_list * env_list)
{
  unsigned int i;
  int r;
  struct imap_cached_session_state_data * data;
  int res;
  mailimap * imap;
  
  data = get_cached_data(session);
  imap = get_imap_session(session);
  
  /* free all UID cache */
  for(i = 0 ; i < carray_count(data->imap_uid_list) ; i ++) {
    struct uid_cache_item * cache_item;
    
    cache_item = carray_get(data->imap_uid_list, i);
    free(cache_item);
  }
  
  if (env_list == NULL) {
    r = carray_set_size(data->imap_uid_list, 0);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
  }
  else {
    /* build UID cache */
    r = carray_set_size(data->imap_uid_list,
        carray_count(env_list->msg_tab));
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
  
    for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
      struct uid_cache_item * cache_item;
      mailmessage * msg;
    
      cache_item = malloc(sizeof(* cache_item));
      if (cache_item == NULL) {
        res = MAIL_ERROR_MEMORY;
        goto err;
      }
      msg = carray_get(env_list->msg_tab, i);
      cache_item->uid = msg->msg_index;
      cache_item->size = msg->msg_size;
    
      carray_set(data->imap_uid_list, i, cache_item);
    }
  }
  data->imap_uidvalidity = imap->imap_selection_info->sel_uidvalidity;
  
  return MAIL_NO_ERROR;

 err:
  return res;
}

static void check_for_uid_cache(mailsession * session)
{
#if 0
  mailsession * imap;
#endif
  mailimap * imap;
#if 0
  struct imap_session_state_data * imap_data;
#endif
  clist * list;
  clistiter * cur;
  struct imap_cached_session_state_data * data;
  unsigned int i;
  unsigned dest;

  data = get_cached_data(session);
#if 0
  imap = get_ancestor(session);
  
  imap_data = imap->data;
#endif

  imap = get_imap_session(session);
  
  if (imap->imap_response_info == NULL)
    return;

  list = imap->imap_response_info->rsp_expunged;
  if (list == NULL)
    return;

  dest = 0;
  i = 0;
  /* remove expunged */
  for(cur = clist_begin(list) ; cur != NULL ; cur = clist_next(cur)) {
    uint32_t expunged;
    
    expunged = * (uint32_t *) clist_content(cur);

    while (i < carray_count(data->imap_uid_list)) {
      struct uid_cache_item * cache_item;
      
      if (dest + 1 == expunged) {
        cache_item = carray_get(data->imap_uid_list, i);
        free(cache_item);
        i ++;
        break;
      }
      else {
        cache_item = carray_get(data->imap_uid_list, i);
        carray_set(data->imap_uid_list, dest, cache_item);
        i ++;
        dest ++;
      }
    }
  }
  /* complete list */
  while (i < carray_count(data->imap_uid_list)) {
    struct uid_cache_item * cache_item;

    cache_item = carray_get(data->imap_uid_list, i);
    carray_set(data->imap_uid_list, dest, cache_item);
    i ++;
    dest ++;
  }
  carray_set_size(data->imap_uid_list, dest);
}

static void imapdriver_cached_uninitialize(mailsession * session)
{
  struct imap_cached_session_state_data * data;
  unsigned int i;

  data = get_cached_data(session);
  
  for(i = 0 ; i < carray_count(data->imap_uid_list) ; i ++) {
    struct uid_cache_item * cache_item;
    
    cache_item = carray_get(data->imap_uid_list, i);
    free(cache_item);
  }
  carray_free(data->imap_uid_list);
  free_quoted_mb(data);
  mailsession_free(data->imap_ancestor);
  free(data);
  
  session->sess_data = NULL;
}


static int imapdriver_cached_parameters(mailsession * session,
					int id, void * value)
{
  struct imap_cached_session_state_data * data;
  int r;

  data = get_cached_data(session);

  switch (id) {
  case IMAPDRIVER_CACHED_SET_CACHE_DIRECTORY:
    strncpy(data->imap_cache_directory, value, PATH_MAX);
    data->imap_cache_directory[PATH_MAX - 1] = '\0';

    r = generic_cache_create_dir(data->imap_cache_directory);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;
  }

  return MAIL_ERROR_INVAL;
}


static int imapdriver_cached_connect_stream(mailsession * session,
					    mailstream * s)
{
  int r;

  check_for_uid_cache(session);
  
  r = mailsession_connect_stream(get_ancestor(session), s);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_starttls(mailsession * session)
{
  int r;
  
  r =  mailsession_starttls(get_ancestor(session));
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_login(mailsession * session,
				   const char * userid, const char * password)
{
  int r;
  
  r = mailsession_login(get_ancestor(session), userid, password);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_logout(mailsession * session)
{
  int r;

  r = mailsession_logout(get_ancestor(session));

  check_for_uid_cache(session);

  if (r == MAIL_NO_ERROR) {
    struct imap_cached_session_state_data * imap_cached_data;

    imap_cached_data = get_cached_data(session);

    free_quoted_mb(imap_cached_data);
  }
  
  return r;
}

static int imapdriver_cached_noop(mailsession * session)
{
  int r;
  
  r = mailsession_noop(get_ancestor(session));
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_build_folder_name(mailsession * session,
					       const char * mb,
					       const char * name, char ** result)
{
  int r;
  
  r = mailsession_build_folder_name(get_ancestor(session), mb,
      name, result);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_create_folder(mailsession * session, const char * mb)
{
  int r;
  
  r = mailsession_create_folder(get_ancestor(session), mb);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_delete_folder(mailsession * session, const char * mb)
{
  int r;
  
  r = mailsession_delete_folder(get_ancestor(session), mb);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_rename_folder(mailsession * session, const char * mb,
					   const char * new_name)
{
  int r;
  
  r = mailsession_rename_folder(get_ancestor(session), mb, new_name);

  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_check_folder(mailsession * session)
{
  int r;
  
  r = mailsession_check_folder(get_ancestor(session));
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_examine_folder(mailsession * session,
					    const char * mb)
{
  int r;
 
  r = mailsession_examine_folder(get_ancestor(session), mb);

  check_for_uid_cache(session);
  
  return r;
}

static int get_cache_folder(mailsession * session, char ** result)
{
#if 0
  mailsession * imap_session;
#endif
  mailimap * imap;
  char * mb;
  char * cache_dir;
  char * dirname;
  char * quoted_mb;
  int res;
  int r;
  char key[PATH_MAX];
#if 0
  struct imap_session_state_data * imap_data;
  struct imap_cached_session_state_data * cached_data;
#endif

#if 0
  imap_session = get_ancestor(session);
  imap_data = imap_session->data;
  imap = imap_data->session;
#endif
  imap = get_imap_session(session);
  
  mb = get_ancestor_data(session)->imap_mailbox;
  
  cache_dir = get_cached_data(session)->imap_cache_directory;

  if (imap->imap_state != MAILIMAP_STATE_SELECTED)
    return MAIL_ERROR_BAD_STATE;

  if (imap->imap_selection_info == NULL)
    return MAIL_ERROR_BAD_STATE;

  quoted_mb = maildriver_quote_mailbox(mb);
  if (quoted_mb == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  snprintf(key, PATH_MAX, "%s/%s", cache_dir, quoted_mb);

  dirname = strdup(key);
  if (dirname == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_mb;
  }

  r = generic_cache_create_dir(dirname);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_dirname;
  }

  free(quoted_mb);

  * result = dirname;

  return MAIL_NO_ERROR;

 free_dirname:
  free(dirname);
 free_mb:
  free(quoted_mb);
 err:
  return res;
}

static int imapdriver_cached_select_folder(mailsession * session, const char * mb)
{
  int r;
  char * quoted_mb;
  struct imap_cached_session_state_data * data;
  mailsession * imap;
  char * old_mb;
  
  imap = get_ancestor(session);
  
  old_mb = get_ancestor_data(session)->imap_mailbox;
  if (old_mb != NULL)
    if (strcmp(mb, old_mb) == 0)
      return MAIL_NO_ERROR;
  
  r = mailsession_select_folder(get_ancestor(session), mb);
  if (r != MAIL_NO_ERROR)
    return r;

  check_for_uid_cache(session);

  quoted_mb = NULL;
  r = get_cache_folder(session, &quoted_mb);
  if (r != MAIL_NO_ERROR)
    return r;

  data = get_cached_data(session);
  if (data->imap_quoted_mb != NULL)
    free(data->imap_quoted_mb);
  data->imap_quoted_mb = quoted_mb;

  /* clear UID cache */
  carray_set_size(data->imap_uid_list, 0);
  
  return MAIL_NO_ERROR;
}

static int imapdriver_cached_expunge_folder(mailsession * session)
{
  int r;

  r = mailsession_expunge_folder(get_ancestor(session));

  check_for_uid_cache(session);

  return r;
}

static int imapdriver_cached_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  int r;

  r = mailsession_status_folder(get_ancestor(session), mb, result_messages,
      result_recent, result_unseen);

  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_messages_number(mailsession * session,
					     const char * mb,
					     uint32_t * result)
{
  int r;
  
  r = mailsession_messages_number(get_ancestor(session), mb, result);
  
  check_for_uid_cache(session);

  return r;
}

static int imapdriver_cached_recent_number(mailsession * session, const char * mb,
					   uint32_t * result)
{
  int r;
  
  r = mailsession_recent_number(get_ancestor(session), mb, result);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_unseen_number(mailsession * session, const char * mb,
					   uint32_t * result)
{
  int r;
  
  r = mailsession_unseen_number(get_ancestor(session), mb, result);

  check_for_uid_cache(session);

  return r;
}

static int imapdriver_cached_list_folders(mailsession * session, const char * mb,
					  struct mail_list ** result)
{
  int r;
  
  r = mailsession_list_folders(get_ancestor(session), mb, result);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_lsub_folders(mailsession * session, const char * mb,
					  struct mail_list ** result)
{
  int r;
  
  r = mailsession_lsub_folders(get_ancestor(session), mb, result);
  
  check_for_uid_cache(session);

  return r;
}

static int imapdriver_cached_subscribe_folder(mailsession * session,
					      const char * mb)
{
  int r;
  
  r = mailsession_subscribe_folder(get_ancestor(session), mb);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_unsubscribe_folder(mailsession * session,
						const char * mb)
{
  int r;

  r = mailsession_unsubscribe_folder(get_ancestor(session), mb);
  
  check_for_uid_cache(session);

  return r;
}

static int imapdriver_cached_append_message(mailsession * session,
					    const char * message, size_t size)
{
  int r;
  
  r = mailsession_append_message(get_ancestor(session), message, size);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  int r;
  
  r = mailsession_append_message_flags(get_ancestor(session),
      message, size, flags);
  
  check_for_uid_cache(session);
  
  return r;
}

static int imapdriver_cached_copy_message(mailsession * session,
					  uint32_t num, const char * mb)
{
  int r;

  r = mailsession_copy_message(get_ancestor(session), num, mb);

  check_for_uid_cache(session);
  
  return r;
}

static int cmp_uid(uint32_t ** pa, uint32_t ** pb)
{
  uint32_t * a;
  uint32_t * b;
  
  a = * pa;
  b = * pb;
  
  return * a - * b;
}


static void get_uid_from_filename(char * filename)
{
  char * p;
  
  p = strstr(filename, "-part");
  if (p != NULL)
    * p = 0;
  p = strstr(filename, "-envelope");
  if (p != NULL)
    * p = 0;
  p = strstr(filename, "-rfc822");
  if (p != NULL)
    * p = 0;
}  

#define ENV_NAME "env.db"

static int boostrap_cache(mailsession * session)
{
  struct mail_cache_db * cache_db;
  char filename[PATH_MAX];
  mailimap * imap;
  struct imap_cached_session_state_data * data;
  MMAPString * mmapstr;
  int r;
  int res;
  chashiter * iter;
  chash * keys;
  chash * keys_uid;
  
  data = get_cached_data(session);
  imap = get_imap_session(session);
  
  if (data->imap_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }
  
  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  snprintf(filename, PATH_MAX, "%s/%s", data->imap_quoted_mb, ENV_NAME);
  
  r = mail_cache_db_open_lock(filename, &cache_db);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }
  
  keys = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (keys == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }
  
  r = mail_cache_db_get_keys(cache_db, keys);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_keys;
  }
  
  keys_uid = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (keys_uid == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_keys;
  }
  
  for(iter = chash_begin(keys) ; iter != NULL ; iter = chash_next(keys, iter)) {
    chashdatum key;
    chashdatum value;
    char msg_uid[PATH_MAX];
    
    chash_key(iter, &key);
    if (key.len >= sizeof(msg_uid)) {
      strncpy(msg_uid, key.data, sizeof(msg_uid));
      msg_uid[sizeof(msg_uid) - 1] = 0;
    }
    else {
      strncpy(msg_uid, key.data, key.len);
      msg_uid[key.len] = 0;
    }
    
    get_uid_from_filename(msg_uid);
    key.data = msg_uid;
    key.len = strlen(msg_uid) + 1;
    value.data = NULL;
    value.len = 0;
    chash_set(keys_uid, &key, &value, NULL);
  }
  
  for(iter = chash_begin(keys_uid) ; iter != NULL ; iter = chash_next(keys_uid, iter)) {
    chashdatum key;
    uint32_t uidvalidity;
    uint32_t index;
    char * uid;
    char * p1, * p2;
    struct uid_cache_item * cache_item;
    
    chash_key(iter, &key);
    uid = key.data;
    
    uidvalidity = strtoul(uid, &p1, 10);
    if (p1 == uid || * p1 != '-')
      continue;
    
    data->imap_uidvalidity = uidvalidity;
    
    p1++;
    index = strtoul(p1, &p2, 10);
    if (p2 == p1 || * p2 != '\0')
      continue;
    
    cache_item = malloc(sizeof(* cache_item));
    if (cache_item == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_keys_uid;
    }
    
    cache_item->uid = index;
    cache_item->size = 0;
    
    carray_add(data->imap_uid_list, cache_item, NULL);
  }
  
  chash_free(keys_uid);
  chash_free(keys);
  
  mail_cache_db_close_unlock(filename, cache_db);
  mmap_string_free(mmapstr);
  
  return MAIL_NO_ERROR;
  
 free_keys_uid:
  chash_free(keys_uid);
 free_keys:
  chash_free(keys);
 close_db:
  mail_cache_db_close_unlock(filename, cache_db);
 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}


static int imapdriver_cached_get_messages_list(mailsession * session,
					       struct mailmessage_list **
					       result)
{
  mailimap * imap;
  uint32_t uid_max;
  struct imap_cached_session_state_data * data;
  struct mailmessage_list * env_list;
  unsigned i;
  int r;
  int res;
  carray * tab;

  data = get_cached_data(session);
  imap = get_imap_session(session);

  uid_max = 0;

#ifdef CACHE_MESSAGE_LIST
  if (data->imap_uidvalidity == 0) {
    boostrap_cache(session);
  }
  
  if (imap->imap_selection_info->sel_uidvalidity != data->imap_uidvalidity) {
    update_uid_cache(session, NULL);
  }
  
  /* get UID max */
  uid_max = 0;
  for(i = 0 ; i < carray_count(data->imap_uid_list) ; i ++) {
    struct uid_cache_item * cache_item;
    
    cache_item = carray_get(data->imap_uid_list, i);
    if (cache_item->uid > uid_max)
      uid_max = cache_item->uid;
  }
#endif
  
  r = imap_get_messages_list(imap,  session, imap_cached_message_driver,
      uid_max + 1, &env_list);
  
  check_for_uid_cache(session);

  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

#ifdef CACHE_MESSAGE_LIST
  /* remove unsollicited message */
  i = 0;
  while (i < carray_count(env_list->msg_tab)) {
    mailmessage * msg;
    
    msg = carray_get(env_list->msg_tab, i);
    if (msg->msg_index < uid_max + 1) {
      mailmessage * msg;
      
      msg = carray_get(env_list->msg_tab, i);
      mailmessage_free(msg);
      carray_delete(env_list->msg_tab, i);
    }
    else {
      i ++;
    }
  }
  
  tab = carray_new(carray_count(env_list->msg_tab) +
      carray_count(data->imap_uid_list));
  if (tab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free;
  }
  carray_set_size(tab,
      carray_count(env_list->msg_tab) + carray_count(data->imap_uid_list));

  /* sort cached data before adding them to the list */
  qsort(carray_data(data->imap_uid_list), carray_count(data->imap_uid_list),
      sizeof(* carray_data(data->imap_uid_list)),
      (int (*)(const void *, const void *)) cmp_uid);
  
  /* adds cached UID */
  for(i = 0 ; i < carray_count(data->imap_uid_list) ; i ++) {
    struct uid_cache_item * cache_item;
    mailmessage * msg;
    
    cache_item = carray_get(data->imap_uid_list, i);
    
    msg = mailmessage_new();
    if (msg == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free;
    }

    r = mailmessage_init(msg, session, imap_cached_message_driver,
        cache_item->uid, cache_item->size);
    if (r != MAIL_NO_ERROR) {
      mailmessage_free(msg);
      res = r;
      goto free;
    }
    
    carray_set(tab, i, msg);
  }

  /* adds new elements */
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(env_list->msg_tab, i);
    carray_set(tab, carray_count(data->imap_uid_list) + i, msg);
  }
  
  /* replace list of messages in env_list */
  carray_free(env_list->msg_tab);
  env_list->msg_tab = tab;

  r = update_uid_cache(session, env_list);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }
#endif
  
  * result = env_list;
  
  return MAIL_NO_ERROR;
  
 free:
  mailmessage_list_free(env_list);
 err:
  return res;
}

#define IMAP_SET_MAX_COUNT 100

static int get_flags_list(mailsession * session,
			  struct mailmessage_list * env_list)
{
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  int res;
  clist * fetch_result;
  int r;
  clist * msg_list;
#if 0
  struct imap_session_state_data * data;
#endif
  unsigned i;
  unsigned dest;
  clistiter * set_iter;

#if 0
  data = session->data;
#endif

  fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  fetch_att = mailimap_fetch_att_new_uid();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  fetch_att = mailimap_fetch_att_new_flags();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = maildriver_env_list_to_msg_list_no_flags(env_list, &msg_list);
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  if (clist_begin(msg_list) == NULL) {
    /* no need to fetch envelopes */
    
    clist_free(msg_list);
    mailimap_fetch_type_free(fetch_type);
    return MAIL_NO_ERROR;
  }

  r = msg_list_to_imap_set(msg_list, &set);
  if (r != MAIL_NO_ERROR) {
    clist_foreach(msg_list, (clist_func) free, NULL);
    clist_free(msg_list);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }
  clist_foreach(msg_list, (clist_func) free, NULL);
  clist_free(msg_list);

  set_iter = clist_begin(set->set_list);
  while (set_iter != NULL) {
    struct mailimap_set * subset;
    unsigned int count;
    
    subset = mailimap_set_new_empty();
    if (subset == NULL) {
      res = MAIL_ERROR_MEMORY;
      mailimap_fetch_type_free(fetch_type);
      mailimap_set_free(set);
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    
    count = 0;
    while (count < IMAP_SET_MAX_COUNT) {
      struct mailimap_set_item * item;
      
      item = clist_content(set_iter);
      set_iter = clist_delete(set->set_list, set_iter);
      
      r = mailimap_set_add(subset, item);
      if (r != MAILIMAP_NO_ERROR) {
        mailimap_set_item_free(item);
        mailimap_set_free(subset);
        mailimap_fetch_type_free(fetch_type);
        mailimap_set_free(set);
        res = MAIL_ERROR_MEMORY;
        goto err;
      }
      
      count ++;
      
      if (set_iter == NULL)
        break;
    }
    
    r = mailimap_uid_fetch(get_imap_session(session), subset,
        fetch_type, &fetch_result);
    
    mailimap_set_free(subset);
    
    switch (r) {
    case MAILIMAP_NO_ERROR:
      break;
    default:
      mailimap_fetch_type_free(fetch_type);
      mailimap_set_free(set);
      return imap_error_to_mail_error(r);
    }
    
#if 0
    if (clist_begin(fetch_result) == NULL) {
      res = MAIL_ERROR_FETCH;
      goto err;
    }
#endif
    
    r = imap_fetch_result_to_envelop_list(fetch_result, env_list);
    mailimap_fetch_list_free(fetch_result);
    
    if (r != MAIL_NO_ERROR) {
      mailimap_fetch_type_free(fetch_type);
      mailimap_set_free(set);
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
  }

#if 0
  r = mailimap_uid_fetch(get_imap_session(session), set,
			 fetch_type, &fetch_result);
#endif

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

#if 0
  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  r = imap_fetch_result_to_envelop_list(fetch_result, env_list);
  mailimap_fetch_list_free(fetch_result);

  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
#endif

  /* remove messages that don't have flags */
  i = 0;
  dest = 0;
  while (i < carray_count(env_list->msg_tab)) {
    mailmessage * msg;
    
    msg = carray_get(env_list->msg_tab, i);
    if (msg->msg_flags != NULL) {
      carray_set(env_list->msg_tab, dest, msg);
      dest ++;
    }
    else {
      mailmessage_free(msg);
    }
    i ++;
  }
  carray_set_size(env_list->msg_tab, dest);
  
  return MAIL_NO_ERROR;
  
 free_fetch_type:
  mailimap_fetch_type_free(fetch_type);
 err:
  return res;
}


static int
imapdriver_cached_get_envelopes_list(mailsession * session,
				     struct mailmessage_list * env_list)
{
  int r;
  int res;
  uint32_t i;
  struct imap_cached_session_state_data * data;
  MMAPString * mmapstr;
  struct mail_cache_db * cache_db;
  char filename[PATH_MAX];

  data = get_cached_data(session);
  if (data->imap_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  snprintf(filename, PATH_MAX, "%s/%s", data->imap_quoted_mb, ENV_NAME);

  r = mail_cache_db_open_lock(filename, &cache_db);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }

  /* fill with cached */

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    struct mailimf_fields * fields;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields == NULL) {
      r = imapdriver_get_cached_envelope(cache_db, mmapstr,
          session, msg, &fields);
      if (r == MAIL_NO_ERROR) {
	msg->msg_cached = TRUE;
	msg->msg_fields = fields;
      }
    }
  }

  mail_cache_db_close_unlock(filename, cache_db);
  
  r = mailsession_get_envelopes_list(get_ancestor(session), env_list);

  check_for_uid_cache(session);
  
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }
  
  r = get_flags_list(session, env_list);

  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }

#ifdef CACHE_MESSAGE_LIST
  r = update_uid_cache(session, env_list);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mmapstr;
  }
#endif

  /* must write cache */

  r = mail_cache_db_open_lock(filename, &cache_db);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_mmapstr;
  }

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields != NULL) {
      if (!msg->msg_cached) {
	r = imapdriver_write_cached_envelope(cache_db, mmapstr,
            session, msg, msg->msg_fields);
      }
    }
  }
  
  /* flush cache */
  
  maildriver_cache_clean_up(cache_db, NULL, env_list);
  
  mail_cache_db_close_unlock(filename, cache_db);
  mmap_string_free(mmapstr);

  /* remove cache files */

  maildriver_message_cache_clean_up(data->imap_quoted_mb, env_list,
      get_uid_from_filename);
  
  return MAIL_NO_ERROR;

 free_mmapstr:
  mmap_string_free(mmapstr);
 err:
  return res;
}

static int imapdriver_cached_remove_message(mailsession * session,
					    uint32_t num)
{
  int r;
  
  r = mailsession_remove_message(get_ancestor(session), num);
  
  check_for_uid_cache(session);
  
  return r;
}

#if 0
static int imapdriver_cached_search_messages(mailsession * session,
					     char * charset,
					     struct mail_search_key * key,
					     struct mail_search_result **
					     result)
{
  int r;
  
  r = mailsession_search_messages(get_ancestor(session), charset, key, result);
  
  check_for_uid_cache(session);
  
  return r;
}
#endif

static int imapdriver_cached_get_message(mailsession * session,
					 uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, imap_cached_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

/* Retrieve a message by UID
 * libEtPan! uid format for IMAP is "UIDVALIDITY-UID"
 * where UIDVALIDITY and UID are decimal representation of 
 * respectively uidvalidity and uid numbers. 
 * Return value:
 * MAIL_ERROR_INVAL if uid is NULL or has an incorrect format.
 * MAIL_ERROR_MSG_NOT_FOUND if uidvalidity has changed or uid was not found
 * MAIL_NO_ERROR if message was found. Result is in result
 */
static int imapdriver_cached_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result)
{
  uint32_t uidvalidity;
  uint32_t num;
  char * p1, * p2;
  mailimap *imap;
  
  if (uid == NULL)
    return MAIL_ERROR_INVAL;

  uidvalidity = strtoul(uid, &p1, 10);
  if (p1 == uid || * p1 != '-')
    return MAIL_ERROR_INVAL;

  p1++;
  num = strtoul(p1, &p2, 10);
  if (p2 == p1 || * p2 != '\0')
    return MAIL_ERROR_INVAL;
 
  imap = get_imap_session(session);
  if (imap->imap_selection_info->sel_uidvalidity != uidvalidity)
    return MAIL_ERROR_MSG_NOT_FOUND;

  return imapdriver_cached_get_message(session, num, result);
}

static int imapdriver_cached_login_sasl(mailsession * session,
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
