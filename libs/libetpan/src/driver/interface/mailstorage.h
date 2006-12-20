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
 * $Id: mailstorage.h,v 1.17 2006/06/02 15:44:30 smarinier Exp $
 */

#ifndef MAIL_STORAGE_H

#define MAIL_STORAGE_H

#include <libetpan/maildriver_types.h>
#include <libetpan/mailstorage_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* storage */

/*
  mailstorage_new

  This function creates an empty storage. This storage have to be initialized.
  The "driver" and "data" fields should be initialized.

  @param id  is the name of the storage. It can be NULL.
    The given parameter is no more needed when the creation is finished.
    The given string is duplicated.

  @return The mail storage is returned.
*/

LIBETPAN_EXPORT
struct mailstorage * mailstorage_new(const char * sto_id);

LIBETPAN_EXPORT
void mailstorage_free(struct mailstorage * storage);

/*
  session will be initialized on success.
*/

LIBETPAN_EXPORT
int mailstorage_connect(struct mailstorage * storage);

LIBETPAN_EXPORT
void mailstorage_disconnect(struct mailstorage * storage);

LIBETPAN_EXPORT
int mailstorage_noop(struct mailstorage * storage);


/* folder */

LIBETPAN_EXPORT
struct mailfolder * mailfolder_new(struct mailstorage * fld_storage,
    const char * fld_pathname, const char * fld_virtual_name);

LIBETPAN_EXPORT
void mailfolder_free(struct mailfolder * folder);

LIBETPAN_EXPORT
int mailfolder_add_child(struct mailfolder * parent,
        struct mailfolder * child);

LIBETPAN_EXPORT
int mailfolder_detach_parent(struct mailfolder * folder);

LIBETPAN_EXPORT
int mailfolder_connect(struct mailfolder * folder);

LIBETPAN_EXPORT
void mailfolder_disconnect(struct mailfolder * folder);

#ifdef __cplusplus
}
#endif

#endif


