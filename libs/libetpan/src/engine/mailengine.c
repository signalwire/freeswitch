/*
 * libEtPan! -- a mail library
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
 * $Id: mailengine.c,v 1.11 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailengine.h"

#include "mailfolder.h"
#include "maildriver.h"
#include "imapdriver_cached.h"
#include "mailstorage.h"
#include "imapdriver_cached_message.h"
#include <stdlib.h>
#include "mailprivacy.h"
#ifdef LIBETPAN_REENTRANT
#include <pthread.h>
#endif
#include <string.h>


/* ************************************************************* */
/* Message ref info */

struct message_ref_elt {
  mailmessage * msg;
  int ref_count;
  int mime_ref_count;
  struct mailfolder * folder;
  int lost;
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_t lock;
#endif
};

static struct message_ref_elt *
message_ref_elt_new(struct mailfolder * folder, mailmessage * msg)
{
  struct message_ref_elt * ref;
  int r;
  
  ref = malloc(sizeof(* ref));
  if (ref == NULL)
    goto err;
  
#ifdef LIBETPAN_REENTRANT
  r = pthread_mutex_init(&ref->lock, NULL);
  if (r != 0)
    goto free;
#endif
  
  ref->msg = msg;
  ref->ref_count = 0;
  ref->mime_ref_count = 0;
  ref->folder = folder;
  ref->lost = 0;
  
  return ref;
  
 free:
  free(ref);
 err:
  return NULL;
}

static void message_ref_elt_free(struct message_ref_elt * ref_elt)
{
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_destroy(&ref_elt->lock);
#endif
  free(ref_elt);
}

static inline int message_ref(struct message_ref_elt * ref_elt)
{
  int count;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&ref_elt->lock);
#endif
  
  ref_elt->ref_count ++;
  count = ref_elt->ref_count;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&ref_elt->lock);
#endif
  
  return count;
}

static inline int message_unref(struct message_ref_elt * ref_elt)
{
  int count;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&ref_elt->lock);
#endif
  
  ref_elt->ref_count --;
  count = ref_elt->ref_count;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&ref_elt->lock);
#endif
  
  return count;
}


static inline int message_mime_ref(struct mailprivacy * privacy,
    struct message_ref_elt * ref_elt)
{
  int r;
  int count;
  
  if (ref_elt->mime_ref_count == 0) {
    struct mailmime * mime;

    r = mailprivacy_msg_get_bodystructure(privacy, ref_elt->msg, &mime);
    if (r != MAIL_NO_ERROR)
      return -r;
  }
  
  message_ref(ref_elt);
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&ref_elt->lock);
#endif
  ref_elt->mime_ref_count ++;
  count = ref_elt->mime_ref_count;
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&ref_elt->lock);
#endif
  
  return count;
}

static inline int message_mime_unref(struct mailprivacy * privacy,
    struct message_ref_elt * ref_elt)
{
  int count;
  
  message_unref(ref_elt);
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&ref_elt->lock);
#endif
  ref_elt->mime_ref_count --;
  
  if (ref_elt->mime_ref_count == 0)
    mailprivacy_msg_flush(privacy, ref_elt->msg);
  
  count = ref_elt->mime_ref_count;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&ref_elt->lock);
#endif
  
  return count;
}


/* ************************************************************* */
/* Folder ref info */

struct folder_ref_info {
  struct mailfolder * folder;
  
  /* msg => msg_ref_info */
  chash * msg_hash;
  
  /* uid => msg */
  chash * uid_hash;
  
  int lost_session;
};

static struct folder_ref_info *
folder_ref_info_new(struct mailfolder * folder
    /*, struct message_folder_finder * msg_folder_finder */)
{
  struct folder_ref_info * ref_info;
  
  ref_info = malloc(sizeof(* ref_info));
  if (ref_info == NULL)
    goto err;
  
  ref_info->folder = folder;
  
  ref_info->msg_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (ref_info->msg_hash == NULL)
    goto free;

  ref_info->uid_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYNONE);
  if (ref_info->uid_hash == NULL)
    goto free_msg_hash;
  
  ref_info->lost_session = 1;
  
  return ref_info;
  
 free_msg_hash:
  chash_free(ref_info->msg_hash);
 free:
  free(ref_info);
 err:
  return NULL;
}

static void folder_ref_info_free(struct folder_ref_info * ref_info)
{
  chash_free(ref_info->uid_hash);
  chash_free(ref_info->msg_hash);
  free(ref_info);
}

