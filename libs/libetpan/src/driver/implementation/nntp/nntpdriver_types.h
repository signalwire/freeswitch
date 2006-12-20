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
 * $Id: nntpdriver_types.h,v 1.9 2006/05/22 13:39:40 hoa Exp $
 */

#ifndef NNTPDRIVER_TYPES_H

#define NNTPDRIVER_TYPES_H

#include <libetpan/libetpan-config.h>

#include <libetpan/maildriver_types.h>
#include <libetpan/newsnntp.h>
#include <libetpan/clist.h>
#include <libetpan/generic_cache_types.h>
#include <libetpan/mailstorage_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NNTP driver for session */

enum {
  NNTPDRIVER_SET_MAX_ARTICLES = 1
};

struct nntp_session_state_data {
  newsnntp * nntp_session;
  char * nntp_userid;
  char * nntp_password;

  struct newsnntp_group_info * nntp_group_info;
  char * nntp_group_name;

  clist * nntp_subscribed_list;

  uint32_t nntp_max_articles;

  int nntp_mode_reader;
};

/* cached NNTP driver for session */

enum {
  /* the mapping of the parameters should be the same as for nntp */
  NNTPDRIVER_CACHED_SET_MAX_ARTICLES = 1,
  /* cache specific */
  NNTPDRIVER_CACHED_SET_CACHE_DIRECTORY,
  NNTPDRIVER_CACHED_SET_FLAGS_DIRECTORY
};

struct nntp_cached_session_state_data {
  mailsession * nntp_ancestor;
  char nntp_cache_directory[PATH_MAX];
  char nntp_flags_directory[PATH_MAX];
  struct mail_flags_store * nntp_flags_store;
};


/* nntp storage */

/*
  nntp_mailstorage is the state data specific to the IMAP4rev1 storage.

  - storage this is the storage to initialize.

  - servername  this is the name of the NNTP server
  
  - port is the port to connect to, on the server.
    you give 0 to use the default port.

  - connection_type is the type of socket layer to use.
    The value can be CONNECTION_TYPE_PLAIN or CONNECTION_TYPE_TLS.
    
  - auth_type is the authenticate mechanism to use.
    The value can be NNTP_AUTH_TYPE_PLAIN.

  - login is the login of the POP3 account.

  - password is the password of the POP3 account.

  - cached if this value is != 0, a persistant cache will be
    stored on local system.

  - cache_directory is the location of the cache

  - flags_directory is the location of the flags
*/

struct nntp_mailstorage {
  char * nntp_servername;
  uint16_t nntp_port;
  char * nntp_command;
  int nntp_connection_type;

  int nntp_auth_type;
  char * nntp_login;
  char * nntp_password;

  int nntp_cached;
  char * nntp_cache_directory;
  char * nntp_flags_directory;
};

/* this is the type of NNTP authentication */

enum {
  NNTP_AUTH_TYPE_PLAIN  /* plain text authentication */
};

#ifdef __cplusplus
}
#endif

#endif
