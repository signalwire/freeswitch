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
 * Piotr Gregor     <piotrgregor@rsyncme.org>
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
#include <float.h>

#ifdef WIN32
    #include <float.h>
    #define ISNAN(x) (!!(_isnan(x)))
    #define ISINF(x) (isinf(x))
#else
    int __isnan(double);
	int __isinf(double);
    #define ISNAN(x) (__isnan(x))
    #define ISINF(x) (__isinf(x))
#endif

#include "avmd_buffer.h"
#include "avmd_desa2_tweaked.h"
#include "avmd_sma_buf.h"
#include "avmd_options.h"
#include "avmd_fir.h"

#include "avmd_fast_acosf.h"


/*! Calculate how many audio samples per ms based on the rate */
#define AVMD_SAMPLES_PER_MS(r, m) ((r) / (1000/(m)))
/*! Minimum beep length */
#define AVMD_BEEP_TIME (2)
/*! How often to evaluate the output of DESA-2 in ms */
#define AVMD_SINE_TIME (1*0.125)
/*! How long in samples does DESA-2 results get evaluated */
#define AVMD_SINE_LEN(r) AVMD_SAMPLES_PER_MS((r), AVMD_SINE_TIME)
/*! How long in samples is the minimum beep length */
#define AVMD_BEEP_LEN(r) AVMD_SAMPLES_PER_MS((r), AVMD_BEEP_TIME)
/*! Number of points in DESA-2 sample */
#define AVMD_P (5)
/*! Guesstimate frame length in ms */
#define AVMD_FRAME_TIME (20)
/*! Length in samples of the frame (guesstimate) */
#define AVMD_FRAME_LEN(r) AVMD_SAMPLES_PER_MS((r), AVMD_FRAME_TIME)
/*! Conversion to Hertz */
#define AVMD_TO_HZ(r, f) (((r) * (f)) / (2.0 * M_PI))
/*! Minimum absolute pressure/amplitude */
#define AVMD_MIN_AMP (17.0)
/*! Minimum beep frequency in Hertz */
#define AVMD_MIN_FREQUENCY (440.0)
/*! Minimum frequency as digital normalized frequency */
#define AVMD_MIN_FREQUENCY_R(r) ((2.0 * M_PI * AVMD_MIN_FREQUENCY) / (r))
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
#define AVMD_MAX_FREQUENCY (2000.0)
/*! Maximum frequency as digital normalized frequency */
#define AVMD_MAX_FREQUENCY_R(r) ((2.0 * M_PI * AVMD_MAX_FREQUENCY) / (r))
#define AVMD_VARIANCE_RSD_THRESHOLD (0.000025)
#define AVMD_AMPLITUDE_RSD_THRESHOLD (0.0148)

/*! Syntax of the API call. */
#define AVMD_SYNTAX "<uuid> < start | stop | set [inbound|outbound|default] | load [inbound|outbound] | reload | show >"

/*! Number of expected parameters in api call. */
#define AVMD_PARAMS_API_MIN 1u
#define AVMD_PARAMS_API_MAX 2u
#define AVMD_PARAMS_APP_MAX 30u
#define AVMD_PARAMS_APP_START_MIN 0u
#define AVMD_PARAMS_APP_START_MAX 20u

#define AVMD_READ_REPLACE	0
#define AVMD_WRITE_REPLACE	1


/* don't forget to update avmd_events_str table if you modify this */
enum avmd_event
{
    AVMD_EVENT_BEEP = 0,
    AVMD_EVENT_SESSION_START = 1,
    AVMD_EVENT_SESSION_STOP = 2
};
/* This array MUST be NULL terminated! */
const char* avmd_events_str[] = {
    [AVMD_EVENT_BEEP] =             "avmd::beep",
    [AVMD_EVENT_SESSION_START] =    "avmd::start",
    [AVMD_EVENT_SESSION_STOP] =     "avmd::stop",
    NULL                                            /* MUST be last and always here */
};

#define AVMD_CHAR_BUF_LEN 20u
#define AVMD_BUF_LINEAR_LEN 160u

enum avmd_app
{
    AVMD_APP_START_APP = 0,
    AVMD_APP_STOP_APP = 1,
    AVMD_APP_START_FUNCTION = 2     /* deprecated since version 1.6.8 */
};

enum avmd_detection_mode
{
    AVMD_DETECT_AMP = 0,
    AVMD_DETECT_FREQ = 1,
    AVMD_DETECT_BOTH = 2,
    AVMD_DETECT_NONE = 3
};

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
    uint16_t    sample_n_to_skip;
    uint8_t     require_continuous_streak_amp;
    uint16_t    sample_n_continuous_streak_amp;
    uint8_t     simplified_estimation;
    uint8_t     inbound_channnel;
    uint8_t     outbound_channnel;
    enum avmd_detection_mode mode;
    uint8_t     detectors_n;
    uint8_t     detectors_lagged_n;
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

struct avmd_session;
typedef struct avmd_session avmd_session_t;

struct avmd_buffer {
    sma_buffer_t    sma_b;
    sma_buffer_t    sqa_b;

    sma_buffer_t    sma_b_fir;
    sma_buffer_t    sqa_b_fir;

    sma_buffer_t    sma_amp_b;
    sma_buffer_t    sqa_amp_b;

    uint8_t         resolution;
    uint8_t         offset;
    double          amplitude_max;
    size_t samples_streak, samples_streak_amp; /* number of DESA samples in single streak without reset needed to validate SMA estimator */
};

struct avmd_detector {
    switch_thread_t *thread;
    switch_mutex_t  *mutex;
    uint8_t                     flag_processing_done;
    uint8_t                     flag_should_exit;
    enum avmd_detection_mode    result;
    switch_thread_cond_t        *cond_start_processing;
    struct avmd_buffer          buffer;
    avmd_session_t              *s;
    size_t                      samples;
    uint8_t                     idx;
    uint8_t                     lagged, lag;
};

/*! Type that holds avmd detection session information. */
struct avmd_session {
    switch_core_session_t   *session;
    switch_mutex_t          *mutex;
    struct avmd_settings    settings;
    uint32_t        rate;
    circ_buffer_t   b;
    size_t          pos;
    double          f;
    avmd_state_t    state;
    switch_time_t   start_time, stop_time, detection_start_time, detection_stop_time;
    size_t          frame_n;
    uint8_t         frame_n_to_skip;

    switch_mutex_t          *mutex_detectors_done;
    switch_thread_cond_t    *cond_detectors_done;
    struct avmd_detector    *detectors;
};

static struct avmd_globals
{
    switch_mutex_t          *mutex;
    struct avmd_settings    settings;
    switch_memory_pool_t    *pool;
    size_t                  session_n;
} avmd_globals;

static void avmd_process(avmd_session_t *session, switch_frame_t *frame, uint8_t direction);

static switch_bool_t avmd_callback(switch_media_bug_t * bug, void *user_data, switch_abc_type_t type);
static switch_status_t avmd_register_all_events(void);

static void avmd_unregister_all_events(void);

static void avmd_fire_event(enum avmd_event type, switch_core_session_t *fs_s, double freq, double v_freq, double amp, double v_amp, avmd_beep_state_t beep_status, uint8_t info,
        switch_time_t detection_start_time, switch_time_t detection_stop_time, switch_time_t start_time, switch_time_t stop_time, uint8_t resolution, uint8_t offset, uint8_t idx);

static enum avmd_detection_mode avmd_process_sample(avmd_session_t *s, circ_buffer_t *b, size_t sample_n, size_t pos, struct avmd_detector *d);

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
static void avmd_reloadxml_event_handler(switch_event_t *event);

/* API command */
static void avmd_show(switch_stream_handle_t *stream, switch_mutex_t *mutex);

static void* SWITCH_THREAD_FUNC
avmd_detector_func(switch_thread_t *thread, void *arg);

static uint8_t
avmd_detection_in_progress(avmd_session_t *s);

static switch_status_t avmd_launch_threads(avmd_session_t *s) {
	uint8_t                 idx;
	struct avmd_detector    *d;
	switch_threadattr_t     *thd_attr = NULL;

	idx = 0;
	while (idx < s->settings.detectors_n) {
		d = &s->detectors[idx];
		d->flag_processing_done = 1;
		d->flag_should_exit = 0;
		d->result = AVMD_DETECT_NONE;
		d->lagged = 0;
		d->lag = 0;
		switch_threadattr_create(&thd_attr, avmd_globals.pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		if (switch_thread_create(&d->thread, thd_attr, avmd_detector_func, d, switch_core_session_get_pool(s->session)) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}

		if (s->settings.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "AVMD: started thread idx=%u\n", idx);
		}

		++idx;
	}

	idx = 0;
	while (idx < s->settings.detectors_lagged_n) {
		d = &s->detectors[s->settings.detectors_n + idx];
		d->flag_processing_done = 1;
		d->flag_should_exit = 0;
		d->result = AVMD_DETECT_NONE;
		d->lagged = 1;
		d->lag = idx + 1;
		switch_threadattr_create(&thd_attr, avmd_globals.pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		if (switch_thread_create(&d->thread, thd_attr, avmd_detector_func, d, switch_core_session_get_pool(s->session)) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}

		if (s->settings.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "AVMD: started lagged thread idx=%u\n", s->settings.detectors_n + idx);
		}

		++idx;
	}

	return SWITCH_STATUS_SUCCESS;
}

static void avmd_join_threads(avmd_session_t *s) {
    uint8_t                 idx;
    struct avmd_detector    *d;
    switch_status_t         status;

    idx = 0;
    while (idx < s->settings.detectors_n) {
        d = &s->detectors[idx];
        switch_mutex_lock(d->mutex);
        if (d->thread != NULL) {
            d->flag_should_exit = 1;
            d->samples = 0;
            switch_thread_cond_signal(d->cond_start_processing);
            switch_mutex_unlock(d->mutex);
            switch_thread_join(&status, d->thread);
            d->thread = NULL;
            switch_mutex_destroy(d->mutex);
            switch_thread_cond_destroy(d->cond_start_processing);
        } else {
            switch_mutex_unlock(d->mutex);
        }
        ++idx;
    }
    idx = 0;
    while (idx < s->settings.detectors_lagged_n) {
        d = &s->detectors[s->settings.detectors_n + idx];
        switch_mutex_lock(d->mutex);
        if (d->thread != NULL) {
            d->flag_should_exit = 1;
            d->samples = 0;
            switch_thread_cond_signal(d->cond_start_processing);
            switch_mutex_unlock(d->mutex);
            switch_thread_join(&status, d->thread);
            d->thread = NULL;
            switch_mutex_destroy(d->mutex);
            switch_thread_cond_destroy(d->cond_start_processing);
        } else {
            switch_mutex_unlock(d->mutex);
        }
        ++idx;
    }
}

