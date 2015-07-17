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


void conference_event_mod_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id)
{
	cJSON *data, *addobj = NULL;
	const char *action = NULL;
	char *value = NULL;
	cJSON *jid = 0;
	char *conference_name = strdup(event_channel + 15);
	int cid = 0;
	char *p;
	switch_stream_handle_t stream = { 0 };
	char *exec = NULL;
	cJSON *msg, *jdata, *jvalue;
	char *argv[10] = {0};
	int argc = 0;

	if (conference_name && (p = strchr(conference_name, '@'))) {
		*p = '\0';
	}

	if ((data = cJSON_GetObjectItem(json, "data"))) {
		action = cJSON_GetObjectCstr(data, "command");
		if ((jid = cJSON_GetObjectItem(data, "id"))) {
			cid = jid->valueint;
		}

		if ((jvalue = cJSON_GetObjectItem(data, "value"))) {

			if (jvalue->type == cJSON_Array) {
				int i;
				argc = cJSON_GetArraySize(jvalue);
				if (argc > 10) argc = 10;

				for (i = 0; i < argc; i++) {
					cJSON *str = cJSON_GetArrayItem(jvalue, i);
					if (str->type == cJSON_String) {
						argv[i] = str->valuestring;
					}
				}
			} else if (jvalue->type == cJSON_String) {
				value = jvalue->valuestring;
				argv[argc++] = value;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "conf %s CMD %s [%s] %d\n", conference_name, key, action, cid);

	if (zstr(action)) {
		goto end;
	}

	SWITCH_STANDARD_STREAM(stream);

	if (!strcasecmp(action, "kick") ||
		!strcasecmp(action, "mute") ||
		!strcasecmp(action, "unmute") ||
		!strcasecmp(action, "tmute") ||
		!strcasecmp(action, "vmute") ||
		!strcasecmp(action, "unvmute") ||
		!strcasecmp(action, "tvmute")
		) {
		exec = switch_mprintf("%s %s %d", conference_name, action, cid);
	} else if (!strcasecmp(action, "volume_in") ||
			   !strcasecmp(action, "volume_out") ||
			   !strcasecmp(action, "vid-res-id") ||
			   !strcasecmp(action, "vid-floor") ||
			   !strcasecmp(action, "vid-layer") ||
			   !strcasecmp(action, "vid-canvas") ||
			   !strcasecmp(action, "vid-watching-canvas") ||
			   !strcasecmp(action, "vid-banner")) {
		exec = switch_mprintf("%s %s %d %s", conference_name, action, cid, argv[0]);
	} else if (!strcasecmp(action, "play") || !strcasecmp(action, "stop")) {
		exec = switch_mprintf("%s %s %s", conference_name, action, argv[0]);
	} else if (!strcasecmp(action, "recording") || !strcasecmp(action, "vid-layout") || !strcasecmp(action, "vid-write-png")) {

		if (!argv[1]) {
			argv[1] = "all";
		}

		exec = switch_mprintf("%s %s %s %s", conference_name, action, argv[0], argv[1]);

	} else if (!strcasecmp(action, "transfer") && cid) {
		conference_member_t *member;
		conference_obj_t *conference;

		exec = switch_mprintf("%s %s %s", argv[0], switch_str_nil(argv[1]), switch_str_nil(argv[2]));
		stream.write_function(&stream, "+OK Call transferred to %s", argv[0]);

		if ((conference = conference_find(conference_name, NULL))) {
			if ((member = conference_member_get(conference, cid))) {
				switch_ivr_session_transfer(member->session, argv[0], argv[1], argv[2]);
				switch_thread_rwlock_unlock(member->rwlock);
			}
			switch_thread_rwlock_unlock(conference->rwlock);
		}
		goto end;
	} else if (!strcasecmp(action, "list-videoLayouts")) {
		switch_hash_index_t *hi;
		void *val;
		const void *vvar;
		cJSON *array = cJSON_CreateArray();
		conference_obj_t *conference = NULL;
		if ((conference = conference_find(conference_name, NULL))) {
			switch_mutex_lock(conference_globals.setup_mutex);
			if (conference->layout_hash) {
				for (hi = switch_core_hash_first(conference->layout_hash); hi; hi = switch_core_hash_next(&hi)) {
					switch_core_hash_this(hi, &vvar, NULL, &val);
					cJSON_AddItemToArray(array, cJSON_CreateString((char *)vvar));
				}
			}

			if (conference->layout_group_hash) {
				for (hi = switch_core_hash_first(conference->layout_group_hash); hi; hi = switch_core_hash_next(&hi)) {
					char *name;
					switch_core_hash_this(hi, &vvar, NULL, &val);
					name = switch_mprintf("group:%s", (char *)vvar);
					cJSON_AddItemToArray(array, cJSON_CreateString(name));
					free(name);
				}
			}

			switch_mutex_unlock(conference_globals.setup_mutex);
			switch_thread_rwlock_unlock(conference->rwlock);
		}
		addobj = array;
	}

	if (exec) {
		conference_api_main_real(exec, NULL, &stream);
	}

 end:

	msg = cJSON_CreateObject();
	jdata = json_add_child_obj(msg, "data", NULL);

	cJSON_AddItemToObject(msg, "eventChannel", cJSON_CreateString(event_channel));
	cJSON_AddItemToObject(jdata, "action", cJSON_CreateString("response"));

	if (addobj) {
		cJSON_AddItemToObject(jdata, "conf-command", cJSON_CreateString(action));
		cJSON_AddItemToObject(jdata, "response", cJSON_CreateString("OK"));
		cJSON_AddItemToObject(jdata, "responseData", addobj);
	} else if (exec) {
		cJSON_AddItemToObject(jdata, "conf-command", cJSON_CreateString(exec));
		cJSON_AddItemToObject(jdata, "response", cJSON_CreateString((char *)stream.data));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT,"RES [%s][%s]\n", exec, (char *)stream.data);
	} else {
		cJSON_AddItemToObject(jdata, "error", cJSON_CreateString("Invalid Command"));
	}

	switch_event_channel_broadcast(event_channel, &msg, __FILE__, conference_globals.event_channel_id);


	switch_safe_free(stream.data);
	switch_safe_free(exec);

	switch_safe_free(conference_name);

}

void conference_event_la_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id)
{
	switch_live_array_parse_json(json, conference_globals.event_channel_id);
}

