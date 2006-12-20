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
 * $Id: mailimf_types.c,v 1.18 2005/06/01 12:22:01 smarinier Exp $
 */

#include "mailimf_types.h"
#include "mmapstring.h"
#include <stdlib.h>

void mailimf_atom_free(char * atom)
{
  free(atom);
}

void mailimf_dot_atom_free(char * dot_atom)
{
  free(dot_atom);
}

void mailimf_dot_atom_text_free(char * dot_atom)
{
  free(dot_atom);
}

void mailimf_quoted_string_free(char * quoted_string)
{
  free(quoted_string);
}

void mailimf_word_free(char * word)
{
  free(word);
}

void mailimf_phrase_free(char * phrase)
{
  free(phrase);
}

void mailimf_unstructured_free(char * unstructured)
{
  free(unstructured);
}


LIBETPAN_EXPORT
struct mailimf_date_time *
mailimf_date_time_new(int dt_day, int dt_month, int dt_year,
    int dt_hour, int dt_min, int dt_sec, int dt_zone)
{
  struct mailimf_date_time * date_time;

  date_time = malloc(sizeof(* date_time));
  if (date_time == NULL)
    return NULL;

  date_time->dt_day = dt_day;
  date_time->dt_month = dt_month;
  date_time->dt_year = dt_year;
  date_time->dt_hour = dt_hour;
  date_time->dt_min = dt_min;
  date_time->dt_sec = dt_sec;
  date_time->dt_zone = dt_zone;

  return date_time;
}

LIBETPAN_EXPORT
void mailimf_date_time_free(struct mailimf_date_time * date_time)
{
  free(date_time);
}



LIBETPAN_EXPORT
struct mailimf_address *
mailimf_address_new(int ad_type, struct mailimf_mailbox * ad_mailbox,
    struct mailimf_group * ad_group)
{
  struct mailimf_address * address;

  address = malloc(sizeof(* address));
  if (address == NULL)
    return NULL;

  address->ad_type = ad_type;
  switch (ad_type) {
  case MAILIMF_ADDRESS_MAILBOX:
    address->ad_data.ad_mailbox = ad_mailbox;
    break;
  case MAILIMF_ADDRESS_GROUP:
    address->ad_data.ad_group = ad_group;
    break;
  }

  return address;
}

LIBETPAN_EXPORT
void mailimf_address_free(struct mailimf_address * address)
{
  switch (address->ad_type) {
  case MAILIMF_ADDRESS_MAILBOX:
    mailimf_mailbox_free(address->ad_data.ad_mailbox);
    break;
  case MAILIMF_ADDRESS_GROUP:
    mailimf_group_free(address->ad_data.ad_group);
  }
  free(address);
}

LIBETPAN_EXPORT
struct mailimf_mailbox *
mailimf_mailbox_new(char * mb_display_name, char * mb_addr_spec)
{
  struct mailimf_mailbox * mb;

  mb = malloc(sizeof(* mb));
  if (mb == NULL)
    return NULL;

  mb->mb_display_name = mb_display_name;
  mb->mb_addr_spec = mb_addr_spec;

  return mb;
}

LIBETPAN_EXPORT
void mailimf_mailbox_free(struct mailimf_mailbox * mailbox)
{
  if (mailbox->mb_display_name != NULL)
    mailimf_display_name_free(mailbox->mb_display_name);
  mailimf_addr_spec_free(mailbox->mb_addr_spec);
  free(mailbox);
}

void mailimf_angle_addr_free(char * angle_addr)
{
  free(angle_addr);
}

LIBETPAN_EXPORT
struct mailimf_group *
mailimf_group_new(char * grp_display_name,
    struct mailimf_mailbox_list * grp_mb_list)
{
  struct mailimf_group * group;

  group = malloc(sizeof(* group));
  if (group == NULL)
    return NULL;

  group->grp_display_name = grp_display_name;
  group->grp_mb_list = grp_mb_list;
  
  return group;
}

LIBETPAN_EXPORT
void mailimf_group_free(struct mailimf_group * group)
{
  if (group->grp_mb_list)
    mailimf_mailbox_list_free(group->grp_mb_list);
  mailimf_display_name_free(group->grp_display_name);
  free(group);
}

