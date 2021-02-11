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

#ifndef MPF_DTMF_DETECTOR_H
#define MPF_DTMF_DETECTOR_H

/**
 * @file mpf_dtmf_detector.h
 * @brief DTMF detector
 *
 * Detector of DTMF tones sent both out-of-band (RFC4733) and in-band (audio).
 */

#include "apr.h"
#include "apr_pools.h"
#include "apt.h"
#include "mpf_frame.h"
#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/** DTMF detector band */
typedef enum mpf_dtmf_detector_band_e {
	/** Detect tones in-band */
	MPF_DTMF_DETECTOR_INBAND  = 0x1,
	/** Detect named events out-of-band */
	MPF_DTMF_DETECTOR_OUTBAND = 0x2,
	/** Detect both in-band and out-of-band digits */
	MPF_DTMF_DETECTOR_BOTH = MPF_DTMF_DETECTOR_INBAND | MPF_DTMF_DETECTOR_OUTBAND
} mpf_dtmf_detector_band_e;

/** Opaque MPF DTMF detector structure definition */
typedef struct mpf_dtmf_detector_t mpf_dtmf_detector_t;


/**
 * Create MPF DTMF detector (advanced).
 * @param stream      A stream to get digits from.
 * @param band        One of:
 *   - MPF_DTMF_DETECTOR_INBAND: detect audible tones only
 *   - MPF_DTMF_DETECTOR_OUTBAND: detect out-of-band named-events only
 *   - MPF_DTMF_DETECTOR_BOTH: detect digits in both bands if supported by
 *     stream. When out-of-band digit arrives, in-band detection is turned off.
 * @param pool        Memory pool to allocate DTMF detector from.
 * @return The object or NULL on error.
 * @see mpf_dtmf_detector_create
 */
MPF_DECLARE(struct mpf_dtmf_detector_t *) mpf_dtmf_detector_create_ex(
								const struct mpf_audio_stream_t *stream,
								enum mpf_dtmf_detector_band_e band,
								struct apr_pool_t *pool);

/**
 * Create MPF DTMF detector (simple). Calls mpf_dtmf_detector_create_ex
 * with band = MPF_DTMF_DETECTOR_BOTH if out-of-band supported by the stream,
 * MPF_DTMF_DETECTOR_INBAND otherwise.
 * @param stream      A stream to get digits from.
 * @param pool        Memory pool to allocate DTMF detector from.
 * @return The object or NULL on error.
 * @see mpf_dtmf_detector_create_ex
 */
static APR_INLINE struct mpf_dtmf_detector_t *mpf_dtmf_detector_create(
								const struct mpf_audio_stream_t *stream,
								struct apr_pool_t *pool)
{
	return mpf_dtmf_detector_create_ex(stream,
		stream->tx_event_descriptor ? MPF_DTMF_DETECTOR_BOTH : MPF_DTMF_DETECTOR_INBAND,
		pool);
}

/**
 * Get DTMF digit from buffer of digits detected so far and remove it.
 * @param detector  The detector.
 * @return DTMF character [0-9*#A-D] or NUL if the buffer is empty.
 */
MPF_DECLARE(char) mpf_dtmf_detector_digit_get(struct mpf_dtmf_detector_t *detector);

/**
 * Retrieve how many digits was lost due to full buffer.
 * @param detector  The detector.
 * @return Number of lost digits.
 */
MPF_DECLARE(apr_size_t) mpf_dtmf_detector_digits_lost(const struct mpf_dtmf_detector_t *detector);

/**
 * Empty the buffer and reset detection states.
 * @param detector  The detector.
 */
MPF_DECLARE(void) mpf_dtmf_detector_reset(struct mpf_dtmf_detector_t *detector);

/**
 * Detect DTMF digits in the frame.
 * @param detector  The detector.
 * @param frame     Frame object passed in stream_write().
 */
MPF_DECLARE(void) mpf_dtmf_detector_get_frame(
								struct mpf_dtmf_detector_t *detector,
								const struct mpf_frame_t *frame);

/**
 * Free all resources associated with the detector.
 * @param detector  The detector.
 */
MPF_DECLARE(void) mpf_dtmf_detector_destroy(struct mpf_dtmf_detector_t *detector);

APT_END_EXTERN_C

#endif /* MPF_DTMF_DETECTOR_H */
