/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
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
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 *
 * mod_skinny.c -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#include <switch.h>
#include "mod_skinny.h"
#include "skinny_protocol.h"
#include "skinny_server.h"
#include "skinny_tables.h"
#include "skinny_api.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_skinny_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skinny_shutdown);

SWITCH_MODULE_DEFINITION(mod_skinny, mod_skinny_load, mod_skinny_shutdown, NULL);

switch_endpoint_interface_t *skinny_endpoint_interface;

skinny_globals_t globals;

/*****************************************************************************/
/* SQL TABLES */
/*****************************************************************************/
static char devices_sql[] =
"CREATE TABLE skinny_devices (\n"
"   name             VARCHAR(16),\n"
"   user_id          INTEGER,\n"
"   instance         INTEGER,\n"
"   ip               VARCHAR(15),\n"
"   type             INTEGER,\n"
"   max_streams      INTEGER,\n"
"   port             INTEGER,\n"
"   codec_string     VARCHAR(255),\n"
"   headset          INTEGER,\n"
"   handset          INTEGER,\n"
"   speaker          INTEGER\n"
");\n";

static char lines_sql[] =
"CREATE TABLE skinny_lines (\n"
"   device_name          VARCHAR(16),\n"
"   device_instance      INTEGER,\n"
"   position             INTEGER,\n"
"   line_instance        INTEGER,\n"
"   label                VARCHAR(40),\n"
"   value                VARCHAR(24),\n"
"   caller_name          VARCHAR(44),\n"
"   ring_on_idle         INTEGER,\n"
"   ring_on_active       INTEGER,\n"
"   busy_trigger         INTEGER,\n"
"   forward_all          VARCHAR(255),\n"
"   forward_busy         VARCHAR(255),\n"
"   forward_noanswer     VARCHAR(255),\n"
"   noanswer_duration    INTEGER\n"
");\n";

static char buttons_sql[] =
"CREATE TABLE skinny_buttons (\n"
"   device_name      VARCHAR(16),\n"
"   device_instance  INTEGER,\n"
"   position         INTEGER,\n"
"   type             INTEGER,\n"
"   label            VARCHAR(40),\n"
"   value            VARCHAR(255),\n"
"   settings         VARCHAR(44)\n"
");\n";

static char active_lines_sql[] =
"CREATE TABLE skinny_active_lines (\n"
"   device_name      VARCHAR(16),\n"
"   device_instance  INTEGER,\n"
"   line_instance    INTEGER,\n"
"   channel_uuid     VARCHAR(256),\n"
"   call_id          INTEGER,\n"
"   call_state       INTEGER\n"
");\n";

/*****************************************************************************/
/* TEXT FUNCTIONS */
/*****************************************************************************/
char *skinny_expand_textid(const char *str)
{
	char *tmp;
	int i;

	/* Look for \200, if found, next character indicates string id */
	char match = (char) 128;
	
	tmp = switch_mprintf("");

	if (zstr(str)) {
		return tmp;
	}

	for (i=0; i<strlen(str); i++)
	{
		char *old = tmp;

		if ( str[i] == match ) {
			if ( tmp[0] ) {
				tmp = switch_mprintf("%s [%s] ", old, skinny_textid2str(str[i+1]));
			} else {
				tmp = switch_mprintf("[%s] ", skinny_textid2str(str[i+1]));
			}
			switch_safe_free(old);
			i++;
		} else {
			tmp = switch_mprintf("%s%c", old, str[i]);
			switch_safe_free(old);
		}
	}

	return tmp;
}

/*****************************************************************************/
/* PROFILES FUNCTIONS */
/*****************************************************************************/
switch_status_t skinny_profile_dump(const skinny_profile_t *profile, switch_stream_handle_t *stream)
{
	const char *line = "=================================================================================================";
	switch_assert(profile);
	stream->write_function(stream, "%s\n", line);
	/* prefs */
	stream->write_function(stream, "Name              \t%s\n", profile->name);
	stream->write_function(stream, "Domain Name       \t%s\n", profile->domain);
	stream->write_function(stream, "IP                \t%s\n", profile->ip);
	stream->write_function(stream, "Port              \t%d\n", profile->port);
	stream->write_function(stream, "Dialplan          \t%s\n", profile->dialplan);
	stream->write_function(stream, "Context           \t%s\n", profile->context);
	stream->write_function(stream, "Patterns-Dialplan \t%s\n", profile->patterns_dialplan);
	stream->write_function(stream, "Patterns-Context  \t%s\n", profile->patterns_context);
	stream->write_function(stream, "Keep-Alive        \t%d\n", profile->keep_alive);
	stream->write_function(stream, "Date-Format       \t%s\n", profile->date_format);
	stream->write_function(stream, "DBName            \t%s\n", profile->dbname ? profile->dbname : switch_str_nil(profile->odbc_dsn));
	stream->write_function(stream, "Debug             \t%d\n", profile->debug);
	stream->write_function(stream, "Auto-Restart      \t%d\n", profile->auto_restart);
	/* stats */
	stream->write_function(stream, "CALLS-IN          \t%d\n", profile->ib_calls);
	stream->write_function(stream, "FAILED-CALLS-IN   \t%d\n", profile->ib_failed_calls);
	stream->write_function(stream, "CALLS-OUT         \t%d\n", profile->ob_calls);
	stream->write_function(stream, "FAILED-CALLS-OUT  \t%d\n", profile->ob_failed_calls);
	/* listener */
	stream->write_function(stream, "Listener-Threads  \t%d\n", profile->listener_threads);
	stream->write_function(stream, "Ext-Voicemail     \t%s\n", profile->ext_voicemail);
	stream->write_function(stream, "Ext-Redial        \t%s\n", profile->ext_redial);
	stream->write_function(stream, "%s\n", line);

	return SWITCH_STATUS_SUCCESS;
}


skinny_profile_t *skinny_find_profile(const char *profile_name)
{
	skinny_profile_t *profile;
	switch_mutex_lock(globals.mutex);
	profile = (skinny_profile_t *) switch_core_hash_find(globals.profile_hash, profile_name);
	switch_mutex_unlock(globals.mutex);
	return profile;
}

skinny_profile_t *skinny_find_profile_by_domain(const char *domain_name)
{

	switch_hash_index_t *hi;
	void *val;
	skinny_profile_t *profile = NULL, *tmp_profile;

	switch_mutex_lock(globals.mutex);
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		tmp_profile = (skinny_profile_t *) val;

		switch_mutex_lock(tmp_profile->listener_mutex);
		if (!strcmp(tmp_profile->domain, domain_name)) {
			profile = tmp_profile;
		}
		switch_mutex_unlock(tmp_profile->listener_mutex);
		if (profile) {
			break;
		}
	}
	switch_mutex_unlock(globals.mutex);

	return profile;
}

switch_status_t skinny_profile_find_listener_by_device_name(skinny_profile_t *profile, const char *device_name, listener_t **listener)
{
	listener_t *l;

	switch_mutex_lock(profile->listener_mutex);
	for (l = profile->listeners; l; l = l->next) {
		if (!strcmp(l->device_name, device_name)) {
			*listener = l;
		}
	}
	switch_mutex_unlock(profile->listener_mutex);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_profile_find_listener_by_device_name_and_instance(skinny_profile_t *profile, const char *device_name, uint32_t device_instance, listener_t **listener)
{
	listener_t *l;

	switch_mutex_lock(profile->listener_mutex);
	for (l = profile->listeners; l; l = l->next) {
		if (!strcmp(l->device_name, device_name) && (l->device_instance == device_instance)) {
			*listener = l;
		}
	}
	switch_mutex_unlock(profile->listener_mutex);

	return SWITCH_STATUS_SUCCESS;
}

struct skinny_profile_find_session_uuid_helper {
	skinny_profile_t *profile;
	char *channel_uuid;
	uint32_t line_instance;
};

int skinny_profile_find_session_uuid_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct skinny_profile_find_session_uuid_helper *helper = pArg;

	char *channel_uuid = argv[0];
	uint32_t line_instance = atoi(argv[1]);

	if(helper->channel_uuid == NULL) {
		helper->channel_uuid = switch_mprintf("%s", channel_uuid);
		helper->line_instance = line_instance;
	}

	return 0;
}

char * skinny_profile_find_session_uuid(skinny_profile_t *profile, listener_t *listener, uint32_t *line_instance_p, uint32_t call_id)
{
	struct skinny_profile_find_session_uuid_helper helper = {0};
	char *sql;
	char *device_condition = NULL;
	char *line_instance_condition = NULL;
	char *call_id_condition = NULL;

	switch_assert(profile);
	helper.profile = profile;
	helper.channel_uuid = NULL;

	if(listener) {
		device_condition = switch_mprintf("device_name='%s' AND device_instance=%d",
				listener->device_name, listener->device_instance);
	} else {
		device_condition = switch_mprintf("1=1");
	}
	switch_assert(device_condition);
	if(*line_instance_p > 0) {
		line_instance_condition = switch_mprintf("line_instance=%d", *line_instance_p);
	} else {
		line_instance_condition = switch_mprintf("1=1");
	}
	switch_assert(line_instance_condition);
	if(call_id > 0) {
		call_id_condition = switch_mprintf("call_id=%d", call_id);
	} else {
		call_id_condition = switch_mprintf("1=1");
	}
	switch_assert(call_id_condition);
	if((sql = switch_mprintf(
					"SELECT channel_uuid, line_instance "
					"FROM skinny_active_lines "
					"WHERE %s AND %s AND %s "
					"ORDER BY call_state, channel_uuid", /* off hook first */
					device_condition, line_instance_condition, call_id_condition
				))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql,
				skinny_profile_find_session_uuid_callback, &helper);
		switch_safe_free(sql);
	}
	switch_safe_free(device_condition);
	switch_safe_free(line_instance_condition);
	switch_safe_free(call_id_condition);
	*line_instance_p = helper.line_instance;
	return helper.channel_uuid;
}

