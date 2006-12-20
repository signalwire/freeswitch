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
 * $Id: maildriver_tools.c,v 1.34 2006/06/07 15:10:01 smarinier Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildriver_tools.h"

#include "libetpan-config.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifdef _MSC_VER
#include "win_etpan.h"
#else
#	include <dirent.h>
#endif
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include "maildriver.h"
#include "mailmessage.h"
#include "mailstream.h"
#include "mailmime.h"
#include "mail_cache_db.h"

/* ********************************************************************* */
/* tools */


int
maildriver_generic_get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list)
{
  int r;
  unsigned i;
#if 0
  uint32_t j;
  uint32_t last_msg;

  last_msg = 0;
#endif

#if 0
  j = 0;
  i = 0;
  while (i < env_list->tab->len) {
    mailmessage * msg;
    uint32_t index;
    
    msg = carray_get(env_list->tab, i);

    index = msg->index;

    if (msg->fields == NULL) {
      struct mailimf_fields * fields;

      r = mailmessage_fetch_envelope(msg, &fields);
      if (r != MAIL_NO_ERROR) {
	/* do nothing */
      }
      else {
	msg->fields = fields;
	
	carray_set(env_list->tab, j, msg);
	
	j ++;
	last_msg = i + 1;
      }
      mailmessage_flush(msg);
    }
    else {
      j ++;
      last_msg = i + 1;
    }

    i ++;
  }

  for(i = last_msg ; i < env_list->tab->len ; i ++) {
    mailmessage_free(carray_get(env_list->tab, i));
    carray_set(env_list->tab, i, NULL);
  }

  r = carray_set_size(env_list->tab, j);
  if (r < 0)
    return MAIL_ERROR_MEMORY;
#endif
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(env_list->msg_tab, i);
    
    if (msg->msg_fields == NULL) {
      struct mailimf_fields * fields;

      r = mailmessage_fetch_envelope(msg, &fields);
      if (r != MAIL_NO_ERROR) {
	/* do nothing */
      }
      else {
	msg->msg_fields = fields;
      }
      mailmessage_flush(msg);
    }
  }

  return MAIL_NO_ERROR;
}


#if 0
static int is_search_header_only(struct mail_search_key * key)
{
  clistiter * cur;
  int result;

  switch (key->type) {
  case MAIL_SEARCH_KEY_ANSWERED:
  case MAIL_SEARCH_KEY_BCC:
  case MAIL_SEARCH_KEY_BEFORE:
  case MAIL_SEARCH_KEY_CC:
  case MAIL_SEARCH_KEY_DELETED:
  case MAIL_SEARCH_KEY_FLAGGED:
  case MAIL_SEARCH_KEY_FROM:
  case MAIL_SEARCH_KEY_NEW:
  case MAIL_SEARCH_KEY_OLD:
  case MAIL_SEARCH_KEY_ON:
  case MAIL_SEARCH_KEY_RECENT:
  case MAIL_SEARCH_KEY_SEEN:
  case MAIL_SEARCH_KEY_SINCE:
  case MAIL_SEARCH_KEY_SUBJECT:
  case MAIL_SEARCH_KEY_TO:
  case MAIL_SEARCH_KEY_UNANSWERED:
  case MAIL_SEARCH_KEY_UNDELETED:
  case MAIL_SEARCH_KEY_UNFLAGGED:
  case MAIL_SEARCH_KEY_UNSEEN:
  case MAIL_SEARCH_KEY_HEADER:
  case MAIL_SEARCH_KEY_LARGER:
  case MAIL_SEARCH_KEY_NOT:
  case MAIL_SEARCH_KEY_SMALLER:
  case MAIL_SEARCH_KEY_ALL:
    return TRUE;

  case MAIL_SEARCH_KEY_BODY:
  case MAIL_SEARCH_KEY_TEXT:
    return FALSE;

  case MAIL_SEARCH_KEY_OR:
    return (is_search_header_only(key->or1) &&
	    is_search_header_only(key->or2));

  case MAIL_SEARCH_KEY_MULTIPLE:
    result = TRUE;
    for (cur = clist_begin(key->multiple) ; cur != NULL ;
         cur = clist_next(cur))
      result = result && is_search_header_only(clist_content(cur));
    return result;

  default:
    return TRUE;
  }
}

