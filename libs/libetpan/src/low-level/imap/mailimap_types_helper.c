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
 * $Id: mailimap_types_helper.c,v 1.12 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_types.h"
#include "mail.h"

#include <stdlib.h>

/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */

/* in helper */




struct mailimap_set_item * mailimap_set_item_new_single(uint32_t index)
{
  return mailimap_set_item_new(index, index);
}

struct mailimap_set *
mailimap_set_new_single_item(struct mailimap_set_item * item)
{
  struct mailimap_set * set;
  clist * list;
  int r;
  
  list = clist_new();
  if (list == NULL)
    return NULL;
  
  r = clist_append(list, item);
  if (r < 0) {
    clist_free(list);
    return NULL;
  }
	  
  set = mailimap_set_new(list);
  if (set == NULL) {
    clist_free(list);
    return NULL;
  }

  return set;
}

struct mailimap_set * mailimap_set_new_interval(uint32_t first, uint32_t last)
{
  struct mailimap_set_item * item;
  struct mailimap_set * set;

  item = mailimap_set_item_new(first, last);
  if (item == NULL)
    return NULL;

  set = mailimap_set_new_single_item(item);
  if (set == NULL) {
    mailimap_set_item_free(item);
    return NULL;
  }

  return set;
}

struct mailimap_set * mailimap_set_new_single(uint32_t index)
{
  return mailimap_set_new_interval(index, index);
}


struct mailimap_set * mailimap_set_new_empty(void)
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_set_new(list);
}

int mailimap_set_add(struct mailimap_set * set,
                     struct mailimap_set_item * set_item)
{
  int r;
  
  r = clist_append(set->set_list, set_item);
  if (r < 0)
    return MAILIMAP_ERROR_MEMORY;
  
  return MAILIMAP_NO_ERROR;
}

int mailimap_set_add_interval(struct mailimap_set * set,
			      uint32_t first, uint32_t last)
{
  struct mailimap_set_item * item;
  int r;
  
  item = mailimap_set_item_new(first, last);
  if (item == NULL)
    return MAILIMAP_ERROR_MEMORY;

  r = mailimap_set_add(set, item);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_set_item_free(item);
    return r;
  }
  else
    return MAILIMAP_NO_ERROR;
}

int mailimap_set_add_single(struct mailimap_set * set,
           		    uint32_t index)
{
  return mailimap_set_add_interval(set, index, index);
}

/* CHECK */
/* no args */

/* CLOSE */
/* no args */

/* EXPUNGE */
/* no args */

/* COPY */
/* set and gchar */

/* FETCH */
/* set and gchar fetch_type */



/* section */

#if 0
/* not correct XXX */

struct mailimap_section * mailimap_section_new_empty(void)
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_section_new(list);
}
#endif

static struct mailimap_section *
mailimap_section_new_msgtext(struct mailimap_section_msgtext * msgtext)
{
  struct mailimap_section_spec * spec;
  struct mailimap_section * section;

  spec = mailimap_section_spec_new(MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT,
				   msgtext, NULL, NULL);
  if (spec == NULL)
    return NULL;

  section = mailimap_section_new(spec);
  if (section == NULL) {
    /* detach section_msgtext so that it will not be freed */
    spec->sec_data.sec_msgtext = NULL;
    mailimap_section_spec_free(spec);
    return NULL;
  }
  
  return section;
}

static struct mailimap_section *
mailimap_section_new_part_msgtext(struct mailimap_section_part * part,
    struct mailimap_section_msgtext * msgtext)
{
  struct mailimap_section_spec * spec;
  struct mailimap_section * section;
  struct mailimap_section_text * text;

  text = mailimap_section_text_new(MAILIMAP_SECTION_TEXT_SECTION_MSGTEXT,
      msgtext);
  if (text == NULL)
    return NULL;

  spec = mailimap_section_spec_new(MAILIMAP_SECTION_SPEC_SECTION_PART,
				   NULL, part, text);
  if (spec == NULL) {
    /* detach section_msgtext so that it will not be freed */
    text->sec_msgtext = NULL;
    mailimap_section_text_free(text);
    return NULL;
  }

  section = mailimap_section_new(spec);
  if (section == NULL) {
    /* detach section_msgtext so that it will not be freed */
    text->sec_msgtext = NULL;
    mailimap_section_spec_free(spec);
    return NULL;
  }
  
  return section;
}

/*
HEADER
HEADER.FIELDS fields
HEADER.FIELDS.NOT fields
TEXT
*/

