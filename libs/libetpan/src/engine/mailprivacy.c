/*
 * libEtPan! -- a mail library
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
 * $Id: mailprivacy.c,v 1.7 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailprivacy.h"

#include <libetpan/libetpan.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include "mailprivacy_tools.h"

carray * mailprivacy_get_protocols(struct mailprivacy * privacy)
{
  return privacy->protocols;
}

static int recursive_check_privacy(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime);

static int check_tmp_dir(char * tmp_dir)
{
  struct stat stat_info;
  int r;
  
  r = stat(tmp_dir, &stat_info);
  if (r < 0)
    return MAIL_ERROR_FILE;
 
  /* check if the directory belongs to the user */
  if (stat_info.st_uid != getuid())
    return MAIL_ERROR_INVAL;
  
  if ((stat_info.st_mode & 00777) != 0700) {
    r = chmod(tmp_dir, 0700);
    if (r < 0)
      return MAIL_ERROR_FILE;
  }
  
  return MAIL_NO_ERROR;
}

struct mailprivacy * mailprivacy_new(char * tmp_dir, int make_alternative)
{
  struct mailprivacy * privacy;
  
  privacy = malloc(sizeof(* privacy));
  if (privacy == NULL)
    goto err;
  
  privacy->tmp_dir = strdup(tmp_dir); 
  if (privacy->tmp_dir == NULL)
    goto free;
  
  privacy->msg_ref = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (privacy->msg_ref == NULL)
    goto free_tmp_dir;
  
  privacy->mmapstr = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (privacy->mmapstr == NULL)
    goto free_msg_ref;
  
  privacy->mime_ref = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (privacy->mime_ref == NULL)
    goto free_mmapstr;
  
  privacy->protocols = carray_new(16);
  if (privacy->protocols == NULL)
    goto free_mime_ref;

  privacy->make_alternative = make_alternative;
  
  return privacy;
  
 free_mime_ref:
  chash_free(privacy->mime_ref);
 free_mmapstr:
  chash_free(privacy->mmapstr);
 free_msg_ref:
  chash_free(privacy->msg_ref);
 free_tmp_dir:
  free(privacy->tmp_dir);
 free:
  free(privacy);
 err:
  return NULL;
}

void mailprivacy_free(struct mailprivacy * privacy)
{
  carray_free(privacy->protocols);
  chash_free(privacy->mime_ref);
  chash_free(privacy->mmapstr);
  chash_free(privacy->msg_ref);
  free(privacy->tmp_dir);
  free(privacy);
}

static int msg_is_modified(struct mailprivacy * privacy,
    mailmessage * msg)
{
  chashdatum key;
  chashdatum data;
  int r;
  
  if (privacy == NULL)
    return 0;
  
  key.data = &msg;
  key.len = sizeof(msg);
  
  r = chash_get(privacy->msg_ref, &key, &data);
  if (r < 0)
    return 0;
  else
    return 1;
}

static int register_msg(struct mailprivacy * privacy,
    mailmessage * msg)
{
  chashdatum key;
  chashdatum data;
  int r;
  
  if (privacy == NULL)
    return MAIL_NO_ERROR;
  
  key.data = &msg;
  key.len = sizeof(msg);
  data.data = msg;
  data.len = 0;
  
  r = chash_set(privacy->msg_ref, &key, &data, NULL);
  if (r < 0)
    return MAIL_ERROR_MEMORY;
  else
    return MAIL_NO_ERROR;
}

static void unregister_message(struct mailprivacy * privacy,
    mailmessage * msg)
{
  chashdatum key;
  
  key.data = &msg;
  key.len = sizeof(msg);
  
  chash_delete(privacy->msg_ref, &key, NULL);
}

static int result_is_mmapstr(struct mailprivacy * privacy, char * str)
{
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = &str;
  key.len = sizeof(str);
  
  r = chash_get(privacy->mmapstr, &key, &data);
  if (r < 0)
    return 0;
  else
    return 1;
}

static int register_result_mmapstr(struct mailprivacy * privacy,
    char * content)
{
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = &content;
  key.len = sizeof(content);
  data.data = content;
  data.len = 0;
  
  r = chash_set(privacy->mmapstr, &key, &data, NULL);
  if (r < 0)
    return MAIL_ERROR_MEMORY;
  
  return 0;
}

static void unregister_result_mmapstr(struct mailprivacy * privacy,
    char * str)
{
  chashdatum key;
  
  mmap_string_unref(str);
  
  key.data = &str;
  key.len = sizeof(str);
  
  chash_delete(privacy->mmapstr, &key, NULL);
}

