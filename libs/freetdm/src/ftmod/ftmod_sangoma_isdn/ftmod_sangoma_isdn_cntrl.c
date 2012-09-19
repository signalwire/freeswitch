/*
 * Copyright (c) 2010, Sangoma Technologies
 * David Yat Sin <davidy@sangoma.com>

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
 ******************************************************************************/

#include "ftmod_sangoma_isdn.h"

void sngisdn_set_chan_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status);

void sngisdn_set_chan_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status)
{
	ftdm_sigmsg_t sig;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)ftdmchan->span->signal_data;
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Signalling link status changed to %s\n", ftdm_signaling_status2str(status));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;
	sig.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
	sig.ev_data.sigstatus.status = status;
	ftdm_span_send_signal(ftdmchan->span, &sig);

	if (FTDM_SPAN_IS_BRI(ftdmchan->span)) {		
		sngisdn_chan_data_t		*sngisdn_info = ftdmchan->call_data;
		if (ftdm_test_flag(sngisdn_info, FLAG_ACTIVATING)) {
			ftdm_clear_flag(sngisdn_info, FLAG_ACTIVATING);

			ftdm_sched_timer(signal_data->sched, "delayed_setup", 1000, sngisdn_delayed_setup, (void*) sngisdn_info, NULL);
		}
	}
	return;
}

void sngisdn_set_span_sig_status(ftdm_span_t *span, ftdm_signaling_status_t status)
{
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;

	((sngisdn_span_data_t*)span->signal_data)->sigstatus = status;

	if (status == FTDM_SIG_STATE_UP) {
		((sngisdn_span_data_t*)span->signal_data)->dl_request_pending = 0;
	}	

	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
		sngisdn_set_chan_sig_status(((ftdm_channel_t*)ftdm_iterator_current(curr)), status);
	}
	ftdm_iterator_free(chaniter);
	return;
}


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

/******************************************************************************/

