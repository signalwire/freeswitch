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
 * $Id: imapstorage.c,v 1.17 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imapstorage.h"

#include <stdlib.h>
#include <string.h>

#include "mail.h"
#include "imapdriver.h"
#include "imapdriver_cached.h"
#include "mailstorage_tools.h"
#include "maildriver.h"

/* imap storage */

#define IMAP_DEFAULT_PORT  143
#define IMAPS_DEFAULT_PORT 993

static int imap_mailstorage_connect(struct mailstorage * storage);
static int
imap_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result);
static void imap_mailstorage_uninitialize(struct mailstorage * storage);

static mailstorage_driver imap_mailstorage_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sto_name               */ "imap",
  /* sto_connect            */ imap_mailstorage_connect,
  /* sto_get_folder_session */ imap_mailstorage_get_folder_session,
  /* sto_uninitialize       */ imap_mailstorage_uninitialize,
#else
  .sto_name               = "imap",
  .sto_connect            = imap_mailstorage_connect,
  .sto_get_folder_session = imap_mailstorage_get_folder_session,
  .sto_uninitialize       = imap_mailstorage_uninitialize,
#endif
};

LIBETPAN_EXPORT
int imap_mailstorage_init(struct mailstorage * storage,
    const char * imap_servername, uint16_t imap_port,
    const char * imap_command,
    int imap_connection_type, int imap_auth_type,
    const char * imap_login, const char * imap_password,
    int imap_cached, const char * imap_cache_directory)
{
  struct imap_mailstorage * imap_storage;
  int r;
  
  r = imap_mailstorage_init_sasl(storage,
      imap_servername, imap_port,
      imap_command,
      imap_connection_type,
      NULL,
      NULL,
      NULL, NULL,
      imap_login, imap_login,
      imap_password, NULL,
      imap_cached, imap_cache_directory);
  
  if (r == MAIL_NO_ERROR) {
    imap_storage = storage->sto_data;
    imap_storage->imap_auth_type = imap_auth_type;
    imap_storage->imap_login = imap_storage->imap_sasl.sasl_login;
    imap_storage->imap_password = imap_storage->imap_sasl.sasl_password;
  }
  
  return r;
}

