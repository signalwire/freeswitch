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
 * $Id: mailimap_parser.c,v 1.34 2006/10/20 00:13:30 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mailstream.h"
#include "mailimap_keywords.h"
#include "mailimap_parser.h"
#include "mailimap_extension.h"
#include "mmapstring.h"
#include "mail.h"

#ifndef UNSTRICT_SYNTAX
#define UNSTRICT_SYNTAX
#endif

/*
  Document: internet-drafts/draft-crispin-imapv-15.txt
  RFC 2060 (IMAP but rather used draft)
  RFC 2234 for all token that are not defined such as ALPHA
*/



/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */




static int mailimap_address_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index,
				  struct mailimap_address ** result,
				  size_t progr_rate,
				  progress_function * progr_fun);

static int mailimap_addr_adl_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, char ** result,
				   size_t progr_rate,
				   progress_function * progr_fun);

static int mailimap_addr_host_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, char ** result,
				    size_t progr_rate,
				    progress_function * progr_fun);

static int mailimap_addr_mailbox_parse(mailstream * fd, MMAPString * buffer,
				       size_t * index, char ** result,
				       size_t progr_rate,
				       progress_function * progr_fun);

static int mailimap_addr_name_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, char ** result,
				    size_t progr_rate,
				    progress_function * progr_fun);

static int mailimap_atom_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, char ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);

static int mailimap_auth_type_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, char ** result,
				    size_t progr_rate,
				    progress_function * progr_fun);

static int mailimap_base64_parse(mailstream * fd, MMAPString * buffer,
				 size_t * index, char ** result,
				 size_t progr_rate,
				 progress_function * progr_fun);

static int mailimap_body_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);


static int
mailimap_body_extension_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_extension ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);


static int
mailimap_body_ext_1part_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_ext_1part ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);



static int
mailimap_body_ext_mpart_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_ext_mpart ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);


static int
mailimap_body_fields_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   struct mailimap_body_fields ** result,
			   size_t progr_rate,
			   progress_function * progr_fun);

static int mailimap_body_fld_desc_parse(mailstream * fd, MMAPString * buffer,
					size_t * index, char ** result,
					size_t progr_rate,
					progress_function * progr_fun);


static int
mailimap_body_fld_dsp_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_body_fld_dsp ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);



static int
mailimap_body_fld_enc_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_body_fld_enc ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);



static int mailimap_body_fld_id_parse(mailstream * fd, MMAPString * buffer,
				      size_t * index, char ** result,
				      size_t progr_rate,
				      progress_function * progr_fun);


static int
mailimap_body_fld_lang_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_body_fld_lang ** result,
			     size_t progr_rate,
			     progress_function * progr_fun);

static int mailimap_body_fld_lines_parse(mailstream * fd,
					 MMAPString * buffer, size_t * index,
					 uint32_t * result);

static int mailimap_body_fld_md5_parse(mailstream * fd, MMAPString * buffer,
				       size_t * index, char ** result,
				       size_t progr_rate,
				       progress_function * progr_fun);

static int mailimap_body_fld_octets_parse(mailstream * fd,
					  MMAPString * buffer, size_t * index,
					  uint32_t * result);

static int
mailimap_body_fld_param_parse(mailstream * fd,
			      MMAPString * buffer, size_t * index,
			      struct mailimap_body_fld_param ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);



static int
mailimap_body_type_1part_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body_type_1part ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);



static int
mailimap_body_type_basic_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body_type_basic ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);



static int
mailimap_body_type_mpart_parse(mailstream * fd,
			       MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body_type_mpart ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);



static int
mailimap_body_type_msg_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_body_type_msg ** result,
			     size_t progr_rate,
			     progress_function * progr_fun);



static int
mailimap_body_type_text_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_type_text **
			      result,
			      size_t progr_rate,
			      progress_function * progr_fun);



static int
mailimap_capability_parse(mailstream * fd, MMAPString * buffer,
			  size_t * index,
			  struct mailimap_capability ** result,
			  size_t progr_rate,
			  progress_function * progr_fun);



static int
mailimap_capability_data_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_capability_data ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);


/*
static gboolean mailimap_date_day_parse(mailstream * fd,
					MMAPString * buffer,
					guint32 * index,
					gint * result);
*/
static int mailimap_date_day_fixed_parse(mailstream * fd,
					 MMAPString * buffer,
					 size_t * index,
					 int * result);

static int mailimap_date_month_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index, int * result);

/*
struct mailimap_date_text {
  gint day;
  gint month;
  gint year;
};

static gboolean
mailimap_date_text_parse(mailstream * fd, MMAPString * buffer,
			 guint32 * index, struct mailimap_date_text ** result);
static void mailimap_date_text_free(struct mailimap_date_text * date_text);
*/

static int mailimap_date_year_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, int * result);

static int mailimap_date_time_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    struct mailimap_date_time ** t,
				    size_t progr_rate,
				    progress_function * progr_fun);

#ifndef UNSTRICT_SYNTAX
static int mailimap_digit_nz_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, int * result);
#endif


static int mailimap_envelope_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index,
				   struct mailimap_envelope ** result,
				   size_t progr_rate,
				   progress_function * progr_fun);


static int
mailimap_env_bcc_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index, struct mailimap_env_bcc ** result,
		       size_t progr_rate,
		       progress_function * progr_fun);


static int
mailimap_env_cc_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, struct mailimap_env_cc ** result,
		      size_t progr_rate,
		      progress_function * progr_fun);

static int mailimap_env_date_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, char ** result,
				   size_t progr_rate,
				   progress_function * progr_fun);


static int
mailimap_env_from_parse(mailstream * fd, MMAPString * buffer,
			size_t * index, struct mailimap_env_from ** result,
			size_t progr_rate,
			progress_function * progr_fun);


static int mailimap_env_in_reply_to_parse(mailstream * fd,
					  MMAPString * buffer,
					  size_t * index, char ** result,
					  size_t progr_rate,
					  progress_function * progr_fun);

static int mailimap_env_message_id_parse(mailstream * fd,
					 MMAPString * buffer,
					 size_t * index, char ** result,
					 size_t progr_rate,
					 progress_function * progr_fun);

static int
mailimap_env_reply_to_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_env_reply_to ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);



static int
mailimap_env_sender_parse(mailstream * fd, MMAPString * buffer,
			  size_t * index, struct mailimap_env_sender ** result,
			  size_t progr_rate,
			  progress_function * progr_fun);

static int mailimap_env_subject_parse(mailstream * fd, MMAPString * buffer,
				      size_t * index, char ** result,
				      size_t progr_rate,
				      progress_function * progr_fun);


static int
mailimap_env_to_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index,
		      struct mailimap_env_to ** result,
		      size_t progr_rate,
		      progress_function * progr_fun);


static int mailimap_flag_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_flag ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);

static int mailimap_flag_extension_parse(mailstream * fd,
					 MMAPString * buffer,
					 size_t * index,
					 char ** result,
					 size_t progr_rate,
					 progress_function * progr_fun);




static int
mailimap_flag_fetch_parse(mailstream * fd, MMAPString * buffer,
			  size_t * index,
			  struct mailimap_flag_fetch ** result,
			  size_t progr_rate,
			  progress_function * progr_fun);



static int
mailimap_flag_perm_parse(mailstream * fd, MMAPString * buffer,
			 size_t * index,
			 struct mailimap_flag_perm ** result,
			 size_t progr_rate,
			 progress_function * progr_fun);


static int mailimap_flag_keyword_parse(mailstream * fd, MMAPString * buffer,
				       size_t * index,
				       char ** result,
				       size_t progr_rate,
				       progress_function * progr_fun);


static int mailimap_flag_list_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    struct mailimap_flag_list ** result,
				    size_t progr_rate,
				    progress_function * progr_fun);


static int
mailimap_header_fld_name_parse(mailstream * fd,
			       MMAPString * buffer,
			       size_t * index,
			       char ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);




static int
mailimap_header_list_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   struct mailimap_header_list ** result,
			   size_t progr_rate,
			   progress_function * progr_fun);

static int mailimap_literal_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index, char ** result,
				  size_t * result_len,
				  size_t progr_rate,
				  progress_function * progr_fun);


static int
mailimap_mailbox_data_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_mailbox_data ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);


static int
mailimap_mbx_list_flags_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_mbx_list_flags ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);


static int
mailimap_mbx_list_oflag_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_mbx_list_oflag ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);

static int
mailimap_mbx_list_oflag_no_sflag_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    struct mailimap_mbx_list_oflag ** result,
    size_t progr_rate,
    progress_function * progr_fun);

static int
mailimap_mbx_list_sflag_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      int * result);


static int
mailimap_mailbox_list_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_mailbox_list ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);



static int
mailimap_media_basic_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   struct mailimap_media_basic ** result,
			   size_t progr_rate,
			   progress_function * progr_fun);

static int
mailimap_media_message_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index);

static int
mailimap_media_subtype_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     char ** result,
			     size_t progr_rate,
			     progress_function * progr_fun);

static int mailimap_media_text_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index,
				     char ** result,
				     size_t progr_rate,
				     progress_function * progr_fun);



static int
mailimap_message_data_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_message_data ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);





static int
mailimap_msg_att_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index, struct mailimap_msg_att ** result,
		       size_t progr_rate,
		       progress_function * progr_fun);



static int
mailimap_msg_att_dynamic_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_msg_att_dynamic ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);


static int
mailimap_msg_att_static_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_msg_att_static ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);

static int mailimap_nil_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index);

static int
mailimap_number_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, uint32_t * result);

static int
mailimap_quoted_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, char ** result,
		      size_t progr_rate,
		      progress_function * progr_fun);

static int
mailimap_quoted_char_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index, char * result);


static int
mailimap_quoted_specials_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, char * result);





static int
mailimap_response_data_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_response_data ** result,
			     size_t progr_rate,
			     progress_function * progr_fun);




static int
mailimap_response_done_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_response_done ** result,
			     size_t progr_rate,
			     progress_function * progr_fun);

static int
mailimap_response_fatal_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_response_fatal ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);


static int
mailimap_response_tagged_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_response_tagged ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);


static int
mailimap_resp_cond_auth_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_resp_cond_auth ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);

static int
mailimap_resp_cond_bye_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_resp_cond_bye ** result,
			     size_t progr_rate,
			     progress_function * progr_fun);


static int
mailimap_resp_cond_state_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_resp_cond_state ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);


static int
mailimap_resp_text_parse(mailstream * fd, MMAPString * buffer,
			 size_t * index,
			 struct mailimap_resp_text ** result,
			 size_t progr_rate,
			 progress_function * progr_fun);


static int
mailimap_resp_text_code_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_resp_text_code ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);


static int
mailimap_section_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index,
		       struct mailimap_section ** result,
		       size_t progr_rate,
		       progress_function * progr_fun);


static int
mailimap_section_msgtext_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_section_msgtext ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);


static int
mailimap_section_part_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_section_part ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);




static int
mailimap_section_spec_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_section_spec ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);


static int
mailimap_section_text_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_section_text ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);


static int mailimap_status_att_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index, int * result);

static int mailimap_tag_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index, char ** result,
			      size_t progr_rate,
			      progress_function * progr_fun);

static int mailimap_text_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, char ** result,
			       size_t progr_rate,
			       progress_function * progr_fun);

static int mailimap_time_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       int * phour, int * pmin, int * psec);

static int mailimap_zone_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, int * result);



/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */





/* ******************** TOOLS **************************** */


static int mailimap_unstrict_char_parse(mailstream * fd, MMAPString * buffer,
					size_t * index, char token)
{
  size_t cur_token;
  int r;

  cur_token = * index;
  
#ifdef UNSTRICT_SYNTAX
  /* can accept unstrict syntax */

  mailimap_space_parse(fd, buffer, &cur_token);
  if (token == ' ') {
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }
#endif

  r = mailimap_char_parse(fd, buffer, &cur_token, token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

int mailimap_oparenth_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '(');
}

int mailimap_cparenth_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, ')');
}

static int mailimap_oaccolade_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '{');
}

static int mailimap_caccolade_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '}');
}

static int mailimap_plus_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '+');
}

static int mailimap_minus_parse(mailstream * fd, MMAPString * buffer,
				size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '-');
}

static int mailimap_star_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '*');
}

static int mailimap_dot_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '.');
}

int mailimap_colon_parse(mailstream * fd, MMAPString * buffer,
    size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, ':');
}

static int mailimap_lower_parse(mailstream * fd, MMAPString * buffer,
				size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '<');
}

static int mailimap_greater_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '>');
}

static int mailimap_obracket_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, '[');
}

static int mailimap_cbracket_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index)
{
  return mailimap_unstrict_char_parse(fd, buffer, index, ']');
}

static int mailimap_dquote_parse(mailstream * fd, MMAPString * buffer,
				 size_t * index)
{
  return mailimap_char_parse(fd, buffer, index, '\"');
}

static int mailimap_crlf_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index)
{
  size_t cur_token = * index;

#ifdef UNSTRICT_SYNTAX
  mailimap_space_parse(fd, buffer, &cur_token);
#endif

  if (mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "\r\n")) {
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }

#ifdef UNSTRICT_SYNTAX
  else if (mailimap_unstrict_char_parse(fd, buffer, &cur_token, '\n')) {
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }
#endif

  else
    return MAILIMAP_ERROR_PARSE;
}

static int
mailimap_struct_multiple_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, clist ** result,
			       mailimap_struct_parser * parser,
			       mailimap_struct_destructor * destructor,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  clist * struct_list;
  size_t cur_token;
  void * value;
  int r;
  int res;

  cur_token = * index;

  r = parser(fd, buffer, &cur_token, &value, progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  struct_list = clist_new();
  if (struct_list == NULL) {
    destructor(value);
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }

  r = clist_append(struct_list, value);
  if (r < 0) {
    destructor(value);
    res = MAILIMAP_ERROR_MEMORY;
    goto free_list;
  }

  while (1) {
    r = parser(fd, buffer, &cur_token, &value, progr_rate, progr_fun);
    if (r == MAILIMAP_ERROR_PARSE)
      break;
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto free_list;
    }
    
    r = clist_append(struct_list, value);
    if (r < 0) {
      destructor(value);
      res = MAILIMAP_ERROR_MEMORY;
      goto free_list;
    }
  }

  * result = struct_list;
  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;

 free_list:
  clist_foreach(struct_list, (clist_func) destructor, NULL);
  clist_free(struct_list);
 err:
  return res;
}

int
mailimap_struct_list_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index, clist ** result,
			   char symbol,
			   mailimap_struct_parser * parser,
			   mailimap_struct_destructor * destructor,
			   size_t progr_rate,
			   progress_function * progr_fun)
{
  clist * struct_list;
  size_t cur_token;
  void * value;
  size_t final_token;
  int r;
  int res;
  
  cur_token = * index;
  struct_list = NULL;

  r = parser(fd, buffer, &cur_token, &value, progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  struct_list = clist_new();
  if (struct_list == NULL) {
    destructor(value);
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }

  r = clist_append(struct_list, value);
  if (r < 0) {
    destructor(value);
    res = MAILIMAP_ERROR_MEMORY;
    goto free_list;
  }

  final_token = cur_token;

  while (1) {
    r = mailimap_unstrict_char_parse(fd, buffer, &cur_token, symbol);
    if (r == MAILIMAP_ERROR_PARSE)
      break;
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = parser(fd, buffer, &cur_token, &value, progr_rate, progr_fun);
    if (r == MAILIMAP_ERROR_PARSE)
      break;

    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = clist_append(struct_list, value);
    if (r < 0) {
      destructor(value);
      res = MAILIMAP_ERROR_MEMORY;
      goto free_list;
    }
      
    final_token = cur_token;
  }

  * result = struct_list;
  * index = final_token;

  return MAILIMAP_NO_ERROR;

 free_list:
  clist_foreach(struct_list, (clist_func) destructor, NULL);
  clist_free(struct_list);
 err:
  return res;
}

int
mailimap_struct_spaced_list_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index, clist ** result,
				  mailimap_struct_parser * parser,
				  mailimap_struct_destructor * destructor,
				  size_t progr_rate,
				  progress_function * progr_fun)
{
  return mailimap_struct_list_parse(fd, buffer, index, result,
				    ' ', parser, destructor,
				    progr_rate, progr_fun);
}



static int
mailimap_custom_string_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index, char ** result,
			     int (* is_custom_char)(char))
{
  size_t begin;
  size_t end;
  char * gstr;

  begin = * index;

#ifdef UNSTRICT_SYNTAX
  mailimap_space_parse(fd, buffer, &begin);
#endif

  end = begin;

  while (is_custom_char(buffer->str[end]))
    end ++;

  if (end != begin) {
    gstr = malloc(end - begin + 1);
    if (gstr == NULL)
      return MAILIMAP_ERROR_MEMORY;

    strncpy(gstr, buffer->str + begin, end - begin);
    gstr[end - begin] = '\0';
    
    * index = end;
    * result = gstr;
    return MAILIMAP_NO_ERROR;
  }
  else
    return MAILIMAP_ERROR_PARSE;
}



static int
mailimap_nz_number_alloc_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       uint32_t ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  uint32_t number;
  uint32_t * number_alloc;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimap_nz_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  number_alloc = mailimap_number_alloc_new(number);
  if (number_alloc == NULL)
    return MAILIMAP_ERROR_MEMORY;

  * index = cur_token;
  * result = number_alloc;

  return MAILIMAP_NO_ERROR;
}


static int is_ctl(char ch)
{
  unsigned char uch = (unsigned char) ch;

  return (uch <= 0x1F);
}

static int is_char(char ch)
{
#ifdef UNSTRICT_SYNTAX
  return (ch != 0);
#else
  unsigned char uch = ch;

  return (uch >= 0x01) && (uch <= 0x7f);
#endif
}

static int is_alpha(char ch)
{
  return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && (ch <= 'z')));
}

static int is_digit(char ch)
{
  return (ch >= '0') && (ch <= '9');
}

static int mailimap_digit_parse(mailstream * fd, MMAPString * buffer,
				size_t * index, int * result)
{
  size_t cur_token;

  cur_token = * index;

  if (is_digit(buffer->str[cur_token])) {
    * result = buffer->str[cur_token] - '0';
    cur_token ++;
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }
  else
    return MAILIMAP_ERROR_PARSE;
}


/* ******************** parser **************************** */

/*
   address         = "(" addr-name SP addr-adl SP addr-mailbox SP
                     addr-host ")"
*/

static int mailimap_address_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index,
				  struct mailimap_address ** result,
				  size_t progr_rate,
				  progress_function * progr_fun)
{
  size_t cur_token;
  char * addr_name;
  char * addr_adl;
  char * addr_mailbox;
  char * addr_host;
  struct mailimap_address * addr;
  int r;
  int res;
  
  cur_token = * index;

  addr_name = NULL;
  addr_adl = NULL;
  addr_mailbox = NULL;
  addr_host = NULL;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_addr_name_parse(fd, buffer, &cur_token, &addr_name,
			       progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto addr_name_free;
  }

  r = mailimap_addr_adl_parse(fd, buffer, &cur_token, &addr_adl,
			      progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto addr_name_free;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto addr_adl_free;
  }

  r = mailimap_addr_mailbox_parse(fd, buffer, &cur_token, &addr_mailbox,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto addr_adl_free;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto addr_mailbox_free;
  }

  r = mailimap_addr_host_parse(fd, buffer, &cur_token, &addr_host,
			       progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto addr_mailbox_free;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto addr_host_free;
  }

  addr = mailimap_address_new(addr_name, addr_adl, addr_mailbox, addr_host);

  if (addr == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto addr_host_free;
  }

  * result = addr;
  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;

 addr_host_free:
  mailimap_addr_host_free(addr_host);
 addr_mailbox_free:
  mailimap_addr_mailbox_free(addr_mailbox);
 addr_adl_free:
  mailimap_addr_adl_free(addr_adl);
 addr_name_free:
  mailimap_addr_name_free(addr_name);
 err:
  return res;
}

/*
   addr-adl        = nstring
                       ; Holds route from [RFC-822] route-addr if
                       ; non-NIL
*/

