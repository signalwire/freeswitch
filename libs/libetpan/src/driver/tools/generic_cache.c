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
 * $Id: generic_cache.c,v 1.32 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "generic_cache.h"

#include "libetpan-config.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#	include <sys/mman.h>
#endif
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "maildriver_types.h"
#include "imfcache.h"
#include "chash.h"
#include "mailmessage.h"
#include "mail_cache_db.h"

int generic_cache_create_dir(char * dirname)
{
  struct stat buf;
  int r;

  r = stat(dirname, &buf);
  if (r != 0) {
 
#ifdef WIN32
	r = mkdir(dirname);
#else
    r = mkdir(dirname, 0700);
#endif

    if (r < 0)
      return MAIL_ERROR_FILE;
  }
  else {
    if (!S_ISDIR(buf.st_mode))
      return MAIL_ERROR_FILE;
  }

  return MAIL_NO_ERROR;
}

int generic_cache_store(char * filename, char * content, size_t length)
{
  int fd;
  char * str;

  fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd == -1)
    return MAIL_ERROR_FILE;

  if (ftruncate(fd, length) < 0)
    return MAIL_ERROR_FILE;
  
  str = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (str == (char *)MAP_FAILED)
    return MAIL_ERROR_FILE;

  memcpy(str, content, length);
  msync(str, length, MS_SYNC);
  munmap(str, length);

  close(fd);

  return MAIL_NO_ERROR;
}

int generic_cache_read(char * filename, char ** result, size_t * result_len)
{
  int fd;
  char * str;
  struct stat buf;
  MMAPString * mmapstr;
  char * content;
  int res;

  if (stat(filename, &buf) < 0) {
    res = MAIL_ERROR_CACHE_MISS;
    goto err;
  }

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    res = MAIL_ERROR_CACHE_MISS;
    goto err;
  }

  str = mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (str == (char *)MAP_FAILED) {
    res = MAIL_ERROR_FILE;
    goto close;
  }

  mmapstr = mmap_string_new_len(str, buf.st_size);
  if (mmapstr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto unmap;
  }
  
  if (mmap_string_ref(mmapstr) < 0) {
    res = MAIL_ERROR_MEMORY;
    goto free;
  }
  
  content = mmapstr->str;

  munmap(str, buf.st_size);
  close(fd);

  * result = content;
  * result_len = buf.st_size;

  return MAIL_NO_ERROR;

 free:
  mmap_string_free(mmapstr);
 unmap:
  munmap(str, buf.st_size);
 close:
  close(fd);
 err:
  return res;
}

static int flags_extension_read(MMAPString * mmapstr, size_t * index,
				clist ** result)
{
  clist * list;
  int r;
  uint32_t count;
  uint32_t i;
  int res;

  r = mailimf_cache_int_read(mmapstr, index, &count);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(i = 0 ; i < count ; i++) {
    char * str;

    r = mailimf_cache_string_read(mmapstr, index, &str);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = clist_append(list, str);
    if (r < 0) {
      free(str);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  * result = list;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) free, NULL);
  clist_free(list);
 err:
  return res;
}

static int generic_flags_read(MMAPString * mmapstr, size_t * index,
			      struct mail_flags ** result)
{
  clist * ext;
  int r;
  struct mail_flags * flags;
  uint32_t value;
  int res;
  
  r = mailimf_cache_int_read(mmapstr, index, &value);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  ext = NULL;
  r = flags_extension_read(mmapstr, index, &ext);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  flags = mail_flags_new(value, ext);
  if (flags == NULL) {
    res = r;
    goto free;
  }

  * result = flags;
  
  return MAIL_NO_ERROR;

 free:
  clist_foreach(ext, (clist_func) free, NULL);
  clist_free(ext);
 err:
  return res;
}

static int flags_extension_write(MMAPString * mmapstr, size_t * index,
				 clist * ext)
{
  int r;
  clistiter * cur;

  r = mailimf_cache_int_write(mmapstr, index, clist_count(ext));
  if (r != MAIL_NO_ERROR)
    return r;
  
  for(cur = clist_begin(ext) ; cur != NULL ; cur = clist_next(cur)) {
    char * ext_flag;
    
    ext_flag = clist_content(cur);
    r = mailimf_cache_string_write(mmapstr, index,
        ext_flag, strlen(ext_flag));
    if (r != MAIL_NO_ERROR)
      return r;
  }

  return MAIL_NO_ERROR;
}

static int generic_flags_write(MMAPString * mmapstr, size_t * index,
			       struct mail_flags * flags)
{
  int r;

  r = mailimf_cache_int_write(mmapstr, index,
      flags->fl_flags & ~MAIL_FLAG_NEW);
  if (r != MAIL_NO_ERROR)
    return r;

  r = flags_extension_write(mmapstr, index,
      flags->fl_extension);
  if (r != MAIL_NO_ERROR)
    return r;

