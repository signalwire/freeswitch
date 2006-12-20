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
 * $Id: imapstorage.h,v 1.12 2006/06/02 15:44:29 smarinier Exp $
 */

#ifndef IMAPSTORAGE_H

#define IMAPSTORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/imapdriver_types.h>

/*
  imap_mailstorage_init is the constructor for a IMAP4rev1 storage

  @param storage this is the storage to initialize.

  @param servername  this is the name of the IMAP4rev1 server
  
  @param port is the port to connect to, on the server.
    you give 0 to use the default port.

  @param command the command used to connect to the server instead of
    allowing normal TCP connections to be used.

  @param connection_type is the type of socket layer to use.
    The value can be CONNECTION_TYPE_PLAIN, CONNECTION_TYPE_STARTTLS,
    CONNECTION_TYPE_TRY_STARTTLS, CONNECTION_TYPE_TLS,
    CONNECTION_TYPE_COMMAND, CONNECTION_TYPE_COMMAND_STARTTLS,
    CONNECTION_TYPE_COMMAND_TRY_STARTTLS, CONNECTION_TYPE_COMMAND_TLS,.
    
  @param auth_type is the authenticate mechanism to use.
    The value can be IMAP_AUTH_TYPE_PLAIN.
    Other values are not yet implemented.

  @param login is the login of the IMAP4rev1 account.

  @param password is the password of the IMAP4rev1 account.

  @param cached if this value is != 0, a persistant cache will be
    stored on local system.

  @param cache_directory is the location of the cache
*/

LIBETPAN_EXPORT
int imap_mailstorage_init(struct mailstorage * storage,
    const char * imap_servername, uint16_t imap_port,
    const char * imap_command,
    int imap_connection_type, int imap_auth_type,
    const char * imap_login, const char * imap_password,
    int imap_cached, const char * imap_cache_directory);

LIBETPAN_EXPORT
int imap_mailstorage_init_sasl(struct mailstorage * storage,
    const char * imap_servername, uint16_t imap_port,
    const char * imap_command,
    int imap_connection_type,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm,
    int imap_cached, const char * imap_cache_directory);

#ifdef __cplusplus
}
#endif

#endif