#ifdef SWITCH_DEBUG_RWLOCKS
switch_core_session_t * skinny_profile_perform_find_session(skinny_profile_t *profile, listener_t *listener, uint32_t *line_instance_p, uint32_t call_id, const char *file, const char *func, int line)
#else
switch_core_session_t * skinny_profile_find_session(skinny_profile_t *profile, listener_t *listener, uint32_t *line_instance_p, uint32_t call_id)
#endif
{
	char *uuid;
	switch_core_session_t *result = NULL;
	uuid = skinny_profile_find_session_uuid(profile, listener, line_instance_p, call_id);

	if(!zstr(uuid)) {
#ifdef SWITCH_DEBUG_RWLOCKS
		result = switch_core_session_perform_locate(uuid, file, func, line);
#else
		result = switch_core_session_locate(uuid);
#endif
		if(!result) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 
					"Unable to find session %s on %s:%d, line %d\n",
					uuid, listener->device_name, listener->device_instance, *line_instance_p);
		}
		switch_safe_free(uuid);
	}
	return result;
}

/*****************************************************************************/
/* SQL FUNCTIONS */
/*****************************************************************************/
switch_cache_db_handle_t *skinny_get_db_handle(skinny_profile_t *profile)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(profile->odbc_dsn)) {
		dsn = profile->odbc_dsn;
	} else {
		dsn = profile->dbname;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;

}


switch_status_t skinny_execute_sql(skinny_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = skinny_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}

switch_bool_t skinny_execute_sql_callback(skinny_profile_t *profile, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback,
		void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = skinny_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}

/*****************************************************************************/
/* CHANNEL FUNCTIONS */
/*****************************************************************************/
void skinny_line_perform_set_state(const char *file, const char *func, int line, listener_t *listener, uint32_t line_instance, uint32_t call_id, uint32_t call_state)
{
	switch_event_t *event = NULL;
	switch_assert(listener);

	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_CALL_STATE);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Line-Instance", "%d", line_instance);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Call-Id", "%d", call_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Call-State", "%d", call_state);
	switch_event_fire(&event);
	send_call_state(listener, call_state, line_instance, call_id);

	skinny_log_l_ffl(listener, file, func, line, SWITCH_LOG_DEBUG,
			"Line %d, Call %d Change State to %s (%d)\n",
			line_instance, call_id,
			skinny_call_state2str(call_state), call_state);	
}


struct skinny_line_get_state_helper {
	uint32_t call_state;
};

int skinny_line_get_state_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct skinny_line_get_state_helper *helper = pArg;
	if (helper->call_state == -1) {
		helper->call_state = atoi(argv[0]);
	}
	return 0;
}

uint32_t skinny_line_get_state(listener_t *listener, uint32_t line_instance, uint32_t call_id)
{
	char *line_instance_condition;
	char *call_id_condition;
	char *sql;
	struct skinny_line_get_state_helper helper = {0};

	switch_assert(listener);

	if(line_instance > 0) {
		line_instance_condition = switch_mprintf("line_instance=%d", line_instance);
	} else {
		line_instance_condition = switch_mprintf("1=1");
	}
	switch_assert(line_instance_condition);
	if(call_id > 0) {
		call_id_condition = switch_mprintf("call_id=%d", call_id);
	} else {
		call_id_condition = switch_mprintf("1=1");
	}
	switch_assert(call_id_condition);

	helper.call_state = -1;
	if ((sql = switch_mprintf(
					"SELECT call_state FROM skinny_active_lines "
					"WHERE device_name='%s' AND device_instance=%d "
					"AND %s AND %s "
					"ORDER BY call_state, channel_uuid", /* off hook first */
					listener->device_name, listener->device_instance,
					line_instance_condition, call_id_condition
				 ))) {
		skinny_execute_sql_callback(listener->profile, listener->profile->sql_mutex, sql, skinny_line_get_state_callback, &helper);
		switch_safe_free(sql);
	}
	switch_safe_free(line_instance_condition);
	switch_safe_free(call_id_condition);

	return helper.call_state;
}


switch_status_t skinny_tech_set_codec(private_t *tech_pvt, int force)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int resetting = 0;

	if (!tech_pvt->iananame) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "No audio codec available\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (switch_core_codec_ready(&tech_pvt->read_codec)) {
		if (!force) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
		if (strcasecmp(tech_pvt->read_impl.iananame, tech_pvt->iananame) ||
				tech_pvt->read_impl.samples_per_second != tech_pvt->rm_rate ||
				tech_pvt->codec_ms != (uint32_t)tech_pvt->read_impl.microseconds_per_packet / 1000) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Changing Codec from %s@%dms to %s@%dms\n",
					tech_pvt->read_impl.iananame, tech_pvt->read_impl.microseconds_per_packet / 1000, 
					tech_pvt->rm_encoding, tech_pvt->codec_ms);

			switch_core_session_lock_codec_write(tech_pvt->session);
			switch_core_session_lock_codec_read(tech_pvt->session);
			resetting = 1;
			switch_core_codec_destroy(&tech_pvt->read_codec);
			switch_core_codec_destroy(&tech_pvt->write_codec);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Already using %s\n", tech_pvt->read_impl.iananame);
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
	}

	if (switch_core_codec_init(&tech_pvt->read_codec,
				tech_pvt->iananame,
				tech_pvt->rm_fmtp,
				tech_pvt->rm_rate,
				tech_pvt->codec_ms,
				1,
				SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | 0 /* TODO tech_pvt->profile->codec_flags */,
				NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (switch_core_codec_init(&tech_pvt->write_codec,
				tech_pvt->iananame,
				tech_pvt->rm_fmtp,
				tech_pvt->rm_rate,
				tech_pvt->codec_ms,
				1,
				SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | 0 /* TODO tech_pvt->profile->codec_flags */,
				NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_assert(tech_pvt->read_codec.implementation);
	switch_assert(tech_pvt->write_codec.implementation);

	tech_pvt->read_impl = *tech_pvt->read_codec.implementation;
	tech_pvt->write_impl = *tech_pvt->write_codec.implementation;

	switch_core_session_set_read_impl(tech_pvt->session, tech_pvt->read_codec.implementation);
	switch_core_session_set_write_impl(tech_pvt->session, tech_pvt->write_codec.implementation);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_assert(tech_pvt->read_codec.implementation);

		if (switch_rtp_change_interval(tech_pvt->rtp_session, 
					tech_pvt->read_impl.microseconds_per_packet,
					tech_pvt->read_impl.samples_per_packet
					) != SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);

			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			switch_goto_status(SWITCH_STATUS_FALSE, end);				
		}
	}

	tech_pvt->read_frame.rate = tech_pvt->rm_rate;
	//ms = tech_pvt->write_codec.implementation->microseconds_per_packet / 1000;

	if (!switch_core_codec_ready(&tech_pvt->read_codec)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Codec %s %s/%ld %d ms %d samples\n",
			"" /* TODO switch_channel_get_name(tech_pvt->channel)*/, tech_pvt->iananame, tech_pvt->rm_rate, tech_pvt->codec_ms,
			tech_pvt->read_impl.samples_per_packet);
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;

	tech_pvt->write_codec.agreed_pt = tech_pvt->agreed_pt;
	tech_pvt->read_codec.agreed_pt = tech_pvt->agreed_pt;

	if (force != 2) {
		switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
	}

	/* TODO
	   tech_pvt->fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->write_codec.fmtp_out);
	 */

	/* TODO
	   if (switch_rtp_ready(tech_pvt->rtp_session)) {
	   switch_rtp_set_default_payload(tech_pvt->rtp_session, tech_pvt->pt);
	   }
	 */

end:
	if (resetting) {
		switch_core_session_unlock_codec_write(tech_pvt->session);
		switch_core_session_unlock_codec_read(tech_pvt->session);
	}

	return status;
}

void tech_init(private_t *tech_pvt, skinny_profile_t *profile, switch_core_session_t *session)
{
	switch_assert(tech_pvt);
	switch_assert(session);

	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	tech_pvt->profile = profile;
	tech_pvt->call_id = ++profile->next_call_id;
	tech_pvt->party_id = tech_pvt->call_id;
	switch_core_session_set_private(session, tech_pvt);
	tech_pvt->session = session;
}

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
 */
switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL INIT\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

struct channel_on_routing_helper {
	private_t *tech_pvt;
	listener_t *listener;
	uint32_t line_instance;
};

int channel_on_routing_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct channel_on_routing_helper *helper = pArg;
	listener_t *listener = NULL;
	char *label;

	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	/* char *value = argv[5]; */
	/* char *caller_name = argv[6]; */
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	/* uint32_t call_id = atoi(argv[15]); */
	/* uint32_t call_state = atoi(argv[16]); */

	skinny_profile_find_listener_by_device_name_and_instance(helper->tech_pvt->profile, device_name, device_instance, &listener);
	if(listener) {
		if(!strcmp(device_name, helper->listener->device_name) 
				&& (device_instance == helper->listener->device_instance)
				&& (line_instance == helper->line_instance)) {/* the calling line */
			helper->tech_pvt->caller_profile->dialplan = switch_core_strdup(helper->tech_pvt->caller_profile->pool, listener->profile->dialplan);
			helper->tech_pvt->caller_profile->context = switch_core_strdup(helper->tech_pvt->caller_profile->pool, listener->profile->context);
			send_dialed_number(listener, helper->tech_pvt->caller_profile->destination_number, line_instance, helper->tech_pvt->call_id);
			skinny_line_set_state(listener, line_instance, helper->tech_pvt->call_id, SKINNY_PROCEED);
			skinny_session_send_call_info(helper->tech_pvt->session, listener, line_instance);
			skinny_session_ring_out(helper->tech_pvt->session, listener, line_instance);
		} else {
			send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_ON);
			skinny_line_set_state(listener, line_instance, helper->tech_pvt->call_id, SKINNY_IN_USE_REMOTELY);
			send_select_soft_keys(listener, line_instance, helper->tech_pvt->call_id, SKINNY_KEY_SET_IN_USE_HINT, 0xffff);

			label = skinny_textid2raw(SKINNY_TEXTID_IN_USE_REMOTE);
			send_display_prompt_status(listener, 0, label, line_instance, helper->tech_pvt->call_id);
			switch_safe_free(label);

			skinny_session_send_call_info(helper->tech_pvt->session, listener, line_instance);
		}
	}
	return 0;
}

switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
		skinny_action_t action;
		private_t *tech_pvt = switch_core_session_get_private(session);
		char *data = NULL;
		listener_t *listener = NULL;
		struct channel_on_routing_helper helper = {0};

		if(switch_test_flag(tech_pvt, TFLAG_FORCE_ROUTE)) {
			action = SKINNY_ACTION_PROCESS;
		} else {
			action = skinny_session_dest_match_pattern(session, &data);
		}
		switch(action) {
			case SKINNY_ACTION_PROCESS:
				skinny_profile_find_listener_by_device_name_and_instance(tech_pvt->profile,
						switch_channel_get_variable(channel, "skinny_device_name"),
						atoi(switch_channel_get_variable(channel, "skinny_device_instance")), &listener);
				if (listener) {
					helper.tech_pvt = tech_pvt;
					helper.listener = listener;
					helper.line_instance = atoi(switch_channel_get_variable(channel, "skinny_line_instance"));
					skinny_session_walk_lines(tech_pvt->profile, switch_core_session_get_uuid(session), channel_on_routing_callback, &helper);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Could not find listener %s:%s for Channel %s\n",
							switch_channel_get_variable(channel, "skinny_device_name"), switch_channel_get_variable(channel, "skinny_device_instance"),
							switch_channel_get_name(channel));
				}
				/* Future bridge should go straight */
				switch_set_flag_locked(tech_pvt, TFLAG_FORCE_ROUTE);
				break;
			case SKINNY_ACTION_WAIT:
				/* for now, wait forever */
				switch_channel_set_state(channel, CS_HIBERNATE);
				if (!zstr(data)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "skinny-wait doesn't support timeout yet (See #FS-477)");
				}
				break;
			case SKINNY_ACTION_DROP:
			default:
				switch_channel_hangup(channel, SWITCH_CAUSE_UNALLOCATED_NUMBER);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_on_execute(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);

	if (tech_pvt) {
		if (switch_core_codec_ready(&tech_pvt->read_codec)) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->write_codec)) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL DESTROY\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

struct channel_on_hangup_helper {
	private_t *tech_pvt;
	switch_call_cause_t cause;
};

int channel_on_hangup_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct channel_on_hangup_helper *helper = pArg;
	listener_t *listener = NULL;
	char *label;

	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	/* char *value = argv[5]; */
	/* char *caller_name = argv[6]; */
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	uint32_t call_id = atoi(argv[15]);
	uint32_t call_state = atoi(argv[16]);

	skinny_profile_find_listener_by_device_name_and_instance(helper->tech_pvt->profile, device_name, device_instance, &listener);
	if(listener) {
		if((call_state == SKINNY_PROCEED) || (call_state == SKINNY_CONNECTED)) { /* calling parties */
			send_stop_tone(listener, line_instance, call_id);
			send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_OFF); 
			send_clear_prompt_status(listener, line_instance, call_id); 
		}
		send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_OFF);
		switch (helper->cause) {
			case SWITCH_CAUSE_UNALLOCATED_NUMBER:
				send_start_tone(listener, SKINNY_TONE_REORDER, 0, line_instance, call_id);
				skinny_session_send_call_info(helper->tech_pvt->session, listener, line_instance);
				label = skinny_textid2raw(SKINNY_TEXTID_UNKNOWN_NUMBER);
				send_display_prompt_status(listener, 0, label, line_instance, call_id);
				switch_safe_free(label);
				break;
			case SWITCH_CAUSE_USER_BUSY:
				send_start_tone(listener, SKINNY_TONE_BUSYTONE, 0, line_instance, call_id);
				label = skinny_textid2raw(SKINNY_TEXTID_BUSY);
				send_display_prompt_status(listener, 0, label, line_instance, call_id);
				switch_safe_free(label);
				break;
			case SWITCH_CAUSE_NORMAL_CLEARING:
				send_clear_prompt_status(listener, line_instance, call_id);
				break;
			default:
				send_display_prompt_status(listener, 0, switch_channel_cause2str(helper->cause), line_instance, call_id);
		}
		if((call_state == SKINNY_PROCEED) || (call_state == SKINNY_RING_OUT) || (call_state == SKINNY_CONNECTED)) { /* calling parties */
			skinny_session_stop_media(helper->tech_pvt->session, listener, line_instance);
		}

		skinny_line_set_state(listener, line_instance, call_id, SKINNY_ON_HOOK);
		send_select_soft_keys(listener, line_instance, call_id, SKINNY_KEY_SET_ON_HOOK, 0xffff);
		send_define_current_time_date(listener);
		if((call_state == SKINNY_PROCEED) || (call_state == SKINNY_RING_OUT) || (call_state == SKINNY_CONNECTED)) { /* calling parties */
			// This is NOT correct, but results in slightly better behavior than before
			// leaving note here to revisit.

			//send_set_speaker_mode(listener, SKINNY_SPEAKER_OFF);
		}
		send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, call_id);
	}
	return 0;
}

switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	struct channel_on_hangup_helper helper = {0};
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_call_cause_t cause = switch_channel_get_cause(channel);
	private_t *tech_pvt = switch_core_session_get_private(session);
	char *sql;

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP [%s]\n", 
		switch_channel_get_name(channel), switch_channel_cause2str(cause));

	helper.tech_pvt= tech_pvt;
	helper.cause= cause;

	skinny_session_walk_lines(tech_pvt->profile, switch_core_session_get_uuid(session), channel_on_hangup_callback, &helper);
	if ((sql = switch_mprintf(
					"DELETE FROM skinny_active_lines WHERE channel_uuid='%s'",
					switch_core_session_get_uuid(session)
				 ))) {
		skinny_execute_sql(tech_pvt->profile, sql, tech_pvt->profile->sql_mutex);
		switch_safe_free(sql);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);

	switch (sig) {
		case SWITCH_SIG_KILL:
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			break;
		case SWITCH_SIG_BREAK:
			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				switch_rtp_break(tech_pvt->rtp_session);
			}
			break;
		default:
			break;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL KILL %d\n", switch_channel_get_name(channel), sig);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DTMF ON CALL %d [%c]\n", tech_pvt->call_id, dtmf->digit);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);

	while (!(tech_pvt->read_codec.implementation && switch_rtp_ready(tech_pvt->rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}

	tech_pvt->read_frame.datalen = 0;
	switch_set_flag_locked(tech_pvt, TFLAG_READING);

	if (switch_test_flag(tech_pvt, TFLAG_IO)) {
		switch_status_t status;

		switch_assert(tech_pvt->rtp_session != NULL);
		tech_pvt->read_frame.datalen = 0;


		while (switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->read_frame.datalen == 0) {
			tech_pvt->read_frame.flags = SFF_NONE;

			status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame, flags);
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				return SWITCH_STATUS_FALSE;
			}

			//payload = tech_pvt->read_frame.payload;

			if (switch_rtp_has_dtmf(tech_pvt->rtp_session)) {
				switch_dtmf_t dtmf = { 0 };
				switch_rtp_dequeue_dtmf(tech_pvt->rtp_session, &dtmf);
				switch_channel_queue_dtmf(channel, &dtmf);
			}


			if (tech_pvt->read_frame.datalen > 0) {
				size_t bytes = 0;
				int frames = 1;

				if (!switch_test_flag((&tech_pvt->read_frame), SFF_CNG)) {
					if ((bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_packet)) {
						frames = (tech_pvt->read_frame.datalen / bytes);
					}
					tech_pvt->read_frame.samples = (int) (frames * tech_pvt->read_codec.implementation->samples_per_packet);
				}
				break;
			}
		}
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_READING);

	if (tech_pvt->read_frame.datalen == 0) {
		*frame = NULL;
		return SWITCH_STATUS_GENERR;
	}

	*frame = &tech_pvt->read_frame;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	//switch_frame_t *pframe;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_FALSE;
	}
	switch_set_flag_locked(tech_pvt, TFLAG_WRITING);

	switch_rtp_write_frame(tech_pvt->rtp_session, frame);

	switch_clear_flag_locked(tech_pvt, TFLAG_WRITING);

	return status;

}

switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);
	listener_t *listener = NULL;

	skinny_profile_find_listener_by_device_name_and_instance(tech_pvt->profile,
			switch_channel_get_variable(channel, "skinny_device_name"),
			atoi(switch_channel_get_variable(channel, "skinny_device_instance")), &listener);
	if (listener) {
		int x = 0;
		skinny_session_start_media(session, listener, atoi(switch_channel_get_variable(channel, "skinny_line_instance")));
		/* Wait for media */
		while(!switch_test_flag(tech_pvt, TFLAG_IO)) {
			switch_cond_next();
			if (++x > 1000) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Wait tooo long to answer %s:%s\n",
						switch_channel_get_variable(channel, "skinny_device_name"), switch_channel_get_variable(channel, "skinny_device_instance"));
				return SWITCH_STATUS_FALSE;
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Unable to find listener to answer %s:%s\n",
				switch_channel_get_variable(channel, "skinny_device_name"), switch_channel_get_variable(channel, "skinny_device_instance"));
	}
	return SWITCH_STATUS_SUCCESS;
}


switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	private_t *tech_pvt = switch_core_session_get_private(session);

	switch (msg->message_id) {
		case SWITCH_MESSAGE_INDICATE_ANSWER:
			switch_clear_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
			return channel_answer_channel(session);

		case SWITCH_MESSAGE_INDICATE_DISPLAY:
			skinny_session_send_call_info_all(session);
			return SWITCH_STATUS_SUCCESS;

		case SWITCH_MESSAGE_INDICATE_PROGRESS:
			if (!switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {
				/* early media */
				switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
				return channel_answer_channel(session);
			}
			return SWITCH_STATUS_SUCCESS;

		default:
			return SWITCH_STATUS_SUCCESS;

	}

}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
 */
switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
		switch_caller_profile_t *outbound_profile,
		switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	switch_core_session_t *nsession = NULL;
	private_t *tech_pvt;

	char *profile_name, *dest;
	skinny_profile_t *profile = NULL;
	char *sql;
	char name[128];
	switch_channel_t *nchannel;
	switch_caller_profile_t *caller_profile;

	if (!outbound_profile || zstr(outbound_profile->destination_number)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Invalid Destination\n");
		goto error;
	}

	if (!(nsession = switch_core_session_request(skinny_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	if (!(tech_pvt = (struct private_object *) switch_core_session_alloc(nsession, sizeof(*tech_pvt)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error Creating Session private object\n");
		goto error;
	}

	if(!(profile_name = switch_core_session_strdup(nsession, outbound_profile->destination_number))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error Creating Session Info\n");
		goto error;
	}

	if (!(dest = strchr(profile_name, '/'))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Skinny URL. Should be skinny/<profile>/<number>.\n");
		cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
		goto error;
	}
	*dest++ = '\0';

	profile = skinny_find_profile(profile_name);
	if (!(profile = skinny_find_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Profile %s\n", profile_name);
		cause = SWITCH_CAUSE_UNALLOCATED_NUMBER;
		goto error;
	}

	snprintf(name, sizeof(name), "SKINNY/%s/%s", profile->name, dest);

	nchannel = switch_core_session_get_channel(nsession);
	switch_channel_set_name(nchannel, name);

	tech_init(tech_pvt, profile, nsession);

	caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
	switch_channel_set_caller_profile(nchannel, caller_profile);
	tech_pvt->caller_profile = caller_profile;

	if ((sql = switch_mprintf(
					"INSERT INTO skinny_active_lines "
					"(device_name, device_instance, line_instance, channel_uuid, call_id, call_state) "
					"SELECT device_name, device_instance, line_instance, '%s', %d, %d "
					"FROM skinny_lines "
					"WHERE value='%s'",
					switch_core_session_get_uuid(nsession), tech_pvt->call_id, SKINNY_ON_HOOK, dest
				 ))) {
		skinny_execute_sql(profile, sql, profile->sql_mutex);
		switch_safe_free(sql);
	}

	/* FIXME: ring_lines need BOND before switch_core_session_outgoing_channel() set it */
	if (session) {
		switch_channel_set_variable(switch_core_session_get_channel(session), SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(nsession));
		switch_channel_set_variable(nchannel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(session));
	}

	cause = skinny_ring_lines(tech_pvt, session);

	if(cause != SWITCH_CAUSE_SUCCESS) {
		goto error;
	}

	*new_session = nsession;

	/* ?? switch_channel_mark_ring_ready(channel); */

	if (switch_channel_get_state(nchannel) == CS_NEW) {
		switch_channel_set_state(nchannel, CS_INIT);
	}

	cause = SWITCH_CAUSE_SUCCESS;
	goto done;

error:
	if (nsession) {
		switch_core_session_destroy(&nsession);
	}

	if (pool) {
		*pool = NULL;
	}


done:

	if (profile) {
		if (cause == SWITCH_CAUSE_SUCCESS) {
			profile->ob_calls++;
		} else {
			profile->ob_failed_calls++;
		}
	}
	return cause;
}

switch_status_t channel_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	char *body = switch_event_get_body(event);

	if (!body) {
		body = "";
	}

	return SWITCH_STATUS_SUCCESS;
}



switch_state_handler_table_t skinny_state_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media*/ NULL,
	/*.on_hibernate*/ NULL,
	/*.on_reset*/ NULL,
	/*.on_park*/ NULL,
	/*.on_reporting*/ NULL,
	/*.on_destroy*/ channel_on_destroy

};

switch_io_routines_t skinny_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message,
	/*.receive_event */ channel_receive_event
};

/*****************************************************************************/
/* LISTENER FUNCTIONS */
/*****************************************************************************/

uint8_t listener_is_ready(listener_t *listener)
{
	return globals.running
		&& listener
		&& listener->sock
		&& switch_test_flag(listener, LFLAG_RUNNING)
		&& switch_test_flag(listener->profile, PFLAG_LISTENER_READY)
		&& !switch_test_flag(listener->profile, PFLAG_RESPAWN);
}

static void add_listener(listener_t *listener)
{
	skinny_profile_t *profile;
	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;

	switch_mutex_lock(profile->listener_mutex);
	listener->next = profile->listeners;
	profile->listeners = listener;
	switch_mutex_unlock(profile->listener_mutex);
}

static void remove_listener(listener_t *listener)
{
	listener_t *l, *last = NULL;
	skinny_profile_t *profile;
	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;

	switch_mutex_lock(profile->listener_mutex);
	for (l = profile->listeners; l; l = l->next) {
		if (l == listener) {
			if (last) {
				last->next = l->next;
			} else {
				profile->listeners = l->next;
			}
		}
		last = l;
	}
	switch_mutex_unlock(profile->listener_mutex);
}


static void walk_listeners(skinny_listener_callback_func_t callback, void *pvt)
{
	switch_hash_index_t *hi;
	void *val;
	skinny_profile_t *profile;

	/* walk listeners */
	switch_mutex_lock(globals.mutex);
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;

		profile_walk_listeners(profile, callback, pvt);
	}
	switch_mutex_unlock(globals.mutex);
}

static int flush_listener_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	char *profile_name = argv[0];
	char *value = argv[1];
	char *domain_name = argv[2];
	char *device_name = argv[3];
	char *device_instance = argv[4];

	char *token = switch_mprintf("skinny/%q/%q/%q:%q", profile_name, value, device_name, device_instance);
	switch_core_del_registration(value, domain_name, token);
	switch_safe_free(token);

	return 0;
}

static void flush_listener(listener_t *listener)
{

	if(!zstr(listener->device_name)) {
		skinny_profile_t *profile = listener->profile;
		char *sql;

		if ((sql = switch_mprintf(
						"SELECT '%q', value, '%q', '%q', '%d' "
						"FROM skinny_lines "
						"WHERE device_name='%s' AND device_instance=%d "
						"ORDER BY position",
						profile->name, profile->domain, listener->device_name, listener->device_instance,
						listener->device_name, listener->device_instance
					 ))) {
			skinny_execute_sql_callback(profile, profile->sql_mutex, sql, flush_listener_callback, NULL);
			switch_safe_free(sql);
		}

		if ((sql = switch_mprintf(
						"DELETE FROM skinny_devices "
						"WHERE name='%s' and instance=%d",
						listener->device_name, listener->device_instance))) {
			skinny_execute_sql(profile, sql, profile->sql_mutex);
			switch_safe_free(sql);
		}

		if ((sql = switch_mprintf(
						"DELETE FROM skinny_lines "
						"WHERE device_name='%s' and device_instance=%d",
						listener->device_name, listener->device_instance))) {
			skinny_execute_sql(profile, sql, profile->sql_mutex);
			switch_safe_free(sql);
		}

		if ((sql = switch_mprintf(
						"DELETE FROM skinny_buttons "
						"WHERE device_name='%s' and device_instance=%d",
						listener->device_name, listener->device_instance))) {
			skinny_execute_sql(profile, sql, profile->sql_mutex);
			switch_safe_free(sql);
		}

		strcpy(listener->device_name, "");
	}
}

