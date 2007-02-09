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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Neal Horman <neal at wanlink dot com>
 * Matt Klein <mklein@nmedia.net>
 * Michael Jerris <mike@jerris.com>
 *
 * switch_ivr.c -- IVR Library
 *
 */
#include <switch.h>
#include <switch_ivr.h>
#include <libteletone.h>

static const switch_state_handler_table_t noop_state_handler = {0};

static const switch_state_handler_table_t audio_bridge_peer_state_handlers;

typedef enum {
	IDX_CANCEL = -2,
	IDX_NADA = -1
} abort_t;

SWITCH_DECLARE(switch_status_t) switch_ivr_sleep(switch_core_session_t *session, uint32_t ms)
{
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_time_t start, now, done = switch_time_now() + (ms * 1000);
	switch_frame_t *read_frame;
	int32_t left, elapsed;
	
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	start = switch_time_now();

	for(;;) {
		now = switch_time_now();
		elapsed = (int32_t)((now - start) / 1000);
		left = ms - elapsed;

		if (!switch_channel_ready(channel)) {
			status = SWITCH_STATUS_FALSE;
			break;
		}

		if (now > done || left <= 0) {
			break;
		}

		if (switch_channel_test_flag(channel, CF_SERVICE) || 
            (!switch_channel_test_flag(channel, CF_ANSWERED) && !switch_channel_test_flag(channel, CF_EARLY_MEDIA))) {
			switch_yield(1000);
		} else {
			status = switch_core_session_read_frame(session, &read_frame, left, 0);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}
		}
	}
	

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_parse_event(switch_core_session_t *session, switch_event_t *event)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *cmd = switch_event_get_header(event, "call-command");
	unsigned long cmd_hash;
	apr_ssize_t hlen = APR_HASH_KEY_STRING;
	unsigned long CMD_EXECUTE = apr_hashfunc_default("execute", &hlen);
	unsigned long CMD_HANGUP = apr_hashfunc_default("hangup", &hlen);
	unsigned long CMD_NOMEDIA = apr_hashfunc_default("nomedia", &hlen);
	
    assert(channel != NULL);

	if (switch_strlen_zero(cmd)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Command!\n");
        return SWITCH_STATUS_FALSE;
    }

	hlen = (switch_size_t) strlen(cmd);
	cmd_hash = apr_hashfunc_default(cmd, &hlen);

	switch_channel_set_flag(channel, CF_EVENT_PARSE);
	

    if (cmd_hash == CMD_EXECUTE) {
        const switch_application_interface_t *application_interface;
        char *app_name = switch_event_get_header(event, "execute-app-name");
        char *app_arg = switch_event_get_header(event, "execute-app-arg");
						
        if (app_name && app_arg) {
            if ((application_interface = switch_loadable_module_get_application_interface(app_name))) {
                if (application_interface->application_function) {
                    application_interface->application_function(session, app_arg);
                }
            }
        }
    } else if (cmd_hash == CMD_HANGUP) {
        char *cause_name = switch_event_get_header(event, "hangup-cause");
        switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

        if (cause_name) {
            cause = switch_channel_str2cause(cause_name);
        }

        switch_channel_hangup(channel, cause);
    } else if (cmd_hash == CMD_NOMEDIA) {
        char *uuid = switch_event_get_header(event, "nomedia-uuid");
        switch_ivr_nomedia(uuid, SMF_REBRIDGE);
    } 
	

	switch_channel_clear_flag(channel, CF_EVENT_PARSE);
    return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_park(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel;
	switch_frame_t *frame;
	int stream_id = 0;
	switch_event_t *event;
	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);	

	switch_channel_answer(channel);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_PARK) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}

	switch_channel_set_flag(channel, CF_CONTROLLED);
	while (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_CONTROLLED)) {
        
        if ((status = switch_core_session_read_frame(session, &frame, -1, stream_id)) == SWITCH_STATUS_SUCCESS) {
            if (!SWITCH_READ_ACCEPTABLE(status)) {
                break;
            }
            
            if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
                switch_ivr_parse_event(session, event);
                switch_event_destroy(&event);
            }

			if (switch_channel_has_dtmf(channel)) {
                char dtmf[128];
                switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
            }

            if (switch_core_session_dequeue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
                switch_channel_event_set_data(channel, event);
                switch_event_fire(&event);
            }
        }
		
	}
	switch_channel_clear_flag(channel, CF_CONTROLLED);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNPARK) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_callback(switch_core_session_t *session,
                                                                   switch_input_args_t *args,
																   uint32_t timeout)
{
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_time_t started = 0;
	uint32_t elapsed;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (!args->input_callback) {
		return SWITCH_STATUS_GENERR;
	}

	if (timeout) {
		started = switch_time_now();
	}

	while(switch_channel_ready(channel)) {
		switch_frame_t *read_frame;
		switch_event_t *event;
		char dtmf[128];

		if (timeout) {
			elapsed = (uint32_t)((switch_time_now() - started) / 1000);
			if (elapsed >= timeout) {
				break;
			}
		}

		if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
			switch_ivr_parse_event(session, event);
			switch_event_destroy(&event);
		}

		if (switch_channel_has_dtmf(channel)) {
			switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
			status = args->input_callback(session, dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
		}

		if (switch_core_session_dequeue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
			status = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);			
			switch_event_destroy(&event);
		}

		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}

		if (switch_channel_test_flag(channel, CF_SERVICE)) {
			switch_yield(1000);
		} else {
			status = switch_core_session_read_frame(session, &read_frame, -1, 0);
		}

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
	}

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_count(switch_core_session_t *session,
																char *buf,
																uint32_t buflen,
																uint32_t maxdigits,
																const char *terminators,
																char *terminator,
																uint32_t timeout)
{
	uint32_t i = 0, x =  (uint32_t) strlen(buf);
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_time_t started = 0;
	uint32_t elapsed;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (terminator != NULL)
		*terminator = '\0';

	if (!switch_strlen_zero(terminators)) {
		for (i = 0 ; i < x; i++) {
			if (strchr(terminators, buf[i]) && terminator != NULL) {
				*terminator = buf[i];
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	if (timeout) {
		started = switch_time_now();
	}

	while(switch_channel_ready(channel)) {
		switch_frame_t *read_frame;
		switch_event_t *event;

		if (timeout) {
			elapsed = (uint32_t)((switch_time_now() - started) / 1000);
			if (elapsed >= timeout) {
				break;
			}
		}
		
		if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
			switch_ivr_parse_event(session, event);
			switch_event_destroy(&event);
		}
		
		if (switch_channel_has_dtmf(channel)) {
			char dtmf[128];

			switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
			for(i =0 ; i < (uint32_t) strlen(dtmf); i++) {

				if (!switch_strlen_zero(terminators) && strchr(terminators, dtmf[i]) && terminator != NULL) {
					*terminator = dtmf[i];
					return SWITCH_STATUS_SUCCESS;
				}

				buf[x++] = dtmf[i];
				buf[x] = '\0';
				if (x >= buflen || x >= maxdigits) {
					return SWITCH_STATUS_SUCCESS;
				}
			}
		}

		if (switch_channel_test_flag(channel, CF_SERVICE)) {
			switch_yield(1000);
		} else {
			status = switch_core_session_read_frame(session, &read_frame, -1, 0);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}
		}
	}

	return status;
}



SWITCH_DECLARE(switch_status_t) switch_ivr_record_file(switch_core_session_t *session, 
                                                       switch_file_handle_t *fh,
                                                       char *file,
                                                       switch_input_args_t *args,
                                                       uint32_t limit)
{
	switch_channel_t *channel;
    char dtmf[128];
	switch_file_handle_t lfh = {0};
	switch_frame_t *read_frame;
	switch_codec_t codec, *read_codec;
	char *codec_name;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *p;
	const char *vval;
    time_t start = 0;
	uint32_t org_silence_hits = 0;

	if (!fh) {
		fh = &lfh;
	}

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(session);
	assert(read_codec != NULL);

	fh->channels = read_codec->implementation->number_of_channels;
	fh->samplerate = read_codec->implementation->samples_per_second;


	if (switch_core_file_open(fh,
							  file,
							  SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT,
							  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_core_session_reset(session);
		return SWITCH_STATUS_GENERR;
	}

	switch_channel_answer(channel);

	if ((p = switch_channel_get_variable(channel, "RECORD_TITLE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_TITLE, vval);
		switch_channel_set_variable(channel, "RECORD_TITLE", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_COPYRIGHT"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_COPYRIGHT, vval);
		switch_channel_set_variable(channel, "RECORD_COPYRIGHT", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_SOFTWARE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_SOFTWARE, vval);
		switch_channel_set_variable(channel, "RECORD_SOFTWARE", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_ARTIST"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_ARTIST, vval);
		switch_channel_set_variable(channel, "RECORD_ARTIST", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_COMMENT"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_COMMENT, vval);
		switch_channel_set_variable(channel, "RECORD_COMMENT", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_DATE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_DATE, vval);
		switch_channel_set_variable(channel, "RECORD_DATE", NULL);
	}
	
	codec_name = "L16";
	if (switch_core_codec_init(&codec,
							   codec_name,
							   NULL,
							   read_codec->implementation->samples_per_second,
							   read_codec->implementation->microseconds_per_frame / 1000,
							   read_codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activated\n");
		switch_core_session_set_read_codec(session, &codec);		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Raw Codec Activation Failed %s@%uhz %u channels %dms\n",
							  codec_name, fh->samplerate, fh->channels, read_codec->implementation->microseconds_per_frame / 1000);
		switch_core_file_close(fh);
		switch_core_session_reset(session);
		return SWITCH_STATUS_GENERR;
	}
	
    if (limit) {
        start = time(NULL);
    }

    if (fh->thresh) {
        if (fh->silence_hits) {
			fh->silence_hits = fh->samplerate * fh->silence_hits / read_codec->implementation->samples_per_frame;
		} else {
            fh->silence_hits = fh->samplerate * 3 / read_codec->implementation->samples_per_frame;
        }
		org_silence_hits = fh->silence_hits;
	}

	while(switch_channel_ready(channel)) {
		switch_size_t len;
		switch_event_t *event;

        if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
			switch_ivr_parse_event(session, event);
			switch_event_destroy(&event);
		}

        if (start && (time(NULL) - start) > limit) {
            break;
        }

		if (args && (args->input_callback || args->buf || args->buflen)) {
			/*
			  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			  if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			*/
			if (switch_channel_has_dtmf(channel)) {
				if (!args->input_callback && !args->buf) {
					status = SWITCH_STATUS_BREAK;
					break;
				}
				switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
				if (args->input_callback) {
					status = args->input_callback(session, dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
				} else {
					switch_copy_string((char *)args->buf, dtmf, args->buflen);
					status = SWITCH_STATUS_BREAK;
				}
			}

			if (args->input_callback) {
				if (switch_core_session_dequeue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
					status = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);			
					switch_event_destroy(&event);
				}
			}


			if (status != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
		
		status = switch_core_session_read_frame(session, &read_frame, -1, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

        if (fh->thresh) {
            int16_t *fdata = (int16_t *) read_frame->data;
            uint32_t samples = read_frame->datalen / sizeof(*fdata);
            uint32_t score, count = 0, j = 0;
            double energy = 0;

            for (count = 0; count < samples; count++) {
                energy += abs(fdata[j]);
                j += read_codec->implementation->number_of_channels;
            }
		
            score = (uint32_t)(energy / samples);
            if (score < fh->thresh) {
                if (!--fh->silence_hits) {
                    break;
                }
			} else {
				fh->silence_hits = org_silence_hits;
			}
        }

		if (!switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
			len = (switch_size_t) read_frame->datalen / 2;
			switch_core_file_write(fh, read_frame->data, &len);
		}
	}

	switch_core_session_set_read_codec(session, read_codec);
	switch_core_file_close(fh);
	switch_core_session_reset(session);
	return status;
}

static void record_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_file_handle_t *fh = (switch_file_handle_t *) user_data;
	uint8_t data[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = {0};

	frame.data = data;
	frame.buflen = SWITCH_RECCOMMENDED_BUFFER_SIZE;
	
	switch(type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
        if (fh) {
            switch_core_file_close(fh);
        }
        break;
	case SWITCH_ABC_TYPE_READ:
		if (fh) {
			switch_size_t len;

			if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
				len = (switch_size_t) frame.datalen / 2;
				switch_core_file_write(fh, frame.data, &len);
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
    default:
		break;
	}
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_record_session(switch_core_session_t *session, char *file) 
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	assert(channel != NULL);
	if ((bug = switch_channel_get_private(channel, file))) {
		switch_channel_set_private(channel, file, NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}
	
	return SWITCH_STATUS_FALSE;
	
}

SWITCH_DECLARE(switch_status_t) switch_ivr_record_session(switch_core_session_t *session, char *file,  switch_file_handle_t *fh)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	char *p;
	const char *vval;
	switch_media_bug_t *bug;
	switch_status_t status;

	if (!fh) {
		if (!(fh = switch_core_session_alloc(session, sizeof(*fh)))) {
			return SWITCH_STATUS_MEMERR;
		}
	}

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

    read_codec = switch_core_session_get_read_codec(session);
    assert(read_codec != NULL);

    fh->channels = read_codec->implementation->number_of_channels;
    fh->samplerate = read_codec->implementation->samples_per_second;


    if (switch_core_file_open(fh,
                              file,
                              SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT,
                              switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
        switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
        switch_core_session_reset(session);
        return SWITCH_STATUS_GENERR;
    }

    switch_channel_answer(channel);

	if ((p = switch_channel_get_variable(channel, "RECORD_TITLE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_TITLE, vval);
		switch_channel_set_variable(channel, "RECORD_TITLE", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_COPYRIGHT"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_COPYRIGHT, vval);
		switch_channel_set_variable(channel, "RECORD_COPYRIGHT", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_SOFTWARE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_SOFTWARE, vval);
		switch_channel_set_variable(channel, "RECORD_SOFTWARE", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_ARTIST"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_ARTIST, vval);
		switch_channel_set_variable(channel, "RECORD_ARTIST", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_COMMENT"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_COMMENT, vval);
		switch_channel_set_variable(channel, "RECORD_COMMENT", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_DATE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_DATE, vval);
		switch_channel_set_variable(channel, "RECORD_DATE", NULL);
	}

	

	if ((status = switch_core_media_bug_add(session,
											record_callback,
											fh,
											SMBF_BOTH,
											&bug)) != SWITCH_STATUS_SUCCESS) {
		switch_core_file_close(fh);
		return status;
	}

	switch_channel_set_private(channel, file, bug);
	
	return SWITCH_STATUS_SUCCESS;
}

typedef struct {
	switch_core_session_t *session;
	teletone_dtmf_detect_state_t dtmf_detect;
} switch_inband_dtmf_t;

static void inband_dtmf_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_inband_dtmf_t *pvt = (switch_inband_dtmf_t *) user_data;
	uint8_t data[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = {0};
	char digit_str[80];
	switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

	assert(channel != NULL);
	frame.data = data;
	frame.buflen = SWITCH_RECCOMMENDED_BUFFER_SIZE;

	switch(type) {
		case SWITCH_ABC_TYPE_INIT:
			break;
		case SWITCH_ABC_TYPE_CLOSE:
			break;
		case SWITCH_ABC_TYPE_READ:
			if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
				teletone_dtmf_detect(&pvt->dtmf_detect, frame.data, frame.samples);
				teletone_dtmf_get(&pvt->dtmf_detect, digit_str, sizeof(digit_str));
				if(digit_str[0]) {
					switch_channel_queue_dtmf(channel, digit_str);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DTMF DETECTED: [%s]\n", digit_str);
				}
			}
			break;
		case SWITCH_ABC_TYPE_WRITE:
		default:
			break;
	}
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_inband_dtmf_session(switch_core_session_t *session) 
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	assert(channel != NULL);
	if ((bug = switch_channel_get_private(channel, "dtmf"))) {
		switch_channel_set_private(channel, "dtmf", NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_inband_dtmf_session(switch_core_session_t *session)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_inband_dtmf_t *pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(session);
	assert(read_codec != NULL);

	if (!(pvt = switch_core_session_alloc(session, sizeof(*pvt)))) {
		return SWITCH_STATUS_MEMERR;
	}

	teletone_dtmf_detect_init(&pvt->dtmf_detect, read_codec->implementation->samples_per_second);

	pvt->session = session;

	switch_channel_answer(channel);	

	if ((status = switch_core_media_bug_add(session,
		inband_dtmf_callback,
		pvt,
		SMBF_READ_STREAM,
		&bug)) != SWITCH_STATUS_SUCCESS) {
			return status;
	}

	switch_channel_set_private(channel, "dtmf", bug);

	return SWITCH_STATUS_SUCCESS;
}

struct speech_thread_handle {
	switch_core_session_t *session;
	switch_asr_handle_t *ah;
	switch_media_bug_t *bug;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC speech_thread(switch_thread_t *thread, void *obj)
{
	struct speech_thread_handle *sth = (struct speech_thread_handle *) obj;
	switch_channel_t *channel = switch_core_session_get_channel(sth->session);
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	switch_status_t status;

	switch_thread_cond_create(&sth->cond, sth->pool);
	switch_mutex_init(&sth->mutex, SWITCH_MUTEX_NESTED, sth->pool);
	

	switch_core_session_read_lock(sth->session);
	switch_mutex_lock(sth->mutex);
	
	while (switch_channel_ready(channel) && !switch_test_flag(sth->ah, SWITCH_ASR_FLAG_CLOSED)) {
		char *xmlstr = NULL;
		
		switch_thread_cond_wait(sth->cond, sth->mutex);
		if (switch_core_asr_check_results(sth->ah, &flags) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *event;

			status = switch_core_asr_get_results(sth->ah, &xmlstr, &flags);
		
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				goto done;
			}


			if (switch_event_create(&event, SWITCH_EVENT_DETECTED_SPEECH) == SWITCH_STATUS_SUCCESS) {
				if (status == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Speech-Type", "detected-speech");
					switch_event_add_body(event, xmlstr);
				} else {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Speech-Type", "begin-speaking");
				}

				if (switch_test_flag(sth->ah, SWITCH_ASR_FLAG_FIRE_EVENTS)) {
					switch_event_t *dup;

					if (switch_event_dup(&dup, event) == SWITCH_STATUS_SUCCESS) {
						switch_event_fire(&dup);
					}
						
				}
				
				if (switch_core_session_queue_event(sth->session, &event) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event queue failed!\n");
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
					switch_event_fire(&event);
				}
			}
			
			switch_safe_free(xmlstr);
		}
	}
 done:
	
	switch_mutex_unlock(sth->mutex);
	switch_core_session_rwunlock(sth->session);

	return NULL;
}

static void speech_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	struct speech_thread_handle *sth = (struct speech_thread_handle *) user_data;
	uint8_t data[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = {0};
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;

	frame.data = data;
	frame.buflen = SWITCH_RECCOMMENDED_BUFFER_SIZE;
	
	switch(type) {
	case SWITCH_ABC_TYPE_INIT: {
		switch_thread_t *thread;
		switch_threadattr_t *thd_attr = NULL;

		switch_threadattr_create(&thd_attr, sth->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
        switch_thread_create(&thread, thd_attr, speech_thread, sth, sth->pool);
		
	}
		break;
	case SWITCH_ABC_TYPE_CLOSE: {
		switch_core_asr_close(sth->ah, &flags);
		switch_mutex_lock(sth->mutex);
		switch_thread_cond_signal(sth->cond);
		switch_mutex_unlock(sth->mutex);
    }
        break;
	case SWITCH_ABC_TYPE_READ:
		if (sth->ah) {
			if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
				if (switch_core_asr_feed(sth->ah, frame.data, frame.datalen, &flags) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error Feeding Data\n");
					return;
				}
				if (switch_core_asr_check_results(sth->ah, &flags) == SWITCH_STATUS_SUCCESS) {
					switch_mutex_lock(sth->mutex);
					switch_thread_cond_signal(sth->cond);
					switch_mutex_unlock(sth->mutex);
				}
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
    default:
		break;
	}
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_detect_speech(switch_core_session_t *session) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct speech_thread_handle *sth;

	assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		switch_channel_set_private(channel, SWITCH_SPEECH_KEY, NULL);
		switch_core_media_bug_remove(session, &sth->bug);
		return SWITCH_STATUS_SUCCESS;
	}
	
	return SWITCH_STATUS_FALSE;
	
}



SWITCH_DECLARE(switch_status_t) switch_ivr_pause_detect_speech(switch_core_session_t *session) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct speech_thread_handle *sth;

	assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		switch_core_asr_pause(sth->ah);
		return SWITCH_STATUS_SUCCESS;
	}
	
	return SWITCH_STATUS_FALSE;
	
}

SWITCH_DECLARE(switch_status_t) switch_ivr_resume_detect_speech(switch_core_session_t *session) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct speech_thread_handle *sth;

	assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		switch_core_asr_resume(sth->ah);
		return SWITCH_STATUS_SUCCESS;
	}
	
	return SWITCH_STATUS_FALSE;
	
}


SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_load_grammar(switch_core_session_t *session, char *grammar, char *path) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	struct speech_thread_handle *sth;

	assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {		
		if (switch_core_asr_load_grammar(sth->ah, grammar, path) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading Grammar\n");
			switch_core_asr_close(sth->ah, &flags);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_unload_grammar(switch_core_session_t *session, char *grammar) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	struct speech_thread_handle *sth;

	assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {		
		if (switch_core_asr_unload_grammar(sth->ah, grammar) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error unloading Grammar\n");
			switch_core_asr_close(sth->ah, &flags);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech(switch_core_session_t *session,
														 char *mod_name,
														 char *grammar,
														 char *path,
														 char *dest,
														 switch_asr_handle_t *ah)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	switch_status_t status;
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	struct speech_thread_handle *sth;
	char *val;

	if (!ah) {
		if (!(ah = switch_core_session_alloc(session, sizeof(*ah)))) {
			return SWITCH_STATUS_MEMERR;
		}
	}

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

    read_codec = switch_core_session_get_read_codec(session);
    assert(read_codec != NULL);


	if ((val = switch_channel_get_variable(channel, "fire_asr_events"))) {
		switch_set_flag(ah, SWITCH_ASR_FLAG_FIRE_EVENTS);
	}

	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		if (switch_core_asr_load_grammar(sth->ah, grammar, path) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading Grammar\n");
            switch_core_asr_close(sth->ah, &flags);
            return SWITCH_STATUS_FALSE;
        }

        return SWITCH_STATUS_SUCCESS;
	}
	
	if (switch_core_asr_open(ah, 
							 mod_name,
							 "L16",
							 read_codec->implementation->samples_per_second,
							 dest,
							 &flags,
							 switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {

		if (switch_core_asr_load_grammar(ah, grammar, path) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading Grammar\n");
			switch_core_asr_close(ah, &flags);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}


	sth = switch_core_session_alloc(session, sizeof(*sth));
	sth->pool = switch_core_session_get_pool(session);
	sth->session = session;
	sth->ah = ah;
	
	if ((status = switch_core_media_bug_add(session,
											speech_callback,
											sth,
											SMBF_READ_STREAM,
											&sth->bug)) != SWITCH_STATUS_SUCCESS) {
		switch_core_asr_close(ah, &flags);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return status;
	}

	switch_channel_set_private(channel, SWITCH_SPEECH_KEY, sth);
	
	return SWITCH_STATUS_SUCCESS;
}

#define FILE_STARTSAMPLES 1024 * 32
#define FILE_BLOCKSIZE 1024 * 8
#define FILE_BUFSIZE 1024 * 64

SWITCH_DECLARE(switch_status_t) switch_ivr_play_file(switch_core_session_t *session, 
                                                     switch_file_handle_t *fh,
                                                     char *file,
                                                     switch_input_args_t *args)
{
	switch_channel_t *channel;
	int16_t abuf[FILE_STARTSAMPLES];
	char dtmf[128];
	uint32_t interval = 0, samples = 0, framelen, sample_start = 0;
	uint32_t ilen = 0;
	switch_size_t olen = 0, llen = 0;
	switch_frame_t write_frame = {0};
	switch_timer_t timer;
	switch_core_thread_session_t thread_session;
	switch_codec_t codec;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	char *codec_name;
	int stream_id = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_file_handle_t lfh;
	switch_codec_t *read_codec = switch_core_session_get_read_codec(session);
	const char *p;
	char *title = "", *copyright = "", *software = "", *artist = "", *comment = "", *date = "";
	uint8_t asis = 0;
	char *ext;
    char *prefix;
    char *timer_name;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

    prefix = switch_channel_get_variable(channel, "sound_prefix");
    timer_name = switch_channel_get_variable(channel, "timer_name");
	
	if (file) {
        if (prefix && *file != '/' && *file != '\\' && *(file+1) != ':') {
            char *new_file;
            uint32_t len;
            len = (uint32_t)strlen(file) + (uint32_t)strlen(prefix) + 10;
            new_file = switch_core_session_alloc(session, len);
            snprintf(new_file, len, "%s/%s", prefix, file);
            file = new_file;
        }

		if ((ext = strrchr(file, '.'))) {
			ext++;
		} else {
			char *new_file;
			uint32_t len;
			ext = read_codec->implementation->iananame;
			len = (uint32_t)strlen(file) + (uint32_t)strlen(ext) + 2;
			new_file = switch_core_session_alloc(session, len);
			snprintf(new_file, len, "%s.%s", file, ext);
			file = new_file;
			asis = 1;
		}
	}

	if (!fh) {
		fh = &lfh;
		memset(fh, 0, sizeof(lfh));
	}


	if (fh->samples > 0) {
		sample_start = fh->samples;
		fh->samples = 0;
	}

	if (switch_core_file_open(fh,
							  file,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_core_session_reset(session);
		return SWITCH_STATUS_NOTFOUND;
	}


	write_frame.data = abuf;
	write_frame.buflen = sizeof(abuf);

    if (sample_start > 0) {
        uint32_t pos = 0;
        switch_core_file_seek(fh, &pos, sample_start, SEEK_CUR);
    }
	
	if (switch_core_file_get_string(fh, SWITCH_AUDIO_COL_STR_TITLE, &p) == SWITCH_STATUS_SUCCESS) {
		title = (char *) switch_core_session_strdup(session, (char *)p);
		switch_channel_set_variable(channel, "RECORD_TITLE", (char *)p);
	}
	
	if (switch_core_file_get_string(fh, SWITCH_AUDIO_COL_STR_COPYRIGHT, &p) == SWITCH_STATUS_SUCCESS) {
		copyright = (char *) switch_core_session_strdup(session, (char *)p);
		switch_channel_set_variable(channel, "RECORD_COPYRIGHT", (char *)p);
	}
	
	if (switch_core_file_get_string(fh, SWITCH_AUDIO_COL_STR_SOFTWARE, &p) == SWITCH_STATUS_SUCCESS) {
		software = (char *) switch_core_session_strdup(session, (char *)p);
		switch_channel_set_variable(channel, "RECORD_SOFTWARE", (char *)p);
	}
	
	if (switch_core_file_get_string(fh, SWITCH_AUDIO_COL_STR_ARTIST, &p) == SWITCH_STATUS_SUCCESS) {
		artist = (char *) switch_core_session_strdup(session, (char *)p);
		switch_channel_set_variable(channel, "RECORD_ARTIST", (char *)p);
	}
	
	if (switch_core_file_get_string(fh, SWITCH_AUDIO_COL_STR_COMMENT, &p) == SWITCH_STATUS_SUCCESS) {
		comment = (char *) switch_core_session_strdup(session, (char *)p);
		switch_channel_set_variable(channel, "RECORD_COMMENT", (char *)p);
	}
	
	if (switch_core_file_get_string(fh, SWITCH_AUDIO_COL_STR_DATE, &p) == SWITCH_STATUS_SUCCESS) {
		date = (char *) switch_core_session_strdup(session, (char *)p);
		switch_channel_set_variable(channel, "RECORD_DATE", (char *)p);
	}
#if 0
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					  "OPEN FILE %s %uhz %u channels\n"
					  "TITLE=%s\n"
					  "COPYRIGHT=%s\n"
					  "SOFTWARE=%s\n"
					  "ARTIST=%s\n"
					  "COMMENT=%s\n"
					  "DATE=%s\n", file, fh->samplerate, fh->channels,
					  title,
					  copyright,
					  software,
					  artist,
					  comment,
					  date);
#endif

	assert(read_codec != NULL);
	interval = read_codec->implementation->microseconds_per_frame / 1000;

	if (!fh->audio_buffer) {
		switch_buffer_create_dynamic(&fh->audio_buffer, FILE_BLOCKSIZE, FILE_BUFSIZE, 0);
	} 

	if (asis) {
		write_frame.codec = read_codec;
		samples = read_codec->implementation->samples_per_frame;
		framelen = read_codec->implementation->encoded_bytes_per_frame;
	} else {
		codec_name = "L16";

		if (switch_core_codec_init(&codec,
								   codec_name,
								   NULL,
								   fh->samplerate,
								   interval,
								   fh->channels,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, pool) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG,
							  SWITCH_LOG_DEBUG,
							  "Codec Activated %s@%uhz %u channels %dms\n",
							  codec_name,
							  fh->samplerate,
							  fh->channels,
							  interval);

			write_frame.codec = &codec;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed %s@%uhz %u channels %dms\n",
							  codec_name, fh->samplerate, fh->channels, interval);
			switch_core_file_close(fh);
			switch_core_session_reset(session);
			return SWITCH_STATUS_GENERR;
		}
		samples = codec.implementation->samples_per_frame;
		framelen = codec.implementation->bytes_per_frame;
	}


	if (timer_name) {
		uint32_t len;

		len = samples * 2;
		if (switch_core_timer_init(&timer, timer_name, interval, samples, pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
			switch_core_codec_destroy(&codec);
			switch_core_file_close(fh);
			switch_core_session_reset(session);
			return SWITCH_STATUS_GENERR;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "setup timer success %u bytes per %d ms!\n", len, interval);
	}
	write_frame.rate = fh->samplerate;

	if (timer_name) {
		/* start a thread to absorb incoming audio */
		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			switch_core_service_session(session, &thread_session, stream_id);
		}
	}

	ilen = samples;

	while(switch_channel_ready(channel)) {
		int done = 0;
		int do_speed = 1;
		int last_speed = -1;
		switch_event_t *event;
	

		if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
			switch_ivr_parse_event(session, event);
			switch_event_destroy(&event);
		}

		if (args && (args->input_callback || args->buf || args->buflen)) {
			/*
			  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			  if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			*/
			if (switch_channel_has_dtmf(channel)) {
				if (!args->input_callback && !args->buf) {
					status = SWITCH_STATUS_BREAK;
					done = 1;
					break;
				}
				switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
				if (args->input_callback) {
					status = args->input_callback(session, dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
				} else {
					switch_copy_string((char *)args->buf, dtmf, args->buflen);
					status = SWITCH_STATUS_BREAK;
				}
			}

			if (args->input_callback) {
				if (switch_core_session_dequeue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
					status = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);			
					switch_event_destroy(&event);
				}
			}
			
			if (status != SWITCH_STATUS_SUCCESS) {
				done = 1;
				break;
			}
		}
		
		if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
			memset(abuf, 0, framelen);
			olen = ilen;
            do_speed = 0;
		} else if (fh->audio_buffer && (switch_buffer_inuse(fh->audio_buffer) > (switch_size_t)(framelen))) {
			switch_buffer_read(fh->audio_buffer, abuf, framelen);
			olen = asis ? framelen : ilen;
			do_speed = 0;
		} else {
			olen = 32 * framelen;
			switch_core_file_read(fh, abuf, &olen);
			switch_buffer_write(fh->audio_buffer, abuf, asis ? olen : olen * 2);
			olen = switch_buffer_read(fh->audio_buffer, abuf, framelen);
			if (!asis) {
				olen /= 2;
			} 
		}

		if (done || olen <= 0) {
			break;
		}

		if (!asis) {
			if (fh->speed > 2) {
				fh->speed = 2;
			} else if (fh->speed < -2) {
				fh->speed = -2;
			}
		}
		
		if (!asis && fh->audio_buffer && last_speed > -1 && last_speed != fh->speed) {
			switch_buffer_zero(fh->audio_buffer);
		}

		
		if (!asis && fh->speed && do_speed) {
			float factor = 0.25f * abs(fh->speed);
			switch_size_t newlen, supplement, step;
			short *bp = write_frame.data;
			switch_size_t wrote = 0;
			
			
			supplement = (int) (factor * olen);
			newlen = (fh->speed > 0) ? olen - supplement : olen + supplement;
			step = (fh->speed > 0) ? (newlen / supplement) : (olen / supplement);
			
			while ((wrote + step) < newlen) {
				switch_buffer_write(fh->audio_buffer, bp, step * 2);
				wrote += step;
				bp += step;
				if (fh->speed > 0) {
					bp++;
				} else {
					float f;
					short s;
					f = (float)(*bp + *(bp+1) + *(bp-1));
					f /= 3;
					s = (short) f;
					switch_buffer_write(fh->audio_buffer, &s, 2);
					wrote++;
				}
			}
			if (wrote < newlen) {
				switch_size_t r = newlen - wrote;
				switch_buffer_write(fh->audio_buffer, bp, r*2);
				wrote += r;
			}
			last_speed = fh->speed;
			continue;
		}
		if (olen < llen) {
            uint8_t *dp = (uint8_t *) write_frame.data;
            memset(dp + (int)olen, 0, (int)(llen - olen));
            olen = llen;
        }

		write_frame.datalen = (uint32_t)(olen * (asis ? 1 : 2));
		write_frame.samples = (uint32_t)olen;

		llen = olen;


#ifndef WIN32
#if __BYTE_ORDER == __BIG_ENDIAN
		if (!asis) {switch_swap_linear(write_frame.data, (int) write_frame.datalen / 2);}
#endif
#endif
		stream_id = 0; 

        status = switch_core_session_write_frame(session, &write_frame, -1, stream_id);
        
        if (status == SWITCH_STATUS_MORE_DATA) {
            status = SWITCH_STATUS_SUCCESS;
            continue;
        } else if (status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Bad Write\n");
            done = 1;
            break;
        }

        if (done) {
            break;
        }
    
		
		if (timer_name) {
			if (switch_core_timer_next(&timer) < 0) {
				break;
			}
		} else { /* time off the channel (if you must) */
			switch_frame_t *read_frame;
			switch_status_t status; 
			while (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_HOLD)) {
				switch_yield(10000);
			}
			status = switch_core_session_read_frame(session, &read_frame, -1, 0);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "done playing file\n");
	switch_core_file_close(fh);
	switch_buffer_destroy(&fh->audio_buffer);
	if (!asis) {
		switch_core_codec_destroy(&codec);
	}
	if (timer_name) {
		/* End the audio absorbing thread */
		switch_core_thread_session_end(&thread_session);
		switch_core_timer_destroy(&timer);
	}

	switch_core_session_reset(session);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_regex_match(char *target, char *expression) {
	const char* error	= NULL;		//Used to hold any errors
	int error_offset	= 0;		//Holds the offset of an error
	pcre* pcre_prepared	= NULL;		//Holds the compiled regex
	int match_count		= 0;		//Number of times the regex was matched
	int offset_vectors[2];			//not used, but has to exist or pcre won't even try to find a match
	
	//Compile the expression
	pcre_prepared = pcre_compile(expression, 0, &error, &error_offset, NULL);

	//See if there was an error in the expression
	if (error != NULL) {
		//Clean up after ourselves
		if (pcre_prepared) {
			pcre_free(pcre_prepared);
			pcre_prepared = NULL;
		}	       

		//Note our error	
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Regular Expression Error expression[%s] error[%s] location[%d]\n", expression, error, error_offset);

		//We definitely didn't match anything
		return SWITCH_STATUS_FALSE;
	}

	//So far so good, run the regex
	match_count = pcre_exec(pcre_prepared, NULL, target, (int) strlen(target), 0, 0, offset_vectors, sizeof(offset_vectors) / sizeof(offset_vectors[0]));

	//Clean up
	if (pcre_prepared) {
		pcre_free(pcre_prepared);
		pcre_prepared = NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "number of matches: %d\n", match_count);

	//Was it a match made in heaven?
	if (match_count > 0) {
		return SWITCH_STATUS_SUCCESS;
	} else {
		return SWITCH_STATUS_FALSE;
	}
}

SWITCH_DECLARE(switch_status_t) switch_play_and_get_digits(switch_core_session_t *session,
														   uint32_t min_digits,
														   uint32_t max_digits,
														   uint32_t max_tries,
														   uint32_t timeout,
														   char* valid_terminators,
														   char* prompt_audio_file,
														   char* bad_input_audio_file,
														   void* digit_buffer,
														   uint32_t digit_buffer_length,
														   char* digits_regex) 
{

	char terminator;			//used to hold terminator recieved from  
	switch_channel_t *channel;	//the channel contained in session
	switch_status_t status;		//used to recieve state out of called functions

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "switch_play_and_get_digits(session, %d, %d, %d, %d, %s, %s, %s, digit_buffer, %d, %s)\n", min_digits, max_digits, max_tries, timeout, valid_terminators, prompt_audio_file, bad_input_audio_file, digit_buffer_length, digits_regex);

	//Get the channel
	channel = switch_core_session_get_channel(session);

	//Make sure somebody is home
	assert(channel != NULL);

	//Answer the channel if it hasn't already been answered
	switch_channel_answer(channel);

	//Start pestering the user for input
	for(;(switch_channel_get_state(channel) == CS_EXECUTE) && max_tries > 0; max_tries--) {
        switch_input_args_t args = {0};
		//make the buffer so fresh and so clean clean
		memset(digit_buffer, 0, digit_buffer_length);
        
        args.buf = digit_buffer;
        args.buflen = digit_buffer_length;
		//Play the file
		status = switch_ivr_play_file(session, NULL, prompt_audio_file, &args);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "play gave up %s", digit_buffer);

		//Make sure we made it out alive
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			break;
		}

		//we only get one digit out of playback, see if thats all we needed and what we got
		if (max_digits == 1 && status == SWITCH_STATUS_BREAK) {
			//Check the digit if we have a regex
			if (digits_regex != NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Checking regex [%s] on [%s]\n", digits_regex, digit_buffer);

				//Make sure the digit is allowed
				if (switch_regex_match(digit_buffer, digits_regex) == SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Match found!\n");
					//jobs done
					break;
				} else {
					//See if a bad input prompt was specified, if so, play it
					if (strlen(bad_input_audio_file) > 0) {
						status = switch_ivr_play_file(session, NULL, bad_input_audio_file, NULL);

						//Make sure we made it out alive
						if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
							switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
							break;
						}
					}
				}
			} else {
				//jobs done
				break;
			}
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Calling more digits try %d\n", max_tries);

		//Try to grab some more digits for the timeout period
		status = switch_ivr_collect_digits_count(session, digit_buffer, digit_buffer_length, max_digits, valid_terminators, &terminator, timeout);

		//Make sure we made it out alive
		if (status != SWITCH_STATUS_SUCCESS) {
			//Bail
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			break;
		}

		//see if we got enough
		if (min_digits <= strlen(digit_buffer)) {
			//See if we need to test a regex
			if (digits_regex != NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Checking regex [%s] on [%s]\n", digits_regex, digit_buffer);
				//Test the regex
				if (switch_regex_match(digit_buffer, digits_regex) == SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Match found!\n");
					//Jobs done
					return SWITCH_STATUS_SUCCESS;
				} else {
					//See if a bad input prompt was specified, if so, play it
					if (strlen(bad_input_audio_file) > 0) {
						status = switch_ivr_play_file(session, NULL, bad_input_audio_file, NULL);

						//Make sure we made it out alive
						if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
							switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
							break;
						}
					}
				}
			} else {
				//Jobs done
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	//if we got here, we got no digits or lost the channel
	digit_buffer = "\0";
	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_speak_text_handle(switch_core_session_t *session, 
															 switch_speech_handle_t *sh,
															 switch_codec_t *codec,
															 switch_timer_t *timer,
															 char *text,
                                                             switch_input_args_t *args)
{
	switch_channel_t *channel;
	short abuf[960];
	char dtmf[128];
	uint32_t len = 0;
	switch_size_t ilen = 0;
	switch_frame_t write_frame = {0};
	int x;
	int stream_id = 0;
	int done = 0;
	int lead_in_out = 10;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	uint32_t rate = 0, samples = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (!sh) {
		return SWITCH_STATUS_FALSE;
	}

	switch_channel_answer(channel);

	write_frame.data = abuf;
	write_frame.buflen = sizeof(abuf);

	samples = (uint32_t)(sh->rate / 50);
	len = samples * 2;

	flags = 0;
	switch_sleep(200000);
	switch_core_speech_feed_tts(sh, text, &flags);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Speaking text: %s\n", text);

	write_frame.rate = sh->rate;

	memset(write_frame.data, 0, len);
	write_frame.datalen = len;
	write_frame.samples = len / 2;
	write_frame.codec = codec;

	for( x = 0; !done && x < lead_in_out; x++) {
        if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Bad Write\n");
            done = 1;
            break;
        }		
	}

	ilen = len;
	while(switch_channel_ready(channel)) {
		switch_event_t *event;

		if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
			switch_ivr_parse_event(session, event);
			switch_event_destroy(&event);
		}

		if (args && (args->input_callback || args->buf || args->buflen)) {
			/*
			dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			*/
			if (switch_channel_has_dtmf(channel)) {
				if (!args->input_callback && !args->buf) {
					status = SWITCH_STATUS_BREAK;
					done = 1;
					break;
				}
				if (args->buf && !strcasecmp(args->buf, "_break_")) {
					status = SWITCH_STATUS_BREAK;
				} else {
					switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
					if (args->input_callback) {
						status = args->input_callback(session, dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
					} else {
						switch_copy_string((char *)args->buf, dtmf, args->buflen);
						status = SWITCH_STATUS_BREAK;
					}
				}
			}

			if (args->input_callback) {
				if (switch_core_session_dequeue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
					status = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);			
					switch_event_destroy(&event);
				}
			}

			if (status != SWITCH_STATUS_SUCCESS) {
				done = 1;
				break;
			}
		}

		if (switch_test_flag(sh, SWITCH_SPEECH_FLAG_PAUSE)) {
			if (timer) {
				if ((x = switch_core_timer_next(timer)) < 0) {
					break;
				}
			} else {
				switch_frame_t *read_frame;
				switch_status_t status = switch_core_session_read_frame(session, &read_frame, -1, 0);

				while (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_HOLD)) {
					switch_yield(10000);
				}

				if (!SWITCH_READ_ACCEPTABLE(status)) {
					break;
				}
			}
			continue;
		}

		flags = SWITCH_SPEECH_FLAG_BLOCKING;
		status = switch_core_speech_read_tts(sh,
			abuf,
			&ilen,
			&rate,
			&flags);

		if (status != SWITCH_STATUS_SUCCESS) {
			for( x = 0; !done && x < lead_in_out; x++) {
                if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Bad Write\n");
                    done = 1;
                    break;
                }
			}
			if (status == SWITCH_STATUS_BREAK) {
				status = SWITCH_STATUS_SUCCESS;
			}
			done = 1;
		}

		if (done) {
			break;
		}

		write_frame.datalen = (uint32_t)ilen;
		write_frame.samples = (uint32_t)(ilen / 2);

        if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Bad Write\n");
            done = 1;
            break;
        }

        if (done) {
            break;
        }

		if (timer) {
			if ((x = switch_core_timer_next(timer)) < 0) {
				break;
			}
		} else { /* time off the channel (if you must) */
			switch_frame_t *read_frame;
			switch_status_t status = switch_core_session_read_frame(session, &read_frame, -1, 0);

			while (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_HOLD)) {
				switch_yield(10000);
			}

			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}
		}

	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "done speaking text\n");
	flags = 0;	
	switch_core_speech_flush_tts(sh);
	return status;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_speak_text(switch_core_session_t *session, 
													  char *tts_name,
													  char *voice_name,
													  uint32_t rate,
													  char *text,
                                                      switch_input_args_t *args)
{
	switch_channel_t *channel;
	int interval = 0;
	uint32_t samples = 0;
	uint32_t len = 0;
	switch_frame_t write_frame = {0};
	switch_timer_t timer;
	switch_core_thread_session_t thread_session;
	switch_codec_t codec;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	char *codec_name;
	int stream_id = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_speech_handle_t sh;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	switch_codec_t *read_codec;
    char *timer_name;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

    timer_name = switch_channel_get_variable(channel, "timer_name");

	if (rate == 0) {
		read_codec = switch_core_session_get_read_codec(session);
		rate = read_codec->implementation->samples_per_second;
	}

	memset(&sh, 0, sizeof(sh));
	if (switch_core_speech_open(&sh,
								tts_name,
								voice_name,
								(uint32_t)rate,
								&flags,
								switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid TTS module!\n");
		switch_core_session_reset(session);
		return SWITCH_STATUS_FALSE;
	}
		
	switch_channel_answer(channel);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "OPEN TTS %s\n", tts_name);
	
	interval = 20;
	samples = (uint32_t)(rate / 50);
	len = samples * 2;

	codec_name = "L16";

	if (switch_core_codec_init(&codec,
							   codec_name,
							   NULL,
							   (int)rate,
							   interval,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activated\n");
		write_frame.codec = &codec;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed %s@%uhz 1 channel %dms\n",
							  codec_name, rate, interval);
		flags = 0;
		switch_core_speech_close(&sh, &flags);
		switch_core_session_reset(session);
		return SWITCH_STATUS_GENERR;
	}

	if (timer_name) {
		if (switch_core_timer_init(&timer, timer_name, interval, (int)samples, pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
			switch_core_codec_destroy(&codec);
			flags = 0;
			switch_core_speech_close(&sh, &flags);

			switch_core_session_reset(session);
			return SWITCH_STATUS_GENERR;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "setup timer success %u bytes per %d ms!\n", len, interval);

		/* start a thread to absorb incoming audio */
		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			switch_core_service_session(session, &thread_session, stream_id);
		}
	}

	switch_ivr_speak_text_handle(session, &sh, &codec, timer_name ? &timer : NULL, text, args);
	flags = 0;	
	switch_core_speech_close(&sh, &flags);
	switch_core_codec_destroy(&codec);

	if (timer_name) {
		/* End the audio absorbing thread */
		switch_core_thread_session_end(&thread_session);
		switch_core_timer_destroy(&timer);
	}

	switch_core_session_reset(session);
	return status;
}


/* Bridge Related Stuff*/
/*********************************************************************************/
struct audio_bridge_data {
	switch_core_session_t *session_a;
	switch_core_session_t *session_b;
	int running;
};

static void *audio_bridge_thread(switch_thread_t *thread, void *obj)
{
	switch_core_thread_session_t *his_thread, *data = obj;
	int *stream_id_p;
	int stream_id = 0, pre_b = 0, ans_a = 0, ans_b = 0, originator = 0;
	switch_input_callback_function_t input_callback;
	switch_core_session_message_t *message, msg = {0};
	void *user_data;

	switch_channel_t *chan_a, *chan_b;
	switch_frame_t *read_frame;
	switch_core_session_t *session_a, *session_b;

	assert(!thread || thread);

	session_a = data->objs[0];
	session_b = data->objs[1];

	stream_id_p = data->objs[2];
	input_callback = data->input_callback;
	user_data = data->objs[4];
	his_thread = data->objs[5];

	if (stream_id_p) {
		stream_id = *stream_id_p;
	}

	chan_a = switch_core_session_get_channel(session_a);
	chan_b = switch_core_session_get_channel(session_b);

	ans_a = switch_channel_test_flag(chan_a, CF_ANSWERED);
	if ((originator = switch_channel_test_flag(chan_a, CF_ORIGINATOR))) {
		pre_b = switch_channel_test_flag(chan_a, CF_EARLY_MEDIA);
		ans_b = switch_channel_test_flag(chan_b, CF_ANSWERED);
	}


	switch_channel_set_flag(chan_a, CF_BRIDGED);

	while (switch_channel_ready(chan_a) && data->running > 0 && his_thread->running > 0) {
		switch_channel_state_t b_state = switch_channel_get_state(chan_b);
		switch_status_t status;
		switch_event_t *event;

		switch (b_state) {
		case CS_HANGUP:
		case CS_DONE:
			switch_mutex_lock(data->mutex);
			data->running = -1;
			switch_mutex_unlock(data->mutex);
			continue;
		default:
			break;
		}

		if (switch_channel_test_flag(chan_a, CF_TRANSFER)) {
			switch_channel_clear_flag(chan_a, CF_HOLD);
			switch_channel_clear_flag(chan_a, CF_SUSPEND);
			break;
		}
		
		if (switch_core_session_dequeue_private_event(session_a, &event) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_flag(chan_b, CF_SUSPEND);
			switch_ivr_parse_event(session_a, event);
			switch_channel_clear_flag(chan_b, CF_SUSPEND);
			switch_event_destroy(&event);
		}

		/* if 1 channel has DTMF pass it to the other */
		if (switch_channel_has_dtmf(chan_a)) {
			char dtmf[128];
			switch_channel_dequeue_dtmf(chan_a, dtmf, sizeof(dtmf));
			switch_core_session_send_dtmf(session_b, dtmf);
			
			if (input_callback) {
				if (input_callback(session_a, dtmf, SWITCH_INPUT_TYPE_DTMF, user_data, 0) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s ended call via DTMF\n", switch_channel_get_name(chan_a));
					switch_mutex_lock(data->mutex);
					data->running = -1;
					switch_mutex_unlock(data->mutex);
					break;
				}
			}
		}

		if (switch_core_session_dequeue_event(session_a, &event) == SWITCH_STATUS_SUCCESS) {
			if (input_callback) {
				status = input_callback(session_a, event, SWITCH_INPUT_TYPE_EVENT, user_data, 0);
			}

			if (event->event_id != SWITCH_EVENT_MESSAGE || switch_core_session_receive_event(session_b, &event) != SWITCH_STATUS_SUCCESS) {
				switch_event_destroy(&event);
			}

		}

		if (switch_core_session_dequeue_message(session_b, &message) == SWITCH_STATUS_SUCCESS) {
			switch_core_session_receive_message(session_a, message);
			if (switch_test_flag(message, SCSMF_DYNAMIC)) {
				switch_safe_free(message);
			} else {
				message = NULL;
			}
		}

		if (!ans_a && originator) {

			if (!ans_b && switch_channel_test_flag(chan_b, CF_ANSWERED)) {
				switch_channel_answer(chan_a);
				ans_a++;
			} else if (!pre_b && switch_channel_test_flag(chan_b, CF_EARLY_MEDIA)) {
				if (switch_channel_pre_answer(chan_a) == SWITCH_STATUS_SUCCESS) {
					pre_b++;
				}
			}
			if (!pre_b) {
				switch_yield(10000);
				continue;
			}
		}


		if (switch_channel_test_flag(chan_a, CF_SUSPEND) || switch_channel_test_flag(chan_b, CF_SUSPEND)) {
			switch_yield(10000);
			continue;
		}

		/* read audio from 1 channel and write it to the other */
		status = switch_core_session_read_frame(session_a, &read_frame, -1, stream_id);

		if (SWITCH_READ_ACCEPTABLE(status)) {
			if (status != SWITCH_STATUS_BREAK && !switch_channel_test_flag(chan_a, CF_HOLD)) {
				if (switch_core_session_write_frame(session_b, read_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "write: %s Bad Frame....[%u] Bubye!\n",
									  switch_channel_get_name(chan_b), read_frame->datalen);
					switch_mutex_lock(data->mutex);
					data->running = -1;
					switch_mutex_unlock(data->mutex);
				}
			} 
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "read: %s Bad Frame.... Bubye!\n", switch_channel_get_name(chan_a));
			switch_mutex_lock(data->mutex);
			data->running = -1;
			switch_mutex_unlock(data->mutex);
		}
	}

    switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
	
	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	msg.from = __FILE__;
	switch_core_session_receive_message(session_a, &msg);

	switch_channel_set_variable(chan_a, SWITCH_BRIDGE_VARIABLE, NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BRIDGE THREAD DONE [%s]\n", switch_channel_get_name(chan_a));

	switch_channel_clear_flag(chan_a, CF_BRIDGED);
	switch_mutex_lock(data->mutex);
	data->running = 0;
	switch_mutex_unlock(data->mutex);
	return NULL;
}

static switch_status_t audio_bridge_on_loopback(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	void *arg;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if ((arg = switch_channel_get_private(channel, "_bridge_"))) {
		switch_channel_set_private(channel, "_bridge_", NULL);
		audio_bridge_thread(NULL, (void *) arg);
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
	switch_channel_clear_state_handler(channel, &audio_bridge_peer_state_handlers);

	if (!switch_channel_test_flag(channel, CF_TRANSFER)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	}
	
	return SWITCH_STATUS_FALSE;
}


static switch_status_t audio_bridge_on_ring(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CUSTOM RING\n");

	/* put the channel in a passive state so we can loop audio to it */
	switch_channel_set_state(channel, CS_HOLD);
	return SWITCH_STATUS_FALSE;
}

static switch_status_t audio_bridge_on_hold(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CUSTOM HOLD\n");

	/* put the channel in a passive state so we can loop audio to it */
	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t audio_bridge_peer_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ audio_bridge_on_ring,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_loopback */ audio_bridge_on_loopback,
	/*.on_transmit */ NULL,
	/*.on_hold */ audio_bridge_on_hold,
};


static switch_status_t uuid_bridge_on_transmit(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	switch_core_session_t *other_session;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CUSTOM TRANSMIT\n");
	switch_channel_clear_state_handler(channel, NULL);


	if (!switch_channel_test_flag(channel, CF_ORIGINATOR)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((other_session = switch_channel_get_private(channel, SWITCH_UUID_BRIDGE))) {
		switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
		switch_channel_state_t state = switch_channel_get_state(other_channel);
		switch_channel_state_t mystate = switch_channel_get_state(channel);
		switch_event_t *event;
		uint8_t ready_a, ready_b;
		switch_caller_profile_t *profile, *new_profile;


		switch_channel_clear_flag(channel, CF_TRANSFER);
		switch_channel_set_private(channel, SWITCH_UUID_BRIDGE, NULL);

		while (mystate <= CS_HANGUP && state <= CS_HANGUP && !switch_channel_test_flag(other_channel, CF_TAGGED)) {
			switch_yield(1000);
			state = switch_channel_get_state(other_channel);
			mystate = switch_channel_get_state(channel);
		}

		switch_channel_clear_flag(other_channel, CF_TRANSFER);
		switch_channel_clear_flag(other_channel, CF_TAGGED);
		

		switch_core_session_reset(session);
		switch_core_session_reset(other_session);
		
		ready_a = switch_channel_ready(channel);
		ready_b = switch_channel_ready(other_channel);

		if (!ready_a || !ready_b) {
			if (!ready_a) {
				switch_channel_hangup(other_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}

			if (!ready_b) {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
			return SWITCH_STATUS_FALSE;
		}
		
		/* add another profile to both sessions for CDR's sake */
		if ((profile = switch_channel_get_caller_profile(channel))) {
			new_profile = switch_caller_profile_clone(session, profile);
			new_profile->destination_number = switch_core_session_strdup(session, switch_core_session_get_uuid(other_session));
			switch_channel_set_caller_profile(channel, new_profile);
		} 

		if ((profile = switch_channel_get_caller_profile(other_channel))) {
			new_profile = switch_caller_profile_clone(other_session, profile);
			new_profile->destination_number = switch_core_session_strdup(other_session, switch_core_session_get_uuid(session));
			switch_channel_set_caller_profile(other_channel, new_profile);
		} 

		/* fire events that will change the data table from "show channels" */
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "uuid_bridge");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s", switch_core_session_get_uuid(other_session));
			switch_event_fire(&event);
		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(other_channel, event);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "uuid_bridge");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s", switch_core_session_get_uuid(session));
			switch_event_fire(&event);
		}

		switch_ivr_multi_threaded_bridge(session, other_session, NULL, NULL, NULL);
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}



	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t uuid_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_loopback */ NULL,
	/*.on_transmit */ uuid_bridge_on_transmit,
	/*.on_hold */ NULL
};

struct key_collect {
	char *key;
	char *file;
	switch_core_session_t *session;
};


static void *SWITCH_THREAD_FUNC collect_thread_run(switch_thread_t *thread, void *obj)
{
	struct key_collect *collect = (struct key_collect *) obj;
	switch_channel_t *channel = switch_core_session_get_channel(collect->session);
	char buf[10] = "";
	char *p, term;


	if (!strcasecmp(collect->key, "exec")) {
		char *data;
		const switch_application_interface_t *application_interface;
		char *app_name, *app_data;

		if (!(data = collect->file)) {
			goto wbreak;
		}

		app_name = data;

		if ((app_data = strchr(app_name, ' '))) {
			*app_data++ = '\0';
		}
		
		if ((application_interface = switch_loadable_module_get_application_interface(app_name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Application %s\n", app_name);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto wbreak;
		}

		if (!application_interface->application_function) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Function for %s\n", app_name);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto wbreak;
		}

		application_interface->application_function(collect->session, app_data);
		if (switch_channel_get_state(channel) < CS_HANGUP) {
			switch_channel_set_flag(channel, CF_WINNER);
		}
		goto wbreak;
	}

	if (!switch_channel_ready(channel)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto wbreak;
	}

	while(switch_channel_ready(channel)) {
		memset(buf, 0, sizeof(buf));

		if (collect->file) {
            switch_input_args_t args = {0};
            args.buf = buf;
            args.buflen = sizeof(buf);
			switch_ivr_play_file(collect->session, NULL, collect->file, &args);
		} else {
			switch_ivr_collect_digits_count(collect->session, buf, sizeof(buf), 1, "", &term, 0);
		}

		for(p = buf; *p; p++) {
			if (*collect->key == *p) {
				switch_channel_set_flag(channel, CF_WINNER);
				goto wbreak;
			}
		}
	}
 wbreak:

	return NULL;
}

static void launch_collect_thread(struct key_collect *collect)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(collect->session));
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, collect_thread_run, collect, switch_core_session_get_pool(collect->session));
}

static uint8_t check_channel_status(switch_channel_t **peer_channels,
									switch_core_session_t **peer_sessions,
									uint32_t len,
									int32_t *idx,
									char *file,
									char *key,
									uint8_t early_ok)
{

	uint32_t i;
	uint32_t hups = 0;	
	*idx = -1;
	
	for (i = 0; i < len; i++) {
		if (!peer_channels[i]) {
			continue;
		}
		if (switch_channel_get_state(peer_channels[i]) >= CS_HANGUP) {
			hups++;
		} else if ((switch_channel_test_flag(peer_channels[i], CF_ANSWERED) || 
                    (early_ok && len == 1 && switch_channel_test_flag(peer_channels[i], CF_EARLY_MEDIA))) && 
				   !switch_channel_test_flag(peer_channels[i], CF_TAGGED)) {

			if (key) {
				struct key_collect *collect;
				
				if ((collect = switch_core_session_alloc(peer_sessions[i], sizeof(*collect)))) {
					switch_channel_set_flag(peer_channels[i], CF_TAGGED);
					collect->key = key;
					if (file) {
						collect->file = switch_core_session_strdup(peer_sessions[i], file);
					}
				
					collect->session = peer_sessions[i];
					launch_collect_thread(collect);
				}
			} else {
				*idx = i;
				return 0;
					
			}
		} else if (switch_channel_test_flag(peer_channels[i], CF_WINNER)) {
			*idx = i;
			return 0;
		}
	}

	if (hups == len) {
		return 0;
	} else {
		return 1;
	}
	
}

struct ringback {
	switch_buffer_t *audio_buffer;
    switch_buffer_t *loop_buffer;
	teletone_generation_session_t ts;	
	switch_file_handle_t fhb;
	switch_file_handle_t *fh;
	uint8_t asis;
};

typedef struct ringback ringback_t;

static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	ringback_t *tto = ts->user_data;
	int wrote;

	if (!tto) {
		return -1;
	}
	wrote = teletone_mux_tones(ts, map);
	switch_buffer_write(tto->audio_buffer, ts->buffer, wrote * 2);

	return 0;
}

#define MAX_PEERS 256
SWITCH_DECLARE(switch_status_t) switch_ivr_originate(switch_core_session_t *session,
													 switch_core_session_t **bleg,
													 switch_call_cause_t *cause,
													 char *bridgeto,
													 uint32_t timelimit_sec,
													 const switch_state_handler_table_t *table,
													 char *cid_name_override,
													 char *cid_num_override,
													 switch_caller_profile_t *caller_profile_override
													 )
										  
{
	char *pipe_names[MAX_PEERS] = {0};
	char *data = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *caller_channel = NULL;
	char *peer_names[MAX_PEERS] = {0};
	switch_core_session_t *peer_session, *peer_sessions[MAX_PEERS] = {0};
	switch_caller_profile_t *caller_profiles[MAX_PEERS] = {0}, *caller_caller_profile;
	char *chan_type = NULL, *chan_data;
	switch_channel_t *peer_channel = NULL, *peer_channels[MAX_PEERS] = {0};
	ringback_t ringback = {0};
	time_t start;
	switch_frame_t *read_frame = NULL;
	switch_memory_pool_t *pool = NULL;
	int r = 0, i, and_argc = 0, or_argc = 0;
	int32_t idx = IDX_NADA;
	switch_codec_t write_codec = {0};
	switch_frame_t write_frame = {0};
	uint8_t fdata[1024], pass = 0;
	char *file = NULL, *key = NULL, *odata, *var;
	switch_call_cause_t reason = SWITCH_CAUSE_UNALLOCATED;
	uint8_t to = 0;
	char *var_val, *vars = NULL, *ringback_data = NULL;
	switch_codec_t *read_codec = NULL;
	uint8_t sent_ring = 0, early_ok = 1;
	switch_core_session_message_t *message = NULL;
    switch_event_t *var_event = NULL;

	write_frame.data = fdata;
	
	*bleg = NULL;
	odata = strdup(bridgeto);
	data = odata;

    /* strip leading spaces */
    while (data && *data && *data == ' ') {
        data++;
    }

    if (*data == '{') {
        vars = data + 1;
        if (!(data = strchr(data, '}'))) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
            status = SWITCH_STATUS_GENERR;
            goto done;
        }
        *data++ = '\0';
    }

    /* strip leading spaces (again)*/
    while (data && *data && *data == ' ') {
        data++;
    }
    
    if (switch_strlen_zero(data)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
        status = SWITCH_STATUS_GENERR;
        goto done;
    }
    
    /* Some channel are created from an originating channel and some aren't so not all outgoing calls have a way to get params
       so we will normalize dialstring params and channel variables (when there is an originator) into an event that we 
       will use as a pseudo hash to consult for params as needed.
     */
    if (switch_event_create(&var_event, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
        status = SWITCH_STATUS_MEMERR;
        goto done;
    }
    
	if (session) {
        switch_hash_index_t *hi;
        void *vval;
        const void *vvar;
        
        caller_channel = switch_core_session_get_channel(session);
		assert(caller_channel != NULL);

        /* Copy all the channel variables into the event */
        for (hi = switch_channel_variable_first(caller_channel, switch_core_session_get_pool(session)); hi; hi = switch_hash_next(hi)) {
            switch_hash_this(hi, &vvar, NULL, &vval);
            if (vvar && vval) {
                switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, (void *)vvar, vval);
            }
        }

    }

    if (vars) { /* Parse parameters specified from the dialstring */
        char *var_array[1024] = {0};
        int var_count = 0;
        if ((var_count = switch_separate_string(vars, ',', var_array, (sizeof(var_array) / sizeof(var_array[0]))))) {
            int x = 0;
            for (x = 0; x < var_count; x++) {
                char *inner_var_array[2];
                int inner_var_count;
                if ((inner_var_count = 
                     switch_separate_string(var_array[x], '=', inner_var_array, (sizeof(inner_var_array) / sizeof(inner_var_array[0])))) == 2) {
                    
                    switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, inner_var_array[0], inner_var_array[1]);
                    if (caller_channel) {
                        switch_channel_set_variable(caller_channel, inner_var_array[0], inner_var_array[1]);
                    }
                }
            }
        }
    }


    if (caller_channel) { /* ringback is only useful when there is an originator */
		ringback_data = switch_channel_get_variable(caller_channel, "ringback");
		switch_channel_set_variable(caller_channel, "originate_disposition", "failure");
	}
	
    if ((var = switch_event_get_header(var_event, "group_confirm_key"))) {
        key = switch_core_session_strdup(session, var);
        if ((var = switch_event_get_header(var_event, "group_confirm_file"))) {
            file = switch_core_session_strdup(session, var);
        }
    }

	if (file && !strcmp(file, "undef")) {
		file = NULL;
	}

    if ((var_val = switch_event_get_header(var_event, "noanswer_early_media")) && switch_true(var_val)) {
        early_ok = 0;
    }

    if (!cid_name_override) {
        cid_name_override = switch_event_get_header(var_event, "origination_caller_id_name");
    }

    if (!cid_num_override) {
        cid_num_override = switch_event_get_header(var_event, "origination_caller_id_number");
    }
                                                        

	or_argc = switch_separate_string(data, '|', pipe_names, (sizeof(pipe_names) / sizeof(pipe_names[0])));

	if (caller_channel && or_argc > 1 && !ringback_data) {
		switch_channel_ringback(caller_channel);
		sent_ring = 1;
	}

	for (r = 0; r < or_argc; r++) {
        reason = SWITCH_CAUSE_UNALLOCATED;
		memset(peer_names, 0, sizeof(peer_names));
		peer_session = NULL;
		memset(peer_sessions, 0, sizeof(peer_sessions));
		memset(peer_channels, 0, sizeof(peer_channels));
		memset(caller_profiles, 0, sizeof(caller_profiles));
		chan_type = NULL;
		chan_data = NULL;
		peer_channel = NULL;
		start = 0;
		read_frame = NULL;
		pool = NULL;
		pass = 0;
		file = NULL;
		key = NULL;
		var = NULL;
		to = 0;

		and_argc = switch_separate_string(pipe_names[r], ',', peer_names, (sizeof(peer_names) / sizeof(peer_names[0])));
	
		if (caller_channel && !sent_ring && and_argc > 1 && !ringback_data) {
			switch_channel_ringback(caller_channel);
			sent_ring = 1;
		}

		for (i = 0; i < and_argc; i++) {
		
			chan_type = peer_names[i];
			if ((chan_data = strchr(chan_type, '/')) != 0) {
				*chan_data = '\0';
				chan_data++;
			}
	
			if (session) {
				if (!switch_channel_ready(caller_channel)) {
					status = SWITCH_STATUS_FALSE;
					goto done;
				}

				caller_caller_profile = caller_profile_override ? caller_profile_override : switch_channel_get_caller_profile(caller_channel);
                
				if (!cid_name_override) {
					cid_name_override = caller_caller_profile->caller_id_name;
				}
				if (!cid_num_override) {
					cid_num_override = caller_caller_profile->caller_id_number;
				}

				caller_profiles[i] = switch_caller_profile_new(switch_core_session_get_pool(session),
															   caller_caller_profile->username,
															   caller_caller_profile->dialplan,
															   cid_name_override,
															   cid_num_override,
															   caller_caller_profile->network_addr,
															   NULL,
															   NULL,
															   caller_caller_profile->rdnis,
															   caller_caller_profile->source,
															   caller_caller_profile->context,
															   chan_data);
				caller_profiles[i]->flags = caller_caller_profile->flags;
				pool = NULL;
			} else {
				if (!cid_name_override) {
					cid_name_override = "FreeSWITCH";
				}
				if (!cid_num_override) {
					cid_num_override = "0000000000";
				}

				if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
					status = SWITCH_STATUS_TERM;
					goto done;
				}

				if (caller_profile_override) {
					caller_profiles[i] = switch_caller_profile_new(pool,
																   caller_profile_override->username,
																   caller_profile_override->dialplan,
																   caller_profile_override->caller_id_name,
																   caller_profile_override->caller_id_number,
																   caller_profile_override->network_addr, 
																   caller_profile_override->ani,
																   caller_profile_override->aniii,
																   caller_profile_override->rdnis,
																   caller_profile_override->source,
																   caller_profile_override->context,
																   chan_data);
				} else {
					caller_profiles[i] = switch_caller_profile_new(pool,
																   NULL,
																   NULL,
																   cid_name_override,
																   cid_num_override,
																   NULL,
																   NULL, 
																   NULL,
																   NULL,
																   __FILE__,
																   NULL,
																   chan_data);
				}
			}

			if ((reason = switch_core_session_outgoing_channel(session, chan_type, caller_profiles[i], &peer_sessions[i], pool)) != SWITCH_CAUSE_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create Outgoing Channel! cause: %s\n", switch_channel_cause2str(reason));
				if (pool) {
					switch_core_destroy_memory_pool(&pool);
				}
				caller_profiles[i] = NULL;
				peer_channels[i] = NULL;
				peer_sessions[i] = NULL;
				continue;
			}
			
			switch_core_session_read_lock(peer_sessions[i]);
			pool = NULL;
	
			peer_channels[i] = switch_core_session_get_channel(peer_sessions[i]);
			assert(peer_channels[i] != NULL);

			//switch_channel_set_flag(peer_channels[i], CF_NO_INDICATE);

			if (table == &noop_state_handler) {
				table = NULL;
			} else if (!table) {
				table = &audio_bridge_peer_state_handlers;
			}

			if (table) {
				switch_channel_add_state_handler(peer_channels[i], table);
			}

			if (switch_core_session_running(peer_sessions[i])) {
				switch_channel_set_state(peer_channels[i], CS_RING);
			} else {
				switch_core_session_thread_launch(peer_sessions[i]);
			}
		}

		time(&start);

		for (;;) {
			uint32_t valid_channels = 0;
			for (i = 0; i < and_argc; i++) {
				int state;

				if (!peer_channels[i]) {
					continue;
				}
				valid_channels++;
				state = switch_channel_get_state(peer_channels[i]);
			
				if (state >= CS_RING) {
					goto endfor1;
				}
		
				if (caller_channel && !switch_channel_ready(caller_channel)) {
					goto notready;
				}
		
				if ((time(NULL) - start) > (time_t)timelimit_sec) {
					to++;
					idx = IDX_CANCEL;
					goto notready;
				}
				switch_yield(1000);
			}

			if (valid_channels == 0) {
				status = SWITCH_STATUS_GENERR;
				goto done;
			}

		}
	endfor1:

		if (ringback_data && !switch_channel_test_flag(caller_channel, CF_ANSWERED) && !switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA)) {
			switch_channel_pre_answer(caller_channel);
		}

		if (session && (ringback_data || !switch_channel_test_flag(caller_channel, CF_NOMEDIA))) {
			read_codec = switch_core_session_get_read_codec(session);
			assert(read_codec != NULL);

			if (!(pass = (uint8_t)switch_test_flag(read_codec, SWITCH_CODEC_FLAG_PASSTHROUGH))) {
				if (switch_core_codec_init(&write_codec,
										   "L16",
										   NULL,
										   read_codec->implementation->samples_per_second,
										   read_codec->implementation->microseconds_per_frame / 1000,
										   1,
										   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
										   NULL,
										   pool) == SWITCH_STATUS_SUCCESS) {
					
					
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Success L16@%uhz 1 channel %dms\n",
									  read_codec->implementation->samples_per_second,
									  read_codec->implementation->microseconds_per_frame / 1000);
					write_frame.codec = &write_codec;
					write_frame.datalen = read_codec->implementation->bytes_per_frame;
					write_frame.samples = write_frame.datalen / 2;
					memset(write_frame.data, 255, write_frame.datalen);

					if (ringback_data) {
						char *tmp_data = NULL;
						
						switch_buffer_create_dynamic(&ringback.audio_buffer, 512, 1024, 0);
						switch_buffer_create_dynamic(&ringback.loop_buffer, 512, 1024, 0);

						if (*ringback_data == '/') {
							char *ext;
							
							if ((ext = strrchr(ringback_data, '.'))) {
								switch_core_session_set_read_codec(session, &write_codec);
								ext++;
							} else {
								ringback.asis++;
								write_frame.codec = read_codec;
								ext = read_codec->implementation->iananame;
								tmp_data = switch_mprintf("%s.%s", ringback_data, ext);
								ringback_data = tmp_data;
							}

							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play Ringback File [%s]\n", ringback_data);

							ringback.fhb.channels = read_codec->implementation->number_of_channels;
							ringback.fhb.samplerate = read_codec->implementation->samples_per_second;
							if (switch_core_file_open(&ringback.fhb,
													  ringback_data,
													  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
													  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Playing File\n");
								switch_safe_free(tmp_data);
								goto notready;
							}
							ringback.fh = &ringback.fhb;

							
						} else {
							teletone_init_session(&ringback.ts, 0, teletone_handler, &ringback);
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play Ringback Tone [%s]\n", ringback_data);
							//ringback.ts.debug = 1;
							//ringback.ts.debug_stream = switch_core_get_console();
							if (teletone_run(&ringback.ts, ringback_data)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Playing Tone\n");
								teletone_destroy_session(&ringback.ts);
								switch_buffer_destroy(&ringback.audio_buffer);
								switch_buffer_destroy(&ringback.loop_buffer);
								ringback_data = NULL;
							}
						}
						switch_safe_free(tmp_data);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec Error!");
					switch_channel_hangup(caller_channel, SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE);
					read_codec = NULL;
				}
			}
		}
        
        if (ringback_data) {
            early_ok = 0;
        }

        while ((!caller_channel || switch_channel_ready(caller_channel)) && 
			   check_channel_status(peer_channels, peer_sessions, and_argc, &idx, file, key, early_ok)) {

			if ((to = (uint8_t)((time(NULL) - start) >= (time_t)timelimit_sec))) {
				idx = IDX_CANCEL;
				goto notready;
			}

			if (peer_sessions[0] && switch_core_session_dequeue_message(peer_sessions[0], &message) == SWITCH_STATUS_SUCCESS) {
				if (session && !ringback_data && or_argc == 1 && and_argc == 1) { /* when there is only 1 channel to call and bridge and no ringback */
					switch_core_session_receive_message(session, message);
				}
				
				if (switch_test_flag(message, SCSMF_DYNAMIC)) {
					switch_safe_free(message);
				} else {
					message = NULL;
				}
			}

			/* read from the channel while we wait if the audio is up on it */
			if (session && (ringback_data || !switch_channel_test_flag(caller_channel, CF_NOMEDIA)) && 
				(switch_channel_test_flag(caller_channel, CF_ANSWERED) || switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA))) {
				switch_status_t status = switch_core_session_read_frame(session, &read_frame, 1000, 0);
			
				if (!SWITCH_READ_ACCEPTABLE(status)) {
					break;
				}

				if (read_frame && !pass && !switch_test_flag(read_frame, SFF_CNG) && read_frame->datalen > 1) {
					if (ringback.fh) {
						uint8_t abuf[1024];
						switch_size_t mlen, olen;
						unsigned int pos = 0;

						if (ringback.asis) {
							mlen = read_frame->datalen;
						} else {
							mlen = read_frame->datalen  / 2;
						}

						olen = mlen;
						switch_core_file_read(ringback.fh, abuf, &olen);
						
						if (olen == 0) {
							olen = mlen;
							ringback.fh->speed = 0;
							switch_core_file_seek(ringback.fh, &pos, 0, SEEK_SET);
							switch_core_file_read(ringback.fh, abuf, &olen);
							if (olen == 0) {
								break;
							}
						}
						write_frame.data = abuf;
						write_frame.datalen = (uint32_t) (ringback.asis ? olen : olen * 2);
						if (switch_core_session_write_frame(session, &write_frame, 1000, 0) != SWITCH_STATUS_SUCCESS) {
							break;
						}
					} else if (ringback.audio_buffer) {
						if ((write_frame.datalen = (uint32_t)switch_buffer_read(ringback.audio_buffer,
																				write_frame.data,
																				write_frame.codec->implementation->bytes_per_frame)) <= 0) {
							switch_buffer_t *tmp;
							tmp = ringback.audio_buffer;
							ringback.audio_buffer = ringback.loop_buffer;
							ringback.loop_buffer = tmp;
							if ((write_frame.datalen = (uint32_t)switch_buffer_read(ringback.audio_buffer,
																					write_frame.data,
																					write_frame.codec->implementation->bytes_per_frame)) <= 0) {
								break;
							}
						}
					}

					if (switch_core_session_write_frame(session, &write_frame, 1000, 0) != SWITCH_STATUS_SUCCESS) {
						break;
					}
					if (ringback.loop_buffer) {
						switch_buffer_write(ringback.loop_buffer, write_frame.data, write_frame.datalen);
					}
				}

			} else {
				switch_yield(1000);
			}
		
		}

	notready:

		if (caller_channel && !switch_channel_ready(caller_channel)) {
			idx = IDX_CANCEL;
		}

		if (session && (ringback_data || !switch_channel_test_flag(caller_channel, CF_NOMEDIA))) {
			switch_core_session_reset(session);
		}

		for (i = 0; i < and_argc; i++) {
			if (!peer_channels[i]) {
				continue;
			}
			if (i != idx) {
				if (idx == IDX_CANCEL) {
					if (to) {
						reason = SWITCH_CAUSE_NO_ANSWER;
                    } else {
                        reason = SWITCH_CAUSE_ORIGINATOR_CANCEL;
                    }
				} else {
					if (to) {
						reason = SWITCH_CAUSE_NO_ANSWER;
					} else if (and_argc > 1) {
						reason = SWITCH_CAUSE_LOSE_RACE;
					} else {
						reason = SWITCH_CAUSE_NO_ANSWER;
					}
				}

				
				switch_channel_hangup(peer_channels[i], reason);
			}
		}


		if (idx > IDX_NADA) {
			peer_session = peer_sessions[idx];
			peer_channel = peer_channels[idx];
		} else {
			status = SWITCH_STATUS_FALSE;
			goto done;
		}

		if (caller_channel) {
			if (switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
				switch_channel_answer(caller_channel);
			} else if (switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
				switch_channel_pre_answer(caller_channel);
			}
		}

		if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
			*bleg = peer_session;
			status = SWITCH_STATUS_SUCCESS;
		} else {
			status = SWITCH_STATUS_FALSE;
		}

	done:
		*cause = SWITCH_CAUSE_UNALLOCATED;
        
        if (var_event) {
			if (peer_channel && !caller_channel) { /* install the vars from the {} params */
                switch_event_header_t *header;
                for (header = var_event->headers; header; header = header->next) {
                    switch_channel_set_variable(peer_channel, header->name, header->value);
                }
            }
            switch_event_destroy(&var_event);
        }

		if (status == SWITCH_STATUS_SUCCESS) {
			if (caller_channel) {
				switch_channel_set_variable(caller_channel, "originate_disposition", "call accepted");
			} 
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Originate Resulted in Success: [%s]\n", switch_channel_get_name(peer_channel));
            *cause = SWITCH_CAUSE_SUCCESS;
            
		} else {
			if (peer_channel) {
				*cause = switch_channel_get_cause(peer_channel);
			} else {
				for (i = 0; i < and_argc; i++) {
					if (!peer_channels[i]) {
						continue;
					}
                    *cause = switch_channel_get_cause(peer_channels[i]);
					break;
				}
			}

            if (!*cause) {
                if (reason) {
                    *cause = reason;
                } else if (caller_channel) {
                    *cause = switch_channel_get_cause(caller_channel);
                } else {
                    *cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
                }
            }

			if (idx == IDX_CANCEL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Originate Cancelled by originator termination Cause: %d [%s]\n",
								  *cause, switch_channel_cause2str(*cause));
                
			} else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Originate Resulted in Error Cause: %d [%s]\n",
								  *cause, switch_channel_cause2str(*cause));
			}
		}
        
        if (caller_channel) {
            switch_channel_set_variable(caller_channel, "originate_disposition", switch_channel_cause2str(*cause));
        }

		if (!pass && write_codec.implementation) {
			switch_core_codec_destroy(&write_codec);
		}

		if (ringback.fh) {
			switch_core_file_close(ringback.fh);
			ringback.fh = NULL;
			if (read_codec && !ringback.asis) {
				switch_core_session_set_read_codec(session, read_codec);
				switch_core_session_reset(session);
			}
		} else if (ringback.audio_buffer) {
			teletone_destroy_session(&ringback.ts);
			switch_buffer_destroy(&ringback.audio_buffer);
			switch_buffer_destroy(&ringback.loop_buffer);
		}

		for (i = 0; i < and_argc; i++) {
			if (!peer_channels[i]) {
				continue;
			}
			switch_core_session_rwunlock(peer_sessions[i]);
		}

		if (status == SWITCH_STATUS_SUCCESS) {
			break;
		}
	}

	switch_safe_free(odata);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_hold(switch_core_session_t *session)
{
	switch_core_session_message_t msg = {0};
	switch_channel_t *channel;

	msg.message_id = SWITCH_MESSAGE_INDICATE_HOLD;
	msg.from = __FILE__;
	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
		
	switch_channel_set_flag(channel, CF_HOLD);
	switch_channel_set_flag(channel, CF_SUSPEND);

	switch_core_session_receive_message(session, &msg);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_hold_uuid(char *uuid)
{
	switch_core_session_t *session;

	if ((session = switch_core_session_locate(uuid))) {
		switch_ivr_hold(session);
		switch_core_session_rwunlock(session);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_unhold(switch_core_session_t *session)
{
	switch_core_session_message_t msg = {0};
	switch_channel_t *channel;
		
	msg.message_id = SWITCH_MESSAGE_INDICATE_UNHOLD;
	msg.from = __FILE__;
	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
		
	switch_channel_clear_flag(channel, CF_HOLD);
	switch_channel_clear_flag(channel, CF_SUSPEND);
	
	switch_core_session_receive_message(session, &msg);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_unhold_uuid(char *uuid)
{
	switch_core_session_t *session;

	if ((session = switch_core_session_locate(uuid))) {
		switch_ivr_unhold(session);
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_broadcast(char *uuid, char *path, switch_media_flag_t flags)
{
    switch_channel_t *channel;
	int nomedia;
	switch_core_session_t *session, *master;
	switch_event_t *event;
	switch_core_session_t *other_session = NULL;
	char *other_uuid = NULL;

	if ((session = switch_core_session_locate(uuid))) {
		char *app;
		master = session;

		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		if ((nomedia = switch_channel_test_flag(channel, CF_NOMEDIA))) {
			switch_ivr_media(uuid, SMF_REBRIDGE);
		}
		
		if (!strncasecmp(path, "speak:", 6)) {
			path += 6;
			app = "speak";
		} else {
			app = "playback";
		}
		
		if ((flags & SMF_ECHO_BLEG) && (other_uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE))
			&& (other_session = switch_core_session_locate(other_uuid))) {
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-name", app);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-arg", "%s", path);
				switch_core_session_queue_private_event(other_session, &event);
			}
			switch_core_session_rwunlock(other_session);
			master = other_session;
			other_session = NULL;
		}
		
		if ((flags & SMF_ECHO_ALEG)) {
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-name", app);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-arg", "%s", path);
				switch_core_session_queue_private_event(session, &event);
			}
			master = session;
		}

		if (nomedia) {
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "nomedia");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "nomedia-uuid", "%s", uuid);
				switch_core_session_queue_private_event(master, &event);
			}
		}
		
		switch_core_session_rwunlock(session);
	}
	return SWITCH_STATUS_SUCCESS;
	
}

