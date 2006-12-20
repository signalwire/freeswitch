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
 * $Id: mailmh.h,v 1.26 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef MAILMH_H

#define MAILMH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#include <libetpan/libetpan-config.h>
#include <libetpan/carray.h>
#include <libetpan/chash.h>

enum {
  MAILMH_NO_ERROR = 0,
  MAILMH_ERROR_FOLDER,
  MAILMH_ERROR_MEMORY,
  MAILMH_ERROR_FILE,
  MAILMH_ERROR_COULD_NOT_ALLOC_MSG,
  MAILMH_ERROR_RENAME,
  MAILMH_ERROR_MSG_NOT_FOUND
};

struct mailmh {
  struct mailmh_folder * mh_main;
};

struct mailmh_msg_info {
  unsigned int msg_array_index;
  uint32_t msg_index;
  size_t msg_size;
  time_t msg_mtime;
};

struct mailmh_folder {
  char * fl_filename;
  unsigned int fl_array_index;

  char * fl_name;
  time_t fl_mtime;
  struct mailmh_folder * fl_parent;
  uint32_t fl_max_index;

  carray * fl_msgs_tab;
#if 0
  cinthash_t * fl_msgs_hash;
#endif
  chash * fl_msgs_hash;

  carray * fl_subfolders_tab;
  chash * fl_subfolders_hash;
};

struct mailmh * mailmh_new(const char * foldername);
void mailmh_free(struct mailmh * f);

struct mailmh_msg_info *
mailmh_msg_info_new(uint32_t index, size_t size, time_t mtime);
void mailmh_msg_info_free(struct mailmh_msg_info * msg_info);

struct mailmh_folder * mailmh_folder_new(struct mailmh_folder * parent,
					 const char * name);
void mailmh_folder_free(struct mailmh_folder * folder);

int mailmh_folder_add_subfolder(struct mailmh_folder * parent,
				const char * name);

struct mailmh_folder * mailmh_folder_find(struct mailmh_folder * root,
					  const char * filename);

int mailmh_folder_remove_subfolder(struct mailmh_folder * folder);

int mailmh_folder_rename_subfolder(struct mailmh_folder * src_folder,
				   struct mailmh_folder * dst_folder,
				   const char * new_name);

int mailmh_folder_get_message_filename(struct mailmh_folder * folder,
				       uint32_t index, char ** result);

int mailmh_folder_get_message_fd(struct mailmh_folder * folder,
				 uint32_t index, int flags, int * result);

int mailmh_folder_get_message_size(struct mailmh_folder * folder,
				   uint32_t index, size_t * result);

int mailmh_folder_add_message_uid(struct mailmh_folder * folder,
    const char * message, size_t size,
    uint32_t * pindex);

int mailmh_folder_add_message(struct mailmh_folder * folder,
			      const char * message, size_t size);

int mailmh_folder_add_message_file_uid(struct mailmh_folder * folder,
    int fd, uint32_t * pindex);

int mailmh_folder_add_message_file(struct mailmh_folder * folder,
				   int fd);

int mailmh_folder_remove_message(struct mailmh_folder * folder,
				 uint32_t index);

int mailmh_folder_move_message(struct mailmh_folder * dest_folder,
			       struct mailmh_folder * src_folder,
			       uint32_t index);

int mailmh_folder_update(struct mailmh_folder * folder);

unsigned int mailmh_folder_get_message_number(struct mailmh_folder * folder);

#ifdef __cplusplus
}
#endif

#endif
