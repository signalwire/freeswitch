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
 * $Id: mailthread.c,v 1.28 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailthread.h"
#include "mailthread_types.h"

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "mail.h"
#include "chash.h"
#include "carray.h"
#include "clist.h"
#include "mailmessage.h"
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif

/* SEB */
#ifdef NO_MACROS
#	define inline
#endif

static inline char * get_msg_id(mailmessage * msg)
{
  if (msg->msg_single_fields.fld_message_id != NULL)
    return msg->msg_single_fields.fld_message_id->mid_value;
  else
    return NULL;
}

static inline clist * get_ref(mailmessage * msg)
{
  if (msg->msg_single_fields.fld_references != NULL)
    return msg->msg_single_fields.fld_references->mid_list;
  else
    return NULL;
}

static inline clist * get_in_reply_to(mailmessage * msg)
{
  if (msg->msg_single_fields.fld_in_reply_to != NULL)
    return msg->msg_single_fields.fld_in_reply_to->mid_list;
  else
    return NULL;
}

static inline int skip_subj_blob(char * subj, size_t * begin,
				 size_t length)
{
  /* subj-blob       = "[" *BLOBCHAR "]" *WSP */
  size_t cur_token;

  cur_token = * begin;

  if (subj[cur_token] != '[')
    return FALSE;

  cur_token ++;

  while (1) {
    if (cur_token >= length)
      return FALSE;

    if (subj[cur_token] == '[')
      return FALSE;

    if (subj[cur_token] == ']')
      break;

    cur_token ++;
  }

  cur_token ++;

  while (1) {
    if (cur_token >= length)
      break;

    if (subj[cur_token] != ' ')
      break;

    cur_token ++;
  }

  * begin = cur_token;

  return TRUE;
}

static inline int skip_subj_refwd(char * subj, size_t * begin,
				  size_t length)
{
  /* subj-refwd      = ("re" / ("fw" ["d"])) *WSP [subj-blob] ":" */
  size_t cur_token;
  int prefix;

  cur_token = * begin;

  prefix = FALSE;
  if (length >= 3) {
    if (strncasecmp(subj + cur_token, "fwd", 3) == 0) {
      cur_token += 3;
      prefix = TRUE;
    }
  }
  if (!prefix) {
    if (length >= 2) {
      if (strncasecmp(subj + cur_token, "fw", 2) == 0) {
	cur_token += 2;
	prefix = TRUE;
      }
      else if (strncasecmp(subj + cur_token, "re", 2) == 0) {
	cur_token += 2;
	prefix = TRUE;
      }
    }
  }

  if (!prefix)
    return FALSE;

  while (1) {
    if (cur_token >= length)
      break;
      
    if (subj[cur_token] != ' ')
      break;

    cur_token ++;
  }
  
  skip_subj_blob(subj, &cur_token, length);

  if (subj[cur_token] != ':')
    return FALSE;

  cur_token ++;

  * begin = cur_token;

  return TRUE;
}

static inline int skip_subj_leader(struct mailmessage_tree * tree,
				   char * subj, size_t * begin,
				   size_t length)
{
  size_t cur_token;

  cur_token = * begin;

  /* subj-leader     = (*subj-blob subj-refwd) / WSP */
  
  if (subj[cur_token] == ' ') {
    cur_token ++;
  }
  else {
    while (cur_token < length) {
      if (!skip_subj_blob(subj, &cur_token, length))
	break;
    }
    if (!skip_subj_refwd(subj, &cur_token, length))
      return FALSE;
    tree->node_is_reply = TRUE;
  }

  * begin = cur_token;

  return TRUE;
}