struct mailimap_section * mailimap_section_new_header(void)
{
  struct mailimap_section_msgtext * msgtext;
  struct mailimap_section * section;

  msgtext = mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_HEADER,
					 NULL);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_msgtext(msgtext);
  if (section == NULL) {
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

struct mailimap_section *
mailimap_section_new_header_fields(struct mailimap_header_list * header_list)
{
  struct mailimap_section * section;
  struct mailimap_section_msgtext * msgtext;

  msgtext =
    mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS,
				 header_list);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_msgtext(msgtext);
  if (section == NULL) {
    /* detach header_list so that it will not be freed */
    msgtext->sec_header_list = NULL;
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

struct mailimap_section *
mailimap_section_new_header_fields_not(struct mailimap_header_list * header_list)
{
  struct mailimap_section * section;
  struct mailimap_section_msgtext * msgtext;

  msgtext =
    mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT,
				 header_list);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_msgtext(msgtext);
  if (section == NULL) {
    /* detach header_list so that it will not be freed */
    msgtext->sec_header_list = NULL;
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

struct mailimap_section * mailimap_section_new_text(void)
{
  struct mailimap_section * section;
  struct mailimap_section_msgtext * msgtext;

  msgtext = mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_TEXT, NULL);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_msgtext(msgtext);
  if (section == NULL) {
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

/*
section-part
section-part . MIME
section-part . HEADER
section-part . HEADER.FIELDS fields
section-part . HEADER.FIELDS.NOT fields
section-part . TEXT
*/

struct mailimap_section *
mailimap_section_new_part(struct mailimap_section_part * part)
{
  struct mailimap_section_spec * spec;
  struct mailimap_section * section;

  spec = mailimap_section_spec_new(MAILIMAP_SECTION_SPEC_SECTION_PART,
				   NULL, part, NULL);
  if (spec == NULL)
    return NULL;

  section = mailimap_section_new(spec);
  if (section == NULL) {
    /* detach section_part so that it will not be freed */
    spec->sec_data.sec_part = NULL;
    mailimap_section_spec_free(spec);
    return NULL;
  }

  return section;
}

struct mailimap_section *
mailimap_section_new_part_mime(struct mailimap_section_part * part)
{
  struct mailimap_section_spec * spec;
  struct mailimap_section * section;
  struct mailimap_section_text * text;

  text = mailimap_section_text_new(MAILIMAP_SECTION_TEXT_MIME, NULL);
  if (text == NULL)
    return NULL;

  spec = mailimap_section_spec_new(MAILIMAP_SECTION_SPEC_SECTION_PART,
				   NULL, part, text);
  if (spec == NULL) {
    mailimap_section_text_free(text);
    return NULL;
  }

  section = mailimap_section_new(spec);
  if (section == NULL) {
    /* detach section_part so that it will not be freed */
    spec->sec_data.sec_part = NULL;
    mailimap_section_spec_free(spec);
    return NULL;
  }
  
  return section;
}

struct mailimap_section *
mailimap_section_new_part_header(struct mailimap_section_part * part)
{
  struct mailimap_section_msgtext * msgtext;
  struct mailimap_section * section;

  msgtext = mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_HEADER,
					 NULL);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_part_msgtext(part, msgtext);
  if (section == NULL) {
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

struct mailimap_section *
mailimap_section_new_part_header_fields(struct mailimap_section_part *
					part,
					struct mailimap_header_list *
					header_list)
{
  struct mailimap_section * section;
  struct mailimap_section_msgtext * msgtext;

  msgtext =
    mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS,
				 header_list);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_part_msgtext(part, msgtext);
  if (section == NULL) {
    /* detach header_list so that it will not be freed */
    msgtext->sec_header_list = NULL;
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

struct mailimap_section *
mailimap_section_new_part_header_fields_not(struct mailimap_section_part
					    * part,
					    struct mailimap_header_list
					    * header_list)
{
  struct mailimap_section * section;
  struct mailimap_section_msgtext * msgtext;

  msgtext =
    mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT,
				 header_list);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_part_msgtext(part, msgtext);
  if (section == NULL) {
    /* detach header_list so that it will not be freed */
    msgtext->sec_header_list = NULL;
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

struct mailimap_section *
mailimap_section_new_part_text(struct mailimap_section_part * part)
{
  struct mailimap_section * section;
  struct mailimap_section_msgtext * msgtext;

  msgtext = mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_TEXT, NULL);
  if (msgtext == NULL)
    return NULL;

  section = mailimap_section_new_part_msgtext(part, msgtext);
  if (section == NULL) {
    mailimap_section_msgtext_free(msgtext);
    return NULL;
  }

  return section;
}

/* end of section */






struct mailimap_fetch_att *
mailimap_fetch_att_new_envelope(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_ENVELOPE, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_flags(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_FLAGS, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_internaldate(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_INTERNALDATE, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_RFC822, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822_header(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_RFC822_HEADER, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822_size(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_RFC822_SIZE, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_rfc822_text(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_RFC822_TEXT, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_body(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_BODY, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_bodystructure(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_BODYSTRUCTURE, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_uid(void)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_UID, NULL, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_section(struct mailimap_section * section)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_BODY_SECTION, section, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_peek_section(struct mailimap_section * section)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_BODY_PEEK_SECTION, section, 0, 0);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_section_partial(struct mailimap_section * section,
					    uint32_t offset, uint32_t size)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_BODY_SECTION, section,
				offset, size);
}

struct mailimap_fetch_att *
mailimap_fetch_att_new_body_peek_section_partial(struct mailimap_section * section,
						 uint32_t offset, uint32_t size)
{
  return mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_BODY_PEEK_SECTION, section,
				offset, size);
}



struct mailimap_fetch_type *
mailimap_fetch_type_new_all(void)
{
  return mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_ALL, NULL, NULL);
}

struct mailimap_fetch_type *
mailimap_fetch_type_new_full(void)
{
  return mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FULL, NULL, NULL);
}

struct mailimap_fetch_type *
mailimap_fetch_type_new_fast(void)
{
  return mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FAST, NULL, NULL);
}

struct mailimap_fetch_type *
mailimap_fetch_type_new_fetch_att(struct mailimap_fetch_att * fetch_att)
{
  return mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FETCH_ATT, fetch_att, NULL);
}

struct mailimap_fetch_type *
mailimap_fetch_type_new_fetch_att_list(clist * fetch_att_list)
{
  return mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST,
				 NULL, fetch_att_list);
}

struct mailimap_fetch_type *
mailimap_fetch_type_new_fetch_att_list_empty(void)
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST,
				 NULL, list);
}

int
mailimap_fetch_type_new_fetch_att_list_add(struct mailimap_fetch_type *
    fetch_type,
    struct mailimap_fetch_att * fetch_att)
{
  int r;
	
  r = clist_append(fetch_type->ft_data.ft_fetch_att_list, fetch_att);
  if (r < 0)
    return MAILIMAP_ERROR_MEMORY;

  return MAILIMAP_NO_ERROR;
}



/* STORE */
/* set and store_att_flags */

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_set_flags(struct mailimap_flag_list * flags)
{
  return mailimap_store_att_flags_new(0, FALSE, flags);
}

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_set_flags_silent(struct mailimap_flag_list *
					      flags)
{
  return mailimap_store_att_flags_new(0, TRUE, flags);
}

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_add_flags(struct mailimap_flag_list * flags)
{
  return mailimap_store_att_flags_new(1, FALSE, flags);
}

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_add_flags_silent(struct mailimap_flag_list *
					      flags)
{
  return mailimap_store_att_flags_new(1, TRUE, flags);
}

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_remove_flags(struct mailimap_flag_list * flags)
{
  return mailimap_store_att_flags_new(-1, FALSE, flags);
}

struct mailimap_store_att_flags *
mailimap_store_att_flags_new_remove_flags_silent(struct mailimap_flag_list *
						 flags)
{
  return mailimap_store_att_flags_new(-1, TRUE, flags);
}

/* SEARCH */
/* date search-key set */

/*
  return mailimap_search_key_new(type, bcc, before,
				 body, cc, from, keyword, on, since,
				 subject, text, to, unkeyword, header_name,
				 header_value, larger, not,
				 or1, or2, sentbefore, senton, sentsince,
				 smaller, uid, set, multiple);
*/

struct mailimap_search_key *
mailimap_search_key_new_all(void)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_ALL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_bcc(char * sk_bcc)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_BCC, sk_bcc, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_before(struct mailimap_date * sk_before)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_BEFORE, NULL, sk_before,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_body(char * sk_body)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_BODY, NULL, NULL,
				 sk_body, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_cc(char * sk_cc)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_CC, NULL, NULL,
				 NULL, sk_cc, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_from(char * sk_from)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_FROM, NULL, NULL,
				 NULL, NULL, sk_from, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_keyword(char * sk_keyword)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_FROM, NULL, NULL,
				 NULL, NULL, NULL, sk_keyword, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_on(struct mailimap_date * sk_on)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_ON, NULL, NULL,
				 NULL, NULL, NULL, NULL, sk_on, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_since(struct mailimap_date * sk_since)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_SINCE, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, sk_since,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_subject(char * sk_subject)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_SINCE, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 sk_subject, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_text(char * sk_text)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_TEXT, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, sk_text, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_to(char * sk_to)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_TO, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, sk_to, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_unkeyword(char * sk_unkeyword)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_UNKEYWORD, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, sk_unkeyword, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_header(char * sk_header_name, char * sk_header_value)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_HEADER, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, sk_header_name,
				 sk_header_value, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_larger(uint32_t sk_larger)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_LARGER, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, sk_larger, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_not(struct mailimap_search_key * sk_not)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_NOT, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, sk_not,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_or(struct mailimap_search_key * sk_or1,
			   struct mailimap_search_key * sk_or2)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_OR, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 sk_or1, sk_or2, NULL, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_sentbefore(struct mailimap_date * sk_sentbefore)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_NOT, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, sk_sentbefore, NULL, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_senton(struct mailimap_date * sk_senton)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_SENTON, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, sk_senton, NULL,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_sentsince(struct mailimap_date * sk_sentsince)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_SENTSINCE, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, sk_sentsince,
				 0, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_smaller(uint32_t sk_smaller)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_SMALLER, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 sk_smaller, NULL, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_uid(struct mailimap_set * sk_uid)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_UID, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, sk_uid, NULL, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_set(struct mailimap_set * sk_set)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_SET, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, sk_set, NULL);
}

struct mailimap_search_key *
mailimap_search_key_new_multiple(clist * sk_multiple)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_MULTIPLE, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 NULL, 0, NULL,
				 NULL, NULL, NULL, NULL, NULL,
				 0, NULL, NULL, sk_multiple);
}