static int dump_device_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_stream_handle_t *stream = (switch_stream_handle_t *) pArg;

	char *device_name = argv[0];
	char *user_id = argv[1];
	char *instance = argv[2];
	char *ip = argv[3];
	char *type = argv[4];
	char *max_streams = argv[5];
	char *port = argv[6];
	char *codec_string = argv[7];
	char *headset = argv[8];
	char *handset = argv[9];
	char *speaker = argv[10];

	const char *line = "=================================================================================================";
	stream->write_function(stream, "%s\n", line);
	stream->write_function(stream, "DeviceName    \t%s\n", switch_str_nil(device_name));
	stream->write_function(stream, "UserId        \t%s\n", user_id);
	stream->write_function(stream, "Instance      \t%s\n", instance);
	stream->write_function(stream, "IP            \t%s\n", ip);
	stream->write_function(stream, "DeviceTypeId  \t%s\n", type);
	stream->write_function(stream, "DeviceType    \t%s\n", skinny_device_type2str(atoi(type)));
	stream->write_function(stream, "MaxStreams    \t%s\n", max_streams);
	stream->write_function(stream, "Port          \t%s\n", port);
	stream->write_function(stream, "Codecs        \t%s\n", codec_string);
	stream->write_function(stream, "HeadsetId     \t%s\n", headset);
	if (headset) {
		stream->write_function(stream, "Headset       \t%s\n", skinny_accessory_state2str(atoi(headset)));
	}
	stream->write_function(stream, "HandsetId     \t%s\n", handset);
	if (handset) {
		stream->write_function(stream, "Handset       \t%s\n", skinny_accessory_state2str(atoi(handset)));
	}
	stream->write_function(stream, "SpeakerId     \t%s\n", speaker);
	if (speaker) {
		stream->write_function(stream, "Speaker       \t%s\n", skinny_accessory_state2str(atoi(speaker)));
	}
	stream->write_function(stream, "%s\n", line);

	return 0;
}

switch_status_t dump_device(skinny_profile_t *profile, const char *device_name, switch_stream_handle_t *stream)
{
	char *sql;
	if ((sql = switch_mprintf("SELECT name, user_id, instance, ip, type, max_streams, port, codec_string, headset, handset, speaker "
					"FROM skinny_devices WHERE name='%s'",
					device_name))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, dump_device_callback, stream);
		switch_safe_free(sql);
	}

	return SWITCH_STATUS_SUCCESS;
}


static void close_socket(switch_socket_t **sock, skinny_profile_t *profile)
{
	switch_mutex_lock(profile->sock_mutex);
	if (*sock) {
		switch_socket_shutdown(*sock, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(*sock);
		*sock = NULL;
	}
	switch_mutex_unlock(profile->sock_mutex);
}

switch_status_t kill_listener(listener_t *listener, void *pvt)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Killing listener %s:%d.\n",
			listener->device_name, listener->device_instance);
	switch_clear_flag(listener, LFLAG_RUNNING);
	close_socket(&listener->sock, listener->profile);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t keepalive_listener(listener_t *listener, void *pvt)
{
	skinny_profile_t *profile;
	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;

	listener->expire_time = switch_epoch_time_now(NULL)+profile->keep_alive*110/100;

	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj)
{
	listener_t *listener = (listener_t *) obj;
	switch_status_t status;
	skinny_message_t *request = NULL;
	skinny_profile_t *profile;
	int destroy_pool = 1;

	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;

	switch_mutex_lock(profile->listener_mutex);
	profile->listener_threads++;
	switch_mutex_unlock(profile->listener_mutex);

	switch_assert(listener != NULL);

#if MOD_SKINNY_NONBLOCK
	switch_socket_opt_set(listener->sock, SWITCH_SO_TCP_NODELAY, TRUE);
	switch_socket_opt_set(listener->sock, SWITCH_SO_NONBLOCK, TRUE);
#else
	switch_socket_opt_set(listener->sock, SWITCH_SO_NONBLOCK, FALSE);
	switch_socket_timeout_set(listener->sock, 5000000);
#endif
	if (listener->profile->debug > 0) {
		skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Connection Open\n");
	}

	switch_set_flag_locked(listener, LFLAG_RUNNING);
	keepalive_listener(listener, NULL);
	add_listener(listener);


	while (listener_is_ready(listener)) {
		status = skinny_read_packet(listener, &request);

		if (status != SWITCH_STATUS_SUCCESS) {
			switch(status) {
				case SWITCH_STATUS_TIMEOUT:
					skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Communication Time Out\n");

					if(listener->expire_time < switch_epoch_time_now(NULL)) {
						switch_event_t *event = NULL;
						/* skinny::expire event */
						skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_EXPIRE);
						switch_event_fire(&event);
					}
					break;
				default: 
					skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Communication Error\n");
			}
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			break;
		}
		if (!listener_is_ready(listener)) {
			break;
		}

		if (!request) {
			continue;
		}

		if (skinny_handle_request(listener, request) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			break;
		}

	}

	remove_listener(listener);

	if (listener->profile->debug > 0) {
		skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Communication Complete\n");
	}

	switch_thread_rwlock_wrlock(listener->rwlock);
	flush_listener(listener);

	if (listener->sock) {
		close_socket(&listener->sock, profile);
	}

	switch_thread_rwlock_unlock(listener->rwlock);

	if (listener->profile->debug > 0) {
		skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Communication Closed\n");
	}

	if(destroy_pool == 0) {
		goto no_destroy_pool;
	}
	if (listener->pool) {
		switch_memory_pool_t *pool = listener->pool;
		switch_core_destroy_memory_pool(&pool);
	}

no_destroy_pool:

	switch_mutex_lock(profile->listener_mutex);
	profile->listener_threads--;
	switch_mutex_unlock(profile->listener_mutex);

	return NULL;
}

/* Create a thread for the socket and launch it */
static void launch_listener_thread(listener_t *listener)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, listener->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, listener_run, listener, listener->pool);
}

static void *SWITCH_THREAD_FUNC skinny_profile_run(switch_thread_t *thread, void *obj)
{
	skinny_profile_t *profile = (skinny_profile_t *) obj;
	switch_status_t rv;
	switch_sockaddr_t *sa;
	switch_socket_t *inbound_socket = NULL;
	listener_t *listener;
	switch_memory_pool_t *tmp_pool = NULL, *listener_pool = NULL;
	uint32_t errs = 0;
	switch_sockaddr_t *local_sa = NULL;
	switch_sockaddr_t *remote_sa =NULL;

	if (switch_core_new_memory_pool(&tmp_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return NULL;
	}

new_socket:
	while(globals.running && !profile->sock) {
		char *listening_ip = NULL;
		switch_clear_flag_locked(profile, PFLAG_RESPAWN);
		rv = switch_sockaddr_info_get(&sa, profile->ip, SWITCH_UNSPEC, profile->port, 0, tmp_pool);
		if (rv)
			goto fail;
		rv = switch_socket_create(&profile->sock, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, tmp_pool);
		if (rv)
			goto sock_fail;
		rv = switch_socket_opt_set(profile->sock, SWITCH_SO_REUSEADDR, 1);
		if (rv)
			goto sock_fail;
		rv = switch_socket_bind(profile->sock, sa);
		if (rv)
			goto sock_fail;
		rv = switch_socket_listen(profile->sock, 5);
		if (rv)
			goto sock_fail;
		switch_sockaddr_ip_get(&listening_ip, sa);
		if (!profile->ip || strcmp(listening_ip, profile->ip)) {
			profile->ip = switch_core_strdup(profile->pool, listening_ip);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Socket up listening on %s:%u\n", profile->ip, profile->port);

		break;
sock_fail:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error! Could not listen on %s:%u\n", profile->ip, profile->port);
		if (profile->sock) {
			close_socket(&profile->sock, profile);
			profile->sock = NULL;
		}
		switch_yield(100000);
	}

	switch_set_flag_locked(profile, PFLAG_LISTENER_READY);

	while(globals.running) {

		if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			goto fail;
		}

		assert(profile->sock);

		if ((rv = switch_socket_accept(&inbound_socket, profile->sock, listener_pool))) {
			if (!globals.running) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
				goto end;
			} else if (switch_test_flag(profile, PFLAG_RESPAWN)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Respawn in progress. Waiting for socket to close.\n");
				while (profile->sock) {
					switch_cond_next();
				}
				goto new_socket;
			} else {
				/* I wish we could use strerror_r here but its not defined everywhere =/ */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error [%s]\n", strerror(errno));
				if (++errs > 100) {
					goto end;
				}
			}
		} else {
			errs = 0;
		}


		if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
			break;
		}

		switch_thread_rwlock_create(&listener->rwlock, listener_pool);

		listener->sock = inbound_socket;
		listener->pool = listener_pool;
		listener_pool = NULL;
		strcpy(listener->device_name, "");
		listener->profile = profile;

		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);

		switch_socket_addr_get(&remote_sa, SWITCH_TRUE, listener->sock);
		switch_get_addr(listener->remote_ip, sizeof(listener->remote_ip), remote_sa);
		listener->remote_port = switch_sockaddr_get_port(remote_sa);

		switch_socket_addr_get(&local_sa, SWITCH_FALSE, listener->sock);
		switch_get_addr(listener->local_ip, sizeof(listener->local_ip), local_sa);
		listener->local_port = switch_sockaddr_get_port(local_sa);

		launch_listener_thread(listener);

	}