static int mailimap_addr_adl_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, char ** result,
				   size_t progr_rate,
				   progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}

/*
   addr-host       = nstring
                       ; NIL indicates [RFC-822] group syntax.
                       ; Otherwise, holds [RFC-822] domain name
*/

static int mailimap_addr_host_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, char ** result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}

/*
   addr-mailbox    = nstring
                       ; NIL indicates end of [RFC-822] group; if
                       ; non-NIL and addr-host is NIL, holds
                       ; [RFC-822] group name.
                       ; Otherwise, holds [RFC-822] local-part
                       ; after removing [RFC-822] quoting
 */

static int mailimap_addr_mailbox_parse(mailstream * fd, MMAPString * buffer,
				       size_t * index, char ** result,
				       size_t progr_rate,
				       progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}


/*
   addr-name       = nstring
                       ; If non-NIL, holds phrase from [RFC-822]
                       ; mailbox after removing [RFC-822] quoting
*/

static int mailimap_addr_name_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, char ** result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}


/*
  NOT IMPLEMENTED
   append          = "APPEND" SP mailbox [SP flag-list] [SP date-time] SP
                     literal
*/

/*
   astring         = 1*ASTRING-CHAR / string
*/

static int is_astring_char(char ch);

static int
mailimap_atom_astring_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index, char ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  return mailimap_custom_string_parse(fd, buffer, index, result,
				      is_astring_char);
}

int
mailimap_astring_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index,
		       char ** result,
		       size_t progr_rate,
		       progress_function * progr_fun)
{
  size_t cur_token;
  char * astring;
  int r;

  cur_token = * index;

  r = mailimap_atom_astring_parse(fd, buffer, &cur_token, &astring,
				  progr_rate, progr_fun);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;

  case MAILIMAP_ERROR_PARSE:
    r = mailimap_string_parse(fd, buffer, &cur_token, &astring, NULL,
			      progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    break;

  default:
    return r;
  }

  * result = astring;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
   ASTRING-CHAR   = ATOM-CHAR / resp-specials
*/

static int is_atom_char(char ch);
static int is_resp_specials(char ch);

static int is_astring_char(char ch)
{
  if (is_atom_char(ch))
    return TRUE;
  if (is_resp_specials(ch))
    return TRUE;
  return FALSE;
}

/*
   atom            = 1*ATOM-CHAR
*/

static int mailimap_atom_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, char ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  return mailimap_custom_string_parse(fd, buffer, index, result,
				      is_atom_char);
}

/*
   ATOM-CHAR       = <any CHAR except atom-specials>
*/

static int is_atom_specials(char ch);

static int is_atom_char(char ch)
{
  if (is_atom_specials(ch))
    return FALSE;

  return is_char(ch);
}

/*
   atom-specials   = "(" / ")" / "{" / SP / CTL / list-wildcards /
                     quoted-specials / resp-specials

no "}" because there is no need (Mark Crispin)
*/

static int is_quoted_specials(char ch);
static int is_list_wildcards(char ch);

static int is_atom_specials(char ch)
{
  switch (ch) {
  case '(':
  case ')':
  case '{':
  case ' ':
    return TRUE;
  };
  if (is_ctl(ch))
    return TRUE;
  if (is_list_wildcards(ch))
    return TRUE;
  if (is_resp_specials(ch))
    return TRUE;

  return is_quoted_specials(ch);
}

/*
  NOT IMPLEMENTED
   authenticate    = "AUTHENTICATE" SP auth-type *(CRLF base64)
*/

/*
   auth-type       = atom
                       ; Defined by [SASL]
*/

static int mailimap_auth_type_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, char ** result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  return mailimap_atom_parse(fd, buffer, index, result,
			     progr_rate, progr_fun);
}

/*
   base64          = *(4base64-char) [base64-terminal]
*/

static int is_base64_4char(char * str);
static int is_base64_terminal(char * str);

static int mailimap_base64_parse(mailstream * fd, MMAPString * buffer,
				 size_t * index, char ** result,
				 size_t progr_rate,
				 progress_function * progr_fun)
{
  size_t begin;
  size_t end;
  char * gstr;

  begin = * index;
  end = begin;

  while (is_base64_4char(buffer->str + end))
    end += 4;
  if (is_base64_terminal(buffer->str + end))
    end += 4;
#if 0
  else
    return MAILIMAP_ERROR_PARSE;
#endif

  gstr = malloc(end - begin + 1);
  if (gstr == NULL)
    return MAILIMAP_ERROR_MEMORY;
  strncpy(gstr, buffer->str + begin, end - begin);
  gstr[end - begin] = '\0';

  * result = gstr;
  * index = end;

  return MAILIMAP_NO_ERROR;
}

/*
   base64-char     = ALPHA / DIGIT / "+" / "/"
                       ; Case-sensitive
*/

static int is_base64_char(char ch)
{
  return (is_alpha(ch) || is_digit(ch) || ch == '+' || ch == '/');
}

static int is_base64_4char(char * str)
{
  size_t i;

  for (i = 0 ; i < 4 ; i++)
    if (!is_base64_char(str[i]))
      return FALSE;
  return TRUE;
}

/*
   base64-terminal = (2base64-char "==") / (3base64-char "=")
*/

static int is_base64_terminal(char * str)
{
  if (str[0] == 0)
    return FALSE;
  if (str[1] == 0)
    return FALSE;
  if (str[2] == 0)
    return FALSE;
  if (str[3] == 0)
    return FALSE;

  if (is_base64_char(str[0]) || is_base64_char(str[1])
      || str[2] == '=' || str[3] == '=')
    return TRUE;
  if (is_base64_char(str[0]) || is_base64_char(str[1])
      || is_base64_char(str[2]) || str[3] == '=')
    return TRUE;
  return FALSE;
}


/*
   body            = "(" (body-type-1part / body-type-mpart) ")"
*/

static int mailimap_body_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  struct mailimap_body_type_1part * body_type_1part;
  struct mailimap_body_type_mpart * body_type_mpart;
  struct mailimap_body * body;
  size_t cur_token;
  int type;
  int r;
  int res;

  cur_token = * index;

  body_type_1part = NULL;
  body_type_mpart = NULL;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  type = MAILIMAP_BODY_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_body_type_1part_parse(fd, buffer, &cur_token, &body_type_1part,
				     progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_BODY_1PART;

  if (r == MAILIMAP_ERROR_PARSE) {
   r = mailimap_body_type_mpart_parse(fd, buffer, &cur_token,
				      &body_type_mpart,
				      progr_rate, progr_fun);
   
   if (r == MAILIMAP_NO_ERROR)
     type = MAILIMAP_BODY_MPART;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  body = mailimap_body_new(type, body_type_1part, body_type_mpart);
  if (body == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = body;
  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;

 free:
  if (body_type_1part)
    mailimap_body_type_1part_free(body_type_1part);
  if (body_type_mpart)
    mailimap_body_type_mpart_free(body_type_mpart);
 err:
  return res;
}

/*
   body-extension  = nstring / number /
                      "(" body-extension *(SP body-extension) ")"
                       ; Future expansion.  Client implementations
                       ; MUST accept body-extension fields.  Server
                       ; implementations MUST NOT generate
                       ; body-extension fields except as defined by
                       ; future standard or standards-track
                       ; revisions of this specification.
*/

/*
  "(" body-extension *(SP body-extension) ")"
*/

static int
mailimap_body_ext_list_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     clist ** result,
			     size_t progr_rate,
			     progress_function * progr_fun)
{
  size_t cur_token;
  clist * list;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer,
					&cur_token, &list,
					(mailimap_struct_parser * )
					mailimap_body_extension_parse,
					(mailimap_struct_destructor * )
					mailimap_body_extension_free,
					progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_list;
  }

  * index = cur_token;
  * result = list;

  return MAILIMAP_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimap_body_extension_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
   body-extension  = nstring / number /
                      "(" body-extension *(SP body-extension) ")"
                       ; Future expansion.  Client implementations
                       ; MUST accept body-extension fields.  Server
                       ; implementations MUST NOT generate
                       ; body-extension fields except as defined by
                       ; future standard or standards-track
                       ; revisions of this specification.
*/

static int
mailimap_body_extension_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_extension ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;
  uint32_t number;
  char * nstring;
  clist * body_extension_list;
  struct mailimap_body_extension * body_extension;
  int type;
  int r;
  int res;

  cur_token = * index;

  nstring = NULL;
  number = 0;
  body_extension_list = NULL;
  type = MAILIMAP_BODY_EXTENSION_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_nstring_parse(fd, buffer, &cur_token, &nstring, NULL,
			     progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_BODY_EXTENSION_NSTRING;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_number_parse(fd, buffer, &cur_token, &number);

    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_BODY_EXTENSION_NUMBER;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_body_ext_list_parse(fd, buffer, &cur_token,
				     &body_extension_list,
				     progr_rate, progr_fun);

    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_BODY_EXTENSION_LIST;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  body_extension = mailimap_body_extension_new(type, nstring, number,
					       body_extension_list);
  
  if (body_extension == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = body_extension;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (nstring != NULL)
    mailimap_nstring_free(nstring);
  if (body_extension_list) {
    clist_foreach(body_extension_list,
		  (clist_func) mailimap_body_extension_free,
		  NULL);
    clist_free(body_extension_list);
  }
 err:
  return res;
}

/*
   body-ext-1part  = body-fld-md5 [SP body-fld-dsp [SP body-fld-lang
                     *(SP body-extension)]]
                       ; MUST NOT be returned on non-extensible
                       ; "BODY" fetch
*/

/*
 *(SP body-extension)
*/

static int
mailimap_body_ext_1part_3_parse(mailstream * fd, MMAPString * buffer,
				size_t * index,
				clist ** body_ext_list,
				size_t progr_rate,
				progress_function * progr_fun)
{
  size_t cur_token;
  int r;
  
  cur_token = * index;
  * body_ext_list = NULL;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token,
					body_ext_list,
					(mailimap_struct_parser *)
					mailimap_body_extension_parse,
					(mailimap_struct_destructor *)
					mailimap_body_extension_free,
					progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
  [SP body-fld-lang
  *(SP body-extension)]]
*/

static int
mailimap_body_ext_1part_2_parse(mailstream * fd, MMAPString * buffer,
				size_t * index,
				struct mailimap_body_fld_lang ** fld_lang,
				clist ** body_ext_list,
				size_t progr_rate,
				progress_function * progr_fun)
{
  size_t cur_token;
  int r;
  
  cur_token = * index;
  * fld_lang = NULL;
  * body_ext_list = NULL;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_body_fld_lang_parse(fd, buffer, &cur_token, fld_lang,
				   progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_body_ext_1part_3_parse(fd, buffer, &cur_token,
				      body_ext_list, progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE))
    return r;

  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;
}


/*
  SP body-fld-dsp [SP body-fld-lang
  *(SP body-extension)]]
*/

static int
mailimap_body_ext_1part_1_parse(mailstream * fd, MMAPString * buffer,
				size_t * index,
				struct mailimap_body_fld_dsp ** fld_dsp,
				struct mailimap_body_fld_lang ** fld_lang,
				clist ** body_ext_list,
				size_t progr_rate,
				progress_function * progr_fun)
{
  size_t cur_token;
  int r;
  
  cur_token = * index;
  * fld_dsp = NULL;
  * fld_lang = NULL;
  * body_ext_list = NULL;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_body_fld_dsp_parse(fd, buffer, &cur_token, fld_dsp,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_body_ext_1part_2_parse(fd, buffer, &cur_token,
				      fld_lang, body_ext_list,
				      progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE))
    return r;

  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
   body-ext-1part  = body-fld-md5 [SP body-fld-dsp [SP body-fld-lang
                     *(SP body-extension)]]
                       ; MUST NOT be returned on non-extensible
                       ; "BODY" fetch
*/

static int
mailimap_body_ext_1part_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_ext_1part ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;

  char * fld_md5;
  struct mailimap_body_fld_dsp * fld_dsp;
  struct mailimap_body_fld_lang * fld_lang;
  clist * body_ext_list;
  int r;
  int res;

  struct mailimap_body_ext_1part * ext_1part;

  cur_token = * index;

  fld_md5 = NULL;
  fld_dsp = NULL;
  fld_lang = NULL;
  body_ext_list = NULL;

  r = mailimap_body_fld_md5_parse(fd, buffer, &cur_token, &fld_md5,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_body_ext_1part_1_parse(fd, buffer, &cur_token,
				      &fld_dsp,
				      &fld_lang,
				      &body_ext_list,
				      progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto free;
  }

  ext_1part = mailimap_body_ext_1part_new(fld_md5, fld_dsp, fld_lang,
					  body_ext_list);
  
  if (ext_1part == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }
  
  * result = ext_1part;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
  
 free:
  if (body_ext_list) {
    clist_foreach(body_ext_list, (clist_func) mailimap_body_extension_free,
		  NULL);
    clist_free(body_ext_list);
  }
  if (fld_lang)
    mailimap_body_fld_lang_free(fld_lang);
  if (fld_dsp)
    mailimap_body_fld_dsp_free(fld_dsp);
  mailimap_body_fld_md5_free(fld_md5);
 err:
  return res;
}
					      

/*
   body-ext-mpart  = body-fld-param [SP body-fld-dsp [SP body-fld-lang
                     *(SP body-extension)]]
                       ; MUST NOT be returned on non-extensible
                       ; "BODY" fetch
*/

static int
mailimap_body_ext_mpart_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_ext_mpart ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;

  struct mailimap_body_fld_dsp * fld_dsp;
  struct mailimap_body_fld_lang * fld_lang;
  struct mailimap_body_fld_param * fld_param;
  clist * body_ext_list;

  struct mailimap_body_ext_mpart * ext_mpart;
  int r;
  int res;

  cur_token = * index;

  fld_param = NULL;
  fld_dsp = NULL;
  fld_lang = NULL;
  body_ext_list = NULL;

  r = mailimap_body_fld_param_parse(fd, buffer, &cur_token, &fld_param,
				    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_body_ext_1part_1_parse(fd, buffer, &cur_token,
				      &fld_dsp,
				      &fld_lang,
				      &body_ext_list,
				      progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto free;
  }

  ext_mpart = mailimap_body_ext_mpart_new(fld_param, fld_dsp, fld_lang,
					  body_ext_list);
  if (ext_mpart == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }
  
  * result = ext_mpart;
  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;

 free:
  if (body_ext_list) {
    clist_foreach(body_ext_list, (clist_func) mailimap_body_extension_free,
		   NULL);
    clist_free(body_ext_list);
  }
  if (fld_lang)
    mailimap_body_fld_lang_free(fld_lang);
  if (fld_dsp)
    mailimap_body_fld_dsp_free(fld_dsp);
  if (fld_param != NULL)
    mailimap_body_fld_param_free(fld_param);
 err:
  return res;
}

/*
   body-fields     = body-fld-param SP body-fld-id SP body-fld-desc SP
                     body-fld-enc SP body-fld-octets
*/

static int
mailimap_body_fields_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   struct mailimap_body_fields ** result,
			   size_t progr_rate,
			   progress_function * progr_fun)
{
  struct mailimap_body_fields * body_fields;
  size_t cur_token;
  struct mailimap_body_fld_param * body_fld_param;
  char * body_fld_id;
  char * body_fld_desc;
  struct mailimap_body_fld_enc * body_fld_enc;
  uint32_t body_fld_octets;
  int r;
  int res;
  
  body_fld_param = NULL;
  body_fld_id = NULL;
  body_fld_desc = NULL;
  body_fld_enc = NULL;
  body_fld_octets = 0;

  cur_token = * index;

  r = mailimap_body_fld_param_parse(fd, buffer, &cur_token, &body_fld_param,
				    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_param_free;
  }

  r = mailimap_body_fld_id_parse(fd, buffer, &cur_token, &body_fld_id,
				 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_param_free;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_id_free;
  }
  
  r = mailimap_body_fld_desc_parse(fd, buffer, &cur_token, &body_fld_desc,
				   progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_id_free;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_desc_free;
  }
  
  r = mailimap_body_fld_enc_parse(fd, buffer, &cur_token, &body_fld_enc,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_desc_free;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_enc_free;
  }

  r = mailimap_body_fld_octets_parse(fd, buffer, &cur_token,
				     &body_fld_octets);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto fld_enc_free;
  }

  body_fields = mailimap_body_fields_new(body_fld_param,
					 body_fld_id,
					 body_fld_desc,
					 body_fld_enc,
					 body_fld_octets);
  if (body_fields == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto fld_enc_free;
  }

  * result = body_fields;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 fld_enc_free:
  mailimap_body_fld_enc_free(body_fld_enc);
 fld_desc_free:
  mailimap_body_fld_desc_free(body_fld_desc);
 fld_id_free:
  mailimap_body_fld_id_free(body_fld_id);
 fld_param_free:
  if (body_fld_param != NULL)
    mailimap_body_fld_param_free(body_fld_param);
 err:  
  return res;
}
  
/*
   body-fld-desc   = nstring
*/

static int mailimap_body_fld_desc_parse(mailstream * fd, MMAPString * buffer,
					size_t * index, char ** result,
					size_t progr_rate,
					progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}

/*
   body-fld-dsp    = "(" string SP body-fld-param ")" / nil
*/

static int
mailimap_body_fld_dsp_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_body_fld_dsp ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  size_t cur_token;
  char * name;
  struct mailimap_body_fld_param * body_fld_param;
  struct mailimap_body_fld_dsp * body_fld_dsp;
  int res;
  int r;

  cur_token = * index;
  name = NULL;
  body_fld_param = NULL;

  r = mailimap_nil_parse(fd, buffer, &cur_token);
  if (r == MAILIMAP_NO_ERROR) {
    * result = NULL;
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }

  if (r != MAILIMAP_ERROR_PARSE) {
    res = r;
    goto err;
  }

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_string_parse(fd, buffer, &cur_token, &name, NULL,
			    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto string_free;
  }

  r = mailimap_body_fld_param_parse(fd, buffer, &cur_token,
				    &body_fld_param,
				    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto string_free;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto string_free;
  }

  body_fld_dsp = mailimap_body_fld_dsp_new(name, body_fld_param);
  if (body_fld_dsp == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto fld_param_free;
  }

  * index = cur_token;
  * result = body_fld_dsp;

  return MAILIMAP_NO_ERROR;

 fld_param_free:
  if (body_fld_param != NULL)
    mailimap_body_fld_param_free(body_fld_param);
 string_free:
    mailimap_string_free(name);
 err:
  return res;
}

/*
   body-fld-enc    = (DQUOTE ("7BIT" / "8BIT" / "BINARY" / "BASE64"/
                     "QUOTED-PRINTABLE") DQUOTE) / string
*/

static inline int 
mailimap_body_fld_known_enc_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    int * result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  int type;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  type = mailimap_encoding_get_token_value(fd, buffer, &cur_token);

  if (type == -1) {
    res = MAILIMAP_ERROR_PARSE;
    goto err;
  }
  
  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  * result = type;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
  
 err:
  return res;
}     

static int
mailimap_body_fld_enc_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_body_fld_enc ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  size_t cur_token;
  int type;
  char * value;
  struct mailimap_body_fld_enc * body_fld_enc;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_body_fld_known_enc_parse(fd, buffer, &cur_token,
      &type, progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR) {
    value = NULL;
  }
  else if (r == MAILIMAP_ERROR_PARSE) {
    type = MAILIMAP_BODY_FLD_ENC_OTHER;

    r = mailimap_string_parse(fd, buffer, &cur_token, &value, NULL,
			      progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
  }
  else {
    res = r;
    goto err;
  }
  
  body_fld_enc = mailimap_body_fld_enc_new(type, value);
  if (body_fld_enc == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto value_free;
  }
  
  * result = body_fld_enc;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 value_free:
  if (value)
    mailimap_string_free(value);
 err:
  return res;
}

/*
   body-fld-id     = nstring
*/

static int mailimap_body_fld_id_parse(mailstream * fd, MMAPString * buffer,
				      size_t * index, char ** result,
				      size_t progr_rate,
				      progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}


