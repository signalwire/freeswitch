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
 * $Id: mailmbox_parse.c,v 1.15 2006/05/22 13:39:42 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmbox_parse.h"

#include "mailmbox.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#define UID_HEADER "X-LibEtPan-UID:"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum {
  UNSTRUCTURED_START,
  UNSTRUCTURED_CR,
  UNSTRUCTURED_LF,
  UNSTRUCTURED_WSP,
  UNSTRUCTURED_OUT
};

static inline int
mailmbox_fields_parse(char * str, size_t length,
		      size_t * index,
		      uint32_t * puid,
		      size_t * phlen)
{
  size_t cur_token;
  int r;
  size_t hlen;
  size_t uid;
  int end;

  cur_token = * index;

  end = FALSE;
  uid = 0;
  while (!end) {
    size_t begin;

    begin = cur_token;

    r = mailimf_ignore_field_parse(str, length, &cur_token);
    switch (r) {
    case MAILIMF_NO_ERROR:
      if (str[begin] == 'X') {

	if (strncasecmp(str + begin, UID_HEADER, strlen(UID_HEADER)) == 0) {
	  begin += strlen(UID_HEADER);

	  while (str[begin] == ' ')
	    begin ++;
	  
	  uid = strtoul(str + begin, NULL, 10);
	}
      }
      
      break;
    case MAILIMF_ERROR_PARSE:
    default:
      end = TRUE;
      break;
    }
  }

  hlen = cur_token - * index;

  * phlen = hlen;
  * puid = uid;
  * index = cur_token;

  return MAILMBOX_NO_ERROR;
}

enum {
  IN_MAIL,
  FIRST_CR,
  FIRST_LF,
  SECOND_CR,
  SECOND_LF,
  PARSING_F,
  PARSING_R,
  PARSING_O,
  PARSING_M,
  OUT_MAIL
};