struct mailimap_search_key *
mailimap_search_key_new_multiple_empty(void)
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_search_key_new_multiple(list);
}

int
mailimap_search_key_multiple_add(struct mailimap_search_key * keys,
				 struct mailimap_search_key * key_item)
{
  int r;
	
  r = clist_append(keys->sk_data.sk_multiple, key_item);
  if (r < 0)
    return MAILIMAP_ERROR_MEMORY;

  return MAILIMAP_NO_ERROR;
}



/* CAPABILITY */
/* no args */

/* LOGOUT */
/* no args */

/* NOOP */
/* no args */

/* APPEND */
/* gchar flag_list date_time gchar */

struct mailimap_flag_list *
mailimap_flag_list_new_empty(void)
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_flag_list_new(list);
}

int mailimap_flag_list_add(struct mailimap_flag_list * flag_list,
		struct mailimap_flag * f)
{
  int r;
  
  r = clist_append(flag_list->fl_list, f);
  if (r < 0)
    return MAILIMAP_ERROR_MEMORY;
  
  return MAILIMAP_NO_ERROR;
}

struct mailimap_flag * mailimap_flag_new_answered(void)
{
  return mailimap_flag_new(MAILIMAP_FLAG_ANSWERED, NULL, NULL);
}

struct mailimap_flag * mailimap_flag_new_flagged(void)
{
  return mailimap_flag_new(MAILIMAP_FLAG_FLAGGED, NULL, NULL);
}

