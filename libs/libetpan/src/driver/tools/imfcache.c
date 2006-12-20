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
 * $Id: imfcache.c,v 1.18 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imfcache.h"

#include <stdlib.h>
#include <string.h>

static int mailimf_cache_field_write(MMAPString * mmapstr, size_t * index,
				     struct mailimf_field * field);
static int mailimf_cache_orig_date_write(MMAPString * mmapstr, size_t * index,
					 struct mailimf_orig_date * date);
static int mailimf_cache_date_time_write(MMAPString * mmapstr, size_t * index,
					 struct mailimf_date_time * date_time);
static int mailimf_cache_from_write(MMAPString * mmapstr, size_t * index,
				    struct mailimf_from * from);
static int mailimf_cache_sender_write(MMAPString * mmapstr, size_t * index,
				      struct mailimf_sender * sender);
static int mailimf_cache_reply_to_write(MMAPString * mmapstr, size_t * index,
					struct mailimf_reply_to * reply_to);
static int mailimf_cache_to_write(MMAPString * mmapstr, size_t * index,
				  struct mailimf_to * to);
static int mailimf_cache_cc_write(MMAPString * mmapstr, size_t * index,
				  struct mailimf_cc * to);
static int mailimf_cache_bcc_write(MMAPString * mmapstr, size_t * index,
				   struct mailimf_bcc * to);
static int mailimf_cache_message_id_write(MMAPString * mmapstr, size_t * index,
					  struct mailimf_message_id * message_id);
static int mailimf_cache_msg_id_list_write(MMAPString * mmapstr, size_t * index,
					   clist * list);
static int mailimf_cache_in_reply_to_write(MMAPString * mmapstr, size_t * index,
					   struct mailimf_in_reply_to *
					   in_reply_to);
static int mailimf_cache_references_write(MMAPString * mmapstr, size_t * index,
					  struct mailimf_references * references);
static int mailimf_cache_subject_write(MMAPString * mmapstr, size_t * index,
				       struct mailimf_subject * subject);
static int mailimf_cache_address_list_write(MMAPString * mmapstr,
					    size_t * index,
					    struct mailimf_address_list *
					    addr_list);
static int mailimf_cache_address_write(MMAPString * mmapstr, size_t * index,
				       struct mailimf_address * addr);
static int mailimf_cache_group_write(MMAPString * mmapstr, size_t * index,
				     struct mailimf_group * group);
static int mailimf_cache_mailbox_list_write(MMAPString * mmapstr,
					    size_t * index,
					    struct mailimf_mailbox_list * mb_list);
static int mailimf_cache_mailbox_write(MMAPString * mmapstr, size_t * index,
				       struct mailimf_mailbox * mb);


static int mailimf_cache_field_read(MMAPString * mmapstr, size_t * index,
				    struct mailimf_field ** result);
static int mailimf_cache_orig_date_read(MMAPString * mmapstr, size_t * index,
					struct mailimf_orig_date ** result);
static int mailimf_cache_date_time_read(MMAPString * mmapstr, size_t * index,
					struct mailimf_date_time ** result);
static int mailimf_cache_from_read(MMAPString * mmapstr, size_t * index,
				   struct mailimf_from ** result);
static int mailimf_cache_sender_read(MMAPString * mmapstr, size_t * index,
				     struct mailimf_sender ** result);
static int mailimf_cache_reply_to_read(MMAPString * mmapstr, size_t * index,
				       struct mailimf_reply_to ** result);
static int mailimf_cache_to_read(MMAPString * mmapstr, size_t * index,
				 struct mailimf_to ** result);
static int mailimf_cache_cc_read(MMAPString * mmapstr, size_t * index,
				 struct mailimf_cc ** result);
static int mailimf_cache_bcc_read(MMAPString * mmapstr, size_t * index,
				  struct mailimf_bcc ** result);
static int mailimf_cache_message_id_read(MMAPString * mmapstr, size_t * index,
					 struct mailimf_message_id ** result);
static int mailimf_cache_msg_id_list_read(MMAPString * mmapstr, size_t * index,
					  clist ** result);
static int
mailimf_cache_in_reply_to_read(MMAPString * mmapstr, size_t * index,
			       struct mailimf_in_reply_to ** result);

static int mailimf_cache_references_read(MMAPString * mmapstr, size_t * index,
					 struct mailimf_references ** result);
static int mailimf_cache_subject_read(MMAPString * mmapstr, size_t * index,
				      struct mailimf_subject ** result);
static int mailimf_cache_address_list_read(MMAPString * mmapstr, size_t * index,
					   struct mailimf_address_list ** result);
static int mailimf_cache_address_read(MMAPString * mmapstr, size_t * index,
				      struct mailimf_address ** result);
