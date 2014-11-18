/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * $Id: mpf_codec_descriptor.h 2239 2014-11-12 01:52:59Z achaloyan@gmail.com $
 */

#ifndef MPF_CODEC_DESCRIPTOR_H
#define MPF_CODEC_DESCRIPTOR_H

/**
 * @file mpf_codec_descriptor.h
 * @brief MPF Codec Descriptor
 */ 

#include <apr_tables.h>
#include "apt_string.h"
#include "mpf.h"

APT_BEGIN_EXTERN_C

/** Codec frame time base in msec */
#define CODEC_FRAME_TIME_BASE 10
/** Bytes per sample for linear pcm */
#define BYTES_PER_SAMPLE 2
/** Bits per sample for linear pcm */
#define BITS_PER_SAMPLE 16

/** Supported sampling rates */
typedef enum {
	MPF_SAMPLE_RATE_NONE  = 0x00,
	MPF_SAMPLE_RATE_8000  = 0x01,
	MPF_SAMPLE_RATE_16000 = 0x02,
	MPF_SAMPLE_RATE_32000 = 0x04,
	MPF_SAMPLE_RATE_48000 = 0x08,

	MPF_SAMPLE_RATE_SUPPORTED =	MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000 | 
								MPF_SAMPLE_RATE_32000 | MPF_SAMPLE_RATE_48000
} mpf_sample_rates_e;

/** Codec descriptor declaration */
typedef struct mpf_codec_descriptor_t mpf_codec_descriptor_t;
/** Codec attributes declaration */
typedef struct mpf_codec_attribs_t mpf_codec_attribs_t;
/** Codec list declaration */
typedef struct mpf_codec_list_t mpf_codec_list_t;
/** Codec capabilities declaration */
typedef struct mpf_codec_capabilities_t mpf_codec_capabilities_t;
/** Codec frame declaration */
typedef struct mpf_codec_frame_t mpf_codec_frame_t;


/** Codec descriptor */
struct mpf_codec_descriptor_t {
	/** Payload type used in RTP packet */
	apr_byte_t   payload_type;
	/** Codec name */
	apt_str_t    name;
	/** Sampling rate */
	apr_uint16_t sampling_rate;
	/** Channel count */
	apr_byte_t   channel_count;
	/** Codec dependent additional format */
	apt_str_t    format;
	/**  Enabled/disabled state */
	apt_bool_t   enabled;
};

/** List of codec descriptors */
struct mpf_codec_list_t {
	/** Dynamic array of codec descriptors (mpf_codec_descriptor_t) */
	apr_array_header_t     *descriptor_arr;
	/** Preffered primary (audio/video codec) descriptor from descriptor_arr */
	mpf_codec_descriptor_t *primary_descriptor;
	/** Preffered named event (telephone-event) descriptor from descriptor_arr */
	mpf_codec_descriptor_t *event_descriptor;
};

/** Codec attributes */
struct mpf_codec_attribs_t {
	/** Codec name */
	apt_str_t  name;
	/** Bits per sample */
	apr_byte_t bits_per_sample;
	/** Supported sampling rates (mpf_sample_rates_e) */
	int        sample_rates;
};

/** List of codec attributes (capabilities) */
struct mpf_codec_capabilities_t {
	/** Dynamic array of codec attributes (mpf_codec_attrribs_t) */
	apr_array_header_t *attrib_arr;
	/** Allow/support named events */
	apt_bool_t          allow_named_events;
};

/** Codec frame */
struct mpf_codec_frame_t {
	/** Raw buffer, which may contain encoded or decoded data */
	void      *buffer;
	/** Buffer size */
	apr_size_t size;
};


/** Initialize codec descriptor */
static APR_INLINE void mpf_codec_descriptor_init(mpf_codec_descriptor_t *descriptor)
{
	descriptor->payload_type = 0;
	apt_string_reset(&descriptor->name);
	descriptor->sampling_rate = 0;
	descriptor->channel_count = 0;
	apt_string_reset(&descriptor->format);
	descriptor->enabled = TRUE;
}

