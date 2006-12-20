/*
 * libEtPan! -- a mail library
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
 * $Id: mailprivacy.h,v 1.4 2005/11/21 16:17:57 hoa Exp $
 */

#ifndef MAILPRIVACY_H

#define MAILPRIVACY_H

#include <libetpan/mailmessage.h>
#include <libetpan/mailprivacy_types.h>
#include <libetpan/mailprivacy_tools.h>

struct mailprivacy * mailprivacy_new(char * tmp_dir, int make_alternative);

void mailprivacy_free(struct mailprivacy * privacy);

int mailprivacy_msg_get_bodystructure(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime ** result);

void mailprivacy_msg_flush(struct mailprivacy * privacy,
    mailmessage * msg_info);

int mailprivacy_msg_fetch_section(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result, size_t * result_len);

int mailprivacy_msg_fetch_section_header(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result,
    size_t * result_len);

int mailprivacy_msg_fetch_section_mime(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result,
    size_t * result_len);

int mailprivacy_msg_fetch_section_body(struct mailprivacy * privacy,
    mailmessage * msg_info,
    struct mailmime * mime,
    char ** result,
    size_t * result_len);

void mailprivacy_msg_fetch_result_free(struct mailprivacy * privacy,
    mailmessage * msg_info,
    char * msg);

int mailprivacy_msg_fetch(struct mailprivacy * privacy,
    mailmessage * msg_info,
    char ** result,
    size_t * result_len);

int mailprivacy_msg_fetch_header(struct mailprivacy * privacy,
    mailmessage * msg_info,
    char ** result,
    size_t * result_len);

int mailprivacy_register(struct mailprivacy * privacy,
    struct mailprivacy_protocol * protocol);

void mailprivacy_unregister(struct mailprivacy * privacy,
    struct mailprivacy_protocol * protocol);

char * mailprivacy_get_encryption_name(struct mailprivacy * privacy,
    char * privacy_driver, char * privacy_encryption);

/* deprecated */
int mailprivacy_encrypt(struct mailprivacy * privacy,
    char * privacy_driver, char * privacy_encryption,
    struct mailmime * mime,
    struct mailmime ** result);

/* introduced the use of passphrase */
int mailprivacy_encrypt_msg(struct mailprivacy * privacy,
    char * privacy_driver, char * privacy_encryption,
    mailmessage * msg,
    struct mailmime * mime,
    struct mailmime ** result);

void mailprivacy_debug(struct mailprivacy * privacy, FILE * f);

carray * mailprivacy_get_protocols(struct mailprivacy * privacy);

int mailprivacy_is_encrypted(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime);

void mailprivacy_recursive_unregister_mime(struct mailprivacy * privacy,
    struct mailmime * mime);

#endif