static switch_status_t avmd_init_buffer(struct avmd_buffer *b, size_t buf_sz, uint8_t resolution, uint8_t offset, switch_core_session_t *fs_session) {
    INIT_SMA_BUFFER(&b->sma_b, buf_sz, fs_session);
    if (b->sma_b.data == NULL) {
        return SWITCH_STATUS_FALSE;
    }
    memset(b->sma_b.data, 0, sizeof(BUFF_TYPE) * buf_sz);

    INIT_SMA_BUFFER(&b->sqa_b, buf_sz, fs_session);
    if (b->sqa_b.data == NULL) {
        return SWITCH_STATUS_FALSE;
    }
    memset(b->sqa_b.data, 0, sizeof(BUFF_TYPE) * buf_sz);

    INIT_SMA_BUFFER(&b->sma_b_fir, buf_sz, fs_session);
    if (b->sma_b_fir.data == NULL) {
        return SWITCH_STATUS_FALSE;
    }
    memset(b->sma_b_fir.data, 0, sizeof(BUFF_TYPE) * buf_sz);

    INIT_SMA_BUFFER(&b->sqa_b_fir, buf_sz, fs_session);
    if (b->sqa_b_fir.data == NULL) {
        return SWITCH_STATUS_FALSE;
    }
    memset(b->sqa_b_fir.data, 0, sizeof(BUFF_TYPE) * buf_sz);

    INIT_SMA_BUFFER(&b->sma_amp_b, buf_sz, fs_session);
    if (b->sma_amp_b.data == NULL) {
        return SWITCH_STATUS_FALSE;
    }
    memset(b->sma_amp_b.data, 0, sizeof(BUFF_TYPE) * buf_sz);

    INIT_SMA_BUFFER(&b->sqa_amp_b, buf_sz, fs_session);
    if (b->sqa_amp_b.data == NULL) {
        return SWITCH_STATUS_FALSE;
    }
    memset(b->sqa_amp_b.data, 0, sizeof(BUFF_TYPE) * buf_sz);

    b->amplitude_max = 0.0;
    b->samples_streak = 0;
    b->samples_streak_amp = 0;
    b->resolution = resolution;
    b->offset = offset;

    return SWITCH_STATUS_SUCCESS;
}

/*! \brief  The avmd session data initialization function.
 * @param   avmd_session A reference to a avmd session.
 * @param   fs_session A reference to a FreeSWITCH session.
 * @details Avmd globals mutex must be locked.
 */
static switch_status_t init_avmd_session_data(avmd_session_t *avmd_session, switch_core_session_t *fs_session, switch_mutex_t *mutex)
{
    uint8_t         idx, resolution, offset;
    size_t          buf_sz;
    struct avmd_detector *d;
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    if (mutex != NULL)
    {
        switch_mutex_lock(mutex);
    }

    /*! This is a worst case sample rate estimate */
    avmd_session->rate = 48000;
    INIT_CIRC_BUFFER(&avmd_session->b, (size_t) AVMD_BEEP_LEN(avmd_session->rate), (size_t) AVMD_FRAME_LEN(avmd_session->rate), fs_session);
    if (avmd_session->b.buf == NULL) {
        status =  SWITCH_STATUS_MEMERR;
        goto end;
    }
    avmd_session->session = fs_session;
    avmd_session->pos = 0;
    avmd_session->f = 0.0;
    avmd_session->state.last_beep = 0;
    avmd_session->state.beep_state = BEEP_NOTDETECTED;
    switch_mutex_init(&avmd_session->mutex, SWITCH_MUTEX_DEFAULT, switch_core_session_get_pool(fs_session));
    avmd_session->frame_n = 0;
    avmd_session->detection_start_time = 0;
    avmd_session->detection_stop_time = 0;
    avmd_session->frame_n_to_skip = 0;

    buf_sz = AVMD_BEEP_LEN((uint32_t)avmd_session->rate) / (uint32_t) AVMD_SINE_LEN(avmd_session->rate);
    if (buf_sz < 1) {
        status = SWITCH_STATUS_MORE_DATA;
        goto end;
    }
    avmd_session->detectors = (struct avmd_detector*) switch_core_session_alloc(fs_session, (avmd_session->settings.detectors_n + avmd_session->settings.detectors_lagged_n) * sizeof(struct avmd_detector));
    if (avmd_session->detectors == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Can't allocate memory for avmd detectors!\n");
        status = SWITCH_STATUS_NOT_INITALIZED;
        goto end;
    }
    idx = 0;
    resolution = 0;
    while (idx < avmd_session->settings.detectors_n) {
        ++resolution;
        offset = 0;
        while ((offset < resolution) && (idx < avmd_session->settings.detectors_n)) {
            d = &avmd_session->detectors[idx];
            if (avmd_init_buffer(&d->buffer, buf_sz, resolution, offset, fs_session) != SWITCH_STATUS_SUCCESS) {
                status = SWITCH_STATUS_FALSE;
                goto end;
            }
            d->s = avmd_session;
            d->flag_processing_done = 1;
            d->flag_should_exit = 1;
            d->idx = idx;
            d->thread = NULL;
            switch_mutex_init(&d->mutex, SWITCH_MUTEX_DEFAULT, switch_core_session_get_pool(fs_session));
            switch_thread_cond_create(&d->cond_start_processing, switch_core_session_get_pool(fs_session));
            ++offset;
            ++idx;
        }
    }
    idx = 0;
    resolution = 1;
    offset = 0;
    while (idx < avmd_session->settings.detectors_lagged_n) {
            d = &avmd_session->detectors[avmd_session->settings.detectors_n + idx];
            if (avmd_init_buffer(&d->buffer, buf_sz, resolution, offset, fs_session) != SWITCH_STATUS_SUCCESS) {
                status = SWITCH_STATUS_FALSE;
                goto end;
            }
            d->s = avmd_session;
            d->flag_processing_done = 1;
            d->flag_should_exit = 1;
            d->idx = avmd_session->settings.detectors_n + idx;
            d->thread = NULL;
            switch_mutex_init(&d->mutex, SWITCH_MUTEX_DEFAULT, switch_core_session_get_pool(fs_session));
            switch_thread_cond_create(&d->cond_start_processing, switch_core_session_get_pool(fs_session));
            ++idx;
    }
    switch_mutex_init(&avmd_session->mutex_detectors_done, SWITCH_MUTEX_DEFAULT, switch_core_session_get_pool(fs_session));
    switch_thread_cond_create(&avmd_session->cond_detectors_done, switch_core_session_get_pool(fs_session));

end:
    if (mutex != NULL)
    {
        switch_mutex_unlock(mutex);
    }
    return status;
}

static void avmd_session_close(avmd_session_t *s) {
    uint8_t                 idx;
    struct avmd_detector    *d;
    switch_status_t         status;

    switch_mutex_lock(s->mutex);

    switch_mutex_lock(s->mutex_detectors_done);
    while (avmd_detection_in_progress(s) == 1) {
        switch_thread_cond_wait(s->cond_detectors_done, s->mutex_detectors_done);
    }
    switch_mutex_unlock(s->mutex_detectors_done);

    idx = 0;
    while (idx < (s->settings.detectors_n + s->settings.detectors_lagged_n)) {
        d = &s->detectors[idx];
        switch_mutex_lock(d->mutex);
        d = &s->detectors[idx];
        d->flag_processing_done = 0;
        d->flag_should_exit = 1;
        d->samples = 0;
        switch_thread_cond_signal(d->cond_start_processing);
        switch_mutex_unlock(d->mutex);

        switch_thread_join(&status, d->thread);
        d->thread = NULL;

        switch_mutex_destroy(d->mutex);
        switch_thread_cond_destroy(d->cond_start_processing);
        ++idx;
    }
    switch_mutex_unlock(s->mutex);
    switch_mutex_destroy(s->mutex_detectors_done);
    switch_thread_cond_destroy(s->cond_detectors_done);
    switch_mutex_destroy(s->mutex);
}

/*! \brief The callback function that is called when new audio data becomes available.
 * @param bug A reference to the media bug.
 * @param user_data The session information for this call.
 * @param type The switch callback type.
 * @return The success or failure of the function.
 */
static switch_bool_t avmd_callback(switch_media_bug_t * bug, void *user_data, switch_abc_type_t type) {
    avmd_session_t          *avmd_session;
    switch_codec_t          *read_codec;
    switch_codec_t          *write_codec;
    switch_frame_t          *frame;
    switch_core_session_t   *fs_session;
    switch_channel_t        *channel = NULL;


    avmd_session = (avmd_session_t *) user_data;
    if (avmd_session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No avmd session assigned!\n");
        return SWITCH_FALSE;
    }
    if ((type != SWITCH_ABC_TYPE_INIT) && (type != SWITCH_ABC_TYPE_CLOSE)) {
        switch_mutex_lock(avmd_session->mutex);
    }
    fs_session = avmd_session->session;
    if (fs_session == NULL) {
        if (type != SWITCH_ABC_TYPE_INIT) {
            switch_mutex_unlock(avmd_session->mutex);
        }
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No FreeSWITCH session assigned!\n");
        return SWITCH_FALSE;
    }

    channel = switch_core_session_get_channel(fs_session);
    if (channel == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No channel for FreeSWITCH session!\n");
        return SWITCH_FALSE;
    }

    switch (type) {

        case SWITCH_ABC_TYPE_INIT:
            if ((SWITCH_CALL_DIRECTION_OUTBOUND == switch_channel_direction(channel)) && (avmd_session->settings.outbound_channnel == 1)) {
                read_codec = switch_core_session_get_read_codec(fs_session);
                if (read_codec == NULL) {
                    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING, "No read codec assigned, default session rate to 8000 samples/s\n");
                    avmd_session->rate = 8000;
                } else {
                    if (read_codec->implementation == NULL) {
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING, "No read codec implementation assigned, default session rate to 8000 samples/s\n");
                        avmd_session->rate = 8000;
                    } else {
                        avmd_session->rate = read_codec->implementation->samples_per_second;
                    }
                }
            }
            if ((SWITCH_CALL_DIRECTION_INBOUND == switch_channel_direction(channel)) && (avmd_session->settings.inbound_channnel == 1)) {
                write_codec = switch_core_session_get_write_codec(fs_session);
                if (write_codec == NULL) {
                    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING, "No write codec assigned, default session rate to 8000 samples/s\n");
                    avmd_session->rate = 8000;
                } else {
                    if (write_codec->implementation == NULL) {
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_WARNING, "No write codec implementation assigned, default session rate to 8000 samples/s\n");
                        avmd_session->rate = 8000;
                    } else {
                        avmd_session->rate = write_codec->implementation->samples_per_second;
                    }
                }
            }
            avmd_session->start_time = switch_micro_time_now();
            /* avmd_session->vmd_codec.channels =  read_codec->implementation->number_of_channels; */
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session),SWITCH_LOG_INFO, "Avmd session initialized, [%u] samples/s\n", avmd_session->rate);
            break;

        case SWITCH_ABC_TYPE_READ_REPLACE:
            frame = switch_core_media_bug_get_read_replace_frame(bug);
            avmd_process(avmd_session, frame, AVMD_READ_REPLACE);
            break;

        case SWITCH_ABC_TYPE_WRITE_REPLACE:
            frame = switch_core_media_bug_get_write_replace_frame(bug);
            avmd_process(avmd_session, frame, AVMD_WRITE_REPLACE);
            break;

        case SWITCH_ABC_TYPE_CLOSE:
            avmd_session_close(avmd_session);
			switch_mutex_lock(avmd_globals.mutex);
            if (avmd_globals.session_n > 0) {
                --avmd_globals.session_n;
            }
			switch_mutex_unlock(avmd_globals.mutex);
            break;

        default:
            break;
    }

    if ((type != SWITCH_ABC_TYPE_INIT) && (type != SWITCH_ABC_TYPE_CLOSE)) {
        switch_mutex_unlock(avmd_session->mutex);
    }
    return SWITCH_TRUE;
}

