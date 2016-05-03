/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Eric des Courtis <eric.des.courtis@benbria.com>
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
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Copyright (C) Benbria. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Piotr Gregor <piotrek.gregor gmail.com>:
 *
 * mod_avmd.c -- Advanced Voicemail Detection Module
 *
 * This module detects single frequency tones (used in voicemail to denote
 * the moment caller's voice is started to be recorded, aka. beep sounds,
 * beeps) using modified DESA-2 algorithm.
 */

#include <switch.h>
#include <g711.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef WIN32
#include <float.h>
#define ISNAN(x) (!!(_isnan(x)))
#else
#define ISNAN(x) (isnan(x))
#endif


#include "avmd_buffer.h"
#include "avmd_desa2_tweaked.h"
#include "avmd_sma_buf.h"
#include "avmd_options.h"

#include "avmd_fast_acosf.h"


/*! Calculate how many audio samples per ms based on the rate */
#define SAMPLES_PER_MS(r, m) ((r) / (1000/(m)))
/*! Minimum beep length */
#define BEEP_TIME (2)
/*! How often to evaluate the output of DESA-2 in ms */
#define SINE_TIME (2*0.125)
/*! How long in samples does DESA-2 results get evaluated */
#define SINE_LEN(r) SAMPLES_PER_MS((r), SINE_TIME)
/*! How long in samples is the minimum beep length */
#define BEEP_LEN(r) SAMPLES_PER_MS((r), BEEP_TIME)
/*! Number of points in DESA-2 sample */
#define P (5)
/*! Guesstimate frame length in ms */
#define FRAME_TIME (20)
/*! Length in samples of the frame (guesstimate) */
#define FRAME_LEN(r) SAMPLES_PER_MS((r), FRAME_TIME)
/*! Conversion to Hertz */
#define TO_HZ(r, f) (((r) * (f)) / (2.0 * M_PI))
/*! Minimum beep frequency in Hertz */
#define MIN_FREQUENCY (300.0)
/*! Minimum frequency as digital normalized frequency */
#define MIN_FREQUENCY_R(r) ((2.0 * M_PI * MIN_FREQUENCY) / (r))
/*! 
 * Maximum beep frequency in Hertz
 * Note: The maximum frequency the DESA-2 algorithm can uniquely
 * identify is 0.25 of the sampling rate. All the frequencies
 * below that level are detected unambiguously. This means 2kHz
 * for 8kHz audio. All the frequencies above 0.25 sampling rate
 * will be aliased to frequencies below that threshold,
 * i.e. OMEGA > PI/2 will be aliased to PI - OMEGA.
 * This is not a problem here as we are interested in detection
 * of any constant amplitude and frequency sine wave instead
 * of detection of particular frequency.
 * In case of DESA-1, frequencies up to 0.5 sampling rate are
 * identified uniquely.
 */
#define MAX_FREQUENCY (2500.0)
/*! Maximum frequency as digital normalized frequency */
#define MAX_FREQUENCY_R(r) ((2.0 * M_PI * MAX_FREQUENCY) / (r))
/* decrease this value to eliminate false positives */
#define VARIANCE_THRESHOLD (0.00025)

#ifdef AVMD_REQUIRE_CONTINUOUS_STREAK
    /* increase this value to eliminate false positives */
    #define SAMPLES_CONSECUTIVE_STREAK 15
#endif

/*! Syntax of the API call. */
#define AVMD_SYNTAX "<uuid> < start | stop | set [inbound|outbound|default] | load [inbound|outbound] | reload | show >"

/*! Number of expected parameters in api call. */
#define AVMD_PARAMS_MIN 1u
#define AVMD_PARAMS_MAX 2u

/* don't forget to update avmd_events_str table
 * if you modify this */
enum avmd_event
{
    AVMD_EVENT_BEEP = 0,
    AVMD_EVENT_SESSION_START = 1,
    AVMD_EVENT_SESSION_STOP = 2
};
/* This array MUST be NULL terminated! */
const char* avmd_events_str[] = {   [AVMD_EVENT_BEEP] =             "avmd::beep",
                                    [AVMD_EVENT_SESSION_START] =    "avmd::start",
                                    [AVMD_EVENT_SESSION_STOP] =     "avmd::stop",
                                    NULL                                            /* MUST be last and always here */
};

#define AVMD_CHAR_BUF_LEN 20u
#define AVMD_BUF_LINEAR_LEN 160u


/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_avmd_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_avmd_load);
SWITCH_MODULE_DEFINITION(mod_avmd, mod_avmd_load, mod_avmd_shutdown, NULL);
SWITCH_STANDARD_API(avmd_api_main);
SWITCH_STANDARD_APP(avmd_start_app);
SWITCH_STANDARD_APP(avmd_stop_app);
SWITCH_STANDARD_APP(avmd_start_function);

struct avmd_settings {
    uint8_t     debug;
    uint8_t     report_status;
    uint8_t     fast_math;
    uint8_t     require_continuous_streak;
    uint16_t    sample_n_continuous_streak;
    uint16_t    sample_n_to_skeep;
    uint8_t     simplified_estimation;
    uint8_t     inbound_channnel;
    uint8_t     outbound_channnel;
};

/*! Status of the beep detection */
typedef enum {
	BEEP_DETECTED,
	BEEP_NOTDETECTED
} avmd_beep_state_t;

/*! Data related to the current status of the beep */
typedef struct {
	avmd_beep_state_t beep_state;
	size_t last_beep;
} avmd_state_t;

/*! Type that holds session information pertinent to the avmd module. */
typedef struct {
	/*! Internal FreeSWITCH session. */
	switch_core_session_t   *session;
    switch_mutex_t          *mutex;
    struct avmd_settings    settings;
	uint32_t        rate;
	circ_buffer_t   b;
	sma_buffer_t    sma_b;
	sma_buffer_t    sqa_b;
	size_t          pos;
	double          f;
	/* freq_table_t ft; */
	avmd_state_t state;
	switch_time_t start_time;
#ifdef AVMD_REQUIRE_CONTINUOUS_STREAK
    size_t samples_streak; /* number of DESA samples in single streak without reset
                              needed to validate SMA estimator */
#endif
    size_t sample_count;
} avmd_session_t;

struct avmd_globals
{
    switch_mutex_t          *mutex;
    struct avmd_settings    settings;
    switch_memory_pool_t    *pool;
} avmd_globals;

static void avmd_process(avmd_session_t *session, switch_frame_t *frame);

static switch_bool_t avmd_callback(switch_media_bug_t * bug,
                                    void *user_data, switch_abc_type_t type);
