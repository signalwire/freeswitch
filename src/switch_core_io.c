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
 * switch_core_io.c -- Main Core Library (Media I/O)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(switch_status_t) switch_core_session_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags,
																	  int stream_id)
{
	switch_io_event_hook_video_write_frame_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	if (session->endpoint_interface->io_routines->write_video_frame) {
		if ((status = session->endpoint_interface->io_routines->write_video_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.video_write_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->video_write_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags,
																	 int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_io_event_hook_video_read_frame_t *ptr;

	switch_assert(session != NULL);

	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	if (session->endpoint_interface->io_routines->read_video_frame) {
		if ((status = session->endpoint_interface->io_routines->read_video_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.video_read_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->video_read_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	if (!(*frame)) {
		goto done;
	}

	switch_assert(*frame != NULL);

	if (switch_test_flag(*frame, SFF_CNG)) {
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

  done:

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags,
															   int stream_id)
{
	switch_io_event_hook_read_frame_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int need_codec, perfect, do_bugs = 0, do_resample = 0, is_cng = 0;
	unsigned int flag = 0;

	switch_assert(session != NULL);

	if (!(session->read_codec && session->read_codec->implementation)) {
		if (switch_channel_test_flag(session->channel, CF_PROXY_MODE) || switch_channel_get_state(session->channel) == CS_HIBERNATE) {
			*frame = &runtime.dummy_cng_frame;
			return SWITCH_STATUS_SUCCESS;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s has no read codec.\n", switch_channel_get_name(session->channel));
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(session->codec_read_mutex);
	switch_mutex_lock(session->read_codec->mutex);

  top:

	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		*frame = NULL;
		status = SWITCH_STATUS_FALSE; goto even_more_done;
	}


	status = SWITCH_STATUS_FALSE;
	need_codec = perfect = 0;

	*frame = NULL;

	if (session->read_codec && !session->track_id && session->track_duration) {
		if (session->read_frame_count == 0) {
			switch_event_t *event;
			session->read_frame_count = (session->read_codec->implementation->actual_samples_per_second / 
										 session->read_codec->implementation->samples_per_packet) * session->track_duration;

			switch_event_create(&event, SWITCH_EVENT_SESSION_HEARTBEAT);
			switch_channel_event_set_data(session->channel, event);
			switch_event_fire(&event);
		} else {
			session->read_frame_count--;
		}
	}


	if (switch_channel_test_flag(session->channel, CF_HOLD)) {
		switch_yield(session->read_codec->implementation->microseconds_per_packet);
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
		
		if (!session->read_codec) {
			*frame = NULL;
			return SWITCH_STATUS_FALSE;
		}
		
		switch_mutex_lock(session->codec_read_mutex);
		switch_mutex_lock(session->read_codec->mutex);
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

	if (switch_test_flag(*frame, SFF_CNG)) {
		status = SWITCH_STATUS_SUCCESS;
		if (!session->bugs) {
			goto done;
		}
		is_cng = 1;
	}

	switch_assert((*frame)->codec != NULL);


	if (((*frame)->codec && session->read_codec->implementation != (*frame)->codec->implementation)) {
		need_codec = TRUE;
	}

	if (session->read_codec && !(*frame)->codec) {
		need_codec = TRUE;
	}

	if (!session->read_codec && (*frame)->codec) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((*frame)->codec->implementation->actual_samples_per_second != session->read_codec->implementation->actual_samples_per_second) {
		do_resample = 1;
	}

	if (session->bugs && !need_codec) {
		do_bugs = 1;
		need_codec = 1;
	}

	if (status == SWITCH_STATUS_SUCCESS && need_codec) {
		switch_frame_t *enc_frame, *read_frame = *frame;

		if (!switch_test_flag(session, SSF_WARN_TRANSCODE)) {
			switch_core_session_message_t msg = { 0 };

			msg.message_id = SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY;
			switch_core_session_receive_message(session, &msg);
			switch_set_flag(session, SSF_WARN_TRANSCODE);
		}

		if (read_frame->codec || is_cng) {
			session->raw_read_frame.datalen = session->raw_read_frame.buflen;

			if (is_cng) {
				memset(session->raw_read_frame.data, 255, read_frame->codec->implementation->decoded_bytes_per_packet);
				session->raw_read_frame.datalen = read_frame->codec->implementation->decoded_bytes_per_packet;
				session->raw_read_frame.samples = session->raw_read_frame.datalen / sizeof(int16_t);
				read_frame = &session->raw_read_frame;
				status = SWITCH_STATUS_SUCCESS;
			} else {
				switch_codec_t *use_codec = read_frame->codec;
				if (do_bugs) {
					if (!session->bug_codec.implementation) {
						switch_core_codec_copy(read_frame->codec, &session->bug_codec, switch_core_session_get_pool(session));
					}
					use_codec = &session->bug_codec;
				}

				status = switch_core_codec_decode(use_codec,
												  session->read_codec,
												  read_frame->data,
												  read_frame->datalen,
												  session->read_codec->implementation->actual_samples_per_second,
												  session->raw_read_frame.data, &session->raw_read_frame.datalen, &session->raw_read_frame.rate, &flag);
			}

			if (do_resample && ((status == SWITCH_STATUS_SUCCESS) || is_cng)) {
				status = SWITCH_STATUS_RESAMPLE;
			}

			switch (status) {
			case SWITCH_STATUS_RESAMPLE:
				if (!session->read_resampler) {
					switch_mutex_lock(session->resample_mutex);
					status = switch_resample_create(&session->read_resampler,
													read_frame->codec->implementation->actual_samples_per_second,
													read_frame->codec->implementation->decoded_bytes_per_packet,
													session->read_codec->implementation->actual_samples_per_second,
													session->read_codec->implementation->decoded_bytes_per_packet, session->pool);
					switch_mutex_unlock(session->resample_mutex);

					if (status != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to allocate resampler\n");
						status = SWITCH_STATUS_FALSE;
						goto done;
					}
				}
			case SWITCH_STATUS_SUCCESS:
				session->raw_read_frame.samples = session->raw_read_frame.datalen / sizeof(int16_t);
				session->raw_read_frame.rate = read_frame->rate;
				if (read_frame->codec->implementation->samples_per_packet != session->read_codec->implementation->samples_per_packet) {
					session->raw_read_frame.timestamp = 0;
				} else {
					session->raw_read_frame.timestamp = read_frame->timestamp;
				}
				session->raw_read_frame.ssrc = read_frame->ssrc;
				session->raw_read_frame.seq = read_frame->seq;
				session->raw_read_frame.m = read_frame->m;
				session->raw_read_frame.payload = read_frame->payload;
				read_frame = &session->raw_read_frame;
				break;
			case SWITCH_STATUS_NOOP:
				if (session->read_resampler) {
					switch_mutex_lock(session->resample_mutex);
					switch_resample_destroy(&session->read_resampler);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deactivating read resampler\n");
					switch_mutex_unlock(session->resample_mutex);
				}

				status = SWITCH_STATUS_SUCCESS;
				break;
			case SWITCH_STATUS_BREAK:
				memset(session->raw_read_frame.data, 255, read_frame->codec->implementation->decoded_bytes_per_packet);
				session->raw_read_frame.datalen = read_frame->codec->implementation->decoded_bytes_per_packet;
				session->raw_read_frame.samples = session->raw_read_frame.datalen / sizeof(int16_t);
				session->raw_read_frame.timestamp = read_frame->timestamp;
				session->raw_read_frame.rate = read_frame->rate;
				session->raw_read_frame.ssrc = read_frame->ssrc;
				session->raw_read_frame.seq = read_frame->seq;
				session->raw_read_frame.m = read_frame->m;
				session->raw_read_frame.payload = read_frame->payload;
				read_frame = &session->raw_read_frame;
				status = SWITCH_STATUS_SUCCESS;
				break;
			case SWITCH_STATUS_NOT_INITALIZED:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec init error!\n");
				goto done;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s decoder error!\n", session->read_codec->codec_interface->interface_name);
				goto done;
			}
		}

		if (session->bugs) {
			switch_media_bug_t *bp, *dp, *last = NULL;
			switch_bool_t ok = SWITCH_TRUE;
			switch_thread_rwlock_rdlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
				if (bp->ready && switch_test_flag(bp, SMBF_READ_STREAM)) {
					switch_mutex_lock(bp->read_mutex);
					switch_buffer_write(bp->raw_read_buffer, read_frame->data, read_frame->datalen);
					if (bp->callback) {
						ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ);
					}
					switch_mutex_unlock(bp->read_mutex);
				}
				
				if (ok && switch_test_flag(bp, SMBF_READ_REPLACE)) {
					do_bugs = 0;
					if (bp->callback) {
						bp->read_replace_frame_in = read_frame;
						bp->read_replace_frame_out = read_frame;
						if ((ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ_REPLACE)) == SWITCH_TRUE) {
							read_frame = bp->read_replace_frame_out;
						}
					}
				}
				
				if (bp->stop_time && bp->stop_time <= switch_timestamp(NULL)) {
					ok = SWITCH_FALSE;
				}

				if (ok == SWITCH_FALSE) {
					bp->ready = 0;
					if (last) {
						last->next = bp->next;
					} else {
						session->bugs = bp->next;
					}
					dp = bp;
					bp = last;
					switch_core_media_bug_close(&dp);
					if (!bp) {
						break;
					}
					continue;
				}
				last = bp;
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
		}

		if (do_bugs) {
			goto done;
		}

		if (session->read_codec) {
			if (session->read_resampler) {
				short *data = read_frame->data;
				switch_mutex_lock(session->resample_mutex);

				session->read_resampler->from_len = switch_short_to_float(data, session->read_resampler->from, (int) read_frame->datalen / 2);
				session->read_resampler->to_len =
					switch_resample_process(session->read_resampler, session->read_resampler->from,
											session->read_resampler->from_len, session->read_resampler->to, session->read_resampler->to_size, 0);
				switch_float_to_short(session->read_resampler->to, data, read_frame->datalen);
				read_frame->samples = session->read_resampler->to_len;
				read_frame->datalen = session->read_resampler->to_len * 2;
				read_frame->rate = session->read_resampler->to_rate;
				switch_mutex_unlock(session->resample_mutex);

			}

			if (read_frame->datalen == session->read_codec->implementation->decoded_bytes_per_packet) {
				perfect = TRUE;
			} else {
				if (!session->raw_read_buffer) {
					switch_size_t bytes = session->read_codec->implementation->decoded_bytes_per_packet;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Engaging Read Buffer at %u bytes vs %u\n", 
									  (uint32_t) bytes, (uint32_t) (*frame)->datalen);
					switch_buffer_create_dynamic(&session->raw_read_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, 0);
				}
				if (!switch_buffer_write(session->raw_read_buffer, read_frame->data, read_frame->datalen)) {
					status = SWITCH_STATUS_MEMERR;
					goto done;
				}
			}


			if (perfect || switch_buffer_inuse(session->raw_read_buffer) >= session->read_codec->implementation->decoded_bytes_per_packet) {
				if (perfect) {
					enc_frame = read_frame;
					session->raw_read_frame.rate = read_frame->rate;
				} else {
					session->raw_read_frame.datalen = (uint32_t) switch_buffer_read(session->raw_read_buffer,
																					session->raw_read_frame.data,
																					session->read_codec->implementation->decoded_bytes_per_packet);

					session->raw_read_frame.rate = session->read_codec->implementation->actual_samples_per_second;
					enc_frame = &session->raw_read_frame;
				}
				session->enc_read_frame.datalen = session->enc_read_frame.buflen;

				switch_assert(session->read_codec != NULL);
				switch_assert(enc_frame != NULL);
				switch_assert(enc_frame->data != NULL);

				status = switch_core_codec_encode(session->read_codec,
												  enc_frame->codec,
												  enc_frame->data,
												  enc_frame->datalen,
												  session->read_codec->implementation->actual_samples_per_second,
												  session->enc_read_frame.data, &session->enc_read_frame.datalen, &session->enc_read_frame.rate, &flag);

				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Fixme 1\n");
				case SWITCH_STATUS_SUCCESS:
					session->enc_read_frame.samples = session->read_codec->implementation->decoded_bytes_per_packet / sizeof(int16_t);
					if (perfect) {
						if (enc_frame->codec->implementation->samples_per_packet != session->read_codec->implementation->samples_per_packet) {
							session->enc_read_frame.timestamp = 0;
						} else {
							session->enc_read_frame.timestamp = read_frame->timestamp;
						}
						session->enc_read_frame.rate = read_frame->rate;
						session->enc_read_frame.ssrc = read_frame->ssrc;
						session->enc_read_frame.seq = read_frame->seq;
						session->enc_read_frame.m = read_frame->m;
						session->enc_read_frame.payload = session->read_codec->implementation->ianacode;
					}
					*frame = &session->enc_read_frame;
					break;
				case SWITCH_STATUS_NOOP:
					session->raw_read_frame.samples = enc_frame->codec->implementation->samples_per_packet;
					session->raw_read_frame.timestamp = read_frame->timestamp;
					session->raw_read_frame.payload = enc_frame->codec->implementation->ianacode;
					session->raw_read_frame.m = read_frame->m;
					session->raw_read_frame.ssrc = read_frame->ssrc;
					session->raw_read_frame.seq = read_frame->seq;
					*frame = &session->raw_read_frame;
					status = SWITCH_STATUS_SUCCESS;
					break;
				case SWITCH_STATUS_NOT_INITALIZED:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec init error!\n");
					*frame = NULL;
                    status = SWITCH_STATUS_GENERR;
                    break;
				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s encoder error!\n",
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
			switch_media_bug_t *bp, *dp, *last = NULL;
			switch_bool_t ok = SWITCH_TRUE;
			switch_thread_rwlock_rdlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
				if (bp->ready && switch_test_flag(bp, SMBF_READ_PING)) {
					switch_mutex_lock(bp->read_mutex);
					if (bp->callback) {
						if (bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ_PING) == SWITCH_FALSE
							|| (bp->stop_time && bp->stop_time <= switch_timestamp(NULL))) {
							ok = SWITCH_FALSE;
						}
					}
					switch_mutex_unlock(bp->read_mutex);
				}

				if (ok == SWITCH_FALSE) {
					bp->ready = 0;
					if (last) {
						last->next = bp->next;
					} else {
						session->bugs = bp->next;
					}
					dp = bp;
					bp = last;
					switch_core_media_bug_close(&dp);
					if (!bp) {
						break;
					}
					continue;
				}
				last = bp;
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
		}
	}

  even_more_done:

	if (!*frame || !(*frame)->codec || !(*frame)->codec->implementation) {
		*frame = &runtime.dummy_cng_frame;
	}

	switch_mutex_unlock(session->read_codec->mutex);
	switch_mutex_unlock(session->codec_read_mutex);

	return status;
}

static switch_status_t perform_write(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_io_event_hook_write_frame_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->write_frame) {

		if ((status = session->endpoint_interface->io_routines->write_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.write_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->write_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags,
																int stream_id)
{

	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_frame_t *enc_frame = NULL, *write_frame = frame;
	unsigned int flag = 0, need_codec = 0, perfect = 0, do_bugs = 0, do_write = 0, do_resample = 0, ptime_mismatch = 0, pass_cng = 0;
	
	switch_assert(session != NULL);
	switch_assert(frame != NULL);

	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(frame, SFF_CNG)) {
		if (switch_channel_test_flag(session->channel, CF_ACCEPT_CNG)) {
			pass_cng = 1;
		}
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!(session->write_codec && session->write_codec->implementation) && !pass_cng) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s has no write codec.\n", switch_channel_get_name(session->channel));
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_test_flag(session->channel, CF_HOLD)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(frame, SFF_PROXY_PACKET) || pass_cng) {
		/* Fast PASS! */
		return perform_write(session, frame, flag, stream_id);
	}

	switch_assert(frame->codec != NULL);
	switch_assert(frame->codec->implementation != NULL);

	if (!(session->write_codec && frame->codec)) {
		return SWITCH_STATUS_FALSE;
	}
	switch_mutex_lock(session->write_codec->mutex);
	switch_mutex_lock(frame->codec->mutex);
	switch_mutex_lock(session->codec_write_mutex);	

	if ((session->write_codec && frame->codec && session->write_codec->implementation != frame->codec->implementation)) {
		need_codec = TRUE;
		if (session->write_codec->implementation->codec_id == frame->codec->implementation->codec_id) {
			ptime_mismatch = TRUE;
		}
	}

	if (session->write_codec && !frame->codec) {
		need_codec = TRUE;
	}

	if (session->bugs && !need_codec) {
		do_bugs = TRUE;
		need_codec = TRUE;
	}

	if (frame->codec->implementation->actual_samples_per_second != session->write_codec->implementation->actual_samples_per_second) {
		need_codec = TRUE;
		do_resample = TRUE;
	}

	if (need_codec) {
		if (!switch_test_flag(session, SSF_WARN_TRANSCODE)) {
			switch_core_session_message_t msg = { 0 };

			msg.message_id = SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY;
			switch_core_session_receive_message(session, &msg);
			switch_set_flag(session, SSF_WARN_TRANSCODE);
		}

		if (frame->codec) {
			session->raw_write_frame.datalen = session->raw_write_frame.buflen;
			status = switch_core_codec_decode(frame->codec,
											  session->write_codec,
											  frame->data,
											  frame->datalen,
											  session->write_codec->implementation->actual_samples_per_second,
											  session->raw_write_frame.data, &session->raw_write_frame.datalen, &session->raw_write_frame.rate, &flag);




			if (do_resample && status == SWITCH_STATUS_SUCCESS) {
				status = SWITCH_STATUS_RESAMPLE;
			}

			switch (status) {
			case SWITCH_STATUS_RESAMPLE:
				write_frame = &session->raw_write_frame;
				if (!session->write_resampler) {
					switch_mutex_lock(session->resample_mutex);
					status = switch_resample_create(&session->write_resampler,
													frame->codec->implementation->actual_samples_per_second,
													frame->codec->implementation->decoded_bytes_per_packet,
													session->write_codec->implementation->actual_samples_per_second,
													session->write_codec->implementation->decoded_bytes_per_packet, session->pool);
					switch_mutex_unlock(session->resample_mutex);
					if (status != SWITCH_STATUS_SUCCESS) {
						goto done;
					}
				}
				break;
			case SWITCH_STATUS_SUCCESS:
				session->raw_write_frame.samples = session->raw_write_frame.datalen / sizeof(int16_t);
				session->raw_write_frame.timestamp = frame->timestamp;
				session->raw_write_frame.rate = frame->rate;
				session->raw_write_frame.m = frame->m;
				session->raw_write_frame.ssrc = frame->ssrc;
				session->raw_write_frame.seq = frame->seq;
				session->raw_write_frame.payload = frame->payload;
				write_frame = &session->raw_write_frame;
				break;
			case SWITCH_STATUS_BREAK:
				status = SWITCH_STATUS_SUCCESS; goto error;
			case SWITCH_STATUS_NOOP:
				if (session->write_resampler) {
					switch_mutex_lock(session->resample_mutex);
					switch_resample_destroy(&session->write_resampler);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deactivating write resampler\n");
					switch_mutex_unlock(session->resample_mutex);
				}
				write_frame = frame;
				status = SWITCH_STATUS_SUCCESS;
				break;
			default:
				if (status == SWITCH_STATUS_NOT_INITALIZED) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec init error!\n");
					goto error;
				}
				if (ptime_mismatch) {
					status = perform_write(session, frame, flags, stream_id);
					status = SWITCH_STATUS_SUCCESS; goto error;
				}
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s decoder error!\n", frame->codec->codec_interface->interface_name);
				goto error;
			}
		}

		if (session->write_resampler) {
			short *data = write_frame->data;

			switch_mutex_lock(session->resample_mutex);

			session->write_resampler->from_len = write_frame->datalen / 2;
			switch_short_to_float(data, session->write_resampler->from, session->write_resampler->from_len);

			session->write_resampler->to_len = (uint32_t)
				switch_resample_process(session->write_resampler, session->write_resampler->from,
										session->write_resampler->from_len, session->write_resampler->to, session->write_resampler->to_size, 0);

			switch_float_to_short(session->write_resampler->to, data, session->write_resampler->to_len);

			write_frame->samples = session->write_resampler->to_len;
			write_frame->datalen = write_frame->samples * 2;
			write_frame->rate = session->write_resampler->to_rate;
			switch_mutex_unlock(session->resample_mutex);
		}

		if (session->bugs) {
			switch_media_bug_t *bp, *dp, *last = NULL;

			switch_thread_rwlock_rdlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
				switch_bool_t ok = SWITCH_TRUE;
				if (!bp->ready) {
					continue;
				}
				if (switch_test_flag(bp, SMBF_WRITE_STREAM)) {

					switch_mutex_lock(bp->write_mutex);
					switch_buffer_write(bp->raw_write_buffer, write_frame->data, write_frame->datalen);
					switch_mutex_unlock(bp->write_mutex);
					if (bp->callback) {
						ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_WRITE);
					}
				}

				if (switch_test_flag(bp, SMBF_WRITE_REPLACE)) {
					do_bugs = 0;
					if (bp->callback) {
						bp->write_replace_frame_in = write_frame;
						bp->write_replace_frame_out = write_frame;
						if ((ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_WRITE_REPLACE)) == SWITCH_TRUE) {
							write_frame = bp->write_replace_frame_out;
						}
					}
				}

				if (bp->stop_time && bp->stop_time <= switch_timestamp(NULL)) {
					ok = SWITCH_FALSE;
				}


				if (ok == SWITCH_FALSE) {
					bp->ready = 0;
					if (last) {
						last->next = bp->next;
					} else {
						session->bugs = bp->next;
					}
					dp = bp;
					bp = last;
					switch_core_media_bug_close(&dp);
					if (!bp) {
						break;
					}
					continue;
				}
				last = bp;
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
		}

		if (do_bugs) {
			do_write = TRUE;
			write_frame = frame;
			goto done;
		}

		if (session->write_codec) {
			if (write_frame->datalen == session->write_codec->implementation->decoded_bytes_per_packet) {
				perfect = TRUE;
			} else {
				if (!session->raw_write_buffer) {
					switch_size_t bytes = session->write_codec->implementation->decoded_bytes_per_packet;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "Engaging Write Buffer at %u bytes to accommodate %u->%u\n",
									  (uint32_t) bytes, write_frame->datalen, session->write_codec->implementation->decoded_bytes_per_packet);
					if ((status = switch_buffer_create_dynamic(&session->raw_write_buffer,
															   bytes * SWITCH_BUFFER_BLOCK_FRAMES,
															   bytes * SWITCH_BUFFER_START_FRAMES, 0)) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write Buffer Failed!\n");
						goto error;
					}
				}

				if (!(switch_buffer_write(session->raw_write_buffer, write_frame->data, write_frame->datalen))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write Buffer %u bytes Failed!\n", write_frame->datalen);
					status = SWITCH_STATUS_MEMERR; goto error;
				}
			}

			if (perfect) {
				enc_frame = write_frame;
				session->enc_write_frame.datalen = session->enc_write_frame.buflen;

				status = switch_core_codec_encode(session->write_codec,
												  frame->codec,
												  enc_frame->data,
												  enc_frame->datalen,
												  session->write_codec->implementation->actual_samples_per_second,
												  session->enc_write_frame.data, &session->enc_write_frame.datalen, &session->enc_write_frame.rate, &flag);

				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Fixme 2\n"); */
				case SWITCH_STATUS_SUCCESS:
					session->enc_write_frame.codec = session->write_codec;
					session->enc_write_frame.samples = enc_frame->datalen / sizeof(int16_t);
					if (frame->codec->implementation->samples_per_packet != session->write_codec->implementation->samples_per_packet) {
						session->enc_write_frame.timestamp = 0;
					} else {
						session->enc_write_frame.timestamp = frame->timestamp;
					}
					session->enc_write_frame.payload = session->write_codec->implementation->ianacode;
					session->enc_write_frame.m = frame->m;
					session->enc_write_frame.ssrc = frame->ssrc;
					session->enc_write_frame.seq = frame->seq;
					write_frame = &session->enc_write_frame;
					break;
				case SWITCH_STATUS_NOOP:
					enc_frame->codec = session->write_codec;
					enc_frame->samples = enc_frame->datalen / sizeof(int16_t);
					enc_frame->timestamp = frame->timestamp;
					enc_frame->m = frame->m;
					enc_frame->seq = frame->seq;
					enc_frame->ssrc = frame->ssrc;
					enc_frame->payload = enc_frame->codec->implementation->ianacode;
					write_frame = enc_frame;
					status = SWITCH_STATUS_SUCCESS;
					break;
				case SWITCH_STATUS_NOT_INITALIZED:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec init error!\n");
					write_frame = NULL;
                    goto error;					
				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s encoder error!\n",
									  session->read_codec->codec_interface->interface_name);
					write_frame = NULL;
					goto error;
				}
				if (flag & SFF_CNG) {
					switch_set_flag(write_frame, SFF_CNG);
				}
				status = perform_write(session, write_frame, flags, stream_id);
				goto error;
			} else {
				switch_size_t used = switch_buffer_inuse(session->raw_write_buffer);
				uint32_t bytes = session->write_codec->implementation->decoded_bytes_per_packet;
				switch_size_t frames = (used / bytes);

				status = SWITCH_STATUS_SUCCESS;
				if (!frames) {
					goto error;
				} else {
					switch_size_t x;
					for (x = 0; x < frames; x++) {
						if ((session->raw_write_frame.datalen = (uint32_t)
							 switch_buffer_read(session->raw_write_buffer, session->raw_write_frame.data, bytes)) != 0) {
							int rate;
							enc_frame = &session->raw_write_frame;
							session->raw_write_frame.rate = session->write_codec->implementation->actual_samples_per_second;
							session->enc_write_frame.datalen = session->enc_write_frame.buflen;

							if (frame->codec && frame->codec->implementation) {
								rate = frame->codec->implementation->actual_samples_per_second;
							} else {
								rate = session->write_codec->implementation->actual_samples_per_second;
							}

							status = switch_core_codec_encode(session->write_codec,
															  frame->codec,
															  enc_frame->data,
															  enc_frame->datalen,
															  rate,
															  session->enc_write_frame.data,
															  &session->enc_write_frame.datalen, &session->enc_write_frame.rate, &flag);
							switch (status) {
							case SWITCH_STATUS_RESAMPLE:
								session->enc_write_frame.codec = session->write_codec;
								session->enc_write_frame.samples = enc_frame->datalen / sizeof(int16_t);
								session->enc_write_frame.m = frame->m;
								session->enc_write_frame.ssrc = frame->ssrc;
								session->enc_write_frame.payload = session->write_codec->implementation->ianacode;
								write_frame = &session->enc_write_frame;
								if (!session->write_resampler) {
									switch_mutex_lock(session->resample_mutex);
									status = switch_resample_create(&session->write_resampler,
																	frame->codec->implementation->actual_samples_per_second,
																	frame->codec->implementation->decoded_bytes_per_packet,
																	session->write_codec->implementation->actual_samples_per_second,
																	session->write_codec->implementation->decoded_bytes_per_packet, session->pool);
									switch_mutex_unlock(session->resample_mutex);

									if (status != SWITCH_STATUS_SUCCESS) {
										goto done;
									}
								}
								break;
							case SWITCH_STATUS_SUCCESS:
								session->enc_write_frame.codec = session->write_codec;
								session->enc_write_frame.samples = enc_frame->datalen / sizeof(int16_t);
								session->enc_write_frame.m = frame->m;
								session->enc_write_frame.ssrc = frame->ssrc;
								session->enc_write_frame.payload = session->write_codec->implementation->ianacode;
								write_frame = &session->enc_write_frame;
								break;
							case SWITCH_STATUS_NOOP:
								if (session->write_resampler) {
									switch_mutex_lock(session->resample_mutex);
									switch_resample_destroy(&session->write_resampler);
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deactivating write resampler\n");
									switch_mutex_unlock(session->resample_mutex);
								}
								enc_frame->codec = session->write_codec;
								enc_frame->samples = enc_frame->datalen / sizeof(int16_t);
								enc_frame->m = frame->m;
								enc_frame->ssrc = frame->ssrc;
								enc_frame->payload = enc_frame->codec->implementation->ianacode;
								write_frame = enc_frame;
								status = SWITCH_STATUS_SUCCESS;
								break;
							case SWITCH_STATUS_NOT_INITALIZED:
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec init error!\n");
								write_frame = NULL;
								goto error;
							default:
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s encoder error %d!\n",
												  session->read_codec->codec_interface->interface_name, status);
								write_frame = NULL;
								goto error;
							}

							if (session->read_resampler) {
								short *data = write_frame->data;
								switch_mutex_lock(session->resample_mutex);

								session->read_resampler->from_len =
									switch_short_to_float(data, session->read_resampler->from, (int) write_frame->datalen / 2);
								session->read_resampler->to_len = (uint32_t)
									switch_resample_process(session->read_resampler, session->read_resampler->from,
															session->read_resampler->from_len,
															session->read_resampler->to, session->read_resampler->to_size, 0);
								switch_float_to_short(session->read_resampler->to, data, write_frame->datalen * 2);
								write_frame->samples = session->read_resampler->to_len;
								write_frame->datalen = session->read_resampler->to_len * 2;
								write_frame->rate = session->read_resampler->to_rate;
								switch_mutex_unlock(session->resample_mutex);

							}
							if (flag & SFF_CNG) {
								switch_set_flag(write_frame, SFF_CNG);
							}

							if ((status = perform_write(session, write_frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
								break;
							}
						}
					}
					goto error;
				}
			}
		}
	} else {
		do_write = TRUE;
	}

 done:

	if (do_write) {
		status = perform_write(session, frame, flags, stream_id);
	}
	
 error:

	switch_mutex_unlock(session->write_codec->mutex);
	switch_mutex_unlock(frame->codec->mutex);
	switch_mutex_unlock(session->codec_write_mutex);

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

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG, "Send signal %s [%s]\n", switch_channel_get_name(session->channel),
					  SIG_NAMES[sig]);

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

	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(dtmf);

	new_dtmf = *dtmf;

	if (new_dtmf.duration > switch_core_max_dtmf_duration(0)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s EXCESSIVE DTMF DIGIT [%c] LEN [%d]\n",
						  switch_channel_get_name(session->channel), new_dtmf.digit, new_dtmf.duration);
		new_dtmf.duration = switch_core_max_dtmf_duration(0);
	} else if (!new_dtmf.duration) {
		new_dtmf.duration = switch_core_default_dtmf_duration(0);
	}

	for (ptr = session->event_hooks.recv_dtmf; ptr; ptr = ptr->next) {
		if ((status = ptr->recv_dtmf(session, &new_dtmf, SWITCH_DTMF_RECV)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	switch_io_event_hook_send_dtmf_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_dtmf_t new_dtmf;

	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(dtmf);

	new_dtmf = *dtmf;

	if (new_dtmf.duration > switch_core_max_dtmf_duration(0)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s EXCESSIVE DTMF DIGIT [%c] LEN [%d]\n",
						  switch_channel_get_name(session->channel), new_dtmf.digit, new_dtmf.duration);
		new_dtmf.duration = switch_core_max_dtmf_duration(0);
	} else if (!new_dtmf.duration) {
		new_dtmf.duration = switch_core_default_dtmf_duration(0);
	}


	for (ptr = session->event_hooks.send_dtmf; ptr; ptr = ptr->next) {
		if ((status = ptr->send_dtmf(session, dtmf, SWITCH_DTMF_SEND)) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (session->endpoint_interface->io_routines->send_dtmf) {
		if (dtmf->digit == 'w') {
			switch_yield(500000);
		} else if (dtmf->digit == 'W') {
			switch_yield(1000000);
		} else {
			status = session->endpoint_interface->io_routines->send_dtmf(session, &new_dtmf);
		}
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_send_dtmf_string(switch_core_session_t *session, const char *dtmf_string)
{
	char *p;
	switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0) };
	int sent = 0, dur;
	char *string;
	int i, argc;
	char *argv[256];
	int dur_total = 0;

	switch_assert(session != NULL);


	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(dtmf_string)) {
		return SWITCH_STATUS_FALSE;
	}

	if (strlen(dtmf_string) > 99) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Attempt to send very large dtmf string ignored!\n");
		return SWITCH_STATUS_FALSE;
	}

	string = switch_core_session_strdup(session, dtmf_string);
	argc = switch_separate_string(string, '+', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc) {
		switch_channel_pre_answer(session->channel);
	}

	for (i = 0; i < argc; i++) {
		dtmf.duration = switch_core_default_dtmf_duration(0);
		dur = switch_core_default_dtmf_duration(0) / 8;
		if ((p = strchr(argv[i], '@'))) {
			*p++ = '\0';
			if ((dur = atoi(p)) > 50) {
				dtmf.duration = dur * 8;
			}
		}


		if (dtmf.duration > switch_core_max_dtmf_duration(0)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s EXCESSIVE DTMF DIGIT [%c] LEN [%d]\n",
							  switch_channel_get_name(session->channel), dtmf.digit, dtmf.duration);
			dtmf.duration = switch_core_max_dtmf_duration(0);
		} else if (!dtmf.duration) {
			dtmf.duration = switch_core_default_dtmf_duration(0);
		}

		for (p = argv[i]; p && *p; p++) {
			if (is_dtmf(*p)) {
				dtmf.digit = *p;
				if (switch_core_session_send_dtmf(session, &dtmf) == SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s send dtmf\ndigit=%c ms=%u samples=%u\n",
									  switch_channel_get_name(session->channel), dtmf.digit, dur, dtmf.duration);
					sent++;
					dur_total += dtmf.duration + 2000; /* account for 250ms pause */
				}
			}
		}

		if (dur_total) {
			char tmp[32] = "";
			switch_snprintf(tmp, sizeof(tmp), "%d", dur_total / 8);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