/** Initialize codec descriptor */
static APR_INLINE  mpf_codec_descriptor_t* mpf_codec_descriptor_create(apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor = (mpf_codec_descriptor_t*) apr_palloc(pool,sizeof(mpf_codec_descriptor_t));
	mpf_codec_descriptor_init(descriptor);
	return descriptor;
}

/** Calculate encoded frame size in bytes */
static APR_INLINE apr_size_t mpf_codec_frame_size_calculate(const mpf_codec_descriptor_t *descriptor, const mpf_codec_attribs_t *attribs)
{
	return (size_t) descriptor->channel_count * attribs->bits_per_sample * CODEC_FRAME_TIME_BASE * 
			descriptor->sampling_rate / 1000 / 8; /* 1000 - msec per sec, 8 - bits per byte */
}

/** Calculate samples of the frame (ts) */
static APR_INLINE apr_size_t mpf_codec_frame_samples_calculate(const mpf_codec_descriptor_t *descriptor)
{
	return (size_t) descriptor->channel_count * CODEC_FRAME_TIME_BASE * descriptor->sampling_rate / 1000;
}

/** Calculate linear frame size in bytes */
static APR_INLINE apr_size_t mpf_codec_linear_frame_size_calculate(apr_uint16_t sampling_rate, apr_byte_t channel_count)
{
	return (size_t) channel_count * BYTES_PER_SAMPLE * CODEC_FRAME_TIME_BASE * sampling_rate / 1000;
}



/** Reset list of codec descriptors */
static APR_INLINE void mpf_codec_list_reset(mpf_codec_list_t *codec_list)
{
	codec_list->descriptor_arr = NULL;
	codec_list->primary_descriptor = NULL;
	codec_list->event_descriptor = NULL;
}

/** Initialize list of codec descriptors */
static APR_INLINE void mpf_codec_list_init(mpf_codec_list_t *codec_list, apr_size_t initial_count, apr_pool_t *pool)
{
	codec_list->descriptor_arr = apr_array_make(pool,(int)initial_count, sizeof(mpf_codec_descriptor_t));
	codec_list->primary_descriptor = NULL;
	codec_list->event_descriptor = NULL;
}

/** Copy list of codec descriptors */
static APR_INLINE void mpf_codec_list_copy(mpf_codec_list_t *codec_list, const mpf_codec_list_t *src_codec_list, apr_pool_t *pool)
{
	codec_list->descriptor_arr = apr_array_copy(pool,src_codec_list->descriptor_arr);
}

/** Increment number of codec descriptors in the list and return the descriptor to fill */
static APR_INLINE mpf_codec_descriptor_t* mpf_codec_list_add(mpf_codec_list_t *codec_list)
{
	mpf_codec_descriptor_t *descriptor = (mpf_codec_descriptor_t*)apr_array_push(codec_list->descriptor_arr);
	mpf_codec_descriptor_init(descriptor);
	return descriptor;
}

/** Determine if codec list is empty */
static APR_INLINE apt_bool_t mpf_codec_list_is_empty(const mpf_codec_list_t *codec_list)
{
	return apr_is_empty_array(codec_list->descriptor_arr);
}

/** Get codec descriptor by index */
static APR_INLINE mpf_codec_descriptor_t* mpf_codec_list_descriptor_get(const mpf_codec_list_t *codec_list, apr_size_t id)
{
	if(id >= (apr_size_t)codec_list->descriptor_arr->nelts) {
		return NULL;
	}
	return &APR_ARRAY_IDX(codec_list->descriptor_arr,id,mpf_codec_descriptor_t);
}

/** Create linear PCM descriptor */
MPF_DECLARE(mpf_codec_descriptor_t*) mpf_codec_lpcm_descriptor_create(apr_uint16_t sampling_rate, apr_byte_t channel_count, apr_pool_t *pool);

/** Create codec descriptor by capabilities */
MPF_DECLARE(mpf_codec_descriptor_t*) mpf_codec_descriptor_create_by_capabilities(const mpf_codec_capabilities_t *capabilities, const mpf_codec_descriptor_t *peer, apr_pool_t *pool);

