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
 * $Id: maildriver_types.c,v 1.28 2006/06/16 09:23:38 smarinier Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildriver_types.h"
#include <time.h>
#include <stdlib.h>
#include "mailmessage.h"

LIBETPAN_EXPORT
struct mailmessage_list * mailmessage_list_new(carray * msg_tab)
{
  struct mailmessage_list * env_list;

  env_list = malloc(sizeof(* env_list));
  if (env_list == NULL)
    return NULL;

  env_list->msg_tab = msg_tab;

  return env_list;
}

LIBETPAN_EXPORT
void mailmessage_list_free(struct mailmessage_list * env_list)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(env_list->msg_tab, i);
    if (msg != NULL)
      mailmessage_free(msg);
  }
  carray_free(env_list->msg_tab);
  free(env_list);
}

LIBETPAN_EXPORT
struct mail_list * mail_list_new(clist * list)
{
  struct mail_list * resp;

  resp = malloc(sizeof(* resp));
  if (resp == NULL)
    return NULL;
  resp->mb_list = list;
  
  return resp;
}

LIBETPAN_EXPORT
void mail_list_free(struct mail_list * resp)
{
  clist_foreach(resp->mb_list, (clist_func) free, NULL);
  clist_free(resp->mb_list);
  free(resp);
}

static int32_t mailimf_date_time_to_int(struct mailimf_date_time * date)
{
  return date->dt_year * 12 * 30 * 24 * 60 * 60 +
    date->dt_month * 30 * 24 * 60 * 60  + date->dt_day * 24 * 60 * 60 +
    (date->dt_hour - date->dt_zone) * 60 * 60 +
    date->dt_min * 60 + date->dt_sec;
}

LIBETPAN_EXPORT
int32_t mailimf_date_time_comp(struct mailimf_date_time * date1,
    struct mailimf_date_time * date2)
{
  return mailimf_date_time_to_int(date1) - mailimf_date_time_to_int(date2);
}







#if 0
struct mail_search_key *
mail_search_key_new(int sk_type,
		    char * sk_bcc,
		    struct mailimf_date_time * sk_before,
		    char * sk_body,
		    char * sk_cc,
		    char * sk_from,
		    struct mailimf_date_time * sk_on,
		    struct mailimf_date_time * sk_since,
		    char * sk_subject,
		    char * sk_text,
		    char * sk_to,
		    char * sk_header_name,
		    char * sk_header_value,
		    size_t sk_larger,
		    struct mail_search_key * sk_not,
		    struct mail_search_key * sk_or1,
		    struct mail_search_key * sk_or2,
		    size_t sk_smaller,
		    clist * sk_multiple)
{
  struct mail_search_key * key;

  key = malloc(sizeof(* key));
  if (key == NULL)
    return NULL;

  key->sk_type = sk_type;
  key->sk_bcc = sk_bcc;
  key->sk_before = sk_before;
  key->sk_body = sk_body;
  key->sk_cc = sk_cc;
  key->sk_from = sk_from;
  key->sk_on = sk_on;
  key->sk_since = sk_since;
  key->sk_subject = sk_subject;
  key->sk_text = sk_text;
  key->sk_to = sk_to;
  key->sk_header_name = sk_header_name;
  key->sk_header_value = sk_header_value;
  key->sk_larger = sk_larger;
  key->sk_not = sk_not;
  key->sk_or1 = sk_or1;
  key->sk_or2 = sk_or2;
  key->sk_smaller = sk_smaller;
  key->sk_multiple = sk_multiple;

  return key;
}


void mail_search_key_free(struct mail_search_key * key)
{
  if (key->sk_bcc)
    free(key->sk_bcc);
  if (key->sk_before)
    mailimf_date_time_free(key->sk_before);
  if (key->sk_body)
    free(key->sk_body);
  if (key->sk_cc)
    free(key->sk_cc);
  if (key->sk_from)
    free(key->sk_from);
  if (key->sk_on)
    mailimf_date_time_free(key->sk_on);
  if (key->sk_since)
    mailimf_date_time_free(key->sk_since);
  if (key->sk_subject)
    free(key->sk_subject);
  if (key->sk_text)
    free(key->sk_text);
  if (key->sk_to)
    free(key->sk_to);
  if (key->sk_header_name)
    free(key->sk_header_name);
  if (key->sk_header_value)
    free(key->sk_header_value);
  if (key->sk_not)
    mail_search_key_free(key->sk_not);
  if (key->sk_or1)
    mail_search_key_free(key->sk_or1);
  if (key->sk_or2)
    mail_search_key_free(key->sk_or2);
  if (key->sk_multiple) {
    clist_foreach(key->sk_multiple, (clist_func) mail_search_key_free, NULL);
    clist_free(key->sk_multiple);
  }

  free(key);
}


struct mail_search_result * mail_search_result_new(clist * list)
{
  struct mail_search_result * search_result;

  search_result = malloc(sizeof(* search_result));
  if (search_result == NULL)
    return NULL;
  search_result->list = list;
  
  return search_result;
}

void mail_search_result_free(struct mail_search_result * search_result)
{
  clist_foreach(search_result->list, (clist_func) free, NULL);
  clist_free(search_result->list);
  free(search_result);
}
#endif

struct error_message {
  int code;
  char * message;
};

