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
 * $Id: imapdriver_cached_message.c,v 1.25 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imapdriver_cached_message.h"

#include "imapdriver_tools.h"
#include "imapdriver_message.h"
#include "imapdriver_cached.h"
#include "imapdriver_types.h"
#include "imapdriver.h"
#include "mailmessage.h"
#include "generic_cache.h"
#include "mail_cache_db.h"

#include <string.h>
#include <stdlib.h>

static int imap_initialize(mailmessage * msg_info);

static void imap_uninitialize(mailmessage * msg_info);

static void imap_flush(mailmessage * msg_info);

static void imap_check(mailmessage * msg_info);

static void imap_fetch_result_free(mailmessage * msg_info,
				   char * msg);

static int imap_fetch(mailmessage * msg_info,
		      char ** result,
		      size_t * result_len);

static int imap_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len);

static int imap_fetch_body(mailmessage * msg_info,
			   char ** result, size_t * result_len);

static int imap_fetch_size(mailmessage * msg_info,
			   size_t * result);

static int imap_get_bodystructure(mailmessage * msg_info,
				  struct mailmime ** result);

static int imap_fetch_section(mailmessage * msg_info,
			      struct mailmime * mime,
			      char ** result, size_t * result_len);

static int imap_fetch_section_header(mailmessage * msg_info,
				     struct mailmime * mime,
				     char ** result,
				     size_t * result_len);

static int imap_fetch_section_mime(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len);

static int imap_fetch_section_body(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len);


static int imap_fetch_envelope(mailmessage * msg_info,
			       struct mailimf_fields ** result);

static int imap_get_flags(mailmessage * msg_info,
			  struct mail_flags ** result);

static mailmessage_driver local_imap_cached_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* msg_name */ "imap-cached",

  /* msg_initialize */ imap_initialize,
  /* msg_uninitialize */ imap_uninitialize,

  /* msg_flush */ imap_flush,
  /* msg_check */ imap_check,

  /* msg_fetch_result_free */ imap_fetch_result_free,

  /* msg_fetch */ imap_fetch,
  /* msg_fetch_header */ imap_fetch_header,
  /* msg_fetch_body */ imap_fetch_body,
  /* msg_fetch_size */ imap_fetch_size,
  /* msg_get_bodystructure */ imap_get_bodystructure,
  /* msg_fetch_section */ imap_fetch_section,
  /* msg_fetch_section_header */ imap_fetch_section_header,
  /* msg_fetch_section_mime */ imap_fetch_section_mime,
  /* msg_fetch_section_body */ imap_fetch_section_body,
  /* msg_fetch_envelope */ imap_fetch_envelope,

  /* msg_get_flags */ imap_get_flags,
#else
  .msg_name = "imap-cached",

  .msg_initialize = imap_initialize,
  .msg_uninitialize = imap_uninitialize,

  .msg_flush = imap_flush,
  .msg_check = imap_check,

  .msg_fetch_result_free = imap_fetch_result_free,

  .msg_fetch = imap_fetch,
  .msg_fetch_header = imap_fetch_header,
  .msg_fetch_body = imap_fetch_body,
  .msg_fetch_size = imap_fetch_size,
  .msg_get_bodystructure = imap_get_bodystructure,
  .msg_fetch_section = imap_fetch_section,
  .msg_fetch_section_header = imap_fetch_section_header,
  .msg_fetch_section_mime = imap_fetch_section_mime,
  .msg_fetch_section_body = imap_fetch_section_body,
  .msg_fetch_envelope = imap_fetch_envelope,

  .msg_get_flags = imap_get_flags,
#endif
};

mailmessage_driver * imap_cached_message_driver =
&local_imap_cached_message_driver;

static inline struct imap_cached_session_state_data *
get_cached_session_data(mailmessage * msg)
{
  return msg->msg_session->sess_data;
}

static inline mailmessage * get_ancestor(mailmessage * msg_info)
{
  return msg_info->msg_data;
}

static inline struct imap_cached_session_state_data *
cached_session_get_data(mailsession * s)
{
  return s->sess_data;
}

static inline mailsession * cached_session_get_ancestor(mailsession * s)
{
  return cached_session_get_data(s)->imap_ancestor;
}

static inline struct imap_session_state_data *
cached_session_get_ancestor_data(mailsession * s)
{
  return cached_session_get_ancestor(s)->sess_data;
}

static inline mailimap *
cached_session_get_imap_session(mailsession * session)
{
  return cached_session_get_ancestor_data(session)->imap_session;
}

