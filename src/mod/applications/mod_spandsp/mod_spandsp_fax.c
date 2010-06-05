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

/* The global stuff */
static struct {
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;

	uint32_t total_sessions;

	short int use_ecm;
	short int verbose;
	short int disable_v17;
    short int enable_t38;
    short int enable_t38_request;
    short int enable_t38_insist;
	char ident[20];
	char header[50];
	char *prepend_string;
	char *spool;
} globals;

struct pvt_s {
	switch_core_session_t *session;

	mod_spandsp_fax_application_mode_t app_mode;

	fax_state_t *fax_state;
	t38_terminal_state_t *t38_state;
	t38_gateway_state_t *t38_gateway_state;
    t38_core_state_t *t38_core;

    udptl_state_t *udptl_state;

	char *filename;
	char *ident;
	char *header;

	int use_ecm;
	int disable_v17;
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

static int add_pvt(pvt_t *pvt)
{
    int r = 0;
    uint32_t sanity = 50;

    switch_mutex_lock(t38_state_list.mutex);
    if (!t38_state_list.thread_running) {

        launch_timer_thread();

        while(--sanity && !t38_state_list.thread_running) {
            switch_yield(10000);
        }
    }
    switch_mutex_unlock(t38_state_list.mutex);
    
    if (t38_state_list.thread_running) {
        switch_mutex_lock(t38_state_list.mutex);
        pvt->next = t38_state_list.head;
        t38_state_list.head = pvt;
        switch_mutex_unlock(t38_state_list.mutex);
    }

    return r;

}


static int del_pvt(pvt_t *del_pvt)
{
    pvt_t *p, *l = NULL;
    int r = 0;

    if (!t38_state_list.thread_running) goto end;
    
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
            goto end;
        }

        l = p;
    }

 end:

    switch_mutex_unlock(t38_state_list.mutex);

    return r;

}

static void *SWITCH_THREAD_FUNC timer_thread_run(switch_thread_t *thread, void *obj)
{
    switch_timer_t timer = { 0 };
    pvt_t *pvt;
    int samples = 240;
    int ms = 30;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "timer thread started.\n");

	if (switch_core_timer_init(&timer, "soft", ms, samples, NULL) != SWITCH_STATUS_SUCCESS) {
        return NULL;
    }

    t38_state_list.thread_running = 1;

    while(t38_state_list.thread_running) {

        switch_mutex_lock(t38_state_list.mutex);

        if (!t38_state_list.head) {
            switch_mutex_unlock(t38_state_list.mutex);
            goto end;
        }

        for (pvt = t38_state_list.head; pvt; pvt = pvt->next) {
            if (pvt->udptl_state) {
                t38_terminal_send_timeout(pvt->t38_state, samples);
            }
        }

        switch_mutex_unlock(t38_state_list.mutex);

        switch_core_timer_next(&timer);
    }
    
 end:

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "timer thread ended.\n");

    t38_state_list.thread_running = 0;
    switch_core_timer_destroy(&timer);
    
    return NULL;
}

static void launch_timer_thread(void)
{

	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&t38_state_list.thread, thd_attr, timer_thread_run, NULL, globals.pool);
}


/*****************************************************************************
	LOGGING AND HELPER FUNCTIONS
*****************************************************************************/

static void counter_increment(void)
{
	switch_mutex_lock(globals.mutex);
	globals.total_sessions++;
	switch_mutex_unlock(globals.mutex);
}

static void spanfax_log_message(int level, const char *msg)
{
	int fs_log_level;

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
		switch_log_printf(SWITCH_CHANNEL_LOG, fs_log_level, "%s", msg);
	}
}

/*
 * Called at the end of the document
 */