static struct message_ref_elt *
folder_info_get_msg_ref(struct folder_ref_info * ref_info, mailmessage * msg)
{
  chashdatum key;
  chashdatum data;
  struct message_ref_elt * ref_elt;
  int r;
  
  key.data = &msg;
  key.len = sizeof(msg);
  r = chash_get(ref_info->msg_hash, &key, &data);
  if (r < 0)
    return NULL;
  
  ref_elt = data.data;
  
  return ref_elt;
}

static mailmessage *
folder_info_get_msg_by_uid(struct folder_ref_info * ref_info,
    char * uid)
{
  chashdatum key;
  chashdatum data;
  mailmessage * msg;
  int r;
  
  key.data = uid;
  key.len = strlen(uid);
  r = chash_get(ref_info->uid_hash, &key, &data);
  if (r < 0)
    return NULL;
  
  msg = data.data;
  
  return msg;
}

static int folder_message_ref(struct folder_ref_info * ref_info,
    mailmessage * msg)
{
  struct message_ref_elt * msg_ref;
  
  msg_ref = folder_info_get_msg_ref(ref_info, msg);
  return message_ref(msg_ref);
}

static void folder_message_remove(struct folder_ref_info * ref_info,
    mailmessage * msg);

#ifdef DEBUG_ENGINE
#include "etpan-app.h"

void * engine_app = NULL;
#endif

static int folder_message_unref(struct folder_ref_info * ref_info,
    mailmessage * msg)
{
  struct message_ref_elt * msg_ref;
  int count;
  
  msg_ref = folder_info_get_msg_ref(ref_info, msg);
  
  if (msg_ref->ref_count == 0) {
#ifdef ETPAN_APP_DEBUG
    ETPAN_APP_DEBUG((engine_app, "** BUG detected negative ref count !"));
#endif
  }
  
  count = message_unref(msg_ref);
  if (count == 0) {
    folder_message_remove(ref_info, msg);
    mailmessage_free(msg);
  }
  
  return count;
}

static int folder_message_mime_ref(struct mailprivacy * privacy,
    struct folder_ref_info * ref_info,
    mailmessage * msg)
{
  struct message_ref_elt * msg_ref;
  
  msg_ref = folder_info_get_msg_ref(ref_info, msg);
  
  return message_mime_ref(privacy, msg_ref);
}

static int folder_message_mime_unref(struct mailprivacy * privacy,
    struct folder_ref_info * ref_info,
    mailmessage * msg)
{
  struct message_ref_elt * msg_ref;
  
  msg_ref = folder_info_get_msg_ref(ref_info, msg);
  return message_mime_unref(privacy, msg_ref);
}

static int folder_message_add(struct folder_ref_info * ref_info,
    mailmessage * msg)
{
  chashdatum key;
  chashdatum data;
  struct message_ref_elt * msg_ref;
  int r;
  
  msg_ref = message_ref_elt_new(ref_info->folder, msg);  
  if (msg_ref == NULL)
    goto err;
  
  key.data = &msg;
  key.len = sizeof(msg);
  data.data = msg_ref;
  data.len = 0;
  
  r = chash_set(ref_info->msg_hash, &key, &data, NULL);
  if (r < 0)
    goto free_msg_ref;
  
  if (msg->msg_uid != NULL) {
    key.data = msg->msg_uid;
    key.len = strlen(msg->msg_uid);
    data.data = msg;
    data.len = 0;
    
    r = chash_set(ref_info->uid_hash, &key, &data, NULL);
    if (r < 0)
      goto remove_msg_ref;
  }
  
  return MAIL_NO_ERROR;
  
 remove_msg_ref:
  key.data = &msg;
  key.len = sizeof(msg);
  chash_delete(ref_info->msg_hash, &key, NULL);
 free_msg_ref:
  message_ref_elt_free(msg_ref);
 err:
  return MAIL_ERROR_MEMORY;
}


static void folder_message_remove(struct folder_ref_info * ref_info,
    mailmessage * msg)
{
  chashdatum key;
  struct message_ref_elt * msg_ref;

  if (msg->msg_uid != NULL) {
    key.data = msg->msg_uid;
    key.len = strlen(msg->msg_uid);
    
    chash_delete(ref_info->uid_hash, &key, NULL);
  }
  
  msg_ref = folder_info_get_msg_ref(ref_info, msg);
  message_ref_elt_free(msg_ref);
  
  key.data = &msg;
  key.len = sizeof(msg);
  
  chash_delete(ref_info->msg_hash, &key, NULL);
}


