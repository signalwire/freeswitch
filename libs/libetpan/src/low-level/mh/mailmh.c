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
 * $Id: mailmh.c,v 1.33 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmh.h"

/*
perfs :

/net/home/dinh/Mail/inbox/sylpheed 686

2724    /net/home/dinh/Mail/inbox/sylpheed

bart:~/LibEtPan/libetpan/tests> time ./mhtest >/dev/null

real    0m0.385s
user    0m0.270s
sys     0m0.110s

*/

#ifdef _MSC_VER
#	include "win_etpan.h"
#else
#	include <dirent.h>
#	include <unistd.h>
#	include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libetpan-config.h"

struct mailmh * mailmh_new(const char * foldername)
{
  struct mailmh * f;

  f = malloc(sizeof(*f));
  if (f == NULL)
    return NULL;

  f->mh_main = mailmh_folder_new(NULL, foldername);
  if (f->mh_main == NULL) {
    free(f);
    return NULL;
  }
  
  return f;
}

void mailmh_free(struct mailmh * f)
{
  mailmh_folder_free(f->mh_main);
  free(f);
}



struct mailmh_msg_info * mailmh_msg_info_new(uint32_t index, size_t size,
					     time_t mtime)
{
  struct mailmh_msg_info * msg_info;

  msg_info = malloc(sizeof(* msg_info));
  if (msg_info == NULL)
    return NULL;
  msg_info->msg_index = index;
  msg_info->msg_size = size;
  msg_info->msg_mtime = mtime;

  msg_info->msg_array_index = 0;

  return msg_info;
}

void mailmh_msg_info_free(struct mailmh_msg_info * msg_info)
{
  free(msg_info);
}

struct mailmh_folder * mailmh_folder_new(struct mailmh_folder * parent,
					 const char * name)
{
  char * filename;
  char * parent_filename;

  struct mailmh_folder * folder;

  folder = malloc(sizeof(* folder));
  if (folder == NULL)
    goto err;

  if (parent == NULL) {
    filename = strdup(name);
    if (filename == NULL)
      goto free_folder;
  }
  else {
    parent_filename = parent->fl_filename;
    filename = malloc(strlen(parent_filename) + strlen(name) + 2);
    if (filename == NULL)
      goto free_folder;

    strcpy(filename, parent_filename);
    strcat(filename, MAIL_DIR_SEPARATOR_S);
    strcat(filename, name);
  }

  folder->fl_filename = filename;

  folder->fl_name = strdup(name);
  if (folder->fl_name == NULL)
    goto free_filename;

  folder->fl_msgs_tab = carray_new(128);
  if (folder->fl_msgs_tab == NULL)
    goto free_name;

#if 0
  folder->fl_msgs_hash = cinthash_new(128);
  if (folder->fl_msgs_hash == NULL)
    goto free_msgs_tab;
#endif
  folder->fl_msgs_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
  if (folder->fl_msgs_hash == NULL)
    goto free_msgs_tab;

  folder->fl_subfolders_tab = carray_new(128);
  if (folder->fl_subfolders_tab == NULL)
    goto free_msgs_hash;

  folder->fl_subfolders_hash = chash_new(128, CHASH_COPYNONE);
  if (folder->fl_subfolders_hash == NULL)
    goto free_subfolders_tab;

  folder->fl_mtime = 0;
  folder->fl_parent = parent;
  folder->fl_max_index = 0;

  return folder;

 free_subfolders_tab:
  carray_free(folder->fl_subfolders_tab);
 free_msgs_hash:
#if 0
  cinthash_free(folder->fl_msgs_hash);
#endif
  chash_free(folder->fl_msgs_hash);
 free_msgs_tab:
  carray_free(folder->fl_msgs_tab);
 free_name:
  free(folder->fl_name);
 free_filename:
  free(folder->fl_filename);
 free_folder:
  free(folder);
 err:
  return NULL;
}

void mailmh_folder_free(struct mailmh_folder * folder)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(folder->fl_subfolders_tab) ; i++) {
    struct mailmh_folder * subfolder;

    subfolder = carray_get(folder->fl_subfolders_tab, i);
    if (subfolder != NULL)
      mailmh_folder_free(subfolder);
  }
  carray_free(folder->fl_subfolders_tab);
  chash_free(folder->fl_subfolders_hash);

  for(i = 0 ; i < carray_count(folder->fl_msgs_tab) ; i++) {
    struct mailmh_msg_info * msg_info;

    msg_info = carray_get(folder->fl_msgs_tab, i);
    if (msg_info != NULL)
      mailmh_msg_info_free(msg_info);
  }
  carray_free(folder->fl_msgs_tab);
  chash_free(folder->fl_msgs_hash);