/*
   body-fld-lang   = nstring / "(" string *(SP string) ")"
*/

/*
"(" string *(SP string) ")"
*/

static int
mailimap_body_fld_lang_list_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index, clist ** result,
				  size_t progr_rate,
				  progress_function * progr_fun)
{
  size_t cur_token;
  clist * list;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  list = clist_new();
  if (list == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }
  
  while (1) {
    char * elt;

    r = mailimap_string_parse(fd, buffer, &cur_token, &elt, NULL,
			      progr_rate, progr_fun);
    if (r != MAILIMAP_ERROR_PARSE)
      break;
    else if (r == MAILIMAP_NO_ERROR) {
      r = clist_append(list, elt);
      if (r < 0) {
	mailimap_string_free(elt);
	res = r;
	goto list_free;
      }
    }
    else {
      res = r;
      goto list_free;
    }
  }
  
  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto list_free;
  }

  * index = cur_token;
  * result = list;

  return MAILIMAP_NO_ERROR;

 list_free:
  clist_foreach(list, (clist_func) mailimap_string_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
   body-fld-lang   = nstring / "(" string *(SP string) ")"
*/

static int
mailimap_body_fld_lang_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_body_fld_lang ** result,
			     size_t progr_rate,
			     progress_function * progr_fun)
{
  char * value;
  clist * list;
  struct mailimap_body_fld_lang * fld_lang;
  int type;
  int r;
  int res;

  size_t cur_token;

  cur_token = * index;

  value = NULL;
  list = NULL;
  type = MAILIMAP_BODY_FLD_LANG_ERROR; /* XXX - removes a gcc warning */
  
  r = mailimap_nstring_parse(fd, buffer, &cur_token, &value, NULL,
			     progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_BODY_FLD_LANG_SINGLE;
  
  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_body_fld_lang_list_parse(fd, buffer, &cur_token, &list,
					  progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_BODY_FLD_LANG_LIST;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  fld_lang = mailimap_body_fld_lang_new(type, value, list);
  if (fld_lang == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = fld_lang;
  
  return MAILIMAP_NO_ERROR;

 free:
  if (value)
    mailimap_nstring_free(value);
  if (list) {
    clist_foreach(list, (clist_func) mailimap_string_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}

/*
   body-fld-lines  = number
*/

static int mailimap_body_fld_lines_parse(mailstream * fd,
					 MMAPString * buffer, size_t * index,
					 uint32_t * result)
{
  return mailimap_number_parse(fd, buffer, index, result);
}

/*
   body-fld-md5    = nstring
*/

static int mailimap_body_fld_md5_parse(mailstream * fd, MMAPString * buffer,
				       size_t * index, char ** result,
				       size_t progr_rate,
				       progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}

/*
   body-fld-octets = number
*/

static int mailimap_body_fld_octets_parse(mailstream * fd,
					  MMAPString * buffer, size_t * index,
					  uint32_t * result)
{
  return mailimap_number_parse(fd, buffer, index, result);
}

/*
   body-fld-param  = "(" string SP string *(SP string SP string) ")" / nil
*/

/*
  string SP string
*/

static int
mailimap_single_body_fld_param_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index,
				     struct mailimap_single_body_fld_param **
				     result,
				     size_t progr_rate,
				     progress_function * progr_fun)
{
  struct mailimap_single_body_fld_param * param;
  char * name;
  char * value;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;
  
  name = NULL;
  value = NULL;

  r = mailimap_string_parse(fd, buffer, &cur_token, &name, NULL,
			    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_name;
  }
  
  r = mailimap_string_parse(fd, buffer, &cur_token, &value, NULL,
			    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_name;
  }
  
  param = mailimap_single_body_fld_param_new(name, value);
  if (param == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_value;
  }

  * result = param;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_value:
  mailimap_string_free(name);
 free_name:
  mailimap_string_free(value);
 err:
  return res;
}

/*
   body-fld-param  = "(" string SP string *(SP string SP string) ")" / nil
*/

static int
mailimap_body_fld_param_parse(mailstream * fd,
			      MMAPString * buffer, size_t * index,
			      struct mailimap_body_fld_param ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;
  clist * param_list;
  struct mailimap_body_fld_param * fld_param;
  int r;
  int res;

  param_list = NULL;
  cur_token = * index;

  r = mailimap_nil_parse(fd, buffer, &cur_token);
  if (r == MAILIMAP_NO_ERROR) {
    * result = NULL;
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }

  if (r != MAILIMAP_ERROR_PARSE) {
    res = r;
    goto err;
  }

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &param_list,
					(mailimap_struct_parser *)
					mailimap_single_body_fld_param_parse,
					(mailimap_struct_destructor *)
					mailimap_single_body_fld_param_free,
					progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  fld_param = mailimap_body_fld_param_new(param_list);
  if (fld_param == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = fld_param;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(param_list,
		(clist_func) mailimap_single_body_fld_param_free,
		NULL);
  clist_free(param_list);
 err:
  return res;
}

/*
   body-type-1part = (body-type-basic / body-type-msg / body-type-text)
                     [SP body-ext-1part]
*/

static int
mailimap_body_type_1part_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body_type_1part ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_body_type_1part * body_type_1part;
  struct mailimap_body_type_basic * body_type_basic;
  struct mailimap_body_type_msg * body_type_msg;
  struct mailimap_body_type_text * body_type_text;
  struct mailimap_body_ext_1part * body_ext_1part;
  int type;
  size_t final_token;
  int r;
  int res;
  
  cur_token = * index;

  body_type_basic = NULL;
  body_type_msg = NULL;
  body_type_text = NULL;
  body_ext_1part = NULL;
  
  type = MAILIMAP_BODY_TYPE_1PART_ERROR; /* XXX - removes a gcc warning */
  
  r = mailimap_body_type_msg_parse(fd, buffer, &cur_token,
				   &body_type_msg,
				   progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_BODY_TYPE_1PART_MSG;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_body_type_text_parse(fd, buffer, &cur_token,
				      &body_type_text,
				      progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_BODY_TYPE_1PART_TEXT;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_body_type_basic_parse(fd, buffer, &cur_token,
				       &body_type_basic,
				       progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_BODY_TYPE_1PART_BASIC;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  final_token = cur_token;
  body_ext_1part = NULL;

  r = mailimap_space_parse(fd, buffer, &cur_token);

  if (r == MAILIMAP_NO_ERROR) {
    r = mailimap_body_ext_1part_parse(fd, buffer, &cur_token, &body_ext_1part,
				      progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      final_token = cur_token;
    else if (r == MAILIMAP_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto free;
    }
  }
  else if (r == MAILIMAP_ERROR_PARSE) {
    /* do nothing */
  }
  else {
    res = r;
    goto free;
  }

  body_type_1part = mailimap_body_type_1part_new(type, body_type_basic,
						 body_type_msg, body_type_text,
						 body_ext_1part);
  if (body_type_1part == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = final_token;
  * result = body_type_1part;

  return MAILIMAP_NO_ERROR;

 free:
  if (body_type_basic)
    mailimap_body_type_basic_free(body_type_basic);
  if (body_type_msg)
    mailimap_body_type_msg_free(body_type_msg);
  if (body_type_text)
    mailimap_body_type_text_free(body_type_text);
  if (body_ext_1part)
    mailimap_body_ext_1part_free(body_ext_1part);
 err:
  return res;
}

/*
   body-type-basic = media-basic SP body-fields
                       ; MESSAGE subtype MUST NOT be "RFC822"
*/

static int
mailimap_body_type_basic_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body_type_basic ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_body_type_basic * body_type_basic;
  struct mailimap_media_basic * media_basic;
  struct mailimap_body_fields * body_fields;
  int r;
  int res;

  cur_token = * index;

  media_basic = NULL;
  body_fields = NULL;

  r = mailimap_media_basic_parse(fd, buffer, &cur_token, &media_basic,
				 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_media_basic;
  }

  r = mailimap_body_fields_parse(fd, buffer, &cur_token, &body_fields,
				 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_media_basic;
  }

  body_type_basic = mailimap_body_type_basic_new(media_basic, body_fields);
  if (body_type_basic == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_body_fields;
  }

  * index = cur_token;
  * result = body_type_basic;

  return MAILIMAP_NO_ERROR;

 free_body_fields:
  mailimap_body_fields_free(body_fields);
 free_media_basic:
  mailimap_media_basic_free(media_basic);
 err:
  return res;
}

/*
   body-type-mpart = 1*body SP media-subtype
                     [SP body-ext-mpart]
*/

static int
mailimap_body_type_mpart_parse(mailstream * fd,
			       MMAPString * buffer,
			       size_t * index,
			       struct mailimap_body_type_mpart ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  struct mailimap_body_type_mpart * body_type_mpart;
  clist * body_list;
  size_t cur_token;
  size_t final_token;
  char * media_subtype;
  struct mailimap_body_ext_mpart * body_ext_mpart;
  int r;
  int res;
  
  cur_token = * index;

  body_list = NULL;
  media_subtype = NULL;
  body_ext_mpart = NULL;

  r = mailimap_struct_multiple_parse(fd, buffer, &cur_token,
				     &body_list,
				     (mailimap_struct_parser *)
				     mailimap_body_parse,
				     (mailimap_struct_destructor *)
				     mailimap_body_free,
				     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_body_list;
  }

  r = mailimap_media_subtype_parse(fd, buffer, &cur_token, &media_subtype,
				   progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_body_list;
  }

  final_token = cur_token;

  body_ext_mpart = NULL;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r == MAILIMAP_NO_ERROR) {
    r = mailimap_body_ext_mpart_parse(fd, buffer, &cur_token, &body_ext_mpart,
				      progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      final_token = cur_token;
    else if (r == MAILIMAP_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto free_body_list;
    }
  }
  else if (r == MAILIMAP_ERROR_PARSE) {
    /* do nothing */
  }
  else {
    res = r;
    goto free_body_list;
  }

  body_type_mpart = mailimap_body_type_mpart_new(body_list, media_subtype,
						 body_ext_mpart);
  if (body_type_mpart == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_body_ext_mpart;
  }
  
  * result = body_type_mpart;
  * index = final_token;

  return MAILIMAP_NO_ERROR;
  
 free_body_ext_mpart:
  if (body_ext_mpart)
    mailimap_body_ext_mpart_free(body_ext_mpart);
  mailimap_media_subtype_free(media_subtype);
 free_body_list:
  clist_foreach(body_list, (clist_func) mailimap_body_free, NULL);
  clist_free(body_list);
 err:
  return res;
}

/*
   body-type-msg   = media-message SP body-fields SP envelope
                     SP body SP body-fld-lines
*/

static int
mailimap_body_type_msg_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_body_type_msg ** result,
			     size_t progr_rate,
			     progress_function * progr_fun)
{
  struct mailimap_body_fields * body_fields;
  struct mailimap_envelope * envelope;
  struct mailimap_body * body;
  uint32_t body_fld_lines;
  struct mailimap_body_type_msg * body_type_msg;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  body_fields = NULL;
  envelope = NULL;
  body = NULL;
  body_fld_lines = 0;

  r = mailimap_media_message_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_body_fields_parse(fd, buffer, &cur_token, &body_fields,
				 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto body_fields;
  }

  r = mailimap_envelope_parse(fd, buffer, &cur_token, &envelope,
			      progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto body_fields;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto envelope;
  }

  r = mailimap_body_parse(fd, buffer, &cur_token, &body,
			  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto envelope;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto body;
  }

  r = mailimap_body_fld_lines_parse(fd, buffer, &cur_token,
				    &body_fld_lines);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto body;
  }

  body_type_msg = mailimap_body_type_msg_new(body_fields, envelope,
					     body, body_fld_lines);
  if (body_type_msg == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto body;
  }

  * result = body_type_msg;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 body:
  mailimap_body_free(body);
 envelope:
  mailimap_envelope_free(envelope);
 body_fields:
  mailimap_body_fields_free(body_fields);
 err:
  return res;
}

/*
   body-type-text  = media-text SP body-fields SP body-fld-lines
*/

static int
mailimap_body_type_text_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_body_type_text **
			      result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  char * media_text;
  struct mailimap_body_fields * body_fields;
  uint32_t body_fld_lines;
  struct mailimap_body_type_text * body_type_text;
  size_t cur_token;
  int r;
  int res;

  media_text = NULL;
  body_fields = NULL;
  body_fld_lines = 0;
  
  cur_token = * index;

  r = mailimap_media_text_parse(fd, buffer, &cur_token, &media_text,
				progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_media_text;
  }

  r = mailimap_body_fields_parse(fd, buffer, &cur_token, &body_fields,
				 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_media_text;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_body_fields;
  }

  r = mailimap_body_fld_lines_parse(fd, buffer, &cur_token, &body_fld_lines);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_body_fields;
  }

  body_type_text = mailimap_body_type_text_new(media_text, body_fields,
					       body_fld_lines);
  if (body_type_text == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_body_fields;
  }

  * result = body_type_text;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_body_fields:
  mailimap_body_fields_free(body_fields);
 free_media_text:
  mailimap_media_text_free(media_text);
 err:
  return res;
}


/*
   capability      = ("AUTH=" auth-type) / atom
                       ; New capabilities MUST begin with "X" or be
                       ; registered with IANA as standard or
                       ; standards-track
*/

static int
mailimap_capability_parse(mailstream * fd, MMAPString * buffer,
			  size_t * index,
			  struct mailimap_capability ** result,
			  size_t progr_rate,
			  progress_function * progr_fun)
{
  size_t cur_token;
  int type;
  char * auth_type;
  char * atom;
  struct mailimap_capability * cap;
  int r;
  int res;

  cur_token = * index;

  auth_type = NULL;
  atom = NULL;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "AUTH=");
  switch (r) {
  case MAILIMAP_NO_ERROR:
    type = MAILIMAP_CAPABILITY_AUTH_TYPE;

    r = mailimap_auth_type_parse(fd, buffer, &cur_token, &auth_type,
				 progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
    break;

  case MAILIMAP_ERROR_PARSE:
    r = mailimap_atom_parse(fd, buffer, &cur_token, &atom,
			    progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }

    type = MAILIMAP_CAPABILITY_NAME;
    break;

  default:
    res = r;
    goto err;
  }

  cap = mailimap_capability_new(type, auth_type, atom);
  if (cap == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = cap;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (auth_type)
    mailimap_auth_type_free(auth_type);
  if (atom)
    mailimap_atom_free(atom);
 err:
  return res;
}

/*
   capability-data = "CAPABILITY" *(SP capability) SP "IMAP4rev1"
                     *(SP capability)
                       ; IMAP4rev1 servers which offer RFC 1730
                       ; compatibility MUST list "IMAP4" as the first
                       ; capability.
*/

/*
 SP capability *(SP capability)
*/

static int mailimap_capability_list_parse(mailstream * fd,
					  MMAPString * buffer,
					  size_t * index,
					  clist ** result,
					  size_t progr_rate,
					  progress_function * progr_fun)
{
  size_t cur_token;
  clist * list;
  int r;
  
  cur_token = * index;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &list,
					(mailimap_struct_parser *)
					mailimap_capability_parse,
					(mailimap_struct_destructor *)
					mailimap_capability_free,
					progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * index = cur_token;
  * result = list;

  return MAILIMAP_NO_ERROR;
}

static int
mailimap_capability_data_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_capability_data ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  size_t cur_token;
  clist * cap_list;
#if 0
  clist * cap_list_2;
#endif
  struct mailimap_capability_data * cap_data;
  int r;
  int res;

  cur_token = * index;

  cap_list = NULL;
#if 0
  cap_list_2 = NULL;
#endif

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "CAPABILITY");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_capability_list_parse(fd, buffer, &cur_token,
				     &cap_list,
				     progr_rate, progr_fun);

  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto err;
  }

#if 0
  if (!mailimap_space_parse(fd, buffer, &cur_token)) {
    res = r;
    goto free_list;
  }

  if (!mailimap_token_case_insensitive_parse(fd, buffer,
					     &cur_token, "IMAP4rev1"))
    goto free_list;

  r = mailimap_capability_list_parse(fd, buffer, &cur_token,
				     &cap_list_2,
				     progr_rate, progr_fun);

  cap_list = g_list_concat(cap_list, cap_list_2);
#endif

  cap_data = mailimap_capability_data_new(cap_list);
  if (cap_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_list;
  }

  * result = cap_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_list:
  if (cap_list) {
    clist_foreach(cap_list, (clist_func) mailimap_capability_free, NULL);
    clist_free(cap_list);
  }
 err:
  return res;
}     

/*
  UNIMPLEMENTED BECAUSE UNUSED (only in literal)
   CHAR8           = %x01-ff
                       ; any OCTET except NUL, %x00
*/

/*
static gboolean is_char8(gchar ch)
{
  return (ch != 0x00);
}
*/


/*
UNIMPLEMENTED
   command         = tag SP (command-any / command-auth / command-nonauth /
                     command-select) CRLF
                       ; Modal based on state
*/

/*
UNIMPLEMENTED
   command-any     = "CAPABILITY" / "LOGOUT" / "NOOP" / x-command
                       ; Valid in all states
*/

/*
UNIMPLEMENTED
   command-auth    = append / create / delete / examine / list / lsub /
                     rename / select / status / subscribe / unsubscribe
                       ; Valid only in Authenticated or Selected state
*/

/*
UNIMPLEMENTED
   command-nonauth = login / authenticate
                       ; Valid only when in Not Authenticated state
*/

/*
UNIMPLEMENTED
   command-select  = "CHECK" / "CLOSE" / "EXPUNGE" / copy / fetch / store /
                     uid / search
                       ; Valid only when in Selected state
*/

/*
   continue-req    = "+" SP (resp-text / base64) CRLF
*/

int
mailimap_continue_req_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_continue_req ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  struct mailimap_resp_text * resp_text;
  size_t cur_token;
  struct mailimap_continue_req * cont_req;
  char * base64;
  int type;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_plus_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  resp_text = NULL;
  base64 = NULL;

  type = MAILIMAP_CONTINUE_REQ_ERROR; /* XXX - removes a gcc warning */
  
  r = mailimap_base64_parse(fd, buffer, &cur_token, &base64,
      progr_rate, progr_fun);

  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_CONTINUE_REQ_BASE64;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_resp_text_parse(fd, buffer, &cur_token, &resp_text,
        progr_rate, progr_fun);

    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_CONTINUE_REQ_TEXT;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_crlf_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  cont_req = mailimap_continue_req_new(type, resp_text, base64);
  if (cont_req == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }
  
  * result = cont_req;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (base64 != NULL)
    mailimap_base64_free(base64);
  if (resp_text != NULL)
    mailimap_resp_text_free(resp_text);
 err:
  return res;
}

/*
  UNIMPLEMENTED
   copy            = "COPY" SP set SP mailbox
*/

/*
  UNIMPLEMENTED
   create          = "CREATE" SP mailbox
                       ; Use of INBOX gives a NO error
*/

/*
  UNIMPLEMENTED
   date            = date-text / DQUOTE date-text DQUOTE
*/

/*
  UNIMPLEMENTED
   date-day        = 1*2DIGIT
                       ; Day of month
*/

/*
static gboolean mailimap_date_day_parse(mailstream * fd,
                                        MMAPString * buffer,
					guint32 * index,
					gint * result)
{
  guint32 cur_token;
  gint digit;
  gint number;

  cur_token = * index;
  
  if (!mailimap_digit_parse(fd, buffer, &cur_token, &digit))
    return FALSE;

  number = digit;

  if (mailimap_digit_parse(fd, buffer, &cur_token, &digit))
    number = number * 10 + digit;
  
  * result = number;
  * index = cur_token;

  return TRUE;
}
*/

/*
   date-day-fixed  = (SP DIGIT) / 2DIGIT
                       ; Fixed-format version of date-day
*/

static int mailimap_date_day_fixed_parse(mailstream * fd,
					 MMAPString * buffer,
					 size_t * index,
					 int * result)
{
#ifdef UNSTRICT_SYNTAX
  size_t cur_token;
  uint32_t day;
  int r;

  cur_token = * index;

  r = mailimap_number_parse(fd, buffer, &cur_token, &day);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * index = cur_token;
  * result = day;

  return MAILIMAP_NO_ERROR;

#else
  size_t cur_token;
  int r;

  cur_token = * index;

  if (mailimap_space_parse(fd, buffer, &cur_token)) {
    int digit;

    r = mailimap_digit_parse(fd, buffer, &cur_token, &digit);
    if (r != MAILIMAP_NO_ERROR)
      return r;

    * result = digit;
    * index = cur_token;

    return MAILIMAP_NO_ERROR;
  }
  else {
    int digit1;
    int digit2;

    r = mailimap_digit_parse(fd, buffer, &cur_token, &digit1);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    r = mailimap_digit_parse(fd, buffer, &cur_token, &digit2);
    if (r != MAILIMAP_NO_ERROR)
      return r;

    * result = digit1 * 10 + digit2;
    * index = cur_token;

    return MAILIMAP_NO_ERROR;
  }
#endif
}


/*
   date-month      = "Jan" / "Feb" / "Mar" / "Apr" / "May" / "Jun" /
                     "Jul" / "Aug" / "Sep" / "Oct" / "Nov" / "Dec"
*/

static int mailimap_date_month_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index, int * result)
{
  size_t cur_token;
  int month;

  cur_token = * index;

  month = mailimap_month_get_token_value(fd, buffer, &cur_token);
  if (month == -1)
    return MAILIMAP_ERROR_PARSE;

  * result = month;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  UNIMPLEMENTED
   date-text       = date-day "-" date-month "-" date-year
*/

/*
static struct mailimap_date_text *
mailimap_date_text_new(gint day, gint month, gint year)
{
  struct mailimap_date_text * date_text;
  
  date_text = g_new(struct mailimap_date_text, 1);
  if (date_text == NULL)
    return NULL;

  date_text->day = day;
  date_text->month = month;
  date_text->year = year;

  return date_text;
}

static void mailimap_date_text_free(struct mailimap_date_text * date_text)
{
  g_free(date_text);
}

static gboolean
mailimap_date_text_parse(mailstream * fd, MMAPString * buffer,
			 guint32 * index, struct mailimap_date_text ** result)
{
  struct mailimap_date_text * date_text;
  gint day;
  gint month;
  gint year;
  guint32 cur_token;

  cur_token = * index;

  if (!mailimap_date_day_parse(fd, buffer, &cur_token, &day))
    return FALSE;

  if (!mailimap_minus_parse(fd, buffer, &cur_token))
    return FALSE;

  if (!mailimap_date_month_parse(fd, buffer, &cur_token, &month))
    return FALSE;

  if (!mailimap_minus_parse(fd, buffer, &cur_token))
    return FALSE;

  if (!mailimap_date_year_parse(fd, buffer, &cur_token, &year))
    return FALSE;

  date_text = mailimap_date_text_new(day, month, year);
  if (date_text == NULL)
    return FALSE;

  * result = date_text;
  * index = cur_token;

  return TRUE;
}
*/

/*
   date-year       = 4DIGIT
*/

static int mailimap_date_year_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index, int * result)
{
#ifdef UNSTRICT_SYNTAX
  uint32_t year;
  int r;
  size_t cur_token;
  
  cur_token = * index;
  
  r = mailimap_number_parse(fd, buffer, &cur_token, &year);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = year;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
#else
  int i;
  size_t cur_token;
  int year;
  int digit;
  int r;

  cur_token = * index;
  year = 0;

  for(i = 0 ; i < 4 ; i ++) {
    r = mailimap_digit_parse(fd, buffer, &cur_token, &digit);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    year = year * 10 + digit;
  }

  * result = year;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
#endif
}

