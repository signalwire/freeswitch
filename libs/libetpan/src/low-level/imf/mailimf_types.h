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
 * $Id: mailimf_types.h,v 1.34 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef MAILIMF_TYPES_H

#define MAILIMF_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>
#include <sys/types.h>

/*
  IMPORTANT NOTE:
  
  All allocation functions will take as argument allocated data
  and will store these data in the structure they will allocate.
  Data should be persistant during all the use of the structure
  and will be freed by the free function of the structure

  allocation functions will return NULL on failure
*/

/*
  mailimf_date_time is a date
  
  - day is the day of month (1 to 31)

  - month (1 to 12)

  - year (4 digits)

  - hour (0 to 23)

  - min (0 to 59)

  - sec (0 to 59)

  - zone (this is the decimal value that we can read, for example:
    for "-0200", the value is -200)
*/

struct mailimf_date_time {
  int dt_day;
  int dt_month;
  int dt_year;
  int dt_hour;
  int dt_min;
  int dt_sec;
  int dt_zone;
};

LIBETPAN_EXPORT
struct mailimf_date_time *
mailimf_date_time_new(int dt_day, int dt_month, int dt_year,
    int dt_hour, int dt_min, int dt_sec, int dt_zone);

LIBETPAN_EXPORT
void mailimf_date_time_free(struct mailimf_date_time * date_time);



/* this is the type of address */

enum {
  MAILIMF_ADDRESS_ERROR,   /* on parse error */
  MAILIMF_ADDRESS_MAILBOX, /* if this is a mailbox (mailbox@domain) */
  MAILIMF_ADDRESS_GROUP    /* if this is a group
                              (group_name: address1@domain1,
                                  address2@domain2; ) */
};

/*
  mailimf_address is an address

  - type can be MAILIMF_ADDRESS_MAILBOX or MAILIMF_ADDRESS_GROUP

  - mailbox is a mailbox if type is MAILIMF_ADDRESS_MAILBOX

  - group is a group if type is MAILIMF_ADDRESS_GROUP
*/

struct mailimf_address {
  int ad_type;
  union {
    struct mailimf_mailbox * ad_mailbox; /* can be NULL */
    struct mailimf_group * ad_group;     /* can be NULL */
  } ad_data;
};

LIBETPAN_EXPORT
struct mailimf_address *
mailimf_address_new(int ad_type, struct mailimf_mailbox * ad_mailbox,
    struct mailimf_group * ad_group);

LIBETPAN_EXPORT
void mailimf_address_free(struct mailimf_address * address);



/*
  mailimf_mailbox is a mailbox

  - display_name is the name that will be displayed for this mailbox,
    for example 'name' in '"name" <mailbox@domain>,
    should be allocated with malloc()
  
  - addr_spec is the mailbox, for example 'mailbox@domain'
    in '"name" <mailbox@domain>, should be allocated with malloc()
*/