static int mailimf_cache_group_read(MMAPString * mmapstr, size_t * index,
				    struct mailimf_group ** result);
static int
mailimf_cache_mailbox_list_read(MMAPString * mmapstr, size_t * index,
				struct mailimf_mailbox_list ** result);
static int mailimf_cache_mailbox_read(MMAPString * mmapstr, size_t * index,
				      struct mailimf_mailbox ** result);

enum {
  CACHE_NULL_POINTER = 0,
  CACHE_NOT_NULL     = 1
};

int mail_serialize_clear(MMAPString * mmapstr, size_t * index)
{
  if (mmap_string_set_size(mmapstr, 0) == NULL)
    return MAIL_ERROR_MEMORY;

  * index = 0;

  return MAIL_NO_ERROR;
}

int mail_serialize_write(MMAPString * mmapstr, size_t * index,
			 char * buf, size_t size)
{
  if (mmap_string_append_len(mmapstr, buf, size) == NULL)
    return MAIL_ERROR_MEMORY;

  * index = * index + size;

  return MAIL_NO_ERROR;
}

int mail_serialize_read(MMAPString * mmapstr, size_t * index,
			char * buf, size_t size)
{
  size_t cur_token;
  
  cur_token = * index;

  if (cur_token + size > mmapstr->len)
    return MAIL_ERROR_STREAM;

  memcpy(buf, mmapstr->str + cur_token, size);
  * index = cur_token + size;

  return MAIL_NO_ERROR;
}

int mailimf_cache_int_write(MMAPString * mmapstr, size_t * index,
			    uint32_t value)
{
  unsigned char ch;
  int r;
  int i;

  for(i = 0 ; i < 4 ; i ++) {
    ch = value % 256;

    r = mail_serialize_write(mmapstr, index, (char *) &ch, 1);
    if (r != MAIL_NO_ERROR)
      return r;
    value /= 256;
  }

  return MAIL_NO_ERROR;
}

int mailimf_cache_int_read(MMAPString * mmapstr, size_t * index,
			   uint32_t * result)
{
  unsigned char ch;
  uint32_t value;
  int i;
  int r;
  
  value = 0;
  for(i = 0 ; i < 4 ; i ++) {
    r = mail_serialize_read(mmapstr, index, (char *) &ch, 1);
    if (r != MAIL_NO_ERROR)
      return r;
    value = value | ch << (i << 3);
  }
  
  * result = value;

  return MAIL_NO_ERROR;
}


int mailimf_cache_string_write(MMAPString * mmapstr, size_t * index,
			       char * str, size_t length)
{
  int r;

  if (str == NULL) {
    r = mailimf_cache_int_write(mmapstr, index, CACHE_NULL_POINTER);
    if (r != MAIL_NO_ERROR)
      return r;
  }
  else {
    r = mailimf_cache_int_write(mmapstr, index, CACHE_NOT_NULL);
    if (r != MAIL_NO_ERROR)
      return r;
    
    r = mailimf_cache_int_write(mmapstr, index, length);
    if (r != MAIL_NO_ERROR)
      return r;
    
    if (length != 0) {
      r = mail_serialize_write(mmapstr, index, str, length);
      if (r != MAIL_NO_ERROR)
        return MAIL_ERROR_FILE;
    }
  }

  return MAIL_NO_ERROR;
}

int mailimf_cache_string_read(MMAPString * mmapstr, size_t * index,
			      char ** result)
{
  int r;
  uint32_t length;
  char * str;
  uint32_t type;
  
  r = mailimf_cache_int_read(mmapstr, index, &type);
  if (r != MAIL_NO_ERROR)
    return r;

  if (type == CACHE_NULL_POINTER) {
    str = NULL;
  }
  else {
    r = mailimf_cache_int_read(mmapstr, index, &length);
    if (r != MAIL_NO_ERROR)
      return r;
    
    str = malloc(length + 1);
    if (str == NULL)
      return MAIL_ERROR_MEMORY;
    
    r = mail_serialize_read(mmapstr, index, str, length);
    if (r != MAIL_NO_ERROR)
      return MAIL_ERROR_FILE;
    
    str[length] = 0;
  }

  * result = str;

  return MAIL_NO_ERROR;
}

int mailimf_cache_fields_write(MMAPString * mmapstr, size_t * index,
			       struct mailimf_fields * fields)
{
  clistiter * cur;
  int r;

  r = mailimf_cache_int_write(mmapstr, index,
      clist_count(fields->fld_list));
  if (r != MAIL_NO_ERROR)
    return r;
  
