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
 * $Id: mailimf.c,v 1.44 2006/08/05 02:34:07 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimf.h"

/*
  RFC 2822

  RFC 2821 ... 
   A message-originating SMTP system SHOULD NOT send a message that
   already contains a Return-path header.  SMTP servers performing a
   relay function MUST NOT inspect the message data, and especially not
   to the extent needed to determine if Return-path headers are present.
   SMTP servers making final delivery MAY remove Return-path headers
   before adding their own.
*/

#include <ctype.h>
#include "mmapstring.h"
#include <stdlib.h>
#include <string.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif







static inline int is_dtext(char ch);

static int mailimf_quoted_pair_parse(const char * message, size_t length,
				     size_t * index, char * result);

static int mailimf_ccontent_parse(const char * message, size_t length,
				  size_t * index);

static int
mailimf_comment_fws_ccontent_parse(const char * message, size_t length,
				   size_t * index);

static inline int mailimf_comment_parse(const char * message, size_t length,
				 size_t * index);

static int mailimf_qcontent_parse(const char * message, size_t length,
				  size_t * index, char * ch);

static int mailimf_phrase_parse(const char * message, size_t length,
				size_t * index, char ** result);

static int mailimf_unstructured_parse(const char * message, size_t length,
				      size_t * index, char ** result);

static int mailimf_ignore_unstructured_parse(const char * message, size_t length,
					     size_t * index);

static int mailimf_day_of_week_parse(const char * message, size_t length,
				     size_t * index, int * result);

static int mailimf_day_name_parse(const char * message, size_t length,
				  size_t * index, int * result);

static int mailimf_date_parse(const char * message, size_t length,
			      size_t * index,
			      int * pday, int * pmonth, int * pyear);

static int mailimf_year_parse(const char * message, size_t length,
			      size_t * index, int * result);

static int mailimf_month_parse(const char * message, size_t length,
			       size_t * index, int * result);

static int mailimf_month_name_parse(const char * message, size_t length,
				    size_t * index, int * result);

static int mailimf_day_parse(const char * message, size_t length,
				  size_t * index, int * result);

static int mailimf_time_parse(const char * message, size_t length,
			      size_t * index, 
			      int * phour, int * pmin,
			      int * psec,
			      int * zone);
static int mailimf_time_of_day_parse(const char * message, size_t length,
				     size_t * index,
				     int * phour, int * pmin,
				     int * psec);

static int mailimf_hour_parse(const char * message, size_t length,
			      size_t * index, int * result);

static int mailimf_minute_parse(const char * message, size_t length,
				size_t * index, int * result);

static int mailimf_second_parse(const char * message, size_t length,
				size_t * index, int * result);

static int mailimf_zone_parse(const char * message, size_t length,
			      size_t * index, int * result);

static int mailimf_name_addr_parse(const char * message, size_t length,
				   size_t * index,
				   char ** pdisplay_name,
				   char ** pangle_addr);

static int mailimf_angle_addr_parse(const char * message, size_t length,
				    size_t * index, char ** result);

static int mailimf_group_parse(const char * message, size_t length,
			       size_t * index,
			       struct mailimf_group ** result);

static int mailimf_display_name_parse(const char * message, size_t length,
				      size_t * index, char ** result);

static int mailimf_addr_spec_parse(const char * message, size_t length,
				   size_t * index,
				   char ** address);

#if 0
static int mailimf_local_part_parse(const char * message, size_t length,
				    size_t * index,
				    char ** result);

static int mailimf_domain_parse(const char * message, size_t length,
				size_t * index,
				char ** result);
#endif

#if 0
static int mailimf_domain_literal_parse(const char * message, size_t length,
					size_t * index, char ** result);
#endif

#if 0
static int mailimf_dcontent_parse(const char * message, size_t length,
				  size_t * index, char * result);
#endif

static int
mailimf_orig_date_parse(const char * message, size_t length,
			size_t * index, struct mailimf_orig_date ** result);

static int
mailimf_from_parse(const char * message, size_t length,
		   size_t * index, struct mailimf_from ** result);

static int
mailimf_sender_parse(const char * message, size_t length,
		     size_t * index, struct mailimf_sender ** result);

static int
mailimf_reply_to_parse(const char * message, size_t length,
		       size_t * index, struct mailimf_reply_to ** result);

static int
mailimf_to_parse(const char * message, size_t length,
		 size_t * index, struct mailimf_to ** result);

static int
mailimf_cc_parse(const char * message, size_t length,
		 size_t * index, struct mailimf_cc ** result);

static int
mailimf_bcc_parse(const char * message, size_t length,
		  size_t * index, struct mailimf_bcc ** result);

static int mailimf_message_id_parse(const char * message, size_t length,
				    size_t * index,
				    struct mailimf_message_id ** result);

static int
mailimf_in_reply_to_parse(const char * message, size_t length,
			  size_t * index,
			  struct mailimf_in_reply_to ** result);

#if 0
static int mailimf_references_parse(const char * message, size_t length,
				    size_t * index,
				    struct mailimf_references **
				    result);
#endif

static int mailimf_unstrict_msg_id_parse(const char * message, size_t length,
					 size_t * index,
					 char ** result);

#if 0
static int mailimf_id_left_parse(const char * message, size_t length,
				 size_t * index, char ** result);

static int mailimf_id_right_parse(const char * message, size_t length,
				  size_t * index, char ** result);
#endif

#if 0
static int mailimf_no_fold_quote_parse(const char * message, size_t length,
				       size_t * index, char ** result);

static int mailimf_no_fold_literal_parse(const char * message, size_t length,
					 size_t * index, char ** result);
#endif

static int mailimf_subject_parse(const char * message, size_t length,
				 size_t * index,
				 struct mailimf_subject ** result);

static int mailimf_comments_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_comments ** result);

static int mailimf_keywords_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_keywords ** result);

static int
mailimf_resent_date_parse(const char * message, size_t length,
			  size_t * index, struct mailimf_orig_date ** result);

static int
mailimf_resent_from_parse(const char * message, size_t length,
			  size_t * index, struct mailimf_from ** result);

static int
mailimf_resent_sender_parse(const char * message, size_t length,
			    size_t * index, struct mailimf_sender ** result);

static int
mailimf_resent_to_parse(const char * message, size_t length,
			size_t * index, struct mailimf_to ** result);

static int
mailimf_resent_cc_parse(const char * message, size_t length,
			size_t * index, struct mailimf_cc ** result);

static int
mailimf_resent_bcc_parse(const char * message, size_t length,
			 size_t * index, struct mailimf_bcc ** result);

static int
mailimf_resent_msg_id_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailimf_message_id ** result);

static int mailimf_return_parse(const char * message, size_t length,
				size_t * index,
				struct mailimf_return ** result);

static int
mailimf_path_parse(const char * message, size_t length,
		   size_t * index, struct mailimf_path ** result);

static int
mailimf_optional_field_parse(const char * message, size_t length,
			     size_t * index,
			     struct mailimf_optional_field ** result);

static int mailimf_field_name_parse(const char * message, size_t length,
				    size_t * index, char ** result);

























/* *************************************************************** */

static inline int is_digit(char ch)
{
  return (ch >= '0') && (ch <= '9');
}

static int mailimf_digit_parse(const char * message, size_t length,
			       size_t * index, int * result)
{
  size_t cur_token;

  cur_token = * index;
  
  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (is_digit(message[cur_token])) {
    * result = message[cur_token] - '0';
    cur_token ++;
    * index = cur_token;
    return MAILIMF_NO_ERROR;
  }
  else
    return MAILIMF_ERROR_PARSE;
}

int
mailimf_number_parse(const char * message, size_t length,
		     size_t * index, uint32_t * result)
{
  size_t cur_token;
  int digit;
  uint32_t number;
  int parsed;
  int r;

  cur_token = * index;
  parsed = FALSE;

  number = 0;
  while (1) {
    r = mailimf_digit_parse(message, length, &cur_token, &digit);
    if (r != MAILIMF_NO_ERROR) {
      if (r == MAILIMF_ERROR_PARSE)
	break;
      else
	return r;
    }
    number *= 10;
    number += digit;
    parsed = TRUE;
  }

  if (!parsed)
    return MAILIMF_ERROR_PARSE;

  * result = number;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

int mailimf_char_parse(const char * message, size_t length,
		       size_t * index, char token)
{
  size_t cur_token;

  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (message[cur_token] == token) {
    cur_token ++;
    * index = cur_token;
    return MAILIMF_NO_ERROR;
  }
  else
    return MAILIMF_ERROR_PARSE;
}

int mailimf_unstrict_char_parse(const char * message, size_t length,
				size_t * index, char token)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_char_parse(message, length, &cur_token, token);
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

int
mailimf_token_case_insensitive_len_parse(const char * message, size_t length,
					 size_t * index, char * token,
					 size_t token_length)
{
  size_t cur_token;

  cur_token = * index;

  if (cur_token + token_length - 1 >= length)
    return MAILIMF_ERROR_PARSE;

  if (strncasecmp(message + cur_token, token, token_length) == 0) {
    cur_token += token_length;
    * index = cur_token;
    return MAILIMF_NO_ERROR;
  }
  else
    return MAILIMF_ERROR_PARSE;
}

static int mailimf_oparenth_parse(const char * message, size_t length,
				  size_t * index)
{
  return mailimf_char_parse(message, length, index, '(');
}

static int mailimf_cparenth_parse(const char * message, size_t length,
				  size_t * index)
{
  return mailimf_char_parse(message, length, index, ')');
}

static int mailimf_comma_parse(const char * message, size_t length,
			       size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, ',');
}

static int mailimf_dquote_parse(const char * message, size_t length,
				size_t * index)
{
  return mailimf_char_parse(message, length, index, '\"');
}

static int mailimf_colon_parse(const char * message, size_t length,
			       size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, ':');
}

static int mailimf_semi_colon_parse(const char * message, size_t length,
				    size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, ';');
}

static int mailimf_plus_parse(const char * message, size_t length,
			      size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, '+');
}

static int mailimf_minus_parse(const char * message, size_t length,
			       size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, '-');
}

static int mailimf_lower_parse(const char * message, size_t length,
			       size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, '<');
}

static int mailimf_greater_parse(const char * message, size_t length,
				      size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, '>');
}

#if 0
static int mailimf_obracket_parse(const char * message, size_t length,
				       size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, '[');
}

static int mailimf_cbracket_parse(const char * message, size_t length,
				       size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, ']');
}
#endif

static int mailimf_at_sign_parse(const char * message, size_t length,
				      size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, '@');
}

static int mailimf_point_parse(const char * message, size_t length,
				      size_t * index)
{
  return mailimf_unstrict_char_parse(message, length, index, '.');
}

int
mailimf_custom_string_parse(const char * message, size_t length,
			    size_t * index, char ** result,
			    int (* is_custom_char)(char))
{
  size_t begin;
  size_t end;
  char * gstr;

  begin = * index;

  end = begin;

  if (end >= length)
    return MAILIMF_ERROR_PARSE;

  while (is_custom_char(message[end])) {
    end ++;
    if (end >= length)
      break;
  }

  if (end != begin) {
    /*
    gstr = strndup(message + begin, end - begin);
    */
    gstr = malloc(end - begin + 1);
    if (gstr == NULL)
      return MAILIMF_ERROR_MEMORY;
    strncpy(gstr, message + begin, end - begin);
    gstr[end - begin] = '\0';

    * index = end;
    * result = gstr;
    return MAILIMF_NO_ERROR;
  }
  else
    return MAILIMF_ERROR_PARSE;
}







typedef int mailimf_struct_parser(const char * message, size_t length,
				  size_t * index, void * result);

typedef int mailimf_struct_destructor(void * result);


