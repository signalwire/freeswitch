/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "openzap.h"
#include "zap_zt.h"

static ZIO_CONFIGURE_SPAN_FUNCTION(zt_configure_span)
{
	return ZAP_FAIL;
}

static ZIO_CONFIGURE_FUNCTION(zt_configure)
{
	return ZAP_FAIL;
}

static ZIO_OPEN_FUNCTION(zt_open) 
{
	ZIO_OPEN_MUZZLE;
	return ZAP_FAIL;
}

static ZIO_CLOSE_FUNCTION(zt_close)
{
	ZIO_CLOSE_MUZZLE;
	return ZAP_FAIL;
}

static ZIO_COMMAND_FUNCTION(zt_command)
{
	return ZAP_FAIL;
}

static ZIO_WAIT_FUNCTION(zt_wait)
{
	return ZAP_FAIL;
}

static ZIO_READ_FUNCTION(zt_read)
{
	return ZAP_FAIL;
}

static ZIO_WRITE_FUNCTION(zt_write)
{
	return ZAP_FAIL;
}

static zap_io_interface_t zt_interface;

zap_status_t zt_init(zap_io_interface_t **zio)
{
	assert(zio != NULL);
	memset(&zt_interface, 0, sizeof(zt_interface));

	zt_interface.name = "zt";
	zt_interface.configure =  zt_configure;
	zt_interface.open = zt_open;
	zt_interface.close = zt_close;
	zt_interface.wait = zt_wait;
	zt_interface.read = zt_read;
	zt_interface.write = zt_write;
	*zio = &zt_interface;

	return ZAP_FAIL;
}

zap_status_t zt_destroy(void)
{
	return ZAP_FAIL;
}
