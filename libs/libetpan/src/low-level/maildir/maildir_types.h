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
 * $Id: maildir_types.h,v 1.9 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef MAILDIR_TYPES_H

#define MAILDIR_TYPES_H

#include <sys/types.h>
#include <libetpan/libetpan-config.h>
#include <libetpan/chash.h>
#include <libetpan/carray.h>
#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif

#include <libetpan/libetpan-config.h>

#define LIBETPAN_MAILDIR

enum {
  MAILDIR_NO_ERROR = 0,
  MAILDIR_ERROR_CREATE,
  MAILDIR_ERROR_DIRECTORY,
  MAILDIR_ERROR_MEMORY,
  MAILDIR_ERROR_FILE,
  MAILDIR_ERROR_NOT_FOUND,
  MAILDIR_ERROR_FOLDER
};

#define MAILDIR_FLAG_NEW      (1 << 0)
#define MAILDIR_FLAG_SEEN     (1 << 1)
#define MAILDIR_FLAG_REPLIED  (1 << 2)
#define MAILDIR_FLAG_FLAGGED  (1 << 3)
#define MAILDIR_FLAG_TRASHED  (1 << 4)

struct maildir_msg {
  char * msg_uid;
  char * msg_filename;
  int msg_flags;
};

/*
  work around for missing #define HOST_NAME_MAX in Linux
*/

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

struct maildir {
  pid_t mdir_pid;
  char mdir_hostname[HOST_NAME_MAX];
  char mdir_path[PATH_MAX];
  uint32_t mdir_counter;
  time_t mdir_mtime_new;
  time_t mdir_mtime_cur;
  carray * mdir_msg_list;
  chash * mdir_msg_hash;
};

#endif
