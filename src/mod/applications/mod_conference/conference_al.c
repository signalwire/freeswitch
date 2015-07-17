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
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter at 0xdecafbad dot com>
 * Dale Thatcher <freeswitch at dalethatcher dot com>
 * Chris Danielson <chris at maxpowersoft dot com>
 * Rupa Schomaker <rupa@rupa.com>
 * David Weekly <david@weekly.org>
 * Joao Mesquita <jmesquita@gmail.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */
#include <mod_conference.h>



al_handle_t *conference_al_create(switch_memory_pool_t *pool)
{
	al_handle_t *al;

	al = switch_core_alloc(pool, sizeof(al_handle_t));
	switch_mutex_init(&al->mutex, SWITCH_MUTEX_NESTED, pool);

	return al;
}

#ifndef OPENAL_POSITIONING
void conference_al_gen_arc(conference_obj_t *conference, switch_stream_handle_t *stream)
{
}
void conference_al_process(al_handle_t *al, void *data, switch_size_t datalen, int rate)
{
}

#else
void conference_al_gen_arc(conference_obj_t *conference, switch_stream_handle_t *stream)
{
	float offset;
	float pos;
	float radius;
	float x, z;
	float div = 3.14159f / 180;
	conference_member_t *member;
	uint32_t count = 0;

	if (!conference->count) {
		return;
	}

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (member->channel && conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) && !conference_utils_member_test_flag(member, MFLAG_NO_POSITIONAL)) {
			count++;
		}
	}

	if (count < 3) {
		for (member = conference->members; member; member = member->next) {
			if (member->channel && !conference_utils_member_test_flag(member, MFLAG_NO_POSITIONAL) && member->al) {

				member->al->pos_x = 0;
				member->al->pos_y = 0;
				member->al->pos_z = 0;
				member->al->setpos = 1;

				if (stream) {
					stream->write_function(stream, "Member %d (%s) 0.0:0.0:0.0\n", member->id, switch_channel_get_name(member->channel));
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Member %d (%s) 0.0:0.0:0.0\n",
									  member->id, switch_channel_get_name(member->channel));
				}
			}
		}

		goto end;
	}

	offset = 180 / (count - 1);

	radius = 1.0f;

	pos = -90.0f;

	for (member = conference->members; member; member = member->next) {

		if (!member->channel || conference_utils_member_test_flag(member, MFLAG_NO_POSITIONAL) || !conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
			continue;
		}

		if (!member->al) {
			member->al = conference_al_create(member->pool);
		}
		conference_utils_member_set_flag(member, MFLAG_POSITIONAL);

		if (pos == 0) {
			x = 0;
			z = radius;
		} else if (pos == -90) {
			z = 0;
			x = radius * -1;
		} else if (pos == 90) {
			z = 0;
			x = radius;
		} else if (pos < 0) {
			z = cos((90+pos) * div) * radius;
			x = sin((90+pos) * div) * radius * -1.0f;
		} else {
			x = cos(pos * div) * radius;
			z = sin(pos * div) * radius;
		}

		member->al->pos_x = x;
		member->al->pos_y = 0;
		member->al->pos_z = z;
		member->al->setpos = 1;

		if (stream) {
			stream->write_function(stream, "Member %d (%s) %0.2f:0.0:%0.2f\n", member->id, switch_channel_get_name(member->channel), x, z);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Member %d (%s) %0.2f:0.0:%0.2f\n",
							  member->id, switch_channel_get_name(member->channel), x, z);
		}

		pos += offset;
	}

 end:

	switch_mutex_unlock(conference->member_mutex);

	return;

}



