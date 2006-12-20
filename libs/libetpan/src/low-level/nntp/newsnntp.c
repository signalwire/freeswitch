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
 * $Id: newsnntp.c,v 1.24 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "newsnntp.h"


#include <stdio.h>
#ifndef _MSC_VER
#	include <unistd.h>
#	include <netinet/in.h>
#	include <netdb.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "connect.h"
#include "mail.h"
#include "clist.h"

/*
  NNTP Protocol

  RFC 977
  RFC 2980

  TODO :

   XPAT header range|<message-id> pat [pat...]


 */




#define NNTP_STRING_SIZE 513



static char * read_line(newsnntp * f);
static char * read_multiline(newsnntp * f, size_t size,
			      MMAPString * multiline_buffer);
static int parse_response(newsnntp * f, char * response);

static int send_command(newsnntp * f, char * command);

newsnntp * newsnntp_new(size_t progr_rate, progress_function * progr_fun)
{
  newsnntp * f;

  f = malloc(sizeof(* f));
  if (f == NULL)
    goto err;
  
  f->nntp_stream = NULL;
  f->nntp_readonly = FALSE;

  f->nntp_progr_rate = progr_rate;
  f->nntp_progr_fun = progr_fun;

  f->nntp_stream_buffer = mmap_string_new("");
  if (f->nntp_stream_buffer == NULL)
    goto free_f;

  f->nntp_response_buffer = mmap_string_new("");
  if (f->nntp_response_buffer == NULL)
    goto free_stream_buffer;

  return f;

 free_stream_buffer:
    mmap_string_free(f->nntp_stream_buffer);
 free_f:
    free(f);
 err:
    return NULL;
}

void newsnntp_free(newsnntp * f)
{
  if (f->nntp_stream)
    newsnntp_quit(f);

  mmap_string_free(f->nntp_response_buffer);
  mmap_string_free(f->nntp_stream_buffer);

  free(f);
}
















int newsnntp_quit(newsnntp * f)
{
  char command[NNTP_STRING_SIZE];
  char * response;
  int r;
  int res;

  if (f->nntp_stream == NULL)
    return NEWSNNTP_ERROR_BAD_STATE;

  snprintf(command, NNTP_STRING_SIZE, "QUIT\r\n");
  r = send_command(f, command);
  if (r == -1) {
    res = NEWSNNTP_ERROR_STREAM;
    goto close;
  }
  
  response = read_line(f);
  if (response == NULL) {
    res = NEWSNNTP_ERROR_STREAM;
    goto close;
  }

  parse_response(f, response);

  res = NEWSNNTP_NO_ERROR;

 close:

  mailstream_close(f->nntp_stream);

  f->nntp_stream = NULL;
  
  return res;
}

int newsnntp_connect(newsnntp * f, mailstream * s)
{
  char * response;
  int r;

  if (f->nntp_stream != NULL)
    return NEWSNNTP_ERROR_BAD_STATE;

  f->nntp_stream = s;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 200:
      f->nntp_readonly = FALSE;
      return NEWSNNTP_NO_ERROR;

  case 201:
      f->nntp_readonly = TRUE;
      return NEWSNNTP_NO_ERROR;

  default:
      f->nntp_stream = NULL;
      return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}




















/*
static struct newsnntp_xover_resp_item * get_xover_info(newsnntp * f,
							guint32 article);
*/

static void newsnntp_multiline_response_free(char * str)
{
  mmap_string_unref(str);
}

void newsnntp_head_free(char * str)
{
  newsnntp_multiline_response_free(str);
}

void newsnntp_article_free(char * str)
{
  newsnntp_multiline_response_free(str);
}

void newsnntp_body_free(char * str)
{
  newsnntp_multiline_response_free(str);
}

/* ******************** HEADER ******************************** */

/*
  message content in (* result) is still there until the
  next retrieve or top operation on the mailpop3 structure
*/

