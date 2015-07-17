/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter at 0xdecafbad dot com>
 * Dale Thatcher <freeswitch at dalethatcher dot com>
 * Chris Danielson <chris at maxpowersoft dot com>
 * Rupa Schomaker <rupa@rupa.com>
 * David Weekly <david@weekly.org>
 * Joao Mesquita <jmesquita@gmail.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */
#include <mod_conference.h>


inline switch_bool_t conference_cdr_test_mflag(conference_cdr_node_t *np, member_flag_t mflag)
{
	return !!np->mflags[mflag];
}

const char *conference_cdr_audio_flow(conference_member_t *member)
{
	const char *flow = "sendrecv";

	if (!conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		flow = "recvonly";
	}

	if (member->channel && switch_channel_test_flag(member->channel, CF_HOLD)) {
		flow = conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) ? "sendonly" : "inactive";
	}

	return flow;
}


char *conference_cdr_rfc4579_render(conference_obj_t *conference, switch_event_t *event, switch_event_t *revent)
{
	switch_xml_t xml, x_tag, x_tag1, x_tag2, x_tag3, x_tag4;
	char tmp[30];
	const char *domain;	const char *name;
	char *dup_domain = NULL;
	char *uri;
	int off = 0, off1 = 0, off2 = 0, off3 = 0, off4 = 0;
	conference_cdr_node_t *np;
	char *tmpp = tmp;
	char *xml_text = NULL;

	if (!(xml = switch_xml_new("conference-info"))) {
		abort();
	}

	switch_mutex_lock(conference->mutex);
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->doc_version);
	conference->doc_version++;
	switch_mutex_unlock(conference->mutex);

	if (!event || !(name = switch_event_get_header(event, "conference-name"))) {
		if (!(name = conference->name)) {
			name = "conference";
		}
	}

	if (!event || !(domain = switch_event_get_header(event, "conference-domain"))) {
		if (!(domain = conference->domain)) {
			dup_domain = switch_core_get_domain(SWITCH_TRUE);
			if (!(domain = dup_domain)) {
				domain = "cluecon.com";
			}
		}
	}

	switch_xml_set_attr_d(xml, "version", tmpp);

	switch_xml_set_attr_d(xml, "state", "full");
	switch_xml_set_attr_d(xml, "xmlns", "urn:ietf:params:xml:ns:conference-info");


	uri = switch_mprintf("sip:%s@%s", name, domain);
	switch_xml_set_attr_d(xml, "entity", uri);

	if (!(x_tag = switch_xml_add_child_d(xml, "conference-description", off++))) {
		abort();
	}

	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "display-text", off1++))) {
		abort();
	}
	switch_xml_set_txt_d(x_tag1, conference->desc ? conference->desc : "FreeSWITCH Conference");


	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "conf-uris", off1++))) {
		abort();
	}

	if (!(x_tag2 = switch_xml_add_child_d(x_tag1, "entry", off2++))) {
		abort();
	}

	if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "uri", off3++))) {
		abort();
	}
	switch_xml_set_txt_d(x_tag3, uri);



	if (!(x_tag = switch_xml_add_child_d(xml, "conference-state", off++))) {
		abort();
	}
	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "user-count", off1++))) {
		abort();
	}
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->count);
	switch_xml_set_txt_d(x_tag1, tmpp);

#if 0
	if (conference->count == 0) {
		switch_event_add_header(revent, SWITCH_STACK_BOTTOM, "notfound", "true");
	}
