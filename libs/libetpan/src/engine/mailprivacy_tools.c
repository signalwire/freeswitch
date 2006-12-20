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
 * $Id: mailprivacy_tools.c,v 1.10 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailprivacy_tools.h"

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libetpan/mailmessage.h>
#include <ctype.h>
#include "mailprivacy.h"
#include <libetpan/libetpan-config.h>
#include <libetpan/data_message_driver.h>

void mailprivacy_mime_clear(struct mailmime * mime)
{
  struct mailmime_data * data;
  clistiter * cur;
  
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    data = mime->mm_data.mm_single;
    if (data != NULL) {
      if (data->dt_type == MAILMIME_DATA_FILE)
        unlink(data->dt_data.dt_filename);
    }
    break;
    
  case MAILMIME_MULTIPLE:
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
    
    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * submime;
      
      submime = clist_content(cur);
      
      mailprivacy_mime_clear(submime);
    }
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
      mailprivacy_mime_clear(mime->mm_data.mm_message.mm_msg_mime);
    }
    break;
  }
}


static FILE * get_tmp_file(char * filename)
{
  int fd;
  mode_t old_mask;
  FILE * f;
  
  old_mask = umask(0077);
  fd = mkstemp(filename);
  umask(old_mask);
  if (fd == -1)
    return NULL;
  
  f = fdopen(fd, "r+");
  if (f == NULL) {
    close(fd);
    unlink(filename);
  }
  
  return f;
}

FILE * mailprivacy_get_tmp_file(struct mailprivacy * privacy,
    char * filename, size_t size)
{
  snprintf(filename, size, "%s/libetpan-privacy-XXXXXX", privacy->tmp_dir);
  return get_tmp_file(filename);
}

int mailprivacy_get_tmp_filename(struct mailprivacy * privacy,
    char * filename, size_t size)
{
  FILE * f;
  
  f = mailprivacy_get_tmp_file(privacy, filename, size);
  if (f == NULL)
    return MAIL_ERROR_FILE;
  
  fclose(f);
  
  return MAIL_NO_ERROR;
}

static char * dup_file(struct mailprivacy * privacy,
    char * source_filename)
{
  char filename[PATH_MAX];
  FILE * dest_f;
  int r;
  struct stat stat_info;
  char * dest_filename;
  char * mapping;
  size_t written;
  int fd;
  
  dest_f = mailprivacy_get_tmp_file(privacy, filename, sizeof(filename));
  if (dest_f == NULL)
    goto err;
  
  dest_filename = strdup(filename);
  if (dest_filename == NULL)
    goto close_dest;
  
  fd = open(source_filename, O_RDONLY);
  if (fd < 0)
    goto free_dest;
  
  r = fstat(fd, &stat_info);
  if (r < 0)
    goto close_src;
  
  mapping = mmap(NULL, stat_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapping == (char *)MAP_FAILED)
    goto close_src;
  
  written = fwrite(mapping, 1, stat_info.st_size, dest_f);
  if (written != (size_t) stat_info.st_size)
    goto unmap;
  
  munmap(mapping, stat_info.st_size);
  close(fd);
  fclose(dest_f);
  
  return dest_filename;
  
 unmap:
  munmap(mapping, stat_info.st_size);
 close_src:
  close(fd);
 free_dest:
  free(dest_filename);
 close_dest:
  fclose(dest_f);
 err:
  return NULL;
}


/*
  mime_data_replace()
  
  write a mime part to a file and change the reference of mailmime_data
  to the file.
*/