#if 0
  cinthash_free(folder->fl_msgs_hash);
#endif
  
  free(folder->fl_filename);
  free(folder->fl_name);

  free(folder);
}

struct mailmh_folder * mailmh_folder_find(struct mailmh_folder * root,
					  const char * filename)
{
  int r;
  char pathname[PATH_MAX];
  char * p;
  chashdatum key;
  chashdatum data;
  struct mailmh_folder * folder;
  char * start;

  if (strcmp(root->fl_filename, filename) == 0)
    return root;

#if 0
  r = mailmh_folder_update(root);
  if (r != MAILMH_NO_ERROR)
    return NULL;
#endif

#if 0
  for(i = 0 ; i < root->fl_subfolders_tab->len ; i++) {
    struct mailmh_folder * subfolder;

    subfolder = carray_get(root->fl_subfolders_tab, i);
    if (subfolder != NULL)
      if (strncmp(subfolder->fl_filename, filename,
		  strlen(subfolder->fl_filename)) == 0)
	return mailmh_folder_find(subfolder, filename);
  }
#endif
  strncpy(pathname, filename, PATH_MAX);
  pathname[PATH_MAX - 1] = 0;
  start = pathname + strlen(root->fl_filename) + 1;

  p = strchr(start, MAIL_DIR_SEPARATOR);
  if (p != NULL) {
    * p = 0;

    root = mailmh_folder_find(root, pathname);
    if (root != NULL) {
      folder = mailmh_folder_find(root, filename);
      if (folder == NULL)
	return NULL;
      return folder;
    }

    return NULL;
  }
  else {
    key.data = pathname;
    key.len = strlen(pathname);
    r = chash_get(root->fl_subfolders_hash, &key, &data);
    if (r < 0)
      return NULL;

    return data.data;
  }
}