/*
   date-time       = DQUOTE date-day-fixed "-" date-month "-" date-year
                     SP time SP zone DQUOTE
*/

static int mailimap_date_time_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    struct mailimap_date_time ** result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  int day;
  int month;
  int year;
  int hour;
  int min;
  int sec;
  struct mailimap_date_time * date_time;
  size_t cur_token;
  int zone;
  int r;

  cur_token = * index;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_date_day_fixed_parse(fd, buffer, &cur_token, &day);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_minus_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_date_month_parse(fd, buffer, &cur_token, &month);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_minus_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_date_year_parse(fd, buffer, &cur_token, &year);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_time_parse(fd, buffer, &cur_token, &hour, &min, &sec);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_zone_parse(fd, buffer, &cur_token, &zone);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  date_time = mailimap_date_time_new(day, month, year, hour, min, sec, zone);
  if (date_time == NULL)
    return MAILIMAP_ERROR_MEMORY;

  * result = date_time;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
  UNIMPLEMENTED
   delete          = "DELETE" SP mailbox
                       ; Use of INBOX gives a NO error
*/

/*
   digit-nz        = %x31-39
                       ; 1-9
*/

#ifndef UNSTRICT_SYNTAX
static int is_digit_nz(char ch)
{
  return (ch >= '1') && (ch <= '9');
}

static int mailimap_digit_nz_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, int * result)
{
  size_t cur_token;

  cur_token = * index;

  if (is_digit_nz(buffer->str[cur_token])) {
    * result = buffer->str[cur_token] - '0';
    cur_token ++;
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }
  else
    return MAILIMAP_ERROR_PARSE;
}
#endif

/*
   envelope        = "(" env-date SP env-subject SP env-from SP env-sender SP
                     env-reply-to SP env-to SP env-cc SP env-bcc SP
                     env-in-reply-to SP env-message-id ")"
*/

static int mailimap_envelope_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index,
				   struct mailimap_envelope ** result,
				   size_t progr_rate,
				   progress_function * progr_fun)
{
  size_t cur_token;
  char * date;
  char * subject;
  struct mailimap_env_from * from;
  struct mailimap_env_sender * sender;
  struct mailimap_env_reply_to * reply_to;
  struct mailimap_env_to * to;
  struct mailimap_env_cc * cc;
  struct mailimap_env_bcc * bcc;
  char * in_reply_to;
  char * message_id;
  struct mailimap_envelope * envelope;
  int r;
  int res;
  
  date = NULL;
  subject = NULL;
  from = NULL;
  sender = NULL;
  reply_to = NULL;
  to = NULL;
  cc = NULL;
  bcc = NULL;
  in_reply_to = NULL;
  message_id = NULL;

  cur_token = * index;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_env_date_parse(fd, buffer, &cur_token, &date,
			      progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto date;
  }

  r = mailimap_env_subject_parse(fd, buffer, &cur_token, &subject,
				 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto date;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto subject;
  }

  r = mailimap_env_from_parse(fd, buffer, &cur_token, &from,
			      progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto subject;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto from;
  }

  r = mailimap_env_sender_parse(fd, buffer, &cur_token, &sender,
				progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto from;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto sender;
  }

  r = mailimap_env_reply_to_parse(fd, buffer, &cur_token, &reply_to,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto sender;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto reply_to;
  }

  r = mailimap_env_to_parse(fd, buffer, &cur_token, &to,
			    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto reply_to;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto to;
  }

  r = mailimap_env_cc_parse(fd, buffer, &cur_token, &cc,
			    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto to;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto cc;
  }

  r = mailimap_env_bcc_parse(fd, buffer, &cur_token, &bcc,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto cc;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto bcc;
  }

  r = mailimap_env_in_reply_to_parse(fd, buffer, &cur_token, &in_reply_to,
				     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto bcc;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto in_reply_to;
  }

  r = mailimap_env_message_id_parse(fd, buffer, &cur_token, &message_id,
				    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto in_reply_to;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto message_id;
  }

  envelope = mailimap_envelope_new(date, subject, from, sender, reply_to, to,
				   cc, bcc, in_reply_to, message_id);
  if (envelope == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto message_id;
  }

  * result = envelope;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 message_id:
  mailimap_env_message_id_free(message_id);
 in_reply_to:
  mailimap_env_in_reply_to_free(in_reply_to);
 bcc:
  mailimap_env_bcc_free(bcc);
 cc:
  mailimap_env_cc_free(cc);
 to:
  mailimap_env_to_free(to);
 reply_to:
  mailimap_env_reply_to_free(reply_to);
 sender:
  mailimap_env_sender_free(sender);
 from:
  mailimap_env_from_free(from);
 subject:
  mailimap_env_subject_free(date);
 date:
  mailimap_env_date_free(date);
 err:
  return res;
}

/*
  "(" 1*address ")"
*/

static int mailimap_address_list_parse(mailstream * fd, MMAPString * buffer,
				       size_t * index,
				       clist ** result,
				       size_t progr_rate,
				       progress_function * progr_fun)
{
  size_t cur_token;
  clist * address_list;
  int r;
  int res;
  
  cur_token = * index;

  address_list = NULL;

  r = mailimap_nil_parse(fd, buffer, &cur_token);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    address_list = NULL;
    break;
 
  case MAILIMAP_ERROR_PARSE:
    r = mailimap_oparenth_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
				   
    r = mailimap_struct_multiple_parse(fd, buffer, &cur_token, &address_list,
				       (mailimap_struct_parser *)
				       mailimap_address_parse,
				       (mailimap_struct_destructor *)
				       mailimap_address_free,
				       progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
    
    r = mailimap_cparenth_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto address_list;
    }

    break;
    
  default:
    res = r;
    goto err;
  }

  * result = address_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 address_list:
  if (address_list) {
    clist_foreach(address_list, (clist_func) mailimap_address_free, NULL);
    clist_free(address_list);
  }
 err:
  return res;
}

/*
   env-bcc         = "(" 1*address ")" / nil
*/

