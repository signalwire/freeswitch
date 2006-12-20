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
 * $Id: maildir.c,v 1.17 2006/10/12 08:00:21 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildir.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _MSC_VER
#	include "win_etpan.h"
#else
#	include <unistd.h>
#	include <sys/mman.h>
#	include <dirent.h>
#endif
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#ifdef LIBETPAN_SYSTEM_BASENAME
#include <libgen.h>
#endif

/*
  We suppose the maildir mailbox remains on one unique filesystem.
*/

struct maildir * maildir_new(const char * path)
{
  struct maildir * md;
  
  md = malloc(sizeof(* md));
  if (md == NULL)
    goto err;
  
  md->mdir_counter = 0;
  md->mdir_mtime_new = (time_t) -1;
  md->mdir_mtime_cur = (time_t) -1;
  
  md->mdir_pid = getpid();
  gethostname(md->mdir_hostname, sizeof(md->mdir_hostname));
  strncpy(md->mdir_path, path, sizeof(md->mdir_path));
  md->mdir_path[PATH_MAX - 1] = '\0';
  
  md->mdir_msg_list = carray_new(128);
  if (md->mdir_msg_list == NULL)
    goto free;
  
  md->mdir_msg_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYNONE);
  if (md->mdir_msg_hash == NULL)
    goto free_msg_list;
  
  return md;
  
 free_msg_list:
  carray_free(md->mdir_msg_list);
 free:
  free(md);
 err:
  return NULL;
}

static void maildir_flush(struct maildir * md, int msg_new);
static void msg_free(struct maildir_msg * msg);

void maildir_free(struct maildir * md)
{
  maildir_flush(md, 0);
  maildir_flush(md, 1);
  chash_free(md->mdir_msg_hash);
  carray_free(md->mdir_msg_list);
  free(md);
}

#define MAX_TRY_ALLOC 32

static char * maildir_get_new_message_filename(struct maildir * md,
    char * tmpfile)
{
  char filename[PATH_MAX];
  char basename[PATH_MAX];
  int k;
  time_t now;
  int got_file;
  int r;
  
  got_file = 0;
  now = time(NULL);
  k = 0;
  while (k < MAX_TRY_ALLOC) {
    snprintf(basename, sizeof(basename), "%lu.%u_%u.%s",
        (unsigned long) now, md->mdir_pid, md->mdir_counter, md->mdir_hostname);
    snprintf(filename, sizeof(filename), "%s/tmp/%s",
        md->mdir_path, basename);
    
    if (link(tmpfile, filename) == 0) {
      got_file = 1;
      unlink(tmpfile);
    }
    else if (errno == EXDEV) {
      unlink(tmpfile);
      return NULL;
    }
    else if (errno == EPERM) {
      r = rename(tmpfile, filename);
      if (r < 0) {
        unlink(tmpfile);
        return NULL;
      }
      got_file = 1;
    }
    
    if (got_file) {
      char * dup_filename;
      
      dup_filename = strdup(filename);
      if (dup_filename == NULL) {
        unlink(filename);
        return NULL;
      }
      
      md->mdir_counter ++;
      
      return dup_filename;
    }
    
    md->mdir_counter ++;
    k ++;
  }
  
  return NULL;
}


static void msg_free(struct maildir_msg * msg)
{
  free(msg->msg_uid);
  free(msg->msg_filename);
  free(msg);
}

/*
  msg_new()
  
  filename is given without path
*/

static struct maildir_msg * msg_new(char * filename, int new_msg)
{
  struct maildir_msg * msg;
  char * p;
  int flags;
  size_t uid_len;
  char * begin_uid;

  /* name of file : xxx-xxx_xxx-xxx:2,SRFT */
  
  msg = malloc(sizeof(* msg));
  if (msg == NULL)
    goto err;
  
  msg->msg_filename = strdup(filename);
  if (msg->msg_filename == NULL)
    goto free;
  