  return MAIL_NO_ERROR;
}




static struct mail_flags * mail_flags_dup(struct mail_flags * flags)
{
  clist * list;
  struct mail_flags * new_flags;
  int r;
  clistiter * cur;

  list = clist_new();
  if (list == NULL) {
    goto err;
  }

  for(cur = clist_begin(flags->fl_extension) ; cur != NULL ;
      cur = clist_next(cur)) {
    char * ext;
    char * original_ext;
    
    original_ext = clist_content(cur);
    ext = strdup(original_ext);
    if (ext == NULL) {
      goto free;
    }

    r = clist_append(list, ext);
    if (r < 0) {
      free(ext);
      goto free;
    }
  }

  new_flags = mail_flags_new(flags->fl_flags, list);
  if (new_flags == NULL) {
    goto free;
  }

  return new_flags;

 free:
  clist_foreach(list, (clist_func) free, NULL);
  clist_free(list);
 err:
  return NULL;
}

static mailmessage * mailmessage_build(mailmessage * msg)
{
  mailmessage * new_msg;

  new_msg = malloc(sizeof(* new_msg));
  if (new_msg == NULL)
    goto err;

  new_msg->msg_session = msg->msg_session;
  new_msg->msg_driver = msg->msg_driver;
  new_msg->msg_index = msg->msg_index;
  if (msg->msg_uid == NULL)
    new_msg->msg_uid = NULL;
  else {
    new_msg->msg_uid = strdup(msg->msg_uid);
    if (new_msg->msg_uid == NULL)
      goto free;
  }

  new_msg->msg_cached = msg->msg_cached;
  new_msg->msg_size = msg->msg_size;
  new_msg->msg_fields = NULL;
  new_msg->msg_flags = mail_flags_dup(msg->msg_flags);
  if (new_msg->msg_flags == NULL) {
    free(new_msg->msg_uid);
    goto free;
  }

  new_msg->msg_mime = NULL;
  new_msg->msg_data = NULL;

  return new_msg;

 free:
  free(new_msg);
 err:
  return NULL;
}

struct mail_flags_store * mail_flags_store_new(void)
{
  struct mail_flags_store * flags_store;

  flags_store = malloc(sizeof(struct mail_flags_store));
  if (flags_store == NULL)
    goto err;

  flags_store->fls_tab = carray_new(128);
  if (flags_store->fls_tab == NULL)
    goto free;

  flags_store->fls_hash = chash_new(128, CHASH_COPYALL);
  if (flags_store->fls_hash == NULL)
    goto free_tab;
  
  return flags_store;

 free_tab:
  carray_free(flags_store->fls_tab);
 free:
  free(flags_store);
 err:
  return NULL;
}

void mail_flags_store_clear(struct mail_flags_store * flags_store)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(flags_store->fls_tab) ; i ++) {
    chashdatum key;
    mailmessage * msg;

    msg = carray_get(flags_store->fls_tab, i);

    key.data = &msg->msg_index;
    key.len = sizeof(msg->msg_index);
    chash_delete(flags_store->fls_hash, &key, NULL);

    mailmessage_free(msg);
  }
  carray_set_size(flags_store->fls_tab, 0);
}

void mail_flags_store_free(struct mail_flags_store * flags_store)
{
  mail_flags_store_clear(flags_store);
  chash_free(flags_store->fls_hash);
  carray_free(flags_store->fls_tab);
  free(flags_store);
}

