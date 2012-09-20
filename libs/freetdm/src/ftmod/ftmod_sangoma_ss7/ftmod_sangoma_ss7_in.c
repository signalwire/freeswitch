/*
 * Copyright (c) 2009, Konrad Hammel <konrad@sangoma.com>
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

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
void sngss7_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
void sngss7_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
void sngss7_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType);
void sngss7_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
void sngss7_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
void sngss7_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt);
void sngss7_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
void sngss7_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
void sngss7_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit);
void sngss7_resm_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiResmEvnt *siResmEvnt);
void sngss7_susp_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiSuspEvnt *siSuspEvnt);
void sngss7_ssp_sta_cfm(uint32_t infId);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
void sngss7_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_CON_IND_EVENT;
	memcpy(&sngss7_event->event.siConEvnt, siConEvnt, sizeof(*siConEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_CON_CFM_EVENT;
	memcpy(&sngss7_event->event.siConEvnt, siConEvnt, sizeof(*siConEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->evntType	= evntType;
	sngss7_event->event_id	= SNGSS7_CON_STA_EVENT;
	memcpy(&sngss7_event->event.siCnStEvnt, siCnStEvnt, sizeof(*siCnStEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_REL_IND_EVENT;
	memcpy(&sngss7_event->event.siRelEvnt, siRelEvnt, sizeof(*siRelEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_REL_CFM_EVENT;
	memcpy(&sngss7_event->event.siRelEvnt, siRelEvnt, sizeof(*siRelEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_DAT_IND_EVENT;
	memcpy(&sngss7_event->event.siInfoEvnt, siInfoEvnt, sizeof(*siInfoEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->evntType	= evntType;
	sngss7_event->event_id	= SNGSS7_FAC_IND_EVENT;
	memcpy(&sngss7_event->event.siFacEvnt, siFacEvnt, sizeof(*siFacEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->evntType	= evntType;
	sngss7_event->event_id	= SNGSS7_FAC_CFM_EVENT;
	memcpy(&sngss7_event->event.siFacEvnt, siFacEvnt, sizeof(*siFacEvnt));

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
}

/******************************************************************************/
void sngss7_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_UMSG_IND_EVENT;

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);

}
/* GENERAL STATUS *************************************************************/
void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;
	uint32_t			intfId;
	int 				x;



	/* check if the eventType is a pause/resume */
	switch (evntType) {
	/**************************************************************************/
	case (SIT_STA_PAUSEIND):
	case (SIT_STA_RESUMEIND):
		/* the circuit may or may not be on the local system so we have to find 
		 * circuit with the same intfId.  The circuit specified might also be
		 * a non-voice cic so we also need to find the first voice cic on this 
		 * system with the same intfId.
		 */
		intfId = g_ftdm_sngss7_data.cfg.isupCkt[circuit].infId;

		if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
			SS7_DEBUG("Rx %s on circuit that is not a voice CIC (%d) finding a new circuit\n", 
						DECODE_LCC_EVENT(evntType),
						g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic);
		}

		x = (g_ftdm_sngss7_data.cfg.procId * MAX_CIC_MAP_LENGTH) + 1;
		while ((g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) &&
			   (g_ftdm_sngss7_data.cfg.isupCkt[x].id < ((g_ftdm_sngss7_data.cfg.procId + 1) * MAX_CIC_MAP_LENGTH))) {
			/**********************************************************************/
			/* confirm this is a voice channel and not a gap/sig (no ftdmchan there) */
			if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
				/* compare the intfIds */
				if (g_ftdm_sngss7_data.cfg.isupCkt[x].infId == intfId) {
					/* we have a match, setup the pointers to the correct values */
					circuit = x;

					/* confirm that the circuit is active on our side otherwise move to the next circuit */
					if (!sngss7_test_flag(&g_ftdm_sngss7_data.cfg.isupCkt[circuit], SNGSS7_ACTIVE)) {
						SS7_DEBUG("[CIC:%d]Rx %s but circuit is not active yet, skipping!\n",
									g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic,
									DECODE_LCC_EVENT(evntType));
						x++;
						continue;
					}

					if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
						SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
						SS7_FUNC_TRACE_EXIT(__FUNCTION__);
						return;
					}

					/* bounce out of the loop */
					break;
				}
			}

			x++;
			/**********************************************************************/
		}

		/* check if we found any circuits that are on the intfId, drop the message
		 * if none are found */
		if (!ftdmchan) {
			SS7_FUNC_TRACE_EXIT(__FUNCTION__);
			return;
		}

		break;
	/**************************************************************************/
	default:
		if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
			ftdm_log(FTDM_LOG_DEBUG, "Rx %s on circuit that is not a voice CIC (%d) (circuit:%d)\n",
						DECODE_LCC_EVENT(evntType), g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic, circuit);
			SS7_FUNC_TRACE_EXIT(__FUNCTION__);
			return;
		}

		/* get the ftdmchan and ss7_chan_data from the circuit */
		if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
			SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
			SS7_FUNC_TRACE_EXIT(__FUNCTION__);
			return;
		}
		break;
	/**************************************************************************/
	} /* switch (evntType) */

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->globalFlg	= globalFlg;
	sngss7_event->evntType	= evntType;
	sngss7_event->event_id	= SNGSS7_STA_IND_EVENT;
	if (siStaEvnt != NULL) {
		memcpy(&sngss7_event->event.siStaEvnt, siStaEvnt, sizeof(*siStaEvnt));
	}

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);
}

/******************************************************************************/
void sngss7_susp_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiSuspEvnt *siSuspEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_SUSP_IND_EVENT;
	if (siSuspEvnt != NULL) {
		memcpy(&sngss7_event->event.siSuspEvnt, siSuspEvnt, sizeof(*siSuspEvnt));
	}

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);

}

/******************************************************************************/
void sngss7_resm_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiResmEvnt *siResmEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].type != SNG_CKT_VOICE) {
		SS7_ERROR("Rx sig event on circuit that is not a voice CIC (%d)\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_RESM_IND_EVENT;
	if (siResmEvnt != NULL) {
		memcpy(&sngss7_event->event.siResmEvnt, siResmEvnt, sizeof(*siResmEvnt));
	}

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);

}

/******************************************************************************/
void sngss7_ssp_sta_cfm(uint32_t infId)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);
#if 0
	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* initalize the sngss7_event */
	sngss7_event = ftdm_malloc(sizeof(*sngss7_event));
	if (sngss7_event == NULL) {
		SS7_ERROR("Failed to allocate memory for sngss7_event!\n");
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}
	memset(sngss7_event, 0x0, sizeof(*sngss7_event));

	/* fill in the sngss7_event struct */
	sngss7_event->spInstId	= spInstId;
	sngss7_event->suInstId	= suInstId;
	sngss7_event->circuit	= circuit;
	sngss7_event->event_id	= SNGSS7_RESM_IND_EVENT;
	if (siSuspEvnt != NULL) {
		memcpy(&sngss7_event->event.siResmEvnt, siResmEvnt, sizeof(*siResmEvnt));
	}

	/* enqueue this event */
	ftdm_queue_enqueue(((sngss7_span_data_t*)sngss7_info->ftdmchan->span->signal_data)->event_queue, sngss7_event);
#endif
	SS7_FUNC_TRACE_EXIT(__FUNCTION__);

}
/******************************************************************************/
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

