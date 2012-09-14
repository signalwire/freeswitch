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
 *
 * mod_avmd.c -- Advanced Voicemail Detection Module
 *
 * This module detects voicemail beeps using a generalized approach.
 *
 */

#include <switch.h>
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

/*! Calculate how many audio samples per ms based on the rate */
#define SAMPLES_PER_MS(r, m) ((r) / (1000/(m)))
/*! Minimum beep length */
#define BEEP_TIME (100)
/*! How often to evaluate the output of desa2 in ms */
#define SINE_TIME (10)
/*! How long in samples does desa2 results get evaluated */
#define SINE_LEN(r) SAMPLES_PER_MS((r), SINE_TIME)
/*! How long in samples is the minimum beep length */
#define BEEP_LEN(r) SAMPLES_PER_MS((r), BEEP_TIME)
/*! Number of points in desa2 sample */
#define P (5)
/*! Guesstimate frame length in ms */
#define FRAME_TIME (20)
/*! Length in samples of the frame (guesstimate) */
#define FRAME_LEN(r) SAMPLES_PER_MS((r), FRAME_TIME)
/*! Conversion to Hertz */
#define TO_HZ(r, f) (((r) * (f)) / (2.0 * M_PI))
/*! Minimum beep frequency in Hertz */
#define MIN_FREQUENCY (300.0)
#define MIN_FREQUENCY_R(r) ((2.0 * M_PI * MIN_FREQUENCY) / (r))
/*! Maximum beep frequency in Hertz */
#define MAX_FREQUENCY (2500.0)
#define MAX_FREQUENCY_R(r) ((2.0 * M_PI * MAX_FREQUENCY) / (r))
/* decrease this value to eliminate false positives */
#define VARIANCE_THRESHOLD (0.001)

#include "amplitude.h"
#include "buffer.h"
#include "desa2.h"
//#include "goertzel.h"
#include "psi.h"
#include "sma_buf.h"
#include "options.h"

#ifdef FASTMATH
#include "fast_acosf.h"
#endif

/*! Syntax of the API call. */
#define AVMD_SYNTAX "<uuid> <command>"

/*! Number of expected parameters in api call. */
#define AVMD_PARAMS 2

/*! FreeSWITCH CUSTOM event type. */
#define AVMD_EVENT_BEEP "avmd::beep"


/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_avmd_shutdown);
SWITCH_STANDARD_API(avmd_api_main);

SWITCH_MODULE_LOAD_FUNCTION(mod_avmd_load);
SWITCH_MODULE_DEFINITION(mod_avmd, mod_avmd_load, NULL, NULL);
SWITCH_STANDARD_APP(avmd_start_function);

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
    switch_core_session_t *session;
    uint32_t rate;
    circ_buffer_t b;
    sma_buffer_t sma_b;
    sma_buffer_t sqa_b;
    size_t pos;
    double f;
    /* freq_table_t ft; */
    avmd_state_t state;
} avmd_session_t;

static void avmd_process(avmd_session_t *session, switch_frame_t *frame);
static switch_bool_t avmd_callback(switch_media_bug_t * bug, void *user_data, switch_abc_type_t type);
static void init_avmd_session_data(avmd_session_t *avmd_session,  switch_core_session_t *fs_session);


/*! \brief The avmd session data initialization function
 * @author Eric des Courtis
 * @param avmd_session A reference to a avmd session
 * @param fs_session A reference to a FreeSWITCH session
 */
static void init_avmd_session_data(avmd_session_t *avmd_session,  switch_core_session_t *fs_session)
{
	/*! This is a worst case sample rate estimate */
    avmd_session->rate = 48000;
    INIT_CIRC_BUFFER(&avmd_session->b, BEEP_LEN(avmd_session->rate), FRAME_LEN(avmd_session->rate), fs_session);

    avmd_session->session = fs_session;
    avmd_session->pos = 0;
    avmd_session->f = 0.0;
    avmd_session->state.last_beep = 0;
    avmd_session->state.beep_state = BEEP_NOTDETECTED;

    INIT_SMA_BUFFER(
        &avmd_session->sma_b,
        BEEP_LEN(avmd_session->rate) / SINE_LEN(avmd_session->rate),
        fs_session
    );

    INIT_SMA_BUFFER(
        &avmd_session->sqa_b,
        BEEP_LEN(avmd_session->rate) / SINE_LEN(avmd_session->rate),
        fs_session
    );
}


