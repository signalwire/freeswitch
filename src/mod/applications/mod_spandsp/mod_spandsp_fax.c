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
 * The Original Code is FreeSWITCH mod_fax.
 *
 * The Initial Developer of the Original Code is
 * Massimo Cetra <devel@navynet.it>
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Brian West <brian@freeswitch.org>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Steve Underwood <steveu@coppice.org>
 * Antonio Gallo <agx@linux.it>
 * mod_spandsp_fax.c -- Fax applications provided by SpanDSP
 *
 */

#include "mod_spandsp.h"

#include "udptl.h"

#define LOCAL_FAX_MAX_DATAGRAM      400
#define MAX_FEC_ENTRIES             4
#define MAX_FEC_SPAN                4
#define DEFAULT_FEC_ENTRIES         3
#define DEFAULT_FEC_SPAN            3

#define SPANDSP_EVENT_TXFAXRESULT "spandsp::txfaxresult"
#define SPANDSP_EVENT_RXFAXRESULT "spandsp::rxfaxresult"

#define SPANDSP_EVENT_TXFAXPAGERESULT "spandsp::txfaxpageresult"
#define SPANDSP_EVENT_RXFAXPAGERESULT "spandsp::rxfaxpageresult"

#define SPANDSP_EVENT_TXFAXNEGOCIATERESULT "spandsp::txfaxnegociateresult"
#define SPANDSP_EVENT_RXFAXNEGOCIATERESULT "spandsp::rxfaxnegociateresult"


/*****************************************************************************
	OUR DEFINES AND STRUCTS
*****************************************************************************/

typedef enum {
	T38_MODE,
	AUDIO_MODE,
	T38_GATEWAY_MODE
} transport_mode_t;

typedef enum {
	T38_MODE_UNKNOWN = 0,
	T38_MODE_NEGOTIATED = 1,
	T38_MODE_REQUESTED = 2,
	T38_MODE_REFUSED = -1,
} t38_mode_t;


struct pvt_s {
	switch_core_session_t *session;

	mod_spandsp_fax_application_mode_t app_mode;

	t30_state_t *t30;
	fax_state_t *fax_state;
	t38_terminal_state_t *t38_state;
	t38_gateway_state_t *t38_gateway_state;
	t38_core_state_t *t38_core;

	udptl_state_t *udptl_state;

	char *filename;
	char *ident;
	char *header;
	char *timezone;

	int use_ecm;
	int disable_v17;
	int enable_colour_fax;
	int enable_image_resizing;
	int enable_colour_to_bilevel;
	int enable_grayscale_to_bilevel;
	int verbose;
	int caller;

	int tx_page_start;
	int tx_page_end;

	int done;

	t38_mode_t t38_mode;

	struct pvt_s *next;
};

typedef struct pvt_s pvt_t;

static void launch_timer_thread(void);

static struct {
	pvt_t *head;
	switch_mutex_t *mutex;
	switch_thread_t *thread;
	int thread_running;
} t38_state_list;



static void wake_thread(int force)
{
	if (force) {
		switch_thread_cond_signal(spandsp_globals.cond);
		return;
	}

	if (switch_mutex_trylock(spandsp_globals.cond_mutex) == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(spandsp_globals.cond);
		switch_mutex_unlock(spandsp_globals.cond_mutex);
	}
}

static int add_pvt(pvt_t *pvt)
{
	int r = 0;

	if (t38_state_list.thread_running) {
		switch_mutex_lock(t38_state_list.mutex);
		pvt->next = t38_state_list.head;
		t38_state_list.head = pvt;
		switch_mutex_unlock(t38_state_list.mutex);
		r = 1;
		wake_thread(0);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error launching thread\n");
	}

	return r;

}


static int del_pvt(pvt_t *del_pvt)
{
	pvt_t *p, *l = NULL;
	int r = 0;


	switch_mutex_lock(t38_state_list.mutex);

	for (p = t38_state_list.head; p; p = p->next) {
		if (p == del_pvt) {
			if (l) {
				l->next = p->next;
			} else {
				t38_state_list.head = p->next;
			}
			p->next = NULL;
			r = 1;
			break;
		}

		l = p;
	}

	switch_mutex_unlock(t38_state_list.mutex);

	wake_thread(0);

	return r;
}

static void *SWITCH_THREAD_FUNC timer_thread_run(switch_thread_t *thread, void *obj)
{
	switch_timer_t timer = { 0 };
	pvt_t *pvt;
	int samples = 160;
	int ms = 20;

	switch_mutex_lock(t38_state_list.mutex);
	t38_state_list.thread_running = 1;
	switch_mutex_unlock(t38_state_list.mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FAX timer thread started.\n");

	if (switch_core_timer_init(&timer, "soft", ms, samples, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "timer init failed.\n");
		goto end;
	}

	switch_mutex_lock(spandsp_globals.cond_mutex);

	while(t38_state_list.thread_running) {

		switch_mutex_lock(t38_state_list.mutex);

		if (!t38_state_list.head) {
			switch_mutex_unlock(t38_state_list.mutex);
			switch_thread_cond_wait(spandsp_globals.cond, spandsp_globals.cond_mutex);
			switch_core_timer_sync(&timer);
			continue;
		}

		for (pvt = t38_state_list.head; pvt; pvt = pvt->next) {
			if (pvt->udptl_state && pvt->session && switch_channel_ready(switch_core_session_get_channel(pvt->session))) {
				t38_terminal_send_timeout(pvt->t38_state, samples);
			}
		}

		switch_mutex_unlock(t38_state_list.mutex);

		switch_core_timer_next(&timer);
	}

	switch_mutex_unlock(spandsp_globals.cond_mutex);

 end:

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FAX timer thread ended.\n");

	switch_mutex_lock(t38_state_list.mutex);
	t38_state_list.thread_running = 0;
	switch_mutex_unlock(t38_state_list.mutex);

	if (timer.timer_interface) {
		switch_core_timer_destroy(&timer);
	}

	return NULL;
}

static void launch_timer_thread(void)
{

	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, spandsp_globals.pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&t38_state_list.thread, thd_attr, timer_thread_run, NULL, spandsp_globals.pool);
}


/*****************************************************************************
	LOGGING AND HELPER FUNCTIONS
*****************************************************************************/

static void counter_increment(void)
{
	switch_mutex_lock(spandsp_globals.mutex);
	spandsp_globals.total_sessions++;
	switch_mutex_unlock(spandsp_globals.mutex);
}

void mod_spandsp_log_message(void *user_data, int level, const char *msg)
{
	int fs_log_level;
	switch_core_session_t *session = (switch_core_session_t *)user_data;

	switch (level) {
	case SPAN_LOG_NONE:
		return;
	case SPAN_LOG_ERROR:
	case SPAN_LOG_PROTOCOL_ERROR:
		fs_log_level = SWITCH_LOG_ERROR;
		break;
	case SPAN_LOG_WARNING:
	case SPAN_LOG_PROTOCOL_WARNING:
		fs_log_level = SWITCH_LOG_WARNING;
		break;
	case SPAN_LOG_FLOW:
	case SPAN_LOG_FLOW_2:
	case SPAN_LOG_FLOW_3:
	default:					/* SPAN_LOG_DEBUG, SPAN_LOG_DEBUG_2, SPAN_LOG_DEBUG_3 */
		fs_log_level = SWITCH_LOG_DEBUG;
		break;
	}

	if (!zstr(msg)) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), fs_log_level, "%s", msg);
	}
}

static int phase_b_handler(void *user_data, int result)
{
	t30_stats_t t30_stats;
	switch_core_session_t *session;
	switch_channel_t *channel;
	const char *local_ident;
	const char *far_ident;
	char *fax_transfer_rate = NULL;
	pvt_t *pvt;
	switch_event_t *event;

	pvt = (pvt_t *) user_data;
	switch_assert(pvt);

	session = pvt->session;
	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel);

	t30_get_transfer_statistics(pvt->t30, &t30_stats);

	local_ident = switch_str_nil(t30_get_tx_ident(pvt->t30));
	far_ident = switch_str_nil(t30_get_rx_ident(pvt->t30));

	fax_transfer_rate = switch_core_session_sprintf(session, "%i", t30_stats.bit_rate);
	if (fax_transfer_rate) {
		switch_channel_set_variable(channel, "fax_transfer_rate", fax_transfer_rate);
	}
	switch_channel_set_variable(channel, "fax_ecm_used", (t30_stats.error_correcting_mode) ? "on" : "off");
	switch_channel_set_variable(channel, "fax_local_station_id", local_ident);
	switch_channel_set_variable(channel, "fax_remote_station_id", far_ident);
	switch_channel_set_variable(channel, "fax_remote_country", switch_str_nil(t30_get_rx_country(pvt->t30)));
	switch_channel_set_variable(channel, "fax_remote_vendor", switch_str_nil(t30_get_rx_vendor(pvt->t30)));
	switch_channel_set_variable(channel, "fax_remote_model", switch_str_nil(t30_get_rx_model(pvt->t30)));


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "=== Negotiation Result =======================================================\n");
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote station id: %s\n", far_ident);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Local station id:  %s\n", local_ident);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Transfer Rate:     %i\n", t30_stats.bit_rate);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "ECM status         %s\n", (t30_stats.error_correcting_mode) ? "on" : "off");
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote country:   %s\n", switch_str_nil(t30_get_rx_country(pvt->t30)));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote vendor:    %s\n", switch_str_nil(t30_get_rx_vendor(pvt->t30)));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote model:     %s\n", switch_str_nil(t30_get_rx_model(pvt->t30)));

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "==============================================================================\n");

	switch_channel_execute_on(channel, "execute_on_fax_phase_b");

	/* Fire event */

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, pvt->app_mode == FUNCTION_TX ? SPANDSP_EVENT_TXFAXNEGOCIATERESULT : SPANDSP_EVENT_RXFAXNEGOCIATERESULT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "uuid", switch_core_session_get_uuid(session));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-transfer-rate", fax_transfer_rate);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-ecm-used", (t30_stats.error_correcting_mode) ? "on" : "off");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-local-station-id", local_ident);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-remote-station-id", far_ident);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-remote-country", switch_str_nil(t30_get_rx_country(pvt->t30)));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-remote-vendor", switch_str_nil(t30_get_rx_vendor(pvt->t30)));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-remote-model", switch_str_nil(t30_get_rx_model(pvt->t30)));
		switch_event_fire(&event);
	}

	return T30_ERR_OK;
}

