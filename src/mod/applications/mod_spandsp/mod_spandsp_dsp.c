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
 * The Original Code is FreeSWITCH mod_spandsp.
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
 * Christopher M. Rienzo <chris@rienzo.net>
 * mod_spandsp_dsp.c -- dsp applications provided by SpanDSP
 *
 */

#include "mod_spandsp.h"

typedef struct {
	switch_core_session_t *session;
	dtmf_rx_state_t *dtmf_detect;
	char last_digit;
	uint32_t samples;
	uint32_t last_digit_end;
	uint32_t digit_begin;
	uint32_t min_dup_digit_spacing;
} switch_inband_dtmf_t;

static void spandsp_dtmf_rx_realtime_callback(void *user_data, int code, int level, int delay)
{
	switch_inband_dtmf_t *pvt = (switch_inband_dtmf_t *)user_data;
	char digit = (char)code;
	if (digit) {
		/* prevent duplicate DTMF */
		if (digit != pvt->last_digit || (pvt->samples - pvt->last_digit_end) > pvt->min_dup_digit_spacing) {
			switch_dtmf_t dtmf;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "DTMF BEGIN DETECTED: [%c]\n", digit);
			pvt->last_digit = digit;
			dtmf.digit = digit;
			dtmf.duration = switch_core_default_dtmf_duration(0);
			switch_channel_queue_dtmf(switch_core_session_get_channel(pvt->session), &dtmf);
			pvt->digit_begin = pvt->samples;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "DUP DTMF DETECTED: [%c]\n", digit);
			pvt->last_digit_end = pvt->samples;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "DTMF END DETECTED: [%c], duration = %u ms\n", pvt->last_digit, (pvt->samples - pvt->digit_begin) / 8);
		pvt->last_digit_end = pvt->samples;
	}
}