void conference_event_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id)
{
	char *domain = NULL, *name = NULL;
	conference_obj_t *conference = NULL;
	cJSON *data, *reply = NULL, *conference_desc = NULL;
	const char *action = NULL;
	char *dup = NULL;

	if ((data = cJSON_GetObjectItem(json, "data"))) {
		action = cJSON_GetObjectCstr(data, "action");
	}

	if (!action) action = "";

	reply = cJSON_Duplicate(json, 1);
	cJSON_DeleteItemFromObject(reply, "data");

	if ((name = strchr(event_channel, '.'))) {
		dup = strdup(name + 1);
		switch_assert(dup);
		name = dup;

		if ((domain = strchr(name, '@'))) {
			*domain++ = '\0';
		}
	}

	if (!strcasecmp(action, "bootstrap")) {
		if (!zstr(name) && (conference = conference_find(name, domain))) {
			conference_desc = conference_cdr_json_render(conference, json);
		} else {
			conference_desc = cJSON_CreateObject();
			json_add_child_string(conference_desc, "conferenceDescription", "FreeSWITCH Conference");
			json_add_child_string(conference_desc, "conferenceState", "inactive");
			json_add_child_array(conference_desc, "users");
			json_add_child_array(conference_desc, "oldUsers");
		}
	} else {
		conference_desc = cJSON_CreateObject();
		json_add_child_string(conference_desc, "error", "Invalid action");
	}

	json_add_child_string(conference_desc, "action", "conferenceDescription");

	cJSON_AddItemToObject(reply, "data", conference_desc);

	switch_safe_free(dup);

	switch_event_channel_broadcast(event_channel, &reply, "mod_conference", conference_globals.event_channel_id);
}

void conference_event_send_json(conference_obj_t *conference)
{
	cJSON *event, *conference_desc = NULL;
	char *name = NULL, *domain = NULL, *dup_domain = NULL;
	char *event_channel = NULL;

	if (!conference_utils_test_flag(conference, CFLAG_JSON_EVENTS)) {
		return;
	}

	conference_desc = conference_cdr_json_render(conference, NULL);

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}

	event_channel = switch_mprintf("conference.%q@%q", name, domain);

	event = cJSON_CreateObject();

	json_add_child_string(event, "eventChannel", event_channel);
	cJSON_AddItemToObject(event, "data", conference_desc);

	switch_event_channel_broadcast(event_channel, &event, "mod_conference", conference_globals.event_channel_id);

	switch_safe_free(dup_domain);
	switch_safe_free(event_channel);
}

