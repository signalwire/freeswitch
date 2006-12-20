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
 * $Id: mailimf_write_generic.c,v 1.3 2006/05/22 13:39:42 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimf_write_generic.h"

#include <time.h>
#include <string.h>
#include <ctype.h>

#define MAX_MAIL_COL 72

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define MAX_VALID_IMF_LINE 998

static int mailimf_orig_date_write_driver(int (* do_write)(void *, const char *, size_t), void * data,
    int * col,
    struct mailimf_orig_date * date);
static int mailimf_date_time_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailimf_date_time * date_time);
static int mailimf_from_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			      struct mailimf_from * from);
static int mailimf_sender_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailimf_sender * sender);
static int mailimf_reply_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_reply_to * reply_to);
static int mailimf_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			    struct mailimf_to * to);
static int mailimf_cc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			    struct mailimf_cc * to);
static int mailimf_bcc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			     struct mailimf_bcc * to);
static int mailimf_message_id_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailimf_message_id * message_id);
static int mailimf_msg_id_list_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				     clist * list);
static int mailimf_in_reply_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				     struct mailimf_in_reply_to *
				     in_reply_to);
static int mailimf_references_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailimf_references * references);
static int mailimf_subject_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailimf_subject * subject);

static int mailimf_address_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailimf_address * addr);
static int mailimf_group_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			       struct mailimf_group * group);

static int mailimf_mailbox_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailimf_mailbox * mb);

static int mailimf_comments_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_comments * comments);

static int mailimf_optional_field_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
					struct mailimf_optional_field * field);

static int mailimf_keywords_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_keywords * keywords);

static int mailimf_return_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailimf_return * return_path);

static int mailimf_path_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			      struct mailimf_path * path);

static int mailimf_resent_date_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				     struct mailimf_orig_date * date);

static int mailimf_resent_from_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				     struct mailimf_from * from);

static int mailimf_resent_sender_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				       struct mailimf_sender * sender);

static int mailimf_resent_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailimf_to * to);

static int mailimf_resent_cc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailimf_cc * cc);

static int mailimf_resent_bcc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailimf_bcc * bcc);

static int
mailimf_resent_msg_id_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			    struct mailimf_message_id * message_id);



/* ************************ */

#if 0
int mailimf_string_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			 char * str, size_t length)
{
  int r;

  if (length != 0) {
    r = fwrite(str, sizeof(char), length, f);
    if (r < 0)
      return MAILIMF_ERROR_FILE;
    * col += length;
  }

  return MAILIMF_NO_ERROR;
}
#endif

#define CRLF "\r\n"
#define HEADER_FOLD "\r\n "

static inline int flush_buf(int (* do_write)(void *, const char *, size_t), void * data, const char * str, size_t length)
{
  if (length != 0) {
    int r;
    
    if (length > 0) {
      r = do_write(data, str, length);
      if (r == 0)
        return MAILIMF_ERROR_FILE;
    }
  }
  return MAILIMF_NO_ERROR;
}

#define CUT_AT_MAX_VALID_IMF_LINE

int mailimf_string_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    const char * str, size_t length)
{
  int r;
  size_t count;
  const char * block_begin;
  const char * p;
  int done;

  p = str;
  block_begin = str;
  count = 0;
  
  while (length > 0) {
#ifdef CUT_AT_MAX_VALID_IMF_LINE
    if (count >= 998) {
      /*
        cut lines at maximum valid length for internet message
        format standard (currently RFC 2822)
        
        This should not happen.
        In case there are some lines larger than 998 in body,
        the encoding must be changed into base64 or quoted-printable
        so that wrapping to 72 columns is done.
      */
      
      r = flush_buf(do_write, data, block_begin, count);
      if (r != MAILIMF_NO_ERROR)
        return r;
      
      r = do_write(data, CRLF, sizeof(CRLF) - 1);
      if (r == 0)
        return MAILIMF_ERROR_FILE;
      
      count = 0;
      block_begin = p;
      
      * col = 0;
    }
#endif
    switch (* p) {
    case '\n':
      r = flush_buf(do_write, data, block_begin, count);
      if (r != MAILIMF_NO_ERROR)
        return r;
      
      r = do_write(data, CRLF, sizeof(CRLF) - 1);
      if (r == 0)
        return MAILIMF_ERROR_FILE;
      
      p ++;
      length --;
      count = 0;
      block_begin = p;
      
      * col = 0;
      break;
      
    case '\r':
      done = 0;
      if (length >= 2) {
        if (* (p + 1) == '\n') {
          r = flush_buf(do_write, data, block_begin, count);
          if (r != MAILIMF_NO_ERROR)
            return r;
          
          r = do_write(data, CRLF, sizeof(CRLF) - 1);
          if (r == 0)
            return MAILIMF_ERROR_FILE;
          
          p += 2;
          length -= 2;
          count = 0;
          block_begin = p;
          
          * col = 0;
          
          done = 1;
        }
      }
      if (!done) {
        r = flush_buf(do_write, data, block_begin, count);
        if (r != MAILIMF_NO_ERROR)
          return r;
        
        r = do_write(data, CRLF, sizeof(CRLF) - 1);
        if (r == 0)
          return MAILIMF_ERROR_FILE;
        
        p ++;
        length --;
        count = 0;
        block_begin = p;
        
        * col = 0;
      }
      break;
      
    default:
      p ++;
      count ++;
      length --;
      break;
    }
  }
  
  r = flush_buf(do_write, data, block_begin, count);
  if (r != MAILIMF_NO_ERROR)
    return r;
  * col += count;
  
  return MAILIMF_NO_ERROR;
}

