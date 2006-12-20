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
 * $Id: pop3driver.c,v 1.38 2006/06/28 06:13:47 skunk Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "pop3driver.h"

#include <string.h>
#include <stdlib.h>

#include "pop3driver_message.h"
#include "maildriver_tools.h"
#include "pop3driver_tools.h"
#include "mailmessage.h"

static int pop3driver_initialize(mailsession * session);

static void pop3driver_uninitialize(mailsession * session);

static int pop3driver_parameters(mailsession * session,
    int id, void * value);

static int pop3driver_connect_stream(mailsession * session, mailstream * s);

static int pop3driver_starttls(mailsession * session);

static int pop3driver_login(mailsession * session,
			    const char * userid, const char * password);

static int pop3driver_logout(mailsession * session);

static int pop3driver_noop(mailsession * session);

static int pop3driver_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);

static int pop3driver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result);

static int pop3driver_remove_message(mailsession * session, uint32_t num);

static int pop3driver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result);

static int pop3driver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result);

static int pop3driver_login_sasl(mailsession * session,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);

static mailsession_driver local_pop3_session_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
	
  /* sess_name */ "pop3",

  /* sess_initialize */ pop3driver_initialize,
  /* sess_uninitialize */ pop3driver_uninitialize,

  /* sess_parameters */ pop3driver_parameters,

  /* sess_connect_stream */ pop3driver_connect_stream,
  /* sess_connect_path */ NULL,
  /* sess_starttls */ pop3driver_starttls,
  /* sess_login */ pop3driver_login,
  /* sess_logout */ pop3driver_logout,
  /* sess_noop */ pop3driver_noop,

  /* sess_build_folder_name */ NULL,
  /* sess_create_folder */ NULL,
  /* sess_delete_folder */ NULL,
  /* sess_rename_folder */ NULL,
  /* sess_check_folder */ NULL,
  /* sess_examine_folder */ NULL,
  /* sess_select_folder */ NULL,
  /* sess_expunge_folder */ NULL,
  /* sess_status_folder */ pop3driver_status_folder,
  /* sess_messages_number */ pop3driver_messages_number,
  /* sess_recent_number */ pop3driver_messages_number,
  /* sess_unseen_number */ pop3driver_messages_number,
  /* sess_list_folders */ NULL,
  /* sess_lsub_folders */ NULL,
  /* sess_subscribe_folder */ NULL,
  /* sess_unsubscribe_folder */ NULL,

  /* sess_append_message */ NULL,
  /* sess_append_message_flags */ NULL,
  /* sess_copy_message */ NULL,
  /* sess_move_message */ NULL,

  /* sess_get_message */ pop3driver_get_message,
  /* sess_get_message_by_uid */ NULL,

  /* sess_get_messages_list */ pop3driver_get_messages_list,
  /* sess_get_envelopes_list */ maildriver_generic_get_envelopes_list,
  /* sess_remove_message */ pop3driver_remove_message,
#if 0
  /* sess_search_messages */ maildriver_generic_search_messages,
#endif
  /* sess_login_sasl */ pop3driver_login_sasl,
  
#else
  .sess_name = "pop3",

  .sess_initialize = pop3driver_initialize,
  .sess_uninitialize = pop3driver_uninitialize,

  .sess_parameters = pop3driver_parameters,

  .sess_connect_stream = pop3driver_connect_stream,
  .sess_connect_path = NULL,
  .sess_starttls = pop3driver_starttls,
  .sess_login = pop3driver_login,
  .sess_logout = pop3driver_logout,
  .sess_noop = pop3driver_noop,

  .sess_build_folder_name = NULL,
  .sess_create_folder = NULL,
  .sess_delete_folder = NULL,
  .sess_rename_folder = NULL,
  .sess_check_folder = NULL,
  .sess_examine_folder = NULL,
  .sess_select_folder = NULL,
  .sess_expunge_folder = NULL,
  .sess_status_folder = pop3driver_status_folder,
  .sess_messages_number = pop3driver_messages_number,
  .sess_recent_number = pop3driver_messages_number,
  .sess_unseen_number = pop3driver_messages_number,
  .sess_list_folders = NULL,
  .sess_lsub_folders = NULL,
  .sess_subscribe_folder = NULL,
  .sess_unsubscribe_folder = NULL,

  .sess_append_message = NULL,
  .sess_append_message_flags = NULL,
  .sess_copy_message = NULL,
  .sess_move_message = NULL,

  .sess_get_messages_list = pop3driver_get_messages_list,
  .sess_get_envelopes_list = maildriver_generic_get_envelopes_list,
  .sess_remove_message = pop3driver_remove_message,
#if 0
  .sess_search_messages = maildriver_generic_search_messages,
#endif

  .sess_get_message = pop3driver_get_message,
  .sess_get_message_by_uid = NULL,
  .sess_login_sasl = pop3driver_login_sasl,
#endif
};

mailsession_driver * pop3_session_driver = &local_pop3_session_driver;

static inline struct pop3_session_state_data *
get_data(mailsession * session)
{
  return session->sess_data;
}

static mailpop3 * get_pop3_session(mailsession * session)
{
  return get_data(session)->pop3_session;
}

static int pop3driver_initialize(mailsession * session)
{
  struct pop3_session_state_data * data;
  mailpop3 * pop3;

  pop3 = mailpop3_new(0, NULL);
  if (session == NULL)
    goto err;

  data = malloc(sizeof(* data));
  if (data == NULL)
    goto free;

  data->pop3_session = pop3;
  data->pop3_auth_type = POP3DRIVER_AUTH_TYPE_PLAIN;

  session->sess_data = data;

  return MAIL_NO_ERROR;

 free:
  mailpop3_free(pop3);
 err:
  return MAIL_ERROR_MEMORY;
}

