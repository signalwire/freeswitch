/*
 * Copyright 2009-2010 Tomas Valenta, Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * $Id: mpf_dtmf_generator.c 1474 2010-02-07 20:51:47Z achaloyan $
 */

#include "mpf_dtmf_generator.h"
#include "apr.h"
#include "apr_thread_mutex.h"
#include "apt_log.h"
#include "mpf_named_event.h"
#include <math.h>

#ifndef M_PI
#	define M_PI 3.141592653589793238462643
#endif

/** Max DTMF digits waiting to be sent */
#define MPF_DTMFGEN_QUEUE_LEN  32

/** See RFC4733 */
#define DTMF_EVENT_ID_MAX      15  /* 0123456789*#ABCD */

/** See RFC4733 */
#define DTMF_EVENT_VOLUME      10

/** Amplitude of single sine wave from tone generator */
#define DTMF_SINE_AMPLITUDE 12288

/** State of the DTMF generator */
typedef enum mpf_dtmf_generator_state_e {
	/** Ready to generate next digit in queue */
	DTMF_GEN_STATE_IDLE,
	/** Generating tone */
	DTMF_GEN_STATE_TONE,
	/** Retransmitting final RTP packet */
	DTMF_GEN_STATE_ENDING,
	/** Generating silence between tones */
	DTMF_GEN_STATE_SILENCE
} mpf_dtmf_generator_state_e;

/**
 * Sine wave generator (second-order IIR filter) state:
 *
 * s(t) = Amp*sin(2*pi*f_tone/f_sampling*t)
 *
 * s(t) = coef * s(t-1) - s(t-2); s(0)=0; s(1)=Amp*sin(2*pi*f_tone/f_sampling)
 */
typedef struct sine_state_t {
	/** coef = cos(2*pi*f_tone/f_sampling) */
	double coef;
	/** s(t-2) @see sine_state_t */
	double s1;
	/** s(t-1) @see sine_state_t */
	double s2;
} sine_state_t;

/** Mapping event_it to frequency pair */
static const double dtmf_freq[DTMF_EVENT_ID_MAX+1][2] = {
	{941, 1336},  /* 0 */
	{697, 1209},  /* 1 */
	{697, 1336},  /* 2 */
	{697, 1477},  /* 3 */
	{770, 1209},  /* 4 */
	{770, 1336},  /* 5 */
	{770, 1477},  /* 6 */
	{852, 1209},  /* 7 */
	{852, 1336},  /* 8 */
	{852, 1477},  /* 9 */
	{941, 1209},  /* * */
	{941, 1477},  /* # */
	{697, 1633},  /* A */
	{770, 1633},  /* B */
	{852, 1633},  /* C */
	{941, 1633}   /* D */
};

/** Media Processing Framework's Dual Tone Multiple Frequncy generator */
struct mpf_dtmf_generator_t {
	/** Generator state */
	enum mpf_dtmf_generator_state_e  state;
	/** In-band or out-of-band */
	enum mpf_dtmf_generator_band_e   band;
	/** Mutex to guard the queue */
	struct apr_thread_mutex_t       *mutex;
	/** Queue of digits to generate */
	char                             queue[MPF_DTMFGEN_QUEUE_LEN+1];
	/** DTMF event_id according to RFC4733 */
	apr_byte_t                       event_id;
	/** Duration in RTP units: (sample_rate / 1000) * milliseconds */
	apr_uint32_t                     tone_duration;
	/** Duration of inter-digit silence @see tone_duration */
	apr_uint32_t                     silence_duration;
	/** Multipurpose counter; mostly in RTP time units */
	apr_uint32_t                     counter;
	/** Frame duration in RTP units */
	apr_uint32_t                     frame_duration;
	/** RTP named event duration (0..0xFFFF) */
	apr_uint32_t                     event_duration;
	/** Set MPF_MARKER_NEW_SEGMENT in the next event frame */
	apt_bool_t                       new_segment;
	/** Lower frequency generator */
	struct sine_state_t              sine1;
	/** Higher frequency generator */
	struct sine_state_t              sine2;
	/** Sampling rate of audio in Hz; used in tone generator */
	apr_uint32_t                     sample_rate_audio;
	/** Sampling rate of telephone-events in Hz; used for timing */
	apr_uint32_t                     sample_rate_events;
	/** How often to issue event packet */
	apr_uint32_t                     events_ptime;
	/** Milliseconds elapsed since last event packet */
	apr_uint32_t                     since_last_event;
};


MPF_DECLARE(struct mpf_dtmf_generator_t *) mpf_dtmf_generator_create_ex(
								const struct mpf_audio_stream_t *stream,
								enum mpf_dtmf_generator_band_e band,
								apr_uint32_t tone_ms,
								apr_uint32_t silence_ms,
								struct apr_pool_t *pool)
{
	struct mpf_dtmf_generator_t *gen;
	apr_status_t status;
	int flg_band = band;

