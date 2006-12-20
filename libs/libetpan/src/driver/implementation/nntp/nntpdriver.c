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
 * $Id: nntpdriver.c,v 1.51 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "nntpdriver.h"

#include <string.h>
#include <stdlib.h>

#include "mail.h"
#include "mailmessage.h"
#include "nntpdriver_tools.h"
#include "maildriver_tools.h"
#include "nntpdriver_message.h"

static int nntpdriver_initialize(mailsession * session);

static void nntpdriver_uninitialize(mailsession * session);

static int nntpdriver_parameters(mailsession * session,
				 int id, void * value);

static int nntpdriver_connect_stream(mailsession * session, mailstream * s);

static int nntpdriver_login(mailsession * session,
			    const char * userid, const char * password);

static int nntpdriver_logout(mailsession * session);

static int nntpdriver_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages,
    uint32_t * result_recent,
    uint32_t * result_unseen);

static int nntpdriver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result);

static int nntpdriver_append_message(mailsession * session,
				     const char * message, size_t size);

static int nntpdriver_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);

static int
nntpdriver_get_envelopes_list(mailsession * session,
			      struct mailmessage_list * env_list);


static int nntpdriver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result);

static int nntpdriver_list_folders(mailsession * session, const char * mb,
				   struct mail_list ** result);

static int nntpdriver_lsub_folders(mailsession * session, const char * mb,
				   struct mail_list ** result);

static int nntpdriver_subscribe_folder(mailsession * session, const char * mb);

static int nntpdriver_unsubscribe_folder(mailsession * session, const char * mb);

static int nntpdriver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result);

static int nntpdriver_get_message_by_uid(mailsession * session,
    const char * uid,
    mailmessage ** result);

static int nntpdriver_noop(mailsession * session);

static mailsession_driver local_nntp_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sess_name */ "nntp",

  /* sess_initialize */ nntpdriver_initialize,
  /* sess_uninitialize */ nntpdriver_uninitialize,

  /* sess_parameters */ nntpdriver_parameters,

  /* sess_connect_stream */ nntpdriver_connect_stream,
  /* sess_connect_path */ NULL,
  /* sess_starttls */ NULL,
  /* sess_login */ nntpdriver_login,
  /* sess_logout */ nntpdriver_logout,
  /* sess_noop */ nntpdriver_noop,

  /* sess_build_folder_name */ NULL,
  /* sess_create_folder */ NULL,
  /* sess_delete_folder */ NULL,
  /* sess_rename_folder */ NULL,
  /* sess_check_folder */ NULL,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ nntpdriver_select_folder,
  /* sess_expunge_folder */ NULL,
  /* sess_status_folder */ nntpdriver_status_folder,
  /* sess_messages_number */ nntpdriver_messages_number,
  /* sess_recent_number */ nntpdriver_messages_number,
  /* sess_unseen_number */ nntpdriver_messages_number,
  /* sess_list_folders */ nntpdriver_list_folders,
  /* sess_lsub_folders */ nntpdriver_lsub_folders,
  /* sess_subscribe_folder */ nntpdriver_subscribe_folder,
  /* sess_unsubscribe_folder */ nntpdriver_unsubscribe_folder,

  /* sess_append_message */ nntpdriver_append_message,
  /* sess_append_message_flags */ nntpdriver_append_message_flags,
  /* sess_copy_message */ NULL,
  /* sess_move_message */ NULL,

  /* sess_get_message */ nntpdriver_get_message,
  /* sess_get_message_by_uid */ nntpdriver_get_message_by_uid,

  /* sess_get_messages_list */ nntpdriver_get_messages_list,
  /* sess_get_envelopes_list */ nntpdriver_get_envelopes_list,
  /* sess_remove_message */ NULL,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ NULL,