#if 0
int mailimf_header_string_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    char * str, size_t length)
{
  char * p;
  char * block_begin;
  int current_col;
  char * last_cut;
  int r;
  int first;
  
  if (* col + length < MAX_MAIL_COL)
    return mailimf_string_write_driver(do_write, data, col, str, length);
  
  first = 1;
  p = str;
  block_begin = p;
  last_cut = block_begin;
  current_col = * col;
  
  while (1) {
    if (current_col >= MAX_MAIL_COL) {
      /* if we reach the maximum recommanded size of line */
      if (last_cut == block_begin) {
        /* if we could not find any place to cut */
        if (first) {
          /* fold the header */
          r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
              sizeof(HEADER_FOLD) - 1);
          if (r != MAILIMF_NO_ERROR)
            return r;
          current_col = * col + p - block_begin;
          first = 0;
        }
        else {
          /* cut the header */
          r = mailimf_string_write_driver(do_write, data, col, block_begin, p - block_begin);
          if (r != MAILIMF_NO_ERROR)
            return r;
          r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
              sizeof(HEADER_FOLD) - 1);
          if (r != MAILIMF_NO_ERROR)
            return r;
          first = 0;
          block_begin = p;
          last_cut = block_begin;
          current_col = * col + p - block_begin;
        }
      }
      else {
        /* if we found a place to cut */
        r = mailimf_string_write_driver(do_write, data, col, block_begin, last_cut - block_begin);
        if (r != MAILIMF_NO_ERROR)
          return r;
        r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
            sizeof(HEADER_FOLD) - 1);
        if (r != MAILIMF_NO_ERROR)
          return r;
        first = 0;
        block_begin = last_cut;
        last_cut = block_begin;
        current_col = * col + p - block_begin;
        continue;
      }
    }
    else {
      if (length == 0)
        break;
      
      switch (* p) {
      case ' ':
      case '\t':
        last_cut = p;
        current_col ++;
        break;
        
      case '\r':
      case '\n':
        current_col = 0;
        break;
        
      default:
        current_col ++;
        break;
      }
      
      p ++;
      length --;
    }
  }
  
  return mailimf_string_write_driver(do_write, data, col, block_begin, p - block_begin);
}
#endif

#if 0
enum {
  STATE_LOWER_72,
  STATE_LOWER_72_CUT,
  STATE_EQUAL_72,
  STATE_LOWER_998,
  STATE_EQUAL_998,
};