static int folder_update_msg_list(struct folder_ref_info * ref_info,
    struct mailmessage_list ** p_new_msg_list,
    struct mailmessage_list ** p_lost_msg_list)
{
  int r;
  int res;
  struct mailmessage_list * new_env_list;
  unsigned int i;
  carray * lost_msg_tab;
  struct mailmessage_list * lost_msg_list;
  unsigned int free_start_index;
  chashiter * iter;
  unsigned int lost_count;

  r = mailfolder_get_messages_list(ref_info->folder, &new_env_list);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  for(iter = chash_begin(ref_info->msg_hash) ; iter != NULL ;
      iter = chash_next(ref_info->msg_hash, iter)) {
    struct message_ref_elt * msg_ref;
    chashdatum data;
    
    chash_value(iter, &data);
    msg_ref = data.data;
    msg_ref->lost = 1;
  }
  
  lost_count = chash_count(ref_info->msg_hash);
  
  for(i = 0 ; i < carray_count(new_env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    mailmessage * old_msg;
    
    msg = carray_get(new_env_list->msg_tab, i);
    
    if (msg->msg_uid == NULL)
      continue;
    
    old_msg = folder_info_get_msg_by_uid(ref_info, msg->msg_uid);
    if (old_msg != NULL) {
      struct message_ref_elt * msg_ref;
      
      /* replace old message */
      old_msg->msg_index = msg->msg_index;
      carray_set(new_env_list->msg_tab, i, old_msg);
      mailmessage_free(msg);
      
      msg_ref = folder_info_get_msg_ref(ref_info, old_msg);
      msg_ref->lost = 0;
      lost_count --;
    }
    else {
      /* set new message */
      r = folder_message_add(ref_info, msg);
      if (r != MAIL_NO_ERROR) {
        free_start_index = i;
        res = r;
        goto free_remaining;
      }
    }
  }
  
  /* build the table of lost messages */
  lost_msg_tab = carray_new(lost_count);
  if (lost_msg_tab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_env_list;
  }
  
  carray_set_size(lost_msg_tab, lost_count);
  
  i = 0;
  for(iter = chash_begin(ref_info->msg_hash) ; iter != NULL ;
      iter = chash_next(ref_info->msg_hash, iter)) {
    struct message_ref_elt * msg_ref;
    chashdatum key;
    chashdatum value;
    mailmessage * msg;
    
    chash_key(iter, &key);
    memcpy(&msg, key.data, sizeof(msg));
    
    chash_value(iter, &value);
    msg_ref = value.data;
    if (msg_ref->lost) {
      carray_set(lost_msg_tab, i, msg);
      i ++;
    }
  }
  
  lost_msg_list = mailmessage_list_new(lost_msg_tab);
  if (lost_msg_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_lost_msg_tab;
  }
  
  /* reference messages */
  for(i = 0 ; i < carray_count(new_env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(new_env_list->msg_tab, i);
    folder_message_ref(ref_info, msg);
  }
  
  * p_new_msg_list = new_env_list;
  * p_lost_msg_list = lost_msg_list;
  
  return MAIL_NO_ERROR;
  
 free_lost_msg_tab:
  carray_free(lost_msg_tab);
 free_env_list:
  for(i = 0 ; i < carray_count(new_env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    struct message_ref_elt * msg_ref;
    
    msg = carray_get(new_env_list->msg_tab, i);
    msg_ref = folder_info_get_msg_ref(ref_info, msg);
    if (msg_ref != NULL) {
      if (msg_ref->ref_count == 0)
        folder_message_remove(ref_info, msg);
    }
  }
  carray_set_size(new_env_list->msg_tab, 0);
  mailmessage_list_free(new_env_list);
  goto err;
 free_remaining:
  for(i = 0 ; i < carray_count(new_env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    struct message_ref_elt * msg_ref;
    
    msg = carray_get(new_env_list->msg_tab, i);
    msg_ref = folder_info_get_msg_ref(ref_info, msg);
    if (msg_ref != NULL) {
      if (msg_ref->ref_count == 0)
        folder_message_remove(ref_info, msg);
    }
  }
  for(i = free_start_index ; i < carray_count(new_env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(new_env_list->msg_tab, i);
    mailmessage_free(msg);
  }
  carray_set_size(new_env_list->msg_tab, 0);
  mailmessage_list_free(new_env_list);
 err:
  return res;
}

/*
  folder_fetch_env_list()
*/

static int folder_fetch_env_list(struct folder_ref_info * ref_info,
    struct mailmessage_list * msg_list)
{
  return mailfolder_get_envelopes_list(ref_info->folder, msg_list);
}

static void folder_free_msg_list(struct folder_ref_info * ref_info,
    struct mailmessage_list * env_list)
{
  unsigned int i;
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    int count;
    
    msg = carray_get(env_list->msg_tab, i);
    
    count = folder_message_unref(ref_info, msg);
  }
  carray_set_size(env_list->msg_tab, 0);
  mailmessage_list_free(env_list);
}


/* ************************************************************* */
/* Storage ref info */

struct storage_ref_info {
  struct mailstorage * storage;
  
  /* folder => folder_ref_info */
  chash * folder_ref_info;
};

static struct storage_ref_info *
storage_ref_info_new(struct mailstorage * storage
    /*, struct message_folder_finder * msg_folder_finder */)
{
  struct storage_ref_info * ref_info;
  
  ref_info = malloc(sizeof(* ref_info));
  if (ref_info == NULL)
    goto err;
  
  ref_info->storage = storage;
  
  ref_info->folder_ref_info = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (ref_info->folder_ref_info == NULL)
    goto free;

  return ref_info;
  
 free:
  free(ref_info);
 err:
  return NULL;
}

static void storage_ref_info_free(struct storage_ref_info * ref_info)
{
  chash_free(ref_info->folder_ref_info);
  free(ref_info);
}


static struct folder_ref_info *
storage_get_folder_ref(struct storage_ref_info * ref_info,
    struct mailfolder * folder)
{
  struct folder_ref_info * folder_ref;
  chashdatum key;
  chashdatum value;
  int r;

  key.data = &folder;
  key.len = sizeof(folder);
  r = chash_get(ref_info->folder_ref_info, &key, &value);
  if (r < 0)
    return NULL;

  folder_ref = value.data;

  return folder_ref;
}

static struct folder_ref_info *
storage_folder_add_ref(struct storage_ref_info * ref_info,
    struct mailfolder * folder)
{
  struct folder_ref_info * folder_ref;
  chashdatum key;
  chashdatum value;
  int r;
  
  folder_ref = folder_ref_info_new(folder /*, ref_info->msg_folder_finder */);
  if (folder_ref == NULL)
    goto err;
  
  key.data = &folder;
  key.len = sizeof(folder);
  value.data = folder_ref;
  value.len = 0;
  r = chash_set(ref_info->folder_ref_info, &key, &value, NULL);
  if (r < 0)
    goto free;

  return folder_ref;
  
 free:
  folder_ref_info_free(folder_ref);
 err:
  return NULL;
}


static void storage_folder_remove_ref(struct storage_ref_info * ref_info,
    struct mailfolder * folder)
{
  struct folder_ref_info * folder_ref;
  chashdatum key;
  chashdatum value;
  int r;
  
  key.data = &folder;
  key.len = sizeof(folder);
  r = chash_get(ref_info->folder_ref_info, &key, &value);
  if (r < 0)
      return;
  
  folder_ref = value.data;
  
  if (folder_ref == NULL)
    return;
  
  folder_ref_info_free(folder_ref);
  
  chash_delete(ref_info->folder_ref_info, &key, &value);
}

static int storage_folder_get_msg_list(struct storage_ref_info * ref_info,
    struct mailfolder * folder,
    struct mailmessage_list ** p_new_msg_list,
    struct mailmessage_list ** p_lost_msg_list)
{
  struct folder_ref_info * folder_ref_info;
  
  folder_ref_info = storage_get_folder_ref(ref_info, folder);
  if (folder_ref_info == NULL)
    return MAIL_ERROR_INVAL;
  
  return folder_update_msg_list(folder_ref_info,
      p_new_msg_list, p_lost_msg_list);
}

static int storage_folder_fetch_env_list(struct storage_ref_info * ref_info,
    struct mailfolder * folder,
    struct mailmessage_list * msg_list)
{
  struct folder_ref_info * folder_ref_info;
  
  folder_ref_info = storage_get_folder_ref(ref_info, folder);
  if (folder_ref_info == NULL)
    return MAIL_ERROR_INVAL;
  
  return folder_fetch_env_list(folder_ref_info, msg_list);
}

static void
storage_folder_free_msg_list(struct storage_ref_info * ref_info,
    struct mailfolder * folder,
    struct mailmessage_list * env_list)
{
  struct folder_ref_info * folder_ref_info;
  
  folder_ref_info = storage_get_folder_ref(ref_info, folder);
  
  folder_free_msg_list(folder_ref_info, env_list);
}


/* connection and disconnection */

static void
folder_restore_session(struct folder_ref_info * ref_info)
{
  chashiter * iter;
  mailsession * session;
  
  session = ref_info->folder->fld_session;
  
  for(iter = chash_begin(ref_info->msg_hash) ; iter != NULL ;
      iter = chash_next(ref_info->msg_hash, iter)) {
    chashdatum key;
    mailmessage * msg;
    
    chash_key(iter, &key);
    memcpy(&msg, key.data, sizeof(msg));
    msg->msg_session = session;
    
    if (msg->msg_driver == imap_cached_message_driver) {
      struct imap_cached_session_state_data * imap_cached_data;
      mailmessage * ancestor_msg;
      
      imap_cached_data = ref_info->folder->fld_session->sess_data;
      ancestor_msg = msg->msg_data;
      ancestor_msg->msg_session = imap_cached_data->imap_ancestor;
    }
  }
}

static void
storage_restore_message_session(struct storage_ref_info * ref_info)
{
  chashiter * iter;

  for(iter = chash_begin(ref_info->folder_ref_info) ; iter != NULL ;
      iter = chash_next(ref_info->folder_ref_info, iter)) {
    chashdatum data;
    struct folder_ref_info * folder_ref_info;
    
    chash_value(iter, &data);
    folder_ref_info = data.data;
    if (folder_ref_info->lost_session) {
      if (folder_ref_info->folder->fld_session != NULL) {
        /* restore folder session */
        folder_restore_session(folder_ref_info);
        
        folder_ref_info->lost_session = 0;
      }
    }
  }
}


static int do_storage_connect(struct storage_ref_info * ref_info)
{
  int r;
  
  r = mailstorage_connect(ref_info->storage);
  if (r != MAIL_NO_ERROR)
    return r;

  return MAIL_NO_ERROR;
}

static void do_storage_disconnect(struct storage_ref_info * ref_info)
{
  clistiter * cur;
  
  /* storage is disconnected, session is lost */
  for(cur = clist_begin(ref_info->storage->sto_shared_folders) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct folder_ref_info * folder_ref_info;
    struct mailfolder * folder;
    
    folder = clist_content(cur);
    /* folder is disconnected (in storage), session is lost */
    
    folder_ref_info = storage_get_folder_ref(ref_info, folder);
    folder_ref_info->lost_session = 1;
  }
  
  /* storage is disconnected */
  mailstorage_disconnect(ref_info->storage);
}



static int folder_connect(struct storage_ref_info * ref_info,
    struct mailfolder * folder)
{
  int r;
  struct folder_ref_info * folder_ref_info;
  
  r = do_storage_connect(ref_info);
  if (r != MAIL_NO_ERROR)
    return r;
  
  r = mailfolder_connect(folder);
  if (r != MAIL_NO_ERROR)
    return r;

  folder_ref_info = storage_get_folder_ref(ref_info, folder);
 
  return MAIL_NO_ERROR;
}


static void folder_disconnect(struct storage_ref_info * ref_info,
    struct mailfolder * folder)
{
  struct folder_ref_info * folder_ref_info;
  
  folder_ref_info = storage_get_folder_ref(ref_info, folder);
  
  /* folder is disconnected, session is lost */
  folder_ref_info->lost_session = 1;
  mailfolder_disconnect(folder);
  
  if (folder->fld_shared_session)
    do_storage_disconnect(ref_info);
}


static int storage_folder_connect(struct storage_ref_info * ref_info,
    struct mailfolder * folder)
{
  int r;
  int res;
  struct folder_ref_info * folder_ref_info;

  folder_ref_info = storage_get_folder_ref(ref_info, folder);
  if (folder_ref_info == NULL) {
    folder_ref_info = storage_folder_add_ref(ref_info, folder);
    if (folder_ref_info == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
  }
  
  /* connect folder */
  
  r = folder_connect(ref_info, folder);
  if (r == MAIL_ERROR_STREAM) {
    /* properly handles disconnection */

    /* reconnect */
    folder_disconnect(ref_info, folder);
    r = folder_connect(ref_info, folder);
  }
  
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto remove_ref;
  }
  
  /* test folder connection */
  r = mailfolder_noop(folder);
  if (r == MAIL_ERROR_STREAM) {
    /* reconnect */
    folder_disconnect(ref_info, folder);
    r = folder_connect(ref_info, folder);
  }
  
  if ((r != MAIL_ERROR_NOT_IMPLEMENTED) && (r != MAIL_NO_ERROR)) {
    res = r;
    goto disconnect;
  }
  
  storage_restore_message_session(ref_info);
  
  return MAIL_NO_ERROR;
  
 disconnect:
  folder_disconnect(ref_info, folder);
 remove_ref:
  storage_folder_remove_ref(ref_info, folder);
 err:
  return res;
}

static void storage_folder_disconnect(struct storage_ref_info * ref_info,
    struct mailfolder * folder)
{
  mailfolder_disconnect(folder);
  storage_folder_remove_ref(ref_info, folder);
}

static int storage_connect(struct storage_ref_info * ref_info)
{
  int r;
  int res;

  /* connect storage */

  /* properly handles disconnection */
  r = do_storage_connect(ref_info);
  if (r == MAIL_ERROR_STREAM) {
    /* reconnect storage */
    do_storage_disconnect(ref_info);
    r = do_storage_connect(ref_info);
  }
  
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto disconnect;
  }
  
  /* test storage connection */
  
  r = mailsession_noop(ref_info->storage->sto_session);
  if ((r != MAIL_ERROR_NOT_IMPLEMENTED) && (r != MAIL_NO_ERROR)) {
    /* properly handles disconnection */
    
    /* reconnect storage */
    do_storage_disconnect(ref_info);
    r = do_storage_connect(ref_info);
  }
  
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto disconnect;
  }
  
  storage_restore_message_session(ref_info);
  
  return MAIL_NO_ERROR;
  
 disconnect:
  do_storage_disconnect(ref_info);
  return res;
}


static void storage_disconnect(struct storage_ref_info * ref_info)
{
  chashiter * iter;

  /* disconnect folders */
  while ((iter = chash_begin(ref_info->folder_ref_info)) != NULL) {
    chashdatum key;
    struct mailfolder * folder;
    
    chash_key(iter, &key);
    memcpy(&folder, key.data, sizeof(folder));
    
    storage_folder_disconnect(ref_info, folder);
  }
  
  /* disconnect storage */
  do_storage_disconnect(ref_info);
}


/* ************************************************************* */
/* interface for mailengine */

struct mailengine {
  struct mailprivacy * privacy;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_t storage_hash_lock;
#endif  
  /* storage => storage_ref_info */
  chash * storage_hash;
};

static struct storage_ref_info *
get_storage_ref_info(struct mailengine * engine,
    struct mailstorage * storage)
{
  chashdatum key;
  chashdatum data;
  int r;
  struct storage_ref_info * ref_info;
  
  key.data = &storage;
  key.len = sizeof(storage);
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&engine->storage_hash_lock);
#endif  
  r = chash_get(engine->storage_hash, &key, &data);
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&engine->storage_hash_lock);
#endif  
  if (r < 0)
    return NULL;
  