static inline int
mailmbox_single_parse(char * str, size_t length,
		      size_t * index,
		      size_t * pstart,
		      size_t * pstart_len,
		      size_t * pheaders,
		      size_t * pheaders_len,
		      size_t * pbody,
		      size_t * pbody_len,
		      size_t * psize,
		      size_t * ppadding,
		      uint32_t * puid)
{
  size_t cur_token;
  size_t start;
  size_t start_len;
  size_t headers;
  size_t headers_len;
  size_t body;
  size_t end;
  size_t next;
  size_t message_length;
  uint32_t uid;
  int r;
#if 0
  int in_mail_data;
#endif
#if 0
  size_t begin;
#endif

  int state;

  cur_token = * index;

  if (cur_token >= length)
    return MAILMBOX_ERROR_PARSE;

  start = cur_token;
  start_len = 0;
  headers = cur_token;

  if (cur_token + 5 < length) {
    if (strncmp(str + cur_token, "From ", 5) == 0) {
      cur_token += 5;
      while (str[cur_token] != '\n') {
        cur_token ++;
        if (cur_token >= length)
          break;
      }
      if (cur_token < length) {
        cur_token ++;
        headers = cur_token;
        start_len = headers - start;
      }
    }
  }

  next = length;

  r = mailmbox_fields_parse(str, length, &cur_token,
			    &uid, &headers_len);
  if (r != MAILMBOX_NO_ERROR)
    return r;

  /* save position */
#if 0
  begin = cur_token;
#endif
  
  mailimf_crlf_parse(str, length, &cur_token);

#if 0
  if (str[cur_token] == 'F') {
    printf("start !\n");
    printf("%50.50s\n", str + cur_token);
    getchar();
  }
#endif
  
  body = cur_token;

  /* restore position */
  /*  cur_token = begin; */

  state = FIRST_LF;

  end = length;

#if 0
  in_mail_data = 0;
#endif
  while (state != OUT_MAIL) {

    if (cur_token >= length) {
      if (state == IN_MAIL)
	end = length;
      next = length;
      break;
    }

    switch(state) {
    case IN_MAIL:
      switch(str[cur_token]) {
      case '\r':
        state = FIRST_CR;
        break;
      case '\n':
        state = FIRST_LF;
        break;
      case 'F':
        if (cur_token == body) {
          end = cur_token;
          next = cur_token;
          state = PARSING_F;
        }
        break;
#if 0
      default:
        in_mail_data = 1;
        break;
#endif
      }
      break;
      
    case FIRST_CR:
      end = cur_token;
      switch(str[cur_token]) {
      case '\r':
        state = SECOND_CR;
        break;
      case '\n':
        state = FIRST_LF;
        break;
      default:
        state = IN_MAIL;
#if 0
        in_mail_data = 1;
#endif
        break;
      }
      break;
      
    case FIRST_LF:
      end = cur_token;
      switch(str[cur_token]) {
      case '\r':
        state = SECOND_CR;
        break;
      case '\n':
        state = SECOND_LF;
        break;
      default:
        state = IN_MAIL;
#if 0
        in_mail_data = 1;
#endif
        break;
      }
      break;
      
    case SECOND_CR:
      switch(str[cur_token]) {
        case '\r':
          end = cur_token;
          break;
        case '\n':
          state = SECOND_LF;
          break;
        case 'F':
          next = cur_token;
          state = PARSING_F;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;

    case SECOND_LF:
      switch(str[cur_token]) {
        case '\r':
          state = SECOND_CR;
          break;
        case '\n':
          end = cur_token;
          break;
        case 'F':
          next = cur_token;
          state = PARSING_F;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;
      
    case PARSING_F:
      switch(str[cur_token]) {
        case 'r':
          state = PARSING_R;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;
      
    case PARSING_R:
      switch(str[cur_token]) {
        case 'o':
          state = PARSING_O;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;
      
    case PARSING_O:
      switch(str[cur_token]) {
        case 'm':
          state = PARSING_M;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;

    case PARSING_M:
      switch(str[cur_token]) {
        case ' ':
          state = OUT_MAIL;
          break;
      default:
          state = IN_MAIL;
          break;
      }
      break;
    }
    
    cur_token ++;
  }

  message_length = end - start;

  * pstart = start;
  * pstart_len = start_len;
  * pheaders = headers;
  * pheaders_len = headers_len;
  * pbody = body;
  * pbody_len = end - body;
  * psize = message_length;
  * ppadding = next - end;
  * puid = uid;

  * index = next;

  return MAILMBOX_NO_ERROR;
}


int
mailmbox_parse_additionnal(struct mailmbox_folder * folder,
			   size_t * index)
{
  size_t cur_token;

  size_t start;
  size_t start_len;
  size_t headers;
  size_t headers_len;
  size_t body;
  size_t body_len;
  size_t size;
  size_t padding;
  uint32_t uid;
  int r;
  int res;

  uint32_t max_uid;
  uint32_t first_index;
  unsigned int i;
  unsigned int j;

  cur_token = * index;

  /* remove temporary UID that we will parse */

  first_index = carray_count(folder->mb_tab);

  for(i = 0 ; i < carray_count(folder->mb_tab) ; i++) {
    struct mailmbox_msg_info * info;
    
    info = carray_get(folder->mb_tab, i);

    if (info->msg_start < cur_token) {
      continue;
    }

    if (!info->msg_written_uid) {
      chashdatum key;
      
      key.data = &info->msg_uid;
      key.len = sizeof(info->msg_uid);
      
      chash_delete(folder->mb_hash, &key, NULL);
      carray_delete_fast(folder->mb_tab, i);
      mailmbox_msg_info_free(info);
      if (i < first_index)
	first_index = i;
    }
  }

  /* make a sequence in the table */

  max_uid = folder->mb_written_uid;

  i = 0;
  j = 0;
  while (i < carray_count(folder->mb_tab)) {
    struct mailmbox_msg_info * info;
    
    info = carray_get(folder->mb_tab, i);
    if (info != NULL) {
      carray_set(folder->mb_tab, j, info);

      if (info->msg_uid > max_uid)
	max_uid = info->msg_uid;

      info->msg_index = j;
      j ++;
    }
    i ++;
  }
  carray_set_size(folder->mb_tab, j);

  /* parse content */

  first_index = j;

  while (1) {
    struct mailmbox_msg_info * info;
    chashdatum key;
    chashdatum data;
    
    r = mailmbox_single_parse(folder->mb_mapping, folder->mb_mapping_size,
			      &cur_token,
			      &start, &start_len,
			      &headers, &headers_len,
			      &body, &body_len,
			      &size, &padding, &uid);
    if (r == MAILMBOX_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILMBOX_ERROR_PARSE)
      break;
    else {
      res = r;
      goto err;
    }
    
    key.data = &uid;
    key.len = sizeof(uid);
    
    r = chash_get(folder->mb_hash, &key, &data);
    if (r == 0) {
      info = data.data;
      
      if (!info->msg_written_uid) {
	/* some new mail has been written and override an
	   existing temporary UID */
        
	chash_delete(folder->mb_hash, &key, NULL);
	info->msg_uid = 0;

	if (info->msg_index < first_index)
	  first_index = info->msg_index;
      }
      else
        uid = 0;
    }

    if (uid > max_uid)
      max_uid = uid;

    r = mailmbox_msg_info_update(folder,
				 start, start_len, headers, headers_len,
				 body, body_len, size, padding, uid);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  * index = cur_token;

  folder->mb_written_uid = max_uid;

  /* attribute uid */

  for(i = first_index ; i < carray_count(folder->mb_tab) ; i ++) {
    struct mailmbox_msg_info * info;
    chashdatum key;
    chashdatum data;

    info = carray_get(folder->mb_tab, i);

    if (info->msg_uid != 0) {
      continue;
    }

    max_uid ++;
    info->msg_uid = max_uid;
    
    key.data = &info->msg_uid;
    key.len = sizeof(info->msg_uid);
    data.data = info;
    data.len = 0;
    
    r = chash_set(folder->mb_hash, &key, &data, NULL);
    if (r < 0) {
      res = MAILMBOX_ERROR_MEMORY;
      goto err;
    }
  }

  folder->mb_max_uid = max_uid;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

static void flush_uid(struct mailmbox_folder * folder)
{
  unsigned int i;
  
  for(i = 0 ; i < carray_count(folder->mb_tab) ; i++) {
    struct mailmbox_msg_info * info;
    
    info = carray_get(folder->mb_tab, i);
    if (info != NULL)
      mailmbox_msg_info_free(info);
  }
  
  chash_clear(folder->mb_hash);
  carray_set_size(folder->mb_tab, 0);
}

int mailmbox_parse(struct mailmbox_folder * folder)
{
  int r;
  int res;
  size_t cur_token;

  flush_uid(folder);
  
  cur_token = 0;

  r = mailmbox_parse_additionnal(folder, &cur_token);

  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}
