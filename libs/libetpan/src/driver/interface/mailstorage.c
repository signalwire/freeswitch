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
 * $Id: mailstorage.c,v 1.24 2006/06/02 15:44:30 smarinier Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailstorage.h"

#include "maildriver.h"

#include <stdlib.h>
#include <string.h>

static int mailstorage_get_folder(struct mailstorage * storage,
    char * pathname, mailsession ** result);

LIBETPAN_EXPORT
struct mailfolder * mailfolder_new(struct mailstorage * storage,
    const char * pathname, const char * virtual_name)
{
  struct mailfolder * folder;
  
  folder = malloc(sizeof(struct mailfolder));
  if (folder == NULL)
    goto err;

  if (pathname != NULL) {
    folder->fld_pathname = strdup(pathname);
    if (folder->fld_pathname == NULL)
      goto free;
  }
  else
    folder->fld_pathname = NULL;
  
  if (virtual_name != NULL) {
    folder->fld_virtual_name = strdup(virtual_name);
    if (folder->fld_virtual_name == NULL)
      goto free_pathname;
  }
  else
    folder->fld_virtual_name = NULL;

  folder->fld_storage = storage;

  folder->fld_session = NULL;
  folder->fld_shared_session = 0;
  folder->fld_pos = NULL;

  folder->fld_parent = NULL;
  folder->fld_sibling_index = 0;
  folder->fld_children = carray_new(128);
  if (folder->fld_children == NULL)
    goto free_virtualname;

  return folder;

free_virtualname:
  if (folder->fld_virtual_name != NULL)
    free(folder->fld_virtual_name);
free_pathname:
  if (folder->fld_pathname != NULL)
    free(folder->fld_pathname);
free:
  free(folder);
err:
  return NULL;
}

LIBETPAN_EXPORT
void mailfolder_free(struct mailfolder * folder)
{
  if (folder->fld_parent != NULL)
    mailfolder_detach_parent(folder);

  while (carray_count(folder->fld_children) > 0) {
    struct mailfolder * child;

    child = carray_get(folder->fld_children, 0);
    mailfolder_detach_parent(child);
  }

  carray_free(folder->fld_children);

  if (folder->fld_session != NULL)
    mailfolder_disconnect(folder);

  if (folder->fld_virtual_name != NULL)
    free(folder->fld_virtual_name);
  if (folder->fld_pathname != NULL)
    free(folder->fld_pathname);
  free(folder);
}