  begin_uid = filename;
  
  uid_len = strlen(begin_uid);
  
  flags = 0;
  p = strstr(filename, ":2,");
  if (p != NULL) {
    uid_len = p - begin_uid;
    
    p += 3;
    
    /* parse flags */
    while (* p != '\0') {
      switch (* p) {
      case 'S':
        flags |= MAILDIR_FLAG_SEEN;
        break;
      case 'R':
        flags |= MAILDIR_FLAG_REPLIED;
        break;
      case 'F':
        flags |= MAILDIR_FLAG_FLAGGED;
        break;
      case 'T':
        flags |= MAILDIR_FLAG_TRASHED;
        break;
      }
      p ++;
    }
  }
  
  if (new_msg)
    flags |= MAILDIR_FLAG_NEW;
  
  msg->msg_flags = flags;

  msg->msg_uid = malloc(uid_len + 1);
  if (msg->msg_uid == NULL)
    goto free_filename;
  
  strncpy(msg->msg_uid, begin_uid, uid_len);
  msg->msg_uid[uid_len] = '\0';
  
  return msg;
  
 free_filename:
  free(msg->msg_filename);
 free:
  free(msg);
 err:
  return NULL;
}

static void maildir_flush(struct maildir * md, int msg_new)
{
  unsigned int i;
  
  i = 0;
  while (i < carray_count(md->mdir_msg_list)) {
    struct maildir_msg * msg;
    int delete;

    msg = carray_get(md->mdir_msg_list, i);
    
    if (msg_new) {
      delete = 0;
      if ((msg->msg_flags & MAILDIR_FLAG_NEW) != 0)
        delete = 1;
    }
    else {
      delete = 1;
      if ((msg->msg_flags & MAILDIR_FLAG_NEW) != 0)
        delete = 0;
    }
    
    if (delete) {
      chashdatum key;
      
      key.data = msg->msg_uid;
      key.len = strlen(msg->msg_uid);
      chash_delete(md->mdir_msg_hash, &key, NULL);
      
      carray_delete(md->mdir_msg_list, i);
      msg_free(msg);
    }
    else {
      i ++;
    }
  }
}

static int add_message(struct maildir * md,
    char * filename, int is_new)
{
  struct maildir_msg * msg;
  chashdatum key;
  chashdatum value;
  unsigned int i;
  int res;
  int r;
  
  msg = msg_new(filename, is_new);
  if (msg == NULL) {
    res = MAILDIR_ERROR_MEMORY;
    goto err;
  }
  
  r = carray_add(md->mdir_msg_list, msg, &i);
  if (r < 0) {
    res = MAILDIR_ERROR_MEMORY;
    goto free_msg;
  }
  
  key.data = msg->msg_uid;
  key.len = strlen(msg->msg_uid);
  value.data = msg;
  value.len = 0;
  
  r = chash_set(md->mdir_msg_hash, &key, &value, NULL);
  if (r < 0) {
    res = MAILDIR_ERROR_MEMORY;
    goto delete;
  }
  
  return MAILDIR_NO_ERROR;
  
 delete:
  carray_delete(md->mdir_msg_list, i);
 free_msg:
  msg_free(msg);
 err:
  return res;
}

static int add_directory(struct maildir * md, char * path, int is_new)
{
  DIR * d;
  struct dirent * entry;
  int res;
  int r;
#if 0
  char filename[PATH_MAX];
#endif
  
  d = opendir(path);
  if (d == NULL) {
    res = MAILDIR_ERROR_DIRECTORY;
    goto err;
  }
  
  while ((entry = readdir(d)) != NULL) {
#if 0
    struct stat stat_info;
    
    snprintf(filename, sizeof(filename), "%s/%s", path, entry->d_name);
    
    r = stat(filename, &stat_info);
    if (r < 0)
      continue;
    
    if (S_ISDIR(stat_info.st_mode))
      continue;
#endif
    
    if (entry->d_name[0] == '.')
      continue;
    
    r = add_message(md, entry->d_name, is_new);
    if (r != MAILDIR_NO_ERROR) {
      /* ignore errors */
    }
  }
  
  closedir(d);
  
  return MAILDIR_NO_ERROR;
  
 err:
  return res;
}

