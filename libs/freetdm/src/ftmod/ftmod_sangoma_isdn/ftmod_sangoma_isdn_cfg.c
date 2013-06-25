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

static ftdm_status_t parse_timer(const char* val, int32_t *target);
static ftdm_status_t parse_switchtype(const char* switch_name, ftdm_span_t *span);
static ftdm_status_t parse_signalling(const char* signalling, ftdm_span_t *span);
static ftdm_status_t parse_trunkgroup(const char *_trunkgroup);
static ftdm_status_t add_local_number(const char* val, ftdm_span_t *span);
static ftdm_status_t parse_yesno(const char* var, const char* val, uint8_t *target);
static ftdm_status_t set_switchtype_defaults(ftdm_span_t *span);

extern ftdm_sngisdn_data_t	g_sngisdn_data;


static ftdm_status_t parse_timer(const char* val, int32_t *target)
{
	*target = atoi(val);
	if (*target < 0) {
		*target = 0;
	}
	return FTDM_SUCCESS;
}

static ftdm_status_t parse_yesno(const char* var, const char* val, uint8_t *target)
{
	if (ftdm_true(val)) {
		*target = SNGISDN_OPT_TRUE; 
	} else {
		*target = SNGISDN_OPT_FALSE;
	}
	return FTDM_SUCCESS;
}

static ftdm_status_t add_local_number(const char* val, ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;

	if (signal_data->num_local_numbers >= SNGISDN_NUM_LOCAL_NUMBERS) {
		ftdm_log(FTDM_LOG_ERROR, "%s: Maximum number of local-numbers exceeded (max:%d)\n", span->name, SNGISDN_NUM_LOCAL_NUMBERS);
		return FTDM_FAIL;
	}
	
	signal_data->local_numbers[signal_data->num_local_numbers++] = ftdm_strdup(val);
	return FTDM_SUCCESS;
}

