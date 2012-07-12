/*
 * Copyright (c) 2011, Sangoma Technologies
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
 */

#include "ftmod_sangoma_isdn.h"

#define TRANSFER_FUNC(name) ftdm_status_t (name)(ftdm_channel_t *ftdmchan, sngisdn_transfer_type_t type, char* target)

#define SNGISDN_ATT_TRANSFER_RESPONSE_CP_DROP_OFF		"**1"
#define SNGISDN_ATT_TRANSFER_RESPONSE_LIMITS_EXCEEDED	"**5"
#define SNGISDN_ATT_TRANSFER_RESPONSE_OK				"**6"
#define SNGISDN_ATT_TRANSFER_RESPONSE_INVALID_NUM		"**7"
#define SNGISDN_ATT_TRANSFER_RESPONSE_INVALID_COMMAND	"**8"


void att_courtesy_transfer_complete(sngisdn_chan_data_t *sngisdn_info, ftdm_transfer_response_t response);
void att_courtesy_transfer_timeout(void* p_sngisdn_info);
static ftdm_status_t att_courtesy_vru(ftdm_channel_t *ftdmchan, sngisdn_transfer_type_t type, char* target);

typedef struct transfer_interfaces {
	const char *name;
	sngisdn_transfer_type_t type;
	TRANSFER_FUNC(*func);
}transfer_interface_t;

static transfer_interface_t transfer_interfaces[] = {
	/* AT&T TR-50075 Courtesy Transfer - VRU -- No Data (Section 4.3) */
	{ "ATT_COURTESY_TRANSFER_V", SNGISDN_TRANSFER_ATT_COURTESY_VRU, att_courtesy_vru},
	/* AT&T TR-50075 Courtesy Transfer - VRU --Data (Section 4.4) */
	{ "ATT_COURTESY_TRANSFER_V_DATA", SNGISDN_TRANSFER_ATT_COURTESY_VRU_DATA, att_courtesy_vru},
};

void att_courtesy_transfer_complete(sngisdn_chan_data_t *sngisdn_info, ftdm_transfer_response_t response)
{
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_RX_DISABLED);
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_DTMF_DETECT, NULL);

	sngisdn_info->transfer_data.type = SNGISDN_TRANSFER_NONE;
	sngisdn_info->transfer_data.response = response;

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Transfer Complete:%s\n", ftdm_transfer_response2str(sngisdn_info->transfer_data.response));
	sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_TRANSFER_COMPLETED);

	ftdm_channel_command(ftdmchan, FTDM_COMMAND_FLUSH_RX_BUFFERS, NULL);
	return;
}

