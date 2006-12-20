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
 * $Id: mailpop3.c,v 1.28 2006/08/29 23:14:02 hoa Exp $
 */

/*
  POP3 Protocol

  RFC 1734
  RFC 1939
  RFC 2449

 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailpop3.h"
#include <stdio.h>
#include <string.h>
#include "md5.h"
#include "mail.h"
#include <stdlib.h>

#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#include "mailsasl.h"




enum {
  POP3_STATE_DISCONNECTED,
  POP3_STATE_AUTHORIZATION,
  POP3_STATE_TRANSACTION
};



/*
  mailpop3_msg_info structure
*/

static struct mailpop3_msg_info *
mailpop3_msg_info_new(unsigned int index, uint32_t size, char * uidl)
{
  struct mailpop3_msg_info * msg;

  msg = malloc(sizeof(* msg));
  if (msg == NULL)
    return NULL;
  msg->msg_index = index;
  msg->msg_size = size;
  msg->msg_deleted = FALSE;
  msg->msg_uidl = uidl;

  return msg;
}

static void mailpop3_msg_info_free(struct mailpop3_msg_info * msg)
{
  if (msg->msg_uidl != NULL)
    free(msg->msg_uidl);
  free(msg);
}

static void mailpop3_msg_info_tab_free(carray * msg_tab)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(msg_tab) ; i++) {
    struct mailpop3_msg_info * msg;
    
    msg = carray_get(msg_tab, i);
    mailpop3_msg_info_free(msg);
  }
  carray_free(msg_tab);
}

static void mailpop3_msg_info_tab_reset(carray * msg_tab)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(msg_tab) ; i++) {
    struct mailpop3_msg_info * msg;
    msg = carray_get(msg_tab, i);
    msg->msg_deleted = FALSE;
  }
}

static inline struct mailpop3_msg_info *
mailpop3_msg_info_tab_find_msg(carray * msg_tab, unsigned int index)
{
  struct mailpop3_msg_info * msg;

  if (index == 0)
    return NULL;

  if (index > carray_count(msg_tab))
    return NULL;

  msg = carray_get(msg_tab, index - 1);

  return msg;
}



int mailpop3_get_msg_info(mailpop3 * f, unsigned int index,
			   struct mailpop3_msg_info ** result)
{
  carray * tab;
  struct mailpop3_msg_info * info;

  mailpop3_list(f, &tab);
  
  if (tab == NULL)
    return MAILPOP3_ERROR_BAD_STATE;

  info = mailpop3_msg_info_tab_find_msg(tab, index);
  if (info == NULL)
    return MAILPOP3_ERROR_NO_SUCH_MESSAGE;

  * result = info;

  return MAILPOP3_NO_ERROR;
}


/*
  mailpop3_capa
*/

struct mailpop3_capa * mailpop3_capa_new(char * name, clist * param)
{
  struct mailpop3_capa * capa;

  capa = malloc(sizeof(* capa));
  if (capa == NULL)
    return NULL;
  capa->cap_name = name;
  capa->cap_param = param;
  
  return capa;
}


void mailpop3_capa_free(struct mailpop3_capa * capa)
{
  clist_foreach(capa->cap_param, (clist_func) free, NULL);
  clist_free(capa->cap_param);
  free(capa->cap_name);
  free(capa);
}

/*
  mailpop3 structure
*/

mailpop3 * mailpop3_new(size_t progr_rate, progress_function * progr_fun)
{
  mailpop3 * f;

  f = malloc(sizeof(* f));
  if (f == NULL)
    goto err;

  f->pop3_timestamp = NULL;
  f->pop3_response = NULL;
  
  f->pop3_stream = NULL;

  f->pop3_progr_rate = progr_rate;
  f->pop3_progr_fun = progr_fun;

  f->pop3_stream_buffer = mmap_string_new("");
  if (f->pop3_stream_buffer == NULL)
    goto free_f;

  f->pop3_response_buffer = mmap_string_new("");
  if (f->pop3_response_buffer == NULL)
    goto free_stream_buffer;

  f->pop3_msg_tab = NULL;
  f->pop3_deleted_count = 0;
  f->pop3_state = POP3_STATE_DISCONNECTED;
  
#ifdef USE_SASL
  f->pop3_sasl.sasl_conn = NULL;
#endif
  
  return f;

 free_stream_buffer:
  mmap_string_free(f->pop3_stream_buffer);
 free_f:
  free(f);
 err:
  return NULL;
}