static ftdm_status_t parse_switchtype(const char* switch_name, ftdm_span_t *span)
{
	unsigned i;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;	
	//sngisdn_dchan_data_t *dchan_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;
	
	switch(span->trunk_type) {
		case FTDM_TRUNK_T1:
			if (!strcasecmp(switch_name, "ni2") ||
				!strcasecmp(switch_name, "national")) {
				signal_data->switchtype = SNGISDN_SWITCH_NI2;
			} else if (!strcasecmp(switch_name, "5ess")) {
				signal_data->switchtype = SNGISDN_SWITCH_5ESS;
			} else if (!strcasecmp(switch_name, "4ess")) {
				signal_data->switchtype = SNGISDN_SWITCH_4ESS;
			} else if (!strcasecmp(switch_name, "dms100")) {
				signal_data->switchtype = SNGISDN_SWITCH_DMS100;
			} else if (!strcasecmp(switch_name, "qsig")) {
				signal_data->switchtype = SNGISDN_SWITCH_QSIG;
			} else {
				ftdm_log(FTDM_LOG_ERROR, "%s:Unsupported switchtype %s for trunktype:%s\n", span->name, switch_name, ftdm_trunk_type2str(span->trunk_type));
				return FTDM_FAIL;
			}
			break;
		case FTDM_TRUNK_E1:
			if (!strcasecmp(switch_name, "euroisdn") ||
				!strcasecmp(switch_name, "etsi")) {
				signal_data->switchtype = SNGISDN_SWITCH_EUROISDN;
			} else if (!strcasecmp(switch_name, "qsig")) {
				signal_data->switchtype = SNGISDN_SWITCH_QSIG;
			} else {
				ftdm_log(FTDM_LOG_ERROR, "%s:Unsupported switchtype %s for trunktype:%s\n", span->name, switch_name, ftdm_trunk_type2str(span->trunk_type));
				return FTDM_FAIL;
			}
			break;
		case FTDM_TRUNK_BRI:
		case FTDM_TRUNK_BRI_PTMP:
			if (!strcasecmp(switch_name, "euroisdn") ||
				!strcasecmp(switch_name, "etsi")) {
				signal_data->switchtype = SNGISDN_SWITCH_EUROISDN;
			} else if (!strcasecmp(switch_name, "insnet") ||
						!strcasecmp(switch_name, "ntt")) {
				signal_data->switchtype = SNGISDN_SWITCH_INSNET;
			} else {
				ftdm_log(FTDM_LOG_ERROR, "%s:Unsupported switchtype %s for trunktype:%s\n", span->name, switch_name, ftdm_trunk_type2str(span->trunk_type));
				return FTDM_FAIL;
			}
			ftdm_set_flag(span, FTDM_SPAN_USE_AV_RATE);
			ftdm_set_flag(span, FTDM_SPAN_PWR_SAVING);
			 /* can be > 1 for some BRI variants */
			break;
		default:
			ftdm_log(FTDM_LOG_ERROR, "%s:Unsupported trunktype:%s\n", span->name, ftdm_trunk_type2str(span->trunk_type));
			return FTDM_FAIL;
	}

	/* see if we have profile with this switch_type already */
	for (i = 1; i <= g_sngisdn_data.num_cc; i++) {
		if (g_sngisdn_data.ccs[i].switchtype == signal_data->switchtype &&
			g_sngisdn_data.ccs[i].trunktype == span->trunk_type) {
			break;
		}
	}

	/* need to create a new switch_type */
	if (i > g_sngisdn_data.num_cc) {
		g_sngisdn_data.num_cc++;
		g_sngisdn_data.ccs[i].switchtype = signal_data->switchtype;
		g_sngisdn_data.ccs[i].trunktype = span->trunk_type;
		ftdm_log(FTDM_LOG_DEBUG, "%s: New switchtype:%s  cc_id:%u\n", span->name, switch_name, i);
	}

	/* add this span to its ent_cc */
	signal_data->cc_id = i;

	g_sngisdn_data.spans[signal_data->link_id] = signal_data;
	
	ftdm_log(FTDM_LOG_DEBUG, "%s: cc_id:%d link_id:%d\n", span->name, signal_data->cc_id, signal_data->link_id);
	
	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
		int32_t chan_id;
		ftdm_channel_t *ftdmchan = (ftdm_channel_t*)ftdm_iterator_current(curr);
		if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
			/* set the d-channel */
			signal_data->dchan = ftdmchan;
		} else {
			/* Add the channels to the span */
			chan_id = ftdmchan->physical_chan_id;
			signal_data->channels[chan_id] = (sngisdn_chan_data_t*)ftdmchan->call_data;
			signal_data->num_chans++;
		}
	}
	ftdm_iterator_free(chaniter);
	return FTDM_SUCCESS;
}