void mailimf_display_name_free(char * display_name)
{
  mailimf_phrase_free(display_name);
}

LIBETPAN_EXPORT
struct mailimf_mailbox_list *
mailimf_mailbox_list_new(clist * mb_list)
{
  struct mailimf_mailbox_list * mbl;

  mbl = malloc(sizeof(* mbl));
  if (mbl == NULL)
    return NULL;

  mbl->mb_list = mb_list;

  return mbl;
}

LIBETPAN_EXPORT
void mailimf_mailbox_list_free(struct mailimf_mailbox_list * mb_list)
{
  clist_foreach(mb_list->mb_list, (clist_func) mailimf_mailbox_free, NULL);
  clist_free(mb_list->mb_list);
  free(mb_list);
}


LIBETPAN_EXPORT
struct mailimf_address_list *
mailimf_address_list_new(clist * ad_list)
{
  struct mailimf_address_list * addr_list;

  addr_list = malloc(sizeof(* addr_list));
  if (addr_list == NULL)
    return NULL;

  addr_list->ad_list = ad_list;

  return addr_list;
}

LIBETPAN_EXPORT
void mailimf_address_list_free(struct mailimf_address_list * addr_list)
{
  clist_foreach(addr_list->ad_list, (clist_func) mailimf_address_free, NULL);
  clist_free(addr_list->ad_list);
  free(addr_list);
}


void mailimf_addr_spec_free(char * addr_spec)
{
  free(addr_spec);
}

void mailimf_local_part_free(char * local_part)
{
  free(local_part);
}

void mailimf_domain_free(char * domain)
{
  free(domain);
}

void mailimf_domain_literal_free(char * domain_literal)
{
  free(domain_literal);
}

LIBETPAN_EXPORT
struct mailimf_message * 
mailimf_message_new(struct mailimf_fields * msg_fields,
    struct mailimf_body * msg_body)
{
  struct mailimf_message * message;
  
  message = malloc(sizeof(* message));
  if (message == NULL)
    return NULL;

  message->msg_fields = msg_fields;
  message->msg_body = msg_body;

  return message;
}

LIBETPAN_EXPORT
void mailimf_message_free(struct mailimf_message * message)
{
  mailimf_body_free(message->msg_body);
  mailimf_fields_free(message->msg_fields);
  free(message);
}

LIBETPAN_EXPORT
struct mailimf_body * mailimf_body_new(const char * bd_text, size_t bd_size)
{
  struct mailimf_body * body;

  body = malloc(sizeof(* body));
  if (body == NULL)
    return NULL;
  body->bd_text = bd_text;
  body->bd_size = bd_size;

  return body;
}

LIBETPAN_EXPORT
void mailimf_body_free(struct mailimf_body * body)
{
  free(body);
}


LIBETPAN_EXPORT
struct mailimf_field *
mailimf_field_new(int fld_type,
    struct mailimf_return * fld_return_path,
    struct mailimf_orig_date * fld_resent_date,
    struct mailimf_from * fld_resent_from,
    struct mailimf_sender * fld_resent_sender,
    struct mailimf_to * fld_resent_to,
    struct mailimf_cc * fld_resent_cc,
    struct mailimf_bcc * fld_resent_bcc,
    struct mailimf_message_id * fld_resent_msg_id,
    struct mailimf_orig_date * fld_orig_date,
    struct mailimf_from * fld_from,
    struct mailimf_sender * fld_sender,
    struct mailimf_reply_to * fld_reply_to,
    struct mailimf_to * fld_to,
    struct mailimf_cc * fld_cc,
    struct mailimf_bcc * fld_bcc,
    struct mailimf_message_id * fld_message_id,
    struct mailimf_in_reply_to * fld_in_reply_to,
    struct mailimf_references * fld_references,
    struct mailimf_subject * fld_subject,
    struct mailimf_comments * fld_comments,
    struct mailimf_keywords * fld_keywords,
    struct mailimf_optional_field * fld_optional_field)
{
  struct mailimf_field * field;