#endif

	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "active", off1++))) {
		abort();
	}
	switch_xml_set_txt_d(x_tag1, "true");

	off1 = off2 = off3 = off4 = 0;

	if (!(x_tag = switch_xml_add_child_d(xml, "users", off++))) {
		abort();
	}

	switch_mutex_lock(conference->member_mutex);

	for (np = conference->cdr_nodes; np; np = np->next) {
		char *user_uri = NULL;
		switch_channel_t *channel = NULL;

		if (!np->cp || (np->member && !np->member->session) || np->leave_time) { /* for now we'll remove participants when the leave */
			continue;
		}

		if (np->member && np->member->session) {
			channel = switch_core_session_get_channel(np->member->session);
		}

		if (!(x_tag1 = switch_xml_add_child_d(x_tag, "user", off1++))) {
			abort();
		}

		if (channel) {
			const char *uri = switch_channel_get_variable_dup(channel, "conference_invite_uri", SWITCH_FALSE, -1);

			if (uri) {
				user_uri = strdup(uri);
			}
		}

		if (!user_uri) {
			user_uri = switch_mprintf("sip:%s@%s", np->cp->caller_id_number, domain);
		}


		switch_xml_set_attr_d(x_tag1, "state", "full");
		switch_xml_set_attr_d(x_tag1, "entity", user_uri);

		if (!(x_tag2 = switch_xml_add_child_d(x_tag1, "display-text", off2++))) {
			abort();
		}
		switch_xml_set_txt_d(x_tag2, np->cp->caller_id_name);


		if (!(x_tag2 = switch_xml_add_child_d(x_tag1, "endpoint", off2++))) {
			abort();
		}
		switch_xml_set_attr_d(x_tag2, "entity", user_uri);

		if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "display-text", off3++))) {
			abort();
		}
		switch_xml_set_txt_d(x_tag3, np->cp->caller_id_name);


		if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "status", off3++))) {
			abort();
		}
		switch_xml_set_txt_d(x_tag3, np->leave_time ? "disconnected" : "connected");


		if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "joining-info", off3++))) {
			abort();
		}
		if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "when", off4++))) {
			abort();
		} else {
			switch_time_exp_t tm;
			switch_size_t retsize;
			const char *fmt = "%Y-%m-%dT%H:%M:%S%z";
			char *p;

			switch_time_exp_lt(&tm, (switch_time_t) conference->start_time * 1000000);
			switch_strftime_nocheck(tmp, &retsize, sizeof(tmp), fmt, &tm);
			p = end_of_p(tmpp) -1;
			snprintf(p, 4, ":00");


			switch_xml_set_txt_d(x_tag4, tmpp);
		}




		/** ok so this is in the rfc but not the xsd
			if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "joining-method", off3++))) {
			abort();
			}
			switch_xml_set_txt_d(x_tag3, np->cp->direction == SWITCH_CALL_DIRECTION_INBOUND ? "dialed-in" : "dialed-out");
		*/

		if (np->member) {
			const char *var;
			//char buf[1024];

			//switch_snprintf(buf, sizeof(buf), "conference_%s_%s_%s", conference->name, conference->domain, np->cp->caller_id_number);
			//switch_channel_set_variable(channel, "conference_call_key", buf);

			if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "media", off3++))) {
				abort();
			}

			snprintf(tmp, sizeof(tmp), "%ua", np->member->id);
			switch_xml_set_attr_d(x_tag3, "id", tmpp);


			if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "type", off4++))) {
				abort();
			}
			switch_xml_set_txt_d(x_tag4, "audio");

			if ((var = switch_channel_get_variable(channel, "rtp_use_ssrc"))) {
				if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "src-id", off4++))) {
					abort();
				}
				switch_xml_set_txt_d(x_tag4, var);
			}

			if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "status", off4++))) {
				abort();
			}
			switch_xml_set_txt_d(x_tag4, conference_cdr_audio_flow(np->member));


			if (switch_channel_test_flag(channel, CF_VIDEO)) {
				off4 = 0;

				if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "media", off3++))) {
					abort();
				}

				snprintf(tmp, sizeof(tmp), "%uv", np->member->id);
				switch_xml_set_attr_d(x_tag3, "id", tmpp);


				if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "type", off4++))) {
					abort();
				}
				switch_xml_set_txt_d(x_tag4, "video");

				if ((var = switch_channel_get_variable(channel, "rtp_use_video_ssrc"))) {
					if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "src-id", off4++))) {
						abort();
					}
					switch_xml_set_txt_d(x_tag4, var);
				}

				if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "status", off4++))) {
					abort();
				}
				switch_xml_set_txt_d(x_tag4, switch_channel_test_flag(channel, CF_HOLD) ? "sendonly" : "sendrecv");

			}
		}

		switch_safe_free(user_uri);
	}

	switch_mutex_unlock(conference->member_mutex);

	off1 = off2 = off3 = off4 = 0;

	xml_text = switch_xml_toxml(xml, SWITCH_TRUE);
	switch_xml_free(xml);

	switch_safe_free(dup_domain);
	switch_safe_free(uri);

	return xml_text;
}


