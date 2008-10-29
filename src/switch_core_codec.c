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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_codec.c -- Main Core Library (codec functions)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

static uint32_t CODEC_ID = 0;

SWITCH_DECLARE(uint32_t) switch_core_codec_next_id(void)
{
	return CODEC_ID++;
}

SWITCH_DECLARE(void) switch_core_session_unset_read_codec(switch_core_session_t *session)
{
	session->real_read_codec = session->read_codec = NULL;
}

SWITCH_DECLARE(void) switch_core_session_unset_write_codec(switch_core_session_t *session)
{	
	session->real_write_codec = session->write_codec = NULL;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_read_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char tmp[30];
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!codec || !codec->implementation) {
		if (session->real_read_codec) {
			session->read_codec = session->real_read_codec;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Restore original codec.\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set UNINITIALIZED codec!\n");
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	} else {

		if (!session->real_read_codec) {
			session->read_codec = session->real_read_codec = codec;
		} else {
			session->read_codec = codec;
		}
	}

	if (!session->read_codec) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No READ codec!\n");
        status = SWITCH_STATUS_FALSE;
        goto end;
	}

	session->read_impl = *session->read_codec->implementation;


	if (session->read_codec && session->read_impl.decoded_bytes_per_packet) {
		if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-read-codec-name", session->read_impl.iananame);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-read-codec-rate", "%d", session->read_impl.actual_samples_per_second);
			if (session->read_impl.actual_samples_per_second != session->read_impl.samples_per_second) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-reported-read-codec-rate", "%d", session->read_impl.samples_per_second);
			}
			switch_event_fire(&event);
		}

		switch_channel_set_variable(channel, "read_codec", session->read_impl.iananame);
		switch_snprintf(tmp, sizeof(tmp), "%d", session->read_impl.actual_samples_per_second);
		switch_channel_set_variable(channel, "read_rate", tmp);


		session->raw_read_frame.codec = session->read_codec;
		session->raw_write_frame.codec = session->read_codec;
		session->enc_read_frame.codec = session->read_codec;
		session->enc_write_frame.codec = session->read_codec;
	}

 end:

	return status;

}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_effective_read_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;	codec = session->read_codec;
	return codec;
}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_read_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;	codec = session->real_read_codec ? session->real_read_codec : session->read_codec;
	return codec;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_read_impl(switch_core_session_t *session,  switch_codec_implementation_t *impp)
{
	if (session->read_impl.decoded_bytes_per_packet) {
		*impp = session->read_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_write_impl(switch_core_session_t *session,  switch_codec_implementation_t *impp)
{
	if (session->write_impl.decoded_bytes_per_packet) {
		*impp = session->write_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_video_read_impl(switch_core_session_t *session,  switch_codec_implementation_t *impp)
{
	if (session->video_read_impl.decoded_bytes_per_packet) {
		*impp = session->video_read_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_video_write_impl(switch_core_session_t *session,  switch_codec_implementation_t *impp)
{
	if (session->video_write_impl.decoded_bytes_per_packet) {
		*impp = session->video_write_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_write_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char tmp[30];
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!codec || !codec->implementation) {
		if (session->real_write_codec) {
			session->write_codec = session->real_write_codec;
			session->write_impl = *session->real_write_codec->implementation;
			session->real_write_codec = NULL;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set NULL codec!\n");
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	} else if (session->write_codec) {
		if (session->real_write_codec) {
			if (codec == session->real_write_codec) {
				session->write_codec = codec;
				session->write_impl = *codec->implementation;
				session->real_write_codec = NULL;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot double-set codec!\n");
				status = SWITCH_STATUS_FALSE;
				goto end;
			}
		} else {
			session->real_write_codec = session->write_codec;
			session->write_codec = codec;
			session->write_impl = *codec->implementation;
		}
	} else {
		session->write_codec = codec;
		session->write_impl = *codec->implementation;
	}

	if (session->write_codec && codec && session->write_impl.decoded_bytes_per_packet) {
		if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-write-codec-name", session->write_impl.iananame);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-write-codec-rate", "%d", session->write_impl.actual_samples_per_second);
			if (session->write_impl.actual_samples_per_second != session->write_impl.samples_per_second) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-reported-write-codec-rate", "%d",
										session->write_impl.actual_samples_per_second);
			}
			switch_event_fire(&event);
		}

		switch_channel_set_variable(channel, "write_codec", session->write_impl.iananame);
		switch_snprintf(tmp, sizeof(tmp), "%d", session->write_impl.actual_samples_per_second);
		switch_channel_set_variable(channel, "write_rate", tmp);
	}

 end:

	return status;
}


SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_effective_write_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;
	codec = session->write_codec;

	return codec;
}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_write_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;
	codec = session->real_write_codec ? session->real_write_codec : session->write_codec;

	return codec;
}



SWITCH_DECLARE(switch_status_t) switch_core_session_set_video_read_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char tmp[30];
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!codec || !codec->implementation) {
		if (session->video_read_codec) {
			session->video_read_codec = NULL;
        	status = SWITCH_STATUS_SUCCESS;
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set NULL codec!\n");
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-video-read-codec-name", codec->implementation->iananame);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-video-read-codec-rate", "%d", codec->implementation->actual_samples_per_second);
		switch_event_fire(&event);
	}

	switch_channel_set_variable(channel, "video_read_codec", codec->implementation->iananame);
	switch_snprintf(tmp, sizeof(tmp), "%d", codec->implementation->actual_samples_per_second);
	switch_channel_set_variable(channel, "video_read_rate", tmp);

	session->video_read_codec = codec;
	session->video_read_impl = *codec->implementation;
 end:

	return status;
}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_video_read_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;
	codec = session->video_read_codec;
	
	return codec;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_video_write_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char tmp[30];
	switch_status_t status = SWITCH_STATUS_SUCCESS;
		if (!codec || !codec->implementation) {
		if (session->video_write_codec) {
			session->video_write_codec = NULL;
        	status = SWITCH_STATUS_SUCCESS;
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set NULL codec!\n");
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

    if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-video-write-codec-name", codec->implementation->iananame);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-video-write-codec-rate", "%d", codec->implementation->actual_samples_per_second);
		switch_event_fire(&event);
	}

	switch_channel_set_variable(channel, "video_write_codec", codec->implementation->iananame);
	switch_snprintf(tmp, sizeof(tmp), "%d", codec->implementation->actual_samples_per_second);
	switch_channel_set_variable(channel, "video_write_rate", tmp);

	session->video_write_codec = codec;
	session->video_write_impl = *codec->implementation;

 end:
	
	return status;
}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_video_write_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;
	codec = session->video_write_codec;

	return codec;

}