static switch_status_t avmd_register_all_events(void) {
    size_t idx = 0;
    const char *e = avmd_events_str[0];
    while (e != NULL)
    {
        if (switch_event_reserve_subclass(e) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass [%s]!\n", e);
            return SWITCH_STATUS_TERM;
        }
        ++idx;
        e = avmd_events_str[idx];
    }
    return SWITCH_STATUS_SUCCESS;
}

static void avmd_unregister_all_events(void) {
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

static void avmd_fire_event(enum avmd_event type, switch_core_session_t *fs_s, double freq, double v_freq, double amp, double v_amp, avmd_beep_state_t beep_status, uint8_t info,
        switch_time_t detection_start_time, switch_time_t detection_stop_time, switch_time_t start_time, switch_time_t stop_time, uint8_t resolution, uint8_t offset, uint8_t idx) {
    int res;
    switch_event_t      *event;
    switch_time_t       detection_time, total_time;
    switch_status_t     status;
    switch_event_t      *event_copy;
    char                buf[AVMD_CHAR_BUF_LEN];

    status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, avmd_events_str[type]);
    if (status != SWITCH_STATUS_SUCCESS) {
        return;
    }
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(fs_s));
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Call-command", "avmd");
    switch (type)
    {
        case AVMD_EVENT_BEEP:
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Beep-Status", "DETECTED");
            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%f", freq);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Frequency truncated [%s], [%d] attempted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Frequency", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Frequency", buf);

            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%f", v_freq);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Error, truncated [%s], [%d] attempeted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Frequency-variance", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Frequency-variance", buf);

            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%f", amp);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Amplitude truncated [%s], [%d] attempted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Amplitude", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Amplitude", buf);

            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%f", v_amp);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Error, truncated [%s], [%d] attempeted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Amplitude-variance", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Amplitude-variance", buf);

            detection_time = detection_stop_time - detection_start_time;
            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%" PRId64 "", detection_time);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Detection time truncated [%s], [%d] attempted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detection-time", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detection-time", buf);

            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%u", resolution);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Error, truncated [%s], [%d] attempeted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detector-resolution", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detector-resolution", buf);

            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%u", offset);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Error, truncated [%s], [%d] attempeted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detector-offset", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detector-offset", buf);

            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%u", idx);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Error, truncated [%s], [%d] attempeted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detector-index", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detector-index", buf);
            break;

        case AVMD_EVENT_SESSION_START:
            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%" PRId64 "", start_time);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Start time truncated [%s], [%d] attempted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Start-time", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Start-time", buf);
            break;

        case AVMD_EVENT_SESSION_STOP:
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Beep-Status", beep_status == BEEP_DETECTED ? "DETECTED" : "NOTDETECTED");
            if (info == 0) {
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Stop-status", "ERROR (AVMD SESSION OBJECT NOT FOUND IN MEDIA BUG)");
            }
            total_time = stop_time - start_time;
            res = snprintf(buf, AVMD_CHAR_BUF_LEN, "%" PRId64 "", total_time);
            if (res < 0 || res > AVMD_CHAR_BUF_LEN - 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_s), SWITCH_LOG_ERROR, "Total time truncated [%s], [%d] attempted!\n", buf, res);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Total-time", "ERROR (TRUNCATED)");
            }
            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Total-time", buf);
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

int avmd_parse_u8_user_input(const char *input, uint8_t *output, uint8_t min, uint8_t max) {
    char            *pCh;
    unsigned long   helper;
    helper = strtoul(input, &pCh, 10);
    if (helper < min || helper > UINT8_MAX || helper > max || (pCh == input) || (*pCh != '\0')) {
        return -1;
    }
    *output = (uint8_t) helper;
    return 0;
}