static char * extract_subject(char * default_from,
    struct mailmessage_tree * tree,
    char * str)
{
  char * subj;
  char * cur;
  char * write_pos;
  size_t len;
  size_t begin;

  char * decoded;
  size_t cur_token;

  int do_repeat_5;
  int do_repeat_6;
  int r;

  /*
    (1) Convert any RFC 2047 encoded-words in the subject to
    UTF-8.
  */

  decoded = NULL;

  cur_token = 0;
  r = mailmime_encoded_phrase_parse(default_from, str, strlen(str),
				    &cur_token, "utf-8",
				    &decoded);

  if (r == MAILIMF_NO_ERROR) {
    subj = decoded;
  }
  else
    subj = strdup(str);

  len = strlen(subj);

  /*
    Convert all tabs and continuations to space.
    Convert all multiple spaces to a single space.
  */

  cur = subj;
  write_pos = subj;
  while (* cur != '\0') {
    int cont;

    switch (* cur) {
    case '\t':
    case '\r':
    case '\n':
      cont = TRUE;

      cur ++;
      while (* cur && cont) {
	switch (* cur) {
	case '\t':
	case '\r':
	case '\n':
	  cont = TRUE;
	  break;
	default:
	  cont = FALSE;
	  break;
	}
	cur ++;
      }
      
      * write_pos = ' ';
      write_pos ++;

      break;
      
    default:
      * write_pos = * cur;
      write_pos ++;

      cur ++;

      break;
    }
  }
  * write_pos = '\0';

  begin = 0;

  do {
    do_repeat_6 = FALSE;

    /*
      (2) Remove all trailing text of the subject that matches
      the subj-trailer ABNF, repeat until no more matches are
      possible.
    */

    while (len > 0) {
      int chg;

      chg = FALSE;

      /* subj-trailer    = "(fwd)" / WSP */
      if (subj[len - 1] == ' ') {
	subj[len - 1] = '\0';
	len --;
      }
      else {
	if (len < 5)
	  break;

	if (strncasecmp(subj + len - 5, "(fwd)", 5) != 0)
	  break;

	subj[len - 5] = '\0';
	len -= 5;
	tree->node_is_reply = TRUE;
      }
    }

    do {
      size_t saved_begin;

      do_repeat_5 = FALSE;

      /*
	(3) Remove all prefix text of the subject that matches the
	subj-leader ABNF.
      */
    
      if (skip_subj_leader(tree, subj, &begin, len))
	do_repeat_5 = TRUE;

      /*
	(4) If there is prefix text of the subject that matches the
	subj-blob ABNF, and removing that prefix leaves a non-empty
	subj-base, then remove the prefix text.
      */
    
      saved_begin = begin;
      if (skip_subj_blob(subj, &begin, len)) {
	if (begin == len) {
	  /* this will leave a empty subject base */
	  begin = saved_begin;
	}
	else
	  do_repeat_5 = TRUE;
      }

      /*
	(5) Repeat (3) and (4) until no matches remain.
	Note: it is possible to defer step (2) until step (6),
	but this requires checking for subj-trailer in step (4).
      */
    
    }
    while (do_repeat_5);

    /*
      (6) If the resulting text begins with the subj-fwd-hdr ABNF
      and ends with the subj-fwd-trl ABNF, remove the
      subj-fwd-hdr and subj-fwd-trl and repeat from step (2).
    */

    if (len >= 5) {
      size_t saved_begin;

      saved_begin = begin;
      if (strncasecmp(subj + begin, "[fwd:", 5) == 0) {
	begin += 5;
	
	if (subj[len - 1] != ']')
	  saved_begin = begin;
	else {
	  tree->node_is_reply = TRUE;

	  subj[len - 1] = '\0';
	  len --;
	  do_repeat_6 = TRUE;
	}
      }
    }

  }
  while (do_repeat_6);

  /*
    (7) The resulting text is the "base subject" used in
    threading.
  */

  /* convert to upper case */

  cur = subj + begin;
  write_pos = subj;

  while (* cur != '\0') {
    * write_pos = (char) toupper((unsigned char) * cur);
    cur ++;
    write_pos ++;
  }
  * write_pos = '\0';

  return subj;
}

static int get_extracted_subject(char * default_from,
    struct mailmessage_tree * tree,
    char ** result)
{
  if (tree->node_msg->msg_single_fields.fld_subject != NULL) {
    char * subj;

    subj = extract_subject(default_from,
        tree, tree->node_msg->msg_single_fields.fld_subject->sbj_value);
    if (subj == NULL)
      return MAIL_ERROR_MEMORY;
    
    * result = subj;
    
    return MAIL_NO_ERROR;
  }

  return MAIL_ERROR_SUBJECT_NOT_FOUND;
}

static int get_thread_subject(char * default_from,
    struct mailmessage_tree * tree,
    char ** result)
{
  char * thread_subject;
  int r;
  unsigned int i;

  if (tree->node_msg != NULL) {
    if (tree->node_msg->msg_fields != NULL) {
      r = get_extracted_subject(default_from, tree, &thread_subject);

      if (r != MAIL_NO_ERROR)
	return r;

      * result = thread_subject;
      return MAIL_NO_ERROR;
    }
  }

  for(i = 0 ; i < carray_count(tree->node_children) ; i ++) {
    struct mailmessage_tree * child;
    
    child = carray_get(tree->node_children, i);

    r = get_thread_subject(default_from, child, &thread_subject);
    
    switch (r) {
    case MAIL_NO_ERROR:
      * result = thread_subject;
      return MAIL_NO_ERROR;

    case MAIL_ERROR_SUBJECT_NOT_FOUND:
      /* do nothing */
      break;

    default:
      return r;
    }
  }

  return MAIL_ERROR_SUBJECT_NOT_FOUND;
}



#ifndef WRONG
#define WRONG	(-1)
#endif /* !defined WRONG */

static int tmcomp(struct tm * atmp, struct tm * btmp)
{
  register int	result;
  
  if ((result = (atmp->tm_year - btmp->tm_year)) == 0 &&
      (result = (atmp->tm_mon - btmp->tm_mon)) == 0 &&
      (result = (atmp->tm_mday - btmp->tm_mday)) == 0 &&
      (result = (atmp->tm_hour - btmp->tm_hour)) == 0 &&
      (result = (atmp->tm_min - btmp->tm_min)) == 0)
    result = atmp->tm_sec - btmp->tm_sec;
  return result;
}