static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
	t30_stats_t t;
	const char *local_ident;
	const char *far_ident;
	switch_core_session_t *session;
	switch_channel_t *channel;
	pvt_t *pvt;
	char *tmp;

	pvt = (pvt_t *) user_data;
	switch_assert(pvt);

	session = pvt->session;
	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel);

	t30_get_transfer_statistics(s, &t);
	local_ident = switch_str_nil(t30_get_tx_ident(s));
	far_ident = switch_str_nil(t30_get_rx_ident(s));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");

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
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote country:   %s\n", switch_str_nil(t30_get_rx_country(s)));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote vendor:    %s\n", switch_str_nil(t30_get_rx_vendor(s)));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "remote model:     %s\n", switch_str_nil(t30_get_rx_model(s)));

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "==============================================================================\n");

	/*
	   Set our channel variables
	 */

	tmp = switch_mprintf("%i", result);
	if (tmp) {
		switch_channel_set_variable(channel, "fax_result_code", tmp);
		switch_safe_free(tmp);
	}

	switch_channel_set_variable(channel, "fax_result_text", t30_completion_code_to_str(result));

	switch_channel_set_variable(channel, "fax_ecm_used", (t.error_correcting_mode) ? "on" : "off");
	switch_channel_set_variable(channel, "fax_local_station_id", local_ident);
	switch_channel_set_variable(channel, "fax_remote_station_id", far_ident);

	tmp = switch_mprintf("%i", pvt->app_mode == FUNCTION_TX ? t.pages_tx : t.pages_rx);
	if (tmp) {
		switch_channel_set_variable(channel, "fax_document_transferred_pages", tmp);
		switch_safe_free(tmp);
	}

	tmp = switch_mprintf("%i", t.pages_in_file);
	if (tmp) {
		switch_channel_set_variable(channel, "fax_document_total_pages", tmp);
		switch_safe_free(tmp);
	}

	tmp = switch_mprintf("%ix%i", t.x_resolution, t.y_resolution);
	if (tmp) {
		switch_channel_set_variable(channel, "fax_image_resolution", tmp);
		switch_safe_free(tmp);
	}

	tmp = switch_mprintf("%d", t.image_size);
	if (tmp) {
		switch_channel_set_variable(channel, "fax_image_size", tmp);
		switch_safe_free(tmp);
	}

	tmp = switch_mprintf("%d", t.bad_rows);
	if (tmp) {
		switch_channel_set_variable(channel, "fax_bad_rows", tmp);
		switch_safe_free(tmp);
	}

	tmp = switch_mprintf("%i", t.bit_rate);
	if (tmp) {
		switch_channel_set_variable(channel, "fax_transfer_rate", tmp);
		switch_safe_free(tmp);
	}

	/* switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING); */

	pvt->done = 1;

	/*
	   TODO Fire events
	 */
}

static int t38_tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    switch_frame_t out_frame = { 0 };
    switch_core_session_t *session;
    switch_channel_t *channel;
    pvt_t *pvt;
    uint8_t pkt[LOCAL_FAX_MAX_DATAGRAM];
    int x;
    int r = 0;

    pvt = (pvt_t *) user_data;
    session = pvt->session;
    channel = switch_core_session_get_channel(session);

    /* we need to build a real packet here and make write_frame.packet and write_frame.packetlen point to it */
    out_frame.flags = SFF_UDPTL_PACKET | SFF_PROXY_PACKET;
    out_frame.packet = pkt;
    out_frame.packetlen = udptl_build_packet(pvt->udptl_state, pkt, buf, len);
    
    //switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "WRITE %d udptl bytes\n", out_frame.packetlen);

    for (x = 0; x < count; x++) {
        if (switch_core_session_write_frame(session, &out_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
            r = -1;
            break;
        }
    }

    return r;
}