int maildir_update(struct maildir * md)
{
  struct stat stat_info;
  char path_new[PATH_MAX];
  char path_cur[PATH_MAX];
  char path_maildirfolder[PATH_MAX];
  int r;
  int res;
  int changed;
  
  snprintf(path_new, sizeof(path_new), "%s/new", md->mdir_path);
  snprintf(path_cur, sizeof(path_cur), "%s/cur", md->mdir_path);
  
  changed = 0;
  
  /* did new/ changed ? */
  
  r = stat(path_new, &stat_info);
  if (r < 0) {
    res = MAILDIR_ERROR_DIRECTORY;
    goto free;
  }
  
  if (md->mdir_mtime_new != stat_info.st_mtime) {
    md->mdir_mtime_new = stat_info.st_mtime;
    changed = 1;
  }
  
  /* did cur/ changed ? */
  
  r = stat(path_cur, &stat_info);
  if (r < 0) {
    res = MAILDIR_ERROR_DIRECTORY;
    goto free;
  }

  if (md->mdir_mtime_cur != stat_info.st_mtime) {
    md->mdir_mtime_cur = stat_info.st_mtime;
    changed = 1;
  }
  
  if (changed) {
    maildir_flush(md, 0);
    maildir_flush(md, 1);
    
    /* messages in new */
    r = add_directory(md, path_new, 1);
    if (r != MAILDIR_NO_ERROR) {
      res = r;
      goto free;
    }
    
    /* messages in cur */
    r = add_directory(md, path_cur, 0);
    if (r != MAILDIR_NO_ERROR) {
      res = r;
      goto free;
    }
  }
  
  snprintf(path_maildirfolder, sizeof(path_maildirfolder),
      "%s/maildirfolder", md->mdir_path);
  
  if (stat(path_maildirfolder, &stat_info) == -1) {
    int fd;
    
    fd = creat(path_maildirfolder, S_IRUSR | S_IWUSR);
    if (fd != -1)
      close(fd);
  }
  
  return MAILDIR_NO_ERROR;
  
 free:
  maildir_flush(md, 0);
  maildir_flush(md, 1);
  md->mdir_mtime_cur = (time_t) -1;
  md->mdir_mtime_new = (time_t) -1;
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

int maildir_message_add_uid(struct maildir * md,
    const char * message, size_t size,
    char * uid, size_t max_uid_len)
{
  char path_new[PATH_MAX];
  char tmpname[PATH_MAX];
  int fd;
  int r;
  char * mapping;
  char * delivery_tmp_name;
  char * delivery_tmp_basename;
  char delivery_new_name[PATH_MAX];
  char * delivery_new_basename;
  int res;
  struct stat stat_info;
  
  r = maildir_update(md);
  if (r != MAILDIR_NO_ERROR) {
    res = r;
    goto err;
  }

  /* write to tmp/ with a classic temporary file */

  snprintf(tmpname, sizeof(tmpname), "%s/tmp/etpan-maildir-XXXXXX",
      md->mdir_path);
  fd = mkstemp(tmpname);
  if (fd < 0) {
    res = MAILDIR_ERROR_FILE;
    goto err;
  }
  
  r = ftruncate(fd, size);
  if (r < 0) {
    res = MAILDIR_ERROR_FILE;
    goto close;
  }
  
  mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapping == (char *)MAP_FAILED) {
    res = MAILDIR_ERROR_FILE;
    goto close;
  }
  
  memcpy(mapping, message, size);
  
  msync(mapping, size, MS_SYNC);
  munmap(mapping, size);
  
  close(fd);

  /* write to tmp/ with maildir standard name */
  
  delivery_tmp_name = maildir_get_new_message_filename(md, tmpname);
  if (delivery_tmp_name == NULL) {
    res = MAILDIR_ERROR_FILE;
    goto unlink;
  }
  
  /* write to new/ with maildir standard name */
  
  strncpy(tmpname, delivery_tmp_name, sizeof(tmpname));
  tmpname[sizeof(tmpname) - 1] = '\0';
  
  delivery_tmp_basename = libetpan_basename(tmpname);
  
  snprintf(delivery_new_name, sizeof(delivery_new_name), "%s/new/%s",
      md->mdir_path, delivery_tmp_basename);
  
  r = link(delivery_tmp_name, delivery_new_name);
  if (r == 0) {
    unlink(delivery_tmp_name);
  }
  else if (errno == EXDEV) {
    res = MAILDIR_ERROR_FOLDER;
    goto unlink_tmp;
  }
  else if (errno == EPERM) {
    r = rename(delivery_tmp_name, delivery_new_name);
    if (r < 0) {
      res = MAILDIR_ERROR_FILE;
      goto unlink_tmp;
    }
  }
  
  snprintf(path_new, sizeof(path_new), "%s/new", md->mdir_path);
  r = stat(path_new, &stat_info);
  if (r < 0) {
    unlink(delivery_new_name);
    res = MAILDIR_ERROR_FILE;
    goto unlink_tmp;
  }

  md->mdir_mtime_new = stat_info.st_mtime;
  
  delivery_new_basename = libetpan_basename(delivery_new_name);
  
  r = add_message(md, delivery_new_basename, 1);
  if (r != MAILDIR_NO_ERROR) {
    unlink(delivery_new_name);
    res = MAILDIR_ERROR_FILE;
    goto unlink_tmp;
  }
  
  if (uid != NULL)
    strncpy(uid, delivery_new_basename, max_uid_len);
  
  free(delivery_tmp_name);
  
  return MAILDIR_NO_ERROR;
  
 unlink_tmp:
  unlink(delivery_tmp_name);
  free(delivery_tmp_name);
  goto err;
 close:
  close(fd);
 unlink:
  unlink(tmpname);
 err:
  return res;
}