end:

	close_socket(&profile->sock, profile);

	if (tmp_pool) {
		switch_core_destroy_memory_pool(&tmp_pool);
	}

	if (listener_pool) {
		switch_core_destroy_memory_pool(&listener_pool);
	}


fail:
	return NULL;
}


void launch_skinny_profile_thread(skinny_profile_t *profile) {
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, skinny_profile_run, profile, profile->pool);
}

/*****************************************************************************/
/* MODULE FUNCTIONS */
/*****************************************************************************/
switch_endpoint_interface_t *skinny_get_endpoint_interface()
{
	return skinny_endpoint_interface;
}

switch_status_t skinny_profile_respawn(skinny_profile_t *profile, int force)
{
	if (force || switch_test_flag(profile, PFLAG_SHOULD_RESPAWN)) {
		switch_clear_flag_locked(profile, PFLAG_SHOULD_RESPAWN);
		switch_set_flag_locked(profile, PFLAG_RESPAWN);
		switch_clear_flag_locked(profile, PFLAG_LISTENER_READY);
		profile_walk_listeners(profile, kill_listener, NULL);
		close_socket(&profile->sock, profile);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_profile_set(skinny_profile_t *profile, const char *var, const char *val)
{
	if (!var)
		return SWITCH_STATUS_FALSE;

	if (profile->sock && !strcasecmp(var, "odbc-dsn")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				"Skinny profile setting 'odbc-dsn' can't be changed while running\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!strcasecmp(var, "domain")) {
		profile->domain = switch_core_strdup(profile->pool, val);
	} else if (!strcasecmp(var, "ip")) {
		if (!profile->ip || strcmp(val, profile->ip)) {
			profile->ip = switch_core_strdup(profile->pool, zstr(val) ? NULL : val);
			switch_set_flag_locked(profile, PFLAG_SHOULD_RESPAWN);
		}
	} else if (!strcasecmp(var, "port")) {
		if (atoi(val) != profile->port) {
			profile->port = atoi(val);
			switch_set_flag_locked(profile, PFLAG_SHOULD_RESPAWN);
		}
	} else if (!strcasecmp(var, "patterns-dialplan")) {
		profile->patterns_dialplan = switch_core_strdup(profile->pool, val);
	} else if (!strcasecmp(var, "patterns-context")) {
		profile->patterns_context = switch_core_strdup(profile->pool, val);
	} else if (!strcasecmp(var, "dialplan")) {
		profile->dialplan = switch_core_strdup(profile->pool, val);
	} else if (!strcasecmp(var, "context")) {
		profile->context = switch_core_strdup(profile->pool, val);
	} else if (!strcasecmp(var, "keep-alive")) {
		profile->keep_alive = atoi(val);
	} else if (!strcasecmp(var, "date-format")) {
		strncpy(profile->date_format, val, 6);
	} else if (!strcasecmp(var, "odbc-dsn")) {
		if (!zstr(val)) {
			if (switch_odbc_available()) {
				profile->odbc_dsn = switch_core_strdup(profile->pool, val);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
			}
		}
	} else if (!strcasecmp(var, "debug")) {
		profile->debug = atoi(val);
	} else if (!strcasecmp(var, "auto-restart")) {
		profile->auto_restart = switch_true(val);
	} else if (!strcasecmp(var, "ext-voicemail")) {
		if (!profile->ext_voicemail || strcmp(val, profile->ext_voicemail)) {
			profile->ext_voicemail = switch_core_strdup(profile->pool, val);
		}
	} else if (!strcasecmp(var, "ext-redial")) {
		if (!profile->ext_redial || strcmp(val, profile->ext_redial)) {
			profile->ext_redial = switch_core_strdup(profile->pool, val);
		}
	} else {
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

void profile_walk_listeners(skinny_profile_t *profile, skinny_listener_callback_func_t callback, void *pvt)
{
	listener_t *l;

	switch_mutex_lock(profile->listener_mutex);
	for (l = profile->listeners; l; l = l->next) {
		callback(l, pvt);
	}
	switch_mutex_unlock(profile->listener_mutex);
}

static switch_status_t load_skinny_config(void)
{
	char *cf = "skinny.conf";
	switch_xml_t xcfg, xml, xprofiles, xprofile;
	switch_cache_db_handle_t *dbh = NULL;

	if (!(xml = switch_xml_open_cfg(cf, &xcfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_lock(globals.mutex);
	if ((xprofiles = switch_xml_child(xcfg, "profiles"))) {
		for (xprofile = switch_xml_child(xprofiles, "profile"); xprofile; xprofile = xprofile->next) {
			char *profile_name = (char *) switch_xml_attr_soft(xprofile, "name");
			switch_xml_t xsettings, xdevice_types, xsoft_key_set_sets;
			if (zstr(profile_name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						"<profile> is missing name attribute\n");
				continue;
			}
			if ((xsettings = switch_xml_child(xprofile, "settings"))) {
				switch_memory_pool_t *profile_pool = NULL;
				char dbname[256];
				skinny_profile_t *profile = NULL;
				switch_xml_t param;

				if (switch_core_new_memory_pool(&profile_pool) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
					return SWITCH_STATUS_TERM;
				}
				profile = switch_core_alloc(profile_pool, sizeof(skinny_profile_t));
				profile->pool = profile_pool;
				profile->name = switch_core_strdup(profile->pool, profile_name);
				profile->auto_restart = SWITCH_TRUE;
				switch_mutex_init(&profile->sql_mutex, SWITCH_MUTEX_NESTED, profile->pool);
				switch_mutex_init(&profile->listener_mutex, SWITCH_MUTEX_NESTED, profile->pool);
				switch_mutex_init(&profile->sock_mutex, SWITCH_MUTEX_NESTED, profile->pool);
				switch_mutex_init(&profile->flag_mutex, SWITCH_MUTEX_NESTED, profile->pool);

				for (param = switch_xml_child(xsettings, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					char *val = (char *) switch_xml_attr_soft(param, "value");

					if (skinny_profile_set(profile, var, val) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								"Unable to set skinny setting '%s'. Does it exists?\n", var);
					}
				} /* param */

				if (!profile->dialplan) {
					skinny_profile_set(profile, "dialplan","XML");
				}

				if (!profile->context) {
					skinny_profile_set(profile, "context","default");
				}

				if (!profile->patterns_dialplan) {
					skinny_profile_set(profile, "patterns-dialplan","XML");
				}

				if (!profile->patterns_context) {
					skinny_profile_set(profile, "patterns-context","skinny-patterns");
				}

				if (!profile->ext_voicemail) {
					skinny_profile_set(profile, "ext-voicemail", "vmain");
				}

				if (!profile->ext_redial) {
					skinny_profile_set(profile, "ext-redial", "redial");
				}

				if (profile->port == 0) {
					profile->port = 2000;
				}

				/* Soft Key Set Sets */
				switch_core_hash_init(&profile->soft_key_set_sets_hash, profile->pool);
				if ((xsoft_key_set_sets = switch_xml_child(xprofile, "soft-key-set-sets"))) {
					switch_xml_t xsoft_key_set_set;
					for (xsoft_key_set_set = switch_xml_child(xsoft_key_set_sets, "soft-key-set-set"); xsoft_key_set_set; xsoft_key_set_set = xsoft_key_set_set->next) {
						char *soft_key_set_set_name = (char *) switch_xml_attr_soft(xsoft_key_set_set, "name");
						if (soft_key_set_set_name) {
							switch_xml_t xsoft_key_set;
							skinny_message_t *message;
							message = switch_core_alloc(profile->pool, 12+sizeof(message->data.soft_key_set));
							message->type = SOFT_KEY_SET_RES_MESSAGE;
							message->length = 4 + sizeof(message->data.soft_key_set);
							message->data.soft_key_set.soft_key_set_offset = 0;
							message->data.soft_key_set.soft_key_set_count = 11;
							message->data.soft_key_set.total_soft_key_set_count = 11;
							for (xsoft_key_set = switch_xml_child(xsoft_key_set_set, "soft-key-set"); xsoft_key_set; xsoft_key_set = xsoft_key_set->next) {
								uint32_t soft_key_set_id;
								if ((soft_key_set_id = skinny_str2soft_key_set(switch_xml_attr_soft(xsoft_key_set, "name"))) != -1) {
									char *val =switch_core_strdup(profile->pool, switch_xml_attr_soft(xsoft_key_set, "value"));
									size_t string_len = strlen(val);
									size_t string_pos, start = 0;
									int field_no = 0;
									if (zstr(val)) {
										continue;
									}
									if (soft_key_set_id > 15) {
										switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
												"soft-key-set name '%s' is greater than 15 in soft-key-set-set '%s' in profile %s.\n",
												switch_xml_attr_soft(xsoft_key_set, "name"), soft_key_set_set_name, profile->name);
										continue;
									}
									for (string_pos = 0; string_pos <= string_len; string_pos++) {
										if ((val[string_pos] == ',') || (string_pos == string_len)) {
											val[string_pos] = '\0';
											if (field_no > 15) {
												switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
														"soft-key-set name '%s' is limited to 16 buttons in soft-key-set-set '%s' in profile %s.\n",
														switch_xml_attr_soft(xsoft_key_set, "name"), soft_key_set_set_name, profile->name);
												break;
											}
											message->data.soft_key_set.soft_key_set[soft_key_set_id].soft_key_template_index[field_no++] = skinny_str2soft_key_event(&val[start]);
											start = string_pos+1;
										}
									}
								} else {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
											"Unknown soft-key-set name '%s' in soft-key-set-set '%s' in profile %s.\n",
											switch_xml_attr_soft(xsoft_key_set, "name"), soft_key_set_set_name, profile->name);
								}
							} /* soft-key-set */
							switch_core_hash_insert(profile->soft_key_set_sets_hash, soft_key_set_set_name, message);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									"<soft-key-set-set> is missing a name attribute in profile %s.\n", profile->name);
						}
					} /* soft-key-set-set */
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							"<soft-key-set-sets> is missing in profile %s.\n", profile->name);
				} /* soft-key-set-sets */
				if (!switch_core_hash_find(profile->soft_key_set_sets_hash, "default")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							"Profile %s doesn't have a default <soft-key-set-set>. Profile ignored.\n", profile->name);
					switch_core_destroy_memory_pool(&profile_pool);
					continue;
				}


				/* Device types */
				switch_core_hash_init(&profile->device_type_params_hash, profile->pool);
				if ((xdevice_types = switch_xml_child(xprofile, "device-types"))) {
					switch_xml_t xdevice_type;
					for (xdevice_type = switch_xml_child(xdevice_types, "device-type"); xdevice_type; xdevice_type = xdevice_type->next) {
						uint32_t id = skinny_str2device_type(switch_xml_attr_soft(xdevice_type, "id"));
						if (id != 0) {
							char *id_str = switch_mprintf("%d", id);
							skinny_device_type_params_t *params = switch_core_alloc(profile->pool, sizeof(skinny_device_type_params_t));
							for (param = switch_xml_child(xdevice_type, "param"); param; param = param->next) {
								char *var = (char *) switch_xml_attr_soft(param, "name");
								char *val = (char *) switch_xml_attr_soft(param, "value");

								if (!strcasecmp(var, "firmware-version")) {
									strncpy(params->firmware_version, val, 16);
								}
							} /* param */
							switch_core_hash_insert(profile->device_type_params_hash, id_str, params);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									"Unknow device type %s in profile %s.\n", switch_xml_attr_soft(xdevice_type, "id"), profile->name);
						}
					}
				}

				/* Database */
				switch_snprintf(dbname, sizeof(dbname), "skinny_%s", profile->name);
				profile->dbname = switch_core_strdup(profile->pool, dbname);

				
				
				
				if ((dbh = skinny_get_db_handle(profile))) {
					switch_cache_db_test_reactive(dbh, "DELETE FROM skinny_devices", "DROP TABLE skinny_devices", devices_sql);
					switch_cache_db_test_reactive(dbh, "DELETE FROM skinny_lines", "DROP TABLE skinny_lines", lines_sql);
					switch_cache_db_test_reactive(dbh, "DELETE FROM skinny_buttons", "DROP TABLE skinny_buttons", buttons_sql);
					switch_cache_db_test_reactive(dbh, "DELETE FROM skinny_active_lines", "DROP TABLE skinny_active_lines", active_lines_sql);
					switch_cache_db_release_db_handle(&dbh);
				}
					
				skinny_profile_respawn(profile, 0);

				/* Register profile */
				switch_mutex_lock(globals.mutex);
				switch_core_hash_insert(globals.profile_hash, profile->name, profile);
				switch_mutex_unlock(globals.mutex);
				profile = NULL;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						"Settings are missing from profile %s.\n", profile_name);
			} /* settings */
		} /* profile */
	}
	switch_xml_free(xml);
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static void skinny_user_to_device_event_handler(switch_event_t *event)
{
	char *profile_name = switch_event_get_header_nil(event, "Skinny-Profile-Name");
	skinny_profile_t *profile;

	if ((profile = skinny_find_profile(profile_name))) {
		char *device_name = switch_event_get_header_nil(event, "Skinny-Device-Name");
		uint32_t device_instance = atoi(switch_event_get_header_nil(event, "Skinny-Station-Instance"));
		listener_t *listener = NULL;
		skinny_profile_find_listener_by_device_name_and_instance(profile, device_name, device_instance, &listener);
		if(listener) {
			uint32_t message_type = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Message-Id"));
			uint32_t application_id = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Application-Id"));
			uint32_t line_instance = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Line-Instance"));
			uint32_t call_id = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Call-Id"));
			uint32_t transaction_id = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Transaction-Id"));
			uint32_t data_length = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Data-Length"));
			uint32_t sequence_flag = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Sequence-Flag"));
			uint32_t display_priority = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Display-Priority"));
			uint32_t conference_id = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Conference-Id"));
			uint32_t app_instance_id = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-App-Instance-Id"));
			uint32_t routing_id = atoi(switch_event_get_header_nil(event, "Skinny-UserToDevice-Routing-Id"));
			char *data = switch_event_get_body(event);
			if (message_type == 0) {
				message_type = skinny_str2message_type(switch_event_get_header_nil(event, "Skinny-UserToDevice-Message-Id-String"));
			}
			switch(message_type) {
				case USER_TO_DEVICE_DATA_MESSAGE:
					data_length = strlen(data); /* we ignore data_length sent */
					send_data(listener, message_type,
							application_id, line_instance, call_id, transaction_id, data_length,
							data);
				case USER_TO_DEVICE_DATA_VERSION1_MESSAGE:
					data_length = strlen(data); /* we ignore data_length sent */
					send_extended_data(listener, message_type,
							application_id, line_instance, call_id, transaction_id, data_length,
							sequence_flag, display_priority, conference_id, app_instance_id, routing_id,
							data);
					break;
				default:
					skinny_log_l(listener, SWITCH_LOG_WARNING, "Incorrect message type %s (%d).\n", skinny_message_type2str(message_type), message_type);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
					"Device %s:%d in profile '%s' not found.\n", device_name, device_instance, profile_name);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
				"Profile '%s' not found.\n", profile_name);
	}
}