int mailmh_folder_update(struct mailmh_folder * folder)
{
  DIR * d;
  struct dirent * ent;
  struct stat buf;
  char * mh_seq;
  char filename[PATH_MAX];
  int res;
  int r;
  uint32_t max_index;
#if 0
  int add_folder;
#endif
  unsigned int i;

  if (stat(folder->fl_filename, &buf) == -1) {
    res = MAILMH_ERROR_FOLDER;
    goto err;
  }

  if (folder->fl_mtime == buf.st_mtime) {
    res = MAILMH_NO_ERROR;
    goto err;
  }

  folder->fl_mtime = buf.st_mtime;

  d = opendir(folder->fl_filename);
  if (d == NULL) {
    res = MAILMH_ERROR_FOLDER;
    goto err;
  }

  max_index = 0;

#if 0
  if (folder->fl_subfolders_tab->len == 0)
    add_folder = 1;
  else
    add_folder = 0;
#endif

  /* clear the message list */

  for(i = 0 ; i < carray_count(folder->fl_msgs_tab) ; i ++) {
    struct mailmh_msg_info * msg_info;
    chashdatum key;
    
    msg_info = carray_get(folder->fl_msgs_tab, i);
    if (msg_info == NULL)
      continue;

#if 0
    cinthash_remove(folder->fl_msgs_hash, msg_info->msg_index);
#endif
    key.data = &msg_info->msg_index;
    key.len = sizeof(msg_info->msg_index);
    chash_delete(folder->fl_msgs_hash, &key, NULL);
    
    mailmh_msg_info_free(msg_info);
  }

  carray_set_size(folder->fl_msgs_tab, 0);

  do {
    uint32_t index;

    ent = readdir(d);

    if (ent != NULL) {

      snprintf(filename, PATH_MAX,
	       "%s%c%s", folder->fl_filename, MAIL_DIR_SEPARATOR, ent->d_name);

      if (stat(filename, &buf) == -1)
	continue;

      if (S_ISREG(buf.st_mode)) {
	index = strtoul(ent->d_name, NULL, 10);
	if (index != 0) {
	  struct mailmh_msg_info * msg_info;
	  unsigned int array_index;
          chashdatum key;
          chashdatum data;

	  msg_info = mailmh_msg_info_new(index, buf.st_size, buf.st_mtime);
	  if (msg_info == NULL) {
	    res = MAILMH_ERROR_MEMORY;
	    goto closedir;
	  }
	  
	  r = carray_add(folder->fl_msgs_tab, msg_info, &array_index);
	  if (r < 0) {
	    mailmh_msg_info_free(msg_info);
	    res = MAILMH_ERROR_MEMORY;
	    goto closedir;
	  }
	  msg_info->msg_array_index = array_index;

	  if (index > max_index)
	    max_index = index;

#if 0
	  r = cinthash_add(folder->fl_msgs_hash, msg_info->msg_index, msg_info);
#endif
          key.data = &msg_info->msg_index;
          key.len = sizeof(msg_info->msg_index);
          data.data = msg_info;
          data.len = 0;
          
          r = chash_set(folder->fl_msgs_hash, &key, &data, NULL);
	  if (r < 0) {
	    carray_delete_fast(folder->fl_msgs_tab, msg_info->msg_array_index);
	    mailmh_msg_info_free(msg_info);
	    res = MAILMH_ERROR_MEMORY;
	    goto closedir;
	  }
	}
      }
      else if (S_ISDIR(buf.st_mode)) {
	struct mailmh_folder * subfolder;
	unsigned int array_index;
	chashdatum key;
	chashdatum data;

	if (ent->d_name[0] == '.') {
	  if (ent->d_name[1] == 0)
	    continue;
	  if ((ent->d_name[1] == '.') && (ent->d_name[2] == 0))
	    continue;
	}

	key.data = ent->d_name;
	key.len = strlen(ent->d_name);
	r = chash_get(folder->fl_subfolders_hash, &key, &data);
	if (r < 0) {
	  subfolder = mailmh_folder_new(folder, ent->d_name);
	  if (subfolder == NULL) {
	    res = MAILMH_ERROR_MEMORY;
	    goto closedir;
	  }
	  
	  r = carray_add(folder->fl_subfolders_tab, subfolder, &array_index);
	  if (r < 0) {
	    mailmh_folder_free(subfolder);
	    res = MAILMH_ERROR_MEMORY;
	    goto closedir;
	  }
	  subfolder->fl_array_index = array_index;
	  
	  key.data = subfolder->fl_filename;
	  key.len = strlen(subfolder->fl_filename);
	  data.data = subfolder;
	  data.len = 0;
	  r = chash_set(folder->fl_subfolders_hash, &key, &data, NULL);
	  if (r < 0) {
	    carray_delete_fast(folder->fl_subfolders_tab, subfolder->fl_array_index);
	    mailmh_folder_free(subfolder);
	    res = MAILMH_ERROR_MEMORY;
	    goto closedir;
	  }
	}
      }
    }
  }
  while (ent != NULL);

  folder->fl_max_index = max_index;

  mh_seq = malloc(strlen(folder->fl_filename) + 2 + sizeof(".mh_sequences"));
  if (mh_seq == NULL) {
    res = MAILMH_ERROR_MEMORY;
    goto closedir;
  }
  strcpy(mh_seq, folder->fl_filename);
  strcat(mh_seq, MAIL_DIR_SEPARATOR_S);
  strcat(mh_seq, ".mh_sequences");

  if (stat(mh_seq, &buf) == -1) {
    int fd;

    fd = creat(mh_seq, S_IRUSR | S_IWUSR);
    if (fd != -1)
      close(fd);
  }
  free(mh_seq);

  closedir(d);

  return MAILMH_NO_ERROR;

 closedir:
  closedir(d);
 err:
  return res;
}

int mailmh_folder_add_subfolder(struct mailmh_folder * parent,
				const char * name)
{
  char * foldername;
  int r;
  struct mailmh_folder * folder;
  unsigned int array_index;
  chashdatum key;
  chashdatum data;

  foldername = malloc(strlen(parent->fl_filename) + strlen(name) + 2);
  if (foldername == NULL)
    return MAILMH_ERROR_MEMORY;
  strcpy(foldername, parent->fl_filename);
  strcat(foldername, MAIL_DIR_SEPARATOR_S);
  strcat(foldername, name);

#ifdef WIN32
  r = mkdir(foldername);
#else
  r = mkdir(foldername, 0700);
#endif
  free(foldername);

  if (r < 0)
    return MAILMH_ERROR_FOLDER;

  folder = mailmh_folder_new(parent, name);
  if (folder == NULL)
    return MAILMH_ERROR_MEMORY;
  
  r = carray_add(parent->fl_subfolders_tab, folder, &array_index);
  if (r < 0) {
    mailmh_folder_free(folder);
    return MAILMH_ERROR_MEMORY;
  }
  folder->fl_array_index = array_index;

  key.data = folder->fl_filename;
  key.len = strlen(folder->fl_filename);
  data.data = folder;
  data.len = 0;

  r = chash_set(parent->fl_subfolders_hash, &key, &data, NULL);
  if (r < 0) {
    carray_delete_fast(folder->fl_subfolders_tab, folder->fl_array_index);
    mailmh_folder_free(folder);
    return MAILMH_ERROR_MEMORY;
  }

  return MAILMH_NO_ERROR;
}