void mailpop3_free(mailpop3 * f)
{
#ifdef USE_SASL
  if (f->pop3_sasl.sasl_conn != NULL) {
    sasl_dispose((sasl_conn_t **) &f->pop3_sasl.sasl_conn);
    mailsasl_unref();
  }
#endif
  
  if (f->pop3_stream)
    mailpop3_quit(f);

  mmap_string_free(f->pop3_response_buffer);
  mmap_string_free(f->pop3_stream_buffer);

  free(f);
}











/*
  operations on mailpop3 structure
*/

#define RESPONSE_OK 0
#define RESPONSE_ERR -1
#define RESPONSE_AUTH_CONT 1

static int send_command(mailpop3 * f, char * command);

static char * read_line(mailpop3 * f);

static char * read_multiline(mailpop3 * f, size_t size,
			      MMAPString * multiline_buffer);

static int parse_response(mailpop3 * f, char * response);


/* get the timestamp in the connection response */

#define TIMESTAMP_START '<'
#define TIMESTAMP_END '>'

static char * mailpop3_get_timestamp(char * response)
{
  char * begin_timestamp;
  char * end_timestamp;
  char * timestamp;
  int len_timestamp;

  if (response == NULL)
    return NULL;
  
  begin_timestamp = strchr(response, TIMESTAMP_START);

  end_timestamp = NULL;
  if (begin_timestamp != NULL) {
    end_timestamp = strchr(begin_timestamp, TIMESTAMP_END);
    if (end_timestamp == NULL)
      begin_timestamp = NULL;
  }
   
  if (!begin_timestamp)
    return NULL;

  len_timestamp = end_timestamp - begin_timestamp + 1;

  timestamp = malloc(len_timestamp + 1);
  if (timestamp == NULL)
    return NULL;
  strncpy(timestamp, begin_timestamp, len_timestamp);
  timestamp[len_timestamp] = '\0';
 
  return timestamp;
}

/*
  connect a stream to the mailpop3 structure
*/

int mailpop3_connect(mailpop3 * f, mailstream * s)
{
  char * response;
  int r;
  char * timestamp;

  if (f->pop3_state != POP3_STATE_DISCONNECTED)
    return MAILPOP3_ERROR_BAD_STATE;

  f->pop3_stream = s;

  response = read_line(f);

  r = parse_response(f, response);
  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_UNAUTHORIZED;

  f->pop3_state = POP3_STATE_AUTHORIZATION;

  timestamp = mailpop3_get_timestamp(f->pop3_response);
  if (timestamp != NULL)
    f->pop3_timestamp = timestamp;

  return MAILPOP3_NO_ERROR;
}


/*
  disconnect from a pop3 server
*/

int mailpop3_quit(mailpop3 * f)
{
  char command[POP3_STRING_SIZE];
  char * response;
  int r;
  int res;

  if ((f->pop3_state != POP3_STATE_AUTHORIZATION) 
      && (f->pop3_state != POP3_STATE_TRANSACTION)) {
    res = MAILPOP3_ERROR_BAD_STATE;
    goto close;
  }

  snprintf(command, POP3_STRING_SIZE, "QUIT\r\n");
  r = send_command(f, command);
  if (r == -1) {
    res = MAILPOP3_ERROR_STREAM;
    goto close;
  }

  response = read_line(f);
  if (response == NULL) {
    res = MAILPOP3_ERROR_STREAM;
    goto close;
  }
  parse_response(f, response);

  res = MAILPOP3_NO_ERROR;

 close:
  if (f->pop3_state != POP3_STATE_DISCONNECTED)
    mailstream_close(f->pop3_stream);

  if (f->pop3_timestamp != NULL) {
    free(f->pop3_timestamp);
    f->pop3_timestamp = NULL;
  }

  f->pop3_stream = NULL;
  if (f->pop3_msg_tab != NULL) {
    mailpop3_msg_info_tab_free(f->pop3_msg_tab);
    f->pop3_msg_tab = NULL;
  }
  
  f->pop3_state = POP3_STATE_DISCONNECTED;
  
  return res;
}
