static void skinny_call_state_event_handler(switch_event_t *event)
{
	char *subclass;

	if ((subclass = switch_event_get_header_nil(event, "Event-Subclass")) && !strcasecmp(subclass, SKINNY_EVENT_CALL_STATE)) {
		char *profile_name = switch_event_get_header_nil(event, "Skinny-Profile-Name");
		char *device_name = switch_event_get_header_nil(event, "Skinny-Device-Name");
		uint32_t device_instance = atoi(switch_event_get_header_nil(event, "Skinny-Station-Instance"));
		uint32_t line_instance = atoi(switch_event_get_header_nil(event, "Skinny-Line-Instance"));
		uint32_t call_id = atoi(switch_event_get_header_nil(event, "Skinny-Call-Id"));
		uint32_t call_state = atoi(switch_event_get_header_nil(event, "Skinny-Call-State"));
		skinny_profile_t *profile;
		listener_t *listener = NULL;
		char *line_instance_condition, *call_id_condition;
		char *sql;

		if ((profile = skinny_find_profile(profile_name))) {
			skinny_profile_find_listener_by_device_name_and_instance(profile, device_name, device_instance, &listener);
			if(listener) {
				if(line_instance > 0) {
					line_instance_condition = switch_mprintf("line_instance=%d", line_instance);
				} else {
					line_instance_condition = switch_mprintf("1=1");
				}
				switch_assert(line_instance_condition);
				if(call_id > 0) {
					call_id_condition = switch_mprintf("call_id=%d", call_id);
				} else {
					call_id_condition = switch_mprintf("1=1");
				}
				switch_assert(call_id_condition);

				if ((sql = switch_mprintf(
								"UPDATE skinny_active_lines "
								"SET call_state=%d "
								"WHERE device_name='%s' AND device_instance=%d "
								"AND %s AND %s",
								call_state,
								listener->device_name, listener->device_instance,
								line_instance_condition, call_id_condition
							 ))) {
					skinny_execute_sql(listener->profile, sql, listener->profile->sql_mutex);
					switch_safe_free(sql);
				}
				switch_safe_free(line_instance_condition);
				switch_safe_free(call_id_condition);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
						"Device %s:%d in profile '%s' not found.\n", device_name, device_instance, profile_name);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
					"Profile '%s' not found.\n", profile_name);
		}
	}
}

struct skinny_message_waiting_event_handler_helper {
	skinny_profile_t *profile;
	switch_bool_t yn;
	int total_new_messages;
	int total_saved_messages;
	int total_new_urgent_messages;
	int total_saved_urgent_messages;
};