int mailmh_folder_remove_subfolder(struct mailmh_folder * folder)
{
  struct mailmh_folder * parent;
  chashdatum key;
  chashdatum data;
  int r;
  
  parent = folder->fl_parent;

  key.data = folder->fl_filename;
  key.len = strlen(folder->fl_filename);
  
  r = chash_get(parent->fl_subfolders_hash, &key, &data);
  if (r < 0)
    return MAILMH_ERROR_FOLDER;

  chash_delete(parent->fl_subfolders_hash, &key, NULL);
  carray_delete_fast(parent->fl_subfolders_tab, folder->fl_array_index);
  
  mailmh_folder_free(folder);
      
  return MAILMH_NO_ERROR;

}

int mailmh_folder_rename_subfolder(struct mailmh_folder * src_folder,
				   struct mailmh_folder * dst_folder,
				   const char * new_name)
{
  int r;
  struct mailmh_folder * folder;
  struct mailmh_folder * parent;
  char * new_foldername;
  
  parent = src_folder->fl_parent;
  if (parent == NULL)
    return MAILMH_ERROR_RENAME;

  new_foldername = malloc(strlen(dst_folder->fl_filename) + 2 + strlen(new_name));
  if (new_foldername == NULL)
    return MAILMH_ERROR_MEMORY;

  strcpy(new_foldername, dst_folder->fl_filename);
  strcat(new_foldername, MAIL_DIR_SEPARATOR_S);
  strcat(new_foldername, new_name);

  r = rename(src_folder->fl_filename, new_foldername);
  free(new_foldername);
  if (r < 0)
    return MAILMH_ERROR_RENAME;

  r = mailmh_folder_remove_subfolder(src_folder);
  if (r != MAILMH_NO_ERROR)
    return r;

  folder = mailmh_folder_new(dst_folder, new_name);
  if (folder == NULL)
    return MAILMH_ERROR_MEMORY;
  
  r = carray_add(parent->fl_subfolders_tab, folder, NULL);
  if (r < 0) {
    mailmh_folder_free(folder);
    return MAILMH_ERROR_MEMORY;
  }

  return MAILMH_NO_ERROR;
}

#define MAX_TRY_ALLOC 32

/* initial file MUST be in the same directory */

static int mailmh_folder_alloc_msg(struct mailmh_folder * folder,
				   char * filename, uint32_t * result)
{
  uint32_t max;
  uint32_t k;
  char * new_filename;
  size_t len;
  int got_file;
  int r;
  
  len = strlen(folder->fl_filename) + 20;
  new_filename = malloc(len);
  if (new_filename == NULL)
    return MAILMH_ERROR_MEMORY;

  max = folder->fl_max_index + 1;
  
  got_file = 0;
  k = 0;
  while (k < MAX_TRY_ALLOC) {
    snprintf(new_filename, len, "%s%c%lu", folder->fl_filename,
        MAIL_DIR_SEPARATOR, (unsigned long) (max + k));
    
/* SEB */
#ifdef WIN32
	if (rename( filename, new_filename) == 0) {
		got_file = 1;
	}
#else
    if (link(filename, new_filename) == 0) {
      unlink(filename);
      got_file = 1;
    }
#endif /* WIN32 */
    else if (errno == EXDEV) {
      free(filename);
      return MAILMH_ERROR_FOLDER;
    }
    else if (errno == EPERM) {
      rename(filename, new_filename);
      got_file = 1;
    }
    
    if (got_file) {
      free(new_filename);
      
      if (k > MAX_TRY_ALLOC / 2) {
	r = mailmh_folder_update(folder);
	/* ignore errors */
      }
      
      * result = max + k;
      
      folder->fl_max_index = max + k;
      
      return MAILMH_NO_ERROR;
    }
    k ++;
  }

  free(new_filename);

  return MAILMH_ERROR_FOLDER;
}