static time_t mkgmtime(struct tm * tmp)
{
  register int			dir;
  register int			bits;
  register int			saved_seconds;
  time_t				t;
  struct tm			yourtm, mytm;
  
  yourtm = *tmp;
  saved_seconds = yourtm.tm_sec;
  yourtm.tm_sec = 0;
  /*
  ** Calculate the number of magnitude bits in a time_t
  ** (this works regardless of whether time_t is
  ** signed or unsigned, though lint complains if unsigned).
  */
  for (bits = 0, t = 1; t > 0; ++bits, t <<= 1)
    ;
  /*
  ** If time_t is signed, then 0 is the median value,
  ** if time_t is unsigned, then 1 << bits is median.
  */
  t = (t < 0) ? 0 : ((time_t) 1 << bits);
  for ( ; ; ) {
    gmtime_r(&t, &mytm);
    dir = tmcomp(&mytm, &yourtm);
    if (dir != 0) {
      if (bits-- < 0)
	return WRONG;
      if (bits < 0)
	--t;
      else if (dir > 0)
	t -= (time_t) 1 << bits;
      else	t += (time_t) 1 << bits;
      continue;
    }
    break;
  }
  t += saved_seconds;
  return t;
}

static inline time_t get_date(mailmessage * msg)
{
  struct tm tmval;
  time_t timeval;
  struct mailimf_date_time * date_time;

  if (msg->msg_single_fields.fld_orig_date == NULL)
    return (time_t) -1;
  
  date_time = msg->msg_single_fields.fld_orig_date->dt_date_time;
  
  tmval.tm_sec  = date_time->dt_sec;
  tmval.tm_min  = date_time->dt_min;
  tmval.tm_hour = date_time->dt_hour;
  tmval.tm_sec  = date_time->dt_sec;
  tmval.tm_mday = date_time->dt_day;
  tmval.tm_mon  = date_time->dt_month - 1;
  tmval.tm_year = date_time->dt_year - 1900;
  
  timeval = mkgmtime(&tmval);
  
  timeval -= date_time->dt_zone * 36;
  
  return timeval;
}

static inline int is_descendant(struct mailmessage_tree * node,
			 struct mailmessage_tree * maybe_child)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(node->node_children) ; i++) {
    struct mailmessage_tree * tree;

    tree = carray_get(node->node_children, i);
    if (tree == maybe_child)
      return TRUE;
    if (carray_count(tree->node_children) != 0)
      if (is_descendant(tree, maybe_child))
	return TRUE;
  }

  return FALSE;
}

static int delete_dummy(carray * rootlist, carray * sibling_list,
    unsigned int cur, unsigned int * pnext)
{
  struct mailmessage_tree * env_tree;
  int res;
  int r;
  unsigned int cur_child;
  unsigned int next;

  env_tree = carray_get(sibling_list, cur);

  cur_child = 0;
  while (cur_child < carray_count(env_tree->node_children)) {
    delete_dummy(rootlist, env_tree->node_children, cur_child, &cur_child);
  }

  if (env_tree->node_msg == NULL) {
    if (carray_count(env_tree->node_children) == 0) {

      /* If it is a dummy message with NO children, delete it. */
      mailmessage_tree_free(env_tree);
      carray_delete(sibling_list, cur);
      next = cur;
    }
    else {
      /* If it is a dummy message with children, delete it, but
	 promote its children to the current level. */

      /*
	Do not promote the children if doing so would make them
	children of the root, unless there is only one child.
      */

      cur_child = 0;
      if ((sibling_list != rootlist) ||
          (carray_count(env_tree->node_children) == 1)) {
	while (cur_child < carray_count(env_tree->node_children)) {
	  struct mailmessage_tree * child;
	  
	  child = carray_get(env_tree->node_children, cur_child);
	  r = carray_add(sibling_list, child, NULL);
	  if (r < 0) {
	    res = MAIL_ERROR_MEMORY;
	    goto err;
	  }
          /* set new parent of the children */
          child->node_parent = env_tree->node_parent;

	  carray_delete(env_tree->node_children, cur_child);
	}
        mailmessage_tree_free(env_tree);
	carray_delete(sibling_list, cur);
        next = cur;
      }
      else
	next = cur + 1;
    }
  }
  else
    next = cur + 1;

  * pnext = next;

  return MAIL_NO_ERROR;

 err:
  return res;       
}

static inline time_t tree_get_date(struct mailmessage_tree * tree)
{
  if (tree->node_msg != NULL) {
    return tree->node_date;
  }
  else {
    struct mailmessage_tree * subtree;
    
    if (carray_count(tree->node_children) == 0)
      return (time_t) -1;
    
    subtree = carray_get(tree->node_children, 0);
    
    return subtree->node_date;
  }
}

static inline uint32_t tree_get_index(struct mailmessage_tree * tree)
{
  if (tree->node_msg == NULL)
    return 0;

  return tree->node_msg->msg_index;
}

int mailthread_tree_timecomp(struct mailmessage_tree ** ptree1,
    struct mailmessage_tree ** ptree2)
{
  time_t date1;
  time_t date2;