int avmd_parse_u16_user_input(const char *input, uint16_t *output, uint16_t min, uint16_t max) {
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

static void avmd_set_xml_default_configuration(switch_mutex_t *mutex) {
    if (mutex != NULL) {
        switch_mutex_lock(mutex);
    }

    avmd_globals.settings.debug = 0;
    avmd_globals.settings.report_status = 1;
    avmd_globals.settings.fast_math = 0;
    avmd_globals.settings.require_continuous_streak = 1;
    avmd_globals.settings.sample_n_continuous_streak = 3;
    avmd_globals.settings.sample_n_to_skip = 0;
    avmd_globals.settings.require_continuous_streak_amp = 1;
    avmd_globals.settings.sample_n_continuous_streak_amp = 3;
    avmd_globals.settings.simplified_estimation = 1;
    avmd_globals.settings.inbound_channnel = 0;
    avmd_globals.settings.outbound_channnel = 1;
    avmd_globals.settings.mode = AVMD_DETECT_BOTH;
    avmd_globals.settings.detectors_n = 36;
    avmd_globals.settings.detectors_lagged_n = 1;

    if (mutex != NULL) {
        switch_mutex_unlock(avmd_globals.mutex);
    }
    return;
}

static void avmd_set_xml_inbound_configuration(switch_mutex_t *mutex)
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

static void avmd_set_xml_outbound_configuration(switch_mutex_t *mutex) {
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

static switch_status_t avmd_load_xml_configuration(switch_mutex_t *mutex) {
	switch_xml_t xml = NULL, x_lists = NULL, x_list = NULL, cfg = NULL;
	uint8_t bad_debug = 1, bad_report = 1, bad_fast = 1, bad_req_cont = 1, bad_sample_n_cont = 1,
			bad_sample_n_to_skip = 1, bad_req_cont_amp = 1, bad_sample_n_cont_amp = 1, bad_simpl = 1,
			bad_inbound = 1, bad_outbound = 1, bad_mode = 1, bad_detectors = 1, bad_lagged = 1, bad = 0;

	if (mutex != NULL) {
		switch_mutex_lock(mutex);
	}

	if ((xml = switch_xml_open_cfg("avmd.conf", &cfg, NULL)) != NULL) {

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
					bad_debug = 0;
				} else if (!strcmp(name, "report_status")) {
					avmd_globals.settings.report_status = switch_true(value) ? 1 : 0;
					bad_report = 0;
				} else if (!strcmp(name, "fast_math")) {
					avmd_globals.settings.fast_math = switch_true(value) ? 1 : 0;
					bad_fast = 0;
				} else if (!strcmp(name, "require_continuous_streak")) {
					avmd_globals.settings.require_continuous_streak = switch_true(value) ? 1 : 0;
					bad_req_cont = 0;
				} else if (!strcmp(name, "sample_n_continuous_streak")) {
					if(!avmd_parse_u16_user_input(value, &avmd_globals.settings.sample_n_continuous_streak, 0, UINT16_MAX)) {
						bad_sample_n_cont = 0;
					}
				} else if (!strcmp(name, "sample_n_to_skip")) {
					if(!avmd_parse_u16_user_input(value, &avmd_globals.settings.sample_n_to_skip, 0, UINT16_MAX)) {
						bad_sample_n_to_skip = 0;
					}
				} else if (!strcmp(name, "require_continuous_streak_amp")) {
					avmd_globals.settings.require_continuous_streak_amp = switch_true(value) ? 1 : 0;
					bad_req_cont_amp = 0;
				} else if (!strcmp(name, "sample_n_continuous_streak_amp")) {
					if(!avmd_parse_u16_user_input(value, &avmd_globals.settings.sample_n_continuous_streak_amp, 0, UINT16_MAX)) {
						bad_sample_n_cont_amp = 0;
					}
				} else if (!strcmp(name, "simplified_estimation")) {
					avmd_globals.settings.simplified_estimation = switch_true(value) ? 1 : 0;
					bad_simpl = 0;
				} else if (!strcmp(name, "inbound_channel")) {
					avmd_globals.settings.inbound_channnel = switch_true(value) ? 1 : 0;
					bad_inbound = 0;
				} else if (!strcmp(name, "outbound_channel")) {
					avmd_globals.settings.outbound_channnel = switch_true(value) ? 1 : 0;
					bad_outbound = 0;
				} else if (!strcmp(name, "detection_mode")) {
					if(!avmd_parse_u8_user_input(value, (uint8_t*)&avmd_globals.settings.mode, 0, 2)) {
						bad_mode = 0;
					}
				} else if (!strcmp(name, "detectors_n")) {
					if(!avmd_parse_u8_user_input(value, &avmd_globals.settings.detectors_n, 0, UINT8_MAX)) {
						bad_detectors = 0;
					}
				} else if (!strcmp(name, "detectors_lagged_n")) {
					if(!avmd_parse_u8_user_input(value, &avmd_globals.settings.detectors_lagged_n, 0, UINT8_MAX)) {
						bad_lagged = 0;
					}
				}
			} // for
		} // if list

		switch_xml_free(xml);
	} // if open OK

	if (bad_debug) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'debug' missing or invalid - using default\n");
		avmd_globals.settings.debug = 0;
	}

	if (bad_report) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'report_status' missing or invalid - using default\n");
		avmd_globals.settings.report_status = 1;
	}

	if (bad_fast) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'fast_math' missing or invalid - using default\n");
		avmd_globals.settings.fast_math = 0;
	}

	if (bad_req_cont) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'require_continuous_streak' missing or invalid - using default\n");
		avmd_globals.settings.require_continuous_streak = 1;
	}

	if (bad_sample_n_cont) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'sample_n_continuous_streak' missing or invalid - using default\n");
		avmd_globals.settings.sample_n_continuous_streak = 3;
	}

	if (bad_sample_n_to_skip) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'sample_n_to_skip' missing or invalid - using default\n");
		avmd_globals.settings.sample_n_to_skip = 0;
	}

	if (bad_req_cont_amp) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'require_continuous_streak_amp' missing or invalid - using default\n");
		avmd_globals.settings.require_continuous_streak_amp = 1;
	}

	if (bad_sample_n_cont_amp) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'sample_n_continuous_streak_amp' missing or invalid - using default\n");
		avmd_globals.settings.sample_n_continuous_streak_amp = 3;
	}

	if (bad_simpl) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'simplified_estimation' missing or invalid - using default\n");
		avmd_globals.settings.simplified_estimation = 1;
	}

	if (bad_inbound) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'inbound_channel' missing or invalid - using default\n");
		avmd_globals.settings.inbound_channnel = 0;
	}

	if (bad_outbound) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'outbound_channel' missing or invalid - using default\n");
		avmd_globals.settings.outbound_channnel = 1;
	}

	if (bad_mode) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'detection_mode' missing or invalid - using default\n");
		avmd_globals.settings.mode = AVMD_DETECT_BOTH;
	}

	if (bad_detectors) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'detectors_n' missing or invalid - using default\n");
		avmd_globals.settings.detectors_n = 36;
	}

	if (bad_lagged) {
		bad = 1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AVMD config parameter 'detectors_lagged_n' missing or invalid - using default\n");
		avmd_globals.settings.detectors_lagged_n = 1;
	}

	/**
	 * Hint.
	 */
	if (bad) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Type 'avmd show' to display default settings. Type 'avmd ' + TAB for autocompletion.\n");
	}

	if (mutex != NULL) {
		switch_mutex_unlock(mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t avmd_load_xml_inbound_configuration(switch_mutex_t *mutex) {
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

static switch_status_t avmd_load_xml_outbound_configuration(switch_mutex_t *mutex) {
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

static void avmd_show(switch_stream_handle_t *stream, switch_mutex_t *mutex) {
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
    stream->write_function(stream, "debug                          \t%u\n", avmd_globals.settings.debug);
    stream->write_function(stream, "report status                  \t%u\n", avmd_globals.settings.report_status);
    stream->write_function(stream, "fast_math                      \t%u\n", avmd_globals.settings.fast_math);
    stream->write_function(stream, "require continuous streak      \t%u\n", avmd_globals.settings.require_continuous_streak);
    stream->write_function(stream, "sample n continuous streak     \t%u\n", avmd_globals.settings.sample_n_continuous_streak);
    stream->write_function(stream, "sample n to skip               \t%u\n", avmd_globals.settings.sample_n_to_skip);
    stream->write_function(stream, "require continuous streak amp  \t%u\n", avmd_globals.settings.require_continuous_streak_amp);
    stream->write_function(stream, "sample n continuous streak amp \t%u\n", avmd_globals.settings.sample_n_continuous_streak_amp);
    stream->write_function(stream, "simplified estimation          \t%u\n", avmd_globals.settings.simplified_estimation);
    stream->write_function(stream, "inbound channel                \t%u\n", avmd_globals.settings.inbound_channnel);
    stream->write_function(stream, "outbound channel               \t%u\n", avmd_globals.settings.outbound_channnel);
    stream->write_function(stream, "detection mode                 \t%u\n", avmd_globals.settings.mode);
    stream->write_function(stream, "sessions                       \t%"PRId64"\n", avmd_globals.session_n);
    stream->write_function(stream, "detectors n                    \t%u\n", avmd_globals.settings.detectors_n);
    stream->write_function(stream, "detectors lagged n             \t%u\n", avmd_globals.settings.detectors_lagged_n);
    stream->write_function(stream, "\n\n");

    if (mutex != NULL) {
        switch_mutex_unlock(mutex);
    }
}

SWITCH_MODULE_LOAD_FUNCTION(mod_avmd_load) {
#ifndef WIN32
    char    err[150];
    int     ret;
#endif

    switch_application_interface_t *app_interface;
    switch_api_interface_t *api_interface;
    /* connect my internal structure to the blank pointer passed to me */
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    if (avmd_register_all_events() != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register avmd events!\n");
        return SWITCH_STATUS_TERM;
    }

    memset(&avmd_globals, 0, sizeof(avmd_globals));
    if (pool == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No memory pool assigned!\n");
        return SWITCH_STATUS_TERM;
    }
    switch_mutex_init(&avmd_globals.mutex, SWITCH_MUTEX_NESTED, pool);
    avmd_globals.pool = pool;

    if (avmd_load_xml_configuration(NULL) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load XML configuration! Loading default settings\n");
        avmd_set_xml_default_configuration(NULL);
    }

    if ((switch_event_bind(modname, SWITCH_EVENT_RELOADXML, NULL, avmd_reloadxml_event_handler, NULL) != SWITCH_STATUS_SUCCESS)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind our reloadxml handler! Module will not react to changes made in XML configuration\n");
        /* Not so severe to prevent further loading, well - it depends, anyway */
    }

#ifndef WIN32
    if (avmd_globals.settings.fast_math == 1) {
        ret = init_fast_acosf();
        if (ret != 0) {
            strerror_r(errno, err, 150);
            switch (ret) {

                case -1:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't access file [%s], error [%s]\n", ACOS_TABLE_FILENAME, err);
                    break;
                case -2:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating file [%s], error [%s]\n", ACOS_TABLE_FILENAME, err);
                    break;
                case -3:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Access rights are OK but can't open file [%s], error [%s]\n", ACOS_TABLE_FILENAME, err);
                    break;
                case -4:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Access rights are OK but can't mmap file [%s], error [%s]\n",ACOS_TABLE_FILENAME, err);
                    break;
                default:
                    switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Unknown error [%d] while initializing fast cos table [%s], errno [%s]\n", ret, ACOS_TABLE_FILENAME, err);
                    return SWITCH_STATUS_TERM;
            }
            return SWITCH_STATUS_TERM;
        } else
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Advanced voicemail detection: fast math enabled, arc cosine table is [%s]\n", ACOS_TABLE_FILENAME);
    }
#endif

    SWITCH_ADD_APP(app_interface, "avmd_start","Start avmd detection", "Start avmd detection", avmd_start_app, "", SAF_NONE);
    SWITCH_ADD_APP(app_interface, "avmd_stop","Stop avmd detection", "Stop avmd detection", avmd_stop_app, "", SAF_NONE);
    SWITCH_ADD_APP(app_interface, "avmd","Beep detection", "Advanced detection of voicemail beeps", avmd_start_function, AVMD_SYNTAX, SAF_NONE);

    SWITCH_ADD_API(api_interface, "avmd", "Voicemail beep detection", avmd_api_main, AVMD_SYNTAX);

    switch_console_set_complete("add avmd ::console::list_uuid ::[start:stop");
    switch_console_set_complete("add avmd set inbound");    /* set inbound = 1, outbound = 0 */
    switch_console_set_complete("add avmd set outbound");   /* set inbound = 0, outbound = 1 */
    switch_console_set_complete("add avmd set default");    /* restore to factory settings */
    switch_console_set_complete("add avmd load inbound");   /* reload + set inbound */
    switch_console_set_complete("add avmd load outbound");  /* reload + set outbound */
    switch_console_set_complete("add avmd reload");         /* reload XML (it loads from FS installation
                                                             * folder, not module's conf/autoload_configs */
    switch_console_set_complete("add avmd show");

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Advanced voicemail detection enabled\n");

    return SWITCH_STATUS_SUCCESS; /* indicate that the module should continue to be loaded */
}

void avmd_config_dump(avmd_session_t *s) {
    struct avmd_settings *settings;

    if (s == NULL) {
        return;
    }
    settings = &s->settings;
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_INFO, "Avmd dynamic configuration: debug [%u], report_status [%u], fast_math [%u],"
            " require_continuous_streak [%u], sample_n_continuous_streak [%u], sample_n_to_skip [%u], require_continuous_streak_amp [%u], sample_n_continuous_streak_amp [%u],"
           " simplified_estimation [%u], inbound_channel [%u], outbound_channel [%u], detection_mode [%u], detectors_n [%u], detectors_lagged_n [%u]\n",
            settings->debug, settings->report_status, settings->fast_math, settings->require_continuous_streak, settings->sample_n_continuous_streak,
            settings->sample_n_to_skip, settings->require_continuous_streak_amp, settings->sample_n_continuous_streak_amp,
            settings->simplified_estimation, settings->inbound_channnel, settings->outbound_channnel, settings->mode, settings->detectors_n, settings->detectors_lagged_n);
    return;
}

static switch_status_t avmd_parse_cmd_data_one_entry(char *candidate, struct avmd_settings *settings) {
    char        *candidate_parsed[3];
    int         argc;
    const char *key;
    const char *val;

    if (settings == NULL) {
        return SWITCH_STATUS_TERM;
    }
    if (candidate == NULL) {
        return SWITCH_STATUS_NOOP;
    }

    argc = switch_separate_string(candidate, '=', candidate_parsed, (sizeof(candidate_parsed) / sizeof(candidate_parsed[0])));
    if (argc > 2) { /* currently we accept only option=value syntax */
        return SWITCH_STATUS_IGNORE;
    }

    /* this may be option parameter if valid */
    key = candidate_parsed[0];      /* option name */
    if (zstr(key)) { /* empty key */
        return SWITCH_STATUS_NOT_INITALIZED;
    }
    val = candidate_parsed[1];      /* value of the option: whole string starting at 1 past the '=' */
    if (zstr(val)) { /* nothing after "=" found, empty value */
        return SWITCH_STATUS_MORE_DATA;
    }
    /* candidate string has "=" somewhere in the middle and some value,
     * try to find what option it is by comparing at most given number of bytes */
    if (!strcmp(key, "debug")) {
        settings->debug = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "report_status")) {
        settings->report_status = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "fast_math")) {
        settings->fast_math = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "require_continuous_streak")) {
        settings->require_continuous_streak = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "sample_n_continuous_streak")) {
        if(avmd_parse_u16_user_input(val, &settings->sample_n_continuous_streak, 0, UINT16_MAX) == -1) {
            return SWITCH_STATUS_FALSE;
        }
    } else if (!strcmp(key, "sample_n_to_skip")) {
        if(avmd_parse_u16_user_input(val, &settings->sample_n_to_skip, 0, UINT16_MAX) == -1) {
            return SWITCH_STATUS_FALSE;
        }
    } else if (!strcmp(key, "require_continuous_streak_amp")) {
        settings->require_continuous_streak_amp = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "sample_n_continuous_streak_amp")) {
        if(avmd_parse_u16_user_input(val, &settings->sample_n_continuous_streak_amp, 0, UINT16_MAX) == -1) {
            return SWITCH_STATUS_FALSE;
        }
    } else if (!strcmp(key, "simplified_estimation")) {
        settings->simplified_estimation = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "inbound_channel")) {
        settings->inbound_channnel = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "outbound_channel")) {
        settings->outbound_channnel = (uint8_t) switch_true(val);
    } else if (!strcmp(key, "detection_mode")) {
        if(avmd_parse_u8_user_input(val, (uint8_t*)&settings->mode, 0, 2) == -1) {
            return SWITCH_STATUS_FALSE;
        }
    } else if (!strcmp(key, "detectors_n")) {
        if(avmd_parse_u8_user_input(val, &settings->detectors_n, 0, UINT8_MAX) == -1) {
            return SWITCH_STATUS_FALSE;
        }
    } else if (!strcmp(key, "detectors_lagged_n")) {
        if(avmd_parse_u8_user_input(val, &settings->detectors_lagged_n, 0, UINT8_MAX) == -1) {
            return SWITCH_STATUS_FALSE;
        }
    } else {
        return SWITCH_STATUS_NOTFOUND;
    }
    return SWITCH_STATUS_SUCCESS;
}

