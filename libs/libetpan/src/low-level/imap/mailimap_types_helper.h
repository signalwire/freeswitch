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
 * $Id: mailimap_types_helper.h,v 1.11 2004/11/21 21:53:37 hoa Exp $
 */

#ifndef MAILIMAP_TYPES_HELPER_H

#define MAILIMAP_TYPES_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailimap_types.h>

/*
  IMPORTANT NOTE:
  
  All allocation functions will take as argument allocated data
  and will store these data in the structure they will allocate.
  Data should be persistant during all the use of the structure
  and will be freed by the free function of the structure

  allocation functions will return NULL on failure
*/

/*
  this function creates a new set item with a single message
  given by index
*/

struct mailimap_set_item * mailimap_set_item_new_single(uint32_t index);

/*
  this function creates a new set with one set item
 */

struct mailimap_set *
mailimap_set_new_single_item(struct mailimap_set_item * item);

/*
  this function creates a set with a single interval
*/

struct mailimap_set * mailimap_set_new_interval(uint32_t first, uint32_t last);

/*
  this function creates a set with a single message
*/

struct mailimap_set * mailimap_set_new_single(uint32_t index);

/*
  this function creates an empty set of messages
*/

struct mailimap_set * mailimap_set_new_empty(void);

/*
  this function adds a set item to the set of messages

  @return MAILIMAP_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimap_set_add(struct mailimap_set * set,
		struct mailimap_set_item * set_item);

/*
  this function adds an interval to the set

  @return MAILIMAP_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimap_set_add_interval(struct mailimap_set * set,
		uint32_t first, uint32_t last);

/*
  this function adds a single message to the set

  @return MAILIMAP_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimap_set_add_single(struct mailimap_set * set,
			 uint32_t index);

/*
  this function creates a mailimap_section structure to request
  the header of a message
*/

struct mailimap_section * mailimap_section_new_header(void);

/*
  this functions creates a mailimap_section structure to describe
  a list of headers
*/

struct mailimap_section *
mailimap_section_new_header_fields(struct mailimap_header_list * header_list);

/*
  this functions creates a mailimap_section structure to describe headers
  other than those given
*/

struct mailimap_section *
mailimap_section_new_header_fields_not(struct mailimap_header_list * header_list);

/*
  this function creates a mailimap_section structure to describe the
  text of a message
 */

struct mailimap_section * mailimap_section_new_text(void);

/*
  this function creates a mailimap_section structure to describe the 
  content of a MIME part
*/

struct mailimap_section *
mailimap_section_new_part(struct mailimap_section_part * part);

/*
  this function creates a mailimap_section structure to describe the
  MIME fields of a MIME part
*/

struct mailimap_section *
mailimap_section_new_part_mime(struct mailimap_section_part * part);

/*
  this function creates a mailimap_section structure to describe the
  headers of a MIME part if the MIME type is a message/rfc822
*/

struct mailimap_section *
mailimap_section_new_part_header(struct mailimap_section_part * part);

/*
  this function creates a mailimap_section structure to describe
  a list of headers of a MIME part if the MIME type is a message/rfc822
*/

struct mailimap_section *
mailimap_section_new_part_header_fields(struct mailimap_section_part *
					part,
					struct mailimap_header_list *
					header_list);

/*
  this function creates a mailimap_section structure to describe
  headers of a MIME part other than those given if the MIME type
  is a message/rfc822
*/

struct mailimap_section *
mailimap_section_new_part_header_fields_not(struct mailimap_section_part
					    * part,
					    struct mailimap_header_list
					    * header_list);

/*
  this function creates a mailimap_section structure to describe
  text part of message if the MIME type is a message/rfc822
*/

struct mailimap_section *
mailimap_section_new_part_text(struct mailimap_section_part * part);


/*
  this function creates a mailimap_fetch_att structure to request
  envelope of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_envelope(void);


/*
  this function creates a mailimap_fetch_att structure to request
  flags of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_flags(void);

/*
  this function creates a mailimap_fetch_att structure to request
  internal date of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_internaldate(void);


/*
  this function creates a mailimap_fetch_att structure to request
  text part of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822(void);


/*
  this function creates a mailimap_fetch_att structure to request
  header of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822_header(void);

/*
  this function creates a mailimap_fetch_att structure to request
  size of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822_size(void);

/*
  this function creates a mailimap_fetch_att structure to request
  envelope of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822_text(void);

/*
  this function creates a mailimap_fetch_att structure to request
  the MIME structure of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_body(void);

/*
  this function creates a mailimap_fetch_att structure to request
  the MIME structure of a message and additional MIME information
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_bodystructure(void);

/*
  this function creates a mailimap_fetch_att structure to request
  unique identifier of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_uid(void);

/*
  this function creates a mailimap_fetch_att structure to request
  a given section of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_section(struct mailimap_section * section);

/*
  this function creates a mailimap_fetch_att structure to request
  a given section of a message without marking it as read
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_peek_section(struct mailimap_section * section);

/*
  this function creates a mailimap_fetch_att structure to request
  a part of a section of a message
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_section_partial(struct mailimap_section * section,
					    uint32_t offset, uint32_t size);

/*
  this function creates a mailimap_fetch_att structure to request
  a part of a section of a message without marking it as read
*/

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_peek_section_partial(struct mailimap_section * section,
						 uint32_t offset, uint32_t size);