  for(cur = clist_begin(fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    r = mailimf_cache_field_write(mmapstr, index, clist_content(cur));
    if (r != MAIL_NO_ERROR)
      return r;
  }

  return MAIL_NO_ERROR;
}

int mailimf_cache_fields_read(MMAPString * mmapstr, size_t * index,
			      struct mailimf_fields ** result)
{
  clist * list;
  int r;
  uint32_t count;
  uint32_t i;
  struct mailimf_fields * fields;
  int res;

  r = mailimf_cache_int_read(mmapstr, index, &count);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < count ; i++) {
    struct mailimf_field * field;

    field = NULL;
    r = mailimf_cache_field_read(mmapstr, index, &field);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = clist_append(list, field);
    if (r < 0) {
      mailimf_field_free(field);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  fields = mailimf_fields_new(list);
  if (fields == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }
  
  * result = fields;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_field_free, NULL);
  clist_free(list);
 err:
  return res;
}


static int mailimf_cache_field_write(MMAPString * mmapstr, size_t * index,
				     struct mailimf_field * field)
{
  int r;
  
  r = mailimf_cache_int_write(mmapstr, index, field->fld_type);
  if (r != MAIL_NO_ERROR)
    return r;
  
  switch (field->fld_type) {
  case MAILIMF_FIELD_ORIG_DATE:
    r = mailimf_cache_orig_date_write(mmapstr, index,
        field->fld_data.fld_orig_date);
    break;
  case MAILIMF_FIELD_FROM:
    r = mailimf_cache_from_write(mmapstr, index,
        field->fld_data.fld_from);
    break;
  case MAILIMF_FIELD_SENDER:
    r = mailimf_cache_sender_write(mmapstr, index,
        field->fld_data.fld_sender);
    break;
  case MAILIMF_FIELD_REPLY_TO:
    r = mailimf_cache_reply_to_write(mmapstr, index,
        field->fld_data.fld_reply_to);
    break;
  case MAILIMF_FIELD_TO:
    r = mailimf_cache_to_write(mmapstr, index,
        field->fld_data.fld_to);
    break;
  case MAILIMF_FIELD_CC:
    r = mailimf_cache_cc_write(mmapstr, index,
        field->fld_data.fld_cc);
    break;
  case MAILIMF_FIELD_BCC:
    r = mailimf_cache_bcc_write(mmapstr, index,
        field->fld_data.fld_bcc);
    break;
  case MAILIMF_FIELD_MESSAGE_ID:
    r = mailimf_cache_message_id_write(mmapstr, index,
        field->fld_data.fld_message_id);
    break;
  case MAILIMF_FIELD_IN_REPLY_TO:
    r = mailimf_cache_in_reply_to_write(mmapstr, index,
        field->fld_data.fld_in_reply_to);
    break;
  case MAILIMF_FIELD_REFERENCES:
    r = mailimf_cache_references_write(mmapstr, index,
        field->fld_data.fld_references);
    break;
  case MAILIMF_FIELD_SUBJECT:
    r = mailimf_cache_subject_write(mmapstr, index,
        field->fld_data.fld_subject);
    break;
  default:
    r = 0;
    break;
  }

  if (r != MAIL_NO_ERROR)
    return r;

  return MAIL_NO_ERROR;
}


static int mailimf_cache_field_read(MMAPString * mmapstr, size_t * index,
				    struct mailimf_field ** result)
{
  int r;
  uint32_t type;
  struct mailimf_orig_date * orig_date;
  struct mailimf_from * from;
  struct mailimf_sender * sender;
  struct mailimf_to * to;
  struct mailimf_reply_to * reply_to;
  struct mailimf_cc * cc;
  struct mailimf_bcc * bcc;
  struct mailimf_message_id * message_id;
  struct mailimf_in_reply_to * in_reply_to;
  struct mailimf_references * references;
  struct mailimf_subject * subject;
  struct mailimf_field * field;
  int res;

  orig_date = NULL;
  from = NULL;
  sender = NULL;
  to = NULL;
  reply_to = NULL;
  cc = NULL;
  bcc = NULL;
  message_id = NULL;
  in_reply_to = NULL;
  references = NULL;
  subject = NULL;
  field = NULL;