static int register_mime(struct mailprivacy * privacy,
    struct mailmime * mime)
{
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = &mime;
  key.len = sizeof(mime);
  data.data = mime;
  data.len = 0;
  
  r = chash_set(privacy->mime_ref, &key, &data, NULL);
  if (r < 0)
    return MAIL_ERROR_MEMORY;
  else
    return MAIL_NO_ERROR;
}

static void unregister_mime(struct mailprivacy * privacy,
    struct mailmime * mime)
{
  chashdatum key;
  
  key.data = &mime;
  key.len = sizeof(mime);
  
  chash_delete(privacy->mime_ref, &key, NULL);
}

static int mime_is_registered(struct mailprivacy * privacy,
    struct mailmime * mime)
{
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = &mime;
  key.len = sizeof(mime);
  
  r = chash_get(privacy->mime_ref, &key, &data);
  if (r < 0)
    return 0;
  else
    return 1;
}

static int recursive_register_mime(struct mailprivacy * privacy,
    struct mailmime * mime)
{
  clistiter * cur;
  int r;
  
  r = register_mime(privacy, mime);
  if (r != MAIL_NO_ERROR)
    return r;
  
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    break;
    
  case MAILMIME_MULTIPLE:
    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * child;
      
      child = clist_content(cur);
      
      r = recursive_register_mime(privacy, child);
      if (r != MAIL_NO_ERROR)
        return r;
    }
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime) {
      r = recursive_register_mime(privacy,
          mime->mm_data.mm_message.mm_msg_mime);
      if (r != MAIL_NO_ERROR)
        return r;
    }
    break;
  }
  
  return MAIL_NO_ERROR;
}

void mailprivacy_recursive_unregister_mime(struct mailprivacy * privacy,
    struct mailmime * mime)
{
  clistiter * cur;
  
  unregister_mime(privacy, mime);
  
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    break;
    
  case MAILMIME_MULTIPLE:
    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * child;
      
      child = clist_content(cur);
      
      mailprivacy_recursive_unregister_mime(privacy, child);
    }
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime)
      mailprivacy_recursive_unregister_mime(privacy,
          mime->mm_data.mm_message.mm_msg_mime);
    break;
  }
}

static void recursive_clear_registered_mime(struct mailprivacy * privacy,
    struct mailmime * mime)
{
  clistiter * cur;
  struct mailmime_data * data;
  
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    if (mime_is_registered(privacy, mime)) {
      data = mime->mm_data.mm_single;
      if (data != NULL) {
        if (data->dt_type == MAILMIME_DATA_FILE)
          unlink(data->dt_data.dt_filename);
      }
    }
    break;
    
  case MAILMIME_MULTIPLE:
    if (mime_is_registered(privacy, mime)) {
      data = mime->mm_data.mm_multipart.mm_preamble;
      if (data != NULL) {
        if (data->dt_type == MAILMIME_DATA_FILE)
          unlink(data->dt_data.dt_filename);
      }
      data = mime->mm_data.mm_multipart.mm_epilogue;
      if (data != NULL) {
        if (data->dt_type == MAILMIME_DATA_FILE)
          unlink(data->dt_data.dt_filename);
      }
    }
    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * child;
      
      child = clist_content(cur);
      
      recursive_clear_registered_mime(privacy, child);
    }
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime)
      recursive_clear_registered_mime(privacy,
          mime->mm_data.mm_message.mm_msg_mime);
    break;
  }
  unregister_mime(privacy, mime);
}


/* **************************************************** */
/* fetch operations start here */


static void recursive_clear_registered_mime(struct mailprivacy * privacy,
    struct mailmime * mime);

#if 0
static void display_recursive_part(struct mailmime * mime)
{
  clistiter * cur;
  
  fprintf(stderr, "part %p\n", mime->mm_body);
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    fprintf(stderr, "single %p - %i\n", mime->mm_data.mm_single,
        mime->mm_data.mm_single->dt_type);
    if (mime->mm_data.mm_single->dt_type == MAILMIME_DATA_TEXT) {
      fprintf(stderr, "data : %p %i\n",
          mime->mm_data.mm_single->dt_data.dt_text.dt_data,
          mime->mm_data.mm_single->dt_data.dt_text.dt_length);
    }
    break;
  case MAILMIME_MESSAGE:
    fprintf(stderr, "message %p\n", mime->mm_data.mm_message.mm_msg_mime);
    display_recursive_part(mime->mm_data.mm_message.mm_msg_mime);
    break;
  case MAILMIME_MULTIPLE:
    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ; cur != NULL ;
        cur = clist_next(cur)) {

      fprintf(stderr, "multipart\n");
      display_recursive_part(clist_content(cur));
    }
    break;
  }
}
#endif

