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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Christopher M. Rienzo <chris@rienzo.com>
 *
 *
 * switch_core_codec.c -- Main Core Library (codec functions)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

static uint32_t CODEC_ID = 1;

SWITCH_DECLARE(uint32_t) switch_core_codec_next_id(void)
{
	return CODEC_ID++;
}

SWITCH_DECLARE(void) switch_core_session_unset_read_codec(switch_core_session_t *session)
{
	switch_mutex_t *mutex = NULL;

	switch_mutex_lock(session->codec_read_mutex);
	if (session->read_codec) mutex = session->read_codec->mutex;
	if (mutex) switch_mutex_lock(mutex);
	session->real_read_codec = session->read_codec = NULL;
	session->raw_read_frame.codec = session->read_codec;
	session->raw_write_frame.codec = session->read_codec;
	session->enc_read_frame.codec = session->read_codec;
	session->enc_write_frame.codec = session->read_codec;
	if (mutex) switch_mutex_unlock(mutex);
	switch_mutex_unlock(session->codec_read_mutex);
}

SWITCH_DECLARE(void) switch_core_session_lock_codec_write(switch_core_session_t *session)
{
	switch_mutex_lock(session->codec_write_mutex);
}

SWITCH_DECLARE(void) switch_core_session_unlock_codec_write(switch_core_session_t *session)
{
	switch_mutex_unlock(session->codec_write_mutex);
}

SWITCH_DECLARE(void) switch_core_session_lock_codec_read(switch_core_session_t *session)
{
	switch_mutex_lock(session->codec_read_mutex);
}

SWITCH_DECLARE(void) switch_core_session_unlock_codec_read(switch_core_session_t *session)
{
	switch_mutex_unlock(session->codec_read_mutex);
}

