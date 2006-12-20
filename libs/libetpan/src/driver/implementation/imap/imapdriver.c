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
 * $Id: imapdriver.c,v 1.55 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imapdriver.h"

#include "mail.h"
#include "imapdriver_tools.h"
#include "mailmessage.h"
#include "imapdriver_message.h"
#include "imapdriver_types.h"
#include "maildriver.h"
#include "maildriver_tools.h"
#include "generic_cache.h"

#include <stdlib.h>
#include <string.h>

static int imapdriver_initialize(mailsession * session);

static void imapdriver_uninitialize(mailsession * session);

static int imapdriver_connect_stream(mailsession * session, mailstream * s);

static int imapdriver_starttls(mailsession * session);

static int imapdriver_login(mailsession * session,
			    const char * userid, const char * password);

static int imapdriver_logout(mailsession * session);

static int imapdriver_noop(mailsession * session);

static int imapdriver_build_folder_name(mailsession * session, const char * mb,
					const char * name, char ** result);

static int imapdriver_create_folder(mailsession * session, const char * mb);

static int imapdriver_delete_folder(mailsession * session, const char * mb);

static int imapdriver_rename_folder(mailsession * session, const char * mb,
				    const char * new_name);

static int imapdriver_check_folder(mailsession * session);

static int imapdriver_examine_folder(mailsession * session, const char * mb);

static int imapdriver_select_folder(mailsession * session, const char * mb);
static int imapdriver_expunge_folder(mailsession * session);

static int imapdriver_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);

static int imapdriver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result);

static int imapdriver_recent_number(mailsession * session, const char * mb,
				      uint32_t * result);

static int imapdriver_unseen_number(mailsession * session, const char * mb,
				      uint32_t * result);

static int imapdriver_list_folders(mailsession * session, const char * mb,
				    struct mail_list ** result);
static int imapdriver_lsub_folders(mailsession * session, const char * mb,
				   struct mail_list ** result);
static int imapdriver_subscribe_folder(mailsession * session, const char * mb);
static int imapdriver_unsubscribe_folder(mailsession * session, const char * mb);
static int imapdriver_append_message(mailsession * session,
				     const char * message, size_t size);
static int imapdriver_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);
static int imapdriver_copy_message(mailsession * session,
				   uint32_t num, const char * mb);

static int imapdriver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result);

static int
imapdriver_get_envelopes_list(mailsession * session,
			      struct mailmessage_list * env_list);


#if 0
static int imapdriver_search_messages(mailsession * session, const char * charset,
				      struct mail_search_key * key,
				      struct mail_search_result ** result);
#endif

static int imapdriver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result);

static int imapdriver_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result);

static int imapdriver_login_sasl(mailsession * session,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);

static int imapdriver_remove_message(mailsession * session, uint32_t num);

static mailsession_driver local_imap_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "imap",

  /* sess_initialize */ imapdriver_initialize,
  /* sess_uninitialize */ imapdriver_uninitialize,

  /* sess_parameters */ NULL,

  /* sess_connect_stream */ imapdriver_connect_stream,
  /* sess_connect_path */ NULL,
  /* sess_starttls */ imapdriver_starttls,
  /* sess_login */ imapdriver_login,
  /* sess_logout */ imapdriver_logout,
  /* sess_noop */ imapdriver_noop,

  /* sess_build_folder_name */ imapdriver_build_folder_name,
  /* sess_create_folder */ imapdriver_create_folder,
  /* sess_delete_folder */ imapdriver_delete_folder,
  /* sess_rename_folder */ imapdriver_rename_folder,
  /* sess_check_folder */ imapdriver_check_folder,
  /* sess_examine_folder */ imapdriver_examine_folder,
  /* sess_select_folder */ imapdriver_select_folder,
  /* sess_expunge_folder */ imapdriver_expunge_folder,
  /* sess_status_folder */ imapdriver_status_folder,
  /* sess_messages_number */ imapdriver_messages_number,
  /* sess_recent_number */ imapdriver_recent_number,
  /* sess_unseen_number */ imapdriver_unseen_number,
  /* sess_list_folders */ imapdriver_list_folders,
  /* sess_lsub_folders */ imapdriver_lsub_folders,
  /* sess_subscribe_folder */ imapdriver_subscribe_folder,
  /* sess_unsubscribe_folder */ imapdriver_unsubscribe_folder,

  /* sess_append_message */ imapdriver_append_message,
  /* sess_append_message_flags */ imapdriver_append_message_flags,
  /* sess_copy_message */ imapdriver_copy_message,
  /* sess_move_message */ NULL,

  /* sess_get_message */ imapdriver_get_message,
  /* sess_get_message_by_uid */ imapdriver_get_message_by_uid,

  /* sess_get_messages_list */ imapdriver_get_messages_list,
  /* sess_get_envelopes_list */ imapdriver_get_envelopes_list,
  /* sess_remove_message */ imapdriver_remove_message,