int mailpop3_apop(mailpop3 * f,
		  const char * user, const char * password)
{
  char command[POP3_STRING_SIZE];
  MD5_CTX md5context;
  unsigned char md5digest[16];
  char md5string[33];
  char * cmd_ptr;
  int r;
  int i;
  char * response;

  if (f->pop3_state != POP3_STATE_AUTHORIZATION)
    return MAILPOP3_ERROR_BAD_STATE;

  if (f->pop3_timestamp == NULL)
    return MAILPOP3_ERROR_APOP_NOT_SUPPORTED;

  /* calculate md5 sum */

  MD5Init(&md5context);
  MD5Update(&md5context, f->pop3_timestamp, strlen (f->pop3_timestamp));
  MD5Update(&md5context, password, strlen (password));
  MD5Final(md5digest, &md5context);
  
  cmd_ptr = md5string;
  for(i = 0 ; i < 16 ; i++, cmd_ptr += 2)
    snprintf(cmd_ptr, 3, "%02x", md5digest[i]);
  * cmd_ptr = 0;
  
  /* send apop command */
  
  snprintf(command, POP3_STRING_SIZE, "APOP %s %s\r\n", user, md5string);
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);

  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);
  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_DENIED;

  f->pop3_state = POP3_STATE_TRANSACTION;

  return MAILPOP3_NO_ERROR;
}

int mailpop3_user(mailpop3 * f, const char * user)
{
  char command[POP3_STRING_SIZE];
  int r;
  char * response;

  if (f->pop3_state != POP3_STATE_AUTHORIZATION)
    return MAILPOP3_ERROR_BAD_STATE;
  
  /* send user command */
    
  snprintf(command, POP3_STRING_SIZE, "USER %s\r\n", user);
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);

  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_BAD_USER;

  return MAILPOP3_NO_ERROR;
}

int mailpop3_pass(mailpop3 * f, const char * password)
{
  char command[POP3_STRING_SIZE];
  int r;
  char * response;

  if (f->pop3_state != POP3_STATE_AUTHORIZATION)
    return MAILPOP3_ERROR_BAD_STATE;

  /* send password command */

  snprintf(command, POP3_STRING_SIZE, "PASS %s\r\n", password);
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);

  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_BAD_PASSWORD;

  f->pop3_state = POP3_STATE_TRANSACTION;

  return MAILPOP3_NO_ERROR;
}

static int read_list(mailpop3 * f, carray ** result);



static int read_uidl(mailpop3 * f, carray * msg_tab);



static int mailpop3_do_uidl(mailpop3 * f, carray * msg_tab)
{
  char command[POP3_STRING_SIZE];
  int r;
  char * response;

  if (f->pop3_state != POP3_STATE_TRANSACTION)
    return MAILPOP3_ERROR_BAD_STATE;

  /* send list command */
  
  snprintf(command, POP3_STRING_SIZE, "UIDL\r\n");
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);

  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_CANT_LIST;
  
  r = read_uidl(f, msg_tab);
  if (r != MAILPOP3_NO_ERROR)
    return r;

  return MAILPOP3_NO_ERROR;
}



static int mailpop3_do_list(mailpop3 * f)
{
  char command[POP3_STRING_SIZE];
  int r;
  carray * msg_tab;
  char * response;

  if (f->pop3_msg_tab != NULL) {
    mailpop3_msg_info_tab_free(f->pop3_msg_tab);
    f->pop3_msg_tab = NULL;
  }

  if (f->pop3_state != POP3_STATE_TRANSACTION)
    return MAILPOP3_ERROR_BAD_STATE;

  /* send list command */

  snprintf(command, POP3_STRING_SIZE, "LIST\r\n");
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);

  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_CANT_LIST;
  
  r = read_list(f, &msg_tab);
  if (r != MAILPOP3_NO_ERROR)
    return r;

  f->pop3_msg_tab = msg_tab;
  f->pop3_deleted_count = 0;
  
  mailpop3_do_uidl(f, msg_tab);

  return MAILPOP3_NO_ERROR;
}