static int mime_data_replace(struct mailprivacy * privacy,
    int encoding_type,
    struct mailmime_data * data,
    int reencode)
{
  char filename[PATH_MAX];
  FILE * f;
  size_t written;
  char * dup_filename;
  int res;
  int r;
  int decoded;

  if (data->dt_type != MAILMIME_DATA_TEXT) {
    res = MAIL_NO_ERROR;
    goto err;
  }
  
  f = mailprivacy_get_tmp_file(privacy, filename, sizeof(filename));
  if (f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  decoded = 0;
  if (reencode) {
    if (encoding_type != -1) {
      char * content;
      size_t content_len;
      size_t cur_token;
    
      cur_token = 0;
      r = mailmime_part_parse(data->dt_data.dt_text.dt_data,
          data->dt_data.dt_text.dt_length,
          &cur_token, encoding_type, &content, &content_len);
    
      if (r == MAILIMF_NO_ERROR) {
        /* write decoded */
        written = fwrite(content, 1, content_len, f);
        if (written != content_len) {
          fclose(f);
          unlink(filename);
          res = MAIL_ERROR_FILE;
          goto err;
        }
        mmap_string_unref(content);
      
        decoded = 1;
        data->dt_encoded = 0;
      }
    }
  }
  
  if (!decoded) {
    written = fwrite(data->dt_data.dt_text.dt_data, 1,
        data->dt_data.dt_text.dt_length, f);
    if (written != data->dt_data.dt_text.dt_length) {
      fclose(f);
      unlink(filename);
      res = MAIL_ERROR_FILE;
      goto err;
    }
  }
  
  fclose(f);
  
  dup_filename = strdup(filename);
  if (dup_filename == NULL) {
    unlink(filename);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  data->dt_type = MAILMIME_DATA_FILE;
  data->dt_data.dt_filename = dup_filename;
  
  return MAIL_NO_ERROR;
  
 err:
  return res;
}


/*
  recursive_replace_single_parts()
  
  write all parts of the given mime part to file.
*/

static int recursive_replace_single_parts(struct mailprivacy * privacy,
    struct mailmime * mime, int reencode)
{
  int r;
  int res;
  clistiter * cur;
  
  mime->mm_mime_start = NULL;
  
  switch(mime->mm_type) {
  case MAILMIME_SINGLE:
    if (mime->mm_data.mm_single != NULL) {
      int encoding_type;
      struct mailmime_single_fields single_fields;
      
      mailmime_single_fields_init(&single_fields, mime->mm_mime_fields,
          mime->mm_content_type);
      
      if (single_fields.fld_encoding != NULL)
        encoding_type = single_fields.fld_encoding->enc_type;
      else
        encoding_type = -1;
      
      r = mime_data_replace(privacy, encoding_type,
          mime->mm_data.mm_single, reencode);
      if (r != MAIL_NO_ERROR) {
        res = r;
        goto err;
      }
    }
    break;
    
  case MAILMIME_MULTIPLE:
    if (mime->mm_data.mm_multipart.mm_preamble != NULL) {
      r = mime_data_replace(privacy, -1,
          mime->mm_data.mm_multipart.mm_preamble, reencode);
      if (r != MAIL_NO_ERROR) {
        res = r;
        goto err;
      }
    }
    
    if (mime->mm_data.mm_multipart.mm_epilogue != NULL) {
      r = mime_data_replace(privacy, -1,
          mime->mm_data.mm_multipart.mm_epilogue, reencode);
      if (r != MAIL_NO_ERROR) {
        res = r;
        goto err;
      }
    }
    
    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * child;
      
      child = clist_content(cur);
      
      r = recursive_replace_single_parts(privacy, child, reencode);
      if (r != MAIL_NO_ERROR) {
        res = r;
        goto err;
      }
    }
    
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
      r = recursive_replace_single_parts(privacy,
          mime->mm_data.mm_message.mm_msg_mime, reencode);
      if (r != MAIL_NO_ERROR) {
        res = r;
        goto err;
      }
    }
    break;
  }
  
  return MAIL_NO_ERROR;
  
 err:
  return res;
}

/*
  mailprivacy_get_mime()
  
  parse the message in MIME structure,
  all single MIME parts are stored in files.
  
  privacy can be set to NULL to disable privacy check.
*/

int mailprivacy_get_mime(struct mailprivacy * privacy,
    int check_privacy, int reencode,
    char * content, size_t content_len,
    struct mailmime ** result_mime)
{
  struct mailmime * mime;
  mailmessage * msg;
  int r;
  int res;

#if 0
  int check_privacy;
  
  check_privacy = (privacy != NULL);
#endif
  
  /*
    use message data driver, get bodystructure and
    convert all the data part in MAILMIME_SINGLE to files.
  */
  