static int newsnntp_get_content(newsnntp * f, char ** result,
				size_t * result_len)
{
  int r;
  char * response;
  MMAPString * buffer;
  char * result_multiline;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
    
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
    
  case 220:
  case 221:
  case 222:
  case 223:
    buffer = mmap_string_new("");
    if (buffer == NULL)
      return NEWSNNTP_ERROR_MEMORY;

    result_multiline = read_multiline(f, 0, buffer);
    if (result_multiline == NULL) {
      mmap_string_free(buffer);
      return NEWSNNTP_ERROR_MEMORY;
    }
    else {
      r = mmap_string_ref(buffer);
      if (r < 0) {
        mmap_string_free(buffer);
        return NEWSNNTP_ERROR_MEMORY;
      }
      
      * result = result_multiline;
      * result_len = buffer->len;
      return NEWSNNTP_NO_ERROR;
    }

  case 412:
    return NEWSNNTP_ERROR_NO_NEWSGROUP_SELECTED;

  case 420:
    return NEWSNNTP_ERROR_NO_ARTICLE_SELECTED;

  case 423:
    return NEWSNNTP_ERROR_INVALID_ARTICLE_NUMBER;

  case 430:
    return NEWSNNTP_ERROR_ARTICLE_NOT_FOUND;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

int newsnntp_head(newsnntp * f, uint32_t index, char ** result,
		  size_t * result_len)
{
  char command[NNTP_STRING_SIZE];
  int r;

  snprintf(command, NNTP_STRING_SIZE, "HEAD %i\r\n", index);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  return newsnntp_get_content(f, result, result_len);
}

/* ******************** ARTICLE ******************************** */

int newsnntp_article(newsnntp * f, uint32_t index, char ** result,
		     size_t * result_len)
{
  char command[NNTP_STRING_SIZE];
  int r;

  snprintf(command, NNTP_STRING_SIZE, "ARTICLE %i\r\n", index);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  return newsnntp_get_content(f, result, result_len);
}

/* ******************** BODY ******************************** */

int newsnntp_body(newsnntp * f, uint32_t index, char ** result,
		  size_t * result_len)
{
  char command[NNTP_STRING_SIZE];
  int r;

  snprintf(command, NNTP_STRING_SIZE, "BODY %i\r\n", index);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  return newsnntp_get_content(f, result, result_len);
}

/* ******************** GROUP ******************************** */

static struct newsnntp_group_info *
group_info_init(char * name, uint32_t first, uint32_t last, uint32_t count,
		char type)
{
  struct newsnntp_group_info * n;
  
  n = malloc(sizeof(* n));

  if (n == NULL)
    return NULL;
  
  n->grp_name = strdup(name);
  if (n->grp_name == NULL) {
    free(n);
    return NULL;
  }

  n->grp_first = first;
  n->grp_last = last;
  n->grp_count = count;
  n->grp_type = type;

  return n;
}

static void group_info_free(struct newsnntp_group_info * n)
{
  if (n->grp_name)
    free(n->grp_name);
  free(n);
}

static void group_info_list_free(clist * l)
{
  clist_foreach(l, (clist_func) group_info_free, NULL);
  clist_free(l);
}

static int parse_group_info(char * response,
			    struct newsnntp_group_info ** info);

int newsnntp_group(newsnntp * f, const char * groupname,
		   struct newsnntp_group_info ** info)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "GROUP %s\r\n", groupname);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
    
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 211:
    if (!parse_group_info(f->nntp_response, info))
      return NEWSNNTP_ERROR_INVALID_RESPONSE;
    return NEWSNNTP_NO_ERROR;
      
  case 411:
    return NEWSNNTP_ERROR_NO_SUCH_NEWS_GROUP;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_group_free(struct newsnntp_group_info * info)
{
  group_info_free(info);
}

/* ******************** LIST ******************************** */

static clist * read_groups_list(newsnntp * f);

int newsnntp_list(newsnntp * f, clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "LIST\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
    
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 215:
    * result = read_groups_list(f);
    return NEWSNNTP_NO_ERROR;
    
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_list_free(clist * l)
{
  group_info_list_free(l);
}

/* ******************** POST ******************************** */

static void send_data(newsnntp * f, const char * message, uint32_t size)
{
  mailstream_send_data(f->nntp_stream, message, size,
		       f->nntp_progr_rate, f->nntp_progr_fun);
}


int newsnntp_post(newsnntp * f, const char * message, size_t size)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "POST\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
    
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;

  case 340:
    break;
      
  case 440:
    return NEWSNNTP_ERROR_POSTING_NOT_ALLOWED;
    
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }

  send_data(f, message, size); 

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 240:
    return NEWSNNTP_NO_ERROR;
    return 1;

  case 441:
    return NEWSNNTP_ERROR_POSTING_FAILED;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}


/* ******************** AUTHINFO ******************************** */

int newsnntp_authinfo_username(newsnntp * f, const char * username)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "AUTHINFO USER %s\r\n", username);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;

  case 482:
    return NEWSNNTP_ERROR_AUTHENTICATION_REJECTED;

  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;

  case 281:
    return NEWSNNTP_NO_ERROR;
      
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

int newsnntp_authinfo_password(newsnntp * f, const char * password)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "AUTHINFO PASS %s\r\n", password);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;

  case 482:
    return NEWSNNTP_ERROR_AUTHENTICATION_REJECTED;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;

  case 281:
    return NEWSNNTP_NO_ERROR;
      
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

/* ******************** LIST OVERVIEW.FMT ******************************** */

static clist * read_headers_list(newsnntp * f);

static void headers_list_free(clist * l)
{
  clist_foreach(l, (clist_func) free, NULL);
  clist_free(l);
}

