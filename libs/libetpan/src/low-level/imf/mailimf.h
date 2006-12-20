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
 * $Id: mailimf.h,v 1.25 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef MAILIMF_H

#define MAILIMF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailimf_types.h>
#include <libetpan/mailimf_write_generic.h>
#include <libetpan/mailimf_write_file.h>
#include <libetpan/mailimf_write_mem.h>
#include <libetpan/mailimf_types_helper.h>

#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#	include <sys/types.h>
#endif

/*
  mailimf_message_parse will parse the given message
  
  @param message this is a string containing the message content
  @param length this is the size of the given string
  @param index this is a pointer to the start of the message in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int mailimf_message_parse(const char * message, size_t length,
			  size_t * index,
			  struct mailimf_message ** result);

/*
  mailimf_body_parse will parse the given text part of a message
  
  @param message this is a string containing the message text part
  @param length this is the size of the given string
  @param index this is a pointer to the start of the message text part in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int mailimf_body_parse(const char * message, size_t length,
		       size_t * index,
		       struct mailimf_body ** result);

/*
  mailimf_fields_parse will parse the given header fields
  
  @param message this is a string containing the header fields
  @param length this is the size of the given string
  @param index this is a pointer to the start of the header fields in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int mailimf_fields_parse(const char * message, size_t length,
			 size_t * index,
			 struct mailimf_fields ** result);

/*
  mailimf_mailbox_list_parse will parse the given mailbox list
  
  @param message this is a string containing the mailbox list
  @param length this is the size of the given string
  @param index this is a pointer to the start of the mailbox list in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int
mailimf_mailbox_list_parse(const char * message, size_t length,
			   size_t * index,
			   struct mailimf_mailbox_list ** result);

/*
  mailimf_address_list_parse will parse the given address list
  
  @param message this is a string containing the address list
  @param length this is the size of the given string
  @param index this is a pointer to the start of the address list in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int
mailimf_address_list_parse(const char * message, size_t length,
			   size_t * index,
			   struct mailimf_address_list ** result);

/*
  mailimf_address_parse will parse the given address
  
  @param message this is a string containing the address
  @param length this is the size of the given string
  @param index this is a pointer to the start of the address in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int mailimf_address_parse(const char * message, size_t length,
			  size_t * index,
			  struct mailimf_address ** result);

/*
  mailimf_mailbox_parse will parse the given address
  
  @param message this is a string containing the mailbox
  @param length this is the size of the given string
  @param index this is a pointer to the start of the mailbox in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int mailimf_mailbox_parse(const char * message, size_t length,
			  size_t * index,
			  struct mailimf_mailbox ** result);

/*
  mailimf_date_time_parse will parse the given RFC 2822 date
  
  @param message this is a string containing the date
  @param length this is the size of the given string
  @param index this is a pointer to the start of the date in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int mailimf_date_time_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailimf_date_time ** result);

/*
  mailimf_envelope_fields_parse will parse the given fields (Date,
  From, Sender, Reply-To, To, Cc, Bcc, Message-ID, In-Reply-To,
  References and Subject)
  
  @param message this is a string containing the header fields
  @param length this is the size of the given string
  @param index this is a pointer to the start of the header fields in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int mailimf_envelope_fields_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_fields ** result);

/*
  mailimf_ignore_field_parse will skip the given field
  
  @param message this is a string containing the header field
  @param length this is the size of the given string
  @param index this is a pointer to the start of the header field in
    the given string, (* index) is modified to point at the end
    of the parsed data

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/

LIBETPAN_EXPORT
int mailimf_ignore_field_parse(const char * message, size_t length,
			       size_t * index);

/*
  mailimf_envelope_fields will parse the given fields (Date,
  From, Sender, Reply-To, To, Cc, Bcc, Message-ID, In-Reply-To,
  References and Subject), other fields will be added as optional
  fields.
  
  @param message this is a string containing the header fields
  @param length this is the size of the given string
  @param index this is a pointer to the start of the header fields in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/

LIBETPAN_EXPORT
int
mailimf_envelope_and_optional_fields_parse(const char * message, size_t length,
					   size_t * index,
					   struct mailimf_fields ** result);

/*
  mailimf_envelope_fields will parse the given fields as optional
  fields.
  
  @param message this is a string containing the header fields
  @param length this is the size of the given string
  @param index this is a pointer to the start of the header fields in
    the given string, (* index) is modified to point at the end
    of the parsed data
  @param result the result of the parse operation is stored in
    (* result)

  @return MAILIMF_NO_ERROR on success, MAILIMF_ERROR_XXX on error
*/
LIBETPAN_EXPORT
int
mailimf_optional_fields_parse(const char * message, size_t length,
			      size_t * index,
			      struct mailimf_fields ** result);


/* internal use, exported for MIME */

int mailimf_fws_parse(const char * message, size_t length, size_t * index);

int mailimf_cfws_parse(const char * message, size_t length,
		       size_t * index);

int mailimf_char_parse(const char * message, size_t length,
		       size_t * index, char token);

int mailimf_unstrict_char_parse(const char * message, size_t length,
				size_t * index, char token);

int mailimf_crlf_parse(const char * message, size_t length, size_t * index);

int
mailimf_custom_string_parse(const char * message, size_t length,
			    size_t * index, char ** result,
			    int (* is_custom_char)(char));

int
mailimf_token_case_insensitive_len_parse(const char * message, size_t length,
					 size_t * index, char * token,
					 size_t token_length);

#define mailimf_token_case_insensitive_parse(message, length, index, token) \
    mailimf_token_case_insensitive_len_parse(message, length, index, token, \
					     sizeof(token) - 1)

int mailimf_quoted_string_parse(const char * message, size_t length,
				size_t * index, char ** result);

int
mailimf_number_parse(const char * message, size_t length,
		     size_t * index, uint32_t * result);

int mailimf_msg_id_parse(const char * message, size_t length,
			 size_t * index,
			 char ** result);

int mailimf_msg_id_list_parse(const char * message, size_t length,
			      size_t * index, clist ** result);

int mailimf_word_parse(const char * message, size_t length,
		       size_t * index, char ** result);

int mailimf_atom_parse(const char * message, size_t length,
		       size_t * index, char ** result);

int mailimf_fws_atom_parse(const char * message, size_t length,
			   size_t * index, char ** result);

int mailimf_fws_word_parse(const char * message, size_t length,
			   size_t * index, char ** result);

int mailimf_fws_quoted_string_parse(const char * message, size_t length,
				    size_t * index, char ** result);

/* exported for IMAP */

int mailimf_references_parse(const char * message, size_t length,
			     size_t * index,
			     struct mailimf_references ** result);

#ifdef __cplusplus
}
#endif

#endif