static int phase_d_handler(void *user_data, int msg)
{
	t30_stats_t t30_stats;
	char *fax_file_image_resolution = NULL;
	char *fax_line_image_resolution = NULL;
	char *fax_file_image_pixel_size = NULL;
	char *fax_line_image_pixel_size = NULL;
	char *fax_image_size = NULL;
	char *fax_bad_rows = NULL;
	char *fax_encoding = NULL;
	char *fax_longest_bad_row_run = NULL;
	char *fax_document_transferred_pages = NULL;
	switch_core_session_t *session;
	switch_channel_t *channel;
	pvt_t *pvt;
	switch_event_t *event;

	pvt = (pvt_t *) user_data;
	switch_assert(pvt);

	session = pvt->session;
	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel);

	t30_get_transfer_statistics(pvt->t30, &t30_stats);

	/* Set Channel Variable */

	fax_line_image_resolution = switch_core_session_sprintf(session, "%ix%i", t30_stats.x_resolution, t30_stats.y_resolution);
	if (fax_line_image_resolution) {
		switch_channel_set_variable(channel, "fax_image_resolution", fax_line_image_resolution);
	}

	fax_file_image_resolution = switch_core_session_sprintf(session, "%ix%i", t30_stats.image_x_resolution, t30_stats.image_y_resolution);
	if (fax_file_image_resolution) {
		switch_channel_set_variable(channel, "fax_file_image_resolution", fax_file_image_resolution);
	}

	fax_line_image_pixel_size = switch_core_session_sprintf(session, "%ix%i", t30_stats.width, t30_stats.length);
	if (fax_line_image_pixel_size) {
		switch_channel_set_variable(channel, "fax_image_pixel_size", fax_line_image_pixel_size);;
	}

	fax_file_image_pixel_size = switch_core_session_sprintf(session, "%ix%i", t30_stats.image_width, t30_stats.image_length);
	if (fax_file_image_pixel_size) {
		switch_channel_set_variable(channel, "fax_file_image_pixel_size", fax_file_image_pixel_size);;
	}

	fax_image_size = switch_core_session_sprintf(session, "%d", t30_stats.image_size);
	if (fax_image_size) {
		switch_channel_set_variable(channel, "fax_image_size", fax_image_size);
	}

	fax_bad_rows = switch_core_session_sprintf(session, "%d", t30_stats.bad_rows);
	if (fax_bad_rows) {
		switch_channel_set_variable(channel, "fax_bad_rows", fax_bad_rows);
	}

	fax_longest_bad_row_run = switch_core_session_sprintf(session, "%d", t30_stats.longest_bad_row_run);
	if (fax_longest_bad_row_run) {
		switch_channel_set_variable(channel, "fax_longest_bad_row_run", fax_longest_bad_row_run);
	}

	fax_encoding = switch_core_session_sprintf(session, "%d", t30_stats.compression);
	if (fax_encoding) {
		switch_channel_set_variable(channel, "fax_encoding", fax_encoding);
	}

	switch_channel_set_variable(channel, "fax_encoding_name", t4_compression_to_str(t30_stats.compression));

	fax_document_transferred_pages = switch_core_session_sprintf(session, "%d", (pvt->app_mode == FUNCTION_TX)  ?  t30_stats.pages_tx  :  t30_stats.pages_rx);
	if (fax_document_transferred_pages) {
		switch_channel_set_variable(channel, "fax_document_transferred_pages", fax_document_transferred_pages);
	}


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "==== Page %s===========================================================\n", pvt->app_mode == FUNCTION_TX ? "Sent ====": "Received ");
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Page no = %d\n", (pvt->app_mode == FUNCTION_TX)  ?  t30_stats.pages_tx  :  t30_stats.pages_rx);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Image type = %s (%s in the file)\n", t4_image_type_to_str(t30_stats.type), t4_image_type_to_str(t30_stats.image_type));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Image size = %d x %d pixels (%d x %d pixels in the file)\n", t30_stats.width, t30_stats.length, t30_stats.image_width, t30_stats.image_length);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Image resolution = %d/m x %d/m (%d/m x %d/m in the file)\n", t30_stats.x_resolution, t30_stats.y_resolution, t30_stats.image_x_resolution, t30_stats.image_y_resolution);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Compression = %s (%d)\n", t4_compression_to_str(t30_stats.compression), t30_stats.compression);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Compressed image size = %d bytes\n", t30_stats.image_size);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Bad rows = %d\n", t30_stats.bad_rows);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Longest bad row run = %d\n", t30_stats.longest_bad_row_run);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "==============================================================================\n");

	switch_channel_execute_on(channel, "execute_on_fax_phase_d");

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, pvt->app_mode == FUNCTION_TX ? SPANDSP_EVENT_TXFAXPAGERESULT : SPANDSP_EVENT_RXFAXPAGERESULT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "uuid", switch_core_session_get_uuid(session));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-document-transferred-pages", fax_document_transferred_pages);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-image-resolution", fax_line_image_resolution);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-file-image-resolution", fax_file_image_resolution);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-image-size", fax_image_size);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-image-pixel-size", fax_line_image_pixel_size);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-file-image-pixel-size", fax_file_image_pixel_size);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-bad-rows", fax_bad_rows);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-longest-bad-row-run", fax_longest_bad_row_run);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-encoding", fax_encoding);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-encoding-name", t4_compression_to_str(t30_stats.compression));
		switch_event_fire(&event);
	}

	return T30_ERR_OK; /* I don't think this does anything */
}

/*
 * Called at the end of the document
 */
static void phase_e_handler(void *user_data, int result)
{
	t30_stats_t t;
	const char *local_ident;
	const char *far_ident;
	switch_core_session_t *session;
	switch_channel_t *channel;
	pvt_t *pvt;
	char *fax_document_transferred_pages = NULL;
	char *fax_document_total_pages = NULL;
	char *fax_image_resolution = NULL;
	char *fax_image_size = NULL;
	char *fax_bad_rows = NULL;
	char *fax_transfer_rate = NULL;
	char *fax_result_code = NULL;
	switch_event_t *event;
	const char *var;
	char *expanded;

	pvt = (pvt_t *) user_data;
	switch_assert(pvt);

	session = pvt->session;
	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel);

	t30_get_transfer_statistics(pvt->t30, &t);
	local_ident = switch_str_nil(t30_get_tx_ident(pvt->t30));
	far_ident = switch_str_nil(t30_get_rx_ident(pvt->t30));

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "==============================================================================\n");

	if (result == T30_ERR_OK) {
		if (pvt->app_mode == FUNCTION_TX) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Fax successfully sent.\n");
		} else if (pvt->app_mode == FUNCTION_RX) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Fax successfully received.\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Fax successfully managed. How ?\n");
		}
		switch_channel_set_variable(channel, "fax_success", "1");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Fax processing not successful - result (%d) %s.\n", result,
						  t30_completion_code_to_str(result));
		switch_channel_set_variable(channel, "fax_success", "0");
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote station id: %s\n", far_ident);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Local station id:  %s\n", local_ident);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Pages transferred: %i\n",
					  pvt->app_mode == FUNCTION_TX ? t.pages_tx : t.pages_rx);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Total fax pages:   %i\n", t.pages_in_file);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Image resolution:  %ix%i\n", t.x_resolution, t.y_resolution);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Transfer Rate:     %i\n", t.bit_rate);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "ECM status         %s\n", (t.error_correcting_mode) ? "on" : "off");
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote country:   %s\n", switch_str_nil(t30_get_rx_country(pvt->t30)));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote vendor:    %s\n", switch_str_nil(t30_get_rx_vendor(pvt->t30)));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote model:     %s\n", switch_str_nil(t30_get_rx_model(pvt->t30)));

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "==============================================================================\n");

	/*
	   Set our channel variables, variables are also used in event
	 */

	fax_result_code = switch_core_session_sprintf(session, "%i", result);
	if (fax_result_code) {
		switch_channel_set_variable(channel, "fax_result_code", fax_result_code);
	}

	switch_channel_set_variable(channel, "fax_result_text", t30_completion_code_to_str(result));

	switch_channel_set_variable(channel, "fax_ecm_used", (t.error_correcting_mode) ? "on" : "off");
	switch_channel_set_variable(channel, "fax_local_station_id", local_ident);
	switch_channel_set_variable(channel, "fax_remote_station_id", far_ident);

	fax_document_transferred_pages = switch_core_session_sprintf(session, "%i", pvt->app_mode == FUNCTION_TX ? t.pages_tx : t.pages_rx);
	if (fax_document_transferred_pages) {
		switch_channel_set_variable(channel, "fax_document_transferred_pages", fax_document_transferred_pages);
	}

	fax_document_total_pages = switch_core_session_sprintf(session, "%i", t.pages_in_file);
	if (fax_document_total_pages) {
		switch_channel_set_variable(channel, "fax_document_total_pages", fax_document_total_pages);
	}

	fax_image_resolution = switch_core_session_sprintf(session, "%ix%i", t.x_resolution, t.y_resolution);
	if (fax_image_resolution) {
		switch_channel_set_variable(channel, "fax_image_resolution", fax_image_resolution);
	}

	fax_image_size = switch_core_session_sprintf(session, "%d", t.image_size);
	if (fax_image_size) {
		switch_channel_set_variable(channel, "fax_image_size", fax_image_size);
	}

	fax_bad_rows = switch_core_session_sprintf(session, "%d", t.bad_rows);
	if (fax_bad_rows) {
		switch_channel_set_variable(channel, "fax_bad_rows", fax_bad_rows);
	}

	fax_transfer_rate = switch_core_session_sprintf(session, "%i", t.bit_rate);
	if (fax_transfer_rate) {
		switch_channel_set_variable(channel, "fax_transfer_rate", fax_transfer_rate);
	}

	/* switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING); */

	pvt->done = 1;

	/* Fire event */

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, pvt->app_mode == FUNCTION_TX ? SPANDSP_EVENT_TXFAXRESULT : SPANDSP_EVENT_RXFAXRESULT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-success", (result == T30_ERR_OK) ? "1" : "0");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-result-code", fax_result_code);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-result-text", t30_completion_code_to_str(result));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-document-transferred-pages", fax_document_transferred_pages);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-document-total-pages", fax_document_total_pages);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-image-resolution", fax_image_resolution);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-image-size", fax_image_size);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-bad-rows", fax_bad_rows);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-transfer-rate", fax_transfer_rate);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-ecm-used", (t.error_correcting_mode) ? "on" : "off");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-local-station-id", local_ident);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fax-remote-station-id", far_ident);
		switch_event_fire(&event);
	}

	if ((var = switch_channel_get_variable(channel, "system_on_fax_result"))) {
		expanded = switch_channel_expand_variables(channel, var);
		switch_system(expanded, SWITCH_FALSE);
		if (expanded != var) {
			free(expanded);
		}
	}

	switch_channel_execute_on(channel, "execute_on_fax_result");

	if (result == T30_ERR_OK) {
		if ((var = switch_channel_get_variable(channel, "system_on_fax_success"))) {
			expanded = switch_channel_expand_variables(channel, var);
			switch_system(expanded, SWITCH_FALSE);
			if (expanded != var) {
				free(expanded);
			}
		}
		switch_channel_execute_on(channel, "execute_on_fax_success");
	} else {
		if ((var = switch_channel_get_variable(channel, "system_on_fax_failure"))) {
			expanded = switch_channel_expand_variables(channel, var);
			switch_system(expanded, SWITCH_FALSE);
			if (expanded != var) {
				free(expanded);
			}
		}
		switch_channel_execute_on(channel, "execute_on_fax_failure");
	}
}

