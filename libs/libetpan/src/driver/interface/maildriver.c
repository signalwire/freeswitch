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
 * $Id: maildriver.c,v 1.28 2006/06/07 15:10:01 smarinier Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildriver.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* *********************************************************************** */
/* mail session */

LIBETPAN_EXPORT
mailsession * mailsession_new(mailsession_driver * sess_driver)
{
  mailsession * session;
  int r;
  
  session = malloc(sizeof(* session));
  
  session->sess_data = NULL;
  
  if (sess_driver->sess_initialize != NULL) {
    r = sess_driver->sess_initialize(session);
    if (r != MAIL_NO_ERROR)
      goto free;
  }
  
  session->sess_driver = sess_driver;
  
  return session;

 free:
  free(session);
  return NULL;
}

LIBETPAN_EXPORT
void mailsession_free(mailsession * session)
{
  if (session->sess_driver->sess_uninitialize != NULL)
    session->sess_driver->sess_uninitialize(session);
  free(session);
}

LIBETPAN_EXPORT
int mailsession_parameters(mailsession * session,
			   int id, void * value)
{
  if (session->sess_driver->sess_parameters == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_parameters(session, id, value);
}

LIBETPAN_EXPORT
int mailsession_connect_stream(mailsession * session, mailstream * s)
{
  if (session->sess_driver->sess_connect_stream == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_connect_stream(session, s);
}

LIBETPAN_EXPORT
int mailsession_connect_path(mailsession * session, const char * path)
{
  if (session->sess_driver->sess_connect_path == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_connect_path(session, path);
}

LIBETPAN_EXPORT
int mailsession_starttls(mailsession * session)
{
  if (session->sess_driver->sess_starttls == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_starttls(session);
}

LIBETPAN_EXPORT
int mailsession_login(mailsession * session,
		      const char * userid, const char * password)
{
  if (session->sess_driver->sess_login == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_login(session, userid, password);
}

LIBETPAN_EXPORT
int mailsession_logout(mailsession * session)
{
  if (session->sess_driver->sess_logout == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_logout(session);
}

LIBETPAN_EXPORT
int mailsession_noop(mailsession * session)
{
  if (session->sess_driver->sess_noop == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;
  
  return session->sess_driver->sess_noop(session);
}

/* folders operations */

LIBETPAN_EXPORT
int mailsession_build_folder_name(mailsession * session, const char * mb,
				  const char * name, char ** result)
{
  if (session->sess_driver->sess_build_folder_name == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_build_folder_name(session,
      mb, name, result);
}

LIBETPAN_EXPORT
int mailsession_create_folder(mailsession * session, const char * mb)
{
  if (session->sess_driver->sess_create_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_create_folder(session, mb);
}

LIBETPAN_EXPORT
int mailsession_delete_folder(mailsession * session, const char * mb)
{
  if (session->sess_driver->sess_delete_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_delete_folder(session, mb);
}

LIBETPAN_EXPORT
int mailsession_rename_folder(mailsession * session,
    const char * mb, const char * new_name)
{
  if (session->sess_driver->sess_rename_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_rename_folder(session, mb, new_name);
}

LIBETPAN_EXPORT
int mailsession_check_folder(mailsession * session)
{
  if (session->sess_driver->sess_check_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_check_folder(session);
}

LIBETPAN_EXPORT
int mailsession_examine_folder(mailsession * session, const char * mb)
{
  if (session->sess_driver->sess_examine_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_examine_folder(session, mb);
}

LIBETPAN_EXPORT
int mailsession_select_folder(mailsession * session, const char * mb)
{
  if (session->sess_driver->sess_select_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_select_folder(session, mb);
}

LIBETPAN_EXPORT
int mailsession_expunge_folder(mailsession * session)
{
  if (session->sess_driver->sess_expunge_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_expunge_folder(session);
}

LIBETPAN_EXPORT
int mailsession_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen)
{
  if (session->sess_driver->sess_status_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_status_folder(session, mb,
      result_messages, result_recent, result_unseen);
}

LIBETPAN_EXPORT
int mailsession_messages_number(mailsession * session, const char * mb,
				uint32_t * result)
{
  if (session->sess_driver->sess_messages_number == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_messages_number(session, mb, result);
}

LIBETPAN_EXPORT
int mailsession_recent_number(mailsession * session, const char * mb,
			      uint32_t * result)
{
  if (session->sess_driver->sess_recent_number == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_recent_number(session, mb, result);
}

LIBETPAN_EXPORT
int mailsession_unseen_number(mailsession * session, const char * mb,
    uint32_t * result)
{
  if (session->sess_driver->sess_unseen_number == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_unseen_number(session, mb, result);
}

LIBETPAN_EXPORT
int mailsession_list_folders(mailsession * session, const char * mb,
			     struct mail_list ** result)
{
  if (session->sess_driver->sess_list_folders == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_list_folders(session, mb, result);
}

LIBETPAN_EXPORT
int mailsession_lsub_folders(mailsession * session, const char * mb,
    struct mail_list ** result)
{
  if (session->sess_driver->sess_lsub_folders == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_lsub_folders(session, mb, result);
}

LIBETPAN_EXPORT
int mailsession_subscribe_folder(mailsession * session, const char * mb)
{
  if (session->sess_driver->sess_subscribe_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_subscribe_folder(session, mb);
}

LIBETPAN_EXPORT
int mailsession_unsubscribe_folder(mailsession * session, const char * mb)
{
  if (session->sess_driver->sess_unsubscribe_folder == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_unsubscribe_folder(session, mb);
}

/* message */

LIBETPAN_EXPORT
int mailsession_append_message(mailsession * session,
			       const char * message, size_t size)
{
  if (session->sess_driver->sess_append_message == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_append_message(session, message, size);
}

LIBETPAN_EXPORT
int mailsession_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags)
{
  if (session->sess_driver->sess_append_message_flags == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_append_message_flags(session,
      message, size, flags);
}

LIBETPAN_EXPORT
int mailsession_copy_message(mailsession * session,
			     uint32_t num, const char * mb)
{
  if (session->sess_driver->sess_copy_message == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_copy_message(session, num, mb);
}

LIBETPAN_EXPORT
int mailsession_move_message(mailsession * session,
			     uint32_t num, const char * mb)
{
  if (session->sess_driver->sess_move_message == NULL) {
    int r;

    if ((session->sess_driver->sess_copy_message == NULL) &&
	(session->sess_driver->sess_remove_message == NULL))
      return MAIL_ERROR_NOT_IMPLEMENTED;

    r = mailsession_copy_message(session, num, mb);
    if (r != MAIL_NO_ERROR)
      return r;

    r = mailsession_remove_message(session, num);
    if (r != MAIL_NO_ERROR)
      return r;

    return MAIL_NO_ERROR;
  }

  return session->sess_driver->sess_move_message(session, num, mb);
}

LIBETPAN_EXPORT
int mailsession_get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list)
{
  if (session->sess_driver->sess_get_envelopes_list == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_get_envelopes_list(session, env_list);
}

LIBETPAN_EXPORT
int mailsession_get_messages_list(mailsession * session,
    struct mailmessage_list ** result)
{
  if (session->sess_driver->sess_get_messages_list == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_get_messages_list(session, result);
}

LIBETPAN_EXPORT
int mailsession_remove_message(mailsession * session, uint32_t num)
{
  if (session->sess_driver->sess_remove_message == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_remove_message(session, num);
}

#if 0
LIBETPAN_EXPORT
int mailsession_search_messages(mailsession * session, const char * charset,
    struct mail_search_key * key,
    struct mail_search_result ** result)
{
  if (session->sess_driver->sess_search_messages == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_search_messages(session,
      charset, key, result);
}
#endif

LIBETPAN_EXPORT
int mailsession_get_message(mailsession * session,
			    uint32_t num, mailmessage ** result)
{
  if (session->sess_driver->sess_get_message == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_get_message(session, num, result);
}

LIBETPAN_EXPORT
int mailsession_get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result)
{
  if (session->sess_driver->sess_get_message_by_uid == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;
  
  return session->sess_driver->sess_get_message_by_uid(session, uid, result);
}

LIBETPAN_EXPORT
int mailsession_login_sasl(mailsession * session, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
  if (session->sess_driver->sess_login_sasl == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return session->sess_driver->sess_login_sasl(session, auth_type,
      server_fqdn,
      local_ip_port,
      remote_ip_port,
      login, auth_name,
      password, realm);
}