#else
  .sess_name = "nntp",

  .sess_initialize = nntpdriver_initialize,
  .sess_uninitialize = nntpdriver_uninitialize,

  .sess_parameters = nntpdriver_parameters,

  .sess_connect_stream = nntpdriver_connect_stream,
  .sess_connect_path = NULL,
  .sess_starttls = NULL,
  .sess_login = nntpdriver_login,
  .sess_logout = nntpdriver_logout,
  .sess_noop = nntpdriver_noop,

  .sess_build_folder_name = NULL,
  .sess_create_folder = NULL,
  .sess_delete_folder = NULL,
  .sess_rename_folder = NULL,
  .sess_check_folder = NULL,
  .sess_examine_folder = NULL,
  .sess_select_folder = nntpdriver_select_folder,
  .sess_expunge_folder = NULL,
  .sess_status_folder = nntpdriver_status_folder,
  .sess_messages_number = nntpdriver_messages_number,
  .sess_recent_number = nntpdriver_messages_number,
  .sess_unseen_number = nntpdriver_messages_number,
  .sess_list_folders = nntpdriver_list_folders,
  .sess_lsub_folders = nntpdriver_lsub_folders,
  .sess_subscribe_folder = nntpdriver_subscribe_folder,
  .sess_unsubscribe_folder = nntpdriver_unsubscribe_folder,

  .sess_append_message = nntpdriver_append_message,
  .sess_append_message_flags = nntpdriver_append_message_flags,
  .sess_copy_message = NULL,
  .sess_move_message = NULL,

  .sess_get_messages_list = nntpdriver_get_messages_list,
  .sess_get_envelopes_list = nntpdriver_get_envelopes_list,
  .sess_remove_message = NULL,
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = nntpdriver_get_message,
  .sess_get_message_by_uid = nntpdriver_get_message_by_uid,
  .sess_login_sasl = NULL,
#endif
};


mailsession_driver * nntp_session_driver = &local_nntp_session_driver;

static inline struct nntp_session_state_data *
get_data(mailsession * session)
{
  return session->sess_data;
}

static inline newsnntp * get_nntp_session(mailsession * session)
{
  return get_data(session)->nntp_session;
}

static int nntpdriver_initialize(mailsession * session)
{
  struct nntp_session_state_data * data;
  newsnntp * nntp;

  nntp = newsnntp_new(0, NULL);
  if (nntp == NULL)
    goto err;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto free;

  data->nntp_session = nntp;

  data->nntp_userid = NULL;
  data->nntp_password = NULL;

  data->nntp_group_info = NULL;
  data->nntp_group_name = NULL;
  
  data->nntp_subscribed_list = clist_new();
  if (data->nntp_subscribed_list == NULL)
    goto free_data;

  data->nntp_max_articles = 0;

  data->nntp_mode_reader = FALSE;

  session->sess_data = data;

  return MAIL_NO_ERROR;

 free_data:
  free(data);
 free:
  newsnntp_free(nntp);
 err:
  return MAIL_ERROR_MEMORY;
}

static void nntpdriver_uninitialize(mailsession * session)
{
  struct nntp_session_state_data * data;

  data = get_data(session);

  clist_foreach(data->nntp_subscribed_list, (clist_func) free, NULL);
  clist_free(data->nntp_subscribed_list);

  if (data->nntp_group_info != NULL)
    newsnntp_group_free(data->nntp_group_info);

  if (data->nntp_group_name != NULL)
    free(data->nntp_group_name);

  if (data->nntp_userid != NULL)
    free(data->nntp_userid);

  if (data->nntp_password != NULL)
    free(data->nntp_password);

  newsnntp_free(data->nntp_session);
  free(data);

  session->sess_data = NULL;
}


static int nntpdriver_parameters(mailsession * session,
				 int id, void * value)
{
  struct nntp_session_state_data * data;

  data = get_data(session);

  switch (id) {
  case NNTPDRIVER_SET_MAX_ARTICLES:
    {
      uint32_t * param;

      param = value;

      data->nntp_max_articles = * param;
      return MAIL_NO_ERROR;
    }
  }

  return MAIL_ERROR_INVAL;
}


static int add_to_list(mailsession * session, const char * mb)
{
  char * new_mb;
  int r;
  struct nntp_session_state_data * data;

  data = get_data(session);

  new_mb = strdup(mb);
  if (new_mb == NULL)
    return -1;

  r = clist_append(data->nntp_subscribed_list, new_mb);
  if (r < 0) {
    free(new_mb);
    return -1;
  }

  return 0;
}