/*
  this function creates a mailimap_fetch_type structure to request
  (FLAGS INTERNALDATE RFC822.SIZE ENVELOPE) of a message
*/

struct mailimap_fetch_type *
mailimap_fetch_type_new_all(void);

/*
  this function creates a mailimap_fetch_type structure to request
  (FLAGS INTERNALDATE RFC822.SIZE ENVELOPE BODY)
*/

struct mailimap_fetch_type *
mailimap_fetch_type_new_full(void);

/*
  this function creates a mailimap_fetch_type structure to request
  (FLAGS INTERNALDATE RFC822.SIZE)
*/

struct mailimap_fetch_type *
mailimap_fetch_type_new_fast(void);

/*
  this function creates a mailimap_fetch_type structure to request
  the given fetch attribute
*/

struct mailimap_fetch_type *
mailimap_fetch_type_new_fetch_att(struct mailimap_fetch_att * fetch_att);

/*
  this function creates a mailimap_fetch_type structure to request
  the list of fetch attributes
*/

struct mailimap_fetch_type *
mailimap_fetch_type_new_fetch_att_list(clist * fetch_att_list);

/*
  this function creates a mailimap_fetch_type structure
*/

struct mailimap_fetch_type *
mailimap_fetch_type_new_fetch_att_list_empty(void);

/*
  this function adds a given fetch attribute to the mailimap_fetch
  structure

  @return MAILIMAP_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int
mailimap_fetch_type_new_fetch_att_list_add(struct mailimap_fetch_type *
					   fetch_type,
					   struct mailimap_fetch_att *
					   fetch_att);

/*
  this function creates a store attribute to set the given flags
*/

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_set_flags(struct mailimap_flag_list * flags);

/*
  this function creates a store attribute to silently set the given flags
*/

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_set_flags_silent(struct mailimap_flag_list *
					      flags);

/*
  this function creates a store attribute to add the given flags
*/

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_add_flags(struct mailimap_flag_list * flags);

/*
  this function creates a store attribute to add silently the given flags
*/

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_add_flags_silent(struct mailimap_flag_list *
					      flags);

/*
  this function creates a store attribute to remove the given flags
*/

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_remove_flags(struct mailimap_flag_list * flags);

/*
  this function creates a store attribute to remove silently the given flags
*/

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_remove_flags_silent(struct mailimap_flag_list *
						 flags);


/*
  this function creates a condition structure to match all messages
*/

struct mailimap_search_key *
mailimap_search_key_new_all(void);

/*
  this function creates a condition structure to match messages with Bcc field
  
  @param bcc this is the content of Bcc to match, it should be allocated
    with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_bcc(char * sk_bcc);

/*
  this function creates a condition structure to match messages with
  internal date
*/

struct mailimap_search_key *
mailimap_search_key_new_before(struct mailimap_date * sk_before);

/*
  this function creates a condition structure to match messages with
  message content

  @param body this is the content of the message to match, it should
    be allocated with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_body(char * sk_body);

/*
  this function creates a condition structure to match messages with 
  Cc field

  
  @param cc this is the content of Cc to match, it should be allocated
    with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_cc(char * sk_cc);

/*
  this function creates a condition structure to match messages with 
  From field

  @param from this is the content of From to match, it should be allocated
    with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_from(char * sk_from);

/*
  this function creates a condition structure to match messages with 
  a flag given by keyword
*/

struct mailimap_search_key *
mailimap_search_key_new_keyword(char * sk_keyword);

/*
  this function creates a condition structure to match messages with
  internal date
*/

struct mailimap_search_key *
mailimap_search_key_new_on(struct mailimap_date * sk_on);

/*
  this function creates a condition structure to match messages with
  internal date
*/

struct mailimap_search_key *
mailimap_search_key_new_since(struct mailimap_date * sk_since);

/*
  this function creates a condition structure to match messages with 
  Subject field

  @param subject this is the content of Subject to match, it should
    be allocated with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_subject(char * sk_subject);

/*
  this function creates a condition structure to match messages with
  message text part

  @param text this is the message text to match, it should
    be allocated with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_text(char * sk_text);

/*
  this function creates a condition structure to match messages with 
  To field

  @param to this is the content of To to match, it should be allocated
    with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_to(char * sk_to);

/*
  this function creates a condition structure to match messages with 
  no a flag given by unkeyword
*/

