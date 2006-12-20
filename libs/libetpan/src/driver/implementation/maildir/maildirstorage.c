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
 * $Id: maildirstorage.c,v 1.10 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildirstorage.h"
#include "mailstorage.h"

#include "mail.h"
#include "mailmessage.h"
#include "maildirdriver.h"
#include "maildirdriver_cached.h"
#include "maildriver.h"

#include <stdlib.h>
#include <string.h>

/* maildir storage */

static int maildir_mailstorage_connect(struct mailstorage * storage);
static int
maildir_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result);
static void maildir_mailstorage_uninitialize(struct mailstorage * storage);

static mailstorage_driver maildir_mailstorage_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sto_name               */ "maildir",
  /* sto_connect            */ maildir_mailstorage_connect,
  /* sto_get_folder_session */ maildir_mailstorage_get_folder_session,
  /* sto_uninitialize       */ maildir_mailstorage_uninitialize,
#else
  .sto_name               = "maildir",
  .sto_connect            = maildir_mailstorage_connect,
  .sto_get_folder_session = maildir_mailstorage_get_folder_session,
  .sto_uninitialize       = maildir_mailstorage_uninitialize,
#endif
};

LIBETPAN_EXPORT
int maildir_mailstorage_init(struct mailstorage * storage,
    const char * md_pathname, int md_cached,
    const char * md_cache_directory, const char * md_flags_directory)
{
  struct maildir_mailstorage * maildir_storage;

  maildir_storage = malloc(sizeof(* maildir_storage));
  if (maildir_storage == NULL)
    goto err;

  maildir_storage->md_pathname = strdup(md_pathname);
  if (maildir_storage->md_pathname == NULL)
    goto free;

  maildir_storage->md_cached = md_cached;

  if (md_cached && (md_cache_directory != NULL) &&
      (md_flags_directory != NULL)) {
    maildir_storage->md_cache_directory = strdup(md_cache_directory);
    if (maildir_storage->md_cache_directory == NULL)
      goto free_pathname;

    maildir_storage->md_flags_directory = strdup(md_flags_directory);
    if (maildir_storage->md_flags_directory == NULL)
      goto free_cache_directory;
  }
  else {
    maildir_storage->md_cached = FALSE;
    maildir_storage->md_cache_directory = NULL;
    maildir_storage->md_flags_directory = NULL;
  }

  storage->sto_data = maildir_storage;
  storage->sto_driver = &maildir_mailstorage_driver;
  
  return MAIL_NO_ERROR;

 free_cache_directory:
  free(maildir_storage->md_cache_directory);
 free_pathname:
  free(maildir_storage->md_pathname);
 free:
  free(maildir_storage);
 err:
  return MAIL_ERROR_MEMORY;
}

static void maildir_mailstorage_uninitialize(struct mailstorage * storage)
{
  struct maildir_mailstorage * maildir_storage;

  maildir_storage = storage->sto_data;
  if (maildir_storage->md_flags_directory != NULL)
    free(maildir_storage->md_flags_directory);
  if (maildir_storage->md_cache_directory != NULL)
    free(maildir_storage->md_cache_directory);
  free(maildir_storage->md_pathname);
  free(maildir_storage);
  
  storage->sto_data = NULL;
}

static int maildir_mailstorage_connect(struct mailstorage * storage)
{
  struct maildir_mailstorage * maildir_storage;
  mailsession_driver * driver;
  int r;
  int res;
  mailsession * session;

  maildir_storage = storage->sto_data;

  if (maildir_storage->md_cached)
    driver = maildir_cached_session_driver;
  else
    driver = maildir_session_driver;

  session = mailsession_new(driver);
  if (session == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  if (maildir_storage->md_cached) {
    r = mailsession_parameters(session,
        MAILDIRDRIVER_CACHED_SET_CACHE_DIRECTORY,
        maildir_storage->md_cache_directory);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free;
    }

    r = mailsession_parameters(session,
        MAILDIRDRIVER_CACHED_SET_FLAGS_DIRECTORY,
        maildir_storage->md_flags_directory);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free;
    }
  }
  
  r = mailsession_connect_path(session, maildir_storage->md_pathname);
  switch (r) {
  case MAIL_NO_ERROR_NON_AUTHENTICATED:
  case MAIL_NO_ERROR_AUTHENTICATED:
  case MAIL_NO_ERROR:
    break;
  default:
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

static int
maildir_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result)
{
  * result = storage->sto_session;

  return MAIL_NO_ERROR;
}