int mailprivacy_msg_get_bodystructure(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime ** result)
{
  int r;
  struct mailmime * mime;
  
  if (msg_info->msg_mime != NULL)
    return MAIL_NO_ERROR;
  
  if (msg_is_modified(privacy, msg_info))
    return MAIL_NO_ERROR;
  
  r = mailmessage_get_bodystructure(msg_info, &mime);
  if (r != MAIL_NO_ERROR)
    return r;
  
  /* modification on message if necessary */
  r = recursive_check_privacy(privacy, msg_info, msg_info->msg_mime);
  if (r != MAIL_NO_ERROR) {
    * result = msg_info->msg_mime;
    return MAIL_NO_ERROR;
  }
  
  r = register_msg(privacy, msg_info);
  if (r != MAIL_NO_ERROR) {
    recursive_clear_registered_mime(privacy, mime);
    mailmessage_flush(msg_info);
    return MAIL_ERROR_MEMORY;
  }
  
  * result = msg_info->msg_mime;

  return MAIL_NO_ERROR;
}

void mailprivacy_msg_flush(struct mailprivacy * privacy,
    mailmessage * msg_info)
{
  if (msg_is_modified(privacy, msg_info)) {
    /* remove all modified parts */
    if (msg_info->msg_mime != NULL)
      recursive_clear_registered_mime(privacy, msg_info->msg_mime);
    unregister_message(privacy, msg_info);
  }
  
  mailmessage_flush(msg_info);
}

static int fetch_registered_part(struct mailprivacy * privacy,
    int (* fetch_section)(mailmessage *, struct mailmime *,
        char **, size_t *),
    struct mailmime * mime,
    char ** result, size_t * result_len)
{
  mailmessage * dummy_msg;
  int res;
  char * content;
  size_t content_len;
  int r;
  
  dummy_msg = mime_message_init(NULL);
  if (dummy_msg == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = mime_message_set_tmpdir(dummy_msg, privacy->tmp_dir);
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto free_msg;
  }
  
  r = fetch_section(dummy_msg, mime, &content, &content_len);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_msg;
  }
  
  r = register_result_mmapstr(privacy, content);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_fetch;
  }
  
  mailmessage_free(dummy_msg);
  
  * result = content;
  * result_len = content_len;
  
  return MAIL_NO_ERROR;
  
 free_fetch:
  mailmessage_fetch_result_free(dummy_msg, content);
 free_msg:
  mailmessage_free(dummy_msg);
 err:
  return res;
}

int mailprivacy_msg_fetch_section(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result, size_t * result_len)
{
  if (msg_is_modified(privacy, msg_info) &&
      mime_is_registered(privacy, mime)) {
    return fetch_registered_part(privacy, mailmessage_fetch_section,
        mime, result, result_len);
  }

  return mailmessage_fetch_section(msg_info, mime, result, result_len);
}

int mailprivacy_msg_fetch_section_header(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result,
    size_t * result_len)
{
  if (msg_is_modified(privacy, msg_info) &&
      mime_is_registered(privacy, mime)) {
    return fetch_registered_part(privacy, mailmessage_fetch_section_header,
        mime, result, result_len);
  }
  
  return mailmessage_fetch_section_header(msg_info, mime, result, result_len);
}

int mailprivacy_msg_fetch_section_mime(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result,
    size_t * result_len)
{
  if (msg_is_modified(privacy, msg_info) &&
      mime_is_registered(privacy, mime)) {
    return fetch_registered_part(privacy, mailmessage_fetch_section_mime,
        mime, result, result_len);
  }
  
  return mailmessage_fetch_section_mime(msg_info, mime, result, result_len);
}

int mailprivacy_msg_fetch_section_body(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result,
    size_t * result_len)
{
  if (msg_is_modified(privacy, msg_info) &&
      mime_is_registered(privacy, mime)) {
    return fetch_registered_part(privacy, mailmessage_fetch_section_body,
        mime, result, result_len);
  }
  
  return mailmessage_fetch_section_body(msg_info, mime, result, result_len);
}

void mailprivacy_msg_fetch_result_free(struct mailprivacy * privacy,
    mailmessage * msg_info,
    char * msg)
{
  if (msg == NULL)
    return;
  
  if (msg_is_modified(privacy, msg_info)) {
    if (result_is_mmapstr(privacy, msg)) {
      unregister_result_mmapstr(privacy, msg);
      return;
    }
  }
  
  mailmessage_fetch_result_free(msg_info, msg);
}

