/*
 * Copyright 2008 Arsen Chaloyan
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

#include "mpf_activity_detector.h"
#include "apt_log.h"

/** Detector states */
typedef enum {
	DETECTOR_STATE_INACTIVITY,           /**< inactivity detected */
	DETECTOR_STATE_ACTIVITY_TRANSITION,  /**< activity detection is in-progress */
	DETECTOR_STATE_ACTIVITY,             /**< activity detected */
	DETECTOR_STATE_INACTIVITY_TRANSITION /**< inactivity detection is in-progress */
} mpf_detector_state_e;

/** Activity detector */
struct mpf_activity_detector_t {
	/* voice activity (silence) level threshold */
	apr_size_t           level_threshold;

	/* period of activity required to complete transition to active state */
	apr_size_t           speech_timeout;
	/* period of inactivity required to complete transition to inactive state */
	apr_size_t           silence_timeout;
	/* noinput timeout */
	apr_size_t           noinput_timeout;

	/* current state */
	mpf_detector_state_e state;
	/* duration spent in current state  */
	apr_size_t           duration;
};

/** Create activity detector */
MPF_DECLARE(mpf_activity_detector_t*) mpf_activity_detector_create(apr_pool_t *pool)
{
	mpf_activity_detector_t *detector = apr_palloc(pool,sizeof(mpf_activity_detector_t));
	detector->level_threshold = 2; /* 0 .. 255 */
	detector->speech_timeout = 300; /* 0.3 s */
	detector->silence_timeout = 300; /* 0.3 s */
	detector->noinput_timeout = 5000; /* 5 s */
	detector->duration = 0;
	detector->state = DETECTOR_STATE_INACTIVITY;
	return detector;
}

/** Reset activity detector */
MPF_DECLARE(void) mpf_activity_detector_reset(mpf_activity_detector_t *detector)
{
	detector->duration = 0;
	detector->state = DETECTOR_STATE_INACTIVITY;
}

/** Set threshold of voice activity (silence) level */
MPF_DECLARE(void) mpf_activity_detector_level_set(mpf_activity_detector_t *detector, apr_size_t level_threshold)
{
	detector->level_threshold = level_threshold;
}

/** Set noinput timeout */
MPF_DECLARE(void) mpf_activity_detector_noinput_timeout_set(mpf_activity_detector_t *detector, apr_size_t noinput_timeout)
{
	detector->noinput_timeout = noinput_timeout;
}

/** Set timeout required to trigger speech (transition from inactive to active state) */
MPF_DECLARE(void) mpf_activity_detector_speech_timeout_set(mpf_activity_detector_t *detector, apr_size_t speech_timeout)
{
	detector->speech_timeout = speech_timeout;
}

/** Set timeout required to trigger silence (transition from active to inactive state) */
MPF_DECLARE(void) mpf_activity_detector_silence_timeout_set(mpf_activity_detector_t *detector, apr_size_t silence_timeout)
{
	detector->silence_timeout = silence_timeout;
}


static APR_INLINE void mpf_activity_detector_state_change(mpf_activity_detector_t *detector, mpf_detector_state_e state)
{
	detector->duration = 0;
	detector->state = state;
}

static apr_size_t mpf_activity_detector_level_calculate(const mpf_frame_t *frame)
{
	apr_size_t sum = 0;
	apr_size_t count = frame->codec_frame.size/2;
	const apr_int16_t *cur = frame->codec_frame.buffer;
	const apr_int16_t *end = cur + count;

	for(; cur < end; cur++) {
		if(*cur < 0) {
			sum -= *cur;
		}
		else {
			sum += *cur;
		}
	}

	return sum / count;
}

/** Process current frame */
MPF_DECLARE(mpf_detector_event_e) mpf_activity_detector_process(mpf_activity_detector_t *detector, const mpf_frame_t *frame)
{
	mpf_detector_event_e det_event = MPF_DETECTOR_EVENT_NONE;
	apr_size_t level = 0;
	if((frame->type & MEDIA_FRAME_TYPE_AUDIO) == MEDIA_FRAME_TYPE_AUDIO) {
		/* first, calculate current activity level of processed frame */
		level = mpf_activity_detector_level_calculate(frame);
#if 0
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Activity Detector [%"APR_SIZE_T_FMT"]",level);
#endif
	}

	if(detector->state == DETECTOR_STATE_INACTIVITY) {
		if(level >= detector->level_threshold) {
			/* start to detect activity */
			mpf_activity_detector_state_change(detector,DETECTOR_STATE_ACTIVITY_TRANSITION);
		}
		else {
			detector->duration += CODEC_FRAME_TIME_BASE;
			if(detector->duration >= detector->noinput_timeout) {
				/* detected noinput */
				det_event = MPF_DETECTOR_EVENT_NOINPUT;
			}
		}
	}
	else if(detector->state == DETECTOR_STATE_ACTIVITY_TRANSITION) {
		if(level >= detector->level_threshold) {
			detector->duration += CODEC_FRAME_TIME_BASE;
			if(detector->duration >= detector->speech_timeout) {
				/* finally detected activity */
				det_event = MPF_DETECTOR_EVENT_ACTIVITY;
				mpf_activity_detector_state_change(detector,DETECTOR_STATE_ACTIVITY);
			}
		}
		else {
			/* fallback to inactivity */
			mpf_activity_detector_state_change(detector,DETECTOR_STATE_INACTIVITY);
		}
	}
	else if(detector->state == DETECTOR_STATE_ACTIVITY) {
		if(level >= detector->level_threshold) {
			detector->duration += CODEC_FRAME_TIME_BASE;
		}
		else {
			/* start to detect inactivity */
			mpf_activity_detector_state_change(detector,DETECTOR_STATE_INACTIVITY_TRANSITION);
		}
	}
	else if(detector->state == DETECTOR_STATE_INACTIVITY_TRANSITION) {
		if(level >= detector->level_threshold) {
			/* fallback to activity */
			mpf_activity_detector_state_change(detector,DETECTOR_STATE_ACTIVITY);
		}
		else {
			detector->duration += CODEC_FRAME_TIME_BASE;
			if(detector->duration >= detector->silence_timeout) {
				/* detected inactivity */
				det_event = MPF_DETECTOR_EVENT_INACTIVITY;
				mpf_activity_detector_state_change(detector,DETECTOR_STATE_INACTIVITY);
			}
		}
	}

	return det_event;
}
