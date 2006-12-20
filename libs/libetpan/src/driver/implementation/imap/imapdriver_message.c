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
 * $Id: imapdriver_message.c,v 1.24 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imapdriver_message.h"

#include "imapdriver_tools.h"
#include "imapdriver.h"
#include "imapdriver_types.h"
#include "mailimap.h"
#include "maildriver_tools.h"
#include "generic_cache.h"

#include <stdlib.h>
#include <string.h>

static int imap_initialize(mailmessage * msg_info);

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

static void imap_flush(mailmessage * msg_info);

static void imap_check(mailmessage * msg_info);

static mailmessage_driver local_imap_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* msg_name */ "imap",
  
  /* msg_initialize */ imap_initialize,
  /* msg_uninitialize */ NULL,

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
  .msg_name = "imap",
  
  .msg_initialize = imap_initialize,
  .msg_uninitialize = NULL,

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

mailmessage_driver * imap_message_driver = &local_imap_message_driver;

static inline struct imap_session_state_data *
get_session_data(mailmessage * msg)
{
  return msg->msg_session->sess_data;
}

static inline mailimap * get_imap_session(mailmessage * msg)
{
  return get_session_data(msg)->imap_session;
}



static int imap_initialize(mailmessage * msg_info)
{
  char key[PATH_MAX];
  char * uid;
  mailimap * imap;

  imap = get_imap_session(msg_info);

  snprintf(key, PATH_MAX, "%u-%u",
      imap->imap_selection_info->sel_uidvalidity, msg_info->msg_index);

  uid = strdup(key);
  if (uid == NULL) {
    return MAIL_ERROR_MEMORY;
  }
  
  msg_info->msg_uid = uid;

  return MAIL_NO_ERROR;
}


static void imap_fetch_result_free(mailmessage * msg_info,
				   char * msg)
{
  if (msg != NULL) {
    if (mmap_string_unref(msg) != 0)
      free(msg);
  }
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
  int r;
  
  if (msg_info->msg_flags != NULL) {
    r = mail_flags_store_set(get_session_data(msg_info)->imap_flags_store,
        msg_info);
    /* ignore errors */
  }
}

static int imap_fetch(mailmessage * msg_info,
		      char ** result,
		      size_t * result_len)
{
  int r;
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  clist * fetch_result;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * msg_att_item;
  char * text;
  size_t text_length;
  int res;
  clistiter * cur;
  struct mailimap_section * section;

  set = mailimap_set_new_single(msg_info->msg_index);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

#if 0
  fetch_att = mailimap_fetch_att_new_rfc822();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info->session), set,
			 fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
#endif

  section = mailimap_section_new(NULL);
  if (section == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info), set,
			 fetch_type, &fetch_result);
  
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  
  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }
  
  if (clist_begin(fetch_result) == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAIL_ERROR_FETCH;
  }

  msg_att = clist_begin(fetch_result)->data;

  text = NULL;
  text_length = 0;

  for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    msg_att_item = clist_content(cur);

    if (msg_att_item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
#if 0
      if (msg_att_item->msg_att_static->type == MAILIMAP_MSG_ATT_RFC822) {
	text = msg_att_item->msg_att_static->rfc822;
	msg_att_item->msg_att_static->rfc822 = NULL;
	text_length = msg_att_item->msg_att_static->length;
      }
#endif
      if (msg_att_item->att_data.att_static->att_type ==
	  MAILIMAP_MSG_ATT_BODY_SECTION) {
	text = msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part;
        /* detach */
	msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part = NULL;
	text_length =
	  msg_att_item->att_data.att_static->att_data.att_body_section->sec_length;
      }
    }
  }

  mailimap_fetch_list_free(fetch_result);

  if (text == NULL)
    return MAIL_ERROR_FETCH;

  * result = text;
  * result_len = text_length;
  
  return MAIL_NO_ERROR;

 free_fetch_att:
  mailimap_fetch_att_free(fetch_att);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}
       