  date1 = tree_get_date(* ptree1);
  date2 = tree_get_date(* ptree2);

  if ((date1 == (time_t) -1) || (date2 == (time_t) -1)) {
    uint32_t index1;
    uint32_t index2;

    index1 = tree_get_index(* ptree1);
    index2 = tree_get_index(* ptree2);
    return (int) ((long) index1 - (long) index2);
  }

  return (int) ((long) date1 - (long) date2);
}

static int tree_subj_time_comp(struct mailmessage_tree ** ptree1,
			       struct mailmessage_tree ** ptree2)
{
  char * subj1;
  char * subj2;
  time_t date1;
  time_t date2;
  int r;

  subj1 = (* ptree1)->node_base_subject;
  subj2 = (* ptree2)->node_base_subject;

  if ((subj1 != NULL) && (subj2 != NULL))
    r = strcmp(subj1, subj2);
  else {
    if ((subj1 == NULL) && (subj2 == NULL))
      r = 0;
    else if (subj1 == NULL)
      r = -1;
    else /* subj2 == NULL */
      r = 1;
  }   

  if (r != 0)
    return r;

  date1 = (* ptree1)->node_date;
  date2 = (* ptree2)->node_date;
  
  if ((date1 == (time_t) -1) || (date2 == (time_t) -1))
    return ((int32_t) (* ptree1)->node_msg->msg_index) -
      ((int32_t) (* ptree2)->node_msg->msg_index);
  
  return (int) ((long) date1 - (long) date2);
}



int mail_thread_sort(struct mailmessage_tree * tree,
    int (* comp_func)(struct mailmessage_tree **,
        struct mailmessage_tree **),
    int sort_sub)
{
  unsigned int cur;
  int r;
  int res;

  for(cur = 0 ; cur < carray_count(tree->node_children) ; cur ++) {
    struct mailmessage_tree * subtree;

    subtree = carray_get(tree->node_children, cur);

    if (sort_sub) {
      r = mail_thread_sort(subtree, comp_func, sort_sub);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto err;
      }
    }
  }

  qsort(carray_data(tree->node_children), carray_count(tree->node_children),
      sizeof(struct mailmessage_tree *),
	(int (*)(const void *, const void *)) comp_func);

  return MAIL_NO_ERROR;
  
 err:
  return res;
}


static int
mail_build_thread_references(char * default_from,
    struct mailmessage_list * env_list,
    struct mailmessage_tree ** result,
    int use_subject, 
    int (* comp_func)(struct mailmessage_tree **,
        struct mailmessage_tree **))
{
  int r;
  int res;
  chash * msg_id_hash;
  unsigned int cur;
  struct mailmessage_tree * root;
  carray * rootlist;
  carray * msg_list;
  unsigned int i;
  chash * subject_hash;