static void mailpop3_list_if_needed(mailpop3 * f)
{
  if (f->pop3_msg_tab == NULL)
    mailpop3_do_list(f);
}

/*
  mailpop3_list
*/

void mailpop3_list(mailpop3 * f, carray ** result)
{
  mailpop3_list_if_needed(f);
  * result = f->pop3_msg_tab;
}

static inline struct mailpop3_msg_info *
find_msg(mailpop3 * f, unsigned int index)
{
  mailpop3_list_if_needed(f);

  if (f->pop3_msg_tab == NULL)
    return NULL;

  return mailpop3_msg_info_tab_find_msg(f->pop3_msg_tab, index);
}








static void mailpop3_multiline_response_free(char * str)
{
  mmap_string_unref(str);
}

void mailpop3_top_free(char * str)
{
  mailpop3_multiline_response_free(str);
}

void mailpop3_retr_free(char * str)
{
  mailpop3_multiline_response_free(str);
}

/*
  mailpop3_retr

  message content in (* result) is still there until the
  next retrieve or top operation on the mailpop3 structure
*/

static int
mailpop3_get_content(mailpop3 * f, struct mailpop3_msg_info * msginfo,
		     char ** result, size_t * result_len)
{
  char * response;
  char * result_multiline;
  MMAPString * buffer;
  int r;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);
  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_NO_SUCH_MESSAGE;

  buffer = mmap_string_new("");
  if (buffer == NULL)
    return MAILPOP3_ERROR_MEMORY;

  result_multiline = read_multiline(f, msginfo->msg_size, buffer);
  if (result_multiline == NULL) {
    mmap_string_free(buffer);
    return MAILPOP3_ERROR_STREAM;
  }
  else {
    r = mmap_string_ref(buffer);
    if (r < 0) {
      mmap_string_free(buffer);
      return MAILPOP3_ERROR_MEMORY;
    }
    
    * result = result_multiline;
    * result_len = buffer->len;
    return MAILPOP3_NO_ERROR;
  }
}

int mailpop3_retr(mailpop3 * f, unsigned int index, char ** result,
		  size_t * result_len)
{
  char command[POP3_STRING_SIZE];
  struct mailpop3_msg_info * msginfo;
  int r;

  if (f->pop3_state != POP3_STATE_TRANSACTION)
    return MAILPOP3_ERROR_BAD_STATE;

  msginfo = find_msg(f, index);

  if (msginfo == NULL) {
    f->pop3_response = NULL;
    return MAILPOP3_ERROR_NO_SUCH_MESSAGE;
  }

  snprintf(command, POP3_STRING_SIZE, "RETR %i\r\n", index);
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  return mailpop3_get_content(f, msginfo, result, result_len);
}

int mailpop3_top(mailpop3 * f, unsigned int index,
    unsigned int count, char ** result,
    size_t * result_len)
{
  char command[POP3_STRING_SIZE];
  struct mailpop3_msg_info * msginfo;
  int r;

  if (f->pop3_state != POP3_STATE_TRANSACTION)
    return MAILPOP3_ERROR_BAD_STATE;

  msginfo = find_msg(f, index);

  if (msginfo == NULL) {
    f->pop3_response = NULL;
    return MAILPOP3_ERROR_NO_SUCH_MESSAGE;
  }

  snprintf(command, POP3_STRING_SIZE, "TOP %i %i\r\n", index, count);
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  return mailpop3_get_content(f, msginfo, result, result_len);
}

int mailpop3_dele(mailpop3 * f, unsigned int index)
{
  char command[POP3_STRING_SIZE];
  struct mailpop3_msg_info * msginfo;
  char * response;
  int r;

  if (f->pop3_state != POP3_STATE_TRANSACTION)
    return MAILPOP3_ERROR_BAD_STATE;

  msginfo = find_msg(f, index);

  if (msginfo == NULL) {
    f->pop3_response = NULL;
    return MAILPOP3_ERROR_NO_SUCH_MESSAGE;
  }

  snprintf(command, POP3_STRING_SIZE, "DELE %i\r\n", index);
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);
  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_NO_SUCH_MESSAGE;

  msginfo->msg_deleted = TRUE;
  f->pop3_deleted_count ++;
  
  return MAILPOP3_NO_ERROR;
}