static int
mailimf_struct_multiple_parse(const char * message, size_t length,
			      size_t * index, clist ** result,
			      mailimf_struct_parser * parser,
			      mailimf_struct_destructor * destructor)
{
  clist * struct_list;
  size_t cur_token;
  void * value;
  int r;
  int res;

  cur_token = * index;

  r = parser(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  struct_list = clist_new();
  if (struct_list == NULL) {
    destructor(value);
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  r = clist_append(struct_list, value);
  if (r < 0) {
    destructor(value);
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  while (1) {
    r = parser(message, length, &cur_token, &value);
    if (r != MAILIMF_NO_ERROR) {
      if (r == MAILIMF_ERROR_PARSE)
	break;
      else {
	res = r;
	goto free;
      }
    }
    r = clist_append(struct_list, value);
    if (r < 0) {
      (* destructor)(value);
      res = MAILIMF_ERROR_MEMORY;
      goto free;
    }
  }

  * result = struct_list;
  * index = cur_token;
  
  return MAILIMF_NO_ERROR;

 free:
  clist_foreach(struct_list, (clist_func) destructor, NULL);
  clist_free(struct_list);
 err:
  return res;
}



static int
mailimf_struct_list_parse(const char * message, size_t length,
			  size_t * index, clist ** result,
			  char symbol,
			  mailimf_struct_parser * parser,
			  mailimf_struct_destructor * destructor)
{
  clist * struct_list;
  size_t cur_token;
  void * value;
  size_t final_token;
  int r;
  int res;

  cur_token = * index;

  r = parser(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  struct_list = clist_new();
  if (struct_list == NULL) {
    destructor(value);
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  r = clist_append(struct_list, value);
  if (r < 0) {
    destructor(value);
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  final_token = cur_token;

  while (1) {
    r = mailimf_unstrict_char_parse(message, length, &cur_token, symbol);
    if (r != MAILIMF_NO_ERROR) {
      if (r == MAILIMF_ERROR_PARSE)
	break;
      else {
	res = r;
	goto free;
      }
    }

    r = parser(message, length, &cur_token, &value);
    if (r != MAILIMF_NO_ERROR) {
      if (r == MAILIMF_ERROR_PARSE)
	break;
      else {
	res = r;
	goto free;
      }
    }

    r = clist_append(struct_list, value);
    if (r < 0) {
      destructor(value);
      res = MAILIMF_ERROR_MEMORY;
      goto free;
    }

    final_token = cur_token;
  }
  
  * result = struct_list;
  * index = final_token;
  
  return MAILIMF_NO_ERROR;
  
 free:
  clist_foreach(struct_list, (clist_func) destructor, NULL);
  clist_free(struct_list);
 err:
  return res;
}

static inline int mailimf_wsp_parse(const char * message, size_t length,
				    size_t * index)
{
  size_t cur_token;

  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if ((message[cur_token] != ' ') && (message[cur_token] != '\t'))
    return MAILIMF_ERROR_PARSE;

  cur_token ++;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}


int mailimf_crlf_parse(const char * message, size_t length, size_t * index)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_char_parse(message, length, &cur_token, '\r');
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_char_parse(message, length, &cur_token, '\n');
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  return MAILIMF_NO_ERROR;
}

static int mailimf_unstrict_crlf_parse(const char * message,
				       size_t length, size_t * index)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  mailimf_cfws_parse(message, length, &cur_token);

  r = mailimf_char_parse(message, length, &cur_token, '\r');
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_char_parse(message, length, &cur_token, '\n');
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  return MAILIMF_NO_ERROR;
}

/* ************************************************************************ */



/* RFC 2822 grammar */

/*
NO-WS-CTL       =       %d1-8 /         ; US-ASCII control characters
                        %d11 /          ;  that do not include the
                        %d12 /          ;  carriage return, line feed,
                        %d14-31 /       ;  and white space characters
                        %d127
*/

static inline int is_no_ws_ctl(char ch)
{
  if ((ch == 9) || (ch == 10) || (ch == 13))
    return FALSE;

  if (ch == 127)
     return TRUE;

  return (ch >= 1) && (ch <= 31);
}

/*
text            =       %d1-9 /         ; Characters excluding CR and LF
                        %d11 /
                        %d12 /
                        %d14-127 /
                        obs-text
*/

/*
specials        =       "(" / ")" /     ; Special characters used in
                        "<" / ">" /     ;  other parts of the syntax
                        "[" / "]" /
                        ":" / ";" /
                        "@" / "\" /
                        "," / "." /
                        DQUOTE
*/

/*
quoted-pair     =       ("\" text) / obs-qp
*/

static inline int mailimf_quoted_pair_parse(const char * message, size_t length,
					    size_t * index, char * result)
{
  size_t cur_token;

  cur_token = * index;
  
  if (cur_token + 1 >= length)
    return MAILIMF_ERROR_PARSE;

  if (message[cur_token] != '\\')
    return MAILIMF_ERROR_PARSE;

  cur_token ++;
  * result = message[cur_token];
  cur_token ++;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
FWS             =       ([*WSP CRLF] 1*WSP) /   ; Folding white space
                        obs-FWS
*/

int mailimf_fws_parse(const char * message, size_t length, size_t * index)
{
  size_t cur_token;
  size_t final_token;
  int fws_1;
  int fws_2;
  int fws_3;
  int r;
  
  cur_token = * index;

  fws_1 = FALSE;
  while (1) {
    r = mailimf_wsp_parse(message, length, &cur_token);
    if (r != MAILIMF_NO_ERROR) {
      if (r == MAILIMF_ERROR_PARSE)
	break;
      else
	return r;
    }
    fws_1 = TRUE;
  }
  final_token = cur_token;

  r = mailimf_crlf_parse(message, length, &cur_token);
  switch (r) {
  case MAILIMF_NO_ERROR:
    fws_2 = TRUE;
    break;
  case MAILIMF_ERROR_PARSE:
    fws_2 = FALSE;
    break;
  default:
      return r;
  }
  
  fws_3 = FALSE;
  if (fws_2) {
    while (1) {
      r = mailimf_wsp_parse(message, length, &cur_token);
      if (r != MAILIMF_NO_ERROR) {
	if (r == MAILIMF_ERROR_PARSE)
	  break;
	else
	  return r;
      }
      fws_3 = TRUE;
    }
  }

  if ((!fws_1) && (!fws_3))
    return MAILIMF_ERROR_PARSE;

  if (!fws_3)
    cur_token = final_token;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}


/*
ctext           =       NO-WS-CTL /     ; Non white space controls

                        %d33-39 /       ; The rest of the US-ASCII
                        %d42-91 /       ;  characters not including "(",
                        %d93-126        ;  ")", or "\"
*/

static inline int is_ctext(char ch)
{
  unsigned char uch = (unsigned char) ch;

  if (is_no_ws_ctl(ch))
    return TRUE;

  if (uch < 33)
    return FALSE;

  if ((uch == 40) || (uch == 41))
    return FALSE;
  
  if (uch == 92)
    return FALSE;

  if (uch == 127)
    return FALSE;

  return TRUE;
}

/*
ccontent        =       ctext / quoted-pair / comment
*/

static inline int mailimf_ccontent_parse(const char * message, size_t length,
					 size_t * index)
{
  size_t cur_token;
  char ch;
  int r;
  
  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (is_ctext(message[cur_token])) {
    cur_token ++;
  }
  else {
    r = mailimf_quoted_pair_parse(message, length, &cur_token, &ch);
    
    if (r == MAILIMF_ERROR_PARSE)
      r = mailimf_comment_parse(message, length, &cur_token);
    
    if (r == MAILIMF_ERROR_PARSE)
      return r;
  }

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
[FWS] ccontent
*/

static inline int
mailimf_comment_fws_ccontent_parse(const char * message, size_t length,
				   size_t * index)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_ccontent_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
comment         =       "(" *([FWS] ccontent) [FWS] ")"
*/

static inline int mailimf_comment_parse(const char * message, size_t length,
				 size_t * index)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_oparenth_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  while (1) {
    r = mailimf_comment_fws_ccontent_parse(message, length, &cur_token);
    if (r != MAILIMF_NO_ERROR) {
      if (r == MAILIMF_ERROR_PARSE)
	break;
      else
	return r;
    }
  }

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_cparenth_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
[FWS] comment
*/

static inline int mailimf_cfws_fws_comment_parse(const char * message, size_t length,
						 size_t * index)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_comment_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
CFWS            =       *([FWS] comment) (([FWS] comment) / FWS)
*/

int mailimf_cfws_parse(const char * message, size_t length,
		       size_t * index)
{
  size_t cur_token;
  int has_comment;
  int r;

  cur_token = * index;

  has_comment = FALSE;
  while (1) {
    r = mailimf_cfws_fws_comment_parse(message, length, &cur_token);
    if (r != MAILIMF_NO_ERROR) {
      if (r == MAILIMF_ERROR_PARSE)
	break;
      else
	return r;
    }
    has_comment = TRUE;
  }

  if (!has_comment) {
    r = mailimf_fws_parse(message, length, &cur_token);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
atext           =       ALPHA / DIGIT / ; Any character except controls,
                        "!" / "#" /     ;  SP, and specials.
                        "$" / "%" /     ;  Used for atoms
                        "&" / "'" /
                        "*" / "+" /
                        "-" / "/" /
                        "=" / "?" /
                        "^" / "_" /
                        "`" / "{" /
                        "|" / "}" /
                        "~"
*/

static inline int is_atext(char ch)
{
  switch (ch) {
  case ' ':
  case '\t':
  case '\n':
  case '\r':
#if 0
  case '(':
  case ')':
#endif
  case '<':
  case '>':
#if 0
  case '@':
#endif
  case ',':
  case '"':
  case ':':
  case ';':
    return FALSE;
  default:
    return TRUE;
  }
}

/*
atom            =       [CFWS] 1*atext [CFWS]
*/

int mailimf_atom_parse(const char * message, size_t length,
		       size_t * index, char ** result)
{
  size_t cur_token;
  int r;
  int res;
  char * atom;
  size_t end;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }
  
  end = cur_token;
  if (end >= length) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  while (is_atext(message[end])) {
    end ++;
    if (end >= length)
      break;
  }
  if (end == cur_token) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  atom = malloc(end - cur_token + 1);
  if (atom == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }
  strncpy(atom, message + cur_token, end - cur_token);
  atom[end - cur_token] = '\0';

  cur_token = end;

  * index = cur_token;
  * result = atom;

  return MAILIMF_NO_ERROR;

 err:
  return res;
}

int mailimf_fws_atom_parse(const char * message, size_t length,
			   size_t * index, char ** result)
{
  size_t cur_token;
  int r;
  int res;
  char * atom;
  size_t end;

  cur_token = * index;

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  end = cur_token;
  if (end >= length) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  while (is_atext(message[end])) {
    end ++;
    if (end >= length)
      break;
  }
  if (end == cur_token) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  atom = malloc(end - cur_token + 1);
  if (atom == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }
  strncpy(atom, message + cur_token, end - cur_token);
  atom[end - cur_token] = '\0';

  cur_token = end;

  * index = cur_token;
  * result = atom;

  return MAILIMF_NO_ERROR;

 err:
  return res;
}

/*
dot-atom        =       [CFWS] dot-atom-text [CFWS]
*/

#if 0
static int mailimf_dot_atom_parse(const char * message, size_t length,
				  size_t * index, char ** result)
{
  return mailimf_atom_parse(message, length, index, result);
}
#endif

/*
dot-atom-text   =       1*atext *("." 1*atext)
*/

#if 0
static int
mailimf_dot_atom_text_parse(const char * message, size_t length,
			    size_t * index, char ** result)
{
  return mailimf_atom_parse(message, length, index, result);
}
#endif

/*
qtext           =       NO-WS-CTL /     ; Non white space controls

                        %d33 /          ; The rest of the US-ASCII
                        %d35-91 /       ;  characters not including "\"
                        %d93-126        ;  or the quote character
*/

static inline int is_qtext(char ch)
{
  unsigned char uch = (unsigned char) ch;

  if (is_no_ws_ctl(ch))
    return TRUE;

  if (uch < 33)
    return FALSE;

  if (uch == 34)
    return FALSE;

  if (uch == 92)
    return FALSE;

  if (uch == 127)
    return FALSE;

  return TRUE;
}

/*
qcontent        =       qtext / quoted-pair
*/

static int mailimf_qcontent_parse(const char * message, size_t length,
				  size_t * index, char * result)
{
  size_t cur_token;
  char ch;
  int r;

  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (is_qtext(message[cur_token])) {
    ch = message[cur_token];
    cur_token ++;
  }
  else {
    r = mailimf_quoted_pair_parse(message, length, &cur_token, &ch);
    
    if (r != MAILIMF_NO_ERROR)
      return r;
  }
  
  * result = ch;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
quoted-string   =       [CFWS]
                        DQUOTE *([FWS] qcontent) [FWS] DQUOTE
                        [CFWS]
*/

int mailimf_quoted_string_parse(const char * message, size_t length,
				size_t * index, char ** result)
{
  size_t cur_token;
  MMAPString * gstr;
  char ch;
  char * str;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_dquote_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  gstr = mmap_string_new("");
  if (gstr == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

#if 0
  if (mmap_string_append_c(gstr, '\"') == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_gstr;
  }
#endif

  while (1) {
    r = mailimf_fws_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      if (mmap_string_append_c(gstr, ' ') == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto free_gstr;
      }
    }
    else if (r != MAILIMF_ERROR_PARSE) {
      res = r;
      goto free_gstr;
    }

    r = mailimf_qcontent_parse(message, length, &cur_token, &ch);
    if (r == MAILIMF_NO_ERROR) {
      if (mmap_string_append_c(gstr, ch) == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto free_gstr;
      }
    }
    else if (r == MAILIMF_ERROR_PARSE)
      break;
    else {
      res = r;
      goto free_gstr;
    }
  }

  r = mailimf_dquote_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_gstr;
  }

#if 0
  if (mmap_string_append_c(gstr, '\"') == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_gstr;
  }
#endif

  str = strdup(gstr->str);
  if (str == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_gstr;
  }
  mmap_string_free(gstr);

  * index = cur_token;
  * result = str;

  return MAILIMF_NO_ERROR;

 free_gstr:
  mmap_string_free(gstr);
 err:
  return res;
}

int mailimf_fws_quoted_string_parse(const char * message, size_t length,
				    size_t * index, char ** result)
{
  size_t cur_token;
  MMAPString * gstr;
  char ch;
  char * str;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_dquote_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  gstr = mmap_string_new("");
  if (gstr == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

#if 0
  if (mmap_string_append_c(gstr, '\"') == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_gstr;
  }
#endif

  while (1) {
    r = mailimf_fws_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      if (mmap_string_append_c(gstr, ' ') == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto free_gstr;
      }
    }
    else if (r != MAILIMF_ERROR_PARSE) {
      res = r;
      goto free_gstr;
    }

    r = mailimf_qcontent_parse(message, length, &cur_token, &ch);
    if (r == MAILIMF_NO_ERROR) {
      if (mmap_string_append_c(gstr, ch) == NULL) {
	res = MAILIMF_ERROR_MEMORY;
	goto free_gstr;
      }
    }
    else if (r == MAILIMF_ERROR_PARSE)
      break;
    else {
      res = r;
      goto free_gstr;
    }
  }

  r = mailimf_dquote_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_gstr;
  }

#if 0
  if (mmap_string_append_c(gstr, '\"') == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_gstr;
  }
#endif

  str = strdup(gstr->str);
  if (str == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_gstr;
  }
  mmap_string_free(gstr);

  * index = cur_token;
  * result = str;

  return MAILIMF_NO_ERROR;

 free_gstr:
  mmap_string_free(gstr);
 err:
  return res;
}

/*
word            =       atom / quoted-string
*/

int mailimf_word_parse(const char * message, size_t length,
		       size_t * index, char ** result)
{
  size_t cur_token;
  char * word;
  int r;

  cur_token = * index;

  r = mailimf_atom_parse(message, length, &cur_token, &word);

  if (r == MAILIMF_ERROR_PARSE)
    r = mailimf_quoted_string_parse(message, length, &cur_token, &word);

  if (r != MAILIMF_NO_ERROR)
    return r;

  * result = word;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

int mailimf_fws_word_parse(const char * message, size_t length,
			   size_t * index, char ** result)
{
  size_t cur_token;
  char * word;
  int r;

  cur_token = * index;

  r = mailimf_fws_atom_parse(message, length, &cur_token, &word);

  if (r == MAILIMF_ERROR_PARSE)
    r = mailimf_fws_quoted_string_parse(message, length, &cur_token, &word);

  if (r != MAILIMF_NO_ERROR)
    return r;

  * result = word;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
phrase          =       1*word / obs-phrase
*/

static int mailimf_phrase_parse(const char * message, size_t length,
				size_t * index, char ** result)
{
  MMAPString * gphrase;
  char * word;
  int first;
  size_t cur_token;
  int r;
  int res;
  char * str;

  cur_token = * index;

  gphrase = mmap_string_new("");
  if (gphrase == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  first = TRUE;

  while (1) {
    r = mailimf_fws_word_parse(message, length, &cur_token, &word);
    if (r == MAILIMF_NO_ERROR) {
      if (!first) {
	if (mmap_string_append_c(gphrase, ' ') == NULL) {
	  mailimf_word_free(word);
	  res = MAILIMF_ERROR_MEMORY;
	  goto free;
	}
      }
      if (mmap_string_append(gphrase, word) == NULL) {
	mailimf_word_free(word);
	res = MAILIMF_ERROR_MEMORY;
	goto free;
      }
      mailimf_word_free(word);
      first = FALSE;
    }
    else if (r == MAILIMF_ERROR_PARSE)
      break;
    else {
      res = r;
      goto free;
    }
  }

  if (first) {
    res = MAILIMF_ERROR_PARSE;
    goto free;
  }

  str = strdup(gphrase->str);
  if (str == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }
  mmap_string_free(gphrase);

  * result = str;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  mmap_string_free(gphrase);
 err:
  return res;
}

/*
utext           =       NO-WS-CTL /     ; Non white space controls
                        %d33-126 /      ; The rest of US-ASCII
                        obs-utext

added : WSP
*/

enum {
  UNSTRUCTURED_START,
  UNSTRUCTURED_CR,
  UNSTRUCTURED_LF,
  UNSTRUCTURED_WSP,
  UNSTRUCTURED_OUT
};

static int mailimf_unstructured_parse(const char * message, size_t length,
				      size_t * index, char ** result)
{
  size_t cur_token;
  int state;
  size_t begin;
  size_t terminal;
  char * str;

  cur_token = * index;


  while (1) {
    int r;

    r = mailimf_wsp_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE)
      break;
    else {
      return r;
    }
  }

  state = UNSTRUCTURED_START;
  begin = cur_token;
  terminal = cur_token;

  while (state != UNSTRUCTURED_OUT) {

    switch(state) {
    case UNSTRUCTURED_START:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      terminal = cur_token;
      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    case UNSTRUCTURED_CR:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;

    case UNSTRUCTURED_LF:
      if (cur_token >= length) {
	state = UNSTRUCTURED_OUT;
	break;
      }

      switch(message[cur_token]) {
      case '\t':
      case ' ':
	state = UNSTRUCTURED_WSP;
	break;
      default:
	state = UNSTRUCTURED_OUT;
	break;
      }
      break;
    case UNSTRUCTURED_WSP:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    }

    cur_token ++;
  }

  str = malloc(terminal - begin + 1);
  if (str == NULL)
    return MAILIMF_ERROR_MEMORY;
  strncpy(str, message + begin,  terminal - begin);
  str[terminal - begin] = '\0';

  * index = terminal;
  * result = str;

  return MAILIMF_NO_ERROR;
}


static int mailimf_ignore_unstructured_parse(const char * message, size_t length,
					     size_t * index)
{
  size_t cur_token;
  int state;
  size_t terminal;

  cur_token = * index;

  state = UNSTRUCTURED_START;
  terminal = cur_token;

  while (state != UNSTRUCTURED_OUT) {

    switch(state) {
    case UNSTRUCTURED_START:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;
      terminal = cur_token;
      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    case UNSTRUCTURED_CR:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;
      switch(message[cur_token]) {
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    case UNSTRUCTURED_LF:
      if (cur_token >= length) {
	state = UNSTRUCTURED_OUT;
	break;
      }
      switch(message[cur_token]) {
      case '\t':
      case ' ':
	state = UNSTRUCTURED_WSP;
	break;
      default:
	state = UNSTRUCTURED_OUT;
	break;
      }
      break;
    case UNSTRUCTURED_WSP:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;
      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    }

    cur_token ++;
  }

  * index = terminal;

  return MAILIMF_NO_ERROR;
}


int mailimf_ignore_field_parse(const char * message, size_t length,
			       size_t * index)
{
  int has_field;
  size_t cur_token;
  int state;
  size_t terminal;

  has_field = FALSE;
  cur_token = * index;

  terminal = cur_token;
  state = UNSTRUCTURED_START;

  /* check if this is not a beginning CRLF */

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  switch (message[cur_token]) {
  case '\r':
    return MAILIMF_ERROR_PARSE;
  case '\n':
    return MAILIMF_ERROR_PARSE;
  }

  while (state != UNSTRUCTURED_OUT) {

    switch(state) {
    case UNSTRUCTURED_START:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      case ':':
	has_field = TRUE;
	state = UNSTRUCTURED_START;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    case UNSTRUCTURED_CR:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      case ':':
	has_field = TRUE;
	state = UNSTRUCTURED_START;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    case UNSTRUCTURED_LF:
      if (cur_token >= length) {
	terminal = cur_token;
	state = UNSTRUCTURED_OUT;
	break;
      }

      switch(message[cur_token]) {
      case '\t':
      case ' ':
	state = UNSTRUCTURED_WSP;
	break;
      default:
	terminal = cur_token;
	state = UNSTRUCTURED_OUT;
	break;
      }
      break;
    case UNSTRUCTURED_WSP:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      case ':':
	has_field = TRUE;
	state = UNSTRUCTURED_START;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    }

    cur_token ++;
  }

  if (!has_field)
    return MAILIMF_ERROR_PARSE;

  * index = terminal;

  return MAILIMF_NO_ERROR;
}


/*
date-time       =       [ day-of-week "," ] date FWS time [CFWS]
*/

int mailimf_date_time_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailimf_date_time ** result)
{
  size_t cur_token;
  int day_of_week;
  struct mailimf_date_time * date_time;
  int day;
  int month;
  int year;
  int hour;
  int min;
  int sec;
  int zone;
  int r;

  cur_token = * index;

  day_of_week = -1;
  r = mailimf_day_of_week_parse(message, length, &cur_token, &day_of_week);
  if (r == MAILIMF_NO_ERROR) {
    r = mailimf_comma_parse(message, length, &cur_token);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }
  else if (r != MAILIMF_ERROR_PARSE)
    return r;

  day = 0;
  month = 0;
  year = 0;
  r = mailimf_date_parse(message, length, &cur_token, &day, &month, &year);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_fws_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  hour = 0;
  min = 0;
  sec = 0;
  zone = 0;
  r = mailimf_time_parse(message, length, &cur_token,
			 &hour, &min, &sec, &zone);
  if (r != MAILIMF_NO_ERROR)
    return r;

  date_time = mailimf_date_time_new(day, month, year, hour, min, sec, zone);
  if (date_time == NULL)
    return MAILIMF_ERROR_MEMORY;

  * index = cur_token;
  * result = date_time;

  return MAILIMF_NO_ERROR;
}

/*
day-of-week     =       ([FWS] day-name) / obs-day-of-week
*/

static int mailimf_day_of_week_parse(const char * message, size_t length,
				     size_t * index, int * result)
{
  size_t cur_token;
  int day_of_week;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_day_name_parse(message, length, &cur_token, &day_of_week);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  * result = day_of_week;

  return MAILIMF_NO_ERROR;
}

/*
day-name        =       "Mon" / "Tue" / "Wed" / "Thu" /
                        "Fri" / "Sat" / "Sun"
*/

struct mailimf_token_value {
  int value;
  char * str;
};

static struct mailimf_token_value day_names[] = {
  {1, "Mon"},
  {2, "Tue"},
  {3, "Wed"},
  {4, "Thu"},
  {5, "Fri"},
  {6, "Sat"},
  {7, "Sun"},
};

enum {
  DAY_NAME_START,
  DAY_NAME_T,
  DAY_NAME_S
};

static int guess_day_name(const char * message, size_t length, size_t index)
{
  int state;

  state = DAY_NAME_START;

  while (1) {

    if (index >= length)
      return -1;

    switch(state) {
    case DAY_NAME_START:
      switch((char) toupper((unsigned char) message[index])) {
      case 'M': /* Mon */
	return 1;
	break;
      case 'T': /* Tue Thu */
	state = DAY_NAME_T;
	break;
      case 'W': /* Wed */
	return 3;
      case 'F':
	return 5;
      case 'S': /* Sat Sun */
	state = DAY_NAME_S;
	break;
      default:
	return -1;
      }
      break;
    case DAY_NAME_T:
      switch((char) toupper((unsigned char) message[index])) {
      case 'U':
	return 2;
      case 'H':
	return 4;
      default:
	return -1;
      }
      break;
    case DAY_NAME_S:
      switch((char) toupper((unsigned char) message[index])) {
      case 'A':
	return 6;
      case 'U':
	return 7;
      default:
	return -1;
      }
      break;
    }

    index ++;
  }
}

static int mailimf_day_name_parse(const char * message, size_t length,
				  size_t * index, int * result)
{
  size_t cur_token;
  int day_of_week;
  int guessed_day;
  int r;

  cur_token = * index;

  guessed_day = guess_day_name(message, length, cur_token);
  if (guessed_day == -1)
    return MAILIMF_ERROR_PARSE;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token,
					   day_names[guessed_day - 1].str);
  if (r != MAILIMF_NO_ERROR)
    return r;

  day_of_week = guessed_day;

  * result = day_of_week;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
date            =       day month year
*/

static int mailimf_date_parse(const char * message, size_t length,
			      size_t * index,
			      int * pday, int * pmonth, int * pyear)
{
  size_t cur_token;
  int day;
  int month;
  int year;
  int r;

  cur_token = * index;

  day = 1;
  r = mailimf_day_parse(message, length, &cur_token, &day);
  if (r != MAILIMF_NO_ERROR)
    return r;

  month = 1;
  r = mailimf_month_parse(message, length, &cur_token, &month);
  if (r != MAILIMF_NO_ERROR)
    return r;

  year = 2001;
  r = mailimf_year_parse(message, length, &cur_token, &year);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * pday = day;
  * pmonth = month;
  * pyear = year;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
year            =       4*DIGIT / obs-year
*/

static int mailimf_year_parse(const char * message, size_t length,
			      size_t * index, int * result)
{
  uint32_t number;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_number_parse(message, length, &cur_token, &number);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  * result = number;

  return MAILIMF_NO_ERROR;
}

/*
month           =       (FWS month-name FWS) / obs-month
*/

static int mailimf_month_parse(const char * message, size_t length,
			       size_t * index, int * result)
{
  size_t cur_token;
  int month;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_month_name_parse(message, length, &cur_token, &month);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * result = month;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
month-name      =       "Jan" / "Feb" / "Mar" / "Apr" /
                        "May" / "Jun" / "Jul" / "Aug" /
                        "Sep" / "Oct" / "Nov" / "Dec"
*/

static struct mailimf_token_value month_names[] = {
  {1, "Jan"},
  {2, "Feb"},
  {3, "Mar"},
  {4, "Apr"},
  {5, "May"},
  {6, "Jun"},
  {7, "Jul"},
  {8, "Aug"},
  {9, "Sep"},
  {10, "Oct"},
  {11, "Nov"},
  {12, "Dec"},
};

enum {
  MONTH_START,
  MONTH_J,
  MONTH_JU,
  MONTH_M,
  MONTH_MA,
  MONTH_A
};

static int guess_month(const char * message, size_t length, size_t index)
{
  int state;

  state = MONTH_START;

  while (1) {

    if (index >= length)
      return -1;

    switch(state) {
    case MONTH_START:
      switch((char) toupper((unsigned char) message[index])) {
      case 'J': /* Jan Jun Jul */
	state = MONTH_J;
	break;
      case 'F': /* Feb */
	return 2;
      case 'M': /* Mar May */
	state = MONTH_M;
	break;
      case 'A': /* Apr Aug */
	state = MONTH_A;
	break;
      case 'S': /* Sep */
	return 9;
      case 'O': /* Oct */
	return 10;
      case 'N': /* Nov */
	return 11;
      case 'D': /* Dec */
	return 12;
      default:
	return -1;
      }
      break;
    case MONTH_J:
      switch((char) toupper((unsigned char) message[index])) {
      case 'A':
	return 1;
      case 'U':
	state = MONTH_JU;
	break;
      default:
	return -1;
      }
      break;
    case MONTH_JU:
      switch((char) toupper((unsigned char) message[index])) {
      case 'N':
	return 6;
      case 'L':
	return 7;
      default:
	return -1;
      }
      break;
    case MONTH_M:
      switch((char) toupper((unsigned char) message[index])) {
      case 'A':
	state = MONTH_MA;
	break;
      default:
	return -1;
      }
      break;
    case MONTH_MA:
      switch((char) toupper((unsigned char) message[index])) {
      case 'Y':
	return 5;
      case 'R':
	return 3;
      default:
	return -1;
      }
      break;
    case MONTH_A:
      switch((char) toupper((unsigned char) message[index])) {
      case 'P':
	return 4;
      case 'U':
	return 8;
      default:
	return -1;
      }
      break;
    }

    index ++;
  }
}

static int mailimf_month_name_parse(const char * message, size_t length,
				    size_t * index, int * result)
{
  size_t cur_token;
  int month;
  int guessed_month;
  int r;

  cur_token = * index;

  guessed_month = guess_month(message, length, cur_token);
  if (guessed_month == -1)
    return MAILIMF_ERROR_PARSE;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token,
					   month_names[guessed_month - 1].str);
  if (r != MAILIMF_NO_ERROR)
    return r;

  month = guessed_month;

  * result = month;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
day             =       ([FWS] 1*2DIGIT) / obs-day
*/

static int mailimf_day_parse(const char * message, size_t length,
			     size_t * index, int * result)
{
  size_t cur_token;
  uint32_t day;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_number_parse(message, length, &cur_token, &day);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * result = day;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
time            =       time-of-day FWS zone
*/

static int mailimf_time_parse(const char * message, size_t length,
			      size_t * index, 
			      int * phour, int * pmin,
			      int * psec,
			      int * pzone)
{
  size_t cur_token;
  int hour;
  int min;
  int sec;
  int zone;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_time_of_day_parse(message, length, &cur_token,
				&hour, &min, &sec);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_zone_parse(message, length, &cur_token, &zone);
  if (r == MAILIMF_NO_ERROR) {
    /* do nothing */
  }
  else if (r == MAILIMF_ERROR_PARSE) {
    zone = 0;
  }
  else {
    return r;
  }

  * phour = hour;
  * pmin = min;
  * psec = sec;
  * pzone = zone;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
time-of-day     =       hour ":" minute [ ":" second ]
*/

static int mailimf_time_of_day_parse(const char * message, size_t length,
				     size_t * index,
				     int * phour, int * pmin,
				     int * psec)
{
  int hour;
  int min;
  int sec;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_hour_parse(message, length, &cur_token, &hour);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_minute_parse(message, length, &cur_token, &min);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r == MAILIMF_NO_ERROR) {
    r = mailimf_second_parse(message, length, &cur_token, &sec);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }
  else if (r == MAILIMF_ERROR_PARSE)
    sec = 0;
  else
    return r;

  * phour = hour;
  * pmin = min;
  * psec = sec;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
hour            =       2DIGIT / obs-hour
*/

static int mailimf_hour_parse(const char * message, size_t length,
			      size_t * index, int * result)
{
  uint32_t hour;
  int r;

  r = mailimf_number_parse(message, length, index, &hour);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * result = hour;

  return MAILIMF_NO_ERROR;
}

/*
minute          =       2DIGIT / obs-minute
*/

static int mailimf_minute_parse(const char * message, size_t length,
				size_t * index, int * result)
{
  uint32_t minute;
  int r;

  r = mailimf_number_parse(message, length, index, &minute);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * result = minute;

  return MAILIMF_NO_ERROR;
}

/*
second          =       2DIGIT / obs-second
*/

static int mailimf_second_parse(const char * message, size_t length,
				size_t * index, int * result)
{
  uint32_t second;
  int r;

  r = mailimf_number_parse(message, length, index, &second);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * result = second;

  return MAILIMF_NO_ERROR;
}

/*
zone            =       (( "+" / "-" ) 4DIGIT) / obs-zone
*/

/*
obs-zone        =       "UT" / "GMT" /          ; Universal Time
                                                ; North American UT
                                                ; offsets
                        "EST" / "EDT" /         ; Eastern:  - 5/ - 4
                        "CST" / "CDT" /         ; Central:  - 6/ - 5
                        "MST" / "MDT" /         ; Mountain: - 7/ - 6
                        "PST" / "PDT" /         ; Pacific:  - 8/ - 7

                        %d65-73 /               ; Military zones - "A"
                        %d75-90 /               ; through "I" and "K"
                        %d97-105 /              ; through "Z", both
                        %d107-122               ; upper and lower case
*/

enum {
  STATE_ZONE_1 = 0,
  STATE_ZONE_2 = 1,
  STATE_ZONE_3 = 2,
  STATE_ZONE_OK  = 3,
  STATE_ZONE_ERR = 4,
  STATE_ZONE_CONT = 5
};

static int mailimf_zone_parse(const char * message, size_t length,
			      size_t * index, int * result)
{
  uint32_t zone;
  int sign;
  size_t cur_token;
  int r;

  cur_token = * index;

  if (cur_token + 1 < length) {
    if ((message[cur_token] == 'U') && (message[cur_token] == 'T')) {
      * result = TRUE;
      * index = cur_token + 2;

      return MAILIMF_NO_ERROR;
    }
  }

  if (cur_token + 2 < length) {
    int state;

    state = STATE_ZONE_1;
    
    while (state <= 2) {
      switch (state) {
      case STATE_ZONE_1:
	switch (message[cur_token]) {
	case 'G':
	  if (message[cur_token + 1] == 'M' && message[cur_token + 2] == 'T') {
	    zone = 0;
	    state = STATE_ZONE_OK;
	  }
	  else {
	    state = STATE_ZONE_ERR;
	  }
	  break;
	case 'E':
	  zone = -5;
	  state = STATE_ZONE_2;
	  break;
	case 'C':
	  zone = -6;
	  state = STATE_ZONE_2;
	  break;
	case 'M':
	  zone = -7;
	  state = STATE_ZONE_2;
	  break;
	case 'P':
	  zone = -8;
	  state = STATE_ZONE_2;
	  break;
	default:
	  state = STATE_ZONE_CONT;
	  break;
	}
	break;
      case STATE_ZONE_2:
	switch (message[cur_token + 1]) {
	case 'S':
	  state = STATE_ZONE_3;
	  break;
	case 'D':
	  zone ++;
	  state = STATE_ZONE_3;
	  break;
	default:
	  state = STATE_ZONE_ERR;
	  break;
	}
	break;
      case STATE_ZONE_3:
	if (message[cur_token + 2] == 'T') {
	  zone *= 100;
	  state = STATE_ZONE_OK;
	}
	else
	  state = STATE_ZONE_ERR;
	break;
      }
    }

    switch (state) {
    case STATE_ZONE_OK:
      * result = zone;
      * index = cur_token + 3;
      return MAILIMF_NO_ERROR;
      
    case STATE_ZONE_ERR:
      return MAILIMF_ERROR_PARSE;
    }
  }

  sign = 1;
  r = mailimf_plus_parse(message, length, &cur_token);
  if (r == MAILIMF_NO_ERROR)
    sign = 1;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_minus_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR)
      sign = -1;
  }

  if (r == MAILIMF_NO_ERROR) {
    /* do nothing */
  }
  else if (r == MAILIMF_ERROR_PARSE)
    sign = 1;
  else
    return r;

  r = mailimf_number_parse(message, length, &cur_token, &zone);
  if (r != MAILIMF_NO_ERROR)
    return r;

  zone = zone * sign;

  * index = cur_token;
  * result = zone;

  return MAILIMF_NO_ERROR;
}

/*
address         =       mailbox / group
*/

int mailimf_address_parse(const char * message, size_t length,
			  size_t * index,
			  struct mailimf_address ** result)
{
  int type;
  size_t cur_token;
  struct mailimf_mailbox * mailbox;
  struct mailimf_group * group;
  struct mailimf_address * address;
  int r;
  int res;

  cur_token = * index;

  mailbox = NULL;
  group = NULL;

  type = MAILIMF_ADDRESS_ERROR; /* XXX - removes a gcc warning */
  r = mailimf_group_parse(message, length, &cur_token, &group);
  if (r == MAILIMF_NO_ERROR)
    type = MAILIMF_ADDRESS_GROUP;
  
  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_mailbox_parse(message, length, &cur_token, &mailbox);
    if (r == MAILIMF_NO_ERROR)
      type = MAILIMF_ADDRESS_MAILBOX;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  address = mailimf_address_new(type, mailbox, group);
  if (address == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = address;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
  
 free:
  if (mailbox != NULL)
    mailimf_mailbox_free(mailbox);
  if (group != NULL)
    mailimf_group_free(group);
 err:
  return res;
}


/*
mailbox         =       name-addr / addr-spec
*/


int mailimf_mailbox_parse(const char * message, size_t length,
			  size_t * index,
			  struct mailimf_mailbox ** result)
{
  size_t cur_token;
  char * display_name;
  struct mailimf_mailbox * mailbox;
  char * addr_spec;
  int r;
  int res;

  cur_token = * index;
  display_name = NULL;
  addr_spec = NULL;

  r = mailimf_name_addr_parse(message, length, &cur_token,
			      &display_name, &addr_spec);
  if (r == MAILIMF_ERROR_PARSE)
    r = mailimf_addr_spec_parse(message, length, &cur_token, &addr_spec);

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  mailbox = mailimf_mailbox_new(display_name, addr_spec);
  if (mailbox == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = mailbox;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (display_name != NULL)
    mailimf_display_name_free(display_name);
  if (addr_spec != NULL)
    mailimf_addr_spec_free(addr_spec);
 err:
  return res;
}

/*
name-addr       =       [display-name] angle-addr
*/

static int mailimf_name_addr_parse(const char * message, size_t length,
				   size_t * index,
				   char ** pdisplay_name,
				   char ** pangle_addr)
{
  char * display_name;
  char * angle_addr;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  display_name = NULL;
  angle_addr = NULL;

  r = mailimf_display_name_parse(message, length, &cur_token, &display_name);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_angle_addr_parse(message, length, &cur_token, &angle_addr);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_display_name;
  }

  * pdisplay_name = display_name;
  * pangle_addr = angle_addr;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_display_name:
  if (display_name != NULL)
    mailimf_display_name_free(display_name);
 err:
  return res;
}

/*
angle-addr      =       [CFWS] "<" addr-spec ">" [CFWS] / obs-angle-addr
*/

static int mailimf_angle_addr_parse(const char * message, size_t length,
				    size_t * index, char ** result)
{
  size_t cur_token;
  char * addr_spec;
  int r;
  
  cur_token = * index;
  
  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;
  
  r = mailimf_lower_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailimf_addr_spec_parse(message, length, &cur_token, &addr_spec);
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailimf_greater_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    free(addr_spec);
    return r;
  }

  * result = addr_spec;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
group           =       display-name ":" [mailbox-list / CFWS] ";"
                        [CFWS]
*/

static int mailimf_group_parse(const char * message, size_t length,
			       size_t * index,
			       struct mailimf_group ** result)
{
  size_t cur_token;
  char * display_name;
  struct mailimf_mailbox_list * mailbox_list;
  struct mailimf_group * group;
  int r;
  int res;

  cur_token = * index;

  mailbox_list = NULL;

  r = mailimf_display_name_parse(message, length, &cur_token, &display_name);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_display_name;
  }

  r = mailimf_mailbox_list_parse(message, length, &cur_token, &mailbox_list);
  switch (r) {
  case MAILIMF_NO_ERROR:
    break;
  case MAILIMF_ERROR_PARSE:
    r = mailimf_cfws_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
      return r;
    break;
  default:
    return r;
  }

  r = mailimf_semi_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_mailbox_list;
  }

  group = mailimf_group_new(display_name, mailbox_list);
  if (group == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_mailbox_list;
  }

  * index = cur_token;
  * result = group;

  return MAILIMF_NO_ERROR;

 free_mailbox_list:
  mailimf_mailbox_list_free(mailbox_list);
 free_display_name:
  mailimf_display_name_free(display_name);
 err:
  return res;
}

/*
display-name    =       phrase
*/

static int mailimf_display_name_parse(const char * message, size_t length,
				      size_t * index, char ** result)
{
  return mailimf_phrase_parse(message, length, index, result);
}

/*
mailbox-list    =       (mailbox *("," mailbox)) / obs-mbox-list
*/

int
mailimf_mailbox_list_parse(const char * message, size_t length,
			   size_t * index,
			   struct mailimf_mailbox_list ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_mailbox_list * mailbox_list;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_struct_list_parse(message, length, 
				&cur_token, &list, ',',
				(mailimf_struct_parser *)
				mailimf_mailbox_parse,
				(mailimf_struct_destructor *)
				mailimf_mailbox_free);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  mailbox_list = mailimf_mailbox_list_new(list);
  if (mailbox_list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = mailbox_list;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_mailbox_free, NULL);
  clist_free(list);
 err:
  return res;
}				   

/*
address-list    =       (address *("," address)) / obs-addr-list
*/


int
mailimf_address_list_parse(const char * message, size_t length,
			   size_t * index,
			   struct mailimf_address_list ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_address_list * address_list;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_struct_list_parse(message, length,
				&cur_token, &list, ',',
				(mailimf_struct_parser *)
				mailimf_address_parse,
				(mailimf_struct_destructor *)
				mailimf_address_free);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  address_list = mailimf_address_list_new(list);
  if (address_list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = address_list;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_address_free, NULL);
  clist_free(list);
 err:
  return res;
}				   

/*
addr-spec       =       local-part "@" domain
*/


static int mailimf_addr_spec_parse(const char * message, size_t length,
				   size_t * index,
				   char ** result)
{
  size_t cur_token;
#if 0
  char * local_part;
  char * domain;
#endif
  char * addr_spec;
  int r;
  int res;
  size_t begin;
  size_t end;
  int final;
  size_t count;
  const char * src;
  char * dest;
  size_t i;
  
  cur_token = * index;
  
  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  end = cur_token;
  if (end >= length) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  begin = cur_token;

  final = FALSE;
  while (1) {
    switch (message[end]) {
    case '>':
    case ',':
    case '\r':
    case '\n':
    case '(':
    case ')':
    case ':':
    case ';':
      final = TRUE;
      break;
    }

    if (final)
      break;

    end ++;
    if (end >= length)
      break;
  }

  if (end == begin) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }
  
  addr_spec = malloc(end - cur_token + 1);
  if (addr_spec == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }
  
  count = end - cur_token;
  src = message + cur_token;
  dest = addr_spec;
  for(i = 0 ; i < count ; i ++) {
    if ((* src != ' ') && (* src != '\t')) {
      * dest = * src;
      dest ++;
    }
    src ++;
  }
  * dest = '\0';
  
#if 0
  strncpy(addr_spec, message + cur_token, end - cur_token);
  addr_spec[end - cur_token] = '\0';
#endif

  cur_token = end;

#if 0
  r = mailimf_local_part_parse(message, length, &cur_token, &local_part);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_at_sign_parse(message, length, &cur_token);
  switch (r) {
  case MAILIMF_NO_ERROR:
    r = mailimf_domain_parse(message, length, &cur_token, &domain);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto free_local_part;
    }
    break;

  case MAILIMF_ERROR_PARSE:
    domain = NULL;
    break;

  default:
    res = r;
    goto free_local_part;
  }

  if (domain) {
    addr_spec = malloc(strlen(local_part) + strlen(domain) + 2);
    if (addr_spec == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto free_domain;
    }
    
    strcpy(addr_spec, local_part);
    strcat(addr_spec, "@");
    strcat(addr_spec, domain);

    mailimf_domain_free(domain);
    mailimf_local_part_free(local_part);
  }
  else {
    addr_spec = local_part;
  }
#endif

  * result = addr_spec;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

#if 0
 free_domain:
  mailimf_domain_free(domain);
 free_local_part:
  mailimf_local_part_free(local_part);
#endif
 err:
  return res;
}

/*
local-part      =       dot-atom / quoted-string / obs-local-part
*/

#if 0
static int mailimf_local_part_parse(const char * message, size_t length,
				    size_t * index,
				    char ** result)
{
  int r;

  r = mailimf_dot_atom_parse(message, length, index, result);
  switch (r) {
  case MAILIMF_NO_ERROR:
    return r;
  case MAILIMF_ERROR_PARSE:
    break;
  default:
    return r;
  }

  r = mailimf_quoted_string_parse(message, length, index, result);
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}
#endif

/*
domain          =       dot-atom / domain-literal / obs-domain
*/

#if 0
static int mailimf_domain_parse(const char * message, size_t length,
				size_t * index,
				char ** result)
{
  int r;

  r = mailimf_dot_atom_parse(message, length, index, result);
  switch (r) {
  case MAILIMF_NO_ERROR:
    return r;
  case MAILIMF_ERROR_PARSE:
    break;
  default:
    return r;
  }

  r = mailimf_domain_literal_parse(message, length, index, result);
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}
#endif

/*
[FWS] dcontent
*/

#if 0
static int
mailimf_domain_literal_fws_dcontent_parse(const char * message, size_t length,
					  size_t * index)
{
  size_t cur_token;
  char ch;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;
  
  r = mailimf_dcontent_parse(message, length, &cur_token, &ch);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;

  return MAILIMF_NO_ERROR;
}
#endif

/*
domain-literal  =       [CFWS] "[" *([FWS] dcontent) [FWS] "]" [CFWS]
*/

#if 0
static int mailimf_domain_literal_parse(const char * message, size_t length,
					size_t * index, char ** result)
{
  size_t cur_token;
  int len;
  int begin;
  char * domain_literal;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  begin = cur_token;
  r = mailimf_obracket_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  while (1) {
    r = mailimf_domain_literal_fws_dcontent_parse(message, length,
						  &cur_token);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE)
      break;
    else
      return r;
  }

  r = mailimf_fws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_cbracket_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  len = cur_token - begin;

  domain_literal = malloc(len + 1);
  if (domain_literal == NULL)
    return MAILIMF_ERROR_MEMORY;
  strncpy(domain_literal, message + begin, len);
  domain_literal[len] = '\0';

  * result = domain_literal;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}
#endif

/*
dcontent        =       dtext / quoted-pair
*/

#if 0
static int mailimf_dcontent_parse(const char * message, size_t length,
				  size_t * index, char * result)
{
  size_t cur_token;
  char ch;
  int r;
  
  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (is_dtext(message[cur_token])) {
    ch = message[cur_token];
    cur_token ++;
  }
  else {
    r = mailimf_quoted_pair_parse(message, length, &cur_token, &ch);
    
    if (r != MAILIMF_NO_ERROR)
      return r;
  }
    
  * index = cur_token;
  * result = ch;

  return MAILIMF_NO_ERROR;
}
#endif


/*
dtext           =       NO-WS-CTL /     ; Non white space controls

                        %d33-90 /       ; The rest of the US-ASCII
                        %d94-126        ;  characters not including "[",
                                        ;  "]", or "\"
*/

static inline int is_dtext(char ch)
{
  unsigned char uch = (unsigned char) ch;

  if (is_no_ws_ctl(ch))
    return TRUE;

  if (uch < 33)
    return FALSE;

  if ((uch >= 91) && (uch <= 93))
    return FALSE;

  if (uch == 127)
    return FALSE;

  return TRUE;
}

/*
message         =       (fields / obs-fields)
                        [CRLF body]
*/

int mailimf_message_parse(const char * message, size_t length,
			  size_t * index,
			  struct mailimf_message ** result)
{
  struct mailimf_fields * fields;
  struct mailimf_body * body;
  struct mailimf_message * msg;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_fields_parse(message, length, &cur_token, &fields);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_crlf_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_body_parse(message, length, &cur_token, &body);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_fields;
  }

  msg = mailimf_message_new(fields, body);
  if (msg == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_body;
  }

  * index = cur_token;
  * result = msg;

  return MAILIMF_NO_ERROR;

 free_body:
  mailimf_body_free(body);
 free_fields:
  mailimf_fields_free(fields);
 err:
  return res;
}

/*
body            =       *(*998text CRLF) *998text
*/

int mailimf_body_parse(const char * message, size_t length,
		       size_t * index,
		       struct mailimf_body ** result)
{
  size_t cur_token;
  struct mailimf_body * body;

  cur_token = * index;

  body = mailimf_body_new(message + cur_token, length - cur_token);
  if (body == NULL)
    return MAILIMF_ERROR_MEMORY;

  cur_token = length;

  * result = body;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
CHANGE TO THE RFC 2822

original :

fields          =       *(trace
                          *(resent-date /
                           resent-from /
                           resent-sender /
                           resent-to /
                           resent-cc /
                           resent-bcc /
                           resent-msg-id))
                        *(orig-date /
                        from /
                        sender /
                        reply-to /
                        to /
                        cc /
                        bcc /
                        message-id /
                        in-reply-to /
                        references /
                        subject /
                        comments /
                        keywords /
                        optional-field)

INTO THE FOLLOWING :
*/

/*
resent-fields-list =      *(resent-date /
                           resent-from /
                           resent-sender /
                           resent-to /
                           resent-cc /
                           resent-bcc /
                           resent-msg-id))
*/

#if 0
enum {
  RESENT_HEADER_START,
};

static int guess_resent_header_type(char * message,
				    size_t length, size_t index)
{
  int r;

  r = mailimf_token_case_insensitive_parse(message,
					   length, &index, "Resent-");
  if (r != MAILIMF_NO_ERROR)
    return MAILIMF_RESENT_FIELD_NONE;
  
  if (index >= length)
    return MAILIMF_RESENT_FIELD_NONE;

  switch(toupper(message[index])) {
  case 'D':
    return MAILIMF_RESENT_FIELD_DATE;
  case 'F':
    return MAILIMF_RESENT_FIELD_FROM;
  case 'S':
    return MAILIMF_RESENT_FIELD_SENDER;
  case 'T':
    return MAILIMF_RESENT_FIELD_TO;
  case 'C':
    return MAILIMF_RESENT_FIELD_CC;
  case 'B':
    return MAILIMF_RESENT_FIELD_BCC;
  case 'M':
    return MAILIMF_RESENT_FIELD_MSG_ID;
  default:
    return MAILIMF_RESENT_FIELD_NONE;
  }
}
#endif

#if 0
static int
mailimf_resent_field_parse(const char * message, size_t length,
			   size_t * index,
			   struct mailimf_resent_field ** result)
{
  struct mailimf_orig_date * resent_date;
  struct mailimf_from * resent_from;
  struct mailimf_sender * resent_sender;
  struct mailimf_to* resent_to;
  struct mailimf_cc * resent_cc;
  struct mailimf_bcc * resent_bcc;
  struct mailimf_message_id * resent_msg_id;
  size_t cur_token;
  int type;
  struct mailimf_resent_field * resent_field;
  int r;
  int res;

  cur_token = * index;

  resent_date = NULL;
  resent_from = NULL;
  resent_sender = NULL;
  resent_to = NULL;
  resent_cc = NULL;
  resent_bcc = NULL;
  resent_msg_id = NULL;

  type = guess_resent_header_type(message, length, cur_token);

  switch(type) {
  case MAILIMF_RESENT_FIELD_DATE:
    r = mailimf_resent_date_parse(message, length, &cur_token,
				  &resent_date);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_RESENT_FIELD_FROM:
    r = mailimf_resent_from_parse(message, length, &cur_token,
				  &resent_from);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_RESENT_FIELD_SENDER:
    r = mailimf_resent_sender_parse(message, length, &cur_token,
				    &resent_sender);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_RESENT_FIELD_TO:
    r = mailimf_resent_to_parse(message, length, &cur_token,
				&resent_to);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_RESENT_FIELD_CC:
    r= mailimf_resent_cc_parse(message, length, &cur_token,
			       &resent_cc);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_RESENT_FIELD_BCC:
    r = mailimf_resent_bcc_parse(message, length, &cur_token,
				 &resent_bcc);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_RESENT_FIELD_MSG_ID:
    r = mailimf_resent_msg_id_parse(message, length, &cur_token,
				    &resent_msg_id);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  default:
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  resent_field = mailimf_resent_field_new(type, resent_date,
					  resent_from, resent_sender,
					  resent_to, resent_cc,
					  resent_bcc, resent_msg_id);
  if (resent_field == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_resent;
  }

  * result = resent_field;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_resent:
  if (resent_msg_id != NULL)
    mailimf_message_id_free(resent_msg_id);
  if (resent_bcc != NULL)
    mailimf_bcc_free(resent_bcc);
  if (resent_cc != NULL)
    mailimf_cc_free(resent_cc);
  if (resent_to != NULL)
    mailimf_to_free(resent_to);
  if (resent_sender != NULL)
    mailimf_sender_free(resent_sender);
  if (resent_from != NULL)
    mailimf_from_free(resent_from);
  if (resent_date != NULL)
    mailimf_orig_date_free(resent_date);
 err:
  return res;
}
#endif

#if 0
static int
mailimf_resent_fields_list_parse(const char * message, size_t length,
				 size_t * index,
				 struct mailimf_resent_fields_list ** result)
{
  clist * list;
  size_t cur_token;
  struct mailimf_resent_fields_list * resent_fields_list;
  int r;
  int res;

  cur_token = * index;
  list = NULL;

  r = mailimf_struct_multiple_parse(message, length, &cur_token, &list,
				    (mailimf_struct_parser *)
				    mailimf_resent_field_parse,
				    (mailimf_struct_destructor *)
				    mailimf_resent_field_free);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  resent_fields_list = mailimf_resent_fields_list_new(list);
  if (resent_fields_list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = resent_fields_list;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_resent_field_free, NULL);
  clist_free(list);
 err:
  return res;
}
#endif

/*
 ([trace]
  [resent-fields-list])
*/

#if 0
static int
mailimf_trace_resent_fields_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_trace_resent_fields ** result)
{
  size_t cur_token;
  struct mailimf_return * return_path;
  struct mailimf_resent_fields_list * resent_fields;
  struct mailimf_trace_resent_fields * trace_resent_fields;
  int res;
  int r;

  cur_token = * index;

  return_path = NULL;
  resent_fields = NULL;

  r = mailimf_return_parse(message, length, &cur_token,
			   &return_path);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_resent_fields_list_parse(message, length, &cur_token,
				       &resent_fields);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  if ((return_path == NULL) && (resent_fields == NULL)) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  trace_resent_fields = mailimf_trace_resent_fields_new(return_path,
							resent_fields);
  if (trace_resent_fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_resent_fields;
  }

  * result = trace_resent_fields;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_resent_fields:
  if (resent_fields != NULL)
    mailimf_resent_fields_list_free(resent_fields);
  if (return_path != NULL)
    mailimf_return_free(return_path);
 err:
  return res;
}
#endif

/*
delivering-info =       *([trace]
                          [resent-fields-list])
*/

#if 0
static int
mailimf_delivering_info_parse(const char * message, size_t length,
			      size_t * index,
			      struct mailimf_delivering_info ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_delivering_info * delivering_info;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_struct_multiple_parse(message, length, &cur_token,
				    &list,
				    (mailimf_struct_parser *)
				    mailimf_trace_resent_fields_parse,
				    (mailimf_struct_destructor *)
				    mailimf_trace_resent_fields_free);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  delivering_info = mailimf_delivering_info_new(list);
  if (delivering_info == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = delivering_info;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_trace_resent_fields_free, NULL);
  clist_free(list);
 err:
  return res;
}
#endif

/*
field           =       delivering-info /
                        orig-date /
                        from /
                        sender /
                        reply-to /
                        to /
                        cc /
                        bcc /
                        message-id /
                        in-reply-to /
                        references /
                        subject /
                        comments /
                        keywords /
                        optional-field
*/

enum {
  HEADER_START,
  HEADER_C,
  HEADER_R,
  HEADER_RE,
  HEADER_S,
  HEADER_RES
};

static int guess_header_type(const char * message, size_t length, size_t index)
{
  int state;
  int r;

  state = HEADER_START;
  
  while (1) {

    if (index >= length)
      return MAILIMF_FIELD_NONE;

    switch(state) {
    case HEADER_START:
      switch((char) toupper((unsigned char) message[index])) {
      case 'B':
	return MAILIMF_FIELD_BCC;
      case 'C':
	state = HEADER_C;
	break;
      case 'D':
	return MAILIMF_FIELD_ORIG_DATE;
      case 'F':
	return MAILIMF_FIELD_FROM;
      case 'I':
	return MAILIMF_FIELD_IN_REPLY_TO;
      case 'K':
	return MAILIMF_FIELD_KEYWORDS;
      case 'M':
	return MAILIMF_FIELD_MESSAGE_ID;
      case 'R':
	state = HEADER_R;
	break;
      case 'T':
	return MAILIMF_FIELD_TO;
	break;
      case 'S':
	state = HEADER_S;
	break;
      default:
	return MAILIMF_FIELD_NONE;
      }
      break;
    case HEADER_C:
      switch((char) toupper((unsigned char) message[index])) {
      case 'O':
	return MAILIMF_FIELD_COMMENTS;
      case 'C':
	return MAILIMF_FIELD_CC;
      default:
	return MAILIMF_FIELD_NONE;
      }
      break;
    case HEADER_R:
      switch((char) toupper((unsigned char) message[index])) {
      case 'E':
	state = HEADER_RE;
	break;
      default:
	return MAILIMF_FIELD_NONE;
      }
      break;
    case HEADER_RE:
      switch((char) toupper((unsigned char) message[index])) {
      case 'F':
	return MAILIMF_FIELD_REFERENCES;
      case 'P':
	return MAILIMF_FIELD_REPLY_TO;
      case 'S':
        state = HEADER_RES;
        break;
      case 'T':
        return MAILIMF_FIELD_RETURN_PATH;
      default:
	return MAILIMF_FIELD_NONE;
      }
      break;
    case HEADER_S:
      switch((char) toupper((unsigned char) message[index])) {
      case 'E':
	return MAILIMF_FIELD_SENDER;
      case 'U':
	return MAILIMF_FIELD_SUBJECT;
      default:
	return MAILIMF_FIELD_NONE;
      }
      break;

    case HEADER_RES:
      r = mailimf_token_case_insensitive_parse(message,
          length, &index, "ent-");
      if (r != MAILIMF_NO_ERROR)
        return MAILIMF_FIELD_NONE;
      
      if (index >= length)
        return MAILIMF_FIELD_NONE;
      
      switch((char) toupper((unsigned char) message[index])) {
      case 'D':
        return MAILIMF_FIELD_RESENT_DATE;
      case 'F':
        return MAILIMF_FIELD_RESENT_FROM;
      case 'S':
        return MAILIMF_FIELD_RESENT_SENDER;
      case 'T':
        return MAILIMF_FIELD_RESENT_TO;
      case 'C':
        return MAILIMF_FIELD_RESENT_CC;
      case 'B':
        return MAILIMF_FIELD_RESENT_BCC;
      case 'M':
        return MAILIMF_FIELD_RESENT_MSG_ID;
      default:
        return MAILIMF_FIELD_NONE;
      }
      break;
    }
    index ++;
  }
}

static int mailimf_field_parse(const char * message, size_t length,
			       size_t * index,
			       struct mailimf_field ** result)
{
  size_t cur_token;
  int type;
  struct mailimf_return * return_path;
  struct mailimf_orig_date * resent_date;
  struct mailimf_from * resent_from;
  struct mailimf_sender * resent_sender;
  struct mailimf_to* resent_to;
  struct mailimf_cc * resent_cc;
  struct mailimf_bcc * resent_bcc;
  struct mailimf_message_id * resent_msg_id;
  struct mailimf_orig_date * orig_date;
  struct mailimf_from * from;
  struct mailimf_sender * sender;
  struct mailimf_reply_to * reply_to;
  struct mailimf_to * to;
  struct mailimf_cc * cc;
  struct mailimf_bcc * bcc;
  struct mailimf_message_id * message_id;
  struct mailimf_in_reply_to * in_reply_to;
  struct mailimf_references * references;
  struct mailimf_subject * subject;
  struct mailimf_comments * comments;
  struct mailimf_keywords * keywords;
  struct mailimf_optional_field * optional_field;
  struct mailimf_field * field;
  int guessed_type;
  int r;
  int res;
  
  cur_token = * index;

  return_path = NULL;
  resent_date = NULL;
  resent_from = NULL;
  resent_sender = NULL;
  resent_to = NULL;
  resent_cc = NULL;
  resent_bcc = NULL;
  resent_msg_id = NULL;
  orig_date = NULL;
  from = NULL;
  sender = NULL;
  reply_to = NULL;
  to = NULL;
  cc = NULL;
  bcc = NULL;
  message_id = NULL;
  in_reply_to = NULL;
  references = NULL;
  subject = NULL;
  comments = NULL;
  keywords = NULL;
  optional_field = NULL;

  guessed_type = guess_header_type(message, length, cur_token);
  type = MAILIMF_FIELD_NONE;

  switch (guessed_type) {
  case MAILIMF_FIELD_ORIG_DATE:
    r = mailimf_orig_date_parse(message, length, &cur_token,
				&orig_date);
    if (r == MAILIMF_NO_ERROR)
      type = MAILIMF_FIELD_ORIG_DATE;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_FROM:
    r = mailimf_from_parse(message, length, &cur_token,
			   &from);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_SENDER:
    r = mailimf_sender_parse(message, length, &cur_token,
			     &sender);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_REPLY_TO:
    r = mailimf_reply_to_parse(message, length, &cur_token,
			       &reply_to);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_TO:
    r = mailimf_to_parse(message, length, &cur_token,
			 &to);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_CC:
    r = mailimf_cc_parse(message, length, &cur_token,
			 &cc);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_BCC:
    r = mailimf_bcc_parse(message, length, &cur_token,
			  &bcc);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_MESSAGE_ID:
    r = mailimf_message_id_parse(message, length, &cur_token,
				 &message_id);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_IN_REPLY_TO:
    r = mailimf_in_reply_to_parse(message, length, &cur_token,
				  &in_reply_to);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_REFERENCES:
    r = mailimf_references_parse(message, length, &cur_token,
				 &references);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_SUBJECT:
    r = mailimf_subject_parse(message, length, &cur_token,
			      &subject);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_COMMENTS:
    r = mailimf_comments_parse(message, length, &cur_token,
			       &comments);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_KEYWORDS:
    r = mailimf_keywords_parse(message, length, &cur_token,
			       &keywords);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RETURN_PATH:
    r = mailimf_return_parse(message, length, &cur_token,
        &return_path);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RESENT_DATE:
    r = mailimf_resent_date_parse(message, length, &cur_token,
				  &resent_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RESENT_FROM:
    r = mailimf_resent_from_parse(message, length, &cur_token,
				  &resent_from);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RESENT_SENDER:
    r = mailimf_resent_sender_parse(message, length, &cur_token,
				    &resent_sender);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RESENT_TO:
    r = mailimf_resent_to_parse(message, length, &cur_token,
				&resent_to);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RESENT_CC:
    r= mailimf_resent_cc_parse(message, length, &cur_token,
			       &resent_cc);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RESENT_BCC:
    r = mailimf_resent_bcc_parse(message, length, &cur_token,
				 &resent_bcc);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_RESENT_MSG_ID:
    r = mailimf_resent_msg_id_parse(message, length, &cur_token,
				    &resent_msg_id);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  }

  if (type == MAILIMF_FIELD_NONE) {
    r = mailimf_optional_field_parse(message, length, &cur_token,
        &optional_field);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }

    type = MAILIMF_FIELD_OPTIONAL_FIELD;
  }

  field = mailimf_field_new(type, return_path, resent_date,
      resent_from, resent_sender, resent_to, resent_cc, resent_bcc,
      resent_msg_id, orig_date, from, sender, reply_to, to,
      cc, bcc, message_id, in_reply_to, references,
      subject, comments, keywords, optional_field);
  if (field == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_field;
  }

  * result = field;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_field:
  if (return_path != NULL)
    mailimf_return_free(return_path);
  if (resent_date != NULL)
    mailimf_orig_date_free(resent_date);
  if (resent_from != NULL)
    mailimf_from_free(resent_from);
  if (resent_sender != NULL)
    mailimf_sender_free(resent_sender);
  if (resent_to != NULL)
    mailimf_to_free(resent_to);
  if (resent_cc != NULL)
    mailimf_cc_free(resent_cc);
  if (resent_bcc != NULL)
    mailimf_bcc_free(resent_bcc);
  if (resent_msg_id != NULL)
    mailimf_message_id_free(resent_msg_id);
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
  if (comments != NULL)
    mailimf_comments_free(comments);
  if (keywords != NULL)
    mailimf_keywords_free(keywords);
  if (optional_field != NULL)
    mailimf_optional_field_free(optional_field);
 err:
  return res;
}


/*
fields          =       *(delivering-info /
			orig-date /
                        from /
                        sender /
                        reply-to /
                        to /
                        cc /
                        bcc /
                        message-id /
                        in-reply-to /
                        references /
                        subject /
                        comments /
                        keywords /
                        optional-field)
*/

#if 0
int
mailimf_unparsed_fields_parse(const char * message, size_t length,
			      size_t * index,
			      struct mailimf_unparsed_fields ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_unparsed_fields * fields;
  int r;
  int res;

  cur_token = * index;

  list = NULL;

  r = mailimf_struct_multiple_parse(message, length, &cur_token,
				    &list,
				    (mailimf_struct_parser *)
				    mailimf_optional_field_parse,
				    (mailimf_struct_destructor *)
				    mailimf_optional_field_free);
  /*
  if ((r = MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }
  */

  switch (r) {
  case MAILIMF_NO_ERROR:
    /* do nothing */
    break;

  case MAILIMF_ERROR_PARSE:
    list = clist_new();
    if (list == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }
    break;

  default:
    res = r;
    goto err;
  }

  fields = mailimf_unparsed_fields_new(list);
  if (fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = fields;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimf_optional_field_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}
#endif

int mailimf_fields_parse(const char * message, size_t length,
			 size_t * index,
			 struct mailimf_fields ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_fields * fields;
  int r;
  int res;

  cur_token = * index;

  list = NULL;

  r = mailimf_struct_multiple_parse(message, length, &cur_token,
				    &list,
				    (mailimf_struct_parser *)
				    mailimf_field_parse,
				    (mailimf_struct_destructor *)
				    mailimf_field_free);
  /*
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }
  */

  switch (r) {
  case MAILIMF_NO_ERROR:
    /* do nothing */
    break;

  case MAILIMF_ERROR_PARSE:
    list = clist_new();
    if (list == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }
    break;

  default:
    res = r;
    goto err;
  }

  fields = mailimf_fields_new(list);
  if (fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = fields;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimf_field_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}

/*
orig-date       =       "Date:" date-time CRLF
*/


static int
mailimf_orig_date_parse(const char * message, size_t length,
			size_t * index, struct mailimf_orig_date ** result)
{
  struct mailimf_date_time * date_time;
  struct mailimf_orig_date * orig_date;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Date:");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_date_time_parse(message, length, &cur_token, &date_time);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_ignore_unstructured_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_date_time;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_date_time;
  }

  orig_date = mailimf_orig_date_new(date_time);
  if (orig_date == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_date_time;
  }

  * result = orig_date;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_date_time:
  mailimf_date_time_free(date_time);
 err:
  return res;
}

/*
from            =       "From:" mailbox-list CRLF
*/

static int
mailimf_from_parse(const char * message, size_t length,
		   size_t * index, struct mailimf_from ** result)
{
  struct mailimf_mailbox_list * mb_list;
  struct mailimf_from * from;
  size_t cur_token;
  int r;
  int res;

  cur_token =  * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "From");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_mailbox_list_parse(message, length, &cur_token, &mb_list);

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_mb_list;
  }

  from = mailimf_from_new(mb_list);
  if (from == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_mb_list;
  }

  * result = from;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_mb_list:
  mailimf_mailbox_list_free(mb_list);
 err:
  return res;
}

/*
sender          =       "Sender:" mailbox CRLF
*/

static int
mailimf_sender_parse(const char * message, size_t length,
		     size_t * index, struct mailimf_sender ** result)
{
  struct mailimf_mailbox * mb;
  struct mailimf_sender * sender;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Sender");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_mailbox_parse(message, length, &cur_token, &mb);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_mb;
  }

  sender = mailimf_sender_new(mb);
  if (sender == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_mb;
  }

  * result = sender;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_mb:
  mailimf_mailbox_free(mb);
 err:
  return res;
}

/*
reply-to        =       "Reply-To:" address-list CRLF
*/


static int
mailimf_reply_to_parse(const char * message, size_t length,
		       size_t * index, struct mailimf_reply_to ** result)
{
  struct mailimf_address_list * addr_list;
  struct mailimf_reply_to * reply_to;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Reply-To");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_address_list_parse(message, length, &cur_token, &addr_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_addr_list;
  }

  reply_to = mailimf_reply_to_new(addr_list);
  if (reply_to == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_addr_list;
  }

  * result = reply_to;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_addr_list:
  mailimf_address_list_free(addr_list);
 err:
  return res;
}

/*
to              =       "To:" address-list CRLF
*/

static int
mailimf_to_parse(const char * message, size_t length,
		 size_t * index, struct mailimf_to ** result)
{
  struct mailimf_address_list * addr_list;
  struct mailimf_to * to;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "To");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_address_list_parse(message, length, &cur_token, &addr_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_addr_list;
  }

  to = mailimf_to_new(addr_list);
  if (to == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_addr_list;
  }

  * result = to;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_addr_list:
  mailimf_address_list_free(addr_list);
 err:
  return res;
}

/*
cc              =       "Cc:" address-list CRLF
*/


static int
mailimf_cc_parse(const char * message, size_t length,
		 size_t * index, struct mailimf_cc ** result)
{
  struct mailimf_address_list * addr_list;
  struct mailimf_cc * cc;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Cc");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_address_list_parse(message, length, &cur_token, &addr_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_addr_list;
  }

  cc = mailimf_cc_new(addr_list);
  if (cc == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_addr_list;
  }

  * result = cc;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_addr_list:
  mailimf_address_list_free(addr_list);
 err:
  return res;
}

/*
bcc             =       "Bcc:" (address-list / [CFWS]) CRLF
*/


static int
mailimf_bcc_parse(const char * message, size_t length,
		  size_t * index, struct mailimf_bcc ** result)
{
  struct mailimf_address_list * addr_list;
  struct mailimf_bcc * bcc;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;
  addr_list = NULL;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Bcc");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_address_list_parse(message, length, &cur_token, &addr_list);
  switch (r) {
  case MAILIMF_NO_ERROR:
    /* do nothing */
    break;
  case MAILIMF_ERROR_PARSE:
    r = mailimf_cfws_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto err;
    }
    break;
  default:
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  bcc = mailimf_bcc_new(addr_list);
  if (bcc == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  * result = bcc;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 err:
  if (addr_list != NULL)
    mailimf_address_list_free(addr_list);
  return res;
}

/*
message-id      =       "Message-ID:" msg-id CRLF
*/

static int mailimf_message_id_parse(const char * message, size_t length,
				    size_t * index,
				    struct mailimf_message_id ** result)
{
  char * value;
  size_t cur_token;
  struct mailimf_message_id * message_id;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Message-ID");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_msg_id_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_value;
  }

  message_id = mailimf_message_id_new(value);
  if (message_id == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_value;
  }

  * result = message_id;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_value:
  mailimf_msg_id_free(value);
 err:
  return res;
}

/*
in-reply-to     =       "In-Reply-To:" 1*msg-id CRLF
*/

int mailimf_msg_id_list_parse(const char * message, size_t length,
			      size_t * index, clist ** result)
{
  return mailimf_struct_multiple_parse(message, length, index,
				       result,
				       (mailimf_struct_parser *)
				       mailimf_unstrict_msg_id_parse,
				       (mailimf_struct_destructor *)
				       mailimf_msg_id_free);
}

static int mailimf_in_reply_to_parse(const char * message, size_t length,
				     size_t * index,
				     struct mailimf_in_reply_to ** result)
{
  struct mailimf_in_reply_to * in_reply_to;
  size_t cur_token;
  clist * msg_id_list;
  int res;
  int r;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "In-Reply-To");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_msg_id_list_parse(message, length, &cur_token, &msg_id_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_list;
  }

  in_reply_to = mailimf_in_reply_to_new(msg_id_list);
  if (in_reply_to == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = in_reply_to;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(msg_id_list, (clist_func) mailimf_msg_id_free, NULL);
  clist_free(msg_id_list);
 err:
  return res;
}

/*
references      =       "References:" 1*msg-id CRLF
*/

int mailimf_references_parse(const char * message, size_t length,
			     size_t * index,
			     struct mailimf_references ** result)
{
  struct mailimf_references * references;
  size_t cur_token;
  clist * msg_id_list;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "References");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_msg_id_list_parse(message, length, &cur_token, &msg_id_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_list;
  }

  references = mailimf_references_new(msg_id_list);
  if (references == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = references;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(msg_id_list, (clist_func) mailimf_msg_id_free, NULL);
  clist_free(msg_id_list);
 err:
  return res;
}

/*
msg-id          =       [CFWS] "<" id-left "@" id-right ">" [CFWS]
*/

int mailimf_msg_id_parse(const char * message, size_t length,
			 size_t * index,
			 char ** result)
{
  size_t cur_token;
#if 0
  char * id_left;
  char * id_right;
#endif
  char * msg_id;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_lower_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_addr_spec_parse(message, length, &cur_token, &msg_id);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimf_greater_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    free(msg_id);
    res = r;
    goto err;
  }

#if 0
  r = mailimf_id_left_parse(message, length, &cur_token, &id_left);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_at_sign_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_id_left;
  }

  r = mailimf_id_right_parse(message, length, &cur_token, &id_right);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_id_left;
  }

  r = mailimf_greater_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_id_right;
  }

  msg_id = malloc(strlen(id_left) + strlen(id_right) + 2);
  if (msg_id == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_id_right;
  }
  strcpy(msg_id, id_left);
  strcat(msg_id, "@");
  strcat(msg_id, id_right);

  mailimf_id_left_free(id_left);
  mailimf_id_right_free(id_right);
