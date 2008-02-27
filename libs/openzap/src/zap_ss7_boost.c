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
#include "ss7_boost_client.h"
#include "zap_ss7_boost.h"


static ZIO_CHANNEL_REQUEST_FUNCTION(ss7_boost_channel_request)
{
	zap_status_t status = ZAP_SUCCESS;
	return status;
}


static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(ss7_boost_outgoing_call)
{
	zap_status_t status = ZAP_SUCCESS;
	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DIALING);
	return status;
}

static void *zap_ss7_boost_run(zap_thread_t *me, void *obj)
{
    zap_span_t *span = (zap_span_t *) obj;
    zap_ss7_boost_data_t *ss7_boost_data = span->signal_data;

	while (zap_running() && zap_test_flag(ss7_boost_data, ZAP_SS7_BOOST_RUNNING)) {
		break;
	}

	zap_clear_flag(ss7_boost_data, ZAP_SS7_BOOST_RUNNING);

	zap_log(ZAP_LOG_DEBUG, "SS7_BOOST thread ended.\n");
	return NULL;
}

zap_status_t zap_ss7_boost_init(void)
{
	return ZAP_SUCCESS;
}

zap_status_t zap_ss7_boost_start(zap_span_t *span)
{
	zap_ss7_boost_data_t *ss7_boost_data = span->signal_data;
	zap_set_flag(ss7_boost_data, ZAP_SS7_BOOST_RUNNING);
	return zap_thread_create_detached(zap_ss7_boost_run, span);
}

zap_status_t zap_ss7_boost_configure_span(zap_span_t *span,
										  const char *local_ip, int local_port, 
										  const char *remote_ip, int remote_port,
										  zio_signal_cb_t sig_cb)
{
	zap_ss7_boost_data_t *ss7_boost_data = NULL;
	
	ss7_boost_data = malloc(sizeof(*ss7_boost_data));
	assert(ss7_boost_data);
	memset(ss7_boost_data, 0, sizeof(*ss7_boost_data));
	
	ss7_boost_data->local_ip = local_ip;
	ss7_boost_data->local_port = local_port;
	ss7_boost_data->remote_ip = remote_ip;
	ss7_boost_data->remote_port = remote_port;
	ss7_boost_data->signal_cb = sig_cb;

	span->signal_data = ss7_boost_data;
    span->signal_type = ZAP_SIGTYPE_SS7BOOST;
    span->outgoing_call = ss7_boost_outgoing_call;
	span->channel_request = ss7_boost_channel_request;
	
	return ZAP_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