/*! \brief The callback function that is called when new audio data becomes available
 *
 * @author Eric des Courtis
 * @param bug A reference to the media bug.
 * @param user_data The session information for this call.
 * @param type The switch callback type.
 * @return The success or failure of the function.
 */
static switch_bool_t avmd_callback(switch_media_bug_t * bug, void *user_data, switch_abc_type_t type)
{
    avmd_session_t *avmd_session;
    switch_codec_t *read_codec;
    switch_frame_t *frame;


    avmd_session = (avmd_session_t *) user_data;
    if (avmd_session == NULL) {
        return SWITCH_FALSE;
    }

    switch (type) {

    case SWITCH_ABC_TYPE_INIT:
        read_codec = switch_core_session_get_read_codec(avmd_session->session);
        avmd_session->rate = read_codec->implementation->samples_per_second;
        /* avmd_session->vmd_codec.channels = read_codec->implementation->number_of_channels; */
        break;

    case SWITCH_ABC_TYPE_READ_PING:
        break;
    case SWITCH_ABC_TYPE_CLOSE:
		
        break;
    case SWITCH_ABC_TYPE_READ:
        break;
    case SWITCH_ABC_TYPE_WRITE:
        break;

    case SWITCH_ABC_TYPE_READ_REPLACE:
        frame = switch_core_media_bug_get_read_replace_frame(bug);
        avmd_process(avmd_session, frame);
        return SWITCH_TRUE;

    case SWITCH_ABC_TYPE_WRITE_REPLACE:
        break;
    }

    return SWITCH_TRUE;
}

/*! \brief FreeSWITCH module loading function
 *
 * @author Eric des Courtis
 * @return Load success or failure.
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_avmd_load)
{

    switch_application_interface_t *app_interface;
    switch_api_interface_t *api_interface;
    /* connect my internal structure to the blank pointer passed to me */
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    switch_log_printf(
        SWITCH_CHANNEL_LOG,
        SWITCH_LOG_NOTICE,
        "Advanced Voicemail detection enabled\n"
    );

#ifdef FASTMATH
    init_fast_acosf();
    switch_log_printf(
        SWITCH_CHANNEL_LOG,
        SWITCH_LOG_NOTICE,
        "Advanced Voicemail detection: fast math enabled\n"
    );
#endif

    SWITCH_ADD_APP(
        app_interface,
        "avmd",
        "Beep detection",
        "Advanced detection of voicemail beeps",
        avmd_start_function,
        "[start] [stop]",
        SAF_NONE
    );

    SWITCH_ADD_API(api_interface, "avmd", "Voicemail beep detection", avmd_api_main, AVMD_SYNTAX);

    /* indicate that the module should continue to be loaded */
    return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH application handler function.
 *  This handles calls made from applications such as LUA and the dialplan
 *
 * @author Eric des Courtis
 * @return Success or failure of the function.
 */
SWITCH_STANDARD_APP(avmd_start_function)
{
    switch_media_bug_t *bug;
    switch_status_t status;
    switch_channel_t *channel;
    avmd_session_t *avmd_session;

    if (session == NULL)
        return;

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
            SWITCH_LOG_WARNING,
            "Cannot run 2 at once on the same channel!\n"
        );

        return;
    }

    avmd_session = (avmd_session_t *)switch_core_session_alloc(session, sizeof(avmd_session_t));

    init_avmd_session_data(avmd_session, session);

    status = switch_core_media_bug_add(
        session,
        "avmd",
        NULL,
        avmd_callback,
        avmd_session,
        0,
        SMBF_READ_REPLACE,
        &bug
    );

    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(
            SWITCH_CHANNEL_SESSION_LOG(session),
            SWITCH_LOG_ERROR,
            "Failure hooking to stream\n"
        );

        return;
    }

    switch_channel_set_private(channel, "_avmd_", bug);
}