  msg = data_message_init(content, content_len);
  if (msg == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
#if 0
  if (msg->mime == NULL) {
    if (check_privacy) {
      r = mailprivacy_msg_get_bodystructure(privacy, msg, &mime);
    }
    else {
      /*
        don't use etpan_msg_get_bodystructure because it is not useful
        and to avoid loops due to security part
      */
      r = mailmessage_get_bodystructure(msg, &mime);
    }
  }
  else {
    mime = msg->mime;
  }
#endif

  if (check_privacy)
    r = mailprivacy_msg_get_bodystructure(privacy, msg, &mime);
  else
    r = mailmessage_get_bodystructure(msg, &mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_msg;
  }

  /*
    should be done so that the MIME structure need not to be unregistered.
  */
  mailprivacy_recursive_unregister_mime(privacy, mime);

  r = recursive_replace_single_parts(privacy, mime, reencode);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto clear_mime;
  }
  
  data_message_detach_mime(msg);
#if 0
  if (check_privacy)
    mailprivacy_msg_flush(privacy, msg);
  else
    mailmessage_flush(msg);
#endif
  mailprivacy_msg_flush(privacy, msg);
  mailmessage_free(msg);
  
  * result_mime = mime;
  
  return MAIL_NO_ERROR;
  
 clear_mime:
  mailprivacy_mime_clear(mime);
  mailprivacy_msg_flush(privacy, msg);
 free_msg:
  mailmessage_free(msg);
 err:
  return res;
}

#ifndef LIBETPAN_SYSTEM_BASENAME
static char * libetpan_basename(char * filename)
{
  char * next;
  char * p;
  
  p = filename;
  next = strchr(p, '/');
  
  while (next != NULL) {
    p = next;
    next = strchr(p + 1, '/');
  }
  
  if (p == filename)
    return filename;
  else
    return p + 1;
}
#else
#define libetpan_basename(a) basename(a)
#endif

struct mailmime *
mailprivacy_new_file_part(struct mailprivacy * privacy,
    char * filename,
    char * default_content_type, int default_encoding)
{
  char basename_buf[PATH_MAX];
  char * name;
  struct mailmime_mechanism * encoding;
  struct mailmime_content * content;
  struct mailmime * mime;
  int r;
  char * dup_filename;
  struct mailmime_fields * mime_fields;
  int encoding_type;
  char * content_type_str;
  int do_encoding;
  
  if (filename != NULL) {
    strncpy(basename_buf, filename, PATH_MAX);
    name = libetpan_basename(basename_buf);
  }
  else {
    name = NULL;
  }
  
  encoding = NULL;
  
  /* default content-type */
  if (default_content_type == NULL)
    content_type_str = "application/octet-stream";
  else
    content_type_str = default_content_type;
    
  content = mailmime_content_new_with_str(content_type_str);
  if (content == NULL) {
    goto free_content;
  }
  
  do_encoding = 1;
  if (content->ct_type->tp_type == MAILMIME_TYPE_COMPOSITE_TYPE) {
    struct mailmime_composite_type * composite;
    
    composite = content->ct_type->tp_data.tp_composite_type;
    
    switch (composite->ct_type) {
    case MAILMIME_COMPOSITE_TYPE_MESSAGE:
      if (strcasecmp(content->ct_subtype, "rfc822") == 0)
        do_encoding = 0;
      break;

    case MAILMIME_COMPOSITE_TYPE_MULTIPART:
      do_encoding = 0;
      break;
    }
  }
  
  if (do_encoding) {
    if (default_encoding == -1)
      encoding_type = MAILMIME_MECHANISM_BASE64;
    else
      encoding_type = default_encoding;
    
    /* default Content-Transfer-Encoding */
    encoding = mailmime_mechanism_new(encoding_type, NULL);
    if (encoding == NULL) {
      goto free_content;
    }
  }
  
  mime_fields = mailmime_fields_new_with_data(encoding,
      NULL, NULL, NULL, NULL);
  if (mime_fields == NULL) {
    goto free_content;
  }
  
  mime = mailmime_new_empty(content, mime_fields);
  if (mime == NULL) {
    goto free_mime_fields;
  }
  
  if ((filename != NULL) && (mime->mm_type == MAILMIME_SINGLE)) {
    /*
      duplicates the file so that the file can be deleted when
      the MIME part is done
    */
    dup_filename = dup_file(privacy, filename);
    if (dup_filename == NULL) {
      goto free_mime;
    }
  
    r = mailmime_set_body_file(mime, dup_filename);
    if (r != MAILIMF_NO_ERROR) {
      free(dup_filename);
      goto free_mime;
    }
  }
  
  return mime;
  
 free_mime:
  mailmime_free(mime);
  goto err;
 free_mime_fields:
  mailmime_fields_free(mime_fields);
  mailmime_content_free(content);
  goto err;
 free_content:
  if (encoding != NULL)
    mailmime_mechanism_free(encoding);
  if (content != NULL)
    mailmime_content_free(content);
 err:
  return NULL;
}


