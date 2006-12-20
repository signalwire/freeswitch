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
 * $Id: pop3storage.c,v 1.17 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "pop3storage.h"

#include <stdlib.h>
#include <string.h>

#include "mail.h"
#include "mailstorage_tools.h"
#include "maildriver.h"

/* pop3 storage */

#define POP3_DEFAULT_PORT  110
#define POP3S_DEFAULT_PORT 995

static int pop3_mailstorage_connect(struct mailstorage * storage);
static int pop3_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result);
static void pop3_mailstorage_uninitialize(struct mailstorage * storage);

static mailstorage_driver pop3_mailstorage_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sto_name               */ "pop3",
  /* sto_connect            */ pop3_mailstorage_connect,
  /* sto_get_folder_session */ pop3_mailstorage_get_folder_session,
  /* sto_uninitialize       */ pop3_mailstorage_uninitialize,
#else
  .sto_name               = "pop3",
  .sto_connect            = pop3_mailstorage_connect,
  .sto_get_folder_session = pop3_mailstorage_get_folder_session,
  .sto_uninitialize       = pop3_mailstorage_uninitialize,
#endif
};

LIBETPAN_EXPORT
int pop3_mailstorage_init(struct mailstorage * storage,
    const char * pop3_servername, uint16_t pop3_port,
    const char * pop3_command,
    int pop3_connection_type, int pop3_auth_type,
    const char * pop3_login, const char * pop3_password,
    int pop3_cached, const char * pop3_cache_directory, const char * pop3_flags_directory)
{
  struct pop3_mailstorage * pop3_storage;
  int r;
  
  r =  pop3_mailstorage_init_sasl(storage,
      pop3_servername, pop3_port,
      pop3_command,
      pop3_connection_type,
      NULL,
      NULL,
      NULL, NULL,
      pop3_login, pop3_login,
      pop3_password, NULL,
      pop3_cached, pop3_cache_directory,
      pop3_flags_directory);
  
  if (r == MAIL_NO_ERROR) {
    pop3_storage = storage->sto_data;
    pop3_storage->pop3_auth_type = pop3_auth_type;
    pop3_storage->pop3_login = pop3_storage->pop3_sasl.sasl_login;
    pop3_storage->pop3_password = pop3_storage->pop3_sasl.sasl_password;
  }
  
  return r;
}