#if 0
  /* sess_search_messages */ imapdriver_search_messages,
#endif
  /* sess_login_sasl */ imapdriver_login_sasl,
  
#else
  .sess_name = "imap",

  .sess_initialize = imapdriver_initialize,
  .sess_uninitialize = imapdriver_uninitialize,

  .sess_parameters = NULL,

  .sess_connect_stream = imapdriver_connect_stream,
  .sess_connect_path = NULL,
  .sess_starttls = imapdriver_starttls,
  .sess_login = imapdriver_login,
  .sess_logout = imapdriver_logout,
  .sess_noop = imapdriver_noop,

  .sess_build_folder_name = imapdriver_build_folder_name,
  .sess_create_folder = imapdriver_create_folder,
  .sess_delete_folder = imapdriver_delete_folder,
  .sess_rename_folder = imapdriver_rename_folder,
  .sess_check_folder = imapdriver_check_folder,
  .sess_examine_folder = imapdriver_examine_folder,
  .sess_select_folder = imapdriver_select_folder,
  .sess_expunge_folder = imapdriver_expunge_folder,
  .sess_status_folder = imapdriver_status_folder,
  .sess_messages_number = imapdriver_messages_number,
  .sess_recent_number = imapdriver_recent_number,
  .sess_unseen_number = imapdriver_unseen_number,
  .sess_list_folders = imapdriver_list_folders,
  .sess_lsub_folders = imapdriver_lsub_folders,
  .sess_subscribe_folder = imapdriver_subscribe_folder,
  .sess_unsubscribe_folder = imapdriver_unsubscribe_folder,

  .sess_append_message = imapdriver_append_message,
  .sess_append_message_flags = imapdriver_append_message_flags,
  .sess_copy_message = imapdriver_copy_message,
  .sess_move_message = NULL,

  .sess_get_messages_list = imapdriver_get_messages_list,
  .sess_get_envelopes_list = imapdriver_get_envelopes_list,
  .sess_remove_message = imapdriver_remove_message,
#if 0
  .sess_search_messages = imapdriver_search_messages,
#endif
  
  .sess_get_message = imapdriver_get_message,
  .sess_get_message_by_uid = imapdriver_get_message_by_uid,
  .sess_login_sasl = imapdriver_login_sasl,
#endif
};

mailsession_driver * imap_session_driver = &local_imap_session_driver;

static inline struct imap_session_state_data * get_data(mailsession * session)
{
  return session->sess_data;
}

static mailimap * get_imap_session(mailsession * session)
{
  return get_data(session)->imap_session;
}

static int imapdriver_initialize(mailsession * session)
{
  struct imap_session_state_data * data;
  mailimap * imap;
  struct mail_flags_store * flags_store;

  imap = mailimap_new(0, NULL);
  if (imap == NULL)
    goto err;

  flags_store = mail_flags_store_new();
  if (flags_store == NULL)
    goto free_session;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto free_flags_store;

  data->imap_mailbox = NULL;
  data->imap_session = imap;
  data->imap_flags_store = flags_store;

  session->sess_data = data;

  return MAIL_NO_ERROR;

 free_flags_store:
  mail_flags_store_free(flags_store);
 free_session:
  mailimap_free(imap);
 err:
  return MAIL_ERROR_MEMORY;
}