int mailpop3_noop(mailpop3 * f)
{
  char command[POP3_STRING_SIZE];
  char * response;
  int r;

  if (f->pop3_state != POP3_STATE_TRANSACTION)
    return MAILPOP3_ERROR_BAD_STATE;

  snprintf(command, POP3_STRING_SIZE, "NOOP\r\n");
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  parse_response(f, response);

  return MAILPOP3_NO_ERROR;
}

int mailpop3_rset(mailpop3 * f)
{
  char command[POP3_STRING_SIZE];
  char * response;
  int r;

  if (f->pop3_state != POP3_STATE_TRANSACTION)
    return MAILPOP3_ERROR_BAD_STATE;

  snprintf(command, POP3_STRING_SIZE, "RSET\r\n");
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  parse_response(f, response);

  if (f->pop3_msg_tab != NULL) {
    mailpop3_msg_info_tab_reset(f->pop3_msg_tab);
    f->pop3_deleted_count = 0;
  }

  return MAILPOP3_NO_ERROR;
}



static int read_capa_resp(mailpop3 * f, clist ** result);

int mailpop3_capa(mailpop3 * f, clist ** result)
{
  clist * capa_list;
  char command[POP3_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, POP3_STRING_SIZE, "CAPA\r\n");
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);

  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_CAPA_NOT_SUPPORTED;
  
  capa_list = NULL;
  r = read_capa_resp(f, &capa_list);
  if (r != MAILPOP3_NO_ERROR)
    return r;

  * result = capa_list;

  return MAILPOP3_NO_ERROR;
}

void mailpop3_capa_resp_free(clist * capa_list)
{
  clist_foreach(capa_list, (clist_func) mailpop3_capa_free, NULL);
  clist_free(capa_list);
}

int mailpop3_stls(mailpop3 * f)
{
  char command[POP3_STRING_SIZE];
  int r;
  char * response;

  snprintf(command, POP3_STRING_SIZE, "STLS\r\n");
  r = send_command(f, command);
  if (r == -1)
    return MAILPOP3_ERROR_STREAM;

  response = read_line(f);
  if (response == NULL)
    return MAILPOP3_ERROR_STREAM;
  r = parse_response(f, response);

  if (r != RESPONSE_OK)
    return MAILPOP3_ERROR_STLS_NOT_SUPPORTED;
  
  return MAILPOP3_NO_ERROR;
}
























#define RESP_OK_STR "+OK"
#define RESP_ERR_STR "-ERR"
#define RESP_AUTH_CONT_STR "+"


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


static int parse_response(mailpop3 * f, char * response)
{
  char * msg;
  
  if (response == NULL) {
    f->pop3_response = NULL;
    return RESPONSE_ERR;
  }

  if (strncmp(response, RESP_OK_STR, strlen(RESP_OK_STR)) == 0) {

    if (response[strlen(RESP_OK_STR)] == ' ')
      msg = response + strlen(RESP_OK_STR) + 1;
    else
      msg = response + strlen(RESP_OK_STR);
    
    if (mmap_string_assign(f->pop3_response_buffer, msg))
      f->pop3_response = f->pop3_response_buffer->str;
    else
      f->pop3_response = NULL;

    return RESPONSE_OK;
  }
  else if (strncmp(response, RESP_ERR_STR, strlen(RESP_ERR_STR)) == 0) {

    if (response[strlen(RESP_ERR_STR)] == ' ')
      msg = response + strlen(RESP_ERR_STR) + 1;
    else
      msg = response + strlen(RESP_ERR_STR);
    
    if (mmap_string_assign(f->pop3_response_buffer, msg))
      f->pop3_response = f->pop3_response_buffer->str;
    else
      f->pop3_response = NULL;
  }

  f->pop3_response = NULL;
  return RESPONSE_ERR;
}