LIBETPAN_EXPORT
int pop3_mailstorage_init_sasl(struct mailstorage * storage,
    const char * pop3_servername, uint16_t pop3_port,
    const char * pop3_command,
    int pop3_connection_type,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm,
    int pop3_cached, const char * pop3_cache_directory,
    const char * pop3_flags_directory)
{
  struct pop3_mailstorage * pop3_storage;

  pop3_storage = malloc(sizeof(* pop3_storage));
  if (pop3_storage == NULL)
    goto err;
  
  pop3_storage->pop3_servername = strdup(pop3_servername);
  if (pop3_storage->pop3_servername == NULL)
    goto free;

  pop3_storage->pop3_connection_type = pop3_connection_type;
  
  if (pop3_port == 0) {
    switch (pop3_connection_type) {
    case CONNECTION_TYPE_PLAIN:
    case CONNECTION_TYPE_TRY_STARTTLS:
    case CONNECTION_TYPE_STARTTLS:
    case CONNECTION_TYPE_COMMAND:
    case CONNECTION_TYPE_COMMAND_TRY_STARTTLS:
    case CONNECTION_TYPE_COMMAND_STARTTLS:
      pop3_port = POP3_DEFAULT_PORT;
      break;

    case CONNECTION_TYPE_TLS:
    case CONNECTION_TYPE_COMMAND_TLS:
      pop3_port = POP3S_DEFAULT_PORT;
      break;
    }
  }

  pop3_storage->pop3_port = pop3_port;
  
  if (pop3_command != NULL) {
    pop3_storage->pop3_command = strdup(pop3_command);
    if (pop3_storage->pop3_command == NULL)
      goto free_servername;
  }
  else
    pop3_storage->pop3_command = NULL;
  
  pop3_storage->pop3_auth_type = POP3_AUTH_TYPE_PLAIN;
  
  pop3_storage->pop3_sasl.sasl_enabled = (auth_type != NULL);
  
  if (auth_type != NULL) {
    pop3_storage->pop3_sasl.sasl_auth_type = strdup(auth_type);
    if (pop3_storage->pop3_sasl.sasl_auth_type == NULL)
      goto free_command;
  }
  else
    pop3_storage->pop3_sasl.sasl_auth_type = NULL;
  
  if (server_fqdn != NULL) {
    pop3_storage->pop3_sasl.sasl_server_fqdn = strdup(server_fqdn);
    if (pop3_storage->pop3_sasl.sasl_server_fqdn == NULL)
      goto free_auth_type;
  }
  else
    pop3_storage->pop3_sasl.sasl_server_fqdn = NULL;
  
  if (local_ip_port != NULL) {
    pop3_storage->pop3_sasl.sasl_local_ip_port = strdup(local_ip_port);
    if (pop3_storage->pop3_sasl.sasl_local_ip_port == NULL)
      goto free_server_fqdn;
  }
  else
    pop3_storage->pop3_sasl.sasl_local_ip_port = NULL;
  
  if (remote_ip_port != NULL) {
    pop3_storage->pop3_sasl.sasl_remote_ip_port = strdup(remote_ip_port);
    if (pop3_storage->pop3_sasl.sasl_remote_ip_port == NULL)
      goto free_local_ip_port;
  }
  else
    pop3_storage->pop3_sasl.sasl_remote_ip_port = NULL;
  
  if (login != NULL) {
    pop3_storage->pop3_sasl.sasl_login = strdup(login);
    if (pop3_storage->pop3_sasl.sasl_login == NULL)
      goto free_remote_ip_port;
  }
  else
    pop3_storage->pop3_sasl.sasl_login = NULL;

  if (auth_name != NULL) {
    pop3_storage->pop3_sasl.sasl_auth_name = strdup(auth_name);
    if (pop3_storage->pop3_sasl.sasl_auth_name == NULL)
      goto free_login;
  }
  else
    pop3_storage->pop3_sasl.sasl_auth_name = NULL;

  if (password != NULL) {
    pop3_storage->pop3_sasl.sasl_password = strdup(password);
    if (pop3_storage->pop3_sasl.sasl_password == NULL)
      goto free_auth_name;
  }
  else
    pop3_storage->pop3_sasl.sasl_password = NULL;

  if (realm != NULL) {
    pop3_storage->pop3_sasl.sasl_realm = strdup(realm);
    if (pop3_storage->pop3_sasl.sasl_realm == NULL)
      goto free_password;
  }
  else
    pop3_storage->pop3_sasl.sasl_realm = NULL;
  
  pop3_storage->pop3_cached = pop3_cached;

  if (pop3_cached && (pop3_cache_directory != NULL) &&
      (pop3_flags_directory != NULL)) {
    pop3_storage->pop3_cache_directory = strdup(pop3_cache_directory);
    if (pop3_storage->pop3_cache_directory == NULL)
      goto free_realm;
    pop3_storage->pop3_flags_directory = strdup(pop3_flags_directory);
    if (pop3_storage->pop3_flags_directory == NULL)
      goto free_cache_directory;
  }
  else {
    pop3_storage->pop3_cached = FALSE;
    pop3_storage->pop3_cache_directory = NULL;
    pop3_storage->pop3_flags_directory = NULL;
  }

  storage->sto_data = pop3_storage;
  storage->sto_driver = &pop3_mailstorage_driver;

  return MAIL_NO_ERROR;

 free_cache_directory:
  free(pop3_storage->pop3_cache_directory);
 free_realm:
  free(pop3_storage->pop3_sasl.sasl_realm);
 free_password:
  free(pop3_storage->pop3_sasl.sasl_password);
 free_auth_name:
  free(pop3_storage->pop3_sasl.sasl_auth_name);
 free_login:
  free(pop3_storage->pop3_sasl.sasl_login);
 free_remote_ip_port:
  free(pop3_storage->pop3_sasl.sasl_remote_ip_port);
 free_local_ip_port:
  free(pop3_storage->pop3_sasl.sasl_local_ip_port);
 free_server_fqdn:
  free(pop3_storage->pop3_sasl.sasl_server_fqdn);
 free_auth_type:
  free(pop3_storage->pop3_sasl.sasl_auth_type);
 free_command:
  if (pop3_storage->pop3_command != NULL)
    free(pop3_storage->pop3_command);
 free_servername:
  if (pop3_storage->pop3_servername != NULL)
    free(pop3_storage->pop3_servername);
 free:
  free(pop3_storage);
 err:
  return MAIL_ERROR_MEMORY;
}

