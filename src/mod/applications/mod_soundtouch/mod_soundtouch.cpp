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

struct soundtouch_helper {
	SoundTouch *st;
	switch_core_session_t *session;
	int send;
	int read;
	float pitch;
	float octaves;
	float semi;
	float rate;
	float tempo;
	int literal;
};

static switch_status_t on_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf, switch_dtmf_direction_t direction)
{

	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_soundtouch_"))) {
		struct soundtouch_helper *sth = (struct soundtouch_helper *) switch_core_media_bug_get_user_data(bug);

		if (sth) {
			if (sth->literal) {
				sth->literal = 0;
				return SWITCH_STATUS_SUCCESS;
			}


			switch (dtmf->digit) {
			case '*':
				sth->literal++;
				break;
			case '3':
				sth->semi += .5;
				sth->st->setPitchSemiTones(sth->semi);
				sth->st->flush();
				break;
			case '2':
				sth->semi = 0;
				sth->st->setPitchSemiTones(sth->semi);
				sth->st->flush();
				break;
			case '1':
				sth->semi -= .5;
				sth->st->setPitchSemiTones(sth->semi);
				sth->st->flush();
				break;

			case '6':
				sth->pitch += .2;
				sth->st->setPitch(sth->pitch);
				sth->st->flush();
				break;
			case '5':
				sth->pitch = 1;
				sth->st->setPitch(sth->pitch);
				sth->st->flush();
				break;
			case '4':
				sth->pitch -= .2;
				if (sth->pitch <= 0) {
					sth->pitch = .2;
				}
				sth->st->setPitch(sth->pitch);
				sth->st->flush();
				break;

			case '9':
				sth->octaves += .2;
				sth->st->setPitchOctaves(sth->octaves);
				sth->st->flush();
				break;
			case '8':
				sth->octaves = 0;
				sth->st->setPitchOctaves(sth->octaves);
				sth->st->flush();
				break;
			case '7':
				sth->octaves -= .2;
				sth->st->setPitchOctaves(sth->octaves);
				sth->st->flush();
				break;


			case '0':
				sth->octaves = 0;
				sth->st->setPitchOctaves(sth->octaves);
				sth->pitch = 1;
				sth->st->setPitch(sth->pitch);
				sth->semi = 0;
				sth->st->setPitchSemiTones(sth->semi);
				sth->st->flush();

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

			if (sth->semi) {
				sth->st->setPitchSemiTones(sth->semi);
			}

			if (sth->pitch) {
				sth->st->setPitch(sth->pitch);
			}

			if (sth->octaves) {
				sth->st->setPitchOctaves(sth->octaves);
			}

			if (sth->rate) {
				sth->st->setRate(sth->rate);
			}

			if (sth->tempo) {
				sth->st->setRate(sth->tempo);
			}

			if (sth->send) {
				switch_core_event_hook_add_send_dtmf(sth->session, on_dtmf);
			} else {
				switch_core_event_hook_add_recv_dtmf(sth->session, on_dtmf);
			}
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
			delete sth->st;
			if (sth->send) {
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

			if (sth->read) {
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

			if (sth->read) {
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
	int x;

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_soundtouch_"))) {
		if (!switch_strlen_zero(data) && !strcasecmp(data, "stop")) {
			switch_channel_set_private(channel, "_soundtouch_", NULL);
			switch_core_media_bug_remove(session, &bug);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");
		}
		return;
	}

	sth = (struct soundtouch_helper *) switch_core_session_alloc(session, sizeof(*sth));
	assert(sth != NULL);


	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		sth->send = 0;
		sth->read = 0;
		sth->pitch = 1;
		for (x = 0; x < argc; x++) {
			if (!strncasecmp(argv[x], "send", 4)) {
				sth->send = 1;
			} else if (!strncasecmp(argv[x], "read", 4)) {
				sth->read = 1;
			} else if (strchr(argv[x], 'p')) {
				if ((sth->pitch = atof(argv[x]) < 0)) {
					sth->pitch = 0;
				}
			} else if (strchr(argv[x], 'r')) {
				sth->rate = atof(argv[x]);
			} else if (strchr(argv[x], 'o')) {
				sth->octaves = atof(argv[x]);
			} else if (strchr(argv[x], 's')) {
				sth->semi = atof(argv[x]);
			} else if (strchr(argv[x], 't')) {
				if ((sth->tempo = atof(argv[x]) < 0)) {
					sth->tempo = 0;
				}
			}
		}
	}


	sth->session = session;

	if ((status = switch_core_media_bug_add(session, soundtouch_callback, sth, 0,
											sth->read ? SMBF_READ_REPLACE : SMBF_WRITE_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failure!\n");
		return;
	}

	switch_channel_set_private(channel, "_soundtouch_", bug);

}
static switch_application_interface_t soundtouch_application_interface = {
	/*.interface_name */ "soundtouch",
	/*.application_function */ soundtouch_start_function,
	/* long_desc */ "Alter the audio stream",
	/* short_desc */ "Alter the audio stream",
	/* syntax */ "[send|recv] [-]<X>s [.]<X>p",
	/* flags */ SAF_NONE,
	/* next */

};

static switch_loadable_module_interface_t soundtouch_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &soundtouch_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};


SWITCH_MODULE_LOAD_FUNCTION(mod_soundtouch_load)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &soundtouch_module_interface;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
