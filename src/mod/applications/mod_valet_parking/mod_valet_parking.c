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
 * Anthony Minessale II <anthm@freeswitch.org>
 * William King <william.king@quentustech.com>
 *
 * mod_valet_parking.c -- Valet Parking Module
 *
 */
#include <switch.h>
#define VALET_EVENT "valet_parking::info"
#define VALET_PROTO "park"
#define TOKEN_FREQ 5

/* Prototypes */
SWITCH_MODULE_LOAD_FUNCTION(mod_valet_parking_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_valet_parking, mod_valet_parking_load, NULL, NULL);

typedef struct {
	char ext[256];
	char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	time_t timeout;
	int bridged;
	time_t start_time;
} valet_token_t;

typedef struct {
	switch_hash_t *hash;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	time_t last_timeout_check;
	char *name;
} valet_lot_t;

static valet_lot_t globals = { 0 };


static valet_lot_t *valet_find_lot(const char *name, switch_bool_t create)
{
	valet_lot_t *lot;

	switch_mutex_lock(globals.mutex);
	lot = switch_core_hash_find(globals.hash, name);
	if (!lot && create) {
		switch_zmalloc(lot, sizeof(*lot));
		lot->name = strdup(name);
		switch_mutex_init(&lot->mutex, SWITCH_MUTEX_NESTED, globals.pool);
		switch_core_hash_init(&lot->hash, NULL);
		switch_core_hash_insert(globals.hash, name, lot);
	}
	switch_mutex_unlock(globals.mutex);
	return lot;
}

static switch_status_t valet_on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;

			if (dtmf->digit == '#') {
				return SWITCH_STATUS_BREAK;
			}
		}
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static void check_timeouts(void)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	time_t now;
	valet_lot_t *lot;
	switch_console_callback_match_t *matches = NULL;
	switch_console_callback_match_node_t *m;
	switch_hash_index_t *i_hi;
	const void *i_var;
	void *i_val;
	char *i_ext;
	valet_token_t *token;

	now = switch_epoch_time_now(NULL);

	switch_mutex_lock(globals.mutex);
	if (now - globals.last_timeout_check < TOKEN_FREQ) {
		switch_mutex_unlock(globals.mutex);
		return;
	}

	globals.last_timeout_check = now;
	for (hi = switch_hash_first(NULL, globals.hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		switch_console_push_match(&matches, (const char *) var);
	}
	switch_mutex_unlock(globals.mutex);


	if (matches) {
		for (m = matches->head; m; m = m->next) {

			lot = valet_find_lot(m->val, SWITCH_FALSE);
			switch_mutex_lock(lot->mutex);

		top:
		
			for (i_hi = switch_hash_first(NULL, lot->hash); i_hi; i_hi = switch_hash_next(i_hi)) {
				switch_hash_this(i_hi, &i_var, NULL, &i_val);
				i_ext = (char *) i_var;
				token = (valet_token_t *) i_val;

				if (token->timeout > 0 && (token->timeout < now || token->timeout == 1)) {
					switch_core_hash_delete(lot->hash, i_ext);
					switch_safe_free(token);
					goto top;
				}
			}

			switch_mutex_unlock(lot->mutex);
		}

		switch_console_free_matches(&matches);
	}

}

static int find_longest(valet_lot_t *lot, int min, int max)
{

	switch_hash_index_t *i_hi;
	const void *i_var;
	void *i_val;
	valet_token_t *token;
	int longest = 0, cur = 0, longest_ext = 0;
	time_t now = switch_epoch_time_now(NULL);

	switch_mutex_lock(lot->mutex);
	for (i_hi = switch_hash_first(NULL, lot->hash); i_hi; i_hi = switch_hash_next(i_hi)) {
		int i;
		switch_hash_this(i_hi, &i_var, NULL, &i_val);
		token = (valet_token_t *) i_val;
		cur = (now - token->start_time);
		i = atoi(token->ext);
		
		if (cur > longest && i >= min && i <= max) {
			longest = cur;
			longest_ext = i;
		}
	}
	switch_mutex_unlock(lot->mutex);

	return longest_ext;
}