static switch_status_t
avmd_register_all_events(void);

static void
avmd_unregister_all_events(void);

static void
avmd_fire_event(enum avmd_event type, switch_core_session_t *fs_s,
                    double freq, double v);

/* API [set default], reset to factory settings */
static void avmd_set_xml_default_configuration(switch_mutex_t *mutex);
/* API [set inbound], set inbound = 1, outbound = 0 */
static void avmd_set_xml_inbound_configuration(switch_mutex_t *mutex);
/* API [set outbound], set inbound = 0, outbound = 1 */
static void avmd_set_xml_outbound_configuration(switch_mutex_t *mutex);

/* API [reload], reload XML configuration data from RAM */
static switch_status_t avmd_load_xml_configuration(switch_mutex_t *mutex);
/* API [load inbound], reload + set inbound */
static switch_status_t avmd_load_xml_inbound_configuration(switch_mutex_t *mutex);
/* API [load outbound], reload + set outbound */
static switch_status_t avmd_load_xml_outbound_configuration(switch_mutex_t *mutex);

/* bind reloadxml callback */
static void
avmd_reloadxml_event_handler(switch_event_t *event);

/* API command */
static void
avmd_show(switch_stream_handle_t *stream, switch_mutex_t *mutex);

/*! \brief The avmd session data initialization function.
 * @param avmd_session A reference to a avmd session.
 * @param fs_session A reference to a FreeSWITCH session.
 */
static switch_status_t
init_avmd_session_data(avmd_session_t *avmd_session,
        switch_core_session_t *fs_session, switch_mutex_t *mutex)
{
    size_t          buf_sz;
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    if (mutex != NULL)
    {
        switch_mutex_lock(mutex);
    }

	/*! This is a worst case sample rate estimate */
	avmd_session->rate = 48000;
	INIT_CIRC_BUFFER(&avmd_session->b,
            (size_t)BEEP_LEN(avmd_session->rate),
            (size_t)FRAME_LEN(avmd_session->rate),
            fs_session);
    if (avmd_session->b.buf == NULL) {
            status =  SWITCH_STATUS_MEMERR;
            goto end;
    }
	avmd_session->session = fs_session;
	avmd_session->pos = 0;
	avmd_session->f = 0.0;
	avmd_session->state.last_beep = 0;
	avmd_session->state.beep_state = BEEP_NOTDETECTED;
#ifdef AVMD_REQUIRE_CONTINUOUS_STREAK
    avmd_session->samples_streak = SAMPLES_CONSECUTIVE_STREAK;
#endif
    memset(&avmd_session->settings, 0, sizeof(struct avmd_settings));
	switch_mutex_init(&avmd_session->mutex, SWITCH_MUTEX_DEFAULT,
            switch_core_session_get_pool(fs_session));
    avmd_session->sample_count = 0;

    buf_sz = BEEP_LEN((uint32_t)avmd_session->rate) / (uint32_t)SINE_LEN(avmd_session->rate);
    if (buf_sz < 1) {
            status = SWITCH_STATUS_MORE_DATA;
            goto end;
    }

    INIT_SMA_BUFFER(&avmd_session->sma_b, buf_sz, fs_session);
    if (avmd_session->sma_b.data == NULL) {
            status = SWITCH_STATUS_FALSE;
            goto end;
    }
    memset(avmd_session->sma_b.data, 0, sizeof(BUFF_TYPE) * buf_sz);

    INIT_SMA_BUFFER(&avmd_session->sqa_b, buf_sz, fs_session);
    if (avmd_session->sqa_b.data == NULL) {
            status = SWITCH_STATUS_FALSE;
            goto end;
    }
    memset(avmd_session->sqa_b.data, 0, sizeof(BUFF_TYPE) * buf_sz);
end:
    if (mutex != NULL)
    {
        switch_mutex_unlock(mutex);
    }
    return status;
}


/*! \brief The callback function that is called when new audio data becomes available.
 * @param bug A reference to the media bug.
 * @param user_data The session information for this call.
 * @param type The switch callback type.
 * @return The success or failure of the function.
 */
static switch_bool_t avmd_callback(switch_media_bug_t * bug,
                                        void *user_data, switch_abc_type_t type)
{
	avmd_session_t          *avmd_session;
#ifdef AVMD_OUTBOUND_CHANNEL
	switch_codec_t          *read_codec;
#endif
#ifdef AVMD_INBOUND_CHANNEL
    switch_codec_t          *write_codec;
#endif
	switch_frame_t          *frame;
    switch_core_session_t   *fs_session;


	avmd_session = (avmd_session_t *) user_data;
	if (avmd_session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			    "No avmd session assigned!\n");
		return SWITCH_FALSE;
	}
	fs_session = avmd_session->session;
	if (fs_session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			    "No FreeSWITCH session assigned!\n");
		return SWITCH_FALSE;
	}

	switch (type) {

	case SWITCH_ABC_TYPE_INIT:
#ifdef AVMD_OUTBOUND_CHANNEL
		read_codec = switch_core_session_get_read_codec(fs_session);
        if (read_codec == NULL) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING,
			    "No read codec assigned, default session rate to 8000 samples/s\n");
		    avmd_session->rate = 8000;
        } else {
            if (read_codec->implementation == NULL) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING,
			        "No read codec implementation assigned, default session rate to 8000 samples/s\n");
		        avmd_session->rate = 8000;
            } else {
                avmd_session->rate = read_codec->implementation->samples_per_second;
            }
        }
#endif
#ifdef AVMD_INBOUND_CHANNEL
		write_codec = switch_core_session_get_write_codec(fs_session);
        if (write_codec == NULL) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING,
			    "No write codec assigned, default session rate to 8000 samples/s\n");
		    avmd_session->rate = 8000;
        } else {
            if (write_codec->implementation == NULL) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING,
			        "No write codec implementation assigned, default session rate to 8000 samples/s\n");
		        avmd_session->rate = 8000;
            } else {
                avmd_session->rate = write_codec->implementation->samples_per_second;
            }
        }
#endif

		avmd_session->start_time = switch_micro_time_now();
		/* avmd_session->vmd_codec.channels = 
         *                  read_codec->implementation->number_of_channels; */
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),SWITCH_LOG_INFO,
			    "Avmd session initialized, [%u] samples/s\n", avmd_session->rate);
		break;

	case SWITCH_ABC_TYPE_READ_REPLACE:
		frame = switch_core_media_bug_get_read_replace_frame(bug);
		avmd_process(avmd_session, frame);
		return SWITCH_TRUE;

	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		frame = switch_core_media_bug_get_write_replace_frame(bug);
		avmd_process(avmd_session, frame);
		return SWITCH_TRUE;

	default:
		break;
	}

	return SWITCH_TRUE;
}

