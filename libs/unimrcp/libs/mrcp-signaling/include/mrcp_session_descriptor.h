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

#ifndef __MRCP_SESSION_DESCRIPTOR_H__
#define __MRCP_SESSION_DESCRIPTOR_H__

/**
 * @file mrcp_session_descriptor.h
 * @brief MRCP Session Descriptor
 */ 

#include "mpf_rtp_descriptor.h"
#include "mrcp_sig_types.h"

APT_BEGIN_EXTERN_C

/** MRCP session status */
typedef enum {
	MRCP_SESSION_STATUS_OK,			          /**< OK */
	MRCP_SESSION_STATUS_NO_SUCH_RESOURCE,     /**< no such resource found */
	MRCP_SESSION_STATUS_UNACCEPTABLE_RESOURCE,/**< resource exists, but no implementation (plugin) found */
	MRCP_SESSION_STATUS_UNAVAILABLE_RESOURCE, /**< resource exists, but is temporary unavailable */
	MRCP_SESSION_STATUS_ERROR                 /**< internal error occuried */
} mrcp_session_status_e;

/** MRCP session descriptor */
struct mrcp_session_descriptor_t {
	/** SDP origin */
	apt_str_t             origin;
	/** Session level IP address */
	apt_str_t             ip;
	/** Session level external (NAT) IP address */
	apt_str_t             ext_ip;
	/** Session level resource name (MRCPv1 only) */
	apt_str_t             resource_name;
	/** Resource state (MRCPv1 only) */
	apt_bool_t            resource_state;
	/** Session status */
	mrcp_session_status_e status;

	/** MRCP control media array (mrcp_control_descriptor_t) */
	apr_array_header_t   *control_media_arr;
	/** Audio media array (mpf_rtp_media_descriptor_t) */
	apr_array_header_t   *audio_media_arr;
	/** Video media array (mpf_rtp_media_descriptor_t) */
	apr_array_header_t   *video_media_arr;
};

/** Create session descriptor  */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_session_descriptor_create(apr_pool_t *pool);

static APR_INLINE apr_size_t mrcp_session_media_count_get(const mrcp_session_descriptor_t *descriptor)
{
	return descriptor->control_media_arr->nelts + descriptor->audio_media_arr->nelts + descriptor->video_media_arr->nelts;
}

static APR_INLINE apr_size_t mrcp_session_control_media_add(mrcp_session_descriptor_t *descriptor, void *media)
{
	APR_ARRAY_PUSH(descriptor->control_media_arr,void*) = media;
	return mrcp_session_media_count_get(descriptor) - 1;
}

static APR_INLINE void* mrcp_session_control_media_get(const mrcp_session_descriptor_t *descriptor, apr_size_t id)
{
	if((int)id >= descriptor->control_media_arr->nelts) {
		return NULL;
	}
	return APR_ARRAY_IDX(descriptor->control_media_arr,id,void*);
}

static APR_INLINE apt_bool_t mrcp_session_control_media_set(mrcp_session_descriptor_t *descriptor, apr_size_t id, void *media)
{
	if((int)id >= descriptor->control_media_arr->nelts) {
		return FALSE;
	}
	APR_ARRAY_IDX(descriptor->control_media_arr,id,void*) = media;
	return TRUE;
}


static APR_INLINE apr_size_t mrcp_session_audio_media_add(mrcp_session_descriptor_t *descriptor, mpf_rtp_media_descriptor_t *media)
{
	APR_ARRAY_PUSH(descriptor->audio_media_arr,mpf_rtp_media_descriptor_t*) = media;
	return mrcp_session_media_count_get(descriptor) - 1;
}

static APR_INLINE mpf_rtp_media_descriptor_t* mrcp_session_audio_media_get(const mrcp_session_descriptor_t *descriptor, apr_size_t id)
{
	if((int)id >= descriptor->audio_media_arr->nelts) {
		return NULL;
	}
	return APR_ARRAY_IDX(descriptor->audio_media_arr,id,mpf_rtp_media_descriptor_t*);
}

static APR_INLINE apt_bool_t mrcp_session_audio_media_set(const mrcp_session_descriptor_t *descriptor, apr_size_t id, mpf_rtp_media_descriptor_t* media)
{
	if((int)id >= descriptor->audio_media_arr->nelts) {
		return FALSE;
	}
	APR_ARRAY_IDX(descriptor->audio_media_arr,id,mpf_rtp_media_descriptor_t*) = media;
	return TRUE;
}


static APR_INLINE apr_size_t mrcp_session_video_media_add(mrcp_session_descriptor_t *descriptor, mpf_rtp_media_descriptor_t *media)
{
	APR_ARRAY_PUSH(descriptor->video_media_arr,mpf_rtp_media_descriptor_t*) = media;
	return mrcp_session_media_count_get(descriptor) - 1;
}

static APR_INLINE mpf_rtp_media_descriptor_t* mrcp_session_video_media_get(const mrcp_session_descriptor_t *descriptor, apr_size_t id)
{
	if((int)id >= descriptor->video_media_arr->nelts) {
		return NULL;
	}
	return APR_ARRAY_IDX(descriptor->video_media_arr,id,mpf_rtp_media_descriptor_t*);
}

static APR_INLINE apt_bool_t mrcp_session_video_media_set(mrcp_session_descriptor_t *descriptor, apr_size_t id, mpf_rtp_media_descriptor_t* media)
{
	if((int)id >= descriptor->video_media_arr->nelts) {
		return FALSE;
	}
	APR_ARRAY_IDX(descriptor->video_media_arr,id,mpf_rtp_media_descriptor_t*) = media;
	return TRUE;
}

/** Get session status phrase  */
MRCP_DECLARE(const char*) mrcp_session_status_phrase_get(mrcp_session_status_e status);

APT_END_EXTERN_C

#endif /*__MRCP_SESSION_DESCRIPTOR_H__*/