int newsnntp_list_overview_fmt(newsnntp * f, clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "LIST OVERVIEW.FMT\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 215:
    * result = read_headers_list(f);
    return NEWSNNTP_NO_ERROR;

  case 503: 
    return NEWSNNTP_ERROR_PROGRAM_ERROR;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_list_overview_fmt_free(clist * l)
{
  headers_list_free(l);
}






/* ******************** LIST ACTIVE ******************************** */

int newsnntp_list_active(newsnntp * f, const char * wildcard, clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  if (wildcard != NULL)
    snprintf(command, NNTP_STRING_SIZE, "LIST ACTIVE %s\r\n", wildcard);
  else
    snprintf(command, NNTP_STRING_SIZE, "LIST ACTIVE\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;

  case 215:
    * result = read_groups_list(f);
    return NEWSNNTP_NO_ERROR;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_list_active_free(clist * l)
{
  group_info_list_free(l);
}






/* ******************** LIST ACTIVE.TIMES ******************************** */

static struct newsnntp_group_time *
group_time_new(char * group_name, time_t date, char * email)
{
  struct newsnntp_group_time * n;
  
  n = malloc(sizeof(* n));

  if (n == NULL)
    return NULL;
  
  n->grp_name = strdup(group_name);
  if (n->grp_name == NULL) {
    free(n);
    return NULL;
  }

  n->grp_email = strdup(email);
  if (n->grp_email == NULL) {
    free(n->grp_name);
    free(n);
    return NULL;
  }

  n->grp_date = date;

  return n;
}

static void group_time_free(struct newsnntp_group_time * n)
{
  if (n->grp_name)
    free(n->grp_name);
  if (n->grp_email)
    free(n->grp_email);
  free(n);
}

static void group_time_list_free(clist * l)
{
  clist_foreach(l, (clist_func) group_time_free, NULL);
  clist_free(l);
}







static clist * read_group_time_list(newsnntp * f);


int newsnntp_list_active_times(newsnntp * f, clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "LIST ACTIVE.TIMES\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 215:
    * result = read_group_time_list(f);
    return NEWSNNTP_NO_ERROR;

  case 503: 
    return NEWSNNTP_ERROR_PROGRAM_ERROR;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_list_active_times_free(clist * l)
{
  group_time_list_free(l);
}








/* ********************** LIST DISTRIBUTION ***************************** */

static struct newsnntp_distrib_value_meaning *
distrib_value_meaning_new(char * value, char * meaning)
{
  struct newsnntp_distrib_value_meaning * n;
  
  n = malloc(sizeof(* n));

  if (n == NULL)
    return NULL;
  
  n->dst_value = strdup(value);
  if (n->dst_value == NULL) {
    free(n);
    return NULL;
  }

  n->dst_meaning = strdup(meaning);
  if (n->dst_meaning == NULL) {
    free(n->dst_value);
    free(n);
    return NULL;
  }

  return n;
}


static void
distrib_value_meaning_free(struct newsnntp_distrib_value_meaning * n)
{
  if (n->dst_value)
    free(n->dst_value);
  if (n->dst_meaning)
    free(n->dst_meaning);
  free(n);
}

static void distrib_value_meaning_list_free(clist * l)
{
  clist_foreach(l, (clist_func) distrib_value_meaning_free, NULL);
  clist_free(l);
}

static clist * read_distrib_value_meaning_list(newsnntp * f);


int newsnntp_list_distribution(newsnntp * f, clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "LIST DISTRIBUTION\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;
  
  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 215:
    * result = read_distrib_value_meaning_list(f);
    return NEWSNNTP_NO_ERROR;
    
  case 503: 
    return NEWSNNTP_ERROR_PROGRAM_ERROR;
    
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}


void newsnntp_list_distribution_free(clist * l)
{
  distrib_value_meaning_list_free(l);
}











/* ********************** LIST DISTRIB.PATS ***************************** */

static struct newsnntp_distrib_default_value *
distrib_default_value_new(uint32_t weight, char * group_pattern, char * value)
{
  struct newsnntp_distrib_default_value * n;

  n = malloc(sizeof(* n));
  if (n == NULL)
    return NULL;
  
  n->dst_group_pattern = strdup(group_pattern);
  if (n->dst_group_pattern == NULL) {
    free(n);
    return NULL;
  }

  n->dst_value = strdup(value);
  if (n->dst_value == NULL) {
    free(n->dst_group_pattern);
    free(n);
    return NULL;
  }

  n->dst_weight = weight;

  return n;
}

static void
distrib_default_value_free(struct newsnntp_distrib_default_value * n)
{
  if (n->dst_group_pattern)
    free(n->dst_group_pattern);
  if (n->dst_value)
    free(n->dst_value);
  free(n);
}

static void distrib_default_value_list_free(clist * l)
{
  clist_foreach(l, (clist_func) distrib_default_value_free, NULL);
  clist_free(l);
}

static clist * read_distrib_default_value_list(newsnntp * f);

int newsnntp_list_distrib_pats(newsnntp * f, clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "LIST DISTRIB.PATS\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 215:
    * result = read_distrib_default_value_list(f);
    return NEWSNNTP_NO_ERROR;

  case 503: 
    return NEWSNNTP_ERROR_PROGRAM_ERROR;
    
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_list_distrib_pats_free(clist * l)
{
  distrib_default_value_list_free(l);
}












/* ********************** LIST NEWSGROUPS ***************************** */

static struct newsnntp_group_description *
group_description_new(char * group_name, char * description)
{
  struct newsnntp_group_description * n;

  n = malloc(sizeof(* n));
  if (n == NULL)
    return NULL;
  
  n->grp_name = strdup(group_name);
  if (n->grp_name == NULL) {
    free(n);
    return NULL;
  }

  n->grp_description = strdup(description);
  if (n->grp_description == NULL) {
    free(n->grp_name);
    free(n);
    return NULL;
  }

  return n;
}

static void group_description_free(struct newsnntp_group_description * n)
{
  if (n->grp_name)
    free(n->grp_name);
  if (n->grp_description)
    free(n->grp_description);
  free(n);
}

static void group_description_list_free(clist * l)
{
  clist_foreach(l, (clist_func) group_description_free, NULL);
  clist_free(l);
}

static clist * read_group_description_list(newsnntp * f);

int newsnntp_list_newsgroups(newsnntp * f, const char * pattern,
			      clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  if (pattern)
    snprintf(command, NNTP_STRING_SIZE, "LIST NEWSGROUPS %s\r\n", pattern);
  else
    snprintf(command, NNTP_STRING_SIZE, "LIST NEWSGROUPS\r\n");
  
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 215:
    * result = read_group_description_list(f);
    return NEWSNNTP_NO_ERROR;

  case 503: 
    return NEWSNNTP_ERROR_PROGRAM_ERROR;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_list_newsgroups_free(clist * l)
{
  group_description_list_free(l);
}












/* ******************** LIST SUBSCRIPTIONS ******************************** */

static void subscriptions_list_free(clist * l)
{
  clist_foreach(l, (clist_func) free, NULL);
  clist_free(l);
}

static clist * read_subscriptions_list(newsnntp * f);

int newsnntp_list_subscriptions(newsnntp * f, clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "LIST SUBSCRIPTIONS\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 215:
    * result = read_subscriptions_list(f);
    return NEWSNNTP_NO_ERROR;

  case 503: 
    return NEWSNNTP_ERROR_PROGRAM_ERROR;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_list_subscriptions_free(clist * l)
{
  subscriptions_list_free(l);
}












/* ******************** LISTGROUP ******************************** */

static void articles_list_free(clist * l)
{
  clist_foreach(l, (clist_func) free, NULL);
  clist_free(l);
}

static clist * read_articles_list(newsnntp * f);

int newsnntp_listgroup(newsnntp * f, const char * group_name,
		       clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  if (group_name)
    snprintf(command, NNTP_STRING_SIZE, "LISTGROUP %s\r\n", group_name);
  else
    snprintf(command, NNTP_STRING_SIZE, "LISTGROUP\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 211:
    * result = read_articles_list(f);
    return NEWSNNTP_NO_ERROR;
      
  case 412:
    return NEWSNNTP_ERROR_NO_NEWSGROUP_SELECTED;

  case 502: 
    return NEWSNNTP_ERROR_NO_PERMISSION;
    
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

void newsnntp_listgroup_free(clist * l)
{
  articles_list_free(l);
}







/* ********************** MODE READER ***************************** */

int newsnntp_mode_reader(newsnntp * f)
{
  char command[NNTP_STRING_SIZE];
  char * response;
  int r;

  snprintf(command, NNTP_STRING_SIZE, "MODE READER\r\n");
  
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);
  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 200:
    return NEWSNNTP_NO_ERROR;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}

/* ********************** DATE ***************************** */

#define strfcpy(a,b,c) {if (c) {strncpy(a,b,c);a[c-1]=0;}}

int newsnntp_date(newsnntp * f, struct tm * tm)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;
  char year[5];
  char month[3];
  char day[3];
  char hour[3];
  char minute[3];
  char second[3];

  snprintf(command, NNTP_STRING_SIZE, "DATE\r\n");
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 111:
    strfcpy(year, f->nntp_response, 4);
    strfcpy(month, f->nntp_response + 4, 2);
    strfcpy(day, f->nntp_response + 6, 2);
    strfcpy(hour, f->nntp_response + 8, 2);
    strfcpy(minute, f->nntp_response + 10, 2);
    strfcpy(second, f->nntp_response + 12, 2);

    tm->tm_year = atoi(year);
    tm->tm_mon = atoi(month);
    tm->tm_mday = atoi(day);
    tm->tm_hour = atoi(hour);
    tm->tm_min = atoi(minute);
    tm->tm_sec = atoi(second);

    return NEWSNNTP_NO_ERROR;
      
  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}









/* ********************** XHDR ***************************** */

static struct newsnntp_xhdr_resp_item * xhdr_resp_item_new(uint32_t article,
							   char * value)
{
  struct newsnntp_xhdr_resp_item * n;

  n = malloc(sizeof(* n));
  if (n == NULL)
    return NULL;
  
  n->hdr_value = strdup(value);
  if (n->hdr_value == NULL) {
    free(n);
    return NULL;
  }

  n->hdr_article = article;

  return n;
}

static void xhdr_resp_item_free(struct newsnntp_xhdr_resp_item * n)
{
  if (n->hdr_value)
    free(n->hdr_value);
  free(n);
}

static void xhdr_resp_list_free(clist * l)
{
  clist_foreach(l, (clist_func) xhdr_resp_item_free, NULL);
  clist_free(l);
}

static clist * read_xhdr_resp_list(newsnntp * f);

static int newsnntp_xhdr_resp(newsnntp * f, clist ** result);

int newsnntp_xhdr_single(newsnntp * f, const char * header, uint32_t article,
			  clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;

  snprintf(command, NNTP_STRING_SIZE, "XHDR %s %i\r\n", header, article);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  return newsnntp_xhdr_resp(f, result);
}

int newsnntp_xhdr_range(newsnntp * f, const char * header,
			 uint32_t rangeinf, uint32_t rangesup,
			 clist ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;

  snprintf(command, NNTP_STRING_SIZE, "XHDR %s %i-%i\r\n", header,
	   rangeinf, rangesup);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  return newsnntp_xhdr_resp(f, result);
}

void newsnntp_xhdr_free(clist * l)
{
  xhdr_resp_list_free(l);
}

static int newsnntp_xhdr_resp(newsnntp * f, clist ** result)
{
  int r;
  char * response;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 221:
    * result = read_xhdr_resp_list(f);
    return NEWSNNTP_NO_ERROR;

  case 412:
    return NEWSNNTP_ERROR_NO_NEWSGROUP_SELECTED;

  case 420:
    return NEWSNNTP_ERROR_NO_ARTICLE_SELECTED;

  case 430:
    return NEWSNNTP_ERROR_ARTICLE_NOT_FOUND;

  case 502: 
    return NEWSNNTP_ERROR_NO_PERMISSION;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}














/* ********************** XOVER ***************************** */

static struct newsnntp_xover_resp_item *
xover_resp_item_new(uint32_t article,
		    char * subject,
		    char * author,
		    char * date,
		    char * message_id,
		    char * references,
		    size_t size,
		    uint32_t line_count,
		    clist * others)
{
  struct newsnntp_xover_resp_item * n;

  n = malloc(sizeof(* n));
  if (n == NULL)
    return NULL;
  
  n->ovr_subject = strdup(subject);
  if (n->ovr_subject == NULL) {
    free(n);
    return NULL;
  }

  n->ovr_author = strdup(author);
  if (n->ovr_author == NULL) {
    free(n->ovr_subject);
    free(n);
    return NULL;
  }

  n->ovr_date = strdup(date);
  if (n->ovr_date == NULL) {
    free(n->ovr_subject);
    free(n->ovr_author);
    free(n);
    return NULL;
  }

  n->ovr_message_id = strdup(message_id);
  if (n->ovr_message_id == NULL) {
    free(n->ovr_subject);
    free(n->ovr_author);
    free(n->ovr_date);
    free(n);
    return NULL;
  }

  n->ovr_references = strdup(references);
  if (n->ovr_references == NULL) {
    free(n->ovr_subject);
    free(n->ovr_author);
    free(n->ovr_date);
    free(n->ovr_message_id);
    free(n);
    return NULL;
  }

  n->ovr_article = article;
  n->ovr_size = size;
  n->ovr_line_count = line_count;
  n->ovr_others = others;

  return n;
}

void xover_resp_item_free(struct newsnntp_xover_resp_item * n)
{
  if (n->ovr_subject)
    free(n->ovr_subject);
  if (n->ovr_author)
    free(n->ovr_author);
  if (n->ovr_date)
    free(n->ovr_date);
  if (n->ovr_message_id)
    free(n->ovr_message_id);
  if (n->ovr_references)
    free(n->ovr_references);
  clist_foreach(n->ovr_others, (clist_func) free, NULL);
  clist_free(n->ovr_others);
  
  free(n);
}

void newsnntp_xover_resp_list_free(clist * l)
{
  clist_foreach(l, (clist_func) xover_resp_item_free, NULL);
  clist_free(l);
}

static clist * read_xover_resp_list(newsnntp * f);


static int newsnntp_xover_resp(newsnntp * f, clist ** result);

int newsnntp_xover_single(newsnntp * f, uint32_t article,
			   struct newsnntp_xover_resp_item ** result)
{
  char command[NNTP_STRING_SIZE];
  int r;
  clist * list;
  clistiter * cur;
  struct newsnntp_xover_resp_item * item;

  snprintf(command, NNTP_STRING_SIZE, "XOVER %i\r\n", article);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  r = newsnntp_xover_resp(f, &list);
  if (r != NEWSNNTP_NO_ERROR)
    return r;

  cur = clist_begin(list);
  item = clist_content(cur);
  clist_free(list);
  
  * result = item;

  return r;
}

int newsnntp_xover_range(newsnntp * f, uint32_t rangeinf, uint32_t rangesup,
			  clist ** result)
{
  int r;
  char command[NNTP_STRING_SIZE];

  snprintf(command, NNTP_STRING_SIZE, "XOVER %i-%i\r\n", rangeinf, rangesup);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  return newsnntp_xover_resp(f, result);
}

static int newsnntp_xover_resp(newsnntp * f, clist ** result)
{
  int r;
  char * response;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 224:
    * result = read_xover_resp_list(f);
    return NEWSNNTP_NO_ERROR;

  case 412:
    return NEWSNNTP_ERROR_NO_NEWSGROUP_SELECTED;

  case 420:
    return NEWSNNTP_ERROR_NO_ARTICLE_SELECTED;

  case 502:
    return NEWSNNTP_ERROR_NO_PERMISSION;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}







/* ********************** AUTHINFO GENERIC ***************************** */

int newsnntp_authinfo_generic(newsnntp * f, const char * authentificator,
			       const char * arguments)
{
  char command[NNTP_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, NNTP_STRING_SIZE, "AUTHINFO GENERIC %s %s\r\n",
	   authentificator, arguments);
  r = send_command(f, command);
  if (r == -1)
    return NEWSNNTP_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return NEWSNNTP_ERROR_STREAM;

  r = parse_response(f, response);

  switch (r) {
  case 480:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME;
      
  case 381:
    return NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD;
      
  case 281:
    return NEWSNNTP_NO_ERROR;

  case 500:
    return NEWSNNTP_ERROR_COMMAND_NOT_UNDERSTOOD;

  case 501: 
    return NEWSNNTP_ERROR_COMMAND_NOT_SUPPORTED;

  case 502:
    return NEWSNNTP_ERROR_NO_PERMISSION;

  case 503:
    return NEWSNNTP_ERROR_PROGRAM_ERROR;

  default:
    return NEWSNNTP_ERROR_UNEXPECTED_RESPONSE;
  }
}



















static int parse_space(char ** line)
{
  char * p;

  p = * line;

  while ((* p == ' ') || (* p == '\t'))
    p ++;

  if (p != * line) {
    * line = p;
    return TRUE;
  }
  else
    return FALSE;
}

static char * cut_token(char * line)
{
  char * p;
  char * p_tab;
  char * p_space;

  p = line;

  p_space = strchr(line, ' ');
  p_tab = strchr(line, '\t');
  if (p_tab == NULL)
    p = p_space;
  else if (p_space == NULL)
    p = p_tab;
  else {
    if (p_tab < p_space)
      p = p_tab;
    else
      p = p_space;
  }
  if (p == NULL)
    return NULL;
  * p = 0;
  p ++;

  return p;
}

static int parse_response(newsnntp * f, char * response)
{
  int code;

  code = strtol(response, &response, 10);

  if (response == NULL) {
    f->nntp_response = NULL;
    return code;
  }

  parse_space(&response);

  if (mmap_string_assign(f->nntp_response_buffer, response) != NULL)
    f->nntp_response = f->nntp_response_buffer->str;
  else
    f->nntp_response = NULL;
 
  return code;
}


static char * read_line(newsnntp * f)
{
  return mailstream_read_line_remove_eol(f->nntp_stream, f->nntp_stream_buffer);
}

static char * read_multiline(newsnntp * f, size_t size,
			     MMAPString * multiline_buffer)
{
  return mailstream_read_multiline(f->nntp_stream, size,
				   f->nntp_stream_buffer, multiline_buffer,
				   f->nntp_progr_rate, f->nntp_progr_fun);
}







static int parse_group_info(char * response,
			    struct newsnntp_group_info ** result)
{
  char * line;
  uint32_t first;
  uint32_t last;
  uint32_t count;
  char * name;
  struct newsnntp_group_info * info;

  line = response;

  count = strtoul(line, &line, 10);
  if (!parse_space(&line))
    return FALSE;

  first = strtoul(line, &line, 10);
  if (!parse_space(&line))
    return FALSE;

  last = strtoul(line, &line, 10);
  if (!parse_space(&line))
    return FALSE;

  name = line;

  info = group_info_init(name, first, last, count, FALSE);
  if (info == NULL)
    return FALSE;

  * result = info;
  
  return TRUE;
}


static clist * read_groups_list(newsnntp * f)
{
  char * line;
  char * group_name;
  uint32_t first;
  uint32_t last;
  uint32_t count;
  int type;
  clist * groups_list;
  struct newsnntp_group_info * n;
  int r;

  groups_list = clist_new();
  if (groups_list == NULL)
    goto err;

  while (1) {
    char * p;
      
    line = read_line(f);
    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;

    p = cut_token(line);
    if (p == NULL)
      continue;

    group_name = line;
    line = p;

    last = strtol(line, &line, 10);
    if (!parse_space(&line))
      continue;

    first = strtol(line, &line, 10);
    if (!parse_space(&line))
      continue;

    count = last - first + 1;

    type = * line;

    n = group_info_init(group_name, first, last, count, type);
    if (n == NULL)
      goto free_list;

    r = clist_append(groups_list, n);
    if (r < 0) {
      group_info_free(n);
      goto free_list;
    }
  }

  return groups_list;

 free_list:
  group_info_list_free(groups_list);
 err:
  return NULL;
}


static clist * read_headers_list(newsnntp * f)
{
  char * line;
  clist * headers_list;
  char * header;
  int r;

  headers_list = clist_new();
  if (headers_list == NULL)
    goto err;

  while (1) {
    line = read_line(f);
    
    if (line == NULL)
      goto free_list;
    
    if (mailstream_is_end_multiline(line))
      break;
    
    header = strdup(line);
    if (header == NULL)
      goto free_list;

    r = clist_append(headers_list, header);
    if (r < 0) {
      free(header);
      goto free_list;
    }
  }

  return headers_list;

 free_list:
  headers_list_free(headers_list);
 err:
  return NULL;
}




static clist * read_group_time_list(newsnntp * f)
{
  char * line;
  char * group_name;
  time_t date;
  char * email;
  clist * group_time_list;
  struct newsnntp_group_time * n;
  int r;

  group_time_list = clist_new();
  if (group_time_list == NULL)
    goto err;

  while (1) {
    char * p;
    char * remaining;
    
    line = read_line(f);
    
    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;
    
    p = cut_token(line);
    if (p == NULL)
      continue;
      
    date = strtoul(p, &remaining, 10);

    p = remaining;
    parse_space(&p);

    email = p;
    
    group_name = line;
    
    n = group_time_new(group_name, date, email);
    if (n == NULL)
      goto free_list;
    
    r = clist_append(group_time_list, n);
    if (r < 0) {
      group_time_free(n);
      goto free_list;
    }
  }
  
  return group_time_list;

 free_list:
  group_time_list_free(group_time_list);
 err:
  return NULL;
}




static clist * read_distrib_value_meaning_list(newsnntp * f)
{
  char * line;
  char * value;
  char * meaning;
  clist * distrib_value_meaning_list;
  struct newsnntp_distrib_value_meaning * n;
  int r;

  distrib_value_meaning_list = clist_new();
  if (distrib_value_meaning_list == NULL)
    goto err;

  while (1) {
    char * p;
      
    line = read_line(f);
    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;
      
    p = cut_token(line);
    if (p == NULL)
      continue;
      
    meaning = p;

    value = line;

    n = distrib_value_meaning_new(value, meaning);
    if (n == NULL)
      goto free_list;

    r = clist_append(distrib_value_meaning_list, n);
    if (r < 0) {
      distrib_value_meaning_free(n);
      goto free_list;
    }
  }

  return distrib_value_meaning_list;

 free_list:
  distrib_value_meaning_list_free(distrib_value_meaning_list);
 err:
  return NULL;
}




static clist * read_distrib_default_value_list(newsnntp * f)
{
  char * line;
  uint32_t weight;
  char * group_pattern;
  char * meaning;
  clist * distrib_default_value_list;
  struct newsnntp_distrib_default_value * n;
  int r;

  distrib_default_value_list = clist_new();
  if (distrib_default_value_list == NULL)
    goto err;

  while (1) {
    char * p;
    char * remaining;
      
    line = read_line(f);
    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;

    p = line;

    weight = strtoul(p, &remaining, 10);
    p = remaining;
    parse_space(&p);
      
    p = cut_token(line);
    if (p == NULL)
      continue;

    meaning = p;
    group_pattern = line;

    n = distrib_default_value_new(weight, group_pattern, meaning);
    if (n == NULL)
      goto free_list;

    r = clist_append(distrib_default_value_list, n);
    if (r < 0) {
      distrib_default_value_free(n);
      goto free_list;
    }
  }

  return distrib_default_value_list;

 free_list:
  distrib_default_value_list_free(distrib_default_value_list);
 err:
  return NULL;
}



static clist * read_group_description_list(newsnntp * f)
{
  char * line;
  char * group_name;
  char * description;
  clist * group_description_list;
  struct newsnntp_group_description * n;
  int r;

  group_description_list = clist_new();
  if (group_description_list == NULL)
    goto err;

  while (1) {
    char * p;
      
    line = read_line(f);
    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;

    p = cut_token(line);
    if (p == NULL)
      continue;

    description = p;

    group_name = line;

    n = group_description_new(group_name, description);
    if (n == NULL)
      goto free_list;

    r = clist_append(group_description_list, n);
    if (r < 0) {
      group_description_free(n);
      goto free_list;
    }
  }

  return group_description_list;

 free_list:
  group_description_list_free(group_description_list);
 err:
  return NULL;
}



static clist * read_subscriptions_list(newsnntp * f)
{
  char * line;
  clist * subscriptions_list;
  char * group_name;
  int r;

  subscriptions_list = clist_new();
  if (subscriptions_list == NULL)
    goto err;

  while (1) {
    line = read_line(f);

    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;

    group_name = strdup(line);
    if (group_name == NULL)
      goto free_list;

    r = clist_append(subscriptions_list, group_name);
    if (r < 0) {
      free(group_name);
      goto free_list;
    }
  }

  return subscriptions_list;

 free_list:
  subscriptions_list_free(subscriptions_list);
 err:
  return NULL;
}



static clist * read_articles_list(newsnntp * f)
{
  char * line;
  clist * articles_list;
  uint32_t * article_num;
  int r;

  articles_list = clist_new();
  if (articles_list == NULL)
    goto err;

  while (1) {
    line = read_line(f);
    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;

    article_num = malloc(sizeof(* article_num));
    if (article_num == NULL)
      goto free_list;
    * article_num = atoi(line);

    r = clist_append(articles_list, article_num);
    if (r < 0) {
      free(article_num);
      goto free_list;
    }
  }

  return articles_list;

 free_list:
  articles_list_free(articles_list);
 err:
  return NULL;
}

static clist * read_xhdr_resp_list(newsnntp * f)
{
  char * line;
  uint32_t article;
  char * value;
  clist * xhdr_resp_list;
  struct newsnntp_xhdr_resp_item * n;
  int r;

  xhdr_resp_list = clist_new();
  if (xhdr_resp_list == NULL)
    goto err;

  while (1) {
    line = read_line(f);
    
    if (line == NULL)
      goto free_list;
    
    if (mailstream_is_end_multiline(line))
      break;
    
    article = strtoul(line, &line, 10);
    if (!parse_space(&line))
      continue;
    
    value = line;
    
    n = xhdr_resp_item_new(article, value);
    if (n == NULL)
      goto free_list;
    
    r = clist_append(xhdr_resp_list, n);
    if (r < 0) {
      xhdr_resp_item_free(n);
      goto free_list;
    }
  }
  
  return xhdr_resp_list;

 free_list:
  xhdr_resp_list_free(xhdr_resp_list);
 err:
  return NULL;
}


static clist * read_xover_resp_list(newsnntp * f)
{
  char * line;
  clist * xover_resp_list;
  struct newsnntp_xover_resp_item * n;
  clist * values_list;
  clistiter * current;
  uint32_t article;
  char * subject;
  char * author;
  char * date;
  char * message_id;
  char * references;
  size_t size;
  uint32_t line_count;
  clist * others;
  int r;
  
  xover_resp_list = clist_new();
  if (xover_resp_list == NULL)
    goto err;

  while (1) {
    char * p;
      
    line = read_line(f);

    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;

    /* parse the data separated with \t */

    values_list = clist_new();
    if (values_list == NULL)
      goto free_list;

    while ((p = strchr(line, '\t')) != NULL) {
      * p = 0;
      p ++;

      r = clist_append(values_list, line);
      if (r < 0)
	goto free_values_list;
      line = p;
    }

    r = clist_append(values_list, line);
    if (r < 0)
      goto free_values_list;

    /* set the known data */
    current = clist_begin(values_list);
    article = atoi((char *) clist_content(current));

    current = clist_next(current);
    if (current == NULL) {
      clist_free(values_list);
      continue;
    }
    subject = clist_content(current);

    current = clist_next(current);
    if (current == NULL) {
      clist_free(values_list);
      continue;
    }
    author = clist_content(current);

    current = clist_next(current);
    if (current == NULL) {
      clist_free(values_list);
      continue;
    }
    date = clist_content(current);

    current = clist_next(current);
    if (current == NULL) {
      clist_free(values_list);
      continue;
    }
    message_id = clist_content(current);

    current = clist_next(current);
    if (current == NULL) {
      clist_free(values_list);
      continue;
    }
    references = clist_content(current);

    current = clist_next(current);
    if (current == NULL) {
      clist_free(values_list);
      continue;
    }
    size = atoi((char *) clist_content(current));

    current = clist_next(current);
    if (current == NULL) {
      clist_free(values_list);
      continue;
    }
    line_count = atoi((char *) clist_content(current));

    current = clist_next(current);

    /* make a copy of the other data */
    others = clist_new();
    if (others == NULL) {
      goto free_values_list;
    }

    while (current) {
      char * val;
      char * original_val;

      original_val = clist_content(current);
      val = strdup(original_val);
      if (val == NULL) {
	clist_foreach(others, (clist_func) free, NULL);
	clist_free(others);
	goto free_list;
      }

      r = clist_append(others, val);
      if (r < 0) {
	goto free_list;
      }

      current = clist_next(current);
    }

    clist_free(values_list);

    n = xover_resp_item_new(article, subject, author, date, message_id,
			    references, size, line_count, others);
    if (n == NULL) {
      clist_foreach(others, (clist_func) free, NULL);
      clist_free(others);
      goto free_list;
    }

    r = clist_append(xover_resp_list, n);
    if (r < 0) {
      xover_resp_item_free(n);
      goto free_list;
    }
  }

  return xover_resp_list;

 free_list:
  newsnntp_xover_resp_list_free(xover_resp_list);
 err:
  return NULL;

 free_values_list:
  clist_foreach(values_list, (clist_func) free, NULL);
  clist_free(values_list);
  return NULL;
}

static int send_command(newsnntp * f, char * command)
{
  ssize_t r;

  r = mailstream_write(f->nntp_stream, command, strlen(command));
  if (r == -1)
    return -1;

  r = mailstream_flush(f->nntp_stream);
  if (r == -1)
    return -1;

  return 0;
}