cJSON *conference_cdr_json_render(conference_obj_t *conference, cJSON *req)
{
	char tmp[30];
	const char *domain;	const char *name;
	char *dup_domain = NULL;
	char *uri;
	conference_cdr_node_t *np;
	char *tmpp = tmp;
	cJSON *json = cJSON_CreateObject(), *jusers = NULL, *jold_users = NULL, *juser = NULL, *jvars = NULL;

	switch_assert(json);

	switch_mutex_lock(conference->mutex);
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->doc_version);
	conference->doc_version++;
	switch_mutex_unlock(conference->mutex);

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}


	uri = switch_mprintf("%s@%s", name, domain);
	json_add_child_string(json, "entity", uri);
	json_add_child_string(json, "conferenceDescription", conference->desc ? conference->desc : "FreeSWITCH Conference");
	json_add_child_string(json, "conferenceState", "active");
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->count);
	json_add_child_string(json, "userCount", tmp);

	jusers = json_add_child_array(json, "users");
	jold_users = json_add_child_array(json, "oldUsers");

	switch_mutex_lock(conference->member_mutex);

	for (np = conference->cdr_nodes; np; np = np->next) {
		char *user_uri = NULL;
		switch_channel_t *channel = NULL;
		switch_time_exp_t tm;
		switch_size_t retsize;
		const char *fmt = "%Y-%m-%dT%H:%M:%S%z";
		char *p;

		if (np->record_path || !np->cp) {
			continue;
		}

		//if (!np->cp || (np->member && !np->member->session) || np->leave_time) { /* for now we'll remove participants when they leave */
		//continue;
		//}

		if (np->member && np->member->session) {
			channel = switch_core_session_get_channel(np->member->session);
		}

		juser = cJSON_CreateObject();

		if (channel) {
			const char *uri = switch_channel_get_variable_dup(channel, "conference_invite_uri", SWITCH_FALSE, -1);

			if (uri) {
				user_uri = strdup(uri);
			}
		}

		if (np->cp) {

			if (!user_uri) {
				user_uri = switch_mprintf("%s@%s", np->cp->caller_id_number, domain);
			}

			json_add_child_string(juser, "entity", user_uri);
			json_add_child_string(juser, "displayText", np->cp->caller_id_name);
		}

		//if (np->record_path) {
		//json_add_child_string(juser, "recordingPATH", np->record_path);
		//}

		json_add_child_string(juser, "status", np->leave_time ? "disconnected" : "connected");

		switch_time_exp_lt(&tm, (switch_time_t) conference->start_time * 1000000);
		switch_strftime_nocheck(tmp, &retsize, sizeof(tmp), fmt, &tm);
		p = end_of_p(tmpp) -1;
		snprintf(p, 4, ":00");

		json_add_child_string(juser, "joinTime", tmpp);

		snprintf(tmp, sizeof(tmp), "%u", np->id);
		json_add_child_string(juser, "memberId", tmp);

		jvars = cJSON_CreateObject();

		if (!np->member && np->var_event) {
			switch_json_add_presence_data_cols(np->var_event, jvars, "PD-");
		} else if (np->member) {
			const char *var;
			const char *prefix = NULL;
			switch_event_t *var_event = NULL;
			switch_event_header_t *hp;
			int all = 0;

			switch_channel_get_variables(channel, &var_event);

			if ((prefix = switch_event_get_header(var_event, "json_conference_var_prefix"))) {
				all = strcasecmp(prefix, "__all__");
			} else {
				prefix = "json_";
			}

			for(hp = var_event->headers; hp; hp = hp->next) {
				if (all || !strncasecmp(hp->name, prefix, strlen(prefix))) {
					json_add_child_string(jvars, hp->name, hp->value);
				}
			}

			switch_json_add_presence_data_cols(var_event, jvars, "PD-");

			switch_event_destroy(&var_event);

			if ((var = switch_channel_get_variable(channel, "rtp_use_ssrc"))) {
				json_add_child_string(juser, "rtpAudioSSRC", var);
			}

			json_add_child_string(juser, "rtpAudioDirection", conference_cdr_audio_flow(np->member));


			if (switch_channel_test_flag(channel, CF_VIDEO)) {
				if ((var = switch_channel_get_variable(channel, "rtp_use_video_ssrc"))) {
					json_add_child_string(juser, "rtpVideoSSRC", var);
				}

				json_add_child_string(juser, "rtpVideoDirection", switch_channel_test_flag(channel, CF_HOLD) ? "sendonly" : "sendrecv");
			}
		}

		if (jvars) {
			json_add_child_obj(juser, "variables", jvars);
		}

		cJSON_AddItemToArray(np->leave_time ? jold_users : jusers, juser);

		switch_safe_free(user_uri);
	}

	switch_mutex_unlock(conference->member_mutex);

	switch_safe_free(dup_domain);
	switch_safe_free(uri);

	return json;
}

void conference_cdr_del(conference_member_t *member)
{
	if (member->channel) {
		switch_channel_get_variables(member->channel, &member->cdr_node->var_event);
	}
	if (member->cdr_node) {
		member->cdr_node->leave_time = switch_epoch_time_now(NULL);
		memcpy(member->cdr_node->mflags, member->flags, sizeof(member->flags));
		member->cdr_node->member = NULL;
	}
}

