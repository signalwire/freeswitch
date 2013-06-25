/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * mod_soundtouch.cpp -- Example of writeable media bugs
 *
 */

#include <stdexcept>
#include <stdio.h>
#include <SoundTouch.h>
using namespace soundtouch;
using namespace std;

#include <switch.h>
#define STSTART 1024 * 2
#define STBLOCK 1024

SWITCH_MODULE_LOAD_FUNCTION(mod_soundtouch_load);
SWITCH_MODULE_DEFINITION(mod_soundtouch, mod_soundtouch_load, NULL, NULL);

static const float ADJUST_AMOUNT = 0.05f;
struct soundtouch_helper {
	SoundTouch *st;
	switch_core_session_t *session;
	bool send_not_recv;
	bool hook_dtmf;
	float pitch;
	float rate;
	float tempo;
	bool literal;
};

/* type is p=>pitch,r=>rate,t=>tempo */
static float normalize_soundtouch_value(char type, float value)
{
	float min,max;
	switch(type)
	{
		case 'p':
			min = 0.01f;
			max = 1000.0f;
			break;
		case 'r':
			min = 0.01f;
			max = 1000.0f;
			break;
		case 't':
			min = 0.01f;
			max = 1000.0f;
			break;
	}
	if (value < min)
		value = min;
	if (value > max)
		value = max;
	return value;
}

/*Computation taken from SoundTouch library for conversion*/
static float compute_pitch_from_octaves(float octaves)
{
	return (float)exp(0.69314718056f * octaves);
}