static void imap_flags_store_process(mailimap * imap,
				     struct mail_flags_store * flags_store)
{
  unsigned int i;
  int r;
  mailmessage * first;
  mailmessage * last;

  mail_flags_store_sort(flags_store);

  if (carray_count(flags_store->fls_tab) == 0)
    return;
  
  first = carray_get(flags_store->fls_tab, 0);
  last = first;

  for(i = 1 ; i < carray_count(flags_store->fls_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(flags_store->fls_tab, i);

    if (last->msg_index + 1 == msg->msg_index) {
      r = mail_flags_compare(first->msg_flags, msg->msg_flags);
      if (r == 0) {
	last = msg;
	continue;
      }
    }

    r = imap_store_flags(imap, first->msg_index,
        last->msg_index, first->msg_flags);

    first = msg;
    last = msg;
  }

  r = imap_store_flags(imap, first->msg_index, last->msg_index,
      first->msg_flags);
  
  mail_flags_store_clear(flags_store);
}

static void imapdriver_uninitialize(mailsession * session)
{
  struct imap_session_state_data * data;

  data = get_data(session);

  imap_flags_store_process(data->imap_session,
      data->imap_flags_store);
  mail_flags_store_free(data->imap_flags_store); 
  
  mailimap_free(data->imap_session);
  if (data->imap_mailbox != NULL)
    free(data->imap_mailbox);
  free(data);
  
  session->sess_data = NULL;
}

static int imapdriver_connect_stream(mailsession * session, mailstream * s)
{
  int r;
  
  r = mailimap_connect(get_imap_session(session), s);

  return imap_error_to_mail_error(r);
}

static int imapdriver_login(mailsession * session,
			    const char * userid, const char * password)
{
  int r;

  r = mailimap_login(get_imap_session(session), userid, password);

  return imap_error_to_mail_error(r);
}

static int imapdriver_logout(mailsession * session)
{
  int r;

  imap_flags_store_process(get_imap_session(session),
			   get_data(session)->imap_flags_store);

  r = mailimap_logout(get_imap_session(session));

  return imap_error_to_mail_error(r);
}

static int imapdriver_noop(mailsession * session)
{
  int r;

  r = mailimap_noop(get_imap_session(session));

  return imap_error_to_mail_error(r);
}

static int imapdriver_build_folder_name(mailsession * session, const char * mb,
					const char * name, char ** result)
{
  char delimiter[2] = "X";
  char * folder_name;
  mailimap * imap;
  struct mailimap_mailbox_list * mb_list;
  int r;
  clist * imap_list;

  imap = get_imap_session(session);

  r = mailimap_list(imap, mb, "", &imap_list);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (clist_begin(imap_list) == NULL)
    return MAIL_ERROR_LIST;

  mb_list = clist_begin(imap_list)->data;
  delimiter[0] = mb_list->mb_delimiter;

  folder_name = malloc(strlen(mb) + strlen(delimiter) + strlen(name) + 1);
  if (folder_name == NULL)
    return MAIL_ERROR_MEMORY;

  strcpy(folder_name, mb);
  strcat(folder_name, delimiter);
  strcat(folder_name, name);

  * result = folder_name;
  
  return MAIL_NO_ERROR;
}

/* folders operations */

static int imapdriver_create_folder(mailsession * session, const char * mb)
{
  int r;

  r = mailimap_create(get_imap_session(session), mb);

  return imap_error_to_mail_error(r);
}

static int imapdriver_delete_folder(mailsession * session, const char * mb)
{
  int r;

  r = mailimap_delete(get_imap_session(session), mb);

  return imap_error_to_mail_error(r);
}

static int imapdriver_rename_folder(mailsession * session, const char * mb,
				    const char * new_name)
{
  int r;

  r = mailimap_rename(get_imap_session(session), mb, new_name);

  return imap_error_to_mail_error(r);
}

static int imapdriver_check_folder(mailsession * session)
{
  int r;

  imap_flags_store_process(get_imap_session(session),
			   get_data(session)->imap_flags_store);

  r = mailimap_check(get_imap_session(session));

  return imap_error_to_mail_error(r);
}

static int imapdriver_examine_folder(mailsession * session, const char * mb)
{
  int r;

  r = mailimap_examine(get_imap_session(session), mb);

  return imap_error_to_mail_error(r);
}

static int imapdriver_select_folder(mailsession * session, const char * mb)
{
  int r;
  char * new_mb;
  char * old_mb;

  old_mb = get_data(session)->imap_mailbox;
  if (old_mb != NULL)
    if (strcmp(mb, old_mb) == 0)
      return MAIL_NO_ERROR;

  imap_flags_store_process(get_imap_session(session),
			   get_data(session)->imap_flags_store);

  r = mailimap_select(get_imap_session(session), mb);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    new_mb = strdup(mb);
    if (new_mb == NULL) {
      if (old_mb != NULL)
        free(old_mb);
      get_data(session)->imap_mailbox = NULL;
      return MAIL_ERROR_MEMORY;
    }

    get_data(session)->imap_mailbox = new_mb;

    return MAIL_NO_ERROR;
  default:
    return imap_error_to_mail_error(r);
  }
}