static switch_status_t
avmd_register_all_events(void)
{
    size_t idx = 0;
    const char *e = avmd_events_str[0];
    while (e != NULL)
    {
        if (switch_event_reserve_subclass(e) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                    "Couldn't register subclass [%s]!\n", e);
        return SWITCH_STATUS_TERM;
        }
        ++idx;
        e = avmd_events_str[idx];
    }
    return SWITCH_STATUS_SUCCESS;
}

static void
avmd_unregister_all_events(void)
{
    size_t idx = 0;
    const char *e = avmd_events_str[0];
    while (e != NULL)
    {
        switch_event_free_subclass(e);
        ++idx;
        e = avmd_events_str[idx];
    }
    return;
}

static void
avmd_fire_event(enum avmd_event type, switch_core_session_t *fs_s, double freq, double v)
{
    int res;
    switch_event_t      *event;
    switch_status_t     status;
    switch_event_t      *event_copy;
    char                buf[AVMD_CHAR_BUF_LEN];

    status = switch_event_create_subclass(&event,
            SWITCH_EVENT_CUSTOM, avmd_events_str[type]);
    if (status != SWITCH_STATUS_SUCCESS) {
        return;
    }
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID",
                    switch_core_session_get_uuid(fs_s));
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-command", "avmd");
    switch (type)
    {
        case AVMD_EVENT_BEEP:
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM,
                    "Beep-Status", "detected");
            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%f", freq);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s),
                        SWITCH_LOG_ERROR, "Frequency truncated [%s], [%d] attempted!\n",
                        buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "frequency",
                        "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM,
                    "frequency", buf);

            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%f", v);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s),
                        SWITCH_LOG_ERROR, "Error, truncated [%s], [%d] attempeted!\n",
                        buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM,
                        "variance", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "variance", buf);
            break;

        case AVMD_EVENT_SESSION_START:
        case AVMD_EVENT_SESSION_STOP:
            break;

        default:
            switch_event_destroy(&event);
            return;
    }

    if ((switch_event_dup(&event_copy, event)) != SWITCH_STATUS_SUCCESS) {
        return;
    }

    switch_core_session_queue_event(fs_s, &event);
    switch_event_fire(&event_copy);
    return;
}

int
avmd_parse_u8_user_input(const char *input, uint8_t *output,
                    uint8_t min, uint8_t max)
{
	char            *pCh;
    unsigned long   helper;
    helper = strtoul(input, &pCh, 10);
    if (helper < min || helper > UINT8_MAX || helper > max || (pCh == input) || (*pCh != '\0')) {
        return -1;
    }
    *output = (uint8_t) helper;
    return 0;
}

int
avmd_parse_u16_user_input(const char *input, uint16_t *output,
                    uint16_t min, uint16_t max)
{
	char            *pCh;
    unsigned long   helper;
    if (min > max) {
        return -1;
    }
    helper = strtoul(input, &pCh, 10);
    if (helper < min || helper > UINT16_MAX || helper > max || (pCh == input) || (*pCh != '\0')) {
        return -1;
    }
    *output = (uint16_t) helper;
    return 0;
}

static void avmd_set_xml_default_configuration(switch_mutex_t *mutex)
{
    if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

    avmd_globals.settings.debug = 0;
    avmd_globals.settings.report_status = 1;
    avmd_globals.settings.fast_math = 0;
    avmd_globals.settings.require_continuous_streak = 1;
    avmd_globals.settings.sample_n_continuous_streak = 15;
    avmd_globals.settings.sample_n_to_skeep = 6;
    avmd_globals.settings.simplified_estimation = 1;
    avmd_globals.settings.inbound_channnel = 0;
    avmd_globals.settings.outbound_channnel = 1;

    if (mutex != NULL) {
        switch_mutex_unlock(avmd_globals.mutex);
    }
	return;
}

static void
avmd_set_xml_inbound_configuration(switch_mutex_t *mutex)
{
    if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

    avmd_globals.settings.inbound_channnel = 1;
    avmd_globals.settings.outbound_channnel = 0;

    if (mutex != NULL) {
        switch_mutex_unlock(avmd_globals.mutex);
    }
	return;
}

static void
avmd_set_xml_outbound_configuration(switch_mutex_t *mutex)
{
    if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

    avmd_globals.settings.inbound_channnel = 0;
    avmd_globals.settings.outbound_channnel = 1;

    if (mutex != NULL) {
        switch_mutex_unlock(avmd_globals.mutex);
    }
	return;
}

