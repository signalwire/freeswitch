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
 * $Id: mailstream_helper.h,v 1.12 2004/11/21 21:53:31 hoa Exp $
 */

#ifndef MAILSTREAM_HELPER_H

#define MAILSTREAM_HELPER_H

#include <libetpan/mmapstring.h>
#include <libetpan/mailstream.h>

#ifdef __cplusplus
extern "C" {
#endif

char * mailstream_read_line(mailstream * stream, MMAPString * line);

char * mailstream_read_line_append(mailstream * stream, MMAPString * line);

char * mailstream_read_line_remove_eol(mailstream * stream, MMAPString * line);

char * mailstream_read_multiline(mailstream * s, size_t size,
				  MMAPString * stream_buffer,
				  MMAPString * multiline_buffer,
				  size_t progr_rate,
				  progress_function * progr_fun);

int mailstream_is_end_multiline(const char * line);

int mailstream_send_data_crlf(mailstream * s, const char * message,
    size_t size,
    size_t progr_rate,
    progress_function * progr_fun);

int mailstream_send_data(mailstream * s, const char * message,
			  size_t size,
			  size_t progr_rate,
			  progress_function * progr_fun);

size_t mailstream_get_data_crlf_size(const char * message, size_t size);

#ifdef __cplusplus
}
#endif

#endif