static int t38_tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
	switch_frame_t out_frame = { 0 };
	switch_core_session_t *session;
	pvt_t *pvt;
	uint8_t pkt[LOCAL_FAX_MAX_DATAGRAM];
	int x;
	int r = 0;

	pvt = (pvt_t *) user_data;
	session = pvt->session;

	/* we need to build a real packet here and make write_frame.packet and write_frame.packetlen point to it */
	out_frame.flags = SFF_UDPTL_PACKET | SFF_PROXY_PACKET;
	out_frame.packet = pkt;
	if ((r = udptl_build_packet(pvt->udptl_state, pkt, buf, len)) > 0) {
		out_frame.packetlen = r;
		//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "WRITE %d udptl bytes\n", out_frame.packetlen);

		for (x = 0; x < count; x++) {
			if (switch_core_session_write_frame(session, &out_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID WRITE: %d:%d\n", out_frame.packetlen, count);
				r = -1;
				break;
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID PACKETLEN: %d PASSED: %d:%d\n", r, len, count);
	}

	if (r < 0) {
		t30_state_t *t30;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "TERMINATING T30 STATE\n");

		if (pvt->t38_state && (t30 = t38_terminal_get_t30_state(pvt->t38_state))) {
			t30_terminate(t30);
		}
		switch_yield(10000);
	}

	return r < 0 ? r : 0;
}

static switch_status_t spanfax_init(pvt_t *pvt, transport_mode_t trans_mode)
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	fax_state_t *fax;
	t38_terminal_state_t *t38;
	t30_state_t *t30;
	const char *tmp;
    const char *tz;
	int fec_entries = DEFAULT_FEC_ENTRIES;
	int fec_span = DEFAULT_FEC_SPAN;
	int compressions;

	session = (switch_core_session_t *) pvt->session;
	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel);

	if ((tmp = switch_channel_get_variable(channel, "t38_gateway_redundancy"))) {
		int tmp_value;
		tmp_value = atoi(tmp);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FAX changing redundancy from %d:%d to %d:%d\n", fec_span, fec_entries, tmp_value, tmp_value );
		fec_entries = tmp_value;
		fec_span = tmp_value;
	}

	switch (trans_mode) {
	case AUDIO_MODE:
		if (pvt->fax_state == NULL) {
			pvt->fax_state = (fax_state_t *) switch_core_session_alloc(pvt->session, sizeof(fax_state_t));
		}
		if (pvt->fax_state == NULL) {
			return SWITCH_STATUS_FALSE;
		}

		fax = pvt->fax_state;
		pvt->t30 = fax_get_t30_state(fax);
		t30 = pvt->t30;

		memset(fax, 0, sizeof(fax_state_t));
		if (fax_init(fax, pvt->caller) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my fax structs\n");
			return SWITCH_STATUS_FALSE;
		}

		fax_set_transmit_on_idle(fax, TRUE);

		span_log_set_message_handler(fax_get_logging_state(fax), mod_spandsp_log_message, pvt->session);
		span_log_set_message_handler(t30_get_logging_state(t30), mod_spandsp_log_message, pvt->session);

		if (pvt->verbose) {
			span_log_set_level(fax_get_logging_state(fax), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
			span_log_set_level(t30_get_logging_state(t30), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
		}
		break;
	case T38_MODE:
		{
			switch_core_session_message_t msg = { 0 };

			if (pvt->t38_state == NULL) {
				pvt->t38_state = (t38_terminal_state_t *) switch_core_session_alloc(pvt->session, sizeof(t38_terminal_state_t));
			}
			if (pvt->t38_state == NULL) {
				return SWITCH_STATUS_FALSE;
			}
			if (pvt->udptl_state == NULL) {
				pvt->udptl_state = (udptl_state_t *) switch_core_session_alloc(pvt->session, sizeof(udptl_state_t));
			}
			if (pvt->udptl_state == NULL) {
				t38_terminal_free(pvt->t38_state);
				pvt->t38_state = NULL;
				return SWITCH_STATUS_FALSE;
			}

			t38 = pvt->t38_state;
			pvt->t30 = t38_terminal_get_t30_state(t38);
            t30 = pvt->t30;

			memset(t38, 0, sizeof(t38_terminal_state_t));

			if (t38_terminal_init(t38, pvt->caller, t38_tx_packet_handler, pvt) == NULL) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my T.38 structs\n");
				return SWITCH_STATUS_FALSE;
			}

			pvt->t38_core = t38_terminal_get_t38_core_state(pvt->t38_state);

			if (udptl_init(pvt->udptl_state, UDPTL_ERROR_CORRECTION_REDUNDANCY, fec_span, fec_entries,
					(udptl_rx_packet_handler_t *) t38_core_rx_ifp_packet, (void *) pvt->t38_core) == NULL) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my UDPTL structs\n");
				return SWITCH_STATUS_FALSE;
			}

			msg.from = __FILE__;
			msg.message_id = SWITCH_MESSAGE_INDICATE_UDPTL_MODE;
			switch_core_session_receive_message(pvt->session, &msg);

			/* add to timer thread processing */
			if (!add_pvt(pvt)) {
				if (channel) {
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}
			}

			span_log_set_message_handler(t38_terminal_get_logging_state(t38), mod_spandsp_log_message, pvt->session);
			span_log_set_message_handler(t30_get_logging_state(t30), mod_spandsp_log_message, pvt->session);

			if (pvt->verbose) {
				span_log_set_level(t38_terminal_get_logging_state(t38), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
				span_log_set_level(t30_get_logging_state(t30), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
			}
		}
		break;
	case T38_GATEWAY_MODE:
		if (pvt->t38_gateway_state == NULL) {
			pvt->t38_gateway_state = (t38_gateway_state_t *) switch_core_session_alloc(pvt->session, sizeof(t38_gateway_state_t));
		}

		if (pvt->udptl_state == NULL) {
			pvt->udptl_state = (udptl_state_t *) switch_core_session_alloc(pvt->session, sizeof(udptl_state_t));
		}

		if (t38_gateway_init(pvt->t38_gateway_state, t38_tx_packet_handler, pvt) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my T.38 structs\n");
			t38_gateway_free(pvt->t38_gateway_state);
			pvt->t38_gateway_state = NULL;

			return SWITCH_STATUS_FALSE;
		}

		pvt->t38_core = t38_gateway_get_t38_core_state(pvt->t38_gateway_state);

		if (udptl_init(pvt->udptl_state, UDPTL_ERROR_CORRECTION_REDUNDANCY, fec_span, fec_entries,
						(udptl_rx_packet_handler_t *) t38_core_rx_ifp_packet, (void *) pvt->t38_core) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my UDPTL structs\n");
			t38_gateway_free(pvt->t38_gateway_state);
			udptl_release(pvt->udptl_state);
			pvt->udptl_state = NULL;
			return SWITCH_STATUS_FALSE;
		}

		t38_gateway_set_transmit_on_idle(pvt->t38_gateway_state, TRUE);

		if (switch_true(switch_channel_get_variable(channel, "fax_v17_disabled"))) {
			t38_gateway_set_supported_modems(pvt->t38_gateway_state, T30_SUPPORT_V29 | T30_SUPPORT_V27TER);
		} else {
			t38_gateway_set_supported_modems(pvt->t38_gateway_state, T30_SUPPORT_V17 | T30_SUPPORT_V29 | T30_SUPPORT_V27TER);
		}

		t38_gateway_set_ecm_capability(pvt->t38_gateway_state, pvt->use_ecm);
		switch_channel_set_variable(channel, "fax_ecm_requested", pvt->use_ecm ? "true" : "false");

		if (switch_true(switch_channel_get_variable(channel, "FAX_DISABLE_ECM"))) {
			t38_gateway_set_ecm_capability(pvt->t38_gateway_state, FALSE);
		} else {
			t38_gateway_set_ecm_capability(pvt->t38_gateway_state, TRUE);
		}

		span_log_set_message_handler(t38_gateway_get_logging_state(pvt->t38_gateway_state), mod_spandsp_log_message, pvt->session);
		span_log_set_message_handler(t38_core_get_logging_state(pvt->t38_core), mod_spandsp_log_message, pvt->session);

		if (pvt->verbose) {
			span_log_set_level(t38_gateway_get_logging_state(pvt->t38_gateway_state), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
			span_log_set_level(t38_core_get_logging_state(pvt->t38_core), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
		}

		t38_set_t38_version(pvt->t38_core, 0);
		t38_gateway_set_ecm_capability(pvt->t38_gateway_state, 1);

		return SWITCH_STATUS_SUCCESS;
	default:
		assert(0);				/* What? */
		return SWITCH_STATUS_SUCCESS;
	}							/* Switch trans mode */

	/* All the things which are common to audio and T.38 FAX setup */
	t30_set_tx_ident(t30, pvt->ident);
	t30_set_tx_page_header_info(t30, pvt->header);
	if (pvt->timezone && pvt->timezone[0]) {
		if ((tz = switch_lookup_timezone(pvt->timezone)))
			t30_set_tx_page_header_tz(t30, tz);
		else
			t30_set_tx_page_header_tz(t30, pvt->timezone);
	}

	t30_set_phase_e_handler(t30, phase_e_handler, pvt);
	t30_set_phase_d_handler(t30, phase_d_handler, pvt);
	t30_set_phase_b_handler(t30, phase_b_handler, pvt);

	t30_set_supported_image_sizes(t30,
								  T4_SUPPORT_LENGTH_US_LETTER
								| T4_SUPPORT_LENGTH_US_LEGAL
								| T4_SUPPORT_LENGTH_UNLIMITED
								| T4_SUPPORT_WIDTH_215MM
								| T4_SUPPORT_WIDTH_255MM
								| T4_SUPPORT_WIDTH_303MM);
	t30_set_supported_bilevel_resolutions(t30,
										  T4_RESOLUTION_R8_STANDARD
										| T4_RESOLUTION_R8_FINE
										| T4_RESOLUTION_R8_SUPERFINE
										| T4_RESOLUTION_R16_SUPERFINE
                                        | T4_RESOLUTION_200_100
                                        | T4_RESOLUTION_200_200
                                        | T4_RESOLUTION_200_400
                                        | T4_RESOLUTION_400_400);
	compressions = T4_COMPRESSION_T4_1D
				 | T4_COMPRESSION_T4_2D
				 | T4_COMPRESSION_T6
				 | T4_COMPRESSION_T85
				 | T4_COMPRESSION_T85_L0;
	if (pvt->enable_colour_fax) {
		t30_set_supported_colour_resolutions(t30, T4_RESOLUTION_100_100
												| T4_RESOLUTION_200_200
												| T4_RESOLUTION_300_300
												| T4_RESOLUTION_400_400);
		compressions |= (T4_COMPRESSION_COLOUR | T4_COMPRESSION_T42_T81);
	} else {
		t30_set_supported_colour_resolutions(t30, 0);
	}
	if (pvt->enable_image_resizing)
		compressions |= T4_COMPRESSION_RESCALING;
	if (pvt->enable_colour_to_bilevel)
		compressions |= T4_COMPRESSION_COLOUR_TO_BILEVEL;
	if (pvt->enable_grayscale_to_bilevel)
		compressions |= T4_COMPRESSION_GRAY_TO_BILEVEL;

	t30_set_supported_compressions(t30, compressions);

	if (pvt->disable_v17) {
		t30_set_supported_modems(t30, T30_SUPPORT_V29 | T30_SUPPORT_V27TER);
		switch_channel_set_variable(channel, "fax_v17_disabled", "1");
	} else {
		t30_set_supported_modems(t30, T30_SUPPORT_V29 | T30_SUPPORT_V27TER | T30_SUPPORT_V17);
		switch_channel_set_variable(channel, "fax_v17_disabled", "0");
	}

	if (pvt->use_ecm) {
		t30_set_ecm_capability(t30, TRUE);
		switch_channel_set_variable(channel, "fax_ecm_requested", "1");
	} else {
		t30_set_ecm_capability(t30, FALSE);
		switch_channel_set_variable(channel, "fax_ecm_requested", "0");
	}

	if (pvt->app_mode == FUNCTION_TX) {
		t30_set_tx_file(t30, pvt->filename, pvt->tx_page_start, pvt->tx_page_end);
	} else {
		t30_set_rx_file(t30, pvt->filename, -1);
	}
	switch_channel_set_variable(channel, "fax_filename", pvt->filename);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t spanfax_destroy(pvt_t *pvt)
{
	int terminate;
	t30_state_t *t30;

	if (!pvt) return SWITCH_STATUS_FALSE;

	if (pvt->fax_state) {
		if (pvt->t38_state) {
			terminate = 0;
		} else {
			terminate = 1;
		}

		t30 = fax_get_t30_state(pvt->fax_state);
		if (terminate && t30) {
			t30_terminate(t30);
		}

		fax_release(pvt->fax_state);
	}

	if (pvt->t38_state) {

		/* remove from timer thread processing */
		del_pvt(pvt);

		if (pvt->t38_state) {
			terminate = 1;
		} else {
			terminate = 0;
		}

		t30 = t38_terminal_get_t30_state(pvt->t38_state);
		if (terminate && t30) {
			t30_terminate(t30);
		}

		t38_terminal_release(pvt->t38_state);
	}

	if (pvt->t38_gateway_state) {
		t38_gateway_release(pvt->t38_gateway_state);
	}

	if (pvt->udptl_state) {
		udptl_release(pvt->udptl_state);
	}
	return SWITCH_STATUS_SUCCESS;
}

static t38_mode_t configure_t38(pvt_t *pvt)
{
	switch_core_session_t *session = pvt->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_t38_options_t *t38_options = switch_channel_get_private(channel, "t38_options");
	int method = 2;

	if (!t38_options || !pvt || !pvt->t38_core) {
		pvt->t38_mode = T38_MODE_REFUSED;
		return pvt->t38_mode;
	}

	t38_set_t38_version(pvt->t38_core, t38_options->T38FaxVersion);
	t38_set_max_buffer_size(pvt->t38_core, t38_options->T38FaxMaxBuffer);
	t38_set_fastest_image_data_rate(pvt->t38_core, t38_options->T38MaxBitRate);
	t38_set_fill_bit_removal(pvt->t38_core, t38_options->T38FaxFillBitRemoval);
	t38_set_mmr_transcoding(pvt->t38_core, t38_options->T38FaxTranscodingMMR);
	t38_set_jbig_transcoding(pvt->t38_core, t38_options->T38FaxTranscodingJBIG);
	t38_set_max_datagram_size(pvt->t38_core, t38_options->T38FaxMaxDatagram);

	if (t38_options->T38FaxRateManagement) {
		if (!strcasecmp(t38_options->T38FaxRateManagement, "transferredTCF")) {
			method = 2;
		} else {
			method = 1;
		}
	}

	t38_set_data_rate_management_method(pvt->t38_core, method);


	//t38_set_data_transport_protocol(pvt->t38_core, int data_transport_protocol);
	//t38_set_redundancy_control(pvt->t38_core, int category, int setting);

	return pvt->t38_mode;
}

static t38_mode_t negotiate_t38(pvt_t *pvt)
{
	switch_core_session_t *session = pvt->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_core_session_message_t msg = { 0 };
	switch_t38_options_t *t38_options = switch_channel_get_private(channel, "t38_options");
	int enabled = 0, insist = 0;
	const char *v;

	pvt->t38_mode = T38_MODE_REFUSED;

	if (pvt->app_mode == FUNCTION_GW) {
		enabled = 1;
	} else if ((v = switch_channel_get_variable(channel, "fax_enable_t38"))) {
		enabled = switch_true(v);
	} else {
		enabled = spandsp_globals.enable_t38;
	}

	if (!(enabled && t38_options)) {
		/* if there is no t38_options the endpoint will refuse the transition */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s NO T38 options detected.\n", switch_channel_get_name(channel));
		switch_channel_set_private(channel, "t38_options", NULL);
	} else {
		pvt->t38_mode = T38_MODE_NEGOTIATED;
		switch_channel_set_app_flag_key("T38", channel, CF_APP_T38_NEGOTIATED);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38 SDP Origin = %s\n", t38_options->sdp_o_line);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxVersion = %d\n", t38_options->T38FaxVersion);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38MaxBitRate = %d\n", t38_options->T38MaxBitRate);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxFillBitRemoval = %d\n", t38_options->T38FaxFillBitRemoval);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxTranscodingMMR = %d\n", t38_options->T38FaxTranscodingMMR);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxTranscodingJBIG = %d\n", t38_options->T38FaxTranscodingJBIG);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxRateManagement = '%s'\n", t38_options->T38FaxRateManagement);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxMaxBuffer = %d\n", t38_options->T38FaxMaxBuffer);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxMaxDatagram = %d\n", t38_options->T38FaxMaxDatagram);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38FaxUdpEC = '%s'\n", t38_options->T38FaxUdpEC);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "T38VendorInfo = '%s'\n", switch_str_nil(t38_options->T38VendorInfo));
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "ip = '%s'\n",
				t38_options->remote_ip ? t38_options->remote_ip : "Not specified");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "port = %d\n", t38_options->remote_port);

		/* Time to practice our negotiating skills, by editing the t38_options */

		if (t38_options->T38FaxVersion > 3) {
			t38_options->T38FaxVersion = 3;
		}
		t38_options->T38MaxBitRate = (pvt->disable_v17)  ?  9600  :  14400;

		/* cisco gets mad when we set this to one in a response where they set it to 0, are we allowed to hardcode this to 1 on responses?  */
		/*
		if (!zstr(t38_options->sdp_o_line) && !switch_stristr("cisco", t38_options->sdp_o_line)) {
			t38_options->T38FaxFillBitRemoval = 1;
		}
		*/

		t38_options->T38FaxTranscodingMMR = 0;
		t38_options->T38FaxTranscodingJBIG = 0;
		t38_options->T38FaxRateManagement = "transferredTCF";
        if (!t38_options->T38FaxMaxBuffer) {
            t38_options->T38FaxMaxBuffer = 2000;
        }
		t38_options->T38FaxMaxDatagram = LOCAL_FAX_MAX_DATAGRAM;
		if (!zstr(t38_options->T38FaxUdpEC) &&
				(strcasecmp(t38_options->T38FaxUdpEC, "t38UDPRedundancy") == 0 ||
				strcasecmp(t38_options->T38FaxUdpEC, "t38UDPFEC") == 0)) {
			t38_options->T38FaxUdpEC = "t38UDPRedundancy";
		} else {
			t38_options->T38FaxUdpEC = NULL;
		}
		t38_options->T38VendorInfo = "0 0 0";
	}

	if ((v = switch_channel_get_variable(channel, "fax_enable_t38_insist"))) {
		insist = switch_true(v);
	} else {
		insist = spandsp_globals.enable_t38_insist;
	}

	/* This will send the options back in a response */
	msg.from = __FILE__;
	msg.message_id = SWITCH_MESSAGE_INDICATE_T38_DESCRIPTION;
	msg.numeric_arg = insist;
	switch_core_session_receive_message(session, &msg);

	return pvt->t38_mode;
}



