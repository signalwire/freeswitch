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
 * $Id: dbstorage.c,v 1.6 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "dbstorage.h"
#include "mailstorage.h"

#include "mail.h"
#include "mailmessage.h"
#include "dbdriver.h"
#include "maildriver.h"

#include <stdlib.h>
#include <string.h>

/* db storage */

static int db_mailstorage_connect(struct mailstorage * storage);
static int
db_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result);
static void db_mailstorage_uninitialize(struct mailstorage * storage);

static mailstorage_driver db_mailstorage_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* sto_name               */ "db",
  /* sto_connect            */ db_mailstorage_connect,
  /* sto_get_folder_session */ db_mailstorage_get_folder_session,
  /* sto_uninitialize       */ db_mailstorage_uninitialize,
#else
  .sto_name               = "db",
  .sto_connect            = db_mailstorage_connect,
  .sto_get_folder_session = db_mailstorage_get_folder_session,
  .sto_uninitialize       = db_mailstorage_uninitialize,
#endif
};

LIBETPAN_EXPORT
int db_mailstorage_init(struct mailstorage * storage,
    char * db_pathname)
{
  struct db_mailstorage * db_storage;
  
  db_storage = malloc(sizeof(* db_storage));
  if (db_storage == NULL)
    goto err;
  
  db_storage->db_pathname = strdup(db_pathname);
  if (db_storage->db_pathname == NULL)
    goto free;
  
  storage->sto_data = db_storage;
  storage->sto_driver = &db_mailstorage_driver;
  
  return MAIL_NO_ERROR;
  
 free:
  free(db_storage);
 err:
  return MAIL_ERROR_MEMORY;
}

static void db_mailstorage_uninitialize(struct mailstorage * storage)
{
  struct db_mailstorage * db_storage;

  db_storage = storage->sto_data;
  free(db_storage->db_pathname);
  free(db_storage);
  
  storage->sto_data = NULL;
}

static int db_mailstorage_connect(struct mailstorage * storage)
{
  struct db_mailstorage * db_storage;
  mailsession_driver * driver;
  int r;
  int res;
  mailsession * session;

  db_storage = storage->sto_data;

  driver = db_session_driver;
  
  session = mailsession_new(driver);
  if (session == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mailsession_connect_path(session, db_storage->db_pathname);
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
db_mailstorage_get_folder_session(struct mailstorage * storage,
    char * pathname, mailsession ** result)
{
  * result = storage->sto_session;

  return MAIL_NO_ERROR;
}

