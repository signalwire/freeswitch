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
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* Daniel Bryars <danb@aeriandi.com>
* Tim Brown <tim.brown@aeriandi.com>
* Anthony Minessale II <anthm@freeswitch.org>
* William King <william.king@quentustech.com>
* Mike Jerris <mike@jerris.com>
*
* mod_amqp.c -- Sends FreeSWITCH events to an AMQP broker
*
*/

#include "mod_amqp.h"

int mod_amqp_log_if_amqp_error(amqp_rpc_reply_t x, char const *context)
{
	switch (x.reply_type) {
	case AMQP_RESPONSE_NORMAL:
		return 0;

	case AMQP_RESPONSE_NONE:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s: missing RPC reply type!\n", context);
		break;

	case AMQP_RESPONSE_LIBRARY_EXCEPTION:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s: %s\n", context, amqp_error_string2(x.library_error));
		break;

	case AMQP_RESPONSE_SERVER_EXCEPTION:
		switch (x.reply.id) {
		case AMQP_CONNECTION_CLOSE_METHOD: {
			amqp_connection_close_t *m = (amqp_connection_close_t *) x.reply.decoded;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s: server connection error %d, message: %.*s\n",
							  context, m->reply_code, (int) m->reply_text.len, (char *) m->reply_text.bytes);
			break;
		}
		case AMQP_CHANNEL_CLOSE_METHOD: {
			amqp_channel_close_t *m = (amqp_channel_close_t *) x.reply.decoded;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s: server channel error %d, message: %.*s\n",
							  context, m->reply_code, (int) m->reply_text.len, (char *) m->reply_text.bytes);
			break;
		}
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s: unknown server error, method id 0x%08X\n", context, x.reply.id);
			break;
		}
		break;

	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s: unknown reply_type: %d \n", context, x.reply_type);
			break;
	}

	return -1;
}

int mod_amqp_count_chars(const char* string, char ch)
{
	int c = 0;
	while (*string) c += *(string++) == ch;
	return c;
}

switch_status_t mod_amqp_do_config(switch_bool_t reload)
{
	switch_xml_t cfg = NULL, xml = NULL, profiles = NULL, profile = NULL;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, reload ? "Reloading Config\n" : "Loading Config\n");

	if (!(xml = switch_xml_open_cfg("amqp.conf", &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of amqp.conf.xml failed\n");
		return SWITCH_STATUS_FALSE;
	}

	if (reload) {
		switch_hash_index_t *hi = NULL;
		mod_amqp_producer_profile_t *producer;
		mod_amqp_command_profile_t *command;
		mod_amqp_logging_profile_t *logging;

		switch_event_unbind_callback(mod_amqp_producer_event_handler);

		while ((hi = switch_core_hash_first_iter(mod_amqp_globals.producer_hash, hi))) {
			switch_core_hash_this(hi, NULL, NULL, (void **)&producer);
			mod_amqp_producer_destroy(&producer);
		}
		while ((hi = switch_core_hash_first_iter(mod_amqp_globals.command_hash, hi))) {
			switch_core_hash_this(hi, NULL, NULL, (void **)&command);
			mod_amqp_command_destroy(&command);
		}

		switch_log_unbind_logger(mod_amqp_logging_recv);

		while ((hi = switch_core_hash_first_iter(mod_amqp_globals.logging_hash, hi))) {
			switch_core_hash_this(hi, NULL, NULL, (void **)&logging);
			mod_amqp_logging_destroy(&logging);
		}
	}

	if ((profiles = switch_xml_child(cfg, "producers"))) {
		if ((profile = switch_xml_child(profiles, "profile"))) {
			for (; profile; profile = profile->next)	{
				char *name = (char *) switch_xml_attr_soft(profile, "name");

				if (zstr(name)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load mod_amqp profile. Check configs missing name attr\n");
					continue;
				}

				if ( mod_amqp_producer_create(name, profile) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load mod_amqp profile [%s]. Check configs\n", name);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loaded mod_amqp profile [%s] successfully\n", name);
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unable to locate a profile for mod_amqp\n" );
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to locate producers section for mod_amqp\n" );
	}

	if ((profiles = switch_xml_child(cfg, "commands"))) {
		if ((profile = switch_xml_child(profiles, "profile"))) {
			for (; profile; profile = profile->next)	{
				char *name = (char *) switch_xml_attr_soft(profile, "name");

				if (zstr(name)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load mod_amqp profile. Check configs missing name attr\n");
					continue;
				}
				name = switch_core_strdup(mod_amqp_globals.pool, name);

				if ( mod_amqp_command_create(name, profile) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load mod_amqp profile [%s]. Check configs\n", name);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loaded mod_amqp profile [%s] successfully\n", name);
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unable to locate a profile for mod_amqp\n" );
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to locate commands section for mod_amqp\n" );
	}

	if ((profiles = switch_xml_child(cfg, "logging"))) {
		if ((profile = switch_xml_child(profiles, "profile"))) {
			for (; profile; profile = profile->next)	{
				char *name = (char *) switch_xml_attr_soft(profile, "name");

				if (zstr(name)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load mod_amqp profile. Check configs missing name attr\n");
					continue;
				}
				name = switch_core_strdup(mod_amqp_globals.pool, name);

				if ( mod_amqp_logging_create(name, profile) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load mod_amqp profile [%s]. Check configs\n", name);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loaded mod_amqp profile [%s] successfully\n", name);
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unable to locate a profile for mod_amqp\n" );
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to locate logging section for mod_amqp\n" );
	}

	switch_xml_free(xml);
	return SWITCH_STATUS_SUCCESS;
}


#define KEY_SAFE(C) ((C >= 'a' && C <= 'z') || \
					(C >= 'A' && C <= 'Z') || \
					(C >= '0' && C <= '9') || \
					(C == '-' || C == '~' || C == '_'))

#define HI4(C) (C>>4)
#define LO4(C) (C & 0x0F)

#define hexint(C) (C < 10?('0' + C):('A'+ C - 10))

char *amqp_util_encode(char *key, char *dest) {
	char *p, *end;
 	if ((strlen(key) == 1) && (key[0] == '#' || key[0] == '*')) {
 		*dest++ = key[0];
		*dest = '\0';
		return dest;
    }
	for (p = key, end = key + strlen(key); p < end; p++) {
		if (KEY_SAFE(*p)) {
			*dest++ = *p;
		} else if (*p == '.') {
			memcpy(dest, "%2E", 3);
			dest += 3;
		} else if (*p == ' ') {
			*dest++ = '+';
		} else {
			*dest++ = '%';
			sprintf(dest, "%c%c", hexint(HI4(*p)), hexint(LO4(*p)));
			dest += 2;
		}
	}
	*dest = '\0';
	return dest;
}

void mod_amqp_util_msg_destroy(mod_amqp_message_t **msg)
{
	if (!msg || !*msg) return;
	switch_safe_free((*msg)->pjson);
	switch_safe_free(*msg);
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