static inline mailimap * get_imap_session(mailmessage * msg)
{
  return cached_session_get_imap_session(msg->msg_session);
}

static inline mailsession * get_ancestor_session(mailmessage * msg_info)
{
  return cached_session_get_ancestor(msg_info->msg_session);
}


static void generate_key_from_mime_section(char * key, size_t size,
					   struct mailmime * mime)
{
  clistiter * cur;
  MMAPString * gstr;
  struct mailmime_section * part;
  int r;

  snprintf(key, size, "unvalid");

  r = mailmime_get_section_id(mime, &part);
  if (r != MAILIMF_NO_ERROR)
    goto err;

  gstr = mmap_string_new("part");
  if (gstr == NULL)
    goto free_section;

  for(cur = clist_begin(part->sec_list) ;
      cur != NULL ; cur = clist_next(cur)) {
    char s[20];

    snprintf(s, 20, ".%u", * (uint32_t *) clist_content(cur));
    if (mmap_string_append(gstr, s) == NULL)
      goto free_str;
  }

  snprintf(key, size, "%s", gstr->str);

  mmap_string_free(gstr);
  mailmime_section_free(part);

  return;

 free_str:
  mmap_string_free(gstr);
 free_section:
  mailmime_section_free(part);
 err:;
}

static void generate_key_from_section(char * key, size_t size,
				      mailmessage * msg_info,
				      struct mailmime * mime, int type)
{
  char section_str[PATH_MAX];

  generate_key_from_mime_section(section_str, PATH_MAX, mime);

  switch (type) {
  case IMAP_SECTION_MESSAGE:
    snprintf(key, size, "%s-%s", msg_info->msg_uid, section_str);
    break;
  case IMAP_SECTION_HEADER:
    snprintf(key, size, "%s-%s-header", msg_info->msg_uid, section_str);
    break;
  case IMAP_SECTION_MIME:
    snprintf(key, size, "%s-%s-mime", msg_info->msg_uid, section_str);
    break;
  case IMAP_SECTION_BODY:
    snprintf(key, size, "%s-%s-text", msg_info->msg_uid, section_str);
    break;
  }
}

static void generate_key_from_message(char * key, size_t size,
				      mailmessage * msg_info,
				      int type)
{
  switch (type) {
  case MAILIMAP_MSG_ATT_RFC822:
    snprintf(key, size, "%s-rfc822", msg_info->msg_uid);
    break;
  case MAILIMAP_MSG_ATT_RFC822_HEADER:
    snprintf(key, size, "%s-rfc822-header", msg_info->msg_uid);
    break;
  case MAILIMAP_MSG_ATT_RFC822_TEXT:
    snprintf(key, size, "%s-rfc822-text", msg_info->msg_uid);
    break;
  case MAILIMAP_MSG_ATT_ENVELOPE:
    snprintf(key, size, "%s-envelope", msg_info->msg_uid);
    break;
  }
}

static void build_cache_name(char * filename, size_t size,
			     mailmessage * msg, char * key)
{
  char * quoted_mb;

  quoted_mb = get_cached_session_data(msg)->imap_quoted_mb;

  snprintf(filename, size, "%s/%s", quoted_mb, key);
}