int maildir_message_add(struct maildir * md,
    const char * message, size_t size)
{
  return maildir_message_add_uid(md, message, size,
      NULL, 0);
}

int maildir_message_add_file_uid(struct maildir * md, int fd,
    char * uid, size_t max_uid_len)
{
  char * message;
  struct stat buf;
  int r;
  
  if (fstat(fd, &buf) == -1)
    return MAILDIR_ERROR_FILE;
  
  message = mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (message == (char *)MAP_FAILED)
    return MAILDIR_ERROR_FILE;
  
  r = maildir_message_add_uid(md, message, buf.st_size, uid, max_uid_len);
  
  munmap(message, buf.st_size);
  
  return r;
}

int maildir_message_add_file(struct maildir * md, int fd)
{
  return maildir_message_add_file_uid(md, fd,
      NULL, 0);
}

char * maildir_message_get(struct maildir * md, const char * uid)
{
  chashdatum key;
  chashdatum value;
  char filename[PATH_MAX];
  char * dup_filename;
  struct maildir_msg * msg;
  char * dir;
  int r;
  
  key.data = (void *) uid;
  key.len = strlen(uid);
  r = chash_get(md->mdir_msg_hash, &key, &value);
  if (r < 0)
    return NULL;
  
  msg = value.data;
  if ((msg->msg_flags & MAILDIR_FLAG_NEW) != 0)
    dir = "new";
  else
    dir = "cur";
  
  snprintf(filename, sizeof(filename), "%s/%s/%s",
      md->mdir_path, dir, msg->msg_filename);
  
  dup_filename = strdup(filename);
  if (dup_filename == NULL)
    return NULL;
  
  return dup_filename;
}

