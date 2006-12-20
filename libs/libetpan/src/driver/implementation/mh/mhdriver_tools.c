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
 * $Id: mhdriver_tools.c,v 1.25 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mhdriver_tools.h"

#include "mailmessage.h"
#include "mhdriver.h"
#include "mhdriver_cached.h"
#include "maildriver_types.h"
#include "mailmh.h"
#include "generic_cache.h"
#include "imfcache.h"
#include "mail_cache_db.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#	include <sys/mman.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif

int mhdriver_mh_error_to_mail_error(int error)
{
  switch (error) {
  case MAILMH_NO_ERROR:
    return MAIL_NO_ERROR;
    
  case MAILMH_ERROR_FOLDER:
    return MAIL_ERROR_FOLDER;

  case MAILMH_ERROR_MEMORY:
    return MAIL_ERROR_MEMORY;

  case MAILMH_ERROR_FILE:
    return MAIL_ERROR_FILE;

  case MAILMH_ERROR_COULD_NOT_ALLOC_MSG:
    return MAIL_ERROR_APPEND;
    
  case MAILMH_ERROR_RENAME:
    return MAIL_ERROR_RENAME;

  case MAILMH_ERROR_MSG_NOT_FOUND:
    return MAIL_ERROR_MSG_NOT_FOUND;

  default:
    return MAIL_ERROR_INVAL;
  }
}


static inline struct mh_session_state_data * get_data(mailsession * session)
{
  return session->sess_data;
}

static inline struct mailmh_folder * get_mh_cur_folder(mailsession * session)
{
  return get_data(session)->mh_cur_folder;
}

static inline struct mh_cached_session_state_data *
cached_get_data(mailsession * session)
{
  return session->sess_data;
}


static inline mailsession * cached_get_ancestor(mailsession * session)
{
  return cached_get_data(session)->mh_ancestor;
}

static inline struct mh_session_state_data *
cached_get_ancestor_data(mailsession * session)
{
  return get_data(cached_get_ancestor(session));
}

static inline struct mailmh_folder *
cached_get_mh_cur_folder(mailsession * session)
{
  return get_mh_cur_folder(cached_get_ancestor(session));
}

int mhdriver_fetch_message(mailsession * session, uint32_t index,
			   char ** result, size_t * result_len)
{
  size_t size;
  size_t cur_token;
  struct mailmh_folder * folder;
  int fd;
  MMAPString * mmapstr;
  char * str;
  int res;
  int r;

  folder = get_mh_cur_folder(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  r = mailmh_folder_get_message_fd(folder, index, O_RDONLY, &fd);

  switch (r) {
  case MAILMH_NO_ERROR:
    break;

  default:
    res = mhdriver_mh_error_to_mail_error(r);
    goto close;
  }

  r = mhdriver_fetch_size(session, index, &size);

  switch (r) {
  case MAILMH_NO_ERROR:
    break;

  default:
    res = mhdriver_mh_error_to_mail_error(r);
    goto close;
  }

  str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (str == (char *)MAP_FAILED) {
    res = MAIL_ERROR_FETCH;
    goto close;
  }

  /* strip "From " header for broken implementations */
  /* XXX - called twice, make a function */
  cur_token = 0;
  if (size > 5) {
    if (strncmp("From ", str, 5) == 0) {
      cur_token += 5;
    
      while (1) {
        if (str[cur_token] == '\n') {
          cur_token ++;
          break;
        }
        if (cur_token >= size)
          break;
        cur_token ++;
      }
    }
  }
  
  mmapstr = mmap_string_new_len(str + cur_token, size - cur_token);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto unmap;
  }
    
  if (mmap_string_ref(mmapstr) != 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_str;
  }

  munmap(str, size);
  close(fd);

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  return MAIL_NO_ERROR;

 free_str:
  mmap_string_free(mmapstr);
 unmap:
  munmap(str, size);
 close:
  close(fd);
 err:
  return res;
}