static switch_status_t
avmd_load_xml_configuration(switch_mutex_t *mutex)
{
	switch_xml_t xml = NULL, x_lists = NULL, x_list = NULL, cfg = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

	if ((xml = switch_xml_open_cfg("avmd.conf", &cfg, NULL)) == NULL) {
        status = SWITCH_STATUS_TERM;
    } else {
		status = SWITCH_STATUS_SUCCESS;

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

				if (!strcmp(name, "debug")) {
						avmd_globals.settings.debug = switch_true(value) ? 1 : 0;
                } else if (!strcmp(name, "report_status")) {
						avmd_globals.settings.report_status = switch_true(value) ? 1 : 0;
				} else if (!strcmp(name, "fast_math")) {
						avmd_globals.settings.fast_math = switch_true(value) ? 1 : 0;
				} else if (!strcmp(name, "require_continuous_streak")) {
						avmd_globals.settings.require_continuous_streak = switch_true(value) ? 1 : 0;
				} else if (!strcmp(name, "sample_n_continuous_streak")) {
                    if(avmd_parse_u16_user_input(value,
                                &avmd_globals.settings.sample_n_continuous_streak, 0, UINT16_MAX) == -1)
                    {
                        status = SWITCH_STATUS_TERM;
                        goto done;
                    }
				} else if (!strcmp(name, "sample_n_to_skeep")) {
                    if(avmd_parse_u16_user_input(value,
                                &avmd_globals.settings.sample_n_to_skeep, 0, UINT16_MAX) == -1)
                    {
                        status = SWITCH_STATUS_TERM;
                        goto done;
                    }
				} else if (!strcmp(name, "simplified_estimation")) {
						avmd_globals.settings.simplified_estimation = switch_true(value) ? 1 : 0;
				} else if (!strcmp(name, "inbound_channel")) {
						avmd_globals.settings.inbound_channnel = switch_true(value) ? 1 : 0;
				} else if (!strcmp(name, "outbound_channel")) {
						avmd_globals.settings.outbound_channnel = switch_true(value) ? 1 : 0;
				}
			}
		}

 done:

		switch_xml_free(xml);
	}

    if (mutex != NULL) {
        switch_mutex_unlock(mutex);
    }

	return status;
}
static switch_status_t avmd_load_xml_inbound_configuration(switch_mutex_t *mutex)
{
    if (avmd_load_xml_configuration(mutex) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_TERM;
    }

    if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

    avmd_globals.settings.inbound_channnel = 1;
    avmd_globals.settings.outbound_channnel = 0;

    if (mutex != NULL) {
        switch_mutex_unlock(avmd_globals.mutex);
    }
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t avmd_load_xml_outbound_configuration(switch_mutex_t *mutex)
{
    if (avmd_load_xml_configuration(mutex) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_TERM;
    }

	if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

    avmd_globals.settings.inbound_channnel = 0;
    avmd_globals.settings.outbound_channnel = 1;

	if (mutex != NULL) {
        switch_mutex_unlock(avmd_globals.mutex);
    }
	return SWITCH_STATUS_SUCCESS;
}

static void
avmd_show(switch_stream_handle_t *stream, switch_mutex_t *mutex)
{
    const char *line = "=================================================================================================";
    if (stream == NULL) {
        return;
    }

    if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

    stream->write_function(stream, "\n\n");
    stream->write_function(stream, "%s\n\n", line);
    stream->write_function(stream, "%s\n", "Avmd global settings\n\n");
    stream->write_function(stream, "debug                   \t%u\n", avmd_globals.settings.debug);
    stream->write_function(stream, "report status           \t%u\n", avmd_globals.settings.report_status);
    stream->write_function(stream, "fast_math               \t%u\n", avmd_globals.settings.fast_math);
    stream->write_function(stream, "require continuous streak\t%u\n", avmd_globals.settings.require_continuous_streak);
    stream->write_function(stream, "sample n continuous streak\t%u\n", avmd_globals.settings.sample_n_continuous_streak);
    stream->write_function(stream, "sample n to skeep       \t%u\n", avmd_globals.settings.sample_n_to_skeep);
    stream->write_function(stream, "simplified estimation   \t%u\n", avmd_globals.settings.simplified_estimation);
    stream->write_function(stream, "inbound channel         \t%u\n", avmd_globals.settings.inbound_channnel);
    stream->write_function(stream, "outbound channel        \t%u\n", avmd_globals.settings.outbound_channnel);
    stream->write_function(stream, "\n\n");

    if (mutex != NULL) {
        switch_mutex_unlock(mutex);
    }
}

SWITCH_MODULE_LOAD_FUNCTION(mod_avmd_load)
{
#ifndef WIN32
    char    err[150];
    int     ret;
#endif

	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (avmd_register_all_events() != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                "Couldn't register avmd events!\n");
		return SWITCH_STATUS_TERM;
	}

    memset(&avmd_globals, 0, sizeof(avmd_globals));
    if (pool == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                "No memory pool assigned!\n");
        return SWITCH_STATUS_TERM;
    }
	switch_mutex_init(&avmd_globals.mutex, SWITCH_MUTEX_DEFAULT, pool);
    avmd_globals.pool = pool;

    if (avmd_load_xml_configuration(NULL) != SWITCH_STATUS_SUCCESS)
    {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                "Couldn't load XML configuration\n");
        return SWITCH_STATUS_TERM;
    }

	if ((switch_event_bind(modname, SWITCH_EVENT_RELOADXML, NULL,
                    avmd_reloadxml_event_handler, NULL) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                "Couldn't bind our reloadxml handler! Module will not react "
                "to changes made in XML configuration\n");
		/* Not so severe to prevent further loading, well - it depends, anyway */
	}

#ifndef WIN32
    if (avmd_globals.settings.fast_math == 1) {
        ret = init_fast_acosf();
        if (ret != 0) {
            strerror_r(errno, err, 150);
            switch (ret) {

                case -1:
	                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		                "Can't access file [%s], error [%s]\n",
                        ACOS_TABLE_FILENAME, err);
                    break;
                case -2:
	                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		                "Error creating file [%s], error [%s]\n",
                        ACOS_TABLE_FILENAME, err);
                    break;
                case -3:
	                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		                "Access rights are OK but can't open file [%s], error [%s]\n",
                        ACOS_TABLE_FILENAME, err);
                    break;
                case -4:
	                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		                "Access rights are OK but can't mmap file [%s], error [%s]\n",
                        ACOS_TABLE_FILENAME, err);
                    break;
                default:
	                switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR,
		                "Unknown error [%d] while initializing fast cos table [%s], "
                        "errno [%s]\n", ret, ACOS_TABLE_FILENAME, err);
                    return SWITCH_STATUS_TERM;
            }
            return SWITCH_STATUS_TERM;
        } else
	    switch_log_printf(
		    SWITCH_CHANNEL_LOG,
		    SWITCH_LOG_NOTICE,
		    "Advanced voicemail detection: fast math enabled, arc cosine table "
            "is [%s]\n", ACOS_TABLE_FILENAME
		    );
    }