static int imap_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len)
{
  int r;
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  clist * fetch_result;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * msg_att_item;
  char * text;
  size_t text_length;
  int res;
  clistiter * cur;
  struct mailimap_section * section;
  
  set = mailimap_set_new_single(msg_info->msg_index);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

#if 0
  fetch_att = mailimap_fetch_att_new_rfc822_header();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info->session),
			 set, fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
#endif

  section = mailimap_section_new_header();
  if (section == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info), set,
      fetch_type, &fetch_result);
  
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  if (clist_begin(fetch_result) == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAIL_ERROR_FETCH;
  }

  msg_att = clist_begin(fetch_result)->data;

  text = NULL;
  text_length = 0;

  for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    msg_att_item = clist_content(cur);

    if (msg_att_item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
#if 0
      if (msg_att_item->msg_att_static->type ==
	  MAILIMAP_MSG_ATT_RFC822_HEADER) {
	text = msg_att_item->msg_att_static->rfc822_header;
	msg_att_item->msg_att_static->rfc822_header = NULL;
	text_length = msg_att_item->msg_att_static->length;
      }
#endif
      if (msg_att_item->att_data.att_static->att_type ==
	  MAILIMAP_MSG_ATT_BODY_SECTION) {
	text = msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part;
	msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part = NULL;
	text_length =
	  msg_att_item->att_data.att_static->att_data.att_body_section->sec_length;
      }
    }
  }

  mailimap_fetch_list_free(fetch_result);

  if (text == NULL)
    return MAIL_ERROR_FETCH;

  * result = text;
  * result_len = text_length;

  return MAIL_NO_ERROR;

 free_fetch_att:
  mailimap_fetch_att_free(fetch_att);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}
  
static int imap_fetch_body(mailmessage * msg_info,
			   char ** result, size_t * result_len)
{
  int r;
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  clist * fetch_result;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * msg_att_item;
  char * text;
  size_t text_length;
  int res;
  clistiter * cur;
  struct mailimap_section * section;
  
  set = mailimap_set_new_single(msg_info->msg_index);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

#if 0
  fetch_att = mailimap_fetch_att_new_rfc822_text();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info->session), set,
			 fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
#endif
  section = mailimap_section_new_text();
  if (section == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info), set,
      fetch_type, &fetch_result);
  
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  
  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  cur = clist_begin(fetch_result);
  if (cur == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAIL_ERROR_FETCH;
  }

  msg_att = clist_content(cur);

  text = NULL;
  text_length = 0;

  for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    msg_att_item = clist_content(cur);

    if (msg_att_item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
#if 0
      if (msg_att_item->msg_att_static->type ==
	  MAILIMAP_MSG_ATT_RFC822_TEXT) {
	text = msg_att_item->msg_att_static->rfc822_text;
	msg_att_item->msg_att_static->rfc822_text = NULL;
	text_length = msg_att_item->msg_att_static->length;
      }
#endif
      if (msg_att_item->att_data.att_static->att_type ==
	  MAILIMAP_MSG_ATT_BODY_SECTION) {
	text = msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part;
	msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part = NULL;
	text_length =
	  msg_att_item->att_data.att_static->att_data.att_body_section->sec_length;
      }
    }
  }

  mailimap_fetch_list_free(fetch_result);

  if (text == NULL)
    return MAIL_ERROR_FETCH;

  * result = text;
  * result_len = text_length;

  return MAIL_NO_ERROR;

 free_fetch_att:
  mailimap_fetch_att_free(fetch_att);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}

static int imap_fetch_size(mailmessage * msg_info,
			   size_t * result)
{
  int r;
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  clist * fetch_result;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * msg_att_item;
  size_t size;
  int res;
  clistiter * cur;
  
  set = mailimap_set_new_single(msg_info->msg_index);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  fetch_att = mailimap_fetch_att_new_rfc822_size();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info), set,
			 fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  
  switch (r) {
  case MAILIMAP_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;
  case MAILIMAP_ERROR_STREAM:
    return MAIL_ERROR_STREAM;
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return MAIL_ERROR_FETCH;
  }

  if (clist_begin(fetch_result) == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAIL_ERROR_FETCH;
  }

  msg_att = clist_begin(fetch_result)->data;

  for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    msg_att_item = clist_content(cur);

    if (msg_att_item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {

      if (msg_att_item->att_data.att_static->att_type ==
	  MAILIMAP_MSG_ATT_RFC822_SIZE) {
	size = msg_att_item->att_data.att_static->att_data.att_rfc822_size;

	* result = size;
	
	mailimap_fetch_list_free(fetch_result);
	return MAIL_NO_ERROR;
      }
    }
  }

  mailimap_fetch_list_free(fetch_result);

  return MAIL_ERROR_FETCH;

 free_fetch_att:
  mailimap_fetch_att_free(fetch_att);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}