  ref_info = data.data;

  return ref_info;
}

static struct storage_ref_info *
add_storage_ref_info(struct mailengine * engine,
    struct mailstorage * storage)
{
  chashdatum key;
  chashdatum data;
  int r;
  struct storage_ref_info * ref_info;
  
  ref_info = storage_ref_info_new(storage
      /* , &engine->msg_folder_finder */);
  if (ref_info == NULL)
    goto err;
  
  key.data = &storage;
  key.len = sizeof(storage);
  data.data = ref_info;
  data.len = 0;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&engine->storage_hash_lock);
#endif  
  r = chash_set(engine->storage_hash, &key, &data, NULL);
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&engine->storage_hash_lock);
#endif  
  if (r < 0)
    goto free;
  
  ref_info = data.data;
  
  return ref_info;
  
 free:
  storage_ref_info_free(ref_info);
 err:
  return NULL;
}

static void
remove_storage_ref_info(struct mailengine * engine,
    struct mailstorage * storage)
{
  chashdatum key;
  chashdatum data;
  struct storage_ref_info * ref_info;
  
  key.data = &storage;
  key.len = sizeof(storage);
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&engine->storage_hash_lock);
#endif
  
  chash_get(engine->storage_hash, &key, &data);
  ref_info = data.data;
  
  if (ref_info != NULL) {
    storage_ref_info_free(ref_info);
    
    chash_delete(engine->storage_hash, &key, NULL);
  }
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&engine->storage_hash_lock);
#endif  
}