/* RCU style: reads, copies and then updates only if everything is fine,
 * if it returns SWITCH_STATUS_SUCCESS parsing went OK and avmd settings
 * are updated accordingly to @cmd_data, if SWITCH_STATUS_FALSE then
 * parsing error occurred and avmd session is left untouched */
static switch_status_t avmd_parse_cmd_data(avmd_session_t *s, const char *cmd_data, enum avmd_app app) {
    char *mydata;
    struct avmd_settings    settings;
    int argc = 0, idx;
    char *argv[AVMD_PARAMS_APP_MAX * 2] = { 0 };
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    if (s == NULL) {
        return SWITCH_STATUS_NOOP;
    }

    memcpy(&settings, &avmd_globals.settings, sizeof (struct avmd_settings));   /* copy globally set settings first */
    if (zstr(cmd_data)) {
        goto end_copy;
    }

    switch (app) {

        case AVMD_APP_START_APP:
            /* try to parse settings */
            mydata = switch_core_session_strdup(s->session, cmd_data);
            argc = switch_separate_string(mydata, ',', argv, (sizeof(argv) / sizeof(argv[0])));
            if (argc < AVMD_PARAMS_APP_START_MIN || argc > AVMD_PARAMS_APP_START_MAX) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                        "Syntax Error, avmd_start APP takes [%u] to [%u] parameters\n",
                        AVMD_PARAMS_APP_START_MIN, AVMD_PARAMS_APP_START_MAX);
                switch_goto_status(SWITCH_STATUS_MORE_DATA, fail);
            }
            /* iterate over params, check if they mean something to us, set */
            idx = 0;
            while (idx < argc) {
                status = avmd_parse_cmd_data_one_entry(argv[idx], &settings);
                if (status != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                            "Error parsing option [%d] [%s]\n", idx + 1, argv[idx]);    /* idx + 1 to report option 0 as 1 for users convenience */
                    switch (status)
                    {
                        case SWITCH_STATUS_TERM:
                            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                                    "NULL settings struct passed to parser\n");
                            break;
                        case SWITCH_STATUS_NOOP:
                            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                                    "NULL settings string passed to parser\n");
                            break;
                        case SWITCH_STATUS_IGNORE:
                            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                                    "Syntax error. Currently we accept only option=value syntax\n");
                            break;
                        case SWITCH_STATUS_NOT_INITALIZED:
                            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                                    "Syntax error. No key specified\n");
                            break;
                        case SWITCH_STATUS_MORE_DATA:
                            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                                    "Syntax error. No value for the key? Currently we accept only option=value syntax\n");
                            break;
                        case SWITCH_STATUS_FALSE:
                            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                                    "Bad value for this option\n");
                            break;
                        case SWITCH_STATUS_NOTFOUND:
                            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR,
                                    "Option not found. Please check option name is correct\n");
                            break;
                        default:
                            break;
                    }
                    status = SWITCH_STATUS_FALSE;
                    goto fail;
                }
                ++idx;
            }
            /* OK */
            goto end_copy;
        default:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_ERROR, "There is no app with index [%u] for avmd\n", app);
            switch_goto_status(SWITCH_STATUS_NOTFOUND, fail);
    }

end_copy:
    memcpy(&s->settings, &settings, sizeof (struct avmd_settings)); /* commit the change */
    return SWITCH_STATUS_SUCCESS;
fail:
    return status;
}

SWITCH_STANDARD_APP(avmd_start_app) {
    switch_media_bug_t  *bug = NULL;
    switch_status_t     status = SWITCH_STATUS_FALSE;
    switch_channel_t    *channel = NULL;
    avmd_session_t      *avmd_session = NULL;
    switch_core_media_flag_t flags = 0;
	const char *direction = "NO DIRECTION";

    if (session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "BUGGG. FreeSWITCH session is NULL! Please report to developers\n");
        return;
    }

    /* Get current channel of the session to tag the session. This indicates that our module is present
     * At this moment this cannot return NULL, it will either succeed or assert failed, but we make ourself secure anyway */
    channel = switch_core_session_get_channel(session);
    if (channel == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "BUGGG. No channel for FreeSWITCH session! Please report this to the developers.\n");
        goto end;
    }

    bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_"); /* Is this channel already set? */
    if (bug != NULL) { /* We have already started */
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Avmd already started!\n");
        return;
    }

    /* Allocate memory attached to this FreeSWITCH session for use in the callback routine and to store state information */
    avmd_session = (avmd_session_t *) switch_core_session_alloc(session, sizeof(avmd_session_t));
    if (avmd_session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't allocate memory for avmd session!\n");
        status = SWITCH_STATUS_FALSE;
        goto end;
    }
    avmd_session->session = session;

    status = avmd_parse_cmd_data(avmd_session, data, AVMD_APP_START_APP);   /* dynamic configuation */
    switch (status) {
        case SWITCH_STATUS_SUCCESS:
            break;
        case SWITCH_STATUS_NOOP:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to set dynamic parameters for avmd session. Session is NULL!\n");
            goto end;
        case SWITCH_STATUS_FALSE:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to set dynamic parameters for avmd session. Parsing error, please check the parameters passed to this APP.\n");
            goto end;
        default:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to set dynamic parameteres for avmd session. Unknown error\n");
            goto end;
    }

    status = init_avmd_session_data(avmd_session, session, avmd_globals.mutex);
    if (status != SWITCH_STATUS_SUCCESS) {
        switch (status) {
            case SWITCH_STATUS_MEMERR:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to init avmd session. Buffer error!\n");
                break;
            case SWITCH_STATUS_MORE_DATA:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to init avmd session. SMA buffer size is 0!\n");
                break;
            case SWITCH_STATUS_FALSE:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to init avmd session. SMA buffers error\n");
                break;
            default:
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to init avmd session. Unknown error\n");
                break;
        }
        goto end;
    }

    switch_mutex_lock(avmd_session->mutex);
    if (avmd_session->settings.report_status == 1) { /* dump dynamic parameters */
        avmd_config_dump(avmd_session);
    }
    if ((SWITCH_CALL_DIRECTION_OUTBOUND == switch_channel_direction(channel)) && (avmd_session->settings.outbound_channnel == 1)) {
            flags |= SMBF_READ_REPLACE;
			direction = "READ_REPLACE";
    }
    if ((SWITCH_CALL_DIRECTION_INBOUND == switch_channel_direction(channel)) && (avmd_session->settings.inbound_channnel == 1)) {
            flags |= SMBF_WRITE_REPLACE;
			if (!strcmp(direction, "READ_REPLACE")) {
				direction = "READ_REPLACE | WRITE_REPLACE";
			} else {
				direction = "WRITE_REPLACE";
			}
    }

    if (flags == 0) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't set direction for channel [%s]\n", switch_channel_get_name(channel));
        status = SWITCH_STATUS_FALSE;
        goto end_unlock;
    }

    if ((SWITCH_CALL_DIRECTION_OUTBOUND == switch_channel_direction(channel)) && (avmd_session->settings.outbound_channnel == 1)) {
        if (switch_channel_test_flag(channel, CF_MEDIA_SET) == 0) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Channel [%s] has no codec assigned yet. Please try again\n", switch_channel_get_name(channel));
            status = SWITCH_STATUS_FALSE;
            goto end_unlock;
        }
    }

    status = avmd_launch_threads(avmd_session);
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to start detection threads\n");
        avmd_join_threads(avmd_session);
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Joined detection threads\n");
        goto end_unlock;
    }

    status = switch_core_media_bug_add(session, "avmd", NULL, avmd_callback, avmd_session, 0, flags, &bug); /* Add a media bug that allows me to intercept the audio stream */
    if (status != SWITCH_STATUS_SUCCESS) { /* If adding a media bug fails exit */
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to add media bug!\n");
        goto end_unlock;
    }

    switch_mutex_lock(avmd_globals.mutex);
    ++avmd_globals.session_n;
    switch_mutex_unlock(avmd_globals.mutex);

    switch_channel_set_private(channel, "_avmd_", bug); /* Set the avmd tag to detect an existing avmd media bug */
    avmd_fire_event(AVMD_EVENT_SESSION_START, session, 0, 0, 0, 0, 0, 0, 0, 0, avmd_session->start_time, 0, 0, 0, 0);
    if (avmd_session->settings.report_status == 1) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Avmd on channel [%s] started! direction=%s\n", switch_channel_get_name(channel), direction);
    }

end_unlock:
    switch_mutex_unlock(avmd_session->mutex);

end:
    if (status != SWITCH_STATUS_SUCCESS) {
        if (avmd_session == NULL || avmd_session->settings.report_status == 1) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Avmd on channel [%s] NOT started\n", switch_channel_get_name(channel));
        }
    }
    return;
}

SWITCH_STANDARD_APP(avmd_stop_app) {
    switch_media_bug_t  *bug;
    switch_channel_t    *channel;
    avmd_session_t      *avmd_session;
    switch_time_t       start_time, stop_time, total_time;
    uint8_t             report_status = 0;
    avmd_beep_state_t   beep_status = BEEP_NOTDETECTED;

    if (session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "FreeSWITCH is NULL! Please report to developers\n");
        return;
    }

    /* Get current channel of the session to tag the session. This indicates that our module is present
     * At this moment this cannot return NULL, it will either succeed or assert failed, but we make ourself secure anyway */
    channel = switch_core_session_get_channel(session);
    if (channel == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No channel for FreeSWITCH session! Please report this to the developers.\n");
        return;
    }

    bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
    if (bug == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Stop failed - no avmd session running on this channel [%s]!\n", switch_channel_get_name(channel));
        return;
    }

    avmd_session = switch_core_media_bug_get_user_data(bug);
    if (avmd_session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Stop failed - no avmd session object, stop event not fired on this channel [%s]!\n", switch_channel_get_name(channel));
    } else {
        switch_mutex_lock(avmd_session->mutex);
        report_status = avmd_session->settings.report_status;
        beep_status = avmd_session->state.beep_state;
        avmd_session->stop_time = switch_micro_time_now();
        start_time = avmd_session->start_time;
        stop_time = avmd_session->stop_time;
        total_time = stop_time - start_time;
        switch_mutex_unlock(avmd_session->mutex);
        avmd_fire_event(AVMD_EVENT_SESSION_STOP, session, 0, 0, 0, 0, beep_status, 1, 0, 0, start_time, stop_time, 0, 0, 0);
        if (report_status == 1) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Avmd on channel [%s] stopped, beep status: [%s], total running time [%" PRId64 "] [us]\n", switch_channel_get_name(channel), beep_status == BEEP_DETECTED ? "DETECTED" : "NOTDETECTED", total_time);
        }
    }
    switch_channel_set_private(channel, "_avmd_", NULL);
    switch_core_media_bug_remove(session, &bug);

    return;
}

