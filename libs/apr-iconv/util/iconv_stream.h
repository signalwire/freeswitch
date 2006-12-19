#ifndef _ICONV_STREAM_H_
#define _ICONV_STREAM_H_

#include <stdio.h>	/* FILE */
#include "iconv.h"	/* iconv_t */

typedef apr_ssize_t (*iconv_stream_func)(void *d, void *buf, apr_size_t nbytes);

typedef struct {
	apr_iconv_t	cd;
	apr_size_t	chars;
	apr_size_t	in_bytes;
	apr_size_t	out_bytes;
	char *		buffer;
	char *		buf_ptr;
	void *		handle;
	iconv_stream_func method;
} iconv_stream;

iconv_stream *iconv_stream_open(apr_iconv_t cd, void *handle,
                                iconv_stream_func method);
void iconv_stream_close(iconv_stream *stream);

iconv_stream *iconv_ostream_fopen(apr_iconv_t cd, FILE *handle);

apr_ssize_t iconv_write(void *stream, const void *buf, apr_size_t nbytes);
apr_ssize_t iconv_bwrite(void *stream, const void *buf, apr_size_t nbytes);

#endif /*_ICONV_STREAM_H_*/
