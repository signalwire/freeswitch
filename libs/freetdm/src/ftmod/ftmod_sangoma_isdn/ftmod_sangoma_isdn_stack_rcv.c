/*
 * Copyright (c) 2010, Sangoma Technologies
 * David Yat Sin <dyatsin@sangoma.com>
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

#include "ftmod_sangoma_isdn.h"

void sngisdn_rcv_con_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, ConEvnt *conEvnt, int16_t dChan, uint8_t ces)
{
	uint8_t bchan_no = 0;
	sngisdn_chan_data_t *sngisdn_info = NULL;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	ftdm_assert(g_sngisdn_data.ccs[suId].activation_done != 0, "Con Ind on unconfigured cc\n");
	ftdm_assert(g_sngisdn_data.dchans[dChan].num_spans != 0, "Con Ind on unconfigured dchan\n");
		
	if (conEvnt->chanId.eh.pres != PRSNT_NODEF) {
		/* TODO: Implement me */
		ftdm_log(FTDM_LOG_ERROR, "Incoming call without Channel Id not supported yet\n");
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	if (conEvnt->chanId.chanNmbSlotMap.pres) {
		bchan_no = conEvnt->chanId.chanNmbSlotMap.val[0];
	} else if (conEvnt->chanId.infoChanSel.pres) {
		bchan_no = conEvnt->chanId.infoChanSel.val;
	}

	if (!bchan_no) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to obtain b-channel number from SETUP message\n");
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	if (g_sngisdn_data.dchans[dChan].channels[bchan_no] == NULL) {
		ftdm_log(FTDM_LOG_ERROR, "Incoming call on unconfigured b-channel:%d\n", bchan_no);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	sngisdn_info = g_sngisdn_data.dchans[dChan].channels[bchan_no];

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received SETUP (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_CON_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->dChan = dChan;
	sngisdn_event->ces = ces;

	ftdm_mutex_lock(g_sngisdn_data.ccs[suId].mutex);
	g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] = sngisdn_info;	
	ftdm_mutex_unlock(g_sngisdn_data.ccs[suId].mutex);

	memcpy(&sngisdn_event->event.conEvnt, conEvnt, sizeof(*conEvnt));

	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_con_cfm (int16_t suId, uint32_t suInstId, uint32_t spInstId, CnStEvnt *cnStEvnt, int16_t dChan, uint8_t ces)
{
	sngisdn_chan_data_t *sngisdn_info = NULL;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	ftdm_assert(g_sngisdn_data.ccs[suId].activation_done != 0, "Con Cfm on unconfigured cc\n");
	ftdm_assert(g_sngisdn_data.dchans[dChan].num_spans != 0, "Con Cfm on unconfigured dchan\n");

	if (get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	if (!sngisdn_info->spInstId) {
		ftdm_mutex_lock(g_sngisdn_data.ccs[suId].mutex);

		sngisdn_info->spInstId = spInstId;
		g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] = sngisdn_info;
		ftdm_mutex_unlock(g_sngisdn_data.ccs[suId].mutex);
	}

	
	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received CONNECT/CONNECT ACK (suId:%u suInstId:%u spInstId:%u ces:%d)\n", suId, suInstId, spInstId, ces);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_CON_CFM;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->dChan = dChan;
	sngisdn_event->ces = ces;
	memcpy(&sngisdn_event->event.cnStEvnt, cnStEvnt, sizeof(*cnStEvnt));
	
	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_cnst_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, CnStEvnt *cnStEvnt, uint8_t evntType, int16_t dChan, uint8_t ces)
{	
	sngisdn_chan_data_t *sngisdn_info = NULL;
	sngisdn_event_data_t *sngisdn_event = NULL;
	
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	ftdm_assert(g_sngisdn_data.ccs[suId].activation_done != 0, "Cnst Ind on unconfigured cc\n");
	ftdm_assert(g_sngisdn_data.dchans[dChan].num_spans != 0, "Cnst Ind on unconfigured dchan\n");

	if (get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	if (!sngisdn_info->spInstId) {
		ftdm_mutex_lock(g_sngisdn_data.ccs[suId].mutex);

		sngisdn_info->spInstId = spInstId;
		g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] = sngisdn_info;
		ftdm_mutex_unlock(g_sngisdn_data.ccs[suId].mutex);
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received %s (suId:%u suInstId:%u spInstId:%u ces:%d)\n",
													(evntType == MI_ALERTING)?"ALERT":
													(evntType == MI_CALLPROC)?"PROCEED":
													(evntType == MI_PROGRESS)?"PROGRESS":
													(evntType == MI_SETUPACK)?"SETUP ACK":
															(evntType == MI_INFO)?"INFO":"UNKNOWN",
															suId, suInstId, spInstId, ces);

	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_CNST_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->dChan = dChan;
	sngisdn_event->ces = ces;
	sngisdn_event->evntType = evntType;

	memcpy(&sngisdn_event->event.cnStEvnt, cnStEvnt, sizeof(*cnStEvnt));
	
	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_disc_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, DiscEvnt *discEvnt)
{
	sngisdn_chan_data_t *sngisdn_info = NULL;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	ftdm_assert(spInstId != 0, "Received DISCONNECT with invalid id");

	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}
	
	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received DISCONNECT (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_DISC_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;

	memcpy(&sngisdn_event->event.discEvnt, discEvnt, sizeof(*discEvnt));
	
	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_rel_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, RelEvnt *relEvnt)
{
	sngisdn_chan_data_t  *sngisdn_info = NULL;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		/* It seems that Trillium has a bug where they sometimes send release twice on a call, so do not crash on these for now */
		/* ftdm_assert(0, "Inconsistent call states\n"); */
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received RELEASE/RELEASE COMPLETE (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_REL_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;

	memcpy(&sngisdn_event->event.relEvnt, relEvnt, sizeof(*relEvnt));
	
 	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_dat_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, InfoEvnt *infoEvnt)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;
	
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received DATA IND suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_DAT_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;

	memcpy(&sngisdn_event->event.infoEvnt, infoEvnt, sizeof(*infoEvnt));

 	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_sshl_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, SsHlEvnt *ssHlEvnt, uint8_t action)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received SSHL IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_SSHL_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->action = action;

	memcpy(&sngisdn_event->event.ssHlEvnt, ssHlEvnt, sizeof(*ssHlEvnt));

 	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_sshl_cfm (int16_t suId, uint32_t suInstId, uint32_t spInstId, SsHlEvnt *ssHlEvnt, uint8_t action)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received SSHL CFM (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_SSHL_CFM;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->action = action;

	memcpy(&sngisdn_event->event.ssHlEvnt, ssHlEvnt, sizeof(*ssHlEvnt));

	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}
void sngisdn_rcv_rmrt_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, RmRtEvnt *rmRtEvnt, uint8_t action)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received RMRT IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_RMRT_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->action = action;

	memcpy(&sngisdn_event->event.rmRtEvnt, rmRtEvnt, sizeof(*rmRtEvnt));

	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_rmrt_cfm (int16_t suId, uint32_t suInstId, uint32_t spInstId, RmRtEvnt *rmRtEvnt, uint8_t action)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received RESUME/RETRIEVE CFM (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_RMRT_CFM;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->action = action;

	memcpy(&sngisdn_event->event.rmRtEvnt, rmRtEvnt, sizeof(*rmRtEvnt));

	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_flc_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, StaEvnt *staEvnt)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
			 !(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received FLOW CONTROL IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_FLC_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;

	memcpy(&sngisdn_event->event.staEvnt, staEvnt, sizeof(*staEvnt));

	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}