#endif

  * result = msg_id;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

#if 0
 free_id_right:
  mailimf_id_right_free(id_right);
 free_id_left:
  mailimf_id_left_free(id_left);
#endif
  /*
 free:
  mailimf_atom_free(msg_id);
  */
 err:
  return res;
}

static int mailimf_parse_unwanted_msg_id(const char * message, size_t length,
					 size_t * index)
{
  size_t cur_token;
  int r;
  char * word;
  int token_parsed;

  cur_token = * index;

  token_parsed = TRUE;
  while (token_parsed) {
    token_parsed = FALSE;
    r = mailimf_word_parse(message, length, &cur_token, &word);
    if (r == MAILIMF_NO_ERROR) {
      mailimf_word_free(word);
      token_parsed = TRUE;
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else
      return r;
    r = mailimf_semi_colon_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR)
      token_parsed = TRUE;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else
      return r;
    r = mailimf_comma_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR)
      token_parsed = TRUE;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else
      return r;
    r = mailimf_plus_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR)
      token_parsed = TRUE;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else
      return r;
    r = mailimf_colon_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR)
      token_parsed = TRUE;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else
      return r;
    r = mailimf_point_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR)
      token_parsed = TRUE;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else
      return r;
    r = mailimf_at_sign_parse(message, length, &cur_token);
    if (r == MAILIMF_NO_ERROR)
      token_parsed = TRUE;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else
      return r;
  }

  return MAILIMF_NO_ERROR;
}