SWITCH_DECLARE(switch_status_t) switch_ivr_media(char *uuid, switch_media_flag_t flags)
{
	char *other_uuid = NULL;
	switch_channel_t *channel, *other_channel = NULL;
    switch_core_session_t *session, *other_session;
	switch_core_session_message_t msg = {0};
	switch_status_t status = SWITCH_STATUS_GENERR;
	uint8_t swap = 0;

	msg.message_id = SWITCH_MESSAGE_INDICATE_MEDIA;
	msg.from = __FILE__;

	if ((session = switch_core_session_locate(uuid))) {
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);
		if ((flags & SMF_REBRIDGE) && !switch_channel_test_flag(channel, CF_ORIGINATOR)) {
			swap = 1;
		}
		
		if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
			status = SWITCH_STATUS_SUCCESS;
			switch_channel_clear_flag(channel, CF_NOMEDIA);
			switch_core_session_receive_message(session, &msg);

			if ((flags & SMF_REBRIDGE) && (other_uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE)) 
				&& (other_session = switch_core_session_locate(other_uuid))) {
				other_channel = switch_core_session_get_channel(other_session);
				assert(other_channel != NULL);
				switch_core_session_receive_message(other_session, &msg);
				switch_channel_clear_state_handler(other_channel, NULL);
				switch_core_session_rwunlock(other_session);
			}
			if (other_channel) {
				switch_channel_clear_state_handler(channel, NULL);
			}
		}
		
		switch_core_session_rwunlock(session);

		if (other_channel) {
			if (swap) {
				switch_ivr_uuid_bridge(other_uuid, uuid);
			} else {
				switch_ivr_uuid_bridge(uuid, other_uuid);
			}
		}
	}

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_nomedia(char *uuid, switch_media_flag_t flags)
{
	char *other_uuid;
	switch_channel_t *channel, *other_channel = NULL;
    switch_core_session_t *session, *other_session = NULL;
	switch_core_session_message_t msg = {0};
	switch_status_t status = SWITCH_STATUS_GENERR;
	uint8_t swap = 0;

	msg.message_id = SWITCH_MESSAGE_INDICATE_NOMEDIA;
	msg.from = __FILE__;

	if ((session = switch_core_session_locate(uuid))) {
		status = SWITCH_STATUS_SUCCESS;
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		if ((flags & SMF_REBRIDGE) && !switch_channel_test_flag(channel, CF_ORIGINATOR)) {
			swap = 1;
		}

		if ((flags & SMF_FORCE) || !switch_channel_test_flag(channel, CF_NOMEDIA)) {
			switch_channel_set_flag(channel, CF_NOMEDIA);
			switch_core_session_receive_message(session, &msg);
			if ((flags & SMF_REBRIDGE) && (other_uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) &&
				(other_session = switch_core_session_locate(other_uuid))) {
				other_channel = switch_core_session_get_channel(other_session);
				assert(other_channel != NULL);
				switch_core_session_receive_message(other_session, &msg);
				switch_channel_clear_state_handler(other_channel, NULL);

			}
			if (other_channel) {
				switch_channel_clear_state_handler(channel, NULL);
				if (swap) {
					switch_ivr_signal_bridge(other_session, session);
				} else {
					switch_ivr_signal_bridge(session, other_session);
				}
				switch_core_session_rwunlock(other_session);
			}
		}
		switch_core_session_rwunlock(session);
	}

	return status;
}