static valet_token_t *next_id(switch_core_session_t *session, valet_lot_t *lot, int min, int max, int in)
{
	int i, r = 0;
	char buf[256] = "";
	valet_token_t *token;

	if (!min) {
		min = 1;
	}

	switch_mutex_lock(globals.mutex);

	if (!in) {
		int longest = find_longest(lot, min, max);
		if (longest > 0) {
			switch_snprintf(buf, sizeof(buf), "%d", longest);
			switch_mutex_lock(lot->mutex);
			token = (valet_token_t *) switch_core_hash_find(lot->hash, buf);
			switch_mutex_unlock(lot->mutex);
			if (token) {
				goto end;
			}
		}
	}

	for (i = min; (i <= max || max == 0); i++) {
		switch_snprintf(buf, sizeof(buf), "%d", i);
		switch_mutex_lock(lot->mutex);
		token = (valet_token_t *) switch_core_hash_find(lot->hash, buf);
		switch_mutex_unlock(lot->mutex);

		if ((!in && token && !token->timeout)) {
			goto end;
		}

		if (in && !token) {
			r = i;
			break;
		}
	}

	token = NULL;

	if (r) {
		switch_snprintf(buf, sizeof(buf), "%d", r);
		switch_zmalloc(token, sizeof(*token));
		switch_set_string(token->uuid, switch_core_session_get_uuid(session));
		switch_set_string(token->ext, buf);
		token->start_time = switch_epoch_time_now(NULL);
		switch_mutex_lock(lot->mutex);
		switch_core_hash_insert(lot->hash, buf, token);
		switch_mutex_unlock(lot->mutex);
	}

 end:

	switch_mutex_unlock(globals.mutex);

	return token;
}

static int valet_lot_count(valet_lot_t *lot) 
{
	switch_hash_index_t *i_hi;
	const void *i_var;
	void *i_val;
	valet_token_t *token;
	int count = 0;
	time_t now;

	now = switch_epoch_time_now(NULL);

	switch_mutex_lock(lot->mutex);
	for (i_hi = switch_hash_first(NULL, lot->hash); i_hi; i_hi = switch_hash_next(i_hi)) {
		switch_hash_this(i_hi, &i_var, NULL, &i_val);
		token = (valet_token_t *) i_val;
		if (token->timeout > 0 && (token->timeout < now || token->timeout == 1)) {
			continue;
		}
		count++;
	}	
	switch_mutex_unlock(lot->mutex);

	return count;
}

static int EC = 0;

static void valet_send_presence(const char *lot_name, valet_lot_t *lot, valet_token_t *token, switch_bool_t in)
{

	char *domain_name, *dup_lot_name = NULL, *dup_domain_name = NULL;
	switch_event_t *event;
	int count;

	
	dup_lot_name = strdup(lot_name);
	lot_name = dup_lot_name;

	if ((domain_name = strchr(dup_lot_name, '@'))) {
		*domain_name++ = '\0';
	}
	
	if (zstr(domain_name)) {
		dup_domain_name = switch_core_get_domain(SWITCH_TRUE);
		domain_name = dup_domain_name;
	}
	
	if (zstr(domain_name)) {
		domain_name = "cluecon.com";
	}

	count = valet_lot_count(lot);

	if (count > 0) {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", lot_name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", lot_name, domain_name);

			
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d caller%s)", count, count == 1 ? "" : "s");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "active");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", lot_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "confirmed");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
			switch_event_fire(&event);
		}
	} else {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", lot_name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", lot_name, domain_name);

			
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Empty");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", lot_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
			switch_event_fire(&event);
		}		
	}
	


	if (in) {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", token->ext);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", token->ext, domain_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", token->bridged == 0 ? "Holding" : "Active");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "active");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", token->ext);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "confirmed");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", token->bridged == 0 ? "outbound" : "inbound");
			switch_event_fire(&event);
		}
	} else {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", token->ext);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", token->ext, domain_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Empty");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", token->ext);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
			switch_event_fire(&event);
		}		
	}

	switch_safe_free(dup_domain_name);
	switch_safe_free(dup_lot_name);
						
}