static void pop3driver_uninitialize(mailsession * session)
{
  struct pop3_session_state_data * data;

  data = get_data(session);

  mailpop3_free(data->pop3_session);
  free(data);
  
  session->sess_data = data;
}

static int pop3driver_connect_stream(mailsession * session, mailstream * s)
{
  int r;
 
  r = mailpop3_connect(get_pop3_session(session), s);

  switch (r) {
  case MAILPOP3_NO_ERROR:
    return MAIL_NO_ERROR_NON_AUTHENTICATED;

  default:
    return pop3driver_pop3_error_to_mail_error(r);
  }
}

static int pop3driver_starttls(mailsession * session)
{
  int r;
  int fd;
  mailstream_low * low;
  mailstream_low * new_low;
  mailpop3 * pop3;

  pop3 = get_pop3_session(session);

  r = mailpop3_stls(pop3);

  switch (r) {
  case MAILPOP3_NO_ERROR:
    break;
  default:
    return pop3driver_pop3_error_to_mail_error(r);
  }

  low = mailstream_get_low(pop3->pop3_stream);
  fd = mailstream_low_get_fd(low);
  if (fd == -1)
    return MAIL_ERROR_STREAM;
  
  new_low = mailstream_low_tls_open(fd);
  if (new_low == NULL)
    return MAIL_ERROR_STREAM;
  mailstream_low_free(low);
  mailstream_set_low(pop3->pop3_stream, new_low);
  
  return MAIL_NO_ERROR;
}

static int pop3driver_parameters(mailsession * session,
    int id, void * value)
{
  struct pop3_session_state_data * data;

  data = get_data(session);

  switch (id) {
  case POP3DRIVER_SET_AUTH_TYPE:
    {
      int * param;

      param = value;

      data->pop3_auth_type = * param;
      return MAIL_NO_ERROR;
    }
  }

  return MAIL_ERROR_INVAL;
}

static int pop3driver_login(mailsession * session,
			    const char * userid, const char * password)
{
  int r;
  carray * msg_tab;
  struct pop3_session_state_data * data;

  data = get_data(session);

  switch (data->pop3_auth_type) {
  case POP3DRIVER_AUTH_TYPE_TRY_APOP:
    r = mailpop3_login_apop(get_pop3_session(session), userid, password);
    if (r != MAILPOP3_NO_ERROR)
      r = mailpop3_login(get_pop3_session(session), userid, password);
    break;

  case POP3DRIVER_AUTH_TYPE_APOP:
    r = mailpop3_login_apop(get_pop3_session(session), userid, password);
    break;

  default:
  case POP3DRIVER_AUTH_TYPE_PLAIN:
    r = mailpop3_login(get_pop3_session(session), userid, password);
    break;
  }

  mailpop3_list(get_pop3_session(session), &msg_tab);

  return pop3driver_pop3_error_to_mail_error(r);
}

static int pop3driver_logout(mailsession * session)
{
  int r;

  r = mailpop3_quit(get_pop3_session(session));

  return pop3driver_pop3_error_to_mail_error(r);
}

static int pop3driver_noop(mailsession * session)
{
  int r;

  r = mailpop3_noop(get_pop3_session(session));

  return pop3driver_pop3_error_to_mail_error(r);
}

static int pop3driver_status_folder(mailsession * session, const char * mb,
				    uint32_t * result_messages,
				    uint32_t * result_recent,
				    uint32_t * result_unseen)
{
  uint32_t count;
  int r;
  
  r = pop3driver_messages_number(session, mb, &count);
  if (r != MAIL_NO_ERROR)
    return r;
          
  * result_messages = count;
  * result_recent = count;
  * result_unseen = count;
  
  return MAIL_NO_ERROR;
}

static int pop3driver_messages_number(mailsession * session, const char * mb,
				      uint32_t * result)
{
  carray * msg_tab;

  mailpop3_list(get_pop3_session(session), &msg_tab);

  * result = carray_count(msg_tab) -
    get_pop3_session(session)->pop3_deleted_count;

  return MAIL_NO_ERROR;
}


/* messages operations */

static int pop3driver_remove_message(mailsession * session, uint32_t num)
{
  mailpop3 * pop3;
  int r;

  pop3 = get_pop3_session(session);

  r = mailpop3_dele(pop3, num);
  switch (r) {
  case MAILPOP3_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;

  case MAILPOP3_ERROR_NO_SUCH_MESSAGE:
    return MAIL_ERROR_MSG_NOT_FOUND;

  case MAILPOP3_ERROR_STREAM:
    return MAIL_ERROR_STREAM;

  case MAILPOP3_NO_ERROR:
    return MAIL_NO_ERROR;

  default:
    return MAIL_ERROR_REMOVE;
  }
}

static int pop3driver_get_messages_list(mailsession * session,
					struct mailmessage_list ** result)
{
  mailpop3 * pop3;

  pop3 = get_pop3_session(session);

  return pop3_get_messages_list(pop3, session,
				pop3_message_driver, result);
}

static int pop3driver_get_message(mailsession * session,
				  uint32_t num, mailmessage ** result)
{
  mailmessage * msg_info;
  int r;

  msg_info = mailmessage_new();
  if (msg_info == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(msg_info, session, pop3_message_driver, num, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(msg_info);
    return r;
  }

  * result = msg_info;

  return MAIL_NO_ERROR;
}

static int pop3driver_login_sasl(mailsession * session,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
  int r;

  r = mailpop3_auth(get_pop3_session(session),
      auth_type, server_fqdn, local_ip_port, remote_ip_port,
      login, auth_name, password, realm);
  
  return pop3driver_pop3_error_to_mail_error(r);
}