static int
mailimap_env_bcc_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index, struct mailimap_env_bcc ** result,
		       size_t progr_rate,
		       progress_function * progr_fun)
{
  clist * list;
  size_t cur_token;
  struct mailimap_env_bcc * env_bcc;
  int r;
  int res;

  cur_token = * index;
  list = NULL;

  r = mailimap_address_list_parse(fd, buffer, &cur_token, &list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  env_bcc = mailimap_env_bcc_new(list);
  if (env_bcc == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = env_bcc;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimap_address_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
   env-cc          = "(" 1*address ")" / nil
*/

static int
mailimap_env_cc_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, struct mailimap_env_cc ** result,
		      size_t progr_rate,
		      progress_function * progr_fun)
{
  clist * list;
  size_t cur_token;
  struct mailimap_env_cc * env_cc;
  int r;
  int res;

  cur_token = * index;
  list = NULL;

  r = mailimap_address_list_parse(fd, buffer, &cur_token, &list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  env_cc = mailimap_env_cc_new(list);
  if (env_cc == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = env_cc;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimap_address_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
   env-date        = nstring
*/

static int mailimap_env_date_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, char ** result,
				   size_t progr_rate,
				   progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}

/*
   env-from        = "(" 1*address ")" / nil
*/

static int
mailimap_env_from_parse(mailstream * fd, MMAPString * buffer,
			size_t * index, struct mailimap_env_from ** result,
			size_t progr_rate,
			progress_function * progr_fun)
{
  clist * list;
  size_t cur_token;
  struct mailimap_env_from * env_from;
  int r;
  int res;

  cur_token = * index;
  list = NULL;
  
  r = mailimap_address_list_parse(fd, buffer, &cur_token, &list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  env_from = mailimap_env_from_new(list);
  if (env_from == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = env_from;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimap_address_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
   env-in-reply-to = nstring
*/

static int mailimap_env_in_reply_to_parse(mailstream * fd,
					  MMAPString * buffer,
					  size_t * index, char ** result,
					  size_t progr_rate,
					  progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}

/*
   env-message-id  = nstring
*/

static int mailimap_env_message_id_parse(mailstream * fd,
					 MMAPString * buffer,
					 size_t * index, char ** result,
					 size_t progr_rate,
					 progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}

/*
   env-reply-to    = "(" 1*address ")" / nil
*/

static int
mailimap_env_reply_to_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_env_reply_to ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  clist * list;
  size_t cur_token;
  struct mailimap_env_reply_to * env_reply_to;
  int r;
  int res;

  cur_token = * index;
  list = NULL;
  
  r = mailimap_address_list_parse(fd, buffer, &cur_token, &list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  env_reply_to = mailimap_env_reply_to_new(list);
  if (env_reply_to == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = env_reply_to;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimap_address_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
   env-sender      = "(" 1*address ")" / nil
*/


static int
mailimap_env_sender_parse(mailstream * fd, MMAPString * buffer,
			  size_t * index, struct mailimap_env_sender ** result,
			  size_t progr_rate,
			  progress_function * progr_fun)
{
  clist * list;
  size_t cur_token;
  struct mailimap_env_sender * env_sender;
  int r;
  int res;

  cur_token = * index;
  list = NULL;
  
  r = mailimap_address_list_parse(fd, buffer, &cur_token, &list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  env_sender = mailimap_env_sender_new(list);
  if (env_sender == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = env_sender;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimap_address_free, NULL);
  clist_free(list);
 err:
  return res;
}


/*
   env-subject     = nstring
*/

static int mailimap_env_subject_parse(mailstream * fd, MMAPString * buffer,
				      size_t * index, char ** result,
				      size_t progr_rate,
				      progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
				progr_rate, progr_fun);
}


/*
   env-to          = "(" 1*address ")" / nil
*/

static int mailimap_env_to_parse(mailstream * fd, MMAPString * buffer,
				 size_t * index,
				 struct mailimap_env_to ** result,
				 size_t progr_rate,
				 progress_function * progr_fun)
{
  clist * list;
  size_t cur_token;
  struct mailimap_env_to * env_to;
  int r;
  int res;

  cur_token = * index;
  list = NULL;
  
  r = mailimap_address_list_parse(fd, buffer, &cur_token, &list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  env_to = mailimap_env_to_new(list);
  if (env_to == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = env_to;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimap_address_free, NULL);
  clist_free(list);
 err:
  return res;
}


/*
  UNIMPLEMENTED
   examine         = "EXAMINE" SP mailbox
*/

/*
  UNIMPLEMENTED
   fetch           = "FETCH" SP set SP ("ALL" / "FULL" / "FAST" / fetch-att /
                     "(" fetch-att *(SP fetch-att) ")")
*/

/*
  UNIMPLEMENTED
   fetch-att       = "ENVELOPE" / "FLAGS" / "INTERNALDATE" /
                     "RFC822" [".HEADER" / ".SIZE" / ".TEXT"] /
                     "BODY" ["STRUCTURE"] / "UID" /
                     "BODY" [".PEEK"] section ["<" number "." nz-number ">"]
*/

/*
   flag            = "\Answered" / "\Flagged" / "\Deleted" /
                     "\Seen" / "\Draft" / flag-keyword / flag-extension
                       ; Does not include "\Recent"
*/

static int mailimap_flag_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_flag ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  struct mailimap_flag * flag;
  size_t cur_token;
  char * flag_keyword;
  char * flag_extension;
  int type;
  int r;
  int res;
  
  cur_token = * index;

  flag_keyword = NULL;
  flag_extension = NULL;

  type = mailimap_flag_get_token_value(fd, buffer, &cur_token);
  if (type == -1) {
    r = mailimap_flag_keyword_parse(fd, buffer, &cur_token, &flag_keyword,
				    progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_FLAG_KEYWORD;

    if (r == MAILIMAP_ERROR_PARSE) {
      r = mailimap_flag_extension_parse(fd, buffer, &cur_token, 
					&flag_extension,
					progr_rate, progr_fun);
      type = MAILIMAP_FLAG_EXTENSION;
    }

    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  flag = mailimap_flag_new(type, flag_keyword, flag_extension);
  if (flag == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = flag;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (flag_keyword != NULL)
    mailimap_flag_keyword_free(flag_keyword);
  if (flag_extension != NULL)
    mailimap_flag_extension_free(flag_extension);
 err:
  return res;
}

/*
   flag-extension  = "\" atom
                       ; Future expansion.  Client implementations
                       ; MUST accept flag-extension flags.  Server
                       ; implementations MUST NOT generate
                       ; flag-extension flags except as defined by
                       ; future standard or standards-track
                       ; revisions of this specification.
*/

static int mailimap_flag_extension_parse(mailstream * fd,
					 MMAPString * buffer,
					 size_t * index,
					 char ** result,
					 size_t progr_rate,
					 progress_function * progr_fun)
{
  size_t cur_token;
  char * atom;
  int r;
  
  cur_token = * index;

  r = mailimap_char_parse(fd, buffer, &cur_token, '\\');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_atom_parse(fd, buffer, &cur_token, &atom,
			  progr_rate, progr_fun);
  if (r == MAILIMAP_ERROR_PARSE) {
    /* workaround for binc IMAP */
    r = mailimap_char_parse(fd, buffer, &cur_token, '*');
    if (r == MAILIMAP_NO_ERROR) {
      atom = malloc(2);
      if (atom == NULL)
        return MAILIMAP_ERROR_MEMORY;
      
      atom[0] = '*';
      atom[1] = '\0';
    }
  }
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  * result = atom;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
   flag-fetch      = flag / "\Recent"
*/

static int
mailimap_flag_fetch_parse(mailstream * fd, MMAPString * buffer,
			  size_t * index,
			  struct mailimap_flag_fetch ** result,
			  size_t progr_rate,
			  progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_flag * flag;
  struct mailimap_flag_fetch * flag_fetch;
  int type;
  int r;
  int res;

  cur_token = * index;

  flag = NULL;

  type = MAILIMAP_FLAG_FETCH_ERROR; /* XXX - removes a gcc warning */
  
  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "\\Recent");
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_FLAG_FETCH_RECENT;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_flag_parse(fd, buffer, &cur_token, &flag,
			    progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_FLAG_FETCH_OTHER;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  flag_fetch = mailimap_flag_fetch_new(type, flag);
  if (flag_fetch == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = flag_fetch;

  return MAILIMAP_NO_ERROR;

 free:
  if (flag != NULL)
    mailimap_flag_free(flag);
 err:
  return res;
}

/*
   flag-keyword    = atom
   WARNING : parse astring instead of atom to workaround RockLiffe server :
   http://www.rockliffe.com/
*/

static int mailimap_flag_keyword_parse(mailstream * fd, MMAPString * buffer,
				       size_t * index,
				       char ** result,
				       size_t progr_rate,
				       progress_function * progr_fun)
{
#if 0
  return mailimap_atom_parse(fd, buffer, index, result,
      progr_rate, progr_fun);
#endif
  return mailimap_astring_parse(fd, buffer, index, result,
      progr_rate, progr_fun);
}

/*
   flag-list       = "(" [flag *(SP flag)] ")"
*/

static int mailimap_flag_list_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    struct mailimap_flag_list ** result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  size_t cur_token;
  clist * list;
  struct mailimap_flag_list * flag_list;
  int r;
  int res;

  list = NULL;
  cur_token = * index;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &list,
					(mailimap_struct_parser *)
					mailimap_flag_parse,
					(mailimap_struct_destructor *)
					mailimap_flag_free,
					progr_rate, progr_fun);

  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  flag_list = mailimap_flag_list_new(list);
  if (flag_list == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = flag_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimap_flag_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}

/*
   flag-perm       = flag / "\*"
*/

static int
mailimap_flag_perm_parse(mailstream * fd, MMAPString * buffer,
			 size_t * index,
			 struct mailimap_flag_perm ** result,
			 size_t progr_rate,
			 progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_flag_perm * flag_perm;
  struct mailimap_flag * flag;
  int type;
  int r;
  int res;

  flag = NULL;
  cur_token = * index;
  type = MAILIMAP_FLAG_PERM_ERROR; /* XXX - removes a gcc warning */
  
  r = mailimap_flag_parse(fd, buffer, &cur_token, &flag,
			  progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_FLAG_PERM_FLAG;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_token_case_insensitive_parse(fd, buffer,
					      &cur_token, "\\*");
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_FLAG_PERM_ALL;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  flag_perm = mailimap_flag_perm_new(type, flag);
  if (flag_perm == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = flag_perm;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (flag != NULL)
    mailimap_flag_free(flag);
 err:
  return res;
}

/*
   greeting        = "*" SP (resp-cond-auth / resp-cond-bye) CRLF
*/

int mailimap_greeting_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_greeting ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_resp_cond_auth * resp_cond_auth;
  struct mailimap_resp_cond_bye * resp_cond_bye;
  struct mailimap_greeting * greeting;
  int type;
  int r;
  int res;

  cur_token = * index;
  resp_cond_bye = NULL;
  resp_cond_auth = NULL;

  r = mailimap_star_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  type = MAILIMAP_GREETING_RESP_COND_ERROR; /* XXX - removes a gcc warning */
  
  r = mailimap_resp_cond_auth_parse(fd, buffer, &cur_token, &resp_cond_auth,
				    progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_GREETING_RESP_COND_AUTH;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_resp_cond_bye_parse(fd, buffer, &cur_token,
				     &resp_cond_bye,
				     progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_GREETING_RESP_COND_BYE;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_crlf_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  greeting = mailimap_greeting_new(type, resp_cond_auth, resp_cond_bye);
  if (greeting == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = greeting;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (resp_cond_auth)
    mailimap_resp_cond_auth_free(resp_cond_auth);
  if (resp_cond_bye)
    mailimap_resp_cond_bye_free(resp_cond_bye);
 err:
  return res;
}

/*
   header-fld-name = astring
*/

static int
mailimap_header_fld_name_parse(mailstream * fd,
			       MMAPString * buffer,
			       size_t * index,
			       char ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  return mailimap_astring_parse(fd, buffer, index, result,
				progr_rate, progr_fun);
}

/*
   header-list     = "(" header-fld-name *(SP header-fld-name) ")"
*/

static int
mailimap_header_list_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   struct mailimap_header_list ** result,
			   size_t progr_rate,
			   progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_header_list * header_list;
  clist * list;
  int r;
  int res;

  cur_token = * index;

  list = NULL;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &list,
					(mailimap_struct_parser *)
					mailimap_header_fld_name_parse,
					(mailimap_struct_destructor *)
					mailimap_header_fld_name_free,
					progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  header_list = mailimap_header_list_new(list);
  if (header_list == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = header_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimap_header_fld_name_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
UNIMPLEMENTED
   list            = "LIST" SP mailbox SP list-mailbox

UNIMPLEMENTED
   list-mailbox    = 1*list-char / string

UNIMPLEMENTED
   list-char       = ATOM-CHAR / list-wildcards / resp-specials
*/

/*
   list-wildcards  = "%" / "*"
*/

static int is_list_wildcards(char ch)
{
  switch (ch) {
  case '%':
  case '*':
    return TRUE;
  }
  return FALSE;
}


/*
   literal         = "{" number "}" CRLF *CHAR8
                       ; Number represents the number of CHAR8s
*/

static int mailimap_literal_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index, char ** result,
				  size_t * result_len,
				  size_t progr_rate,
				  progress_function * progr_fun)
{
  size_t cur_token;
  uint32_t number;
  MMAPString * literal;
  char * literal_p;
  uint32_t left;
  int r;
  int res;
  size_t number_token;

  cur_token = * index;

  r = mailimap_oaccolade_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  number_token = cur_token;

  r = mailimap_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_caccolade_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_crlf_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  literal = mmap_string_sized_new(number);
  /*
  literal = g_new(char, number + 1);
  */
  if (literal == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }

  left = buffer->len - cur_token;

  if (left >= number) {
    /*
    if (number > 0)
      strncpy(literal, buffer->str + cur_token, number);
      literal[number] = 0;
    */
    if (number > 0)
      if (mmap_string_append_len(literal, buffer->str + cur_token,
				 number) == NULL) {
	res = MAILIMAP_ERROR_MEMORY;
	goto free_literal;
      }
    if ((progr_fun != NULL) && (progr_rate != 0))
      progr_fun(number, number);

    cur_token = cur_token + number;
  }
  else {
    uint32_t needed;
    uint32_t current_prog = 0;
    uint32_t last_prog = 0;

    needed = number - left;
    memcpy(literal->str, buffer->str + cur_token, left);
    literal->len += left;
    literal_p = literal->str + left;
    current_prog = left;

    while (needed > 0) {
      ssize_t read_bytes;

      read_bytes = mailstream_read(fd, literal_p, needed);
      if (read_bytes == -1) {
	res = MAILIMAP_ERROR_STREAM;
	goto free_literal;
      }
      literal->len += read_bytes;
      needed -= read_bytes;
      literal_p += read_bytes;

      current_prog += read_bytes;
      if ((progr_fun != NULL) && (progr_rate != 0))
	if (current_prog - last_prog > progr_rate) {
	  progr_fun(current_prog, number);
	  last_prog = current_prog;
	}
    }

    literal->str[number] = 0;

#if 0   
    literal->str[number] = 0;
    if (mmap_string_append_len(buffer, literal->str + left,
			       literal->len - left) == NULL) {
      res = MAILIMAP_ERROR_STREAM;
      goto free_literal;
    }
#endif

    if (mmap_string_truncate(buffer, number_token) == NULL) {
      res = MAILIMAP_ERROR_MEMORY;
      goto free_literal;
    }

    if (mmap_string_append(buffer, "0}\r\n") == NULL) {
      res = MAILIMAP_ERROR_MEMORY;
      goto free_literal;
    }

    cur_token = number_token + 4;
  }
  if ((progr_fun != NULL) && (progr_rate != 0))
    progr_fun(number, number);

  if (mailstream_read_line_append(fd, buffer) == NULL) {
    res = MAILIMAP_ERROR_STREAM;
    goto free_literal;
  }

  if (mmap_string_ref(literal) < 0) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_literal;
  }

  * result = literal->str;
  if (result_len != NULL)
    * result_len = literal->len;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_literal:
  mmap_string_free(literal);
 err:
  return res;
}

/*
  UNIMPLEMENTED
   login           = "LOGIN" SP userid SP password

   UNIMPLEMENTED
   lsub            = "LSUB" SP mailbox SP list-mailbox
*/

/*
   mailbox         = "INBOX" / astring
                       ; INBOX is case-insensitive.  All case variants of
                       ; INBOX (e.g. "iNbOx") MUST be interpreted as INBOX
                       ; not as an astring.  An astring which consists of
                       ; the case-insensitive sequence "I" "N" "B" "O" "X"
                       ; is considered to be INBOX and not an astring.
                       ;  Refer to section 5.1 for further
                       ; semantic details of mailbox names.
*/

int
mailimap_mailbox_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index, char ** result,
		       size_t progr_rate,
		       progress_function * progr_fun)
{
  size_t cur_token;
  char * name;
  int r;

  cur_token = * index;

  r = mailimap_astring_parse(fd, buffer, &cur_token, &name,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = name;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
   mailbox-data    =  "FLAGS" SP flag-list / "LIST" SP mailbox-list /
                      "LSUB" SP mailbox-list / "SEARCH" *(SP nz-number) /
                      "STATUS" SP mailbox SP "("
                      [status-att SP number *(SP status-att SP number)] ")" /
                      number SP "EXISTS" / number SP "RECENT"
*/

/*
  "FLAGS" SP flag-list
*/

static int
mailimap_mailbox_data_flags_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index,
				  struct mailimap_flag_list ** result,
				  size_t progr_rate,
				  progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_flag_list * flag_list;
  int r;

  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "FLAGS");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  flag_list = NULL;
  r = mailimap_flag_list_parse(fd, buffer, &cur_token, &flag_list,
			       progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = flag_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
  "LIST" SP mailbox-list
*/

static int
mailimap_mailbox_data_list_parse(mailstream * fd, MMAPString * buffer,
				 size_t * index,
				 struct mailimap_mailbox_list ** result,
				 size_t progr_rate,
				 progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_mailbox_list * mb_list;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "LIST");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    return r;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    return r;
  }

  r = mailimap_mailbox_list_parse(fd, buffer, &cur_token, &mb_list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    return r;
  }

  * result = mb_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "LSUB" SP mailbox-list
*/

static int
mailimap_mailbox_data_lsub_parse(mailstream * fd, MMAPString * buffer,
				 size_t * index,
				 struct mailimap_mailbox_list ** result,
				 size_t progr_rate,
				 progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_mailbox_list * mb_list;
  int r;

  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "LSUB");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_list_parse(fd, buffer, &cur_token, &mb_list,
				  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = mb_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "SEARCH" *(SP nz-number)
*/


static int
mailimap_mailbox_data_search_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index,
				   clist ** result,
				   size_t progr_rate,
				   progress_function * progr_fun)
{
  size_t cur_token;
  size_t final_token;
  clist * number_list;
  int r;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "SEARCH");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  final_token = cur_token;
  number_list = NULL;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r == MAILIMAP_NO_ERROR) {
    r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &number_list,
					  (mailimap_struct_parser *)
					  mailimap_nz_number_alloc_parse,
					  (mailimap_struct_destructor *)
					  mailimap_number_alloc_free,
					  progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      final_token = cur_token;
  }
  
  * result = number_list;
  * index = final_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "STATUS" SP mailbox SP "("
  [status-att SP number *(SP status-att SP number)] ")"
*/

/*
  status-att SP number
*/

static int
mailimap_status_info_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   struct mailimap_status_info **
			   result,
			   size_t progr_rate,
			   progress_function * progr_fun)
{
  size_t cur_token;
  int status_att;
  uint32_t value;
  struct mailimap_status_info * info;
  int r;
  
  cur_token = * index;
  value = 0;

  r = mailimap_status_att_parse(fd, buffer, &cur_token, &status_att);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_number_parse(fd, buffer, &cur_token, &value);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  info = mailimap_status_info_new(status_att, value);
  if (info == NULL)
    return MAILIMAP_ERROR_MEMORY;

  * result = info;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "STATUS" SP mailbox SP "("
  [status-att SP number *(SP status-att SP number)] ")"
*/

static int
mailimap_mailbox_data_status_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, struct
				   mailimap_mailbox_data_status ** result,
				   size_t progr_rate,
				   progress_function * progr_fun)
{
  size_t cur_token;
  char * mb;
  clist * status_info_list;
  struct mailimap_mailbox_data_status * data_status;
  int r;
  int res;

  cur_token = * index;
  mb = NULL;
  status_info_list = NULL;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "STATUS");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_mailbox_parse(fd, buffer, &cur_token, &mb,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto mailbox;
  }

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto mailbox;
  }
  
  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token,
					&status_info_list,
					(mailimap_struct_parser *)
					mailimap_status_info_parse,
					(mailimap_struct_destructor *)
					mailimap_status_info_free,
					progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto mailbox;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto status_info_list;
  }

  data_status = mailimap_mailbox_data_status_new(mb, status_info_list);
  if (data_status == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto status_info_list;
  }

  * result = data_status;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 status_info_list:
  if (status_info_list != NULL) {
    clist_foreach(status_info_list, (clist_func) mailimap_status_info_free,
		  NULL);
    clist_free(status_info_list);
  }
 mailbox:
  mailimap_mailbox_free(mb);
 err:
  return res;
}

/*
  number SP "EXISTS"
*/

static int
mailimap_mailbox_data_exists_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index,
				   uint32_t * result)
{
  size_t cur_token;
  uint32_t number;
  int r;
  
  cur_token = * index;

  r = mailimap_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "EXISTS");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = number;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  number SP "RECENT"
*/

static int
mailimap_mailbox_data_recent_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index,
				   uint32_t * result)
{
  size_t cur_token;
  uint32_t number;
  int r;
  
  cur_token = * index;

  r = mailimap_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "RECENT");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = number;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
   mailbox-data    =  "FLAGS" SP flag-list / "LIST" SP mailbox-list /
                      "LSUB" SP mailbox-list / "SEARCH" *(SP nz-number) /
                      "STATUS" SP mailbox SP "("
                      [status-att SP number *(SP status-att SP number)] ")" /
                      number SP "EXISTS" / number SP "RECENT"
*/

static int
mailimap_mailbox_data_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_mailbox_data ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  int type;
  struct mailimap_flag_list * data_flags;
  struct mailimap_mailbox_list * data_list;
  struct mailimap_mailbox_list * data_lsub;
  clist * data_search; 
  struct mailimap_mailbox_data_status * data_status;
  uint32_t data_exists;
  uint32_t data_recent;
  struct mailimap_extension_data * data_extension;

  struct mailimap_mailbox_data * mailbox_data;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  data_flags = NULL;
  data_list = NULL;
  data_lsub = NULL;
  data_search = NULL;
  data_status = NULL;
  data_exists = 0;
  data_recent = 0;
  data_extension = NULL;

  type = MAILIMAP_MAILBOX_DATA_ERROR; /* XXX - removes a gcc warning */
  
  r = mailimap_mailbox_data_flags_parse(fd, buffer, &cur_token,
					&data_flags,
					progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_MAILBOX_DATA_FLAGS;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_mailbox_data_list_parse(fd, buffer, &cur_token,
					 &data_list,
					 progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MAILBOX_DATA_LIST;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_mailbox_data_lsub_parse(fd, buffer, &cur_token,
					 &data_lsub,
					 progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MAILBOX_DATA_LSUB;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_mailbox_data_search_parse(fd, buffer, &cur_token,
					   &data_search,
					   progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MAILBOX_DATA_SEARCH;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_mailbox_data_status_parse(fd, buffer, &cur_token,
					   &data_status,
					   progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MAILBOX_DATA_STATUS;
  }
  
  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_mailbox_data_exists_parse(fd, buffer, &cur_token,
					   &data_exists);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MAILBOX_DATA_EXISTS;
  }
  
  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_mailbox_data_recent_parse(fd, buffer, &cur_token,
					   &data_recent);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MAILBOX_DATA_RECENT;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_extension_data_parse(MAILIMAP_EXTENDED_PARSER_MAILBOX_DATA,
              fd, buffer, &cur_token, &data_extension, progr_rate,
              progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MAILBOX_DATA_EXTENSION_DATA;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  mailbox_data = mailimap_mailbox_data_new(type, data_flags, data_list,
      data_lsub, data_search,
      data_status,
      data_exists, data_recent,
      data_extension);
  
  if (mailbox_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = mailbox_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (data_flags != NULL)
    mailimap_flag_list_free(data_flags);
  if (data_list != NULL)
    mailimap_mailbox_list_free(data_list);
  if (data_lsub != NULL)
    mailimap_mailbox_list_free(data_lsub);
  if (data_search != NULL)
    mailimap_mailbox_data_search_free(data_search);
  if (data_status != NULL)
    mailimap_mailbox_data_status_free(data_status);
  if (data_extension != NULL)
    mailimap_extension_data_free(data_extension);
 err:
  return res;
}

/*
   mailbox-list    = "(" [mbx-list-flags] ")" SP
                      (DQUOTE QUOTED-CHAR DQUOTE / nil) SP mailbox
*/

/*
  DQUOTE QUOTED-CHAR DQUOTE
*/

static int
mailimap_mailbox_list_quoted_char_parse(mailstream * fd, MMAPString * buffer,
					size_t * index,
					char * result)
{
  size_t cur_token;
  char ch;
  int r;

  cur_token = * index;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_quoted_char_parse(fd, buffer, &cur_token, &ch);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * index = cur_token;
  * result = ch;

  return MAILIMAP_NO_ERROR;
}

static int
mailimap_mailbox_list_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_mailbox_list ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  struct mailimap_mailbox_list * mb_list;
  struct mailimap_mbx_list_flags * mb_flag_list;
  char ch; 
  char * mb;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  mb_flag_list = NULL;
  ch = 0;
  mb = NULL;

  r = mailimap_mbx_list_flags_parse(fd, buffer, &cur_token,
				    &mb_flag_list, progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_list_flags;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_list_flags;
  }

  r = mailimap_mailbox_list_quoted_char_parse(fd, buffer, &cur_token, &ch);
  if (r == MAILIMAP_ERROR_PARSE)
    r = mailimap_nil_parse(fd, buffer, &cur_token);

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_list_flags;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_list_flags;
  }

  r = mailimap_mailbox_parse(fd, buffer, &cur_token, &mb,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_list_flags;
  }

  mb_list = mailimap_mailbox_list_new(mb_flag_list, ch, mb);
  if (mb_list == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_mailbox;
  }
  
  * result = mb_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_mailbox:
  mailimap_mailbox_free(mb);
 free_list_flags:
  if (mb_flag_list != NULL)
    mailimap_mbx_list_flags_free(mb_flag_list);
 err:
  return res;
}

/*
   mbx-list-flags  = *(mbx-list-oflag SP) mbx-list-sflag
                     *(SP mbx-list-oflag) /
                     mbx-list-oflag *(SP mbx-list-oflag)
*/

static int
mailimap_mbx_list_flags_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_mbx_list_flags ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  struct mailimap_mbx_list_flags * mbx_list_flag;
  size_t cur_token;
  clist * oflags;
  clist * oflags_2;
  int sflag;
  int type;
  int r;
  int res;
  size_t final_token;
  int try_sflag;
  
  cur_token = * index;
  final_token = cur_token;
  
  oflags = clist_new();
  if (oflags == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }

  sflag = MAILIMAP_MBX_LIST_SFLAG_ERROR;
  oflags_2 = NULL;

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token,
					&oflags_2,
					(mailimap_struct_parser *)
					mailimap_mbx_list_oflag_no_sflag_parse,
					(mailimap_struct_destructor *)
					mailimap_mbx_list_oflag_free,
					progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto free;
  }
  
  try_sflag = 1;
  if (r == MAILIMAP_NO_ERROR) {
    clist_concat(oflags, oflags_2);
    clist_free(oflags_2);
    
    final_token = cur_token;
    try_sflag = 0;
    r = mailimap_space_parse(fd, buffer, &cur_token);
    if (r == MAILIMAP_NO_ERROR)
      try_sflag = 1;
  }
  
  type = MAILIMAP_MBX_LIST_FLAGS_NO_SFLAG;
  if (try_sflag) {
    r = mailimap_mbx_list_sflag_parse(fd, buffer, &cur_token, &sflag);
    switch (r) {
    case MAILIMAP_ERROR_PARSE:
      type = MAILIMAP_MBX_LIST_FLAGS_NO_SFLAG;
      break;
    
    case MAILIMAP_NO_ERROR:
      type = MAILIMAP_MBX_LIST_FLAGS_SFLAG;
    
      final_token = cur_token;
      r = mailimap_space_parse(fd, buffer, &cur_token);
      if (r == MAILIMAP_NO_ERROR) {
        r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token,
            &oflags_2,
            (mailimap_struct_parser *) mailimap_mbx_list_oflag_parse,
            (mailimap_struct_destructor *) mailimap_mbx_list_oflag_free,
            progr_rate, progr_fun);
        if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
          res = r;
          goto err;
        }
      
        if (r == MAILIMAP_NO_ERROR) {
          clist_concat(oflags, oflags_2);
          clist_free(oflags_2);
          
          final_token = cur_token;
        }
      }
      
      break;

    default:
      res = r;
      goto free;
    }
  }
  
  if ((clist_count(oflags) == 0) && (type == MAILIMAP_MBX_LIST_FLAGS_NO_SFLAG)) {
    res = MAILIMAP_ERROR_PARSE;
    goto free;
  }
  
  cur_token = final_token;
  mbx_list_flag = mailimap_mbx_list_flags_new(type, oflags, sflag);
  if (mbx_list_flag == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = mbx_list_flag;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

free:
  clist_foreach(oflags, (clist_func) mailimap_mbx_list_oflag_free, NULL);
  clist_free(oflags);
err:
  return res;
}

/*
   mbx-list-oflag  = "\Noinferiors" / flag-extension
                       ; Other flags; multiple possible per LIST response
*/

static int
mailimap_mbx_list_oflag_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_mbx_list_oflag ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  int type;
  size_t cur_token;
  struct mailimap_mbx_list_oflag * oflag;
  char * flag_ext;
  int r;
  int res;
  
  cur_token = * index;
  flag_ext = NULL;
  type = MAILIMAP_MBX_LIST_OFLAG_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "\\Noinferiors");
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_MBX_LIST_OFLAG_NOINFERIORS;
  
  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_flag_extension_parse(fd, buffer, &cur_token,
				      &flag_ext, progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MBX_LIST_OFLAG_FLAG_EXT;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  oflag = mailimap_mbx_list_oflag_new(type, flag_ext);
  if (oflag == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = oflag;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (flag_ext != NULL)
    mailimap_flag_extension_free(flag_ext);
 err:
  return res;
}

static int
mailimap_mbx_list_oflag_no_sflag_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    struct mailimap_mbx_list_oflag ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  int sflag_type;
  int r;

  cur_token = * index;
  
  r = mailimap_mbx_list_sflag_parse(fd, buffer, &cur_token, &sflag_type);
  if (r == MAILIMAP_NO_ERROR)
    return MAILIMAP_ERROR_PARSE;
  
  return mailimap_mbx_list_oflag_parse(fd, buffer, index, result,
      progr_rate, progr_fun);
}


/*
   mbx-list-sflag  = "\Noselect" / "\Marked" / "\Unmarked"
                       ; Selectability flags; only one per LIST response
*/

static int
mailimap_mbx_list_sflag_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      int * result)
{
  int type;
  size_t cur_token;

  cur_token = * index;

  type = mailimap_mbx_list_sflag_get_token_value(fd, buffer, &cur_token);
  if (type == -1)
    return MAILIMAP_ERROR_PARSE;

  * result = type;
  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;
}


/*
   media-basic     = ((DQUOTE ("APPLICATION" / "AUDIO" / "IMAGE" / "MESSAGE" /
                     "VIDEO") DQUOTE) / string) SP media-subtype
                       ; Defined in [MIME-IMT]
*/

/*
  DQUOTE ("APPLICATION" / "AUDIO" / "IMAGE" / "MESSAGE" /
  "VIDEO") DQUOTE
*/

static int
mailimap_media_basic_standard_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    int * result)
{
  size_t cur_token;
  int type;
  int r;
  
  cur_token = * index;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  type = mailimap_media_basic_get_token_value(fd, buffer, &cur_token);
  if (type == -1)
    return MAILIMAP_ERROR_PARSE;
    
  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return FALSE;

  * index = cur_token;
  * result = type;
  
  return MAILIMAP_NO_ERROR;
}

/*
   media-basic     = ((DQUOTE ("APPLICATION" / "AUDIO" / "IMAGE" / "MESSAGE" /
                     "VIDEO") DQUOTE) / string) SP media-subtype
                       ; Defined in [MIME-IMT]
*/

static int
mailimap_media_basic_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   struct mailimap_media_basic ** result,
			   size_t progr_rate,
			   progress_function * progr_fun)
{
  size_t cur_token;
  int type;
  char * basic_type;
  char * subtype;
  struct mailimap_media_basic * media_basic;
  int r;
  int res;

  cur_token = * index;

  basic_type = NULL;
  subtype = NULL;

  type = MAILIMAP_MEDIA_BASIC_OTHER;
  r = mailimap_media_basic_standard_parse(fd, buffer, &cur_token,
					  &type);

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_string_parse(fd, buffer, &cur_token, &basic_type, NULL,
			      progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MEDIA_BASIC_OTHER;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_basic_type;
  }

  r = mailimap_media_subtype_parse(fd, buffer, &cur_token, &subtype,
				   progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_basic_type;
  }

  media_basic = mailimap_media_basic_new(type, basic_type, subtype);
  if (media_basic == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_subtype;
  }

  * result = media_basic;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_subtype:
  mailimap_media_subtype_free(subtype);
 free_basic_type:
  if (basic_type != NULL)
    mailimap_string_free(basic_type);
 err:
  return res;
}


/*
   media-message   = DQUOTE "MESSAGE" DQUOTE SP DQUOTE "RFC822" DQUOTE
                       ; Defined in [MIME-IMT]
*/

static int
mailimap_media_message_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "MESSAGE");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "RFC822");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
   media-subtype   = string
                       ; Defined in [MIME-IMT]
*/

static int
mailimap_media_subtype_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     char ** result,
			     size_t progr_rate,
			     progress_function * progr_fun)
{
  return mailimap_string_parse(fd, buffer, index, result, NULL,
			       progr_rate, progr_fun);
}

/*
   media-text      = DQUOTE "TEXT" DQUOTE SP media-subtype
                       ; Defined in [MIME-IMT]
*/

static int mailimap_media_text_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index,
				     char ** result,
				     size_t progr_rate,
				     progress_function * progr_fun)
{
  size_t cur_token;
  char * media_subtype;
  int r;

  cur_token = * index;

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "TEXT");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_media_subtype_parse(fd, buffer, &cur_token, &media_subtype,
				   progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = media_subtype;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
   message-data    = nz-number SP ("EXPUNGE" / ("FETCH" SP msg-att))
*/


static int
mailimap_message_data_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_message_data ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  size_t cur_token;
  uint32_t number;
  int type;
  struct mailimap_msg_att * msg_att;
  struct mailimap_message_data * msg_data;
  int r;
  int res;

  cur_token = * index;
  msg_att = NULL;

  r = mailimap_nz_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  type = MAILIMAP_MESSAGE_DATA_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "EXPUNGE");
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_MESSAGE_DATA_EXPUNGE;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					      "FETCH");
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
    
    r = mailimap_space_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }

    r = mailimap_msg_att_parse(fd, buffer, &cur_token, &msg_att,
			       progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }

    type = MAILIMAP_MESSAGE_DATA_FETCH;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  msg_data = mailimap_message_data_new(number, type, msg_att);
  if (msg_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_msg_att;
  }

  * result = msg_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_msg_att:
  if (msg_att != NULL)
    mailimap_msg_att_free(msg_att);
 err:
  return res;
}