static t38_mode_t request_t38(pvt_t *pvt)
{
	switch_core_session_t *session = pvt->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_core_session_message_t msg = { 0 };
	switch_t38_options_t *t38_options = NULL;
	int enabled = 0, insist = 0;
	const char *v;

	pvt->t38_mode = T38_MODE_UNKNOWN;

	if (pvt->app_mode == FUNCTION_GW) {
		enabled = 1;
	} else if ((v = switch_channel_get_variable(channel, "fax_enable_t38"))) {
		enabled = switch_true(v);
	} else {
		enabled = spandsp_globals.enable_t38;
	}

	if (enabled) {
		if ((v = switch_channel_get_variable(channel, "fax_enable_t38_request"))) {
			enabled = switch_true(v);
		} else {
			enabled = spandsp_globals.enable_t38_request;
		}
	}


	if ((v = switch_channel_get_variable(channel, "fax_enable_t38_insist"))) {
		insist = switch_true(v);
	} else {
		insist = spandsp_globals.enable_t38_insist;
	}

	if ((t38_options = switch_channel_get_private(channel, "t38_options"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
				"%s already has T.38 data\n", switch_channel_get_name(channel));
		enabled = 0;
	}



	if (enabled) {

        if (!(t38_options = switch_channel_get_private(channel, "_preconfigured_t38_options"))) {
            t38_options = switch_core_session_alloc(session, sizeof(*t38_options));
            t38_options->T38MaxBitRate = (pvt->disable_v17) ? 9600 : 14400;
            t38_options->T38FaxVersion = 0;
            t38_options->T38FaxFillBitRemoval = 1;
            t38_options->T38FaxTranscodingMMR = 0;
            t38_options->T38FaxTranscodingJBIG = 0;
            t38_options->T38FaxRateManagement = "transferredTCF";
            t38_options->T38FaxMaxBuffer = 2000;
            t38_options->T38FaxMaxDatagram = LOCAL_FAX_MAX_DATAGRAM;
            t38_options->T38FaxUdpEC = "t38UDPRedundancy";
            t38_options->T38VendorInfo = "0 0 0";
        }

	switch_channel_set_private(channel, "t38_options", t38_options);
        switch_channel_set_private(channel, "_preconfigured_t38_options", NULL);

		pvt->t38_mode = T38_MODE_REQUESTED;
		switch_channel_set_app_flag_key("T38", channel, CF_APP_T38_REQ);

		/* This will send a request for t.38 mode */
		msg.from = __FILE__;
		msg.message_id = SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA;
		msg.numeric_arg = insist;
		switch_core_session_receive_message(session, &msg);
	}

	return pvt->t38_mode;
}

/*****************************************************************************
	MAIN FAX PROCESSING
*****************************************************************************/

static pvt_t *pvt_init(switch_core_session_t *session, mod_spandsp_fax_application_mode_t app_mode)
{
	switch_channel_t *channel;
	pvt_t *pvt = NULL;
	const char *tmp;

	/* Make sure we have a valid channel when starting the FAX application */
	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	if (!switch_channel_media_ready(channel)) {
		switch_channel_answer(channel);
	}

	/* Allocate our structs */
	pvt = switch_core_session_alloc(session, sizeof(pvt_t));
	pvt->session = session;

	pvt->app_mode = app_mode;

	pvt->tx_page_start = -1;
	pvt->tx_page_end = -1;


	switch(pvt->app_mode) {

	case FUNCTION_TX:
		pvt->caller = 1;
		break;
	case FUNCTION_RX:
		pvt->caller = 0;
		break;
	case FUNCTION_GW:
		break;
	}

	/* Retrieving our settings from the channel variables */

	if ((tmp = switch_channel_get_variable(channel, "fax_use_ecm"))) {
		pvt->use_ecm = switch_true(tmp);
	} else {
		pvt->use_ecm = spandsp_globals.use_ecm;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_disable_v17"))) {
		pvt->disable_v17 = switch_true(tmp);
	} else {
		pvt->disable_v17 = spandsp_globals.disable_v17;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_enable_colour"))) {
		pvt->enable_colour_fax = switch_true(tmp);
	} else {
		pvt->enable_colour_fax = spandsp_globals.enable_colour_fax;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_enable_image_resizing"))) {
		pvt->enable_image_resizing = switch_true(tmp);
	} else {
		pvt->enable_image_resizing = spandsp_globals.enable_image_resizing;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_enable_colour_to_bilevel"))) {
		pvt->enable_colour_to_bilevel = switch_true(tmp);
	} else {
		pvt->enable_colour_to_bilevel = spandsp_globals.enable_colour_to_bilevel;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_enable_grayscale_to_bilevel"))) {
		pvt->enable_grayscale_to_bilevel = switch_true(tmp);
	} else {
		pvt->enable_grayscale_to_bilevel = spandsp_globals.enable_grayscale_to_bilevel;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_verbose"))) {
		pvt->verbose = switch_true(tmp);
	} else {
		pvt->verbose = spandsp_globals.verbose;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_force_caller"))) {
		if (switch_true(tmp)) {
			pvt->caller = 1;
		} else {
			pvt->caller = 0;
		}
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_ident"))) {
		char *data = NULL;

		data = strdup(tmp);
		switch_url_decode(data);
		pvt->ident = switch_core_session_strdup(session, data);

		switch_safe_free(data);
	} else {
		pvt->ident = switch_core_session_strdup(session, spandsp_globals.ident);
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_header"))) {
		char *data = NULL;

		data = strdup(tmp);
		switch_url_decode(data);
		pvt->header = switch_core_session_strdup(session, data);

		switch_safe_free(data);
	} else {
		pvt->header = switch_core_session_strdup(session, spandsp_globals.header);
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_timezone"))) {
		char *data = NULL;

		data = strdup(tmp);
		switch_url_decode(data);
		pvt->timezone = switch_core_session_strdup(session, data);

		switch_safe_free(data);
	} else {
		pvt->timezone = switch_core_session_strdup(session, spandsp_globals.timezone);
	}

	if (pvt->app_mode == FUNCTION_TX) {
		if ((tmp = switch_channel_get_variable(channel, "fax_start_page"))) {
			pvt->tx_page_start = atoi(tmp);
		}

		if ((tmp = switch_channel_get_variable(channel, "fax_end_page"))) {
			pvt->tx_page_end = atoi(tmp);
		}

		if (pvt->tx_page_end < -1) {
			pvt->tx_page_end = -1;
		}

		if (pvt->tx_page_start < -1) {
			pvt->tx_page_start = -1;
		}

		if ((pvt->tx_page_end < pvt->tx_page_start) && (pvt->tx_page_end != -1)) {
			pvt->tx_page_end = pvt->tx_page_start;
		}
	}

	return pvt;
}

void mod_spandsp_fax_stop_fax(switch_core_session_t *session)
{
	pvt_t *pvt = switch_channel_get_private(switch_core_session_get_channel(session), "_fax_pvt");
	if (pvt) {
		pvt->done = 1;
	}
}

void mod_spandsp_fax_process_fax(switch_core_session_t *session, const char *data, mod_spandsp_fax_application_mode_t app_mode)
{
	pvt_t *pvt;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_codec_t read_codec = { 0 };
	switch_codec_t write_codec = { 0 };
	switch_frame_t *read_frame = { 0 };
	switch_frame_t write_frame = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	int16_t *buf = NULL;
	uint32_t req_counter = 0;

	switch_core_session_get_read_impl(session, &read_impl);

	counter_increment();


	pvt = pvt_init(session, app_mode);
	switch_channel_set_private(channel, "_fax_pvt", pvt);

	buf = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);

	if (!zstr(data)) {
		pvt->filename = switch_core_session_strdup(session, data);
		if (pvt->app_mode == FUNCTION_TX) {
			if ((switch_file_exists(pvt->filename, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot send non-existant fax file [%s]\n",
								  switch_str_nil(pvt->filename));
				goto done;
			}
		}
	} else {
		if (pvt->app_mode == FUNCTION_TX) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Fax TX filename not set.\n");
			goto done;
		} else if (pvt->app_mode == FUNCTION_RX) {
			const char *prefix;
			switch_time_t time;

			time = switch_time_now();

			if (!(prefix = switch_channel_get_variable(channel, "fax_prefix"))) {
				prefix = spandsp_globals.prepend_string;
			}

			if (!(pvt->filename = switch_core_session_sprintf(session, "%s/%s-%ld-%" SWITCH_TIME_T_FMT ".tif", spandsp_globals.spool, prefix, spandsp_globals.total_sessions, time))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot automatically set fax RX destination file\n");
				goto done;
			}
		} else {
			assert(0);			/* UH ?? */
		}
	}

	/*
	 *** Initialize the SpanDSP elements ***

	 Note: we could analyze if a fax was already detected in previous stages
	 and if so, when T.38 will be supported, send a reinvite in T38_MODE,
	 bypassing AUDIO_MODE.
	 */

	if ((spanfax_init(pvt, AUDIO_MODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize Fax engine\n");
		return;
	}

	/*
	   Note: Disable echocan on the channel, remember to call app "disable_ec" in the dialplan
	   before invoking fax applications
	 */

	/*
	   Note: we are disabling the Jitterbuffer, here, before we answer.
	   If you have set it to something else and the channel is pre-answered,
	   it will have no effect. Make sure that if you want more reliable
	   faxes, it is disabled.
	 */
	switch_channel_set_variable(channel, "jitterbuffer_msec", NULL);


	/* We store the original channel codec before switching both
	 * legs of the calls to a linear 16 bit codec that is the one
	 * used internally by spandsp and FS will do the transcoding
	 * from G.711 or any other original codec
	 */
	if (switch_core_codec_init(&read_codec,
							   "L16",
							   NULL,
							   read_impl.samples_per_second,
							   read_impl.microseconds_per_packet / 1000,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Raw read codec activation Success L16 %u\n",
						  read_codec.implementation->microseconds_per_packet);
		switch_core_session_set_read_codec(session, &read_codec);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Raw read codec activation Failed L16\n");
		goto done;
	}

	if (switch_core_codec_init(&write_codec,
							   "L16",
							   NULL,
							   read_impl.samples_per_second,
							   read_impl.microseconds_per_packet / 1000,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Raw write codec activation Success L16\n");
		write_frame.codec = &write_codec;
		write_frame.data = buf;
		write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Raw write codec activation Failed L16\n");
		goto done;
	}

	switch_ivr_sleep(session, 250, SWITCH_TRUE, NULL);

	if (pvt->app_mode == FUNCTION_TX) {
		req_counter = 100;
	} else {
		req_counter = 50;
	}

	while (switch_channel_ready(channel)) {
		int tx = 0;
		switch_status_t status;

		switch_ivr_parse_all_events(session);

		/*
		   if we are in T.38 mode, we should: 1- initialize the ptv->t38_state stuff, if not done
		   and then set some callbacks when reading frames.
		   The only thing we need, then, in this loop, is:
		   - read a frame without blocking
		   - eventually feed that frame in spandsp,
		   - call t38_terminal_send_timeout(), sleep for a while

		   The T.38 stuff can be placed here (and the audio stuff can be skipped)
		*/

		/* read new audio frame from the channel */
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status) || pvt->done) {
			/* Our duty is over */
			goto done;
		}

		switch (pvt->t38_mode) {
		case T38_MODE_REQUESTED:
			{
				if (switch_channel_test_app_flag_key("T38", channel, CF_APP_T38_FAIL)) {
					pvt->t38_mode = T38_MODE_REFUSED;
					continue;
				} else if (switch_channel_test_app_flag_key("T38", channel, CF_APP_T38)) {
					switch_core_session_message_t msg = { 0 };
					pvt->t38_mode = T38_MODE_NEGOTIATED;
					switch_channel_set_app_flag_key("T38", channel, CF_APP_T38_NEGOTIATED);
					spanfax_init(pvt, T38_MODE);
					configure_t38(pvt);

					/* This will change the rtp stack to udptl mode */
					msg.from = __FILE__;
					msg.message_id = SWITCH_MESSAGE_INDICATE_UDPTL_MODE;
					switch_core_session_receive_message(session, &msg);
				}
				continue;
			}
		break;
		case T38_MODE_UNKNOWN:
			{
				if (req_counter) {
					if (!--req_counter) {
						/* If you have the means, I highly recommend picking one up. ...*/
						request_t38(pvt);
					}
				}

				if (switch_channel_test_app_flag_key("T38", channel, CF_APP_T38)) {
					if (negotiate_t38(pvt) == T38_MODE_NEGOTIATED) {
						/* is is safe to call this again, it was already called above in AUDIO_MODE */
						/* but this is the only way to set up the t38 stuff */
						spanfax_init(pvt, T38_MODE);
						continue;
					}
				}
			}
			break;
		case T38_MODE_NEGOTIATED:
			{
				/* do what we need to do when we are in t38 mode */
				if (switch_test_flag(read_frame, SFF_CNG)) {
					/* dunno what to do, most likely you will not get too many of these since we turn off the timer in udptl mode */
					continue;
				}

				if (switch_test_flag(read_frame, SFF_UDPTL_PACKET) && read_frame->packet && read_frame->packetlen) {
					/* now we know we can cast frame->packet to a udptl structure */
					//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "READ %d udptl bytes\n", read_frame->packetlen);

					udptl_rx_packet(pvt->udptl_state, read_frame->packet, read_frame->packetlen);
				}
			}
			continue;
		default:
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			/* We have no real signal data for the FAX software, but we have a space in time if we have a CNG indication.
			   Do a fill-in operation in the FAX machine, to keep things rolling along. */
			if (fax_rx_fillin(pvt->fax_state, read_impl.samples_per_packet)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "fax_rx_fillin reported an error\n");
				continue;
			}
		} else {
			/* Pass the new incoming audio frame to the fax_rx function */
			if (fax_rx(pvt->fax_state, (int16_t *) read_frame->data, read_frame->samples)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "fax_rx reported an error\n");
				goto done;
			}
		}

		if ((tx = fax_tx(pvt->fax_state, buf, write_codec.implementation->samples_per_packet)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "fax_tx reported an error\n");
			goto done;
		}

		if (!tx) {
			/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No audio samples to send\n"); */
			continue;
		} else {
			/* Set our write_frame data */
			write_frame.datalen = tx * sizeof(int16_t);
			write_frame.samples = tx;
		}

		if (switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}

	}

  done:
	/* Destroy the SpanDSP structures */
	spanfax_destroy(pvt);

	/* restore the original codecs over the channel */

	switch_core_session_set_read_codec(session, NULL);

	if (switch_core_codec_ready(&read_codec)) {
		switch_core_codec_destroy(&read_codec);
	}

	if (switch_core_codec_ready(&write_codec)) {
		switch_core_codec_destroy(&write_codec);
	}
}