static int imapdriver_expunge_folder(mailsession * session)
{
  int r;

  imap_flags_store_process(get_imap_session(session),
			   get_data(session)->imap_flags_store);

  r = mailimap_expunge(get_imap_session(session));

  return imap_error_to_mail_error(r);
}

static int status_selected_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  int r;
  int res;
  mailimap * imap;
  uint32_t exists;
  uint32_t unseen;
  uint32_t recent;
  struct mailimap_search_key * search_key;
  clist * search_result;
  
  imap = get_imap_session(session);
  
  exists = imap->imap_selection_info->sel_exists;
  recent = imap->imap_selection_info->sel_recent;
  
  search_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_UNSEEN,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0,
      NULL, NULL, NULL, NULL, NULL,
      NULL, 0, NULL, NULL, NULL);
  if (search_key == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  /* default : use the RECENT count if search fails */
  unseen = recent;
  r = mailimap_search(imap, NULL, search_key, &search_result);
  mailimap_search_key_free(search_key);
  if (r == MAILIMAP_NO_ERROR) {
    /* if this succeed, we use the real count */
    unseen = clist_count(search_result);
    mailimap_mailbox_data_search_free(search_result);
  }
  
  * result_messages = exists;
  * result_unseen = unseen;
  * result_recent = recent;
  
  return MAIL_NO_ERROR;
  
 err:
  return res;
}

static int status_unselected_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  struct mailimap_status_att_list * att_list;
  struct mailimap_mailbox_data_status * status;
  int r;
  int res;
  clistiter * cur;
  mailimap * imap;
  
  imap = get_imap_session(session);
  
  att_list = mailimap_status_att_list_new_empty();
  if (att_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mailimap_status_att_list_add(att_list, MAILIMAP_STATUS_ATT_MESSAGES);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    res = MAIL_ERROR_MEMORY;
    goto free;
  }
    
  r = mailimap_status_att_list_add(att_list, MAILIMAP_STATUS_ATT_RECENT);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    res = MAIL_ERROR_MEMORY;
    goto free;
  }
    
  r = mailimap_status_att_list_add(att_list, MAILIMAP_STATUS_ATT_UNSEEN);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    res = MAIL_ERROR_MEMORY;
    goto free;
  }

  r = mailimap_status(imap, mb, att_list, &status);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    res = imap_error_to_mail_error(r);
    goto free;
  }

  * result_messages = 0;
  * result_recent = 0;
  * result_unseen = 0;
  
  for (cur = clist_begin(status->st_info_list);
       cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_status_info * status_info;
      
    status_info = clist_content(cur);
    switch (status_info->st_att) {
    case MAILIMAP_STATUS_ATT_MESSAGES:
      * result_messages = status_info->st_value;
      break;
    case MAILIMAP_STATUS_ATT_RECENT:
      * result_recent = status_info->st_value;
      break;
    case MAILIMAP_STATUS_ATT_UNSEEN:
      * result_unseen = status_info->st_value;
      break;
    }
  }

  mailimap_mailbox_data_status_free(status);
  mailimap_status_att_list_free(att_list);
  
  return MAIL_NO_ERROR;

 free:
  mailimap_status_att_list_free(att_list);
 err:
  return res;
}