static int parse_auth(mailpop3 * f, char * response)
{
  char * msg;
  
  if (response == NULL) {
    f->pop3_response = NULL;
    return RESPONSE_ERR;
  }

  if ((strncmp(response, RESP_AUTH_CONT_STR, strlen(RESP_AUTH_CONT_STR)) == 0) &&
      (strncmp(response, RESP_OK_STR, strlen(RESP_OK_STR)) != 0)) {
    
    if (response[strlen(RESP_AUTH_CONT_STR)] == ' ')
      msg = response + strlen(RESP_AUTH_CONT_STR) + 1;
    else
      msg = response + strlen(RESP_AUTH_CONT_STR);
    
    if (mmap_string_assign(f->pop3_response_buffer, msg))
      f->pop3_response = f->pop3_response_buffer->str;
    else
      f->pop3_response = NULL;

    return RESPONSE_AUTH_CONT;
  }
  else {
    return parse_response(f, response);
  }
}


static int read_list(mailpop3 * f, carray ** result)
{
  unsigned int index;
  uint32_t size;
  carray * msg_tab;
  struct mailpop3_msg_info * msg;
  char * line;

  msg_tab = carray_new(128);
  if (msg_tab == NULL)
    goto err;

  while (1) {
    line = read_line(f);
    if (line == NULL)
      goto free_list;

    if (mailstream_is_end_multiline(line))
      break;

    index = strtol(line, &line, 10);

    if (!parse_space(&line))
      continue;

    size = strtol(line, &line, 10);
    
    msg = mailpop3_msg_info_new(index, size, NULL);
    if (msg == NULL)
      goto free_list;

    if (carray_count(msg_tab) < index) {
      int r;

      r = carray_set_size(msg_tab, index);
      if (r == -1)
	goto free_list;
    }

    carray_set(msg_tab, index - 1, msg);
  }

  * result = msg_tab;
  
  return MAILPOP3_NO_ERROR;

 free_list:
  mailpop3_msg_info_tab_free(msg_tab);
 err:
  return MAILPOP3_ERROR_STREAM;
}



static int read_uidl(mailpop3 * f, carray * msg_tab)
{
  unsigned int index;
  struct mailpop3_msg_info * msg;
  char * line;

  while (1) {
    char * uidl;

    line = read_line(f);
    if (line == NULL)
      goto err;
    
    if (mailstream_is_end_multiline(line))
      break;
    
    index = strtol(line, &line, 10);

    if (!parse_space(&line))
      continue;

    uidl = strdup(line);
    if (uidl == NULL)
      continue;

    if (index > carray_count(msg_tab)) {
      free(uidl);
      continue;
    }

    msg = carray_get(msg_tab, index - 1);
    if (msg == NULL) {
      free(uidl);
      continue;
    }

    msg->msg_uidl = uidl;
  }
  
  return MAILPOP3_NO_ERROR;

 err:
  return MAILPOP3_ERROR_STREAM;
}



static int read_capa_resp(mailpop3 * f, clist ** result)
{
  char * line;
  int res;
  clist * list;
  int r;
  char * name;
  clist * param_list;

  list = clist_new();
  if (list == NULL) {
    res = MAILPOP3_NO_ERROR;
    goto err;
  }

  while (1) {
    char * next_token;
    char * param;
    struct mailpop3_capa * capa;

    line = read_line(f);
    if (line == NULL) {
      res = MAILPOP3_ERROR_STREAM;
      goto free_list;
    }
    
    if (mailstream_is_end_multiline(line))
      break;

    next_token = cut_token(line);
    name = strdup(line);
    if (name == NULL) {
      res = MAILPOP3_ERROR_MEMORY;
      goto free_list;
    }

    param_list = clist_new();
    if (param_list == NULL) {
      res = MAILPOP3_ERROR_MEMORY;
      goto free_capa_name;
    }

    while (next_token != NULL) {
      line = next_token;
      next_token = cut_token(line);
      param = strdup(line);
      if (param == NULL) {
	res = MAILPOP3_ERROR_MEMORY;
	goto free_param_list;
      }
      r = clist_append(param_list, param);
      if (r < 0) {
	free(param);
	res = MAILPOP3_ERROR_MEMORY;
	goto free_param_list;
      }
    }

    capa = mailpop3_capa_new(name, param_list);
    if (capa == NULL) {
      res = MAILPOP3_ERROR_MEMORY;
      goto free_param_list;
    }

    r = clist_append(list, capa);
    if (r < 0) {
      mailpop3_capa_free(capa);
      res = MAILPOP3_ERROR_MEMORY;
      goto free_list;
    }
  }

  * result = list;
  
  return MAILPOP3_NO_ERROR;

 free_param_list:
  clist_foreach(param_list, (clist_func) free, NULL);
  clist_free(param_list);
 free_capa_name:
  free(name);
 free_list:
  clist_foreach(list, (clist_func) mailpop3_capa_free, NULL);
  clist_free(list);
 err:
  return res;
}