void att_courtesy_transfer_timeout(void* p_sngisdn_info)
{
	sngisdn_chan_data_t  *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	ftdm_mutex_lock(ftdmchan->mutex);
	if (sngisdn_info->transfer_data.type == SNGISDN_TRANSFER_NONE) {
		/* Call was already cleared */
		ftdm_mutex_unlock(ftdmchan->mutex);
		return;
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "AT&T Courtesy Transfer timeout (%d)\n", signal_data->transfer_timeout);
	att_courtesy_transfer_complete(sngisdn_info, FTDM_TRANSFER_RESPONSE_TIMEOUT);
	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

static ftdm_status_t att_courtesy_vru(ftdm_channel_t *ftdmchan, sngisdn_transfer_type_t type, char* args)
{
	char dtmf_digits[64];
	ftdm_status_t status = FTDM_FAIL;
	sngisdn_chan_data_t  *sngisdn_info = ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	char *p = args;
	uint8_t forced_answer = 0;

	switch (signal_data->switchtype) {
		case SNGISDN_SWITCH_5ESS:
		case SNGISDN_SWITCH_4ESS:
			break;
		default:
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "AT&T Courtesy Transfer not supported for switchtype\n");
			return FTDM_FAIL;
	}

	while (!ftdm_strlen_zero(p)) {
		if (!isdigit(*p) && *p != 'w' && *p != 'W') {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Cannot transfer to non-numeric number:%s\n", args);
			return FTDM_FAIL;
		}
		p++;
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Performing AT&T Courtesy Transfer-VRU%s to %s\n", (type == SNGISDN_TRANSFER_ATT_COURTESY_VRU_DATA) ?"--data" : "", args);
	sprintf(dtmf_digits, "*8w%s", args);
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending digits %s\n", dtmf_digits);

	switch (ftdmchan->last_state) {
		case FTDM_CHANNEL_STATE_PROCEED:
		case FTDM_CHANNEL_STATE_PROGRESS:
		case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			/* Call has to be in answered state - so send a CONNECT message if we did not answer this call yet */
			forced_answer++;
			sngisdn_snd_connect(ftdmchan);
			/* fall-through */
		case FTDM_CHANNEL_STATE_UP:
			memset(&sngisdn_info->transfer_data.tdata.att_courtesy_vru.dtmf_digits, 0, sizeof(sngisdn_info->transfer_data.tdata.att_courtesy_vru.dtmf_digits));
			sngisdn_info->transfer_data.type = type;

			/* We will be polling the channel for IO so that we can receive the DTMF events,
			 * Disable user RX otherwise it is a race between who calls channel_read */
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_RX_DISABLED);

			ftdm_channel_command(ftdmchan, FTDM_COMMAND_ENABLE_DTMF_DETECT, NULL);
			ftdm_channel_command(ftdmchan, FTDM_COMMAND_SEND_DTMF, dtmf_digits);

			if (type == SNGISDN_TRANSFER_ATT_COURTESY_VRU_DATA) {
				/* We need to save transfer data, so we can send it in the disconnect msg */
				const char *val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "transfer_data");
				if (ftdm_strlen_zero(val)) {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Cannot perform data transfer because transfer_data variable is not set\n");
					goto done;
				}
				if (strlen(val) > COURTESY_TRANSFER_MAX_DATA_SIZE) {
					ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Data exceeds max size (len:%"FTDM_SIZE_FMT" max:%d), cannot perform transfer\n", strlen(val), COURTESY_TRANSFER_MAX_DATA_SIZE);
					goto done;
				}
				memcpy(sngisdn_info->transfer_data.tdata.att_courtesy_vru.data, val, strlen(val));
			}

			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_UP);
			if (forced_answer) {
				/* Notify the user that we answered the call */
				sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_UP);
			}
			if (signal_data->transfer_timeout) {
				ftdm_sched_timer(((sngisdn_span_data_t*)ftdmchan->span->signal_data)->sched, "courtesy_transfer_timeout", signal_data->transfer_timeout, att_courtesy_transfer_timeout, (void*) sngisdn_info, &sngisdn_info->timers[SNGISDN_CHAN_TIMER_ATT_TRANSFER]);
			}

			status = FTDM_SUCCESS;
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Cannot perform transfer in state %s\n", ftdm_channel_state2str(ftdmchan->state));
			break;

	}
done:
	return status;
}


ftdm_status_t sngisdn_transfer(ftdm_channel_t *ftdmchan)
{
	const char* args;
	char *p;
	char *type = NULL;
	char *target = NULL;
	ftdm_status_t status = FTDM_FAIL;
	sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data;
	unsigned i;

	args = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "transfer_arg");
	if (ftdm_strlen_zero(args)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Cannot perform transfer because call_transfer_arg variable is not set\n");
		goto done;
	}

	type = ftdm_strdup(args);
	if ((p = strchr(type, '/'))) {
		target = ftdm_strdup(p+1);
		*p = '\0';
	}

	if (ftdm_strlen_zero(type) || ftdm_strlen_zero(target)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Invalid parameters for transfer %s, expected <type>/<target>\n", args);
		goto done;
	}

	if (sngisdn_info->transfer_data.type != SNGISDN_TRANSFER_NONE) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Cannot perform transfer because an existing transfer transfer is pending (%s)\n", sngisdn_transfer_type2str(sngisdn_info->transfer_data.type));
		goto done;
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Transfer requested type:%s target:%s\n", type, target);
	for (i = 0; i < ftdm_array_len(transfer_interfaces); i++ ) {
		if (!strcasecmp(transfer_interfaces[i].name, type)) {
			/* Depending on the transfer type, the transfer function may change the
			 * channel state to UP, or last_state, but the transfer function will always result in
			 * an immediate state change if FTDM_SUCCESS is returned */

			status = transfer_interfaces[i].func(ftdmchan, transfer_interfaces[i].type, target);
			goto done;
		}
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Invalid transfer type:%s\n", type);

done:
	if (status != FTDM_SUCCESS) {
		ftdm_set_state(ftdmchan, ftdmchan->last_state);
	}

	ftdm_safe_free(type);
	ftdm_safe_free(target);
	return status;
}