struct mailengine *
libetpan_engine_new(struct mailprivacy * privacy)
{
  struct mailengine * engine;
  int r;
  
  engine = malloc(sizeof(* engine));
  if (engine == NULL)
    goto err;
  
  engine->privacy = privacy;
  
#ifdef LIBETPAN_REENTRANT
  r = pthread_mutex_init(&engine->storage_hash_lock, NULL);
  if (r != 0)
    goto free;
#endif
  
  engine->storage_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (engine->storage_hash == NULL)
    goto destroy_mutex;

  return engine;
  
 destroy_mutex:
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_destroy(&engine->storage_hash_lock);
#endif
 free:
  free(engine);
 err:
  return NULL;
}

void libetpan_engine_free(struct mailengine * engine)
{
  chash_free(engine->storage_hash);
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_destroy(&engine->storage_hash_lock);
#endif  
  free(engine);
}

static struct folder_ref_info *
message_get_folder_ref(struct mailengine * engine,
    mailmessage * msg)
{
  struct mailfolder * folder;
  struct mailstorage * storage;
  struct storage_ref_info * storage_ref_info;
  struct folder_ref_info * folder_ref_info;
  
  folder = msg->msg_folder;
  if (folder == NULL)
    storage = NULL;
  else
    storage = folder->fld_storage;
  
  storage_ref_info = get_storage_ref_info(engine, storage);
  
  folder_ref_info = storage_get_folder_ref(storage_ref_info, folder);
  
  return folder_ref_info;
}

