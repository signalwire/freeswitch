
/*
 *	uart.c
 *
 *	Copyright (c) 2005 Robert Krten.  All Rights Reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions
 *	are met:
 *
 *	1. Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	2. Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 *	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 *	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *	ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 *	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *	OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *	SUCH DAMAGE.
 *
 *	This module contains a simple 8-bit UART, which performs a callback
 *	with the decoded byte value.
 *
 *	2005 06 11	R. Krten		created
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "uart.h"

/*
 *	dsp_uart_attr_init
 *
 *	Initializes the attributes structure; this must be done before the
 *	attributes structure is used.
*/

void dsp_uart_attr_init (dsp_uart_attr_t *attr)
{
	memset (attr, 0, sizeof (*attr));
}

/*
 *	dsp_uart_attr_get_bytehandler
 *	dsp_uart_attr_set_bytehandler
 *
 *	These functions get and set their respective elements from the
 *	attributes structure.  If an error code is returned, it is just
 *	zero == ok, -1 == fail.
*/

bytehandler_func_t dsp_uart_attr_get_bytehandler(dsp_uart_attr_t *attr, void **bytehandler_arg)
{
	*bytehandler_arg = attr->bytehandler_arg;
	return attr->bytehandler;
}

void dsp_uart_attr_set_bytehandler(dsp_uart_attr_t *attr, bytehandler_func_t bytehandler, void *bytehandler_arg)
{
	attr->bytehandler = bytehandler;
	attr->bytehandler_arg = bytehandler_arg;
}

dsp_uart_handle_t *dsp_uart_create(dsp_uart_attr_t *attr)
{
	dsp_uart_handle_t *handle;

	handle = malloc(sizeof (*handle));
	if (handle) {
		memset(handle, 0, sizeof (*handle));

		/* fill the attributes member */
		memcpy(&handle->attr, attr, sizeof (*attr));
	}
	return handle;
}

void dsp_uart_destroy(dsp_uart_handle_t **handle)
{
	if (*handle) {
		free(*handle);
		*handle = NULL;
	}
}


void dsp_uart_bit_handler(void *x, int bit)
{
	dsp_uart_handle_t *handle = (dsp_uart_handle_t *) x;

	if (!handle->have_start) {
		if (bit) {
			return;		/* waiting for start bit (0) */
		}
		handle->have_start = 1;
		handle->data = 0;
		handle->nbits = 0;
		return;
	}

	handle->data >>= 1;
	handle->data |= 0x80 * !!bit;
	handle->nbits++;

	if (handle->nbits == 8) {
		handle->attr.bytehandler(handle->attr.bytehandler_arg, handle->data);
		handle->nbits = 0;
		handle->data = 0;
		handle->have_start = 0;
	}
/* might consider handling errors in the future... */
}

