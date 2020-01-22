/*
 * FreeSWITCH Moular Media Switching Software Library / Soft-Switch Application
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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * switch_core_io.c -- Main Core Library (Media I/O)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(void) switch_core_gen_encoded_silence(unsigned char *data, const switch_codec_implementation_t *read_impl, switch_size_t len)
{
	unsigned char g729_filler[] = {
		114, 170, 250, 103, 54, 211, 203, 194, 94, 64,
		229, 127, 79, 96, 207, 82, 216, 110, 245, 81,
		114, 170, 250, 103, 54, 211, 203, 194, 94, 64,
		229, 127, 79, 96, 207, 82, 216, 110, 245, 81,
		114, 170, 250, 103, 54, 211, 203, 194, 94, 64,
		229, 127, 79, 96, 207, 82, 216, 110, 245, 81,
		114, 170, 250, 103, 54, 211, 203, 194, 94, 64,
		229, 127, 79, 96, 207, 82, 216, 110, 245, 81,
		114, 170, 250, 103, 54, 211, 203, 194, 94, 64,
		229, 127, 79, 96, 207, 82, 216, 110, 245, 81,
		114, 170, 250, 103, 54, 211, 203, 194, 94, 64,
		229, 127, 79, 96, 207, 82, 216, 110, 245, 81,
		114, 170, 250, 103, 54, 211, 203, 194, 94, 64,
		229, 127, 79, 96, 207, 82, 216, 110, 245, 81
	};


	if (read_impl->ianacode == 18 || switch_stristr("g729", read_impl->iananame)) {
		memcpy(data, g729_filler, len);
	} else {
		memset(data, 255, len);
	}

}

SWITCH_DECLARE(switch_status_t) switch_core_session_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags,
															   int stream_id)
{
	switch_io_event_hook_read_frame_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int need_codec, perfect, do_bugs = 0, do_resample = 0, is_cng = 0, tap_only = 0;
	switch_codec_implementation_t codec_impl;
	unsigned int flag = 0;
	int i;

	switch_assert(session != NULL);

	tap_only = switch_test_flag(session, SSF_MEDIA_BUG_TAP_ONLY);

	switch_os_yield();

	if (switch_mutex_trylock(session->codec_read_mutex) == SWITCH_STATUS_SUCCESS) {
		switch_mutex_unlock(session->codec_read_mutex);
	} else {
		switch_cond_next();
		*frame = &runtime.dummy_cng_frame;
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(session->read_codec && session->read_codec->implementation && switch_core_codec_ready(session->read_codec))) {
		if (switch_channel_test_flag(session->channel, CF_PROXY_MODE) || switch_channel_get_state(session->channel) == CS_HIBERNATE) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "%s reading on a session with no media!\n",
							  switch_channel_get_name(session->channel));
			switch_cond_next();
			*frame = &runtime.dummy_cng_frame;
			return SWITCH_STATUS_SUCCESS;
		}

		if (switch_channel_test_flag(session->channel, CF_AUDIO_PAUSE_READ)) {
			switch_yield(20000);
			*frame = &runtime.dummy_cng_frame;
			// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Media Paused!!!!\n");
			return SWITCH_STATUS_SUCCESS;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s has no read codec.\n", switch_channel_get_name(session->channel));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(session->codec_read_mutex);

	if (!switch_core_codec_ready(session->read_codec)) {
		switch_mutex_unlock(session->codec_read_mutex);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s has no read codec.\n", switch_channel_get_name(session->channel));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		*frame = &runtime.dummy_cng_frame;
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(session->read_codec->mutex);

  top:

	for(i = 0; i < 2; i++) {
		if (session->dmachine[i]) {
			switch_channel_dtmf_lock(session->channel);
			switch_ivr_dmachine_ping(session->dmachine[i], NULL);
			switch_channel_dtmf_unlock(session->channel);
		}
	}

	if (switch_channel_down(session->channel) || !switch_core_codec_ready(session->read_codec)) {
		*frame = NULL;
		status = SWITCH_STATUS_FALSE;
		goto even_more_done;
	}


	status = SWITCH_STATUS_FALSE;
	need_codec = perfect = 0;

	*frame = NULL;

	if (session->read_codec && !session->track_id && session->track_duration) {
		if (session->read_frame_count == 0) {
			switch_event_t *event;
			switch_core_session_message_t msg = { 0 };

			session->read_frame_count = (session->read_impl.samples_per_second / session->read_impl.samples_per_packet) * session->track_duration;

			msg.message_id = SWITCH_MESSAGE_HEARTBEAT_EVENT;
			msg.numeric_arg = session->track_duration;
			switch_core_session_receive_message(session, &msg);

			switch_event_create(&event, SWITCH_EVENT_SESSION_HEARTBEAT);
			switch_channel_event_set_data(session->channel, event);
			switch_event_fire(&event);
		} else {
			session->read_frame_count--;
		}
	}


	if (switch_channel_test_flag(session->channel, CF_HOLD)) {
		switch_yield(session->read_impl.microseconds_per_packet);
		status = SWITCH_STATUS_BREAK;
		goto even_more_done;
	}

	if (session->endpoint_interface->io_routines->read_frame) {
		switch_mutex_unlock(session->read_codec->mutex);
		switch_mutex_unlock(session->codec_read_mutex);
		if ((status = session->endpoint_interface->io_routines->read_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.read_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->read_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}

		if (status == SWITCH_STATUS_INUSE) {
			*frame = &runtime.dummy_cng_frame;
			switch_yield(20000);
			return SWITCH_STATUS_SUCCESS;
		}

		if (!SWITCH_READ_ACCEPTABLE(status) || !session->read_codec || !switch_core_codec_ready(session->read_codec)) {
			*frame = NULL;
			return SWITCH_STATUS_FALSE;
		}

		switch_mutex_lock(session->codec_read_mutex);

		if (!switch_core_codec_ready(session->read_codec)) {
			switch_mutex_unlock(session->codec_read_mutex);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s has no read codec.\n", switch_channel_get_name(session->channel));
			switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
			*frame = &runtime.dummy_cng_frame;
			return SWITCH_STATUS_FALSE;
		}

		switch_mutex_lock(session->read_codec->mutex);
		if (!switch_core_codec_ready(session->read_codec)) {
			*frame = NULL;
			status = SWITCH_STATUS_FALSE;
			goto even_more_done;
		}
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	if (!(*frame)) {
		goto done;
	}

	switch_assert(*frame != NULL);

	if (switch_test_flag(*frame, SFF_PROXY_PACKET)) {
		/* Fast PASS! */
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

	switch_assert((*frame)->codec != NULL);

	if (!(session->read_codec && (*frame)->codec && (*frame)->codec->implementation) && switch_core_codec_ready((*frame)->codec)) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if (session->bugs && !((*frame)->flags & SFF_CNG) && !((*frame)->flags & SFF_NOT_AUDIO)) {
		switch_media_bug_t *bp;
		switch_bool_t ok = SWITCH_TRUE;
		int prune = 0;

		switch_thread_rwlock_rdlock(session->bug_rwlock);

		for (bp = session->bugs; bp; bp = bp->next) {
			ok = SWITCH_TRUE;

			if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
				continue;
			}

			if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
				continue;
			}
			if (switch_test_flag(bp, SMBF_PRUNE)) {
				prune++;
				continue;
			}

			if (bp->ready) {
				if (switch_test_flag(bp, SMBF_TAP_NATIVE_READ)) {
					if ((*frame)->codec && (*frame)->codec->implementation &&
						(*frame)->codec->implementation->encoded_bytes_per_packet &&
						(*frame)->datalen != (*frame)->codec->implementation->encoded_bytes_per_packet) {
						switch_set_flag((*frame), SFF_CNG);
						break;
					}
					if (bp->callback) {
						bp->native_read_frame = *frame;
						ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_TAP_NATIVE_READ);
						bp->native_read_frame = NULL;
					}
				}
			}

			if ((bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL)) || ok == SWITCH_FALSE) {
				switch_set_flag(bp, SMBF_PRUNE);
				prune++;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);

		if (prune) {
			switch_core_media_bug_prune(session);
		}
	}

	codec_impl = *(*frame)->codec->implementation;

	if (session->read_codec->implementation->impl_id != codec_impl.impl_id) {
		need_codec = TRUE;
		tap_only = 0;
	}

	if (codec_impl.actual_samples_per_second != session->read_impl.actual_samples_per_second) {
		do_resample = 1;
	}

	if (tap_only) {
		switch_media_bug_t *bp;
		switch_bool_t ok = SWITCH_TRUE;
		int prune = 0;

		if (session->bugs && switch_test_flag((*frame), SFF_CNG)) {
			switch_thread_rwlock_rdlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
				ok = SWITCH_TRUE;

				if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
					continue;
				}

				if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
					continue;
				}
				if (switch_test_flag(bp, SMBF_PRUNE)) {
					prune++;
					continue;
				}

				if (bp->ready && (*frame)->codec && (*frame)->codec->implementation && (*frame)->codec->implementation->encoded_bytes_per_packet) {
					if (switch_test_flag(bp, SMBF_TAP_NATIVE_READ)) {
						if (bp->callback) {
							switch_frame_t tmp_frame = {0};
							unsigned char data[SWITCH_RECOMMENDED_BUFFER_SIZE] = {0};


							tmp_frame.codec = (*frame)->codec;
							tmp_frame.datalen = (*frame)->codec->implementation->encoded_bytes_per_packet;
							tmp_frame.samples = (*frame)->codec->implementation->samples_per_packet;
							tmp_frame.channels = (*frame)->codec->implementation->number_of_channels;
							tmp_frame.data = data;

							switch_core_gen_encoded_silence(data, (*frame)->codec->implementation, tmp_frame.datalen);

							bp->native_read_frame = &tmp_frame;
							ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_TAP_NATIVE_READ);
							bp->native_read_frame = NULL;
						}
					}
				}

				if ((bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL)) || ok == SWITCH_FALSE) {
					switch_set_flag(bp, SMBF_PRUNE);
					prune++;
				}
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);

			if (prune) {
				switch_core_media_bug_prune(session);
			}


		}


		goto done;
	} else if (session->bugs && !need_codec) {
		do_bugs = 1;
		need_codec = 1;
	}

	if (switch_test_flag(*frame, SFF_CNG)) {
		if (!session->bugs && !session->plc) {
			/* Check if other session has bugs */
			unsigned int other_session_bugs = 0;
			switch_core_session_t *other_session = NULL;
			if (switch_channel_test_flag(switch_core_session_get_channel(session), CF_BRIDGED) &&
				switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
				if (other_session->bugs && !switch_test_flag(other_session, SSF_MEDIA_BUG_TAP_ONLY)) {
					other_session_bugs = 1;
				}
				switch_core_session_rwunlock(other_session);
			}

			/* Don't process CNG frame */
			if (!other_session_bugs) {
				status = SWITCH_STATUS_SUCCESS;
				goto done;
			}
		}
		is_cng = 1;
		need_codec = 1;
	} else if (switch_test_flag(*frame, SFF_NOT_AUDIO)) {
		do_resample = 0;
		do_bugs = 0;
		need_codec = 0;
	}

	if (switch_test_flag(session, SSF_READ_TRANSCODE) && !need_codec && switch_core_codec_ready(session->read_codec)) {
		switch_core_session_t *other_session;
		const char *uuid = switch_channel_get_partner_uuid(switch_core_session_get_channel(session));
		switch_clear_flag(session, SSF_READ_TRANSCODE);

		if (uuid && (other_session = switch_core_session_locate(uuid))) {
			switch_set_flag(other_session, SSF_READ_CODEC_RESET);
			switch_set_flag(other_session, SSF_READ_CODEC_RESET);
			switch_set_flag(other_session, SSF_WRITE_CODEC_RESET);
			switch_core_session_rwunlock(other_session);
		}
	}

	if (switch_test_flag(session, SSF_READ_CODEC_RESET)) {
		switch_core_codec_reset(session->read_codec);
		switch_clear_flag(session, SSF_READ_CODEC_RESET);
	}


	if (status == SWITCH_STATUS_SUCCESS && need_codec) {
		switch_frame_t *enc_frame, *read_frame = *frame;

		switch_set_flag(session, SSF_READ_TRANSCODE);

		if (!switch_test_flag(session, SSF_WARN_TRANSCODE)) {
			switch_core_session_message_t msg = { 0 };

			msg.message_id = SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY;
			switch_core_session_receive_message(session, &msg);
			switch_set_flag(session, SSF_WARN_TRANSCODE);
		}

		if (read_frame->codec || (is_cng && session->plc)) {
			session->raw_read_frame.datalen = session->raw_read_frame.buflen;

			if (is_cng) {
				if (session->plc) {
					plc_fillin(session->plc, session->raw_read_frame.data, read_frame->codec->implementation->decoded_bytes_per_packet / 2);
					is_cng = 0;
					flag &= ~SFF_CNG;
				} else {
					memset(session->raw_read_frame.data, 255, read_frame->codec->implementation->decoded_bytes_per_packet);
				}

				session->raw_read_frame.timestamp = 0;
				session->raw_read_frame.datalen = read_frame->codec->implementation->decoded_bytes_per_packet;
				session->raw_read_frame.samples = session->raw_read_frame.datalen / sizeof(int16_t) / session->read_impl.number_of_channels;
				session->raw_read_frame.channels = read_frame->codec->implementation->number_of_channels;
				read_frame = &session->raw_read_frame;
				status = SWITCH_STATUS_SUCCESS;
			} else {
				switch_codec_t *use_codec = read_frame->codec;
				if (do_bugs) {
					switch_thread_rwlock_wrlock(session->bug_rwlock);
					if (!session->bugs) {
						switch_thread_rwlock_unlock(session->bug_rwlock);
						goto done;
					}

					if (!switch_core_codec_ready(&session->bug_codec) && switch_core_codec_ready(read_frame->codec)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setting BUG Codec %s:%d\n",
										  read_frame->codec->implementation->iananame, read_frame->codec->implementation->ianacode);
						switch_core_codec_copy(read_frame->codec, &session->bug_codec, NULL, NULL);
						if (!switch_core_codec_ready(&session->bug_codec)) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s Error setting BUG codec %s!\n",
											  switch_core_session_get_name(session), read_frame->codec->implementation->iananame);
						}
					}

					if (switch_core_codec_ready(&session->bug_codec)) {
						use_codec = &session->bug_codec;
					}
					switch_thread_rwlock_unlock(session->bug_rwlock);

					switch_thread_rwlock_wrlock(session->bug_rwlock);
					if (!session->bugs) {
						do_bugs = 0;
					}
					switch_thread_rwlock_unlock(session->bug_rwlock);
					if (!do_bugs) goto done;
				}

				if (!switch_test_flag(read_frame->codec, SWITCH_CODEC_FLAG_HAS_PLC) &&
					(switch_channel_test_flag(session->channel, CF_JITTERBUFFER_PLC) ||
					 switch_channel_test_flag(session->channel, CF_CNG_PLC)) && !session->plc) {
					session->plc = plc_init(NULL);
				}

				if (!switch_test_flag(read_frame->codec, SWITCH_CODEC_FLAG_HAS_PLC) && session->plc && switch_test_flag(read_frame, SFF_PLC)) {
					session->raw_read_frame.datalen = read_frame->codec->implementation->decoded_bytes_per_packet;
					session->raw_read_frame.samples = session->raw_read_frame.datalen / sizeof(int16_t) / session->read_impl.number_of_channels;
					session->raw_read_frame.channels = session->read_impl.number_of_channels;
					memset(session->raw_read_frame.data, 255, session->raw_read_frame.datalen);
					status = SWITCH_STATUS_SUCCESS;
				} else {
					switch_codec_t *codec = use_codec;

					switch_thread_rwlock_rdlock(session->bug_rwlock);

					if (!switch_core_codec_ready(codec)) {
						codec = read_frame->codec;
					}

					if (!switch_core_codec_ready(codec)) {
						switch_thread_rwlock_unlock(session->bug_rwlock);
						goto done;
					}

					codec->cur_frame = read_frame;
					session->read_codec->cur_frame = read_frame;
					status = switch_core_codec_decode(codec,
													  session->read_codec,
													  read_frame->data,
													  read_frame->datalen,
													  session->read_impl.actual_samples_per_second,
													  session->raw_read_frame.data, &session->raw_read_frame.datalen, &session->raw_read_frame.rate,
													  &read_frame->flags);

					if (status == SWITCH_STATUS_NOT_INITALIZED) {
						switch_thread_rwlock_unlock(session->bug_rwlock);
						goto done;
					}

					session->raw_read_frame.samples = session->raw_read_frame.datalen / 2;
					session->raw_read_frame.channels = codec->implementation->number_of_channels;
					codec->cur_frame = NULL;
					session->read_codec->cur_frame = NULL;
					switch_thread_rwlock_unlock(session->bug_rwlock);

				}

				if (status == SWITCH_STATUS_SUCCESS && session->read_impl.number_of_channels == 1) {
					if (session->plc) {
						if (switch_test_flag(read_frame, SFF_PLC)) {
							plc_fillin(session->plc, session->raw_read_frame.data, session->raw_read_frame.datalen / 2);
							switch_clear_flag(read_frame, SFF_PLC);
						} else {
							plc_rx(session->plc, session->raw_read_frame.data, session->raw_read_frame.datalen / 2);
						}
					}
				}


			}

			if (do_resample && ((status == SWITCH_STATUS_SUCCESS) || is_cng)) {
				status = SWITCH_STATUS_RESAMPLE;
			}

			/* mux or demux to match */
			if (session->raw_read_frame.channels != session->read_impl.number_of_channels) {
				uint32_t rlen = session->raw_read_frame.datalen / 2 / session->raw_read_frame.channels;
				switch_mux_channels((int16_t *) session->raw_read_frame.data, rlen, session->raw_read_frame.channels, session->read_impl.number_of_channels);
				session->raw_read_frame.datalen = rlen * 2 * session->read_impl.number_of_channels;
				session->raw_read_frame.samples = session->raw_read_frame.datalen / 2;
				session->raw_read_frame.channels = session->read_impl.number_of_channels;
			}


			switch (status) {
			case SWITCH_STATUS_RESAMPLE:
				if (!session->read_resampler) {
					switch_mutex_lock(session->resample_mutex);

					status = switch_resample_create(&session->read_resampler,
													read_frame->codec->implementation->actual_samples_per_second,
													session->read_impl.actual_samples_per_second,
													session->read_impl.decoded_bytes_per_packet, SWITCH_RESAMPLE_QUALITY,
													session->read_impl.number_of_channels);

					switch_mutex_unlock(session->resample_mutex);

					if (status != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Unable to allocate resampler\n");
						status = SWITCH_STATUS_FALSE;
						goto done;
					} else {
						switch_core_session_message_t msg = { 0 };
						msg.numeric_arg = 1;
						msg.message_id = SWITCH_MESSAGE_RESAMPLE_EVENT;
						switch_core_session_receive_message(session, &msg);

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Activating read resampler\n");
					}
				}
			case SWITCH_STATUS_SUCCESS:
				session->raw_read_frame.samples = session->raw_read_frame.datalen / sizeof(int16_t) / session->read_impl.number_of_channels;
				session->raw_read_frame.channels = session->read_impl.number_of_channels;
				session->raw_read_frame.rate = read_frame->rate;
				if (read_frame->codec->implementation->samples_per_packet != session->read_impl.samples_per_packet) {
					session->raw_read_frame.timestamp = 0;
				} else {
					session->raw_read_frame.timestamp = read_frame->timestamp;
				}
				session->raw_read_frame.ssrc = read_frame->ssrc;
				session->raw_read_frame.seq = read_frame->seq;
				session->raw_read_frame.m = read_frame->m;
				session->raw_read_frame.payload = read_frame->payload;
				session->raw_read_frame.flags = 0;
				if (switch_test_flag(read_frame, SFF_PLC)) {
					session->raw_read_frame.flags |= SFF_PLC;
				}
				read_frame = &session->raw_read_frame;
				break;
			case SWITCH_STATUS_NOOP:
				if (session->read_resampler) {
					switch_mutex_lock(session->resample_mutex);
					switch_resample_destroy(&session->read_resampler);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Deactivating read resampler\n");
					switch_mutex_unlock(session->resample_mutex);

					{
						switch_core_session_message_t msg = { 0 };
						msg.numeric_arg = 0;
						msg.message_id = SWITCH_MESSAGE_RESAMPLE_EVENT;
						switch_core_session_receive_message(session, &msg);
					}

				}

				status = SWITCH_STATUS_SUCCESS;
				break;
			case SWITCH_STATUS_BREAK:
				memset(session->raw_read_frame.data, 255, read_frame->codec->implementation->decoded_bytes_per_packet);
				session->raw_read_frame.datalen = read_frame->codec->implementation->decoded_bytes_per_packet;
				session->raw_read_frame.samples = session->raw_read_frame.datalen / sizeof(int16_t) / session->read_impl.number_of_channels;
				session->raw_read_frame.channels = session->read_impl.number_of_channels;
				session->raw_read_frame.timestamp = read_frame->timestamp;
				session->raw_read_frame.rate = read_frame->rate;
				session->raw_read_frame.ssrc = read_frame->ssrc;
				session->raw_read_frame.seq = read_frame->seq;
				session->raw_read_frame.m = read_frame->m;
				session->raw_read_frame.payload = read_frame->payload;
				session->raw_read_frame.flags = 0;
				if (switch_test_flag(read_frame, SFF_PLC)) {
					session->raw_read_frame.flags |= SFF_PLC;
				}

				read_frame = &session->raw_read_frame;
				status = SWITCH_STATUS_SUCCESS;
				break;
			case SWITCH_STATUS_NOT_INITALIZED:
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec init error!\n");
				goto done;
			default:
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec %s decoder error! [%d]\n",
								  session->read_codec->codec_interface->interface_name, status);

				if (++session->decoder_errors < 10) {
					status = SWITCH_STATUS_SUCCESS;
				} else {
					goto done;
				}
			}

			session->decoder_errors = 0;
		}

		if (session->bugs) {
			switch_media_bug_t *bp;
			switch_bool_t ok = SWITCH_TRUE;
			int prune = 0;
			switch_thread_rwlock_rdlock(session->bug_rwlock);

			for (bp = session->bugs; bp; bp = bp->next) {
				ok = SWITCH_TRUE;

				if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
					continue;
				}

				if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
					continue;
				}

				if (!switch_channel_test_flag(session->channel, CF_BRIDGED) && switch_core_media_bug_test_flag(bp, SMBF_BRIDGE_REQ)) {
					continue;
				}

				if (switch_test_flag(bp, SMBF_PRUNE)) {
					prune++;
					continue;
				}

				if (switch_test_flag(bp, SMBF_READ_REPLACE)) {
					do_bugs = 0;
					if (bp->callback) {
						bp->read_replace_frame_in = read_frame;
						bp->read_replace_frame_out = read_frame;
						bp->read_demux_frame = NULL;
						if ((ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ_REPLACE)) == SWITCH_TRUE) {
							read_frame = bp->read_replace_frame_out;
						}
					}
				}

				if ((bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL)) || ok == SWITCH_FALSE) {
					switch_set_flag(bp, SMBF_PRUNE);
					prune++;
				}


			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
			if (prune) {
				switch_core_media_bug_prune(session);
			}
		}

		if (session->bugs) {
			switch_media_bug_t *bp;
			switch_bool_t ok = SWITCH_TRUE;
			int prune = 0;
			switch_thread_rwlock_rdlock(session->bug_rwlock);

			for (bp = session->bugs; bp; bp = bp->next) {
				ok = SWITCH_TRUE;

				if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
					continue;
				}

				if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
					continue;
				}

				if (!switch_channel_test_flag(session->channel, CF_BRIDGED) && switch_core_media_bug_test_flag(bp, SMBF_BRIDGE_REQ)) {
					continue;
				}

				if (switch_test_flag(bp, SMBF_PRUNE)) {
					prune++;
					continue;
				}

				if (bp->ready && switch_test_flag(bp, SMBF_READ_STREAM)) {
					switch_mutex_lock(bp->read_mutex);
					if (bp->read_demux_frame) {
						uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
						int bytes = read_frame->datalen;
						uint32_t datalen = 0;
						uint32_t samples = bytes / 2 / bp->read_demux_frame->channels;

						memcpy(data, read_frame->data, read_frame->datalen);
						datalen = switch_unmerge_sln((int16_t *)data, samples,
													 bp->read_demux_frame->data, samples,
													 bp->read_demux_frame->channels) * 2 * bp->read_demux_frame->channels;

						switch_buffer_write(bp->raw_read_buffer, data, datalen);
					} else {
						switch_buffer_write(bp->raw_read_buffer, read_frame->data, read_frame->datalen);
					}

					if (bp->callback) {
						ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ);
					}
					switch_mutex_unlock(bp->read_mutex);
				}

				if ((bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL)) || ok == SWITCH_FALSE) {
					switch_set_flag(bp, SMBF_PRUNE);
					prune++;
				}
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
			if (prune) {
				switch_core_media_bug_prune(session);
			}
		}

		if (do_bugs) {
			goto done;
		}

		if (session->read_codec) {
			if (session->read_resampler) {
				short *data = read_frame->data;
				switch_mutex_lock(session->resample_mutex);
				switch_resample_process(session->read_resampler, data, (int) read_frame->datalen / 2 / session->read_resampler->channels);
				memcpy(data, session->read_resampler->to, session->read_resampler->to_len * 2 * session->read_resampler->channels);
				read_frame->samples = session->read_resampler->to_len;
				read_frame->channels = session->read_resampler->channels;
				read_frame->datalen = session->read_resampler->to_len * 2 * session->read_resampler->channels;
				read_frame->rate = session->read_resampler->to_rate;
				switch_mutex_unlock(session->resample_mutex);
			}

			if (read_frame->datalen == session->read_impl.decoded_bytes_per_packet) {
				perfect = TRUE;
			} else {
				if (!session->raw_read_buffer) {
					switch_size_t bytes = session->read_impl.decoded_bytes_per_packet;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Engaging Read Buffer at %u bytes vs %u\n",
									  (uint32_t) bytes, (uint32_t) (*frame)->datalen);
					switch_buffer_create_dynamic(&session->raw_read_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, 0);
				}

				if (read_frame->datalen && (!switch_buffer_write(session->raw_read_buffer, read_frame->data, read_frame->datalen))) {
					status = SWITCH_STATUS_MEMERR;
					goto done;
				}
			}

			if (perfect || switch_buffer_inuse(session->raw_read_buffer) >= session->read_impl.decoded_bytes_per_packet) {
				if (perfect) {
					enc_frame = read_frame;
					session->raw_read_frame.rate = read_frame->rate;
				} else {
					session->raw_read_frame.datalen = (uint32_t) switch_buffer_read(session->raw_read_buffer,
																					session->raw_read_frame.data,
																					session->read_impl.decoded_bytes_per_packet);

					session->raw_read_frame.rate = session->read_impl.actual_samples_per_second;
					enc_frame = &session->raw_read_frame;
				}
				session->enc_read_frame.datalen = session->enc_read_frame.buflen;

				switch_assert(session->read_codec != NULL);
				switch_assert(enc_frame != NULL);
				switch_assert(enc_frame->data != NULL);
				session->read_codec->cur_frame = enc_frame;
				enc_frame->codec->cur_frame = enc_frame;
				switch_assert(enc_frame->datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);
				switch_assert(session->enc_read_frame.datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);
				status = switch_core_codec_encode(session->read_codec,
												  enc_frame->codec,
												  enc_frame->data,
												  enc_frame->datalen,
												  session->read_impl.actual_samples_per_second,
												  session->enc_read_frame.data, &session->enc_read_frame.datalen, &session->enc_read_frame.rate, &flag);
				switch_assert(session->enc_read_frame.datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);

				session->read_codec->cur_frame = NULL;
				enc_frame->codec->cur_frame = NULL;
				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Fixme 1\n");
				case SWITCH_STATUS_SUCCESS:
					session->enc_read_frame.samples = session->read_impl.decoded_bytes_per_packet / sizeof(int16_t) / session->read_impl.number_of_channels;
					session->enc_read_frame.channels = session->read_impl.number_of_channels;
					if (perfect) {
						if (enc_frame->codec->implementation->samples_per_packet != session->read_impl.samples_per_packet) {
							session->enc_read_frame.timestamp = 0;
						} else {
							session->enc_read_frame.timestamp = read_frame->timestamp;
						}
						session->enc_read_frame.rate = read_frame->rate;
						session->enc_read_frame.ssrc = read_frame->ssrc;
						session->enc_read_frame.seq = read_frame->seq;
						session->enc_read_frame.m = read_frame->m;
						session->enc_read_frame.payload = session->read_impl.ianacode;
					}
					*frame = &session->enc_read_frame;
					break;
				case SWITCH_STATUS_NOOP:
					session->raw_read_frame.samples = enc_frame->codec->implementation->samples_per_packet;
					session->raw_read_frame.channels = enc_frame->codec->implementation->number_of_channels;
					session->raw_read_frame.timestamp = read_frame->timestamp;
					session->raw_read_frame.payload = enc_frame->codec->implementation->ianacode;
					session->raw_read_frame.m = read_frame->m;
					session->raw_read_frame.ssrc = read_frame->ssrc;
					session->raw_read_frame.seq = read_frame->seq;
					*frame = enc_frame;
					status = SWITCH_STATUS_SUCCESS;
					break;
				case SWITCH_STATUS_NOT_INITALIZED:
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec init error!\n");
					*frame = NULL;
					status = SWITCH_STATUS_GENERR;
					break;
				default:
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec %s encoder error!\n",
									  session->read_codec->codec_interface->interface_name);
					*frame = NULL;
					status = SWITCH_STATUS_GENERR;
					break;
				}
			} else {
				goto top;
			}
		}
	}

  done:
	if (!(*frame)) {
		status = SWITCH_STATUS_FALSE;
	} else {
		if (flag & SFF_CNG) {
			switch_set_flag((*frame), SFF_CNG);
		}
		if (session->bugs) {
			switch_media_bug_t *bp;
			switch_bool_t ok = SWITCH_TRUE;
			int prune = 0;
			switch_thread_rwlock_rdlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
				ok = SWITCH_TRUE;

				if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
					continue;
				}

				if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
					continue;
				}

				if (!switch_channel_test_flag(session->channel, CF_BRIDGED) && switch_core_media_bug_test_flag(bp, SMBF_BRIDGE_REQ)) {
					continue;
				}

				if (switch_test_flag(bp, SMBF_PRUNE)) {
					prune++;
					continue;
				}

				if (bp->ready && switch_test_flag(bp, SMBF_READ_PING)) {
					switch_mutex_lock(bp->read_mutex);
					bp->ping_frame = *frame;
					if (bp->callback) {
						if (bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ_PING) == SWITCH_FALSE
							|| (bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL))) {
							ok = SWITCH_FALSE;
						}
					}
					bp->ping_frame = NULL;;
					switch_mutex_unlock(bp->read_mutex);
				}

				if (ok == SWITCH_FALSE) {
					switch_set_flag(bp, SMBF_PRUNE);
					prune++;
				}
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
			if (prune) {
				switch_core_media_bug_prune(session);
			}
		}
	}

  even_more_done:

	if (!*frame ||
			(!switch_test_flag(*frame, SFF_PROXY_PACKET) &&
			 (!(*frame)->codec || !(*frame)->codec->implementation || !switch_core_codec_ready((*frame)->codec)))) {
		*frame = &runtime.dummy_cng_frame;
	}

	switch_mutex_unlock(session->read_codec->mutex);
	switch_mutex_unlock(session->codec_read_mutex);


	if (status == SWITCH_STATUS_SUCCESS && switch_channel_get_callstate(session->channel) == CCS_UNHELD) {
		switch_channel_set_callstate(session->channel, CCS_ACTIVE);
	}


	return status;
}

