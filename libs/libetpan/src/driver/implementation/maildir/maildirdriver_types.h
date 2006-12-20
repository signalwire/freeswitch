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
 * $Id: maildirdriver_types.h,v 1.6 2006/05/22 13:39:40 hoa Exp $
 */

#ifndef MAILDIRDRIVER_TYPES_H

#define MAILDIRDRIVER_TYPES_H

#include <libetpan/libetpan-config.h>

#include <libetpan/maildriver_types.h>
#include <libetpan/maildir.h>
#include <libetpan/generic_cache_types.h>
#include <libetpan/mailstorage_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct maildir_session_state_data {
  struct maildir * md_session;
  struct mail_flags_store * md_flags_store;
};

enum {
  MAILDIRDRIVER_CACHED_SET_CACHE_DIRECTORY = 1,
  MAILDIRDRIVER_CACHED_SET_FLAGS_DIRECTORY
};

struct maildir_cached_session_state_data {
  mailsession * md_ancestor;
  char * md_quoted_mb;
  struct mail_flags_store * md_flags_store;
  char md_cache_directory[PATH_MAX];
  char md_flags_directory[PATH_MAX];
};

/* maildir storage */

/*
  maildir_mailstorage is the state data specific to the maildir storage.

  - pathname is the path of the maildir storage.
  
  - cached if this value is != 0, a persistant cache will be
      stored on local system.
  
  - cache_directory is the location of the cache.

  - flags_directory is the location of the flags.
*/

struct maildir_mailstorage {
  char * md_pathname;
  
  int md_cached;
  char * md_cache_directory;
  char * md_flags_directory;
};

#ifdef __cplusplus
}
#endif

#endif