static char * read_line(mailpop3 * f)
{
  return mailstream_read_line_remove_eol(f->pop3_stream, f->pop3_stream_buffer);
}

static char * read_multiline(mailpop3 * f, size_t size,
			      MMAPString * multiline_buffer)
{
  return mailstream_read_multiline(f->pop3_stream, size,
				   f->pop3_stream_buffer, multiline_buffer,
				   f->pop3_progr_rate, f->pop3_progr_fun);
}

static int send_command(mailpop3 * f, char * command)
{
  ssize_t r;

  r = mailstream_write(f->pop3_stream, command, strlen(command));
  if (r == -1)
    return -1;

  r = mailstream_flush(f->pop3_stream);
  if (r == -1)
    return -1;

  return 0;
}



#ifdef USE_SASL
static int sasl_getsimple(void * context, int id,
    const char ** result, unsigned * len)
{
  mailpop3 * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_USER:
    if (result != NULL)
      * result = session->pop3_sasl.sasl_login;
    if (len != NULL)
      * len = strlen(session->pop3_sasl.sasl_login);
    return SASL_OK;
    
  case SASL_CB_AUTHNAME:
    if (result != NULL)
      * result = session->pop3_sasl.sasl_auth_name;
    if (len != NULL)
      * len = strlen(session->pop3_sasl.sasl_auth_name);
    return SASL_OK;
  }
  
  return SASL_FAIL;
}

static int sasl_getsecret(sasl_conn_t * conn, void * context, int id,
    sasl_secret_t ** psecret)
{
  mailpop3 * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_PASS:
    if (psecret != NULL)
      * psecret = session->pop3_sasl.sasl_secret;
    return SASL_OK;
  }
  
  return SASL_FAIL;
}

static int sasl_getrealm(void * context, int id,
    const char ** availrealms,
    const char ** result)
{
  mailpop3 * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_GETREALM:
    if (result != NULL)
      * result = session->pop3_sasl.sasl_realm;
    return SASL_OK;
  }
  
  return SASL_FAIL;
}
#endif