/*! \brief Called when the module shuts down
 *
 * @author Eric des Courtis
 * @return The success or failure of the function.
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_avmd_shutdown)
{

#ifdef FASTMATH
    destroy_fast_acosf();
#endif

    switch_log_printf(
        SWITCH_CHANNEL_LOG,
        SWITCH_LOG_NOTICE,
        "Advanced Voicemail detection disabled\n"
    );

    return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH API handler function.
 *  This function handles API calls such as the ones from mod_event_socket and in some cases
 *  scripts such as LUA scripts.
 *
 *  @author Eric des Courtis
 *  @return The success or failure of the function.
 */
SWITCH_STANDARD_API(avmd_api_main)
{
    switch_core_session_t *fs_session = NULL;
    switch_media_bug_t *bug;
    avmd_session_t *avmd_session;
    switch_channel_t *channel;
    switch_status_t status;
    int argc;
    char *argv[AVMD_PARAMS];
    char *ccmd = NULL;
    char *uuid;
    char *command;

    /* No command? Display usage */
    if (zstr(cmd)) {
        stream->write_function(stream, "-USAGE: %s\n", AVMD_SYNTAX);
        return SWITCH_STATUS_SUCCESS;
    }

    /* Duplicated contents of original string */
    ccmd = strdup(cmd);
    /* Separate the arguments */
    argc = switch_separate_string(ccmd, ' ', argv, AVMD_PARAMS);

    /* If we don't have the expected number of parameters
     * display usage */
    if (argc != AVMD_PARAMS) {
        stream->write_function(stream, "-USAGE: %s\n", AVMD_SYNTAX);
        goto end;
    }

    uuid = argv[0];
    command = argv[1];

    /* using uuid locate a reference to the FreeSWITCH session */
    fs_session = switch_core_session_locate(uuid);

    /* If the session was not found exit */
    if (fs_session == NULL) {
        stream->write_function(stream, "-USAGE: %s\n", AVMD_SYNTAX);
        goto end;
    }

    /* Get current channel of the session to tag the session
     * This indicates that our module is present */
    channel = switch_core_session_get_channel(fs_session);

    /* Is this channel already set? */
    bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_avmd_");
    /* If yes */
    if (bug != NULL) {
        /* If we have a stop remove audio bug */
        if (strcasecmp(command, "stop") == 0) {
            switch_channel_set_private(channel, "_avmd_", NULL);
            switch_core_media_bug_remove(fs_session, &bug);
            switch_safe_free(ccmd);
            stream->write_function(stream, "+OK\n");
            goto end;
        }

        /* We have already started */
        switch_log_printf(
            SWITCH_CHANNEL_SESSION_LOG(session),
            SWITCH_LOG_WARNING,
            "Cannot run 2 at once on the same channel!\n"
        );

        goto end;
    }

    /* If we don't see the expected start exit */
    if (strcasecmp(command, "start") != 0) {
        stream->write_function(stream, "-USAGE: %s\n", AVMD_SYNTAX);
        goto end;
    }

    /* Allocate memory attached to this FreeSWITCH session for
     * use in the callback routine and to store state information */
    avmd_session = (avmd_session_t *) switch_core_session_alloc(fs_session, sizeof(avmd_session_t));

    init_avmd_session_data(avmd_session, fs_session);

    /* Add a media bug that allows me to intercept the
     * reading leg of the audio stream */
    status = switch_core_media_bug_add(
        fs_session,
        "avmd",
        NULL,
        avmd_callback,
        avmd_session,
        0,
        SMBF_READ_REPLACE,
        &bug
    );

    /* If adding a media bug fails exit */
    if (status != SWITCH_STATUS_SUCCESS) {

        switch_log_printf(
            SWITCH_CHANNEL_SESSION_LOG(session),
            SWITCH_LOG_ERROR,
            "Failure hooking to stream\n"
        );

        goto end;
    }

    /* Set the vmd tag to detect an existing vmd media bug */
    switch_channel_set_private(channel, "_avmd_", bug);

    /* Everything went according to plan! Notify the user */
    stream->write_function(stream, "+OK\n");


end:

    if (fs_session) {
        switch_core_session_rwunlock(fs_session);
    }

    switch_safe_free(ccmd);

    return SWITCH_STATUS_SUCCESS;
}