static switch_bool_t inband_dtmf_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_inband_dtmf_t *pvt = (switch_inband_dtmf_t *) user_data;
	switch_frame_t *frame = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT: {
		const char *min_dup_digit_spacing_str = switch_channel_get_variable(channel, "min_dup_digit_spacing_ms");
		pvt->dtmf_detect = dtmf_rx_init(NULL, NULL, NULL);
		dtmf_rx_set_realtime_callback(pvt->dtmf_detect, spandsp_dtmf_rx_realtime_callback, pvt);
		if (!zstr(min_dup_digit_spacing_str)) {
			pvt->min_dup_digit_spacing = atoi(min_dup_digit_spacing_str) * 8;
		}
		break;
	}
	case SWITCH_ABC_TYPE_CLOSE:
		if (pvt->dtmf_detect) {
			dtmf_rx_free(pvt->dtmf_detect);
		}
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
		if ((frame = switch_core_media_bug_get_read_replace_frame(bug))) {
			pvt->samples += frame->samples;
			dtmf_rx(pvt->dtmf_detect, frame->data, frame->samples);
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



/* private channel data */
#define TONE_PRIVATE "mod_tone_detect_bug"

/**
 * Module global variables
 */
struct globals {
	/** Memory pool */
	switch_memory_pool_t *pool;
	/** Call progress tones mapped by descriptor name */
	switch_hash_t *tones;
	/** Default debug level */
	int debug;
};
typedef struct globals globals_t;
static globals_t globals;

/******************************************************************************
 * TONE DETECTION WITH CADENCE
 */

#define MAX_TONES 32

/**
 * Tone descriptor
 *
 * Defines a set of tones to look for
 */
struct tone_descriptor {
	/** The name of this descriptor set */
	const char *name;

	/** Describes the tones to watch */
	super_tone_rx_descriptor_t *spandsp_tone_descriptor;

	/** The mapping of tone id to key */
	const char *tone_keys[MAX_TONES];
};
typedef struct tone_descriptor tone_descriptor_t;

static switch_status_t tone_descriptor_create(tone_descriptor_t **descriptor, const char *name, switch_memory_pool_t *memory_pool);
static int tone_descriptor_add_tone(tone_descriptor_t *descriptor, const char *name);
static switch_status_t tone_descriptor_add_tone_element(tone_descriptor_t *descriptor, int tone_id, int freq1, int freq2, int min, int max);

/**
 * Tone detector
 *
 * Performs detection for the tones described by the descriptor.
 */
struct tone_detector {
	/** The tones to look for */
	tone_descriptor_t *descriptor;

	/** The detector */
	super_tone_rx_state_t *spandsp_detector;

	/** The detected tone */
	int detected_tone;

	/** The debug level */
	int debug;
};
typedef struct tone_detector tone_detector_t;

static switch_status_t tone_detector_create(tone_detector_t **detector, tone_descriptor_t *descriptor, switch_memory_pool_t *memory_pool);
static switch_bool_t tone_detector_process_buffer(tone_detector_t *detector, void *data, unsigned int len, const char **key);
static void tone_detector_destroy(tone_detector_t *detector);

static switch_bool_t callprogress_detector_process_buffer(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type);

/**
 * Allocate the tone descriptor
 *
 * @param descriptor the descriptor to create
 * @param name the descriptor name 
 * @param memory_pool the pool to use
 * @return SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t tone_descriptor_create(tone_descriptor_t **descriptor, const char *name, switch_memory_pool_t *memory_pool)
{
	tone_descriptor_t *ldescriptor = NULL;
	ldescriptor = switch_core_alloc(memory_pool, sizeof(tone_descriptor_t));
	if (!ldescriptor) {
		return SWITCH_STATUS_FALSE;
	}
	memset(ldescriptor, 0, sizeof(tone_descriptor_t));
	ldescriptor->name = switch_core_strdup(memory_pool, name);
	ldescriptor->spandsp_tone_descriptor = super_tone_rx_make_descriptor(NULL);
	*descriptor = ldescriptor;
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Add a tone to the tone descriptor
 *
 * @param descriptor the tone descriptor
 * @param key the tone key - this will be returned by the detector upon match
 * @return the tone ID
 */
static int tone_descriptor_add_tone(tone_descriptor_t *descriptor, const char *key)
{
	int id = super_tone_rx_add_tone(descriptor->spandsp_tone_descriptor);
	if (id >= MAX_TONES) {
		return -1;
	}
	descriptor->tone_keys[id] = key; 

	return id;
}

/**
 * Add a tone element to the tone descriptor
 *
 * @param descriptor the tone descriptor
 * @param tone_id the tone ID
 * @param freq1 the first frequency (0 if none)
 * @param freq2 the second frequency (0 if none)
 * @param min the minimum tone duration in ms
 * @param max the maximum tone duration in ms
 * @return SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t tone_descriptor_add_tone_element(tone_descriptor_t *descriptor, int tone_id, int freq1, int freq2, int min, int max)
{
	if (super_tone_rx_add_element(descriptor->spandsp_tone_descriptor, tone_id, freq1, freq2, min, max) == 0) {
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

/**
 * Process tone report callback from spandsp
 *
 * @param user_data the tone_detector
 * @param code the detected tone
 * @param level unused
 * @param delay unused
 */
static void tone_report_callback(void *user_data, int code, int level, int delay)
{
	tone_detector_t *detector = (tone_detector_t *)user_data;
	if (detector->debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Tone report: code = %d, level = %d, delay = %d\n", code, level, delay);
	}
	detector->detected_tone = code;
}

/**
 * Process tone segment report from spandsp (for debugging)
 * 
 * @param user_data the tone_detector
 * @param f1 the first frequency of the segment
 * @param f2 the second frequency of the segment
 * @param duration the duration of the segment
 */
static void tone_segment_callback(void *user_data, int f1, int f2, int duration)
{
	tone_detector_t *detector = (tone_detector_t *)user_data;
	if (detector->debug > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Tone segment: f1 = %d, f2 = %d, duration = %d\n", f1, f2, duration);
	}
}

/**
 * Allocate the tone detector
 *
 * @param detector the detector to create
 * @param descriptor the descriptor to use
 * @param memory_pool the pool to use
 * @return SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t tone_detector_create(tone_detector_t **detector, tone_descriptor_t *descriptor, switch_memory_pool_t *memory_pool)
{
	tone_detector_t *ldetector = NULL;
	ldetector = switch_core_alloc(memory_pool, sizeof(tone_detector_t));
	if (!ldetector) {
		return SWITCH_STATUS_FALSE;
	}
	memset(ldetector, 0, sizeof(tone_detector_t));
	ldetector->descriptor = descriptor;
	ldetector->debug = globals.debug;
	*detector = ldetector;
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Initialize detector.  Call when media bug starts detection
 *
 * @param detector the detector to initialize
 */
static void tone_detector_init(tone_detector_t *detector)
{
	detector->spandsp_detector = super_tone_rx_init(NULL, detector->descriptor->spandsp_tone_descriptor, tone_report_callback, detector);
	super_tone_rx_segment_callback(detector->spandsp_detector, tone_segment_callback);
}

/**
 * Process the buffer looking for tones
 *
 * @param data the data to process
 * @param len the amount of data to process
 * @param key the found tone key
 * @return SWITCH_TRUE if a tone was found
 */
static switch_bool_t tone_detector_process_buffer(tone_detector_t *detector, void *data, unsigned int len, const char **key)
{
	detector->detected_tone = -1;
	super_tone_rx(detector->spandsp_detector, data, len);
	if (detector->detected_tone != -1) {
		*key = detector->descriptor->tone_keys[detector->detected_tone];
		return SWITCH_TRUE;
	}
	return SWITCH_FALSE;
}

/**
 * Destroy the tone detector
 * @param detector the detector to destroy
 */
static void tone_detector_destroy(tone_detector_t *detector) 
{
	if (detector) {
		if (detector->spandsp_detector) {
			super_tone_rx_release(detector->spandsp_detector);
			super_tone_rx_free(detector->spandsp_detector);
			detector->spandsp_detector = NULL;
		}
	}
}

/**
 * Start call progress detection
 *
 * @param session the session to detect
 * @param name of the descriptor to use 
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t callprogress_detector_start(switch_core_session_t *session, const char *name)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	tone_detector_t *detector = NULL;
	tone_descriptor_t *descriptor = NULL;
	switch_media_bug_t *bug = NULL;

	/* are we already running? */
	bug = switch_channel_get_private(channel, TONE_PRIVATE);
	if (bug) {
		return SWITCH_STATUS_FALSE;
	}

	/* find the tone descriptor with the matching name and create the detector */
	descriptor = switch_core_hash_find(globals.tones, name);
	if (!descriptor) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "(%s) no tone descriptor defined with name '%s'.  Update configuration. \n", switch_channel_get_name(channel), name);
		return SWITCH_STATUS_FALSE;
	}
	tone_detector_create(&detector, descriptor, switch_core_session_get_pool(session));

	/* start listening for tones */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) Starting tone detection for '%s'\n", switch_channel_get_name(channel), name);
	switch_core_media_bug_add(session, "spandsp_tone_detect", NULL,
                              callprogress_detector_process_buffer, detector, 0 /* stop time */, SMBF_READ_STREAM, &bug);
	if (!bug) {
		return SWITCH_STATUS_FALSE;
	}
	switch_channel_set_private(channel, TONE_PRIVATE, bug);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Process a buffer of audio data for call progress tones
 *
 * @param bug the session's media bug 
 * @param user_data the detector
 * @param type the type of data available from the bug
 * @return SWITCH_TRUE
 */