#endif

	SWITCH_ADD_APP(app_interface, "avmd_start","Start avmd detection",
            "Start avmd detection", avmd_start_app, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "avmd_stop","Stop avmd detection",
            "Stop avmd detection", avmd_stop_app, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "avmd","Beep detection",
            "Advanced detection of voicemail beeps", avmd_start_function,
            AVMD_SYNTAX, SAF_NONE);

	SWITCH_ADD_API(api_interface, "avmd", "Voicemail beep detection",
            avmd_api_main, AVMD_SYNTAX);

	switch_console_set_complete("add avmd ::console::list_uuid ::[start:stop");
	switch_console_set_complete("add avmd set inbound");    /* set inbound = 1, outbound = 0 */
	switch_console_set_complete("add avmd set outbound");   /* set inbound = 0, outbound = 1 */
	switch_console_set_complete("add avmd set default");    /* restore to factory settings */
	switch_console_set_complete("add avmd load inbound");   /* reload + set inbound */
	switch_console_set_complete("add avmd load outbound");  /* reload + set outbound */
	switch_console_set_complete("add avmd reload");         /* reload XML (it loads from FS installation
                                                             * folder, not module's conf/autoload_configs */
	switch_console_set_complete("add avmd show");

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
		"Advanced voicemail detection enabled\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(avmd_start_app)
{
	switch_media_bug_t  *bug;
	switch_status_t     status;
	switch_channel_t    *channel;
	avmd_session_t      *avmd_session;
    switch_core_media_flag_t flags = 0;

	if (session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			    "FreeSWITCH is NULL! Please report to developers\n");
		return;
	}

	/* Get current channel of the session to tag the session
	* This indicates that our module is present
    * At this moment this cannot return NULL, it will either
    * succeed or assert failed, but we make ourself secure anyway */
	channel = switch_core_session_get_channel(session);
	if (channel == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
                "No channel for FreeSWITCH session! Please report this "
                "to the developers.\n");
        return;
	}

	/* Is this channel already set? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
	/* If yes */
	if (bug != NULL) {
		/* We have already started */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
                SWITCH_LOG_ERROR, "Avmd already started!\n");
        return;
	}

    switch_mutex_lock(avmd_globals.mutex);

    if (avmd_globals.settings.outbound_channnel == 1) {
        if (SWITCH_CALL_DIRECTION_OUTBOUND != switch_channel_direction(channel)) {
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
			    "Channel [%s] is not outbound!\n", switch_channel_get_name(channel));
        } else {
            flags |= SMBF_READ_REPLACE;
        }
    } else if (avmd_globals.settings.inbound_channnel == 1) {
        if (SWITCH_CALL_DIRECTION_INBOUND != switch_channel_direction(channel)) {
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
			    "Channel [%s] is not inbound!\n", switch_channel_get_name(channel));
        } else {
            flags |= SMBF_WRITE_REPLACE;
        }
    } else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			"Can't set direction for channel [%s]\n", switch_channel_get_name(channel));
        goto end;
    }


    if (avmd_globals.settings.outbound_channnel == 1) {
        if (switch_channel_test_flag(channel, CF_MEDIA_SET) == 0) {
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			    "Failed to start session. Channel [%s] has no codec assigned yet."
                " Please try again\n", switch_channel_get_name(channel));
            goto end;
        }
    }

	/* Allocate memory attached to this FreeSWITCH session for
	* use in the callback routine and to store state information */
	avmd_session = (avmd_session_t *) switch_core_session_alloc(
                                            session, sizeof(avmd_session_t));
    if (avmd_session == NULL) {
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			    "Can't allocate memory for avmd session!\n");
            goto end;
    }

	status = init_avmd_session_data(avmd_session, session, NULL);
    if (status != SWITCH_STATUS_SUCCESS) {
        switch (status) {
            case SWITCH_STATUS_MEMERR:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " Buffer error!\n");
            break;
            case SWITCH_STATUS_MORE_DATA:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " SMA buffer size is 0!\n");
                break;
            case SWITCH_STATUS_FALSE:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " SMA buffers error\n");
                break;
            default:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " Unknown error\n");
                break;

        }
        goto end;
    }

	/* Add a media bug that allows me to intercept the
	* reading leg of the audio stream */
	status = switch_core_media_bug_add(
		session,
		"avmd",
		NULL,
		avmd_callback,
		avmd_session,
		0,
		flags,
		&bug
		);

	/* If adding a media bug fails exit */
	if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
			SWITCH_LOG_ERROR, "Failed to add media bug!\n");
        goto end;
	}

	/* Set the avmd tag to detect an existing avmd media bug */
	switch_channel_set_private(channel, "_avmd_", bug);

	/* OK */
    avmd_fire_event(AVMD_EVENT_SESSION_START, session, 0, 0);
#ifdef AVMD_REPORT_STATUS
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
            "Avmd on channel [%s] started!\n", switch_channel_get_name(channel));
#endif

end:
    switch_mutex_unlock(avmd_globals.mutex);
    return;
}

SWITCH_STANDARD_APP(avmd_stop_app)
{
	switch_media_bug_t  *bug;
	switch_channel_t    *channel;

	if (session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			    "FreeSWITCH is NULL! Please report to developers\n");
		return;
	}

	/* Get current channel of the session to tag the session
	* This indicates that our module is present
    * At this moment this cannot return NULL, it will either
    * succeed or assert failed, but we make ourself secure anyway */
	channel = switch_core_session_get_channel(session);
	if (channel == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
                "No channel for FreeSWITCH session! Please report this "
                "to the developers.\n");
        return;
	}

	/* Is this channel already set? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
	/* If yes */
	if (bug == NULL) {
		/* We have not started avmd on this channel */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
                SWITCH_LOG_ERROR, "Stop failed - avmd has not yet been started"
                " on this channel [%s]!\n", switch_channel_get_name(channel));
        return;
	}

    switch_channel_set_private(channel, "_avmd_", NULL);
    switch_core_media_bug_remove(session, &bug);
    avmd_fire_event(AVMD_EVENT_SESSION_STOP, session, 0, 0);
#ifdef AVMD_REPORT_STATUS
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
            "Avmd on channel [%s] stopped!\n", switch_channel_get_name(channel));
#endif
    return;
}

/*! \brief FreeSWITCH application handler function.
 *  This handles calls made from applications such as LUA and the dialplan.
 */
SWITCH_STANDARD_APP(avmd_start_function)
{
	switch_media_bug_t  *bug;
	switch_channel_t    *channel;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
			    "YOU ARE USING DEPRECATED APP INTERFACE."
                " Please read documentation about new syntax\n");
	if (session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			    "No FreeSWITCH session assigned!\n");
		return;
    }

	channel = switch_core_session_get_channel(session);

	/* Is this channel already using avmd ? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
	/* If it is using avmd */
	if (bug != NULL) {
		/* If we have a stop remove audio bug */
		if (strcasecmp(data, "stop") == 0) {
			switch_channel_set_private(channel, "_avmd_", NULL);
			switch_core_media_bug_remove(session, &bug);
			return;
		}
		/* We have already started */
		switch_log_printf(
			SWITCH_CHANNEL_SESSION_LOG(session),
			SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");
		return;
	}
    avmd_start_app(session, NULL);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_avmd_shutdown)
{
#ifndef WIN32
	int res;
#endif

    switch_mutex_lock(avmd_globals.mutex);

    avmd_unregister_all_events();

#ifndef WIN32
    if (avmd_globals.settings.fast_math == 1) {
	    res = destroy_fast_acosf();
        if (res != 0) {
            switch (res) {
                case -1:
	                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		                "Failed unmap arc cosine table\n");
                    break;
                case -2:
	                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		                "Failed closing arc cosine table\n");
                    break;
                default:
                break;
            }
        }
    }