  field = malloc(sizeof(* field));
  if (field == NULL)
    return NULL;

  field->fld_type = fld_type;
  switch (fld_type) {
  case MAILIMF_FIELD_RETURN_PATH:
    field->fld_data.fld_return_path = fld_return_path;
    break;
  case MAILIMF_FIELD_RESENT_DATE:
    field->fld_data.fld_resent_date = fld_resent_date;
    break;
  case MAILIMF_FIELD_RESENT_FROM:
    field->fld_data.fld_resent_from = fld_resent_from;
    break;
  case MAILIMF_FIELD_RESENT_SENDER:
    field->fld_data.fld_resent_sender = fld_resent_sender;
    break;
  case MAILIMF_FIELD_RESENT_TO:
    field->fld_data.fld_resent_to = fld_resent_to;
    break;
  case MAILIMF_FIELD_RESENT_CC:
    field->fld_data.fld_resent_cc = fld_resent_cc;
    break;
  case MAILIMF_FIELD_RESENT_BCC:
    field->fld_data.fld_resent_bcc = fld_resent_bcc;
    break;
  case MAILIMF_FIELD_RESENT_MSG_ID:
    field->fld_data.fld_resent_msg_id = fld_resent_msg_id;
    break;
  case MAILIMF_FIELD_ORIG_DATE:
    field->fld_data.fld_orig_date = fld_orig_date;
    break;
  case MAILIMF_FIELD_FROM:
    field->fld_data.fld_from = fld_from;
    break;
  case MAILIMF_FIELD_SENDER:
    field->fld_data.fld_sender = fld_sender;
    break;
  case MAILIMF_FIELD_REPLY_TO:
    field->fld_data.fld_reply_to = fld_reply_to;
    break;
  case MAILIMF_FIELD_TO:
    field->fld_data.fld_to = fld_to;
    break;
  case MAILIMF_FIELD_CC:
    field->fld_data.fld_cc = fld_cc;
    break;
  case MAILIMF_FIELD_BCC:
    field->fld_data.fld_bcc = fld_bcc;
    break;
  case MAILIMF_FIELD_MESSAGE_ID:
    field->fld_data.fld_message_id = fld_message_id;
    break;
  case MAILIMF_FIELD_IN_REPLY_TO:
    field->fld_data.fld_in_reply_to = fld_in_reply_to;
    break;
  case MAILIMF_FIELD_REFERENCES:
    field->fld_data.fld_references = fld_references;
    break;
  case MAILIMF_FIELD_SUBJECT:
    field->fld_data.fld_subject = fld_subject;
    break;
  case MAILIMF_FIELD_COMMENTS:
    field->fld_data.fld_comments = fld_comments;
    break;
  case MAILIMF_FIELD_KEYWORDS:
    field->fld_data.fld_keywords = fld_keywords;
    break;
  case MAILIMF_FIELD_OPTIONAL_FIELD:
    field->fld_data.fld_optional_field = fld_optional_field;
    break;
  }
  
  return field;
}

