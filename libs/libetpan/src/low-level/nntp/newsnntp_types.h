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
 * $Id: newsnntp_types.h,v 1.14 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef NEWSNNTP_TYPES_H

#define NEWSNNTP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>

#include <libetpan/mailstream.h>
#include <libetpan/mmapstring.h>

enum {
  NEWSNNTP_NO_ERROR = 0,
  NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME,
  NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD,
  NEWSNNTP_ERROR_STREAM,
  NEWSNNTP_ERROR_UNEXPECTED,
  NEWSNNTP_ERROR_NO_NEWSGROUP_SELECTED,
  NEWSNNTP_ERROR_NO_ARTICLE_SELECTED,
  NEWSNNTP_ERROR_INVALID_ARTICLE_NUMBER,
  NEWSNNTP_ERROR_ARTICLE_NOT_FOUND,
  NEWSNNTP_ERROR_UNEXPECTED_RESPONSE,
  NEWSNNTP_ERROR_INVALID_RESPONSE,
  NEWSNNTP_ERROR_NO_SUCH_NEWS_GROUP,
  NEWSNNTP_ERROR_POSTING_NOT_ALLOWED,
  NEWSNNTP_ERROR_POSTING_FAILED,
  NEWSNNTP_ERROR_PROGRAM_ERROR,
  NEWSNNTP_ERROR_NO_PERMISSION,
  NEWSNNTP_ERROR_COMMAND_NOT_UNDERSTOOD,
  NEWSNNTP_ERROR_COMMAND_NOT_SUPPORTED,
  NEWSNNTP_ERROR_CONNECTION_REFUSED,
  NEWSNNTP_ERROR_MEMORY,
  NEWSNNTP_ERROR_AUTHENTICATION_REJECTED,
  NEWSNNTP_ERROR_BAD_STATE
};

struct newsnntp
{
  mailstream * nntp_stream;

  int nntp_readonly;

  uint32_t nntp_progr_rate;
  progress_function * nntp_progr_fun;
  
  MMAPString * nntp_stream_buffer;
  MMAPString * nntp_response_buffer;

  char * nntp_response;
};

typedef struct newsnntp newsnntp;

struct newsnntp_group_info
{
  char * grp_name;
  uint32_t grp_first;
  uint32_t grp_last;
  uint32_t grp_count;
  char grp_type;
};

struct newsnntp_group_time {
  char * grp_name;
  uint32_t grp_date;
  char * grp_email;
};

struct newsnntp_distrib_value_meaning {
  char * dst_value;
  char * dst_meaning;
};

struct newsnntp_distrib_default_value {
  uint32_t dst_weight;
  char * dst_group_pattern;
  char * dst_value;
};

struct newsnntp_group_description {
  char * grp_name;
  char * grp_description;
};

struct newsnntp_xhdr_resp_item {
  uint32_t hdr_article;
  char * hdr_value;
};

struct newsnntp_xover_resp_item {
  uint32_t ovr_article;
  char * ovr_subject;
  char * ovr_author;
  char * ovr_date;
  char * ovr_message_id;
  char * ovr_references;
  size_t ovr_size;
  uint32_t ovr_line_count;
  clist * ovr_others;
};

#ifdef __cplusplus
}
#endif

#endif