/*
   msg-att         = "(" (msg-att-dynamic / msg-att-static)
                      *(SP (msg-att-dynamic / msg-att-static)) ")"
*/

/*
  msg-att-dynamic / msg-att-static
*/

static int
mailimap_msg_att_item_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_msg_att_item ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  int type;
  struct mailimap_msg_att_dynamic * msg_att_dynamic;
  struct mailimap_msg_att_static * msg_att_static;
  size_t cur_token;
  struct mailimap_msg_att_item * item;
  int r;
  int res;

  cur_token = * index;

  msg_att_dynamic = NULL;
  msg_att_static = NULL;

  type = MAILIMAP_MSG_ATT_ITEM_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_msg_att_dynamic_parse(fd, buffer, &cur_token,
				     &msg_att_dynamic,
				     progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_MSG_ATT_ITEM_DYNAMIC;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_static_parse(fd, buffer, &cur_token,
				      &msg_att_static,
				      progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_ITEM_STATIC;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  item = mailimap_msg_att_item_new(type, msg_att_dynamic, msg_att_static);
  if (item == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = item;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (msg_att_dynamic != NULL)
    mailimap_msg_att_dynamic_free(msg_att_dynamic);
  if (msg_att_static != NULL)
    mailimap_msg_att_static_free(msg_att_static);
 err:
  return res;
}

/*
   msg-att         = "(" (msg-att-dynamic / msg-att-static)
                      *(SP (msg-att-dynamic / msg-att-static)) ")"
*/

static int
mailimap_msg_att_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index, struct mailimap_msg_att ** result,
		       size_t progr_rate,
		       progress_function * progr_fun)
{
  size_t cur_token;
  clist * list;
  struct mailimap_msg_att * msg_att;
  int r;
  int res;

  cur_token = * index;
  list = NULL;

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &list,
					(mailimap_struct_parser *)
					mailimap_msg_att_item_parse,
					(mailimap_struct_destructor *)
					mailimap_msg_att_item_free,
					progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }
  
  msg_att = mailimap_msg_att_new(list);
  if (msg_att == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = msg_att;

  return MAILIMAP_NO_ERROR;
  
 free:
  clist_foreach(list, (clist_func) mailimap_msg_att_item_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
   msg-att-dynamic = "FLAGS" SP "(" [flag-fetch *(SP flag-fetch)] ")"
                       ; MAY change for a message
*/


static int
mailimap_msg_att_dynamic_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_msg_att_dynamic ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  clist * list;
  struct mailimap_msg_att_dynamic * msg_att_dyn;
  size_t cur_token;
  int r;
  int res;
  
  cur_token = * index;

  list = NULL;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "FLAGS");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token,
					&list,
					(mailimap_struct_parser *)
					mailimap_flag_fetch_parse,
					(mailimap_struct_destructor *)
					mailimap_flag_fetch_free,
					progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }
  
  msg_att_dyn = mailimap_msg_att_dynamic_new(list);
  if (msg_att_dyn == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = msg_att_dyn;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimap_flag_fetch_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}

/*
   msg-att-static  = "ENVELOPE" SP envelope / "INTERNALDATE" SP date-time /
                     "RFC822" [".HEADER" / ".TEXT"] SP nstring /
                     "RFC822.SIZE" SP number / "BODY" ["STRUCTURE"] SP body /
                     "BODY" section ["<" number ">"] SP nstring /
                     "UID" SP uniqueid
                       ; MUST NOT change for a message
*/

/*
  "ENVELOPE" SP envelope
*/


static int
mailimap_msg_att_envelope_parse(mailstream * fd,
				MMAPString * buffer,
				size_t * index,
				struct mailimap_envelope ** result,
				size_t progr_rate,
				progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_envelope * env;
  int r;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "ENVELOPE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_envelope_parse(fd, buffer, &cur_token, &env,
			      progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * index = cur_token;
  * result = env;

  return MAILIMAP_NO_ERROR;
}


/*
  "INTERNALDATE" SP date-time
*/


static int
mailimap_msg_att_internaldate_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    struct mailimap_date_time ** result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_date_time * date_time;
  int r;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "INTERNALDATE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return FALSE;

  r = mailimap_date_time_parse(fd, buffer, &cur_token, &date_time,
			       progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = date_time;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "RFC822" SP nstring
*/

static int
mailimap_msg_att_rfc822_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index, char ** result,
			      size_t * result_len,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;
  char * rfc822_message;
  int r;
  size_t length;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "RFC822");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_nstring_parse(fd, buffer, &cur_token, &rfc822_message, &length,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = rfc822_message;
  if (result_len != NULL)
    * result_len = length;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "RFC822" ".HEADER" SP nstring
*/

static int
mailimap_msg_att_rfc822_header_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index, char ** result,
				     size_t * result_len,
				     size_t progr_rate,
				     progress_function * progr_fun)
{
  size_t cur_token;
  char * rfc822_header;
  int r;
  size_t length;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "RFC822");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    ".HEADER");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_nstring_parse(fd, buffer, &cur_token, &rfc822_header, &length,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = rfc822_header;
  if (result_len != NULL)
    * result_len = length;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "RFC822" ".TEXT" SP nstring
*/

static int
mailimap_msg_att_rfc822_text_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, char ** result,
				   size_t * result_len,
				   size_t progr_rate,
				   progress_function * progr_fun)
{
  size_t cur_token;
  char * rfc822_text;
  int r;
  size_t length;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "RFC822");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    ".TEXT");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_nstring_parse(fd, buffer, &cur_token, &rfc822_text, &length,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = rfc822_text;
  if (result_len != NULL)
    * result_len = length;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "RFC822.SIZE" SP number
*/

static int
mailimap_msg_att_rfc822_size_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index, uint32_t * result)
{
  size_t cur_token;
  uint32_t number;
  int r;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "RFC822.SIZE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = number;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "BODY" SP body
*/


static int
mailimap_msg_att_body_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index, struct mailimap_body ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  struct mailimap_body * body;
  size_t cur_token;
  int r;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "BODY");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_body_parse(fd, buffer, &cur_token, &body,
			  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = body;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "BODY" "STRUCTURE" SP body
*/


static int
mailimap_msg_att_bodystructure_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index,
				     struct mailimap_body ** result,
				     size_t progr_rate,
				     progress_function * progr_fun)
{
  struct mailimap_body * body;
  size_t cur_token;
  int r;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "BODY");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "STRUCTURE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_body_parse(fd, buffer, &cur_token, &body,
			  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = body;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "BODY" section ["<" number ">"] SP nstring
*/

static int
mailimap_msg_att_body_section_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    struct mailimap_msg_att_body_section **
				    result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  size_t cur_token;
  uint32_t number;
  struct mailimap_section * section;
  char * body_part;
  struct mailimap_msg_att_body_section * msg_att_body_section;
  int r;
  int res;
  size_t length;

  cur_token = * index;

  section = NULL;
  number = 0;
  body_part = NULL;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "BODY");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_section_parse(fd, buffer, &cur_token, &section,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_lower_parse(fd, buffer, &cur_token);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    r = mailimap_number_parse(fd, buffer, &cur_token, &number);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto free_section;
    }
    
    r = mailimap_greater_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto free_section;
    }
    break;

  case MAILIMAP_ERROR_PARSE:
    break;
    
  default:
    return r;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_section;
  }

  r = mailimap_nstring_parse(fd, buffer, &cur_token, &body_part, &length,
			     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_section;
  }

  msg_att_body_section =
    mailimap_msg_att_body_section_new(section, number, body_part, length);
  if (msg_att_body_section == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_string;
  }

  * result = msg_att_body_section;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_string:
  mailimap_nstring_free(body_part);
 free_section:
  if (section != NULL)
    mailimap_section_free(section);
 err:
  return res;
}

/*
  "UID" SP uniqueid
*/

static int
mailimap_msg_att_uid_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index,
			   uint32_t * result)
{
  size_t cur_token;
  uint32_t uid;
  int r;

  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "UID");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_uniqueid_parse(fd, buffer, &cur_token, &uid);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  * index = cur_token;
  * result = uid;

  return MAILIMAP_NO_ERROR;
}

/*
   msg-att-static  = "ENVELOPE" SP envelope / "INTERNALDATE" SP date-time /
                     "RFC822" [".HEADER" / ".TEXT"] SP nstring /
                     "RFC822.SIZE" SP number / "BODY" ["STRUCTURE"] SP body /
                     "BODY" section ["<" number ">"] SP nstring /
                     "UID" SP uniqueid
                       ; MUST NOT change for a message
*/

static int
mailimap_msg_att_static_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_msg_att_static ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_envelope * env;
  struct mailimap_date_time * internal_date;
  char * rfc822;
  char * rfc822_header;
  char * rfc822_text;
  uint32_t rfc822_size;
  struct mailimap_body * bodystructure;
  struct mailimap_body * body;
  struct mailimap_msg_att_body_section * body_section;
  uint32_t uid;
  struct mailimap_msg_att_static * msg_att_static;
  int type;
  int r;
  int res;
  size_t length;

  cur_token = * index;

  env = NULL;
  internal_date = NULL;
  rfc822 = NULL;
  rfc822_header = NULL;
  rfc822_text = NULL;
  rfc822_size = 0;
  length = 0;
  bodystructure = NULL;
  body = NULL;
  body_section = NULL;
  uid = 0;

  type = MAILIMAP_MSG_ATT_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_msg_att_envelope_parse(fd, buffer, &cur_token, &env,
				      progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_MSG_ATT_ENVELOPE;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_internaldate_parse(fd, buffer, &cur_token,
					    &internal_date,
					    progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_INTERNALDATE;
  }
  
  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_rfc822_parse(fd, buffer, &cur_token,
				      &rfc822, &length,
				      progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_RFC822;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_rfc822_header_parse(fd, buffer, &cur_token,
					     &rfc822_header, &length,
					     progr_rate, progr_fun);
    type = MAILIMAP_MSG_ATT_RFC822_HEADER;
  }
  
  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_rfc822_text_parse(fd, buffer, &cur_token,
					   &rfc822_text, &length,
					   progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_RFC822_TEXT;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_rfc822_size_parse(fd, buffer, &cur_token,
					   &rfc822_size);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_RFC822_SIZE;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_body_parse(fd, buffer, &cur_token,
				    &body, progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_BODY;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_bodystructure_parse(fd, buffer, &cur_token,
					     &bodystructure,
					     progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_BODYSTRUCTURE;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_body_section_parse(fd, buffer, &cur_token,
					    &body_section,
					    progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_BODY_SECTION;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_msg_att_uid_parse(fd, buffer, &cur_token,
				   &uid);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_MSG_ATT_UID;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  msg_att_static = mailimap_msg_att_static_new(type, env, internal_date,
					       rfc822, rfc822_header,
					       rfc822_text, length,
					       rfc822_size, bodystructure,
					       body, body_section, uid);
  if (msg_att_static == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = msg_att_static;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (env)
    mailimap_msg_att_envelope_free(env);
  if (internal_date)
    mailimap_msg_att_internaldate_free(internal_date);
  if (rfc822)
    mailimap_msg_att_rfc822_free(rfc822);
  if (rfc822_header)
    mailimap_msg_att_rfc822_header_free(rfc822_header);
  if (rfc822_text)
    mailimap_msg_att_rfc822_text_free(rfc822_text);
  if (bodystructure)
    mailimap_msg_att_bodystructure_free(bodystructure);
  if (body)
    mailimap_msg_att_body_free(body);
  if (body_section)
    mailimap_msg_att_body_section_free(body_section);
 err:
  return res;
}


/*
   nil             = "NIL"
*/

static int mailimap_nil_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index)
{
  return mailimap_token_case_insensitive_parse(fd, buffer, index, "NIL");
}

/*
   nstring         = string / nil
*/


int mailimap_nstring_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index, char ** result,
				  size_t * result_len,
				  size_t progr_rate,
				  progress_function * progr_fun)
{
  int r;

  r = mailimap_string_parse(fd, buffer, index, result, result_len,
			    progr_rate, progr_fun);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    return MAILIMAP_NO_ERROR;

  case MAILIMAP_ERROR_PARSE:
    r = mailimap_nil_parse(fd, buffer, index);
    if (r != MAILIMAP_NO_ERROR) {
      return r;
    }
    
    * result = NULL;
    if (result_len != NULL)
      * result_len = 0;
    return MAILIMAP_NO_ERROR;

  default:
    return r;
  }
}

/*
   number          = 1*DIGIT
                       ; Unsigned 32-bit integer
                       ; (0 <= n < 4,294,967,296)
*/

static int
mailimap_number_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, uint32_t * result)
{
  size_t cur_token;
  int digit;
  uint32_t number;
  int parsed;
  int r;
  
  cur_token = * index;
  parsed = FALSE;

#ifdef UNSTRICT_SYNTAX
  mailimap_space_parse(fd, buffer, &cur_token);
#endif

  number = 0;
  while (1) {
    r = mailimap_digit_parse(fd, buffer, &cur_token, &digit);
    if (r == MAILIMAP_ERROR_PARSE)
      break;
    else if (r == MAILIMAP_NO_ERROR) {
      number *= 10;
      number += digit;
      parsed = TRUE;
    }
    else
      return r;
  }

  if (!parsed)
    return MAILIMAP_ERROR_PARSE;

  * result = number;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
   nz-number       = digit-nz *DIGIT
                       ; Non-zero unsigned 32-bit integer
                       ; (0 < n < 4,294,967,296)
*/

int
mailimap_nz_number_parse(mailstream * fd, MMAPString * buffer,
			 size_t * index, uint32_t * result)
{
#ifdef UNSTRICT_SYNTAX
  size_t cur_token;
  uint32_t number;
  int r;
  
  cur_token = * index;

  r = mailimap_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (number == 0)
    return MAILIMAP_ERROR_PARSE;

#else
  size_t cur_token;
  int digit;
  uint32_t number;
  int r;
  
  cur_token = * index;

  r = mailimap_digit_nz_parse(fd, buffer, &cur_token, &digit);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  number = digit;

  while (1) {
    r = mailimap_digit_parse(fd, buffer, &cur_token, &digit);
    if (r == MAILIMAP_ERROR_PARSE)
      break;
    else if (r == MAILIMAP_NO_ERROR) {
      number *= 10;
      number += (guint32) digit;
    }
    else
      return r;
  }
#endif

  * result = number;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
   password        = astring
*/

/*
   quoted          = DQUOTE *QUOTED-CHAR DQUOTE
*/

static int
mailimap_quoted_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, char ** result,
		      size_t progr_rate,
		      progress_function * progr_fun)
{
  char ch;
  size_t cur_token;
  MMAPString * gstr_quoted;
  int r;
  int res;
  
  cur_token = * index;

#ifdef UNSTRICT_SYNTAX
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE))
    return r;
#endif

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  gstr_quoted = mmap_string_new("");
  if (gstr_quoted == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }

  while (1) {
    r = mailimap_quoted_char_parse(fd, buffer, &cur_token, &ch);
    if (r == MAILIMAP_ERROR_PARSE)
      break;
    else if (r == MAILIMAP_NO_ERROR) {
      if (mmap_string_append_c(gstr_quoted, ch) == NULL) {
	res = MAILIMAP_ERROR_MEMORY;
	goto free;
      }
    }
    else {
      res = r;
      goto free;
    }
  }

  r = mailimap_dquote_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  if (mmap_string_ref(gstr_quoted) < 0) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * index = cur_token;
  * result = gstr_quoted->str;

  return MAILIMAP_NO_ERROR;

 free:
  mmap_string_free(gstr_quoted);
 err:
  return res;
}

