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


#include "private/ftdm_core.h"
//#include "ftdm_skel.h"

static FIO_CONFIGURE_FUNCTION(skel_configure)
{
	ftdm_unused_arg(category);
	ftdm_unused_arg(var);
	ftdm_unused_arg(val);
	ftdm_unused_arg(lineno);
	return FTDM_FAIL;
}

static FIO_CONFIGURE_SPAN_FUNCTION(skel_configure_span)
{
	ftdm_unused_arg(span);
	ftdm_unused_arg(str);
	ftdm_unused_arg(type);
	ftdm_unused_arg(name);
	ftdm_unused_arg(number);
	return FTDM_FAIL;
}

static FIO_OPEN_FUNCTION(skel_open)
{
	ftdm_unused_arg(ftdmchan);
	return FTDM_FAIL;
}

static FIO_CLOSE_FUNCTION(skel_close)
{
	ftdm_unused_arg(ftdmchan);
	return FTDM_FAIL;
}

static FIO_WAIT_FUNCTION(skel_wait)
{
	ftdm_unused_arg(ftdmchan);
	ftdm_unused_arg(flags);
	ftdm_unused_arg(to);
	return FTDM_FAIL;
}

static FIO_READ_FUNCTION(skel_read)
{
	ftdm_unused_arg(ftdmchan);
	ftdm_unused_arg(data);
	ftdm_unused_arg(datalen);
	return FTDM_FAIL;
}

static FIO_WRITE_FUNCTION(skel_write)
{
	ftdm_unused_arg(ftdmchan);
	ftdm_unused_arg(data);
	ftdm_unused_arg(datalen);
	return FTDM_FAIL;
}

static FIO_COMMAND_FUNCTION(skel_command)
{
	ftdm_unused_arg(ftdmchan);
	ftdm_unused_arg(command);
	ftdm_unused_arg(obj);
	return FTDM_FAIL;
}

static FIO_SPAN_POLL_EVENT_FUNCTION(skel_poll_event)
{
	ftdm_unused_arg(span);
	ftdm_unused_arg(ms);
	ftdm_unused_arg(poll_events);
	return FTDM_FAIL;
}

static FIO_SPAN_NEXT_EVENT_FUNCTION(skel_next_event)
{
	ftdm_unused_arg(span);
	ftdm_unused_arg(event);
	return FTDM_FAIL;
}

static FIO_CHANNEL_DESTROY_FUNCTION(skel_channel_destroy)
{
	ftdm_unused_arg(ftdmchan);
	return FTDM_FAIL;
}

static FIO_SPAN_DESTROY_FUNCTION(skel_span_destroy)
{
	ftdm_unused_arg(span);
	return FTDM_FAIL;
}

static FIO_GET_ALARMS_FUNCTION(skel_get_alarms)
{
	ftdm_unused_arg(ftdmchan);
	return FTDM_FAIL;
}

static ftdm_io_interface_t skel_interface;

static FIO_IO_LOAD_FUNCTION(skel_init)
{
	assert(fio != NULL);
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
	*fio = &skel_interface;

	return FTDM_SUCCESS;
}

static FIO_IO_UNLOAD_FUNCTION(skel_destroy)
{
	return FTDM_SUCCESS;
}


ftdm_module_t ftdm_module = { 
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