static struct error_message message_tab[] = {
{ MAIL_NO_ERROR, "no error" },
{ MAIL_NO_ERROR_AUTHENTICATED, "no error - authenticated" },
{ MAIL_NO_ERROR_NON_AUTHENTICATED, "no error - not authenticated" },
{ MAIL_ERROR_NOT_IMPLEMENTED, "not implemented" },
{ MAIL_ERROR_UNKNOWN, "unknown"},
{ MAIL_ERROR_CONNECT, "connect"},
{ MAIL_ERROR_BAD_STATE, "bad state"},
{ MAIL_ERROR_FILE, "file error - file could not be accessed" },
{ MAIL_ERROR_STREAM, "stream error - socket could not be read or written" },
{ MAIL_ERROR_LOGIN, "login error" },
{ MAIL_ERROR_CREATE, "create error" },
{ MAIL_ERROR_DELETE, /* 10 */ "delete error" },
{ MAIL_ERROR_LOGOUT, "logout error" },
{ MAIL_ERROR_NOOP, "noop error" },
{ MAIL_ERROR_RENAME, "rename error" },
{ MAIL_ERROR_CHECK, "check error" },
{ MAIL_ERROR_EXAMINE, "examine error" },
{ MAIL_ERROR_SELECT, "select error - folder does not exist" },
{ MAIL_ERROR_MEMORY, "not enough memory" },
{ MAIL_ERROR_STATUS, "status error" },
{ MAIL_ERROR_SUBSCRIBE, "subscribe error" },
{ MAIL_ERROR_UNSUBSCRIBE, /* 20 */ "unsubscribe error" },
{ MAIL_ERROR_LIST, "list error" },
{ MAIL_ERROR_LSUB, "lsub error" },
{ MAIL_ERROR_APPEND, "append error - mail could not be appended" },
{ MAIL_ERROR_COPY, "copy error" },
{ MAIL_ERROR_FETCH, "fetch error" },
{ MAIL_ERROR_STORE, "store error" },
{ MAIL_ERROR_SEARCH, "search error" },
{ MAIL_ERROR_DISKSPACE, " error: not enough diskspace" },
{ MAIL_ERROR_MSG_NOT_FOUND, "message not found" },
{ MAIL_ERROR_PARSE, /* 30 */ "parse error" },
{ MAIL_ERROR_INVAL, "invalid parameter for the function" },
{ MAIL_ERROR_PART_NOT_FOUND, "mime part of the message is not found" },
{ MAIL_ERROR_REMOVE, "remove error - the message did not exist" },
{ MAIL_ERROR_FOLDER_NOT_FOUND, "folder not found" },
{ MAIL_ERROR_MOVE, "move error" },
{ MAIL_ERROR_STARTTLS, "starttls error" },
{ MAIL_ERROR_CACHE_MISS, "mail cache missed" },
{ MAIL_ERROR_NO_TLS, "no starttls" },
{ MAIL_ERROR_EXPUNGE, "expunge error" },
{ MAIL_ERROR_PROTOCOL, "protocol error - server did not respect the protocol" },
{ MAIL_ERROR_CAPABILITY, "capability error" },
{ MAIL_ERROR_CLOSE, "close error" },
{ MAIL_ERROR_FATAL, "fatal error" },
{ MAIL_ERROR_READONLY, "mailbox is readonly" },
{ MAIL_ERROR_NO_APOP, "pop3 error - no apop" },
{ MAIL_ERROR_COMMAND_NOT_SUPPORTED, "nntp error - command not supported" },
{ MAIL_ERROR_NO_PERMISSION, "nntp error - no permission" },
{ MAIL_ERROR_PROGRAM_ERROR, "nntp error - program error" },
{ MAIL_ERROR_SUBJECT_NOT_FOUND, "internal threading error - subject not found" }};

LIBETPAN_EXPORT
const char * maildriver_strerror(int err)
{
  int count;
  int i;

  count = sizeof(message_tab) / sizeof(struct error_message);

  for(i = 0 ; i < count ; i++) {
    if (message_tab[i].code == err) {
      return message_tab[i].message;
    }
  }

  return "unknown error";
}

LIBETPAN_EXPORT
struct mail_flags * mail_flags_new_empty(void)
{
  struct mail_flags * flags;

  flags = malloc(sizeof(* flags));
  if (flags == NULL)
    goto err;

  flags->fl_flags = MAIL_FLAG_NEW;
  flags->fl_extension = clist_new();
  if (flags->fl_extension == NULL)
    goto free;

  return flags;

 free:
  free(flags);
 err:
  return NULL;
}

LIBETPAN_EXPORT
struct mail_flags * mail_flags_new(uint32_t fl_flags, clist * fl_extension)
{
  struct mail_flags * flags;

  flags = malloc(sizeof(* flags));
  if (flags == NULL)
    goto err;

  flags->fl_flags = fl_flags;
  flags->fl_extension = fl_extension;
  
  return flags;
  
err:
  return NULL;
}

LIBETPAN_EXPORT
void mail_flags_free(struct mail_flags * flags)
{
  clist_foreach(flags->fl_extension, (clist_func) free, NULL);
  clist_free(flags->fl_extension);
  free(flags);
}

LIBETPAN_EXPORT
void *libetpan_malloc(size_t length) {
	return malloc( length);
}

LIBETPAN_EXPORT
void libetpan_free(void* data) {
	free( data);
}