/*! \brief FreeSWITCH application handler function.
 *  This handles calls made from applications such as LUA and the dialplan.
 */
SWITCH_STANDARD_APP(avmd_start_function) {
    switch_media_bug_t  *bug;
    switch_channel_t    *channel;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "YOU ARE USING DEPRECATED APP INTERFACE. Please read documentation about new syntax\n");
    if (session == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No FreeSWITCH session assigned!\n");
        return;
    }

    channel = switch_core_session_get_channel(session);

    bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
    if (bug != NULL) {
        if (strcasecmp(data, "stop") == 0) {
            switch_channel_set_private(channel, "_avmd_", NULL);
            switch_core_media_bug_remove(session, &bug);
            return;
        }
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");
        return;
    }
    avmd_start_app(session, NULL);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_avmd_shutdown) {
    size_t session_n;
#ifndef WIN32
    int res;
#endif

    switch_mutex_lock(avmd_globals.mutex);

    session_n = avmd_globals.session_n;
    if (session_n > 0) {
        switch_mutex_unlock(avmd_globals.mutex);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PLEASE DO NOT RELOAD MODULE WHILE SESSIONS ARE RUNNING\n");
    }

    avmd_unregister_all_events();

#ifndef WIN32
    if (avmd_globals.settings.fast_math == 1) {
        res = destroy_fast_acosf();
        if (res != 0) {
            switch (res) {
                case -1:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed unmap arc cosine table\n");
                    break;
                case -2:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed closing arc cosine table\n");
                    break;
                default:
                    break;
            }
        }
    }
#endif

    switch_event_unbind_callback(avmd_reloadxml_event_handler);
    switch_mutex_unlock(avmd_globals.mutex);
    switch_mutex_destroy(avmd_globals.mutex);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Advanced voicemail detection disabled\n");
    return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH API handler function. */
SWITCH_STANDARD_API(avmd_api_main) {
    switch_media_bug_t  *bug = NULL;
    avmd_session_t      *avmd_session = NULL;
    switch_channel_t    *channel = NULL;
    int         argc;
    const char  *uuid = NULL, *uuid_dup = NULL;
    const char  *command = NULL;
    char        *dupped = NULL, *argv[AVMD_PARAMS_API_MAX + 1] = { 0 };
    switch_core_media_flag_t    flags = 0;
    switch_status_t             status = SWITCH_STATUS_SUCCESS;
    switch_core_session_t       *fs_session = NULL;

    switch_mutex_lock(avmd_globals.mutex);

    if (zstr(cmd)) {
        stream->write_function(stream, "-ERR, bad command!\n-USAGE: %s\n\n", AVMD_SYNTAX);
        goto end;
    }

    dupped = strdup(cmd);
    switch_assert(dupped);
    argc = switch_separate_string((char*)dupped, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

    if (argc < AVMD_PARAMS_API_MIN) {
        stream->write_function(stream, "-ERR, avmd takes [%u] min and [%u] max parameters!\n-USAGE: %s\n\n", AVMD_PARAMS_API_MIN, AVMD_PARAMS_API_MAX, AVMD_SYNTAX);
        goto end;
    }

    command = argv[0];
    if (strcasecmp(command, "reload") == 0) {
        status = avmd_load_xml_configuration(NULL);
        if (avmd_globals.settings.report_status == 1) {
            if (status != SWITCH_STATUS_SUCCESS) {
                stream->write_function(stream, "-ERR, couldn't reload XML configuration\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't reload XML configuration\n");
            } else {
                stream->write_function(stream, "+OK\n XML reloaded\n\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "XML reloaded\n");
            }
            goto end;
        }
    }
    if (strcasecmp(command, "load") == 0) {
        if (argc != 2) {
            stream->write_function(stream, "-ERR, load command takes 1 parameter!\n-USAGE: %s\n\n", AVMD_SYNTAX);
            goto end;
        }
        command = argv[1];
        if (strcasecmp(command, "inbound") == 0) {
            status = avmd_load_xml_inbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                if (status != SWITCH_STATUS_SUCCESS) {
                    stream->write_function(stream, "-ERR, couldn't load XML configuration\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load XML configuration\n");
                } else {
                    stream->write_function(stream, "+OK\n inbound XML configuration loaded\n\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Inbound XML configuration loaded\n");
                }
                goto end;
            }
        } else if (strcasecmp(command, "outbound") == 0) {
            status = avmd_load_xml_outbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                if (status != SWITCH_STATUS_SUCCESS) {
                    stream->write_function(stream, "-ERR, couldn't load XML configuration\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load XML configuration\n");
                } else {
                    stream->write_function(stream, "+OK\n outbound XML configuration loaded\n\n");
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Outbound XML configuration loaded\n");
                }
                goto end;
            }
        } else {
            stream->write_function(stream, "-ERR, load command: bad syntax!\n-USAGE: %s\n\n", AVMD_SYNTAX);
        }
        goto end;
    }
    if (strcasecmp(command, "set") == 0) {
        if (argc != 2) {
            stream->write_function(stream, "-ERR, set command takes 1 parameter!\n-USAGE: %s\n\n", AVMD_SYNTAX);
            goto end;
        }
        command = argv[1];
        if (strcasecmp(command, "inbound") == 0) {
            avmd_set_xml_inbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                stream->write_function(stream, "+OK\n inbound XML configuration loaded\n\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Inbound XML configuration loaded\n");
            }
        } else if (strcasecmp(command, "outbound") == 0) {
            avmd_set_xml_outbound_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                stream->write_function(stream, "+OK\n outbound XML configuration loaded\n\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Outbound XML configuration loaded\n");
            }
        } else if (strcasecmp(command, "default") == 0) {
            avmd_set_xml_default_configuration(NULL);
            if (avmd_globals.settings.report_status == 1) {
                stream->write_function(stream, "+OK\n reset to factory settings\n\n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Reset to factory settings\n");
            }
        } else {
            stream->write_function(stream, "-ERR, set command: bad syntax!\n-USAGE: %s\n\n", AVMD_SYNTAX);
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

    fs_session = switch_core_session_locate(uuid);  /* using uuid locate a reference to the FreeSWITCH session */
    if (fs_session == NULL) {
        stream->write_function(stream, "-ERR, no FreeSWITCH session for uuid [%s]!\n-USAGE: %s\n\n", uuid, AVMD_SYNTAX);
        goto end;
    }

    /* Get current channel of the session to tag the session. This indicates that our module is present
     * At this moment this cannot return NULL, it will either succeed or assert failed, but we make ourself secure anyway */
    channel = switch_core_session_get_channel(fs_session);
    if (channel == NULL) {
        stream->write_function(stream, "-ERR, no channel for FreeSWITCH session [%s]!\n Please report this to the developers\n\n", uuid);
        goto end;
    }

    bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
    if (bug != NULL) {
        if (strcasecmp(command, "stop") == 0) {
            avmd_session = (avmd_session_t*) switch_core_media_bug_get_user_data(bug);
            if (avmd_session == NULL) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Stop failed - no avmd session object on this channel [%s]!\n", switch_channel_get_name(channel));
                goto end;
            }
            uuid_dup = switch_core_strdup(switch_core_session_get_pool(fs_session), uuid);
            switch_channel_set_private(channel, "_avmd_", NULL);
            switch_core_media_bug_remove(fs_session, &bug);
            avmd_fire_event(AVMD_EVENT_SESSION_STOP, fs_session, 0, 0, 0, 0, 0, 0, 0, 0, avmd_session->start_time, avmd_session->stop_time, 0, 0, 0);
            if (avmd_globals.settings.report_status == 1) {
                stream->write_function(stream, "+OK\n [%s] [%s] stopped\n\n", uuid_dup, switch_channel_get_name(channel));
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_INFO, "Avmd on channel [%s] stopped!\n", switch_channel_get_name(channel));
            }
            goto end;
        }
        if (avmd_globals.settings.report_status == 1) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Avmd already started!\n");
            stream->write_function(stream, "-ERR, avmd for FreeSWITCH session [%s]\n already started\n\n", uuid);
        }
        goto end;
    }

    if (strcasecmp(command, "stop") == 0) {
        uuid_dup = switch_core_strdup(switch_core_session_get_pool(fs_session), uuid);
        stream->write_function(stream, "+ERR, avmd has not yet been started on\n [%s] [%s]\n\n", uuid_dup, switch_channel_get_name(channel));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Stop failed - avmd has not yet been started on channel [%s]!\n", switch_channel_get_name(channel));
        goto end;
    }
    if ((SWITCH_CALL_DIRECTION_OUTBOUND == switch_channel_direction(channel)) && (avmd_globals.settings.outbound_channnel == 1)) {
            flags |= SMBF_READ_REPLACE;
    }
    if ((SWITCH_CALL_DIRECTION_INBOUND == switch_channel_direction(channel)) && (avmd_globals.settings.inbound_channnel == 1)) {
            flags |= SMBF_WRITE_REPLACE;
    }
    if (flags == 0) {
        stream->write_function(stream, "-ERR, can't set direction for channel [%s]\n for FreeSWITCH session [%s]. Please check avmd configuration\n\n", switch_channel_get_name(channel), uuid);
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Can't set direction for channel [%s]\n", switch_channel_get_name(channel));
        status = SWITCH_STATUS_FALSE;
        goto end;
    }
    if ((SWITCH_CALL_DIRECTION_OUTBOUND == switch_channel_direction(channel)) && (avmd_globals.settings.outbound_channnel == 1)) {
        if (switch_channel_test_flag(channel, CF_MEDIA_SET) == 0) {
            stream->write_function(stream, "-ERR, channel [%s] for FreeSWITCH session [%s]\n has no read codec assigned yet. Please try again.\n\n", switch_channel_get_name(channel), uuid);
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Channel [%s] has no codec assigned yet. Please try again\n", switch_channel_get_name(channel));
            status = SWITCH_STATUS_FALSE;
            goto end;
        }
    }
    if (strcasecmp(command, "start") != 0) { /* If we don't see the expected start exit */
        stream->write_function(stream, "-ERR, did you mean\n api avmd %s start ?\n-USAGE: %s\n\n", uuid, AVMD_SYNTAX);
        goto end;
    }

    avmd_session = (avmd_session_t *) switch_core_session_alloc(fs_session, sizeof(avmd_session_t)); /* Allocate memory attached to this FreeSWITCH session for use in the callback routine and to store state information */
    status = init_avmd_session_data(avmd_session, fs_session, NULL);
    if (status != SWITCH_STATUS_SUCCESS) {
        stream->write_function(stream, "-ERR, failed to initialize avmd session\n for FreeSWITCH session [%s]\n", uuid);
        switch (status) {
            case SWITCH_STATUS_MEMERR:
                stream->write_function(stream, "-ERR, buffer error\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Failed to init avmd session. Buffer error!\n");
                break;
            case SWITCH_STATUS_MORE_DATA:
                stream->write_function(stream, "-ERR, SMA buffer size is 0\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Failed to init avmd session. SMA buffer size is 0!\n");
                break;
            case SWITCH_STATUS_FALSE:
                stream->write_function(stream, "-ERR, SMA buffer error\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Failed to init avmd session. SMA buffers error\n");
                break;
            default:
                stream->write_function(stream, "-ERR, unknown error\n\n");
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Failed to init avmd session. Unknown error\n");
                break;
        }
        goto end;
    }

    status = switch_core_media_bug_add(fs_session, "avmd", NULL, avmd_callback, avmd_session, 0, flags, &bug); /* Add a media bug that allows me to intercept the reading leg of the audio stream */

    if (status != SWITCH_STATUS_SUCCESS) { /* If adding a media bug fails exit */
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_ERROR, "Failed to add media bug!\n");
        stream->write_function(stream, "-ERR, [%s] failed to add media bug!\n\n", uuid);
        goto end;
    }

    switch_channel_set_private(channel, "_avmd_", bug); /* Set the vmd tag to detect an existing vmd media bug */

    avmd_fire_event(AVMD_EVENT_SESSION_START, fs_session, 0, 0, 0, 0, 0, 0, 0, 0, avmd_session->start_time, 0, 0, 0, 0);
    if (avmd_globals.settings.report_status == 1) {
        stream->write_function(stream, "+OK\n [%s] [%s] started!\n\n", uuid, switch_channel_get_name(channel));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_INFO, "Avmd on channel [%s] started!\n", switch_channel_get_name(channel));
        switch_assert(status == SWITCH_STATUS_SUCCESS);
    }