int mailmime_substitute(struct mailmime * old_mime,
    struct mailmime * new_mime)
{
  struct mailmime * parent;
  
  parent = old_mime->mm_parent;
  if (parent == NULL)
    return MAIL_ERROR_INVAL;
  
  if (old_mime->mm_parent_type == MAILMIME_MESSAGE)
    parent->mm_data.mm_message.mm_msg_mime = new_mime;
  else /* MAILMIME_MULTIPLE */
    old_mime->mm_multipart_pos->data = new_mime;
  new_mime->mm_parent = parent;
  new_mime->mm_parent_type = old_mime->mm_parent_type;
  
  /* detach old_mime */
  old_mime->mm_parent = NULL;
  old_mime->mm_parent_type = MAILMIME_NONE;
  
  return MAIL_NO_ERROR;
}



/* write mime headers and body to a file, CR LF fixed */

int mailprivacy_fetch_mime_body_to_file(struct mailprivacy * privacy,
    char * filename, size_t size,
    mailmessage * msg, struct mailmime * mime)
{
  int r;
  int res;
  FILE * f;
  char * content;
  size_t content_len;
  int col;
  
  if (mime->mm_parent_type == MAILMIME_NONE) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  f = mailprivacy_get_tmp_file(privacy, filename, size);
  if (f == NULL) {
    res = MAIL_ERROR_FETCH;
    goto err;
  }
  
  r = mailprivacy_msg_fetch_section_mime(privacy, msg, mime,
      &content, &content_len);
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FETCH;
    goto close;
  }
  
  col = 0;
  r = mailimf_string_write(f, &col, content, content_len);
  mailprivacy_msg_fetch_result_free(privacy, msg, content);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto close;
  }
  
  r = mailprivacy_msg_fetch_section(privacy, msg, mime,
      &content, &content_len);
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FETCH;
    goto close;
  }
  
  r = mailimf_string_write(f, &col, content, content_len);
  mailprivacy_msg_fetch_result_free(privacy, msg, content);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto close;
  }
  
  fclose(f);
  
  return MAIL_NO_ERROR;
  
 close:
  fclose(f);
  unlink(filename);
 err:
  return res;
}


int mailprivacy_get_part_from_file(struct mailprivacy * privacy,
    int check_security, int reencode, char * filename,
    struct mailmime ** result_mime)
{
  int fd;
  struct mailmime * mime;
  int r;
  struct stat stat_info;
  int res;
  char * mapping;

  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }

  r = fstat(fd, &stat_info);
  if (r < 0) {
    res = MAIL_ERROR_FILE;
    goto close;
  }

  mapping = mmap(NULL, stat_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapping == (char *)MAP_FAILED) {
    res = MAIL_ERROR_FILE;
    goto close;
  }
  
  mime = NULL;
  /* check recursive parts if privacy is set */
  r = mailprivacy_get_mime(privacy, check_security, reencode,
      mapping, stat_info.st_size, &mime);
  if (r != MAIL_NO_ERROR) {
    res =  r;
    goto unmap;
  }

  if (mime->mm_type == MAILMIME_MESSAGE) {
    struct mailmime * submime;
    
    submime = mime->mm_data.mm_message.mm_msg_mime;
    if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
      mailmime_remove_part(submime);
      mailmime_free(mime);
      
      mime = submime;
    }
  }

  munmap(mapping, stat_info.st_size);
  
  close(fd);

  * result_mime = mime;

  return MAIL_NO_ERROR;
  
 unmap:
  munmap(mapping, stat_info.st_size);
 close:
  close(fd);
 err:
  return res;
}

