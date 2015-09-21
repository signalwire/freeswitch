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
 * William King <william.king@quentustech.com>
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_sonar.c -- Sonar ping timer
 *
 * 
 */

/*
  TODO:
  1. Use libteltone directly
  2. Use an energy detection to listen for first set of sound back. Use timestamp of detection of energy as the recv stamp if a tone is eventually detected.
  3. Check for milliwatt pings. Listen for frequency changes, and audio loss
 */


#include <switch.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sonar_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_sonar_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_sonar_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_sonar, mod_sonar_load, mod_sonar_shutdown, NULL);


struct sonar_ping_helper_s {
	switch_time_t start, end, diff;
	int samples[1024];
	int received;
	int sum, min, max;
};

typedef struct sonar_ping_helper_s sonar_ping_helper_t;

switch_bool_t sonar_ping_callback(switch_core_session_t *session, const char *app, const char *app_data){
	switch_channel_t *channel = switch_core_session_get_channel(session);
	sonar_ping_helper_t *ph = switch_channel_get_private(channel, "__sonar_ping__");
	int diff;

	if (!ph) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate private sonar helper data\n");
		return SWITCH_TRUE;
	}

	if ( ph->end ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sonar not yet reset. Likely a repeat detection.\n");
		return SWITCH_TRUE;
	}

	ph->end = switch_time_now();
	diff = ph->end - ph->start;

	ph->start = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sonar ping took %ld milliseconds\n", (long)diff / 1000);

	diff /= 1000;
	ph->sum += diff;
	ph->max = MAX(ph->max, diff);
	ph->min = MIN(ph->min, diff);
	ph->samples[ph->received++] = diff;
	
	return SWITCH_TRUE;
}

SWITCH_STANDARD_APP(sonar_app)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *tone = "%(500,0,1004)";
	const char *arg = (char *) data;
	int loops;
	int lost = 0;
	int x;
	int avg = 0, sdev = 0, mdev = 0;
	int sum2;
	switch_event_t *event;
	sonar_ping_helper_t ph = { 0 };

	if (zstr(arg)) {
		loops = 5;
	} else {
		loops = atoi(data);
	}

	if (loops < 0) {
		loops = 5;
	} else if (loops > 1024) {
		loops = 1024;
	}
	
	switch_channel_answer(channel);
	switch_ivr_sleep(session, 1000, SWITCH_FALSE, NULL);
	switch_channel_set_private(channel, "__sonar_ping__", &ph);

	switch_ivr_tone_detect_session(session, 
								   "soar_ping", "1004",
								   "r", 0, 
								   1, NULL, NULL, sonar_ping_callback);
	
	switch_ivr_sleep(session, 1000, SWITCH_FALSE, NULL);

	ph.min = 999999;
	for( x = 0; x < loops; x++ ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending sonar ping\n");
		ph.end = 0;
		ph.start = switch_time_now();
		switch_ivr_gentones(session, tone, 1, NULL);
		switch_ivr_sleep(session, 2000, SWITCH_FALSE, NULL);
		if ( ph.start ) {
			lost++;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Lost sonar ping\n");
		}
	}
	
	switch_ivr_sleep(session, 1000, SWITCH_FALSE, NULL);
	switch_ivr_stop_tone_detect_session(session);

	if (loops == lost) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Too bad, we lost all!\n");
		return;
	}

	if (ph.received + lost != loops) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Race happend %d + %d != %d\n", ph.received, lost, loops);
	}

	if (ph.received > 0) avg = ph.sum / ph.received;

	sum2 = 0;
	for(x = 0; x < ph.received; x++) {
		sum2 += abs(ph.samples[x] - avg);
	}

	if (ph.received > 0) {
		mdev = sum2 / ph.received;
	}


	sum2 = 0;
	for(x = 0; x < ph.received; x++) {
		sum2 += (ph.samples[x] - avg) * (ph.samples[x] - avg);
	}

	if (ph.received > 1) {
		sdev = sqrt(sum2 / (ph.received - 1));
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Sonar Ping (in ms): min:%d max:%d avg:%d sdev:%d mdev:%d sent:%d recv: %d lost:%d lost/send:%2.2f%%\n",
		ph.min, ph.max, avg, sdev, mdev, loops, ph.received, lost, lost * 1.0 / loops);

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, "sonar::ping") == SWITCH_STATUS_SUCCESS) {
		const char *verbose_event;

		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_min", "%d", ph.min);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_max", "%d", ph.max);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_avg", "%d", avg);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_sdev", "%d", sdev);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_mdev", "%d", mdev);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_sent", "%d", loops);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_recv", "%d", ph.received);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ping_lost", "%d", lost);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "lost_rate", "%2.2f%%", lost * 1.0 / loops);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "destination_number",
			switch_channel_get_variable(channel, "ping_destination_number"));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sonar_ping_ref",
			switch_channel_get_variable(channel, "sonar_ping_ref"));

		verbose_event = switch_channel_get_variable(channel, "sonar_channel_event");

		if (verbose_event && switch_true(verbose_event)) {
			switch_channel_event_set_data(channel, event);
		}

		switch_event_fire(&event);
	}

}

/* Macro expands to: switch_status_t mod_sonar_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_sonar_load)
{
	switch_application_interface_t *app_interface;

	if (switch_event_reserve_subclass("sonar::ping") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", "sonar::ping");
		return SWITCH_STATUS_TERM;
	}

	
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "sonar", "sonar", "sonar", sonar_app, "", SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_sonar_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sonar_shutdown)
{

	switch_event_free_subclass("sonar::ping");
	
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
