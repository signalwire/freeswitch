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
 * $Id: mpf_rtp_defs.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MPF_RTP_DEFS_H
#define MPF_RTP_DEFS_H

/**
 * @file mpf_rtp_defs.h
 * @brief Internal RTP Definitions
 */ 

#include "mpf_rtp_stat.h"
#include "mpf_jitter_buffer.h"

APT_BEGIN_EXTERN_C

/** Used to calculate actual number of received packets (32bit) in
 * case seq number (16bit) wrapped around */
#define RTP_SEQ_MOD (1 << 16)
/** Number of max dropout packets (seq numbers) is used to trigger 
 * either a drift in the seq numbers or a misorder packet */
#define MAX_DROPOUT 3000
/** Number of max misorder packets (seq numbers) is used to 
 * differentiate a drift in the seq numbers from a misorder packet */
#define MAX_MISORDER 100
/** Restart receiver if threshold is reached */
#define DISCARDED_TO_RECEIVED_RATIO_THRESHOLD 30 /* 30% */
/** Deviation threshold is used to trigger a drift in timestamps */
#define DEVIATION_THRESHOLD 4000
/** This threshold is used to detect a new talkspurt */
#define INTER_TALKSPURT_GAP 1000 /* msec */

/** RTP receiver history declaration */
typedef struct rtp_rx_history_t rtp_rx_history_t;
/** RTP receiver periodic history declaration */
typedef struct rtp_rx_periodic_history_t rtp_rx_periodic_history_t;
/** RTP receiver declaration */
typedef struct rtp_receiver_t rtp_receiver_t;
/** RTP transmitter declaration */
typedef struct rtp_transmitter_t rtp_transmitter_t;

/** History of RTP receiver */
struct rtp_rx_history_t {
	/** Updated on every seq num wrap around */
	apr_uint32_t seq_cycles;

	/** First seq num received */
	apr_uint16_t seq_num_base;
	/** Max seq num received */
	apr_uint16_t seq_num_max;

	/** Last timestamp received */
	apr_uint32_t ts_last;
	/** Local time measured on last packet received */
	apr_time_t   time_last;

	/** New ssrc, which is in probation */
	apr_uint32_t ssrc_new;
	/** Period of ssrc probation */
	apr_byte_t   ssrc_probation;
};

/** Periodic history of RTP receiver (initialized after every N packets) */
struct rtp_rx_periodic_history_t {
	/** Number of packets received */
	apr_uint32_t received_prior;
	/** Number of packets expected */
	apr_uint32_t expected_prior;
	/** Number of packets discarded */
	apr_uint32_t discarded_prior;

	/** Min jitter */
	apr_uint32_t jitter_min;
	/** Max jitter */
	apr_uint32_t jitter_max;
};

/** Reset RTP receiver history */
static APR_INLINE void mpf_rtp_rx_history_reset(rtp_rx_history_t *rx_history)
{
	memset(rx_history,0,sizeof(rtp_rx_history_t));
}

/** Reset RTP receiver periodic history */
static APR_INLINE void mpf_rtp_rx_periodic_history_reset(rtp_rx_periodic_history_t *rx_periodic_history)
{
	memset(rx_periodic_history,0,sizeof(rtp_rx_periodic_history_t));
}

/** RTP receiver */
struct rtp_receiver_t {
	/** Jitter buffer */
	mpf_jitter_buffer_t      *jb;

	/** RTCP statistics used in RR */
	rtcp_rr_stat_t            rr_stat;
	/** RTP receiver statistics */
	rtp_rx_stat_t             stat;
	/** RTP history */
	rtp_rx_history_t          history;
	/** RTP periodic history */
	rtp_rx_periodic_history_t periodic_history;
};


/** RTP transmitter */
struct rtp_transmitter_t {
	/** Packetization time in msec */
	apr_uint16_t    ptime;

	/** Number of frames in a packet */
	apr_uint16_t    packet_frames;
	/** Current number of frames */
	apr_uint16_t    current_frames;
	/** Samples in frames in timestamp units */
	apr_uint32_t    samples_per_frame;

	/** Indicate silence period among the talkspurts */
	apr_byte_t      inactivity;
	/** Last seq number sent */
	apr_uint16_t    last_seq_num;
	/** Current timestamp (samples processed) */
	apr_uint32_t    timestamp;
	/** Event timestamp base */
	apr_uint32_t    timestamp_base;

	/** RTP packet payload */
	char           *packet_data;
	/** RTP packet payload size */
	apr_size_t      packet_size;

	/** RTCP statistics used in SR */
	rtcp_sr_stat_t  sr_stat;
};


/** Initialize RTP receiver */
static APR_INLINE void rtp_receiver_init(rtp_receiver_t *receiver)
{
	receiver->jb = NULL;

	mpf_rtcp_rr_stat_reset(&receiver->rr_stat);
	mpf_rtp_rx_stat_reset(&receiver->stat);
	mpf_rtp_rx_history_reset(&receiver->history);
	mpf_rtp_rx_periodic_history_reset(&receiver->periodic_history);
}

/** Initialize RTP transmitter */
static APR_INLINE void rtp_transmitter_init(rtp_transmitter_t *transmitter)
{
	transmitter->ptime = 0;

	transmitter->packet_frames = 0;
	transmitter->current_frames = 0;
	transmitter->samples_per_frame = 0;

	transmitter->inactivity = 0;
	transmitter->last_seq_num = 0;
	transmitter->timestamp = 0;
	transmitter->timestamp_base = 0;

	transmitter->packet_data = NULL;
	transmitter->packet_size = 0;

	mpf_rtcp_sr_stat_reset(&transmitter->sr_stat);
}

APT_END_EXTERN_C

#endif /* MPF_RTP_DEFS_H */