static ftdm_status_t parse_signalling(const char *signalling, ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;
	if (!strcasecmp(signalling, "net") ||
		!strcasecmp(signalling, "pri_net")||
		!strcasecmp(signalling, "bri_net")) {

		signal_data->signalling = SNGISDN_SIGNALING_NET;
	} else if (!strcasecmp(signalling, "cpe") ||
		!strcasecmp(signalling, "pri_cpe")||
		!strcasecmp(signalling, "bri_cpe")) {

		signal_data->signalling = SNGISDN_SIGNALING_CPE;
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Unsupported signalling/interface %s\n", signalling);
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

static ftdm_status_t parse_spanmap(const char *_spanmap, ftdm_span_t *span)
{
	int i;
	char *p, *name, *spanmap;
	uint8_t logical_span_id = 0;
	ftdm_status_t ret = FTDM_SUCCESS;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;

	spanmap = ftdm_strdup(_spanmap);

	p = name = NULL;

	i = 0;
	for (p = strtok(spanmap, ","); p; p = strtok(NULL, ",")) {
		while (*p == ' ') {
			p++;
		}
		switch(i++) {
			case 0:
				name = ftdm_strdup(p);
				break;
			case 1:
				logical_span_id = atoi(p);
				break;
		}
	}

	if (!name) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid spanmap syntax %s\n", _spanmap);
		ret = FTDM_FAIL;
		goto done;
	}

	for (i = 0; i < g_sngisdn_data.num_nfas; i++) {
		if (!ftdm_strlen_zero(g_sngisdn_data.nfass[i].name) &&
			!strcasecmp(g_sngisdn_data.nfass[i].name, name)) {

			signal_data->nfas.trunk = &g_sngisdn_data.nfass[i];
			break;
		}
	}

	if (!signal_data->nfas.trunk) {
		ftdm_log(FTDM_LOG_ERROR, "Could not find trunkgroup with name %s\n", name);
		ret = FTDM_FAIL;
		goto done;
	}

	if (signal_data->nfas.trunk->spans[logical_span_id]) {
		ftdm_log(FTDM_LOG_ERROR, "trunkgroup:%s already had a span with logical span id:%d\n", name, logical_span_id);
	} else {
		signal_data->nfas.trunk->spans[logical_span_id] = signal_data;
		signal_data->nfas.interface_id = logical_span_id;
	}

	if (!strcasecmp(signal_data->ftdm_span->name, signal_data->nfas.trunk->dchan_span_name)) {

		signal_data->nfas.sigchan = SNGISDN_NFAS_DCHAN_PRIMARY;
		signal_data->nfas.trunk->dchan = signal_data;
	}

	if (!strcasecmp(signal_data->ftdm_span->name, signal_data->nfas.trunk->backup_span_name)) {

		signal_data->nfas.sigchan = SNGISDN_NFAS_DCHAN_BACKUP;
		signal_data->nfas.trunk->backup = signal_data;
	}

done:
	ftdm_safe_free(spanmap);
	ftdm_safe_free(name);
	return ret;
}

static ftdm_status_t parse_trunkgroup(const char *_trunkgroup)
{
	int i;
	char *p, *name, *dchan_span, *backup_span, *trunkgroup;
	uint8_t num_spans = 0;
	ftdm_status_t ret = FTDM_SUCCESS;

	trunkgroup =  ftdm_strdup(_trunkgroup);

	p = name = dchan_span = backup_span = NULL;

	/* format: name, num_chans, dchan_span, [backup_span] */

	i = 0;
	for (p = strtok(trunkgroup, ","); p; p = strtok(NULL, ",")) {
		while (*p == ' ') {
			p++;
		}
		switch(i++) {
			case 0:
				name = ftdm_strdup(p);
				break;
			case 1:
				num_spans = atoi(p);
				break;
			case 2:
				dchan_span = ftdm_strdup(p);
				break;
			case 3:
				backup_span = ftdm_strdup(p);
		}
	}

	if (!name || !dchan_span || num_spans <= 0) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid parameters for trunkgroup:%s\n", _trunkgroup);
		ret = FTDM_FAIL;
		goto done;
	}

	for (i = 0; i < g_sngisdn_data.num_nfas; i++) {
		if (!ftdm_strlen_zero(g_sngisdn_data.nfass[i].name) &&
		    !strcasecmp(g_sngisdn_data.nfass[i].name, name)) {

			/* We already configured this trunkgroup */
			goto done;
		}
	}

	/* Trunk group was not found, need to configure it */
	strncpy(g_sngisdn_data.nfass[i].name, name, sizeof(g_sngisdn_data.nfass[i].name));
	g_sngisdn_data.nfass[i].num_spans = num_spans;
	strncpy(g_sngisdn_data.nfass[i].dchan_span_name, dchan_span, sizeof(g_sngisdn_data.nfass[i].dchan_span_name));

	if (backup_span) {
		strncpy(g_sngisdn_data.nfass[i].backup_span_name, backup_span, sizeof(g_sngisdn_data.nfass[i].backup_span_name));
	}
	

	g_sngisdn_data.num_nfas++;
done:
	ftdm_safe_free(trunkgroup);
	ftdm_safe_free(name);
	ftdm_safe_free(dchan_span);
	ftdm_safe_free(backup_span);
	return ret;
}

