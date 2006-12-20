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
 * $Id: mailfolder.h,v 1.4 2006/04/06 22:54:56 hoa Exp $
 */

#ifndef MAILFOLDER_H

#define MAILFOLDER_H

#include "mailstorage_types.h"

LIBETPAN_EXPORT
int mailfolder_noop(struct mailfolder * folder);

LIBETPAN_EXPORT
int mailfolder_check(struct mailfolder * folder);

LIBETPAN_EXPORT
int mailfolder_expunge(struct mailfolder * folder);

LIBETPAN_EXPORT
int mailfolder_status(struct mailfolder * folder,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);

LIBETPAN_EXPORT
int mailfolder_append_message(struct mailfolder * folder,
    char * message, size_t size);

LIBETPAN_EXPORT
int mailfolder_append_message_flags(struct mailfolder * folder,
    char * message, size_t size, struct mail_flags * flags);

LIBETPAN_EXPORT
int mailfolder_get_messages_list(struct mailfolder * folder,
    struct mailmessage_list ** result);

LIBETPAN_EXPORT
int mailfolder_get_envelopes_list(struct mailfolder * folder,
    struct mailmessage_list * result);

LIBETPAN_EXPORT
int mailfolder_get_message(struct mailfolder * folder,
    uint32_t num, mailmessage ** result);

LIBETPAN_EXPORT
int mailfolder_get_message_by_uid(struct mailfolder * folder,
    const char * uid, mailmessage ** result);

#endif
