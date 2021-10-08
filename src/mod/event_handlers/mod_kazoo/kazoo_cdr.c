/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Luis Azedo <luis@2600hz.com>
 *
 * mod_hacks.c -- hacks with state handlers
 *
 */
#include "mod_kazoo.h"

#define MY_EVENT_JSON_CDR "KZ_CDR"

#define maybe_add_json_string(_json, _name, _string) \
	if (!zstr(_string)) cJSON_AddItemToObject(_json, _name, cJSON_CreateString((char *)_string))

static void kz_switch_ivr_set_json_profile_data(cJSON *json, switch_caller_profile_t *caller_profile)
{
	cJSON *soft = NULL;
	profile_node_t *pn = NULL;

	maybe_add_json_string(json, "Username", caller_profile->username);
	maybe_add_json_string(json, "Dialplan", caller_profile->dialplan);
	maybe_add_json_string(json, "ANI", caller_profile->ani);
	maybe_add_json_string(json, "ANIII", caller_profile->aniii);
	maybe_add_json_string(json, "Caller-ID-Name", caller_profile->caller_id_name);
	maybe_add_json_string(json, "Caller-ID-Number", caller_profile->caller_id_number);
	maybe_add_json_string(json, "Caller-ID-Original-Name", caller_profile->orig_caller_id_name);
	maybe_add_json_string(json, "Caller-ID-Original-Number", caller_profile->orig_caller_id_number);
	maybe_add_json_string(json, "Network-Address", caller_profile->network_addr);
	maybe_add_json_string(json, "RDNIS", caller_profile->rdnis);
	maybe_add_json_string(json, "Destination-Number", caller_profile->destination_number);
	maybe_add_json_string(json, "Callee-ID-Name", caller_profile->callee_id_name);
	maybe_add_json_string(json, "Callee-ID-Number", caller_profile->callee_id_number);
	maybe_add_json_string(json, "UUID", caller_profile->uuid);
	maybe_add_json_string(json, "Source", caller_profile->source);
	maybe_add_json_string(json, "Context", caller_profile->context);
	maybe_add_json_string(json, "Channel-Name", caller_profile->chan_name);
	maybe_add_json_string(json, "Profile-UUID", caller_profile->uuid_str);
	maybe_add_json_string(json, "Profile-Clone-Of", caller_profile->clone_of);
	maybe_add_json_string(json, "Transfer-Source", caller_profile->transfer_source);
	cJSON_AddItemToObject(json, "Direction", cJSON_CreateString(caller_profile->direction == SWITCH_CALL_DIRECTION_OUTBOUND ? "outbound" : "inbound"));
	cJSON_AddItemToObject(json, "Logical-Direction", cJSON_CreateString(caller_profile->logical_direction == SWITCH_CALL_DIRECTION_OUTBOUND ? "outbound" : "inbound"));

	soft = cJSON_CreateObject();
	for (pn = caller_profile->soft; pn; pn = pn->next) {
		maybe_add_json_string(soft, pn->var, pn->val);
	}

	cJSON_AddItemToObject(json, "Directory", soft);
}

