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
 * $Id: mboxdriver_types.h,v 1.7 2006/05/22 13:39:40 hoa Exp $
 */

#ifndef MBOXDRIVER_TYPES_H

#define MBOXDRIVER_TYPES_H

#include <libetpan/maildriver_types.h>
#include <libetpan/mailmbox.h>
#include <libetpan/mailstorage_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* mbox driver */

enum {
  MBOXDRIVER_SET_READ_ONLY = 1,
  MBOXDRIVER_SET_NO_UID
};

struct mbox_session_state_data {
  struct mailmbox_folder * mbox_folder;
  int mbox_force_read_only;
  int mbox_force_no_uid;
};

/* cached version */

enum {
  /* the mapping of the parameters should be the same as for mbox */
  MBOXDRIVER_CACHED_SET_READ_ONLY = 1,
  MBOXDRIVER_CACHED_SET_NO_UID,
  /* cache specific */
  MBOXDRIVER_CACHED_SET_CACHE_DIRECTORY,
  MBOXDRIVER_CACHED_SET_FLAGS_DIRECTORY
};

struct mbox_cached_session_state_data {
  mailsession * mbox_ancestor;
  char * mbox_quoted_mb;
  char mbox_cache_directory[PATH_MAX];
  char mbox_flags_directory[PATH_MAX];
  struct mail_flags_store * mbox_flags_store;
};

/* mbox storage */

/*
  mbox_mailstorage is the state data specific to the mbox storage.

  - pathname is the filename that contains the mailbox.
  
  - cached if this value is != 0, a persistant cache will be
      stored on local system.
  
  - cache_directory is the location of the cache.

  - flags_directory is the location of the flags.
*/

struct mbox_mailstorage {
  char * mbox_pathname;
  
  int mbox_cached;
  char * mbox_cache_directory;
  char * mbox_flags_directory;
};

#ifdef __cplusplus
}
#endif

#endif