struct mailimap_flag * mailimap_flag_new_deleted(void)
{
  return mailimap_flag_new(MAILIMAP_FLAG_DELETED, NULL, NULL);
}

struct mailimap_flag * mailimap_flag_new_seen(void)
{
  return mailimap_flag_new(MAILIMAP_FLAG_SEEN, NULL, NULL);
}

struct mailimap_flag * mailimap_flag_new_draft(void)
{
  return mailimap_flag_new(MAILIMAP_FLAG_DRAFT, NULL, NULL);
}

struct mailimap_flag * mailimap_flag_new_flag_keyword(char * flag_keyword)
{
  return mailimap_flag_new(MAILIMAP_FLAG_KEYWORD, flag_keyword, NULL);
}

struct mailimap_flag * mailimap_flag_new_flag_extension(char * flag_extension)
{
  return mailimap_flag_new(MAILIMAP_FLAG_EXTENSION, NULL, flag_extension);
}




/* CREATE */
/* gchar */

/* DELETE */
/* gchar */

/* EXAMINE */
/* gchar */

/* LIST  */
/* gchar gchar */

/* LSUB */
/* gchar gchar */

/* RENAME */
/* gchar gchar */

/* SELECT */
/* gchar */

/* STATUS */
/* gchar GList of status_att */