static int match_header(struct mailimf_fields * fields,
    char * name, char * value)
{
  clistiter * cur;

  for(cur = clist_begin(fields->list) ; cur != NULL ;
      cur = clist_content(cur)) {
    struct mailimf_field * field;
    struct mailimf_optional_field * opt_field;

    field = clist_content(cur);
    opt_field = field->optional_field;
    if ((char) toupper((unsigned char) opt_field->name[0]) ==
        (char) toupper((unsigned char) name[0])) {
      if (strcasecmp(opt_field->name, name) == 0)
	if (strstr(opt_field->value, value) != NULL)
	  return TRUE;
    }
  }
  return FALSE;
}

static int comp_date(struct mailimf_fields * fields,
    struct mailimf_date_time * ref_date)
{
  clistiter * cur;
  struct mailimf_date_time * date;
  int r;

  date = NULL;
  for(cur = clist_begin(fields->list) ; cur != NULL ;
      cur = clist_content(cur)) {
    struct mailimf_field * field;
    struct mailimf_optional_field * opt_field;

    field = clist_content(cur);
    opt_field = field->optional_field;
    if ((char) toupper((unsigned char) opt_field->name[0]) == 'D') {
      if (strcasecmp(opt_field->name, "Date") == 0) {
	size_t cur_token;

	cur_token = 0;
	r = mailimf_date_time_parse(opt_field->value, strlen(opt_field->value),
				    &cur_token, &date);
	if (r == MAILIMF_NO_ERROR)
	  break;
	else if (r == MAILIMF_ERROR_PARSE) {
	  /* do nothing */
	}
	else
	  break;
      }
    }
  }

  if (date == NULL)
    return 0;

  return mailimf_date_time_comp(date, ref_date);
}

static int match_messages(char * message,
    size_t size,
    struct mailimf_fields * fields,
    int32_t flags,
    char * charset,
    struct mail_search_key * key)
{
  clistiter * cur;
  size_t length;
  size_t cur_token;
  int r;