static int imapdriver_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  int res;
  int current_folder;
  char * current_mb;
  
  if (mb == NULL) {
    mb = get_data(session)->imap_mailbox;
    if (mb == NULL) {
      res = MAIL_ERROR_BAD_STATE;
      goto err;
    }
  }
  
  current_mb = get_data(session)->imap_mailbox;
  if (strcmp(mb, current_mb) == 0)
    current_folder = 1;
  else
    current_folder = 0;
  
  if (current_folder)
    return status_selected_folder(session, mb, result_messages,
        result_recent, result_unseen);
  else
    return status_unselected_folder(session, mb, result_messages,
        result_recent, result_unseen);
  
 err:
  return res;
}

/* TODO : more efficient functions */

static int imapdriver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = imapdriver_status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = messages;
 
  return MAIL_NO_ERROR;
}

static int imapdriver_recent_number(mailsession * session, const char * mb,
				    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = imapdriver_status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = recent;
 
  return MAIL_NO_ERROR;
}

static int imapdriver_unseen_number(mailsession * session, const char * mb,
				    uint32_t * result)
{
  uint32_t messages;
  uint32_t recent;
  uint32_t unseen;
  int r;
  
  r = imapdriver_status_folder(session, mb, &messages, &recent, &unseen);
  if (r != MAIL_NO_ERROR)
    return r;

  * result = unseen;
 
  return MAIL_NO_ERROR;
}

enum {
  IMAP_LIST, IMAP_LSUB
};

static int imapdriver_list_lsub_folders(mailsession * session, int type,
					const char * mb,
					struct mail_list ** result)
{
  clist * imap_list;
  struct mail_list * resp;
  int r;
  int res;

  switch (type) {
  case IMAP_LIST:
    r = mailimap_list(get_imap_session(session), mb,
		      "*", &imap_list);
    break;
  case IMAP_LSUB:
    r = mailimap_lsub(get_imap_session(session), mb,
		      "*", &imap_list);
    break;
  default:
    res = MAIL_ERROR_LIST;
    goto err;
  }

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    res = imap_error_to_mail_error(r);
    goto err;
  }

  r = imap_list_to_list(imap_list, &resp);
  if (r != MAIL_NO_ERROR) {
    mailimap_list_result_free(imap_list);
    res = r;
    goto err;
  }

  mailimap_list_result_free(imap_list);

  * result = resp;

  return MAIL_NO_ERROR;

 err:
  return res;
}

static int imapdriver_list_folders(mailsession * session, const char * mb,
				   struct mail_list ** result)
{
  return imapdriver_list_lsub_folders(session, IMAP_LIST, mb,
				      result);
}

static int imapdriver_lsub_folders(mailsession * session, const char * mb,
				   struct mail_list ** result)
{
  return imapdriver_list_lsub_folders(session, IMAP_LSUB, mb,
				      result);
}

static int imapdriver_subscribe_folder(mailsession * session, const char * mb)
{
  int r;

  r = mailimap_subscribe(get_imap_session(session), mb);

  return imap_error_to_mail_error(r);
}