#endif

	switch_event_unbind_callback(avmd_reloadxml_event_handler);

    switch_mutex_unlock(avmd_globals.mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
		"Advanced voicemail detection disabled\n");

	return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH API handler function.
 *  This function handles API calls such as the ones
 *  from mod_event_socket and in some cases
 *  scripts such as LUA scripts.
 */
SWITCH_STANDARD_API(avmd_api_main)
{
	switch_media_bug_t  *bug;
	avmd_session_t      *avmd_session;
	switch_channel_t    *channel;
    int         argc;
    const char  *uuid, *uuid_dup;
    const char  *command;
    char        *dupped = NULL, *argv[AVMD_PARAMS_MAX + 1] = { 0 };
    switch_core_media_flag_t    flags = 0;
    switch_status_t             status = SWITCH_STATUS_SUCCESS;
    switch_core_session_t       *fs_session = NULL;

    switch_mutex_lock(avmd_globals.mutex);

	/* No command? Display usage */
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR, bad command!\n"
                "-USAGE: %s\n\n", AVMD_SYNTAX);
		goto end;
	}

	/* Duplicated contents of original string */
	dupped = strdup(cmd);
	switch_assert(dupped);
	/* Separate the arguments */
	argc = switch_separate_string((char*)dupped, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	/* If we don't have the expected number of parameters
	* display usage */
	if (argc < AVMD_PARAMS_MIN) {
		stream->write_function(stream, "-ERR, avmd takes [%u] min and [%u] max parameters!\n"
                "-USAGE: %s\n\n", AVMD_PARAMS_MIN, AVMD_PARAMS_MAX, AVMD_SYNTAX);
		goto end;
	}

    command = argv[0];
    if (strcasecmp(command, "reload") == 0) {
        if (avmd_load_xml_configuration(NULL) != SWITCH_STATUS_SUCCESS)
        {
            stream->write_function(stream, "-ERR, couldn't reload XML configuration\n");
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                "Couldn't reload XML configuration\n");
        }
        if (avmd_globals.settings.report_status == 1) {
            stream->write_function(stream, "+OK\n XML reloaded\n\n");
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                    "XML reloaded\n");
        }
        goto end;
    }
    if (strcasecmp(command, "load") == 0) {
        if (argc != 2) {
            stream->write_function(stream, "-ERR, load command takes 1 parameter!\n"
                "-USAGE: %s\n\n", AVMD_SYNTAX);
            goto end;
        }
        command = argv[1];
        if (strcasecmp(command, "inbound") == 0) {
            status = avmd_load_xml_inbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                if (status != SWITCH_STATUS_SUCCESS) {
                    stream->write_function(stream, "-ERR, couldn't load XML configuration\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                        "Couldn't load XML configuration\n");
                } else {
                    stream->write_function(stream, "+OK\n inbound "
                            "XML configuration loaded\n\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                            "Inbound XML configuration loaded\n");
                }
                goto end;
            }
        } else if (strcasecmp(command, "outbound") == 0) {
            status = avmd_load_xml_outbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                if (status != SWITCH_STATUS_SUCCESS) {
                    stream->write_function(stream, "-ERR, couldn't load XML configuration\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                        "Couldn't load XML configuration\n");
                } else {
                    stream->write_function(stream, "+OK\n outbound "
                            "XML configuration loaded\n\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                            "Outbound XML configuration loaded\n");
                }
                goto end;
            }
        } else {
            stream->write_function(stream, "-ERR, load command: bad syntax!\n"
                "-USAGE: %s\n\n", AVMD_SYNTAX);
        }
        goto end;
    }
    if (strcasecmp(command, "set") == 0) {
        if (argc != 2) {
            stream->write_function(stream, "-ERR, set command takes 1 parameter!\n"
                "-USAGE: %s\n\n", AVMD_SYNTAX);
            goto end;
        }
        command = argv[1];
        if (strcasecmp(command, "inbound") == 0) {
            avmd_set_xml_inbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                stream->write_function(stream, "+OK\n inbound "
                        "XML configuration loaded\n\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                        "Inbound XML configuration loaded\n");
            }
        } else if (strcasecmp(command, "outbound") == 0) {
            avmd_set_xml_outbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                stream->write_function(stream, "+OK\n outbound "
                        "XML configuration loaded\n\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                        "Outbound XML configuration loaded\n");
            }
        } else if (strcasecmp(command, "default") == 0) {
            avmd_set_xml_default_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                stream->write_function(stream, "+OK\n reset "
                        "to factory settings\n\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                        "Reset to factory settings\n");
            }
        } else {
            stream->write_function(stream, "-ERR, set command: bad syntax!\n"
                "-USAGE: %s\n\n", AVMD_SYNTAX);
        }
        goto end;
    }
    if (strcasecmp(command, "show") == 0) {
        avmd_show(stream, NULL);
        if (avmd_globals.settings.report_status == 1) {
            stream->write_function(stream, "+OK\n show\n\n");
        }
        goto end;
    }

	uuid = argv[0];
	command = argv[1];

	/* using uuid locate a reference to the FreeSWITCH session */
	fs_session = switch_core_session_locate(uuid);

	/* If the session was not found exit */
	if (fs_session == NULL) {
		stream->write_function(stream, "-ERR, no FreeSWITCH session for uuid [%s]!"
                "\n-USAGE: %s\n\n", uuid, AVMD_SYNTAX);
		goto end;
	}

	/* Get current channel of the session to tag the session
	* This indicates that our module is present
    * At this moment this cannot return NULL, it will either
    * succeed or assert failed, but we make ourself secure anyway */
	channel = switch_core_session_get_channel(fs_session);
	if (channel == NULL) {
		stream->write_function(stream, "-ERR, no channel for FreeSWITCH session [%s]!"
                "\n Please report this to the developers\n\n", uuid);
		goto end;
	}

	/* Is this channel already set? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
	/* If yes */
	if (bug != NULL) {
		/* If we have a stop remove audio bug */
		if (strcasecmp(command, "stop") == 0) {
            uuid_dup = switch_core_strdup(switch_core_session_get_pool(fs_session), uuid);
			switch_channel_set_private(channel, "_avmd_", NULL);
			switch_core_media_bug_remove(fs_session, &bug);
            avmd_fire_event(AVMD_EVENT_SESSION_STOP, fs_session, 0, 0);
            if (avmd_globals.settings.report_status == 1) {
			    stream->write_function(stream, "+OK\n [%s] [%s] stopped\n\n",
                        uuid_dup, switch_channel_get_name(channel));
		        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_INFO,
			        "Avmd on channel [%s] stopped!\n", switch_channel_get_name(channel));
            }
			goto end;
		}
        if (avmd_globals.settings.report_status == 1) {
		    /* We have already started */
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),
                    SWITCH_LOG_ERROR, "Avmd already started!\n");
		    stream->write_function(stream, "-ERR, avmd for FreeSWITCH session [%s]"
                    "\n already started\n\n", uuid);
        }
		goto end;
	}

	if (strcasecmp(command, "stop") == 0) {
        uuid_dup = switch_core_strdup(switch_core_session_get_pool(fs_session), uuid);
        stream->write_function(stream, "+ERR, avmd has not yet been started on\n"
                " [%s] [%s]\n\n", uuid_dup, switch_channel_get_name(channel));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR,
                "Stop failed - avmd has not yet been started on channel [%s]!\n",
                switch_channel_get_name(channel));
        goto end;
    }

    if (avmd_globals.settings.outbound_channnel == 1) {
        if (SWITCH_CALL_DIRECTION_OUTBOUND != switch_channel_direction(channel)) {
		    stream->write_function(stream, "-ERR, channel for FreeSWITCH session [%s]"
                    "\n is not outbound\n\n", uuid);
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING,
			    "Channel [%s] is not outbound!\n", switch_channel_get_name(channel));
        } else {
            flags |= SMBF_READ_REPLACE;
        }
    } else if (avmd_globals.settings.inbound_channnel == 1) {
        if (SWITCH_CALL_DIRECTION_INBOUND != switch_channel_direction(channel)) {
		    stream->write_function(stream, "-ERR, channel for FreeSWITCH session [%s]"
                    "\n is not inbound\n\n", uuid);
	    	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING,
			    "Channel [%s] is not inbound!\n", switch_channel_get_name(channel));
        } else {
            flags |= SMBF_WRITE_REPLACE;
        }
    } else {
		stream->write_function(stream, "-ERR, can't set direction for channel [%s]\n"
               " for FreeSWITCH session [%s]. Please check avmd configuration\n\n",
               switch_channel_get_name(channel), uuid);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR,
			"Can't set direction for channel [%s]\n", switch_channel_get_name(channel));
        goto end;
    }


    if (avmd_globals.settings.outbound_channnel == 1) {
        if (switch_channel_test_flag(channel, CF_MEDIA_SET) == 0) {
		    stream->write_function(stream, "-ERR, channel [%s] for FreeSWITCH session [%s]"
                    "\n has no read codec assigned yet. Please try again.\n\n",
                    switch_channel_get_name(channel), uuid);
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR,
			    "Failed to start session. Channel [%s] has no codec assigned yet."
                " Please try again\n", switch_channel_get_name(channel));
            goto end;
        }
    }

	/* If we don't see the expected start exit */
	if (strcasecmp(command, "start") != 0) {
		stream->write_function(stream, "-ERR, did you mean\n"
                " api avmd %s start ?\n-USAGE: %s\n\n", uuid, AVMD_SYNTAX);
		goto end;
	}

	/* Allocate memory attached to this FreeSWITCH session for
	* use in the callback routine and to store state information */
    avmd_session = (avmd_session_t *) switch_core_session_alloc(
                                            fs_session, sizeof(avmd_session_t));
    status = init_avmd_session_data(avmd_session, fs_session, NULL);
    if (status != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR, failed to initialize avmd session\n"
                " for FreeSWITCH session [%s]\n", uuid);
        switch (status) {
            case SWITCH_STATUS_MEMERR:
		        stream->write_function(stream, "-ERR, buffer error\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " Buffer error!\n");
            break;
            case SWITCH_STATUS_MORE_DATA:
		        stream->write_function(stream, "-ERR, SMA buffer size is 0\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " SMA buffer size is 0!\n");
                break;
            case SWITCH_STATUS_FALSE:
		        stream->write_function(stream, "-ERR, SMA buffer error\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " SMA buffers error\n");
                break;
            default:
		        stream->write_function(stream, "-ERR, unknown error\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),
                    SWITCH_LOG_ERROR, "Failed to init avmd session."
                    " Unknown error\n");
                break;
        }
		goto end;
    }

	/* Add a media bug that allows me to intercept the
	* reading leg of the audio stream */
	status = switch_core_media_bug_add(
		fs_session,
		"avmd",
		NULL,
		avmd_callback,
		avmd_session,
		0,
		flags,
		&bug
		);

	/* If adding a media bug fails exit */
	if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),
			SWITCH_LOG_ERROR, "Failed to add media bug!\n");
		stream->write_function(stream,
                "-ERR, [%s] failed to add media bug!\n\n", uuid);
		goto end;
	}

	/* Set the vmd tag to detect an existing vmd media bug */
	switch_channel_set_private(channel, "_avmd_", bug);

	/* OK */
    avmd_fire_event(AVMD_EVENT_SESSION_START, fs_session, 0, 0);
    if (avmd_globals.settings.report_status == 1) {
	    stream->write_function(stream, "+OK\n [%s] [%s] started!\n\n",
                uuid, switch_channel_get_name(channel));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_INFO,
                "Avmd on channel [%s] started!\n", switch_channel_get_name(channel));
        switch_assert(status == SWITCH_STATUS_SUCCESS);
    }