int skinny_message_waiting_event_handler_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);

	struct skinny_message_waiting_event_handler_helper *helper = pArg;
	listener_t *listener = NULL;

	skinny_profile_find_listener_by_device_name_and_instance(helper->profile,
			device_name, device_instance, &listener);

	if (listener) {
		if (helper->yn == SWITCH_TRUE) {
			char buffer[32];
			char *label;
			send_set_lamp(listener, SKINNY_BUTTON_VOICEMAIL, 0, SKINNY_LAMP_ON);

			label = skinny_textid2raw(SKINNY_TEXTID_YOU_HAVE_VOICEMAIL);
			sprintf(buffer, "%s: (%d/%d urgents)", label, helper->total_new_messages, helper->total_new_urgent_messages);
			switch_safe_free(label);

			send_display_pri_notify(listener, 5, 10, buffer);
		} else {
			send_set_lamp(listener, SKINNY_BUTTON_VOICEMAIL, 0, SKINNY_LAMP_OFF);
			send_clear_prompt_status(listener, 0, 0);
		}
	}
	return 0;
}

static void skinny_message_waiting_event_handler(switch_event_t *event)
{
	char *account, *dup_account, *yn, *host, *user, *count_str;
	char *pname = NULL;
	skinny_profile_t *profile = NULL;
	char *sql;

	if (!(account = switch_event_get_header(event, "mwi-message-account"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required Header 'MWI-Message-Account'\n");
		return;
	}

	if (!(yn = switch_event_get_header(event, "mwi-messages-waiting"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required Header 'MWI-Messages-Waiting'\n");
		return;
	}
	dup_account = strdup(account);
	switch_assert(dup_account != NULL);
	switch_split_user_domain(dup_account, &user, &host);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "MWI Event received for account %s with messages waiting %s\n", account, yn);

	if ((pname = switch_event_get_header(event, "skinny-profile"))) {
		if (!(profile = skinny_find_profile(pname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No profile %s\n", pname);
		}
	}

	if (!profile) {
		if (!host || !(profile = skinny_find_profile_by_domain(host))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find profile with domain %s\n", switch_str_nil(host));
			switch_safe_free(dup_account);
			return;
		}
	}

	count_str = switch_event_get_header(event, "mwi-voice-message");

	if ((sql = switch_mprintf(
					"SELECT device_name, device_instance FROM skinny_lines "
					"WHERE value='%s' AND line_instance=1", user))) {
		struct skinny_message_waiting_event_handler_helper helper = {0};
		helper.profile = profile;
		helper.yn = switch_true(yn);
		if (count_str) {
			sscanf(count_str,"%d/%d (%d/%d)",
					&helper.total_new_messages, &helper.total_saved_messages,
					&helper.total_new_urgent_messages, &helper.total_saved_urgent_messages);
		}
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql,  skinny_message_waiting_event_handler_callback, &helper);
		switch_safe_free(sql);
	}

	switch_safe_free(dup_account);
}


static void skinny_trap_event_handler(switch_event_t *event)
{
	const char *cond = switch_event_get_header(event, "condition");


	if (cond && !strcmp(cond, "network-address-change") && globals.auto_restart) {
		const char *old_ip4 = switch_event_get_header_nil(event, "network-address-previous-v4");
		const char *new_ip4 = switch_event_get_header_nil(event, "network-address-change-v4");
		const char *old_ip6 = switch_event_get_header_nil(event, "network-address-previous-v6");
		const char *new_ip6 = switch_event_get_header_nil(event, "network-address-change-v6");
		switch_hash_index_t *hi;
		const void *var;
		void *val;
		skinny_profile_t *profile;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "EVENT_TRAP: IP change detected\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "IP change detected [%s]->[%s] [%s]->[%s]\n", old_ip4, new_ip4, old_ip6, new_ip6);

		switch_mutex_lock(globals.mutex);
		if (globals.profile_hash) {
			for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
				switch_hash_this(hi, &var, NULL, &val);
				if ((profile = (skinny_profile_t *) val) && profile->auto_restart) {
					if (!strcmp(profile->ip, old_ip4)) {
						skinny_profile_set(profile, "ip", new_ip4);
					} else if (!strcmp(profile->ip, old_ip6)) {
						skinny_profile_set(profile, "ip", new_ip6);
					}
					skinny_profile_respawn(profile, 0);
				}
			}
		}
		switch_mutex_unlock(globals.mutex);
	}

}

/*****************************************************************************/
SWITCH_MODULE_LOAD_FUNCTION(mod_skinny_load)
{
	switch_hash_index_t *hi;
	/* globals init */
	memset(&globals, 0, sizeof(globals));

	if (switch_core_new_memory_pool(&globals.pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_core_hash_init(&globals.profile_hash, globals.pool);
	globals.running = 1;
	globals.auto_restart = SWITCH_TRUE;

	load_skinny_config();

	/* at least one profile */
	if (!switch_hash_first(NULL, globals.profile_hash)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No profile found!\n");
		return SWITCH_STATUS_TERM;
	}
	/* bind to events */
	if ((switch_event_bind_removable(modname, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_CALL_STATE, skinny_call_state_event_handler, NULL, &globals.call_state_node) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind our call_state handler!\n");
		return SWITCH_STATUS_TERM;
	}
	if ((switch_event_bind_removable(modname, SWITCH_EVENT_MESSAGE_WAITING, NULL, skinny_message_waiting_event_handler, NULL, &globals.message_waiting_node) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't bind our message waiting handler!\n");
		/* Not such severe to prevent loading */
	}
	if ((switch_event_bind_removable(modname, SWITCH_EVENT_TRAP, NULL, skinny_trap_event_handler, NULL, &globals.trap_node) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't bind our trap handler!\n");
		/* Not such severe to prevent loading */
	}
	if ((switch_event_bind_removable(modname, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_USER_TO_DEVICE, skinny_user_to_device_event_handler, NULL, &globals.user_to_device_node) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind our user_to_device handler!\n");
		/* Not such severe to prevent loading */
	}

	/* reserve events */
	if (switch_event_reserve_subclass(SKINNY_EVENT_REGISTER) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_REGISTER);
		return SWITCH_STATUS_TERM;
	}
	if (switch_event_reserve_subclass(SKINNY_EVENT_UNREGISTER) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_UNREGISTER);
		return SWITCH_STATUS_TERM;
	}
	if (switch_event_reserve_subclass(SKINNY_EVENT_EXPIRE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_EXPIRE);
		return SWITCH_STATUS_TERM;
	}
	if (switch_event_reserve_subclass(SKINNY_EVENT_ALARM) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_ALARM);
		return SWITCH_STATUS_TERM;
	}
	if (switch_event_reserve_subclass(SKINNY_EVENT_CALL_STATE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_CALL_STATE);
		return SWITCH_STATUS_TERM;
	}
	if (switch_event_reserve_subclass(SKINNY_EVENT_USER_TO_DEVICE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_USER_TO_DEVICE);
		return SWITCH_STATUS_TERM;
	}
	if (switch_event_reserve_subclass(SKINNY_EVENT_DEVICE_TO_USER) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_DEVICE_TO_USER);
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(globals.pool, modname);
	skinny_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	skinny_endpoint_interface->interface_name = "skinny";
	skinny_endpoint_interface->io_routines = &skinny_io_routines;
	skinny_endpoint_interface->state_handler = &skinny_state_handlers;

	skinny_api_register(module_interface);

	/* launch listeners */
	switch_mutex_lock(globals.mutex);
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		void *val;
		skinny_profile_t *profile;

		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;

		launch_skinny_profile_thread(profile);
	}
	switch_mutex_unlock(globals.mutex);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skinny_shutdown)
{
	switch_hash_index_t *hi;
	void *val;
	switch_memory_pool_t *pool = globals.pool;
	switch_mutex_t *mutex = globals.mutex;
	int sanity = 0;

	skinny_api_unregister();

	/* release events */
	switch_event_unbind(&globals.user_to_device_node);
	switch_event_unbind(&globals.call_state_node);
	switch_event_unbind(&globals.message_waiting_node);
	switch_event_unbind(&globals.trap_node);
	switch_event_free_subclass(SKINNY_EVENT_REGISTER);
	switch_event_free_subclass(SKINNY_EVENT_UNREGISTER);
	switch_event_free_subclass(SKINNY_EVENT_EXPIRE);
	switch_event_free_subclass(SKINNY_EVENT_ALARM);
	switch_event_free_subclass(SKINNY_EVENT_CALL_STATE);
	switch_event_free_subclass(SKINNY_EVENT_USER_TO_DEVICE);
	switch_event_free_subclass(SKINNY_EVENT_DEVICE_TO_USER);

	switch_mutex_lock(mutex);

	globals.running = 0;

	/* kill listeners */
	walk_listeners(kill_listener, NULL);

	/* close sockets */
	switch_mutex_lock(globals.mutex);
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		skinny_profile_t *profile;
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;

		close_socket(&profile->sock, profile);

		while (profile->listener_threads) {
			switch_yield(100000);
			walk_listeners(kill_listener, NULL);
			if (++sanity >= 200) {
				break;
			}
		}
		switch_core_destroy_memory_pool(&profile->pool);
	}
	switch_mutex_unlock(globals.mutex);

	switch_core_hash_destroy(&globals.profile_hash);
	memset(&globals, 0, sizeof(globals));
	switch_mutex_unlock(mutex);
	switch_core_destroy_memory_pool(&pool);
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
