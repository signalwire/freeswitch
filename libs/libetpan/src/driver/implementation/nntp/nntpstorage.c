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
 * $Id: nntpstorage.c,v 1.16 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "nntpstorage.h"

#include <stdlib.h>
#include <string.h>

#include "maildriver.h"
#include "nntpdriver.h"
#include "nntpdriver_cached.h"
#include "mailstorage_tools.h"
#include "mail.h"

/* nntp storage */

#define NNTP_DEFAULT_PORT  119
#define NNTPS_DEFAULT_PORT 563

static int nntp_mailstorage_connect(struct mailstorage * storage);
static int nntp_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result);
static void nntp_mailstorage_uninitialize(struct mailstorage * storage);

static mailstorage_driver nntp_mailstorage_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sto_name               */ "nntp",
  /* sto_connect            */ nntp_mailstorage_connect,
  /* sto_get_folder_session */ nntp_mailstorage_get_folder_session,
  /* sto_uninitialize       */ nntp_mailstorage_uninitialize,
#else
  .sto_name               = "nntp",
  .sto_connect            = nntp_mailstorage_connect,
  .sto_get_folder_session = nntp_mailstorage_get_folder_session,
  .sto_uninitialize       = nntp_mailstorage_uninitialize,
#endif
};

LIBETPAN_EXPORT
int nntp_mailstorage_init(struct mailstorage * storage,
    const char * nn_servername, uint16_t nn_port,
    const char * nn_command,
    int nn_connection_type, int nn_auth_type,
    const char * nn_login, const char * nn_password,
    int nn_cached, const char * nn_cache_directory, const char * nn_flags_directory)
{
  struct nntp_mailstorage * nntp_storage;
  int res;
  
  nntp_storage = malloc(sizeof(* nntp_storage));
  if (nntp_storage == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  nntp_storage->nntp_servername = strdup(nn_servername);
  if (nntp_storage->nntp_servername == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free;
  }

  nntp_storage->nntp_connection_type = nn_connection_type;
  
  if (nn_port == 0) {
    switch (nn_connection_type) {
    case CONNECTION_TYPE_PLAIN:
    case CONNECTION_TYPE_COMMAND:
      nn_port = NNTP_DEFAULT_PORT;
      break;

    case CONNECTION_TYPE_TLS:
    case CONNECTION_TYPE_COMMAND_TLS:
      nn_port = NNTPS_DEFAULT_PORT;
      break;
    
    default:
      res = MAIL_ERROR_INVAL;
      goto free_servername;
    }
  }

  nntp_storage->nntp_port = nn_port;

  if (nn_command != NULL) {
    nntp_storage->nntp_command = strdup(nn_command);
    if (nntp_storage->nntp_command == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_servername;
    }
  }
  else
    nntp_storage->nntp_command = NULL;

  nntp_storage->nntp_auth_type = nn_auth_type;

  if (nn_login != NULL) {
    nntp_storage->nntp_login = strdup(nn_login);
    if (nntp_storage->nntp_login == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_command;
    }
  }
  else
    nntp_storage->nntp_login = NULL;

  if (nn_password != NULL) {
    nntp_storage->nntp_password = strdup(nn_password);
    if (nntp_storage->nntp_password == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_login;
    }
  }
  else
    nntp_storage->nntp_password = NULL;

  nntp_storage->nntp_cached = nn_cached;

  if (nn_cached && (nn_cache_directory != NULL) &&
      (nn_flags_directory != NULL)) {
    nntp_storage->nntp_cache_directory = strdup(nn_cache_directory);
    if (nntp_storage->nntp_cache_directory == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_password;
    }
    nntp_storage->nntp_flags_directory = strdup(nn_flags_directory);
    if (nntp_storage->nntp_flags_directory == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_cache_directory;
    }
  }
  else {
    nntp_storage->nntp_cached = FALSE;
    nntp_storage->nntp_cache_directory = NULL;
    nntp_storage->nntp_flags_directory = NULL;
  }

  storage->sto_data = nntp_storage;
  storage->sto_driver = &nntp_mailstorage_driver;

  return MAIL_NO_ERROR;

 free_cache_directory:
  free(nntp_storage->nntp_cache_directory);
 free_password:
  free(nntp_storage->nntp_password);
 free_login:
  free(nntp_storage->nntp_login);
 free_command:
  free(nntp_storage->nntp_command);
 free_servername:
  free(nntp_storage->nntp_servername);
 free:
  free(nntp_storage);
 err:
  return res;
}

static void nntp_mailstorage_uninitialize(struct mailstorage * storage)
{
  struct nntp_mailstorage * nntp_storage;

  nntp_storage = storage->sto_data;

  if (nntp_storage->nntp_flags_directory != NULL)
    free(nntp_storage->nntp_flags_directory);
  if (nntp_storage->nntp_cache_directory != NULL)
    free(nntp_storage->nntp_cache_directory);
  if (nntp_storage->nntp_password != NULL)
    free(nntp_storage->nntp_password);
  if (nntp_storage->nntp_login != NULL)
    free(nntp_storage->nntp_login);
  if (nntp_storage->nntp_command != NULL)
    free(nntp_storage->nntp_command);
  free(nntp_storage->nntp_servername);
  free(nntp_storage);
  
  storage->sto_data = NULL;
}

static int nntp_mailstorage_connect(struct mailstorage * storage)
{
  struct nntp_mailstorage * nntp_storage;
  mailsession_driver * driver;
  int r;
  int res;
  mailsession * session;

  nntp_storage = storage->sto_data;

  if (nntp_storage->nntp_cached)
    driver = nntp_cached_session_driver;
  else
    driver = nntp_session_driver;

  r = mailstorage_generic_connect(driver,
      nntp_storage->nntp_servername,
      nntp_storage->nntp_port, nntp_storage->nntp_command,
      nntp_storage->nntp_connection_type,
      NNTPDRIVER_CACHED_SET_CACHE_DIRECTORY,
      nntp_storage->nntp_cache_directory,
      NNTPDRIVER_CACHED_SET_FLAGS_DIRECTORY,
      nntp_storage->nntp_flags_directory,
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

  r = mailstorage_generic_auth(session, r,
      nntp_storage->nntp_connection_type,
      nntp_storage->nntp_login,
      nntp_storage->nntp_password);
  if (r != MAIL_NO_ERROR) {
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

static int nntp_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result)
{
  int r;
  
  r = mailsession_select_folder(storage->sto_session, pathname);
  
  * result = storage->sto_session;
  
  return MAIL_NO_ERROR;
}