end:

	if (fs_session) {
		switch_core_session_rwunlock(fs_session);
	}

	switch_safe_free(dupped);

    switch_mutex_unlock(avmd_globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

/*! \brief Process one frame of data with avmd algorithm.
 * @param session An avmd session.
 * @param frame An audio frame.
 */
static void avmd_process(avmd_session_t *s, switch_frame_t *frame)
{
    switch_channel_t    *channel;

    circ_buffer_t       *b;
    size_t              pos;
    double              omega;
#ifdef AVMD_DEBUG
    double f;
#endif
    double      v;
    double      sma_digital_freq;
    uint32_t    sine_len_i;
    int         sample_to_skip_n = AVMD_SAMLPE_TO_SKIP_N;
    size_t      sample_n = 0;

    b = &s->b;

    /* If beep has already been detected skip the CPU heavy stuff */
    if (s->state.beep_state == BEEP_DETECTED) return;

    /* Precompute values used heavily in the inner loop */
    sine_len_i = (uint32_t) SINE_LEN(s->rate);
    //sine_len = (double)sine_len_i;
    //beep_len_i = BEEP_LEN(session->rate);

    channel = switch_core_session_get_channel(s->session);

    /* Insert frame of 16 bit samples into buffer */
    INSERT_INT16_FRAME(b, (int16_t *)(frame->data), frame->samples);
    s->sample_count += frame->samples;

    /* INNER LOOP -- OPTIMIZATION TARGET */
    pos = s->pos;
    while (sample_n < (frame->samples - P)) {
	/*for (pos = session->pos; pos < (GET_CURRENT_POS(b) - P); pos++) { */
		if ((sample_n % sine_len_i) == 0) {
			/* Get a desa2 frequency estimate every sine len */
			omega = avmd_desa2_tweaked(b, pos + sample_n);

			if (omega < -0.999999 || omega > 0.999999) {
#ifdef AVMD_DEBUG
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session),
                        SWITCH_LOG_DEBUG, "<<< AVMD RESET >>>\n");
#endif
				v = 99999.0;
#ifdef AVMD_REQUIRE_CONTINUOUS_STREAK
				RESET_SMA_BUFFER(&s->sma_b);
				RESET_SMA_BUFFER(&s->sqa_b);
                s->samples_streak = SAMPLES_CONSECUTIVE_STREAK;
                sample_to_skip_n = AVMD_SAMLPE_TO_SKIP_N;
#endif
			} else {
                if (isnan(omega)) {
#ifdef AVMD_DEBUG
	                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session),
                            SWITCH_LOG_DEBUG, "<<< AVMD, SKIP NaN >>>\n");
#endif
                    sample_to_skip_n = AVMD_SAMLPE_TO_SKIP_N;
                    goto loop_continue;
                }
                if (s->sma_b.pos > 0 && 
                        (fabs(omega - s->sma_b.data[s->sma_b.pos - 1]) < 0.00000001)) {
#ifdef AVMD_DEBUG
	                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_DEBUG,
                        "<<< AVMD, SKIP >>>\n");
#endif
                    goto loop_continue;
                }