struct read_frame_data {
	const char *dp;
	const char *exten;
	const char *context;
	long to;
};

static switch_status_t read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	struct read_frame_data *rf = (struct read_frame_data *) user_data;

	if (--rf->to <= 0) {
		rf->to = -1;
		return SWITCH_STATUS_FALSE;
	}
	
	return SWITCH_STATUS_SUCCESS;
}

#define VALET_APP_SYNTAX "<lotname> <extension>|[ask [<min>] [<max>] [<to>] [<prompt>]|auto in [min] [max]]"
SWITCH_STANDARD_APP(valet_parking_function)
{
	char *argv[6], *lbuf;
	int argc;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *event;
	char dtmf_buf[128] = "";
	int is_auto = 0, play_announce = 1;
	const char *var;
	valet_token_t *token = NULL;
	struct read_frame_data rf = { 0 };
	long to_val = 0;

	check_timeouts();

	if ((var = switch_channel_get_variable(channel, "valet_announce_slot"))) {
		play_announce = switch_true(var);
	}

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
		char *lot_name = argv[0];
		char *ext = argv[1];
		valet_lot_t *lot;
		const char *uuid;
		const char *music = "silence";
		const char *tmp = NULL;
		switch_status_t status;
		switch_input_args_t args = { 0 };
		char dbuf[10];
		char *dest;
		int in = -1;

		const char *timeout, *orbit_exten, *orbit_dialplan, *orbit_context;
		char *timeout_str = "", *orbit_exten_str = "", *orbit_dialplan_str = "", *orbit_context_str = "";

		lot = valet_find_lot(lot_name, SWITCH_TRUE);
		switch_assert(lot);

		if (!strcasecmp(ext, "auto")) {
			const char *io = argv[2];
			const char *min = argv[3];
			const char *max = argv[4];
			int min_i, max_i;

			if (argc < 5) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", VALET_APP_SYNTAX);
				return;
			}

			if (io) {
				if (!strcasecmp(io, "in")) {
					in = 1;
					is_auto = 1;
				} else if (!strcasecmp(io, "out")) {
					in = 0;
				}
			}

			if (in < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", VALET_APP_SYNTAX);
				return;
			}

			min_i = atoi(min);
			max_i = atoi(max);

			if (!(token = next_id(session, lot, min_i, max_i, in))) {
				switch_ivr_phrase_macro(session, in ? "valet_lot_full" : "valet_lot_empty", "", NULL, NULL);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s lot is %s.\n", switch_channel_get_name(channel), in ? "full" : "empty");
				return;
			}

			switch_snprintf(dtmf_buf, sizeof(dtmf_buf), "%s", token->ext);
			ext = dtmf_buf;

		} else if (!strcasecmp(ext, "ask")) {
			const char *prompt = "ivr/ivr-enter_ext_pound.wav";
			int min = 1;
			int max = 11;
			int to = 10000;
			int i;

			tmp = argv[2] ? argv[2] : switch_channel_get_variable(channel, "valet_ext_min");
			if (tmp) {
				if ((i = atoi(tmp)) > 0) {
					min = i;
				}
			}

			tmp = argv[3] ? argv[3] : switch_channel_get_variable(channel, "valet_ext_max");
			if (tmp) {
				if ((i = atoi(tmp)) > 0) {
					max = i;
				}
			}

			tmp = argv[4] ? argv[4] : switch_channel_get_variable(channel, "valet_ext_to");
			if (tmp) {
				if ((i = atoi(tmp)) > 0) {
					to = i;
				}
			}

			tmp = argv[5] ? argv[5] : switch_channel_get_variable(channel, "valet_ext_prompt");
			if (tmp) {
				prompt = tmp;
			}

			do {
				status = switch_ivr_read(session, min, max, prompt, NULL, dtmf_buf, sizeof(dtmf_buf), to, "#", 0);
			} while (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_FALSE);

			if (status == SWITCH_STATUS_SUCCESS) {
				ext = dtmf_buf;
			} else {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
		}

		if (!token || !in) {

			if (!token) {
				switch_mutex_lock(lot->mutex);
				token = (valet_token_t *) switch_core_hash_find(lot->hash, ext);
				switch_mutex_unlock(lot->mutex);
			}

			if (token && !token->bridged) {
				switch_core_session_t *b_session;
			
				if (token->timeout) {
					const char *var = switch_channel_get_variable(channel, "valet_ticket");
				
					if (!zstr(var)) {
						if (!strcmp(var, token->uuid)) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Valet ticket %s accepted.\n", var);
							token->timeout = 0;
							switch_channel_set_variable(channel, "valet_ticket", NULL);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid token %s\n", token->uuid);
							switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
							return;
						}
					}
				}

				if (!zstr(token->uuid) && (b_session = switch_core_session_locate(token->uuid))) {
					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VALET_EVENT) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Lot-Name", lot_name);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Extension", ext);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "bridge");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-To-UUID", switch_core_session_get_uuid(session));
						switch_channel_event_set_data(switch_core_session_get_channel(b_session), event);
						switch_event_fire(&event);
						switch_core_session_rwunlock(b_session);
						token->timeout = 0;
						token->bridged = 1;
						
						switch_ivr_uuid_bridge(token->uuid, switch_core_session_get_uuid(session));

						return;
					}
				}
			}

			if (token) {
				switch_mutex_lock(lot->mutex);
				switch_core_hash_delete(lot->hash, token->ext);
				switch_mutex_unlock(lot->mutex);
				memset(token, 0, sizeof(*token));
			} else {
				switch_zmalloc(token, sizeof(*token));
			}
			switch_set_string(token->uuid, switch_core_session_get_uuid(session));
			switch_set_string(token->ext, ext);
			token->start_time = switch_epoch_time_now(NULL);
			switch_mutex_lock(lot->mutex);
			switch_core_hash_insert(lot->hash, ext, token);
			switch_mutex_unlock(lot->mutex);
		}

		if (!(tmp = switch_channel_get_variable(channel, "valet_hold_music"))) {
			tmp = switch_channel_get_hold_music(channel);
		}

		if (tmp) {
			music = tmp;
		}

		if (!strcasecmp(music, "silence")) {
			music = "silence_stream://-1";
		}
		
		if ((orbit_exten = switch_channel_get_variable(channel, "valet_parking_orbit_exten"))) {
			orbit_exten_str = switch_core_session_sprintf(session, "set:valet_parking_orbit_exten=%s,", orbit_exten);
		}

		if ((orbit_dialplan = switch_channel_get_variable(channel, "valet_parking_orbit_dialplan"))) {
			orbit_dialplan_str = switch_core_session_sprintf(session, "set:valet_parking_orbit_dialplan=%s,", orbit_dialplan);
		}

		if ((orbit_context = switch_channel_get_variable(channel, "valet_parking_orbit_context"))) {
			orbit_context_str = switch_core_session_sprintf(session, "set:valet_parking_orbit_context=%s,", orbit_context);
		}

		if ((timeout = switch_channel_get_variable(channel, "valet_parking_timeout"))) {
			timeout_str = switch_core_session_sprintf(session, "set:valet_parking_timeout=%s,", timeout);			
		}

		dest = switch_core_session_sprintf(session, "%s%s%s%s"
										   "set:valet_ticket=%s,set:valet_hold_music=%s,sleep:1000,valet_park:%s %s", 
										   timeout_str,
										   orbit_exten_str,
										   orbit_dialplan_str,
										   orbit_context_str,
										   token->uuid, music, lot_name, ext);
		switch_channel_set_variable(channel, "inline_destination", dest);

		if (is_auto) {
			char tmp[512] = "";
			switch_snprintf(tmp, sizeof(tmp), "%s:%s", lot_name, ext);

			if ((uuid = switch_channel_get_partner_uuid(channel))) {
				switch_core_session_t *b_session;

				if ((b_session = switch_core_session_locate(uuid))) {
					token->timeout = switch_epoch_time_now(NULL) + TOKEN_FREQ;		
					if (play_announce) {
						switch_ivr_sleep(session, 1500, SWITCH_TRUE, NULL);
						switch_ivr_phrase_macro(session, "valet_announce_ext", tmp, NULL, NULL);
					}
					switch_ivr_session_transfer(b_session, dest, "inline", NULL);
					switch_core_session_rwunlock(b_session);
					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
					goto end;
				}
			}

			if (play_announce) {
				switch_ivr_sleep(session, 1500, SWITCH_TRUE, NULL);
				switch_ivr_phrase_macro(session, "valet_announce_ext", tmp, NULL, NULL);
			}
		}


		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VALET_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Lot-Name", lot_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Extension", ext);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "hold");
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}

		switch_channel_set_variable(channel, "valet_lot_extension", ext);

		valet_send_presence(lot_name, lot, token, SWITCH_TRUE);

		if ((rf.exten = switch_channel_get_variable(channel, "valet_parking_orbit_exten"))) {
			to_val = 60;
		}

		if ((var = switch_channel_get_variable(channel, "valet_parking_timeout"))) {
			long tmp = atol(var);

			if (tmp > 0) {
				to_val = tmp;
			}
		}
	
		if (to_val) {
			switch_codec_implementation_t read_impl;
			switch_core_session_get_read_impl(session, &read_impl);
			
			rf.to = (1000 / (read_impl.microseconds_per_packet / 1000)) * to_val;
			rf.dp = switch_channel_get_variable(channel, "valet_parking_orbit_dialplan");
			rf.context = switch_channel_get_variable(channel, "valet_parking_orbit_context");
		}


		args.input_callback = valet_on_dtmf;
		args.buf = dbuf;
		args.buflen = sizeof(dbuf);

		if (rf.to) {
			args.read_frame_callback = read_frame_callback;
			args.user_data = &rf;
		}

		while(switch_channel_ready(channel)) {
			switch_status_t pstatus = switch_ivr_play_file(session, NULL, music, &args);

			if (rf.to == -1) {
				if (!zstr(rf.exten)) {
					switch_ivr_session_transfer(session, rf.exten, rf.dp, rf.context);
				}
				break;
			}

			if (pstatus == SWITCH_STATUS_BREAK || pstatus == SWITCH_STATUS_TIMEOUT) {
				break;
			}
		}

		if (token) {
			token->timeout = 1;
			valet_send_presence(lot_name, lot, token, SWITCH_FALSE);
			token = NULL;
		}


		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VALET_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Lot-Name", lot_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Extension", ext);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "exit");
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", VALET_APP_SYNTAX);
	}

 end:

	if (token) {
		token->timeout = 1;
	}

}