static switch_status_t signal_bridge_on_hibernate(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	switch_channel_clear_flag(channel, CF_TRANSFER);

	switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t signal_bridge_on_hangup(switch_core_session_t *session)
{
	char *uuid;
	switch_channel_t *channel = NULL;
    switch_core_session_t *other_session;
	switch_event_t *event;

    channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	if (switch_channel_test_flag(channel, CF_ORIGINATOR)) {
		switch_channel_clear_flag(channel, CF_ORIGINATOR);
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
	}


	if ((uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
		switch_channel_t *other_channel = NULL;

		other_channel = switch_core_session_get_channel(other_session);
		assert(other_channel != NULL);

		switch_channel_hangup(other_channel, switch_channel_get_cause(channel));
		switch_core_session_rwunlock(other_session);
	}


	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table_t signal_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ signal_bridge_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL,
	/*.on_hold */ NULL,
	/*.on_hibernate*/ signal_bridge_on_hibernate
};

SWITCH_DECLARE(switch_status_t) switch_ivr_signal_bridge(switch_core_session_t *session, switch_core_session_t *peer_session)
{
	switch_channel_t *caller_channel, *peer_channel;
	switch_event_t *event;

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	peer_channel = switch_core_session_get_channel(peer_session);
	assert(peer_channel != NULL);

	switch_channel_set_flag(caller_channel, CF_ORIGINATOR);

	switch_channel_clear_state_handler(caller_channel, NULL);
	switch_channel_clear_state_handler(peer_channel, NULL);

	switch_channel_add_state_handler(caller_channel, &signal_bridge_state_handlers);
	switch_channel_add_state_handler(peer_channel, &signal_bridge_state_handlers);


	/* fire events that will change the data table from "show channels" */
	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(caller_channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "signal_bridge");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s", switch_core_session_get_uuid(peer_session));
		switch_event_fire(&event);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(peer_channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "signal_bridge");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s", switch_core_session_get_uuid(session));
		switch_event_fire(&event);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(caller_channel, event);
		switch_event_fire(&event);
	}
	
	switch_channel_set_state_flag(caller_channel, CF_TRANSFER);
	switch_channel_set_state_flag(peer_channel, CF_TRANSFER);


	switch_channel_set_variable(caller_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, switch_core_session_get_uuid(peer_session));
	switch_channel_set_variable(peer_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));

	switch_channel_set_state(caller_channel, CS_HIBERNATE);
	switch_channel_set_state(peer_channel, CS_HIBERNATE);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_multi_threaded_bridge(switch_core_session_t *session, 
																 switch_core_session_t *peer_session,
																 switch_input_callback_function_t input_callback,
																 void *session_data,
																 void *peer_session_data)
	 

															   
{
	switch_core_thread_session_t *this_audio_thread, *other_audio_thread;
	switch_channel_t *caller_channel, *peer_channel;
	int stream_id = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	
	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	switch_channel_set_flag(caller_channel, CF_ORIGINATOR);

	peer_channel = switch_core_session_get_channel(peer_session);
	assert(peer_channel != NULL);

	other_audio_thread = switch_core_session_alloc(peer_session, sizeof(switch_core_thread_session_t));
	this_audio_thread = switch_core_session_alloc(peer_session, sizeof(switch_core_thread_session_t));

	other_audio_thread->objs[0] = session;
	other_audio_thread->objs[1] = peer_session;
	other_audio_thread->objs[2] = &stream_id;
	other_audio_thread->input_callback = input_callback;
	other_audio_thread->objs[4] = session_data;
	other_audio_thread->objs[5] = this_audio_thread;
	other_audio_thread->running = 5;
	switch_mutex_init(&other_audio_thread->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	this_audio_thread->objs[0] = peer_session;
	this_audio_thread->objs[1] = session;
	this_audio_thread->objs[2] = &stream_id;
	this_audio_thread->input_callback = input_callback;
	this_audio_thread->objs[4] = peer_session_data;
	this_audio_thread->objs[5] = other_audio_thread;
	this_audio_thread->running = 2;
	switch_mutex_init(&this_audio_thread->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(peer_session));

	switch_channel_add_state_handler(peer_channel, &audio_bridge_peer_state_handlers);

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) && !switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
		switch_channel_answer(caller_channel);
	}

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
		switch_event_t *event;
		switch_core_session_message_t msg = {0};
		
		switch_channel_set_state(peer_channel, CS_HOLD);

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(caller_channel, event);
			switch_event_fire(&event);
		}
		
		if (switch_core_session_read_lock(peer_session) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_variable(caller_channel, SWITCH_BRIDGE_VARIABLE, switch_core_session_get_uuid(peer_session));
			switch_channel_set_variable(peer_channel, SWITCH_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));

			msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
			msg.from = __FILE__;
			msg.pointer_arg = session;

			switch_core_session_receive_message(peer_session, &msg);

			if (!msg.pointer_arg) {
				status = SWITCH_STATUS_FALSE;
				switch_core_session_rwunlock(peer_session);
				goto done;
			}

			msg.pointer_arg = peer_session;
			switch_core_session_receive_message(session, &msg);

			if (!msg.pointer_arg) {
				status = SWITCH_STATUS_FALSE;
				switch_core_session_rwunlock(peer_session);
				goto done;
			}

			
			switch_channel_set_private(peer_channel, "_bridge_", other_audio_thread);
			switch_channel_set_state(peer_channel, CS_LOOPBACK);
			audio_bridge_thread(NULL, (void *) this_audio_thread);

			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(caller_channel, event);
				switch_event_fire(&event);
			}

            this_audio_thread->objs[0] = NULL;
			this_audio_thread->objs[1] = NULL;
			this_audio_thread->objs[2] = NULL;
			this_audio_thread->input_callback = NULL;
			this_audio_thread->objs[4] = NULL;
			this_audio_thread->objs[5] = NULL;
			switch_mutex_lock(this_audio_thread->mutex);
			this_audio_thread->running = 0;
			switch_mutex_unlock(this_audio_thread->mutex);
			
			switch_channel_clear_flag(caller_channel, CF_ORIGINATOR);

			if (other_audio_thread->running > 0) {
				switch_mutex_lock(other_audio_thread->mutex);
				other_audio_thread->running = -1;
				switch_mutex_unlock(other_audio_thread->mutex);
				while (other_audio_thread->running) {
					switch_yield(1000);
				}
			}
			switch_core_session_rwunlock(peer_session);
			
		} else {
			status = SWITCH_STATUS_FALSE;
		}
	} else {
		status = SWITCH_STATUS_FALSE;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Bridge Failed %s->%s\n", 
						  switch_channel_get_name(caller_channel),
						  switch_channel_get_name(peer_channel)
						  );
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ANSWER);
	} 

 done:

    if (switch_channel_get_state(caller_channel) < CS_HANGUP && 
        switch_true(switch_channel_get_variable(caller_channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE))) {
        switch_channel_hangup(caller_channel, switch_channel_get_cause(peer_channel));
    }

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_uuid_bridge(char *originator_uuid, char *originatee_uuid)
{
	switch_core_session_t *originator_session, *originatee_session;
	switch_channel_t *originator_channel, *originatee_channel;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if ((originator_session = switch_core_session_locate(originator_uuid))) {
		if ((originatee_session = switch_core_session_locate(originatee_uuid))) { 
			originator_channel = switch_core_session_get_channel(originator_session);
			originatee_channel = switch_core_session_get_channel(originatee_session);

			/* override transmit state for originator_channel to bridge to originatee_channel 
			 * install pointer to originatee_session into originator_channel
			 * set CF_TRANSFER on both channels and change state to CS_TRANSMIT to
			 * inturrupt anything they are already doing.
			 * originatee_session will fall asleep and originator_session will bridge to it
			 */
			
			switch_channel_clear_state_handler(originator_channel, NULL);
			switch_channel_clear_state_handler(originatee_channel, NULL);
			switch_channel_set_flag(originator_channel, CF_ORIGINATOR);
			switch_channel_add_state_handler(originator_channel, &uuid_bridge_state_handlers);
			switch_channel_add_state_handler(originatee_channel, &uuid_bridge_state_handlers);
			switch_channel_set_flag(originatee_channel, CF_TAGGED);
			switch_channel_set_private(originator_channel, SWITCH_UUID_BRIDGE, originatee_session);

			/* switch_channel_set_state_flag sets flags you want to be set when the next state change happens */
			switch_channel_set_state_flag(originator_channel, CF_TRANSFER);
			switch_channel_set_state_flag(originatee_channel, CF_TRANSFER);

			/* release the read locks we have on the channels */
			switch_core_session_rwunlock(originator_session);
			switch_core_session_rwunlock(originatee_session);

			/* change the states and let the chips fall where they may */
			switch_channel_set_state(originator_channel, CS_TRANSMIT);
			switch_channel_set_state(originatee_channel, CS_TRANSMIT);

			status = SWITCH_STATUS_SUCCESS;
			
			while(switch_channel_get_state(originatee_channel) < CS_HANGUP && switch_channel_test_flag(originatee_channel, CF_TAGGED)) {
				switch_yield(20000);
			}
		} else {
			switch_core_session_rwunlock(originator_session);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no channel for originatee uuid %s\n", originatee_uuid);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no channel for originator uuid %s\n", originator_uuid);
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_session_transfer(switch_core_session_t *session, char *extension, char *dialplan, char *context)
{
	switch_channel_t *channel;
	switch_caller_profile_t *profile, *new_profile;
	switch_core_session_message_t msg = {0};
	switch_core_session_t *other_session;
	char *uuid = NULL;

	assert(session != NULL);
	assert(extension != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if ((profile = switch_channel_get_caller_profile(channel))) {
		new_profile = switch_caller_profile_clone(session, profile);
		new_profile->destination_number = switch_core_session_strdup(session, extension);

		if (dialplan) {
			new_profile->dialplan = switch_core_session_strdup(session, dialplan);
		} else {
			dialplan = new_profile->dialplan;
		}

		if (context) {
			new_profile->context = switch_core_session_strdup(session, context);
		} else {
			context = new_profile->context;
		}

		if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
			switch_channel_t *other_channel = NULL;

			other_channel = switch_core_session_get_channel(other_session);
			assert(other_channel != NULL);
			
			switch_channel_set_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
			switch_channel_set_variable(other_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);

			switch_channel_hangup(other_channel, SWITCH_CAUSE_BLIND_TRANSFER);
			switch_ivr_media(uuid, SMF_NONE);
			
			switch_core_session_rwunlock(other_session);
		}

		switch_channel_set_caller_profile(channel, new_profile);
		switch_channel_set_flag(channel, CF_TRANSFER);
		switch_channel_set_state(channel, CS_RING);

		msg.message_id = SWITCH_MESSAGE_INDICATE_TRANSFER;
		msg.from = __FILE__;
		switch_core_session_receive_message(session, &msg);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Transfer %s to %s[%s@%s]\n", 
						  switch_channel_get_name(channel), dialplan, extension, context); 
		return SWITCH_STATUS_SUCCESS;
	} 

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_transfer_variable(switch_core_session_t *sessa, switch_core_session_t *sessb, char *var)
{
	switch_channel_t *chana = switch_core_session_get_channel(sessa);
	switch_channel_t *chanb = switch_core_session_get_channel(sessb);
	char *val = NULL;

	if (var) {
		if ((val = switch_channel_get_variable(chana, var))) {
			switch_channel_set_variable(chanb, var, val);
		}
	} else {
		switch_hash_index_t *hi;
		void *vval;
		const void *vvar;

		for (hi = switch_channel_variable_first(chana, switch_core_session_get_pool(sessa)); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &vvar, NULL, &vval);
			if (vvar && vval) {
				switch_channel_set_variable(chanb, (char *) vvar, (char *) vval);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/******************************************************************************************************/

struct switch_ivr_digit_stream_parser {
	int pool_auto_created;
	switch_memory_pool_t *pool;
	switch_hash_t *hash;
	switch_size_t maxlen;
	switch_size_t minlen;
	char terminator;
	unsigned int digit_timeout_ms;
};

struct switch_ivr_digit_stream {
	char *digits;
	switch_time_t last_digit_time;
};

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_new(switch_memory_pool_t *pool, switch_ivr_digit_stream_parser_t **parser)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	if(parser != NULL) {
		int pool_auto_created = 0;

		// if the caller didn't provide a pool, make one
		if (pool == NULL) {
			switch_core_new_memory_pool(&pool);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "created a memory pool\n");
			if (pool != NULL) {
				pool_auto_created = 1;
			}
		}

		// if we have a pool, make a parser object
		if (pool != NULL) {
			*parser = (switch_ivr_digit_stream_parser_t *)switch_core_alloc(pool,sizeof(switch_ivr_digit_stream_parser_t));
		}

		// if we have parser object, initialize it for the caller
		if (*parser != NULL) {
			memset(*parser,0,sizeof(switch_ivr_digit_stream_parser_t));
			(*parser)->pool_auto_created = pool_auto_created;
			(*parser)->pool = pool;
			(*parser)->digit_timeout_ms = 1000;
			switch_core_hash_init(&(*parser)->hash,(*parser)->pool);

			status = SWITCH_STATUS_SUCCESS;
		} else {
			status = SWITCH_STATUS_MEMERR;
			// if we can't create a parser object,clean up the pool if we created it
			if (pool != NULL && pool_auto_created) {
				switch_core_destroy_memory_pool(&pool);
			}
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_destroy(switch_ivr_digit_stream_parser_t *parser)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL) {
		if (parser->hash != NULL) {
			switch_core_hash_destroy(parser->hash);
			parser->hash = NULL;
		}
		// free the memory pool if we created it
		if (parser->pool_auto_created && parser->pool != NULL) {
			status = switch_core_destroy_memory_pool(&parser->pool);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_new(switch_ivr_digit_stream_parser_t *parser, switch_ivr_digit_stream_t **stream)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	// if we have a paser object memory pool and a stream object pointer that is null
	if (parser != NULL && parser->pool && stream != NULL && *stream == NULL) {
		*stream = (switch_ivr_digit_stream_t *)switch_core_alloc(parser->pool,sizeof(switch_ivr_digit_stream_t));
		if (*stream != NULL) {
			memset(*stream,0,sizeof(switch_ivr_digit_stream_t));
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_destroy(switch_ivr_digit_stream_t *stream)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	if (stream == NULL && stream->digits != NULL) {
		free(stream->digits);
		stream->digits = NULL;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_set_event(switch_ivr_digit_stream_parser_t *parser, char *digits, void *data)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL && digits != NULL && *digits && parser->hash != NULL) {

		status = switch_core_hash_insert_dup(parser->hash,digits,data);
		if (status == SWITCH_STATUS_SUCCESS) {
			switch_size_t len = strlen(digits);

			// if we don't have a terminator, then we have to try and
			// figure out when a digit set is completed, therefore we
			// keep track of the min and max digit lengths
			if (parser->terminator == '\0') {
				if (len > parser->maxlen) {
					parser->maxlen = len;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "max len %u\n",parser->maxlen);
				}
				if (parser->minlen == 0 || len < parser->minlen) {
					parser->minlen = len;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "min len %u\n",parser->minlen);
				}
			} else {
				// since we have a terminator, reset min and max
				parser->minlen = 0;
				parser->maxlen = 0;
			}

		}
	}
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unable to add hash for '%s'\n",digits);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_del_event(switch_ivr_digit_stream_parser_t *parser, char *digits)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL && digits != NULL && *digits) {
		status = switch_core_hash_delete(parser->hash,digits);
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unable to del hash for '%s'\n",digits);
	}

	return status;
}

SWITCH_DECLARE(void *) switch_ivr_digit_stream_parser_feed(switch_ivr_digit_stream_parser_t *parser, switch_ivr_digit_stream_t *stream, char digit)
{	void *result = NULL;

	if (parser != NULL && stream != NULL) {
		switch_size_t len = (stream->digits != NULL ? strlen(stream->digits) : 0);

		// handle new digit arrivals
		if(digit != '\0') {

			// if it's not a terminator digit, add it to the collected digits
			if (digit != parser->terminator) {
				// if collected digits length >= the max length of the keys
				// in the hash table, then left shift the digit string
				if (len > 0 && parser->maxlen != 0 && len >= parser->maxlen) {
					char *src = stream->digits + 1;
					char *dst = stream->digits;

					while (*src) {
						*(dst++) = *(src++);
					}
					*dst = digit;
				} else {
					stream->digits = realloc(stream->digits,len+2);
					*(stream->digits+(len++)) = digit;
					*(stream->digits+len) = '\0';
					stream->last_digit_time = switch_time_now() / 1000;
				}
			}
		}

		// don't allow collected digit string testing if there are varying sized keys until timeout
		if ( parser->maxlen - parser->minlen > 0
			&& (switch_time_now() / 1000) - stream->last_digit_time < parser->digit_timeout_ms
		) {
			len = 0;
		}

		// if we have digits to test
		if (len) {
			result = switch_core_hash_find(parser->hash, stream->digits);
			// if we matched the digit string, or this digit is the terminator
			// reset the collected digits for next digit string
			if (result != NULL || parser->terminator == digit) {
				free(stream->digits);
				stream->digits = NULL;
			}
		}
}

	return result;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_reset(switch_ivr_digit_stream_t *stream)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	if (stream != NULL && stream->digits != NULL) {
		free(stream->digits);
		stream->digits = NULL;
		stream->last_digit_time = 0;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_set_terminator(switch_ivr_digit_stream_parser_t *parser, char digit)
{	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL) {
		parser->terminator = digit;
		// since we have a terminator, reset min and max
		parser->minlen = 0;
		parser->maxlen = 0;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

/******************************************************************************************************/

struct switch_ivr_menu_action;

struct switch_ivr_menu {
	char *name;
	char *greeting_sound;
	char *short_greeting_sound;
	char *invalid_sound;
	char *exit_sound;
	char *tts_engine;
	char *tts_voice;
	char *buf;
	char *ptr;
	int max_failures;
	int timeout;
	uint32_t inlen;
	uint32_t flags;
	struct switch_ivr_menu_action *actions;
	struct switch_ivr_menu *next;
	switch_memory_pool_t *pool;
};

struct switch_ivr_menu_action {
	switch_ivr_menu_action_function_t *function;
	switch_ivr_action_t ivr_action;
	char *arg;
	char *bind;
	struct switch_ivr_menu_action *next;
};

static switch_ivr_menu_t *switch_ivr_menu_find(switch_ivr_menu_t *stack, char *name) {
	switch_ivr_menu_t *ret;
	for(ret = stack; ret ; ret = ret->next) {
		if (!name || !strcmp(ret->name, name))
			break;
	}
	return ret;
}

static void switch_ivr_menu_stack_add(switch_ivr_menu_t **top, switch_ivr_menu_t *bottom) 
{
	switch_ivr_menu_t *ptr;

	for(ptr = *top ; ptr && ptr->next ; ptr = ptr->next);

	if (ptr) {
		ptr->next = bottom;
	} else {
		*top = bottom;
	}

}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_init(switch_ivr_menu_t **new_menu,
													 switch_ivr_menu_t *main,
													 char *name, 
													 char *greeting_sound, 
													 char *short_greeting_sound,
													 char *invalid_sound, 
													 char *exit_sound,
													 char *tts_engine,
													 char *tts_voice,
													 int timeout,
													 int max_failures, 
													 switch_memory_pool_t *pool)
{
	switch_ivr_menu_t *menu;
	uint8_t newpool = 0;

	if (!pool) {
		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
			return SWITCH_STATUS_MEMERR;
		}
		newpool = 1;
	}
	
	if (!(menu = switch_core_alloc(pool, sizeof(*menu)))) {
		if (newpool) {
			switch_core_destroy_memory_pool(&pool);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			return SWITCH_STATUS_MEMERR;
		}
	}

	menu->pool = pool;

	if (!switch_strlen_zero(name)) {
		menu->name = switch_core_strdup(menu->pool, name);
	}

	if (!switch_strlen_zero(greeting_sound)) {
		menu->greeting_sound = switch_core_strdup(menu->pool, greeting_sound);
	}

	if (!switch_strlen_zero(short_greeting_sound)) {
		menu->short_greeting_sound = switch_core_strdup(menu->pool, short_greeting_sound);
	}

	if (!switch_strlen_zero(invalid_sound)) {
		menu->invalid_sound = switch_core_strdup(menu->pool, invalid_sound);
	}

	if (!switch_strlen_zero(exit_sound)) {
		menu->exit_sound = switch_core_strdup(menu->pool, exit_sound);
	}

	if (!switch_strlen_zero(tts_engine)) {
		menu->tts_engine = switch_core_strdup(menu->pool, tts_engine);
	}

	if (!switch_strlen_zero(tts_voice)) {
		menu->tts_voice = switch_core_strdup(menu->pool, tts_voice);
	}

	menu->max_failures = max_failures;

	menu->timeout = timeout;

	menu->actions = NULL;

	if (newpool) {
		menu->flags |= SWITCH_IVR_MENU_FLAG_FREEPOOL;
	}

	if (menu->timeout <= 0) {
		menu->timeout = 10000;
	}

	if (main) {
		switch_ivr_menu_stack_add(&main, menu);
	} else {
		menu->flags |= SWITCH_IVR_MENU_FLAG_STACK;
	}
	
	*new_menu = menu;
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_action(switch_ivr_menu_t *menu, switch_ivr_action_t ivr_action, char *arg, char *bind)
{
	switch_ivr_menu_action_t *action;
	uint32_t len;

	if ((action = switch_core_alloc(menu->pool, sizeof(*action)))) {
		action->bind = switch_core_strdup(menu->pool, bind);
		action->next = menu->actions;
		action->arg = switch_core_strdup(menu->pool, arg);
		len = (uint32_t)strlen(action->bind) + 1;
		if (len > menu->inlen) {
			menu->inlen = len;
		}
		action->ivr_action = ivr_action;
		menu->actions = action;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_function(switch_ivr_menu_t *menu, switch_ivr_menu_action_function_t *function, char *arg, char *bind)
{
	switch_ivr_menu_action_t *action;
	uint32_t len;

	if ((action = switch_core_alloc(menu->pool, sizeof(*action)))) {
		action->bind = bind;
		action->next = menu->actions;
		action->arg = switch_core_strdup(menu->pool, arg);
		len = (uint32_t)strlen(action->bind) + 1;
		if (len > menu->inlen) {
			menu->inlen = len;
		}
		action->function = function;
		menu->actions = action;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_free(switch_ivr_menu_t *stack) 
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (stack != NULL && stack->pool != NULL) {
		if (switch_test_flag(stack, SWITCH_IVR_MENU_FLAG_STACK) && switch_test_flag(stack, SWITCH_IVR_MENU_FLAG_FREEPOOL)) {
			switch_memory_pool_t *pool = stack->pool;
			status = switch_core_destroy_memory_pool(&pool);
		} else {
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

static switch_status_t play_or_say(switch_core_session_t *session, switch_ivr_menu_t *menu, char *sound, uint32_t need)
{
	char terminator;
	uint32_t len;
	char *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
    switch_input_args_t args= {0};

	if (session != NULL && menu != NULL && !switch_strlen_zero(sound)) {
		memset(menu->buf, 0, menu->inlen);
		menu->ptr = menu->buf;

		if (!need) {
			len = 1;
			ptr = NULL;
		} else {
			len = menu->inlen;
			ptr = menu->ptr;
		}
        args.buf = ptr;
        args.buflen = len;

		if (*sound == '/' || *sound == '\\') {
			status = switch_ivr_play_file(session, NULL, sound, &args);
		} else {
			if (menu->tts_engine && menu->tts_voice) {
				status = switch_ivr_speak_text(session, menu->tts_engine, menu->tts_voice, 0, sound, &args);
			}
		}

		if (need) {
			menu->ptr += strlen(menu->buf);
			if (strlen(menu->buf) < need) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "waiting for %u digits\n",need);
				status = switch_ivr_collect_digits_count(session, menu->ptr, menu->inlen - strlen(menu->buf), need, "#", &terminator, menu->timeout);
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "digits '%s'\n",menu->buf);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_execute(switch_core_session_t *session, switch_ivr_menu_t *stack, char *name, void *obj)
{
	int reps = 0, errs = 0, match = 0, running = 1;
	char *greeting_sound = NULL, *aptr = NULL;
	char arg[512];
	switch_ivr_action_t todo = SWITCH_IVR_ACTION_DIE;
	switch_ivr_menu_action_t *ap;
	switch_ivr_menu_t *menu;
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (session == NULL || stack == NULL || switch_strlen_zero(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid menu context\n");
		return SWITCH_STATUS_FALSE;
	}

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (!(menu = switch_ivr_menu_find(stack, name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Menu!\n");
		return SWITCH_STATUS_FALSE;
	}
	
	if (!(menu->buf = malloc(menu->inlen))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Memory!\n");
		return SWITCH_STATUS_FALSE;
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Executing IVR menu %s\n", menu->name);

	for (reps = 0 ; (running && status == SWITCH_STATUS_SUCCESS && errs < menu->max_failures) ; reps++) {
		if (!switch_channel_ready(channel)) {
			break;
		}

		if (reps > 0 && menu->short_greeting_sound) {
			greeting_sound = menu->short_greeting_sound;
		} else {
			greeting_sound = menu->greeting_sound;
		}

		match = 0;
		aptr = NULL;

		memset(arg, 0, sizeof(arg));

		memset(menu->buf, 0, menu->inlen);
		status = play_or_say(session, menu, greeting_sound, menu->inlen - 1);

		if (!switch_strlen_zero(menu->buf)) {
			for(ap = menu->actions; ap ; ap = ap->next) {
				if (!strcmp(menu->buf, ap->bind)) {
					char *membuf;
                    
					match++;
					errs = 0;
					if (ap->function) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IVR function on menu '%s' matched '%s' param '%s'\n", menu->name, menu->buf, ap->arg);
						todo = ap->function(menu, ap->arg, arg, sizeof(arg), obj);
						aptr = arg;
					} else {
						todo = ap->ivr_action;
						aptr = ap->arg;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IVR action on menu '%s' matched '%s' param '%s'\n", menu->name, menu->buf,aptr);
					}


					switch(todo) {
					case SWITCH_IVR_ACTION_DIE:
						status = SWITCH_STATUS_FALSE;
						break;
					case SWITCH_IVR_ACTION_PLAYSOUND:
						status = switch_ivr_play_file(session, NULL, aptr, NULL);
						break;
					case SWITCH_IVR_ACTION_SAYTEXT:
						status = switch_ivr_speak_text(session, menu->tts_engine, menu->tts_voice, 0, aptr, NULL);
						break;
					case SWITCH_IVR_ACTION_TRANSFER:
						switch_ivr_session_transfer(session, aptr, NULL, NULL);
						running = 0;
						break;
					case SWITCH_IVR_ACTION_EXECMENU:
						reps = -1;
						status = switch_ivr_menu_execute(session, stack, aptr, obj);
						break;
					case SWITCH_IVR_ACTION_EXECAPP: {
						const switch_application_interface_t *application_interface;

						if ((membuf = strdup(aptr))) {
							char *app_name = membuf;
							char *app_arg = strchr(app_name, ' ');

							if (app_arg) {
								*app_arg = '\0';
								app_arg++;
							}
						
							if (app_name && app_arg) {
								if ((application_interface = switch_loadable_module_get_application_interface(app_name))) {
									if (application_interface->application_function) {
										application_interface->application_function(session, app_arg);
									}
								}
							}
						}
					}
						break;
					case SWITCH_IVR_ACTION_BACK:
						running = 0;
						status = SWITCH_STATUS_SUCCESS;
						break;
					case SWITCH_IVR_ACTION_TOMAIN:
						switch_set_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN);
						status = SWITCH_STATUS_BREAK;
						break;
					case SWITCH_IVR_ACTION_NOOP:
						status = SWITCH_STATUS_SUCCESS;
						break;
					default:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid TODO!\n");
						break;
					}
				}
			}


			if (switch_test_flag(menu, SWITCH_IVR_MENU_FLAG_STACK)) { // top level
				if (switch_test_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN)) { // catch the fallback and recover
					switch_clear_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN);
					status = SWITCH_STATUS_SUCCESS;
					running = 1;
					continue;
				}
			}
		}
		if (*menu->buf && !match) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IVR menu '%s' caught invalid input '%s'\n", menu->name, menu->buf);

			if (menu->invalid_sound) {
				play_or_say(session, menu, menu->invalid_sound, 0);
			}
			errs++;

			if (status == SWITCH_STATUS_SUCCESS) {
				status = switch_ivr_sleep(session, 1000);
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "exit-sound '%s'\n",menu->exit_sound);
	if (!switch_strlen_zero(menu->exit_sound)) {
		status = switch_ivr_play_file(session, NULL, menu->exit_sound, NULL);
	}

	switch_safe_free(menu->buf);

	return status;
}

/******************************************************************************************************/

typedef struct switch_ivr_menu_xml_map {
	char *name;
	switch_ivr_action_t action;
	switch_ivr_menu_action_function_t *function;
	struct switch_ivr_menu_xml_map *next;
} switch_ivr_menu_xml_map_t;

struct switch_ivr_menu_xml_ctx {
	switch_memory_pool_t *pool;
	struct switch_ivr_menu_xml_map *map;
	int autocreated;
};

static switch_ivr_menu_xml_map_t *switch_ivr_menu_stack_xml_find(switch_ivr_menu_xml_ctx_t *xml_ctx, char *name)
{
	switch_ivr_menu_xml_map_t *map = (xml_ctx != NULL ? xml_ctx->map : NULL);
	int rc = -1;

	while (map != NULL && (rc = strcasecmp(map->name,name)) != 0) {
		map =  map->next;
	}

	return (rc == 0 ? map : NULL);
}

static switch_status_t switch_ivr_menu_stack_xml_add(switch_ivr_menu_xml_ctx_t *xml_ctx, char*name, int action, switch_ivr_menu_action_function_t *function)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	// if this action/function does not exist yet
	if (xml_ctx != NULL && name != NULL && xml_ctx->pool != NULL && switch_ivr_menu_stack_xml_find(xml_ctx,name) == NULL) {
		switch_ivr_menu_xml_map_t *map = switch_core_alloc(xml_ctx->pool,sizeof(switch_ivr_menu_xml_map_t));

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "switch_ivr_menu_stack_xml_add bindng '%s'\n",name);
		// and we have memory
		if (map != NULL) {
			map->name = switch_core_strdup(xml_ctx->pool,name);
			map->action = action;
			map->function = function;

			if (map->name != NULL) {
				// insert map item at top of list
				map->next = xml_ctx->map;
				xml_ctx->map = map;
				status = SWITCH_STATUS_SUCCESS;
			} else {
				status = SWITCH_STATUS_MEMERR;
			}
		} else {
			status = SWITCH_STATUS_MEMERR;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_init(switch_ivr_menu_xml_ctx_t **xml_menu_ctx, switch_memory_pool_t *pool)
{
	switch_status_t status	= SWITCH_STATUS_FALSE;
	int autocreated = 0;

	// build a memory pool ?
	if (pool == NULL) {
		status = switch_core_new_memory_pool(&pool);
		autocreated = 1;
	}

	// allocate the xml context
	if (xml_menu_ctx != NULL && pool != NULL) {
		*xml_menu_ctx = switch_core_alloc(pool,sizeof(switch_ivr_menu_xml_ctx_t));
		if (*xml_menu_ctx != NULL) {
			(*xml_menu_ctx)->pool = pool;
			(*xml_menu_ctx)->autocreated = autocreated;
			(*xml_menu_ctx)->map = NULL;
			status = SWITCH_STATUS_SUCCESS;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to alloc xml_ctx\n");
			status = SWITCH_STATUS_FALSE;
		}
	}

	// build the standard/default xml menu handler mappings
	if (status == SWITCH_STATUS_SUCCESS && xml_menu_ctx != NULL && *xml_menu_ctx != NULL) {
		struct iam_s {
			char *name;
			switch_ivr_action_t action;
		} iam [] = {
			{"menu-exit",		SWITCH_IVR_ACTION_DIE},
			{"menu-sub",		SWITCH_IVR_ACTION_EXECMENU},
			{"menu-exec-api",	SWITCH_IVR_ACTION_EXECAPP},
			{"menu-play-sound",	SWITCH_IVR_ACTION_PLAYSOUND},
			{"menu-say-text",	SWITCH_IVR_ACTION_SAYTEXT},
			{"menu-back",		SWITCH_IVR_ACTION_BACK},
			{"menu-top",		SWITCH_IVR_ACTION_TOMAIN},
			{"menu-call-transfer",	SWITCH_IVR_ACTION_TRANSFER},
		};
		int iam_qty = (sizeof(iam)/sizeof(iam[0]));
		int i;

		for(i=0; i<iam_qty && status == SWITCH_STATUS_SUCCESS; i++) {
			status = switch_ivr_menu_stack_xml_add(*xml_menu_ctx,iam[i].name,iam[i].action,NULL);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_add_custom(switch_ivr_menu_xml_ctx_t *xml_menu_ctx, char *name, switch_ivr_menu_action_function_t *function)
{	
	return switch_ivr_menu_stack_xml_add(xml_menu_ctx, name, -1, function);
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_build(switch_ivr_menu_xml_ctx_t *xml_menu_ctx,
                                                                switch_ivr_menu_t **menu_stack,
                                                                switch_xml_t xml_menus,
                                                                switch_xml_t xml_menu)

{
	switch_status_t status	= SWITCH_STATUS_FALSE;

	if (xml_menu_ctx != NULL && menu_stack  != NULL && xml_menu != NULL) {
		char *menu_name		= (char *)switch_xml_attr_soft(xml_menu,"name");		// if the attr doesn't exist, return ""
		char *greet_long	= (char *)switch_xml_attr(xml_menu,"greet-long");		// if the attr doesn't exist, return NULL
		char *greet_short	= (char *)switch_xml_attr(xml_menu,"greet-short");		// if the attr doesn't exist, return NULL
		char *invalid_sound	= (char *)switch_xml_attr(xml_menu,"invalid-sound");		// if the attr doesn't exist, return NULL
		char *exit_sound	= (char *)switch_xml_attr(xml_menu,"exit-sound");		// if the attr doesn't exist, return NULL
		char *tts_engine	= (char *)switch_xml_attr(xml_menu,"tts-engine");		// if the attr doesn't exist, return NULL
		char *tts_voice		= (char *)switch_xml_attr(xml_menu,"tts-voice");		// if the attr doesn't exist, return NULL
		char *timeout		= (char *)switch_xml_attr_soft(xml_menu,"timeout");		// if the attr doesn't exist, return ""
		char *max_failures	= (char *)switch_xml_attr_soft(xml_menu,"max-failures");	// if the attr doesn't exist, return ""
		switch_ivr_menu_t *menu	= NULL;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "building menu '%s'\n",menu_name);
		status = switch_ivr_menu_init(&menu,
									*menu_stack,
									menu_name,
									greet_long,
									greet_short,
									invalid_sound,
									exit_sound,
									tts_engine,
									tts_voice,
									atoi(timeout)*1000,
									atoi(max_failures),
									xml_menu_ctx->pool
									);
		// set the menu_stack for the caller
		if (status == SWITCH_STATUS_SUCCESS && *menu_stack == NULL) {
			*menu_stack = menu;
		}

		if (status == SWITCH_STATUS_SUCCESS && menu != NULL) {
			switch_xml_t xml_kvp;

			// build menu entries
			for(xml_kvp = switch_xml_child(xml_menu, "entry"); xml_kvp != NULL && status == SWITCH_STATUS_SUCCESS; xml_kvp = xml_kvp->next) {
				char *action	= (char *)switch_xml_attr(xml_kvp, "action");
				char *digits	= (char *)switch_xml_attr(xml_kvp, "digits");
				char *param	= (char *)switch_xml_attr_soft(xml_kvp, "param");

				if (!switch_strlen_zero(action) && !switch_strlen_zero(digits)) {
					switch_ivr_menu_xml_map_t *xml_map = xml_menu_ctx->map;
					int found = 0;

					// find and appropriate xml handler
					while(xml_map != NULL && !found) {
						if (!(found = (strcasecmp(xml_map->name,action) == 0))) {
							xml_map = xml_map->next;
						}
					}

					if (found && xml_map != NULL) {
						// do we need to build a new sub-menu ?
						if (xml_map->action == SWITCH_IVR_ACTION_EXECMENU && switch_ivr_menu_find(*menu_stack, param) == NULL) {
							if ((xml_menu = switch_xml_find_child(xml_menus, "menu", "name", param)) != NULL) {
								status = switch_ivr_menu_stack_xml_build(xml_menu_ctx, menu_stack, xml_menus, xml_menu);
							}
						}

						// finally bind the menu entry
						if (status == SWITCH_STATUS_SUCCESS) {
							if (xml_map->function != NULL) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									"binding menu caller control '%s'/'%s' to '%s'\n", xml_map->name, param, digits);
								status = switch_ivr_menu_bind_function(menu, xml_map->function, param, digits);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									"binding menu action '%s' to '%s'\n", xml_map->name, digits);
								status = switch_ivr_menu_bind_action(menu, xml_map->action, param, digits);
							}
						}
					}
				} else {
					status = SWITCH_STATUS_FALSE;
				}
			}
		}
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to build xml menu\n");
	}

	return status;
}


static char *SAY_METHOD_NAMES[] = {
    "N/A",
    "PRONOUNCED",
    "ITERATED",
    "COUNTED",
    NULL
};

static char *SAY_TYPE_NAMES[] = {
	"NUMBER",
	"ITEMS",
	"PERSONS",
	"MESSAGES",
	"CURRENCY",
	"TIME_MEASUREMENT",
	"CURRENT_DATE",
	"CURRENT_TIME",
	"CURRENT_DATE_TIME",
	"TELEPHONE_NUMBER",
	"TELEPHONE_EXTENSION",
	"URL",
    "IP_ADDRESS",
	"EMAIL_ADDRESS",
	"POSTAL_ADDRESS",
	"ACCOUNT_NUMBER",
	"NAME_SPELLED",
	"NAME_PHONETIC",
    NULL
};


static switch_say_method_t get_say_method_by_name(char *name)
{
    int x = 0;
    for (x = 0; SAY_METHOD_NAMES[x]; x++) {
        if (!strcasecmp(SAY_METHOD_NAMES[x], name)) {
            break;
        }
    }

    return (switch_say_method_t) x;
}

static switch_say_method_t get_say_type_by_name(char *name)
{
    int x = 0;
    for (x = 0; SAY_TYPE_NAMES[x]; x++) {
        if (!strcasecmp(SAY_TYPE_NAMES[x], name)) {
            break;
        }
    }
    
    return (switch_say_method_t) x;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_phrase_macro(switch_core_session_t *session,
                                                        char *macro_name,
                                                        char *data,
                                                        char *lang,
                                                        switch_input_args_t *args)

{
	switch_xml_t cfg, xml = NULL, language, macros, macro, input, action;
    char *lname = NULL, *mname = NULL, hint_data[1024] = "", enc_hint[1024] = "";
    switch_status_t status = SWITCH_STATUS_GENERR;
    char *old_sound_prefix = NULL, *sound_path = NULL, *tts_engine = NULL, *tts_voice = NULL;
    switch_channel_t *channel;
    uint8_t done = 0;

    channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	if (!macro_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No phrase macro specified.\n");
		return status;
	}

	if (!lang) {
		lang = "en";
	}

	if (!data) {
		data = "";
	}

    switch_url_encode(data, enc_hint, sizeof(enc_hint));
    snprintf(hint_data, sizeof(hint_data), "macro_name=%s&lang=%s&data=%s", macro_name, lang, enc_hint);
    
	if (switch_xml_locate("phrases", NULL, NULL, NULL, &xml, &cfg, hint_data) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of phrases failed.\n");
        goto done;
	}

    if (!(macros = switch_xml_child(cfg, "macros"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find macros tag.\n");
        goto done;
    }

    if (!(language = switch_xml_child(macros, "language"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find language tag.\n");
        goto done;
    }

    while(language) {
        if ((lname = (char *) switch_xml_attr(language, "name")) && !strcasecmp(lname, lang)) {
            break;
        }
        language = language->next;
    }

    if (!language) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find language %s.\n", lang);
        goto done;
    }

    sound_path = (char *) switch_xml_attr_soft(language, "sound_path");
    tts_engine = (char *) switch_xml_attr_soft(language, "tts_engine");
    tts_voice = (char *) switch_xml_attr_soft(language, "tts_voice");

    old_sound_prefix = switch_channel_get_variable(channel, "sound_prefix");    
    switch_channel_set_variable(channel, "sound_prefix", sound_path);

    if (!(macro = switch_xml_child(language, "macro"))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find any macro tags.\n");
        goto done;
    }
    
    while(macro) {
        if ((mname = (char *) switch_xml_attr(macro, "name")) && !strcasecmp(mname, macro_name)) {
            break;
        }
        macro = macro->next;
    }
    
    if (!macro) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find macro %s.\n", macro_name);
        goto done;
    }

    if (!(input = switch_xml_child(macro, "input"))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find any input tags.\n");
        goto done;
    }

    switch_channel_pre_answer(channel);

    while(input && !done) {
        char *pattern = (char *) switch_xml_attr(input, "pattern");

        if (pattern) {
            pcre *re = NULL;
            int proceed = 0, ovector[30];
            char *substituted = NULL;
            uint32_t len = 0;
            char *odata = NULL;
            char *expanded = NULL;
            switch_xml_t match = NULL;
            
            if ((proceed = switch_perform_regex(data, pattern, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
                match = switch_xml_child(input, "match");
            } else {
                match = switch_xml_child(input, "nomatch");
            }

            if (match) {
				status = SWITCH_STATUS_SUCCESS;
                for (action = switch_xml_child(match, "action"); action && status == SWITCH_STATUS_SUCCESS; action = action->next) {
                    char *adata = (char *) switch_xml_attr_soft(action, "data");
                    char *func = (char *) switch_xml_attr_soft(action, "function");

                    if (strchr(pattern, '(') && strchr(adata, '$')) {
                        len = (uint32_t)(strlen(data) + strlen(adata) + 10);
                        if (!(substituted = malloc(len))) {
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
                            switch_clean_re(re);
                            switch_safe_free(expanded);
                            goto done;
                        }
                        memset(substituted, 0, len);
                        switch_perform_substitution(re, proceed, adata, data, substituted, len, ovector);
                        odata = substituted;
                    } else {
                        odata = adata;
                    }
                    
                    expanded = switch_channel_expand_variables(channel, odata);

                    if (expanded == odata) {
                        expanded = NULL;
                    } else {
                        odata = expanded;
                    }
                    
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Handle %s:[%s] (%s)\n", func, odata, lang);

                    if (!strcasecmp(func, "play-file")) {
                        switch_ivr_play_file(session, NULL, odata, args);
                    } else if (!strcasecmp(func, "break")) {
                        done = 1;
                        break;
                    } else if (!strcasecmp(func, "execute")) {

                    } else if (!strcasecmp(func, "say")) {
                        switch_say_interface_t *si;
                        if ((si = switch_loadable_module_get_say_interface(lang))) {
                            char *say_type = (char *) switch_xml_attr_soft(action, "type");
                            char *say_method = (char *) switch_xml_attr_soft(action, "method");
                            
                            status = si->say_function(session, odata, get_say_type_by_name(say_type), get_say_method_by_name(say_method), args);
                        } else {
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid SAY Interface [%s]!\n", lang);
                        }
                    } else if (!strcasecmp(func, "speak-text")) {
                        switch_codec_t *read_codec;
                        if ((read_codec = switch_core_session_get_read_codec(session))) {
                            
                            status = switch_ivr_speak_text(session,
														   tts_engine,
														   tts_voice,
														   read_codec->implementation->samples_per_second,
														   odata,
														   args);
                        }
                    }
                }
            }
            
            switch_clean_re(re);
            switch_safe_free(expanded);
            switch_safe_free(substituted);
        }

		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}

        input = input->next;
    }

 done:

	switch_channel_set_variable(channel, "sound_prefix", old_sound_prefix);

    if (xml) {
        switch_xml_free(xml);
    }
    return status;
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