/*
   QUOTED-CHAR     = <any TEXT-CHAR except quoted-specials> /
                     "\" quoted-specials
*/

static int is_quoted_specials(char ch);

static int
mailimap_quoted_char_parse(mailstream * fd, MMAPString * buffer,
			   size_t * index, char * result)
{
  size_t cur_token;
  int r;
  
  cur_token = * index;

  if (!is_quoted_specials(buffer->str[cur_token])) {
    * result = buffer->str[cur_token];
    cur_token ++;
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }
  else {
    char quoted_special;

    r = mailimap_char_parse(fd, buffer, &cur_token, '\\');
    if (r != MAILIMAP_NO_ERROR)
      return r;

    r = mailimap_quoted_specials_parse(fd, buffer, &cur_token,
				       &quoted_special);
    if (r != MAILIMAP_NO_ERROR)
      return r;

    * result = quoted_special;
    * index = cur_token;

    return MAILIMAP_NO_ERROR;
  }
}

/*
   quoted-specials = DQUOTE / "\"
*/

static int is_quoted_specials(char ch)
{
  return (ch == '\"') || (ch == '\\');
}

static int
mailimap_quoted_specials_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, char * result)
{
  size_t cur_token;
  
  cur_token = * index;

  if (is_quoted_specials(buffer->str[cur_token])) {
    * result = buffer->str[cur_token];
    cur_token ++;
    * index = cur_token;
    return MAILIMAP_NO_ERROR;
  }
  else
    return MAILIMAP_ERROR_PARSE;
}

/*
  UNIMPLEMENTED
   rename          = "RENAME" SP mailbox SP mailbox
                       ; Use of INBOX as a destination gives a NO error
*/

/*
   response        = *(continue-req / response-data) response-done
*/

/*
  continue-req / response-data
*/

/* static */ int
mailimap_cont_req_or_resp_data_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index,
				     struct mailimap_cont_req_or_resp_data **
				     result,
				     size_t progr_rate,
				     progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
  struct mailimap_continue_req * cont_req;
  struct mailimap_response_data * resp_data;
  int type;
  int r;
  int res;

  cur_token = * index;

  cont_req = NULL;
  resp_data = NULL;
  type = MAILIMAP_RESP_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_continue_req_parse(fd, buffer, &cur_token, &cont_req,
				  progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_RESP_CONT_REQ;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_response_data_parse(fd, buffer, &cur_token, &resp_data,
				     progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_RESP_DATA;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  /*
    multi-lines response
    read another response line because after that token,
    there must have something (continue-req, response-data or response-done)
  */

  if (!mailstream_read_line_append(fd, buffer)) {
    res = MAILIMAP_ERROR_STREAM;
    goto free;
  }

  cont_req_or_resp_data =
    mailimap_cont_req_or_resp_data_new(type, cont_req, resp_data);
  if (cont_req_or_resp_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = cont_req_or_resp_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (cont_req != NULL)
    mailimap_continue_req_free(cont_req);
  if (resp_data != NULL)
    mailimap_response_data_free(resp_data);
 err:
  return res;
}

/*
   response        = *(continue-req / response-data) response-done
*/

int
mailimap_response_parse(mailstream * fd, MMAPString * buffer,
			size_t * index, struct mailimap_response ** result,
			size_t progr_rate,
			progress_function * progr_fun)
{
  size_t cur_token;
  clist * cont_req_or_resp_data_list;
  struct mailimap_response * resp;
  struct mailimap_response_done * resp_done;
  int r;
  int res;

  cur_token = * index;
  cont_req_or_resp_data_list = NULL;
  resp_done = NULL;

  r = mailimap_struct_multiple_parse(fd, buffer,
				     &cur_token, &cont_req_or_resp_data_list,
				     (mailimap_struct_parser *)
				     mailimap_cont_req_or_resp_data_parse,
				     (mailimap_struct_destructor *)
				     mailimap_cont_req_or_resp_data_free,
				     progr_rate, progr_fun);

  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE))
    return r;

  r = mailimap_response_done_parse(fd, buffer, &cur_token, &resp_done,
				   progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_list;
  }

  resp = mailimap_response_new(cont_req_or_resp_data_list, resp_done);
  if (resp == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_resp_done;
  }

  * result = resp;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_resp_done:
  mailimap_response_done_free(resp_done);
 free_list:
  if (cont_req_or_resp_data_list != NULL) {
    clist_foreach(cont_req_or_resp_data_list,
			 		(clist_func) mailimap_cont_req_or_resp_data_free, NULL);
    clist_free(cont_req_or_resp_data_list);
  }
  return res;
}

/*
   response-data   = "*" SP (resp-cond-state / resp-cond-bye /
                     mailbox-data / message-data / capability-data) CRLF
*/

