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
 * $Id: mailsmtp_helper.h,v 1.11 2006/05/22 13:39:43 hoa Exp $
 */

#ifndef MAILSMTP_HELPER_H

#define MAILSMTP_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mailsmtp_types.h"
#include "clist.h"

int mailsmtp_init(mailsmtp * session);

int mailesmtp_send(mailsmtp * session,
		    const char * from,
		    int return_full,
		    const char * envid,
		    clist * addresses,
		    const char * message, size_t size);

int mailsmtp_send(mailsmtp * session,
		   const char * from,
		   clist * addresses,
		   const char * message, size_t size);

clist * esmtp_address_list_new(void);
int esmtp_address_list_add(clist * list, char * address,
			       int notify, char * orcpt);
void esmtp_address_list_free(clist * l);

clist * smtp_address_list_new(void);
int smtp_address_list_add(clist * list, char * address);
void smtp_address_list_free(clist * l);

#ifdef __cplusplus
}
#endif

#endif