  r = mailimf_cache_int_read(mmapstr, index, &type);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  switch (type) {
  case MAILIMF_FIELD_ORIG_DATE:
    r = mailimf_cache_orig_date_read(mmapstr, index, &orig_date);
    break;
  case MAILIMF_FIELD_FROM:
    r = mailimf_cache_from_read(mmapstr, index, &from);
    break;
  case MAILIMF_FIELD_SENDER:
    r = mailimf_cache_sender_read(mmapstr, index, &sender);
    break;
  case MAILIMF_FIELD_REPLY_TO:
    r = mailimf_cache_reply_to_read(mmapstr, index, &reply_to);
    break;
  case MAILIMF_FIELD_TO:
    r = mailimf_cache_to_read(mmapstr, index, &to);
    break;
  case MAILIMF_FIELD_CC:
    r = mailimf_cache_cc_read(mmapstr, index, &cc);
    break;
  case MAILIMF_FIELD_BCC:
    r = mailimf_cache_bcc_read(mmapstr, index, &bcc);
    break;
  case MAILIMF_FIELD_MESSAGE_ID:
    r = mailimf_cache_message_id_read(mmapstr, index, &message_id);
    break;
  case MAILIMF_FIELD_IN_REPLY_TO:
    r = mailimf_cache_in_reply_to_read(mmapstr, index, &in_reply_to);
    break;
  case MAILIMF_FIELD_REFERENCES:
    r = mailimf_cache_references_read(mmapstr, index, &references);
    break;
  case MAILIMF_FIELD_SUBJECT:
    r = mailimf_cache_subject_read(mmapstr, index, &subject);
    break;
  default:
    r = MAIL_ERROR_INVAL;
    break;
  }

  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  field = mailimf_field_new(type, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, orig_date, from, sender, reply_to,
      to, cc, bcc, message_id,
      in_reply_to, references,
      subject, NULL, NULL, NULL);
  if (field == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free;
  }

  * result = field;

  return MAIL_NO_ERROR;

 free:
  if (orig_date != NULL)
    mailimf_orig_date_free(orig_date);
  if (from != NULL)
    mailimf_from_free(from);
  if (sender != NULL)
    mailimf_sender_free(sender);
  if (reply_to != NULL)
    mailimf_reply_to_free(reply_to);
  if (to != NULL)
    mailimf_to_free(to);
  if (cc != NULL)
    mailimf_cc_free(cc);
  if (bcc != NULL)
    mailimf_bcc_free(bcc);
  if (message_id != NULL)
    mailimf_message_id_free(message_id);
  if (in_reply_to != NULL)
    mailimf_in_reply_to_free(in_reply_to);
  if (references != NULL)
    mailimf_references_free(references);
  if (subject != NULL)
    mailimf_subject_free(subject);
 err:
  return res;
}

static int mailimf_cache_orig_date_write(MMAPString * mmapstr, size_t * index,
					 struct mailimf_orig_date * date)
{
  return mailimf_cache_date_time_write(mmapstr, index, date->dt_date_time);
}

static int mailimf_cache_orig_date_read(MMAPString * mmapstr, size_t * index,
					struct mailimf_orig_date ** result)
{
  int r;
  struct mailimf_date_time * date_time;
  struct mailimf_orig_date * orig_date;

  r = mailimf_cache_date_time_read(mmapstr, index, &date_time);
  if (r != MAIL_NO_ERROR)
    return r;

  orig_date = mailimf_orig_date_new(date_time);
  if (orig_date == NULL) {
    mailimf_date_time_free(date_time);
    return MAIL_ERROR_MEMORY;
  }

  * result = orig_date;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_date_time_write(MMAPString * mmapstr, size_t * index,
					 struct mailimf_date_time * date_time)
{
  int r;

  r = mailimf_cache_int_write(mmapstr, index, date_time->dt_day);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_write(mmapstr, index, date_time->dt_month);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_write(mmapstr, index, date_time->dt_year);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_write(mmapstr, index, date_time->dt_hour);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_write(mmapstr, index, date_time->dt_min);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_write(mmapstr, index, date_time->dt_sec);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_write(mmapstr, index, date_time->dt_zone);
  if (r != MAIL_NO_ERROR)
    return r;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_date_time_read(MMAPString * mmapstr, size_t * index,
					struct mailimf_date_time ** result)
{
  int r;
  uint32_t day;
  uint32_t month;
  uint32_t year;
  uint32_t hour;
  uint32_t min;
  uint32_t sec;
  uint32_t zone;
  struct mailimf_date_time * date_time;

  r = mailimf_cache_int_read(mmapstr, index, &day);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_read(mmapstr, index, &month);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_read(mmapstr, index, &year);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_read(mmapstr, index, &hour);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_read(mmapstr, index, &min);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_read(mmapstr, index, &sec);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_int_read(mmapstr, index, &zone);
  if (r != MAIL_NO_ERROR)
    return r;

  date_time = mailimf_date_time_new(day, month, year, hour, min, sec, zone);
  if (date_time == NULL)
    return MAIL_ERROR_MEMORY;

  * result = date_time;