static int imap_get_bodystructure(mailmessage * msg_info,
				  struct mailmime ** result)
{
  int r;
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  clist * fetch_result;
  struct mailimap_msg_att * msg_att;
  struct mailimap_body * imap_body;
  struct mailmime * body;
  int res;
  struct mailimf_fields * fields;
  struct mailmime * new_body;
  struct mailmime_content * content_message;
  struct mailimap_envelope * envelope;
  uint32_t uid;
  char * references;
  size_t ref_size;
  clistiter * cur;

  if (msg_info->msg_mime != NULL) {
    * result = msg_info->msg_mime;

    return MAIL_NO_ERROR;
  }

  set = mailimap_set_new_single(msg_info->msg_index);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }

  fetch_att = mailimap_fetch_att_new_uid();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  fetch_att = mailimap_fetch_att_new_bodystructure();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = imap_add_envelope_fetch_att(fetch_type);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_fetch_type;
  }
  

  r = mailimap_uid_fetch(get_imap_session(msg_info), set,
			 fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  cur = clist_begin(fetch_result);
  if (cur == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAIL_ERROR_FETCH;
  }

  msg_att = clist_content(cur);

  uid = 0;
  references = NULL;
  ref_size = 0;
  imap_body = NULL;
  envelope = NULL;

  r = imap_get_msg_att_info(msg_att,
      &uid, &envelope, &references, &ref_size, NULL, &imap_body);
  if (r != MAIL_NO_ERROR) {
    mailimap_fetch_list_free(fetch_result);
    res = r;
    goto err;
  }

  if (uid != msg_info->msg_index) {
    mailimap_fetch_list_free(fetch_result);
    res = MAIL_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  if (imap_body == NULL) {
    mailimap_fetch_list_free(fetch_result);
    res = MAIL_ERROR_FETCH;
    goto err;
  }

  r = imap_body_to_body(imap_body, &body);
  if (r != MAIL_NO_ERROR) {
    mailimap_fetch_list_free(fetch_result);
    res = r;
    goto err;
  }

  fields = NULL;
  if (envelope != NULL) {
    r = imap_env_to_fields(envelope, references, ref_size, &fields);
    if (r != MAIL_NO_ERROR) {
      mailmime_free(body);
      mailimap_fetch_list_free(fetch_result);
      res = r;
      goto err;
    }
  }

  content_message = mailmime_get_content_message();
  if (content_message == NULL) {
    if (fields != NULL)
      mailimf_fields_free(fields);
    mailmime_free(body);
    mailimap_fetch_list_free(fetch_result);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  new_body = mailmime_new(MAILMIME_MESSAGE, NULL,
      0, NULL, content_message,
      NULL, NULL, NULL, NULL, fields, body);

  if (new_body == NULL) {
    mailmime_content_free(content_message);
    if (fields != NULL)
      mailimf_fields_free(fields);
    mailmime_free(body);
    mailimap_fetch_list_free(fetch_result);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  msg_info->msg_mime = new_body;
  
  mailimap_fetch_list_free(fetch_result);

  * result = new_body;

  return MAIL_NO_ERROR;

 free_fetch_type:
  mailimap_fetch_type_free(fetch_type);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}

static int
fetch_imap(mailmessage * msg,
	   struct mailimap_fetch_type * fetch_type,
	   char ** result, size_t * result_len)
{
  int r;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * msg_att_item;
  clist * fetch_result;
  struct mailimap_set * set;
  char * text;
  size_t text_length;
  clistiter * cur;

  set = mailimap_set_new_single(msg->msg_index);
  if (set == NULL)
    return MAIL_ERROR_MEMORY;

  r = mailimap_uid_fetch(get_imap_session(msg), set,
			 fetch_type, &fetch_result);

  mailimap_set_free(set);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  if (clist_begin(fetch_result) == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAIL_ERROR_FETCH;
  }

  msg_att = clist_begin(fetch_result)->data;

  text = NULL;
  text_length = 0;

  for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    msg_att_item = clist_content(cur);

    if (msg_att_item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {

      if (msg_att_item->att_data.att_static->att_type ==
	  MAILIMAP_MSG_ATT_BODY_SECTION) {
	text = msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part;
	msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part = NULL;
	text_length =
	  msg_att_item->att_data.att_static->att_data.att_body_section->sec_length;
      }
    }
  }

  mailimap_fetch_list_free(fetch_result);

  if (text == NULL)
    return MAIL_ERROR_FETCH;

  * result = text;
  * result_len = text_length;

  return MAIL_NO_ERROR;
}

  
static int imap_fetch_section(mailmessage * msg_info,
			      struct mailmime * mime,
			      char ** result, size_t * result_len)
{
  struct mailimap_section * section;
  struct mailimap_fetch_att * fetch_att;
  int r;
  struct mailimap_fetch_type * fetch_type;
  char * text;
  size_t text_length;
  struct mailmime_section * part;

  if (mime->mm_parent == NULL)
    return imap_fetch(msg_info, result, result_len);
  
  r = mailmime_get_section_id(mime, &part);
  if (r != MAILIMF_NO_ERROR)
    return maildriver_imf_error_to_mail_error(r);

  r = section_to_imap_section(part, IMAP_SECTION_MESSAGE, &section);
  mailmime_section_free(part);
  if (r != MAIL_NO_ERROR)
    return r;

  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    return MAIL_ERROR_MEMORY;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    mailimap_fetch_att_free(fetch_att);
    return MAIL_ERROR_MEMORY;
  }

  r = fetch_imap(msg_info, fetch_type, &text, &text_length);

  mailimap_fetch_type_free(fetch_type);

  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = text;
  * result_len = text_length;

  return MAIL_NO_ERROR;
}
  
static int imap_fetch_section_header(mailmessage * msg_info,
				     struct mailmime * mime,
				     char ** result,
				     size_t * result_len)
{
  struct mailimap_section * section;
  struct mailimap_fetch_att * fetch_att;
  int r;
  struct mailimap_fetch_type * fetch_type;
  char * text;
  size_t text_length;
  struct mailmime_section * part;

  if (mime->mm_parent == NULL)
    return imap_fetch_header(msg_info, result, result_len);

  r = mailmime_get_section_id(mime, &part);
  if (r != MAILIMF_NO_ERROR)
    return maildriver_imf_error_to_mail_error(r);

  r = section_to_imap_section(part, IMAP_SECTION_HEADER, &section);
  mailmime_section_free(part);
  if (r != MAIL_NO_ERROR)
    return r;

  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    return MAIL_ERROR_MEMORY;
  }
  
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    mailimap_fetch_att_free(fetch_att);
    return MAIL_ERROR_MEMORY;
  }

  r = fetch_imap(msg_info, fetch_type, &text, &text_length);
  mailimap_fetch_type_free(fetch_type);

  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = text;
  * result_len = text_length;

  return MAIL_NO_ERROR;
}
  