int mhdriver_fetch_header(mailsession * session, uint32_t index,
			  char ** result, size_t * result_len)
{
  size_t size;
  size_t cur_token;
  size_t begin;
  struct mailmh_folder * folder;
  int fd;
  MMAPString * mmapstr;
  char * str;
  int res;
  int r;

  folder = get_mh_cur_folder(session);
  if (folder == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  r = mailmh_folder_get_message_fd(folder, index, O_RDONLY, &fd);

  switch (r) {
  case MAILMH_NO_ERROR:
    break;

  default:
    res = mhdriver_mh_error_to_mail_error(r);
    goto close;
  }

  r = mhdriver_fetch_size(session, index, &size);

  switch (r) {
  case MAILMH_NO_ERROR:
    break;

  default:
    res = mhdriver_mh_error_to_mail_error(r);
    goto close;
  }

  str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (str == (char *)MAP_FAILED) {
    res = MAIL_ERROR_FETCH;
    goto close;
  }

  /* strip "From " header for broken implementations */
  cur_token = 0;
  if (size > 5) {
    if (strncmp("From ", str, 5) == 0) {
      cur_token += 5;
      
      while (1) {
        if (str[cur_token] == '\n') {
          cur_token ++;
          break;
        }
        if (cur_token >= size)
          break;
        cur_token ++;
      }
    }
  }
    
  begin = cur_token;

  while (1) {
    r = mailimf_ignore_field_parse(str, size, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else
      break;
  }
  mailimf_crlf_parse(str, size, &cur_token);
    
  mmapstr = mmap_string_new_len(str + begin, cur_token - begin);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto unmap;
  }

  if (mmap_string_ref(mmapstr) != 0) {
    res = MAIL_ERROR_MEMORY;
    goto free_str;
  }

  munmap(str, size);
  close(fd);

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  return MAIL_NO_ERROR;

 free_str:
  mmap_string_free(mmapstr);
 unmap:
  munmap(str, size);
 close:
  close(fd);
 err:
  return res;
}


int mhdriver_fetch_size(mailsession * session, uint32_t index,
			size_t * result)
{
  struct mailmh_folder * folder;
  int r;
  struct stat buf;
  char * name;

  folder = get_mh_cur_folder(session);
  if (folder == NULL)
    return MAIL_ERROR_FETCH;

  r = mailmh_folder_get_message_filename(folder, index, &name);

  switch (r) {
  case MAILMH_NO_ERROR:
    break;

  default:
    return mhdriver_mh_error_to_mail_error(r);
  }

  r = stat(name, &buf);
  free(name);
  if (r == -1)
    return MAIL_ERROR_FETCH;

  * result = buf.st_size;

  return MAIL_NO_ERROR;
}

int
mhdriver_get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session,
    uint32_t num, 
    struct mail_flags ** result)
{
  int r;
  char keyname[PATH_MAX];
  struct mail_flags * flags;
  int res;
  struct mailmh_msg_info * msg_info;
  chashdatum key;
  chashdatum data;
  struct mailmh_folder * folder;
  
  folder = cached_get_mh_cur_folder(session);
#if 0
  msg_info = cinthash_find(mh_data->cur_folder->fl_msgs_hash, num);
  if (msg_info == NULL)
    return MAIL_ERROR_CACHE_MISS;
#endif
  key.data = &num;
  key.len = sizeof(num);
  r = chash_get(folder->fl_msgs_hash, &key, &data);
  if (r < 0)
    return MAIL_ERROR_CACHE_MISS;
  msg_info = data.data;
  
  snprintf(keyname, PATH_MAX, "%u-%lu-%lu-flags",
	   num, (unsigned long) msg_info->msg_mtime,
      (unsigned long) msg_info->msg_size);

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
mhdriver_write_cached_flags(struct mail_cache_db * cache_db,
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


int mh_get_messages_list(struct mailmh_folder * folder,
			 mailsession * session, mailmessage_driver * driver,
			 struct mailmessage_list ** result)
{
  unsigned int i;
  struct mailmessage_list * env_list;
  int r;
  carray * tab;
  int res;

  tab = carray_new(128);
  if (tab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < carray_count(folder->fl_msgs_tab) ; i++) {
    struct mailmh_msg_info * mh_info;
    mailmessage * msg;
    
    mh_info = carray_get(folder->fl_msgs_tab, i);
    if (mh_info == NULL)
      continue;

    msg = mailmessage_new();
    if (msg == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = mailmessage_init(msg, session, driver,
			 mh_info->msg_index, mh_info->msg_size);
    if (r != MAIL_NO_ERROR) {
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

  env_list = mailmessage_list_new(tab);
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