int maildir_message_remove(struct maildir * md, const char * uid)
{
  chashdatum key;
  chashdatum value;
  char filename[PATH_MAX];
  struct maildir_msg * msg;
  char * dir;
  int r;
  int res;

  key.data = (void *) uid;
  key.len = strlen(uid);
  r = chash_get(md->mdir_msg_hash, &key, &value);
  if (r < 0) {
    res = MAILDIR_ERROR_NOT_FOUND;
    goto err;
  }
  
  msg = value.data;
  if ((msg->msg_flags & MAILDIR_FLAG_NEW) != 0)
    dir = "new";
  else
    dir = "cur";
  
  snprintf(filename, sizeof(filename), "%s/%s/%s",
      md->mdir_path, dir, msg->msg_filename);
  
  r = unlink(filename);
  if (r < 0) {
    res = MAILDIR_ERROR_FILE;
    goto err;
  }
  
  return MAILDIR_NO_ERROR;
  
 err:
  return res;
}

int maildir_message_change_flags(struct maildir * md,
    const char * uid, int new_flags)
{
  chashdatum key;
  chashdatum value;
  char filename[PATH_MAX];
  struct maildir_msg * msg;
  char * dir;
  int r;
  char new_filename[PATH_MAX];
  char flag_str[5];
  size_t i;
  int res;
  char * dup_filename;

  key.data = (void *) uid;
  key.len = strlen(uid);
  r = chash_get(md->mdir_msg_hash, &key, &value);
  if (r < 0) {
    res = MAILDIR_ERROR_NOT_FOUND;
    goto err;
  }
  
  msg = value.data;
  if ((msg->msg_flags & MAILDIR_FLAG_NEW) != 0)
    dir = "new";
  else
    dir = "cur";
  
  snprintf(filename, sizeof(filename), "%s/%s/%s",
      md->mdir_path, dir, msg->msg_filename);
  
  if ((new_flags & MAILDIR_FLAG_NEW) != 0)
    dir = "new";
  else
    dir = "cur";
  
  i = 0;
  if ((new_flags & MAILDIR_FLAG_SEEN) != 0) {
    flag_str[i] = 'S';
    i ++;
  }
  if ((new_flags & MAILDIR_FLAG_REPLIED) != 0) {
    flag_str[i] = 'R';
    i ++;
  }
  if ((new_flags & MAILDIR_FLAG_FLAGGED) != 0) {
    flag_str[i] = 'F';
    i ++;
  }
  if ((new_flags & MAILDIR_FLAG_TRASHED) != 0) {
    flag_str[i] = 'T';
    i ++;
  }
  flag_str[i] = 0;
  
  if (flag_str[0] == '\0')
    snprintf(new_filename, sizeof(new_filename), "%s/%s/%s",
        md->mdir_path, dir, msg->msg_uid);
  else
    snprintf(new_filename, sizeof(new_filename), "%s/%s/%s:2,%s",
        md->mdir_path, dir, msg->msg_uid, flag_str);
  
  if (strcmp(filename, new_filename) == 0)
    return MAILDIR_NO_ERROR;
  
  r = link(filename, new_filename);
  if (r == 0) {
    unlink(filename);
  }
  else if (errno == EXDEV) {
    res = MAILDIR_ERROR_FOLDER;
    goto err;
  }
  else if (errno == EPERM) {
    r = rename(filename, new_filename);
    if (r < 0) {
      res = MAILDIR_ERROR_FOLDER;
      goto err;
    }
  }
  
  dup_filename = strdup(libetpan_basename(new_filename));
  if (dup_filename != NULL) {
    free(msg->msg_filename);
    msg->msg_filename = dup_filename;
  }
  
  msg->msg_flags = new_flags;
  
  return MAILDIR_NO_ERROR;
  
 err:
  return res;
}