  msg_id_hash = chash_new(128, CHASH_COPYNONE);
  if (msg_id_hash == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  root = mailmessage_tree_new(NULL, (time_t) -1, NULL);
  if (root == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_hash;
  }
  rootlist = root->node_children;

  msg_list = carray_new(128);
  if (msg_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_root;
  }

  /* collect message-ID */
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    char * msgid;
    struct mailmessage_tree * env_tree;
    chashdatum hashkey;
    chashdatum hashdata;
    chashdatum hashold;
    time_t date;

    msg = carray_get(env_list->msg_tab, i);

    if (msg == NULL)
      continue;

    if (msg->msg_fields != NULL) {
      msgid = get_msg_id(msg);

      if (msgid == NULL) {
	msgid = mailimf_get_message_id();
      }
      else {
	hashkey.data = msgid;
	hashkey.len = strlen(msgid);
	
	if (chash_get(msg_id_hash, &hashkey, &hashdata) == 0)
	  msgid = mailimf_get_message_id();
	else
	  msgid = strdup(msgid);
      }
      
      if (msgid == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      date = get_date(msg);
      
      env_tree = mailmessage_tree_new(msgid, date, msg);
      if (env_tree == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      r = carray_add(msg_list, env_tree, NULL);
      if (r < 0) {
	mailmessage_tree_free(env_tree);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      hashkey.data = msgid;
      hashkey.len = strlen(msgid);
      
      hashdata.data = env_tree;
      hashdata.len = 0;
      
      r = chash_set(msg_id_hash, &hashkey, &hashdata, &hashold);
      if (r < 0) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  /* (1) for all messages */

  for(cur = 0 ; cur < carray_count(msg_list) ; cur ++) {
    struct mailmessage_tree * env_tree;
    mailmessage * msg;
    clist * ref;

    env_tree = carray_get(msg_list, cur);

    msg = env_tree->node_msg;

    ref = NULL;
    if (msg != NULL) {
      ref = get_ref(msg);
      if (ref == NULL)
	ref = get_in_reply_to(msg);
    }      

    /* (A) Using the Message IDs in the message's references, link
       the corresponding messages (those whose Message-ID header
       line contains the given reference Message ID) together as
       parent/child.
    */

    if (ref != NULL) {
      /* try to start a tree */

      clistiter * cur_ref;
      chashdatum hashkey;
      chashdatum hashdata;
      chashdatum hashold;
      struct mailmessage_tree * env_cur_tree;
      struct mailmessage_tree * last_env_cur_tree;

      env_cur_tree = NULL;
      for(cur_ref = clist_begin(ref) ; cur_ref != NULL ;
	  cur_ref = clist_next(cur_ref)) {
	char * msgid;

	last_env_cur_tree = env_cur_tree;

	msgid = clist_content(cur_ref);

	hashkey.data = msgid;
	hashkey.len = strlen(msgid);
	
	r = chash_get(msg_id_hash, &hashkey, &hashdata);
	if (r < 0) {
	  /* not found, create a dummy message */
	  msgid = strdup(msgid);
	  if (msgid == NULL) {
	    res = MAIL_ERROR_MEMORY;
	    goto free_list;
	  }

	  env_cur_tree = mailmessage_tree_new(msgid, (time_t) -1, NULL);
	  if (env_cur_tree == NULL) {
	    free(msgid);
	    res = MAIL_ERROR_MEMORY;
	    goto free_list;
	  }

	  r = carray_add(msg_list, env_cur_tree, NULL);
	  if (r < 0) {
	    mailmessage_tree_free(env_cur_tree);
	    res = MAIL_ERROR_MEMORY;
	    goto free_list;
	  }

	  hashkey.data = msgid;
	  hashkey.len = strlen(msgid);
	    
	  hashdata.data = env_cur_tree;
	  hashdata.len = 0;
	  
	  r = chash_set(msg_id_hash, &hashkey, &hashdata, &hashold);
	  if (r < 0) {
	    res = MAIL_ERROR_MEMORY;
	    goto free_list;
	  }
	}
	else {
	  env_cur_tree = hashdata.data;
	}

	if (last_env_cur_tree != NULL) {
	  if (env_cur_tree->node_parent == NULL) {
	    /* make it one child */
	    if (env_cur_tree != last_env_cur_tree) {
	      if (!is_descendant(env_cur_tree, last_env_cur_tree)) {
                /* set parent */
		env_cur_tree->node_parent = last_env_cur_tree;
		r = carray_add(last_env_cur_tree->node_children,
                    env_cur_tree, NULL);
		if (r < 0) {
		  res = MAIL_ERROR_MEMORY;
		  goto free_list;
		}
	      }
	    }
	  }
	}
      }

      /* (B) Create a parent/child link between the last reference
	 (or NIL if there are no references) and the current message.
	 If the current message already has a parent, it is probably
	 the result of a truncated References header line, so break
	 the current parent/child link before creating the new
	 correct one.
      */
      
      last_env_cur_tree = env_cur_tree;
      
      if (last_env_cur_tree != NULL) {
	if (env_tree->node_parent == NULL) {
	  if (last_env_cur_tree != env_tree) {
	    if (!is_descendant(env_tree, last_env_cur_tree)) {
              /* set parent */
	      env_tree->node_parent = last_env_cur_tree;
	      r = carray_add(last_env_cur_tree->node_children, env_tree, NULL);
	      if (r < 0) {
		res = MAIL_ERROR_MEMORY;
		goto free_list;
	      }
	    }
	  }
	}
      }
    }
  }

  chash_free(msg_id_hash);
  msg_id_hash = NULL;

  /* (2) Gather together all of the messages that have no parents
     and make them all children (siblings of one another) of a dummy
     parent (the "root").
  */

  for(cur = 0 ; cur < carray_count(msg_list) ; cur ++) {
    struct mailmessage_tree * env_tree;

    env_tree = carray_get(msg_list, cur);
    if (env_tree->node_parent == NULL) {
      r = carray_add(rootlist, env_tree, NULL);
      if (r < 0) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      /* set parent */
      env_tree->node_parent = root;
    }
  }

  carray_free(msg_list);
  msg_list = NULL;

  /* (3) Prune dummy messages from the thread tree.
   */

  cur = 0;
  while (cur < carray_count(rootlist)) {
    r = delete_dummy(rootlist, rootlist, cur, &cur);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }
  }

  /* (4) Sort the messages under the root (top-level siblings only)
     by sent date.
  */

  r = mail_thread_sort(root, mailthread_tree_timecomp, FALSE);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_list;
  }

  if (use_subject) {

    /* (5) Gather together messages under the root that have the same
       extracted subject text.

       (A) Create a table for associating extracted subjects with
       messages.
    */

    subject_hash = chash_new(128, CHASH_COPYVALUE);
    if (subject_hash == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    /*
      (B) Populate the subject table with one message per
      extracted subject.  For each child of the root:
    */

    for(cur = 0 ; cur < carray_count(rootlist) ; cur ++) {
      struct mailmessage_tree * env_tree;
      chashdatum key;
      chashdatum data;
      char * base_subject;
      int r;

      env_tree = carray_get(rootlist, cur);

      /*
	(i) Find the subject of this thread by extracting the
	base subject from the current message, or its first child
	if the current message is a dummy.
      */

      r = get_thread_subject(default_from, env_tree, &base_subject);

      /*
	(ii) If the extracted subject is empty, skip this
	message.
      */

      if (r == MAIL_ERROR_SUBJECT_NOT_FOUND) {
	/* no subject found */
	continue;
      }
      else if (r == MAIL_NO_ERROR) {
	if (* base_subject == '\0') {
	  /* subject empty */
	  free(base_subject);
	  continue;
	}
	else {
	  /* do nothing */
	}
      }
      else {
	res = r;
	goto free_subject_hash;
      }

      env_tree->node_base_subject = base_subject;

      /*
	(iii) Lookup the message associated with this extracted
	subject in the table.
      */

      key.data = base_subject;
      key.len = strlen(base_subject);

      r = chash_get(subject_hash, &key, &data);

      if (r < 0) {
	/*
	  (iv) If there is no message in the table with this
	  subject, add the current message and the extracted
	  subject to the subject table.
	*/

	data.data = &cur;
	data.len = sizeof(cur);

	r = chash_set(subject_hash, &key, &data, NULL);
	if (r < 0) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}
      }
      else {
	/*
	  Otherwise, replace the message in the table with the
	  current message if the message in the table is not a
	  dummy AND either of the following criteria are true:
	  The current message is a dummy, OR                  
	  The message in the table is a reply or forward (its
	  original subject contains a subj-refwd part and/or a
	  "(fwd)" subj-trailer) and the current message is not.
	*/
	struct mailmessage_tree * msg_in_table;
	unsigned int * iter_in_table;
	int replace;

	iter_in_table = data.data;
	msg_in_table = carray_get(rootlist, cur);

	replace = FALSE;
	/* message is dummy if info is NULL */
	if (msg_in_table->node_msg != NULL) {

	  if (env_tree->node_msg == NULL)
	    replace = TRUE;
	  else {
	    if (env_tree->node_is_reply && !env_tree->node_is_reply)
	      replace = TRUE;
	  }
	}
 
	if (replace) {
	  data.data = &cur;
	  data.len = sizeof(cur);
	
	  r = chash_set(subject_hash, &key, &data, NULL);
	  if (r < 0) {
	    res = MAIL_ERROR_MEMORY;
	    goto free_subject_hash;
	  }
	}
      }
    }

    /*
      (C) Merge threads with the same subject.  For each child of
      the root:
    */

    cur = 0;
    while (cur < carray_count(rootlist)) {
      struct mailmessage_tree * env_tree;
      chashdatum key;
      chashdatum data;
      int r;
      struct mailmessage_tree * main_tree;
      unsigned int * main_cur;

      env_tree = carray_get(rootlist, cur);

      if (env_tree == NULL)
        goto next_msg;

      /*
	(i) Find the subject of this thread as in step 4.B.i
	above.
      */
    
      /* already done in tree->node_base_subject */
    
      /*
	(ii) If the extracted subject is empty, skip this
	message.
      */
    
      if (env_tree->node_base_subject == NULL)
	goto next_msg;

      if (* env_tree->node_base_subject == '\0')
	goto next_msg;

      /*
	(iii) Lookup the message associated with this extracted
	subject in the table.
      */

      key.data = env_tree->node_base_subject;
      key.len = strlen(env_tree->node_base_subject);

      r = chash_get(subject_hash, &key, &data);
      if (r < 0)
	goto next_msg;

      /*
	(iv) If the message in the table is the current message,
	skip this message. 
      */
    
      main_cur = data.data;
      if (* main_cur == cur)
	goto next_msg;

      /*
	Otherwise, merge the current message with the one in the
	table using the following rules:
      */

      main_tree = carray_get(rootlist, * main_cur);

      /*
	If both messages are dummies, append the current
	message's children to the children of the message in
	the table (the children of both messages become
	siblings), and then delete the current message.
      */

      if ((env_tree->node_msg == NULL) && (main_tree->node_msg == NULL)) {
        unsigned int old_size;

        old_size = carray_count(main_tree->node_children);

        r = carray_set_size(main_tree->node_children, old_size +
            carray_count(env_tree->node_children));
        if (r < 0) {
          res = MAIL_ERROR_MEMORY;
          goto free_subject_hash;
        }

        for(i = 0 ; i < carray_count(env_tree->node_children) ; i ++) {
          struct mailmessage_tree * child;

          child = carray_get(env_tree->node_children, i);
          carray_set(main_tree->node_children, old_size + i, child);
          /* set parent */
          child->node_parent = main_tree;
        }
        carray_set_size(env_tree->node_children, 0);
	/* this is the only case where children can be NULL,
	   this is before freeing it */
	mailmessage_tree_free(env_tree);
        carray_delete_fast(rootlist, cur);
      }

      /*
	If the message in the table is a dummy and the current
	message is not, make the current message a child of
	the message in the table (a sibling of it's children).
      */

      else if (main_tree->node_msg == NULL) {
	r = carray_add(main_tree->node_children, env_tree, NULL);
	if (r < 0) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}
        /* set parent */
        env_tree->node_parent = main_tree;

	carray_delete_fast(rootlist, cur);
      }

      /*
	If the current message is a reply or forward and the
	message in the table is not, make the current message
	a child of the message in the table (a sibling of it's
	children).
      */

      else if (env_tree->node_is_reply && !main_tree->node_is_reply) {
	r = carray_add(main_tree->node_children, env_tree, NULL);
	if (r < 0) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}
        /* set parent */
        env_tree->node_parent = main_tree;

	carray_delete_fast(rootlist, cur);
      }

      /*
	Otherwise, create a new dummy message and make both
	the current message and the message in the table
	children of the dummy.  Then replace the message in
	the table with the dummy message.
	Note: Subject comparisons are case-insensitive, as
	described under "Internationalization
	Considerations."
      */

      else {
	struct mailmessage_tree * new_main_tree;
	char * base_subject;
        unsigned int last;

	new_main_tree = mailmessage_tree_new(NULL, (time_t) -1, NULL);
	if (new_main_tree == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}

	/* main_tree->node_base_subject is never NULL */

	base_subject = strdup(main_tree->node_base_subject);
	if (base_subject == NULL) {
	  mailmessage_tree_free(new_main_tree);
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}

	new_main_tree->node_base_subject = base_subject;

	r = carray_add(rootlist, new_main_tree, &last);
	if (r < 0) {
	  mailmessage_tree_free(new_main_tree);
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}

	r = carray_add(new_main_tree->node_children, main_tree, NULL);
	if (r < 0) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}
        /* set parent */
        main_tree->node_parent = new_main_tree;

	carray_delete_fast(rootlist, * main_cur);

	r = carray_add(new_main_tree->node_children, env_tree, NULL);
	if (r < 0) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}
        /* set parent */
        env_tree->node_parent = new_main_tree;

	carray_delete_fast(rootlist, cur);

	data.data = &last;
	data.len = sizeof(last);
      
	r = chash_set(subject_hash, &key, &data, NULL);

	if (r < 0) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_subject_hash;
	}
      }