void mod_spandsp_fax_load(switch_memory_pool_t *pool)
{
	uint32_t sanity = 200;

	memset(&t38_state_list, 0, sizeof(t38_state_list));

	switch_mutex_init(&spandsp_globals.mutex, SWITCH_MUTEX_NESTED, spandsp_globals.pool);
	switch_mutex_init(&t38_state_list.mutex, SWITCH_MUTEX_NESTED, spandsp_globals.pool);

	switch_mutex_init(&spandsp_globals.cond_mutex, SWITCH_MUTEX_NESTED, spandsp_globals.pool);
	switch_thread_cond_create(&spandsp_globals.cond, spandsp_globals.pool);

	launch_timer_thread();

	while(--sanity && !t38_state_list.thread_running) {
		switch_yield(20000);
	}
}

void mod_spandsp_fax_shutdown(void)
{
	switch_status_t tstatus = SWITCH_STATUS_SUCCESS;

	t38_state_list.thread_running = 0;
	wake_thread(1);
	switch_thread_join(&tstatus, t38_state_list.thread);
}

static const switch_state_handler_table_t t38_gateway_state_handlers;

static switch_status_t t38_gateway_on_soft_execute(switch_core_session_t *session)
{
	switch_core_session_t *other_session;

	switch_channel_t *other_channel, *channel = switch_core_session_get_channel(session);
	pvt_t *pvt;
	const char *peer_uuid = switch_channel_get_variable(channel, "t38_peer");
	switch_core_session_message_t msg = { 0 };
	switch_status_t status;
	switch_frame_t *read_frame = { 0 };

	if (!(other_session = switch_core_session_locate(peer_uuid))) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Cannot locate channel with uuid %s",
				switch_channel_get_name(channel), peer_uuid);
		goto end;
	}

	other_channel = switch_core_session_get_channel(other_session);

	pvt = pvt_init(session, FUNCTION_GW);
	request_t38(pvt);

	msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
	msg.from = __FILE__;
	msg.string_arg = peer_uuid;
	switch_core_session_receive_message(session, &msg);

	while (switch_channel_ready(channel) && switch_channel_up(other_channel) && !switch_channel_test_app_flag_key("T38", channel, CF_APP_T38)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (pvt->done) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Premature exit while negotiating\n", switch_channel_get_name(channel));
			/* Our duty is over */
			goto end_unlock;
		}

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Read failed, status=%u\n", switch_channel_get_name(channel), status);
			goto end_unlock;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (switch_core_session_write_frame(other_session, read_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Write failed\n", switch_channel_get_name(channel));
			goto end_unlock;
		}
	}

	if (!(switch_channel_ready(channel) && switch_channel_up(other_channel))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Channel not ready\n", switch_channel_get_name(channel));
		goto end_unlock;
	}

	if (!switch_channel_test_app_flag_key("T38", channel, CF_APP_T38)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Could not negotiate T38\n", switch_channel_get_name(channel));
		goto end_unlock;
	}

	if (pvt->t38_mode == T38_MODE_REQUESTED) {
		spanfax_init(pvt, T38_GATEWAY_MODE);
		configure_t38(pvt);
		pvt->t38_mode = T38_MODE_NEGOTIATED;
		switch_channel_set_app_flag_key("T38", channel, CF_APP_T38_NEGOTIATED);
	} else {
		if (negotiate_t38(pvt) != T38_MODE_NEGOTIATED) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Could not negotiate T38\n", switch_channel_get_name(channel));
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto end_unlock;
		}
		switch_channel_set_app_flag_key("T38", channel, CF_APP_T38_NEGOTIATED);
		spanfax_init(pvt, T38_GATEWAY_MODE);
	}

	/* This will change the rtp stack to udptl mode */
	msg.from = __FILE__;
	msg.message_id = SWITCH_MESSAGE_INDICATE_UDPTL_MODE;
	switch_core_session_receive_message(session, &msg);


	/* wake up the audio side */
	switch_channel_set_private(channel, "_t38_pvt", pvt);
	switch_channel_set_app_flag_key("T38", other_channel, CF_APP_T38);


	while (switch_channel_ready(channel) && switch_channel_up(other_channel)) {

		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status) || pvt->done) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Premature exit while negotiating (%i)\n", switch_channel_get_name(channel), status);
			/* Our duty is over */
			goto end_unlock;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (switch_test_flag(read_frame, SFF_UDPTL_PACKET)) {
			if (udptl_rx_packet(pvt->udptl_state, read_frame->packet, read_frame->packetlen) < 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Error decoding UDPTL (%u bytes)\n", switch_channel_get_name(channel), read_frame->packetlen);
                        }
		}
	}

 end_unlock:


	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	msg.from = __FILE__;
	msg.string_arg = peer_uuid;
	switch_core_session_receive_message(session, &msg);

	switch_channel_hangup(other_channel, SWITCH_CAUSE_NORMAL_CLEARING);
	switch_core_session_rwunlock(other_session);

 end:

	switch_channel_clear_state_handler(channel, &t38_gateway_state_handlers);
	switch_channel_set_variable(channel, "t38_peer", NULL);

	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t t38_gateway_on_consume_media(switch_core_session_t *session)
{
	switch_core_session_t *other_session;
	switch_channel_t *other_channel, *channel = switch_core_session_get_channel(session);
	const char *peer_uuid = switch_channel_get_variable(channel, "t38_peer");
	pvt_t *pvt = NULL;
	switch_codec_t read_codec = { 0 };
	switch_codec_t write_codec = { 0 };
	switch_frame_t *read_frame = { 0 };
	switch_frame_t write_frame = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	int16_t *buf = NULL;
	switch_status_t status;
	int tx;
	const char *t38_trace = switch_channel_get_variable(channel, "t38_trace");
	char *trace_read, *trace_write;
	zap_socket_t read_fd = FAX_INVALID_SOCKET, write_fd = FAX_INVALID_SOCKET;
	switch_core_session_message_t msg = { 0 };
	switch_event_t *event;

	switch_core_session_get_read_impl(session, &read_impl);

	buf = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);

	if (!(other_session = switch_core_session_locate(peer_uuid))) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto end;
	}

	other_channel = switch_core_session_get_channel(other_session);

	msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
	msg.from = __FILE__;
	msg.string_arg = peer_uuid;
	switch_core_session_receive_message(session, &msg);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", peer_uuid);
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}

	while (switch_channel_ready(channel) && switch_channel_up(other_channel) && !switch_channel_test_app_flag_key("T38", channel, CF_APP_T38)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			/* Our duty is over */
			goto end_unlock;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (switch_core_session_write_frame(other_session, read_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
			goto end_unlock;
		}
	}

	if (!(switch_channel_ready(channel) && switch_channel_up(other_channel))) {
		goto end_unlock;
	}

	if (!switch_channel_test_app_flag_key("T38", channel, CF_APP_T38)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto end_unlock;
	}

	if (!(pvt = switch_channel_get_private(other_channel, "_t38_pvt"))) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto end_unlock;
	}

	if (switch_core_codec_init(&read_codec,
							   "L16",
							   NULL,
							   read_impl.samples_per_second,
							   read_impl.microseconds_per_packet / 1000,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Raw read codec activation Success L16 %u\n",
						  read_codec.implementation->microseconds_per_packet);
		switch_core_session_set_read_codec(session, &read_codec);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Raw read codec activation Failed L16\n");
		goto end_unlock;
	}

	if (switch_core_codec_init(&write_codec,
							   "L16",
							   NULL,
							   read_impl.samples_per_second,
							   read_impl.microseconds_per_packet / 1000,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Raw write codec activation Success L16\n");
		write_frame.codec = &write_codec;
		write_frame.data = buf;
		write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Raw write codec activation Failed L16\n");
		goto end_unlock;
	}

	switch_ivr_sleep(session, 0, SWITCH_TRUE, NULL);

	if (switch_true(t38_trace)) {
		trace_read = switch_core_session_sprintf(session, "%s%s%s_read.raw", SWITCH_GLOBAL_dirs.temp_dir,
							SWITCH_PATH_SEPARATOR, switch_core_session_get_uuid(session));

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Tracing inbound audio to %s\n", trace_read);
		switch_channel_set_variable(channel, "t38_trace_read", trace_read);

		trace_write = switch_core_session_sprintf(session, "%s%s%s_write.raw", SWITCH_GLOBAL_dirs.temp_dir,
							SWITCH_PATH_SEPARATOR, switch_core_session_get_uuid(session));

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Tracing outbound audio to %s\n", trace_write);
		switch_channel_set_variable(channel, "t38_trace_read", trace_write);


		if ((write_fd = open(trace_read, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) != FAX_INVALID_SOCKET) {
			if ((read_fd = open(trace_write, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == FAX_INVALID_SOCKET) {
				close(write_fd);
				write_fd = FAX_INVALID_SOCKET;
			}
		}
	}

	while (switch_channel_ready(channel) && switch_channel_up(other_channel)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status) || pvt->done) {
			/* Our duty is over */
			goto end_unlock;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			/* We have no real signal data for the FAX software, but we have a space in time if we have a CNG indication.
			   Do a fill-in operation in the FAX machine, to keep things rolling along. */
			t38_gateway_rx_fillin(pvt->t38_gateway_state, read_impl.samples_per_packet);
		} else {
			if (read_fd != FAX_INVALID_SOCKET) {
				switch_ssize_t rv;
				do { rv = write(read_fd, read_frame->data, read_frame->datalen); } while (rv == -1 && errno == EINTR);
		}
		if (t38_gateway_rx(pvt->t38_gateway_state, (int16_t *) read_frame->data, read_frame->samples)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "fax_rx reported an error\n");
				goto end_unlock;
			}
		}

		if ((tx = t38_gateway_tx(pvt->t38_gateway_state, buf, write_codec.implementation->samples_per_packet)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "fax_tx reported an error\n");
			goto end_unlock;
		}

		if (!tx) {
			/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No audio samples to send\n"); */
			continue;
		} else {
			/* Set our write_frame data */
			write_frame.datalen = tx * sizeof(int16_t);
			write_frame.samples = tx;
		}

		if (write_fd != FAX_INVALID_SOCKET) {
			switch_ssize_t rv;
			do { rv = write(write_fd, write_frame.data, write_frame.datalen); } while (rv == -1 && errno == EINTR);
		}

		if (switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
			goto end_unlock;
		}
	}

 end_unlock:

	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	msg.from = __FILE__;
	msg.string_arg = peer_uuid;
	switch_core_session_receive_message(session, &msg);


	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}

	if (read_fd != FAX_INVALID_SOCKET) {
		close(read_fd);
		read_fd = FAX_INVALID_SOCKET;
	}

	if (write_fd != FAX_INVALID_SOCKET) {
		close(write_fd);
		write_fd = FAX_INVALID_SOCKET;
	}


	switch_channel_hangup(other_channel, SWITCH_CAUSE_NORMAL_CLEARING);
	switch_core_session_rwunlock(other_session);

	switch_core_session_set_read_codec(session, NULL);

	if (switch_core_codec_ready(&read_codec)) {
		switch_core_codec_destroy(&read_codec);
	}

	if (switch_core_codec_ready(&write_codec)) {
		switch_core_codec_destroy(&write_codec);
	}


 end:

	switch_channel_clear_state_handler(channel, &t38_gateway_state_handlers);
	switch_channel_set_variable(channel, "t38_peer", NULL);
	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t t38_gateway_on_reset(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_channel_set_variable(channel, "rtp_autoflush_during_bridge", "false");

	switch_channel_clear_flag(channel, CF_REDIRECT);

	if (switch_channel_test_app_flag_key("T38", channel, CF_APP_TAGGED)) {
		switch_channel_clear_app_flag_key("T38", channel, CF_APP_TAGGED);
		switch_channel_set_state(channel, CS_CONSUME_MEDIA);
	} else {
		switch_channel_set_state(channel, CS_SOFT_EXECUTE);
	}

	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table_t t38_gateway_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ t38_gateway_on_soft_execute,
	/*.on_consume_media */ t38_gateway_on_consume_media,
	/*.on_hibernate */ NULL,
	/*.on_reset */ t38_gateway_on_reset,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ NULL,
	SSH_FLAG_STICKY
};

