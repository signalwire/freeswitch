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
 * $Id: mailmime_write_generic.h,v 1.2 2004/11/21 21:53:39 hoa Exp $
 */

#ifndef MAILMIME_WRITE_GENERIC_H

#define  MAILMIME_WRITE_GENERIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailmime_types.h>
#include <stdio.h>

int mailmime_fields_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			  struct mailmime_fields * fields);

int mailmime_content_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			   struct mailmime_content * content);

int mailmime_content_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailmime_content * content);

int mailmime_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
		   struct mailmime * build_info);

int mailmime_quoted_printable_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, int istext,
    const char * text, size_t size);

int mailmime_base64_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    const char * text, size_t size);

int mailmime_data_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    struct mailmime_data * mime_data,
    int istext);

#ifdef __cplusplus
}
#endif

#endif