	if (!stream->rx_descriptor) flg_band &= ~MPF_DTMF_GENERATOR_INBAND;
	if (!stream->rx_event_descriptor) flg_band &= ~MPF_DTMF_GENERATOR_OUTBAND;
	if (!flg_band) return NULL;

	gen = apr_palloc(pool, sizeof(struct mpf_dtmf_generator_t));
	if (!gen) return NULL;
	status = apr_thread_mutex_create(&gen->mutex, APR_THREAD_MUTEX_DEFAULT, pool);
	if (status != APR_SUCCESS) return NULL;
	gen->band = (enum mpf_dtmf_generator_band_e) flg_band;
	gen->queue[0] = 0;
	gen->state = DTMF_GEN_STATE_IDLE;
	if (stream->rx_descriptor)
		gen->sample_rate_audio = stream->rx_descriptor->sampling_rate;
	gen->sample_rate_events = stream->rx_event_descriptor ?
		stream->rx_event_descriptor->sampling_rate : gen->sample_rate_audio;
	gen->frame_duration = gen->sample_rate_events / 1000 * CODEC_FRAME_TIME_BASE;
	gen->tone_duration = gen->sample_rate_events / 1000 * tone_ms;
	gen->silence_duration = gen->sample_rate_events / 1000 * silence_ms;
	gen->events_ptime = CODEC_FRAME_TIME_BASE;  /* Should be got from event_descriptor */
	return gen;
}


MPF_DECLARE(apt_bool_t) mpf_dtmf_generator_enqueue(
								struct mpf_dtmf_generator_t *generator,
								const char *digits)
{
	apr_size_t qlen, dlen;
	apt_bool_t ret;

	dlen = strlen(digits);
	apr_thread_mutex_lock(generator->mutex);
	qlen = strlen(generator->queue);
	if (qlen + dlen > MPF_DTMFGEN_QUEUE_LEN) {
		ret = FALSE;
		apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "DTMF queue too short (%"APR_SIZE_T_FMT"), "
			"cannot add %d digit%s, already has %"APR_SIZE_T_FMT, MPF_DTMFGEN_QUEUE_LEN,
			dlen, dlen > 1 ? "s" : "", qlen);
	} else {
		strcpy(generator->queue + qlen, digits);
		ret = TRUE;
	}
	apr_thread_mutex_unlock(generator->mutex);
	return ret;
}


MPF_DECLARE(void) mpf_dtmf_generator_reset(struct mpf_dtmf_generator_t *generator)
{
	apr_thread_mutex_lock(generator->mutex);
	generator->state = DTMF_GEN_STATE_IDLE;
	generator->queue[0] = 0;
	apr_thread_mutex_unlock(generator->mutex);
}


MPF_DECLARE(apt_bool_t) mpf_dtmf_generator_sending(const struct mpf_dtmf_generator_t *generator)
{
	return *generator->queue || ((generator->state != DTMF_GEN_STATE_IDLE) &&
		(generator->state != DTMF_GEN_STATE_SILENCE));
}


MPF_DECLARE(apt_bool_t) mpf_dtmf_generator_put_frame(
								struct mpf_dtmf_generator_t *generator,
								struct mpf_frame_t *frame)
{
	apr_thread_mutex_lock(generator->mutex);
	if ((generator->state == DTMF_GEN_STATE_IDLE) && *generator->queue) {
		/* Get next valid digit from queue */
		do {
			generator->event_id = (apr_byte_t) mpf_dtmf_char_to_event_id(*generator->queue);
            /* This used to be a strcpy(), but that can give overlapping buffer issues */
			memmove(generator->queue, &generator->queue[1], strlen(&generator->queue[1]) + 1);
		} while (*generator->queue && (generator->event_id > DTMF_EVENT_ID_MAX));
		/* Reset state */
		if (generator->event_id <= DTMF_EVENT_ID_MAX) {
			generator->state = DTMF_GEN_STATE_TONE;
			generator->counter = 0;
			generator->event_duration = 0;
			generator->since_last_event = generator->events_ptime;
			generator->new_segment = FALSE;
			/* Initialize tone generator */
			if (generator->band & MPF_DTMF_GENERATOR_INBAND) {
				double omega;

				omega = 2 * M_PI * dtmf_freq[generator->event_id][0] / generator->sample_rate_audio;
				generator->sine1.s1 = 0;
				generator->sine1.s2 = DTMF_SINE_AMPLITUDE * sin(omega);
				generator->sine1.coef = 2 * cos(omega);

				omega = 2 * M_PI * dtmf_freq[generator->event_id][1] / generator->sample_rate_audio;
				generator->sine2.s1 = 0;
				generator->sine2.s2 = DTMF_SINE_AMPLITUDE * sin(omega);
				generator->sine2.coef = 2 * cos(omega);
			}
		}
	}
	apr_thread_mutex_unlock(generator->mutex);
	if (generator->state == DTMF_GEN_STATE_IDLE) return FALSE;