static int imapdriver_unsubscribe_folder(mailsession * session, const char * mb)
{
  int r;

  r = mailimap_unsubscribe(get_imap_session(session), mb);

  return imap_error_to_mail_error(r);
}

/* messages operations */

static int imapdriver_append_message(mailsession * session,
				     const char * message, size_t size)
{
  int r;

  r = mailimap_append_simple(get_imap_session(session),
      get_data(session)->imap_mailbox,
      message, size);
  
  return imap_error_to_mail_error(r);
}

static int imapdriver_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  struct mailimap_flag_list * flag_list;
  int r;
  
  if (flags != NULL) {
    r = imap_flags_to_imap_flags(flags, &flag_list);
    if (r != MAIL_NO_ERROR)
      return r;
  }
  else {
    flag_list = NULL;
  }
  
  r = mailimap_append(get_imap_session(session),
      get_data(session)->imap_mailbox,
      flag_list, NULL, message, size);
  
  if (flag_list != NULL)
    mailimap_flag_list_free(flag_list);
  
  return imap_error_to_mail_error(r);
}

static int imapdriver_copy_message(mailsession * session,
				   uint32_t num, const char * mb)
{
  int r;
  struct mailimap_set * set;
  int res;

  set = mailimap_set_new_single(num);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mailimap_uid_copy(get_imap_session(session), set, mb);

  mailimap_set_free(set);

  return imap_error_to_mail_error(r);

 err:
  return res;
}

static int imapdriver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result)
{
  return imap_get_messages_list(get_imap_session(session),
      session, imap_message_driver, 1,
      result);
}



#define IMAP_SET_MAX_COUNT 100

static int
imapdriver_get_envelopes_list(mailsession * session,
			      struct mailmessage_list * env_list)
{
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  int res;
  clist * fetch_result;
  int r;
  uint32_t exists;
  clist * msg_list;
  clistiter * set_iter;
  
  if (get_imap_session(session)->imap_selection_info == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  imap_flags_store_process(get_imap_session(session),
			   get_data(session)->imap_flags_store);

  exists = get_imap_session(session)->imap_selection_info->sel_exists;

  if (exists == 0)
    return MAIL_NO_ERROR;

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

  r = imap_add_envelope_fetch_att(fetch_type);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_fetch_type;
  }

  r = maildriver_env_list_to_msg_list(env_list, &msg_list);
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  if (clist_begin(msg_list) == NULL) {
    /* no need to fetch envelopes */

    mailimap_fetch_type_free(fetch_type);
    clist_free(msg_list);
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
    
    if (clist_begin(fetch_result) == NULL) {
      res = MAIL_ERROR_FETCH;
      goto err;
    }
    
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

  return MAIL_NO_ERROR;
  
 free_fetch_type:
  mailimap_fetch_type_free(fetch_type);
 err:
  return res;
}


#if 0
static int imapdriver_search_messages(mailsession * session, const char * charset,
				      struct mail_search_key * key,
				      struct mail_search_result ** result)
{
  struct mailimap_search_key * imap_key;
  int r;
  clist * imap_result;
  clist * result_list;
  struct mail_search_result * search_result;
  clistiter * cur;

  r = mail_search_to_imap_search(key, &imap_key);
  if (r != MAIL_NO_ERROR)
    return MAIL_ERROR_MEMORY;

  r = mailimap_uid_search(get_imap_session(session), charset, imap_key,
			  &imap_result);

  mailimap_search_key_free(imap_key);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  result_list = clist_new();
  if (result_list == NULL)
    return MAIL_ERROR_MEMORY;

  for(cur = clist_begin(imap_result) ; cur != NULL ; cur = clist_next(cur)) {
    uint32_t val = * (uint32_t *) clist_content(cur);
    uint32_t * new;
    
    new = malloc(sizeof(* new));
    if (new == NULL) {
      goto free_imap_result;
    }

    * new = val;

    r = clist_append(result_list, new);
    if (r != 0) {
      free(new);
      goto free_imap_result;
    }
  }