switch_bool_t t38_gateway_start(switch_core_session_t *session, const char *app, const char *data)
{
	switch_channel_t *other_channel = NULL, *channel = switch_core_session_get_channel(session);
	switch_core_session_t *other_session = NULL;
	int peer = app && !strcasecmp(app, "peer");

	if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
		other_channel = switch_core_session_get_channel(other_session);

		switch_channel_set_variable(channel, "t38_peer", switch_core_session_get_uuid(other_session));
		switch_channel_set_variable(other_channel, "t38_peer", switch_core_session_get_uuid(session));

		switch_channel_set_variable(peer ? other_channel : channel, "t38_gateway_format", "udptl");
		switch_channel_set_variable(peer ? channel : other_channel, "t38_gateway_format", "audio");


		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s starting gateway mode to %s\n",
				switch_channel_get_name(peer ? channel : other_channel),
				switch_channel_get_name(peer ? other_channel : channel));


		switch_channel_clear_state_handler(channel, NULL);
		switch_channel_clear_state_handler(other_channel, NULL);

		switch_channel_add_state_handler(channel, &t38_gateway_state_handlers);
		switch_channel_add_state_handler(other_channel, &t38_gateway_state_handlers);

		switch_channel_set_app_flag_key("T38", peer ? channel : other_channel, CF_APP_TAGGED);
		switch_channel_clear_app_flag_key("T38", peer ? other_channel : channel, CF_APP_TAGGED);

		switch_channel_set_flag(channel, CF_REDIRECT);
		switch_channel_set_state(channel, CS_RESET);

		switch_channel_set_flag(other_channel, CF_REDIRECT);
		switch_channel_set_state(other_channel, CS_RESET);

		switch_core_session_rwunlock(other_session);

	}

	return SWITCH_FALSE;
}