static int imap_initialize(mailmessage * msg_info)
{
  mailmessage * ancestor;
  int r;
  char key[PATH_MAX];
  char * uid;
  mailimap * imap;

  ancestor = mailmessage_new();
  if (ancestor == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailmessage_init(ancestor, get_ancestor_session(msg_info),
		       imap_message_driver,
		       msg_info->msg_index, 0);
  if (r != MAIL_NO_ERROR) {
    mailmessage_free(ancestor);
    return r;
  }

  imap = get_imap_session(msg_info);

  snprintf(key, PATH_MAX, "%u-%u",
	   imap->imap_selection_info->sel_uidvalidity, msg_info->msg_index);
  uid = strdup(key);
  if (uid == NULL) {
    mailmessage_free(ancestor);
    return MAIL_ERROR_MEMORY;
  }
  
  msg_info->msg_data = ancestor;
  msg_info->msg_uid = uid;

  return MAIL_NO_ERROR;
}
  
static void imap_uninitialize(mailmessage * msg_info)
{
  mailmessage_free(get_ancestor(msg_info));
  msg_info->msg_data = NULL;
}

static void imap_flush(mailmessage * msg_info)
{
  if (msg_info->msg_mime != NULL) {
    mailmime_free(msg_info->msg_mime);
    msg_info->msg_mime = NULL;
  }
}

static void imap_check(mailmessage * msg_info)
{
  get_ancestor(msg_info)->msg_flags = msg_info->msg_flags;
  mailmessage_check(get_ancestor(msg_info));
  get_ancestor(msg_info)->msg_flags = NULL;
}

static void imap_fetch_result_free(mailmessage * msg_info,
				   char * msg)
{
  mailmessage_fetch_result_free(get_ancestor(msg_info), msg);
}

static int imap_fetch(mailmessage * msg_info,
		      char ** result,
		      size_t * result_len)
{
  char key[PATH_MAX];
  char filename[PATH_MAX];
  int r;
  char * str;
  size_t len;

  generate_key_from_message(key, PATH_MAX,
			    msg_info, MAILIMAP_MSG_ATT_RFC822);

  build_cache_name(filename, PATH_MAX, msg_info, key);

  r = generic_cache_read(filename, &str, &len);
  if (r == MAIL_NO_ERROR) {
    * result = str;
    * result_len = len;

    return MAIL_NO_ERROR;
  }

  r = mailmessage_fetch(get_ancestor(msg_info),
			result, result_len);
  if (r == MAIL_NO_ERROR)
    generic_cache_store(filename, * result, strlen(* result));

  return r;
}
       
static int imap_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len)
{
  char key[PATH_MAX];
  char filename[PATH_MAX];
  int r;
  char * str;
  size_t len;

  generate_key_from_message(key, PATH_MAX,
			    msg_info, MAILIMAP_MSG_ATT_RFC822_HEADER);

  build_cache_name(filename, PATH_MAX, msg_info, key);

  r = generic_cache_read(filename, &str, &len);
  if (r == MAIL_NO_ERROR) {
    * result = str;
    * result_len = len;

    return MAIL_NO_ERROR;
  }

  r = mailmessage_fetch_header(get_ancestor(msg_info), result,
			       result_len);
  if (r == MAIL_NO_ERROR)
    generic_cache_store(filename, * result, * result_len);

  return r;
}
  
static int imap_fetch_body(mailmessage * msg_info,
			   char ** result, size_t * result_len)
{
  char key[PATH_MAX];
  char filename[PATH_MAX];
  int r;
  char * str;
  size_t len;

  generate_key_from_message(key, PATH_MAX,
			    msg_info, MAILIMAP_MSG_ATT_RFC822_TEXT);

  build_cache_name(filename, PATH_MAX, msg_info, key);

  r = generic_cache_read(filename, &str, &len);
  if (r == MAIL_NO_ERROR) {
    * result = str;
    * result_len = len;
    return MAIL_NO_ERROR;
  }

  r = mailmessage_fetch_body(get_ancestor(msg_info), result,
			     result_len);
  if (r == MAIL_NO_ERROR)
    generic_cache_store(filename, * result, * result_len);

  return r;
}

static int imap_fetch_size(mailmessage * msg_info,
			   size_t * result)
{
  return mailmessage_fetch_size(get_ancestor(msg_info), result);
}

static int imap_get_bodystructure(mailmessage * msg_info,
				  struct mailmime ** result)
{
  int r;
  
  if (msg_info->msg_mime != NULL) {
    * result = msg_info->msg_mime;

    return MAIL_NO_ERROR;
  }
  
  r = mailmessage_get_bodystructure(get_ancestor(msg_info),
      result);
  if (r == MAIL_NO_ERROR) {
    msg_info->msg_mime = get_ancestor(msg_info)->msg_mime;
    get_ancestor(msg_info)->msg_mime = NULL;
  }
  
  return r;
}

static int imap_fetch_section(mailmessage * msg_info,
			      struct mailmime * mime,
			      char ** result, size_t * result_len)
{
  char key[PATH_MAX];
  char filename[PATH_MAX];
  int r;
  char * str;
  size_t len;

  generate_key_from_section(key, PATH_MAX,
			    msg_info, mime, IMAP_SECTION_MESSAGE);

  build_cache_name(filename, PATH_MAX, msg_info, key);

  r = generic_cache_read(filename, &str, &len);
  if (r == MAIL_NO_ERROR) {
    * result = str;
    * result_len = len;

    return MAIL_NO_ERROR;
  }

  r = mailmessage_fetch_section(get_ancestor(msg_info),
				mime, result, result_len);
  if (r == MAIL_NO_ERROR)
    generic_cache_store(filename, * result, * result_len);

  return r;
}
  