static char *SIG_NAMES[] = {
	"NONE",
	"KILL",
	"XFER",
	"BREAK",
	NULL
};

SWITCH_DECLARE(switch_status_t) switch_core_session_perform_kill_channel(switch_core_session_t *session,
																		 const char *file, const char *func, int line, switch_signal_t sig)
{
	switch_io_event_hook_kill_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, switch_core_session_get_uuid(session), SWITCH_LOG_DEBUG10, "Send signal %s [%s]\n",
					  switch_channel_get_name(session->channel), SIG_NAMES[sig]);

	if (session->endpoint_interface->io_routines->kill_channel) {
		if ((status = session->endpoint_interface->io_routines->kill_channel(session, sig)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.kill_channel; ptr; ptr = ptr->next) {
				if ((status = ptr->kill_channel(session, sig)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_recv_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	switch_io_event_hook_recv_dtmf_t *ptr;
	switch_status_t status;
	switch_dtmf_t new_dtmf;
	int fed = 0;

	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(dtmf);

	if (switch_test_flag(dtmf, DTMF_FLAG_SENSITIVE)) {
		return SWITCH_STATUS_SUCCESS;
	}

	new_dtmf = *dtmf;

	if (new_dtmf.duration > switch_core_max_dtmf_duration(0)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s EXCESSIVE DTMF DIGIT [%c] LEN [%d]\n",
						  switch_channel_get_name(session->channel), new_dtmf.digit, new_dtmf.duration);
		new_dtmf.duration = switch_core_max_dtmf_duration(0);
	} else if (new_dtmf.duration < switch_core_min_dtmf_duration(0)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s SHORT DTMF DIGIT [%c] LEN [%d]\n",
						  switch_channel_get_name(session->channel), new_dtmf.digit, new_dtmf.duration);
		new_dtmf.duration = switch_core_min_dtmf_duration(0);
	} else if (!new_dtmf.duration) {
		new_dtmf.duration = switch_core_default_dtmf_duration(0);
	}

	if (!switch_test_flag(dtmf, DTMF_FLAG_SKIP_PROCESS)) {
		if (session->dmachine[0]) {
			char str[2] = { dtmf->digit, '\0' };
			switch_ivr_dmachine_feed(session->dmachine[0], str, NULL);
			fed = 1;
		}

		for (ptr = session->event_hooks.recv_dtmf; ptr; ptr = ptr->next) {
			if ((status = ptr->recv_dtmf(session, &new_dtmf, SWITCH_DTMF_RECV)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
		}
	}

	return fed ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	switch_io_event_hook_send_dtmf_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_dtmf_t new_dtmf;

	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(dtmf, DTMF_FLAG_SENSITIVE)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_channel_test_flag(session->channel, CF_DROP_DTMF)) {
		const char *file = switch_channel_get_variable_dup(session->channel, "drop_dtmf_masking_file", SWITCH_FALSE, -1);
		const char *digits;

		if (!zstr(file)) {
			switch_ivr_broadcast(switch_core_session_get_uuid(session), file, SMF_ECHO_ALEG);
		}

		if ((digits = switch_channel_get_variable_dup(session->channel, "drop_dtmf_masking_digits", SWITCH_FALSE, -1)) && !zstr(digits)) {
			const char *p;
			switch_dtmf_t x_dtmf = { 0, switch_core_default_dtmf_duration(0), DTMF_FLAG_SKIP_PROCESS, 0};

			switch_channel_clear_flag(session->channel, CF_DROP_DTMF);
			for(p = digits; p && *p; p++) {
				if (is_dtmf(*p)) {
					x_dtmf.digit = *p;
					switch_core_session_send_dtmf(session, &x_dtmf);
				}
			}
			switch_channel_set_flag(session->channel, CF_DROP_DTMF);
		}

		return SWITCH_STATUS_SUCCESS;
	}

	switch_assert(dtmf);

	new_dtmf = *dtmf;

	if (new_dtmf.digit != 'w' && new_dtmf.digit != 'W') {
		if (new_dtmf.duration > switch_core_max_dtmf_duration(0)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s EXCESSIVE DTMF DIGIT [%c] LEN [%d]\n",
							  switch_channel_get_name(session->channel), new_dtmf.digit, new_dtmf.duration);
			new_dtmf.duration = switch_core_max_dtmf_duration(0);
		} else if (new_dtmf.duration < switch_core_min_dtmf_duration(0)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s SHORT DTMF DIGIT [%c] LEN [%d]\n",
						  switch_channel_get_name(session->channel), new_dtmf.digit, new_dtmf.duration);
			new_dtmf.duration = switch_core_min_dtmf_duration(0);
		}
	}

	if (!new_dtmf.duration) {
		new_dtmf.duration = switch_core_default_dtmf_duration(0);
	}

	if (!switch_test_flag(dtmf, DTMF_FLAG_SKIP_PROCESS)) {
		for (ptr = session->event_hooks.send_dtmf; ptr; ptr = ptr->next) {
			if ((status = ptr->send_dtmf(session, dtmf, SWITCH_DTMF_SEND)) != SWITCH_STATUS_SUCCESS) {
				return SWITCH_STATUS_SUCCESS;
			}
		}
		if (session->dmachine[1]) {
			char str[2] = { new_dtmf.digit, '\0' };
			switch_ivr_dmachine_feed(session->dmachine[1], str, NULL);
			return SWITCH_STATUS_SUCCESS;
		}
	}


	if (session->endpoint_interface->io_routines->send_dtmf) {
		int send = 0;
		status = SWITCH_STATUS_SUCCESS;

		if (switch_channel_test_cap(session->channel, CC_QUEUEABLE_DTMF_DELAY) && (dtmf->digit == 'w' || dtmf->digit == 'W')) {
			send = 1;
		} else {
			if (dtmf->digit == 'w') {
				switch_yield(500000);
			} else if (dtmf->digit == 'W') {
				switch_yield(1000000);
			} else {
				send = 1;
			}
		}

		if (send) {
			status = session->endpoint_interface->io_routines->send_dtmf(session, &new_dtmf);
		}
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_send_dtmf_string(switch_core_session_t *session, const char *dtmf_string)
{
	char *p;
	switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0), DTMF_FLAG_SKIP_PROCESS, 0};
	int sent = 0, dur;
	char *string;
	int i, argc;
	char *argv[256];
	int dur_total = 0;
	int rate_mult = 8;

	switch_codec_implementation_t write_impl = { 0 };

	switch_assert(session != NULL);

	if (zstr(dtmf_string)) {
		return SWITCH_STATUS_FALSE;
	}

	if (*dtmf_string == '~') {
		dtmf_string++;
		dtmf.flags = 0;
	}

	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}


	if (strlen(dtmf_string) > 99) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Attempt to send very large dtmf string ignored!\n");
		return SWITCH_STATUS_FALSE;
	}

	string = switch_core_session_strdup(session, dtmf_string);
	argc = switch_separate_string(string, '+', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc) {
		switch_channel_pre_answer(session->channel);
	}

	switch_core_session_get_write_impl(session, &write_impl);

	if (write_impl.actual_samples_per_second >= 1000) {
		rate_mult = (write_impl.actual_samples_per_second / 1000);
	} 

	for (i = 0; i < argc; i++) {
		dtmf.duration = switch_core_default_dtmf_duration(0);
		dur = switch_core_default_dtmf_duration(0) / rate_mult;
		if ((p = strchr(argv[i], '@'))) {
			*p++ = '\0';
			if ((dur = atoi(p)) > (int)switch_core_min_dtmf_duration(0) / rate_mult) {
				dtmf.duration = dur * rate_mult;
			}
		}


		for (p = argv[i]; p && *p; p++) {
			if (is_dtmf(*p)) {
				dtmf.digit = *p;

				if (dtmf.digit != 'w' && dtmf.digit != 'W') {
					if (dtmf.duration > switch_core_max_dtmf_duration(0)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s EXCESSIVE DTMF DIGIT [%c] LEN [%d]\n",
										  switch_channel_get_name(session->channel), dtmf.digit, dtmf.duration);
						dtmf.duration = switch_core_max_dtmf_duration(0);
					} else if (dtmf.duration < switch_core_min_dtmf_duration(0)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s SHORT DTMF DIGIT [%c] LEN [%d]\n",
										  switch_channel_get_name(session->channel), dtmf.digit, dtmf.duration);
						dtmf.duration = switch_core_min_dtmf_duration(0);
					}
				}

				if (!dtmf.duration) {
					dtmf.duration = switch_core_default_dtmf_duration(0);
				}


				if (switch_core_session_send_dtmf(session, &dtmf) == SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s send dtmf\ndigit=%c ms=%u samples=%u\n",
									  switch_channel_get_name(session->channel), dtmf.digit, dur, dtmf.duration);
					sent++;
					dur_total += dtmf.duration + (250 * rate_mult);	/* account for 250ms pause */
				}
			}
		}

		if (dur_total) {
			char tmp[32] = "";
			switch_snprintf(tmp, sizeof(tmp), "%d", dur_total / rate_mult);
			switch_channel_set_variable(session->channel, "last_dtmf_duration", tmp);
		}

	}
	return sent ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
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