  return MAIL_NO_ERROR;
  
}


static int mailimf_cache_from_write(MMAPString * mmapstr, size_t * index,
				    struct mailimf_from * from)
{
  return mailimf_cache_mailbox_list_write(mmapstr, index, from->frm_mb_list);
}

static int mailimf_cache_from_read(MMAPString * mmapstr, size_t * index,
				   struct mailimf_from ** result)
{
  struct mailimf_mailbox_list * mb_list;
  struct mailimf_from * from;
  int r;
  
  r = mailimf_cache_mailbox_list_read(mmapstr, index, &mb_list);
  if (r != MAIL_NO_ERROR)
    return r;

  from = mailimf_from_new(mb_list);
  if (from == NULL) {
    mailimf_mailbox_list_free(mb_list);
    return MAIL_ERROR_MEMORY;
  }

  * result = from;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_sender_write(MMAPString * mmapstr, size_t * index,
				      struct mailimf_sender * sender)
{
  return mailimf_cache_mailbox_write(mmapstr, index, sender->snd_mb);
}

static int mailimf_cache_sender_read(MMAPString * mmapstr, size_t * index,
				     struct mailimf_sender ** result)
{
  int r;
  struct mailimf_mailbox * mb;
  struct mailimf_sender * sender;

  r = mailimf_cache_mailbox_read(mmapstr, index, &mb);
  if (r != MAIL_NO_ERROR)
    return r;

  sender = mailimf_sender_new(mb);
  if (sender == NULL) {
    mailimf_mailbox_free(mb);
    return MAIL_ERROR_MEMORY;
  }

  * result = sender;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_reply_to_write(MMAPString * mmapstr, size_t * index,
					struct mailimf_reply_to * reply_to)
{
  return mailimf_cache_address_list_write(mmapstr, index,
      reply_to->rt_addr_list);
}

static int mailimf_cache_reply_to_read(MMAPString * mmapstr, size_t * index,
				       struct mailimf_reply_to ** result)
{
  int r;
  struct mailimf_address_list * addr_list;
  struct mailimf_reply_to * reply_to;

  r = mailimf_cache_address_list_read(mmapstr, index, &addr_list);
  if (r != MAIL_NO_ERROR)
    return r;

  reply_to = mailimf_reply_to_new(addr_list);
  if (reply_to == NULL) {
    mailimf_address_list_free(addr_list);
    return MAIL_ERROR_MEMORY;
  }

  * result = reply_to;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_to_write(MMAPString * mmapstr, size_t * index,
				  struct mailimf_to * to)
{
  return mailimf_cache_address_list_write(mmapstr, index, to->to_addr_list);
}

static int mailimf_cache_to_read(MMAPString * mmapstr, size_t * index,
				 struct mailimf_to ** result)
{
  int r;
  struct mailimf_address_list * addr_list;
  struct mailimf_to * to;

  r = mailimf_cache_address_list_read(mmapstr, index, &addr_list);
  if (r != MAIL_NO_ERROR)
    return r;

  to = mailimf_to_new(addr_list);
  if (to == NULL) {
    mailimf_address_list_free(addr_list);
    return MAIL_ERROR_MEMORY;
  }

  * result = to;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_cc_write(MMAPString * mmapstr, size_t * index,
				  struct mailimf_cc * cc)
{
  return mailimf_cache_address_list_write(mmapstr, index, cc->cc_addr_list);
}

static int mailimf_cache_cc_read(MMAPString * mmapstr, size_t * index,
    struct mailimf_cc ** result)
{
  int r;
  struct mailimf_address_list * addr_list;
  struct mailimf_cc * cc;

  r = mailimf_cache_address_list_read(mmapstr, index, &addr_list);
  if (r != MAIL_NO_ERROR)
    return r;

  cc = mailimf_cc_new(addr_list);
  if (cc == NULL) {
    mailimf_address_list_free(addr_list);
    return MAIL_ERROR_MEMORY;
  }

  * result = cc;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_bcc_write(MMAPString * mmapstr, size_t * index,
				   struct mailimf_bcc * bcc)
{
  return mailimf_cache_address_list_write(mmapstr, index, bcc->bcc_addr_list);
}

static int mailimf_cache_bcc_read(MMAPString * mmapstr, size_t * index,
				  struct mailimf_bcc ** result)
{
  int r;
  struct mailimf_address_list * addr_list;
  struct mailimf_bcc * bcc;

  r = mailimf_cache_address_list_read(mmapstr, index, &addr_list);
  if (r != MAIL_NO_ERROR)
    return r;

  bcc = mailimf_bcc_new(addr_list);
  if (bcc == NULL) {
    mailimf_address_list_free(addr_list);
    return MAIL_ERROR_MEMORY;
  }

  * result = bcc;