/** Match two codec descriptors */
MPF_DECLARE(apt_bool_t) mpf_codec_descriptors_match(const mpf_codec_descriptor_t *descriptor1, const mpf_codec_descriptor_t *descriptor2);

/** Match specified codec descriptor and the default lpcm one */
MPF_DECLARE(apt_bool_t) mpf_codec_lpcm_descriptor_match(const mpf_codec_descriptor_t *descriptor);

/** Match codec descriptor by attribs specified */
MPF_DECLARE(apt_bool_t) mpf_codec_descriptor_match_by_attribs(mpf_codec_descriptor_t *descriptor, const mpf_codec_descriptor_t *static_descriptor, const mpf_codec_attribs_t *attribs);



/** Initialize codec capabilities */
static APR_INLINE void mpf_codec_capabilities_init(mpf_codec_capabilities_t *capabilities, apr_size_t initial_count, apr_pool_t *pool)
{
	capabilities->attrib_arr = apr_array_make(pool,(int)initial_count, sizeof(mpf_codec_attribs_t));
	capabilities->allow_named_events = TRUE;
}

/** Clone codec capabilities */
static APR_INLINE void mpf_codec_capabilities_clone(mpf_codec_capabilities_t *capabilities, const mpf_codec_capabilities_t *src_capabilities, apr_pool_t *pool)
{
	capabilities->attrib_arr = apr_array_copy(pool,src_capabilities->attrib_arr);
	capabilities->allow_named_events = src_capabilities->allow_named_events;
}

/** Merge codec capabilities */
static APR_INLINE apt_bool_t mpf_codec_capabilities_merge(mpf_codec_capabilities_t *capabilities, const mpf_codec_capabilities_t *src_capabilities, apr_pool_t *pool)
{
	if(capabilities->allow_named_events == FALSE && src_capabilities->allow_named_events == TRUE) {
		capabilities->allow_named_events = src_capabilities->allow_named_events;
	}
	capabilities->attrib_arr = apr_array_append(pool,capabilities->attrib_arr,src_capabilities->attrib_arr);
	return TRUE;
}

/** Add codec capabilities */
static APR_INLINE apt_bool_t mpf_codec_capabilities_add(mpf_codec_capabilities_t *capabilities, int sample_rates, const char *codec_name)
{
	mpf_codec_attribs_t *attribs = (mpf_codec_attribs_t*)apr_array_push(capabilities->attrib_arr);
	apt_string_assign(&attribs->name,codec_name,capabilities->attrib_arr->pool);
	attribs->sample_rates = sample_rates;
	attribs->bits_per_sample = 0;
	return TRUE;
}

/** Add default (linear PCM) capabilities */
MPF_DECLARE(apt_bool_t) mpf_codec_default_capabilities_add(mpf_codec_capabilities_t *capabilities);

/** Validate codec capabilities */
static APR_INLINE apt_bool_t mpf_codec_capabilities_validate(mpf_codec_capabilities_t *capabilities)
{
	if(apr_is_empty_array(capabilities->attrib_arr) == TRUE) {
		mpf_codec_default_capabilities_add(capabilities);
	}
	return TRUE;
}



/** Find matched descriptor in codec list */
MPF_DECLARE(mpf_codec_descriptor_t*) mpf_codec_list_descriptor_find(const mpf_codec_list_t *codec_list, const mpf_codec_descriptor_t *descriptor);

/** Match codec list with specified capabilities */
MPF_DECLARE(apt_bool_t) mpf_codec_list_match(mpf_codec_list_t *codec_list, const mpf_codec_capabilities_t *capabilities);

/** Intersect two codec lists */
MPF_DECLARE(apt_bool_t) mpf_codec_lists_intersect(mpf_codec_list_t *codec_list1, mpf_codec_list_t *codec_list2);


/** Get sampling rate mask (mpf_sample_rate_e) by integer value  */
MPF_DECLARE(int) mpf_sample_rate_mask_get(apr_uint16_t sampling_rate);


APT_END_EXTERN_C

#endif /* MPF_CODEC_DESCRIPTOR_H */