void conference_event_la_command_handler(switch_live_array_t *la, const char *cmd, const char *sessid, cJSON *jla, void *user_data)
{
}


void conference_event_adv_la(conference_obj_t *conference, conference_member_t *member, switch_bool_t join)
{

	//if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
	switch_channel_set_flag(member->channel, CF_VIDEO_REFRESH_REQ);
	switch_core_media_gen_key_frame(member->session);
	//}

	if (conference && conference->la && member->session &&
		!switch_channel_test_flag(member->channel, CF_VIDEO_ONLY)) {
		cJSON *msg, *data;
		const char *uuid = switch_core_session_get_uuid(member->session);
		const char *cookie = switch_channel_get_variable(member->channel, "event_channel_cookie");
		const char *event_channel = cookie ? cookie : uuid;
		switch_event_t *variables;
		switch_event_header_t *hp;

		msg = cJSON_CreateObject();
		data = json_add_child_obj(msg, "pvtData", NULL);

		cJSON_AddItemToObject(msg, "eventChannel", cJSON_CreateString(event_channel));
		cJSON_AddItemToObject(msg, "eventType", cJSON_CreateString("channelPvtData"));

		cJSON_AddItemToObject(data, "action", cJSON_CreateString(join ? "conference-liveArray-join" : "conference-liveArray-part"));
		cJSON_AddItemToObject(data, "laChannel", cJSON_CreateString(conference->la_event_channel));
		cJSON_AddItemToObject(data, "laName", cJSON_CreateString(conference->la_name));
		cJSON_AddItemToObject(data, "role", cJSON_CreateString(conference_utils_member_test_flag(member, MFLAG_MOD) ? "moderator" : "participant"));
		cJSON_AddItemToObject(data, "chatID", cJSON_CreateString(conference->chat_id));
		cJSON_AddItemToObject(data, "canvasCount", cJSON_CreateNumber(conference->canvas_count));

		if (conference_utils_member_test_flag(member, MFLAG_SECOND_SCREEN)) {
			cJSON_AddItemToObject(data, "secondScreen", cJSON_CreateTrue());
		}

		if (conference_utils_member_test_flag(member, MFLAG_MOD)) {
			cJSON_AddItemToObject(data, "modChannel", cJSON_CreateString(conference->mod_event_channel));
		}

		switch_core_get_variables(&variables);
		for (hp = variables->headers; hp; hp = hp->next) {
			if (!strncasecmp(hp->name, "conference_verto_", 11)) {
				char *var = hp->name + 11;
				if (var) {
					cJSON_AddItemToObject(data, var, cJSON_CreateString(hp->value));
				}
			}
		}
		switch_event_destroy(&variables);

		switch_event_channel_broadcast(event_channel, &msg, "mod_conference", conference_globals.event_channel_id);

		if (cookie) {
			switch_event_channel_permission_modify(cookie, conference->la_event_channel, join);
			switch_event_channel_permission_modify(cookie, conference->mod_event_channel, join);
		}
	}
}

void conference_event_send_rfc(conference_obj_t *conference)
{
	switch_event_t *event;
	char *body;
	char *name = NULL, *domain = NULL, *dup_domain = NULL;

	if (!conference_utils_test_flag(conference, CFLAG_RFC4579)) {
		return;
	}

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}


	if (switch_event_create(&event, SWITCH_EVENT_CONFERENCE_DATA) == SWITCH_STATUS_SUCCESS) {
		event->flags |= EF_UNIQ_HEADERS;

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-name", name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-domain", domain);

		body = conference_cdr_rfc4579_render(conference, NULL, event);
		switch_event_add_body(event, "%s", body);
		free(body);
		switch_event_fire(&event);
	}

	switch_safe_free(dup_domain);

}


switch_status_t conference_event_add_data(conference_obj_t *conference, switch_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Size", "%u", conference->count);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Ghosts", "%u", conference->count_ghosts);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Profile-Name", conference->profile_name);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Unique-ID", conference->uuid_str);

	return status;
}