int mailimf_header_string_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    const char * str, size_t length)
{
  int state;
  const char * p;
  const char * block_begin;
  size_t size;
  const char * cut;
  int r;
  
  if (* col < MAX_MAIL_COL)
    state = STATE_LOWER_72_CUT;
  else if (* col == MAX_MAIL_COL)
    state = STATE_EQUAL_72;
  else if (* col < MAX_VALID_IMF_LINE)
    state = STATE_LOWER_998;
  else
    state = STATE_EQUAL_998;
  
  p = str;
  block_begin = p;
  size = * col;
  cut = p;
  
  while (length > 0) {
    switch (state) {
    case STATE_LOWER_72:
      switch (* p) {
      case '\r':
      case '\n':
        p ++;
        length --;
        size = 0;
        break;
      
      case ' ':
      case '\t':
        cut = p;
        p ++;
        length --;
        size ++;
        state = STATE_LOWER_72_CUT;
        break;
      
      default:
        if (size < MAX_MAIL_COL - 1) {
          p ++;
          length --;
          size ++;
        }
        else {
          state = STATE_EQUAL_72;
          p ++;
          length --;
          size ++;
        }
        break;
      }
      break; /* end of STATE_LOWER_72 */
    
    case STATE_LOWER_72_CUT:
      switch (* p) {
      case '\r':
      case '\n':
        p ++;
        length --;
        size = 0;
        state = STATE_LOWER_72;
        break;
      
      case ' ':
      case '\t':
        cut = p;
        p ++;
        length --;
        size ++;
        break;
      
      default:
        if (size < MAX_MAIL_COL) {
          p ++;
          length --;
          size ++;
        }
        else {
          r = mailimf_string_write_driver(do_write, data, col, block_begin, cut - block_begin);
          if (r != MAILIMF_NO_ERROR)
            return r;
          r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
              sizeof(HEADER_FOLD) - 1);
          if (r != MAILIMF_NO_ERROR)
            return r;
          p ++;
          length --;
          block_begin = cut;
          if ((* block_begin == ' ') || (* block_begin == '\t'))
            block_begin ++;
          size = p - block_begin + * col;
          state = STATE_LOWER_72;
        }
        break;
      }
      break; /* end of STATE_LOWER_72_CUT */

    case STATE_EQUAL_72:
      switch (* p) {
      case '\r':
      case '\n':
        p ++;
        length --;
        size = 0;
        state = STATE_LOWER_72;
        break;
      
      case ' ':
      case '\t':
        r = mailimf_string_write_driver(do_write, data, col, block_begin, p - block_begin);
        if (r != MAILIMF_NO_ERROR)
          return r;
        r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
            sizeof(HEADER_FOLD) - 1);
        if (r != MAILIMF_NO_ERROR)
          return r;
        p ++;
        length --;
        block_begin = p;
        size = p - block_begin + * col;
        state = STATE_LOWER_72;
        break;
      
      default:
        p ++;
        length --;
        size ++;
        state = STATE_LOWER_998;
        break;
      }
      break; /* end of STATE_EQUAL_72 */

    case STATE_LOWER_998:
      switch (* p) {
      case '\r':
      case '\n':
        p ++;
        length --;
        size = 0;
        state = STATE_LOWER_72;
        break;
      
      case ' ':
      case '\t':
        r = mailimf_string_write_driver(do_write, data, col, block_begin, p - block_begin);
        if (r != MAILIMF_NO_ERROR)
          return r;
        r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
            sizeof(HEADER_FOLD) - 1);
        if (r != MAILIMF_NO_ERROR)
          return r;
        p ++;
        length --;
        block_begin = p;
        size = p - block_begin + * col;
        state = STATE_LOWER_72;
        break;
      
      default:
        if (size < MAX_VALID_IMF_LINE - 1) {
          p ++;
          length --;
          size ++;
        }
        else {
          p ++;
          length --;
          size = 0;
          state = STATE_EQUAL_998;
        }
        break;
      }
      break; /* end of STATE_LOWER_998 */

    case STATE_EQUAL_998:
      switch (* p) {
      case '\r':
      case '\n':
        p ++;
        length --;
        size = 0;
        state = STATE_LOWER_72;
        break;
      
      case ' ':
      case '\t':
        r = mailimf_string_write_driver(do_write, data, col, block_begin, p - block_begin);
        if (r != MAILIMF_NO_ERROR)
          return r;
        r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
            sizeof(HEADER_FOLD) - 1);
        if (r != MAILIMF_NO_ERROR)
          return r;
        p ++;
        length --;
        block_begin = p;
        size = p - block_begin + * col;
        state = STATE_LOWER_72;
        break;
      
      default:
#ifdef CUT_AT_MAX_VALID_IMF_LINE
        r = mailimf_string_write_driver(do_write, data, col, block_begin, p - block_begin);
        if (r != MAILIMF_NO_ERROR)
          return r;
        r = mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
            sizeof(HEADER_FOLD) - 1);
        if (r != MAILIMF_NO_ERROR)
          return r;
        p ++;
        length --;
        block_begin = p;
        size = p - block_begin + * col;
        state = STATE_LOWER_72;
#else
        p ++;
        length --;
        size ++;
#endif
        break;
      }
      break; /* end of STATE_EQUAL_998 */
    }
  }
  
  r = mailimf_string_write_driver(do_write, data, col, block_begin, p - block_begin);
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  return MAILIMF_NO_ERROR;
}
#endif

enum {
  STATE_BEGIN,
  STATE_WORD,
  STATE_SPACE
};

