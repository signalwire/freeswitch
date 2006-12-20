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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimf_types_helper.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif

#include "mailimf.h"

struct mailimf_mailbox_list *
mailimf_mailbox_list_new_empty(void)
{
  clist * list;
  struct mailimf_mailbox_list * mb_list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  mb_list = mailimf_mailbox_list_new(list);
  if (mb_list == NULL)
    return NULL;

  return mb_list;
}

int mailimf_mailbox_list_add(struct mailimf_mailbox_list * mailbox_list,
			     struct mailimf_mailbox * mb)
{
  int r;

  r = clist_append(mailbox_list->mb_list, mb);
  if (r < 0)
    return MAILIMF_ERROR_MEMORY;

  return MAILIMF_NO_ERROR;
}

int mailimf_mailbox_list_add_parse(struct mailimf_mailbox_list * mailbox_list,
				   char * mb_str)
{
  int r;
  size_t cur_token;
  struct mailimf_mailbox * mb;
  int res;

  cur_token = 0;
  r = mailimf_mailbox_parse(mb_str, strlen(mb_str), &cur_token, &mb);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimf_mailbox_list_add(mailbox_list, mb);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free;
  }

  return MAILIMF_NO_ERROR;

 free:
  mailimf_mailbox_free(mb);
 err:
  return res;
}

int mailimf_mailbox_list_add_mb(struct mailimf_mailbox_list * mailbox_list,
				char * display_name, char * address)
{
  int r;
  struct mailimf_mailbox * mb;
  int res;

  mb = mailimf_mailbox_new(display_name, address);
  if (mb == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }
  
  r = mailimf_mailbox_list_add(mailbox_list, mb);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free;
  }

  return MAILIMF_NO_ERROR;

 free:
  mailimf_mailbox_free(mb);
 err:
  return res;
}



struct mailimf_address_list *
mailimf_address_list_new_empty(void)
{
  clist * list;
  struct mailimf_address_list * addr_list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  addr_list = mailimf_address_list_new(list);
  if (addr_list == NULL)
    return NULL;

  return addr_list;
}

int mailimf_address_list_add(struct mailimf_address_list * address_list,
			     struct mailimf_address * addr)
{
  int r;

  r = clist_append(address_list->ad_list, addr);
  if (r < 0)
    return MAILIMF_ERROR_MEMORY;

  return MAILIMF_NO_ERROR;
}

int mailimf_address_list_add_parse(struct mailimf_address_list * address_list,
				   char * addr_str)
{
  int r;
  size_t cur_token;
  struct mailimf_address * addr;
  int res;

  cur_token = 0;
  r = mailimf_address_parse(addr_str, strlen(addr_str), &cur_token, &addr);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimf_address_list_add(address_list, addr);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free;
  }

  return MAILIMF_NO_ERROR;

 free:
  mailimf_address_free(addr);
 err:
  return res;
}

int mailimf_address_list_add_mb(struct mailimf_address_list * address_list,
				char * display_name, char * address)
{
  int r;
  struct mailimf_mailbox * mb;
  struct mailimf_address * addr;
  int res;

  mb = mailimf_mailbox_new(display_name, address);
  if (mb == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  addr = mailimf_address_new(MAILIMF_ADDRESS_MAILBOX, mb, NULL);
  if (addr == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_mb;
  }

  r = mailimf_address_list_add(address_list, addr);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_addr;
  }

  return MAILIMF_NO_ERROR;

 free_addr:
  mailimf_address_free(addr);
 free_mb:
  mailimf_mailbox_free(mb);
 err:
  return res;
}


#if 0
struct mailimf_resent_fields_list *
mailimf_resent_fields_list_new_empty(void)
{
  clist * list;
  struct mailimf_resent_fields_list * rf_list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  rf_list = mailimf_resent_fields_list_new(list);
  if (rf_list == NULL)
    return NULL;

  return rf_list;
}

int mailimf_resent_fields_add(struct mailimf_resent_fields_list * fields,
			      struct mailimf_resent_field * field)
{
  int r;

  r = clist_append(fields->list, field);
  if (r < 0)
    return MAILIMF_ERROR_MEMORY;
  