int mail_quote_filename(char * result, size_t size, char * path)
{
  char * p;
  char * result_p;
  size_t remaining;

  result_p = result;
  remaining = size;

  for(p = path ; * p != '\0' ; p ++) {
    int quote_not_needed;
    
#if 0
    quote_not_needed = (isalpha(* p) || isdigit(* p) || (* p == '/'));
#else
    quote_not_needed = (* p != '\\') && (* p != '\'') && (* p != '\"');
#endif
    if (quote_not_needed) {
      if (remaining > 0) {
        * result_p = * p;
        result_p ++; 
        remaining --;
      }
      else {
        result[size - 1] = '\0';
        return -1;
      }
    }
    else { 
      if (remaining >= 2) {
        * result_p = '\\';
        result_p ++; 
        * result_p = * p;
        result_p ++; 
        remaining -= 2;
      }
      else {
        result[size - 1] = '\0';
        return -1;
      }
    }
  }
  if (remaining > 0) {
    * result_p = '\0';
  }
  else {
    result[size - 1] = '\0';
    return -1;
  }
  
  return 0;
}


static void prepare_mime_single(struct mailmime * mime)
{
  struct mailmime_single_fields single_fields;
  int encoding;
  int r;
  
  if (mime->mm_mime_fields != NULL) {
    mailmime_single_fields_init(&single_fields, mime->mm_mime_fields,
        mime->mm_content_type);
    if (single_fields.fld_encoding != NULL) {
      encoding = single_fields.fld_encoding->enc_type;
      switch (encoding) {
      case MAILMIME_MECHANISM_8BIT:
      case MAILMIME_MECHANISM_7BIT:
      case MAILMIME_MECHANISM_BINARY:
        single_fields.fld_encoding->enc_type =
          MAILMIME_MECHANISM_QUOTED_PRINTABLE;
        break;
      }
    }
    else {
      struct mailmime_mechanism * mechanism;
      struct mailmime_field * field;
      
      mechanism =
        mailmime_mechanism_new(MAILMIME_MECHANISM_QUOTED_PRINTABLE, NULL);
      if (mechanism == NULL)
        return;
      
      field = mailmime_field_new(MAILMIME_FIELD_TRANSFER_ENCODING,
          NULL, mechanism, NULL, NULL, 0, NULL, NULL);
      if (field == NULL) {
        mailmime_mechanism_free(mechanism);
        return;
      }
      
      r = clist_append(mime->mm_mime_fields->fld_list, field);
      if (r < 0) {
        mailmime_field_free(field);
        return;
      }
    }
  }
  
  if (mime->mm_type == MAILMIME_SINGLE) {
    switch (mime->mm_data.mm_single->dt_encoding) {
    case MAILMIME_MECHANISM_8BIT:
    case MAILMIME_MECHANISM_7BIT:
    case MAILMIME_MECHANISM_BINARY:
      mime->mm_data.mm_single->dt_encoding =
        MAILMIME_MECHANISM_QUOTED_PRINTABLE;
      mime->mm_data.mm_single->dt_encoded = 0;
      break;
    }
  }
}

/*
  mailprivacy_prepare_mime()
  
  we assume we built ourself the message.
*/

void mailprivacy_prepare_mime(struct mailmime * mime)
{
  clistiter * cur;
  
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    if (mime->mm_data.mm_single != NULL) {
      prepare_mime_single(mime);
    }
    break;
    
  case MAILMIME_MULTIPLE:
    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * child;
      
      child = clist_content(cur);
      
      mailprivacy_prepare_mime(child);
    }
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime) {
      mailprivacy_prepare_mime(mime->mm_data.mm_message.mm_msg_mime);
    }
    break;
  }
}


char * mailprivacy_dup_imf_file(struct mailprivacy * privacy,
    char * source_filename)
{
  char filename[PATH_MAX];
  FILE * dest_f;
  int r;
  struct stat stat_info;
  char * dest_filename;
  char * mapping;
  int fd;
  int col;
  
  dest_f = mailprivacy_get_tmp_file(privacy,
      filename, sizeof(filename));
  if (dest_f == NULL)
    goto err;
  
  dest_filename = strdup(filename);
  if (dest_filename == NULL)
    goto close_dest;
  
  fd = open(source_filename, O_RDONLY);
  if (fd < 0)
    goto free_dest;
  
  r = fstat(fd, &stat_info);
  if (r < 0)
    goto close_src;
  
  mapping = mmap(NULL, stat_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapping == (char *)MAP_FAILED)
    goto close_src;
  
  col = 0;
  r = mailimf_string_write(dest_f, &col, mapping, stat_info.st_size);
  if (r != MAILIMF_NO_ERROR)
    goto unmap;
  
  munmap(mapping, stat_info.st_size);
  close(fd);
  fclose(dest_f);
  
  return dest_filename;
  
 unmap:
  munmap(mapping, stat_info.st_size);
 close_src:
  close(fd);
 free_dest:
  free(dest_filename);
 close_dest:
  fclose(dest_f);
 err:
  return NULL;
}