      continue;

    next_msg:
      cur ++;
      continue;
    }
    
    i = 0;
    for(cur = 0 ; cur < carray_count(rootlist) ; cur ++) {
      struct mailmessage_tree * env_tree;

      env_tree = carray_get(rootlist, cur);
      if (env_tree == NULL)
        continue;
      
      carray_set(rootlist, i, env_tree);
      i ++;
    }
    carray_set_size(rootlist, i);
    
    chash_free(subject_hash);
  }

  /*
    (6) Traverse the messages under the root and sort each set of
    siblings by sent date.  Traverse the messages in such a way
    that the "youngest" set of siblings are sorted first, and the
    "oldest" set of siblings are sorted last (grandchildren are
    sorted before children, etc).

    In the case of an exact match on
    sent date or if either of the Date: headers used in a
    comparison can not be parsed, use the order in which the
    messages appear in the mailbox (that is, by sequence number) to
    determine the order.  In the case of a dummy message (which can
    only occur with top-level siblings), use its first child for
    sorting.
  */

#if 0
  if (comp_func != NULL) {
    r = mail_thread_sort(root, comp_func, TRUE);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }
  }
#endif
  if (comp_func == NULL)
    comp_func = mailthread_tree_timecomp;
  
  r = mail_thread_sort(root, comp_func, TRUE);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_list;
  }
  
  * result = root;

  return MAIL_NO_ERROR;

 free_subject_hash:
  chash_free(subject_hash);
 free_list:
  if (msg_list != NULL) {
    for(i = 0 ; i < carray_count(msg_list) ; i ++)
      mailmessage_tree_free(carray_get(msg_list, i));
    carray_free(msg_list);
  }
 free_root:
  mailmessage_tree_free_recursive(root);
 free_hash:
  if (msg_id_hash != NULL)
    chash_free(msg_id_hash);
 err:
  return res;
}