void conference_cdr_add(conference_member_t *member)
{
	conference_cdr_node_t *np;
	switch_caller_profile_t *cp;
	switch_channel_t *channel;

	np = switch_core_alloc(member->conference->pool, sizeof(*np));

	np->next = member->conference->cdr_nodes;
	member->conference->cdr_nodes = member->cdr_node = np;
	member->cdr_node->join_time = switch_epoch_time_now(NULL);
	member->cdr_node->member = member;

	if (!member->session) {
		member->cdr_node->record_path = switch_core_strdup(member->conference->pool, member->rec_path);
		return;
	}

	channel = switch_core_session_get_channel(member->session);

	if (!(cp = switch_channel_get_caller_profile(channel))) {
		return;
	}

	member->cdr_node->cp = switch_caller_profile_dup(member->conference->pool, cp);

	member->cdr_node->id = member->id;



}

void conference_cdr_rejected(conference_obj_t *conference, switch_channel_t *channel, cdr_reject_reason_t reason)
{
	conference_cdr_reject_t *rp;
	switch_caller_profile_t *cp;

	rp = switch_core_alloc(conference->pool, sizeof(*rp));

	rp->next = conference->cdr_rejected;
	conference->cdr_rejected = rp;
	rp->reason = reason;
	rp->reject_time = switch_epoch_time_now(NULL);

	if (!(cp = switch_channel_get_caller_profile(channel))) {
		return;
	}

	rp->cp = switch_caller_profile_dup(conference->pool, cp);
}

