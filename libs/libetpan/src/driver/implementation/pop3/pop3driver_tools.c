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
 * $Id: pop3driver_tools.c,v 1.14 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "pop3driver_tools.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#include "maildriver_types.h"
#include "mailpop3.h"
#include "pop3driver.h"
#include "pop3driver_cached.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "mailmessage.h"
#include "mail_cache_db.h"

int pop3driver_pop3_error_to_mail_error(int error)
{
  switch (error) {
  case MAILPOP3_NO_ERROR:
    return MAIL_NO_ERROR;

  case MAILPOP3_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;

  case MAILPOP3_ERROR_UNAUTHORIZED:
    return MAIL_ERROR_CONNECT;

  case MAILPOP3_ERROR_STREAM:
    return MAIL_ERROR_STREAM;
    
  case MAILPOP3_ERROR_DENIED:
    return MAIL_ERROR_CONNECT;

  case MAILPOP3_ERROR_BAD_USER:
  case MAILPOP3_ERROR_BAD_PASSWORD:
    return MAIL_ERROR_LOGIN;

  case MAILPOP3_ERROR_CANT_LIST:
    return MAIL_ERROR_LIST;

  case MAILPOP3_ERROR_NO_SUCH_MESSAGE:
    return MAIL_ERROR_MSG_NOT_FOUND;

  case MAILPOP3_ERROR_MEMORY:
    return MAIL_ERROR_MEMORY;

  case MAILPOP3_ERROR_CONNECTION_REFUSED:
    return MAIL_ERROR_CONNECT;

  case MAILPOP3_ERROR_APOP_NOT_SUPPORTED:
    return MAIL_ERROR_NO_APOP;

  case MAILPOP3_ERROR_CAPA_NOT_SUPPORTED:
    return MAIL_ERROR_CAPABILITY;

  case MAILPOP3_ERROR_STLS_NOT_SUPPORTED:
    return MAIL_ERROR_NO_TLS;

  default:
    return MAIL_ERROR_INVAL;
  }
};

static inline struct pop3_session_state_data *
session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailpop3 * session_get_pop3_session(mailsession * session)
{
  return session_get_data(session)->pop3_session;
}

static inline struct pop3_cached_session_state_data *
cached_session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession *
cached_session_get_ancestor(mailsession * session)
{
  return cached_session_get_data(session)->pop3_ancestor;
}

static inline struct pop3_session_state_data *
cached_session_get_ancestor_data(mailsession * session)
{
  return session_get_data(cached_session_get_ancestor(session));
}

static inline mailpop3 * 
cached_session_get_pop3_session(mailsession * session)
{
  return session_get_pop3_session(cached_session_get_ancestor(session));
}


int pop3driver_retr(mailsession * session, uint32_t index,
		    char ** result, size_t * result_len)
{
  char * msg_content;
  size_t msg_length;
  int r;

  r = mailpop3_retr(session_get_pop3_session(session), index,
      &msg_content, &msg_length);
  
  switch (r) {
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return pop3driver_pop3_error_to_mail_error(r);
  }

  * result = msg_content;
  * result_len = msg_length;

  return MAIL_NO_ERROR;
}

int pop3driver_header(mailsession * session, uint32_t index,
		      char ** result,
		      size_t * result_len)
{
  char * headers;
  size_t headers_length;
  int r;
  
  r = mailpop3_header(session_get_pop3_session(session),
      index, &headers, &headers_length);
  
  switch (r) {
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return pop3driver_pop3_error_to_mail_error(r);
  }
  
  * result = headers;
  * result_len = headers_length;

  return MAIL_NO_ERROR;
}

int pop3driver_size(mailsession * session, uint32_t index,
		    size_t * result)
{
  mailpop3 * pop3;
  carray * msg_tab;
  struct mailpop3_msg_info * info;
  int r;

  pop3 = session_get_pop3_session(session);

  mailpop3_list(pop3, &msg_tab);

  r = mailpop3_get_msg_info(pop3, index, &info);
  switch (r) {
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return pop3driver_pop3_error_to_mail_error(r);
  }

  * result = info->msg_size;

  return MAIL_NO_ERROR;
}

int
pop3driver_get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session,
    uint32_t num, 
    struct mail_flags ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mail_flags * flags;
  int res;
  struct mailpop3_msg_info * info;

  r = mailpop3_get_msg_info(cached_session_get_pop3_session(session),
      num, &info);
  switch (r) {
  case MAILPOP3_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;
  case MAILPOP3_ERROR_NO_SUCH_MESSAGE:
    return MAIL_ERROR_MSG_NOT_FOUND;
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return MAIL_ERROR_FETCH;
  }

  snprintf(keyname, PATH_MAX, "%s-flags", info->msg_uidl);

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
pop3driver_write_cached_flags(struct mail_cache_db * cache_db,
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

int pop3_get_messages_list(mailpop3 * pop3,
			   mailsession * session,
			   mailmessage_driver * driver,
			   struct mailmessage_list ** result)
{
  carray * msg_tab;
  carray * tab;
  struct mailmessage_list * env_list;
  unsigned int i;
  int res;
  int r;

  mailpop3_list(pop3, &msg_tab);

  tab = carray_new(128);
  if (tab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < carray_count(msg_tab) ; i++) {
    struct mailpop3_msg_info * pop3_info;
    mailmessage * msg;

    pop3_info = carray_get(msg_tab, i);

    if (pop3_info == NULL)
      continue;

    if (pop3_info->msg_deleted)
      continue;
      
    msg = mailmessage_new();
    if (msg == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = mailmessage_init(msg, session, driver,
        (uint32_t) pop3_info->msg_index, pop3_info->msg_size);
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

  env_list = mailmessage_list_new(/*list*/ tab);
  if (env_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = env_list;

  return MAIL_NO_ERROR;

 free_list:
  for(i = 0 ; i < carray_count(tab) ; i ++)
    mailmessage_free(carray_get(tab, i));
  carray_free(tab);
 err:
  return res;
}
