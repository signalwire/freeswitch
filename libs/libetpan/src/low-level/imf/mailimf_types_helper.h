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
 * $Id: mailimf_types_helper.h,v 1.13 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef MAILIMF_TYPES_HELPER

#define MAILIMF_TYPES_HELPER

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailimf_types.h>

/*
  IMPORTANT NOTE:
  
  All allocation functions will take as argument allocated data
  and will store these data in the structure they will allocate.
  Data should be persistant during all the use of the structure
  and will be freed by the free function of the structure

  allocation functions will return NULL on failure
*/

/*
  mailimf_mailbox_list_new_empty creates an empty list of mailboxes
*/

struct mailimf_mailbox_list *
mailimf_mailbox_list_new_empty(void);

/*
  mailimf_mailbox_list_add adds a mailbox to the list of mailboxes

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimf_mailbox_list_add(struct mailimf_mailbox_list * mailbox_list,
			     struct mailimf_mailbox * mb);

/*
  mailimf_mailbox_list_add_parse parse the given string
  into a mailimf_mailbox structure and adds it to the list of mailboxes

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimf_mailbox_list_add_parse(struct mailimf_mailbox_list * mailbox_list,
				   char * mb_str);

/*
  mailimf_mailbox creates a mailimf_mailbox structure with the given
  arguments and adds it to the list of mailboxes

  - display_name is the name that will be displayed for this mailbox,
    for example 'name' in '"name" <mailbox@domain>,
    should be allocated with malloc()
  
  - address is the mailbox, for example 'mailbox@domain'
    in '"name" <mailbox@domain>, should be allocated with malloc()

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimf_mailbox_list_add_mb(struct mailimf_mailbox_list * mailbox_list,
				char * display_name, char * address);

/*
  mailimf_address_list_new_empty creates an empty list of addresses
*/

struct mailimf_address_list *
mailimf_address_list_new_empty(void);

/*
  mailimf_address_list_add adds a mailbox to the list of addresses

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimf_address_list_add(struct mailimf_address_list * address_list,
			     struct mailimf_address * addr);

/*
  mailimf_address_list_add_parse parse the given string
  into a mailimf_address structure and adds it to the list of addresses

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimf_address_list_add_parse(struct mailimf_address_list * address_list,
				   char * addr_str);

/*
  mailimf_address_list_add_mb creates a mailbox mailimf_address
  with the given arguments and adds it to the list of addresses

  - display_name is the name that will be displayed for this mailbox,
    for example 'name' in '"name" <mailbox@domain>,
    should be allocated with malloc()
  
  - address is the mailbox, for example 'mailbox@domain'
    in '"name" <mailbox@domain>, should be allocated with malloc()

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimf_address_list_add_mb(struct mailimf_address_list * address_list,
				char * display_name, char * address);

/*
  mailimf_resent_fields_add_data adds a set of resent fields in the
  given mailimf_fields structure.
  
  if you don't want a given field in the set to be added in the list
  of fields, you can give NULL as argument

  @param resent_msg_id sould be allocated with malloc()

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int
mailimf_resent_fields_add_data(struct mailimf_fields * fields,
    struct mailimf_date_time * resent_date,
    struct mailimf_mailbox_list * resent_from,
    struct mailimf_mailbox * resent_sender,
    struct mailimf_address_list * resent_to,
    struct mailimf_address_list * resent_cc,
    struct mailimf_address_list * resent_bcc,
    char * resent_msg_id);

/*
  mailimf_resent_fields_new_with_data_all creates a new mailimf_fields
  structure with a set of resent fields

  if you don't want a given field in the set to be added in the list
  of fields, you can give NULL as argument

  @param resent_msg_id sould be allocated with malloc()

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

struct mailimf_fields *
mailimf_resent_fields_new_with_data_all(struct mailimf_date_time *
    resent_date, struct mailimf_mailbox_list * resent_from,
    struct mailimf_mailbox * resent_sender,
    struct mailimf_address_list * resent_to,
    struct mailimf_address_list * resent_cc,
    struct mailimf_address_list * resent_bcc,
    char * resent_msg_id);

/*
  mailimf_resent_fields_new_with_data_all creates a new mailimf_fields
  structure with a set of resent fields.
  Resent-Date and Resent-Message-ID fields will be generated for you.

  if you don't want a given field in the set to be added in the list
  of fields, you can give NULL as argument

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

struct mailimf_fields *
mailimf_resent_fields_new_with_data(struct mailimf_mailbox_list * from,
    struct mailimf_mailbox * sender,
    struct mailimf_address_list * to,
    struct mailimf_address_list * cc,
    struct mailimf_address_list * bcc);

/*
  this function creates a new mailimf_fields structure with no fields
*/