  switch (key->type) {

    /* flags */
  case MAIL_SEARCH_KEY_ANSWERED:
    return ((flags & MAIL_FLAG_ANSWERED) != 0);

  case MAIL_SEARCH_KEY_FLAGGED:
    return ((flags & MAIL_FLAG_FLAGGED) != 0);

  case MAIL_SEARCH_KEY_DELETED:
    return ((flags & MAIL_FLAG_DELETED) != 0);

  case MAIL_SEARCH_KEY_RECENT:
    return ((flags & MAIL_FLAG_NEW) != 0) &&
      ((flags & MAIL_FLAG_SEEN) == 0);

  case MAIL_SEARCH_KEY_SEEN:
    return ((flags & MAIL_FLAG_SEEN) != 0);

  case MAIL_SEARCH_KEY_NEW:
    return ((flags & MAIL_FLAG_NEW) != 0);

  case MAIL_SEARCH_KEY_OLD:
    return ((flags & MAIL_FLAG_NEW) == 0);

  case MAIL_SEARCH_KEY_UNANSWERED:
    return ((flags & MAIL_FLAG_ANSWERED) == 0);

  case MAIL_SEARCH_KEY_UNDELETED:
    return ((flags & MAIL_FLAG_DELETED) == 0);

  case MAIL_SEARCH_KEY_UNFLAGGED:
    return ((flags & MAIL_FLAG_FLAGGED) == 0);

  case MAIL_SEARCH_KEY_UNSEEN:
    return ((flags & MAIL_FLAG_SEEN) == 0);

    /* headers */
  case MAIL_SEARCH_KEY_BCC:
    return match_header(fields, "Bcc", key->bcc);

  case MAIL_SEARCH_KEY_CC:
    return match_header(fields, "Cc", key->cc);

  case MAIL_SEARCH_KEY_FROM:
    return match_header(fields, "From", key->from);

  case MAIL_SEARCH_KEY_SUBJECT:
    return match_header(fields, "Subject", key->subject);

  case MAIL_SEARCH_KEY_TO:
    return match_header(fields, "To", key->to);

  case MAIL_SEARCH_KEY_HEADER:
    return match_header(fields, key->header_name, key->header_value);

    /* date */
  case MAIL_SEARCH_KEY_BEFORE:
    return (comp_date(fields, key->before) <= 0);

  case MAIL_SEARCH_KEY_ON:
    return (comp_date(fields, key->before) == 0);

  case MAIL_SEARCH_KEY_SINCE:
    return (comp_date(fields, key->before) >= 0);
    
    /* boolean */
  case MAIL_SEARCH_KEY_NOT:
    return (!match_messages(message, size, fields, flags, charset, key->not));
  case MAIL_SEARCH_KEY_OR:
    return (match_messages(message, size, fields, flags, charset, key->or1) ||
	    match_messages(message, size, fields, flags, charset, key->or2));

  case MAIL_SEARCH_KEY_MULTIPLE:
    for(cur = clist_begin(key->multiple) ; cur != NULL ;
        cur = clist_next(cur)) {
      if (!match_messages(message, size, fields, flags, charset,
              clist_content(cur)))
	return FALSE;
    }

    return TRUE;

    /* size */
  case MAIL_SEARCH_KEY_SMALLER:
    return (size <= key->smaller);

  case MAIL_SEARCH_KEY_LARGER:
    return (size >= key->larger);

  case MAIL_SEARCH_KEY_BODY:
    length = strlen(message);

    cur_token = 0;
    while (1) {
      r = mailimf_ignore_field_parse(message, length, &cur_token);
      if (r == MAILIMF_NO_ERROR) {
	/* do nothing */
      }
      else
	break;
    }

    return (strstr(message + cur_token, key->body) != NULL);

  case MAIL_SEARCH_KEY_TEXT:
    return (strstr(message, key->body) != NULL);
    
  case MAIL_SEARCH_KEY_ALL:
  default:
    return TRUE;
  }
}

int maildriver_generic_search_messages(mailsession * session, char * charset,
    struct mail_search_key * key,
    struct mail_search_result ** result)
{
  int header;
  clist * list;
  struct mail_search_result * search_result;
  int r;
  struct mailmessage_list * env_list;
  int res;
  unsigned int i;

  header = is_search_header_only(key);

  r = mailsession_get_messages_list(session, &env_list);
  if (r != MAIL_NO_ERROR)
    return r;

  list = NULL;
  for(i = 0 ; i < carray_count(env_list->tab) ; i ++) {
    char * message;
    size_t length;
    struct mail_info * info;
    uint32_t flags;
    struct mailimf_fields * fields;
    size_t cur_token;

    info = carray_get(env_list->tab, i);

    if (!header) {
      r = mailsession_fetch_message(session, info->index, &message, &length);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      cur_token = 0;
      r = mailimf_optional_fields_parse(message, length,
					&cur_token, &fields);
      if (r != MAILIMF_NO_ERROR) {
	res = MAIL_ERROR_PARSE;
	goto free_list;
      }
    }
    else {
      char * msg_header;
      int r;
      size_t cur_token;
      size_t header_len;

      r = mailsession_fetch_message_header(session, info->index, &msg_header,
					   &header_len);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      message = NULL;
      cur_token = 0;
      r = mailimf_optional_fields_parse(msg_header, header_len,
					&cur_token, &fields);
      if (r != MAILIMF_NO_ERROR) {
	res = MAIL_ERROR_PARSE;
	goto free_list;
      }

      mailsession_fetch_result_free(session, msg_header);
    }

    r = mailsession_get_message_flags(session, info->index, &flags);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }

    if (match_messages(message, info->size, fields, flags,
		       charset, key)) {
      uint32_t * pnum;

      pnum = malloc(sizeof(* pnum));
      if (pnum == NULL) {
	if (message != NULL)
	  mailsession_fetch_result_free(session, message);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      * pnum = info->index;

      r = clist_append(list, pnum);
      if (r < 0) {
	free(pnum);
	if (message != NULL)
	  mailsession_fetch_result_free(session, message);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }

    if (message != NULL)
      mailsession_fetch_result_free(session, message);
  }

  search_result =  mail_search_result_new(list);
  if (search_result == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = search_result;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) free, NULL);
  clist_free(list);
  mailmessage_list_free(env_list);
  return res;
}
#endif

#if 0
int maildriver_generic_search_messages(mailsession * session, char * charset,
    struct mail_search_key * key,
    struct mail_search_result ** result)
{
  return MAIL_ERROR_NOT_IMPLEMENTED;
}
#endif

int
maildriver_env_list_to_msg_list(struct mailmessage_list * env_list,
    clist ** result)
{
  clist * msg_list;
  int r;
  int res;
  unsigned int i;

  msg_list = clist_new();
  if (msg_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields == NULL) {
      uint32_t * pindex;

      pindex = malloc(sizeof(* pindex));
      if (pindex == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_msg_list;
      }

      * pindex = msg->msg_index;

      r = clist_append(msg_list, pindex);
      if (r < 0) {
	free(pindex);
	res = MAIL_ERROR_MEMORY;
	goto free_msg_list;
      }

    }
  }

  * result = msg_list;

  return MAIL_NO_ERROR;

 free_msg_list:
  clist_foreach(msg_list, (clist_func) free, NULL);
  clist_free(msg_list);
 err:
  return res;
}


int
maildriver_env_list_to_msg_list_no_flags(struct mailmessage_list * env_list,
    clist ** result)
{
  clist * msg_list;
  int r;
  int res;
  unsigned int i;

  msg_list = clist_new();
  if (msg_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;

    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_flags == NULL) {
      uint32_t * pindex;

      pindex = malloc(sizeof(* pindex));
      if (pindex == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_msg_list;
      }

      * pindex = msg->msg_index;

      r = clist_append(msg_list, pindex);
      if (r < 0) {
	free(pindex);
	res = MAIL_ERROR_MEMORY;
	goto free_msg_list;
      }

    }
  }

  * result = msg_list;

  return MAIL_NO_ERROR;

 free_msg_list:
  clist_foreach(msg_list, (clist_func) free, NULL);
  clist_free(msg_list);
 err:
  return res;
}



int maildriver_imf_error_to_mail_error(int error)
{
  switch (error) {
  case MAILIMF_NO_ERROR:
    return MAIL_NO_ERROR;

  case MAILIMF_ERROR_PARSE:
    return MAIL_ERROR_PARSE;

  case MAILIMF_ERROR_MEMORY:
    return MAIL_ERROR_MEMORY;

  case MAILIMF_ERROR_INVAL:
    return MAIL_ERROR_INVAL;

  case MAILIMF_ERROR_FILE:
    return MAIL_ERROR_FILE;
    
  default:
    return MAIL_ERROR_INVAL;
  }
}

char * maildriver_quote_mailbox(const char * mb)
{
  MMAPString * gstr;
  char * str;

  gstr = mmap_string_new("");
  if (gstr == NULL)
    return NULL;
  
  while (* mb != 0) {
    char hex[3];

    if (((* mb >= 'a') && (* mb <= 'z')) ||
	((* mb >= 'A') && (* mb <= 'Z')) ||
	((* mb >= '0') && (* mb <= '9')))
      mmap_string_append_c(gstr, * mb);
    else {
      if (mmap_string_append_c(gstr, '%') == NULL)
	goto free;
      snprintf(hex, 3, "%02x", (unsigned char) (* mb));
      if (mmap_string_append(gstr, hex) == NULL)
	goto free;
    }
    mb ++;
  }

  str = strdup(gstr->str);
  if (str == NULL)
    goto free;

  mmap_string_free(gstr);
  
  return str;

 free:
  mmap_string_free(gstr);
  return NULL;
}