  return MAILIMF_NO_ERROR;
}
#endif


static void detach_free_common_fields(struct mailimf_orig_date * imf_date,
				      struct mailimf_from * imf_from,
				      struct mailimf_sender * imf_sender,
				      struct mailimf_to * imf_to,
				      struct mailimf_cc * imf_cc,
				      struct mailimf_bcc * imf_bcc,
				      struct mailimf_message_id * imf_msg_id)
{
  if (imf_date != NULL) {
    imf_date->dt_date_time = NULL;
    mailimf_orig_date_free(imf_date);
  }
  if (imf_from != NULL) {
    imf_from->frm_mb_list = NULL;
    mailimf_from_free(imf_from);
  }
  if (imf_sender != NULL) {
    imf_sender->snd_mb = NULL;
    mailimf_sender_free(imf_sender);
  }
  if (imf_to != NULL) {
    imf_to->to_addr_list = NULL;
    mailimf_to_free(imf_to);
  }
  if (imf_cc != NULL) {
    imf_cc->cc_addr_list = NULL;
    mailimf_to_free(imf_to);
  }
  if (imf_bcc != NULL) {
    imf_bcc->bcc_addr_list = NULL;
    mailimf_bcc_free(imf_bcc);
  }
  if (imf_msg_id != NULL) {
    imf_msg_id->mid_value = NULL;
    mailimf_message_id_free(imf_msg_id);
  }
}

static void detach_resent_field(struct mailimf_field * field)
{
  field->fld_type = MAILIMF_FIELD_NONE;
  mailimf_field_free(field);
}