LIBETPAN_EXPORT
int imap_mailstorage_init_sasl(struct mailstorage * storage,
    const char * imap_servername, uint16_t imap_port,
    const char * imap_command,
    int imap_connection_type,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm,
    int imap_cached, const char * imap_cache_directory)
{
  struct imap_mailstorage * imap_storage;

  imap_storage = malloc(sizeof(* imap_storage));
  if (imap_storage == NULL)
    goto err;

  imap_storage->imap_servername = strdup(imap_servername);
  if (imap_storage->imap_servername == NULL)
    goto free;

  imap_storage->imap_connection_type = imap_connection_type;
  
  if (imap_port == 0) {
    switch (imap_connection_type) {
    case CONNECTION_TYPE_PLAIN:
    case CONNECTION_TYPE_TRY_STARTTLS:
    case CONNECTION_TYPE_STARTTLS:
    case CONNECTION_TYPE_COMMAND:
    case CONNECTION_TYPE_COMMAND_TRY_STARTTLS:
    case CONNECTION_TYPE_COMMAND_STARTTLS:
      imap_port = IMAP_DEFAULT_PORT;
      break;

    case CONNECTION_TYPE_TLS:
    case CONNECTION_TYPE_COMMAND_TLS:
      imap_port = IMAPS_DEFAULT_PORT;
      break;
    }
  }

  imap_storage->imap_port = imap_port;

  if (imap_command != NULL) {
    imap_storage->imap_command = strdup(imap_command);
    if (imap_storage->imap_command == NULL)
      goto free_servername;
  }
  else
    imap_storage->imap_command = NULL;
  
  imap_storage->imap_auth_type = IMAP_AUTH_TYPE_PLAIN;
  
  imap_storage->imap_sasl.sasl_enabled = (auth_type != NULL);
  
  if (auth_type != NULL) {
    imap_storage->imap_sasl.sasl_auth_type = strdup(auth_type);
    if (imap_storage->imap_sasl.sasl_auth_type == NULL)
      goto free_command;
  }
  else
    imap_storage->imap_sasl.sasl_auth_type = NULL;
  
  if (server_fqdn != NULL) {
    imap_storage->imap_sasl.sasl_server_fqdn = strdup(server_fqdn);
    if (imap_storage->imap_sasl.sasl_server_fqdn == NULL)
      goto free_auth_type;
  }
  else
    imap_storage->imap_sasl.sasl_server_fqdn = NULL;
  
  if (local_ip_port != NULL) {
    imap_storage->imap_sasl.sasl_local_ip_port = strdup(local_ip_port);
    if (imap_storage->imap_sasl.sasl_local_ip_port == NULL)
      goto free_server_fqdn;
  }
  else
    imap_storage->imap_sasl.sasl_local_ip_port = NULL;
  
  if (remote_ip_port != NULL) {
    imap_storage->imap_sasl.sasl_remote_ip_port = strdup(remote_ip_port);
    if (imap_storage->imap_sasl.sasl_remote_ip_port == NULL)
      goto free_local_ip_port;
  }
  else
    imap_storage->imap_sasl.sasl_remote_ip_port = NULL;
  
  if (login != NULL) {
    imap_storage->imap_sasl.sasl_login = strdup(login);
    if (imap_storage->imap_sasl.sasl_login == NULL)
      goto free_remote_ip_port;
  }
  else
    imap_storage->imap_sasl.sasl_login = NULL;

  if (auth_name != NULL) {
    imap_storage->imap_sasl.sasl_auth_name = strdup(auth_name);
    if (imap_storage->imap_sasl.sasl_auth_name == NULL)
      goto free_login;
  }
  else
    imap_storage->imap_sasl.sasl_auth_name = NULL;

  if (password != NULL) {
    imap_storage->imap_sasl.sasl_password = strdup(password);
    if (imap_storage->imap_sasl.sasl_password == NULL)
      goto free_auth_name;
  }
  else
    imap_storage->imap_sasl.sasl_password = NULL;

  if (realm != NULL) {
    imap_storage->imap_sasl.sasl_realm = strdup(realm);
    if (imap_storage->imap_sasl.sasl_realm == NULL)
      goto free_password;
  }
  else
    imap_storage->imap_sasl.sasl_realm = NULL;

  imap_storage->imap_cached = imap_cached;

  if (imap_cached && (imap_cache_directory != NULL)) {
    imap_storage->imap_cache_directory = strdup(imap_cache_directory);
    if (imap_storage->imap_cache_directory == NULL)
      goto free_realm;
  }
  else {
    imap_storage->imap_cached = FALSE;
    imap_storage->imap_cache_directory = NULL;
  }

  storage->sto_data = imap_storage;
  storage->sto_driver = &imap_mailstorage_driver;

  return MAIL_NO_ERROR;

 free_realm:
  free(imap_storage->imap_sasl.sasl_realm);
 free_password:
  free(imap_storage->imap_sasl.sasl_password);
 free_auth_name:
  free(imap_storage->imap_sasl.sasl_auth_name);
 free_login:
  free(imap_storage->imap_sasl.sasl_login);
 free_remote_ip_port:
  free(imap_storage->imap_sasl.sasl_remote_ip_port);
 free_local_ip_port:
  free(imap_storage->imap_sasl.sasl_local_ip_port);
 free_server_fqdn:
  free(imap_storage->imap_sasl.sasl_server_fqdn);
 free_auth_type:
  free(imap_storage->imap_sasl.sasl_auth_type);
 free_command:
  free(imap_storage->imap_command);
 free_servername:
  free(imap_storage->imap_servername);
 free:
  free(imap_storage);
 err:
  return MAIL_ERROR_MEMORY;
}