static switch_status_t on_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf, switch_dtmf_direction_t direction)
{

	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_soundtouch_"))) {
		struct soundtouch_helper *sth = (struct soundtouch_helper *) switch_core_media_bug_get_user_data(bug);

		if (sth) {
			if (sth->literal) {
				sth->literal = false;
				return SWITCH_STATUS_SUCCESS;
			}


			switch (dtmf->digit) {
			case '*':
				sth->literal=true;
				break;

			case '1':
				sth->pitch = normalize_soundtouch_value('p',sth->pitch - ADJUST_AMOUNT);
				sth->st->setPitch(sth->pitch);
				break;
			case '2':
				sth->pitch = 1.0f;
				sth->st->setPitch(sth->pitch);
				break;
			case '3':
				sth->pitch = normalize_soundtouch_value('p',sth->pitch + ADJUST_AMOUNT);
				sth->st->setPitch(sth->pitch);
				break;

			case '4':
				sth->rate = normalize_soundtouch_value('r',sth->rate - ADJUST_AMOUNT);
				sth->st->setRate(sth->rate);
				break;
			case '5':
				sth->rate = 1.0f;
				sth->st->setRate(sth->rate);
				break;
			case '6':
				sth->rate = normalize_soundtouch_value('r',sth->rate + ADJUST_AMOUNT);
				sth->st->setRate(sth->rate);
				break;

			case '7':
				sth->tempo = normalize_soundtouch_value('t',sth->tempo - ADJUST_AMOUNT);
				sth->st->setTempo(sth->tempo);
				break;
			case '8':
				sth->tempo = 1.0f;
				sth->st->setTempo(sth->tempo);
				break;
			case '9':
				sth->tempo = normalize_soundtouch_value('t',sth->tempo + ADJUST_AMOUNT);
				sth->st->setTempo(sth->tempo);
				break;

			case '0':
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "pitch: %f tempo: %f rate: %f\n",sth->pitch,sth->tempo,sth->rate);
			}

		}


		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
}
static switch_bool_t soundtouch_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	struct soundtouch_helper *sth = (struct soundtouch_helper *) user_data;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		{
			switch_codec_t *read_codec = switch_core_session_get_read_codec(sth->session);
			sth->st = new SoundTouch();
			sth->st->setSampleRate(read_codec->implementation->samples_per_second);
			sth->st->setChannels(read_codec->implementation->number_of_channels);

			sth->st->setSetting(SETTING_USE_QUICKSEEK, 1);
			sth->st->setSetting(SETTING_USE_AA_FILTER, 1);

			if (sth->pitch) {
				sth->st->setPitch(sth->pitch);
			}

			if (sth->rate) {
				sth->st->setRate(sth->rate);
			}

			if (sth->tempo) {
				sth->st->setTempo(sth->tempo);
			}

			if (sth->hook_dtmf)
			{
				if (sth->send_not_recv) {
					switch_core_event_hook_add_send_dtmf(sth->session, on_dtmf);
				} else {
					switch_core_event_hook_add_recv_dtmf(sth->session, on_dtmf);
				}
			}
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
			delete sth->st;
			if (sth->send_not_recv) {
				switch_core_event_hook_remove_send_dtmf(sth->session, on_dtmf);
			} else {
				switch_core_event_hook_remove_recv_dtmf(sth->session, on_dtmf);
			}
		}
		break;
	case SWITCH_ABC_TYPE_READ:
	case SWITCH_ABC_TYPE_WRITE:
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		{
			switch_frame_t *frame;

			assert(sth != NULL);
			assert(sth->st != NULL);

			if (! sth->send_not_recv) {
				frame = switch_core_media_bug_get_read_replace_frame(bug);
			} else {
				frame = switch_core_media_bug_get_write_replace_frame(bug);
			}

			sth->st->putSamples((SAMPLETYPE *) frame->data, frame->samples);

			if (sth->st->numSamples() >= frame->samples * 2) {
				frame->samples = sth->st->receiveSamples((SAMPLETYPE *) frame->data, frame->samples);
				frame->datalen = frame->samples * 2;
			} else {
				memset(frame->data, 0, frame->datalen);
			}

			if (! sth->send_not_recv) {
				switch_core_media_bug_set_read_replace_frame(bug, frame);
			} else {
				switch_core_media_bug_set_write_replace_frame(bug, frame);
			}

		}
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_STANDARD_APP(soundtouch_start_function)
{
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct soundtouch_helper *sth;
	char *argv[6];
	int argc;
	char *lbuf = NULL;
	int x, n;

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_soundtouch_"))) {
		if (!zstr(data) && !strcasecmp(data, "stop")) {
			switch_channel_set_private(channel, "_soundtouch_", NULL);
			switch_core_media_bug_remove(session, &bug);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");
		}
		return;
	}

	sth = (struct soundtouch_helper *) switch_core_session_alloc(session, sizeof(*sth));
	assert(sth != NULL);


	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		sth->pitch = 1;
		sth->rate = 1;
		sth->tempo = 1;
		sth->hook_dtmf = false;
		sth->send_not_recv = false;
		n = 0;
		for (x = 0; x < argc; x++) {
			if (!strncasecmp(argv[x], "send_leg", 8)) {
				sth->send_not_recv = true;
			} else if (!strncasecmp(argv[x], "hook_dtmf", 9)) {
				sth->hook_dtmf = true;
				n++;
			} else if (strchr(argv[x], 'p')) {
				sth->pitch = normalize_soundtouch_value('p', atof(argv[x]));
				n++;
			} else if (strchr(argv[x], 'r')) {
				sth->rate = normalize_soundtouch_value('r', atof(argv[x]));
				n++;
			} else if (strchr(argv[x], 'o')) {
				sth->pitch = normalize_soundtouch_value('p', compute_pitch_from_octaves(atof(argv[x])) );
				n++;
			} else if (strchr(argv[x], 's')) {
				/*12.0f taken from soundtouch conversion to octaves*/
				sth->pitch = normalize_soundtouch_value('p', compute_pitch_from_octaves(atof(argv[x]) / 12.0f) ); 
				n++;
			} else if (strchr(argv[x], 't')) {
				sth->tempo = normalize_soundtouch_value('t', atof(argv[x]));
				n++;
			}
		}
	}

	if (n < 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run, no pitch set\n");
		return;
	}


	sth->session = session;

	if ((status = switch_core_media_bug_add(session, "soundtouch", NULL, soundtouch_callback, sth, 0,
											sth->send_not_recv ? SMBF_WRITE_REPLACE : SMBF_READ_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure!\n");
		return;
	}

	switch_channel_set_private(channel, "_soundtouch_", bug);

}

/* API Interface Function */
#define SOUNDTOUCH_API_SYNTAX "<uuid> [start|stop] [send_leg] [hook_dtmf] [-]<X>s [-]<X>o <X>p <X>r <X>t"
SWITCH_STANDARD_API(soundtouch_api_function)
{
	switch_core_session_t *rsession = NULL;
	switch_channel_t *channel = NULL;
        switch_media_bug_t *bug;
        switch_status_t status;
        struct soundtouch_helper *sth;
	char *mycmd = NULL;
        int argc = 0;
        char *argv[10] = { 0 };
	char *uuid = NULL;
	char *action = NULL;
	char *lbuf = NULL;
	int x, n;

	if (zstr(cmd)) {
		goto usage;
	}

        if (!(mycmd = strdup(cmd))) {
                goto usage;
        }

        if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
                goto usage;
        }

        uuid = argv[0];
        action = argv[1];

        if (!(rsession = switch_core_session_locate(uuid))) {
                stream->write_function(stream, "-ERR Cannot locate session!\n");
                goto done;
        }

	channel = switch_core_session_get_channel(rsession);

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_soundtouch_"))) {
		if (!zstr(action) && !strcasecmp(action, "stop")) {
			switch_channel_set_private(channel, "_soundtouch_", NULL);
			switch_core_media_bug_remove(rsession, &bug);
			stream->write_function(stream, "+OK Success\n");
		} else {
			stream->write_function(stream, "-ERR Cannot run 2 at once on the same channel!\n");
		}
		goto done;
	}

	if (!zstr(action) && strcasecmp(action, "start")) {
		goto usage;
	}

	if (argc < 3) {
		goto usage;
	}

	sth = (struct soundtouch_helper *) switch_core_session_alloc(rsession, sizeof(*sth));
	assert(sth != NULL);


	sth->pitch = 1;
	sth->rate = 1;
	sth->tempo = 1;
	sth->hook_dtmf = false;
	sth->send_not_recv = false;
	n = 0;
	for (x = 2; x < argc; x++) {
		if (!strncasecmp(argv[x], "send_leg", 8)) {
			sth->send_not_recv = true;
		} else if (!strncasecmp(argv[x], "hook_dtmf", 9)) {
			sth->hook_dtmf = true;
			n++;
		} else if (strchr(argv[x], 'p')) {
			sth->pitch = normalize_soundtouch_value('p', atof(argv[x]));
			n++;
		} else if (strchr(argv[x], 'r')) {
			sth->rate = normalize_soundtouch_value('r', atof(argv[x]));
			n++;
		} else if (strchr(argv[x], 'o')) {
			sth->pitch = normalize_soundtouch_value('p', compute_pitch_from_octaves(atof(argv[x])) );
			n++;
		} else if (strchr(argv[x], 's')) {
			/*12.0f taken from soundtouch conversion to octaves*/
			sth->pitch = normalize_soundtouch_value('p', compute_pitch_from_octaves(atof(argv[x]) / 12.0f) ); 
			n++;
		} else if (strchr(argv[x], 't')) {
			sth->tempo = normalize_soundtouch_value('t', atof(argv[x]));
			n++;
		}
	}

	if (n < 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rsession), SWITCH_LOG_WARNING, "Cannot run, no pitch set\n");
		goto usage;
	}

	sth->session = rsession;

	if ((status = switch_core_media_bug_add(rsession, "soundtouch", NULL, soundtouch_callback, sth, 0,
											sth->send_not_recv ? SMBF_WRITE_REPLACE : SMBF_READ_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR Failure!\n");
		goto done;
	} else {
		switch_channel_set_private(channel, "_soundtouch_", bug);
		stream->write_function(stream, "+OK Success\n");
		goto done;
	}


  usage:
        stream->write_function(stream, "-USAGE: %s\n", SOUNDTOUCH_API_SYNTAX);

  done:
        if (rsession) {
                switch_core_session_rwunlock(rsession);
        }

        switch_safe_free(mycmd);
        return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_soundtouch_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname); 

	SWITCH_ADD_APP(app_interface, "soundtouch", "Alter the audio stream", "Alter the audio stream pitch/rate/tempo", 
                   soundtouch_start_function, "[send_leg] [hook_dtmf] [-]<X>s [-]<X>o <X>p <X>r <X>t", SAF_NONE);

	SWITCH_ADD_API(api_interface, "soundtouch", "soundtouch", soundtouch_api_function, SOUNDTOUCH_API_SYNTAX);

	switch_console_set_complete("add soundtouch ::console::list_uuid ::[start:stop");

	/* indicate that the module should continue to be loaded */
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