int mailprivacy_msg_fetch(struct mailprivacy * privacy,
    mailmessage * msg_info,
    char ** result,
    size_t * result_len)
{
  return mailmessage_fetch(msg_info, result, result_len);
}

int mailprivacy_msg_fetch_header(struct mailprivacy * privacy,
    mailmessage * msg_info,
    char ** result,
    size_t * result_len)
{
  return mailmessage_fetch_header(msg_info, result, result_len);
}

/* end of fetch operations */
/* **************************************************** */

static int privacy_handler(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result);



static struct mailmime *
mime_add_alternative(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime,
    struct mailmime * alternative)
{
  struct mailmime * multipart;
  int r;
  struct mailmime * mime_copy;
  char original_filename[PATH_MAX];
  
  if (mime->mm_parent == NULL)
    goto err;
  
  r = mailmime_new_with_content("multipart/alternative", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR)
    goto err;
  
  r = mailmime_smart_add_part(multipart, alternative);
  if (r != MAILIMF_NO_ERROR) {
    goto free_multipart;
  }

  /* get copy of mime part "mime" and set parts */
  
  r = mailprivacy_fetch_mime_body_to_file(privacy,
      original_filename, sizeof(original_filename),
      msg, mime);
  if (r != MAIL_NO_ERROR)
    goto detach_alternative;
  
  r = mailprivacy_get_part_from_file(privacy, 0, 0,
      original_filename, &mime_copy);
  unlink(original_filename);
  if (r != MAIL_NO_ERROR) {
    goto detach_alternative;
  }
  
  r = mailmime_smart_add_part(multipart, mime_copy);
  if (r != MAILIMF_NO_ERROR) {
    goto free_mime_copy;
  }
  
  r = recursive_register_mime(privacy, multipart);
  if (r != MAIL_NO_ERROR)
    goto detach_mime_copy;
  
  mailmime_substitute(mime, multipart);
  
  mailmime_free(mime);
  
  return multipart;
  
 detach_mime_copy:
  mailprivacy_recursive_unregister_mime(privacy, multipart);
  mailmime_remove_part(alternative);
 free_mime_copy:
  mailprivacy_mime_clear(mime_copy);
  mailmime_free(mime_copy);
 detach_alternative:
  mailmime_remove_part(alternative);
 free_multipart:
  mailmime_free(multipart);
 err:
  return NULL;
}

/*
  recursive_check_privacy returns MAIL_NO_ERROR if at least one 
  part is using a privacy protocol.
*/

static int recursive_check_privacy(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime)
{
  int r;
  clistiter * cur;
  struct mailmime * alternative;
  int res;
  struct mailmime * multipart;
  
  if (privacy == NULL)
    return MAIL_NO_ERROR;
  
  if (mime_is_registered(privacy, mime))
    return MAIL_ERROR_INVAL;
  
  r = privacy_handler(privacy, msg, mime, &alternative);
  if (r == MAIL_NO_ERROR) {
    if (privacy->make_alternative) {
      multipart = mime_add_alternative(privacy, msg, mime, alternative);
      if (multipart == NULL) {
        mailprivacy_mime_clear(alternative);
        mailmime_free(alternative);
        return MAIL_ERROR_MEMORY;
      }
    }
    else {
      mailmime_substitute(mime, alternative);
      mailmime_free(mime);
      mime = NULL;
    }
    
    return MAIL_NO_ERROR;
  }
  else {
    switch (mime->mm_type) {
    case MAILMIME_SINGLE:
      return MAIL_ERROR_INVAL;
    
    case MAILMIME_MULTIPLE:
      res = MAIL_ERROR_INVAL;
    
      for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
          cur != NULL ; cur = clist_next(cur)) {
        struct mailmime * child;
      
        child = clist_content(cur);
      
        r = recursive_check_privacy(privacy, msg, child);
        if (r == MAIL_NO_ERROR)
          res = MAIL_NO_ERROR;
      }
    
      return res;
    
    case MAILMIME_MESSAGE:
      if (mime->mm_data.mm_message.mm_msg_mime != NULL)
        return recursive_check_privacy(privacy, msg,
            mime->mm_data.mm_message.mm_msg_mime);
      return MAIL_ERROR_INVAL;

    default:
      return MAIL_ERROR_INVAL;
    }
  }
}

