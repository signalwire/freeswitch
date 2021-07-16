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

#include "mpf_dtmf_detector.h"
#include "apr_thread_mutex.h"
#include "apt_log.h"
#include "mpf_named_event.h"
#include <math.h>

#ifndef M_PI
#	define M_PI 3.141592653589793238462643
#endif

/** Max detected DTMF digits buffer length */
#define MPF_DTMFDET_BUFFER_LEN  32

/** Number of DTMF frequencies */
#define DTMF_FREQUENCIES         8

/** Window length in samples (at 8kHz) for Goertzel's frequency analysis */
#define GOERTZEL_SAMPLES_8K    102

/** See RFC4733 */
#define DTMF_EVENT_ID_MAX       15  /* 0123456789*#ABCD */

/**
 * Goertzel frequency detector (second-order IIR filter) state:
 *
 * s(t) = x(t) + coef * s(t-1) - s(t-2), where s(0)=0; s(1) = 0;
 * x(t) is the input signal
 *
 * Then energy of frequency f in the signal is:
 * X(f)X'(f) = s(t-2)^2 + s(t-1)^2 - coef*s(t-2)*s(t-1)
 */
typedef struct goertzel_state_t {
	/** coef = cos(2*pi*f_tone/f_sampling) */
	double coef;
	/** s(t-2) or resulting energy @see goertzel_state_t */
	double s1;
	/** s(t-1) @see goertzel_state_t */
	double s2;
} goertzel_state_t;

/** DTMF frequencies */
static const double dtmf_freqs[DTMF_FREQUENCIES] = {
	 697,  770,  852,  941,  /* Row frequencies */
	1209, 1336, 1477, 1633}; /* Col frequencies */

/** [row, col] major frequency to digit mapping */
static const char freq2digits[DTMF_FREQUENCIES/2][DTMF_FREQUENCIES/2] =
	{ { '1', '2', '3', 'A' },
	  { '4', '5', '6', 'B' },
	  { '7', '8', '9', 'C' },
	  { '*', '0', '#', 'D' } };

/** Media Processing Framework's Dual Tone Multiple Frequncy detector */
struct mpf_dtmf_detector_t {
	/** Mutex to guard the buffer */
	struct apr_thread_mutex_t     *mutex;
	/** Recognizer band */
	enum mpf_dtmf_detector_band_e  band;
	/** Detected digits buffer */
	char                           buf[MPF_DTMFDET_BUFFER_LEN+1];
	/** Number of digits in the buffer */
	apr_size_t                     digits;
	/** Number of lost digits due to full buffer */
	apr_size_t                     lost_digits;
	/** Frequency analyzators */
	struct goertzel_state_t        energies[DTMF_FREQUENCIES];
	/** Total energy of signal */
	double                         totenergy;
	/** Number of samples in a window */
	apr_size_t                     wsamples;
	/** Number of samples processed */
	apr_size_t                     nsamples;
	/** Previously detected and last reported digits */
	char                           last1, last2, curr;
};


MPF_DECLARE(struct mpf_dtmf_detector_t *) mpf_dtmf_detector_create_ex(
								const struct mpf_audio_stream_t *stream,
								enum mpf_dtmf_detector_band_e band,
								struct apr_pool_t *pool)
{
	apr_status_t status;
	struct mpf_dtmf_detector_t *det;
	int flg_band = band;

	if (!stream->tx_descriptor) flg_band &= ~MPF_DTMF_DETECTOR_INBAND;
/*
	Event descriptor is not important actually
	if (!stream->tx_event_descriptor) flg_band &= ~MPF_DTMF_DETECTOR_OUTBAND;
*/
	if (!flg_band) return NULL;

	det = apr_palloc(pool, sizeof(mpf_dtmf_detector_t));
	if (!det) return NULL;
	status = apr_thread_mutex_create(&det->mutex, APR_THREAD_MUTEX_DEFAULT, pool);
	if (status != APR_SUCCESS) return NULL;

	det->band = (enum mpf_dtmf_detector_band_e) flg_band;
	det->buf[0] = 0;
	det->digits = 0;
	det->lost_digits = 0;

	if (det->band & MPF_DTMF_DETECTOR_INBAND) {
		apr_size_t i;
		for (i = 0; i < DTMF_FREQUENCIES; i++) {
			det->energies[i].coef = 2 * cos(2 * M_PI * dtmf_freqs[i] /
				stream->tx_descriptor->sampling_rate);
			det->energies[i].s1 = 0;
			det->energies[i].s2 = 0;
		}
		det->nsamples = 0;
		det->wsamples = GOERTZEL_SAMPLES_8K * (stream->tx_descriptor->sampling_rate / 8000);
		det->last1 = det->last2 = det->curr = 0;
		det->totenergy = 0;
	}

	return det;
}

MPF_DECLARE(char) mpf_dtmf_detector_digit_get(struct mpf_dtmf_detector_t *detector)
{
	char digit;
	apr_thread_mutex_lock(detector->mutex);
	digit = detector->buf[0];
	if (digit) {
		memmove(detector->buf, detector->buf + 1, strlen(detector->buf));
		detector->digits--;
	}
	apr_thread_mutex_unlock(detector->mutex);
	return digit;
}

MPF_DECLARE(apr_size_t) mpf_dtmf_detector_digits_lost(const struct mpf_dtmf_detector_t *detector)
{
	return detector->lost_digits;
}

MPF_DECLARE(void) mpf_dtmf_detector_reset(struct mpf_dtmf_detector_t *detector)
{
	apr_thread_mutex_lock(detector->mutex);
	detector->buf[0] = 0;
	detector->lost_digits = 0;
	detector->digits = 0;
	detector->curr = detector->last1 = detector->last2 = 0;
	detector->nsamples = 0;
	detector->totenergy = 0;
	apr_thread_mutex_unlock(detector->mutex);
}

