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
 * $Id: mailmbox_types.c,v 1.22 2006/05/22 13:39:42 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmbox_types.h"

#include <string.h>
#include <stdlib.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* *********************************************************************** */

int mailmbox_msg_info_update(struct mailmbox_folder * folder,
			     size_t msg_start, size_t msg_start_len,
			     size_t msg_headers, size_t msg_headers_len,
			     size_t msg_body, size_t msg_body_len,
			     size_t msg_size, size_t msg_padding,
			     uint32_t msg_uid)
{
  struct mailmbox_msg_info * info;
  int res;
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = &msg_uid;
  key.len = sizeof(msg_uid);
  r = chash_get(folder->mb_hash, &key, &data);
  if (r < 0) {
    unsigned int index;

    info = mailmbox_msg_info_new(msg_start, msg_start_len,
        msg_headers, msg_headers_len,
        msg_body, msg_body_len, msg_size, msg_padding, msg_uid);
    if (info == NULL) {
      res = MAILMBOX_ERROR_MEMORY;
      goto err;
    }

    r = carray_add(folder->mb_tab, info, &index);
    if (r < 0) {
      mailmbox_msg_info_free(info);
      res = MAILMBOX_ERROR_MEMORY;
      goto err;
    }

    if (msg_uid != 0) {
      chashdatum key;
      chashdatum data;
      
      key.data = &msg_uid;
      key.len = sizeof(msg_uid);
      data.data = info;
      data.len = 0;
      
      r = chash_set(folder->mb_hash, &key, &data, NULL);
      if (r < 0) {
	mailmbox_msg_info_free(info);
	carray_delete(folder->mb_tab, index);
	res = MAILMBOX_ERROR_MEMORY;
	goto err;
      }
    }
    
    info->msg_index = index;
  }
  else {
    info = data.data;
    
    info->msg_start = msg_start;
    info->msg_start_len = msg_start_len;
    info->msg_headers = msg_headers;
    info->msg_headers_len = msg_headers_len;
    info->msg_body = msg_body;
    info->msg_body_len = msg_body_len;
    info->msg_size = msg_size;
    info->msg_padding = msg_padding;
  }

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}


struct mailmbox_msg_info *
mailmbox_msg_info_new(size_t msg_start, size_t msg_start_len,
		      size_t msg_headers, size_t msg_headers_len,
		      size_t msg_body, size_t msg_body_len,
		      size_t msg_size, size_t msg_padding,
		      uint32_t msg_uid)
{
  struct mailmbox_msg_info * info;

  info = malloc(sizeof(* info));
  if (info == NULL)
    return NULL;

  info->msg_index = 0;
  info->msg_uid = msg_uid;
  if (msg_uid != 0)
    info->msg_written_uid = TRUE;
  else
    info->msg_written_uid = FALSE;
  info->msg_deleted = FALSE;

  info->msg_start = msg_start;
  info->msg_start_len = msg_start_len;

  info->msg_headers = msg_headers;
  info->msg_headers_len = msg_headers_len;

  info->msg_body = msg_body;
  info->msg_body_len = msg_body_len;

  info->msg_size = msg_size;

  info->msg_padding = msg_padding;

  return info;
}

void mailmbox_msg_info_free(struct mailmbox_msg_info * info)
{
  free(info);
}


/* append info */

struct mailmbox_append_info *
mailmbox_append_info_new(const char * ai_message, size_t ai_size)
{
  struct mailmbox_append_info * info;

  info = malloc(sizeof(* info));
  if (info == NULL)
    return NULL;

  info->ai_message = ai_message;
  info->ai_size = ai_size;
  info->ai_uid = 0;
  
  return info;
}

void mailmbox_append_info_free(struct mailmbox_append_info * info)
{
  free(info);
}

struct mailmbox_folder * mailmbox_folder_new(const char * mb_filename)
{
  struct mailmbox_folder * folder;

  folder = malloc(sizeof(* folder));
  if (folder == NULL)
    goto err;

  strncpy(folder->mb_filename, mb_filename, PATH_MAX);

  folder->mb_mtime = (time_t) -1;

  folder->mb_fd = -1;
  folder->mb_read_only = TRUE;
  folder->mb_no_uid = TRUE;

  folder->mb_changed = FALSE;
  folder->mb_deleted_count = 0;
  
  folder->mb_mapping = NULL;
  folder->mb_mapping_size = 0;

  folder->mb_written_uid = 0;
  folder->mb_max_uid = 0;

  folder->mb_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (folder->mb_hash == NULL)
    goto free;
  
  folder->mb_tab = carray_new(128);
  if (folder->mb_tab == NULL)
    goto free_hash;

  return folder;

 free_hash:
  chash_free(folder->mb_hash);
 free:
  free(folder);
 err:
  return NULL;
}

void mailmbox_folder_free(struct mailmbox_folder * folder)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(folder->mb_tab) ; i++) {
    struct mailmbox_msg_info * info;

    info = carray_get(folder->mb_tab, i);
    if (info != NULL)
      mailmbox_msg_info_free(info);
  }

  carray_free(folder->mb_tab);
  
  chash_free(folder->mb_hash);

  free(folder);
}