LIBETPAN_EXPORT
int mailfolder_connect(struct mailfolder * folder)
{
  mailsession * session;
  int res;
  int r;
  
  if (folder->fld_storage == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  if (folder->fld_storage->sto_session == NULL) {
    r = mailstorage_connect(folder->fld_storage);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  if (folder->fld_session != NULL) {
    if ((folder->fld_pathname != NULL) && (folder->fld_shared_session)) {
      if (folder->fld_session->sess_driver->sess_select_folder != NULL) {
        r = mailsession_select_folder(folder->fld_session,
            folder->fld_pathname);
        if (r != MAIL_NO_ERROR) {
          res = r;
          goto err;
        }
      }
    }

    return MAIL_NO_ERROR;
  }
  
  r = mailstorage_get_folder(folder->fld_storage, folder->fld_pathname,
      &session);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  folder->fld_session = session;
  folder->fld_shared_session = (session == folder->fld_storage->sto_session);
  if (folder->fld_shared_session) {
    r = clist_append(folder->fld_storage->sto_shared_folders, folder);
    if (r < 0) {
      folder->fld_session = NULL;
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    folder->fld_pos = clist_end(folder->fld_storage->sto_shared_folders);
  }

  return MAIL_NO_ERROR;

err:
  return res;
}

LIBETPAN_EXPORT
void mailfolder_disconnect(struct mailfolder * folder)
{
  if (folder->fld_session == NULL)
    return;

  if (folder->fld_shared_session) {
    clist_delete(folder->fld_storage->sto_shared_folders, folder->fld_pos);
    folder->fld_pos = NULL;
  }
  else {
    mailsession_logout(folder->fld_session);
    mailsession_free(folder->fld_session);
  }

  folder->fld_session = NULL;
}

LIBETPAN_EXPORT
int mailfolder_add_child(struct mailfolder * parent,
    struct mailfolder * child)
{
  unsigned int index;
  int r;
  
  r = carray_add(parent->fld_children, child, &index);
  if (r < 0)
    return MAIL_ERROR_MEMORY;
  
  child->fld_sibling_index = index;
  child->fld_parent = parent;

  return MAIL_NO_ERROR;
}

LIBETPAN_EXPORT
int mailfolder_detach_parent(struct mailfolder * folder)
{
  unsigned int i;
  int r;
  
  if (folder->fld_parent == NULL)
    return MAIL_ERROR_INVAL;

  r = carray_delete_slow(folder->fld_parent->fld_children,
      folder->fld_sibling_index);
  if (r < 0)
    return MAIL_ERROR_INVAL;
  
  for(i = 0 ; i < carray_count(folder->fld_parent->fld_children) ; i ++) {
    struct mailfolder * child;
    
    child = carray_get(folder->fld_parent->fld_children, i);
    child->fld_sibling_index = i;
  }

  folder->fld_parent = NULL;
  folder->fld_sibling_index = 0;

  return MAIL_NO_ERROR;
}

LIBETPAN_EXPORT
struct mailstorage * mailstorage_new(const char * sto_id)
{
  struct mailstorage * storage;

  storage = malloc(sizeof(struct mailstorage));
  if (storage == NULL)
    goto err;

  if (sto_id != NULL) {
    storage->sto_id = strdup(sto_id);
    if (storage->sto_id == NULL)
      goto free;
  }
  else
    storage->sto_id = NULL;

  storage->sto_data = NULL;
  storage->sto_session = NULL;
  storage->sto_driver = NULL;
  storage->sto_shared_folders = clist_new();
  if (storage->sto_shared_folders == NULL)
    goto free_id;

  return storage;

 free_id:
  if (storage->sto_id != NULL)
    free(storage->sto_id);
 free:
  free(storage);
 err:
  return NULL;
}

LIBETPAN_EXPORT
void mailstorage_free(struct mailstorage * storage)
{
  if (storage->sto_session != NULL)
    mailstorage_disconnect(storage);
  
  if (storage->sto_driver != NULL) {
    if (storage->sto_driver->sto_uninitialize != NULL)
      storage->sto_driver->sto_uninitialize(storage);
  }
  
  clist_free(storage->sto_shared_folders);

  if (storage->sto_id != NULL)
    free(storage->sto_id);
  
  free(storage);
}

LIBETPAN_EXPORT
int mailstorage_connect(struct mailstorage * storage)
{
  if (storage->sto_session != NULL)
    return MAIL_NO_ERROR;

  if (!clist_isempty(storage->sto_shared_folders))
    return MAIL_ERROR_BAD_STATE;

  if (storage->sto_driver->sto_connect == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return storage->sto_driver->sto_connect(storage);
}


LIBETPAN_EXPORT
void mailstorage_disconnect(struct mailstorage * storage)
{
  int r;
  clistiter * cur;

  while ((cur = clist_begin(storage->sto_shared_folders)) != NULL) {
    struct mailfolder * folder;

    folder = cur->data;
    mailfolder_disconnect(folder);
  }

  if (storage->sto_session == NULL)
    return;

  r = mailsession_logout(storage->sto_session);

  mailsession_free(storage->sto_session);
  storage->sto_session = NULL;
}


LIBETPAN_EXPORT
int mailstorage_noop(struct mailstorage * storage)
{
  return mailsession_noop(storage->sto_session);
}


static int mailstorage_get_folder(struct mailstorage * storage,
    char * pathname, mailsession ** result)
{
  if (storage->sto_driver->sto_get_folder_session == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return storage->sto_driver->sto_get_folder_session(storage,
      pathname, result);
}
