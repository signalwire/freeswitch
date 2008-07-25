/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * The Original Code is FreeSWITCH mod_timezone.
 *
 * The Initial Developer of the Original Code is
 * Massimo Cetra <devel@navynet.it>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * mod_timezone.c -- Access to timezone informations and time string formatting
 *
 */

#include <switch.h>
#include "mod_timezone.h"

/* 
   This converts a struct tm to a switch_time_exp_t
   We have to use UNIX structures to do our exams
   and use switch_* functions for the output.
*/

static void tm2switchtime( tm, xt ) 
	struct tm 		*tm;
switch_time_exp_t 	*xt;
{

	if (!xt || !tm) {
	    return;
	}
	memset( xt, 0, sizeof(xt) );

	xt->tm_sec  	= tm->tm_sec;
	xt->tm_min  	= tm->tm_min;
	xt->tm_hour 	= tm->tm_hour;
	xt->tm_mday 	= tm->tm_mday;
	xt->tm_mon  	= tm->tm_mon;
	xt->tm_year 	= tm->tm_year;
	xt->tm_wday 	= tm->tm_wday;
	xt->tm_yday 	= tm->tm_yday;
	xt->tm_isdst 	= tm->tm_isdst;
	xt->tm_gmtoff 	= tm->tm_gmtoff;

	return;
}

/* **************************************************************************
   LOADING OF THE XML DATA - HASH TABLE & MEMORY POOL MANAGEMENT
   ************************************************************************** */

typedef struct {
	switch_memory_pool_t *pool;
	switch_hash_t *hash;
} switch_timezones_list_t;

static switch_timezones_list_t TIMEZONES_LIST = { 0 };
static switch_event_node_t *NODE = NULL;

const char *switch_lookup_timezone( const char *tzname )
{
	char *value = NULL;

	if ( tzname && (value = switch_core_hash_find(TIMEZONES_LIST.hash, tzname))==NULL ) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timezone '%s' not found!\n", tzname);
	}
	
	return value;
}

void switch_load_timezones(switch_bool_t reload)
{
	switch_xml_t xml = NULL, x_lists = NULL, x_list = NULL, cfg = NULL;
	unsigned total = 0;

	if (TIMEZONES_LIST.hash) {
		switch_core_hash_destroy(&TIMEZONES_LIST.hash);
	}

	if (TIMEZONES_LIST.pool) {
		switch_core_destroy_memory_pool(&TIMEZONES_LIST.pool);
	}

	memset(&TIMEZONES_LIST, 0, sizeof(TIMEZONES_LIST));
	switch_core_new_memory_pool(&TIMEZONES_LIST.pool);
	switch_core_hash_init(&TIMEZONES_LIST.hash, TIMEZONES_LIST.pool);

	if ((xml = switch_xml_open_cfg("timezones.conf", &cfg, NULL))) {
		if ((x_lists = switch_xml_child(cfg, "timezones"))) {
			for (x_list = switch_xml_child(x_lists, "zone"); x_list; x_list = x_list->next) {
				const char *name = switch_xml_attr(x_list, "name");
				const char *value= switch_xml_attr(x_list, "value");

				if (switch_strlen_zero(name)) {
					continue;
				}

				if (switch_strlen_zero(value)) {
					continue;
				}

				switch_core_hash_insert(TIMEZONES_LIST.hash, 
										name, 
										switch_core_strdup(TIMEZONES_LIST.pool, value) );
				total++;
			}
		}
		
		switch_xml_free(xml);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Timezone %sloaded %d definitions\n", reload ? "re" : "", total);
}

/* **************************************************************************
   API FUNCTIONS AND COMMANDS
   ************************************************************************** */

SWITCH_STANDARD_API(strftime_tz_api_function)
{
	switch_time_t thetime;
	time_t timep;

	char *format = NULL;
	const char *tzname;
	const char *tzdef;

	switch_size_t retsize;
	char date[80] = "";

	struct tm tm;
	switch_time_exp_t stm;

	thetime = switch_timestamp_now();

	timep =  (thetime) / (int64_t) (1000000);

	if (!switch_strlen_zero(cmd)) {
		format = strchr(cmd, ' ');
		tzname = cmd;
		if (format) {
			*format++ = '\0';
		}

		tzdef = switch_lookup_timezone( tzname );
	} else {
		/* We set the default timezone to GMT. */
		tzname="GMT";
		tzdef="GMT";
	}
	
	if (tzdef) { /* The lookup of the zone may fail. */
		tztime( &timep, tzdef, &tm );
		tm2switchtime( &tm, &stm );
		switch_strftime(date, &retsize, sizeof(date), switch_strlen_zero(format) ? "%Y-%m-%d %T" : format, &stm);
		stream->write_function(stream, "%s", date);
	} else {
		stream->write_function(stream, "-ERR Invalid Timezone\n");
	}
	
	return SWITCH_STATUS_SUCCESS;
}

/* **************************************************************************

************************************************************************** */


SWITCH_MODULE_LOAD_FUNCTION(mod_timezone_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_timezone_shutdown);
SWITCH_MODULE_DEFINITION(mod_timezone, mod_timezone_load, mod_timezone_shutdown, NULL);

static void event_handler(switch_event_t *event)
{
	switch_load_timezones(1);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_timezone_load)
{
	switch_api_interface_t *api_interface;
	
	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
	}
	switch_load_timezones(0);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "strftime_tz", "strftime_tz", strftime_tz_api_function, "<Timezone name>,<format string>");

	return SWITCH_STATUS_SUCCESS;
}

//  Called when the system shuts down
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_timezone_shutdown)
{

	if (TIMEZONES_LIST.hash) {
		switch_core_hash_destroy(&TIMEZONES_LIST.hash);
	}

	if (TIMEZONES_LIST.pool) {
		switch_core_destroy_memory_pool(&TIMEZONES_LIST.pool);
	}

	return SWITCH_STATUS_UNLOAD;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