static switch_bool_t callprogress_detector_process_buffer(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = { 0 };
	tone_detector_t *detector = (tone_detector_t *)user_data;
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);
	switch_channel_t *channel = switch_core_session_get_channel(session);

	frame.data = data;
	frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch(type) {
	case SWITCH_ABC_TYPE_INIT:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "(%s) initializing tone detector\n", switch_channel_get_name(channel));
		tone_detector_init(detector);
		break;
	case SWITCH_ABC_TYPE_READ:
	{
		const char *detected_tone = NULL;
		if (!detector->spandsp_detector) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "(%s) detector is destroyed\n", switch_channel_get_name(channel));
			return SWITCH_FALSE;
		}
		if (switch_core_media_bug_read(bug, &frame, SWITCH_TRUE) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "(%s) error reading frame\n", switch_channel_get_name(channel));
			return SWITCH_FALSE;
		}
		tone_detector_process_buffer(detector, frame.data, frame.samples, &detected_tone);
		if (detected_tone) {
			switch_event_t *event = NULL;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "(%s) DETECTED TONE: %s\n", switch_channel_get_name(channel), detected_tone);
			if (switch_event_create(&event, SWITCH_EVENT_DETECTED_TONE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detected-Tone", detected_tone);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
				switch_event_fire(&event);
			}
		}
		break;
	}
	case SWITCH_ABC_TYPE_WRITE:
		break;
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
		break;
	case SWITCH_ABC_TYPE_READ_PING:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		if (detector->spandsp_detector) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "(%s) destroying tone detector\n", switch_channel_get_name(channel));
			tone_detector_destroy(detector);
		}
		break;
	}
	return SWITCH_TRUE;
}