/*! \brief Process one frame of data with avmd algorithm
 * @author Eric des Courtis
 * @param session An avmd session
 * @param frame A audio frame
 */
static void avmd_process(avmd_session_t *session, switch_frame_t *frame)
{
    switch_event_t *event;
    switch_status_t status;
    switch_event_t *event_copy;
    switch_channel_t *channel;

    circ_buffer_t *b;
    size_t pos;
    double f;
    double v;
//    double error = 0.0;
//    double success = 0.0;
//    double amp = 0.0;
//    double s_rate;
//    double e_rate;
//    double avg_a;
    //double sine_len;
    uint32_t sine_len_i;
    //uint32_t beep_len_i;
//    int valid;

	b = &session->b;

	/*! If beep has already been detected skip the CPU heavy stuff */
    if(session->state.beep_state == BEEP_DETECTED){
        return;
    }

	/*! Precompute values used heavily in the inner loop */
    sine_len_i = SINE_LEN(session->rate);
    //sine_len = (double)sine_len_i;
    //beep_len_i = BEEP_LEN(session->rate);
	
    channel = switch_core_session_get_channel(session->session);

	/*! Insert frame of 16 bit samples into buffer */
    INSERT_INT16_FRAME(b, (int16_t *)(frame->data), frame->samples);

    //switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session->session), SWITCH_LOG_INFO, "<<< AVMD sine_len_i=%d >>>\n", sine_len_i);

    /*! INNER LOOP -- OPTIMIZATION TARGET */
    for(pos = session->pos; pos < (GET_CURRENT_POS(b) - P); pos++){
       if ((pos % sine_len_i) == 0) {
                 /*! Get a desa2 frequency estimate every sine len */
		f = desa2(b, pos);

		if(f < MIN_FREQUENCY_R(session->rate) || f > MAX_FREQUENCY_R(session->rate)) {
			v = 99999.0;
        	        RESET_SMA_BUFFER(&session->sma_b);
 	               	RESET_SMA_BUFFER(&session->sqa_b);
 		} else {
			APPEND_SMA_VAL(&session->sma_b, f);
			APPEND_SMA_VAL(&session->sqa_b, f * f);
			
			/* calculate variance */
			v = session->sqa_b.sma - (session->sma_b.sma * session->sma_b.sma);

        	   	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session->session), SWITCH_LOG_DEBUG, "<<< AVMD v=%f f=%f %fHz sma=%f sqa=%f >>>\n", v, f, TO_HZ(session->rate, f), session->sma_b.sma, session->sqa_b.sma);
		}

		/*! If variance is less than threshold then we have detection */
            	if(v < VARIANCE_THRESHOLD){

				/*! Throw an event to FreeSWITCH */
                status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, AVMD_EVENT_BEEP);
                if(status != SWITCH_STATUS_SUCCESS) {
                    return;
                }

                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Beep-Status", "stop");
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session->session));
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-command", "avmd");

                if ((switch_event_dup(&event_copy, event)) != SWITCH_STATUS_SUCCESS) {
                    return;
                }

                switch_core_session_queue_event(session->session, &event);
                switch_event_fire(&event_copy);

                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session->session), SWITCH_LOG_DEBUG, "<<< AVMD - Beep Detected >>>\n");
                switch_channel_set_variable(channel, "avmd_detect", "TRUE");
                RESET_SMA_BUFFER(&session->sma_b);
		RESET_SMA_BUFFER(&session->sqa_b);
                session->state.beep_state = BEEP_DETECTED;

                return;
            }

            //amp = 0.0;
            //success = 0.0;
            //error = 0.0;
        }
    }
    session->pos = pos;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

