/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * mod_valet_parking.c -- Valet Parking Module
 *
 */
#include <switch.h>
#define VALET_EVENT "valet_parking::info"
/* Prototypes */
SWITCH_MODULE_LOAD_FUNCTION(mod_valet_parking_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_valet_parking, mod_valet_parking_load, NULL, NULL);

typedef struct {
	switch_hash_t *hash;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
} valet_lot_t;

static valet_lot_t globals = { 0 };


static valet_lot_t *valet_find_lot(const char *name)
{
	valet_lot_t *lot;

	switch_mutex_lock(globals.mutex);
	if (!(lot = switch_core_hash_find(globals.hash, name))) {
		switch_zmalloc(lot, sizeof(*lot));
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



static int next_id(valet_lot_t *lot, int min, int max, int in)
{
	int i, r = 0, m;
	char buf[128] = "";

	if (!min)
		min = 1;

	switch_mutex_lock(globals.mutex);
	for (i = min; (i < max || max == 0); i++) {
		switch_snprintf(buf, sizeof(buf), "%d", i);
		m = !!switch_core_hash_find(lot->hash, buf);

		if ((in && !m) || (!in && m)) {
			r = i;
			break;
		}
	}
	switch_mutex_unlock(globals.mutex);

	return r;
}


#define VALET_APP_SYNTAX "<lotname> <extension>|[ask [<min>] [<max>] [<to>] [<prompt>]|auto in [min] [max]]"
SWITCH_STANDARD_APP(valet_parking_function)
{
	char *argv[6], *lbuf;
	int argc;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *event;
	char dtmf_buf[128] = "";
	int is_auto = 0;

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

		lot = valet_find_lot(lot_name);
		switch_assert(lot);

		if (!strcasecmp(ext, "auto")) {
			const char *io = argv[2];
			const char *min = argv[3];
			const char *max = argv[4];
			int min_i, max_i, id, in = -1;

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
				switch_mutex_unlock(lot->mutex);
				return;
			}

			min_i = atoi(min);
			max_i = atoi(max);

			if (!(id = next_id(lot, min_i, max_i, in))) {
				switch_ivr_phrase_macro(session, in ? "valet_lot_full" : "valet_lot_empty", "", NULL, NULL);
				switch_mutex_unlock(lot->mutex);
				return;
			}

			switch_snprintf(dtmf_buf, sizeof(dtmf_buf), "%d", id);
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
				status = switch_ivr_read(session, min, max, prompt, NULL, dtmf_buf, sizeof(dtmf_buf), to, "#");
			} while (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_FALSE);

			if (status == SWITCH_STATUS_SUCCESS) {
				ext = dtmf_buf;
			} else {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
		}

		switch_mutex_lock(lot->mutex);
		if ((uuid = switch_core_hash_find(lot->hash, ext))) {
			switch_core_session_t *b_session;
			if ((b_session = switch_core_session_locate(uuid))) {
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VALET_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Lot-Name", lot_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Extension", ext);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "bridge");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-To-UUID", switch_core_session_get_uuid(session));
					switch_channel_event_set_data(switch_core_session_get_channel(b_session), event);
					switch_event_fire(&event);
					switch_core_session_rwunlock(b_session);

					switch_ivr_uuid_bridge(switch_core_session_get_uuid(session), uuid);
					switch_mutex_unlock(lot->mutex);
					return;
				}
			}
		}

		dest = switch_core_session_sprintf(session, "sleep:1000,valet_park:%s %s", lot_name, ext);
		switch_channel_set_variable(channel, "inline_destination", dest);

		if (is_auto) {
			char tmp[512] = "";
			switch_snprintf(tmp, sizeof(tmp), "%s:%s", lot_name, ext);

			if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
				switch_core_session_t *b_session;

				if ((b_session = switch_core_session_locate(uuid))) {
					switch_ivr_phrase_macro(session, "valet_announce_ext", tmp, NULL, NULL);
					switch_ivr_session_transfer(b_session, dest, "inline", NULL);
					switch_mutex_unlock(lot->mutex);
					switch_core_session_rwunlock(b_session);
					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
					return;
				}
			}

			switch_ivr_phrase_macro(session, "valet_announce_ext", tmp, NULL, NULL);
		}


		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VALET_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Lot-Name", lot_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Valet-Extension", ext);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "hold");
			switch_event_fire(&event);
		}


		if (!(tmp = switch_channel_get_variable(channel, "valet_hold_music"))) {
			tmp = switch_channel_get_variable(channel, "hold_music");
		}
		if (tmp)
			music = tmp;

		switch_core_hash_insert(lot->hash, ext, switch_core_session_get_uuid(session));

		args.input_callback = valet_on_dtmf;
		args.buf = dbuf;
		args.buflen = sizeof(dbuf);

		switch_mutex_unlock(lot->mutex);
		if (!strcasecmp(music, "silence")) {
			switch_ivr_collect_digits_callback(session, &args, 0, 0);
		} else {
			switch_ivr_play_file(session, NULL, music, &args);
		}
		switch_mutex_lock(lot->mutex);
		switch_core_hash_delete(lot->hash, ext);
		switch_mutex_unlock(lot->mutex);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", VALET_APP_SYNTAX);
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
		char *i_uuid;

		switch_hash_this(hi, &var, NULL, &val);
		name = (char *) var;
		lot = (valet_lot_t *) val;

		if (!zstr(cmd) && strcasecmp(cmd, name))
			continue;

		stream->write_function(stream, "  <lot name=\"%s\">\n", name);

		for (i_hi = switch_hash_first(NULL, lot->hash); i_hi; i_hi = switch_hash_next(i_hi)) {
			switch_hash_this(i_hi, &i_var, NULL, &i_val);
			i_ext = (char *) i_var;
			i_uuid = (char *) i_val;
			stream->write_function(stream, "    <extension uuid=\"%s\">%s</extension>\n", i_uuid, i_ext);
		}
		stream->write_function(stream, "  </lot>\n");
	}

	stream->write_function(stream, "</lots>\n");

	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
