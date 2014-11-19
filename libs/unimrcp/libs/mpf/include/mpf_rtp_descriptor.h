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
 * $Id: mpf_rtp_descriptor.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MPF_RTP_DESCRIPTOR_H
#define MPF_RTP_DESCRIPTOR_H

/**
 * @file mpf_rtp_descriptor.h
 * @brief MPF RTP Stream Descriptor
 */ 

#include <apr_network_io.h>
#include "apt_string.h"
#include "mpf_stream_descriptor.h"

APT_BEGIN_EXTERN_C

/** RTP media descriptor declaration */
typedef struct mpf_rtp_media_descriptor_t mpf_rtp_media_descriptor_t;
/** RTP stream descriptor declaration */
typedef struct mpf_rtp_stream_descriptor_t mpf_rtp_stream_descriptor_t;
/** RTP termination descriptor declaration */
typedef struct mpf_rtp_termination_descriptor_t mpf_rtp_termination_descriptor_t;
/** RTP configuration declaration */
typedef struct mpf_rtp_config_t mpf_rtp_config_t;
/** RTP settings declaration */
typedef struct mpf_rtp_settings_t mpf_rtp_settings_t;
/** Jitter buffer configuration declaration */
typedef struct mpf_jb_config_t mpf_jb_config_t;

/** MPF media state */
typedef enum {
	MPF_MEDIA_DISABLED, /**< disabled media */
	MPF_MEDIA_ENABLED   /**< enabled media */
} mpf_media_state_e;

/** RTP media (local/remote) descriptor */
struct mpf_rtp_media_descriptor_t {
	/** Media state (disabled/enabled)*/
	mpf_media_state_e      state;
	/** Ip address */
	apt_str_t              ip;
	/** External (NAT) Ip address */
	apt_str_t              ext_ip;
	/** Port */
	apr_port_t             port;
	/** Stream mode (send/receive) */
	mpf_stream_direction_e direction;
	/** Packetization time */
	apr_uint16_t           ptime;
	/** Codec list */
	mpf_codec_list_t       codec_list;
	/** Media identifier */
	apr_size_t             mid;
	/** Position, order in SDP message (0,1,...) */
	apr_size_t             id;
};

/** RTP stream descriptor */
struct mpf_rtp_stream_descriptor_t {
	/** Stream capabilities */
	mpf_stream_capabilities_t  *capabilities;
	/** Local media descriptor */
	mpf_rtp_media_descriptor_t *local;
	/** Remote media descriptor */
	mpf_rtp_media_descriptor_t *remote;
	/** Settings loaded from config */
	mpf_rtp_settings_t         *settings;
};

/** RTP termination descriptor */
struct mpf_rtp_termination_descriptor_t {
	/** Audio stream descriptor */
	mpf_rtp_stream_descriptor_t audio;
	/** Video stream descriptor */
	mpf_rtp_stream_descriptor_t video;
};

/** Jitter buffer configuration */
struct mpf_jb_config_t {
	/** Min playout delay in msec */
	apr_uint32_t min_playout_delay;
	/** Initial playout delay in msec */
	apr_uint32_t initial_playout_delay;
	/** Max playout delay in msec */
	apr_uint32_t max_playout_delay;
	/** Mode of operation of the jitter buffer: static - 0, adaptive - 1 */
	apr_byte_t adaptive;
	/** Enable/disable time skew detection */
	apr_byte_t time_skew_detection;
};

/** RTCP BYE transmission policy */
typedef enum {
	RTCP_BYE_DISABLE,       /**< disable RTCP BYE transmission */
	RTCP_BYE_PER_SESSION,   /**< transmit RTCP BYE at the end of session */
	RTCP_BYE_PER_TALKSPURT  /**< transmit RTCP BYE at the end of each talkspurt (input) */
} rtcp_bye_policy_e;

/** RTP factory config */
struct mpf_rtp_config_t {
	/** Local IP address to bind to */
	apt_str_t         ip;
	/** External (NAT) IP address */
	apt_str_t         ext_ip;
	/** Min RTP port */
	apr_port_t        rtp_port_min;
	/** Max RTP port */
	apr_port_t        rtp_port_max;
	/** Current RTP port */
	apr_port_t        rtp_port_cur;
};