void conference_al_process(al_handle_t *al, void *data, switch_size_t datalen, int rate)
{

	if (rate != 48000) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only 48khz is supported.\n");
		return;
	}

	if (!al->device) {
		ALCint contextAttr[] = {
			ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT,
			ALC_FORMAT_TYPE_SOFT, ALC_SHORT_SOFT,
			ALC_FREQUENCY, rate,
			ALC_HRTF_SOFT, AL_TRUE,
			0
		};

		switch_mutex_lock(conference_globals.setup_mutex);
		if ((al->device = alcLoopbackOpenDeviceSOFT(NULL))) {
			const ALshort silence[16] = { 0 };
			float orient[6] = { /*fwd:*/ 0., 0., -1., /*up:*/ 0., 1., 0. };

			al->context = alcCreateContext(al->device, contextAttr);
			alcSetThreadContext(al->context);

			/* listener at origin, facing down -z (ears at 0.0m height) */
			alListener3f( AL_POSITION, 0. ,0, 0. );
			alListener3f( AL_VELOCITY, 0., 0., 0. );
			alListenerfv( AL_ORIENTATION, orient );


			alGenSources(1, &al->source);
			alSourcef( al->source, AL_PITCH, 1.);
			alSourcef( al->source, AL_GAIN, 1.);
			alGenBuffers(2, al->buffer_in);

			alBufferData(al->buffer_in[0], AL_FORMAT_MONO16, data, datalen, rate);
			//alBufferData(al->buffer_in[0], AL_FORMAT_MONO16, NULL, 0, rate);
			alBufferData(al->buffer_in[1], AL_FORMAT_MONO16, silence, sizeof(silence), rate);
			alSourceQueueBuffers(al->source, 2, al->buffer_in);
			alSourcePlay(al->source);
		}
		switch_mutex_unlock(conference_globals.setup_mutex);
	}

	if (al->device) {
		ALint processed = 0, state = 0;

		//alcSetThreadContext(al->context);
		alGetSourcei(al->source, AL_SOURCE_STATE, &state);
		alGetSourcei(al->source, AL_BUFFERS_PROCESSED, &processed);

		if (al->setpos) {
			al->setpos = 0;
			alSource3f(al->source, AL_POSITION, al->pos_x, al->pos_y, al->pos_z);
			//alSource3f(al->source, AL_VELOCITY, .01, 0., 0.);
		}

		if (processed > 0) {
			ALuint bufid;
			alSourceUnqueueBuffers(al->source, 1, &bufid);
			alBufferData(bufid, AL_FORMAT_MONO16, data, datalen, rate);
			alSourceQueueBuffers(al->source, 1, &bufid);
		}

		if (state != AL_PLAYING) {
			alSourcePlay(al->source);
		}

		alcRenderSamplesSOFT(al->device, data, datalen / 2);
	}
}
#endif

#ifndef OPENAL_POSITIONING
switch_status_t conference_al_parse_position(al_handle_t *al, const char *data)
{
	return SWITCH_STATUS_FALSE;
}

#else
switch_status_t conference_al_parse_position(al_handle_t *al, const char *data)
{
	char *args[3];
	int num;
	char *dup;


	dup = strdup((char *)data);
	switch_assert(dup);

	if ((num = switch_split(dup, ':', args)) != 3) {
		return SWITCH_STATUS_FALSE;
	}

	al->pos_x = atof(args[0]);
	al->pos_y = atof(args[1]);
	al->pos_z = atof(args[2]);
	al->setpos = 1;

	switch_safe_free(dup);

	return SWITCH_STATUS_SUCCESS;
}
#endif

#ifdef OPENAL_POSITIONING
void conference_al_close(al_handle_t *al)
{
	if (!al) return;

	switch_mutex_lock(conference_globals.setup_mutex);
	if (al->source) {
		alDeleteSources(1, &al->source);
		al->source = 0;
	}

	if (al->buffer_in[0]) {
		alDeleteBuffers(2, al->buffer_in);
		al->buffer_in[0] = 0;
		al->buffer_in[1] = 0;
	}

	if (al->context) {
		alcDestroyContext(al->context);
		al->context = 0;
	}

	if (al->device) {
		alcCloseDevice(al->device);
		al->device = NULL;
	}
	switch_mutex_unlock(conference_globals.setup_mutex);
}
#endif

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