void conference_cdr_render(conference_obj_t *conference)
{
	switch_xml_t cdr, x_ptr, x_member, x_members, x_conference, x_cp, x_flags, x_tag, x_rejected, x_attempt;
	conference_cdr_node_t *np;
	conference_cdr_reject_t *rp;
	int cdr_off = 0, conference_off = 0;
	char str[512];
	char *path = NULL, *xml_text;
	int fd;

	if (zstr(conference->log_dir) && (conference->cdr_event_mode == CDRE_NONE)) return;

	if (!conference->cdr_nodes && !conference->cdr_rejected) return;

	if (!(cdr = switch_xml_new("cdr"))) {
		abort();
	}

	if (!(x_conference = switch_xml_add_child_d(cdr, "conference", cdr_off++))) {
		abort();
	}

	if (!(x_ptr = switch_xml_add_child_d(x_conference, "name", conference_off++))) {
		abort();
	}
	switch_xml_set_txt_d(x_ptr, conference->name);

	if (!(x_ptr = switch_xml_add_child_d(x_conference, "hostname", conference_off++))) {
		abort();
	}
	switch_xml_set_txt_d(x_ptr, switch_core_get_hostname());

	if (!(x_ptr = switch_xml_add_child_d(x_conference, "rate", conference_off++))) {
		abort();
	}
	switch_snprintf(str, sizeof(str), "%d", conference->rate);
	switch_xml_set_txt_d(x_ptr, str);

	if (!(x_ptr = switch_xml_add_child_d(x_conference, "interval", conference_off++))) {
		abort();
	}
	switch_snprintf(str, sizeof(str), "%d", conference->interval);
	switch_xml_set_txt_d(x_ptr, str);


	if (!(x_ptr = switch_xml_add_child_d(x_conference, "start_time", conference_off++))) {
		abort();
	}
	switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
	switch_snprintf(str, sizeof(str), "%ld", (long)conference->start_time);
	switch_xml_set_txt_d(x_ptr, str);


	if (!(x_ptr = switch_xml_add_child_d(x_conference, "end_time", conference_off++))) {
		abort();
	}
	switch_xml_set_attr_d(x_ptr, "endconference_forced", conference_utils_test_flag(conference, CFLAG_ENDCONF_FORCED) ? "true" : "false");
	switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
	switch_snprintf(str, sizeof(str), "%ld", (long)conference->end_time);
	switch_xml_set_txt_d(x_ptr, str);



	if (!(x_members = switch_xml_add_child_d(x_conference, "members", conference_off++))) {
		abort();
	}

	for (np = conference->cdr_nodes; np; np = np->next) {
		int member_off = 0;
		int flag_off = 0;


		if (!(x_member = switch_xml_add_child_d(x_members, "member", conference_off++))) {
			abort();
		}

		switch_xml_set_attr_d(x_member, "type", np->cp ? "caller" : "recording_node");

		if (!(x_ptr = switch_xml_add_child_d(x_member, "join_time", member_off++))) {
			abort();
		}
		switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
		switch_snprintf(str, sizeof(str), "%ld", (long) np->join_time);
		switch_xml_set_txt_d(x_ptr, str);


		if (!(x_ptr = switch_xml_add_child_d(x_member, "leave_time", member_off++))) {
			abort();
		}
		switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
		switch_snprintf(str, sizeof(str), "%ld", (long) np->leave_time);
		switch_xml_set_txt_d(x_ptr, str);

		if (np->cp) {
			x_flags = switch_xml_add_child_d(x_member, "flags", member_off++);
			switch_assert(x_flags);

			x_tag = switch_xml_add_child_d(x_flags, "is_moderator", flag_off++);
			switch_xml_set_txt_d(x_tag, conference_cdr_test_mflag(np, MFLAG_MOD) ? "true" : "false");

			x_tag = switch_xml_add_child_d(x_flags, "end_conference", flag_off++);
			switch_xml_set_txt_d(x_tag, conference_cdr_test_mflag(np, MFLAG_ENDCONF) ? "true" : "false");

			x_tag = switch_xml_add_child_d(x_flags, "was_kicked", flag_off++);
			switch_xml_set_txt_d(x_tag, conference_cdr_test_mflag(np, MFLAG_KICKED) ? "true" : "false");

			x_tag = switch_xml_add_child_d(x_flags, "is_ghost", flag_off++);
			switch_xml_set_txt_d(x_tag, conference_cdr_test_mflag(np, MFLAG_GHOST) ? "true" : "false");

			if (!(x_cp = switch_xml_add_child_d(x_member, "caller_profile", member_off++))) {
				abort();
			}
			switch_ivr_set_xml_profile_data(x_cp, np->cp, 0);
		}

		if (!zstr(np->record_path)) {
			if (!(x_ptr = switch_xml_add_child_d(x_member, "record_path", member_off++))) {
				abort();
			}
			switch_xml_set_txt_d(x_ptr, np->record_path);
		}


	}

	if (!(x_rejected = switch_xml_add_child_d(x_conference, "rejected", conference_off++))) {
		abort();
	}

	for (rp = conference->cdr_rejected; rp; rp = rp->next) {
		int attempt_off = 0;
		int tag_off = 0;

		if (!(x_attempt = switch_xml_add_child_d(x_rejected, "attempt", attempt_off++))) {
			abort();
		}

		if (!(x_ptr = switch_xml_add_child_d(x_attempt, "reason", tag_off++))) {
			abort();
		}
		if (rp->reason == CDRR_LOCKED) {
			switch_xml_set_txt_d(x_ptr, "conference_locked");
		} else if (rp->reason == CDRR_MAXMEMBERS) {
			switch_xml_set_txt_d(x_ptr, "max_members_reached");
		} else	if (rp->reason == CDRR_PIN) {
			switch_xml_set_txt_d(x_ptr, "invalid_pin");
		}

		if (!(x_ptr = switch_xml_add_child_d(x_attempt, "reject_time", tag_off++))) {
			abort();
		}
		switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
		switch_snprintf(str, sizeof(str), "%ld", (long) rp->reject_time);
		switch_xml_set_txt_d(x_ptr, str);

		if (rp->cp) {
			if (!(x_cp = switch_xml_add_child_d(x_attempt, "caller_profile", attempt_off++))) {
				abort();
			}
			switch_ivr_set_xml_profile_data(x_cp, rp->cp, 0);
		}
	}

	xml_text = switch_xml_toxml(cdr, SWITCH_TRUE);


	if (!zstr(conference->log_dir)) {
		path = switch_mprintf("%s%s%s.cdr.xml", conference->log_dir, SWITCH_PATH_SEPARATOR, conference->uuid_str);



#ifdef _MSC_VER
		if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
				int wrote;
				wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
				wrote++;
				close(fd);
				fd = -1;
			} else {
				char ebuf[512] = { 0 };
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error writing [%s][%s]\n",
								  path, switch_strerror_r(errno, ebuf, sizeof(ebuf)));
			}

			if (conference->cdr_event_mode != CDRE_NONE) {
				switch_event_t *event;

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_CDR) == SWITCH_STATUS_SUCCESS)
					//	if (switch_event_create(&event, SWITCH_EVENT_CDR) == SWITCH_STATUS_SUCCESS)
					{
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CDR-Source", CONF_EVENT_CDR);
						if (conference->cdr_event_mode == CDRE_AS_CONTENT) {
							switch_event_set_body(event, xml_text);
						} else {
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CDR-Path", path);
						}
						switch_event_fire(&event);
					} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not create CDR event");
				}
			}
		}

		switch_safe_free(path);
		switch_safe_free(xml_text);
		switch_xml_free(cdr);
	}

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
