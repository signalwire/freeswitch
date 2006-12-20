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
 * $Id: mailmime_content.h,v 1.15 2005/06/01 12:22:19 smarinier Exp $
 */

#ifndef MAILMIME_CONTENT_H

#define MAILMIME_CONTENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailmime_types.h>

LIBETPAN_EXPORT
char * mailmime_content_charset_get(struct mailmime_content * content);

LIBETPAN_EXPORT
char * mailmime_content_param_get(struct mailmime_content * content,
				  char * name);

LIBETPAN_EXPORT
int mailmime_parse(const char * message, size_t length,
		   size_t * index, struct mailmime ** result);

LIBETPAN_EXPORT
int mailmime_get_section(struct mailmime * mime,
			 struct mailmime_section * section,
			 struct mailmime ** result);


LIBETPAN_EXPORT
char * mailmime_extract_boundary(struct mailmime_content * content_type);


/* decode */

LIBETPAN_EXPORT
int mailmime_base64_body_parse(const char * message, size_t length,
			       size_t * index, char ** result,
			       size_t * result_len);

LIBETPAN_EXPORT
int mailmime_quoted_printable_body_parse(const char * message, size_t length,
					 size_t * index, char ** result,
					 size_t * result_len, int in_header);


LIBETPAN_EXPORT
int mailmime_binary_body_parse(const char * message, size_t length,
			       size_t * index, char ** result,
			       size_t * result_len);

LIBETPAN_EXPORT
int mailmime_part_parse(const char * message, size_t length,
			size_t * index,
			int encoding, char ** result, size_t * result_len);


LIBETPAN_EXPORT
int mailmime_get_section_id(struct mailmime * mime,
			    struct mailmime_section ** result);

#ifdef __cplusplus
}
#endif

#endif
