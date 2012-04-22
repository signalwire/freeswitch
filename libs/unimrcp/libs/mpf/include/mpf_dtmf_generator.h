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
 */

#ifndef MPF_DTMF_GENERATOR_H
#define MPF_DTMF_GENERATOR_H

/**
 * @file mpf_dtmf_generator.h
 * @brief DTMF generator
 *
 * Generator used to send DTMF tones. Capable to send digits
 * either in-band as audible tones or out-of-band according
 * to RFC4733.
 */

#include "apr_pools.h"
#include "apt.h"
#include "mpf_frame.h"
#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/** DTMF generator band */
typedef enum mpf_dtmf_generator_band_e {
	/** Generate tones in-band */
	MPF_DTMF_GENERATOR_INBAND  = 0x1,
	/** Generate named events out-of-band */
	MPF_DTMF_GENERATOR_OUTBAND = 0x2,
	/** Generate both tones and named events */
	MPF_DTMF_GENERATOR_BOTH = MPF_DTMF_GENERATOR_INBAND | MPF_DTMF_GENERATOR_OUTBAND
} mpf_dtmf_generator_band_e;

/** Opaque MPF DTMF generator structure definition */
typedef struct mpf_dtmf_generator_t mpf_dtmf_generator_t;


/**
 * Create MPF DTMF generator (advanced).
 * @param stream      A stream to transport digits via.
 * @param band        MPF_DTMF_GENERATOR_INBAND or MPF_DTMF_GENERATOR_OUTBAND
 * @param tone_ms     Tone duration in milliseconds.
 * @param silence_ms  Inter-digit silence in milliseconds.
 * @param pool        Memory pool to allocate DTMF generator from.
 * @return The object or NULL on error.
 * @see mpf_dtmf_generator_create
 */
MPF_DECLARE(struct mpf_dtmf_generator_t *) mpf_dtmf_generator_create_ex(
								const struct mpf_audio_stream_t *stream,
								enum mpf_dtmf_generator_band_e band,
								apr_uint32_t tone_ms,
								apr_uint32_t silence_ms,
								struct apr_pool_t *pool);

/**
 * Create MPF DTMF generator (simple). Calls mpf_dtmf_generator_create_ex
 * with band = MPF_DTMF_GENERATOR_OUTBAND if supported by the stream or
 * MPF_DTMF_GENERATOR_INBAND otherwise, tone_ms = 70, silence_ms = 50.
 * @param stream      A stream to transport digits via.
 * @param pool        Memory pool to allocate DTMF generator from.
 * @return The object or NULL on error.
 * @see mpf_dtmf_generator_create_ex
 */
static APR_INLINE struct mpf_dtmf_generator_t *mpf_dtmf_generator_create(
								const struct mpf_audio_stream_t *stream,
								struct apr_pool_t *pool)
{
	return mpf_dtmf_generator_create_ex(stream,
		stream->rx_event_descriptor ? MPF_DTMF_GENERATOR_OUTBAND : MPF_DTMF_GENERATOR_INBAND,
		70, 50, pool);
}

/**
 * Add DTMF digits to the queue.
 * @param generator The generator.
 * @param digits    DTMF character sequence [0-9*#A-D].
 * @return TRUE if ok, FALSE if there are too many digits.
 */
MPF_DECLARE(apt_bool_t) mpf_dtmf_generator_enqueue(
								struct mpf_dtmf_generator_t *generator,
								const char *digits);

/**
 * Empty the queue and immediately stop generating.
 * @param generator The generator.
 */
MPF_DECLARE(void) mpf_dtmf_generator_reset(struct mpf_dtmf_generator_t *generator);

/**
 * Check state of the generator.
 * @param generator The generator.
 * @return TRUE if generating a digit or there are digits waiting in queue.
 * FALSE if the queue is empty or generating silence after the last digit.
 */
MPF_DECLARE(apt_bool_t) mpf_dtmf_generator_sending(const struct mpf_dtmf_generator_t *generator);

/**
 * Put frame into the stream.
 * @param generator The generator.
 * @param frame     Frame object passed in stream_read().
 * @return TRUE if frame with tone (both in-band and out-of-band) was generated,
 * FALSE otherwise. In contrast to mpf_dtmf_generator_sending, returns FALSE even
 * if generating inter-digit silence. In other words returns TRUE iff the frame
 * object was filled with data. This method MUST be called for each frame for
 * proper timing.
 */
MPF_DECLARE(apt_bool_t) mpf_dtmf_generator_put_frame(
								struct mpf_dtmf_generator_t *generator,
								struct mpf_frame_t *frame);

/**
 * Free all resources associated with the generator.
 * @param generator The generator.
 */
MPF_DECLARE(void) mpf_dtmf_generator_destroy(struct mpf_dtmf_generator_t *generator);

APT_END_EXTERN_C

#endif /* MPF_DTMF_GENERATOR_H */