int libetpan_message_ref(struct mailengine * engine,
    mailmessage * msg)
{
  struct folder_ref_info * ref_info;
  
  ref_info = message_get_folder_ref(engine, msg);

  return folder_message_ref(ref_info, msg);
}

int libetpan_message_unref(struct mailengine * engine,
    mailmessage * msg)
{
  struct folder_ref_info * ref_info;
  
  ref_info = message_get_folder_ref(engine, msg);
  
  return folder_message_unref(ref_info, msg);
}


int libetpan_message_mime_ref(struct mailengine * engine,
    mailmessage * msg)
{
  struct folder_ref_info * ref_info;
  
  ref_info = message_get_folder_ref(engine, msg);
  
  return folder_message_mime_ref(engine->privacy, ref_info, msg);
}

int libetpan_message_mime_unref(struct mailengine * engine,
    mailmessage * msg)
{
  struct folder_ref_info * ref_info;
  
  ref_info = message_get_folder_ref(engine, msg);
  
  return folder_message_mime_unref(engine->privacy, ref_info, msg);
}

int libetpan_folder_get_msg_list(struct mailengine * engine,
    struct mailfolder * folder,
    struct mailmessage_list ** p_new_msg_list,
    struct mailmessage_list ** p_lost_msg_list)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, folder->fld_storage);
  
  return storage_folder_get_msg_list(ref_info, folder,
      p_new_msg_list, p_lost_msg_list);
}