int maildriver_cache_clean_up(struct mail_cache_db * cache_db_env,
    struct mail_cache_db * cache_db_flags,
    struct mailmessage_list * env_list)
{
  chash * hash_exist;
  int res;
  int r;
  char keyname[PATH_MAX];
  unsigned int i;
  
  /* flush cache */
  
  hash_exist = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  if (hash_exist == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    chashdatum key;
    chashdatum value;

    msg = carray_get(env_list->msg_tab, i);
    
    value.data = NULL;
    value.len = 0;
    
    if (cache_db_env != NULL) {
      snprintf(keyname, PATH_MAX, "%s-envelope", msg->msg_uid);
      
      key.data = keyname;
      key.len = strlen(keyname);
      r = chash_set(hash_exist, &key, &value, NULL);
      if (r < 0) {
        res = MAIL_ERROR_MEMORY;
        goto free;
      }
    }
        
    if (cache_db_flags != NULL) {
      snprintf(keyname, PATH_MAX, "%s-flags", msg->msg_uid);
      
      key.data = keyname;
      key.len = strlen(keyname);
      r = chash_set(hash_exist, &key, &value, NULL);
      if (r < 0) {
        res = MAIL_ERROR_MEMORY;
        goto free;
      }
    }
  }
  
  /* clean up */
  if (cache_db_env != NULL)
    mail_cache_db_clean_up(cache_db_env, hash_exist);
  if (cache_db_flags != NULL)
    mail_cache_db_clean_up(cache_db_flags, hash_exist);
  
  chash_free(hash_exist);
  
  return MAIL_NO_ERROR;

 free:
  chash_free(hash_exist);
 err:
  return res;
}

/*
  maildriver_message_cache_clean_up()
  
  remove files in cache_dir that does not correspond to a message.
  
  get_uid_from_filename() modifies the given filename so that it
  is a uid when returning from the function. If get_uid_from_filename()
  clears the content of file (set to empty string), this means that
  this file should not be deleted.
*/

int maildriver_message_cache_clean_up(char * cache_dir,
    struct mailmessage_list * env_list,
    void (* get_uid_from_filename)(char *))
{
  chash * hash_exist;
  DIR * d;
  char cached_filename[PATH_MAX];
  struct dirent * ent;
  char keyname[PATH_MAX];
  unsigned int i;
  int res;
  int r;
  
  /* remove files */
    
  hash_exist = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  if (hash_exist == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    chashdatum key;
    chashdatum value;
    
    msg = carray_get(env_list->msg_tab, i);
      
    key.data = msg->msg_uid;
    key.len = strlen(msg->msg_uid);
    value.data = NULL;
    value.len = 0;
    r = chash_set(hash_exist, &key, &value, NULL);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto free;
    }
  }
  
  d = opendir(cache_dir);
  while ((ent = readdir(d)) != NULL) {
    chashdatum key;
    chashdatum value;
    
    if (strcmp(ent->d_name, ".") == 0)
      continue;
    
    if (strcmp(ent->d_name, "..") == 0)
      continue;
      
    if (strstr(ent->d_name, ".db") != NULL)
      continue;
    
    strncpy(keyname, ent->d_name, sizeof(keyname));
    keyname[sizeof(keyname) - 1] = '\0';
    
    get_uid_from_filename(keyname);
    
    if (* keyname == '\0')
      continue;
    
    key.data = keyname;
    key.len = strlen(keyname);
    
    r = chash_get(hash_exist, &key, &value);
    if (r < 0) {
      snprintf(cached_filename, sizeof(cached_filename),
          "%s/%s", cache_dir, ent->d_name);
      unlink(cached_filename);
    }
  }
  closedir(d);
    
  chash_free(hash_exist);
  
  return MAIL_NO_ERROR;

 free:
  chash_free(hash_exist);
 err:
  return res;
}