	if (generator->state == DTMF_GEN_STATE_TONE) {
		generator->counter += generator->frame_duration;
		generator->event_duration += generator->frame_duration;
		if (generator->band & MPF_DTMF_GENERATOR_INBAND) {
			apr_size_t i;
			apr_int16_t *samples = (apr_int16_t *) frame->codec_frame.buffer;
			double s;

			frame->type |= MEDIA_FRAME_TYPE_AUDIO;
			/* Tone generator */
			for (i = 0; i < frame->codec_frame.size / 2; i++) {
				s = generator->sine1.s1;
				generator->sine1.s1 = generator->sine1.s2;
				generator->sine1.s2 = generator->sine1.coef * generator->sine1.s1 - s;
				samples[i] = (apr_int16_t) (s + generator->sine2.s1);
				s = generator->sine2.s1;
				generator->sine2.s1 = generator->sine2.s2;
				generator->sine2.s2 = generator->sine2.coef * generator->sine2.s1 - s;
			}
		}
		if (generator->band & MPF_DTMF_GENERATOR_OUTBAND) {
			generator->since_last_event += CODEC_FRAME_TIME_BASE;
			if (generator->since_last_event >= generator->events_ptime)
				generator->since_last_event = 0;
			else
				return TRUE;
			frame->type |= MEDIA_FRAME_TYPE_EVENT;
			frame->event_frame.reserved = 0;
			frame->event_frame.event_id = generator->event_id;
			frame->event_frame.volume = DTMF_EVENT_VOLUME;
			if (generator->counter >= generator->tone_duration) {
				generator->state = DTMF_GEN_STATE_ENDING;
				generator->counter = 0;
				frame->event_frame.edge = 1;
				frame->marker = MPF_MARKER_END_OF_EVENT;
				if (generator->event_duration > 0xFFFF) {
					/* Shorten the tone a bit instead of lenghtening */
					generator->new_segment = TRUE;
					frame->event_frame.duration = 0xFFFF;
					generator->event_duration = 0;
				} else
					frame->event_frame.duration = generator->event_duration;
			} else {
				frame->event_frame.edge = 0;
				if (generator->counter == generator->frame_duration)  /* First chunk of event */
					frame->marker = MPF_MARKER_START_OF_EVENT;
				else if (generator->new_segment) {
					frame->marker = MPF_MARKER_NEW_SEGMENT;
					generator->new_segment = FALSE;
				} else
					frame->marker = MPF_MARKER_NONE;
				if (generator->event_duration > 0xFFFF) {
					frame->event_frame.duration = 0xFFFF;
					generator->event_duration = 0;
					generator->new_segment = TRUE;
				} else
					frame->event_frame.duration = generator->event_duration;
			}
			return TRUE;
		}  /* MPF_DTMF_GENERATOR_OUTBAND */
		if (generator->counter >= generator->tone_duration) {
			generator->state = DTMF_GEN_STATE_SILENCE;
			generator->counter = 0;
		}
		return TRUE;
	}
	else if (generator->state == DTMF_GEN_STATE_ENDING) {
		generator->since_last_event += CODEC_FRAME_TIME_BASE;
		if (generator->since_last_event >= generator->events_ptime)
			generator->since_last_event = 0;
		else
			return TRUE;
		generator->counter++;
		frame->type |= MEDIA_FRAME_TYPE_EVENT;
		frame->marker = MPF_MARKER_END_OF_EVENT;
		frame->event_frame.event_id = generator->event_id;
		frame->event_frame.volume = DTMF_EVENT_VOLUME;
		frame->event_frame.reserved = 0;
		frame->event_frame.edge = 1;
		if (generator->new_segment)
			/* Tone was shortened a little bit */
			frame->event_frame.duration = 0xFFFF;
		else
			frame->event_frame.duration = generator->event_duration;
		if (generator->counter >= 2) {
			generator->state = DTMF_GEN_STATE_SILENCE;
			generator->counter *= generator->frame_duration;
		}
		if (generator->band & MPF_DTMF_GENERATOR_INBAND) {
			frame->type |= MEDIA_FRAME_TYPE_AUDIO;
			memset(frame->codec_frame.buffer, 0, frame->codec_frame.size);
		}
		return TRUE;
	}
	else if (generator->state == DTMF_GEN_STATE_SILENCE) {
		generator->counter += generator->frame_duration;
		if (generator->counter >= generator->silence_duration)
			generator->state = DTMF_GEN_STATE_IDLE;
	}

	return FALSE;
}


MPF_DECLARE(void) mpf_dtmf_generator_destroy(struct mpf_dtmf_generator_t *generator)
{
	mpf_dtmf_generator_reset(generator);
	apr_thread_mutex_destroy(generator->mutex);
	generator->mutex = NULL;
}