struct mailimf_mailbox {
  char * mb_display_name; /* can be NULL */
  char * mb_addr_spec;    /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_mailbox *
mailimf_mailbox_new(char * mb_display_name, char * mb_addr_spec);

LIBETPAN_EXPORT
void mailimf_mailbox_free(struct mailimf_mailbox * mailbox);



/*
  mailimf_group is a group

  - display_name is the name that will be displayed for this group,
    for example 'group_name' in
    'group_name: address1@domain1, address2@domain2;', should be allocated
    with malloc()

  - mb_list is a list of mailboxes
*/

struct mailimf_group {
  char * grp_display_name; /* != NULL */
  struct mailimf_mailbox_list * grp_mb_list; /* can be NULL */
};

LIBETPAN_EXPORT
struct mailimf_group *
mailimf_group_new(char * grp_display_name,
    struct mailimf_mailbox_list * grp_mb_list);

LIBETPAN_EXPORT
void mailimf_group_free(struct mailimf_group * group);



/*
  mailimf_mailbox_list is a list of mailboxes

  - list is a list of mailboxes
*/

struct mailimf_mailbox_list {
  clist * mb_list; /* list of (struct mailimf_mailbox *), != NULL */
};

LIBETPAN_EXPORT
struct mailimf_mailbox_list *
mailimf_mailbox_list_new(clist * mb_list);

LIBETPAN_EXPORT
void mailimf_mailbox_list_free(struct mailimf_mailbox_list * mb_list);



/*
  mailimf_address_list is a list of addresses

  - list is a list of addresses
*/

struct mailimf_address_list {
  clist * ad_list; /* list of (struct mailimf_address *), != NULL */
};

LIBETPAN_EXPORT
struct mailimf_address_list *
mailimf_address_list_new(clist * ad_list);

LIBETPAN_EXPORT
void mailimf_address_list_free(struct mailimf_address_list * addr_list);





/*
  mailimf_body is the text part of a message
  
  - text is the beginning of the text part, it is a substring
    of an other string

  - size is the size of the text part
*/

struct mailimf_body {
  const char * bd_text; /* != NULL */
  size_t bd_size;
};

LIBETPAN_EXPORT
struct mailimf_body * mailimf_body_new(const char * bd_text, size_t bd_size);

LIBETPAN_EXPORT
void mailimf_body_free(struct mailimf_body * body);




/*
  mailimf_message is the content of the message

  - msg_fields is the header fields of the message
  
  - msg_body is the text part of the message
*/

struct mailimf_message {
  struct mailimf_fields * msg_fields; /* != NULL */
  struct mailimf_body * msg_body;     /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_message *
mailimf_message_new(struct mailimf_fields * msg_fields,
    struct mailimf_body * msg_body);

LIBETPAN_EXPORT
void mailimf_message_free(struct mailimf_message * message);




/*
  mailimf_fields is a list of header fields

  - fld_list is a list of header fields
*/

struct mailimf_fields {
  clist * fld_list; /* list of (struct mailimf_field *), != NULL */
};

LIBETPAN_EXPORT
struct mailimf_fields * mailimf_fields_new(clist * fld_list);

LIBETPAN_EXPORT
void mailimf_fields_free(struct mailimf_fields * fields);



/* this is a type of field */

enum {
  MAILIMF_FIELD_NONE,           /* on parse error */
  MAILIMF_FIELD_RETURN_PATH,    /* Return-Path */
  MAILIMF_FIELD_RESENT_DATE,    /* Resent-Date */
  MAILIMF_FIELD_RESENT_FROM,    /* Resent-From */
  MAILIMF_FIELD_RESENT_SENDER,  /* Resent-Sender */
  MAILIMF_FIELD_RESENT_TO,      /* Resent-To */
  MAILIMF_FIELD_RESENT_CC,      /* Resent-Cc */
  MAILIMF_FIELD_RESENT_BCC,     /* Resent-Bcc */
  MAILIMF_FIELD_RESENT_MSG_ID,  /* Resent-Message-ID */
  MAILIMF_FIELD_ORIG_DATE,      /* Date */
  MAILIMF_FIELD_FROM,           /* From */
  MAILIMF_FIELD_SENDER,         /* Sender */
  MAILIMF_FIELD_REPLY_TO,       /* Reply-To */
  MAILIMF_FIELD_TO,             /* To */
  MAILIMF_FIELD_CC,             /* Cc */
  MAILIMF_FIELD_BCC,            /* Bcc */
  MAILIMF_FIELD_MESSAGE_ID,     /* Message-ID */
  MAILIMF_FIELD_IN_REPLY_TO,    /* In-Reply-To */
  MAILIMF_FIELD_REFERENCES,     /* References */
  MAILIMF_FIELD_SUBJECT,        /* Subject */
  MAILIMF_FIELD_COMMENTS,       /* Comments */
  MAILIMF_FIELD_KEYWORDS,       /* Keywords */
  MAILIMF_FIELD_OPTIONAL_FIELD  /* other field */
};

/*
  mailimf_field is a field

  - fld_type is the type of the field

  - fld_data.fld_return_path is the parsed content of the Return-Path
    field if type is MAILIMF_FIELD_RETURN_PATH

  - fld_data.fld_resent_date is the parsed content of the Resent-Date field
    if type is MAILIMF_FIELD_RESENT_DATE

  - fld_data.fld_resent_from is the parsed content of the Resent-From field

  - fld_data.fld_resent_sender is the parsed content of the Resent-Sender field

  - fld_data.fld_resent_to is the parsed content of the Resent-To field

  - fld_data.fld_resent_cc is the parsed content of the Resent-Cc field

  - fld_data.fld_resent_bcc is the parsed content of the Resent-Bcc field

  - fld_data.fld_resent_msg_id is the parsed content of the Resent-Message-ID
    field

  - fld_data.fld_orig_date is the parsed content of the Date field

  - fld_data.fld_from is the parsed content of the From field

  - fld_data.fld_sender is the parsed content of the Sender field

  - fld_data.fld_reply_to is the parsed content of the Reply-To field

  - fld_data.fld_to is the parsed content of the To field

  - fld_data.fld_cc is the parsed content of the Cc field

  - fld_data.fld_bcc is the parsed content of the Bcc field

  - fld_data.fld_message_id is the parsed content of the Message-ID field

  - fld_data.fld_in_reply_to is the parsed content of the In-Reply-To field

  - fld_data.fld_references is the parsed content of the References field

  - fld_data.fld_subject is the content of the Subject field

  - fld_data.fld_comments is the content of the Comments field

  - fld_data.fld_keywords is the parsed content of the Keywords field

  - fld_data.fld_optional_field is an other field and is not parsed
*/

#define LIBETPAN_MAILIMF_FIELD_UNION

struct mailimf_field {
  int fld_type;
  union {
    struct mailimf_return * fld_return_path;              /* can be NULL */
    struct mailimf_orig_date * fld_resent_date;    /* can be NULL */
    struct mailimf_from * fld_resent_from;         /* can be NULL */
    struct mailimf_sender * fld_resent_sender;     /* can be NULL */
    struct mailimf_to * fld_resent_to;             /* can be NULL */
    struct mailimf_cc * fld_resent_cc;             /* can be NULL */
    struct mailimf_bcc * fld_resent_bcc;           /* can be NULL */
    struct mailimf_message_id * fld_resent_msg_id; /* can be NULL */
    struct mailimf_orig_date * fld_orig_date;             /* can be NULL */
    struct mailimf_from * fld_from;                       /* can be NULL */
    struct mailimf_sender * fld_sender;                   /* can be NULL */
    struct mailimf_reply_to * fld_reply_to;               /* can be NULL */
    struct mailimf_to * fld_to;                           /* can be NULL */
    struct mailimf_cc * fld_cc;                           /* can be NULL */
    struct mailimf_bcc * fld_bcc;                         /* can be NULL */
    struct mailimf_message_id * fld_message_id;           /* can be NULL */
    struct mailimf_in_reply_to * fld_in_reply_to;         /* can be NULL */
    struct mailimf_references * fld_references;           /* can be NULL */
    struct mailimf_subject * fld_subject;                 /* can be NULL */
    struct mailimf_comments * fld_comments;               /* can be NULL */
    struct mailimf_keywords * fld_keywords;               /* can be NULL */
    struct mailimf_optional_field * fld_optional_field;   /* can be NULL */
  } fld_data;
};

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
    struct mailimf_optional_field * fld_optional_field);

LIBETPAN_EXPORT
void mailimf_field_free(struct mailimf_field * field);



/*
  mailimf_orig_date is the parsed Date field

  - date_time is the parsed date
*/

struct mailimf_orig_date {
  struct mailimf_date_time * dt_date_time; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_orig_date * mailimf_orig_date_new(struct mailimf_date_time *
    dt_date_time);

LIBETPAN_EXPORT
void mailimf_orig_date_free(struct mailimf_orig_date * orig_date);




/*
  mailimf_from is the parsed From field

  - mb_list is the parsed mailbox list
*/

struct mailimf_from {
  struct mailimf_mailbox_list * frm_mb_list; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_from *
mailimf_from_new(struct mailimf_mailbox_list * frm_mb_list);

LIBETPAN_EXPORT
void mailimf_from_free(struct mailimf_from * from);



/*
  mailimf_sender is the parsed Sender field

  - snd_mb is the parsed mailbox
*/

struct mailimf_sender {
  struct mailimf_mailbox * snd_mb; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_sender * mailimf_sender_new(struct mailimf_mailbox * snd_mb);

LIBETPAN_EXPORT
void mailimf_sender_free(struct mailimf_sender * sender);




/*
  mailimf_reply_to is the parsed Reply-To field

  - rt_addr_list is the parsed address list
 */

struct mailimf_reply_to {
  struct mailimf_address_list * rt_addr_list; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_reply_to *
mailimf_reply_to_new(struct mailimf_address_list * rt_addr_list);

LIBETPAN_EXPORT
void mailimf_reply_to_free(struct mailimf_reply_to * reply_to);




/*
  mailimf_to is the parsed To field
  
  - to_addr_list is the parsed address list
*/

struct mailimf_to {
  struct mailimf_address_list * to_addr_list; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_to * mailimf_to_new(struct mailimf_address_list * to_addr_list);

LIBETPAN_EXPORT
void mailimf_to_free(struct mailimf_to * to);




/*
  mailimf_cc is the parsed Cc field

  - cc_addr_list is the parsed addres list
*/

struct mailimf_cc {
  struct mailimf_address_list * cc_addr_list; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_cc * mailimf_cc_new(struct mailimf_address_list * cc_addr_list);

LIBETPAN_EXPORT
void mailimf_cc_free(struct mailimf_cc * cc);




/*
  mailimf_bcc is the parsed Bcc field

  - bcc_addr_list is the parsed addres list
*/

struct mailimf_bcc {
  struct mailimf_address_list * bcc_addr_list; /* can be NULL */
};

LIBETPAN_EXPORT
struct mailimf_bcc *
mailimf_bcc_new(struct mailimf_address_list * bcc_addr_list);

LIBETPAN_EXPORT
void mailimf_bcc_free(struct mailimf_bcc * bcc);



/*
  mailimf_message_id is the parsed Message-ID field
  
  - mid_value is the message identifier
*/

struct mailimf_message_id {
  char * mid_value; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_message_id * mailimf_message_id_new(char * mid_value);

LIBETPAN_EXPORT
void mailimf_message_id_free(struct mailimf_message_id * message_id);




/*
  mailimf_in_reply_to is the parsed In-Reply-To field

  - mid_list is the list of message identifers
*/

struct mailimf_in_reply_to {
  clist * mid_list; /* list of (char *), != NULL */
};

LIBETPAN_EXPORT
struct mailimf_in_reply_to * mailimf_in_reply_to_new(clist * mid_list);

LIBETPAN_EXPORT
void mailimf_in_reply_to_free(struct mailimf_in_reply_to * in_reply_to);



/*
  mailimf_references is the parsed References field

  - msg_id_list is the list of message identifiers
 */

struct mailimf_references {
  clist * mid_list; /* list of (char *) */
       /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_references * mailimf_references_new(clist * mid_list);

LIBETPAN_EXPORT
void mailimf_references_free(struct mailimf_references * references);



/*
  mailimf_subject is the parsed Subject field
  
  - sbj_value is the value of the field
*/

struct mailimf_subject {
  char * sbj_value; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_subject * mailimf_subject_new(char * sbj_value);

LIBETPAN_EXPORT
void mailimf_subject_free(struct mailimf_subject * subject);


/*
  mailimf_comments is the parsed Comments field

  - cm_value is the value of the field
*/

struct mailimf_comments {
  char * cm_value; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_comments * mailimf_comments_new(char * cm_value);

LIBETPAN_EXPORT
void mailimf_comments_free(struct mailimf_comments * comments);


/*
  mailimf_keywords is the parsed Keywords field

  - kw_list is the list of keywords
*/

struct mailimf_keywords {
  clist * kw_list; /* list of (char *), != NULL */
};

LIBETPAN_EXPORT
struct mailimf_keywords * mailimf_keywords_new(clist * kw_list);

LIBETPAN_EXPORT
void mailimf_keywords_free(struct mailimf_keywords * keywords);


/*
  mailimf_return is the parsed Return-Path field

  - ret_path is the parsed value of Return-Path
*/

struct mailimf_return {
  struct mailimf_path * ret_path; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_return *
mailimf_return_new(struct mailimf_path * ret_path);

LIBETPAN_EXPORT
void mailimf_return_free(struct mailimf_return * return_path);


/*
  mailimf_path is the parsed value of Return-Path

  - pt_addr_spec is a mailbox
*/

struct mailimf_path {
  char * pt_addr_spec; /* can be NULL */
};

LIBETPAN_EXPORT
struct mailimf_path * mailimf_path_new(char * pt_addr_spec);

LIBETPAN_EXPORT
void mailimf_path_free(struct mailimf_path * path);


/*
  mailimf_optional_field is a non-parsed field

  - fld_name is the name of the field

  - fld_value is the value of the field
*/

struct mailimf_optional_field {
  char * fld_name;  /* != NULL */
  char * fld_value; /* != NULL */
};

LIBETPAN_EXPORT
struct mailimf_optional_field *
mailimf_optional_field_new(char * fld_name, char * fld_value);

LIBETPAN_EXPORT
void mailimf_optional_field_free(struct mailimf_optional_field * opt_field);


/*
  mailimf_fields is the native structure that IMF module will use,
  this module will provide an easier structure to use when parsing fields.

  mailimf_single_fields is an easier structure to get parsed fields,
  rather than iteration over the list of fields

  - fld_orig_date is the parsed "Date" field

  - fld_from is the parsed "From" field
  
  - fld_sender is the parsed "Sender "field

  - fld_reply_to is the parsed "Reply-To" field
  
  - fld_to is the parsed "To" field

  - fld_cc is the parsed "Cc" field

  - fld_bcc is the parsed "Bcc" field

  - fld_message_id is the parsed "Message-ID" field

  - fld_in_reply_to is the parsed "In-Reply-To" field

  - fld_references is the parsed "References" field

  - fld_subject is the parsed "Subject" field
  
  - fld_comments is the parsed "Comments" field

  - fld_keywords is the parsed "Keywords" field
*/

struct mailimf_single_fields {
  struct mailimf_orig_date * fld_orig_date;      /* can be NULL */
  struct mailimf_from * fld_from;                /* can be NULL */
  struct mailimf_sender * fld_sender;            /* can be NULL */
  struct mailimf_reply_to * fld_reply_to;        /* can be NULL */
  struct mailimf_to * fld_to;                    /* can be NULL */
  struct mailimf_cc * fld_cc;                    /* can be NULL */
  struct mailimf_bcc * fld_bcc;                  /* can be NULL */
  struct mailimf_message_id * fld_message_id;    /* can be NULL */
  struct mailimf_in_reply_to * fld_in_reply_to;  /* can be NULL */
  struct mailimf_references * fld_references;    /* can be NULL */
  struct mailimf_subject * fld_subject;          /* can be NULL */
  struct mailimf_comments * fld_comments;        /* can be NULL */
  struct mailimf_keywords * fld_keywords;        /* can be NULL */
};






/* internal use */

void mailimf_atom_free(char * atom);

void mailimf_dot_atom_free(char * dot_atom);

void mailimf_dot_atom_text_free(char * dot_atom);

void mailimf_quoted_string_free(char * quoted_string);

void mailimf_word_free(char * word);

void mailimf_phrase_free(char * phrase);

void mailimf_unstructured_free(char * unstructured);

void mailimf_angle_addr_free(char * angle_addr);

void mailimf_display_name_free(char * display_name);

void mailimf_addr_spec_free(char * addr_spec);

void mailimf_local_part_free(char * local_part);

void mailimf_domain_free(char * domain);

void mailimf_domain_literal_free(char * domain);

void mailimf_msg_id_free(char * msg_id);

void mailimf_id_left_free(char * id_left);

void mailimf_id_right_free(char * id_right);

void mailimf_no_fold_quote_free(char * nfq);

void mailimf_no_fold_literal_free(char * nfl);

void mailimf_field_name_free(char * field_name);



/* these are the possible returned error codes */

enum {
  MAILIMF_NO_ERROR = 0,
  MAILIMF_ERROR_PARSE,
  MAILIMF_ERROR_MEMORY,
  MAILIMF_ERROR_INVAL,
  MAILIMF_ERROR_FILE
};


#ifdef __cplusplus
}
#endif

#endif