SWITCH_DECLARE(void) switch_core_session_unset_write_codec(switch_core_session_t *session)
{
	switch_mutex_t *mutex = NULL;

	switch_mutex_lock(session->codec_write_mutex);
	if (session->write_codec) mutex = session->write_codec->mutex;
	if (mutex) switch_mutex_lock(mutex);
	session->real_write_codec = session->write_codec = NULL;
	if (mutex) switch_mutex_unlock(mutex);
	switch_mutex_unlock(session->codec_write_mutex);
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_real_read_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char tmp[30];
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int changed_read_codec = 0;

	switch_mutex_lock(session->codec_read_mutex);

	if (codec && (!codec->implementation || !switch_core_codec_ready(codec))) {
		codec = NULL;
	}

	if (codec) {
		/* set real_read_codec and read_codec */
		if (!session->real_read_codec) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Original read codec set to %s:%d\n",
							  switch_channel_get_name(session->channel), codec->implementation->iananame, codec->implementation->ianacode);
			session->read_codec = session->real_read_codec = codec;
			changed_read_codec = 1;
			if (codec->implementation) {
				session->read_impl = *codec->implementation;
				session->real_read_impl = *codec->implementation;
			} else {
				memset(&session->read_impl, 0, sizeof(session->read_impl));
			}
		} else { /* replace real_read_codec */
			switch_codec_t *cur_codec;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Original read codec replaced with %s:%d\n",
							  switch_channel_get_name(session->channel), codec->implementation->iananame, codec->implementation->ianacode);
			/* Set real_read_codec to front of the list of read_codecs */
			cur_codec = session->read_codec;
			while (cur_codec != NULL) {
				if (cur_codec->next == session->real_read_codec) {
					cur_codec->next = codec;
					break;
				}
				cur_codec = cur_codec->next;
			}
			session->real_read_codec = codec;
			/* set read_codec with real_read_codec if it no longer is ready */
			if (!switch_core_codec_ready(session->read_codec)) {
				session->read_codec = codec;
				changed_read_codec = 1;
				if (codec->implementation) {
					session->read_impl = *codec->implementation;
					session->real_read_impl = *codec->implementation;
				} else {
					memset(&session->read_impl, 0, sizeof(session->read_impl));
				}
			}
		}

		/* force media bugs to copy the read codec from the next frame */
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		if (switch_core_codec_ready(&session->bug_codec)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Destroying BUG Codec %s:%d\n",
				session->bug_codec.implementation->iananame, session->bug_codec.implementation->ianacode);
			switch_core_codec_destroy(&session->bug_codec);
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
	} else {
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if (changed_read_codec && session->read_codec && session->read_impl.decoded_bytes_per_packet) {
		if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-read-codec-name", session->read_impl.iananame);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-read-codec-rate", "%d", session->read_impl.actual_samples_per_second);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-read-codec-bit-rate", "%d", session->read_impl.bits_per_second);
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

	if (session->read_codec) {
		switch_channel_set_flag(channel, CF_MEDIA_SET);
	}

	switch_mutex_unlock(session->codec_read_mutex);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_read_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char tmp[30];
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_mutex_lock(session->codec_read_mutex);

	if (codec && (!codec->implementation || !switch_core_codec_ready(codec))) {
		codec = NULL;
	}

	if (codec) {
		if (!session->real_read_codec) {
			session->read_codec = session->real_read_codec = codec;
			if (codec->implementation) {
				session->read_impl = *codec->implementation;
				session->real_read_impl = *codec->implementation;
			} else {
				memset(&session->read_impl, 0, sizeof(session->read_impl));
			}
		} else {
			if (codec == session->read_codec) {
				goto end;
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Push codec %s:%d\n",
							  switch_channel_get_name(session->channel), codec->implementation->iananame, codec->implementation->ianacode);
			codec->next = session->read_codec;
			session->read_codec = codec;
			if (codec->implementation) {
				session->read_impl = *codec->implementation;
			} else {
				memset(&session->read_impl, 0, sizeof(session->read_impl));
			}
		}
	} else {
		if (session->read_codec == session->real_read_codec) {
			goto end;
		}

		if (session->read_codec->next) {
			switch_codec_t *old = session->read_codec;
			session->read_codec = session->read_codec->next;
			if (session->read_codec->implementation) {
				session->read_impl = *session->read_codec->implementation;
			} else {
				memset(&session->read_impl, 0, sizeof(session->read_impl));
			}
			old->next = NULL;

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Restore previous codec %s:%d.\n",
							  switch_channel_get_name(session->channel),
							  session->read_impl.iananame ? session->read_impl.iananame : "N/A", session->read_impl.ianacode);
			

		} else if (session->real_read_codec) {
			session->read_codec = session->real_read_codec;
			if (session->real_read_codec->implementation) {
				session->read_impl = *session->real_read_codec->implementation;
			} else {
				memset(&session->read_impl, 0, sizeof(session->read_impl));
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Restore original codec.\n");
		} else {
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	}

	if (!session->read_codec) {
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if (session->read_codec && session->read_impl.decoded_bytes_per_packet) {
		if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-read-codec-name", session->read_impl.iananame);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-read-codec-rate", "%d", session->read_impl.actual_samples_per_second);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-read-codec-bit-rate", "%d", session->read_impl.bits_per_second);
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

	if (session->read_codec) {
		switch_channel_set_flag(channel, CF_MEDIA_SET);
	}

	switch_mutex_unlock(session->codec_read_mutex);
	return status;

}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_effective_read_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;
	codec = session->read_codec;
	return codec;
}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_read_codec(switch_core_session_t *session)
{
	switch_codec_t *codec;
	codec = session->real_read_codec ? session->real_read_codec : session->read_codec;
	return codec;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_read_impl(switch_core_session_t *session, switch_codec_implementation_t *impp)
{
	if (session->read_impl.codec_id) {
		*impp = session->read_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_real_read_impl(switch_core_session_t *session, switch_codec_implementation_t *impp)
{
	if (session->real_read_impl.codec_id) {
		*impp = session->real_read_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_write_impl(switch_core_session_t *session, switch_codec_implementation_t *impp)
{
	if (session->write_impl.codec_id) {
		*impp = session->write_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_video_read_impl(switch_core_session_t *session, switch_codec_implementation_t *impp)
{
	if (session->video_read_impl.codec_id) {
		*impp = session->video_read_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_video_write_impl(switch_core_session_t *session, switch_codec_implementation_t *impp)
{
	if (session->video_write_impl.codec_id) {
		*impp = session->video_write_impl;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_read_impl(switch_core_session_t *session, const switch_codec_implementation_t *impp)
{
	session->read_impl = *impp;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_write_impl(switch_core_session_t *session, const switch_codec_implementation_t *impp)
{
	session->write_impl = *impp;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_video_read_impl(switch_core_session_t *session, const switch_codec_implementation_t *impp)
{
	session->video_read_impl = *impp;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_video_write_impl(switch_core_session_t *session, const switch_codec_implementation_t *impp)
{
	session->video_write_impl = *impp;
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_write_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char tmp[30];
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_mutex_lock(session->codec_write_mutex);

	if (!codec || !codec->implementation || !switch_core_codec_ready(codec)) {
		if (session->real_write_codec) {
			session->write_codec = session->real_write_codec;
			session->write_impl = *session->real_write_codec->implementation;
			session->real_write_codec = NULL;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot set NULL codec!\n");
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
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot double-set codec!\n");
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

	if (session->write_codec && codec && session->write_impl.codec_id) {
		if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-Write-Codec-Name", session->write_impl.iananame);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Write-Codec-Rate", "%d", session->write_impl.actual_samples_per_second);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Write-codec-bit-rate", "%d", session->write_impl.bits_per_second);
			if (session->write_impl.actual_samples_per_second != session->write_impl.samples_per_second) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Reported-Write-Codec-Rate", "%d",
										session->write_impl.actual_samples_per_second);
			}
			switch_event_fire(&event);
		}

		switch_channel_set_variable(channel, "write_codec", session->write_impl.iananame);
		switch_snprintf(tmp, sizeof(tmp), "%d", session->write_impl.actual_samples_per_second);
		switch_channel_set_variable(channel, "write_rate", tmp);
	}

  end:
	switch_mutex_unlock(session->codec_write_mutex);

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

	if (!codec || !codec->implementation || !switch_core_codec_ready(codec)) {
		if (session->video_read_codec) {
			session->video_read_codec = NULL;
			status = SWITCH_STATUS_SUCCESS;
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot set NULL codec!\n");
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
	if (codec->implementation) {
		session->video_read_impl = *codec->implementation;
	} else {
		memset(&session->video_read_impl, 0, sizeof(session->video_read_impl));
	}
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
	if (!codec || !codec->implementation || !switch_core_codec_ready(codec)) {
		if (session->video_write_codec) {
			session->video_write_codec = NULL;
			status = SWITCH_STATUS_SUCCESS;
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot set NULL codec!\n");
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

SWITCH_DECLARE(switch_status_t) switch_core_codec_parse_fmtp(const char *codec_name, const char *fmtp, uint32_t rate, switch_codec_fmtp_t *codec_fmtp)
{
	switch_codec_interface_t *codec_interface;
	switch_status_t status = SWITCH_STATUS_FALSE;
	
	if (zstr(codec_name) || zstr(fmtp) || !codec_fmtp) {
		return SWITCH_STATUS_FALSE;
	}

	memset(codec_fmtp, 0, sizeof(*codec_fmtp));
	
	if ((codec_interface = switch_loadable_module_get_codec_interface(codec_name))) {
		if (codec_interface->parse_fmtp) {
			codec_fmtp->actual_samples_per_second = rate;
			status = codec_interface->parse_fmtp(fmtp, codec_fmtp);
		}

		UNPROTECT_INTERFACE(codec_interface);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_reset(switch_codec_t *codec)
{
	switch_assert(codec != NULL);

	codec->implementation->destroy(codec);
	codec->implementation->init(codec, codec->flags, NULL);

	return SWITCH_STATUS_SUCCESS;
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
	}

	new_codec->codec_interface = codec->codec_interface;
	new_codec->implementation = codec->implementation;
	new_codec->flags = codec->flags;

	if (!pool) {
		switch_set_flag(new_codec, SWITCH_CODEC_FLAG_FREE_POOL);
	}

	if (codec->fmtp_in) {
		new_codec->fmtp_in = switch_core_strdup(new_codec->memory_pool, codec->fmtp_in);
	}

	new_codec->implementation->init(new_codec, new_codec->flags, NULL);

	switch_mutex_init(&new_codec->mutex, SWITCH_MUTEX_NESTED, new_codec->memory_pool);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_init_with_bitrate(switch_codec_t *codec, const char *codec_name, const char *fmtp,
													   uint32_t rate, int ms, int channels, uint32_t bitrate, uint32_t flags,
													   const switch_codec_settings_t *codec_settings, switch_memory_pool_t *pool)
{
	switch_codec_interface_t *codec_interface;
	const switch_codec_implementation_t *iptr, *implementation = NULL;

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

	/* If no specific codec interval is requested opt for 20ms above all else because lots of stuff assumes it */
	if (!ms) {
		for (iptr = codec_interface->implementations; iptr; iptr = iptr->next) {
			uint32_t crate = !strcasecmp(codec_name, "g722") ? iptr->samples_per_second : iptr->actual_samples_per_second;
			if ((!rate || rate == crate) && (!bitrate || bitrate == (uint32_t)iptr->bits_per_second) &&
				(20 == (iptr->microseconds_per_packet / 1000)) && (!channels || channels == iptr->number_of_channels)) {
				implementation = iptr;
				goto found;
			}
		}
	}

	/* Either looking for a specific interval or there was no interval specified and there wasn't one @20ms available */
	for (iptr = codec_interface->implementations; iptr; iptr = iptr->next) {
		uint32_t crate = !strcasecmp(codec_name, "g722") ? iptr->samples_per_second : iptr->actual_samples_per_second;
		if ((!rate || rate == crate) && (!bitrate || bitrate == (uint32_t)iptr->bits_per_second) &&
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
		switch_mutex_init(&codec->mutex, SWITCH_MUTEX_NESTED, codec->memory_pool);
		switch_set_flag(codec, SWITCH_CODEC_FLAG_READY);
		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Codec %s Exists but not at the desired implementation. %dhz %dms\n", codec_name, rate,
						  ms);
	}

	UNPROTECT_INTERFACE(codec_interface);

	return SWITCH_STATUS_NOTIMPL;
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_encode(switch_codec_t *codec,
														 switch_codec_t *other_codec,
														 void *decoded_data,
														 uint32_t decoded_data_len,
														 uint32_t decoded_rate,
														 void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate, unsigned int *flag)
{
	switch_status_t status;

	switch_assert(codec != NULL);
	switch_assert(encoded_data != NULL);
	switch_assert(decoded_data != NULL);

	if (!codec->implementation || !switch_core_codec_ready(codec)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_ENCODE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec encoder is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (codec->mutex) switch_mutex_lock(codec->mutex);
	status = codec->implementation->encode(codec, other_codec, decoded_data, decoded_data_len,
										   decoded_rate, encoded_data, encoded_data_len, encoded_rate, flag);
	if (codec->mutex) switch_mutex_unlock(codec->mutex);

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_core_codec_decode(switch_codec_t *codec,
														 switch_codec_t *other_codec,
														 void *encoded_data,
														 uint32_t encoded_data_len,
														 uint32_t encoded_rate,
														 void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate, unsigned int *flag)
{
	switch_status_t status;

	switch_assert(codec != NULL);
	switch_assert(encoded_data != NULL);
	switch_assert(decoded_data != NULL);

	if (!codec->implementation || !switch_core_codec_ready(codec)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decode Codec is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_DECODE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec decoder is not initialized!\n");
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (codec->implementation->encoded_bytes_per_packet) {
		uint32_t frames = encoded_data_len / codec->implementation->encoded_bytes_per_packet;

		if (frames && codec->implementation->decoded_bytes_per_packet * frames > *decoded_data_len) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Buffer size sanity check failed! edl:%u ebpp:%u fr:%u ddl:%u\n", 
							  encoded_data_len, codec->implementation->encoded_bytes_per_packet, frames, *decoded_data_len);
			*decoded_data_len = codec->implementation->decoded_bytes_per_packet;
			memset(decoded_data, 255, *decoded_data_len);
			return SWITCH_STATUS_SUCCESS;
		}
	}
	
	if (codec->mutex) switch_mutex_lock(codec->mutex);
	status = codec->implementation->decode(codec, other_codec, encoded_data, encoded_data_len, encoded_rate,
										   decoded_data, decoded_data_len, decoded_rate, flag);
	if (codec->mutex) switch_mutex_unlock(codec->mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_destroy(switch_codec_t *codec)
{
	switch_mutex_t *mutex = codec->mutex;
	switch_memory_pool_t *pool = codec->memory_pool;
	int free_pool = 0;

	switch_assert(codec != NULL);

	if (mutex) switch_mutex_lock(mutex);

	if (switch_core_codec_ready(codec)) {
		switch_clear_flag(codec, SWITCH_CODEC_FLAG_READY);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Codec is not initialized!\n");
		if (mutex) switch_mutex_unlock(mutex);
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (switch_test_flag(codec, SWITCH_CODEC_FLAG_FREE_POOL)) {
		free_pool = 1;
	}

	codec->implementation->destroy(codec);
	
	UNPROTECT_INTERFACE(codec->codec_interface);

	if (mutex) switch_mutex_unlock(mutex);
	
	if (free_pool) {
		switch_core_destroy_memory_pool(&pool);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