static int
mail_build_thread_orderedsubject(char * default_from,
    struct mailmessage_list * env_list,
    struct mailmessage_tree ** result,
    int (* comp_func)(struct mailmessage_tree **,
        struct mailmessage_tree **))
{
  unsigned int i;
  carray * rootlist;
  unsigned int cur;
  struct mailmessage_tree * root;
  int res;
  int r;
  struct mailmessage_tree * current_thread;

  root = mailmessage_tree_new(NULL, (time_t) -1, NULL);
  if (root == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  rootlist = root->node_children;

  /*
    The ORDEREDSUBJECT threading algorithm is also referred to as
    "poor man's threading."
  */

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    struct mailmessage_tree * env_tree;
    char * base_subject;
    time_t date;

    msg = carray_get(env_list->msg_tab, i);

    if (msg == NULL)
      continue;

    if (msg->msg_fields != NULL) {

      date = get_date(msg);

      env_tree = mailmessage_tree_new(NULL, date, msg);
      if (env_tree == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free;
      }

      /* set parent */
      env_tree->node_parent = root;
      r = carray_add(rootlist, env_tree, NULL);
      if (r < 0) {
	mailmessage_tree_free(env_tree);
	res = MAIL_ERROR_MEMORY;
	goto free;
      }

      r = get_extracted_subject(default_from, env_tree, &base_subject);
      switch (r) {
      case MAIL_NO_ERROR:
	env_tree->node_base_subject = base_subject;
	break;

      case MAIL_ERROR_SUBJECT_NOT_FOUND:
	break;
	
      default:
	res = r;
	goto free;
      }
    }
  }