static int mailimf_unstrict_msg_id_parse(const char * message, size_t length,
					 size_t * index,
					 char ** result)
{
  char * msgid;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_parse_unwanted_msg_id(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_msg_id_parse(message, length, &cur_token, &msgid);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_parse_unwanted_msg_id(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    free(msgid);
    return r;
  }

  * result = msgid;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
id-left         =       dot-atom-text / no-fold-quote / obs-id-left
*/

#if 0
static int mailimf_id_left_parse(const char * message, size_t length,
				 size_t * index, char ** result)
{
  int r;

  r = mailimf_dot_atom_text_parse(message, length, index, result);
  switch (r) {
  case MAILIMF_NO_ERROR:
    return MAILIMF_NO_ERROR;
  case MAILIMF_ERROR_PARSE:
    break;
  default:
    return r;
  }
  
  r = mailimf_no_fold_quote_parse(message, length, index, result);
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}
#endif

/*
id-right        =       dot-atom-text / no-fold-literal / obs-id-right
*/

#if 0
static int mailimf_id_right_parse(const char * message, size_t length,
				  size_t * index, char ** result)
{
  int r;

  r = mailimf_dot_atom_text_parse(message, length, index, result);
  switch (r) {
  case MAILIMF_NO_ERROR:
    return MAILIMF_NO_ERROR;
  case MAILIMF_ERROR_PARSE:
    break;
  default:
    return r;
  }

  r = mailimf_no_fold_literal_parse(message, length, index, result);
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}
#endif

/*
no-fold-quote   =       DQUOTE *(qtext / quoted-pair) DQUOTE
*/

#if 0
static int mailimf_no_fold_quote_char_parse(const char * message, size_t length,
					    size_t * index, char * result)
{
  char ch;
  size_t cur_token;
  int r;

  cur_token = * index;

#if 0
  r = mailimf_qtext_parse(message, length, &cur_token, &ch);
#endif

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (is_qtext(message[cur_token])) {
    ch = message[cur_token];
    cur_token ++;
  }
  else {
    r = mailimf_quoted_pair_parse(message, length, &cur_token, &ch);
    
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  * index = cur_token;
  * result = ch;

  return MAILIMF_NO_ERROR;
}
#endif

#if 0
static int mailimf_no_fold_quote_parse(const char * message, size_t length,
				       size_t * index, char ** result)
{
  size_t cur_token;
  size_t begin;
  char ch;
  char * no_fold_quote;
  int r;
  int res;

  begin = cur_token;
  r = mailimf_dquote_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  while (1) {
    r = mailimf_no_fold_quote_char_parse(message, length, &cur_token, &ch);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE)
      break;
    else {
      res = r;
      goto err;
    }
  }

  r = mailimf_dquote_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  /*  no_fold_quote = strndup(message + begin, cur_token - begin); */
  no_fold_quote = malloc(cur_token - begin + 1);
  if (no_fold_quote == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }
  strncpy(no_fold_quote, message + begin, cur_token - begin);
  no_fold_quote[cur_token - begin] = '\0';

  * result = no_fold_quote;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 err:
  return res;
}
#endif

/*
no-fold-literal =       "[" *(dtext / quoted-pair) "]"
*/

#if 0
static inline int
mailimf_no_fold_literal_char_parse(const char * message, size_t length,
				   size_t * index, char * result)
{
  char ch;
  size_t cur_token;
  int r;

  cur_token = * index;

#if 0
  r = mailimf_dtext_parse(message, length, &cur_token, &ch);
#endif
  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (is_dtext(message[cur_token])) {
    ch = message[cur_token];
    cur_token ++;
  }
  else {
    r = mailimf_quoted_pair_parse(message, length, &cur_token, &ch);
    
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  * index = cur_token;
  * result = ch;

  return MAILIMF_NO_ERROR;
}
#endif

#if 0
static int mailimf_no_fold_literal_parse(const char * message, size_t length,
					 size_t * index, char ** result)
{
  size_t cur_token;
  size_t begin;
  char ch;
  char * no_fold_literal;
  int r;
  int res;

  begin = cur_token;
  r = mailimf_obracket_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  while (1) {
    r = mailimf_no_fold_literal_char_parse(message, length,
					   &cur_token, &ch);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE)
      break;
    else {
      res = r;
      goto err;
    }
  }

  r = mailimf_cbracket_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  /*
  no_fold_literal = strndup(message + begin, cur_token - begin);
  */
  no_fold_literal = malloc(cur_token - begin + 1);
  if (no_fold_literal == NULL) {
    res = MAILIMF_NO_ERROR;
    goto err;
  }
  strncpy(no_fold_literal, message + begin, cur_token - begin);
  no_fold_literal[cur_token - begin] = '\0';

  * result = no_fold_literal;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 err:
  return res;
}
#endif

/*
subject         =       "Subject:" unstructured CRLF
*/

static int mailimf_subject_parse(const char * message, size_t length,
				 size_t * index,
				 struct mailimf_subject ** result)
{
  struct mailimf_subject * subject;
  char * value;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Subject");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimf_unstructured_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_value;
  }
  
  subject = mailimf_subject_new(value);
  if (subject == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_value;
  }

  * result = subject;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_value:
  mailimf_unstructured_free(value);
 err:
  return res;
}

/*
comments        =       "Comments:" unstructured CRLF
*/

static int mailimf_comments_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_comments ** result)
{
  struct mailimf_comments * comments;
  char * value;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Comments");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimf_unstructured_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_value;
  }
  
  comments = mailimf_comments_new(value);
  if (comments == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_value;
  }

  * result = comments;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_value:
  mailimf_unstructured_free(value);
 err:
  return res;
}

