/*-
 * Copyright (c) 1999,2000
 *	Konstantin Chuguev.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Konstantin Chuguev
 *	and its contributors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	iconv (Charset Conversion Library) v1.0
 */

#include <errno.h>	/* E2BIG, EINVAL, errno */
#include <stdio.h>	/* FILE, ferror, fwrite */
#include <stdlib.h>	/* free, malloc */
#include <string.h>	/* memcpy, memmove */

#include "iconv_stream.h"

#define buf_size 4096

iconv_stream *iconv_stream_open(apr_iconv_t cd, void *handle,
                                iconv_stream_func method)
{
    iconv_stream *res = malloc(sizeof(iconv_stream));
    if (!res)
        return NULL;
    res->cd = cd;
    res->chars = res->in_bytes = res->out_bytes = 0;
    res->buffer = res->buf_ptr = NULL;
    res->handle = handle;
    res->method = method;
    return res;
}

void iconv_stream_close(iconv_stream *stream)
{
    if (!stream)
        return;
    if (stream->buffer)
        free(stream->buffer);
    free(stream);
}

apr_ssize_t iconv_write(void *handle, const void *buf, apr_size_t insize)
{
#define stream ((iconv_stream *)handle)
    char buffer[4096];
    apr_size_t outsize = sizeof(buffer), size;
    char *outbuf = buffer;
    const char *inbuf = buf;
    apr_size_t chars;
    apr_status_t status;

    if (!buf)
        insize = 0;
    status = apr_iconv(stream->cd, (const char **)&buf, &insize, &outbuf, &outsize, &chars);
    if ((int)chars < 0)
        return -1;
    stream->chars += chars;
    size = outbuf - buffer;
    if (size) {
        apr_ssize_t r;
        outbuf = buffer;
        while ((r = stream->method(stream->handle, outbuf, size)) < size) {
            if (r < 0)
                return -1;
            outbuf += r;
            size -= r;
            stream->out_bytes += r;
        }
    }
    size = (const char *)buf - inbuf;
    if (size)
        stream->in_bytes += size;
    return size;
#undef stream
}

apr_ssize_t iconv_bwrite(void *handle, const void *buf, apr_size_t insize)
{
#define stream ((iconv_stream *)handle)
    apr_ssize_t res = 0;
    apr_size_t left, size = insize;
    if (!buf)
        return iconv_write(handle, NULL, 0);
    if (stream->buffer && stream->buf_ptr > stream->buffer) {
        do {
            left = stream->buffer + buf_size - stream->buf_ptr;
            if (!left) {
        	errno = E2BIG;
                return -1;
            }
            if (left > size)
                left = size;
            memcpy(stream->buf_ptr, buf, left);
            buf = ((const char *)buf) + left;
            size -= left;
            stream->buf_ptr += left;
            res = iconv_write(handle, stream->buffer,
                              stream->buf_ptr - stream->buffer);
            if (res < 0) {
                if (errno != EINVAL)
                    return -1;
                res = 0;
            }
            left = stream->buf_ptr - (stream->buffer + res);
            if (!res)
                break;
            if (left > 0)
                memmove(stream->buffer, stream->buffer + res, left);
            stream->buf_ptr -= res;
        } while (size && left);
        if (!size)
            return insize;
    }
    do {
        res = iconv_write(handle, buf, size);
        if (res <= 0) {
            if (errno != EINVAL)
                return -1;
            res = 0;
        }
        buf = ((const char *)buf) + res;
        size -= res;
    } while (size && res);
    if (!size)
        return insize;
    if (size > buf_size)
	return -1;
    if (!stream->buffer) {
        if (!(stream->buffer = malloc(buf_size)))
            return -1;
    }
    memcpy(stream->buffer, buf, size);
    stream->buf_ptr = stream->buffer + size;
    return insize;
#undef stream
}

static apr_ssize_t fwrite_wrapper(void *handle, void *buf, apr_size_t size)
{
    apr_size_t res = fwrite(buf, 1, size, (FILE *)handle);
    return (res && !ferror((FILE *)handle)) ? res : -1;
}

iconv_stream *iconv_ostream_fopen(apr_iconv_t cd, FILE *handle)
{
    return iconv_stream_open(cd, handle, fwrite_wrapper);
}