static ftdm_status_t parse_early_media(const char* opt, ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;
	if (!strcasecmp(opt, "on-proceed")) {
		signal_data->early_media_flags |= SNGISDN_EARLY_MEDIA_ON_PROCEED;
	} else if (!strcasecmp(opt, "on-progress")) {
		signal_data->early_media_flags |= SNGISDN_EARLY_MEDIA_ON_PROGRESS;
	} else if (!strcasecmp(opt, "on-alert")) {
		signal_data->early_media_flags |= SNGISDN_EARLY_MEDIA_ON_ALERT;
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Unsupported early-media option %s\n", opt);
		return FTDM_FAIL;
	}
	ftdm_log(FTDM_LOG_DEBUG, "Early media opt:0x%x\n", signal_data->early_media_flags);
	return FTDM_SUCCESS;
}


static ftdm_status_t set_switchtype_defaults(ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;
	switch(signal_data->switchtype) {
		case SNGISDN_SWITCH_NI2:
		case SNGISDN_SWITCH_5ESS:
		case SNGISDN_SWITCH_4ESS:
		case SNGISDN_SWITCH_DMS100:
			if (span->default_caller_data.dnis.plan >= FTDM_NPI_INVALID) {
				ftdm_set_npi("isdn", &span->default_caller_data.dnis.plan);
			}
			if (span->default_caller_data.dnis.type >= FTDM_TON_INVALID) {
				ftdm_set_ton("national", &span->default_caller_data.dnis.type);
			}
			if (span->default_caller_data.cid_num.plan >= FTDM_NPI_INVALID) {
				ftdm_set_npi("isdn", &span->default_caller_data.cid_num.plan);
			}
			if (span->default_caller_data.cid_num.type >= FTDM_TON_INVALID) {
				ftdm_set_ton("national", &span->default_caller_data.cid_num.type);
			}
			if (span->default_caller_data.rdnis.plan >= FTDM_NPI_INVALID) {
				ftdm_set_npi("isdn", &span->default_caller_data.rdnis.plan);
			}
			if (span->default_caller_data.rdnis.type >= FTDM_TON_INVALID) {
				ftdm_set_ton("national", &span->default_caller_data.rdnis.type);
			}
			break;
		case SNGISDN_SWITCH_EUROISDN:
		case SNGISDN_SWITCH_QSIG:
		case SNGISDN_SWITCH_INSNET:
			if (span->default_caller_data.dnis.plan >= FTDM_NPI_INVALID) {
				ftdm_set_npi("unknown", &span->default_caller_data.dnis.plan);
			}
			if (span->default_caller_data.dnis.type >= FTDM_TON_INVALID) {
				ftdm_set_ton("unknown", &span->default_caller_data.dnis.type);
			}
			if (span->default_caller_data.cid_num.plan >= FTDM_NPI_INVALID) {
				ftdm_set_npi("unknown", &span->default_caller_data.cid_num.plan);
			}
			if (span->default_caller_data.cid_num.type >= FTDM_TON_INVALID) {
				ftdm_set_ton("unknown", &span->default_caller_data.cid_num.type);
			}
			if (span->default_caller_data.rdnis.plan >= FTDM_NPI_INVALID) {
				ftdm_set_npi("unknown", &span->default_caller_data.rdnis.plan);
			}
			if (span->default_caller_data.rdnis.type >= FTDM_TON_INVALID) {
				ftdm_set_ton("unknown", &span->default_caller_data.rdnis.type);
			}
			break;
		case SNGISDN_SWITCH_INVALID:
		default:
			ftdm_log(FTDM_LOG_ERROR, "Unsupported switchtype[%d]\n", signal_data->switchtype);
			return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}


ftdm_status_t ftmod_isdn_parse_cfg(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span)
{
	unsigned paramindex;
	const char *var, *val;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;
	/* Set defaults here */
	signal_data->tei = 0;
	signal_data->min_digits = 8;
	signal_data->overlap_dial = SNGISDN_OPT_DEFAULT;
	signal_data->setup_arb = SNGISDN_OPT_DEFAULT;
	signal_data->facility_ie_decode = SNGISDN_OPT_DEFAULT;
	signal_data->ignore_cause_value = SNGISDN_OPT_DEFAULT;
	signal_data->timer_t3 = 8;
	signal_data->restart_opt = SNGISDN_OPT_DEFAULT;
	signal_data->link_id = span->span_id;
	signal_data->transfer_timeout = 20000;
	signal_data->att_remove_dtmf = SNGISDN_OPT_DEFAULT;
	signal_data->force_sending_complete = SNGISDN_OPT_DEFAULT;

	signal_data->cid_name_method = SNGISDN_CID_NAME_AUTO;
	signal_data->send_cid_name = SNGISDN_OPT_DEFAULT;
	signal_data->send_connect_ack = SNGISDN_OPT_DEFAULT;
	
	span->default_caller_data.dnis.plan = FTDM_NPI_INVALID;
	span->default_caller_data.dnis.type = FTDM_TON_INVALID;
	span->default_caller_data.cid_num.plan = FTDM_NPI_INVALID;
	span->default_caller_data.cid_num.type = FTDM_TON_INVALID;
	span->default_caller_data.rdnis.plan = FTDM_NPI_INVALID;
	span->default_caller_data.rdnis.type = FTDM_TON_INVALID;

	span->default_caller_data.bearer_capability = IN_ITC_SPEECH;
	/* Cannot set default bearer_layer1 yet, as we do not know the switchtype */
	span->default_caller_data.bearer_layer1 = FTDM_INVALID_INT_PARM;

	/* Find out if NFAS is enabled first */
	for (paramindex = 0; ftdm_parameters[paramindex].var; paramindex++) {
		ftdm_log(FTDM_LOG_DEBUG, "Sangoma ISDN key=value, %s=%s\n", ftdm_parameters[paramindex].var, ftdm_parameters[paramindex].val);
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;

		if (!strcasecmp(var, "trunkgroup")) {
			if (parse_trunkgroup(val) != FTDM_SUCCESS) {
				return FTDM_FAIL;
			}
		}
	}

	for (paramindex = 0; ftdm_parameters[paramindex].var; paramindex++) {
		ftdm_log(FTDM_LOG_DEBUG, "Sangoma ISDN key=value, %s=%s\n", ftdm_parameters[paramindex].var, ftdm_parameters[paramindex].val);
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;
		
		if (!strcasecmp(var, "switchtype")) {
			if (parse_switchtype(val, span) != FTDM_SUCCESS) {
				return FTDM_FAIL;
			}
			if (set_switchtype_defaults(span) != FTDM_SUCCESS) {
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "signalling") ||
				   !strcasecmp(var, "interface")) {
			if (parse_signalling(val, span) != FTDM_SUCCESS) {
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "spanmap")) {
			if (parse_spanmap(val, span) != FTDM_SUCCESS) {
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "tei")) {
			uint8_t tei = atoi(val);
			if (tei > 127) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid TEI %d, valid values are (0-127)", tei);
				return FTDM_FAIL;
			}
			signal_data->tei = tei;
		} else if (!strcasecmp(var, "overlap")) {
			if (!strcasecmp(val, "yes")) {
				signal_data->overlap_dial = SNGISDN_OPT_TRUE;
			} else if (!strcasecmp(val, "no")) {
				signal_data->overlap_dial = SNGISDN_OPT_FALSE;
			} else {
				ftdm_log(FTDM_LOG_ERROR, "Invalid value for parameter:%s:%s\n", var, val);
			}
		} else if (!strcasecmp(var, "setup-arbitration")) {
			parse_yesno(var, val, &signal_data->setup_arb);
		} else if (!strcasecmp(var, "facility")) {
			parse_yesno(var, val, &signal_data->facility);
		} else if (!strcasecmp(var, "min-digits") ||
					!strcasecmp(var, "min_digits")) {
			signal_data->min_digits = atoi(val);
		} else if (!strcasecmp(var, "outbound-called-ton")) {
			ftdm_set_ton(val, &span->default_caller_data.dnis.type);
		} else if (!strcasecmp(var, "outbound-called-npi")) {
			ftdm_set_npi(val, &span->default_caller_data.dnis.plan);
		} else if (!strcasecmp(var, "outbound-calling-ton")) {
			ftdm_set_ton(val, &span->default_caller_data.cid_num.type);
		} else if (!strcasecmp(var, "outbound-calling-npi")) {
			ftdm_set_npi(val, &span->default_caller_data.cid_num.plan);
		} else if (!strcasecmp(var, "outbound-rdnis-ton")) {
			ftdm_set_ton(val, &span->default_caller_data.rdnis.type);
		} else if (!strcasecmp(var, "outbound-rdnis-npi")) {
			ftdm_set_npi(val, &span->default_caller_data.rdnis.plan);
		} else if (!strcasecmp(var, "outbound-bc-transfer-cap") ||
					!strcasecmp(var, "outbound-bearer_cap")) {
			ftdm_set_bearer_capability(val, (uint8_t*)&span->default_caller_data.bearer_capability);
		} else if (!strcasecmp(var, "outbound-bc-user-layer1") ||
					!strcasecmp(var, "outbound-bearer_layer1")) {
			ftdm_set_bearer_layer1(val, (uint8_t*)&span->default_caller_data.bearer_layer1);
		} else if (!strcasecmp(var, "channel-restart-on-link-up")) {
			parse_yesno(var, val, &signal_data->restart_opt);
		} else if (!strcasecmp(var, "channel-restart-timeout")) {
			signal_data->restart_timeout = atoi(val);
		} else if (!strcasecmp(var, "local-number")) {
			if (add_local_number(val, span) != FTDM_SUCCESS) {
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "facility-timeout")) {
			parse_timer(val, &signal_data->facility_timeout);
		} else if (!strcasecmp(var, "transfer-timeout")) {
			parse_timer(val, &signal_data->transfer_timeout);
		} else if (!strcasecmp(var, "att-remove-dtmf")) {
			parse_yesno(var, val, &signal_data->att_remove_dtmf);
		} else if (!strcasecmp(var, "facility-ie-decode")) {
			parse_yesno(var, val, &signal_data->facility_ie_decode);
		} else if (!strcasecmp(var, "ignore-cause-value")) {
			parse_yesno(var, val, &signal_data->ignore_cause_value);
		} else if (!strcasecmp(var, "q931-trace")) {
			parse_yesno(var, val, &signal_data->trace_q931);
		} else if (!strcasecmp(var, "q921-trace")) {
			parse_yesno(var, val, &signal_data->trace_q921);
		} else if (!strcasecmp(var, "q931-raw-trace")) {
			parse_yesno(var, val, &signal_data->raw_trace_q931);
		} else if (!strcasecmp(var, "q921-raw-trace")) {
			parse_yesno(var, val, &signal_data->raw_trace_q921);
		} else if (!strcasecmp(var, "force-sending-complete")) {
			parse_yesno(var, val, &signal_data->force_sending_complete);
		} else if (!strcasecmp(var, "early-media-override")) {
			if (parse_early_media(val, span) != FTDM_SUCCESS) {
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "chan-id-invert-extend-bit")) {
#ifdef SANGOMA_ISDN_CHAN_ID_INVERT_BIT
			parse_yesno(var, val, &g_sngisdn_data.chan_id_invert_extend_bit);
#else
			ftdm_log(FTDM_LOG_WARNING, "chan-id-invert-extend-bit is not supported in your version of libsng_isdn\n");
#endif
		} else if (!strcasecmp(var, "cid-name-transmit-method")) {
			if (!strcasecmp(val, "display-ie")) {
				signal_data->cid_name_method = SNGISDN_CID_NAME_DISPLAY_IE;
			} else if (!strcasecmp(val, "user-user-ie")) {
				signal_data->cid_name_method = SNGISDN_CID_NAME_USR_USR_IE;
			} else if (!strcasecmp(val, "facility-ie")) {
				signal_data->cid_name_method = SNGISDN_CID_NAME_FACILITY_IE;
			} else if (!strcasecmp(val, "auto") || !strcasecmp(val, "automatic") || !strcasecmp(val, "default")) {
				signal_data->cid_name_method = SNGISDN_CID_NAME_AUTO;
			} else {
				ftdm_log(FTDM_LOG_WARNING, "Invalid option %s for parameter %s\n", val, var);
				signal_data->cid_name_method = SNGISDN_CID_NAME_AUTO;
			}
		} else if (!strcasecmp(var, "send-cid-name")) {
			if (!strcasecmp(val, "yes")) {
				signal_data->send_cid_name = SNGISDN_OPT_TRUE;
			} else if (!strcasecmp(val, "no")) {
				signal_data->send_cid_name = SNGISDN_OPT_FALSE;
			} else if (!strcasecmp(val, "auto") || !strcasecmp(val, "automatic") || !strcasecmp(val, "default")) {
				signal_data->send_cid_name = SNGISDN_OPT_DEFAULT;
			} else {
				ftdm_log(FTDM_LOG_WARNING, "Invalid option %s for parameter %s\n", val, var);
				signal_data->send_cid_name = SNGISDN_OPT_DEFAULT;
			}
		} else if (!strcasecmp(var, "timer-t301")) {
			parse_timer(val, &signal_data->timer_t301);
		} else if (!strcasecmp(var, "timer-t302")) {
			parse_timer(val, &signal_data->timer_t302);
		} else if (!strcasecmp(var, "timer-t303")) {
			parse_timer(val, &signal_data->timer_t303);
		} else if (!strcasecmp(var, "timer-t304")) {
			parse_timer(val, &signal_data->timer_t304);
		} else if (!strcasecmp(var, "timer-t305")) {
			parse_timer(val, &signal_data->timer_t305);
		} else if (!strcasecmp(var, "timer-t306")) {
			parse_timer(val, &signal_data->timer_t306);
		} else if (!strcasecmp(var, "timer-t307")) {
			parse_timer(val, &signal_data->timer_t307);
		} else if (!strcasecmp(var, "timer-t308")) {
			parse_timer(val, &signal_data->timer_t308);
		} else if (!strcasecmp(var, "timer-t310")) {
			parse_timer(val, &signal_data->timer_t310);
		} else if (!strcasecmp(var, "timer-t312")) {
			parse_timer(val, &signal_data->timer_t312);
		} else if (!strcasecmp(var, "timer-t313")) {
			parse_timer(val, &signal_data->timer_t313);
		} else if (!strcasecmp(var, "timer-t314")) {
			parse_timer(val, &signal_data->timer_t314);
		} else if (!strcasecmp(var, "timer-t316")) {
			parse_timer(val, &signal_data->timer_t316);
		} else if (!strcasecmp(var, "timer-t318")) {
			parse_timer(val, &signal_data->timer_t318);
		} else if (!strcasecmp(var, "timer-t319")) {
			parse_timer(val, &signal_data->timer_t319);
		} else if (!strcasecmp(var, "timer-t322")) {
			parse_timer(val, &signal_data->timer_t322);
		} else if (!strcasecmp(var, "trunkgroup")) {
			/* Do nothing, we already parsed this parameter */
		} else if (!strcasecmp(var, "send-connect-ack")) {
			parse_yesno(var, val, &signal_data->send_connect_ack);
		} else {
			ftdm_log(FTDM_LOG_WARNING, "Ignoring unknown parameter %s\n", ftdm_parameters[paramindex].var);
		}
	} /* for (paramindex = 0; ftdm_parameters[paramindex].var; paramindex++) */
	
	if (signal_data->switchtype == SNGISDN_SWITCH_INVALID) {
		ftdm_log(FTDM_LOG_ERROR, "%s: switchtype not specified", span->name);
		return FTDM_FAIL;
	}
	if (signal_data->signalling == SNGISDN_SIGNALING_INVALID) {
		ftdm_log(FTDM_LOG_ERROR, "%s: signalling not specified", span->name);
		return FTDM_FAIL;
	}

	if (span->default_caller_data.bearer_layer1 == FTDM_INVALID_INT_PARM) {
		if (signal_data->switchtype == SNGISDN_SWITCH_EUROISDN ||
			signal_data->switchtype == SNGISDN_SWITCH_QSIG) {
			span->default_caller_data.bearer_layer1 = IN_UIL1_G711ALAW;
		} else {
			span->default_caller_data.bearer_layer1 = IN_UIL1_G711ULAW;
		}
	}
	return FTDM_SUCCESS;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
