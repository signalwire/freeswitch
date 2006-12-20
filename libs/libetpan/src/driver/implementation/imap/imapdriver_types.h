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
 * $Id: imapdriver_types.h,v 1.25 2006/05/22 13:39:40 hoa Exp $
 */

#ifndef IMAPDRIVER_TYPES_H

#define IMAPDRIVER_TYPES_H

#include <libetpan/libetpan-config.h>

#include <libetpan/mailimap.h>
#include <libetpan/maildriver_types.h>
#include <libetpan/generic_cache_types.h>
#include <libetpan/mailstorage_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IMAP driver for session */

struct imap_session_state_data {
  mailimap * imap_session;
  char * imap_mailbox;
  struct mail_flags_store * imap_flags_store;
};

enum {
  IMAP_SECTION_MESSAGE,
  IMAP_SECTION_HEADER,
  IMAP_SECTION_MIME,
  IMAP_SECTION_BODY
};

/* cached IMAP driver for session */

enum {
  IMAPDRIVER_CACHED_SET_CACHE_DIRECTORY = 1
};

struct imap_cached_session_state_data {
  mailsession * imap_ancestor;
  char * imap_quoted_mb;
  char imap_cache_directory[PATH_MAX];
  carray * imap_uid_list;
  uint32_t imap_uidvalidity;
};


/* IMAP storage */

/*
  imap_mailstorage is the state data specific to the IMAP4rev1 storage.

  - servername  this is the name of the IMAP4rev1 server
  
  - port is the port to connect to, on the server.
    you give 0 to use the default port.

  - command, if non-NULL the command used to connect to the
    server instead of allowing normal TCP connections to be used.
    
  - connection_type is the type of socket layer to use.
    The value can be CONNECTION_TYPE_PLAIN, CONNECTION_TYPE_STARTTLS,
    CONNECTION_TYPE_TRY_STARTTLS, CONNECTION_TYPE_TLS or
    CONNECTION_TYPE_COMMAND.

  - auth_type is the authenticate mechanism to use.
    The value can be IMAP_AUTH_TYPE_PLAIN.
    Other values are not yet implemented.

  - login is the login of the IMAP4rev1 account.

  - password is the password of the IMAP4rev1 account.

  - cached if this value is != 0, a persistant cache will be
    stored on local system.

  - cache_directory is the location of the cache
*/

struct imap_mailstorage {
  char * imap_servername;
  uint16_t imap_port;
  char * imap_command;
  int imap_connection_type;
  
  int imap_auth_type;
  char * imap_login; /* deprecated */
  char * imap_password; /* deprecated */
  
  int imap_cached;
  char * imap_cache_directory;
  
  struct {
    int sasl_enabled;
    char * sasl_auth_type;
    char * sasl_server_fqdn;
    char * sasl_local_ip_port;
    char * sasl_remote_ip_port;
    char * sasl_login;
    char * sasl_auth_name;
    char * sasl_password;
    char * sasl_realm;
  } imap_sasl;
};

/* this is the type of IMAP4rev1 authentication */

enum {
  IMAP_AUTH_TYPE_PLAIN,            /* plain text authentication */
  IMAP_AUTH_TYPE_SASL_ANONYMOUS,   /* SASL anonymous */
  IMAP_AUTH_TYPE_SASL_CRAM_MD5,    /* SASL CRAM MD5 */
  IMAP_AUTH_TYPE_SASL_KERBEROS_V4, /* SASL KERBEROS V4 */
  IMAP_AUTH_TYPE_SASL_PLAIN,       /* SASL plain */
  IMAP_AUTH_TYPE_SASL_SCRAM_MD5,   /* SASL SCRAM MD5 */
  IMAP_AUTH_TYPE_SASL_GSSAPI,      /* SASL GSSAPI */
  IMAP_AUTH_TYPE_SASL_DIGEST_MD5   /* SASL digest MD5 */
};


#ifdef __cplusplus
}
#endif

#endif