void sngisdn_rcv_fac_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, FacEvnt *facEvnt, uint8_t evntType, int16_t dChan, uint8_t ces)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_assert(0, "Inconsistent call states\n");
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received FACILITY IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_FAC_IND;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;
	sngisdn_event->evntType = evntType;
	
	memcpy(&sngisdn_event->event.facEvnt, facEvnt, sizeof(*facEvnt));

 	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}


void sngisdn_rcv_sta_cfm (int16_t suId, uint32_t suInstId, uint32_t spInstId, StaEvnt *staEvnt)
{
	sngisdn_chan_data_t  *sngisdn_info;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	/* We sometimes receive a STA CFM after receiving a RELEASE/RELEASE COMPLETE, so we need to lock
		here in case we are calling clear_call_data at the same time this function is called */

	ftdm_mutex_lock(g_sngisdn_data.ccs[suId].mutex);	
	if (!(spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) &&
		!(suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {

		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%u suInstId:%u spInstId:%u\n", suId, suInstId, spInstId);
		ftdm_mutex_unlock(g_sngisdn_data.ccs[suId].mutex);
		return;
	}

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received STATUS CONFIRM (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
	ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
	memset(sngisdn_event, 0, sizeof(*sngisdn_event));

	sngisdn_event->event_id = SNGISDN_EVENT_STA_CFM;
	sngisdn_event->sngisdn_info = sngisdn_info;
	sngisdn_event->suId = suId;
	sngisdn_event->suInstId = suInstId;
	sngisdn_event->spInstId = spInstId;

	memcpy(&sngisdn_event->event.staEvnt, staEvnt, sizeof(*staEvnt));

 	ftdm_queue_enqueue(((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->event_queue, sngisdn_event);
	ftdm_mutex_unlock(g_sngisdn_data.ccs[suId].mutex);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_srv_ind (int16_t suId, Srv *srvEvnt, int16_t dChan, uint8_t ces)
{
	unsigned i;
	sngisdn_span_data_t	*signal_data;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	ftdm_log(FTDM_LOG_INFO, "Received SERVICE IND (dChan:%d ces:%u)\n", dChan, ces);
	
	/* Enqueue the event to each span within the dChan */
	for(i=1; i<=g_sngisdn_data.dchans[dChan].num_spans; i++) {
		signal_data = g_sngisdn_data.dchans[dChan].spans[i];
		sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
		ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
		memset(sngisdn_event, 0, sizeof(*sngisdn_event));

		sngisdn_event->event_id = SNGISDN_EVENT_SRV_IND;		
		sngisdn_event->suId = suId;
		sngisdn_event->dChan = dChan;
		sngisdn_event->ces = ces;
		sngisdn_event->signal_data = signal_data;
		
		memcpy(&sngisdn_event->event.srvEvnt, srvEvnt, sizeof(*srvEvnt));
		ftdm_queue_enqueue((signal_data)->event_queue, sngisdn_event);
	}
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}


void sngisdn_rcv_srv_cfm (int16_t suId, Srv *srvEvnt, int16_t dChan, uint8_t ces)
{
	unsigned i;
	sngisdn_span_data_t	*signal_data = NULL;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	ftdm_log(FTDM_LOG_INFO, "Received SERVICE CFM (dChan:%d ces:%u)\n", dChan, ces);

	/* Enqueue the event to each span within the dChan */
	for(i=1; i<=g_sngisdn_data.dchans[dChan].num_spans; i++) {
		signal_data = g_sngisdn_data.dchans[dChan].spans[i];
		sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
		ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
		memset(sngisdn_event, 0, sizeof(*sngisdn_event));

		sngisdn_event->event_id = SNGISDN_EVENT_SRV_CFM;		
		sngisdn_event->suId = suId;
		sngisdn_event->dChan = dChan;
		sngisdn_event->ces = ces;
		sngisdn_event->signal_data = signal_data;

		memcpy(&sngisdn_event->event.srvEvnt, srvEvnt, sizeof(*srvEvnt));
		ftdm_queue_enqueue((signal_data)->event_queue, sngisdn_event);
	}
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_rst_ind (int16_t suId, Rst *rstEvnt, int16_t dChan, uint8_t ces, uint8_t evntType)
{
	unsigned i;
	sngisdn_span_data_t	*signal_data = NULL;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	ftdm_log(FTDM_LOG_INFO, "Received RESTART IND (dChan:%d ces:%u type:%u)\n", dChan, ces, evntType);
	
	/* Enqueue the event to each span within the dChan */
	for(i=1; i<=g_sngisdn_data.dchans[dChan].num_spans; i++) {
		signal_data = g_sngisdn_data.dchans[dChan].spans[i];

		sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
		ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
		memset(sngisdn_event, 0, sizeof(*sngisdn_event));

		sngisdn_event->event_id = SNGISDN_EVENT_RST_IND;
		sngisdn_event->suId = suId;
		sngisdn_event->dChan = dChan;
		sngisdn_event->ces = ces;
		sngisdn_event->evntType = evntType;
		sngisdn_event->signal_data = signal_data;

		memcpy(&sngisdn_event->event.rstEvnt, rstEvnt, sizeof(*rstEvnt));
		ftdm_queue_enqueue(signal_data->event_queue, sngisdn_event);
	}
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}

void sngisdn_rcv_rst_cfm (int16_t suId, Rst *rstEvnt, int16_t dChan, uint8_t ces, uint8_t evntType)
{
	unsigned i;
	sngisdn_span_data_t	*signal_data;
	sngisdn_event_data_t *sngisdn_event = NULL;

	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);


	ftdm_log(FTDM_LOG_INFO, "Received RESTART CFM (dChan:%d ces:%u type:%u)\n", dChan, ces, evntType);

	/* Enqueue the event to each span within the dChan */
	for(i=1; i<=g_sngisdn_data.dchans[dChan].num_spans; i++) {
		signal_data = g_sngisdn_data.dchans[dChan].spans[i];
		sngisdn_event = ftdm_malloc(sizeof(*sngisdn_event));
		ftdm_assert(sngisdn_event != NULL, "Failed to allocate memory\n");
		memset(sngisdn_event, 0, sizeof(*sngisdn_event));

		sngisdn_event->event_id = SNGISDN_EVENT_RST_CFM;
		sngisdn_event->suId = suId;
		sngisdn_event->dChan = dChan;
		sngisdn_event->ces = ces;
		sngisdn_event->evntType = evntType;
		sngisdn_event->signal_data = signal_data;

		memcpy(&sngisdn_event->event.rstEvnt, rstEvnt, sizeof(*rstEvnt));
		ftdm_queue_enqueue((signal_data)->event_queue, sngisdn_event);
	}
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
}


void sngisdn_rcv_phy_ind(SuId suId, Reason reason)
{
	if (reason != LL1_REASON_CON_REQ_FAIL) {
		ftdm_log(FTDM_LOG_INFO, "[SNGISDN PHY] D-chan %d : %s\n", suId, DECODE_LL1_REASON(reason));
	}
    return;
} 

void sngisdn_rcv_q921_ind(BdMngmt *status)
{	
	ftdm_span_t *ftdmspan;

	sngisdn_span_data_t	*signal_data = g_sngisdn_data.dchans[status->t.usta.lnkNmb].spans[1];
	
	if (!signal_data) {
		ftdm_log(FTDM_LOG_INFO, "Received q921 status on unconfigured span (lnkNmb:%d)\n", status->t.usta.lnkNmb);
		return;
	}
	ftdmspan = signal_data->ftdm_span;

	if (!ftdmspan) {
		ftdm_log(FTDM_LOG_INFO, "Received q921 status on unconfigured span (lnkNmb:%d)\n", status->t.usta.lnkNmb);
		return;
	}

	switch (status->t.usta.alarm.category) {
		case (LCM_CATEGORY_PROTOCOL):
			ftdm_log(FTDM_LOG_DEBUG, "[SNGISDN Q921] %s: %s: %s(%d): %s(%d)\n",
						ftdmspan->name,
						DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
						DECODE_LLD_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
						DECODE_LLD_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);
			break;
		default:
			ftdm_log(FTDM_LOG_INFO, "[SNGISDN Q921] %s: %s: %s(%d): %s(%d)\n",
						ftdmspan->name,
						DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
						DECODE_LLD_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
						DECODE_LLD_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);
			
			switch (status->t.usta.alarm.event) {
				case ENTR_CONG: /* Entering Congestion */
					ftdm_log(FTDM_LOG_WARNING, "s%d: Entering Congestion\n", ftdmspan->span_id);
					ftdm_set_flag(ftdmspan, FTDM_SPAN_SUSPENDED);
					break;
				case EXIT_CONG: /* Exiting Congestion */
					ftdm_log(FTDM_LOG_WARNING, "s%d: Exiting Congestion\n", ftdmspan->span_id);
					ftdm_clear_flag(ftdmspan, FTDM_SPAN_SUSPENDED);
					break;
			}
			break;
	}
    return;
}
void sngisdn_rcv_q931_ind(InMngmt *status)
{	
#ifndef WIN32
	if (status->t.usta.alarm.cause == 287) {
		sngisdn_get_memory_info();
		return;
	}
#endif

	switch (status->t.usta.alarm.event) {
		case LCM_EVENT_UP:
		case LCM_EVENT_DOWN:
		{
			ftdm_span_t *ftdmspan;
			sngisdn_span_data_t	*signal_data = g_sngisdn_data.dchans[status->t.usta.suId].spans[1];
			if (!signal_data) {
				ftdm_log(FTDM_LOG_INFO, "Received q931 status on unconfigured span (lnkNmb:%d)\n", status->t.usta.suId);
				return;
			}
			ftdmspan = signal_data->ftdm_span;
			
			if (status->t.usta.alarm.event == LCM_EVENT_UP) {
				uint32_t chan_no = status->t.usta.evntParm[2];
				ftdm_log(FTDM_LOG_INFO, "[SNGISDN Q931] s%d: %s: %s(%d): %s(%d)\n",
						 status->t.usta.suId,
								DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
								DECODE_LCM_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
								DECODE_LCM_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);

				if (chan_no) {
					ftdm_channel_t *ftdmchan = ftdm_span_get_channel(ftdmspan, chan_no);
					if (ftdmchan) {
						sngisdn_set_chan_sig_status(ftdmchan, FTDM_SIG_STATE_UP);
						sngisdn_set_chan_avail_rate(ftdmchan, SNGISDN_AVAIL_UP);
					} else {
						ftdm_log(FTDM_LOG_CRIT, "stack alarm event on invalid channel :%d\n", chan_no);
					}
				} else {
					sngisdn_set_span_sig_status(ftdmspan, FTDM_SIG_STATE_UP);
					sngisdn_set_span_avail_rate(ftdmspan, SNGISDN_AVAIL_UP);
				}
			} else {
				ftdm_log(FTDM_LOG_WARNING, "[SNGISDN Q931] s%d: %s: %s(%d): %s(%d)\n",
						 		status->t.usta.suId,
								DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
								DECODE_LCM_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
								DECODE_LCM_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);
				
				sngisdn_set_span_sig_status(ftdmspan, FTDM_SIG_STATE_DOWN);
				sngisdn_set_span_avail_rate(ftdmspan, SNGISDN_AVAIL_PWR_SAVING);
			}
		}
		break;
		default:
			ftdm_log(FTDM_LOG_WARNING, "[SNGISDN Q931] s%d: %s: %s(%d): %s(%d)\n",
					 						status->t.usta.suId,
	  										DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
						  					DECODE_LCM_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
											DECODE_LCM_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);
	}
	
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_cc_ind(CcMngmt *status)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "RECEIVED %s\n", __FUNCTION__);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

void sngisdn_rcv_q931_trace(InMngmt *trc, Buffer *mBuf)
{
	MsgLen mlen;
	int16_t j;
	Buffer *tmp;
	Data *cptr;
	uint8_t data;
	ftdm_trace_dir_t dir;
	uint8_t tdata[1000];

	sngisdn_span_data_t	*signal_data = g_sngisdn_data.dchans[trc->t.trc.suId].spans[1];

	ftdm_assert(mBuf != NULLP, "Received a Q931 trace with no buffer");
	mlen = ((SsMsgInfo*)(mBuf->b_rptr))->len;
	
	if (trc->t.trc.evnt == TL3PKTTX) {
		dir = FTDM_TRACE_DIR_OUTGOING;
	} else {
		dir = FTDM_TRACE_DIR_INCOMING;
	}
	
	if (mlen) {
		tmp = mBuf->b_cont;
		cptr = tmp->b_rptr;
		data = *cptr++;

		for(j=0;j<mlen;j++) {
			tdata[j]= data;

			if (cptr == tmp->b_wptr) {
				tmp = tmp->b_cont;
				if (tmp) cptr = tmp->b_rptr;
			}
			data = *cptr++;
		}
		if (signal_data->raw_trace_q931 == SNGISDN_OPT_TRUE) {
			sngisdn_trace_raw_q931(signal_data, dir, tdata, mlen);
		} else {
			sngisdn_trace_interpreted_q931(signal_data, dir, tdata, mlen);
		}
	}
	return;
}


void sngisdn_rcv_q921_trace(BdMngmt *trc, Buffer *mBuf)
{
	MsgLen mlen;
	Buffer *tmp;	
	MsgLen i;
	int16_t j;
	Data *cptr;
	uint8_t data;
	ftdm_trace_dir_t dir;
	uint8_t tdata[1000];

	sngisdn_span_data_t	*signal_data = g_sngisdn_data.dchans[trc->t.trc.lnkNmb].spans[1];

	if (trc->t.trc.evnt == TL2TMR) {
		return;
	}

	if (trc->t.trc.evnt == TL2FRMTX) {
		dir = FTDM_TRACE_DIR_OUTGOING;
	} else {
		dir = FTDM_TRACE_DIR_INCOMING;
	}
	
	ftdm_assert(mBuf != NULLP, "Received a Q921 trace with no buffer");
	mlen = ((SsMsgInfo*)(mBuf->b_rptr))->len;
	if (mlen != 0) {
		tmp = mBuf->b_cont;
		cptr = tmp->b_rptr;
		data = *cptr++;
		i = 0;
		while (i < mlen) {
			j = 0;
			for(j=0;j<16;j++) {
				if (i<mlen) {
					tdata[j]= data;
				
					if (cptr == tmp->b_wptr) {
						tmp = tmp->b_cont;
						if (tmp) cptr = tmp->b_rptr;
					}
					i++;
					if (i<mlen) data = *cptr++;
				}
			}

		}
		if (signal_data->raw_trace_q921 == SNGISDN_OPT_TRUE) {
			sngisdn_trace_raw_q921(signal_data, dir, tdata, mlen);
		} else {
			sngisdn_trace_interpreted_q921(signal_data, dir, tdata, mlen);
		}		
	}
	return;
}

/* The stacks is wants to transmit a frame */
int16_t sngisdn_rcv_l1_data_req(uint16_t spId, sng_l1_frame_t *l1_frame)
{
	ftdm_status_t status;
	ftdm_wait_flag_t flags = FTDM_WRITE;
	sngisdn_span_data_t	*signal_data = g_sngisdn_data.dchans[spId].spans[1];
	ftdm_size_t length = l1_frame->len;

	ftdm_assert(signal_data, "Received Data request on unconfigured span\n");
	
	do {
		flags = FTDM_WRITE;
		status = signal_data->dchan->fio->wait(signal_data->dchan, &flags, 1000);
		if (status != FTDM_SUCCESS) {
			ftdm_log_chan_msg(signal_data->dchan, FTDM_LOG_WARNING, "transmit timed-out\n");
			return -1;
		}
		
		
		if ((flags & FTDM_WRITE)) {
#if 0
			int i;
			char string [2000];
			unsigned string_len = 0;
			for (i = 0; i < length; i++) {
				string_len += sprintf(&string[string_len], "0x%02x ", l1_frame->data[i]);
			}

			ftdm_log_chan(signal_data->dchan, FTDM_LOG_CRIT, "\nL1 TX [%s]\n", string);
#endif
			
			status = signal_data->dchan->fio->write(signal_data->dchan, l1_frame->data, (ftdm_size_t*)&length);
			if (status != FTDM_SUCCESS) {
				ftdm_log_chan_msg(signal_data->dchan, FTDM_LOG_CRIT, "Failed to transmit frame\n");
				return -1;
			}
			break;
		/* On WIN32, it is possible for poll to return without FTDM_WRITE flag set, so we try to retransmit */
#ifndef WIN32
		} else {
			ftdm_log_chan_msg(signal_data->dchan, FTDM_LOG_WARNING, "Failed to poll for d-channel\n");
			return -1;
#endif
		}
	} while(1);
	return 0;
}

int16_t sngisdn_rcv_l1_cmd_req(uint16_t spId, sng_l1_cmd_t *l1_cmd)
{
	sngisdn_span_data_t	*signal_data = g_sngisdn_data.dchans[spId].spans[1];
	ftdm_assert(signal_data, "Received Data request on unconfigured span\n");
	
	switch(l1_cmd->type) {
		case SNG_L1CMD_SET_LINK_STATUS:
			{
				ftdm_channel_hw_link_status_t status = FTDM_HW_LINK_CONNECTED;
				ftdm_channel_command(signal_data->dchan, FTDM_COMMAND_SET_LINK_STATUS, &status);
			}
			break;
		case SNG_L1CMD_GET_LINK_STATUS:
			{
				ftdm_channel_hw_link_status_t status = 0;
				ftdm_channel_command(signal_data->dchan, FTDM_COMMAND_GET_LINK_STATUS, &status);
				if (status == FTDM_HW_LINK_CONNECTED) {
					l1_cmd->cmd.status = 1;
				} else if (status == FTDM_HW_LINK_DISCONNECTED) {
					l1_cmd->cmd.status = 0;
				} else {
					ftdm_log_chan(signal_data->dchan, FTDM_LOG_CRIT, "Invalid link status reported %d\n", status);
					l1_cmd->cmd.status = 0;
				}
			}
			break;
		case SNG_L1CMD_FLUSH_STATS:
			ftdm_channel_command(signal_data->dchan, FTDM_COMMAND_FLUSH_IOSTATS, NULL);
			break;
		case SNG_L1CMD_FLUSH_BUFFERS:
			ftdm_channel_command(signal_data->dchan, FTDM_COMMAND_FLUSH_BUFFERS, NULL);
			break;
		default:
			ftdm_log_chan(signal_data->dchan, FTDM_LOG_CRIT, "Unsupported channel command:%d\n", l1_cmd->type);
			return -1;
	}
	return 0;
}

void sngisdn_rcv_sng_assert(char *message)
{
	ftdm_assert(0, message);
}

void sngisdn_rcv_sng_log(uint8_t level, char *fmt,...)
{
	char    *data;
    int     ret;
    va_list ap;

    va_start(ap, fmt);
    ret = ftdm_vasprintf(&data, fmt, ap);
    if (ret == -1) {
        return;
    }

    switch (level) {
		case SNG_LOGLEVEL_DEBUG:
			ftdm_log(FTDM_LOG_DEBUG, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_WARN:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_INFO:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_STATS:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_ERROR:
			ftdm_log(FTDM_LOG_ERROR, "sng_isdn->%s", data);
			/*ftdm_assert(0, "Got an error from stack");*/
			break;
		case SNG_LOGLEVEL_CRIT:
   			ftdm_log(FTDM_LOG_CRIT, "sng_isdn->%s", data);
			/* ftdm_assert(0, "Got an error from stack"); */
			break;
		default:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
    }
	ftdm_safe_free(data);
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