static void imap_mailstorage_uninitialize(struct mailstorage * storage)
{
  struct imap_mailstorage * imap_storage;

  imap_storage = storage->sto_data;

  if (imap_storage->imap_cache_directory != NULL)
    free(imap_storage->imap_cache_directory);
  
  free(imap_storage->imap_sasl.sasl_realm);
  free(imap_storage->imap_sasl.sasl_password);
  free(imap_storage->imap_sasl.sasl_auth_name);
  free(imap_storage->imap_sasl.sasl_login);
  free(imap_storage->imap_sasl.sasl_remote_ip_port);
  free(imap_storage->imap_sasl.sasl_local_ip_port);
  free(imap_storage->imap_sasl.sasl_server_fqdn);
  free(imap_storage->imap_sasl.sasl_auth_type);
  
  if (imap_storage->imap_command != NULL)
    free(imap_storage->imap_command);
  free(imap_storage->imap_servername);
  free(imap_storage);
  
  storage->sto_data = NULL;
}

static int imap_connect(struct mailstorage * storage,
    mailsession ** result)
{
  struct imap_mailstorage * imap_storage;
  mailsession_driver * driver;
  int r;
  int res;
  mailsession * session;

  imap_storage = storage->sto_data;

  if (imap_storage->imap_cached)
    driver = imap_cached_session_driver;
  else
    driver = imap_session_driver;
  
  r = mailstorage_generic_connect(driver,
      imap_storage->imap_servername,
      imap_storage->imap_port,
      imap_storage->imap_command,
      imap_storage->imap_connection_type,
      IMAPDRIVER_CACHED_SET_CACHE_DIRECTORY,
      imap_storage->imap_cache_directory,
      0, NULL,
      &session);
  switch (r) {
  case MAIL_NO_ERROR_NON_AUTHENTICATED:
  case MAIL_NO_ERROR_AUTHENTICATED:
  case MAIL_NO_ERROR:
    break;
  default:
    res = r;
    goto err;
  }

  if (imap_storage->imap_sasl.sasl_enabled) {
    r = mailstorage_generic_auth_sasl(session, r,
        imap_storage->imap_sasl.sasl_auth_type,
        imap_storage->imap_sasl.sasl_server_fqdn,
        imap_storage->imap_sasl.sasl_local_ip_port,
        imap_storage->imap_sasl.sasl_remote_ip_port,
        imap_storage->imap_sasl.sasl_login,
        imap_storage->imap_sasl.sasl_auth_name,
        imap_storage->imap_sasl.sasl_password,
        imap_storage->imap_sasl.sasl_realm);
  }
  else {
    r = mailstorage_generic_auth(session, r,
        imap_storage->imap_auth_type,
        imap_storage->imap_sasl.sasl_login,
        imap_storage->imap_sasl.sasl_password);
  }
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  * result = session;

  return MAIL_NO_ERROR;

 free:
  mailsession_free(session);
 err:
  return res;
}

static int imap_mailstorage_connect(struct mailstorage * storage)
{
  mailsession * session;
  int r;
  int res;

  r = imap_connect(storage, &session);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailsession_select_folder(session, "INBOX");
  if (r != MAIL_NO_ERROR) {
    mailsession_logout(session);
    res = r;
    goto err;
  }

  storage->sto_session = session;
  storage->sto_driver = &imap_mailstorage_driver;

  return MAIL_NO_ERROR;

 err:
  return res;
}

static int
imap_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result)
{
  mailsession * session;
  int r;
  int res;

  if (strcasecmp(pathname, "INBOX") == 0) {
    session = storage->sto_session;
  }
  else {
    r = imap_connect(storage, &session);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }

    r = mailsession_select_folder(session, pathname);
    if (r != MAIL_NO_ERROR) {
      mailsession_logout(session);
      res = r;
      goto free;
    }
  }

  * result = session;
  
  return MAIL_NO_ERROR;

 free:
  mailsession_free(session);
 err:
  return res;
}