static int remove_from_list(mailsession * session, const char * mb)
{
  clistiter * cur;
  struct nntp_session_state_data * data;

  data = get_data(session);

  for(cur = clist_begin(data->nntp_subscribed_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    char * cur_name;

    cur_name = clist_content(cur);
    if (strcmp(cur_name, mb) == 0) {
      clist_delete(data->nntp_subscribed_list, cur);
      free(cur_name);
      return 0;
    }
  }

  return -1;
}


static int nntpdriver_connect_stream(mailsession * session, mailstream * s)
{
  int r;
  
  r = newsnntp_connect(get_nntp_session(session), s);

  switch (r) {
  case NEWSNNTP_NO_ERROR:
    return MAIL_NO_ERROR_NON_AUTHENTICATED;

  default:
    return nntpdriver_nntp_error_to_mail_error(r);
  }
}

static int nntpdriver_login(mailsession * session,
			    const char * userid, const char * password)
{
  struct nntp_session_state_data * data;
  char * new_userid;
  char * new_password;
  
  data = get_data(session);

  if (userid != NULL) {
    new_userid = strdup(userid);
    if (new_userid == NULL)
      goto err;
  }
  else
    new_userid = NULL;

  if (password != NULL) {
    new_password = strdup(password);
    if (new_password == NULL)
      goto free_uid;
  }
  else
    new_password = NULL;

  data->nntp_userid = new_userid;
  data->nntp_password = new_password;

  return MAIL_NO_ERROR;

 free_uid:
  if (new_userid != NULL)
    free(new_userid);
 err:
  return MAIL_ERROR_MEMORY;
}

static int nntpdriver_logout(mailsession * session)
{
  int r;

  r = newsnntp_quit(get_nntp_session(session));

  return nntpdriver_nntp_error_to_mail_error(r);
}


static int nntpdriver_status_folder(mailsession * session, const char * mb,
				    uint32_t * result_messages,
				    uint32_t * result_recent,
				    uint32_t * result_unseen)
{
  uint32_t count;
  int r;

  r = nntpdriver_select_folder(session, mb);
  if (r != MAIL_NO_ERROR)
    return r;
  
  r = nntpdriver_messages_number(session, mb, &count);
  if (r != MAIL_NO_ERROR)
    return r;
          
  * result_messages = count;
  * result_recent = count;
  * result_unseen = count;
  
  return MAIL_NO_ERROR;
}

static int nntpdriver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result)
{
  int r;
  struct nntp_session_state_data * data;

  if (mb != NULL) {
    r = nntpdriver_select_folder(session, mb);
    if (r != MAIL_NO_ERROR)
      return r;
  }

  data = get_data(session);
  
  if (data->nntp_group_info == NULL)
    return MAIL_ERROR_FOLDER_NOT_FOUND;

  * result = data->nntp_group_info->grp_last -
    data->nntp_group_info->grp_first + 1;

  return MAIL_NO_ERROR;
}

static int nntpdriver_list_folders(mailsession * session, const char * mb,
				    struct mail_list ** result)
{
  int r;
  clist * group_list;
  newsnntp * nntp;
  clistiter * cur;
  char * new_mb;
  int done;
  clist * list;
  struct mail_list * ml;
  int res;

  nntp = get_nntp_session(session);