/* send a message to every member of the conference */
void conference_event_chat_message_broadcast(conference_obj_t *conference, switch_event_t *event)
{
	conference_member_t *member = NULL;
	switch_event_t *processed;

	switch_assert(conference != NULL);
	switch_event_create(&processed, SWITCH_EVENT_CHANNEL_DATA);

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (member->session && !conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
			const char *presence_id = switch_channel_get_variable(member->channel, "presence_id");
			const char *chat_proto = switch_channel_get_variable(member->channel, "chat_proto");
			switch_event_t *reply = NULL;

			if (presence_id && chat_proto) {
				if (switch_event_get_header(processed, presence_id)) {
					continue;
				}
				switch_event_dup(&reply, event);
				switch_event_add_header_string(reply, SWITCH_STACK_BOTTOM, "to", presence_id);
				switch_event_add_header_string(reply, SWITCH_STACK_BOTTOM, "conference_name", conference->name);
				switch_event_add_header_string(reply, SWITCH_STACK_BOTTOM, "conference_domain", conference->domain);

				switch_event_set_body(reply, switch_event_get_body(event));

				switch_core_chat_deliver(chat_proto, &reply);
				switch_event_add_header_string(processed, SWITCH_STACK_BOTTOM, presence_id, "true");
			}
		}
	}
	switch_event_destroy(&processed);
	switch_mutex_unlock(conference->member_mutex);
}

void conference_event_call_setup_handler(switch_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_obj_t *conference = NULL;
	char *conf = switch_event_get_header(event, "Target-Component");
	char *domain = switch_event_get_header(event, "Target-Domain");
	char *dial_str = switch_event_get_header(event, "Request-Target");
	char *dial_uri = switch_event_get_header(event, "Request-Target-URI");
	char *action = switch_event_get_header(event, "Request-Action");
	char *ext = switch_event_get_header(event, "Request-Target-Extension");
	char *ext_domain = switch_event_get_header(event, "Request-Target-Domain");
	char *full_url = switch_event_get_header(event, "full_url");
	char *call_id = switch_event_get_header(event, "Request-Call-ID");

	if (!ext) ext = dial_str;

	if (!zstr(conf) && !zstr(dial_str) && !zstr(action) && (conference = conference_find(conf, domain))) {
		switch_event_t *var_event;
		switch_event_header_t *hp;

		if (conference_utils_test_flag(conference, CFLAG_RFC4579)) {
			char *key = switch_mprintf("conference_%s_%s_%s_%s", conference->name, conference->domain, ext, ext_domain);
			char *expanded = NULL, *ostr = dial_str;;

			if (!strcasecmp(action, "call")) {
				if((conference->max_members > 0) && (conference->count >= conference->max_members)) {
					// Conference member limit has been reached; do not proceed with setup request
					status = SWITCH_STATUS_FALSE;
				} else {
					if (switch_event_create_plain(&var_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
						abort();
					}

					for(hp = event->headers; hp; hp = hp->next) {
						if (!strncasecmp(hp->name, "var_", 4)) {
							switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, hp->name + 4, hp->value);
						}
					}

					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_call_key", key);
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_destination_number", ext);

					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_invite_uri", dial_uri);

					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_track_status", "true");
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_track_call_id", call_id);
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "sip_invite_domain", domain);
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "sip_invite_contact_params", "~isfocus");

					if (!strncasecmp(ostr, "url+", 4)) {
						ostr += 4;
					} else if (!switch_true(full_url) && conference->outcall_templ) {
						if ((expanded = switch_event_expand_headers(var_event, conference->outcall_templ))) {
							ostr = expanded;
						}
					}

					status = conference_outcall_bg(conference, NULL, NULL, ostr, 60, NULL, NULL, NULL, NULL, NULL, NULL, &var_event);

					if (expanded && expanded != conference->outcall_templ) {
						switch_safe_free(expanded);
					}
				}

			} else if (!strcasecmp(action, "end")) {
				if (switch_core_session_hupall_matching_var("conference_call_key", key, SWITCH_CAUSE_NORMAL_CLEARING)) {
					conference_send_notify(conference, "SIP/2.0 200 OK\r\n", call_id, SWITCH_TRUE);
				} else {
					conference_send_notify(conference, "SIP/2.0 481 Failure\r\n", call_id, SWITCH_TRUE);
				}
				status = SWITCH_STATUS_SUCCESS;
			}

			switch_safe_free(key);
		} else { // Conference found but doesn't support referral.
			status = SWITCH_STATUS_FALSE;
		}


		switch_thread_rwlock_unlock(conference->rwlock);
	} else { // Couldn't find associated conference.  Indicate failure on refer subscription
		status = SWITCH_STATUS_FALSE;
	}

	if(status != SWITCH_STATUS_SUCCESS) {
		// Unable to setup call, need to generate final NOTIFY
		if (switch_event_create(&event, SWITCH_EVENT_CONFERENCE_DATA) == SWITCH_STATUS_SUCCESS) {
			event->flags |= EF_UNIQ_HEADERS;

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-name", conf);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-domain", domain);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-event", "refer");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", call_id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "final", "true");
			switch_event_add_body(event, "%s", "SIP/2.0 481 Failure\r\n");
			switch_event_fire(&event);
		}
	}

}

