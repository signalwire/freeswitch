/*
 * Copyright (c) 2007-2012, Anthony Minessale II
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
//#include "zap_skel.h"

static ZIO_CONFIGURE_FUNCTION(skel_configure)
{
	return ZAP_FAIL;
}

static ZIO_CONFIGURE_SPAN_FUNCTION(skel_configure_span)
{
	return ZAP_FAIL;
}

static ZIO_OPEN_FUNCTION(skel_open) 
{
	return ZAP_FAIL;
}

static ZIO_CLOSE_FUNCTION(skel_close)
{
	return ZAP_FAIL;
}

static ZIO_WAIT_FUNCTION(skel_wait)
{
	return ZAP_FAIL;
}

static ZIO_READ_FUNCTION(skel_read)
{
	return ZAP_FAIL;
}

static ZIO_WRITE_FUNCTION(skel_write)
{
	return ZAP_FAIL;
}

static ZIO_COMMAND_FUNCTION(skel_command)
{
	return ZAP_FAIL;
}

static ZIO_SPAN_POLL_EVENT_FUNCTION(skel_poll_event)
{
	return ZAP_FAIL;
}

static ZIO_SPAN_NEXT_EVENT_FUNCTION(skel_next_event)
{
	return ZAP_FAIL;
}

static ZIO_CHANNEL_DESTROY_FUNCTION(skel_channel_destroy)
{
	return ZAP_FAIL;
}

static ZIO_SPAN_DESTROY_FUNCTION(skel_span_destroy)
{
	return ZAP_FAIL;
}

static ZIO_GET_ALARMS_FUNCTION(skel_get_alarms)
{
	return ZAP_FAIL;
}

static zap_io_interface_t skel_interface;

static ZIO_IO_LOAD_FUNCTION(skel_init)
{
	assert(zio != NULL);
	memset(&skel_interface, 0, sizeof(skel_interface));

	skel_interface.name = "skel";
	skel_interface.configure =  skel_configure;
	skel_interface.configure_span =  skel_configure_span;
	skel_interface.open = skel_open;
	skel_interface.close = skel_close;
	skel_interface.wait = skel_wait;
	skel_interface.read = skel_read;
	skel_interface.write = skel_write;
	skel_interface.command = skel_command;
	skel_interface.poll_event = skel_poll_event;
	skel_interface.next_event = skel_next_event;
	skel_interface.channel_destroy = skel_channel_destroy;
	skel_interface.span_destroy = skel_span_destroy;
	skel_interface.get_alarms = skel_get_alarms;
	*zio = &skel_interface;

	return ZAP_SUCCESS;
}

static ZIO_IO_UNLOAD_FUNCTION(skel_destroy)
{
	return ZAP_SUCCESS;
}


zap_module_t zap_module = { 
	"skel",
	skel_init,
	skel_destroy,
};


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