static int
mailimap_response_data_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_response_data ** result,
			     size_t progr_rate,
			     progress_function * progr_fun)
{
  struct mailimap_response_data * resp_data;
  size_t cur_token;
  int type;
  struct mailimap_resp_cond_state * cond_state;
  struct mailimap_resp_cond_bye * cond_bye;
  struct mailimap_mailbox_data * mb_data;
  struct mailimap_message_data * msg_data;
  struct mailimap_capability_data * cap_data;
  struct mailimap_extension_data * ext_data;
  int r;
  int res;

  cond_state = NULL;
  cond_bye = NULL;
  mb_data = NULL;
  msg_data = NULL;
  cap_data = NULL;
  ext_data = NULL;

  cur_token = * index;

  r = mailimap_star_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  type = MAILIMAP_RESP_DATA_TYPE_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_resp_cond_state_parse(fd, buffer, &cur_token, &cond_state,
				     progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_RESP_DATA_TYPE_COND_STATE;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_resp_cond_bye_parse(fd, buffer, &cur_token, &cond_bye,
				     progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_DATA_TYPE_COND_BYE;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_mailbox_data_parse(fd, buffer, &cur_token, &mb_data,
				    progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_message_data_parse(fd, buffer, &cur_token, &msg_data,
				    progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_capability_data_parse(fd, buffer, &cur_token, &cap_data,
				       progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_extension_data_parse(MAILIMAP_EXTENDED_PARSER_RESPONSE_DATA,
                fd, buffer, &cur_token, &ext_data, progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_crlf_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  resp_data = mailimap_response_data_new(type, cond_state,
      cond_bye, mb_data,
      msg_data, cap_data, ext_data);
  if (resp_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = resp_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (cond_state)
    mailimap_resp_cond_state_free(cond_state);
  if (cond_bye)
    mailimap_resp_cond_bye_free(cond_bye);
  if (mb_data)
    mailimap_mailbox_data_free(mb_data);
  if (msg_data)
    mailimap_message_data_free(msg_data);
  if (cap_data)
    mailimap_capability_data_free(cap_data);
  if (ext_data)
    mailimap_extension_data_free(ext_data);
 err:
  return res;
}

/*
   response-done   = response-tagged / response-fatal
*/

static int
mailimap_response_done_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_response_done ** result,
			     size_t progr_rate,
			     progress_function * progr_fun)
{
  int type;
  struct mailimap_response_done * resp_done;
  size_t cur_token;
  struct mailimap_response_tagged * tagged;
  struct mailimap_response_fatal * fatal;
  int r;
  int res;

  cur_token = * index;

  tagged = NULL;
  fatal = NULL;

  type = MAILIMAP_RESP_DONE_TYPE_ERROR; /* removes a gcc warning */

  r = mailimap_response_tagged_parse(fd, buffer, &cur_token, &tagged,
				     progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_RESP_DONE_TYPE_TAGGED;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_response_fatal_parse(fd, buffer, &cur_token, &fatal,
				      progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_DONE_TYPE_FATAL;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  resp_done = mailimap_response_done_new(type, tagged, fatal);
  if (resp_done == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = resp_done;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
  
 free:
  if (tagged == NULL)
    mailimap_response_tagged_free(tagged);
  if (fatal == NULL)
    mailimap_response_fatal_free(fatal);
 err:
  return res;
}

/*
   response-fatal  = "*" SP resp-cond-bye CRLF
                       ; Server closes connection immediately
*/

static int
mailimap_response_fatal_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_response_fatal ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  struct mailimap_resp_cond_bye * cond_bye;
  struct mailimap_response_fatal * fatal;
  size_t cur_token;
  int res;
  int r;

  cur_token = * index;

  r = mailimap_star_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_resp_cond_bye_parse(fd, buffer, &cur_token, &cond_bye,
				   progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_crlf_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  fatal = mailimap_response_fatal_new(cond_bye);
  if (fatal == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }
  
  * result = fatal;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  mailimap_resp_cond_bye_free(cond_bye);
 err:
  return res;
}

/*
   response-tagged = tag SP resp-cond-state CRLF
*/

static int
mailimap_response_tagged_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_response_tagged ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  size_t cur_token;
  char * tag;
  struct mailimap_resp_cond_state * cond_state;
  struct mailimap_response_tagged * resp_tagged;
  int r;
  int res;

  cur_token = * index;
  cond_state = NULL;

  r = mailimap_tag_parse(fd, buffer, &cur_token, &tag,
			 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_tag;
  }

  r = mailimap_resp_cond_state_parse(fd, buffer, &cur_token, &cond_state,
				     progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_tag;
  }

  resp_tagged = mailimap_response_tagged_new(tag, cond_state);
  if (resp_tagged == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_cond_state;
  }

  * result = resp_tagged;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_cond_state:
  mailimap_resp_cond_state_free(cond_state);
 free_tag:
  mailimap_tag_free(tag);
 err:
  return res;
}

/*
   resp-cond-auth  = ("OK" / "PREAUTH") SP resp-text
                       ; Authentication condition
*/

static int
mailimap_resp_cond_auth_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_resp_cond_auth ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  struct mailimap_resp_cond_auth * cond_auth;
  size_t cur_token;
  struct mailimap_resp_text * text;
  int type;
  int r;
  int res;
  
  cur_token = * index;
  text = NULL;

  type = MAILIMAP_RESP_COND_AUTH_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "OK");
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_RESP_COND_AUTH_OK;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_token_case_insensitive_parse(fd, buffer,
					      &cur_token, "PREAUTH");
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_COND_AUTH_PREAUTH;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_resp_text_parse(fd, buffer, &cur_token, &text,
			       progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  cond_auth = mailimap_resp_cond_auth_new(type, text);
  if (cond_auth == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = cond_auth;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  mailimap_resp_text_free(text);
 err:
  return res;
}

/*
   resp-cond-bye   = "BYE" SP resp-text
*/

static int
mailimap_resp_cond_bye_parse(mailstream * fd, MMAPString * buffer,
			     size_t * index,
			     struct mailimap_resp_cond_bye ** result,
			     size_t progr_rate,
			     progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_resp_cond_bye * cond_bye;
  struct mailimap_resp_text * text;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "BYE");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_resp_text_parse(fd, buffer, &cur_token, &text,
			       progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  cond_bye = mailimap_resp_cond_bye_new(text);
  if (cond_bye == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }
    
  * index = cur_token;
  * result = cond_bye;

  return MAILIMAP_NO_ERROR;

 free:
  mailimap_resp_text_free(text);
 err:
  return res;
}

/*
   resp-cond-state = ("OK" / "NO" / "BAD") SP resp-text
                       ; Status condition
*/

static int
mailimap_resp_cond_state_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_resp_cond_state ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  struct mailimap_resp_cond_state * cond_state;
  size_t cur_token;
  struct mailimap_resp_text * text;
  int type;
  int r;
  int res;
  
  cur_token = * index;
  text = NULL;

  type = mailimap_resp_cond_state_get_token_value(fd, buffer, &cur_token);
  if (type == -1) {
    res = MAILIMAP_ERROR_PARSE;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_resp_text_parse(fd, buffer, &cur_token, &text,
			       progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  cond_state = mailimap_resp_cond_state_new(type, text);
  if (cond_state == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = cond_state;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  mailimap_resp_text_free(text);
 err:
  return res;
}


/*
   resp-specials   = "]"
*/

static int is_resp_specials(char ch)
{
  switch (ch) {
  case ']':
    return TRUE;
  };
  return FALSE;
}

/*
   resp-text       = ["[" resp-text-code "]" SP] text
*/

/* "[" resp-text-code "]" */

static int
mailimap_resp_text_resp_text_code_parse(mailstream * fd, MMAPString * buffer,
					size_t * index,
					struct mailimap_resp_text_code **
					result,
					size_t progr_rate,
					progress_function * progr_fun)
{
  struct mailimap_resp_text_code * text_code;
  size_t cur_token;
  int r;
  int res;
  
  cur_token = * index;

  r = mailimap_obracket_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  text_code = NULL;
  r = mailimap_resp_text_code_parse(fd, buffer, &cur_token, &text_code,
				    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_cbracket_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto free;
  }

  * result = text_code;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  mailimap_resp_text_code_free(text_code);
 err:
  return res;
}

/*
   resp-text       = ["[" resp-text-code "]" SP] text
*/

static int
mailimap_resp_text_parse(mailstream * fd, MMAPString * buffer,
			 size_t * index,
			 struct mailimap_resp_text ** result,
			 size_t progr_rate,
			 progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_resp_text_code * text_code;
  struct mailimap_resp_text * resp_text;
  char * text;
  int r;
  int res;

  cur_token = * index;
  text = NULL;
  text_code = NULL;

  r = mailimap_resp_text_resp_text_code_parse(fd, buffer, &cur_token,
					      &text_code,
					      progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE))
    return r;

  r = mailimap_text_parse(fd, buffer, &cur_token, &text,
			  progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto free_resp_text_code;
  }
  
  resp_text = mailimap_resp_text_new(text_code, text);
  if (resp_text == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_text;
  }

  * result = resp_text;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_resp_text_code:
  if (text_code != NULL)
    mailimap_resp_text_code_free(text_code);
 free_text:
  mailimap_text_free(text);
  return res;
}

/*
   resp-text-code  = "ALERT" /
                     "BADCHARSET" [SP "(" astring *(SP astring) ")" ] /
                     capability-data / "PARSE" /
                     "PERMANENTFLAGS" SP "(" [flag-perm *(SP flag-perm)] ")" /
                     "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
                     "UIDNEXT" SP nz-number / "UIDVALIDITY" SP nz-number /
                     "UNSEEN" SP nz-number /
                     atom [SP 1*<any TEXT-CHAR except "]">]
*/

/*
  ALERT / PARSE / READ-ONLY / READ-WRITE / TRYCREATE
*/

static int
mailimap_resp_text_code_1_parse(mailstream * fd, MMAPString * buffer,
				size_t * index,
				int * result)
{
  int id;
  size_t cur_token;

  cur_token = * index;

  id = mailimap_resp_text_code_1_get_token_value(fd, buffer, &cur_token);
  
  if (id == -1)
    return MAILIMAP_ERROR_PARSE;

  * result = id;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
  "BADCHARSET" [SP "(" astring *(SP astring) ")" ]
*/

/*
  SP "(" astring *(SP astring) ")"
*/

static int
mailimap_resp_text_code_badcharset_1_parse(mailstream * fd,
					   MMAPString * buffer,
					   size_t * index,
					   clist ** result,
					   size_t progr_rate,
					   progress_function * progr_fun)
{
  size_t cur_token;
  clist * charset;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &charset,
					(mailimap_struct_parser *)
					mailimap_astring_parse,
					(mailimap_struct_destructor *)
					mailimap_astring_free,
					progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto charset;
  }

  * index = cur_token;
  * result = charset;

  return MAILIMAP_NO_ERROR;

 charset:
  clist_foreach(charset, (clist_func) mailimap_string_free, NULL);
  clist_free(charset);
 err:
  return res;
}

/*
  "BADCHARSET" [SP "(" astring *(SP astring) ")" ]
*/

static int
mailimap_resp_text_code_badcharset_parse(mailstream * fd, MMAPString * buffer,
					 size_t * index,
					 clist ** result,
					 size_t progr_rate,
					 progress_function * progr_fun)
{
  size_t cur_token;
  clist * charset;
  int r;

  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "BADCHARSET");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  charset = NULL;

  r = mailimap_resp_text_code_badcharset_1_parse(fd, buffer, &cur_token,
						 &charset,
						 progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE))
    return r;
  
  * result = charset;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  "PERMANENTFLAGS" SP "(" [flag-perm *(SP flag-perm)] ")"
*/

static int
mailimap_resp_text_code_permanentflags_parse(mailstream * fd,
					     MMAPString * buffer,
					     size_t * index,
					     clist ** result,
					     size_t progr_rate,
					     progress_function * progr_fun)
{
  size_t cur_token;
  clist * flaglist;
  int r;
  int res;

  cur_token = * index;

  flaglist = NULL;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
					    "PERMANENTFLAGS");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_struct_spaced_list_parse(fd, buffer, &cur_token, &flaglist,
					(mailimap_struct_parser *)
					mailimap_flag_perm_parse,
					(mailimap_struct_destructor *)
					mailimap_flag_perm_free,
					progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto err;
  }
  
  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free;
  }

  * index = cur_token;
  * result = flaglist;

  return MAILIMAP_NO_ERROR;

 free:
  clist_foreach(flaglist, (clist_func) mailimap_flag_perm_free, NULL);
  clist_free(flaglist);
 err:
  return res;
}


/*
  "UIDNEXT" SP nz-number /
  "UIDVALIDITY" SP nz-number /
  "UNSEEN" SP nz-number 
*/

static int
mailimap_resp_text_code_number_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index,
				     struct mailimap_resp_text_code ** result,
				     size_t progr_rate,
				     progress_function * progr_fun)
{
  size_t cur_token;
  int type;
  uint32_t number;
  struct mailimap_resp_text_code * resp_text_code;
  int r;

  cur_token = * index;

  resp_text_code = NULL;

  type = mailimap_resp_text_code_2_get_token_value(fd, buffer, &cur_token);
  if (type == -1)
    return MAILIMAP_ERROR_PARSE;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_nz_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  switch (type) {
  case MAILIMAP_RESP_TEXT_CODE_UIDNEXT:
    resp_text_code = mailimap_resp_text_code_new(type, NULL, NULL, NULL,
						 number, 0, 0, NULL , NULL, NULL);
    break;
  case MAILIMAP_RESP_TEXT_CODE_UIDVALIDITY:
    resp_text_code = mailimap_resp_text_code_new(type, NULL, NULL, NULL,
						 0, number, 0, NULL , NULL, NULL);
    break;
  case MAILIMAP_RESP_TEXT_CODE_UNSEEN:
    resp_text_code = mailimap_resp_text_code_new(type, NULL, NULL, NULL,
						 0, 0, number, NULL , NULL, NULL);
    break;
  }

  if (resp_text_code == NULL)
    return MAILIMAP_ERROR_MEMORY;

  * result = resp_text_code;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  atom [SP 1*<any TEXT-CHAR except "]">]
*/

static int is_text_char(char ch);

/*
  any TEXT-CHAR except "]"
*/

static int is_text_char_1(char ch)
{
  if (ch == ']')
    return FALSE;
  return is_text_char(ch);
}

/*
  1*<any TEXT-CHAR except "]"
*/

static int
mailimap_resp_text_code_other_2_parse(mailstream * fd, MMAPString * buffer,
				      size_t * index, char ** result,
				      size_t progr_rate,
				      progress_function * progr_fun)
{
  return mailimap_custom_string_parse(fd, buffer, index, result,
				      is_text_char_1);
}

/*
  SP 1*<any TEXT-CHAR except "]">
*/

static int
mailimap_resp_text_code_other_1_parse(mailstream * fd, MMAPString * buffer,
				      size_t * index,
				      char ** result,
				      size_t progr_rate,
				      progress_function * progr_fun)
{
  size_t cur_token;
  char * value;
  int r;

  cur_token = * index;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_resp_text_code_other_2_parse(fd, buffer, &cur_token,
					    &value,
					    progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = value;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  atom [SP 1*<any TEXT-CHAR except "]">]
*/

static int
mailimap_resp_text_code_other_parse(mailstream * fd, MMAPString * buffer,
				    size_t * index,
				    struct mailimap_resp_text_code ** result,
				    size_t progr_rate,
				    progress_function * progr_fun)
{
  size_t cur_token;
  char * atom;
  char * value;
  struct mailimap_resp_text_code * resp_text_code;
  int r;
  int res;

  cur_token = * index;
  atom = NULL;
  value = NULL;

  r = mailimap_atom_parse(fd, buffer, &cur_token, &atom,
			  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_resp_text_code_other_1_parse(fd, buffer, &cur_token,
					    &value, progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto err;
  }
  
  resp_text_code = mailimap_resp_text_code_new(MAILIMAP_RESP_TEXT_CODE_OTHER,
					       NULL, NULL, NULL,
					       0, 0, 0, atom, value, NULL);
  if (resp_text_code == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_value;
  }

  * result = resp_text_code;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free_value:
  if (value != NULL)
    free(value);
  mailimap_atom_free(atom);
 err:
  return res;
}



/*
   resp-text-code  = "ALERT" /
                     "BADCHARSET" [SP "(" astring *(SP astring) ")" ] /
                     capability-data / "PARSE" /
                     "PERMANENTFLAGS" SP "(" [flag-perm *(SP flag-perm)] ")" /
                     "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
                     "UIDNEXT" SP nz-number / "UIDVALIDITY" SP nz-number /
                     "UNSEEN" SP nz-number /
                     atom [SP 1*<any TEXT-CHAR except "]">]
*/

static int
mailimap_resp_text_code_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index,
			      struct mailimap_resp_text_code ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_resp_text_code * resp_text_code;
  clist * badcharset;
  clist * permanentflags;
  struct mailimap_capability_data * cap_data;
  struct mailimap_extension_data * ext_data;
  int type;
  int r;
  int res;

  cur_token = * index;

  resp_text_code = NULL;
  badcharset = NULL;
  cap_data = NULL;
  ext_data = NULL;
  permanentflags = NULL;

  type = MAILIMAP_RESP_TEXT_CODE_OTHER;
  r = mailimap_resp_text_code_1_parse(fd, buffer, &cur_token, &type);
  if (r == MAILIMAP_NO_ERROR) {
    /* do nothing */
  }

  if (r == MAILIMAP_ERROR_PARSE) {

    r = mailimap_resp_text_code_badcharset_parse(fd, buffer, &cur_token,
						 &badcharset,
						 progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_TEXT_CODE_BADCHARSET;
  }
  
  if (r == MAILIMAP_ERROR_PARSE) {

    r = mailimap_capability_data_parse(fd, buffer, &cur_token,
				       &cap_data,
				       progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_resp_text_code_permanentflags_parse(fd, buffer, &cur_token,
						     &permanentflags,
						     progr_rate,
						     progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_resp_text_code_number_parse(fd, buffer, &cur_token,
					     &resp_text_code,
					     progr_rate, progr_fun);
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_extension_data_parse(MAILIMAP_EXTENDED_PARSER_RESP_TEXT_CODE,
        fd, buffer, &cur_token, &ext_data,
        progr_rate, progr_fun);
    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_RESP_TEXT_CODE_EXTENSION;
  }

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_resp_text_code_other_parse(fd, buffer, &cur_token,
					    &resp_text_code,
					    progr_rate, progr_fun);
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  if (resp_text_code == NULL) {
    resp_text_code = mailimap_resp_text_code_new(type,
						 badcharset, cap_data,
						 permanentflags,
						 0, 0, 0, NULL, NULL, ext_data);
    if (resp_text_code == NULL) {
      res = MAILIMAP_ERROR_MEMORY;
      goto free;
    }
  }

  * result = resp_text_code;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

free:
  if (permanentflags) {
    clist_foreach(permanentflags,
		  (clist_func) mailimap_flag_perm_free, NULL);
    clist_free(permanentflags);
  }
  if (cap_data)
    mailimap_capability_data_free(cap_data);
  if (badcharset) {
    clist_foreach(badcharset, (clist_func) mailimap_astring_free, NULL);
    clist_free(badcharset);
  }
  if (ext_data)
    mailimap_extension_data_free(ext_data);
err:
  return res;
}

/*
  UNIMPLEMENTED
   search          = "SEARCH" [SP "CHARSET" SP astring] 1*(SP search-key)
                       ; CHARSET argument to MUST be registered with IANA
*/

/*
  UNIMPLEMENTED
   search-key      = "ALL" / "ANSWERED" / "BCC" SP astring /
                     "BEFORE" SP date / "BODY" SP astring /
                     "CC" SP astring / "DELETED" / "FLAGGED" /
                     "FROM" SP astring / "KEYWORD" SP flag-keyword / "NEW" /
                     "OLD" / "ON" SP date / "RECENT" / "SEEN" /
                     "SINCE" SP date / "SUBJECT" SP astring /
                     "TEXT" SP astring / "TO" SP astring /
                     "UNANSWERED" / "UNDELETED" / "UNFLAGGED" /
                     "UNKEYWORD" SP flag-keyword / "UNSEEN" /
                       ; Above this line were in [IMAP2]
                     "DRAFT" / "HEADER" SP header-fld-name SP astring /
                     "LARGER" SP number / "NOT" SP search-key /
                     "OR" SP search-key SP search-key /
                     "SENTBEFORE" SP date / "SENTON" SP date /
                     "SENTSINCE" SP date / "SMALLER" SP number /
                     "UID" SP set / "UNDRAFT" / set /
                     "(" search-key *(SP search-key) ")"
*/

/*
   section         = "[" [section-spec] "]"
*/

static int
mailimap_section_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index,
		       struct mailimap_section ** result,
		       size_t progr_rate,
		       progress_function * progr_fun)
{
  struct mailimap_section_spec * section_spec;
  size_t cur_token;
  struct mailimap_section * section;
  int r;
  int res;

  cur_token = * index;

  section_spec = NULL;

  r = mailimap_obracket_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_section_spec_parse(fd, buffer, &cur_token, &section_spec,
				  progr_rate, progr_fun);
  if ((r != MAILIMAP_NO_ERROR) && (r != MAILIMAP_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimap_cbracket_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  if (section_spec == NULL)
    section = NULL;
  else {
    section = mailimap_section_new(section_spec);
    if (section == NULL) {
      res = MAILIMAP_ERROR_MEMORY;
      goto free;
    }
  }

  * result = section;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  mailimap_section_spec_free(section_spec);
 err:
  return res;
}

/*
   section-msgtext = "HEADER" / "HEADER.FIELDS" [".NOT"] SP header-list /
                     "TEXT"
                       ; top-level or MESSAGE/RFC822 part
*/

static int
mailimap_section_msgtext_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       struct mailimap_section_msgtext ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  size_t cur_token;
  int type;
  struct mailimap_header_list * header_list;
  struct mailimap_section_msgtext * msgtext;
  int r;
  int res;

  cur_token = * index;

  header_list = NULL;

  type = mailimap_section_msgtext_get_token_value(fd, buffer, &cur_token);
  if (type == -1) {
    res = MAILIMAP_ERROR_PARSE;
    goto err;
  }

  if (type == MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS) {
    r = mailimap_header_list_parse(fd, buffer, &cur_token, &header_list,
				   progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
  }
  else if (type == MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT) {
    r = mailimap_header_list_parse(fd, buffer, &cur_token, &header_list,
				   progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  msgtext = mailimap_section_msgtext_new(type, header_list);
  if (msgtext == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_header_list;
  }

  * result = msgtext;
  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;

 free_header_list:
  if (header_list)
    mailimap_header_list_free(header_list);
 err:
  return res;
}

/*
   section-part    = nz-number *("." nz-number)
                       ; body part nesting
*/

static int
mailimap_section_part_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_section_part ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  struct mailimap_section_part * section_part;
  size_t cur_token;
  clist * section_id;
  int r;
  int res;
  
  cur_token = * index;
  section_id = NULL;

  r = mailimap_struct_list_parse(fd, buffer, &cur_token, &section_id, '.',
				 (mailimap_struct_parser *)
				 mailimap_nz_number_alloc_parse,
				 (mailimap_struct_destructor *)
				 mailimap_number_alloc_free,
				 progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  section_part = mailimap_section_part_new(section_id);
  if (section_part == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_section_id;
  }

  * result = section_part;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
  
 free_section_id:
  clist_foreach(section_id, (clist_func) mailimap_number_alloc_free, NULL);
  clist_free(section_id);
 err:
  return res;
}

/*
   section-spec    = section-msgtext / (section-part ["." section-text])
*/

static int
mailimap_section_spec_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_section_spec ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  int type;
  struct mailimap_section_msgtext * section_msgtext;
  struct mailimap_section_part * section_part;
  struct mailimap_section_text * section_text;
  struct mailimap_section_spec * section_spec;
  size_t cur_token;
  int r;
  int res;
  size_t final_token;

  cur_token = * index;

  section_msgtext = NULL;
  section_part = NULL;
  section_text = NULL;

  r = mailimap_section_msgtext_parse(fd, buffer, &cur_token,
				     &section_msgtext,
				     progr_rate, progr_fun);
  switch (r) {
  case MAILIMAP_NO_ERROR:
    type = MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT;
    break;
    
  case MAILIMAP_ERROR_PARSE:
    
    r = mailimap_section_part_parse(fd, buffer, &cur_token,
				    &section_part,
				    progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }
    
    final_token = cur_token;
    
    type = MAILIMAP_SECTION_SPEC_SECTION_PART;
    
    r = mailimap_dot_parse(fd, buffer, &cur_token);
    if (r == MAILIMAP_NO_ERROR) {
      r = mailimap_section_text_parse(fd, buffer, &cur_token, &section_text,
				      progr_rate, progr_fun);
      if (r == MAILIMAP_NO_ERROR) {
	final_token = cur_token;
      }
      else if (r != MAILIMAP_ERROR_PARSE) {
	res = r;
	goto err;
      }
    }
    else if (r != MAILIMAP_ERROR_PARSE) {
      res = r;
      goto err;
    }
    
    cur_token = final_token;
    break;

  default:
    res = r;
    goto err;
  }
  
  section_spec = mailimap_section_spec_new(type, section_msgtext,
					   section_part, section_text);
  if (section_spec == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = section_spec;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (section_msgtext)
    mailimap_section_msgtext_free(section_msgtext);
  if (section_part)
    mailimap_section_part_free(section_part);
  if (section_text)
    mailimap_section_text_free(section_text);
 err:
  return res;
}

/*
   section-text    = section-msgtext / "MIME"
                       ; text other than actual body part (headers, etc.)
*/

static int
mailimap_section_text_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_section_text ** result,
			    size_t progr_rate,
			    progress_function * progr_fun)
{
  struct mailimap_section_msgtext * section_msgtext;
  size_t cur_token;
  struct mailimap_section_text * section_text;
  int type;
  int r;
  int res;
  
  cur_token = * index;

  section_msgtext = NULL;

  type = MAILIMAP_SECTION_TEXT_ERROR; /* XXX - removes a gcc warning */

  r = mailimap_section_msgtext_parse(fd, buffer, &cur_token, &section_msgtext,
				     progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_SECTION_TEXT_SECTION_MSGTEXT;

  if (r == MAILIMAP_ERROR_PARSE) {
    r= mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "MIME");

    if (r == MAILIMAP_NO_ERROR)
      type = MAILIMAP_SECTION_TEXT_MIME;
  }

  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  section_text = mailimap_section_text_new(type, section_msgtext);
  if (section_text == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free;
  }

  * result = section_text;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 free:
  if (section_msgtext)
    mailimap_section_msgtext_free(section_msgtext);
 err:
  return res;
}

/*
  UNIMPLEMENTED
   select          = "SELECT" SP mailbox
*/

/*
  UNIMPLEMENTED
   sequence-num    = nz-number / "*"
                       ; * is the largest number in use.  For message
                       ; sequence numbers, it is the number of messages
                       ; in the mailbox.  For unique identifiers, it is
                       ; the unique identifier of the last message in
                       ; the mailbox.
*/

/*
  UNIMPLEMENTED
   set             = sequence-num / (sequence-num ":" sequence-num) /
                     (set "," set)
                       ; Identifies a set of messages.  For message
                       ; sequence numbers, these are consecutive
                       ; numbers from 1 to the number of messages in
                       ; the mailbox
                       ; Comma delimits individual numbers, colon
                       ; delimits between two numbers inclusive.
                       ; Example: 2,4:7,9,12:* is 2,4,5,6,7,9,12,13,
                       ; 14,15 for a mailbox with 15 messages.
*/

/*
  UNIMPLEMENTED
   status          = "STATUS" SP mailbox SP "(" status-att *(SP status-att) ")"
*/

/*
   status-att      = "MESSAGES" / "RECENT" / "UIDNEXT" / "UIDVALIDITY" /
                     "UNSEEN"
*/

static int mailimap_status_att_parse(mailstream * fd, MMAPString * buffer,
				     size_t * index, int * result)
{
  int type;
  size_t cur_token;

  cur_token = * index;

  type = mailimap_status_att_get_token_value(fd, buffer, &cur_token);

  if (type == -1)
    return MAILIMAP_ERROR_PARSE;

  * result = type;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
  UNIMPLEMENTED
   store           = "STORE" SP set SP store-att-flags
*/

/*
  UNIMPLEMENTED
   store-att-flags = (["+" / "-"] "FLAGS" [".SILENT"]) SP
                     (flag-list / (flag *(SP flag)))
*/

/*
   string          = quoted / literal
*/

int
mailimap_string_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, char ** result,
		      size_t * result_len,
		      size_t progr_rate,
		      progress_function * progr_fun)
{
  size_t cur_token;
  char * string;
  int r;
  size_t len;

  cur_token = * index;

  string = NULL;
  len = 0;
  
  r = mailimap_quoted_parse(fd, buffer, &cur_token, &string,
			    progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    len = strlen(string);
  else if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_literal_parse(fd, buffer, &cur_token, &string, &len,
			       progr_rate, progr_fun);
  }

  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = string;
  if (result_len != NULL)
    * result_len = len;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}


/*
  UNIMPLEMENTED
   subscribe       = "SUBSCRIBE" SP mailbox
*/

/*
   tag             = 1*<any ASTRING-CHAR except "+">
*/

/*
  any ASTRING-CHAR except "+"
*/

static int is_tag_char(char ch)
{
  if (ch == '+')
    return FALSE;
  return is_astring_char(ch);
}

/*
   tag             = 1*<any ASTRING-CHAR except "+">
*/

static int mailimap_tag_parse(mailstream * fd, MMAPString * buffer,
			      size_t * index, char ** result,
			      size_t progr_rate,
			      progress_function * progr_fun)
{
  size_t cur_token;
  char * tag;
  int r;

  cur_token = * index;

  r = mailimap_custom_string_parse(fd, buffer, &cur_token, &tag,
				   is_tag_char);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * index = cur_token;
  * result = tag;

  return MAILIMAP_NO_ERROR;
}

/*
   text            = 1*TEXT-CHAR
*/

static int mailimap_text_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, char ** result,
			       size_t progr_rate,
			       progress_function * progr_fun)
{
  return mailimap_custom_string_parse(fd, buffer, index, result,
				      is_text_char);
}


/*
   TEXT-CHAR       = <any CHAR except CR and LF>
*/

static int is_text_char(char ch)
{
  if ((ch == '\r') || (ch == '\n'))
    return FALSE;

  return is_char(ch);
}

/*
   time            = 2DIGIT ":" 2DIGIT ":" 2DIGIT
                       ; Hours minutes seconds
*/

/*
  2DIGIT
*/

static int mailimap_2digit_parse(mailstream * fd, MMAPString * buffer,
				 size_t * index, int * result)
{
#ifndef UNSTRICT_SYNTAX
  int digit;
  int two_digit;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimap_digit_parse(fd, buffer, &cur_token, &digit);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  two_digit = digit;
  
  r = mailimap_digit_parse(fd, buffer, &cur_token, &digit);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  two_digit = two_digit * 10 + digit;

  * result = two_digit;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
#else
  uint32_t number;
  size_t cur_token;
  int r;
  
  cur_token = * index;
  
  r = mailimap_number_parse(fd, buffer, &cur_token, &number);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  * index = cur_token;
  * result = number;

  return MAILIMAP_NO_ERROR;
#endif
}

/*
   time            = 2DIGIT ":" 2DIGIT ":" 2DIGIT
                       ; Hours minutes seconds
*/

static int mailimap_time_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index,
			       int * phour, int * pmin, int * psec)
{
  size_t cur_token;
  int hour;
  int min;
  int sec;
  int r;
  
  cur_token = * index;

  r = mailimap_2digit_parse(fd, buffer, &cur_token, &hour);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_colon_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_2digit_parse(fd, buffer, &cur_token, &min);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_colon_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_2digit_parse(fd, buffer, &cur_token, &sec);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * phour = hour;
  * pmin = min;
  * psec = sec;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}

/*
  UNIMPLEMENTED
   uid             = "UID" SP (copy / fetch / search / store)
                       ; Unique identifiers used instead of message
                       ; sequence numbers
*/

/*
   uniqueid        = nz-number
                       ; Strictly ascending
*/

int mailimap_uniqueid_parse(mailstream * fd, MMAPString * buffer,
    size_t * index, uint32_t * result)
{
  return mailimap_nz_number_parse(fd, buffer, index, result);
}

/*
  UNIMPLEMENTED
   unsubscribe     = "UNSUBSCRIBE" SP mailbox
*/

/*
  UNIMPLEMENTED
   userid          = astring
*/

/*
  UNIMPLEMENTED
   x-command       = "X" atom <experimental command arguments>
*/

/*
   zone            = ("+" / "-") 4DIGIT
                       ; Signed four-digit value of hhmm representing
                       ; hours and minutes east of Greenwich (that is,
                       ; the amount that the given time differs from
                       ; Universal Time).  Subtracting the timezone
                       ; from the given time will give the UT form.
                       ; The Universal Time zone is "+0000".
*/

static int mailimap_zone_parse(mailstream * fd, MMAPString * buffer,
			       size_t * index, int * result)
{
  size_t cur_token;
  uint32_t zone;
#ifndef UNSTRICT_SYNTAX
  int i;
  int digit;
#endif
  int sign;
  int r;

  cur_token = * index;

  sign = 1;
  r = mailimap_plus_parse(fd, buffer, &cur_token);
  if (r == MAILIMAP_NO_ERROR)
    sign = 1;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_minus_parse(fd, buffer, &cur_token);
    if (r == MAILIMAP_NO_ERROR)
      sign = -1;
  }

  if (r != MAILIMAP_NO_ERROR)
    return r;

#ifdef UNSTRICT_SYNTAX
  r = mailimap_number_parse(fd, buffer, &cur_token, &zone);
  if (r != MAILIMAP_NO_ERROR)
    return r;
#else
  zone = 0;
  for(i = 0 ; i < 4 ; i ++) {
    r = mailimap_digit_parse(fd, buffer, &cur_token, &digit);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    zone = zone * 10 + digit;
  }
#endif

  zone *= sign;

  * result = zone;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
}