int mailpop3_auth(mailpop3 * f, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
#ifdef USE_SASL
  int r;
  char command[POP3_STRING_SIZE];
  sasl_callback_t sasl_callback[5];
  const char * sasl_out;
  unsigned sasl_out_len;
  const char * mechusing;
  sasl_secret_t * secret;
  int res;
  size_t len;
  char * encoded;
  unsigned int encoded_len;
  unsigned int max_encoded;
  
  sasl_callback[0].id = SASL_CB_GETREALM;
  sasl_callback[0].proc =  sasl_getrealm;
  sasl_callback[0].context = f;
  sasl_callback[1].id = SASL_CB_USER;
  sasl_callback[1].proc =  sasl_getsimple;
  sasl_callback[1].context = f;
  sasl_callback[2].id = SASL_CB_AUTHNAME;
  sasl_callback[2].proc =  sasl_getsimple;
  sasl_callback[2].context = f; 
  sasl_callback[3].id = SASL_CB_PASS;
  sasl_callback[3].proc =  sasl_getsecret;
  sasl_callback[3].context = f;
  sasl_callback[4].id = SASL_CB_LIST_END;
  sasl_callback[4].proc =  NULL;
  sasl_callback[4].context = NULL;
  
  len = strlen(password);
  secret = malloc(sizeof(* secret) + len);
  if (secret == NULL) {
    res = MAILPOP3_ERROR_MEMORY;
    goto err;
  }
  secret->len = len;
  memcpy(secret->data, password, len + 1);
  
  f->pop3_sasl.sasl_server_fqdn = server_fqdn;
  f->pop3_sasl.sasl_login = login;
  f->pop3_sasl.sasl_auth_name = auth_name;
  f->pop3_sasl.sasl_password = password;
  f->pop3_sasl.sasl_realm = realm;
  f->pop3_sasl.sasl_secret = secret;
  
  /* init SASL */
  if (f->pop3_sasl.sasl_conn != NULL) {
    sasl_dispose((sasl_conn_t **) &f->pop3_sasl.sasl_conn);
    f->pop3_sasl.sasl_conn = NULL;
  }
  else {
    mailsasl_ref();
  }
  
  r = sasl_client_new("pop", server_fqdn,
      local_ip_port, remote_ip_port, sasl_callback, 0,
      (sasl_conn_t **) &f->pop3_sasl.sasl_conn);
  if (r != SASL_OK) {
    res = MAILPOP3_ERROR_BAD_USER;
    goto free_secret;
  }
  
  r = sasl_client_start(f->pop3_sasl.sasl_conn,
      auth_type, NULL, &sasl_out, &sasl_out_len, &mechusing);
  if ((r != SASL_CONTINUE) && (r != SASL_OK)) {
    res = MAILPOP3_ERROR_BAD_USER;
    goto free_sasl_conn;
  }
  
  snprintf(command, POP3_STRING_SIZE, "AUTH %s\r\n", auth_type);
  
  r = send_command(f, command);
  if (r == -1) {
    res = MAILPOP3_ERROR_STREAM;
    goto free_sasl_conn;
  }
  
  while (1) {
    char * response;
    
    response = read_line(f);
    
    r = parse_auth(f, response);
    switch (r) {
    case RESPONSE_OK:
      res = MAILPOP3_NO_ERROR;
      goto free_sasl_conn;
      
    case RESPONSE_ERR:
      res = MAILPOP3_ERROR_BAD_USER;
      goto free_sasl_conn;
    
    case RESPONSE_AUTH_CONT:
      {
        size_t response_len;
        char * decoded;
        unsigned int decoded_len;
        unsigned int max_decoded;
        int got_response;
        
        got_response = 1;
        if (* f->pop3_response == '\0')
          got_response = 0;
        
        if (got_response) {
          response_len = strlen(f->pop3_response);
          max_decoded = response_len * 3 / 4;
          decoded = malloc(max_decoded + 1);
          if (decoded == NULL) {
            res = MAILPOP3_ERROR_MEMORY;
            goto free_sasl_conn;
          }
          
          r = sasl_decode64(f->pop3_response, response_len,
              decoded, max_decoded + 1, &decoded_len);
          
          if (r != SASL_OK) {
            free(decoded);
            res = MAILPOP3_ERROR_MEMORY;
            goto free_sasl_conn;
          }
          
          r = sasl_client_step(f->pop3_sasl.sasl_conn,
              decoded, decoded_len, NULL, &sasl_out, &sasl_out_len);
          
          free(decoded);
          
          if ((r != SASL_CONTINUE) && (r != SASL_OK)) {
            res = MAILPOP3_ERROR_BAD_USER;
            goto free_sasl_conn;
          }
        }
        
        max_encoded = ((sasl_out_len + 2) / 3) * 4;
        encoded = malloc(max_encoded + 1);
        if (encoded == NULL) {
          res = MAILPOP3_ERROR_MEMORY;
          goto free_sasl_conn;
        }
        
        r = sasl_encode64(sasl_out, sasl_out_len,
            encoded, max_encoded + 1, &encoded_len);
        if (r != SASL_OK) {
          free(encoded);
          res = MAILPOP3_ERROR_MEMORY;
          goto free_sasl_conn;
        }
        
        snprintf(command, POP3_STRING_SIZE, "%s\r\n", encoded);
        r = send_command(f, command);
        
        free(encoded);
        
        if (r == -1) {
          res = MAILPOP3_ERROR_STREAM;
          goto free_sasl_conn;
        }
      }
      break;
    }
  }

  res = MAILPOP3_NO_ERROR;
  
 free_sasl_conn:
  sasl_dispose((sasl_conn_t **) &f->pop3_sasl.sasl_conn);
  f->pop3_sasl.sasl_conn = NULL;
  mailsasl_unref();
 free_secret:
  free(f->pop3_sasl.sasl_secret);
  f->pop3_sasl.sasl_secret = NULL;
 err:
  return res;
#else
  return MAILPOP3_ERROR_BAD_USER;
#endif
}