typedef struct {
	char *app;
	char *data;
	char *key;
	int up;
	int tone_type;
	int total_hits;
	int hits;
	int sleep;
	int expires;
	int default_sleep;
	int default_expires;
	switch_tone_detect_callback_t callback;
	modem_connect_tones_rx_state_t rx_tones;

	switch_media_bug_t *bug;
	switch_core_session_t *session;
	int bug_running;

} spandsp_fax_tone_container_t;

static switch_status_t tone_on_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf, switch_dtmf_direction_t direction)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	spandsp_fax_tone_container_t *cont = switch_channel_get_private(channel, "_fax_tone_detect_");


	if (!cont || dtmf->digit != 'f') {
		return SWITCH_STATUS_SUCCESS;
	}

	if (cont->callback) {
		cont->callback(cont->session, cont->app, cont->data);
	} else {
		switch_channel_execute_on(switch_core_session_get_channel(cont->session), "execute_on_fax_detect");
		if (cont->app) {
			switch_core_session_execute_application_async(cont->session, cont->app, cont->data);
		}
	}

	return SWITCH_STATUS_SUCCESS;

}


static switch_bool_t tone_detect_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	spandsp_fax_tone_container_t *cont = (spandsp_fax_tone_container_t *) user_data;
	switch_frame_t *frame = NULL;
	switch_bool_t rval = SWITCH_TRUE;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		if (cont) {
			cont->bug_running = 1;
			modem_connect_tones_rx_init(&cont->rx_tones, cont->tone_type, NULL, NULL);
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		{
			int skip = 0;

			if (type == SWITCH_ABC_TYPE_READ_REPLACE) {
				frame = switch_core_media_bug_get_read_replace_frame(bug);
			} else {
				frame = switch_core_media_bug_get_write_replace_frame(bug);
			}

			if (cont->sleep) {
				cont->sleep--;
				if (cont->sleep) {
					skip = 1;
				}
			}

			if (cont->expires) {
				cont->expires--;
				if (!cont->expires) {
					cont->hits = 0;
					cont->sleep = 0;
					cont->expires = 0;
				}
			}

			if (!cont->up) {
				skip = 1;
			}

			if (skip) {
				return SWITCH_TRUE;
			}

			cont->hits = 0;
			modem_connect_tones_rx(&cont->rx_tones, frame->data, frame->samples);
			cont->hits = modem_connect_tones_rx_get(&cont->rx_tones);

			if (cont->hits) {
				switch_event_t *event;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_DEBUG,
								  "Fax Tone Detected. [%s][%s]\n", cont->app, switch_str_nil(cont->data));

				if (cont->callback) {
					cont->callback(cont->session, cont->app, cont->data);
				} else {
					switch_channel_execute_on(switch_core_session_get_channel(cont->session), "execute_on_fax_detect");
					if (cont->app) {
						switch_core_session_execute_application_async(cont->session, cont->app, cont->data);
					}
				}


				if (switch_event_create(&event, SWITCH_EVENT_DETECTED_TONE) == SWITCH_STATUS_SUCCESS) {
					switch_event_t *dup;
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detected-Fax-Tone", "true");

					session = switch_core_media_bug_get_session(bug);
					if (session) {
					    channel = switch_core_session_get_channel(session);
					    if (channel) switch_channel_event_set_data(channel, event);
					}

					if (switch_event_dup(&dup, event) == SWITCH_STATUS_SUCCESS) {
						switch_event_fire(&dup);
					}

					if (switch_core_session_queue_event(cont->session, &event) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR,
										  "Event queue failed!\n");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
						switch_event_fire(&event);
					}
				}

				rval = SWITCH_FALSE;
			}

		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	if (rval == SWITCH_FALSE) {
		cont->bug_running = 0;
	}

	return rval;
}