SWITCH_STANDARD_API(valet_info_function)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	char *name;
	valet_lot_t *lot;

	stream->write_function(stream, "<lots>\n");

	switch_mutex_lock(globals.mutex);
	for (hi = switch_hash_first(NULL, globals.hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_index_t *i_hi;
		const void *i_var;
		void *i_val;
		char *i_ext;

		switch_hash_this(hi, &var, NULL, &val);
		name = (char *) var;
		lot = (valet_lot_t *) val;

		if (!zstr(cmd) && strcasecmp(cmd, name))
			continue;

		stream->write_function(stream, "  <lot name=\"%s\">\n", name);

		switch_mutex_lock(lot->mutex);
		for (i_hi = switch_hash_first(NULL, lot->hash); i_hi; i_hi = switch_hash_next(i_hi)) {
			valet_token_t *token;

			switch_hash_this(i_hi, &i_var, NULL, &i_val);
			i_ext = (char *) i_var;
			token = (valet_token_t *) i_val;

			if (!token->timeout) {
				stream->write_function(stream, "    <extension uuid=\"%s\">%s</extension>\n", token->uuid, i_ext);
			}
		}
		switch_mutex_unlock(lot->mutex);

		stream->write_function(stream, "  </lot>\n");
	}

	stream->write_function(stream, "</lots>\n");

	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}