int mailmh_folder_get_message_filename(struct mailmh_folder * folder,
				       uint32_t index, char ** result)
{
  char * filename;
  int len;

#if 0
  r = mailmh_folder_update(folder);
  if (r != MAILMH_NO_ERROR)
    return r;
#endif

  len = strlen(folder->fl_filename) + 20;
  filename = malloc(len);
  if (filename == NULL)
    return MAILMH_ERROR_MEMORY;

  snprintf(filename, len, "%s%c%lu", folder->fl_filename, MAIL_DIR_SEPARATOR,
	   (unsigned long) index);

  * result = filename;

  return MAILMH_NO_ERROR;;
}


int mailmh_folder_get_message_fd(struct mailmh_folder * folder,
				 uint32_t index, int flags, int * result)
{
  char * filename;
  int fd;
  int r;

#if 0
  r = mailmh_folder_update(folder);
  if (r != MAILMH_NO_ERROR)
    return r;
#endif

  r = mailmh_folder_get_message_filename(folder, index, &filename);
  if (r != MAILMH_NO_ERROR)
    return r;

  fd = open(filename, flags);
  free(filename);
  if (fd == -1)
    return MAILMH_ERROR_MSG_NOT_FOUND;

  * result = fd;

  return MAILMH_NO_ERROR;
}

int mailmh_folder_get_message_size(struct mailmh_folder * folder,
				   uint32_t index, size_t * result)
{
  int r;
  char * filename;
  struct stat buf;

  r = mailmh_folder_get_message_filename(folder, index, &filename);
  if (r != MAILMH_NO_ERROR)
    return r;

  r = stat(filename, &buf);
  free(filename);
  if (r < 0)
    return MAILMH_ERROR_FILE;

  * result = buf.st_size;

  return MAILMH_NO_ERROR;
}

int mailmh_folder_add_message_uid(struct mailmh_folder * folder,
    const char * message, size_t size,
    uint32_t * pindex)
{
  char * tmpname;
  int fd;
  size_t namesize;
  size_t left;
  ssize_t res;
  struct mailmh_msg_info * msg_info;
  uint32_t index;
  int error;
  int r;
  unsigned int array_index;
  struct stat buf;
  chashdatum key;
  chashdatum data;

#if 0  
  r = mailmh_folder_update(folder);
  if (r != MAILMH_NO_ERROR) {
    error = r;
    goto err;
  }
#endif

  namesize = strlen(folder->fl_filename) + 20;
  tmpname = malloc(namesize);
  snprintf(tmpname, namesize, "%s%ctmpXXXXXX",
	   folder->fl_filename, MAIL_DIR_SEPARATOR);
  fd = mkstemp(tmpname);
  if (fd < 0) {
    error = MAILMH_ERROR_FILE;
    goto free;
  }

  left = size;
  while (left > 0) {
    res = write(fd, message, left);
    if (res == -1) {
      close(fd);
      error = MAILMH_ERROR_FILE;
      goto free;
    }

    left -= res;
  }
  close(fd);

  r = stat(tmpname, &buf);
  if (r < 0) {
    error = MAILMH_ERROR_FILE;
    goto free;
  }

  r = mailmh_folder_alloc_msg(folder, tmpname, &index);
  if (r != MAILMH_NO_ERROR) {
    unlink(tmpname);
    error = MAILMH_ERROR_COULD_NOT_ALLOC_MSG;
    goto free;
  }
  free(tmpname);

  msg_info = mailmh_msg_info_new(index, size, buf.st_mtime);
  if (msg_info == NULL) {
    mailmh_folder_remove_message(folder, index);
    error = MAILMH_ERROR_MEMORY;
    goto err;
  }
  
  r = carray_add(folder->fl_msgs_tab, msg_info, &array_index);
  if (r < 0) {
    mailmh_folder_remove_message(folder, index);
    mailmh_msg_info_free(msg_info);
    error = MAILMH_ERROR_MEMORY;
    goto err;
  }
  msg_info->msg_array_index = array_index;

#if 0
  r = cinthash_add(folder->fl_msgs_hash, index, msg_info);
#endif
  key.data = &index;
  key.len = sizeof(index);
  data.data = msg_info;
  data.len = 0;
  
  if (pindex != NULL)
    * pindex = index;
  
  r = chash_set(folder->fl_msgs_hash, &key, &data, NULL);
  if (r < 0) {
    carray_delete_fast(folder->fl_msgs_tab, msg_info->msg_array_index);
    mailmh_msg_info_free(msg_info);
    error = MAILMH_ERROR_MEMORY;
    goto err;
  }
  
  return MAILMH_NO_ERROR;

 free:
  free(tmpname);
 err:
  return error;
}