int mailimf_header_string_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    const char * str, size_t length)
{
  int state;
  const char * p;
  const char * word_begin;
  const char * word_end;
  const char * next_word;
  int first;
  
  state = STATE_BEGIN;
  
  p = str;
  word_begin = p;
  word_end = p;
  next_word = p;
  first = 1;
  
  while (length > 0) {
    switch (state) {
    case STATE_BEGIN:
      switch (* p) {
      case '\r':
      case '\n':
      case ' ':
      case '\t':
        p ++;
        length --;
        break;
      
      default:
        word_begin = p;
        state = STATE_WORD;
        break;
      }
      break;
      
    case STATE_SPACE:
      switch (* p) {
      case '\r':
      case '\n':
      case ' ':
      case '\t':
        p ++;
        length --;
        break;
      
      default:
        word_begin = p;
        state = STATE_WORD;
        break;
      }
      break;

    case STATE_WORD:
      switch (* p) {
      case '\r':
      case '\n':
      case ' ':
      case '\t':
        if (p - word_begin + (* col) + 1 > MAX_MAIL_COL)
          mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
              sizeof(HEADER_FOLD) - 1);
        else {
          if (!first)
            mailimf_string_write_driver(do_write, data, col, " ", 1);
        }
        first = 0;
        mailimf_string_write_driver(do_write, data, col, word_begin, p - word_begin);
        state = STATE_SPACE;
        break;
        
      default:
        if (p - word_begin + (* col) >= MAX_VALID_IMF_LINE) {
          mailimf_string_write_driver(do_write, data, col, word_begin, p - word_begin);
          mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
              sizeof(HEADER_FOLD) - 1);
          word_begin = p;
        }
        p ++;
        length --;
        break;
      }
      break;
    }
  }
  
  if (state == STATE_WORD) {
    if (p - word_begin + (* col) >= MAX_MAIL_COL)
      mailimf_string_write_driver(do_write, data, col, HEADER_FOLD,
          sizeof(HEADER_FOLD) - 1);
    else {
      if (!first)
        mailimf_string_write_driver(do_write, data, col, " ", 1);
    }
    first = 0;
    mailimf_string_write_driver(do_write, data, col, word_begin, p - word_begin);
  }
  
  return MAILIMF_NO_ERROR;
}

