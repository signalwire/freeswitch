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
 * $Id: mailsmtp.h,v 1.20 2005/12/20 17:56:11 hoa Exp $
 */

#ifndef MAILSMTP_H

#define MAILSMTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailsmtp_types.h>
#include <libetpan/mailsmtp_helper.h>
#include <libetpan/mailsmtp_socket.h>
#include <libetpan/mailsmtp_ssl.h>


LIBETPAN_EXPORT
mailsmtp * mailsmtp_new(size_t progr_rate,
			progress_function * progr_fun);

LIBETPAN_EXPORT
void mailsmtp_free(mailsmtp * session);

LIBETPAN_EXPORT
int mailsmtp_connect(mailsmtp * session, mailstream * s);

LIBETPAN_EXPORT
int mailsmtp_quit(mailsmtp * session);


/* This call is deprecated and mailesmtp_auth_sasl() should be used instead */
/**
 * Tries AUTH with detected method - "better" method first:
 * CRAM-MD5 -> PLAIN -> LOGIN
 */
LIBETPAN_EXPORT
int mailsmtp_auth(mailsmtp * session, const char * user, const char * pass);

/* This call is deprecated and mailesmtp_auth_sasl() should be used instead */
/**
 * tries to autenticate with the server using given auth-type
 * returns MAILSMTP_NO_ERROR on success
 */
LIBETPAN_EXPORT
int mailsmtp_auth_type(mailsmtp * session,
    const char * user, const char * pass, int type);

LIBETPAN_EXPORT
int mailsmtp_helo(mailsmtp * session);

LIBETPAN_EXPORT
int mailsmtp_mail(mailsmtp * session, const char * from);

LIBETPAN_EXPORT
int mailsmtp_rcpt(mailsmtp * session, const char * to);

LIBETPAN_EXPORT
int mailsmtp_data(mailsmtp * session);

LIBETPAN_EXPORT
int mailsmtp_data_message(mailsmtp * session,
			   const char * message,
			   size_t size);

LIBETPAN_EXPORT
int mailesmtp_ehlo(mailsmtp * session);

LIBETPAN_EXPORT
int mailesmtp_mail(mailsmtp * session,
		    const char * from,
		    int return_full,
		    const char * envid);

LIBETPAN_EXPORT
int mailesmtp_rcpt(mailsmtp * session,
		    const char * to,
		    int notify,
		    const char * orcpt);

LIBETPAN_EXPORT
int mailesmtp_starttls(mailsmtp * session);

LIBETPAN_EXPORT
const char * mailsmtp_strerror(int errnum);

LIBETPAN_EXPORT
int mailesmtp_auth_sasl(mailsmtp * session, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);

LIBETPAN_EXPORT
int mailsmtp_noop(mailsmtp * session);

LIBETPAN_EXPORT
int mailsmtp_reset(mailsmtp * session);

#ifdef __cplusplus
}
#endif

#endif