  /*
    The searched messages are sorted by
    subject and then by the sent date.
  */

  r = mail_thread_sort(root, tree_subj_time_comp, FALSE);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  /*
    The messages are then split
    into separate threads, with each thread containing messages
    with the same extracted subject text.
  */

  current_thread = NULL;

  cur = 0;
  while (cur < carray_count(rootlist)) {
    struct mailmessage_tree * cur_env_tree;

    cur_env_tree = carray_get(rootlist, cur);
    if (current_thread == NULL) {
      current_thread = cur_env_tree;
      cur ++;
      continue;
    }

    if ((cur_env_tree->node_base_subject == NULL) ||
	(current_thread->node_base_subject == NULL)) {
      current_thread = cur_env_tree;
      cur ++;
      continue;
    }

    if (strcmp(cur_env_tree->node_base_subject,
            current_thread->node_base_subject) == 0) {

      /* set parent */
      cur_env_tree->node_parent = current_thread;
      r = carray_add(current_thread->node_children, cur_env_tree, NULL);
      if (r < 0) {
	res = MAIL_ERROR_MEMORY;
	goto free;
      }
      
      carray_delete(rootlist, cur);
    }
    else
      cur ++;
    current_thread = cur_env_tree;
  }

  /*
    Finally, the threads are
    sorted by the sent date of the first message in the thread.
    Note that each message in a thread is a child (as opposed to a
    sibling) of the previous message.
  */

#if 0
  if (comp_func != NULL) {
    r = mail_thread_sort(root, comp_func, FALSE);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free;
    }
  }
#endif
  
  if (comp_func == NULL)
    comp_func = mailthread_tree_timecomp;
  
  r = mail_thread_sort(root, comp_func, FALSE);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }

  * result = root;

  return MAIL_NO_ERROR;

 free:
  mailmessage_tree_free_recursive(root);
 err:
  return res;
}


static int
mail_build_thread_none(char * default_from,
    struct mailmessage_list * env_list,
    struct mailmessage_tree ** result,
    int (* comp_func)(struct mailmessage_tree **,
        struct mailmessage_tree **))
{
  unsigned int i;
  carray * rootlist;
  struct mailmessage_tree * root;
  int res;
  int r;

  root = mailmessage_tree_new(NULL, (time_t) -1, NULL);
  if (root == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  rootlist = root->node_children;

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    struct mailmessage_tree * env_tree;
    char * base_subject;
    time_t date;

    msg = carray_get(env_list->msg_tab, i);

    if (msg == NULL)
      continue;

    if (msg->msg_fields != NULL) {

      date = get_date(msg);

      env_tree = mailmessage_tree_new(NULL, date, msg);
      if (env_tree == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free;
      }

      /* set parent */
      env_tree->node_parent = root;
      r = carray_add(rootlist, env_tree, NULL);
      if (r < 0) {
	mailmessage_tree_free(env_tree);
	res = MAIL_ERROR_MEMORY;
	goto free;
      }

      r = get_extracted_subject(default_from, env_tree, &base_subject);
      switch (r) {
      case MAIL_NO_ERROR:
	env_tree->node_base_subject = base_subject;
	break;

      case MAIL_ERROR_SUBJECT_NOT_FOUND:
	break;
	
      default:
	res = r;
	goto free;
      }
    }
  }
  
  if (comp_func == NULL)
    comp_func = mailthread_tree_timecomp;
  
  r = mail_thread_sort(root, comp_func, FALSE);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free;
  }
  
  * result = root;

  return MAIL_NO_ERROR;

 free:
  mailmessage_tree_free_recursive(root);
 err:
  return res;
}


int mail_build_thread(int type, char * default_from,
    struct mailmessage_list * env_list,
    struct mailmessage_tree ** result,
     int (* comp_func)(struct mailmessage_tree **,
         struct mailmessage_tree **))
{
  unsigned int i;

  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++)
    mailmessage_resolve_single_fields(carray_get(env_list->msg_tab, i));

  switch (type) {
  case MAIL_THREAD_REFERENCES:
    return mail_build_thread_references(default_from,
        env_list, result, TRUE, comp_func);

  case MAIL_THREAD_REFERENCES_NO_SUBJECT:
    return mail_build_thread_references(default_from,
        env_list, result, FALSE, comp_func);

  case MAIL_THREAD_ORDEREDSUBJECT:
    return mail_build_thread_orderedsubject(default_from,
        env_list, result, comp_func);
    
  case MAIL_THREAD_NONE:
    return mail_build_thread_none(default_from,
        env_list, result, comp_func);
    
  default:
    return MAIL_ERROR_NOT_IMPLEMENTED;
  }
}