int
mailimf_resent_fields_add_data(struct mailimf_fields * fields,
    struct mailimf_date_time * resent_date,
    struct mailimf_mailbox_list * resent_from,
    struct mailimf_mailbox * resent_sender,
    struct mailimf_address_list * resent_to,
    struct mailimf_address_list * resent_cc,
    struct mailimf_address_list * resent_bcc,
    char * resent_msg_id)
{
  struct mailimf_orig_date * imf_resent_date;
  struct mailimf_from * imf_resent_from;
  struct mailimf_sender * imf_resent_sender;
  struct mailimf_to * imf_resent_to;
  struct mailimf_cc * imf_resent_cc;
  struct mailimf_bcc * imf_resent_bcc;
  struct mailimf_message_id * imf_resent_msg_id;
  struct mailimf_field * field;
  int r;

  imf_resent_date = NULL;
  imf_resent_from = NULL;
  imf_resent_sender = NULL;
  imf_resent_to = NULL;
  imf_resent_cc = NULL;
  imf_resent_bcc = NULL;
  imf_resent_msg_id = NULL;
  field = NULL;

  if (resent_date != NULL) {
    imf_resent_date = mailimf_orig_date_new(resent_date);
    if (imf_resent_date == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_RESENT_DATE,
        NULL /* return-path */,
        imf_resent_date /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (resent_from != NULL) {
    imf_resent_from = mailimf_from_new(resent_from);
    if (imf_resent_from == NULL)
      goto free_field;
    field = mailimf_field_new(MAILIMF_FIELD_RESENT_FROM,
        NULL /* return-path */,
        NULL /* resent date */,
        imf_resent_from /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (resent_sender != NULL) {
    imf_resent_sender = mailimf_sender_new(resent_sender);
    if (imf_resent_sender == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_RESENT_SENDER,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        imf_resent_sender /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (resent_to != NULL) {
    imf_resent_to = mailimf_to_new(resent_to);
    if (imf_resent_to == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_RESENT_TO,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        imf_resent_to /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (resent_cc != NULL) {
    imf_resent_cc = mailimf_cc_new(resent_cc);
    if (imf_resent_cc == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_RESENT_CC,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        imf_resent_cc /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (resent_bcc != NULL) {
    imf_resent_bcc = mailimf_bcc_new(resent_bcc);
    if (imf_resent_bcc == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_RESENT_BCC,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        imf_resent_bcc /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (resent_msg_id != NULL) {
    imf_resent_msg_id = mailimf_message_id_new(resent_msg_id);
    if (imf_resent_msg_id == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_RESENT_MSG_ID,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        imf_resent_msg_id /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  return MAILIMF_NO_ERROR;

 free_field:
  if (field != NULL) {
    detach_resent_field(field);
    mailimf_field_free(field);
  }
 free:
  detach_free_common_fields(imf_resent_date,
			    imf_resent_from,
			    imf_resent_sender,
			    imf_resent_to,
			    imf_resent_cc,
			    imf_resent_bcc,
			    imf_resent_msg_id);
  return MAILIMF_ERROR_MEMORY;
}

struct mailimf_fields *
mailimf_resent_fields_new_with_data_all(struct mailimf_date_time *
    resent_date,
    struct mailimf_mailbox_list *
    resent_from,
    struct mailimf_mailbox *
    resent_sender,
    struct mailimf_address_list *
    resent_to,
    struct mailimf_address_list *
    resent_cc,
    struct mailimf_address_list *
    resent_bcc,
    char * resent_msg_id)
{
  struct mailimf_fields * resent_fields;
  int r;

  resent_fields = mailimf_fields_new_empty();
  if (resent_fields == NULL)
    goto err;

  r = mailimf_resent_fields_add_data(resent_fields,
      resent_date, resent_from,
      resent_sender, resent_to,
      resent_cc, resent_bcc,
      resent_msg_id);
  if (r != MAILIMF_NO_ERROR)
    goto free;

  return resent_fields;

 free:
  mailimf_fields_free(resent_fields);
 err:
  return NULL;
}


struct mailimf_fields *
mailimf_resent_fields_new_with_data(struct mailimf_mailbox_list * from,
    struct mailimf_mailbox * sender,
    struct mailimf_address_list * to,
    struct mailimf_address_list * cc,
    struct mailimf_address_list * bcc)
{
  struct mailimf_date_time * date;
  char * msg_id;
  struct mailimf_fields * fields;

  date = mailimf_get_current_date();
  if (date == NULL)
    goto err;

  msg_id = mailimf_get_message_id();
  if (msg_id == NULL)
    goto free_date;

  fields = mailimf_resent_fields_new_with_data_all(date,
      from, sender, to, cc, bcc, msg_id);
  if (fields == NULL)
    goto free_msg_id;

  return fields;

 free_msg_id:
  free(msg_id);
 free_date:
  mailimf_date_time_free(date);
 err:
  return NULL;
}


struct mailimf_fields *
mailimf_fields_new_empty(void)
{
  clist * list;
  struct mailimf_fields * fields_list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  fields_list = mailimf_fields_new(list);
  if (fields_list == NULL)
    return NULL;

  return fields_list;
}

int mailimf_fields_add(struct mailimf_fields * fields,
		       struct mailimf_field * field)
{
  int r;

  r = clist_append(fields->fld_list, field);
  if (r < 0)
    return MAILIMF_ERROR_MEMORY;
  
  return MAILIMF_NO_ERROR;
}

static void detach_free_fields(struct mailimf_orig_date * date,
			       struct mailimf_from * from,
			       struct mailimf_sender * sender,
			       struct mailimf_reply_to * reply_to,
			       struct mailimf_to * to,
			       struct mailimf_cc * cc,
			       struct mailimf_bcc * bcc,
			       struct mailimf_message_id * msg_id,
			       struct mailimf_in_reply_to * in_reply_to,
			       struct mailimf_references * references,
			       struct mailimf_subject * subject)
{
  detach_free_common_fields(date,
      from,
      sender,
      to,
      cc,
      bcc,
      msg_id);

  if (reply_to != NULL) {
    reply_to->rt_addr_list = NULL;
    mailimf_reply_to_free(reply_to);
  }

  if (in_reply_to != NULL) {
    in_reply_to->mid_list = NULL;
    mailimf_in_reply_to_free(in_reply_to);
  }

  if (references != NULL) {
    references->mid_list = NULL;
    mailimf_references_free(references);
  }

  if (subject != NULL) {
    subject->sbj_value = NULL;
    mailimf_subject_free(subject);
  }
}


static void detach_field(struct mailimf_field * field)
{
  field->fld_type = MAILIMF_FIELD_NONE;
  mailimf_field_free(field);
}

int mailimf_fields_add_data(struct mailimf_fields * fields,
			    struct mailimf_date_time * date,
			    struct mailimf_mailbox_list * from,
			    struct mailimf_mailbox * sender,
			    struct mailimf_address_list * reply_to,
			    struct mailimf_address_list * to,
			    struct mailimf_address_list * cc,
			    struct mailimf_address_list * bcc,
			    char * msg_id,
			    clist * in_reply_to,
			    clist * references,
			    char * subject)
{
  struct mailimf_orig_date * imf_date;
  struct mailimf_from * imf_from;
  struct mailimf_sender * imf_sender;
  struct mailimf_reply_to * imf_reply_to;
  struct mailimf_to * imf_to;
  struct mailimf_cc * imf_cc;
  struct mailimf_bcc * imf_bcc;
  struct mailimf_message_id * imf_msg_id;
  struct mailimf_references * imf_references;
  struct mailimf_in_reply_to * imf_in_reply_to;
  struct mailimf_subject * imf_subject;
  struct mailimf_field * field;
  int r;

  imf_date = NULL;
  imf_from = NULL;
  imf_sender = NULL;
  imf_reply_to = NULL;
  imf_to = NULL;
  imf_cc = NULL;
  imf_bcc = NULL;
  imf_msg_id = NULL;
  imf_references = NULL;
  imf_in_reply_to = NULL;
  imf_subject =NULL;
  field = NULL;

  if (date != NULL) {
    imf_date = mailimf_orig_date_new(date);
    if (imf_date == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_ORIG_DATE,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        imf_date /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (from != NULL) {
    imf_from = mailimf_from_new(from);
    if (imf_from == NULL)
      goto free_field;
    field = mailimf_field_new(MAILIMF_FIELD_FROM,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        imf_from /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (sender != NULL) {
    imf_sender = mailimf_sender_new(sender);
    if (imf_sender == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_SENDER,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        imf_sender /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (reply_to != NULL) {
    imf_reply_to = mailimf_reply_to_new(reply_to);
    if (imf_reply_to == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_REPLY_TO,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        imf_reply_to /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (to != NULL) {
    imf_to = mailimf_to_new(to);
    if (imf_to == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_TO,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        imf_to /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (cc != NULL) {
    imf_cc = mailimf_cc_new(cc);
    if (imf_cc == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_CC,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        imf_cc /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (bcc != NULL) {
    imf_bcc = mailimf_bcc_new(bcc);
    if (imf_bcc == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_BCC,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        imf_bcc /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (msg_id != NULL) {
    imf_msg_id = mailimf_message_id_new(msg_id);
    if (imf_msg_id == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_MESSAGE_ID,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        imf_msg_id /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (in_reply_to != NULL) {
    imf_in_reply_to = mailimf_in_reply_to_new(in_reply_to);
    if (imf_in_reply_to == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_IN_REPLY_TO,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        imf_in_reply_to /* in reply to */,
        NULL /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (references != NULL) {
    imf_references = mailimf_references_new(references);
    if (imf_references == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_REFERENCES,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        imf_references /* references */,
        NULL /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  if (subject != NULL) {
    imf_subject = mailimf_subject_new(subject);
    if (imf_subject == NULL)
      goto free;
    field = mailimf_field_new(MAILIMF_FIELD_SUBJECT,
        NULL /* return-path */,
        NULL /* resent date */,
        NULL /* resent from */,
        NULL /* resent sender */,
        NULL /* resent to */,
        NULL /* resent cc */,
        NULL /* resent bcc */,
        NULL /* resent msg id */,
        NULL /* date */,
        NULL /* from */,
        NULL /* sender */,
        NULL /* reply-to */,
        NULL /* to */,
        NULL /* cc */,
        NULL /* bcc */,
        NULL /* message id */,
        NULL /* in reply to */,
        NULL /* references */,
        imf_subject /* subject */,
        NULL /* comments */,
        NULL /* keywords */,
        NULL /* optional field */);
    if (field == NULL)
      goto free;
    r =  mailimf_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR)
      goto free_field;
  }

  return MAILIMF_NO_ERROR;

 free_field:
  if (field != NULL) {
    detach_field(field);
    mailimf_field_free(field);
  }
 free:
  detach_free_fields(imf_date,
		     imf_from,
		     imf_sender,
		     imf_reply_to,
		     imf_to,
		     imf_cc,
		     imf_bcc,
		     imf_msg_id,
		     imf_in_reply_to,
		     imf_references,
		     imf_subject);

  return MAILIMF_ERROR_MEMORY;
}

struct mailimf_fields *
mailimf_fields_new_with_data_all(struct mailimf_date_time * date,
				 struct mailimf_mailbox_list * from,
				 struct mailimf_mailbox * sender,
				 struct mailimf_address_list * reply_to,
				 struct mailimf_address_list * to,
				 struct mailimf_address_list * cc,
				 struct mailimf_address_list * bcc,
				 char * message_id,
				 clist * in_reply_to,
				 clist * references,
				 char * subject)
{
  struct mailimf_fields * fields;
  int r;

  fields = mailimf_fields_new_empty();
  if (fields == NULL)
    goto err;

  r = mailimf_fields_add_data(fields,
			      date,
			      from,
			      sender,
			      reply_to,
			      to,
			      cc,
			      bcc,
			      message_id,
			      in_reply_to,
			      references,
			      subject);
  if (r != MAILIMF_NO_ERROR)
    goto free;

  return fields;

 free:
  mailimf_fields_free(fields);
 err:
  return NULL;
}

struct mailimf_fields *
mailimf_fields_new_with_data(struct mailimf_mailbox_list * from,
			     struct mailimf_mailbox * sender,
			     struct mailimf_address_list * reply_to,
			     struct mailimf_address_list * to,
			     struct mailimf_address_list * cc,
			     struct mailimf_address_list * bcc,
			     clist * in_reply_to,
			     clist * references,
			     char * subject)
{
  struct mailimf_date_time * date;
  char * msg_id;
  struct mailimf_fields * fields;

  date = mailimf_get_current_date();
  if (date == NULL)
    goto err;

  msg_id = mailimf_get_message_id();
  if (msg_id == NULL)
    goto free_date;

  fields = mailimf_fields_new_with_data_all(date,
					    from, sender, reply_to,
					    to, cc, bcc,
					    msg_id,
					    in_reply_to, references,
					    subject);
  if (fields == NULL)
    goto free_msg_id;

  return fields;

 free_msg_id:
  free(msg_id);
 free_date:
  mailimf_date_time_free(date);
 err:
  return NULL;
}



#define MAX_MESSAGE_ID 512

char * mailimf_get_message_id(void)
{
  char id[MAX_MESSAGE_ID];
  time_t now;
  char name[MAX_MESSAGE_ID];
  long value;

  now = time(NULL);
  value = random();

  gethostname(name, MAX_MESSAGE_ID);
  snprintf(id, MAX_MESSAGE_ID, "etPan.%lx.%lx.%x@%s",
	   now, value, getpid(), name);

  return strdup(id);
}



static time_t mkgmtime(struct tm * tmp);


struct mailimf_date_time * mailimf_get_current_date(void)
{
  struct tm gmt;
  struct tm lt;
  int off;
  time_t now;
  struct mailimf_date_time * date_time;

  now = time(NULL);

  if (gmtime_r(&now, &gmt) == NULL)
    return NULL;

  if (localtime_r(&now, &lt) == NULL)
    return NULL;

  off = (mkgmtime(&lt) - mkgmtime(&gmt)) / (60 * 60) * 100;

  date_time = mailimf_date_time_new(lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900,
				    lt.tm_hour, lt.tm_min, lt.tm_sec,
				    off);

  return date_time;
}



/* mkgmtime.c - make time corresponding to a GMT timeval struct
 $Id: mailimf_types_helper.c,v 1.18 2006/10/22 20:22:06 hoa Exp $
 
 * Copyright (c) 1998-2000 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 */
/*
 * Copyright (c) 1987, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Arthur David Olson of the National Cancer Institute.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
** Adapted from code provided by Robert Elz, who writes:
**	The "best" way to do mktime I think is based on an idea of Bob
**	Kridle's (so its said...) from a long time ago. (mtxinu!kridle now).
**	It does a binary search of the time_t space.  Since time_t's are
**	just 32 bits, its a max of 32 iterations (even at 64 bits it
**	would still be very reasonable).
*/

/*
  adapted for libEtPan! by DINH V. Hoa
*/

#ifndef WRONG
#define WRONG	(-1)
#endif /* !defined WRONG */

static int tmcomp(struct tm * atmp, struct tm * btmp)
{
  register int	result;
  
  if ((result = (atmp->tm_year - btmp->tm_year)) == 0 &&
      (result = (atmp->tm_mon - btmp->tm_mon)) == 0 &&
      (result = (atmp->tm_mday - btmp->tm_mday)) == 0 &&
      (result = (atmp->tm_hour - btmp->tm_hour)) == 0 &&
      (result = (atmp->tm_min - btmp->tm_min)) == 0)
    result = atmp->tm_sec - btmp->tm_sec;
  return result;
}

static time_t mkgmtime(struct tm * tmp)
{
  register int			dir;
  register int			bits;
  register int			saved_seconds;
  time_t				t;
  struct tm			yourtm, *mytm;
  
  yourtm = *tmp;
  saved_seconds = yourtm.tm_sec;
  yourtm.tm_sec = 0;
  /*
  ** Calculate the number of magnitude bits in a time_t
  ** (this works regardless of whether time_t is
  ** signed or unsigned, though lint complains if unsigned).
  */
  for (bits = 0, t = 1; t > 0; ++bits, t <<= 1)
    ;
  /*
  ** If time_t is signed, then 0 is the median value,
  ** if time_t is unsigned, then 1 << bits is median.
  */
  t = (t < 0) ? 0 : ((time_t) 1 << bits);
  for ( ; ; ) {
    mytm = gmtime(&t);
    dir = tmcomp(mytm, &yourtm);
    if (dir != 0) {
      if (bits-- < 0)
	return WRONG;
      if (bits < 0)
	--t;
      else if (dir > 0)
	t -= (time_t) 1 << bits;
      else	t += (time_t) 1 << bits;
      continue;
    }
    break;
  }
  t += saved_seconds;
  return t;
}







void mailimf_single_fields_init(struct mailimf_single_fields * single_fields,
                                struct mailimf_fields * fields)
{
  clistiter * cur;

  memset(single_fields, 0, sizeof(struct mailimf_single_fields));

  cur = clist_begin(fields->fld_list);
  while (cur != NULL) {
    struct mailimf_field * field;

    field = clist_content(cur);

    switch (field->fld_type) {
    case MAILIMF_FIELD_ORIG_DATE:
      if (single_fields->fld_orig_date == NULL)
        single_fields->fld_orig_date = field->fld_data.fld_orig_date;
      cur = clist_next(cur);
      break;
    case MAILIMF_FIELD_FROM:
      if (single_fields->fld_from == NULL) {
        single_fields->fld_from = field->fld_data.fld_from;
        cur = clist_next(cur);
      }
      else {
        clist_concat(single_fields->fld_from->frm_mb_list->mb_list,
                     field->fld_data.fld_from->frm_mb_list->mb_list);
        mailimf_field_free(field);
        cur = clist_delete(fields->fld_list, cur);
      }
      break;
    case MAILIMF_FIELD_SENDER:
      if (single_fields->fld_sender == NULL)
        single_fields->fld_sender = field->fld_data.fld_sender;
      cur = clist_next(cur);
      break;
    case MAILIMF_FIELD_REPLY_TO:
      if (single_fields->fld_reply_to == NULL) {
        single_fields->fld_reply_to = field->fld_data.fld_reply_to;
        cur = clist_next(cur);
      }
      else {
        clist_concat(single_fields->fld_reply_to->rt_addr_list->ad_list,
                     field->fld_data.fld_reply_to->rt_addr_list->ad_list);
        mailimf_field_free(field);
        cur = clist_delete(fields->fld_list, cur);
      }
      break;
    case MAILIMF_FIELD_TO:
      if (single_fields->fld_to == NULL) {
        single_fields->fld_to = field->fld_data.fld_to;
        cur = clist_next(cur);
      }
      else {
        clist_concat(single_fields->fld_to->to_addr_list->ad_list,
                     field->fld_data.fld_to->to_addr_list->ad_list);
        mailimf_field_free(field);
        cur = clist_delete(fields->fld_list, cur);
      }
      break;
    case MAILIMF_FIELD_CC:
      if (single_fields->fld_cc == NULL) {
        single_fields->fld_cc = field->fld_data.fld_cc;
        cur = clist_next(cur);
      }
      else {
        clist_concat(single_fields->fld_cc->cc_addr_list->ad_list, 
                     field->fld_data.fld_cc->cc_addr_list->ad_list);
        mailimf_field_free(field);
        cur = clist_delete(fields->fld_list, cur);
      }
      break;
    case MAILIMF_FIELD_BCC:
      if (single_fields->fld_bcc == NULL) {
        single_fields->fld_bcc = field->fld_data.fld_bcc;
        cur = clist_next(cur);
      }
      else {
        if (field->fld_data.fld_bcc->bcc_addr_list != NULL) {
          if (single_fields->fld_bcc->bcc_addr_list == NULL) {
            single_fields->fld_bcc->bcc_addr_list = field->fld_data.fld_bcc->bcc_addr_list;
            field->fld_data.fld_bcc->bcc_addr_list = NULL;
          }
          else {
            clist_concat(single_fields->fld_bcc->bcc_addr_list->ad_list,
                field->fld_data.fld_bcc->bcc_addr_list->ad_list);
          }
          mailimf_field_free(field);
          cur = clist_delete(fields->fld_list, cur);
        }
        else {
          cur = clist_next(cur);
        }
      }
      break;
    case MAILIMF_FIELD_MESSAGE_ID:
      if (single_fields->fld_message_id == NULL)
        single_fields->fld_message_id = field->fld_data.fld_message_id;
      cur = clist_next(cur);
      break;
    case MAILIMF_FIELD_IN_REPLY_TO:
      if (single_fields->fld_in_reply_to == NULL)
        single_fields->fld_in_reply_to = field->fld_data.fld_in_reply_to;
      cur = clist_next(cur);
      break;
    case MAILIMF_FIELD_REFERENCES:
      if (single_fields->fld_references == NULL)
        single_fields->fld_references = field->fld_data.fld_references;
      cur = clist_next(cur);
      break;
    case MAILIMF_FIELD_SUBJECT:
      if (single_fields->fld_subject == NULL)
        single_fields->fld_subject = field->fld_data.fld_subject;
      cur = clist_next(cur);
      break;
    case MAILIMF_FIELD_COMMENTS:
      if (single_fields->fld_comments == NULL)
        single_fields->fld_comments = field->fld_data.fld_comments;
      cur = clist_next(cur);
      break;
    case MAILIMF_FIELD_KEYWORDS:
      if (single_fields->fld_keywords == NULL)
        single_fields->fld_keywords = field->fld_data.fld_keywords;
      cur = clist_next(cur);
      break;
    default:
      cur = clist_next(cur);
      break;
    }
  }
}


struct mailimf_single_fields *
mailimf_single_fields_new(struct mailimf_fields * fields)
{
  struct mailimf_single_fields * single_fields;

  single_fields = malloc(sizeof(struct mailimf_single_fields));
  if (single_fields == NULL)
    goto err;

  mailimf_single_fields_init(single_fields, fields);

  return single_fields;

 err:
  return NULL;
}

void mailimf_single_fields_free(struct mailimf_single_fields *
                                single_fields)
{
  free(single_fields);
}

struct mailimf_field * mailimf_field_new_custom(char * name, char * value)
{
  struct mailimf_optional_field * opt_field;
  struct mailimf_field * field;

  opt_field = mailimf_optional_field_new(name, value);
  if (opt_field == NULL)
    goto err;

  field = mailimf_field_new(MAILIMF_FIELD_OPTIONAL_FIELD,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL,
      NULL, NULL, opt_field);
  if (field == NULL)
    goto free_opt_field;

  return field;
  
 free_opt_field:
  mailimf_optional_field_free(opt_field);
 err:
  return NULL;
}