/*
keywords        =       "Keywords:" phrase *("," phrase) CRLF
*/

static int mailimf_keywords_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_keywords ** result)
{
  struct mailimf_keywords * keywords;
  clist * list;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Keywords");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimf_struct_list_parse(message, length, &cur_token,
				&list, ',',
				(mailimf_struct_parser *)
				mailimf_phrase_parse,
				(mailimf_struct_destructor *)
				mailimf_phrase_free);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_list;
  }
  
  keywords = mailimf_keywords_new(list);
  if (keywords == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = keywords;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_phrase_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
resent-date     =       "Resent-Date:" date-time CRLF
*/

static int
mailimf_resent_date_parse(const char * message, size_t length,
			  size_t * index, struct mailimf_orig_date ** result)
{
  struct mailimf_orig_date * orig_date;
  struct mailimf_date_time * date_time;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Resent-Date");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_date_time_parse(message, length, &cur_token, &date_time);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_date_time;
  }

  orig_date = mailimf_orig_date_new(date_time);
  if (orig_date == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_date_time;
  }

  * result = orig_date;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_date_time:
  mailimf_date_time_free(date_time);
 err:
  return res;
}

/*
resent-from     =       "Resent-From:" mailbox-list CRLF
*/

static int
mailimf_resent_from_parse(const char * message, size_t length,
			  size_t * index, struct mailimf_from ** result)
{
  struct mailimf_mailbox_list * mb_list;
  struct mailimf_from * from;
  size_t cur_token;
  int r;
  int res;

  cur_token =  * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Resent-From");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_mailbox_list_parse(message, length, &cur_token, &mb_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_mb_list;
  }

  from = mailimf_from_new(mb_list);
  if (from == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_mb_list;
  }

  * result = from;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_mb_list:
  mailimf_mailbox_list_free(mb_list);
 err:
  return res;
}