SWITCH_DECLARE(switch_status_t) switch_core_codec_copy(switch_codec_t *codec, switch_codec_t *new_codec, switch_memory_pool_t *pool)
{
	switch_status_t status;

	switch_assert(codec != NULL);
	switch_assert(new_codec != NULL);

	if (pool) {
		new_codec->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&new_codec->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(new_codec, SWITCH_CODEC_FLAG_FREE_POOL);
	}

	new_codec->codec_interface = codec->codec_interface;
	new_codec->implementation = codec->implementation;
	new_codec->flags = codec->flags;

	if (codec->fmtp_in) {
		new_codec->fmtp_in = switch_core_strdup(new_codec->memory_pool, codec->fmtp_in);
	}

	new_codec->implementation->init(new_codec, new_codec->flags, NULL);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_init(switch_codec_t *codec, const char *codec_name, const char *fmtp,
													   uint32_t rate, int ms, int channels, uint32_t flags,
													   const switch_codec_settings_t *codec_settings, switch_memory_pool_t *pool)
{
	const switch_codec_interface_t *codec_interface;
	const switch_codec_implementation_t *iptr, *implementation = NULL;
	const char *mode = fmtp;

	switch_assert(codec != NULL);
	switch_assert(codec_name != NULL);

	memset(codec, 0, sizeof(*codec));

	if (channels == 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stereo is currently unsupported. please downsample audio source to mono.\n");
		return SWITCH_STATUS_GENERR;
	}

	if ((codec_interface = switch_loadable_module_get_codec_interface(codec_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid codec %s!\n", codec_name);
		return SWITCH_STATUS_GENERR;
	}

	if (!strcasecmp(codec_name, "ilbc") && mode && strncasecmp(mode, "mode=", 5)) {
		int mms;
		mode += 5;
		if (mode) {
			mms = atoi(mode);
			if (mms > 0 && mms < 120) {
				ms = mms;
			}
		}
	}

	/* If no specific codec interval is requested opt for 20ms above all else because lots of stuff assumes it */
	if (!ms) {
		for (iptr = codec_interface->implementations; iptr; iptr = iptr->next) {
			if ((!rate || rate == iptr->samples_per_second) &&
				(20 == (iptr->microseconds_per_packet / 1000)) && (!channels || channels == iptr->number_of_channels)) {
				implementation = iptr;
				goto found;
			}
		}
	}

	/* Either looking for a specific interval or there was no interval specified and there wasn't one @20ms available */
	for (iptr = codec_interface->implementations; iptr; iptr = iptr->next) {
		if ((!rate || rate == iptr->samples_per_second) &&
			(!ms || ms == (iptr->microseconds_per_packet / 1000)) && (!channels || channels == iptr->number_of_channels)) {
			implementation = iptr;
			break;
		}
	}

  found:

	if (implementation) {
		switch_status_t status;
		codec->codec_interface = codec_interface;
		codec->implementation = implementation;
		codec->flags = flags;

		if (pool) {
			codec->memory_pool = pool;
		} else {
			if ((status = switch_core_new_memory_pool(&codec->memory_pool)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			switch_set_flag(codec, SWITCH_CODEC_FLAG_FREE_POOL);
		}

		if (fmtp) {
			codec->fmtp_in = switch_core_strdup(codec->memory_pool, fmtp);
		}

		implementation->init(codec, flags, codec_settings);

		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Codec %s Exists but not at the desired implementation. %dhz %dms\n", codec_name, rate,
						  ms);
	}

	return SWITCH_STATUS_NOTIMPL;
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_encode(switch_codec_t *codec,
														 switch_codec_t *other_codec,
														 void *decoded_data,
														 uint32_t decoded_data_len,
														 uint32_t decoded_rate,
														 void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate, unsigned int *flag)
{
	switch_assert(codec != NULL);
	switch_assert(encoded_data != NULL);
	switch_assert(decoded_data != NULL);

	if (!codec->implementation) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_ENCODE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec encoder is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	return codec->implementation->encode(codec, other_codec, decoded_data, decoded_data_len, decoded_rate, encoded_data, encoded_data_len, encoded_rate,
										 flag);
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_decode(switch_codec_t *codec,
														 switch_codec_t *other_codec,
														 void *encoded_data,
														 uint32_t encoded_data_len,
														 uint32_t encoded_rate,
														 void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate, unsigned int *flag)
{
	switch_assert(codec != NULL);
	switch_assert(encoded_data != NULL);
	switch_assert(decoded_data != NULL);

	if (!codec->implementation) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_DECODE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec decoder is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	return codec->implementation->decode(codec, other_codec, encoded_data, encoded_data_len, encoded_rate, decoded_data, decoded_data_len, decoded_rate,
										 flag);
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_destroy(switch_codec_t *codec)
{
	switch_assert(codec != NULL);

	if (!codec->implementation) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Codec is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	codec->implementation->destroy(codec);

	if (switch_test_flag(codec, SWITCH_CODEC_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&codec->memory_pool);
	}

	memset(codec, 0, sizeof(*codec));

	return SWITCH_STATUS_SUCCESS;
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