ftdm_status_t sngisdn_att_transfer_process_dtmf(ftdm_channel_t *ftdmchan, const char* dtmf)
{
	ftdm_status_t status = FTDM_SUCCESS;
	sngisdn_chan_data_t  *sngisdn_info = ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	char *dtmf_digits = sngisdn_info->transfer_data.tdata.att_courtesy_vru.dtmf_digits;
	ftdm_size_t dtmf_digits_len = strlen(dtmf_digits);

	dtmf_digits_len += sprintf(&dtmf_digits[dtmf_digits_len], "%s", dtmf);
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Transfer response digits:%s\n", dtmf_digits);
	if (dtmf_digits_len == 3) {
		if (!strcmp(dtmf_digits, SNGISDN_ATT_TRANSFER_RESPONSE_CP_DROP_OFF)) {
			sngisdn_info->transfer_data.response = FTDM_TRANSFER_RESPONSE_CP_DROP_OFF;
		} else if (!strcmp(dtmf_digits, SNGISDN_ATT_TRANSFER_RESPONSE_LIMITS_EXCEEDED)) {
			sngisdn_info->transfer_data.response = FTDM_TRANSFER_RESPONSE_LIMITS_EXCEEDED;
		} else if (!strcmp(dtmf_digits, SNGISDN_ATT_TRANSFER_RESPONSE_OK)) {
			sngisdn_info->transfer_data.response = FTDM_TRANSFER_RESPONSE_OK;
		} else if (!strcmp(dtmf_digits, SNGISDN_ATT_TRANSFER_RESPONSE_INVALID_NUM)) {
			sngisdn_info->transfer_data.response = FTDM_TRANSFER_RESPONSE_INVALID_NUM;
		} else if (!strcmp(dtmf_digits, SNGISDN_ATT_TRANSFER_RESPONSE_INVALID_COMMAND)) {
			sngisdn_info->transfer_data.response = FTDM_TRANSFER_RESPONSE_INVALID_COMMAND;
		} else {
			sngisdn_info->transfer_data.response = FTDM_TRANSFER_RESPONSE_INVALID;
		}
		if (signal_data->transfer_timeout) {
			ftdm_sched_cancel_timer(signal_data->sched, sngisdn_info->timers[SNGISDN_CHAN_TIMER_ATT_TRANSFER]);
		}

		if (sngisdn_info->transfer_data.response == FTDM_TRANSFER_RESPONSE_OK &&
			sngisdn_info->transfer_data.type == SNGISDN_TRANSFER_ATT_COURTESY_VRU_DATA) {
			sngisdn_set_flag(sngisdn_info, FLAG_SEND_DISC);
			ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_CLEARING;
			sngisdn_snd_disconnect(ftdmchan);
		}
		/* Network side will send disconnect in case of NO-DATA Transfer */
		att_courtesy_transfer_complete(sngisdn_info, sngisdn_info->transfer_data.response);
	}

	if (signal_data->att_remove_dtmf != SNGISDN_OPT_FALSE) {
		/* If we return FTDM_BREAK, dtmf event is not queue'ed to user */
		status = FTDM_BREAK;
	}
	return status;
}