void conference_data_event_handler(switch_event_t *event)
{
	switch_event_t *revent;
	char *name = switch_event_get_header(event, "conference-name");
	char *domain = switch_event_get_header(event, "conference-domain");
	conference_obj_t *conference = NULL;
	char *body = NULL;

	if (!zstr(name) && (conference = conference_find(name, domain))) {
		if (conference_utils_test_flag(conference, CFLAG_RFC4579)) {
			switch_event_dup(&revent, event);
			revent->event_id = SWITCH_EVENT_CONFERENCE_DATA;
			revent->flags |= EF_UNIQ_HEADERS;
			switch_event_add_header(revent, SWITCH_STACK_TOP, "Event-Name", "CONFERENCE_DATA");

			body = conference_cdr_rfc4579_render(conference, event, revent);
			switch_event_add_body(revent, "%s", body);
			switch_event_fire(&revent);
			switch_safe_free(body);
		}
		switch_thread_rwlock_unlock(conference->rwlock);
	}
}


void conference_event_pres_handler(switch_event_t *event)
{
	char *to = switch_event_get_header(event, "to");
	char *domain_name = NULL;
	char *dup_to = NULL, *conference_name, *dup_conference_name = NULL;
	conference_obj_t *conference;

	if (!to || strncasecmp(to, "conf+", 5) || !strchr(to, '@')) {
		return;
	}

	if (!(dup_to = strdup(to))) {
		return;
	}


	conference_name = dup_to + 5;

	if ((domain_name = strchr(conference_name, '@'))) {
		*domain_name++ = '\0';
	}

	dup_conference_name = switch_mprintf("%q@%q", conference_name, domain_name);


	if ((conference = conference_find(conference_name, NULL)) || (conference = conference_find(dup_conference_name, NULL))) {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", conference->name, conference->domain);


			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d caller%s)", conference->count, conference->count == 1 ? "" : "s");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", conference_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", conference->count == 1 ? "early" : "confirmed");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", conference->count == 1 ? "outbound" : "inbound");
			switch_event_fire(&event);
		}
		switch_thread_rwlock_unlock(conference->rwlock);
	} else if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", conference_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", to);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Idle");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", conference_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
		switch_event_fire(&event);
	}

	switch_safe_free(dup_to);
	switch_safe_free(dup_conference_name);
}


switch_status_t chat_send(switch_event_t *message_event)
{
	char name[512] = "", *p, *lbuf = NULL;
	conference_obj_t *conference = NULL;
	switch_stream_handle_t stream = { 0 };
	const char *proto;
	const char *from;
	const char *to;
	//const char *subject;
	const char *body;
	//const char *type;
	const char *hint;

	proto = switch_event_get_header(message_event, "proto");
	from = switch_event_get_header(message_event, "from");
	to = switch_event_get_header(message_event, "to");
	body = switch_event_get_body(message_event);
	hint = switch_event_get_header(message_event, "hint");


	if ((p = strchr(to, '+'))) {
		to = ++p;
	}

	if (!body) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((p = strchr(to, '@'))) {
		switch_copy_string(name, to, ++p - to);
	} else {
		switch_copy_string(name, to, sizeof(name));
	}

	if (!(conference = conference_find(name, NULL))) {
		switch_core_chat_send_args(proto, CONF_CHAT_PROTO, to, hint && strchr(hint, '/') ? hint : from, "",
								   "Conference not active.", NULL, NULL, SWITCH_FALSE);
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_STANDARD_STREAM(stream);

	if (body != NULL && (lbuf = strdup(body))) {
		/* special case list */
		if (conference->broadcast_chat_messages) {
			conference_event_chat_message_broadcast(conference, message_event);
		} else if (switch_stristr("list", lbuf)) {
			conference_list_pretty(conference, &stream);
			/* provide help */
		} else {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	switch_safe_free(lbuf);

	if (!conference->broadcast_chat_messages) {
		switch_core_chat_send_args(proto, CONF_CHAT_PROTO, to, hint && strchr(hint, '/') ? hint : from, "", stream.data, NULL, NULL, SWITCH_FALSE);
	}

	switch_safe_free(stream.data);
	switch_thread_rwlock_unlock(conference->rwlock);

	return SWITCH_STATUS_SUCCESS;
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
