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
 * $Id: mhdriver_types.h,v 1.8 2006/05/22 13:39:40 hoa Exp $
 */

#ifndef MHDRIVER_TYPES_H

#define MHDRIVER_TYPES_H

#include <libetpan/libetpan-config.h>

#include <libetpan/maildriver_types.h>
#include <libetpan/mailmh.h>
#include <libetpan/clist.h>
#include <libetpan/generic_cache_types.h>
#include <libetpan/mailstorage_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mh_session_state_data {
  struct mailmh * mh_session;

  struct mailmh_folder * mh_cur_folder;

  clist * mh_subscribed_list;
};

enum {
  MHDRIVER_CACHED_SET_CACHE_DIRECTORY = 1,
  MHDRIVER_CACHED_SET_FLAGS_DIRECTORY
};

struct mh_cached_session_state_data {
  mailsession * mh_ancestor;
  char * mh_quoted_mb;
  char mh_cache_directory[PATH_MAX];
  char mh_flags_directory[PATH_MAX];
  struct mail_flags_store * mh_flags_store;
};

/* mh storage */

/*
  mh_mailstorage is the state data specific to the MH storage.

  - pathname is the root path of the MH storage.
  
  - cached if this value is != 0, a persistant cache will be
      stored on local system.
  
  - cache_directory is the location of the cache.

  - flags_directory is the location of the flags.
*/

struct mh_mailstorage {
  char * mh_pathname;
  
  int mh_cached;
  char * mh_cache_directory;
  char * mh_flags_directory;
};

#ifdef __cplusplus
}
#endif

#endif