/*
resent-sender   =       "Resent-Sender:" mailbox CRLF
*/

static int
mailimf_resent_sender_parse(const char * message, size_t length,
			    size_t * index, struct mailimf_sender ** result)
{
  struct mailimf_mailbox * mb;
  struct mailimf_sender * sender;
  size_t cur_token;
  int r;
  int res;

  cur_token = length;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Resent-Sender");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_mailbox_parse(message, length, &cur_token, &mb);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_mb;
  }

  sender = mailimf_sender_new(mb);
  if (sender == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_mb;
  }

  * result = sender;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_mb:
  mailimf_mailbox_free(mb);
 err:
  return res;
}

/*
resent-to       =       "Resent-To:" address-list CRLF
*/

static int
mailimf_resent_to_parse(const char * message, size_t length,
			size_t * index, struct mailimf_to ** result)
{
  struct mailimf_address_list * addr_list;
  struct mailimf_to * to;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Resent-To");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_address_list_parse(message, length, &cur_token, &addr_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_addr_list;
  }

  to = mailimf_to_new(addr_list);
  if (to == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_addr_list;
  }

  * result = to;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_addr_list:
  mailimf_address_list_free(addr_list);
 err:
  return res;
}

/*
resent-cc       =       "Resent-Cc:" address-list CRLF
*/

