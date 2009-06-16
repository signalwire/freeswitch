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

#ifndef __MPF_RTP_STAT_H__
#define __MPF_RTP_STAT_H__

/**
 * @file mpf_rtp_stat.h
 * @brief RTP Statistics
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** RTP transmit statistics declaration */
typedef struct rtp_tx_stat_t rtp_tx_stat_t;
/** RTP receive statistics declaration */
typedef struct rtp_rx_stat_t rtp_rx_stat_t;


/** RTP transmit statistics */
struct rtp_tx_stat_t {
	/** number of RTP packets received */
	apr_uint32_t    sent_packets;

	/* more to come */
};

/** RTP receive statistics */
struct rtp_rx_stat_t {
	/** number of valid RTP packets received */
	apr_uint32_t           received_packets;
	/** number of invalid RTP packets received */
	apr_uint32_t           invalid_packets;

	/** number of discarded in jitter buffer packets */
	apr_uint32_t           discarded_packets;
	/** number of ignored packets */
	apr_uint32_t           ignored_packets;

	/** number of lost in network packets */
	apr_uint32_t           lost_packets;

	/** number of restarts */
	apr_byte_t             restarts;

	/** network jitter (rfc3550) */
	apr_uint32_t           jitter;

	/** source id of received RTP stream */
	apr_uint32_t           ssrc;
};


/** Reset RTP transmit statistics */
static APR_INLINE void mpf_rtp_tx_stat_reset(rtp_tx_stat_t *tx_stat)
{
	memset(tx_stat,0,sizeof(rtp_tx_stat_t));
}

/** Reset RTP receive statistics */
static APR_INLINE void mpf_rtp_rx_stat_reset(rtp_rx_stat_t *rx_stat)
{
	memset(rx_stat,0,sizeof(rtp_rx_stat_t));
}

APT_END_EXTERN_C

#endif /*__MPF_RTP_STAT_H__*/