  return MAIL_NO_ERROR;
}

static int
mailimf_cache_message_id_write(MMAPString * mmapstr, size_t * index,
			       struct mailimf_message_id * message_id)
{
  return mailimf_cache_string_write(mmapstr, index,
      message_id->mid_value, strlen(message_id->mid_value));
}

static int mailimf_cache_message_id_read(MMAPString * mmapstr, size_t * index,
					 struct mailimf_message_id ** result)
{
  struct mailimf_message_id * message_id;
  char * str;
  int r;

  r = mailimf_cache_string_read(mmapstr, index, &str);
  if (r != MAIL_NO_ERROR)
    return r;

  message_id = mailimf_message_id_new(str);
  if (message_id == NULL) {
    free(str);
    return MAIL_ERROR_MEMORY;
  }

  * result = message_id;
  
  return MAIL_NO_ERROR;
}

static int
mailimf_cache_msg_id_list_write(MMAPString * mmapstr, size_t * index,
				clist * list)
{
  clistiter * cur;
  int r;

  r = mailimf_cache_int_write(mmapstr, index, clist_count(list));
  if (r != MAIL_NO_ERROR)
    return r;
  
  for(cur = clist_begin(list) ; cur != NULL ; cur = clist_next(cur)) {
    char * msgid;

    msgid = clist_content(cur);

    r = mailimf_cache_string_write(mmapstr, index, msgid, strlen(msgid));
    if (r != MAIL_NO_ERROR)
      return r;
  }

  return MAIL_NO_ERROR;
}

static int mailimf_cache_msg_id_list_read(MMAPString * mmapstr, size_t * index,
					  clist ** result)
{
  clist * list;
  int r;
  uint32_t count;
  uint32_t i;
  int res;

  r = mailimf_cache_int_read(mmapstr, index, &count);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < count ; i++) {
    char * msgid;
    
    r = mailimf_cache_string_read(mmapstr, index, &msgid);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }

    r = clist_append(list, msgid);
    if (r < 0) {
      free(msgid);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  * result = list;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) free, NULL);
  clist_free(list);
 err:
  return res;
}

static int
mailimf_cache_in_reply_to_write(MMAPString * mmapstr, size_t * index,
				struct mailimf_in_reply_to * in_reply_to)
{
  return mailimf_cache_msg_id_list_write(mmapstr, index,
      in_reply_to->mid_list);
}

static int mailimf_cache_in_reply_to_read(MMAPString * mmapstr, size_t * index,
					  struct mailimf_in_reply_to ** result)
{
  int r;
  clist * msg_id_list;
  struct mailimf_in_reply_to * in_reply_to;

  r = mailimf_cache_msg_id_list_read(mmapstr, index, &msg_id_list);
  if (r != MAIL_NO_ERROR)
    return r;

  in_reply_to = mailimf_in_reply_to_new(msg_id_list);
  if (in_reply_to == NULL) {
    clist_foreach(msg_id_list, (clist_func) free, NULL);
    clist_free(msg_id_list);
    return MAIL_ERROR_MEMORY;
  }

  * result = in_reply_to;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_references_write(MMAPString * mmapstr, size_t * index,
					  struct mailimf_references * references)
{
  return mailimf_cache_msg_id_list_write(mmapstr, index,
      references->mid_list);
}

static int mailimf_cache_references_read(MMAPString * mmapstr, size_t * index,
					 struct mailimf_references ** result)
{
  int r;
  clist * msg_id_list;
  struct mailimf_references * references;

  r = mailimf_cache_msg_id_list_read(mmapstr, index, &msg_id_list);
  if (r != MAIL_NO_ERROR)
    return r;

  references = mailimf_references_new(msg_id_list);
  if (references == NULL) {
    clist_foreach(msg_id_list, (clist_func) free, NULL);
    clist_free(msg_id_list);
    return MAIL_ERROR_MEMORY;
  }

  * result = references;

  return MAIL_NO_ERROR;
}


static int mailimf_cache_subject_write(MMAPString * mmapstr, size_t * index,
				       struct mailimf_subject * subject)
{
  return mailimf_cache_string_write(mmapstr, index,
      subject->sbj_value, strlen(subject->sbj_value));
}

static int mailimf_cache_subject_read(MMAPString * mmapstr, size_t * index,
				      struct mailimf_subject ** result)
{
  char * str;
  struct mailimf_subject * subject;
  int r;

  r = mailimf_cache_string_read(mmapstr, index, &str);
  if (r != MAIL_NO_ERROR)
    return r;

  if (str == NULL) {
    str = strdup("");
    if (str == NULL)
      return MAIL_ERROR_MEMORY;
  }

  subject = mailimf_subject_new(str);
  if (subject == NULL) {
    free(str);
    return MAIL_ERROR_MEMORY;
  }