static int imap_fetch_section_header(mailmessage * msg_info,
				     struct mailmime * mime,
				     char ** result,
				     size_t * result_len)
{
  char key[PATH_MAX];
  char filename[PATH_MAX];
  int r;
  char * str;
  size_t len;

  generate_key_from_section(key, PATH_MAX,
			    msg_info, mime, IMAP_SECTION_HEADER);

  build_cache_name(filename, PATH_MAX, msg_info, key);

  r = generic_cache_read(filename, &str, &len);
  if (r == MAIL_NO_ERROR) {
    * result = str;
    * result_len = len;

    return MAIL_NO_ERROR;
  }

  r = mailmessage_fetch_section_header(get_ancestor(msg_info),
				       mime, result, result_len);
  if (r == MAIL_NO_ERROR)
    generic_cache_store(filename, * result, * result_len);

  return r;
}
  
static int imap_fetch_section_mime(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len)
{
  char key[PATH_MAX];
  char filename[PATH_MAX];
  int r;
  char * str;
  size_t len;

  generate_key_from_section(key, PATH_MAX,
			    msg_info, mime, IMAP_SECTION_MIME);

  build_cache_name(filename, PATH_MAX, msg_info, key);

  r = generic_cache_read(filename, &str, &len);
  if (r == MAIL_NO_ERROR) {
    * result = str;
    * result_len = len;

    return MAIL_NO_ERROR;
  }

  r = mailmessage_fetch_section_mime(get_ancestor(msg_info),
				     mime, result, result_len);
  if (r == MAIL_NO_ERROR)
    generic_cache_store(filename, * result, * result_len);

  return r;
}
  
static int imap_fetch_section_body(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len)
{
  char key[PATH_MAX];
  char filename[PATH_MAX];
  int r;
  char * str;
  size_t len;
  
  generate_key_from_section(key, PATH_MAX,
			    msg_info, mime, IMAP_SECTION_BODY);

  build_cache_name(filename, PATH_MAX, msg_info, key);

  r = generic_cache_read(filename, &str, &len);
  if (r == MAIL_NO_ERROR) {

    * result = str;
    * result_len = len;

    return MAIL_NO_ERROR;
  }

  r = mailmessage_fetch_section_body(get_ancestor(msg_info),
				     mime, result, result_len);
  if (r == MAIL_NO_ERROR)
    generic_cache_store(filename, * result, * result_len);

  return r;
}

static int imap_get_flags(mailmessage * msg_info,
			  struct mail_flags ** result)
{
  int r;
  struct mail_flags * flags;
  
  if (msg_info->msg_flags != NULL) {
    * result = msg_info->msg_flags;
    return MAIL_NO_ERROR;
  }
  
  r = mailmessage_get_flags(get_ancestor(msg_info), &flags);
  if (r != MAIL_NO_ERROR)
    return r;

  get_ancestor(msg_info)->msg_flags = NULL;
  msg_info->msg_flags = flags;
  * result = flags;
  
  return MAIL_NO_ERROR;
}

#define ENV_NAME "env.db"

static int imap_fetch_envelope(mailmessage * msg_info,
			       struct mailimf_fields ** result)
{
  struct mailimf_fields * fields;
  int r;
  struct mail_cache_db * cache_db;
  MMAPString * mmapstr;
  char filename[PATH_MAX];
  struct imap_cached_session_state_data * data;
  int res;
  
  data = get_cached_session_data(msg_info);
  if (data->imap_quoted_mb == NULL) {
    res = MAIL_ERROR_BAD_STATE;
    goto err;
  }

  snprintf(filename, PATH_MAX, "%s/%s", data->imap_quoted_mb, ENV_NAME);

  r = mail_cache_db_open_lock(filename, &cache_db);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_db;
  }

  r = imapdriver_get_cached_envelope(cache_db, mmapstr,
      msg_info->msg_session, msg_info, &fields);

  if ((r != MAIL_ERROR_CACHE_MISS) && (r != MAIL_NO_ERROR)) {
    res = r;
    goto close_db;
  }
  
  r = mailmessage_fetch_envelope(get_ancestor(msg_info), &fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto close_db;
  }

  r = imapdriver_write_cached_envelope(cache_db, mmapstr,
      msg_info->msg_session, msg_info, fields);

  * result = fields;

  mmap_string_free(mmapstr);
  mail_cache_db_close_unlock(filename, cache_db);

  return MAIL_NO_ERROR;

 close_db:
  mail_cache_db_close_unlock(filename, cache_db);
 err:
  return res;
}