static int
mailimf_resent_cc_parse(const char * message, size_t length,
			size_t * index, struct mailimf_cc ** result)
{
  struct mailimf_address_list * addr_list;
  struct mailimf_cc * cc;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Resent-Cc");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_address_list_parse(message, length, &cur_token, &addr_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_addr_list;
  }

  cc = mailimf_cc_new(addr_list);
  if (cc == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_addr_list;
  }

  * result = cc;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_addr_list:
  mailimf_address_list_free(addr_list);
 err:
  return res;
}

/*
resent-bcc      =       "Resent-Bcc:" (address-list / [CFWS]) CRLF
*/

static int
mailimf_resent_bcc_parse(const char * message, size_t length,
			 size_t * index, struct mailimf_bcc ** result)
{
  struct mailimf_address_list * addr_list;
  struct mailimf_bcc * bcc;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;
  bcc = NULL;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Resent-Bcc");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_address_list_parse(message, length, &cur_token, &addr_list);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_addr_list;
  }

  bcc = mailimf_bcc_new(addr_list);
  if (bcc == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_addr_list;
  }

  * result = bcc;
  * index = cur_token;

  return TRUE;

 free_addr_list:
  mailimf_address_list_free(addr_list);
 err:
  return res;
}

/*
resent-msg-id   =       "Resent-Message-ID:" msg-id CRLF
*/

static int
mailimf_resent_msg_id_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailimf_message_id ** result)
{
  char * value;
  size_t cur_token;
  struct mailimf_message_id * message_id;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Resent-Message-ID");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_msg_id_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_value;
  }

  message_id = mailimf_message_id_new(value);
  if (message_id == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_value;
  }

  * result = message_id;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_value:
  mailimf_msg_id_free(value);
 err:
  return res;
}

/*
trace           =       [return]
                        1*received
*/

#if 0
static int mailimf_trace_parse(const char * message, size_t length,
			       size_t * index,
			       struct mailimf_trace ** result)
{
  size_t cur_token;
  struct mailimf_return * return_path;
  clist * received_list;
  struct mailimf_trace * trace;
  int r;
  int res;

  cur_token = * index;
  return_path = NULL;
  received_list = NULL;

  r = mailimf_return_parse(message, length, &cur_token, &return_path);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_struct_multiple_parse(message, length, &cur_token,
				    &received_list,
				    (mailimf_struct_parser *)
				    mailimf_received_parse,
				    (mailimf_struct_destructor *)
				    mailimf_received_free);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  if ((received_list == NULL) && (return_path == NULL)) {
    res = MAILIMF_ERROR_PARSE;
    goto free_return;
  }

  trace = mailimf_trace_new(return_path, received_list);
  if (trace == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = trace;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(received_list, (clist_func) mailimf_received_free, NULL);
  clist_free(received_list);
 free_return:
  if (return_path != NULL)
    mailimf_return_free(return_path);
 err:
  return res;
}
#endif

/*
return          =       "Return-Path:" path CRLF
*/

static int mailimf_return_parse(const char * message, size_t length,
				size_t * index,
				struct mailimf_return ** result)
{
  struct mailimf_path * path;
  struct mailimf_return * return_path;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Return-Path");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  path = NULL;
  r = mailimf_path_parse(message, length, &cur_token, &path);
  if ( r!= MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_path;
  }

  return_path = mailimf_return_new(path);
  if (return_path == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_path;
  }

  * result = return_path;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_path:
  mailimf_path_free(path);
 err:
  return res;
}

/*
path            =       ([CFWS] "<" ([CFWS] / addr-spec) ">" [CFWS]) /
                        obs-path
*/

static int mailimf_path_parse(const char * message, size_t length,
			      size_t * index, struct mailimf_path ** result)
{
  size_t cur_token;
  char * addr_spec;
  struct mailimf_path * path;
  int res;
  int r;

  cur_token = * index;
  addr_spec = NULL;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  r = mailimf_lower_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_addr_spec_parse(message, length, &cur_token, &addr_spec);
  switch (r) {
  case MAILIMF_NO_ERROR:
    break;
  case MAILIMF_ERROR_PARSE:
    r = mailimf_cfws_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto err;
    }
    break;
  default:
    return r;
  }
  
  r = mailimf_greater_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  path = mailimf_path_new(addr_spec);
  if (path == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_addr_spec;
  }

  * index = cur_token;
  * result = path;

  return MAILIMF_NO_ERROR;

 free_addr_spec:
  if (addr_spec == NULL)
    mailimf_addr_spec_free(addr_spec);
 err:
  return res;
}

/*
received        =       "Received:" name-val-list ";" date-time CRLF
*/

#if 0
static int mailimf_received_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_received ** result)
{
  size_t cur_token;
  struct mailimf_received * received;
  struct mailimf_name_val_list * name_val_list;
  struct mailimf_date_time * date_time;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "Received");
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_name_val_list_parse(message, length,
				  &cur_token, &name_val_list);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_semi_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_name_val_list;
  }

  r = mailimf_date_time_parse(message, length, &cur_token, &date_time);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_name_val_list;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_date_time;
  }

  received = mailimf_received_new(name_val_list, date_time);
  if (received == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_date_time;
  }

  * index = cur_token;
  * result = received;

  return MAILIMF_NO_ERROR;

 free_date_time:
  mailimf_date_time_free(date_time);
 free_name_val_list:
  mailimf_name_val_list_free(name_val_list);
 err:
  return res;
}
#endif

/*
name-val-list   =       [CFWS] [name-val-pair *(CFWS name-val-pair)]
*/

#if 0
static int
mailimf_name_val_list_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailimf_name_val_list ** result)
{
  size_t cur_token;
  struct mailimf_name_val_pair * pair;
  struct mailimf_name_val_list * name_val_list;
  clist* list;
  int res;
  int r;

  cur_token = * index;
  list = NULL;

  r = mailimf_name_val_pair_parse(message, length, &cur_token, &pair);

  if (r == MAILIMF_NO_ERROR){
    size_t final_token;

    list = clist_new();
    if (list == NULL) {
      mailimf_name_val_pair_free(pair);
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }

    r = clist_append(list, pair);
    if (r < 0) {
      mailimf_name_val_pair_free(pair);
      res = MAILIMF_ERROR_MEMORY;
      goto free_list;
    }

    final_token = cur_token;
    
    while (1) {
      r = mailimf_cfws_parse(message, length, &cur_token);
      if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
	res = r;
	goto free_list;
      }

      r = mailimf_name_val_pair_parse(message, length, &cur_token, &pair);
      if (r == MAILIMF_NO_ERROR) {
	/* do nothing */
      }
      else if (r == MAILIMF_ERROR_PARSE)
	break;
      else {
	res = r;
	goto free_list;
      }

      r = clist_append(list, pair);
      if (r < 0) {
	mailimf_name_val_pair_free(pair);
	res = MAILIMF_ERROR_MEMORY;
	goto free_list;
      }

      final_token = cur_token;
    }
    cur_token = final_token;
  }

  name_val_list = mailimf_name_val_list_new(list);
  if (name_val_list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * index = cur_token;
  * result = name_val_list;

  return MAILIMF_NO_ERROR;

 free_list:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimf_name_val_pair_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}