static void pop3_mailstorage_uninitialize(struct mailstorage * storage)
{
  struct pop3_mailstorage * pop3_storage;

  pop3_storage = storage->sto_data;

  if (pop3_storage->pop3_flags_directory != NULL)
    free(pop3_storage->pop3_flags_directory);
  if (pop3_storage->pop3_cache_directory != NULL)
    free(pop3_storage->pop3_cache_directory);

  free(pop3_storage->pop3_sasl.sasl_realm);
  free(pop3_storage->pop3_sasl.sasl_password);
  free(pop3_storage->pop3_sasl.sasl_auth_name);
  free(pop3_storage->pop3_sasl.sasl_login);
  free(pop3_storage->pop3_sasl.sasl_remote_ip_port);
  free(pop3_storage->pop3_sasl.sasl_local_ip_port);
  free(pop3_storage->pop3_sasl.sasl_server_fqdn);
  free(pop3_storage->pop3_sasl.sasl_auth_type);
  
  if (pop3_storage->pop3_command != NULL)
    free(pop3_storage->pop3_command);
  free(pop3_storage->pop3_servername);
  free(pop3_storage);
  
  storage->sto_data = pop3_storage;
}

static int pop3_mailstorage_connect(struct mailstorage * storage)
{
  struct pop3_mailstorage * pop3_storage;
  mailsession_driver * driver;
  int r;
  int res;
  mailsession * session;
  int auth_type;

  pop3_storage = storage->sto_data;

  if (pop3_storage->pop3_cached)
    driver = pop3_cached_session_driver;
  else
    driver = pop3_session_driver;

  r = mailstorage_generic_connect(driver,
      pop3_storage->pop3_servername,
      pop3_storage->pop3_port, pop3_storage->pop3_command,
      pop3_storage->pop3_connection_type,
      POP3DRIVER_CACHED_SET_CACHE_DIRECTORY,
      pop3_storage->pop3_cache_directory,
      POP3DRIVER_CACHED_SET_FLAGS_DIRECTORY,
      pop3_storage->pop3_flags_directory,
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

  auth_type = -1;
  switch (pop3_storage->pop3_auth_type) {
  case POP3_AUTH_TYPE_PLAIN:
    auth_type = POP3DRIVER_AUTH_TYPE_PLAIN;
    break;
  case POP3_AUTH_TYPE_APOP:
    auth_type = POP3DRIVER_AUTH_TYPE_APOP;
    break;
  case POP3_AUTH_TYPE_TRY_APOP:
    auth_type = POP3DRIVER_AUTH_TYPE_TRY_APOP;
    break;
  }

  if (auth_type != -1) {
    mailsession_parameters(session, POP3DRIVER_SET_AUTH_TYPE, &auth_type);
  }

  if (pop3_storage->pop3_sasl.sasl_enabled) {
    r = mailstorage_generic_auth_sasl(session, r,
        pop3_storage->pop3_sasl.sasl_auth_type,
        pop3_storage->pop3_sasl.sasl_server_fqdn,
        pop3_storage->pop3_sasl.sasl_local_ip_port,
        pop3_storage->pop3_sasl.sasl_remote_ip_port,
        pop3_storage->pop3_sasl.sasl_login,
        pop3_storage->pop3_sasl.sasl_auth_name,
        pop3_storage->pop3_sasl.sasl_password,
        pop3_storage->pop3_sasl.sasl_realm);
  }
  else {
    r = mailstorage_generic_auth(session, r,
        pop3_storage->pop3_auth_type,
        pop3_storage->pop3_sasl.sasl_login,
        pop3_storage->pop3_sasl.sasl_password);
  }
  if (r != MAIL_NO_ERROR) {
    if (pop3_storage->pop3_auth_type == POP3_AUTH_TYPE_TRY_APOP) {
      /* try in clear authentication */
      mailsession_free(session);
      
      pop3_storage->pop3_auth_type = POP3_AUTH_TYPE_PLAIN;
      r = mailstorage_connect(storage);
      if (r != MAIL_NO_ERROR) {
        res = r;
        return res;
      }
      pop3_storage->pop3_auth_type = POP3_AUTH_TYPE_TRY_APOP;
      
      return MAIL_NO_ERROR;
    }
    
    res = r;
    goto free;
  }
  
  storage->sto_session = session;
  
  return MAIL_NO_ERROR;

 free:
  mailsession_free(session);
 err:
  return res;
}

static int pop3_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result)
{
  * result = storage->sto_session;
 
  return MAIL_NO_ERROR;
}