switch_status_t spandsp_fax_stop_detect_session(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	spandsp_fax_tone_container_t *cont = switch_channel_get_private(channel, "_fax_tone_detect_");

	if (cont) {
		switch_channel_set_private(channel, "_fax_tone_detect_", NULL);
		cont->up = 0;
		switch_core_media_bug_remove(session, &cont->bug);
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

switch_status_t spandsp_fax_detect_session(switch_core_session_t *session,
														   const char *flags, int timeout, int tone_type,
														   int hits, const char *app, const char *data, switch_tone_detect_callback_t callback)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	time_t to = 0;
	spandsp_fax_tone_container_t *cont = switch_channel_get_private(channel, "_fax_tone_detect_");
	switch_media_bug_flag_t bflags = 0;
	const char *var;
	switch_codec_implementation_t read_impl = { 0 };
	switch_core_session_get_read_impl(session, &read_impl);

	if (timeout) {
		to = switch_epoch_time_now(NULL) + timeout;
	}

	if (cont) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Max Tones Reached!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!cont && !(cont = switch_core_session_alloc(session, sizeof(*cont)))) {
		return SWITCH_STATUS_MEMERR;
	}

	if (app) {
		cont->app = switch_core_session_strdup(session, app);
	}

	if (data) {
		cont->data = switch_core_session_strdup(session, data);
	}

	cont->tone_type = tone_type;
	cont->callback = callback;
	cont->up = 1;
	cont->session = session;

	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	cont->default_sleep = 25;
	cont->default_expires = 250;

	if ((var = switch_channel_get_variable(channel, "fax_tone_detect_sleep"))) {
		int tmp = atoi(var);
		if (tmp > 0) {
			cont->default_sleep = tmp;
		}
	}

	if ((var = switch_channel_get_variable(channel, "fax_tone_detect_expires"))) {
		int tmp = atoi(var);
		if (tmp > 0) {
			cont->default_expires = tmp;
		}
	}

	if (zstr(flags)) {
		bflags = SMBF_READ_REPLACE;
	} else {
		if (strchr(flags, 'r')) {
			bflags |= SMBF_READ_REPLACE;
		} else if (strchr(flags, 'w')) {
			bflags |= SMBF_WRITE_REPLACE;
		}
	}

	bflags |= SMBF_NO_PAUSE;


	switch_core_event_hook_add_send_dtmf(session, tone_on_dtmf);
	switch_core_event_hook_add_recv_dtmf(session, tone_on_dtmf);


	if ((status = switch_core_media_bug_add(session, "fax_tone_detect", "",
											tone_detect_callback, cont, to, bflags, &cont->bug)) != SWITCH_STATUS_SUCCESS) {
		cont->bug_running = 0;
		return status;
	}

	switch_channel_set_private(channel, "_fax_tone_detect_", cont);

	return SWITCH_STATUS_SUCCESS;
}




/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