#endif

/*
name-val-pair   =       item-name CFWS item-value
*/

#if 0
static int
mailimf_name_val_pair_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailimf_name_val_pair ** result)
{
  size_t cur_token;
  char * item_name;
  struct mailimf_item_value * item_value;
  struct mailimf_name_val_pair * name_val_pair;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }
  
  r = mailimf_item_name_parse(message, length, &cur_token, &item_name);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_cfws_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_item_name;
  }

  r = mailimf_item_value_parse(message, length, &cur_token, &item_value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_item_name;
  }

  name_val_pair = mailimf_name_val_pair_new(item_name, item_value);
  if (name_val_pair == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_item_value;
  }

  * result = name_val_pair;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_item_value:
  mailimf_item_value_free(item_value);
 free_item_name:
  mailimf_item_name_free(item_name);
 err:
  return res;
}
#endif

/*
item-name       =       ALPHA *(["-"] (ALPHA / DIGIT))
*/

#if 0
static int mailimf_item_name_parse(const char * message, size_t length,
				   size_t * index, char ** result)
{
  size_t cur_token;
  size_t begin;
  char * item_name;
  char ch;
  int digit;
  int r;
  int res;

  cur_token = * index;

  begin = cur_token;

  r = mailimf_alpha_parse(message, length, &cur_token, &ch);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  while (1) {
    int minus_sign;

    minus_sign = mailimf_minus_parse(message, length, &cur_token);

    r = mailimf_alpha_parse(message, length, &cur_token, &ch);
    if (r == MAILIMF_ERROR_PARSE)
      r = mailimf_digit_parse(message, length, &cur_token, &digit);

    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    if (r == MAILIMF_ERROR_PARSE)
      break;
    else if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  item_name = strndup(message + begin, cur_token - begin);
  if (item_name == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  * index = cur_token;
  * result = item_name;

  return MAILIMF_NO_ERROR;

 err:
  return res;
}
#endif

/*
item-value      =       1*angle-addr / addr-spec /
                         atom / domain / msg-id
*/

#if 0
static int is_item_value_atext(char ch)
{
  switch (ch) {
  case '\t':
  case ' ':
  case '\r':
  case '\n':
  case ';':
    return FALSE;
  default:
    return TRUE;
  }
}

static int mailimf_item_value_atom_parse(const char * message, size_t length,
					 size_t * index, char ** result)
{
  char * atom;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_custom_string_parse(message, length, &cur_token,
				  &atom, is_item_value_atext);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  * index = cur_token;
  * result = atom;

  return MAILIMF_NO_ERROR;
}

static int mailimf_item_value_parse(const char * message, size_t length,
				    size_t * index,
				    struct mailimf_item_value ** result)
{
  size_t cur_token;
  clist * angle_addr_list;
  char * addr_spec;
  char * atom;
  char * domain;
  char * msg_id;
  int type;
  struct mailimf_item_value * item_value;
  int r;
  int res;

  cur_token = * index;
  
  angle_addr_list = NULL;
  addr_spec = NULL;
  atom = NULL;
  domain = NULL;
  msg_id = NULL;

  r = mailimf_struct_multiple_parse(message, length, &cur_token,
				    &angle_addr_list,
				    (mailimf_struct_parser *)
				    mailimf_angle_addr_parse,
				    (mailimf_struct_destructor *)
				    mailimf_angle_addr_free);
  if (r == MAILIMF_NO_ERROR)
    type = MAILIMF_ITEM_VALUE_ANGLE_ADDR_LIST;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_addr_spec_parse(message, length, &cur_token,
				&addr_spec);
    if (r == MAILIMF_NO_ERROR)
      type = MAILIMF_ITEM_VALUE_ADDR_SPEC;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_msg_id_parse(message, length, &cur_token,
			     &msg_id);
    if (r == MAILIMF_NO_ERROR)
      type = MAILIMF_ITEM_VALUE_MSG_ID;
  }

  /*
  else if (mailimf_domain_parse(message, length, &cur_token,
				&domain))
    type = MAILIMF_ITEM_VALUE_DOMAIN;
  */
  /*
  else if (mailimf_atom_parse(message, length, &cur_token,
			      &atom))
    type = MAILIMF_ITEM_VALUE_ATOM;
  */

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_item_value_atom_parse(message, length, &cur_token,
				      &atom);
    if (r == MAILIMF_NO_ERROR)
      type = MAILIMF_ITEM_VALUE_ATOM;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  item_value = mailimf_item_value_new(type, angle_addr_list, addr_spec,
				      atom, domain, msg_id);
  if (item_value == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = item_value;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (angle_addr_list != NULL) {
    clist_foreach(angle_addr_list, (clist_func) mailimf_angle_addr_free, NULL);
    clist_free(angle_addr_list);
  }
  if (addr_spec != NULL)
    mailimf_addr_spec_free(addr_spec);
  if (atom != NULL)
    mailimf_atom_free(atom);
  if (domain != NULL)
    mailimf_domain_free(domain);
  if (msg_id != NULL)
    mailimf_msg_id_free(msg_id);
 err:
  return res;
}
#endif

/*
optional-field  =       field-name ":" unstructured CRLF
*/

static int
mailimf_optional_field_parse(const char * message, size_t length,
			     size_t * index,
			     struct mailimf_optional_field ** result)
{
  char * name;
  char * value;
  struct mailimf_optional_field * optional_field;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_field_name_parse(message, length, &cur_token, &name);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_colon_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_name;
  }

  r = mailimf_unstructured_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_name;
  }

  r = mailimf_unstrict_crlf_parse(message, length, &cur_token);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_value;
  }

  optional_field = mailimf_optional_field_new(name, value);
  if (optional_field == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_value;
  }

  * result = optional_field;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_value:
  mailimf_unstructured_free(value);
 free_name:
  mailimf_field_name_free(name);
 err:
  return res;
}
     
/*
field-name      =       1*ftext
*/

static inline int is_ftext(char ch);

static int mailimf_field_name_parse(const char * message, size_t length,
				    size_t * index, char ** result)
{
  char * field_name;
  size_t cur_token;
  size_t end;
  
  cur_token = * index;

  end = cur_token;
  if (end >= length) {
    return MAILIMF_ERROR_PARSE;
  }

  while (is_ftext(message[end])) {
    end ++;
    if (end >= length)
      break;
  }
  if (end == cur_token) {
    return MAILIMF_ERROR_PARSE;
  }

  /*  field_name = strndup(message + cur_token, end - cur_token); */
  field_name = malloc(end - cur_token + 1);
  if (field_name == NULL) {
    return MAILIMF_ERROR_MEMORY;
  }
  strncpy(field_name, message + cur_token, end - cur_token);
  field_name[end - cur_token] = '\0';

  cur_token = end;
  
  * index = cur_token;
  * result = field_name;
  
  return MAILIMF_NO_ERROR;
}

/*
ftext           =       %d33-57 /               ; Any character except
                        %d59-126                ;  controls, SP, and
                                                ;  ":".
*/

static inline int is_ftext(char ch)
{
  unsigned char uch = (unsigned char) ch;

  if (uch < 33)
    return FALSE;

  if (uch == 58)
    return FALSE;

  return TRUE;
}

/*
static int mailimf_ftext_parse(const char * message, size_t length,
				    size_t * index, gchar * result)
{
  return mailimf_typed_text_parse(message, length, index, result, is_ftext);
}
*/




static int mailimf_envelope_field_parse(const char * message, size_t length,
					size_t * index,
					struct mailimf_field ** result)
{
  size_t cur_token;
  int type;
  struct mailimf_orig_date * orig_date;
  struct mailimf_from * from;
  struct mailimf_sender * sender;
  struct mailimf_reply_to * reply_to;
  struct mailimf_to * to;
  struct mailimf_cc * cc;
  struct mailimf_bcc * bcc;
  struct mailimf_message_id * message_id;
  struct mailimf_in_reply_to * in_reply_to;
  struct mailimf_references * references;
  struct mailimf_subject * subject;
  struct mailimf_optional_field * optional_field;
  struct mailimf_field * field;
  int guessed_type;
  int r;
  int res;
  
  cur_token = * index;

  orig_date = NULL;
  from = NULL;
  sender = NULL;
  reply_to = NULL;
  to = NULL;
  cc = NULL;
  bcc = NULL;
  message_id = NULL;
  in_reply_to = NULL;
  references = NULL;
  subject = NULL;
  optional_field = NULL;

  guessed_type = guess_header_type(message, length, cur_token);
  type = MAILIMF_FIELD_NONE;

  switch (guessed_type) {
  case MAILIMF_FIELD_ORIG_DATE:
    r = mailimf_orig_date_parse(message, length, &cur_token,
				&orig_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_FROM:
    r = mailimf_from_parse(message, length, &cur_token,
			   &from);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_SENDER:
    r = mailimf_sender_parse(message, length, &cur_token,
			     &sender);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_REPLY_TO:
    r = mailimf_reply_to_parse(message, length, &cur_token,
			       &reply_to);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_TO:
    r = mailimf_to_parse(message, length, &cur_token,
			 &to);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_CC:
    r = mailimf_cc_parse(message, length, &cur_token,
			 &cc);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_BCC:
    r = mailimf_bcc_parse(message, length, &cur_token,
			  &bcc);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_MESSAGE_ID:
    r = mailimf_message_id_parse(message, length, &cur_token,
				 &message_id);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_IN_REPLY_TO:
    r = mailimf_in_reply_to_parse(message, length, &cur_token,
				  &in_reply_to);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_REFERENCES:
    r = mailimf_references_parse(message, length, &cur_token,
				 &references);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  case MAILIMF_FIELD_SUBJECT:
    r = mailimf_subject_parse(message, length, &cur_token,
			      &subject);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  }

  if (type == MAILIMF_FIELD_NONE) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  field = mailimf_field_new(type, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL,
      orig_date, from, sender, reply_to, to,
      cc, bcc, message_id, in_reply_to, references,
      subject, NULL, NULL, optional_field);
  if (field == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_field;
  }
  
  * result = field;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_field:
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
  if (optional_field != NULL)
    mailimf_optional_field_free(optional_field);
 err:
  return res;
}

int mailimf_envelope_fields_parse(const char * message, size_t length,
				  size_t * index,
				  struct mailimf_fields ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_fields * fields;
  int r;
  int res;

  cur_token = * index;

  list = clist_new();
  if (list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  while (1) {
    struct mailimf_field * elt;

    r = mailimf_envelope_field_parse(message, length, &cur_token, &elt);
    if (r == MAILIMF_NO_ERROR) {
      r = clist_append(list, elt);
      if (r < 0) {
	res = MAILIMF_ERROR_MEMORY;
	goto free;
      }
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      r = mailimf_ignore_field_parse(message, length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
	/* do nothing */
      }
      else if (r == MAILIMF_ERROR_PARSE) {
	break;
      }
      else {
	res = r;
	goto free;
      }
    }
    else {
      res = r;
      goto free;
    }
  }

  fields = mailimf_fields_new(list);
  if (fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = fields;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimf_field_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}


static int
mailimf_envelope_or_optional_field_parse(const char * message,
					 size_t length,
					 size_t * index,
					 struct mailimf_field ** result)
{
  int r;
  size_t cur_token;
  struct mailimf_optional_field * optional_field;
  struct mailimf_field * field;

  r = mailimf_envelope_field_parse(message, length, index, result);
  if (r == MAILIMF_NO_ERROR)
    return MAILIMF_NO_ERROR;

  cur_token = * index;

  r = mailimf_optional_field_parse(message, length, &cur_token,
				   &optional_field);
  if (r != MAILIMF_NO_ERROR)
    return r;

  field = mailimf_field_new(MAILIMF_FIELD_OPTIONAL_FIELD, NULL,
      NULL, NULL, NULL,
      NULL, NULL, NULL,
      NULL, NULL, NULL,
      NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, optional_field);
  if (field == NULL) {
    mailimf_optional_field_free(optional_field);
    return MAILIMF_ERROR_MEMORY;
  }

  * result = field;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}


int
mailimf_envelope_and_optional_fields_parse(const char * message, size_t length,
					   size_t * index,
					   struct mailimf_fields ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_fields * fields;
  int r;
  int res;

  cur_token = * index;

  list = NULL;

  r = mailimf_struct_multiple_parse(message, length, &cur_token,
				    &list,
				    (mailimf_struct_parser *)
				    mailimf_envelope_or_optional_field_parse,
				    (mailimf_struct_destructor *)
				    mailimf_field_free);
  switch (r) {
  case MAILIMF_NO_ERROR:
    /* do nothing */
    break;

  case MAILIMF_ERROR_PARSE:
    list = clist_new();
    if (list == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }
    break;

  default:
    res = r;
    goto err;
  }

  fields = mailimf_fields_new(list);
  if (fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = fields;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimf_field_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}



static int
mailimf_only_optional_field_parse(const char * message,
				  size_t length,
				  size_t * index,
				  struct mailimf_field ** result)
{
  int r;
  size_t cur_token;
  struct mailimf_optional_field * optional_field;
  struct mailimf_field * field;

  cur_token = * index;

  r = mailimf_optional_field_parse(message, length, &cur_token,
				   &optional_field);
  if (r != MAILIMF_NO_ERROR)
    return r;

  field = mailimf_field_new(MAILIMF_FIELD_OPTIONAL_FIELD, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, optional_field);
  if (field == NULL) {
    mailimf_optional_field_free(optional_field);
    return MAILIMF_ERROR_MEMORY;
  }

  * result = field;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}


int
mailimf_optional_fields_parse(const char * message, size_t length,
			      size_t * index,
			      struct mailimf_fields ** result)
{
  size_t cur_token;
  clist * list;
  struct mailimf_fields * fields;
  int r;
  int res;

  cur_token = * index;

  list = NULL;

  r = mailimf_struct_multiple_parse(message, length, &cur_token,
				    &list,
				    (mailimf_struct_parser *)
				    mailimf_only_optional_field_parse,
				    (mailimf_struct_destructor *)
				    mailimf_field_free);
  switch (r) {
  case MAILIMF_NO_ERROR:
    /* do nothing */
    break;

  case MAILIMF_ERROR_PARSE:
    list = clist_new();
    if (list == NULL) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }
    break;

  default:
    res = r;
    goto err;
  }

  fields = mailimf_fields_new(list);
  if (fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = fields;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (list != NULL) {
    clist_foreach(list, (clist_func) mailimf_field_free, NULL);
    clist_free(list);
  }
 err:
  return res;
}