  new_mb = NULL;
  if ((mb != NULL) && (*mb != '\0')) {
    new_mb = malloc(strlen(mb) + 3);
    if (new_mb == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    strcpy(new_mb, mb);
    strcat(new_mb, ".*");
  }

  done = FALSE;
  do {
    if (new_mb != NULL)
      r = newsnntp_list_active(nntp, new_mb, &group_list);
    else
      r = newsnntp_list(nntp, &group_list);

    switch (r) {
    case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME:
      r = nntpdriver_authenticate_user(session);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto err;
      }
      break;
      
    case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD:
      r = nntpdriver_authenticate_password(session);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto err;
      }
      break;

    case NEWSNNTP_NO_ERROR:
      if (new_mb != NULL)
	free(new_mb);
      done = TRUE;
      break;

    default:
      if (new_mb != NULL)
	free(new_mb);
      return nntpdriver_nntp_error_to_mail_error(r);
    }
  }
  while (!done);

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(group_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct newsnntp_group_info * info;
    char * new_name;

    info = clist_content(cur);
    new_name = strdup(info->grp_name);
    if (new_name == NULL) {
    res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = clist_append(list, new_name);
    if (r < 0) {
      free(new_name);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  ml = mail_list_new(list);
  if (ml == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  newsnntp_list_free(group_list);

  * result = ml;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) free, NULL);
  clist_free(list);
  newsnntp_list_free(group_list);
 err:
  return res;
}

static int nntpdriver_lsub_folders(mailsession * session, const char * mb,
				   struct mail_list ** result)
{
  clist * subscribed;
  clist * lsub_result;
  clistiter * cur;
  struct mail_list * lsub;
  size_t length;
  int res;
  int r;
  struct nntp_session_state_data * data;

  length = strlen(mb);

  data = get_data(session);

  subscribed = data->nntp_subscribed_list;
  lsub_result = clist_new();
  if (lsub_result == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(subscribed) ; cur != NULL ;
      cur = clist_next(cur)) {
    char * cur_mb;
    char * new_mb;
    
    cur_mb = clist_content(cur);

    if (strncmp(mb, cur_mb, length) == 0) {
      new_mb = strdup(cur_mb);
      if (new_mb == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      r = clist_append(lsub_result, new_mb);
      if (r < 0) {
	free(new_mb);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }    
  
  lsub = mail_list_new(lsub_result);
  if (lsub == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = lsub;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(lsub_result, (clist_func) free, NULL);
  clist_free(lsub_result);
 err:
  return res;
}

static int nntpdriver_subscribe_folder(mailsession * session, const char * mb)
{
  int r;

  r = add_to_list(session, mb);
  if (r < 0)
    return MAIL_ERROR_SUBSCRIBE;

  return MAIL_NO_ERROR;
}

static int nntpdriver_unsubscribe_folder(mailsession * session, const char * mb)
{
  int r;

  r = remove_from_list(session, mb);
  if (r < 0)
    return MAIL_ERROR_UNSUBSCRIBE;

  return MAIL_NO_ERROR;
}



/* messages operations */

static int nntpdriver_append_message(mailsession * session,
				     const char * message, size_t size)
{
  int r;
  struct nntp_session_state_data * data;

  data = get_data(session);

  do {
    r = newsnntp_post(get_nntp_session(session), message, size);
    switch (r) {
    case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME:
      r = nntpdriver_authenticate_user(session);
      if (r != MAIL_NO_ERROR)
	return r;
      break;
      
    case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD:
      r = nntpdriver_authenticate_password(session);
      if (r != MAIL_NO_ERROR)
	return r;
      break;

    default:
      return nntpdriver_nntp_error_to_mail_error(r);
    }
  }
  while (1);
}

static int nntpdriver_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  return nntpdriver_append_message(session, message, size);
}


static int xover_resp_to_fields(struct newsnntp_xover_resp_item * item,
				struct mailimf_fields ** result);


static int
nntpdriver_get_envelopes_list(mailsession * session,
			      struct mailmessage_list * env_list)
{
  newsnntp * nntp;
  int r;
  struct nntp_session_state_data * data;
  clist * list;
  int done;
  clistiter * cur;
  uint32_t first_seq;
  unsigned int i;

  nntp = get_nntp_session(session);

  data = get_data(session);

  if (data->nntp_group_info == NULL)
    return MAIL_ERROR_BAD_STATE;

  first_seq = data->nntp_group_info->grp_first;

  if (carray_count(env_list->msg_tab) > 0) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, 0);

    first_seq = msg->msg_index;
  }

  if (carray_count(env_list->msg_tab) > 0) {
    i = carray_count(env_list->msg_tab) - 1;
    while (1) {
      mailmessage * msg;
      
      msg = carray_get(env_list->msg_tab, i);
      
      if (msg->msg_fields != NULL) {
        first_seq = msg->msg_index + 1;
        break;
      }
      
      if (i == 0)
        break;
      
      i --;
    }
  }

  if (first_seq > data->nntp_group_info->grp_last) {
    list = NULL;
  }
  else {
    done = FALSE;
    do {
      r = newsnntp_xover_range(nntp, first_seq,
          data->nntp_group_info->grp_last, &list);
      
      switch (r) {
      case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME:
	r = nntpdriver_authenticate_user(session);
	if (r != MAIL_NO_ERROR)
	  return r;
	break;
	
      case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD:
	r = nntpdriver_authenticate_password(session);
	if (r != MAIL_NO_ERROR)
	  return r;
	break;
	
      case NEWSNNTP_NO_ERROR:
	done = TRUE;
	break;
	
      default:
	return nntpdriver_nntp_error_to_mail_error(r);
      }
    }
    while (!done);
  }

#if 0
  i = 0;
  j = 0;

  if (list != NULL) {
    for(cur = clist_begin(list) ; cur != NULL ; cur = clist_next(cur)) {
      struct newsnntp_xover_resp_item * item;
      struct mailimf_fields * fields;

      item = clist_content(cur);

      while (i < carray_count(env_list->msg_tab)) {
	mailmessage * info;

	info = carray_get(env_list->msg_tab, i);

	if (item->ovr_article == info->msg_index) {

	  if (info->fields == NULL) {
	    r = xover_resp_to_fields(item, &fields);
	    if (r == MAIL_NO_ERROR) {
	      info->fields = fields;
	    }
            
	    info->size = item->ovr_size;

	    carray_set(env_list->msg_tab, j, info);
	    j ++;
	    i ++;
	    break;
	  }
	  else {
	    carray_set(env_list->msg_tab, j, info);
	    j ++;
	  }
	}
	else {
	  if (info->fields != NULL) {
	    carray_set(env_list->msg_tab, j, info);
	    j ++;
	  }
	  else {
            if (info->flags != NULL) {
              info->flags->flags &= ~MAIL_FLAG_NEW;
              info->flags->flags |= MAIL_FLAG_SEEN | MAIL_FLAG_DELETED;
              mailmessage_check(info);
            }
	    mailmessage_free(info);
	    carray_set(env_list->msg_tab, i, NULL);
	  }
	}

	i ++;
      }
    }
  }

  while (i < carray_count(env_list->msg_tab)) {
    mailmessage * info;
    
    info = carray_get(env_list->msg_tab, i);
    if (info->fields != NULL) {
      carray_set(env_list->msg_tab, j, info);
      j ++;
    }
    else {
      if (info->flags != NULL) {
        info->flags->flags &= ~MAIL_FLAG_NEW;
        info->flags->flags |= MAIL_FLAG_SEEN | MAIL_FLAG_DELETED;
        mailmessage_check(info);
      }
      mailmessage_free(info);
      carray_set(env_list->msg_tab, i, NULL);
    }
    
    i ++;
  }

  r = carray_set_size(env_list->msg_tab, j);
  if (r < 0) {
    if (list != NULL)
      newsnntp_xover_resp_list_free(list);
    return MAIL_ERROR_MEMORY;
  }
#endif
  i = 0;

  if (list != NULL) {
    for(cur = clist_begin(list) ; cur != NULL ; cur = clist_next(cur)) {
      struct newsnntp_xover_resp_item * item;
      struct mailimf_fields * fields;

      item = clist_content(cur);

      while (i < carray_count(env_list->msg_tab)) {
	mailmessage * info;

	info = carray_get(env_list->msg_tab, i);

	if (item->ovr_article == info->msg_index) {

	  if (info->msg_fields == NULL) {
            fields = NULL;
	    r = xover_resp_to_fields(item, &fields);
	    if (r == MAIL_NO_ERROR) {
	      info->msg_fields = fields;
	    }
            
	    info->msg_size = item->ovr_size;

	    i ++;
	    break;
	  }
	}
#if 0
	else if ((info->fields == NULL) && (info->flags != NULL)) {
          info->flags->flags &= ~MAIL_FLAG_NEW;
          info->flags->flags |= MAIL_FLAG_CANCELLED;
          mailmessage_check(info);
	}
#endif
        
	i ++;
      }
    }
  }

#if 0
  while (i < env_list->msg_tab->len) {
    mailmessage * info;
    
    info = carray_get(env_list->msg_tab, i);
    if ((info->fields == NULL) && (info->flags != NULL)) {
      info->flags->flags &= ~MAIL_FLAG_NEW;
      info->flags->flags |= MAIL_FLAG_CANCELLED;
      mailmessage_check(info);
    }
    
    i ++;
  }
#endif

  if (list != NULL)
    newsnntp_xover_resp_list_free(list);

  return MAIL_NO_ERROR;
}


static int xover_resp_to_fields(struct newsnntp_xover_resp_item * item,
				struct mailimf_fields ** result)
{
  size_t cur_token;
  clist * list;
  int r;
  struct mailimf_fields * fields;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  if (item->ovr_subject != NULL) {
    char * subject_str;
    struct mailimf_subject * subject;
    struct mailimf_field * field;

    subject_str = strdup(item->ovr_subject);
    if (subject_str == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
    
    subject = mailimf_subject_new(subject_str);
    if (subject == NULL) {
      free(subject_str);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    field = mailimf_field_new(MAILIMF_FIELD_SUBJECT,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, subject, NULL, NULL, NULL);
    if (field == NULL) {
      mailimf_subject_free(subject);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = clist_append(list, field);
    if (r < 0) {
      mailimf_field_free(field);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  if (item->ovr_author != NULL) {
    struct mailimf_mailbox_list * mb_list;
    struct mailimf_from * from;
    struct mailimf_field * field;

    cur_token = 0;
    r = mailimf_mailbox_list_parse(item->ovr_author, strlen(item->ovr_author),
				   &cur_token, &mb_list);
    switch (r) {
    case MAILIMF_NO_ERROR:
      from = mailimf_from_new(mb_list);
      if (from == NULL) {
	mailimf_mailbox_list_free(mb_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_FROM,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, from,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_from_free(from);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r < 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      break;

    case MAILIMF_ERROR_PARSE:
      break;

    default:
      res = maildriver_imf_error_to_mail_error(r);
      goto free_list;
    }
  }

  if (item->ovr_date != NULL) {
    struct mailimf_date_time * date_time;
    struct mailimf_orig_date * orig_date;
    struct mailimf_field * field;

    cur_token = 0;
    r = mailimf_date_time_parse(item->ovr_date, strlen(item->ovr_date),
				&cur_token, &date_time);
    switch (r) {
    case MAILIMF_NO_ERROR:
      orig_date = mailimf_orig_date_new(date_time);
      if (orig_date == NULL) {
	mailimf_date_time_free(date_time);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_ORIG_DATE,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, orig_date, NULL,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_orig_date_free(orig_date);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r < 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      break;

    case MAILIMF_ERROR_PARSE:
      break;

    default:
      res = maildriver_imf_error_to_mail_error(r);
      goto free_list;
    }
  }

  if (item->ovr_message_id != NULL)  {
    char * msgid_str;
    struct mailimf_message_id * msgid;
    struct mailimf_field * field;

    cur_token = 0;
    r = mailimf_msg_id_parse(item->ovr_message_id, strlen(item->ovr_message_id),
			     &cur_token, &msgid_str);
    
    switch (r) {
    case MAILIMF_NO_ERROR:
      msgid = mailimf_message_id_new(msgid_str);
      if (msgid == NULL) {
	mailimf_msg_id_free(msgid_str);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_MESSAGE_ID,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL, msgid, NULL,
          NULL, NULL, NULL, NULL, NULL);

      r = clist_append(list, field);
      if (r < 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      break;

    case MAILIMF_ERROR_PARSE:
      break;

    default:
      res = maildriver_imf_error_to_mail_error(r);
      goto free_list;
    }
  }

  if (item->ovr_references != NULL) {
    clist * msgid_list;
    struct mailimf_references * references;
    struct mailimf_field * field;
    
    cur_token = 0;

    r = mailimf_msg_id_list_parse(item->ovr_references, strlen(item->ovr_references),
				  &cur_token, &msgid_list);

    switch (r) {
    case MAILIMF_NO_ERROR:
      references = mailimf_references_new(msgid_list);
      if (references == NULL) {
	clist_foreach(msgid_list,
		      (clist_func) mailimf_msg_id_free, NULL);
	clist_free(msgid_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_REFERENCES,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          references, NULL, NULL, NULL, NULL);

      r = clist_append(list, field);
      if (r < 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

    case MAILIMF_ERROR_PARSE:
      break;

    default:
      res = maildriver_imf_error_to_mail_error(r);
      goto free_list;
    }
  }

  fields = mailimf_fields_new(list);
  if (fields == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = fields;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_field_free, NULL);
  clist_free(list);
 err:
  return res;
}


/* get messages list with group info */

static int nntpdriver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result)
{
  return nntp_get_messages_list(session, session, nntp_message_driver, result);

}

static int nntpdriver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, nntp_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

static int nntpdriver_noop(mailsession * session)
{
  newsnntp * nntp;
  int r;
  struct tm tm;

  nntp = get_nntp_session(session);

  r = newsnntp_date(nntp, &tm);
  
  return nntpdriver_nntp_error_to_mail_error(r);
}

static int nntpdriver_get_message_by_uid(mailsession * session,
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
  
  return nntpdriver_get_message(session, num, result);
 }