/** RTP settings */
struct mpf_rtp_settings_t {
	/** Packetization time */
	apr_uint16_t      ptime;
	/** Codec list */
	mpf_codec_list_t  codec_list;
	/** Preference in offer/anwser: 1 - own(local) preference, 0 - remote preference */
	apt_bool_t        own_preferrence;
	/** Enable/disable RTCP support */
	apt_bool_t        rtcp;
	/** RTCP BYE policy */
	rtcp_bye_policy_e rtcp_bye_policy;
	/** RTCP report transmission interval */
	apr_uint16_t      rtcp_tx_interval;
	/** RTCP rx resolution (timeout to check for a new RTCP message) */
	apr_uint16_t      rtcp_rx_resolution;
	/** Jitter buffer config */
	mpf_jb_config_t   jb_config;
};

/** Initialize media descriptor */
static APR_INLINE void mpf_rtp_media_descriptor_init(mpf_rtp_media_descriptor_t *media)
{
	media->state = MPF_MEDIA_DISABLED;
	apt_string_reset(&media->ip);
	apt_string_reset(&media->ext_ip);
	media->port = 0;
	media->direction = STREAM_DIRECTION_NONE;
	media->ptime = 0;
	mpf_codec_list_reset(&media->codec_list);
	media->mid = 0;
	media->id = 0;
}

/** Initialize stream descriptor */
static APR_INLINE void mpf_rtp_stream_descriptor_init(mpf_rtp_stream_descriptor_t *descriptor)
{
	descriptor->capabilities = NULL;
	descriptor->local = NULL;
	descriptor->remote = NULL;
	descriptor->settings = NULL;
}

/** Initialize RTP termination descriptor */
static APR_INLINE void mpf_rtp_termination_descriptor_init(mpf_rtp_termination_descriptor_t *rtp_descriptor)
{
	mpf_rtp_stream_descriptor_init(&rtp_descriptor->audio);
	mpf_rtp_stream_descriptor_init(&rtp_descriptor->video);
}

/** Initialize JB config */
static APR_INLINE void mpf_jb_config_init(mpf_jb_config_t *jb_config)
{
	jb_config->adaptive = 0;
	jb_config->initial_playout_delay = 0;
	jb_config->min_playout_delay = 0;
	jb_config->max_playout_delay = 0;
	jb_config->time_skew_detection = 1;
}

/** Allocate RTP config */
static APR_INLINE mpf_rtp_config_t* mpf_rtp_config_alloc(apr_pool_t *pool)
{
	mpf_rtp_config_t *rtp_config = (mpf_rtp_config_t*)apr_palloc(pool,sizeof(mpf_rtp_config_t));
	apt_string_reset(&rtp_config->ip);
	apt_string_reset(&rtp_config->ext_ip);
	rtp_config->rtp_port_cur = 0;
	rtp_config->rtp_port_min = 0;
	rtp_config->rtp_port_max = 0;
	return rtp_config;
}

/** Allocate RTP settings */
static APR_INLINE mpf_rtp_settings_t* mpf_rtp_settings_alloc(apr_pool_t *pool)
{
	mpf_rtp_settings_t *rtp_settings = (mpf_rtp_settings_t*)apr_palloc(pool,sizeof(mpf_rtp_settings_t));
	rtp_settings->ptime = 0;
	mpf_codec_list_init(&rtp_settings->codec_list,0,pool);
	rtp_settings->own_preferrence = FALSE;
	rtp_settings->rtcp = FALSE;
	rtp_settings->rtcp_bye_policy = RTCP_BYE_DISABLE;
	rtp_settings->rtcp_tx_interval = 0;
	rtp_settings->rtcp_rx_resolution = 0;
	mpf_jb_config_init(&rtp_settings->jb_config);
	return rtp_settings;
}


APT_END_EXTERN_C

#endif /* MPF_RTP_DESCRIPTOR_H */