SWITCH_DECLARE(void) kz_switch_ivr_set_json_call_flaws(cJSON *json, switch_core_session_t *session, switch_media_type_t type)
{
	const char *name = (type == SWITCH_MEDIA_TYPE_VIDEO) ? "Video" : "Audio";
	cJSON *j_stat;
	switch_rtp_stats_t *stats = switch_core_media_get_stats(session, type, NULL);

	if (!stats) return;

	if (!stats->inbound.error_log && !stats->outbound.error_log) return;

	j_stat = cJSON_CreateObject();
	cJSON_AddItemToObject(json, name, j_stat);

	if (stats->inbound.error_log) {
		cJSON *j_err_log, *j_err, *j_in;
		switch_error_period_t *ep;

		j_in = cJSON_CreateObject();
		cJSON_AddItemToObject(j_stat, "Inbound", j_in);

		j_err_log = cJSON_CreateArray();
		cJSON_AddItemToObject(j_in, "Error-Log", j_err_log);

		for(ep = stats->inbound.error_log; ep; ep = ep->next) {

			if (!(ep->start && ep->stop)) continue;

			j_err = cJSON_CreateObject();

			cJSON_AddItemToObject(j_err, "Start", cJSON_CreateNumber(ep->start));
			cJSON_AddItemToObject(j_err, "Stop", cJSON_CreateNumber(ep->stop));
			cJSON_AddItemToObject(j_err, "Flaws", cJSON_CreateNumber(ep->flaws));
			cJSON_AddItemToObject(j_err, "Consecutive-Flaws", cJSON_CreateNumber(ep->consecutive_flaws));
			cJSON_AddItemToObject(j_err, "Duration-MS", cJSON_CreateNumber((ep->stop - ep->start) / 1000));
			cJSON_AddItemToArray(j_err_log, j_err);
		}
	}

	if (stats->outbound.error_log) {
		cJSON *j_err_log, *j_err, *j_out;
		switch_error_period_t *ep;

		j_out = cJSON_CreateObject();
		cJSON_AddItemToObject(j_stat, "Outbound", j_out);

		j_err_log = cJSON_CreateArray();
		cJSON_AddItemToObject(j_out, "Error-Log", j_err_log);

		for(ep = stats->outbound.error_log; ep; ep = ep->next) {

			if (!(ep->start && ep->stop)) continue;

			j_err = cJSON_CreateObject();

			cJSON_AddItemToObject(j_err, "Start", cJSON_CreateNumber(ep->start));
			cJSON_AddItemToObject(j_err, "Stop", cJSON_CreateNumber(ep->stop));
			cJSON_AddItemToObject(j_err, "Flaws", cJSON_CreateNumber(ep->flaws));
			cJSON_AddItemToObject(j_err, "Consecutive-Flaws", cJSON_CreateNumber(ep->consecutive_flaws));
			cJSON_AddItemToObject(j_err, "Duration-MS", cJSON_CreateNumber((ep->stop - ep->start) / 1000));
			cJSON_AddItemToArray(j_err_log, j_err);
		}
	}
}

#define add_jstat(_j, _i, _s)											\
	switch_snprintf(var_val, sizeof(var_val), "%" SWITCH_SIZE_T_FMT, _i); \
	cJSON_AddItemToObject(_j, _s, cJSON_CreateNumber(_i))