  * result = subject;

  return MAIL_NO_ERROR;
}


static int
mailimf_cache_address_list_write(MMAPString * mmapstr, size_t * index,
				 struct mailimf_address_list * addr_list)
{
  clistiter * cur;
  int r;

  if (addr_list == NULL) {
    r = mailimf_cache_int_write(mmapstr, index, CACHE_NULL_POINTER);
    if (r != MAIL_NO_ERROR)
      return r;
  }
  else {
    r = mailimf_cache_int_write(mmapstr, index, CACHE_NOT_NULL);
    if (r != MAIL_NO_ERROR)
      return r;
    
    r = mailimf_cache_int_write(mmapstr, index,
        clist_count(addr_list->ad_list));
    if (r != MAIL_NO_ERROR)
      return r;
    
    for(cur = clist_begin(addr_list->ad_list) ; cur != NULL ;
        cur = clist_next(cur)) {
      struct mailimf_address * addr;
      
      addr = clist_content(cur);
      
      r = mailimf_cache_address_write(mmapstr, index, addr);
      if (r != MAIL_NO_ERROR)
	return r;
    }
  }

  return MAIL_NO_ERROR;
}

static int
mailimf_cache_address_list_read(MMAPString * mmapstr, size_t * index,
				struct mailimf_address_list ** result)
{
  struct mailimf_address_list * addr_list;
  uint32_t count;
  uint32_t i;
  int r;
  clist * list;
  int res;
  uint32_t type;
  
  r = mailimf_cache_int_read(mmapstr, index, &type);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  if (type == CACHE_NULL_POINTER) {
    * result = NULL;
    return MAIL_NO_ERROR;
  }
  
  r = mailimf_cache_int_read(mmapstr, index, &count);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < count ; i++) {
    struct mailimf_address * addr;
    
    r = mailimf_cache_address_read(mmapstr, index, &addr);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = clist_append(list, addr);
    if (r < 0) {
      mailimf_address_free(addr);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  addr_list = mailimf_address_list_new(list);
  if (addr_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = addr_list;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_address_free, NULL);
  clist_free(list);
 err:
  return res;
}

static int mailimf_cache_address_write(MMAPString * mmapstr, size_t * index,
				       struct mailimf_address * addr)
{
  int r;

  r = mailimf_cache_int_write(mmapstr, index, addr->ad_type);
  if (r != MAIL_NO_ERROR)
    return r;
  
  switch(addr->ad_type) {
  case MAILIMF_ADDRESS_MAILBOX:
    r = mailimf_cache_mailbox_write(mmapstr, index, addr->ad_data.ad_mailbox);
    if (r != MAIL_NO_ERROR)
      return r;

    break;

  case MAILIMF_ADDRESS_GROUP:
    r = mailimf_cache_group_write(mmapstr, index, addr->ad_data.ad_group);
    if (r != MAIL_NO_ERROR)
      return r;
    
    break;
  }

  return MAIL_NO_ERROR;
}

static int mailimf_cache_address_read(MMAPString * mmapstr, size_t * index,
				      struct mailimf_address ** result)
{
  uint32_t type;
  int r;
  struct mailimf_mailbox * mailbox;
  struct mailimf_group * group;
  struct mailimf_address * addr;

  r = mailimf_cache_int_read(mmapstr, index, &type);
  if (r != MAIL_NO_ERROR)
    return r;
  
  mailbox = NULL;
  group = NULL;

  switch (type) {
  case MAILIMF_ADDRESS_MAILBOX:
    r = mailimf_cache_mailbox_read(mmapstr, index, &mailbox);
    if (r != MAIL_NO_ERROR)
      return r;

    break;

  case MAILIMF_ADDRESS_GROUP:
    r = mailimf_cache_group_read(mmapstr, index, &group);
    if (r != MAIL_NO_ERROR)
      return r;
    
    break;
  }

  addr = mailimf_address_new(type, mailbox, group);
  if (addr == NULL)
    goto free;

  * result = addr;

  return MAIL_NO_ERROR;

 free:
  if (mailbox != NULL)
    mailimf_mailbox_free(mailbox);
  if (group != NULL)
    mailimf_group_free(group);
  return MAIL_ERROR_MEMORY;
}

static int mailimf_cache_group_write(MMAPString * mmapstr, size_t * index,
				     struct mailimf_group * group)
{
  int r;

  r = mailimf_cache_string_write(mmapstr, index, group->grp_display_name,
			   strlen(group->grp_display_name));
  if (r != MAIL_NO_ERROR)
    return r;
  
  r = mailimf_cache_mailbox_list_write(mmapstr, index, group->grp_mb_list);
  if (r != MAIL_NO_ERROR)
    return r;
  
  return MAIL_NO_ERROR;
}

static int mailimf_cache_group_read(MMAPString * mmapstr, size_t * index,
				    struct mailimf_group ** result)
{
  int r;
  char * display_name;
  struct mailimf_mailbox_list * mb_list;
  struct mailimf_group * group;
  int res;

  r = mailimf_cache_string_read(mmapstr, index, &display_name);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_cache_mailbox_list_read(mmapstr, index, &mb_list);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_dsp_name;
  }

  group = mailimf_group_new(display_name, mb_list);
  if (group == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_mb_list;
  }

  * result = group;

  return MAIL_NO_ERROR;

 free_mb_list:
  mailimf_mailbox_list_free(mb_list);
 free_dsp_name:
  free(display_name);
 err:
  return res;
}

static int
mailimf_cache_mailbox_list_write(MMAPString * mmapstr, size_t * index,
				 struct mailimf_mailbox_list * mb_list)
{
  clistiter * cur;
  int r;