#ifdef AVMD_DEBUG
	            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session),
                        SWITCH_LOG_DEBUG, "<<< AVMD omega [%f] >>>\n", omega);
#endif
                if (sample_to_skip_n > 0) {
                    sample_to_skip_n--;
                    goto loop_continue;
                }

                /* saturate */
                if (omega < -0.9999) {
                    omega = -0.9999;
                }
                if (omega > 0.9999) {
                    omega = 0.9999;
                }

                /* append */
				APPEND_SMA_VAL(&s->sma_b, omega);
				APPEND_SMA_VAL(&s->sqa_b, omega * omega);
#ifdef AVMD_REQUIRE_CONTINUOUS_STREAK
                if (s->samples_streak > 0) {
                    --s->samples_streak;
                }
#endif
				/* calculate variance (biased estimator) */
				v = s->sqa_b.sma - (s->sma_b.sma * s->sma_b.sma);
#ifdef AVMD_DEBUG
    #if !defined(WIN32) && defined(AVMD_FAST_MATH)
                f =  0.5 * (double) fast_acosf((float)omega);
                sma_digital_freq =  0.5 * (double) fast_acosf((float)s->sma_b.sma);
    #else
                f = 0.5 * acos(omega);
                sma_digital_freq =  0.5 * acos(s->sma_b.sma);
    #endif /* !WIN32 && AVMD_FAST_MATH */
    #ifdef AVMD_REQUIRE_CONTINUOUS_STREAK
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_DEBUG,
                    "<<< AVMD v[%.10f]\tomega[%f]\tf[%f] [%f]Hz\t\tsma[%f][%f]Hz\t\tsqa[%f]\t"
                    "streak[%zu] pos[%zu] sample_n[%zu] lpos[%zu] s[%zu]>>>\n",
                    v, omega, f, TO_HZ(s->rate, f), s->sma_b.sma,
                    TO_HZ(s->rate, sma_digital_freq), s->sqa_b.sma, s->samples_streak,
                    s->sma_b.pos, sample_n, s->sma_b.lpos, pos);
    #else
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_DEBUG,
                    "<<< AVMD v[%.10f]\tomega[%f]\tf[%f] [%f]Hz\t\tsma[%f][%f]Hz\t\tsqa[%f]\tpos[%zu]"
                    " sample_n[%zu] lpos[%zu] s[%zu]>>>\n", v, omega, f,
                    TO_HZ(s->rate, f), s->sma_b.sma, TO_HZ(s->rate, sma_digital_freq),
                    s->sqa_b.sma, s->sma_b.pos, sample_n, s->sma_b.lpos, pos);
    #endif  /* AVMD_REQUIRE_CONTINUOUS_STREAK */
#endif  /* AVMD_DEBUG */
			}

            /* DECISION */
            /* If variance is less than threshold
             * and we have at least two estimates and more than required by continuous
             * streak option then we have detection */
#ifdef AVMD_REQUIRE_CONTINUOUS_STREAK
			if (v < VARIANCE_THRESHOLD && (s->sma_b.lpos > 1) && (s->samples_streak == 0)) {
#else
			if (v < VARIANCE_THRESHOLD && (s->sma_b.lpos > 1)) {
#endif
    #if !defined(WIN32) && defined(AVMD_FAST_MATH)
                sma_digital_freq =  0.5 * (double) fast_acosf((float)s->sma_b.sma);
    #else
                sma_digital_freq =  0.5 * acos(s->sma_b.sma);
    #endif /* !WIN32 && AVMD_FAST_MATH */

				switch_channel_set_variable_printf(channel, "avmd_total_time",
                        "[%d]", (int)(switch_micro_time_now() - s->start_time) / 1000);
				switch_channel_execute_on(channel, "execute_on_avmd_beep");
				avmd_fire_event(AVMD_EVENT_BEEP, s->session, TO_HZ(s->rate, sma_digital_freq), v);

#ifdef AVMD_REPORT_STATUS
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_INFO,
                        "<<< AVMD - Beep Detected: f = [%f], variance = [%f] >>>\n",
                        TO_HZ(s->rate, sma_digital_freq), v);
#endif
				switch_channel_set_variable(channel, "avmd_detect", "TRUE");
				RESET_SMA_BUFFER(&s->sma_b);
				RESET_SMA_BUFFER(&s->sqa_b);
				s->state.beep_state = BEEP_DETECTED;

				goto done;
            }
		}
loop_continue:
        ++sample_n;
	}

done:
	s->pos += sample_n;
    s->pos &= b->mask;

    return;
}

static void
avmd_reloadxml_event_handler(switch_event_t *event)
{
    avmd_load_xml_configuration(avmd_globals.mutex);
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