static int imap_fetch_section_mime(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len)
{
  struct mailimap_section * section;
  struct mailimap_fetch_att * fetch_att;
  int r;
  struct mailimap_fetch_type * fetch_type;
  char * text;
  size_t text_length;
  struct mailmime_section * part;

  if (mime->mm_parent == NULL)
    return MAIL_ERROR_INVAL;

  if (mime->mm_parent->mm_parent == NULL)
    return imap_fetch_header(msg_info, result, result_len);

  r = mailmime_get_section_id(mime, &part);
  if (r != MAILIMF_NO_ERROR)
    return maildriver_imf_error_to_mail_error(r);

  r = section_to_imap_section(part, IMAP_SECTION_MIME, &section);
  mailmime_section_free(part);
  if (r != MAIL_NO_ERROR)
    return MAIL_ERROR_MEMORY;

  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    return MAIL_ERROR_MEMORY;
  }
  
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    mailimap_fetch_att_free(fetch_att);
    return MAIL_ERROR_MEMORY;
  }

  r = fetch_imap(msg_info, fetch_type, &text, &text_length);

  mailimap_fetch_type_free(fetch_type);

  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = text;
  * result_len = text_length;

  return MAIL_NO_ERROR;
}
  
static int imap_fetch_section_body(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len)
{
  struct mailimap_section * section;
  struct mailimap_fetch_att * fetch_att;
  int r;
  struct mailimap_fetch_type * fetch_type;
  char * text;
  size_t text_length;
  struct mailmime_section * part;

  if (mime->mm_parent == NULL)
    return imap_fetch_body(msg_info, result, result_len);

  if (mime->mm_parent->mm_parent == NULL)
    return imap_fetch_body(msg_info, result, result_len);

  r = mailmime_get_section_id(mime, &part);
  if (r != MAILIMF_NO_ERROR)
    return maildriver_imf_error_to_mail_error(r);

  r = section_to_imap_section(part, IMAP_SECTION_BODY, &section);
  mailmime_section_free(part);
  if (r != MAIL_NO_ERROR)
    return MAIL_ERROR_MEMORY;

  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    return MAIL_ERROR_MEMORY;
  }
  
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    mailimap_fetch_att_free(fetch_att);
    return MAIL_ERROR_MEMORY;
  }

  r = fetch_imap(msg_info, fetch_type, &text, &text_length);

  mailimap_fetch_type_free(fetch_type);

  if (r != MAIL_NO_ERROR)
    return r;
  
  * result = text;
  * result_len = text_length;

  return MAIL_NO_ERROR;
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

  flags = mail_flags_store_get(get_session_data(msg_info)->imap_flags_store,
			       msg_info->msg_index);

  if (flags == NULL) {
    r = imap_fetch_flags(get_imap_session(msg_info),
			 msg_info->msg_index, &flags);
    if (r != MAIL_NO_ERROR)
      return r;
  }

  msg_info->msg_flags = flags;

  * result = flags;

  return MAIL_NO_ERROR;
}

