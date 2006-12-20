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
 * $Id: nntpdriver_tools.c,v 1.21 2006/08/05 02:34:06 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "nntpdriver_tools.h"

#include "mail.h"
#include "nntpdriver.h"
#include "nntpdriver_cached.h"
#include "newsnntp.h"
#include "maildriver_types.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "mailmessage.h"
#include "mail_cache_db.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>

int nntpdriver_nntp_error_to_mail_error(int error)
{
  switch (error) {
  case NEWSNNTP_NO_ERROR:
    return MAIL_NO_ERROR;

  case NEWSNNTP_ERROR_STREAM:
    return MAIL_ERROR_STREAM;

  case NEWSNNTP_ERROR_UNEXPECTED:
    return MAIL_ERROR_UNKNOWN;

  case NEWSNNTP_ERROR_NO_NEWSGROUP_SELECTED:
    return MAIL_ERROR_FOLDER_NOT_FOUND;

  case NEWSNNTP_ERROR_NO_ARTICLE_SELECTED:
  case NEWSNNTP_ERROR_INVALID_ARTICLE_NUMBER:
  case NEWSNNTP_ERROR_ARTICLE_NOT_FOUND:
    return MAIL_ERROR_MSG_NOT_FOUND;

  case NEWSNNTP_ERROR_UNEXPECTED_RESPONSE:
  case NEWSNNTP_ERROR_INVALID_RESPONSE:
    return MAIL_ERROR_PARSE;

  case NEWSNNTP_ERROR_NO_SUCH_NEWS_GROUP:
    return MAIL_ERROR_FOLDER_NOT_FOUND;

  case NEWSNNTP_ERROR_POSTING_NOT_ALLOWED:
    return MAIL_ERROR_READONLY;

  case NEWSNNTP_ERROR_POSTING_FAILED:
    return MAIL_ERROR_APPEND;

  case NEWSNNTP_ERROR_PROGRAM_ERROR:
    return MAIL_ERROR_PROGRAM_ERROR;

  case NEWSNNTP_ERROR_NO_PERMISSION:
    return MAIL_ERROR_NO_PERMISSION;

  case NEWSNNTP_ERROR_COMMAND_NOT_UNDERSTOOD:
  case NEWSNNTP_ERROR_COMMAND_NOT_SUPPORTED:
    return MAIL_ERROR_COMMAND_NOT_SUPPORTED;

  case NEWSNNTP_ERROR_CONNECTION_REFUSED:
    return MAIL_ERROR_CONNECT;

  case NEWSNNTP_ERROR_MEMORY:
    return MAIL_ERROR_MEMORY;

  case NEWSNNTP_ERROR_AUTHENTICATION_REJECTED:
    return MAIL_ERROR_LOGIN;

  case NEWSNNTP_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;

  case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME:
  case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD:
  default:
    return MAIL_ERROR_INVAL;
  }
}

static inline struct nntp_session_state_data *
session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline newsnntp * session_get_nntp_session(mailsession * session)
{
  return session_get_data(session)->nntp_session;
}

static inline struct nntp_cached_session_state_data *
cached_session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession * cached_session_get_ancestor(mailsession * session)
{
  return cached_session_get_data(session)->nntp_ancestor;
}

static inline struct nntp_session_state_data *
cached_session_get_ancestor_data(mailsession * session)
{
  return session_get_data(cached_session_get_ancestor(session));
}

static inline newsnntp * cached_session_get_nntp_session(mailsession * session)
{
  return session_get_nntp_session(cached_session_get_ancestor(session));
}


int nntpdriver_authenticate_password(mailsession * session)
{
  struct nntp_session_state_data * data;
  int r;

  data = session_get_data(session);

  if (data->nntp_password == NULL)
    return MAIL_ERROR_LOGIN;

  r = newsnntp_authinfo_password(session_get_nntp_session(session),
      data->nntp_password);

  return nntpdriver_nntp_error_to_mail_error(r);
}

int nntpdriver_mode_reader(mailsession * session)
{
  int done;
  int r;

  done = FALSE;

  do {
    r = newsnntp_mode_reader(session_get_nntp_session(session));
    
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
      done = TRUE;
      break;
    }
  }
  while (!done);

  return MAIL_NO_ERROR;
}

int nntpdriver_authenticate_user(mailsession * session)
{
  struct nntp_session_state_data * data;
  int r;

  data = session_get_data(session);

  if (data->nntp_userid == NULL)
    return MAIL_ERROR_LOGIN;

  r = newsnntp_authinfo_username(session_get_nntp_session(session),
      data->nntp_userid);

  switch (r) {
  case NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD:
    return nntpdriver_authenticate_password(session);

  default:
    return nntpdriver_nntp_error_to_mail_error(r);
  }
}