int mailimf_envelope_fields_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_fields * fields)
{
  clistiter * cur;

  for(cur = clist_begin(fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    int r;
    struct mailimf_field * field;
    
    field = clist_content(cur);
    if (field->fld_type != MAILIMF_FIELD_OPTIONAL_FIELD) {
      r = mailimf_field_write_driver(do_write, data, col, field);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
  }

  return MAILIMF_NO_ERROR;
}

int mailimf_fields_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			 struct mailimf_fields * fields)
{
  clistiter * cur;

  for(cur = clist_begin(fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    int r;
    
    r = mailimf_field_write_driver(do_write, data, col, clist_content(cur));
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}

#if 0
int mailimf_unparsed_fields_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_unparsed_fields * fields)
{
  clistiter * cur;

  for(cur = clist_begin(fields->list) ; cur != NULL ; cur = cur->next) {
    int r;
    
    r = mailimf_optional_field_write_driver(do_write, data, col, cur->data);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}
#endif

int mailimf_field_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
                        struct mailimf_field * field)
{
  int r;
  
  switch (field->fld_type) {
  case MAILIMF_FIELD_RETURN_PATH:
    r = mailimf_return_write_driver(do_write, data, col, field->fld_data.fld_return_path);
    break;
  case MAILIMF_FIELD_RESENT_DATE:
    r = mailimf_resent_date_write_driver(do_write, data, col, field->fld_data.fld_resent_date);
    break;
  case MAILIMF_FIELD_RESENT_FROM:
    r = mailimf_resent_from_write_driver(do_write, data, col, field->fld_data.fld_resent_from);
    break;
  case MAILIMF_FIELD_RESENT_SENDER:
    r = mailimf_resent_sender_write_driver(do_write, data, col, field->fld_data.fld_resent_sender);
    break;
  case MAILIMF_FIELD_RESENT_TO:
    r = mailimf_resent_to_write_driver(do_write, data, col, field->fld_data.fld_resent_to);
    break;
  case MAILIMF_FIELD_RESENT_CC:
    r = mailimf_resent_cc_write_driver(do_write, data, col, field->fld_data.fld_resent_cc);
    break;
  case MAILIMF_FIELD_RESENT_BCC:
    r = mailimf_resent_bcc_write_driver(do_write, data, col, field->fld_data.fld_resent_bcc);
    break;
  case MAILIMF_FIELD_RESENT_MSG_ID:
    r = mailimf_resent_msg_id_write_driver(do_write, data, col, field->fld_data.fld_resent_msg_id);
    break;
  case MAILIMF_FIELD_ORIG_DATE:
    r = mailimf_orig_date_write_driver(do_write, data, col, field->fld_data.fld_orig_date);
    break;
  case MAILIMF_FIELD_FROM:
    r = mailimf_from_write_driver(do_write, data, col, field->fld_data.fld_from);
    break;
  case MAILIMF_FIELD_SENDER:
    r = mailimf_sender_write_driver(do_write, data, col, field->fld_data.fld_sender);
    break;
  case MAILIMF_FIELD_REPLY_TO:
    r = mailimf_reply_to_write_driver(do_write, data, col, field->fld_data.fld_reply_to);
    break;
  case MAILIMF_FIELD_TO:
    r = mailimf_to_write_driver(do_write, data, col, field->fld_data.fld_to);
    break;
  case MAILIMF_FIELD_CC:
    r = mailimf_cc_write_driver(do_write, data, col, field->fld_data.fld_cc);
    break;
  case MAILIMF_FIELD_BCC:
    r = mailimf_bcc_write_driver(do_write, data, col, field->fld_data.fld_bcc);
    break;
  case MAILIMF_FIELD_MESSAGE_ID:
    r = mailimf_message_id_write_driver(do_write, data, col, field->fld_data.fld_message_id);
    break;
  case MAILIMF_FIELD_IN_REPLY_TO:
    r = mailimf_in_reply_to_write_driver(do_write, data, col, field->fld_data.fld_in_reply_to);
    break;
  case MAILIMF_FIELD_REFERENCES:
    r = mailimf_references_write_driver(do_write, data, col, field->fld_data.fld_references);
    break;
  case MAILIMF_FIELD_SUBJECT:
    r = mailimf_subject_write_driver(do_write, data, col, field->fld_data.fld_subject);
    break;
  case MAILIMF_FIELD_COMMENTS:
    r = mailimf_comments_write_driver(do_write, data, col, field->fld_data.fld_comments);
    break;
  case MAILIMF_FIELD_KEYWORDS:
    r = mailimf_keywords_write_driver(do_write, data, col, field->fld_data.fld_keywords);
    break;
  case MAILIMF_FIELD_OPTIONAL_FIELD:
    r = mailimf_optional_field_write_driver(do_write, data, col, field->fld_data.fld_optional_field);
    break;
  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }

  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}


static int mailimf_orig_date_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailimf_orig_date * date)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Date: ", 6);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_date_time_write_driver(do_write, data, col, date->dt_date_time);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

#define MAX_DATE_STR 256

/* 0 = Sunday */
/* y > 1752 */

static int dayofweek(int year, int month, int day)
{
  static int offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

  year -= month < 3;

  return (year + year/4 - year/100 + year/400 + offset[month-1] + day) % 7;
}

static const char * week_of_day_str[] = { "Sun", "Mon", "Tue", "Wed", "Thu",
                                          "Fri", "Sat"};
static const char * month_str[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static int mailimf_date_time_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailimf_date_time * date_time)
{
  int r;
  char date_str[MAX_DATE_STR];
#if 0
  struct tm tmval;
  time_t timeval;
#endif
  int wday;
  
#if 0
  tmval.tm_sec  = date_time->sec;
  tmval.tm_min  = date_time->min;
  tmval.tm_hour  = date_time->hour;
  tmval.tm_sec  = date_time->sec;
  tmval.tm_mday = date_time->day;
  tmval.tm_mon = date_time->month - 1;
  tmval.tm_year = date_time->year - 1900;
  tmval.tm_isdst = 1;

  timeval = mktime(&tmval);
  
  localtime_r(&timeval, &tmval);
#endif

  wday = dayofweek(date_time->dt_year, date_time->dt_month, date_time->dt_day);

  snprintf(date_str, MAX_DATE_STR, "%s, %i %s %i %02i:%02i:%02i %+05i",
      week_of_day_str[wday], date_time->dt_day,
      month_str[date_time->dt_month - 1],
      date_time->dt_year, date_time->dt_hour,
      date_time->dt_min, date_time->dt_sec,
      date_time->dt_zone);

  r = mailimf_string_write_driver(do_write, data, col, date_str, strlen(date_str));

  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}

static int mailimf_from_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			      struct mailimf_from * from)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "From: ", 6);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_mailbox_list_write_driver(do_write, data, col, from->frm_mb_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_sender_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailimf_sender * sender)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Sender: ", 8);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_mailbox_write_driver(do_write, data, col, sender->snd_mb);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_reply_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_reply_to * reply_to)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Reply-To: ", 10);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_address_list_write_driver(do_write, data, col, reply_to->rt_addr_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			    struct mailimf_to * to)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "To: ", 4);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_address_list_write_driver(do_write, data, col, to->to_addr_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_cc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			    struct mailimf_cc * cc)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Cc: ", 4);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_address_list_write_driver(do_write, data, col, cc->cc_addr_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_bcc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			     struct mailimf_bcc * bcc)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Bcc: ", 5);
  if (r != MAILIMF_NO_ERROR)
    return r;

  if (bcc->bcc_addr_list != NULL) {
    r =  mailimf_address_list_write_driver(do_write, data, col, bcc->bcc_addr_list);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_message_id_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailimf_message_id * message_id)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Message-ID: ", 12);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "<", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col,
      message_id->mid_value,
      strlen(message_id->mid_value));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, ">", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_msg_id_list_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, clist * mid_list)
{
  clistiter * cur;
  int r;
  int first;

  first = TRUE;

  for(cur = clist_begin(mid_list) ; cur != NULL ; cur = clist_next(cur)) {
    char * msgid;
    size_t len;

    msgid = clist_content(cur);
    len = strlen(msgid);
    
    /*
      XXX - if this is the first message ID, don't fold.
      This is a workaround for a bug of old versions of INN.
    */
    if (!first) {
      if (* col > 1) {
        
        if (* col + len >= MAX_MAIL_COL) {
          r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
          if (r != MAILIMF_NO_ERROR)
            return r;
#if 0
          * col = 1;
#endif
          first = TRUE;
        }
      }
    }
    
    if (!first) {
      r = mailimf_string_write_driver(do_write, data, col, " ", 1);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
    else {
      first = FALSE;
    }

    r = mailimf_string_write_driver(do_write, data, col, "<", 1);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_string_write_driver(do_write, data, col, msgid, len);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_string_write_driver(do_write, data, col, ">", 1);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}


static int mailimf_in_reply_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				     struct mailimf_in_reply_to * in_reply_to)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "In-Reply-To: ", 13);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_msg_id_list_write_driver(do_write, data, col, in_reply_to->mid_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_references_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailimf_references * references)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "References: ", 12);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_msg_id_list_write_driver(do_write, data, col, references->mid_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}