/* TODO : better function to duplicate mime fields, currenly such inelegant */

struct mailmime_fields *
mailprivacy_mime_fields_dup(struct mailprivacy * privacy,
    struct mailmime_fields * mime_fields)
{
  FILE * f;
  char tmp_file[PATH_MAX];
  int col;
  int r;
  struct mailmime_fields * dup_mime_fields;
  int fd;
  char * mapping;
  struct stat stat_info;
  struct mailimf_fields * fields;
  size_t cur_token;
  
  f = mailprivacy_get_tmp_file(privacy, tmp_file, sizeof(tmp_file));
  if (f == NULL)
    goto err;
  
  col = 0;
  r = mailmime_fields_write(f, &col, mime_fields);
  if (r != MAILIMF_NO_ERROR)
    goto unlink;
  
  fflush(f);
  
  fd = fileno(f);
  if (fd == -1)
    goto unlink;
  
  r = fstat(fd, &stat_info);
  if (r < 0)
    goto unlink;
  
  mapping = mmap(NULL, stat_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapping == (char *)MAP_FAILED)
    goto unlink;
  
  cur_token = 0;
  r = mailimf_optional_fields_parse(mapping, stat_info.st_size,
      &cur_token, &fields);
  if (r != MAILIMF_NO_ERROR)
    goto unmap;
  
  r = mailmime_fields_parse(fields, &dup_mime_fields);
  mailimf_fields_free(fields);
  if (r != MAILIMF_NO_ERROR)
    goto unmap;
  
  munmap(mapping, stat_info.st_size);
  fclose(f);
  unlink(tmp_file);

  return dup_mime_fields;

 unmap:
  munmap(mapping, stat_info.st_size);
 unlink:
  fclose(f);
  unlink(tmp_file);
 err:
  return NULL;
}



struct mailmime_parameter *
mailmime_parameter_dup(struct mailmime_parameter * param)
{
  char * name;
  char * value;
  struct mailmime_parameter * dup_param;

  name = strdup(param->pa_name);
  if (name == NULL)
    goto err;
  
  value = strdup(param->pa_value);
  if (value == NULL)
    goto free_name;
  
  dup_param = mailmime_parameter_new(name, value);
  if (dup_param == NULL)
    goto free_value;
  
  return dup_param;
  
 free_value:
  free(value);
 free_name:
  free(name);
 err:
  return NULL;
}

struct mailmime_composite_type *
mailmime_composite_type_dup(struct mailmime_composite_type * composite_type)
{
  struct mailmime_composite_type * dup_composite;
  char * token;
  
  token = NULL;
  if (composite_type->ct_token != NULL) {
    token = strdup(composite_type->ct_token);
    if (token == NULL)
      goto err;
  }
  
  dup_composite = mailmime_composite_type_new(composite_type->ct_type, token);
  if (dup_composite == NULL)
    goto free_token;
  
  return dup_composite;
  
 free_token:
  if (token != NULL)
    free(token);
 err:
  return NULL;
}

struct mailmime_discrete_type *
mailmime_discrete_type_dup(struct mailmime_discrete_type * discrete_type)
{
  struct mailmime_discrete_type * dup_discrete;
  char * extension;
  
  extension = NULL;
  if (discrete_type->dt_extension != NULL) {
    extension = strdup(discrete_type->dt_extension);
    if (extension == NULL)
      goto err;
  }
  
  dup_discrete = mailmime_discrete_type_new(discrete_type->dt_type, extension);
  if (dup_discrete == NULL)
    goto free_extension;
  
  return dup_discrete;
  
 free_extension:
  if (extension != NULL)
    free(extension);
 err:
  return NULL;
}

struct mailmime_type * mailmime_type_dup(struct mailmime_type * type)
{
  struct mailmime_type * dup_type;
  struct mailmime_discrete_type * discrete_type;
  struct mailmime_composite_type * composite_type;
  