int nntpdriver_article(mailsession * session, uint32_t index,
		       char ** result,
		       size_t * result_len)
{
  char * msg_content;
  size_t msg_length;
  int r;
  int done;

  done = FALSE;
  do {
    r = newsnntp_article(session_get_nntp_session(session),
        index, &msg_content, &msg_length);
    
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

  * result = msg_content;
  * result_len = msg_length;

  return MAIL_NO_ERROR;
}

int nntpdriver_head(mailsession * session, uint32_t index,
		    char ** result,
		    size_t * result_len)
{
  char * headers;
  size_t headers_length;
  int r;
  int done;

  done = FALSE;
  do {
    r = newsnntp_head(session_get_nntp_session(session),
        index, &headers, &headers_length);
    
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

  * result = headers;
  * result_len = headers_length;

  return MAIL_NO_ERROR;
}

int nntpdriver_size(mailsession * session, uint32_t index,
		    size_t * result)
{
  newsnntp * nntp;
  struct newsnntp_xover_resp_item * item;
  int r;
  int done;

  nntp = session_get_nntp_session(session);

  done = FALSE;
  do {
    r = newsnntp_xover_single(nntp, index, &item);
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

  * result = item->ovr_size;

  xover_resp_item_free(item);

  return MAIL_NO_ERROR;
}

int
nntpdriver_get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    uint32_t num, 
    struct mail_flags ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mail_flags * flags;
  int res;

  snprintf(keyname, PATH_MAX, "%u-flags", num);

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
nntpdriver_write_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    uint32_t num,
    struct mail_flags * flags)
{
  int r;
  char keyname[PATH_MAX];
  int res;

  snprintf(keyname, PATH_MAX, "%u-flags", num);

  r = generic_cache_flags_write(cache_db, mmapstr, keyname, flags);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}


int nntpdriver_select_folder(mailsession * session, const char * mb)
{
  int r;
  struct newsnntp_group_info * info;
  newsnntp * nntp_session;
  struct nntp_session_state_data * data;
  char * new_name;
  int done;

  data = session_get_data(session);

  if (!data->nntp_mode_reader) {
    r = nntpdriver_mode_reader(session);
    if (r != MAIL_NO_ERROR)
      return r;
    
    data->nntp_mode_reader = TRUE;
  }

  if (data->nntp_group_name != NULL)
    if (strcmp(data->nntp_group_name, mb) == 0)
      return MAIL_NO_ERROR;

  nntp_session = session_get_nntp_session(session);

  done = FALSE;
  do {
    r = newsnntp_group(nntp_session, mb, &info);

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

  new_name = strdup(mb);
  if (new_name == NULL)
    return MAIL_ERROR_MEMORY;

  if (data->nntp_group_name != NULL)
    free(data->nntp_group_name);
  data->nntp_group_name = new_name;
  if (data->nntp_group_info != NULL)
    newsnntp_group_free(data->nntp_group_info);
  data->nntp_group_info = info;

  return MAIL_NO_ERROR;
}


int nntp_get_messages_list(mailsession * nntp_session,
			   mailsession * session,
			   mailmessage_driver * driver,
			   struct mailmessage_list ** result)
{
  carray * tab;
  struct mailmessage_list * env_list;
  uint32_t i;
  int res;
  int r;
  struct nntp_session_state_data * data;
  struct newsnntp_group_info * group_info;
  uint32_t max;
  unsigned int cur;

  data = session_get_data(nntp_session);

  if (data->nntp_group_name == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  r = nntpdriver_select_folder(nntp_session, data->nntp_group_name);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  group_info = data->nntp_group_info;
  
  if (group_info == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  max = group_info->grp_first;
  if (data->nntp_max_articles != 0) {
    if (group_info->grp_last - data->nntp_max_articles + 1 > max)
      max = group_info->grp_last - data->nntp_max_articles + 1;
  }

  tab = carray_new(128);
  if (tab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = max ; i <= group_info->grp_last ; i++) {
    mailmessage * msg;

    msg = mailmessage_new();
    if (msg == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = mailmessage_init(msg, session, driver, i, 0);
    if (r != MAIL_NO_ERROR) {
      mailmessage_free(msg);
      res = r;
      goto free_list;
    }

    r = carray_add(tab, msg, NULL);
    if (r < 0) {
      mailmessage_free(msg);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  env_list = mailmessage_list_new(tab);
  if (env_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = env_list;

  return MAIL_NO_ERROR;

 free_list:
  for(cur = 0 ; cur < carray_count(tab) ; cur ++)
    mailmessage_free(carray_get(tab, cur));
  carray_free(tab);
 err:
  return res;
}