SWITCH_DECLARE(void) kz_switch_ivr_set_json_call_stats(cJSON *json, switch_core_session_t *session, switch_media_type_t type)
{
	const char *name = (type == SWITCH_MEDIA_TYPE_VIDEO) ? "Video" : "Audio";
	cJSON *j_stat, *j_in, *j_out;
	switch_rtp_stats_t *stats = switch_core_media_get_stats(session, type, NULL);
	char var_val[35] = "";

	if (!stats) return;

	j_stat = cJSON_CreateObject();
	j_in = cJSON_CreateObject();
	j_out = cJSON_CreateObject();

	cJSON_AddItemToObject(json, name, j_stat);
	cJSON_AddItemToObject(j_stat, "Inbound", j_in);
	cJSON_AddItemToObject(j_stat, "Outbound", j_out);

	stats->inbound.std_deviation = sqrt(stats->inbound.variance);

	add_jstat(j_in, stats->inbound.raw_bytes, "Raw-Bytes");
	add_jstat(j_in, stats->inbound.media_bytes, "Media-Bytes");
	add_jstat(j_in, stats->inbound.packet_count, "Packet-Count");
	add_jstat(j_in, stats->inbound.media_packet_count, "Media-Packet-Count");
	add_jstat(j_in, stats->inbound.skip_packet_count, "Skip-Packet-Count");
	add_jstat(j_in, stats->inbound.jb_packet_count, "Jitter-Packet-Count");
	add_jstat(j_in, stats->inbound.dtmf_packet_count, "DTMF-Packet-Count");
	add_jstat(j_in, stats->inbound.cng_packet_count, "CNG-Packet-Count");
	add_jstat(j_in, stats->inbound.flush_packet_count, "Flush-Packet-Count");
	add_jstat(j_in, stats->inbound.largest_jb_size, "Largest-JB-Size");
	add_jstat(j_in, stats->inbound.min_variance, "Jitter-Min-Variance");
	add_jstat(j_in, stats->inbound.max_variance, "Jitter-Max-Variance");
	add_jstat(j_in, stats->inbound.lossrate, "Jitter-Loss-Rate");
	add_jstat(j_in, stats->inbound.burstrate, "Jitter-Burst-Rate");
	add_jstat(j_in, stats->inbound.mean_interval, "Mean-Interval");
	add_jstat(j_in, stats->inbound.flaws, "Flaw-Total");
	add_jstat(j_in, stats->inbound.R, "Quality-Percentage");
	add_jstat(j_in, stats->inbound.mos, "MOS");


	add_jstat(j_out, stats->outbound.raw_bytes, "Raw-Bytes");
	add_jstat(j_out, stats->outbound.media_bytes, "Media-Bytes");
	add_jstat(j_out, stats->outbound.packet_count, "Packet-Count");
	add_jstat(j_out, stats->outbound.media_packet_count, "Media-Packet-Count");
	add_jstat(j_out, stats->outbound.skip_packet_count, "Skip-Packet-Count");
	add_jstat(j_out, stats->outbound.dtmf_packet_count, "DTMF-Packet-Count");
	add_jstat(j_out, stats->outbound.cng_packet_count, "CNG-Packet-Count");
	add_jstat(j_out, stats->rtcp.packet_count, "RTCP-Packet-Count");
	add_jstat(j_out, stats->rtcp.octet_count, "RTCP-Octet-Count");
}

