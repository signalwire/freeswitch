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
 * $Id: mailpop3.h,v 1.16 2005/07/16 17:55:57 hoa Exp $
 */

#ifndef MAILPOP3_H

#define MAILPOP3_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailpop3_types.h>

#include <libetpan/mailpop3_helper.h>

#include <libetpan/mailpop3_socket.h>
#include <libetpan/mailpop3_ssl.h>

#define POP3_STRING_SIZE 513

mailpop3 * mailpop3_new(size_t pop3_progr_rate,
    progress_function * pop3_progr_fun);

void mailpop3_free(mailpop3 * f);

int mailpop3_connect(mailpop3 * f, mailstream * s);

int mailpop3_quit(mailpop3 * f);


int mailpop3_apop(mailpop3 * f, const char * user, const char * password);

int mailpop3_user(mailpop3 * f, const char * user);

int mailpop3_pass(mailpop3 * f, const char * password);

void mailpop3_list(mailpop3 * f, carray ** result);

int mailpop3_retr(mailpop3 * f, unsigned int index, char ** result,
		  size_t * result_len);

int mailpop3_top(mailpop3 * f, unsigned int index,
    unsigned int count, char ** result,
    size_t * result_len);

int mailpop3_dele(mailpop3 * f, unsigned int index);

int mailpop3_noop(mailpop3 * f);

int mailpop3_rset(mailpop3 * f);

void mailpop3_top_free(char * str);

void mailpop3_retr_free(char * str);

int mailpop3_get_msg_info(mailpop3 * f, unsigned int index,
			   struct mailpop3_msg_info ** result);

int mailpop3_capa(mailpop3 * f, clist ** result);

void mailpop3_capa_resp_free(clist * capa_list);

int mailpop3_stls(mailpop3 * f);

int mailpop3_auth(mailpop3 * f, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);

#ifdef __cplusplus
}
#endif

#endif