int mailmh_folder_add_message(struct mailmh_folder * folder,
    const char * message, size_t size)
{
  return mailmh_folder_add_message_uid(folder, message, size, NULL);
}

int mailmh_folder_add_message_file_uid(struct mailmh_folder * folder,
    int fd, uint32_t * pindex)
{
  char * message;
  struct stat buf;
  int r;

#if 0
  r = mailmh_folder_update(folder);
  if (r != MAILMH_NO_ERROR)
    return r;
#endif

  if (fstat(fd, &buf) == -1)
    return MAILMH_ERROR_FILE;

  message = mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (message == (const char *)MAP_FAILED)
    return MAILMH_ERROR_FILE;

  r = mailmh_folder_add_message_uid(folder, message, buf.st_size, pindex);
  
  munmap(message, buf.st_size);

  return r;
}

int mailmh_folder_add_message_file(struct mailmh_folder * folder,
    int fd)
{
  return mailmh_folder_add_message_file_uid(folder, fd, NULL);
}

int mailmh_folder_remove_message(struct mailmh_folder * folder,
				 uint32_t index)
{
  char * filename;
  struct mailmh_msg_info * msg_info;
  int res;
  int r;
  chashdatum key;
  chashdatum data;

#if 0  
  r = mailmh_folder_update(folder);
  if (r != MAILMH_NO_ERROR) {
    res = r;
    goto err;
  }
#endif

  r = mailmh_folder_get_message_filename(folder, index, &filename);
  if (filename == NULL) {
    res = r;
    goto err;
  }

  if (unlink(filename) == -1) {
    res = MAILMH_ERROR_FILE;
    goto free;
  }

  key.data = &index;
  key.len = sizeof(index);
  r = chash_get(folder->fl_msgs_hash, &key, &data);
#if 0
  msg_info = cinthash_find(folder->fl_msgs_hash, index);
#endif
  if (r == 0) {
    msg_info = data.data;

    carray_delete_fast(folder->fl_msgs_tab, msg_info->msg_array_index);
#if 0
    cinthash_remove(folder->fl_msgs_hash, index);
#endif
    chash_delete(folder->fl_msgs_hash, &key, NULL);
  }

  return MAILMH_NO_ERROR;

 free:
  free(filename);
 err:
  return res;
}


int mailmh_folder_move_message(struct mailmh_folder * dest_folder,
			       struct mailmh_folder * src_folder,
			       uint32_t index)
{
  int fd;
  char * filename;
  int r;

#if 0
  r = mailmh_folder_update(dest_folder);
  if (r != MAILMH_NO_ERROR)
    return r;
  r = mailmh_folder_update(src_folder);
  if (r != MAILMH_NO_ERROR)
    return r;
#endif

  /* move on the same filesystem */
  r = mailmh_folder_get_message_filename(src_folder, index, &filename);
  if (r != MAILMH_NO_ERROR)
    return r;

  r = mailmh_folder_alloc_msg(dest_folder, filename, &index);
  free(filename);
  if (r == MAILMH_NO_ERROR)
    return MAILMH_NO_ERROR;

  /* move on the different filesystems */
  r = mailmh_folder_get_message_fd(src_folder, index, O_RDONLY, &fd);
  if (r != MAILMH_NO_ERROR)
    return r;

  r = mailmh_folder_add_message_file(dest_folder, fd);
  if (r != MAILMH_NO_ERROR) {
    close(fd);
    return r;
  }

  close(fd);

  r = mailmh_folder_remove_message(src_folder, index);

  return MAILMH_NO_ERROR;
}

unsigned int mailmh_folder_get_message_number(struct mailmh_folder * folder)
{
  unsigned int i;
  unsigned int count;
  
  count = 0;
  for(i = 0 ; i < carray_count(folder->fl_msgs_tab) ; i ++)
    if (carray_get(folder->fl_msgs_tab, i) != NULL)
      count ++;
  
  return count;
}