struct mailimf_fields *
mailimf_fields_new_empty(void);


/*
  this function adds a field to the mailimf_fields structure

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimf_fields_add(struct mailimf_fields * fields,
		       struct mailimf_field * field);


/*
  mailimf_fields_add_data adds a set of fields in the
  given mailimf_fields structure.
  
  if you don't want a given field in the set to be added in the list
  of fields, you can give NULL as argument

  @param msg_id sould be allocated with malloc()
  @param subject should be allocated with malloc()
  @param in_reply_to each elements of this list should be allocated
    with malloc()
  @param references each elements of this list should be allocated
    with malloc()

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

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
			    char * subject);

/*
  mailimf_fields_new_with_data_all creates a new mailimf_fields
  structure with a set of fields

  if you don't want a given field in the set to be added in the list
  of fields, you can give NULL as argument

  @param message_id sould be allocated with malloc()
  @param subject should be allocated with malloc()
  @param in_reply_to each elements of this list should be allocated
    with malloc()
  @param references each elements of this list should be allocated
    with malloc()

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

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
				 char * subject);

/*
  mailimf_fields_new_with_data creates a new mailimf_fields
  structure with a set of fields
  Date and Message-ID fields will be generated for you.

  if you don't want a given field in the set to be added in the list
  of fields, you can give NULL as argument

  @param subject should be allocated with malloc()
  @param in_reply_to each elements of this list should be allocated
    with malloc()
  @param references each elements of this list should be allocated
    with malloc()

  @return MAILIMF_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

struct mailimf_fields *
mailimf_fields_new_with_data(struct mailimf_mailbox_list * from,
			     struct mailimf_mailbox * sender,
			     struct mailimf_address_list * reply_to,
			     struct mailimf_address_list * to,
			     struct mailimf_address_list * cc,
			     struct mailimf_address_list * bcc,
			     clist * in_reply_to,
			     clist * references,
			     char * subject);

/*
  this function returns an allocated message identifier to
  use in a Message-ID or Resent-Message-ID field
*/

char * mailimf_get_message_id(void);

/*
  this function returns a mailimf_date_time structure to
  use in a Date or Resent-Date field
*/

struct mailimf_date_time * mailimf_get_current_date(void);


/*
  mailimf_single_fields_init fills a mailimf_single_fields structure
  with the content of a mailimf_fields structure
*/

void mailimf_single_fields_init(struct mailimf_single_fields * single_fields,
                                struct mailimf_fields * fields);

/*
  mailimf_single_fields_new creates a new mailimf_single_fields and
  fills the structure with mailimf_fields
*/

struct mailimf_single_fields *
mailimf_single_fields_new(struct mailimf_fields * fields);

void mailimf_single_fields_free(struct mailimf_single_fields *
                                single_fields);

/*
  mailimf_field_new_custom creates a new field of type optional

  @param name should be allocated with malloc()
  @param value should be allocated with malloc()
*/

struct mailimf_field * mailimf_field_new_custom(char * name, char * value);

#ifdef __cplusplus
}
#endif

#endif