end:

    if (status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fs_session), SWITCH_LOG_INFO, "AVMD session NOT started\n");
            if (avmd_globals.settings.report_status == 1) {
                if ((uuid != NULL) && (channel != NULL)) {
                    stream->write_function(stream, "+ERR\n [%s] [%s] NOT started!\n\n", uuid, switch_channel_get_name(channel));
                } else {
                    stream->write_function(stream, "+ERR\n AVMD session NOT started!\n\n", switch_channel_get_name(channel));
                }
            }
    }
    if (fs_session) {
        switch_core_session_rwunlock(fs_session);
    }

    switch_safe_free(dupped);

    switch_mutex_unlock(avmd_globals.mutex);

    return SWITCH_STATUS_SUCCESS;
}

static int
avmd_decision_amplitude(const avmd_session_t *s, const struct avmd_buffer *b, double v, double rsd_threshold) {
    double a, rsd;
    size_t lpos;

    lpos = b->sma_b.lpos;
    if ((lpos >= AVMD_BEEP_LEN(s->rate) / b->resolution) && ((s->settings.require_continuous_streak_amp == 1 && (b->sma_amp_b.lpos > s->settings.sample_n_continuous_streak_amp) && (b->samples_streak_amp == 0))
            || (s->settings.require_continuous_streak_amp == 0 && (b->sma_amp_b.lpos > 1)))) {
        a = fabs(b->sma_amp_b.sma);
        if (a < AVMD_MIN_AMP) {
            return 0;
        }
        rsd = sqrt(v) / a;
        if (rsd < rsd_threshold) {
            return 1;
        }
    }
    return 0;
}

static int
avmd_decision_freq(const avmd_session_t *s, const struct avmd_buffer *b, double v, double rsd_threshold) {
    double f, rsd;
    size_t lpos;
    f = AVMD_TO_HZ(s->rate, fabs(b->sma_b_fir.sma));
    if ((f < AVMD_MIN_FREQUENCY) || (f > AVMD_MAX_FREQUENCY)) {
        return 0;
    }
    lpos = b->sma_b.lpos;
    if ((lpos >= AVMD_BEEP_LEN(s->rate) / b->resolution) && ((s->settings.require_continuous_streak == 1 && (b->sma_b.lpos > s->settings.sample_n_continuous_streak) && (b->samples_streak == 0))
            || (s->settings.require_continuous_streak == 0 && (b->sma_b.lpos > 1)))) {
        rsd = sqrt(v) / f;
        if ((rsd < 0.3 * rsd_threshold) && (b->sma_amp_b.sma >= 0.005 * b->amplitude_max)) {
            return 1;
        }
        if ((rsd < 0.6 * rsd_threshold) && (b->sma_amp_b.sma >= 0.01 * b->amplitude_max)) {
            return 1;
        }
        if ((rsd < rsd_threshold) && (b->sma_amp_b.sma >= 0.015 * b->amplitude_max)) {
            return 1;
        }
    }
    return 0;
}

static void avmd_report_detection(avmd_session_t *s, enum avmd_detection_mode mode, const struct avmd_detector *d) {
    switch_channel_t    *channel;
    switch_time_t       detection_time;
    double      f_sma = 0.0;
    double      v_amp = 9999.9, v_fir = 9999.9;

    const struct avmd_buffer *b = &d->buffer;
    const sma_buffer_t    *sma_b_fir = &b->sma_b_fir;
    const sma_buffer_t    *sqa_b_fir = &b->sqa_b_fir;

    const sma_buffer_t    *sma_amp_b = &b->sma_amp_b;
    const sma_buffer_t    *sqa_amp_b = &b->sqa_amp_b;

    channel = switch_core_session_get_channel(s->session);

    s->detection_stop_time = switch_micro_time_now();                                                           /* stop detection timer     */
    detection_time = s->detection_stop_time - s->detection_start_time;                                          /* detection time length    */
    switch_channel_set_variable_printf(channel, "avmd_total_time", "[%" PRId64 "]", detection_time / 1000);
    switch_channel_execute_on(channel, "execute_on_avmd_beep");
    switch_channel_set_variable(channel, "avmd_detect", "TRUE");
    switch (mode) {

        case AVMD_DETECT_AMP:
            v_amp = sqa_amp_b->sma - (sma_amp_b->sma * sma_amp_b->sma);                                               /* calculate variance of amplitude (biased estimator) */
            avmd_fire_event(AVMD_EVENT_BEEP, s->session, 0, 0, sma_amp_b->sma, v_amp, 0, 0, s->detection_start_time, s->detection_stop_time, 0, 0, b->resolution, b->offset, d->idx);
            if (s->settings.report_status == 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_INFO, "<<< AVMD - Beep Detected [%u][%u][%u][%u]: amplitude = [%f](max [%f]) variance = [%f], detection time [%" PRId64 "] [us] >>>\n",
                        mode, b->resolution, b->offset, d->idx, sma_amp_b->sma, b->amplitude_max, v_amp, detection_time);
            }
            break;

        case AVMD_DETECT_FREQ:
            f_sma = sma_b_fir->sma;
            v_fir = sqa_b_fir->sma - (sma_b_fir->sma * sma_b_fir->sma);                                               /* calculate variance of filtered samples */
            avmd_fire_event(AVMD_EVENT_BEEP, s->session, AVMD_TO_HZ(s->rate, f_sma), v_fir, 0, 0, 0, 0, s->detection_start_time, s->detection_stop_time, 0, 0, b->resolution, b->offset, d->idx);
            if (s->settings.report_status == 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_INFO, "<<< AVMD - Beep Detected [%u][%u][%u][%u]: f = [%f] variance = [%f], detection time [%" PRId64 "] [us] >>>\n",
                        mode, b->resolution, b->offset, d->idx, AVMD_TO_HZ(s->rate, f_sma), v_fir, detection_time);
            }
            break;

        case AVMD_DETECT_BOTH:
            v_amp = sqa_amp_b->sma - (sma_amp_b->sma * sma_amp_b->sma);                                               /* calculate variance of amplitude (biased estimator) */
            f_sma = sma_b_fir->sma;
            v_fir = sqa_b_fir->sma - (sma_b_fir->sma * sma_b_fir->sma);                                               /* calculate variance of filtered samples */
            avmd_fire_event(AVMD_EVENT_BEEP, s->session, AVMD_TO_HZ(s->rate, f_sma), v_fir, sma_amp_b->sma, v_amp, 0, 0, s->detection_start_time, s->detection_stop_time, 0, 0, b->resolution, b->offset, d->idx);
            if (s->settings.report_status == 1) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_INFO, "<<< AVMD - Beep Detected [%u][%u][%u][%u]: f = [%f] variance = [%f], amplitude = [%f](max [%f]) variance = [%f], detection time [%" PRId64 "] [us] >>>\n",
                        mode, b->resolution, b->offset, d->idx, AVMD_TO_HZ(s->rate, f_sma), v_fir, sma_amp_b->sma, b->amplitude_max, v_amp, detection_time);
            }
            break;

        default:
            break;
    }
    s->state.beep_state = BEEP_DETECTED;
}

static uint8_t
avmd_detection_in_progress(avmd_session_t *s) {
    uint8_t idx = 0;
    while (idx < (s->settings.detectors_n + s->settings.detectors_lagged_n)) {
        switch_mutex_lock(s->detectors[idx].mutex);
        if (s->detectors[idx].flag_processing_done == 0) {
            switch_mutex_unlock(s->detectors[idx].mutex);
            return 1;
        }
        switch_mutex_unlock(s->detectors[idx].mutex);
        ++idx;
    }
    return 0;
}

static enum avmd_detection_mode
avmd_detection_result(avmd_session_t *s) {
    enum avmd_detection_mode res;
    uint8_t idx = 0;
    while (idx < (s->settings.detectors_n + s->settings.detectors_lagged_n)) {
        res = s->detectors[idx].result;
        if (res != AVMD_DETECT_NONE) {
            avmd_report_detection(s, res, &s->detectors[idx]);
            return res;
        }
        ++idx;
    }
    return AVMD_DETECT_NONE;
}

/*! \brief Process one frame of data with avmd algorithm.
 * @param session An avmd session.
 * @param frame An audio frame.
 */