  if (mb_list == NULL) {
    r = mailimf_cache_int_write(mmapstr, index, CACHE_NULL_POINTER);
    if (r != MAIL_NO_ERROR)
      return r;
  }
  else {
    r = mailimf_cache_int_write(mmapstr, index, CACHE_NOT_NULL);
    if (r != MAIL_NO_ERROR)
      return r;
    
    r = mailimf_cache_int_write(mmapstr, index,
        clist_count(mb_list->mb_list));
    if (r != MAIL_NO_ERROR)
      return r;
    
    for(cur = clist_begin(mb_list->mb_list) ; cur != NULL ;
        cur = clist_next(cur)) {
      struct mailimf_mailbox * mb;
      
      mb = clist_content(cur);
      
      r = mailimf_cache_mailbox_write(mmapstr, index, mb);
      if (r != MAIL_NO_ERROR)
        return r;
    }
  }

  return MAIL_NO_ERROR;
}

static int
mailimf_cache_mailbox_list_read(MMAPString * mmapstr, size_t * index,
				struct mailimf_mailbox_list ** result)
{
  clist * list;
  int r;
  uint32_t count;
  uint32_t i;
  struct mailimf_mailbox_list * mb_list;
  int res;
  uint32_t type;
  
  r = mailimf_cache_int_read(mmapstr, index, &type);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  if (type == CACHE_NULL_POINTER) {
    * result = NULL;
    return MAIL_NO_ERROR;
  }
  
  r = mailimf_cache_int_read(mmapstr, index, &count);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < count ; i++) {
    struct mailimf_mailbox * mb;
    
    r = mailimf_cache_mailbox_read(mmapstr, index, &mb);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = clist_append(list, mb);
    if (r < 0) {
      mailimf_mailbox_free(mb);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  mb_list = mailimf_mailbox_list_new(list);
  if (mb_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = mb_list;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_mailbox_free, NULL);
  clist_free(list);
 err:
  return res;
}

static int mailimf_cache_mailbox_write(MMAPString * mmapstr, size_t * index,
				       struct mailimf_mailbox * mb)
{
  int r;

  if (mb->mb_display_name) {
    r = mailimf_cache_string_write(mmapstr, index, 
        mb->mb_display_name, strlen(mb->mb_display_name));
    if (r != MAIL_NO_ERROR)
      return r;
  }
  else {
    r = mailimf_cache_string_write(mmapstr, index, NULL, 0);
    if (r != MAIL_NO_ERROR)
      return r;
  }

  r = mailimf_cache_string_write(mmapstr, index,
      mb->mb_addr_spec, strlen(mb->mb_addr_spec));
  if (r != MAIL_NO_ERROR)
    return r;

  return MAIL_NO_ERROR;
}

static int mailimf_cache_mailbox_read(MMAPString * mmapstr, size_t * index,
				      struct mailimf_mailbox ** result)
{
  int r;
  char * dsp_name;
  char * addr_spec;
  struct mailimf_mailbox * mb;

  dsp_name = NULL;

  r = mailimf_cache_string_read(mmapstr, index, &dsp_name);
  if (r != MAIL_NO_ERROR)
    return r;

  r = mailimf_cache_string_read(mmapstr, index, &addr_spec);
  if (r != MAIL_NO_ERROR)
    goto free_dsp_name;

  mb = mailimf_mailbox_new(dsp_name, addr_spec);
  if (mb == NULL)
    goto free_addr;

  * result = mb;

  return MAIL_NO_ERROR;

 free_addr:
  free(addr_spec);
 free_dsp_name:
  if (dsp_name != NULL)
    free(dsp_name);
  return MAIL_ERROR_MEMORY;
}
