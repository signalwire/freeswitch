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
 * mod_fax.c -- Fax applications provided by SpanDSP
 *
 */

#include <switch.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>
#include <spandsp/version.h>

/*****************************************************************************
	OUR DEFINES AND STRUCTS
*****************************************************************************/

typedef enum {
	FUNCTION_TX,
	FUNCTION_RX
} application_mode_t;

typedef enum {
	T38_MODE,
	AUDIO_MODE
} transport_mode_t;


/* The global stuff */
static struct {
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;

	uint32_t total_sessions;

	short int use_ecm;
	short int verbose;
	short int disable_v17;
	char ident[20];
	char header[50];
	char *prepend_string;
	char *spool;
} globals;

struct pvt_s {
	switch_core_session_t *session;

	application_mode_t app_mode;

	fax_state_t *fax_state;
	t38_terminal_state_t *t38_state;

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

	/* UNUSED AT THE MOMENT
	   int          enable_t38_reinvite;
	 */

};

typedef struct pvt_s pvt_t;

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

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
					  "==============================================================================\n");

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

static switch_status_t spanfax_init(pvt_t *pvt, transport_mode_t trans_mode)
{

	switch_core_session_t *session;
	switch_channel_t *channel;
	fax_state_t *fax;
	t30_state_t *t30;

	session = (switch_core_session_t *) pvt->session;
	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel);


	switch (trans_mode) {
	case AUDIO_MODE:
		if (pvt->fax_state == NULL) {
			pvt->fax_state = (fax_state_t *) switch_core_session_alloc(pvt->session, sizeof(fax_state_t));
		} else {
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
		break;
	case T38_MODE:
		/* 
		   Here goes the T.38 SpanDSP initializing functions 
		   T.38 will require a big effort as it needs a different approach
		   but the pieces are already in place
		 */
	default:
		assert(0);				/* Whaaat ? */
		break;
	}							/* Switch trans mode */

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t spanfax_destroy(pvt_t *pvt)
{
	int terminate;
	t30_state_t *t30;

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

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************
	MAIN FAX PROCESSING
*****************************************************************************/

void process_fax(switch_core_session_t *session, const char *data, application_mode_t app_mode)
{
	pvt_t *pvt;
	const char *tmp;

	switch_channel_t *channel;
	switch_codec_t read_codec = { 0 };
	switch_codec_t write_codec = { 0 };
	switch_frame_t *read_frame = { 0 };
	switch_frame_t write_frame = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	int16_t *buf = NULL;
	switch_core_session_get_read_impl(session, &read_impl);

	/* make sure we have a valid channel when starting the FAX application */
	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	if (!switch_channel_media_ready(channel)) {
		switch_channel_answer(channel);
	}

	/* Allocate our structs */
	pvt = switch_core_session_alloc(session, sizeof(pvt_t));

	counter_increment();

	if (!pvt) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot allocate application private data\n");
		return;
	} else {
		memset(pvt, 0, sizeof(pvt_t));

		pvt->session = session;
		pvt->app_mode = app_mode;

		pvt->tx_page_start = -1;
		pvt->tx_page_end = -1;

		if (pvt->app_mode == FUNCTION_TX) {
			pvt->caller = 1;
		} else if (pvt->app_mode == FUNCTION_RX) {
			pvt->caller = 0;
		} else {
			assert(0);			/* UH ? */
		}
	}


	buf = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);
	if (!buf) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot allocate application buffer data\n");
		return;
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

		/* Skip CNG frames (auto-generated by FreeSWITCH, usually) */
		if (!switch_test_flag(read_frame, SFF_CNG)) {
			/* pass the new incoming audio frame to the fax_rx function */
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
			/* something weird has happened */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
							  "Cannot write frame [datalen: %d, samples: %d]\n", write_frame.datalen, write_frame.samples);
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

static void event_handler(switch_event_t *event)
{
	load_configuration(1);
}






typedef struct {
	switch_core_session_t *session;
    dtmf_rx_state_t *dtmf_detect;
} switch_inband_dtmf_t;

static switch_bool_t inband_dtmf_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_inband_dtmf_t *pvt = (switch_inband_dtmf_t *) user_data;
	switch_frame_t *frame = NULL;
	char digit_str[80];
	switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
        pvt->dtmf_detect = dtmf_rx_init(NULL, NULL, NULL);
		break;
	case SWITCH_ABC_TYPE_CLOSE:
        if (pvt->dtmf_detect) {
            dtmf_rx_free(pvt->dtmf_detect);
        }
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
		if ((frame = switch_core_media_bug_get_read_replace_frame(bug))) {
			dtmf_rx(pvt->dtmf_detect, frame->data, frame->samples);
			dtmf_rx_get(pvt->dtmf_detect, digit_str, sizeof(digit_str));
			if (digit_str[0]) {
				char *p = digit_str;
				while (p && *p) {
					switch_dtmf_t dtmf;
					dtmf.digit = *p;
					dtmf.duration = switch_core_default_dtmf_duration(0);
					switch_channel_queue_dtmf(channel, &dtmf);
					p++;
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_DEBUG, "DTMF DETECTED: [%s]\n",
								  digit_str);
			}
			switch_core_media_bug_set_read_replace_frame(bug, frame);
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

switch_status_t spandsp_stop_inband_dtmf_session(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if ((bug = switch_channel_get_private(channel, "dtmf"))) {
		switch_channel_set_private(channel, "dtmf", NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

switch_status_t spandsp_inband_dtmf_session(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_inband_dtmf_t *pvt;
	switch_codec_implementation_t read_impl = { 0 };

	switch_core_session_get_read_impl(session, &read_impl);

	if (!(pvt = switch_core_session_alloc(session, sizeof(*pvt)))) {
		return SWITCH_STATUS_MEMERR;
	}


   	pvt->session = session;


	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = switch_core_media_bug_add(session, "spandsp_dtmf_detect", NULL,
                                            inband_dtmf_callback, pvt, 0, SMBF_READ_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_channel_set_private(channel, "dtmf", bug);

	return SWITCH_STATUS_SUCCESS;
}














/* **************************************************************************
   FREESWITCH MODULE DEFINITIONS
   ************************************************************************* */

#define SPANFAX_RX_USAGE "<filename>"
#define SPANFAX_TX_USAGE "<filename>"

SWITCH_MODULE_LOAD_FUNCTION(mod_fax_init);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fax_shutdown);
SWITCH_MODULE_DEFINITION(mod_fax, mod_fax_init, mod_fax_shutdown, NULL);

static switch_event_node_t *NODE = NULL;

SWITCH_STANDARD_APP(spanfax_tx_function)
{
	process_fax(session, data, FUNCTION_TX);
}

SWITCH_STANDARD_APP(spanfax_rx_function)
{
	process_fax(session, data, FUNCTION_RX);
}


SWITCH_STANDARD_APP(dtmf_session_function)
{
	spandsp_inband_dtmf_session(session);
}

SWITCH_STANDARD_APP(stop_dtmf_session_function)
{
	spandsp_stop_inband_dtmf_session(session);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_fax_init)
{
	switch_application_interface_t *app_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "rxfax", "FAX Receive Application", "FAX Receive Application", spanfax_rx_function, SPANFAX_RX_USAGE,
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "txfax", "FAX Transmit Application", "FAX Transmit Application", spanfax_tx_function, SPANFAX_TX_USAGE,
				   SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "spandsp_stop_dtmf", "stop inband dtmf", "Stop detecting inband dtmf.", stop_dtmf_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "spandsp_start_dtmf", "Detect dtmf", "Detect inband dtmf on the session", dtmf_session_function, "", SAF_MEDIA_TAP);

	memset(&globals, 0, sizeof(globals));
	switch_core_new_memory_pool(&globals.pool);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	globals.total_sessions = 0;
	globals.verbose = 1;
	globals.use_ecm = 1;
	globals.disable_v17 = 0;
	globals.prepend_string = switch_core_strdup(globals.pool, "fax");
	globals.spool = switch_core_strdup(globals.pool, "/tmp");
	strncpy(globals.ident, "SpanDSP Fax Ident", sizeof(globals.ident) - 1);
	strncpy(globals.header, "SpanDSP Fax Header", sizeof(globals.header) - 1);

	load_configuration(0);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind our reloadxml handler!\n");
		/* Not such severe to prevent loading */
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_fax loaded, using spandsp library version [%s]\n", SPANDSP_RELEASE_DATETIME_STRING);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fax_shutdown)
{
	switch_memory_pool_t *pool = globals.pool;

	switch_event_unbind(&NODE);

	switch_core_destroy_memory_pool(&pool);
	memset(&globals, 0, sizeof(globals));

	return SWITCH_STATUS_UNLOAD;
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