static int mailimf_subject_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailimf_subject * subject)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Subject: ", 9);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_header_string_write_driver(do_write, data, col,
      subject->sbj_value, strlen(subject->sbj_value));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

int mailimf_address_list_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    struct mailimf_address_list * addr_list)
{
  clistiter * cur;
  int r;
  int first;

  first = TRUE;

  for(cur = clist_begin(addr_list->ad_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_address * addr;

    addr = clist_content(cur);

    if (!first) {
      r = mailimf_string_write_driver(do_write, data, col, ", ", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
    else {
      first = FALSE;
    }

    r = mailimf_address_write_driver(do_write, data, col, addr);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}


static int mailimf_address_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailimf_address * addr)
{
  int r;

  switch(addr->ad_type) {
  case MAILIMF_ADDRESS_MAILBOX:
    r = mailimf_mailbox_write_driver(do_write, data, col, addr->ad_data.ad_mailbox);
    if (r != MAILIMF_NO_ERROR)
      return r;

    break;

  case MAILIMF_ADDRESS_GROUP:
    r = mailimf_group_write_driver(do_write, data, col, addr->ad_data.ad_group);
    if (r != MAILIMF_NO_ERROR)
      return r;
    
    break;
  }

  return MAILIMF_NO_ERROR;
}


static int mailimf_group_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			       struct mailimf_group * group)
{
  int r;

  r = mailimf_header_string_write_driver(do_write, data, col, group->grp_display_name,
      strlen(group->grp_display_name));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, ": ", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  if (group->grp_mb_list != NULL) {
    r = mailimf_mailbox_list_write_driver(do_write, data, col, group->grp_mb_list);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  r = mailimf_string_write_driver(do_write, data, col, ";", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}


int mailimf_mailbox_list_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    struct mailimf_mailbox_list * mb_list)
{
  clistiter * cur;
  int r;
  int first;

  first = TRUE;

  for(cur = clist_begin(mb_list->mb_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_mailbox * mb;

    mb = clist_content(cur);

    if (!first) {
      r = mailimf_string_write_driver(do_write, data, col, ", ", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
    else {
      first = FALSE;
    }

    r = mailimf_mailbox_write_driver(do_write, data, col, mb);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}


int mailimf_quoted_string_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    const char * string, size_t len)
{
  int r;
  size_t i;

  r = do_write(data, "\"", 1);
  if (r == 0)
    return MAILIMF_ERROR_FILE;
  for(i = 0 ; i < len ; i ++) {
    switch (string[i]) {
    case '\\':
    case '\"':
      r = do_write(data, "\\", 1);
      if (r == 0)
	return MAILIMF_ERROR_FILE;
      r = do_write(data, &string[i], 1);
      if (r == 0)
	return MAILIMF_ERROR_FILE;
      (* col) += 2;
      break;

    default:
      r = do_write(data, &string[i], 1);
      if (r == 0)
	return MAILIMF_ERROR_FILE;
      (* col) ++;
      break;
    }
  }
  r = do_write(data, "\"", 1);
  if (r == 0)
    return MAILIMF_ERROR_FILE;
  
  return MAILIMF_NO_ERROR;
}


/*
static int 
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

static int is_atext(const char * s)
{
  const char * p;

  for(p = s ; * p != 0 ; p ++) {
    if (isalpha((unsigned char) * p))
      continue;
    if (isdigit((unsigned char) * p))
      continue;
    switch (*p) {
    case ' ':
    case '\t':
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '/':
    case '=':
    case '?':
    case '^':
    case '_':
    case '`':
    case '{':
    case '|':
    case '}':
    case '~':
      break;
    default:
      return 0;
    }
  }
  
  return 1;
}

static int mailimf_mailbox_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailimf_mailbox * mb)
{
  int r;
  int do_fold;
  
#if 0
  if (* col > 1) {
    
    if (mb->mb_display_name != NULL) {
      if (* col + strlen(mb->mb_display_name) >= MAX_MAIL_COL) {
        r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
        if (r != MAILIMF_NO_ERROR)
          return r;
#if 0
        * col = 1;
#endif
      }
    }
  }
#endif
  
  if (mb->mb_display_name) {

    if (is_atext(mb->mb_display_name)) {
      r = mailimf_header_string_write_driver(do_write, data, col, mb->mb_display_name,
          strlen(mb->mb_display_name));
      if (r != MAILIMF_NO_ERROR)
        return r;
    }
    else {
      if (mb->mb_display_name != NULL) {
        if (* col + strlen(mb->mb_display_name) >= MAX_MAIL_COL) {
          r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
          if (r != MAILIMF_NO_ERROR)
            return r;
        }
      }
      
      if (strlen(mb->mb_display_name) > MAX_VALID_IMF_LINE / 2)
        return MAILIMF_ERROR_INVAL;
      
      r = mailimf_quoted_string_write_driver(do_write, data, col, mb->mb_display_name,
          strlen(mb->mb_display_name));
      if (r != MAILIMF_NO_ERROR)
        return r;
    }
    
    do_fold = 0;
    if (* col > 1) {
      
      if (* col + strlen(mb->mb_addr_spec) + 3 >= MAX_MAIL_COL) {
	r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
	if (r != MAILIMF_NO_ERROR)
	  return r;
#if 0
	* col = 1;
#endif
        do_fold = 1;
      }
    }
    
    if (do_fold)
      r = mailimf_string_write_driver(do_write, data, col, "<", 1);
    else
      r = mailimf_string_write_driver(do_write, data, col, " <", 2);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_string_write_driver(do_write, data, col, mb->mb_addr_spec,
        strlen(mb->mb_addr_spec));
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_string_write_driver(do_write, data, col, ">", 1);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }
  else {
    if (* col + strlen(mb->mb_addr_spec) >= MAX_MAIL_COL) {
      r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
      if (r != MAILIMF_NO_ERROR)
        return r;
    }
    
    r = mailimf_string_write_driver(do_write, data, col,
        mb->mb_addr_spec, strlen(mb->mb_addr_spec));
    if (r != MAILIMF_NO_ERROR)
      return r;
  }


  return MAILIMF_NO_ERROR;
}

static int mailimf_comments_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_comments * comments)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Comments: ", 10);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_header_string_write_driver(do_write, data, col,
      comments->cm_value, strlen(comments->cm_value));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_optional_field_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
					struct mailimf_optional_field * field)
{
  int r;

  if (strlen(field->fld_name) + 2 > MAX_VALID_IMF_LINE)
    return MAILIMF_ERROR_INVAL;
  
  r = mailimf_string_write_driver(do_write, data, col, field->fld_name, strlen(field->fld_name));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, ": ", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_header_string_write_driver(do_write, data, col, field->fld_value,
      strlen(field->fld_value));
  if (r != MAILIMF_NO_ERROR)
    return r;

#if 0
  /* XXX parsing debug */
  mailimf_string_write_driver(do_write, data, col, " (X)", 4);
#endif

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_keywords_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_keywords * keywords)
{
  int r;
  clistiter * cur;
  int first;
  
  r = mailimf_string_write_driver(do_write, data, col, "Keywords: ", 10);
  if (r != MAILIMF_NO_ERROR)
    return r;

  first = TRUE;

  for(cur = clist_begin(keywords->kw_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    char * keyword;
    size_t len;

    keyword = clist_content(cur);
    len = strlen(keyword);

    if (!first) {
      r = mailimf_string_write_driver(do_write, data, col, ", ", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
    else {
      first = FALSE;
    }

#if 0
    if (* col > 1) {
      
      if (* col + len >= MAX_MAIL_COL) {
	r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
	if (r != MAILIMF_NO_ERROR)
	  return r;
#if 0
	* col = 1;
#endif
      }
    }
#endif

    r = mailimf_header_string_write_driver(do_write, data, col, keyword, len);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

#if 0
static int mailimf_delivering_info_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
					 struct mailimf_delivering_info * info)
{
  clistiter * cur;
  int r;

  for(cur = clist_begin(info->received_fields) ;
      cur != NULL ; cur = cur->next) {
    struct mailimf_trace_resent_fields * field;

    field = cur->data;

    r = mailimf_trace_resent_fields_write_driver(do_write, data, col, field);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}


static int
mailimf_trace_resent_fields_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailimf_trace_resent_fields * field)
{
  int r;

  if (field->return_path != NULL) {
    r = mailimf_return_write_driver(do_write, data, col, field->return_path);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  if (field->resent_fields != NULL) {
    r = mailimf_resent_fields_write_driver(do_write, data, col, field->resent_fields);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}
#endif

static int mailimf_return_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailimf_return * return_path)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Return-Path: ", 13);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_path_write_driver(do_write, data, col, return_path->ret_path);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_path_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			      struct mailimf_path * path)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "<", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, path->pt_addr_spec,
      strlen(path->pt_addr_spec));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, ">", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}

#if 0
static int mailimf_resent_fields_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				       struct mailimf_resent_fields_list *
				       resent_fields)
{
  clistiter * cur;
  int r;

  for(cur = clist_begin(resent_fields->list) ; cur != NULL ; cur = cur->next) {
    struct mailimf_resent_field * field;

    field = cur->data;

    r = mailimf_resent_field_write_driver(do_write, data, col, field);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  return MAILIMF_NO_ERROR;
}



static int mailimf_resent_field_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				      struct mailimf_resent_field *
				      resent_field)
{
  int r;

  switch (resent_field->type) {
  case MAILIMF_RESENT_FIELD_DATE:
    r = mailimf_resent_date_write_driver(do_write, data, col, resent_field->resent_date);
    break;

  case MAILIMF_RESENT_FIELD_FROM:
    r = mailimf_resent_from_write_driver(do_write, data, col, resent_field->resent_from);
    break;

  case MAILIMF_RESENT_FIELD_SENDER:
    r = mailimf_resent_sender_write_driver(do_write, data, col, resent_field->resent_sender);
    break;

  case MAILIMF_RESENT_FIELD_TO:
    r = mailimf_resent_to_write_driver(do_write, data, col, resent_field->resent_to);
    break;

  case MAILIMF_RESENT_FIELD_CC:
    r = mailimf_resent_cc_write_driver(do_write, data, col, resent_field->resent_cc);
    break;

  case MAILIMF_RESENT_FIELD_BCC:
    r = mailimf_resent_bcc_write_driver(do_write, data, col, resent_field->resent_bcc);
    break;

  case MAILIMF_RESENT_FIELD_MSG_ID:
    r = mailimf_resent_msg_id_write_driver(do_write, data, col, resent_field->resent_msg_id);
    break;
  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }


  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}
#endif

static int mailimf_resent_date_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				     struct mailimf_orig_date * date)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Resent-Date: ", 13);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_date_time_write_driver(do_write, data, col, date->dt_date_time);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_resent_from_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				     struct mailimf_from * from)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Resent-From: ", 13);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_mailbox_list_write_driver(do_write, data, col, from->frm_mb_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_resent_sender_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				       struct mailimf_sender * sender)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Resent-Sender: ", 15);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_mailbox_write_driver(do_write, data, col, sender->snd_mb);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailimf_resent_to_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailimf_to * to)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Resent-To: ", 11);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_address_list_write_driver(do_write, data, col, to->to_addr_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_resent_cc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailimf_cc * cc)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Resent-Cc: ", 11);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_address_list_write_driver(do_write, data, col, cc->cc_addr_list);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int mailimf_resent_bcc_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailimf_bcc * bcc)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Resent-Bcc: ", 12);
  if (r != MAILIMF_NO_ERROR)
    return r;

  if (bcc->bcc_addr_list != NULL) {
    r =  mailimf_address_list_write_driver(do_write, data, col, bcc->bcc_addr_list);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}


static int
mailimf_resent_msg_id_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			    struct mailimf_message_id * message_id)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Resent-Message-ID: ", 19);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "<", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col,
      message_id->mid_value, strlen(message_id->mid_value));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, ">", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}