LIBETPAN_EXPORT
void mailimf_field_free(struct mailimf_field * field)
{
  switch (field->fld_type) {
  case MAILIMF_FIELD_RETURN_PATH:
    mailimf_return_free(field->fld_data.fld_return_path);
    break;
  case MAILIMF_FIELD_RESENT_DATE:
    mailimf_orig_date_free(field->fld_data.fld_resent_date);
    break;
  case MAILIMF_FIELD_RESENT_FROM:
    mailimf_from_free(field->fld_data.fld_resent_from);
    break;
  case MAILIMF_FIELD_RESENT_SENDER:
    mailimf_sender_free(field->fld_data.fld_resent_sender);
    break;
  case MAILIMF_FIELD_RESENT_TO:
    mailimf_to_free(field->fld_data.fld_resent_to);
    break;
  case MAILIMF_FIELD_RESENT_CC:
    mailimf_cc_free(field->fld_data.fld_resent_cc);
    break;
  case MAILIMF_FIELD_RESENT_BCC:
    mailimf_bcc_free(field->fld_data.fld_resent_bcc);
    break;
  case MAILIMF_FIELD_RESENT_MSG_ID:
    mailimf_message_id_free(field->fld_data.fld_resent_msg_id);
    break;
  case MAILIMF_FIELD_ORIG_DATE:
    mailimf_orig_date_free(field->fld_data.fld_orig_date);
    break;
  case MAILIMF_FIELD_FROM:
    mailimf_from_free(field->fld_data.fld_from);
    break;
  case MAILIMF_FIELD_SENDER:
    mailimf_sender_free(field->fld_data.fld_sender);
    break;
  case MAILIMF_FIELD_REPLY_TO:
    mailimf_reply_to_free(field->fld_data.fld_reply_to);
    break;
  case MAILIMF_FIELD_TO:
    mailimf_to_free(field->fld_data.fld_to);
    break;
  case MAILIMF_FIELD_CC:
    mailimf_cc_free(field->fld_data.fld_cc);
    break;
  case MAILIMF_FIELD_BCC:
    mailimf_bcc_free(field->fld_data.fld_bcc);
    break;
  case MAILIMF_FIELD_MESSAGE_ID:
    mailimf_message_id_free(field->fld_data.fld_message_id);
    break;
  case MAILIMF_FIELD_IN_REPLY_TO:
    mailimf_in_reply_to_free(field->fld_data.fld_in_reply_to);
    break;
  case MAILIMF_FIELD_REFERENCES:
    mailimf_references_free(field->fld_data.fld_references);
    break;
  case MAILIMF_FIELD_SUBJECT:
    mailimf_subject_free(field->fld_data.fld_subject);
    break;
  case MAILIMF_FIELD_COMMENTS:
    mailimf_comments_free(field->fld_data.fld_comments);
    break;
  case MAILIMF_FIELD_KEYWORDS:
    mailimf_keywords_free(field->fld_data.fld_keywords);
    break;
  case MAILIMF_FIELD_OPTIONAL_FIELD:
    mailimf_optional_field_free(field->fld_data.fld_optional_field);
    break;
  }
  
  free(field);
}

LIBETPAN_EXPORT
struct mailimf_fields * mailimf_fields_new(clist * fld_list)
{
  struct mailimf_fields * fields;

  fields = malloc(sizeof(* fields));
  if (fields == NULL)
    return NULL;

  fields->fld_list = fld_list;

  return fields;
}

LIBETPAN_EXPORT
void mailimf_fields_free(struct mailimf_fields * fields)
{
  if (fields->fld_list != NULL) {
    clist_foreach(fields->fld_list, (clist_func) mailimf_field_free, NULL);
    clist_free(fields->fld_list);
  }
  free(fields);
}

LIBETPAN_EXPORT
struct mailimf_orig_date * mailimf_orig_date_new(struct mailimf_date_time *
    dt_date_time)
{
  struct mailimf_orig_date * orig_date;

  orig_date = malloc(sizeof(* orig_date));
  if (orig_date == NULL)
    return NULL;

  orig_date->dt_date_time = dt_date_time;

  return orig_date;
}

LIBETPAN_EXPORT
void mailimf_orig_date_free(struct mailimf_orig_date * orig_date)
{
  if (orig_date->dt_date_time != NULL)
    mailimf_date_time_free(orig_date->dt_date_time);
  free(orig_date);
}

LIBETPAN_EXPORT
struct mailimf_from *
mailimf_from_new(struct mailimf_mailbox_list * frm_mb_list)
{
  struct mailimf_from * from;

  from = malloc(sizeof(* from));
  if (from == NULL)
    return NULL;
  
  from->frm_mb_list = frm_mb_list;

  return from;
}

LIBETPAN_EXPORT
void mailimf_from_free(struct mailimf_from * from)
{
  if (from->frm_mb_list != NULL)
    mailimf_mailbox_list_free(from->frm_mb_list);
  free(from);
}

LIBETPAN_EXPORT
struct mailimf_sender * mailimf_sender_new(struct mailimf_mailbox * snd_mb)
{
  struct mailimf_sender * sender;

  sender = malloc(sizeof(* sender));
  if (sender == NULL)
    return NULL;

  sender->snd_mb = snd_mb;