int mail_flags_store_set(struct mail_flags_store * flags_store,
			 mailmessage * msg)
{
  chashdatum key;
  chashdatum value;
  unsigned int index;
  int res;
  int r;
  mailmessage * new_msg;

  if (msg->msg_flags == NULL) {
    res = MAIL_NO_ERROR;
    goto err;
  }

  /* duplicate needed message info */
  new_msg = mailmessage_build(msg);
  if (new_msg == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  key.data = &new_msg->msg_index;
  key.len = sizeof(new_msg->msg_index);

  r = chash_get(flags_store->fls_hash, &key, &value);
  if (r == 0) {
    mailmessage * old_msg;

    index = * (unsigned int *) value.data;
    old_msg = carray_get(flags_store->fls_tab, index);
    mailmessage_free(old_msg);
  }
  else {
    r = carray_set_size(flags_store->fls_tab,
        carray_count(flags_store->fls_tab) + 1);
    if (r != 0) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    index = carray_count(flags_store->fls_tab) - 1;
  }

  carray_set(flags_store->fls_tab, index, new_msg);
  
  value.data = &index;
  value.len = sizeof(index);

  r = chash_set(flags_store->fls_hash, &key, &value, NULL);
  if (r < 0) {
    carray_delete(flags_store->fls_tab, index);
    res = MAIL_ERROR_MEMORY;
    goto free;
  }

  return MAIL_NO_ERROR;

 free:
  mailmessage_free(new_msg);
 err:
  return res;
}

static int msg_index_compare(mailmessage ** msg1, mailmessage ** msg2)
{
  return (* msg1)->msg_index - (* msg2)->msg_index;
}

void mail_flags_store_sort(struct mail_flags_store * flags_store)
{
  qsort(carray_data(flags_store->fls_tab),
      carray_count(flags_store->fls_tab), sizeof(mailmessage *),
      (int (*)(const void *, const void *)) msg_index_compare);
}

struct mail_flags *
mail_flags_store_get(struct mail_flags_store * flags_store, uint32_t index)
{
  struct mail_flags * flags;
  chashdatum key;
  chashdatum value;
  int r;
  unsigned int tab_index;
  mailmessage * msg;
  
  key.data = &index;
  key.len = sizeof(index);

  r = chash_get(flags_store->fls_hash, &key, &value);
  
  if (r < 0)
    return NULL;
  
#if 0
  flags = mail_flags_dup((struct mail_flags *) value.data);
#endif
  tab_index = * (unsigned int *) value.data;
  msg = carray_get(flags_store->fls_tab, tab_index);
  if (msg->msg_flags == NULL)
    return NULL;
  
  flags = mail_flags_dup(msg->msg_flags);
  
  return flags;
}

int mail_flags_compare(struct mail_flags * flags1, struct mail_flags * flags2)
{
  clistiter * cur1;

  if (clist_count(flags1->fl_extension) != clist_count(flags2->fl_extension))
    return -1;

  for(cur1 = clist_begin(flags1->fl_extension) ; cur1 != NULL ;
      cur1 = clist_next(cur1)) {
    char * flag1;
    clistiter * cur2;
    int found;

    flag1 = clist_content(cur1);

    found = 0;
    for(cur2 = clist_begin(flags2->fl_extension) ; cur2 != NULL ;
        cur2 = clist_next(cur2)) {
      char * flag2;

      flag2 = clist_content(cur2);

      if (strcasecmp(flag1, flag2) == 0) {
        found = 1;
        break;
      }
    }

    if (!found)
      return -1;
  }

  return flags1->fl_flags - flags2->fl_flags;
}

int generic_cache_fields_read(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mailimf_fields ** result)
{
  int r;
  int res;
  size_t cur_token;
  struct mailimf_fields * fields;
  void * data;
  size_t data_len;
  
  r = mail_cache_db_get(cache_db, keyname, strlen(keyname), &data, &data_len);
  if (r != 0) {
    res = MAIL_ERROR_CACHE_MISS;
    goto err;
  }
  
  r = mail_serialize_clear(mmapstr, &cur_token);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  if (mmap_string_append_len(mmapstr, data, data_len) == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mailimf_cache_fields_read(mmapstr, &cur_token, &fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  * result = fields;

  return MAIL_NO_ERROR;

 err:
  return res;
}

int generic_cache_fields_write(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mailimf_fields * fields)
{
  int r;
  int res;
  size_t cur_token;

  r = mail_serialize_clear(mmapstr, &cur_token);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_cache_fields_write(mmapstr, &cur_token, fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mail_cache_db_put(cache_db, keyname, strlen(keyname),
      mmapstr->str, mmapstr->len);
  if (r != 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }

  return MAIL_NO_ERROR;
  
 err:
  return res;
}

int generic_cache_flags_read(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mail_flags ** result)
{
  int r;
  int res;
  size_t cur_token;
  struct mail_flags * flags;
  void * data;
  size_t data_len;

  data = NULL;
  data_len = 0;
  r = mail_cache_db_get(cache_db, keyname, strlen(keyname), &data, &data_len);
  if (r != 0) {
    res = MAIL_ERROR_CACHE_MISS;
    goto err;
  }
  
  r = mail_serialize_clear(mmapstr, &cur_token);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  if (mmap_string_append_len(mmapstr, data, data_len) == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  flags = NULL;
  r = generic_flags_read(mmapstr, &cur_token, &flags);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  * result = flags;

  return MAIL_NO_ERROR;

 err:
  return res;
}

int generic_cache_flags_write(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * keyname, struct mail_flags * flags)
{
  int r;
  int res;
  size_t cur_token;

  r = mail_serialize_clear(mmapstr, &cur_token);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  r = generic_flags_write(mmapstr, &cur_token, flags);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mail_cache_db_put(cache_db, keyname, strlen(keyname),
      mmapstr->str, mmapstr->len);
  if (r != 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }

  return MAIL_NO_ERROR;
  
 err:
  return res;
}


int generic_cache_delete(struct mail_cache_db * cache_db,
    char * keyname)
{
  int r;
  int res;

  r = mail_cache_db_del(cache_db, keyname, strlen(keyname));
  if (r != 0) {
    res = MAIL_ERROR_FILE;
    goto err;
  }

  return MAIL_NO_ERROR;
  
 err:
  return res;
}


