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

#include "mrcp_session_descriptor.h"

MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_session_descriptor_create(apr_pool_t *pool)
{
	mrcp_session_descriptor_t *descriptor = apr_palloc(pool,sizeof(mrcp_session_descriptor_t));
	apt_string_reset(&descriptor->origin);
	apt_string_reset(&descriptor->ip);
	apt_string_reset(&descriptor->ext_ip);
	apt_string_reset(&descriptor->resource_name);
	descriptor->resource_state = FALSE;
	descriptor->status = MRCP_SESSION_STATUS_OK;
	descriptor->control_media_arr = apr_array_make(pool,1,sizeof(void*));
	descriptor->audio_media_arr = apr_array_make(pool,1,sizeof(mpf_rtp_media_descriptor_t*));
	descriptor->video_media_arr = apr_array_make(pool,0,sizeof(mpf_rtp_media_descriptor_t*));
	return descriptor;
}

MRCP_DECLARE(const char*) mrcp_session_status_phrase_get(mrcp_session_status_e status)
{
	switch(status) {
		case MRCP_SESSION_STATUS_OK:
			return "OK";
		case MRCP_SESSION_STATUS_NO_SUCH_RESOURCE:
			return "Not Found";
		case MRCP_SESSION_STATUS_UNACCEPTABLE_RESOURCE:
			return "Not Acceptable";
		case MRCP_SESSION_STATUS_UNAVAILABLE_RESOURCE:
			return "Unavailable";
		case MRCP_SESSION_STATUS_ERROR:
			return "Error";
	}
	return "Unknown";
}
