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

#ifndef __MPF_MEDIA_DESCRIPTOR_H__
#define __MPF_MEDIA_DESCRIPTOR_H__

/**
 * @file mpf_media_descriptor.h
 * @brief Media Descriptor Base
 */ 

#include <apr_network_io.h>
#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** MPF media state */
typedef enum {
	MPF_MEDIA_DISABLED, /**< disabled media */
	MPF_MEDIA_ENABLED   /**< enabled media */
} mpf_media_state_e;

/** MPF media descriptor declaration */
typedef struct mpf_media_descriptor_t mpf_media_descriptor_t;

/** MPF media descriptor */
struct mpf_media_descriptor_t {
	/** Media state (disabled/enabled)*/
	mpf_media_state_e state;

	/** Ip address */
	apt_str_t          ip;
	/** External (NAT) Ip address */
	apt_str_t          ext_ip;
	/** Port */
	apr_port_t         port;
	/** Identifier (0,1,...) */
	apr_size_t         id;
};

/** Initialize MPF media descriptor */
static APR_INLINE void mpf_media_descriptor_init(mpf_media_descriptor_t *descriptor)
{
	descriptor->state = MPF_MEDIA_DISABLED;
	apt_string_reset(&descriptor->ip);
	apt_string_reset(&descriptor->ext_ip);
	descriptor->port = 0;
	descriptor->id = 0;
}

APT_END_EXTERN_C

#endif /*__MPF_MEDIA_DESCRIPTOR_H__*/