/**
 * Stop call progress detection
 * @param session the session to stop 
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t callprogress_detector_stop(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug = switch_channel_get_private(channel, TONE_PRIVATE);
	if (bug) {
		switch_core_media_bug_close(&bug);
		switch_channel_set_private(channel, TONE_PRIVATE, NULL);
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Process configuration file
 */
static switch_status_t do_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t cfg = NULL, xml = NULL, callprogress = NULL, xdescriptor = NULL;
	if (!(xml = switch_xml_open_cfg("spandsp.conf", &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not open spandsp.conf\n");
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	/* TODO make configuration param */
	globals.debug = 1;

	/* Configure call progress detector */
	if ((callprogress = switch_xml_child(cfg, "descriptors"))) {
		for (xdescriptor = switch_xml_child(callprogress, "descriptor"); xdescriptor; xdescriptor = switch_xml_next(xdescriptor)) {
			const char *name = switch_xml_attr(xdescriptor, "name");
			const char *tone_name = NULL;
			switch_xml_t tone = NULL, element = NULL;
			tone_descriptor_t *descriptor = NULL;

			/* create descriptor */
			if (zstr(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing <descriptor> name\n");
				return SWITCH_STATUS_FALSE;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Adding tone_descriptor: %s\n", name);
			if (tone_descriptor_create(&descriptor, name, globals.pool) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to allocate tone_descriptor: %s\n", name);
				return SWITCH_STATUS_FALSE;
			}
			switch_core_hash_insert(globals.tones, name, descriptor);

			/* add tones to descriptor */
			for (tone = switch_xml_child(xdescriptor, "tone"); tone; tone = switch_xml_next(tone)) {
				int id = 0;
				tone_name = switch_xml_attr(tone, "name");
				if (zstr(tone_name)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing <tone> name for <descriptor> %s\n", name);
					return SWITCH_STATUS_FALSE;
				}
				id = tone_descriptor_add_tone(descriptor, tone_name);
				if (id == -1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to add tone_descriptor: %s, tone: %s.  (too many tones)\n", name, tone_name);
					return SWITCH_STATUS_FALSE;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Adding tone_descriptor: %s, tone: %s(%d)\n", name, tone_name, id);
				/* add elements to tone */
				for (element = switch_xml_child(tone, "element"); element; element = switch_xml_next(element)) {
					const char *freq1_attr = switch_xml_attr(element, "freq1");
					const char *freq2_attr = switch_xml_attr(element, "freq2");
					const char *min_attr = switch_xml_attr(element, "min");
					const char *max_attr = switch_xml_attr(element, "max");
					int freq1, freq2, min, max;
					if (zstr(freq1_attr)) {
						freq1 = 0;
					} else {
						freq1 = atoi(freq1_attr);
					}
					if (zstr(freq2_attr)) {
						freq2 = 0;
					} else {
						freq2 = atoi(freq2_attr);
					}
					if (zstr(min_attr)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing min in <element> of <descriptor> %s <tone> %s(%d)\n", name, tone_name, id);
						return SWITCH_STATUS_FALSE;
					}
					min = atoi(min_attr);
					if (zstr(max_attr)) {
						max = 0;
					} else {
						max = atoi(max_attr);
					}
					/* check params */
					if ((freq1 < 0 || freq2 < 0 || min < 0 || max < 0) || (freq1 == 0 && min == 0 && max == 0)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid element param.\n");
						return SWITCH_STATUS_FALSE;
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Adding tone_descriptor: %s, tone: %s(%d), element (%d, %d, %d, %d)\n", name, tone_name, id, freq1, freq2, min, max);
					tone_descriptor_add_tone_element(descriptor, id, freq1, freq2, min, max);
				}
			}
		}
	}

done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

/**
 * Called when FreeSWITCH loads the module
 */
switch_status_t mod_spandsp_dsp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
{
	memset(&globals, 0, sizeof(globals_t));
	globals.pool = pool;

	switch_core_hash_init(&globals.tones, globals.pool);
	if (do_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Called when FreeSWITCH stops the module
 */
void mod_spandsp_dsp_shutdown(void)
{
	switch_core_hash_destroy(&globals.tones);
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