struct mailimap_status_att_list * mailimap_status_att_list_new_empty(void)
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_status_att_list_new(list);
}

int
mailimap_status_att_list_add(struct mailimap_status_att_list * sa_list,
			     int status_att)
{
  int * pstatus_att;
  int r;

  pstatus_att = malloc(sizeof(* pstatus_att));
  * pstatus_att = status_att;

  r = clist_append(sa_list->att_list, pstatus_att);
  if (r < 0) {
    free(pstatus_att);
    return MAILIMAP_ERROR_MEMORY;
  }

  return MAILIMAP_NO_ERROR;
}

/* SUBSCRIBE */
/* gchar */

/* UNSUBSCRIBE */
/* gchar */

/* LOGIN */
/* gchar gchar */

/* AUTHENTICATE */
/* gchar */


static int recursive_build_path(struct mailimap_body * root_part,
    struct mailimap_body * part,
    clist ** result);

static int try_build_part(struct mailimap_body * root_part,
    struct mailimap_body * part, uint32_t count,
    clist ** result)
{
  int r;
  clist * imap_id_list;
  uint32_t * id;
  
  r = recursive_build_path(root_part, part, &imap_id_list);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  id = malloc(sizeof(* id));
  if (id == NULL) {
    clist_free(imap_id_list);
    return MAILIMAP_ERROR_MEMORY;
  }
  
  * id = count;
        
  r = clist_prepend(imap_id_list, id);
  if (r < 0) {
    free(id);
    clist_free(imap_id_list);
    return MAILIMAP_ERROR_MEMORY;
  }
        
  * result = imap_id_list;
        
  return MAILIMAP_NO_ERROR;
}


static int recursive_build_path(struct mailimap_body * root_part,
    struct mailimap_body * part,
    clist ** result)
{
  clistiter * cur;
  uint32_t count;
  int r;
  clist * imap_id_list;

  if (part == root_part) {
    imap_id_list = clist_new();
    if (imap_id_list == NULL) {
      return MAILIMAP_ERROR_MEMORY;
    }
    
    * result = imap_id_list;
    
    return MAILIMAP_NO_ERROR;
  }
  
  switch (root_part->bd_type) {
  case MAILIMAP_BODY_MPART:
    count = 0;
    for(cur = clist_begin(root_part->bd_data.bd_body_mpart->bd_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailimap_body * current_part;
      
      current_part = clist_content(cur);
      count ++;

      r = try_build_part(current_part, part, count, &imap_id_list);
      if (r == MAILIMAP_ERROR_INVAL) {
        continue;
      } if (r != MAILIMAP_NO_ERROR) {
        return r;
      }
      else {
        * result = imap_id_list;
        return MAILIMAP_NO_ERROR;
      }
    }
    return MAILIMAP_ERROR_INVAL;
    
  case MAILIMAP_BODY_1PART:
    if (root_part->bd_data.bd_body_1part->bd_type ==
        MAILIMAP_BODY_TYPE_1PART_MSG) {
      struct mailimap_body * current_part;
      
      current_part =
        root_part->bd_data.bd_body_1part->bd_data.bd_type_msg->bd_body;
      
      r = try_build_part(current_part, part, 1, &imap_id_list);
      if (r != MAILIMAP_NO_ERROR) {
        return r;
      }
      else {
        * result = imap_id_list;
        return MAILIMAP_NO_ERROR;
      }
    }
    else {
      return MAILIMAP_ERROR_INVAL;
    }
    break;
    
  default:
    return MAILIMAP_ERROR_INVAL;
  }
}

/* return mailimap_section_part from a given mailimap_body */

int mailimap_get_section_part_from_body(struct mailimap_body * root_part,
    struct mailimap_body * part,
    struct mailimap_section_part ** result)
{
  struct mailimap_section_part * section_part;
  clist * id_list;
  int r;
  int res;
  
  r = recursive_build_path(root_part, part, &id_list);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  section_part = mailimap_section_part_new(id_list);
  if (section_part == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_list;
  }
  
  * result = section_part;
  
  return MAILIMAP_NO_ERROR;
  
 free_list:
  clist_foreach(id_list, (clist_func) free, NULL);
  clist_free(id_list);
 err:
  return res;
}