static int privacy_handler(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  int r;
  struct mailmime * alternative_mime;
  unsigned int i;
  
  alternative_mime = NULL;
  for(i = 0 ; i < carray_count(privacy->protocols) ; i ++) {
    struct mailprivacy_protocol * protocol;
    
    protocol = carray_get(privacy->protocols, i);
    
    if (protocol->decrypt != NULL) {
      r = protocol->decrypt(privacy, msg, mime, &alternative_mime);
      if (r == MAIL_NO_ERROR) {
        
        * result = alternative_mime;
        
        return MAIL_NO_ERROR;
      }
    }
  }
  
  return MAIL_ERROR_INVAL;
}

int mailprivacy_register(struct mailprivacy * privacy,
    struct mailprivacy_protocol * protocol)
{
  int r;
  
  r = carray_add(privacy->protocols, protocol, NULL);
  if (r < 0)
    return MAIL_ERROR_MEMORY;
  
  return MAIL_NO_ERROR;
}

void mailprivacy_unregister(struct mailprivacy * privacy,
    struct mailprivacy_protocol * protocol)
{
  unsigned int i;
  
  for(i = 0 ; i < carray_count(privacy->protocols) ; i ++) {
    if (carray_get(privacy->protocols, i) == protocol) {
      carray_delete(privacy->protocols, i);
      return;
    }
  }
}

static struct mailprivacy_protocol *
get_protocol(struct mailprivacy * privacy, char * privacy_driver)
{
  unsigned int i;
  
  for(i = 0 ; i < carray_count(privacy->protocols) ; i ++) {
    struct mailprivacy_protocol * protocol;
    
    protocol = carray_get(privacy->protocols, i);
    if (strcasecmp(protocol->name, privacy_driver) == 0)
      return protocol;
  }
  
  return NULL;
}

static struct mailprivacy_encryption *
get_encryption(struct mailprivacy_protocol * protocol,
    char * privacy_encryption)
{
  int i;
  
  for(i = 0 ; i < protocol->encryption_count ; i ++) {
    struct mailprivacy_encryption * encryption;
    
    encryption = &protocol->encryption_tab[i];
    if (strcasecmp(encryption->name, privacy_encryption) == 0)
      return encryption;
  }
  
  return NULL;
}

int mailprivacy_encrypt(struct mailprivacy * privacy,
    char * privacy_driver, char * privacy_encryption,
    struct mailmime * mime,
    struct mailmime ** result)
{
  return mailprivacy_encrypt_msg(privacy, privacy_driver, privacy_encryption,
      NULL,  mime, result);
}

int mailprivacy_encrypt_msg(struct mailprivacy * privacy,
    char * privacy_driver, char * privacy_encryption,
    mailmessage * msg,
    struct mailmime * mime,
    struct mailmime ** result)
{
  struct mailprivacy_protocol * protocol;
  struct mailprivacy_encryption * encryption;
  int r;

  protocol = get_protocol(privacy, privacy_driver);
  if (protocol == NULL)
    return MAIL_ERROR_INVAL;
  
  encryption = get_encryption(protocol, privacy_encryption);
  if (encryption == NULL)
    return MAIL_ERROR_INVAL;
  
  if (encryption->encrypt == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;
  
  r = encryption->encrypt(privacy, msg, mime, result);
  if (r != MAIL_NO_ERROR)
    return r;
  
  return MAIL_NO_ERROR;
}

char * mailprivacy_get_encryption_name(struct mailprivacy * privacy,
    char * privacy_driver, char * privacy_encryption)
{
  struct mailprivacy_protocol * protocol;
  struct mailprivacy_encryption * encryption;

  protocol = get_protocol(privacy, privacy_driver);
  if (protocol == NULL)
    return NULL;
  
  encryption = get_encryption(protocol, privacy_encryption);
  if (encryption == NULL)
    return NULL;
  
  return encryption->description;
}

int mailprivacy_is_encrypted(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime)
{
  unsigned int i;
  
  if (mime_is_registered(privacy, mime))
    return 0;
  
  for(i = 0 ; i < carray_count(privacy->protocols) ; i ++) {
    struct mailprivacy_protocol * protocol;
    
    protocol = carray_get(privacy->protocols, i);
    
    if (protocol->is_encrypted != NULL) {
      if (protocol->is_encrypted(privacy, msg, mime))
        return 1;
    }
  }
  
  return 0;
}

void mailprivacy_debug(struct mailprivacy * privacy, FILE * f)
{
  fprintf(f, "privacy debug -- begin\n");
  fprintf(f, "registered message: %i\n", chash_count(privacy->msg_ref));
  fprintf(f, "registered MMAPStr: %i\n", chash_count(privacy->mmapstr));
  fprintf(f, "registered mailmime: %i\n", chash_count(privacy->mime_ref));
  fprintf(f, "privacy debug -- end\n");
}