static int imap_fetch_envelope(mailmessage * msg_info,
			       struct mailimf_fields ** result)
{
  int r;
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  clist * fetch_result;
  struct mailimap_msg_att * msg_att;
  int res;
  struct mailimf_fields * fields;
  struct mailimap_envelope * envelope;
  uint32_t uid;
  char * references;
  size_t ref_size;

  set = mailimap_set_new_single(msg_info->msg_index);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }

  fetch_att = mailimap_fetch_att_new_uid();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = imap_add_envelope_fetch_att(fetch_type);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_fetch_type;
  }

  r = mailimap_uid_fetch(get_imap_session(msg_info), set,
			 fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }

  if (clist_begin(fetch_result) == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAIL_ERROR_FETCH;
  }

  msg_att = clist_begin(fetch_result)->data;

  uid = 0;
  references = NULL;
  ref_size = 0;
  envelope = NULL;

  r = imap_get_msg_att_info(msg_att,
			    &uid,
			    &envelope,
			    &references,
			    &ref_size,
			    NULL,
			    NULL);
  if (r != MAIL_NO_ERROR) {
    mailimap_fetch_list_free(fetch_result);
    res = r;
    goto err;
  }

  if (uid != msg_info->msg_index) {
    mailimap_fetch_list_free(fetch_result);
    res = MAIL_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  fields = NULL;
  if (envelope != NULL) {
    r = imap_env_to_fields(envelope, references, ref_size, &fields);
    if (r != MAIL_NO_ERROR) {
      mailimap_fetch_list_free(fetch_result);
      res = r;
      goto err;
    }
  }

  mailimap_fetch_list_free(fetch_result);

  * result = fields;

  return MAIL_NO_ERROR;

 free_fetch_type:
  mailimap_fetch_type_free(fetch_type);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}
