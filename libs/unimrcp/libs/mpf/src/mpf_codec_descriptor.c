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
 * $Id: mpf_codec_descriptor.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_codec_descriptor.h"
#include "mpf_named_event.h"
#include "mpf_rtp_pt.h"

/* linear PCM (host horder) */
#define LPCM_CODEC_NAME        "LPCM"
#define LPCM_CODEC_NAME_LENGTH (sizeof(LPCM_CODEC_NAME)-1)

/* linear PCM atrributes */
static const mpf_codec_attribs_t lpcm_attribs = {
	{LPCM_CODEC_NAME, LPCM_CODEC_NAME_LENGTH},    /* codec name */
	16,                                           /* bits per sample */
	MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000 |
	MPF_SAMPLE_RATE_32000 | MPF_SAMPLE_RATE_48000 /* supported sampling rates */
};

/** Find matched attribs in codec capabilities by descriptor specified */
static mpf_codec_attribs_t* mpf_codec_capabilities_attribs_find(const mpf_codec_capabilities_t *capabilities, const mpf_codec_descriptor_t *descriptor);


/** Get sampling rate mask (mpf_sample_rate_e) by integer value  */
MPF_DECLARE(int) mpf_sample_rate_mask_get(apr_uint16_t sampling_rate)
{
	switch(sampling_rate) {
		case 8000:
			return MPF_SAMPLE_RATE_8000;
		case 16000:
			return MPF_SAMPLE_RATE_16000;
		case 32000:
			return MPF_SAMPLE_RATE_32000;
		case 48000:
			return MPF_SAMPLE_RATE_48000;
	}
	return MPF_SAMPLE_RATE_NONE;
}

static APR_INLINE apt_bool_t mpf_sampling_rate_check(apr_uint16_t sampling_rate, int mask)
{
	return (mpf_sample_rate_mask_get(sampling_rate) & mask) ? TRUE : FALSE;
}

MPF_DECLARE(mpf_codec_descriptor_t*) mpf_codec_lpcm_descriptor_create(apr_uint16_t sampling_rate, apr_byte_t channel_count, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor = mpf_codec_descriptor_create(pool);
	descriptor->payload_type = RTP_PT_UNKNOWN;
	descriptor->name = lpcm_attribs.name;
	descriptor->sampling_rate = sampling_rate;
	descriptor->channel_count = channel_count;
	return descriptor;
}

/** Create codec descriptor by capabilities */
MPF_DECLARE(mpf_codec_descriptor_t*) mpf_codec_descriptor_create_by_capabilities(const mpf_codec_capabilities_t *capabilities, const mpf_codec_descriptor_t *peer, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor;
	mpf_codec_attribs_t *attribs = NULL;
	if(capabilities && peer) {
		attribs = mpf_codec_capabilities_attribs_find(capabilities,peer);
	}
	
	if(!attribs) {
		return mpf_codec_lpcm_descriptor_create(8000,1,pool);
	}

	descriptor = mpf_codec_descriptor_create(pool);
	*descriptor = *peer;
	if(apt_string_compare(&peer->name,&attribs->name) == FALSE) {
		descriptor->payload_type = RTP_PT_UNKNOWN;
		descriptor->name = attribs->name;
	}
	return descriptor;
}

/** Match two codec descriptors */
MPF_DECLARE(apt_bool_t) mpf_codec_descriptors_match(const mpf_codec_descriptor_t *descriptor1, const mpf_codec_descriptor_t *descriptor2)
{
	apt_bool_t match = FALSE;
	if(descriptor1->payload_type < RTP_PT_DYNAMIC && descriptor2->payload_type < RTP_PT_DYNAMIC) {
		if(descriptor1->payload_type == descriptor2->payload_type) {
			match = TRUE;
		}
	}
	else {
		if(apt_string_compare(&descriptor1->name,&descriptor2->name) == TRUE) {
			if(descriptor1->sampling_rate == descriptor2->sampling_rate && 
				descriptor1->channel_count == descriptor2->channel_count) {
				match = TRUE;
			}
		}
	}
	return match;
}

/** Match specified codec descriptor and the default lpcm one */
MPF_DECLARE(apt_bool_t) mpf_codec_lpcm_descriptor_match(const mpf_codec_descriptor_t *descriptor)
{
	return apt_string_compare(&descriptor->name,&lpcm_attribs.name);
}

/** Add default (linear PCM) capabilities */
MPF_DECLARE(apt_bool_t) mpf_codec_default_capabilities_add(mpf_codec_capabilities_t *capabilities)
{
	return mpf_codec_capabilities_add(capabilities,MPF_SAMPLE_RATE_8000,lpcm_attribs.name.buf);
}

/** Match codec descriptors by attribs specified */
MPF_DECLARE(apt_bool_t) mpf_codec_descriptor_match_by_attribs(mpf_codec_descriptor_t *descriptor, const mpf_codec_descriptor_t *static_descriptor, const mpf_codec_attribs_t *attribs)
{
	apt_bool_t match = FALSE;
	if(descriptor->payload_type < RTP_PT_DYNAMIC) {
		if(static_descriptor && static_descriptor->payload_type == descriptor->payload_type) {
			descriptor->name = static_descriptor->name;
			descriptor->sampling_rate = static_descriptor->sampling_rate;
			descriptor->channel_count = static_descriptor->channel_count;
			match = TRUE;
		}
	}
	else {
		if(apt_string_compare(&attribs->name,&descriptor->name) == TRUE) {
			if(mpf_sampling_rate_check(descriptor->sampling_rate,attribs->sample_rates) == TRUE) {
				match = TRUE;
			}
		}
	}
	return match;
}