  search_result = mail_search_result_new(result_list);
  if (search_result == NULL)
    goto free_imap_result;

  mailimap_search_result_free(imap_result);

  * result = search_result;

  return MAIL_NO_ERROR;
  
 free_imap_result:
  mailimap_search_result_free(imap_result);
  return MAIL_ERROR_MEMORY;
}
#endif

static int imapdriver_starttls(mailsession * session)
{
  mailimap * imap;
  int r;
  struct mailimap_capability_data * cap_data;
  clistiter * cur;
  int starttls;
  int fd;
  mailstream_low * low;
  mailstream_low * new_low;
  int capability_available;
  
  imap = get_imap_session(session);

  capability_available = FALSE;
  if (imap->imap_connection_info != NULL)
    if (imap->imap_connection_info->imap_capability != NULL) {
      capability_available = TRUE;
      cap_data = imap->imap_connection_info->imap_capability;
    }

  if (!capability_available) {
    r = mailimap_capability(imap, &cap_data);
    switch (r) {
    case MAILIMAP_NO_ERROR:
      break;
    default:
      return imap_error_to_mail_error(r);
    }
  }

  starttls = FALSE;
  for(cur = clist_begin(cap_data->cap_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_capability * cap;

    cap = clist_content(cur);

    if (cap->cap_type == MAILIMAP_CAPABILITY_NAME)
      if (strcasecmp(cap->cap_data.cap_name, "STARTTLS") == 0) {
	starttls = TRUE;
	break;
      }
  }

  if (!capability_available)
    mailimap_capability_data_free(cap_data);

  if (!starttls)
    return MAIL_ERROR_NO_TLS;
  
  r = mailimap_starttls(imap);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  low = mailstream_get_low(imap->imap_stream);
  fd = mailstream_low_get_fd(low);
  if (fd == -1)
    return MAIL_ERROR_STREAM;
  
  new_low = mailstream_low_tls_open(fd);
  if (new_low == NULL)
    return MAIL_ERROR_STREAM;

  mailstream_low_free(low);
  mailstream_set_low(imap->imap_stream, new_low);
  
  return MAIL_NO_ERROR;
}

static int imapdriver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, imap_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

/* Retrieve a message by UID
   
   libEtPan! uid format for IMAP is "UIDVALIDITY-UID"
   where UIDVALIDITY and UID are decimal representation of 
   respectively uidvalidity and uid numbers. 
   
   Return value:
   MAIL_ERROR_INVAL if uid is NULL or has an incorrect format.
   MAIL_ERROR_MSG_NOT_FOUND if uidvalidity has changed or uid was not found
   MAIL_NO_ERROR if message was found. Result is in result
*/

static int imapdriver_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result)
{
  uint32_t uidvalidity;
  uint32_t num;
  char * p1, * p2;
  mailimap * imap;

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

  return imapdriver_get_message(session, num, result);
}

static int imapdriver_login_sasl(mailsession * session,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
  int r;

  r = mailimap_authenticate(get_imap_session(session),
      auth_type, server_fqdn, local_ip_port, remote_ip_port,
      login, auth_name, password, realm);
  
  return imap_error_to_mail_error(r);
}


static int imapdriver_remove_message(mailsession * session, uint32_t num) {

  int res;
  struct mail_flags					*flags = NULL;

  /* protection if SELECT folder not done */
  if (get_imap_session(session)->imap_selection_info == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  /* Deleted flag */
  if ((flags = mail_flags_new_empty()) == NULL) { res = MAIL_ERROR_MEMORY; goto err;}
  flags->fl_flags = MAIL_FLAG_DELETED;

  /* STORE num \Deleted */
  /* EXPUNGE */
  if ((res = imap_store_flags(get_imap_session(session), num, num, flags)) == MAILIMAP_NO_ERROR)
		res = mailimap_expunge(get_imap_session(session));
  res = imap_error_to_mail_error(res);

err:
	if (flags != NULL) mail_flags_free( flags);
	return res;
}