static void pres_event_handler(switch_event_t *event)
{
	char *to = switch_event_get_header(event, "to");
	char *dup_to = NULL, *lot_name, *dup_lot_name = NULL, *domain_name;
	valet_lot_t *lot;
	int found = 0;
	
	if (!to || strncasecmp(to, "park+", 5) || !strchr(to, '@')) {
		return;
	}

	if (!(dup_to = strdup(to))) {
		return;
	}

	lot_name = dup_to + 5;

	if ((domain_name = strchr(lot_name, '@'))) {
		*domain_name++ = '\0';
	}

	dup_lot_name = switch_mprintf("%q@%q", lot_name, domain_name);

	if ((lot = valet_find_lot(lot_name, SWITCH_FALSE)) || (dup_lot_name && (lot = valet_find_lot(dup_lot_name, SWITCH_FALSE)))) {
		int count = valet_lot_count(lot);

		if (count) {
			if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
				if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", lot_name);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", lot_name, domain_name);

					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d caller%s)", count, count == 1 ? "" : "s");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "active");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", lot_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "confirmed");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
					switch_event_fire(&event);
				}
				found++;
			}
		} else {
			if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", lot_name, domain_name);

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Empty");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", lot_name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
				switch_event_fire(&event);
			}		
		}
	} else {
		switch_console_callback_match_t *matches = NULL;
		switch_console_callback_match_node_t *m;
		switch_hash_index_t *hi;
		const void *var;
		void *val;
		const char *nvar;

		switch_mutex_lock(globals.mutex);
		for (hi = switch_hash_first(NULL, globals.hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			nvar = (const char *) var;

			if (!strchr(nvar, '@') || switch_stristr(domain_name, nvar)) {
				switch_console_push_match(&matches, nvar);
			}
		}
		switch_mutex_unlock(globals.mutex);		
		
		if (matches) {
			valet_token_t *token;
			
			for (m = matches->head; !found && m; m = m->next) {
				lot = valet_find_lot(m->val, SWITCH_FALSE);
				switch_mutex_lock(lot->mutex);

				if ((token = (valet_token_t *) switch_core_hash_find(lot->hash, lot_name)) && !token->timeout) {
					found++;
					
					if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", lot_name);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", lot_name, domain_name);

						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", token->bridged == 0 ? "Holding" : "Active");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", lot_name);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", token->bridged == 0 ? "early" : "confirmed");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", token->bridged == 0 ? "outbound" : "inbound");
						switch_event_fire(&event);
					}
				}

				switch_mutex_unlock(lot->mutex);
			}
			switch_console_free_matches(&matches);
		}
	}


	if (!found && switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VALET_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", lot_name);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", lot_name, domain_name);

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Empty");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", lot_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
		switch_event_fire(&event);
	}

	switch_safe_free(dup_to);
	switch_safe_free(dup_lot_name);
}

/* Macro expands to: switch_status_t mod_valet_parking_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_valet_parking_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	/* create/register custom event message type */
	if (switch_event_reserve_subclass(VALET_EVENT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", VALET_EVENT);
		return SWITCH_STATUS_TERM;
	}

	switch_event_bind(modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL);

	memset(&globals, 0, sizeof(globals));

	globals.pool = pool;
	switch_core_hash_init(&globals.hash, NULL);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "valet_park", "valet_park", "valet_park", valet_parking_function, VALET_APP_SYNTAX, SAF_NONE);
	SWITCH_ADD_API(api_interface, "valet_info", "Valet Parking Info", valet_info_function, "[<lot name>]");

	return SWITCH_STATUS_NOUNLOAD;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