static void avmd_process(avmd_session_t *s, switch_frame_t *frame, uint8_t direction) {
    circ_buffer_t           *b;
    uint8_t                 idx;
    struct avmd_detector    *d;


    b = &s->b;

    switch_mutex_lock(s->mutex_detectors_done);
    while (avmd_detection_in_progress(s) == 1) {
        switch_thread_cond_wait(s->cond_detectors_done, s->mutex_detectors_done);
    }
    switch_mutex_unlock(s->mutex_detectors_done);

    if (s->state.beep_state == BEEP_DETECTED) {                         /* If beep has already been detected skip the CPU heavy stuff */
        return;
    }

    if (s->frame_n_to_skip > 0) {
        s->frame_n_to_skip--;
        return;
    }

	if (s->settings.debug) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(s->session), SWITCH_LOG_INFO, "AVMD: processing frame [%zu], direction=%s\n", s->frame_n, direction == AVMD_READ_REPLACE ? "READ" : "WRITE");
	}

    if (s->detection_start_time == 0) {
        s->detection_start_time = switch_micro_time_now();              /* start detection timer */
    }

    INSERT_INT16_FRAME(b, (int16_t *)(frame->data), frame->samples);    /* Insert frame of 16 bit samples into buffer */

    idx = 0;
    while (idx < (s->settings.detectors_n + s->settings.detectors_lagged_n)) {
        d = &s->detectors[idx];
        switch_mutex_lock(d->mutex);
        d = &s->detectors[idx];
        if (d->result == AVMD_DETECT_NONE) {
            d->flag_processing_done = 0;
            d->flag_should_exit = 0;
            d->samples = (s->frame_n == 0 ? frame->samples - AVMD_P : frame->samples);
            switch_thread_cond_signal(d->cond_start_processing);
        }
        switch_mutex_unlock(d->mutex);
        ++idx;
    }

    switch_mutex_lock(s->mutex_detectors_done);
    while (avmd_detection_in_progress(s) == 1) {
        switch_thread_cond_wait(s->cond_detectors_done, s->mutex_detectors_done);
    }
    avmd_detection_result(s);
    switch_mutex_unlock(s->mutex_detectors_done);

    ++s->frame_n;
    if (s->frame_n == 1) {
        s->pos += frame->samples - AVMD_P;
    } else {
        s->pos += frame->samples;
    }
    s->pos &= b->mask;

    return;
}

static void avmd_reloadxml_event_handler(switch_event_t *event) {
    avmd_load_xml_configuration(avmd_globals.mutex);
}

static enum avmd_detection_mode avmd_process_sample(avmd_session_t *s, circ_buffer_t *b, size_t sample_n, size_t pos, struct avmd_detector *d) {
    struct avmd_buffer          *buffer = &d->buffer;
    uint16_t                    sample_to_skip_n = s->settings.sample_n_to_skip;
    enum avmd_detection_mode    mode = s->settings.mode;
    uint8_t     valid_amplitude = 1, valid_omega = 1;
    double      omega = 0.0, amplitude = 0.0;
    double      f = 0.0, f_fir = 0.0;
    double      v_amp = 9999.9, v_fir = 9999.9;

    sma_buffer_t    *sma_b = &buffer->sma_b;
    sma_buffer_t    *sqa_b = &buffer->sqa_b;

    sma_buffer_t    *sma_b_fir = &buffer->sma_b_fir;
    sma_buffer_t    *sqa_b_fir = &buffer->sqa_b_fir;

    sma_buffer_t    *sma_amp_b = &buffer->sma_amp_b;
    sma_buffer_t    *sqa_amp_b = &buffer->sqa_amp_b;

    if (sample_to_skip_n > 0) {
        sample_to_skip_n--;
        valid_amplitude = 0;
        valid_omega = 0;
        return AVMD_DETECT_NONE;
    }

    omega = avmd_desa2_tweaked(b, pos + sample_n, &amplitude);

    if (mode == AVMD_DETECT_AMP || mode == AVMD_DETECT_BOTH) {
        if (ISNAN(amplitude) || ISINF(amplitude)) {
            valid_amplitude = 0;
            if (s->settings.require_continuous_streak_amp == 1) {
                RESET_SMA_BUFFER(sma_amp_b);
                RESET_SMA_BUFFER(sqa_amp_b);
                buffer->samples_streak_amp = s->settings.sample_n_continuous_streak_amp;
                sample_to_skip_n = s->settings.sample_n_to_skip;
            }
        } else {
            if (ISINF(amplitude)) {
                amplitude = buffer->amplitude_max;
            }
            if (valid_amplitude == 1) {
                APPEND_SMA_VAL(sma_amp_b, amplitude);               /* append amplitude */
                APPEND_SMA_VAL(sqa_amp_b, amplitude * amplitude);
                if (s->settings.require_continuous_streak_amp == 1) {
                    if (buffer->samples_streak_amp > 0) {
                        --buffer->samples_streak_amp;
                        valid_amplitude = 0;
                    }
                }
            }
            if (sma_amp_b->sma > buffer->amplitude_max) {
                buffer->amplitude_max = sma_amp_b->sma;
            }
        }
    }

    if (mode == AVMD_DETECT_FREQ || mode == AVMD_DETECT_BOTH) {
        if (ISNAN(omega)) {
            valid_omega = 0;
            if (s->settings.require_continuous_streak == 1) {
                RESET_SMA_BUFFER(sma_b);
                RESET_SMA_BUFFER(sqa_b);
                RESET_SMA_BUFFER(sma_b_fir);
                RESET_SMA_BUFFER(sqa_b_fir);
                buffer->samples_streak = s->settings.sample_n_continuous_streak;
                sample_to_skip_n = s->settings.sample_n_to_skip;
            }
            sample_to_skip_n = s->settings.sample_n_to_skip;
        } else if (omega < -0.99999 || omega > 0.99999) {
            valid_omega = 0;
            if (s->settings.require_continuous_streak == 1) {
                RESET_SMA_BUFFER(sma_b);
                RESET_SMA_BUFFER(sqa_b);
                RESET_SMA_BUFFER(sma_b_fir);
                RESET_SMA_BUFFER(sqa_b_fir);
                buffer->samples_streak = s->settings.sample_n_continuous_streak;
                sample_to_skip_n = s->settings.sample_n_to_skip;
            }
        } else {
            if (valid_omega) {

#if !defined(WIN32) && defined(AVMD_FAST_MATH)
                f =  0.5 * (double) fast_acosf((float)omega);
#else
                f = 0.5 * acos(omega);
#endif /* !WIN32 && AVMD_FAST_MATH */
                f_fir = sma_b->pos > 1 ? (AVMD_MEDIAN_FILTER(sma_b->data[sma_b->pos - 2], sma_b->data[sma_b->pos - 1], f)) : f;

                APPEND_SMA_VAL(sma_b, f);                                                                           /* append frequency             */
                APPEND_SMA_VAL(sqa_b, f * f);
                APPEND_SMA_VAL(sma_b_fir, f_fir);                                                                   /* append filtered frequency    */
                APPEND_SMA_VAL(sqa_b_fir, f_fir * f_fir);
                if (s->settings.require_continuous_streak == 1) {
                    if (buffer->samples_streak > 0) {
                        --buffer->samples_streak;
                        valid_omega = 0;
                    }
                }
            }
        }
    }

    if (((mode == AVMD_DETECT_AMP) || (mode == AVMD_DETECT_BOTH)) && (valid_amplitude == 1)) {
        v_amp = sqa_amp_b->sma - (sma_amp_b->sma * sma_amp_b->sma);                                               /* calculate variance of amplitude (biased estimator) */
        if ((mode == AVMD_DETECT_AMP) && (avmd_decision_amplitude(s, buffer, v_amp, AVMD_AMPLITUDE_RSD_THRESHOLD) == 1)) {
            return AVMD_DETECT_AMP;
        }
    }
    if (((mode == AVMD_DETECT_FREQ) || (mode == AVMD_DETECT_BOTH)) && (valid_omega == 1)) {
        v_fir = sqa_b_fir->sma - (sma_b_fir->sma * sma_b_fir->sma);                                               /* calculate variance of filtered samples */
        if ((mode == AVMD_DETECT_FREQ) && (avmd_decision_freq(s, buffer, v_fir, AVMD_VARIANCE_RSD_THRESHOLD) == 1)) {
            return AVMD_DETECT_FREQ;
        }
        if (mode == AVMD_DETECT_BOTH) {
            if ((avmd_decision_amplitude(s, buffer, v_amp, AVMD_AMPLITUDE_RSD_THRESHOLD) == 1) && (avmd_decision_freq(s, buffer, v_fir, AVMD_VARIANCE_RSD_THRESHOLD) == 1) && (valid_omega == 1))  {
                return AVMD_DETECT_BOTH;
            }
        }
    }
    return AVMD_DETECT_NONE;
}

static void* SWITCH_THREAD_FUNC
avmd_detector_func(switch_thread_t *thread, void *arg) {
    size_t      sample_n = 0, samples = AVMD_P;
    size_t      pos;
    uint8_t     resolution, offset;
    avmd_session_t  *s;
    enum avmd_detection_mode res = AVMD_DETECT_NONE;
    struct avmd_detector *d;


    d = (struct avmd_detector*) arg;
    s = d->s;
    pos = s->pos;
    while (1) {
        switch_mutex_lock(d->mutex);
        while ((d->flag_processing_done == 1) && (d->flag_should_exit == 0)) {
            switch_thread_cond_wait(d->cond_start_processing, d->mutex);
        }
        /* master set processing_done flag to 0 or thread should exit */
        if (d->flag_should_exit == 1) {
            d->flag_processing_done = 1;
            goto end;
        }
        resolution = d->buffer.resolution;
        offset = d->buffer.offset;
        samples = d->samples;

        if (d->lagged == 1) {
            if (d->lag > 0) {
                --d->lag;
                goto done;
            }
            pos += AVMD_P;
        }

        switch_mutex_unlock(d->mutex);
        sample_n = 1;
        while (sample_n <= samples) {
            if (((sample_n + offset) % resolution) == 0) {
                res = avmd_process_sample(d->s, &s->b, sample_n, pos, d);
                if (res != AVMD_DETECT_NONE) {
                    break;
                }
            }
            ++sample_n;
        }
        switch_mutex_lock(d->mutex);
done:
        d->flag_processing_done = 1;
        d->result = res;
        switch_mutex_unlock(d->mutex);

        switch_mutex_lock(s->mutex_detectors_done);
        switch_thread_cond_signal(s->cond_detectors_done);
        switch_mutex_unlock(s->mutex_detectors_done);
    }
    return NULL;

end:
    switch_mutex_unlock(d->mutex);
    return NULL;
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