static APR_INLINE void mpf_dtmf_detector_add_digit(
								struct mpf_dtmf_detector_t *detector,
								char digit)
{
	if (!digit) return;
	apr_thread_mutex_lock(detector->mutex);
	if (detector->digits < MPF_DTMFDET_BUFFER_LEN) {
		detector->buf[detector->digits++] = digit;
		detector->buf[detector->digits] = 0;
	} else
		detector->lost_digits++;
	apr_thread_mutex_unlock(detector->mutex);
}

static APR_INLINE void goertzel_sample(
								struct mpf_dtmf_detector_t *detector,
								apr_int16_t sample)
{
	apr_size_t i;
	double s;
	for (i = 0; i < DTMF_FREQUENCIES; i++) {
		s = detector->energies[i].s1;
		detector->energies[i].s1 = detector->energies[i].s2;
		detector->energies[i].s2 = sample +
			detector->energies[i].coef * detector->energies[i].s1 - s;
	}
	detector->totenergy += sample * sample;
}

static void goertzel_energies_digit(struct mpf_dtmf_detector_t *detector)
{
	apr_size_t i, rmax = 0, cmax = 0;
	double reng = 0, ceng = 0;
	char digit = 0;

	/* Calculate energies and maxims */
	for (i = 0; i < DTMF_FREQUENCIES; i++) {
		double eng = detector->energies[i].s1 * detector->energies[i].s1 +
			detector->energies[i].s2 * detector->energies[i].s2 -
			detector->energies[i].coef * detector->energies[i].s1 * detector->energies[i].s2;
		if (i < DTMF_FREQUENCIES/2) {
			if (eng > reng) {
				rmax = i;
				reng = eng;
			}
		} else {
			if (eng > ceng) {
				cmax = i;
				ceng = eng;
			}
		}
	}

	if ((reng < 8.0e10 * detector->wsamples / GOERTZEL_SAMPLES_8K) ||
		(ceng < 8.0e10 * detector->wsamples / GOERTZEL_SAMPLES_8K))
	{
		/* energy not high enough */
	} else if ((ceng > reng) && (reng < ceng * 0.398)) {  /* twist > 4dB, error */
		/* Twist check
		 * CEPT => twist < 6dB
		 * AT&T => forward twist < 4dB and reverse twist < 8dB
		 *  -ndB < 10 log10( v1 / v2 ), where v1 < v2
		 *  -4dB < 10 log10( v1 / v2 )
		 *  -0.4  < log10( v1 / v2 )
		 *  0.398 < v1 / v2
		 *  0.398 * v2 < v1
		 */
	} else if ((ceng < reng) && (ceng < reng * 0.158)) {  /* twist > 8db, error */
		/* Reverse twist check failed */
	} else if (0.25 * detector->totenergy > (reng + ceng)) {  /* 16db */
		/* Signal energy to total energy ratio test failed */
	} else {
		if (cmax >= DTMF_FREQUENCIES/2 && cmax < DTMF_FREQUENCIES)
			digit = freq2digits[rmax][cmax - DTMF_FREQUENCIES/2];
	}

	/* Three successive detections will trigger the detection */
	if (digit != detector->curr) {
		if (digit && ((detector->last1 == digit) && (detector->last2 == digit))) {
			detector->curr = digit;
			mpf_dtmf_detector_add_digit(detector, digit);
		} else if ((detector->last1 != detector->curr) && (detector->last2 != detector->curr)) {
			detector->curr = 0;
		}
	}
	detector->last1 = detector->last2;
	detector->last2 = digit;

	/* Reset Goertzel's detectors */
	for (i = 0; i < DTMF_FREQUENCIES; i++) {
		detector->energies[i].s1 = 0;
		detector->energies[i].s2 = 0;
	}
	detector->totenergy = 0;
}

MPF_DECLARE(void) mpf_dtmf_detector_get_frame(
								struct mpf_dtmf_detector_t *detector,
								const struct mpf_frame_t *frame)
{
	if ((detector->band & MPF_DTMF_DETECTOR_OUTBAND) &&
		(frame->type & MEDIA_FRAME_TYPE_EVENT) &&
		(frame->event_frame.event_id <= DTMF_EVENT_ID_MAX) &&
		(frame->marker == MPF_MARKER_START_OF_EVENT))
	{
		if (detector->band & MPF_DTMF_DETECTOR_INBAND) {
			detector->band &= ~MPF_DTMF_DETECTOR_INBAND;
			apt_log(MPF_LOG_MARK, APT_PRIO_INFO, "Out-of-band digit arrived, turning "
				"in-band DTMF detector off");
		}
		mpf_dtmf_detector_add_digit(detector, mpf_event_id_to_dtmf_char(
			frame->event_frame.event_id));
		return;
	}

	if ((detector->band & MPF_DTMF_DETECTOR_INBAND) && (frame->type & MEDIA_FRAME_TYPE_AUDIO)) {
		apr_int16_t *samples = frame->codec_frame.buffer;
		apr_size_t i;

		for (i = 0; i < frame->codec_frame.size / 2; i++) {
			goertzel_sample(detector, samples[i]);
			if (++detector->nsamples >= detector->wsamples) {
				goertzel_energies_digit(detector);
				detector->nsamples = 0;
			}
		}
	}
}

MPF_DECLARE(void) mpf_dtmf_detector_destroy(struct mpf_dtmf_detector_t *detector)
{
	apr_thread_mutex_destroy(detector->mutex);
	detector->mutex = NULL;
}