int libetpan_folder_fetch_env_list(struct mailengine * engine,
    struct mailfolder * folder,
    struct mailmessage_list * msg_list)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, folder->fld_storage);
  
  return storage_folder_fetch_env_list(ref_info, folder, msg_list);
}

void libetpan_folder_free_msg_list(struct mailengine * engine,
    struct mailfolder * folder,
    struct mailmessage_list * env_list)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, folder->fld_storage);
  
  storage_folder_free_msg_list(ref_info, folder, env_list);
}


int libetpan_storage_add(struct mailengine * engine,
    struct mailstorage * storage)
{
  struct storage_ref_info * storage_ref_info;
  struct folder_ref_info * folder_ref_info;
  
  storage_ref_info = add_storage_ref_info(engine, storage);
  if (storage_ref_info == NULL)
    goto err;
  
  if (storage == NULL) {
    folder_ref_info = storage_folder_add_ref(storage_ref_info, NULL);
    if (folder_ref_info == NULL)
      goto remove_storage_ref_info;
  }
  
  return MAIL_NO_ERROR;
  
 remove_storage_ref_info:
  remove_storage_ref_info(engine, storage);
 err:
  return MAIL_ERROR_MEMORY;
}

void libetpan_storage_remove(struct mailengine * engine,
    struct mailstorage * storage)
{
  struct storage_ref_info * storage_ref_info;
  
  storage_ref_info = get_storage_ref_info(engine, storage);
  if (storage == NULL) {
    storage_folder_remove_ref(storage_ref_info, NULL);
  }
  
  remove_storage_ref_info(engine, storage);
}