/** Find matched descriptor in codec list */
MPF_DECLARE(mpf_codec_descriptor_t*) mpf_codec_list_descriptor_find(const mpf_codec_list_t *codec_list, const mpf_codec_descriptor_t *descriptor)
{
	int i;
	mpf_codec_descriptor_t *matched_descriptor;
	for(i=0; i<codec_list->descriptor_arr->nelts; i++) {
		matched_descriptor = &APR_ARRAY_IDX(codec_list->descriptor_arr,i,mpf_codec_descriptor_t);
		if(mpf_codec_descriptors_match(descriptor,matched_descriptor) == TRUE) {
			return matched_descriptor;
		}
	}
	return NULL;
}

/** Find matched attribs in codec capabilities by descriptor specified */
static mpf_codec_attribs_t* mpf_codec_capabilities_attribs_find(const mpf_codec_capabilities_t *capabilities, const mpf_codec_descriptor_t *descriptor)
{
	int i;
	mpf_codec_attribs_t *attribs;
	for(i=0; i<capabilities->attrib_arr->nelts; i++) {
		attribs = &APR_ARRAY_IDX(capabilities->attrib_arr,i,mpf_codec_attribs_t);
		if(mpf_sampling_rate_check(descriptor->sampling_rate,attribs->sample_rates) == TRUE) {
			return attribs;
		}
	}
	return NULL;
}

/** Match codec list with specified capabilities */
MPF_DECLARE(apt_bool_t) mpf_codec_list_match(mpf_codec_list_t *codec_list, const mpf_codec_capabilities_t *capabilities)
{
	int i;
	mpf_codec_descriptor_t *descriptor;
	apt_bool_t status = FALSE;
	if(!capabilities) {
		return FALSE;
	}

	for(i=0; i<codec_list->descriptor_arr->nelts; i++) {
		descriptor = &APR_ARRAY_IDX(codec_list->descriptor_arr,i,mpf_codec_descriptor_t);
		if(descriptor->enabled == FALSE) continue;

		/* match capabilities */
		if(mpf_codec_capabilities_attribs_find(capabilities,descriptor)) {
			/* at least one codec descriptor matches */
			status = TRUE;
		}
		else {
			descriptor->enabled = FALSE;
		}
	}

	return status;
}

/** Intersect two codec lists */
MPF_DECLARE(apt_bool_t) mpf_codec_lists_intersect(mpf_codec_list_t *codec_list1, mpf_codec_list_t *codec_list2)
{
	int i;
	mpf_codec_descriptor_t *descriptor1;
	mpf_codec_descriptor_t *descriptor2;
	codec_list1->primary_descriptor = NULL;
	codec_list1->event_descriptor = NULL;
	codec_list2->primary_descriptor = NULL;
	codec_list2->event_descriptor = NULL;
	/* find only one match for primary and named event descriptors,
	set the matched descriptors as preffered, disable the others */
	for(i=0; i<codec_list1->descriptor_arr->nelts; i++) {
		descriptor1 = &APR_ARRAY_IDX(codec_list1->descriptor_arr,i,mpf_codec_descriptor_t);
		if(descriptor1->enabled == FALSE) {
			/* this descriptor has been already disabled, process only enabled ones */
			continue;
		}

		/* check whether this is a named event descriptor */
		if(mpf_event_descriptor_check(descriptor1) == TRUE) {
			/* named event descriptor */
			if(codec_list1->event_descriptor) {
				/* named event descriptor has been already set, disable this one */
				descriptor1->enabled = FALSE;
			}
			else {
				/* find if there is a match */
				descriptor2 = mpf_codec_list_descriptor_find(codec_list2,descriptor1);
				if(descriptor2 && descriptor2->enabled == TRUE) {
					descriptor1->enabled = TRUE;
					codec_list1->event_descriptor = descriptor1;
					codec_list2->event_descriptor = descriptor2;
				}
				else {
					/* no match found, disable this descriptor */
					descriptor1->enabled = FALSE;
				}
			}
		}
		else {
			/* primary descriptor */
			if(codec_list1->primary_descriptor) {
				/* primary descriptor has been already set, disable this one */
				descriptor1->enabled = FALSE;
			}
			else {
				/* find if there is a match */
				descriptor2 = mpf_codec_list_descriptor_find(codec_list2,descriptor1);
				if(descriptor2 && descriptor2->enabled == TRUE) {
					descriptor1->enabled = TRUE;
					codec_list1->primary_descriptor = descriptor1;
					codec_list2->primary_descriptor = descriptor2;
				}
				else {
					/* no match found, disable this descriptor */
					descriptor1->enabled = FALSE;
				}
			}
		}
	}

	for(i=0; i<codec_list2->descriptor_arr->nelts; i++) {
		descriptor2 = &APR_ARRAY_IDX(codec_list2->descriptor_arr,i,mpf_codec_descriptor_t);
		if(descriptor2 == codec_list2->primary_descriptor || descriptor2 == codec_list2->event_descriptor) {
			descriptor2->enabled = TRUE;
		}
		else {
			descriptor2->enabled = FALSE;
		}
	}

	/* if primary descriptor is disabled or not set, return FALSE */
	if(!codec_list1->primary_descriptor || codec_list1->primary_descriptor->enabled == FALSE) {
		return FALSE;
	}

	return TRUE;
}