  return sender;
}

LIBETPAN_EXPORT
void mailimf_sender_free(struct mailimf_sender * sender)
{
  if (sender->snd_mb != NULL)
    mailimf_mailbox_free(sender->snd_mb);
  free(sender);
}

LIBETPAN_EXPORT
struct mailimf_reply_to *
mailimf_reply_to_new(struct mailimf_address_list * rt_addr_list)
{
  struct mailimf_reply_to * reply_to;

  reply_to = malloc(sizeof(* reply_to));
  if (reply_to == NULL)
    return NULL;

  reply_to->rt_addr_list = rt_addr_list;

  return reply_to;
}

LIBETPAN_EXPORT
void mailimf_reply_to_free(struct mailimf_reply_to * reply_to)
{
  if (reply_to->rt_addr_list != NULL)
    mailimf_address_list_free(reply_to->rt_addr_list);
  free(reply_to);
}

LIBETPAN_EXPORT
struct mailimf_to * mailimf_to_new(struct mailimf_address_list * to_addr_list)
{
  struct mailimf_to * to;

  to = malloc(sizeof(* to));
  if (to == NULL)
    return NULL;

  to->to_addr_list = to_addr_list;

  return to;
}

LIBETPAN_EXPORT
void mailimf_to_free(struct mailimf_to * to)
{
  if (to->to_addr_list != NULL)
    mailimf_address_list_free(to->to_addr_list);
  free(to);
}

LIBETPAN_EXPORT
struct mailimf_cc * mailimf_cc_new(struct mailimf_address_list * cc_addr_list)
{
  struct mailimf_cc * cc;

  cc = malloc(sizeof(* cc));
  if (cc == NULL)
    return NULL;

  cc->cc_addr_list = cc_addr_list;

  return cc;
}

LIBETPAN_EXPORT
void mailimf_cc_free(struct mailimf_cc * cc)
{
  if (cc->cc_addr_list != NULL)
    mailimf_address_list_free(cc->cc_addr_list);
  free(cc);
}

LIBETPAN_EXPORT
struct mailimf_bcc *
mailimf_bcc_new(struct mailimf_address_list * bcc_addr_list)
{
  struct mailimf_bcc * bcc;

  bcc = malloc(sizeof(* bcc));
  if (bcc == NULL)
    return NULL;

  bcc->bcc_addr_list = bcc_addr_list;

  return bcc;
}

LIBETPAN_EXPORT
void mailimf_bcc_free(struct mailimf_bcc * bcc)
{
  if (bcc->bcc_addr_list != NULL)
    mailimf_address_list_free(bcc->bcc_addr_list);
  free(bcc);
}

LIBETPAN_EXPORT
struct mailimf_message_id * mailimf_message_id_new(char * mid_value)
{
  struct mailimf_message_id * message_id;

  message_id = malloc(sizeof(* message_id));
  if (message_id == NULL)
    return NULL;

  message_id->mid_value = mid_value;

  return message_id;
}

LIBETPAN_EXPORT
void mailimf_message_id_free(struct mailimf_message_id * message_id)
{
  if (message_id->mid_value != NULL)
    mailimf_msg_id_free(message_id->mid_value);
  free(message_id);
}

LIBETPAN_EXPORT
struct mailimf_in_reply_to * mailimf_in_reply_to_new(clist * mid_list)
{
  struct mailimf_in_reply_to * in_reply_to;

  in_reply_to = malloc(sizeof(* in_reply_to));
  if (in_reply_to == NULL)
    return NULL;

  in_reply_to->mid_list = mid_list;

  return in_reply_to;
}

LIBETPAN_EXPORT
void mailimf_in_reply_to_free(struct mailimf_in_reply_to * in_reply_to)
{
  clist_foreach(in_reply_to->mid_list,
		(clist_func) mailimf_msg_id_free, NULL);
  clist_free(in_reply_to->mid_list);
  free(in_reply_to);
}

LIBETPAN_EXPORT
struct mailimf_references * mailimf_references_new(clist * mid_list)
{
  struct mailimf_references * ref;

  ref = malloc(sizeof(* ref));
  if (ref == NULL)
    return NULL;