int libetpan_storage_connect(struct mailengine * engine,
    struct mailstorage * storage)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, storage);
  
  return storage_connect(ref_info);
}


void libetpan_storage_disconnect(struct mailengine * engine,
    struct mailstorage * storage)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, storage);
  
  storage_disconnect(ref_info);
}

int libetpan_storage_used(struct mailengine * engine,
    struct mailstorage * storage)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, storage);
  
  return (chash_count(ref_info->folder_ref_info) != 0);
}


int libetpan_folder_connect(struct mailengine * engine,
    struct mailfolder * folder)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, folder->fld_storage);
  
  return storage_folder_connect(ref_info, folder);
}


void libetpan_folder_disconnect(struct mailengine * engine,
    struct mailfolder * folder)
{
  struct storage_ref_info * ref_info;
  
  ref_info = get_storage_ref_info(engine, folder->fld_storage);
  
  storage_folder_disconnect(ref_info, folder);
}


struct mailfolder *
libetpan_message_get_folder(struct mailengine * engine,
    mailmessage * msg)
{
  return msg->msg_folder;
}


struct mailstorage *
libetpan_message_get_storage(struct mailengine * engine,
    mailmessage * msg)
{
  struct mailfolder * folder;
  
  folder = libetpan_message_get_folder(engine, msg);
  
  if (folder == NULL)
    return NULL;
  else
    return folder->fld_storage;
}


int libetpan_message_register(struct mailengine * engine,
    struct mailfolder * folder,
    mailmessage * msg)
{
  struct storage_ref_info * storage_ref_info;
  int r;
  int res;
  struct folder_ref_info * folder_ref_info;
  struct mailstorage * storage;
  
  if (folder != NULL)
    storage = folder->fld_storage;
  else
    storage = NULL;
  
  storage_ref_info = get_storage_ref_info(engine, storage);

  folder_ref_info = storage_get_folder_ref(storage_ref_info, folder);
  
  r = folder_message_add(folder_ref_info, msg);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  return MAIL_NO_ERROR;
  
 err:
  return res;
}

struct mailprivacy *
libetpan_engine_get_privacy(struct mailengine * engine)
{
  return engine->privacy;
}


static void folder_debug(struct folder_ref_info * folder_ref_info, FILE * f)
{
  fprintf(f, "folder debug -- begin\n");
  if (folder_ref_info->folder == NULL) {
    fprintf(f, "NULL folder\n");
  }
  else {
    if (folder_ref_info->folder->fld_virtual_name != NULL)
      fprintf(f, "folder %s\n", folder_ref_info->folder->fld_virtual_name);
    else
      fprintf(f, "folder [no name]\n");
  }
  
  fprintf(f, "message count: %i\n", chash_count(folder_ref_info->msg_hash));
  fprintf(f, "UID count: %i\n", chash_count(folder_ref_info->uid_hash));
  fprintf(f, "folder debug -- end\n");
}

static void storage_debug(struct storage_ref_info * storage_ref_info, FILE * f)
{
  chashiter * iter;
  
  fprintf(f, "storage debug -- begin\n");
  if (storage_ref_info->storage == NULL) {
    fprintf(f, "NULL storage\n");
  }
  else {
    if (storage_ref_info->storage->sto_id != NULL)
      fprintf(f, "storage %s\n", storage_ref_info->storage->sto_id);
    else
      fprintf(f, "storage [no name]\n");
  }
  fprintf(f, "folder count: %i\n",
      chash_count(storage_ref_info->folder_ref_info));

  for(iter = chash_begin(storage_ref_info->folder_ref_info) ; iter != NULL ;
      iter = chash_next(storage_ref_info->folder_ref_info, iter)) {
    chashdatum data;
    struct folder_ref_info * folder_ref_info;
    
    chash_value(iter, &data);
    folder_ref_info = data.data;
    
    folder_debug(folder_ref_info, f);
  }
  fprintf(f, "storage debug -- end\n");
}

void libetpan_engine_debug(struct mailengine * engine, FILE * f)
{
  chashiter * iter;
  
  fprintf(f, "mail engine debug -- begin\n");
  
  for(iter = chash_begin(engine->storage_hash) ; iter != NULL ;
      iter = chash_next(engine->storage_hash, iter)) {
    chashdatum data;
    struct storage_ref_info * storage_ref_info;
    
    chash_value(iter, &data);
    storage_ref_info = data.data;
    
    storage_debug(storage_ref_info, f);
  }

  fprintf(f, "mail engine debug -- end\n");
}