static switch_status_t spanfax_init(pvt_t *pvt, transport_mode_t trans_mode)
{

	switch_core_session_t *session;
	switch_channel_t *channel;
	fax_state_t *fax;
	t38_terminal_state_t *t38;
	t30_state_t *t30;


	session = (switch_core_session_t *) pvt->session;
	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel);


	switch (trans_mode) {
	case AUDIO_MODE:
		if (pvt->fax_state == NULL) {
			pvt->fax_state = (fax_state_t *) switch_core_session_alloc(pvt->session, sizeof(fax_state_t));
		}
		if (pvt->fax_state == NULL) {
			return SWITCH_STATUS_FALSE;
		}

		fax = pvt->fax_state;
		t30 = fax_get_t30_state(fax);

		memset(fax, 0, sizeof(fax_state_t));
		if (fax_init(fax, pvt->caller) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my fax structs\n");
			return SWITCH_STATUS_FALSE;
		}

		fax_set_transmit_on_idle(fax, TRUE);

		span_log_set_message_handler(&fax->logging, spanfax_log_message);
		span_log_set_message_handler(&t30->logging, spanfax_log_message);

		if (pvt->verbose) {
			span_log_set_level(&fax->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
			span_log_set_level(&t30->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
		}
		break;
	case T38_MODE:
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

        /* add to timer thread processing */
        add_pvt(pvt);
        
		t38 = pvt->t38_state;
		t30 = t38_terminal_get_t30_state(t38);

		memset(t38, 0, sizeof(t38_terminal_state_t));

		if (t38_terminal_init(t38, pvt->caller, t38_tx_packet_handler, pvt) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my T.38 structs\n");
			return SWITCH_STATUS_FALSE;
		}

        pvt->t38_core = t38_terminal_get_t38_core_state(pvt->t38_state);

        if (udptl_init(pvt->udptl_state, UDPTL_ERROR_CORRECTION_REDUNDANCY, 3, 3, 
                       (udptl_rx_packet_handler_t *) t38_core_rx_ifp_packet, (void *) pvt->t38_core) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot initialize my UDPTL structs\n");
			return SWITCH_STATUS_FALSE;
		}

		span_log_set_message_handler(&t38->logging, spanfax_log_message);
		span_log_set_message_handler(&t30->logging, spanfax_log_message);

		if (pvt->verbose) {
			span_log_set_level(&t38->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
			span_log_set_level(&t30->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
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

	 if (udptl_init(pvt->udptl_state, UDPTL_ERROR_CORRECTION_REDUNDANCY, 3, 3, 
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
	 

     span_log_set_message_handler(&pvt->t38_gateway_state->logging, spanfax_log_message);
     span_log_set_message_handler(&pvt->t38_core->logging, spanfax_log_message);

	 if (pvt->verbose) {
		 span_log_set_level(&pvt->t38_gateway_state->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
		 span_log_set_level(&pvt->t38_core->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
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

	t30_set_phase_e_handler(t30, phase_e_handler, pvt);

	t30_set_supported_image_sizes(t30,
								  T30_SUPPORT_US_LETTER_LENGTH | T30_SUPPORT_US_LEGAL_LENGTH | T30_SUPPORT_UNLIMITED_LENGTH
								| T30_SUPPORT_215MM_WIDTH | T30_SUPPORT_255MM_WIDTH | T30_SUPPORT_303MM_WIDTH);
	t30_set_supported_resolutions(t30,
								  T30_SUPPORT_STANDARD_RESOLUTION | T30_SUPPORT_FINE_RESOLUTION | T30_SUPPORT_SUPERFINE_RESOLUTION
								| T30_SUPPORT_R8_RESOLUTION | T30_SUPPORT_R16_RESOLUTION);

	if (pvt->disable_v17) {
		t30_set_supported_modems(t30, T30_SUPPORT_V29 | T30_SUPPORT_V27TER);
		switch_channel_set_variable(channel, "fax_v17_disabled", "1");
	} else {
		t30_set_supported_modems(t30, T30_SUPPORT_V29 | T30_SUPPORT_V27TER | T30_SUPPORT_V17);
		switch_channel_set_variable(channel, "fax_v17_disabled", "0");
	}

	if (pvt->use_ecm) {
		t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);
		t30_set_ecm_capability(t30, TRUE);
		switch_channel_set_variable(channel, "fax_ecm_requested", "1");
	} else {
		t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION);
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
        enabled = globals.enable_t38;
    }

    if (!(enabled && t38_options)) {
        /* if there is no t38_options the endpoint will refuse the transition */
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s NO T38 options detected.\n", switch_channel_get_name(channel));
        switch_channel_set_private(channel, "t38_options", NULL);
    } else {
        pvt->t38_mode = T38_MODE_NEGOTIATED;
        
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
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "ip = '%s'\n", t38_options->ip ? t38_options->ip : "Not specified");
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "port = %d\n", t38_options->port);

        /* Time to practice our negotiating skills, by editing the t38_options */

        /* use default IP/PORT */
        t38_options->ip = NULL;
        t38_options->port = 0;

        if (t38_options->T38FaxVersion > 3) {
            t38_options->T38FaxVersion = 3;
        }
        t38_options->T38MaxBitRate = (pvt->disable_v17)  ?  9600  :  14400;
        t38_options->T38FaxFillBitRemoval = 1;
        t38_options->T38FaxTranscodingMMR = 0;
        t38_options->T38FaxTranscodingJBIG = 0;
        t38_options->T38FaxRateManagement = "transferredTCF";
        t38_options->T38FaxMaxBuffer = 2000;
        t38_options->T38FaxMaxDatagram = LOCAL_FAX_MAX_DATAGRAM;
        if (strcasecmp(t38_options->T38FaxUdpEC, "t38UDPRedundancy") == 0
            ||
            strcasecmp(t38_options->T38FaxUdpEC, "t38UDPFEC") == 0) {
            t38_options->T38FaxUdpEC = "t38UDPRedundancy";
        } else {
            t38_options->T38FaxUdpEC = NULL;
        }
        t38_options->T38VendorInfo = "0 0 0";
    }

    if ((v = switch_channel_get_variable(channel, "fax_enable_t38_insist"))) {
        insist = switch_true(v);
    } else {
        insist = globals.enable_t38_insist;
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
        enabled = globals.enable_t38;
    }

    if (enabled) {
        if ((v = switch_channel_get_variable(channel, "fax_enable_t38_request"))) {
            enabled = switch_true(v);
        } else {
            enabled = globals.enable_t38_request;
        }
    }


    if ((v = switch_channel_get_variable(channel, "fax_enable_t38_insist"))) {
        insist = switch_true(v);
    } else {
        insist = globals.enable_t38_insist;
    }

    if (enabled) {
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
        
        /* use default IP/PORT */
        t38_options->ip = NULL;
        t38_options->port = 0;
        switch_channel_set_private(channel, "t38_options", t38_options);
        pvt->t38_mode = T38_MODE_REQUESTED;

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
		pvt->use_ecm = globals.use_ecm;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_disable_v17"))) {
		pvt->disable_v17 = switch_true(tmp);
	} else {
		pvt->disable_v17 = globals.disable_v17;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_verbose"))) {
		pvt->verbose = switch_true(tmp);
	} else {
		pvt->verbose = globals.verbose;
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_force_caller"))) {
		if (switch_true(tmp)) {
			pvt->caller = 1;
		} else {
			pvt->caller = 0;
		}
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_ident"))) {
		pvt->ident = switch_core_session_strdup(session, tmp);
	} else {
		pvt->ident = switch_core_session_strdup(session, globals.ident);
	}

	if ((tmp = switch_channel_get_variable(channel, "fax_header"))) {
		pvt->header = switch_core_session_strdup(session, tmp);
	} else {
		pvt->header = switch_core_session_strdup(session, globals.header);
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

	switch_core_session_get_read_impl(session, &read_impl);

	counter_increment();

    
    pvt = pvt_init(session, app_mode);
    

	buf = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);

	if (!zstr(data)) {
		pvt->filename = switch_core_session_strdup(session, data);
		if (pvt->app_mode == FUNCTION_TX) {
			if ((switch_file_exists(pvt->filename, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot send inexistant fax file [%s]\n",
								  switch_str_nil(pvt->filename));
				goto done;
			}
		}
	} else {
		if (pvt->app_mode == FUNCTION_TX) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Fax TX filename not set.\n");
			goto done;
		} else if (pvt->app_mode == FUNCTION_RX) {
			char *fname;
			const char *prefix;
			switch_time_t time;

			time = switch_time_now();

			if (!(prefix = switch_channel_get_variable(channel, "fax_prefix"))) {
				prefix = globals.prepend_string;
			}

			fname = switch_mprintf("%s/%s-%ld-%ld.tif", globals.spool, prefix, globals.total_sessions, time);
			if (fname) {
				pvt->filename = switch_core_session_strdup(session, fname);
				switch_safe_free(fname);
			} else {
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
	switch_channel_set_variable(channel, "jitterbuffer_msec", "0");


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


    /* If you have the means, I highly recommend picking one up. ...*/
    request_t38(pvt);


	while (switch_channel_ready(channel)) {
		int tx = 0;
		switch_status_t status;

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
                if (switch_channel_test_app_flag(channel, CF_APP_T38)) {
                    switch_core_session_message_t msg = { 0 };
                    pvt->t38_mode = T38_MODE_NEGOTIATED;
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
                if (switch_channel_test_app_flag(channel, CF_APP_T38)) {
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

                if (switch_test_flag(read_frame, SFF_UDPTL_PACKET)) {
                    /* now we know we can cast frame->packet to a udptl structure */
                    //switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "READ %d udptl bytes\n", read_frame->packetlen);
                    
                    udptl_rx_packet(pvt->udptl_state, read_frame->packet, read_frame->packetlen);


                }
            }
            continue;
        default:
            break;
        }

		/* Skip CNG frames (auto-generated by FreeSWITCH, usually) */
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

/* **************************************************************************
   CONFIGURATION
   ************************************************************************* */

void load_configuration(switch_bool_t reload)
{
	switch_xml_t xml = NULL, x_lists = NULL, x_list = NULL, cfg = NULL;

	if ((xml = switch_xml_open_cfg("fax.conf", &cfg, NULL))) {
		if ((x_lists = switch_xml_child(cfg, "settings"))) {
			for (x_list = switch_xml_child(x_lists, "param"); x_list; x_list = x_list->next) {
				const char *name = switch_xml_attr(x_list, "name");
				const char *value = switch_xml_attr(x_list, "value");

				if (zstr(name)) {
					continue;
				}

				if (zstr(value)) {
					continue;
				}

				if (!strcmp(name, "use-ecm")) {
					if (switch_true(value))
						globals.use_ecm = 1;
					else
						globals.use_ecm = 0;
				} else if (!strcmp(name, "verbose")) {
					if (switch_true(value))
						globals.verbose = 1;
					else
						globals.verbose = 0;
				} else if (!strcmp(name, "disable-v17")) {
					if (switch_true(value))
						globals.disable_v17 = 1;
					else
						globals.disable_v17 = 0;
				} else if (!strcmp(name, "enable-t38")) {
					if (switch_true(value)) {
						globals.enable_t38= 1;
                    } else {
						globals.enable_t38 = 0;
                    }
				} else if (!strcmp(name, "enable-t38-request")) {
					if (switch_true(value)) {
						globals.enable_t38_request = 1;
                    } else {
						globals.enable_t38_request = 0;
                    }
				} else if (!strcmp(name, "ident")) {
					strncpy(globals.ident, value, sizeof(globals.ident) - 1);
				} else if (!strcmp(name, "header")) {
					strncpy(globals.header, value, sizeof(globals.header) - 1);
				} else if (!strcmp(name, "spool-dir")) {
					globals.spool = switch_core_strdup(globals.pool, value);
				} else if (!strcmp(name, "file-prefix")) {
					globals.prepend_string = switch_core_strdup(globals.pool, value);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown parameter %s\n", name);
				}

			}
		}

		switch_xml_free(xml);
	}
}

void mod_spandsp_fax_event_handler(switch_event_t *event)
{
	load_configuration(1);
}


void mod_spandsp_fax_load(switch_memory_pool_t *pool)
{
	memset(&globals, 0, sizeof(globals));
    memset(&t38_state_list, 0, sizeof(t38_state_list));

    globals.pool = pool;

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&t38_state_list.mutex, SWITCH_MUTEX_NESTED, globals.pool);
    
    globals.enable_t38 = 1;
	globals.total_sessions = 0;
	globals.verbose = 1;
	globals.use_ecm = 1;
	globals.disable_v17 = 0;
	globals.prepend_string = switch_core_strdup(globals.pool, "fax");
	globals.spool = switch_core_strdup(globals.pool, "/tmp");
	strncpy(globals.ident, "SpanDSP Fax Ident", sizeof(globals.ident) - 1);
	strncpy(globals.header, "SpanDSP Fax Header", sizeof(globals.header) - 1);

	load_configuration(0);
}

void mod_spandsp_fax_shutdown(void)
{
	memset(&globals, 0, sizeof(globals));
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

    while (switch_channel_ready(channel) && switch_channel_up(other_channel) && !switch_channel_test_app_flag(channel, CF_APP_T38)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status) || pvt->done) {
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

    if (!switch_channel_test_app_flag(channel, CF_APP_T38)) {
        switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Could not negotiate T38\n", switch_channel_get_name(channel));
        goto end_unlock;
    }

    if (pvt->t38_mode == T38_MODE_REQUESTED) {
        spanfax_init(pvt, T38_GATEWAY_MODE);
        configure_t38(pvt);
        pvt->t38_mode = T38_MODE_NEGOTIATED;
    } else {
        if (negotiate_t38(pvt) != T38_MODE_NEGOTIATED) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Could not negotiate T38\n", switch_channel_get_name(channel));
            switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
            goto end_unlock;
        }

        spanfax_init(pvt, T38_GATEWAY_MODE);
    }

    /* This will change the rtp stack to udptl mode */
    msg.from = __FILE__;
    msg.message_id = SWITCH_MESSAGE_INDICATE_UDPTL_MODE;
    switch_core_session_receive_message(session, &msg);


    /* wake up the audio side */
    switch_channel_set_private(channel, "_t38_pvt", pvt);
    switch_channel_set_app_flag(other_channel, CF_APP_T38);


	while (switch_channel_ready(channel) && switch_channel_up(other_channel)) {

		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status) || pvt->done) {
			/* Our duty is over */
			goto end_unlock;
		}
        
        if (switch_test_flag(read_frame, SFF_CNG)) {
            continue;
        }
        
        if (switch_test_flag(read_frame, SFF_UDPTL_PACKET)) {
            udptl_rx_packet(pvt->udptl_state, read_frame->packet, read_frame->packetlen);
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
    switch_size_t tx;
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
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}

    while (switch_channel_ready(channel) && switch_channel_up(other_channel) && !switch_channel_test_app_flag(channel, CF_APP_T38)) {
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

    if (!switch_channel_test_app_flag(channel, CF_APP_T38)) {
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


		/* Skip CNG frames (auto-generated by FreeSWITCH, usually) */
		if (!switch_test_flag(read_frame, SFF_CNG)) {

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

    if (switch_channel_test_app_flag(channel, CF_APP_TAGGED)) {
        switch_channel_clear_app_flag(channel, CF_APP_TAGGED);
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


        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s starting gateway mode to %s\n", 
                          switch_channel_get_name(peer ? channel : other_channel),
                          switch_channel_get_name(peer ? other_channel : channel));
        
        
        switch_channel_clear_state_handler(channel, NULL);
        switch_channel_clear_state_handler(other_channel, NULL);

        switch_channel_add_state_handler(channel, &t38_gateway_state_handlers);
        switch_channel_add_state_handler(other_channel, &t38_gateway_state_handlers);

        switch_channel_set_app_flag(peer ? channel : other_channel, CF_APP_TAGGED);
        switch_channel_clear_app_flag(peer ? other_channel : channel, CF_APP_TAGGED);   
        
        switch_channel_set_flag(channel, CF_REDIRECT);
        switch_channel_set_state(channel, CS_RESET);

        switch_channel_set_flag(other_channel, CF_REDIRECT);
        switch_channel_set_state(other_channel, CS_RESET);
        
        switch_core_session_rwunlock(other_session);

    }
    
    return SWITCH_FALSE;
}



/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