  ref->mid_list = mid_list;

  return ref;
}

LIBETPAN_EXPORT
void mailimf_references_free(struct mailimf_references * references)
{
  clist_foreach(references->mid_list,
      (clist_func) mailimf_msg_id_free, NULL);
  clist_free(references->mid_list);
  free(references);
}

void mailimf_msg_id_free(char * msg_id)
{
  free(msg_id);
}

void mailimf_id_left_free(char * id_left)
{
  free(id_left);
}

void mailimf_id_right_free(char * id_right)
{
  free(id_right);
}

void mailimf_no_fold_quote_free(char * nfq)
{
  free(nfq);
}

void mailimf_no_fold_literal_free(char * nfl)
{
  free(nfl);
}

LIBETPAN_EXPORT
struct mailimf_subject * mailimf_subject_new(char * sbj_value)
{
  struct mailimf_subject * subject;

  subject = malloc(sizeof(* subject));
  if (subject == NULL)
    return NULL;

  subject->sbj_value = sbj_value;

  return subject;
}

LIBETPAN_EXPORT
void mailimf_subject_free(struct mailimf_subject * subject)
{
  mailimf_unstructured_free(subject->sbj_value);
  free(subject);
}

LIBETPAN_EXPORT
struct mailimf_comments * mailimf_comments_new(char * cm_value)
{
  struct mailimf_comments * comments;

  comments = malloc(sizeof(* comments));
  if (comments == NULL)
    return NULL;

  comments->cm_value = cm_value;

  return comments;
}

LIBETPAN_EXPORT
void mailimf_comments_free(struct mailimf_comments * comments)
{
  mailimf_unstructured_free(comments->cm_value);
  free(comments);
}

LIBETPAN_EXPORT
struct mailimf_keywords * mailimf_keywords_new(clist * kw_list)
{
  struct mailimf_keywords * keywords;

  keywords = malloc(sizeof(* keywords));
  if (keywords == NULL)
    return NULL;

  keywords->kw_list = kw_list;
  
  return keywords;
}

LIBETPAN_EXPORT
void mailimf_keywords_free(struct mailimf_keywords * keywords)
{
  clist_foreach(keywords->kw_list, (clist_func) mailimf_phrase_free, NULL);
  clist_free(keywords->kw_list);
  free(keywords);
}

LIBETPAN_EXPORT
struct mailimf_return *
mailimf_return_new(struct mailimf_path * ret_path)
{
  struct mailimf_return * return_path;

  return_path = malloc(sizeof(* return_path));
  if (return_path == NULL)
    return NULL;

  return_path->ret_path = ret_path;

  return return_path;
}

LIBETPAN_EXPORT
void mailimf_return_free(struct mailimf_return * return_path)
{
  mailimf_path_free(return_path->ret_path);
  free(return_path);
}

LIBETPAN_EXPORT
struct mailimf_path * mailimf_path_new(char * pt_addr_spec)
{
  struct mailimf_path * path;

  path = malloc(sizeof(* path));
  if (path == NULL)
    return NULL;

  path->pt_addr_spec = pt_addr_spec;

  return path;
}

LIBETPAN_EXPORT
void mailimf_path_free(struct mailimf_path * path)
{
  if (path->pt_addr_spec != NULL)
    mailimf_addr_spec_free(path->pt_addr_spec);
  free(path);
}

LIBETPAN_EXPORT
struct mailimf_optional_field *
mailimf_optional_field_new(char * fld_name, char * fld_value)
{
  struct mailimf_optional_field * opt_field;

  opt_field = malloc(sizeof(* opt_field));
  if (opt_field == NULL)
    return NULL;
  
  opt_field->fld_name = fld_name;
  opt_field->fld_value = fld_value;

  return opt_field;
}

LIBETPAN_EXPORT
void mailimf_optional_field_free(struct mailimf_optional_field * opt_field)
{
  mailimf_field_name_free(opt_field->fld_name);
  mailimf_unstructured_free(opt_field->fld_value);
  free(opt_field);
}

void mailimf_field_name_free(char * field_name)
{
  free(field_name);
}