struct mailimap_search_key *
mailimap_search_key_new_unkeyword(char * sk_unkeyword);

/*
  this function creates a condition structure to match messages with 
  the given field

  @param header_name this is the name of the field to match, it
    should be allocated with malloc()

  @param header_value this is the content, it should be allocated
    with malloc()
*/

struct mailimap_search_key *
mailimap_search_key_new_header(char * sk_header_name, char * sk_header_value);


/*
  this function creates a condition structure to match messages with size
*/

struct mailimap_search_key *
mailimap_search_key_new_larger(uint32_t sk_larger);

/*
  this function creates a condition structure to match messages that
  do not match the given condition
*/

struct mailimap_search_key *
mailimap_search_key_new_not(struct mailimap_search_key * sk_not);

/*
  this function creates a condition structure to match messages that
  match one of the given conditions
*/

struct mailimap_search_key *
mailimap_search_key_new_or(struct mailimap_search_key * sk_or1,
			   struct mailimap_search_key * sk_or2);

/*
  this function creates a condition structure to match messages
  with Date field
*/

struct mailimap_search_key *
mailimap_search_key_new_sentbefore(struct mailimap_date * sk_sentbefore);

/*
  this function creates a condition structure to match messages
  with Date field
*/

struct mailimap_search_key *
mailimap_search_key_new_senton(struct mailimap_date * sk_senton);

/*
  this function creates a condition structure to match messages
  with Date field
*/

struct mailimap_search_key *
mailimap_search_key_new_sentsince(struct mailimap_date * sk_sentsince);

/*
  this function creates a condition structure to match messages with size
*/

struct mailimap_search_key *
mailimap_search_key_new_smaller(uint32_t sk_smaller);

/*
  this function creates a condition structure to match messages with unique
  identifier
*/

struct mailimap_search_key *
mailimap_search_key_new_uid(struct mailimap_set * sk_uid);

/*
  this function creates a condition structure to match messages with number
  or unique identifier (depending whether SEARCH or UID SEARCH is used)
*/

struct mailimap_search_key *
mailimap_search_key_new_set(struct mailimap_set * sk_set);

/*
  this function creates a condition structure to match messages that match
  all the conditions given in the list
*/

struct mailimap_search_key *
mailimap_search_key_new_multiple(clist * sk_multiple);


/*
  same as previous but the list is empty
*/

struct mailimap_search_key *
mailimap_search_key_new_multiple_empty(void);

/*
  this function adds a condition to the condition list

  @return MAILIMAP_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int
mailimap_search_key_multiple_add(struct mailimap_search_key * keys,
				 struct mailimap_search_key * key_item);


/*
  this function creates an empty list of flags
*/

struct mailimap_flag_list *
mailimap_flag_list_new_empty(void);

/*
  this function adds a flag to the list of flags

  @return MAILIMAP_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int mailimap_flag_list_add(struct mailimap_flag_list * flag_list,
				struct mailimap_flag * f);

/*
  this function creates a \Answered flag
*/

struct mailimap_flag * mailimap_flag_new_answered(void);

/*
  this function creates a \Flagged flag
*/

struct mailimap_flag * mailimap_flag_new_flagged(void);

/*
  this function creates a \Deleted flag
*/

struct mailimap_flag * mailimap_flag_new_deleted(void);

/*
  this function creates a \Seen flag
*/

struct mailimap_flag * mailimap_flag_new_seen(void);

/*
  this function creates a \Draft flag
*/

struct mailimap_flag * mailimap_flag_new_draft(void);

/*
  this function creates a keyword flag

  @param flag_keyword this should be allocated with malloc()
*/

struct mailimap_flag * mailimap_flag_new_flag_keyword(char * flag_keyword);


/*
  this function creates an extension flag

  @param flag_extension this should be allocated with malloc()
*/

struct mailimap_flag * mailimap_flag_new_flag_extension(char * flag_extension);

/*
  this function creates an empty list of status attributes
*/

struct mailimap_status_att_list * mailimap_status_att_list_new_empty(void);

/*
  this function adds status attributes to the list

  @return MAILIMAP_NO_ERROR will be returned on success,
  other code will be returned otherwise
*/

int
mailimap_status_att_list_add(struct mailimap_status_att_list * sa_list,
			     int status_att);

/* return mailimap_section_part from a given mailimap_body */

int mailimap_get_section_part_from_body(struct mailimap_body * root_part,
    struct mailimap_body * part,
    struct mailimap_section_part ** result);

#ifdef __cplusplus
}
#endif

#endif