static switch_status_t kz_report_channel_flaws(switch_core_session_t *session, switch_event_t *cdr_event)
{
	cJSON *callStats = cJSON_CreateObject();

	kz_switch_ivr_set_json_call_flaws(callStats, session, SWITCH_MEDIA_TYPE_AUDIO);
	kz_switch_ivr_set_json_call_flaws(callStats, session, SWITCH_MEDIA_TYPE_VIDEO);

	switch_event_add_header_string(cdr_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "_json_channel_media_errors", cJSON_PrintUnformatted(callStats));

	cJSON_Delete(callStats);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kz_report_channel_stats(switch_core_session_t *session, switch_event_t *cdr_event)
{
	cJSON *callStats = cJSON_CreateObject();

	kz_switch_ivr_set_json_call_stats(callStats, session, SWITCH_MEDIA_TYPE_AUDIO);
	kz_switch_ivr_set_json_call_stats(callStats, session, SWITCH_MEDIA_TYPE_VIDEO);

	switch_event_add_header_string(cdr_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "_json_channel_stats", cJSON_PrintUnformatted(callStats));

	cJSON_Delete(callStats);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t kz_report_app_log(switch_core_session_t *session, switch_event_t *cdr_event)
{
	switch_app_log_t *ap, *app_log = switch_core_session_get_app_log(session);
	cJSON *j_apps = NULL;

	if (!app_log) {
		return SWITCH_STATUS_FALSE;
	}

	j_apps = cJSON_CreateArray();

	for (ap = app_log; ap; ap = ap->next) {
		cJSON *j_application = cJSON_CreateObject();
		cJSON_AddItemToObject(j_application, "app_name", cJSON_CreateString(ap->app));
		cJSON_AddItemToObject(j_application, "app_data", cJSON_CreateString(ap->arg));
		cJSON_AddItemToObject(j_application, "app_stamp", cJSON_CreateNumber(ap->stamp));
		cJSON_AddItemToArray(j_apps, j_application);
	}

	switch_event_add_header_string(cdr_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "_json_application_log", cJSON_PrintUnformatted(j_apps));

	cJSON_Delete(j_apps);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t kz_report_callflow_extension(switch_caller_profile_t *caller_profile, cJSON *j_profile)
{
	cJSON *j_caller_extension, *j_caller_extension_apps, *j_application, *j_inner_extension;
	if (caller_profile->caller_extension) {
		switch_caller_application_t *ap;

		j_caller_extension = cJSON_CreateObject();
		j_caller_extension_apps = cJSON_CreateArray();

		cJSON_AddItemToObject(j_profile, "extension", j_caller_extension);

		cJSON_AddItemToObject(j_caller_extension, "name", cJSON_CreateString(caller_profile->caller_extension->extension_name));
		cJSON_AddItemToObject(j_caller_extension, "number", cJSON_CreateString(caller_profile->caller_extension->extension_number));
		cJSON_AddItemToObject(j_caller_extension, "applications", j_caller_extension_apps);

		if (caller_profile->caller_extension->current_application) {
			cJSON_AddItemToObject(j_caller_extension, "current_app", cJSON_CreateString(caller_profile->caller_extension->current_application->application_name));
		}

		for (ap = caller_profile->caller_extension->applications; ap; ap = ap->next) {
			j_application = cJSON_CreateObject();

			cJSON_AddItemToArray(j_caller_extension_apps, j_application);

			if (ap == caller_profile->caller_extension->current_application) {
				cJSON_AddItemToObject(j_application, "last_executed", cJSON_CreateString("true"));
			}
			cJSON_AddItemToObject(j_application, "app_name", cJSON_CreateString(ap->application_name));
			cJSON_AddItemToObject(j_application, "app_data", cJSON_CreateString(switch_str_nil(ap->application_data)));
		}

		if (caller_profile->caller_extension->children) {
			switch_caller_profile_t *cp = NULL;
			j_inner_extension = cJSON_CreateArray();
			cJSON_AddItemToObject(j_caller_extension, "sub_extensions", j_inner_extension);
			for (cp = caller_profile->caller_extension->children; cp; cp = cp->next) {

				if (!cp->caller_extension) {
					continue;
				}

				j_caller_extension = cJSON_CreateObject();
				cJSON_AddItemToArray(j_inner_extension, j_caller_extension);

				cJSON_AddItemToObject(j_caller_extension, "name", cJSON_CreateString(cp->caller_extension->extension_name));
				cJSON_AddItemToObject(j_caller_extension, "number", cJSON_CreateString(cp->caller_extension->extension_number));

				cJSON_AddItemToObject(j_caller_extension, "dialplan", cJSON_CreateString((char *)cp->dialplan));

				if (cp->caller_extension->current_application) {
					cJSON_AddItemToObject(j_caller_extension, "current_app", cJSON_CreateString(cp->caller_extension->current_application->application_name));
				}

				j_caller_extension_apps = cJSON_CreateArray();
				cJSON_AddItemToObject(j_caller_extension, "applications", j_caller_extension_apps);
				for (ap = cp->caller_extension->applications; ap; ap = ap->next) {
					j_application = cJSON_CreateObject();
					cJSON_AddItemToArray(j_caller_extension_apps, j_application);

					if (ap == cp->caller_extension->current_application) {
						cJSON_AddItemToObject(j_application, "last_executed", cJSON_CreateString("true"));
					}
					cJSON_AddItemToObject(j_application, "app_name", cJSON_CreateString(ap->application_name));
					cJSON_AddItemToObject(j_application, "app_data", cJSON_CreateString(switch_str_nil(ap->application_data)));
				}
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t kz_report_callflow(switch_core_session_t *session, switch_event_t *cdr_event)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	cJSON *j_main_cp, *j_times, *j_callflow, *j_profile, *j_o;


	caller_profile = switch_channel_get_caller_profile(channel);

	j_callflow = cJSON_CreateArray();

	while (caller_profile) {

		j_profile = cJSON_CreateObject();

		if (!zstr(caller_profile->dialplan)) {
			cJSON_AddItemToObject(j_profile, "dialplan", cJSON_CreateString((char *)caller_profile->dialplan));
		}

		if (!zstr(caller_profile->profile_index)) {
			cJSON_AddItemToObject(j_profile, "profile_index", cJSON_CreateString((char *)caller_profile->profile_index));
		}

		kz_report_callflow_extension(caller_profile, j_profile);

		j_main_cp = cJSON_CreateObject();
		cJSON_AddItemToObject(j_profile, "Caller-Profile", j_main_cp);

		kz_switch_ivr_set_json_profile_data(j_main_cp, caller_profile);

		if (caller_profile->originator_caller_profile) {
			j_o = cJSON_CreateObject();
			cJSON_AddItemToObject(j_main_cp, "originator", j_o);
			kz_switch_ivr_set_json_profile_data(j_o, caller_profile->originator_caller_profile);
			kz_report_callflow_extension(caller_profile->originator_caller_profile, j_o);
		}

		if (caller_profile->originatee_caller_profile) {
			j_o = cJSON_CreateObject();
			cJSON_AddItemToObject(j_main_cp, "originatee", j_o);
			kz_switch_ivr_set_json_profile_data(j_o, caller_profile->originatee_caller_profile);
			kz_report_callflow_extension(caller_profile->originatee_caller_profile, j_o);
		}

		if (caller_profile->times) {
			j_times = cJSON_CreateObject();
			cJSON_AddItemToObject(j_profile, "Time", j_times);
			cJSON_AddItemToObject(j_times, "Created", cJSON_CreateNumber(caller_profile->times->created));
			cJSON_AddItemToObject(j_times, "Profile-Created", cJSON_CreateNumber(caller_profile->times->profile_created));
			cJSON_AddItemToObject(j_times, "Progress", cJSON_CreateNumber(caller_profile->times->progress));
			cJSON_AddItemToObject(j_times, "Progress-Media", cJSON_CreateNumber(caller_profile->times->progress_media));
			cJSON_AddItemToObject(j_times, "Answered", cJSON_CreateNumber(caller_profile->times->answered));
			cJSON_AddItemToObject(j_times, "Bridged", cJSON_CreateNumber(caller_profile->times->bridged));
			cJSON_AddItemToObject(j_times, "Last-Hold", cJSON_CreateNumber(caller_profile->times->last_hold));
			cJSON_AddItemToObject(j_times, "Hold-Accumulated", cJSON_CreateNumber(caller_profile->times->hold_accum));
			cJSON_AddItemToObject(j_times, "Hangup", cJSON_CreateNumber(caller_profile->times->hungup));
			cJSON_AddItemToObject(j_times, "Resurrect", cJSON_CreateNumber(caller_profile->times->resurrected));
			cJSON_AddItemToObject(j_times, "Transfer", cJSON_CreateNumber(caller_profile->times->transferred));
		}
		cJSON_AddItemToArray(j_callflow, j_profile);
		caller_profile = caller_profile->next;
	}

	switch_event_add_header_string(cdr_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "_json_callflow", cJSON_PrintUnformatted(j_callflow));

	cJSON_Delete(j_callflow);


	return SWITCH_STATUS_SUCCESS;

}


#define ORIGINATED_LEGS_VARIABLE "originated_legs"
#define ORIGINATED_LEGS_ITEM_DELIM ';'

#define ORIGINATE_CAUSES_VARIABLE "originate_causes"
#define ORIGINATE_CAUSES_ITEM_DELIM ';'

static switch_status_t kz_report_originated_legs(switch_core_session_t *session, switch_event_t *cdr_event)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	cJSON *j_originated = cJSON_CreateArray();
	const char *originated_legs_var = NULL, *originate_causes_var = NULL;
	int idx = 0;

	while(1) {
		char *argv_leg[10] = { 0 }, *argv_cause[10] = { 0 };
		char *originated_legs, *originate_causes;
		cJSON *j_originated_leg;
		originated_legs_var = switch_channel_get_variable_dup(channel, ORIGINATED_LEGS_VARIABLE, SWITCH_FALSE, idx);
		originate_causes_var = switch_channel_get_variable_dup(channel, ORIGINATE_CAUSES_VARIABLE, SWITCH_FALSE, idx);

		if (zstr(originated_legs_var) || zstr(originate_causes_var)) {
			break;
		}

		originated_legs = strdup(originated_legs_var);
		originate_causes = strdup(originate_causes_var);

		switch_separate_string(originated_legs, ORIGINATED_LEGS_ITEM_DELIM, argv_leg, (sizeof(argv_leg) / sizeof(argv_leg[0])));
		switch_separate_string(originate_causes, ORIGINATE_CAUSES_ITEM_DELIM, argv_cause, (sizeof(argv_cause) / sizeof(argv_cause[0])));

		j_originated_leg = cJSON_CreateObject();
		cJSON_AddItemToObject(j_originated_leg, "Call-ID", cJSON_CreateString(argv_leg[0]));
		cJSON_AddItemToObject(j_originated_leg, "Caller-ID-Name", cJSON_CreateString(argv_leg[1]));
		cJSON_AddItemToObject(j_originated_leg, "Caller-ID-Number", cJSON_CreateString(argv_leg[2]));
		cJSON_AddItemToObject(j_originated_leg, "Result", cJSON_CreateString(argv_cause[1]));

		cJSON_AddItemToArray(j_originated, j_originated_leg);

		switch_safe_free(originated_legs);
		switch_safe_free(originate_causes);

		idx++;
	}

	switch_event_add_header_string(cdr_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "_json_originated_legs", cJSON_PrintUnformatted(j_originated));

	cJSON_Delete(j_originated);

	return SWITCH_STATUS_SUCCESS;
}

#define MAX_HISTORY 50
#define HST_ARRAY_DELIM "|:"
#define HST_ITEM_DELIM ':'

static void kz_report_transfer_history_item(char* value, cJSON *json)
{
	char *argv[4] = { 0 };
	char *item = strdup(value);
	int argc = switch_separate_string(item, HST_ITEM_DELIM, argv, (sizeof(argv) / sizeof(argv[0])));
	cJSON *jitem = cJSON_CreateObject();
	char *epoch = NULL, *callid = NULL, *type = NULL;
	int add = 0;
	if(argc == 4) {
		add = 1;
		epoch = argv[0];
		callid = argv[1];
		type = argv[2];

		if(!strncmp(type, "bl_xfer", 7)) {
			//char *split = strchr(argv[3], '/');
			//if(split) *(split++) = '\0';
			cJSON_AddItemToObject(jitem, "Caller-Profile-ID", cJSON_CreateString(callid));
			cJSON_AddItemToObject(jitem, "Type", cJSON_CreateString("blind"));
			cJSON_AddItemToObject(jitem, "Extension", cJSON_CreateString(argv[3]));
			cJSON_AddItemToObject(jitem, "Timestamp", cJSON_CreateNumber(strtod(epoch, NULL)));
		} else if(!strncmp(type, "att_xfer", 8)) {
			char *split = strchr(argv[3], '/');
			if(split) {
				*(split++) = '\0';
				cJSON_AddItemToObject(jitem, "Caller-Profile-ID", cJSON_CreateString(callid));
				cJSON_AddItemToObject(jitem, "Type", cJSON_CreateString("attended"));
				cJSON_AddItemToObject(jitem, "Transferee", cJSON_CreateString(argv[3]));
				cJSON_AddItemToObject(jitem, "Transferer", cJSON_CreateString(split));
				cJSON_AddItemToObject(jitem, "Timestamp", cJSON_CreateNumber(strtod(epoch, NULL)));
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "TRANSFER TYPE '%s' NOT HANDLED => %s\n", type, item);
				add = 0;
			}
		} else if(!strncmp(type, "uuid_br", 7)) {
			cJSON_AddItemToObject(jitem, "Caller-Profile-ID", cJSON_CreateString(callid));
			cJSON_AddItemToObject(jitem, "Type", cJSON_CreateString("bridge"));
			cJSON_AddItemToObject(jitem, "Other-Leg", cJSON_CreateString(argv[3]));
			cJSON_AddItemToObject(jitem, "Timestamp", cJSON_CreateNumber(strtod(epoch, NULL)));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "TRANSFER TYPE '%s' NOT HANDLED => %s\n", type, item);
			add = 0;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "TRANSFER TYPE SPLIT ERROR %i => %s\n", argc, item);
	}
	if(add) {
		cJSON_AddItemToArray(json, jitem);
	} else {
		cJSON_Delete(jitem);
	}
	switch_safe_free(item);
}

static switch_status_t kz_report_transfer_history(switch_core_session_t *session, switch_event_t *cdr_event, const char* var_name)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	cJSON *j_transfer = NULL;
	char *tmp_history = NULL, *history = NULL, *argv[MAX_HISTORY] = { 0 };
	char event_header[50];
	int n, argc = 0;
	const char *transfer_var = switch_channel_get_variable_dup(channel, var_name, SWITCH_FALSE, -1);
	if (zstr(transfer_var)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(tmp_history = strdup(transfer_var))) {
		return SWITCH_STATUS_SUCCESS;
	}

	sprintf(event_header, "_json_%s", var_name);
	history = tmp_history;
	j_transfer = cJSON_CreateArray();

	if (!strncmp(history, "ARRAY::", 7)) {
		history += 7;
		argc = switch_separate_string_string(history, HST_ARRAY_DELIM, argv, (sizeof(argv) / sizeof(argv[0])));
		for(n=0; n < argc; n++) {
			kz_report_transfer_history_item(argv[n], j_transfer);
		}
		switch_event_add_header_string(cdr_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, event_header, cJSON_PrintUnformatted(j_transfer));
	} else if (strchr(history, HST_ITEM_DELIM)) {
		kz_report_transfer_history_item(history, j_transfer);
		switch_event_add_header_string(cdr_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, event_header, cJSON_PrintUnformatted(j_transfer));
	}
	cJSON_Delete(j_transfer);
	switch_safe_free(tmp_history);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kz_report(switch_core_session_t *session, switch_event_t *cdr_event)
{
	kz_report_app_log(session, cdr_event);
	kz_report_callflow(session, cdr_event);
	kz_report_channel_stats(session, cdr_event);
	kz_report_channel_flaws(session, cdr_event);
	kz_report_originated_legs(session, cdr_event);
	kz_report_transfer_history(session, cdr_event, SWITCH_TRANSFER_HISTORY_VARIABLE);
	kz_report_transfer_history(session, cdr_event, SWITCH_TRANSFER_SOURCE_VARIABLE);
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t kz_cdr_on_reporting(switch_core_session_t *session)
{
	switch_event_t *cdr_event = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_event_create_subclass(&cdr_event, SWITCH_EVENT_CUSTOM, MY_EVENT_JSON_CDR) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "error creating event for report data!\n");
		return SWITCH_STATUS_FALSE;
	}

	kz_report(session, cdr_event);
	switch_channel_event_set_data(channel, cdr_event);
	switch_event_fire(&cdr_event);

	return SWITCH_STATUS_SUCCESS;
}


static switch_state_handler_table_t kz_cdr_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ kz_cdr_on_reporting
};


static void kz_cdr_register_state_handlers()
{
	switch_core_add_state_handler(&kz_cdr_state_handlers);
}

static void kz_cdr_unregister_state_handlers()
{
	switch_core_remove_state_handler(&kz_cdr_state_handlers);
}

static void kz_cdr_register_events()
{
	if (switch_event_reserve_subclass(MY_EVENT_JSON_CDR) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", MY_EVENT_JSON_CDR);
	}
}

static void kz_cdr_unregister_events()
{
	switch_event_free_subclass(MY_EVENT_JSON_CDR);
}


void kz_cdr_start()
{
	kz_cdr_register_events();
	kz_cdr_register_state_handlers();
}

void kz_cdr_stop()
{
	kz_cdr_unregister_state_handlers();
	kz_cdr_unregister_events();
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