  discrete_type = NULL;
  composite_type = NULL;
  switch (type->tp_type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    discrete_type =
      mailmime_discrete_type_dup(type->tp_data.tp_discrete_type);
    if (discrete_type == NULL)
      goto err;
    break;
    
    composite_type =
      mailmime_composite_type_dup(type->tp_data.tp_composite_type);
    if (composite_type == NULL)
      goto free_discrete;
  }
  
  dup_type = mailmime_type_new(type->tp_type, discrete_type, composite_type);
  if (dup_type == NULL)
    goto free_composite;
  
  return dup_type;
  
 free_composite:
  if (composite_type != NULL)
    mailmime_composite_type_free(composite_type);
 free_discrete:
  if (discrete_type != NULL)
    mailmime_discrete_type_free(discrete_type);
 err:
  return NULL;
}

struct mailmime_content *
mailmime_content_dup(struct mailmime_content * content)
{
  clist * list;
  struct mailmime_type * type;
  int r;
  struct mailmime_content * dup_content;
  char * subtype;

  type = mailmime_type_dup(content->ct_type);
  if (type == NULL)
    goto err;
  
  subtype = strdup(content->ct_subtype);
  if (subtype == NULL)
    goto free_type;
  
  list = clist_new();
  if (list == NULL)
    goto free_subtype;
  
  if (content->ct_parameters != NULL) {
    clistiter * cur;
    
    for(cur = clist_begin(content->ct_parameters) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime_parameter * param;
      
      param = mailmime_parameter_dup(clist_content(cur));
      if (param == NULL)
        goto free_list;
      
      r = clist_append(list, param);
      if (r < 0) {
        mailmime_parameter_free(param);
        goto free_list;
      }
    }
  }
  
  dup_content = mailmime_content_new(type, subtype, list);
  if (dup_content == NULL)
    goto free_list;

  return dup_content;
  
 free_list:
  clist_foreach(list, (clist_func) mailmime_parameter_free, NULL);
 free_subtype:
  free(subtype);
 free_type:
  mailmime_type_free(type);
 err:
  return NULL;
}


struct mailmime_parameter *
mailmime_param_new_with_data(char * name, char * value)
{
  char * param_name;
  char * param_value;
  struct mailmime_parameter * param;

  param_name = strdup(name);
  if (param_name == NULL)
    goto err;
  
  param_value = strdup(value);
  if (param_value == NULL)
    goto free_name;
  
  param = mailmime_parameter_new(param_name, param_value);
  if (param == NULL)
    goto free_value;
  
  return param;
  
 free_value:
  free(param_value);
 free_name:
  free(param_name);
 err:
  return NULL;
}


int mailprivacy_fetch_decoded_to_file(struct mailprivacy * privacy,
    char * filename, size_t size,
    mailmessage * msg, struct mailmime * mime)
{
  int r;
  int res;
  FILE * f;
  char * content;
  size_t content_len;
  size_t written;
  struct mailmime_single_fields single_fields;
  int encoding;
  size_t cur_token;
  char * parsed_content;
  size_t parsed_content_len;
  
  mailmime_single_fields_init(&single_fields, mime->mm_mime_fields,
      mime->mm_content_type);
  if (single_fields.fld_encoding != NULL)
    encoding = single_fields.fld_encoding->enc_type;
  else
    encoding = MAILMIME_MECHANISM_8BIT;
  
  r = mailprivacy_msg_fetch_section(privacy, msg, mime,
      &content, &content_len);
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FETCH;
    goto err;
  }
  
  cur_token = 0;
  r = mailmime_part_parse(content, content_len, &cur_token,
      encoding, &parsed_content, &parsed_content_len);
  mailprivacy_msg_fetch_result_free(privacy, msg, content);
  if (r != MAILIMF_NO_ERROR) {
    res = MAIL_ERROR_PARSE;
    goto err;
  }
  
  f = mailprivacy_get_tmp_file(privacy, filename, size);
  if (f == NULL) {
    res = MAIL_ERROR_FETCH;
    goto free_fetch;
  }
  written = fwrite(parsed_content, 1, parsed_content_len, f);
  if (written != parsed_content_len) {
    res = MAIL_ERROR_FILE;
    goto close;
  }
  fclose(f);
  
  mmap_string_unref(parsed_content);
  
  return MAIL_NO_ERROR;

 close:
  fclose(f);
  unlink(filename);
 free_fetch:
  mmap_string_unref(parsed_content);
 err:
  return res;
}

