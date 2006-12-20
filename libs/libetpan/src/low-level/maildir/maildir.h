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
 * $Id: maildir.h,v 1.5 2004/11/21 21:53:38 hoa Exp $
 */

#ifndef MAILDIR_H

#define MAILDIR_H

#include <libetpan/maildir_types.h>

struct maildir * maildir_new(const char * path);

void maildir_free(struct maildir * md);

int maildir_update(struct maildir * md);

int maildir_message_add_uid(struct maildir * md,
    const char * message, size_t size,
    char * uid, size_t max_uid_len);

int maildir_message_add(struct maildir * md,
    const char * message, size_t size);

int maildir_message_add_file_uid(struct maildir * md, int fd,
    char * uid, size_t max_uid_len);

int maildir_message_add_file(struct maildir * md, int fd);

char * maildir_message_get(struct maildir * md, const char * uid);

int maildir_message_remove(struct maildir * md, const char * uid);

int maildir_message_change_flags(struct maildir * md,
    const char * uid, int new_flags);

#endif
