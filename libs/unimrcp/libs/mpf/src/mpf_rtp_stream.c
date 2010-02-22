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

#include <apr_network_io.h>
#include "apt_net.h"
#include "mpf_rtp_stream.h"
#include "mpf_termination.h"
#include "mpf_codec_manager.h"
#include "mpf_timer_manager.h"
#include "mpf_rtp_header.h"
#include "mpf_rtcp_packet.h"
#include "mpf_rtp_defs.h"
#include "mpf_rtp_pt.h"
#include "apt_log.h"

/** Max size of RTP packet */
#define MAX_RTP_PACKET_SIZE  1500
/** Max size of RTCP packet */
#define MAX_RTCP_PACKET_SIZE 1500

/* Reason strings used in RTCP BYE messages (informative only) */
#define RTCP_BYE_SESSION_ENDED "Session ended"
#define RTCP_BYE_TALKSPURT_ENDED "Talskpurt ended"

#if ENABLE_RTP_PACKET_TRACE
#define RTP_TRACE printf
#else
static APR_INLINE void null_trace() {}
#define RTP_TRACE null_trace
#endif

/** RTP stream */
typedef struct mpf_rtp_stream_t mpf_rtp_stream_t;
struct mpf_rtp_stream_t {
	mpf_audio_stream_t         *base;

	mpf_rtp_media_descriptor_t *local_media;
	mpf_rtp_media_descriptor_t *remote_media;
	mpf_media_state_e           state;

	rtp_transmitter_t           transmitter;
	rtp_receiver_t              receiver;

	mpf_rtp_config_t           *config;

	apr_socket_t               *rtp_socket;
	apr_socket_t               *rtcp_socket;
	apr_sockaddr_t             *rtp_l_sockaddr;
	apr_sockaddr_t             *rtp_r_sockaddr;
	apr_sockaddr_t             *rtcp_l_sockaddr;
	apr_sockaddr_t             *rtcp_r_sockaddr;

	mpf_timer_t                *rtcp_tx_timer;
	mpf_timer_t                *rtcp_rx_timer;
	
	apr_pool_t                 *pool;
};

static apt_bool_t mpf_rtp_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t mpf_rtp_rx_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t mpf_rtp_rx_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t mpf_rtp_stream_receive(mpf_audio_stream_t *stream, mpf_frame_t *frame);
static apt_bool_t mpf_rtp_tx_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t mpf_rtp_tx_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t mpf_rtp_stream_transmit(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t vtable = {
	mpf_rtp_stream_destroy,
	mpf_rtp_rx_stream_open,
	mpf_rtp_rx_stream_close,
	mpf_rtp_stream_receive,
	mpf_rtp_tx_stream_open,
	mpf_rtp_tx_stream_close,
	mpf_rtp_stream_transmit
};

static apt_bool_t mpf_rtp_socket_pair_create(mpf_rtp_stream_t *stream, mpf_rtp_media_descriptor_t *local_media);
static void mpf_rtp_socket_pair_close(mpf_rtp_stream_t *stream);

static apt_bool_t mpf_rtcp_report_send(mpf_rtp_stream_t *stream);
static apt_bool_t mpf_rtcp_bye_send(mpf_rtp_stream_t *stream, apt_str_t *reason);
static void mpf_rtcp_tx_timer_proc(mpf_timer_t *timer, void *obj);
static void mpf_rtcp_rx_timer_proc(mpf_timer_t *timer, void *obj);


MPF_DECLARE(mpf_audio_stream_t*) mpf_rtp_stream_create(mpf_termination_t *termination, mpf_rtp_config_t *config, apr_pool_t *pool)
{
	mpf_rtp_stream_t *rtp_stream = apr_palloc(pool,sizeof(mpf_rtp_stream_t));
	mpf_stream_capabilities_t *capabilities = mpf_stream_capabilities_create(STREAM_DIRECTION_DUPLEX,pool);
	mpf_audio_stream_t *audio_stream = mpf_audio_stream_create(rtp_stream,&vtable,capabilities,pool);
	if(!audio_stream) {
		return NULL;
	}

	audio_stream->direction = STREAM_DIRECTION_NONE;
	audio_stream->termination = termination;

	rtp_stream->base = audio_stream;
	rtp_stream->pool = pool;
	rtp_stream->config = config;
	rtp_stream->local_media = NULL;
	rtp_stream->remote_media = NULL;
	rtp_stream->rtp_socket = NULL;
	rtp_stream->rtcp_socket = NULL;
	rtp_stream->rtp_l_sockaddr = NULL;
	rtp_stream->rtp_r_sockaddr = NULL;
	rtp_stream->rtcp_l_sockaddr = NULL;
	rtp_stream->rtcp_r_sockaddr = NULL;
	rtp_stream->rtcp_tx_timer = NULL;
	rtp_stream->rtcp_rx_timer = NULL;
	rtp_stream->state = MPF_MEDIA_DISABLED;
	rtp_receiver_init(&rtp_stream->receiver);
	rtp_transmitter_init(&rtp_stream->transmitter);
	rtp_stream->transmitter.sr_stat.ssrc = (apr_uint32_t)apr_time_now();

	if(config->rtcp == TRUE) {
		if(config->rtcp_tx_interval) {
			rtp_stream->rtcp_tx_timer = mpf_timer_create(
										termination->timer_manager,
										mpf_rtcp_tx_timer_proc,
										rtp_stream, pool);
		}
		if(config->rtcp_rx_resolution) {
			rtp_stream->rtcp_rx_timer = mpf_timer_create(
										termination->timer_manager,
										mpf_rtcp_rx_timer_proc,
										rtp_stream, pool);
		}
	}

	return audio_stream;
}

static apt_bool_t mpf_rtp_stream_local_media_create(mpf_rtp_stream_t *rtp_stream, mpf_rtp_media_descriptor_t *local_media, mpf_rtp_media_descriptor_t *remote_media, mpf_stream_capabilities_t *capabilities)
{
	apt_bool_t status = TRUE;
	if(!local_media) {
		/* local media is not specified, create the default one */
		local_media = apr_palloc(rtp_stream->pool,sizeof(mpf_rtp_media_descriptor_t));
		mpf_rtp_media_descriptor_init(local_media);
		local_media->state = MPF_MEDIA_ENABLED;
		local_media->direction = STREAM_DIRECTION_DUPLEX;
	}
	if(remote_media) {
		local_media->id = remote_media->id;
	}
	if(local_media->ip.length == 0) {
		local_media->ip = rtp_stream->config->ip;
		local_media->ext_ip = rtp_stream->config->ext_ip;
	}
	if(local_media->port == 0) {
		/* RTP port management */
		apr_port_t first_port_in_search = rtp_stream->config->rtp_port_cur;
		apt_bool_t is_port_ok = FALSE;

		do {
			local_media->port = rtp_stream->config->rtp_port_cur;
			rtp_stream->config->rtp_port_cur += 2;
			if(rtp_stream->config->rtp_port_cur == rtp_stream->config->rtp_port_max) {
				rtp_stream->config->rtp_port_cur = rtp_stream->config->rtp_port_min;
			}
			if(mpf_rtp_socket_pair_create(rtp_stream,local_media) == TRUE) {
				is_port_ok = TRUE;
			}
		} while((is_port_ok == FALSE) && (first_port_in_search != rtp_stream->config->rtp_port_cur));
		if(is_port_ok == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Find Free RTP Port");
			local_media->state = MPF_MEDIA_DISABLED;
			status = FALSE;
		}
	}
	else if(mpf_rtp_socket_pair_create(rtp_stream,local_media) == FALSE) {
		local_media->state = MPF_MEDIA_DISABLED;
		status = FALSE;
	}

	if(rtp_stream->config->ptime) {
		local_media->ptime = rtp_stream->config->ptime;
	}

	if(mpf_codec_list_is_empty(&local_media->codec_list) == TRUE) {
		if(mpf_codec_list_is_empty(&rtp_stream->config->codec_list) == TRUE) {
			mpf_codec_manager_codec_list_get(
								rtp_stream->base->termination->codec_manager,
								&local_media->codec_list,
								rtp_stream->pool);
		}
		else {
			mpf_codec_list_copy(&local_media->codec_list,
								&rtp_stream->config->codec_list,
								rtp_stream->pool);
		}
		
		if(capabilities) {
			mpf_codec_list_modify(&local_media->codec_list,&capabilities->codecs);
		}
	}

	rtp_stream->local_media = local_media;
	return status;
}

static apt_bool_t mpf_rtp_stream_local_media_update(mpf_rtp_stream_t *rtp_stream, mpf_rtp_media_descriptor_t *media, mpf_stream_capabilities_t *capabilities)
{
	apt_bool_t status = TRUE;
	if(apt_string_compare(&rtp_stream->local_media->ip,&media->ip) == FALSE ||
		rtp_stream->local_media->port != media->port) {

		if(mpf_rtp_socket_pair_create(rtp_stream,media) == FALSE) {
			media->state = MPF_MEDIA_DISABLED;
			status = FALSE;
		}
	}
	if(mpf_codec_list_is_empty(&media->codec_list) == TRUE) {
		mpf_codec_manager_codec_list_get(
							rtp_stream->base->termination->codec_manager,
							&media->codec_list,
							rtp_stream->pool);
		if(capabilities) {
			mpf_codec_list_modify(&media->codec_list,&capabilities->codecs);
		}
	}

	rtp_stream->local_media = media;
	return status;
}

static apt_bool_t mpf_rtp_stream_remote_media_update(mpf_rtp_stream_t *rtp_stream, mpf_rtp_media_descriptor_t *media)
{
	apt_bool_t status = TRUE;
	if(media->state == MPF_MEDIA_ENABLED) {
		if(!rtp_stream->remote_media || 
			apt_string_compare(&rtp_stream->remote_media->ip,&media->ip) == FALSE ||
			rtp_stream->remote_media->port != media->port) {

			/* update RTP port */
			rtp_stream->rtp_r_sockaddr = NULL;
			apr_sockaddr_info_get(
				&rtp_stream->rtp_r_sockaddr,
				media->ip.buf,
				APR_INET,
				media->port,
				0,
				rtp_stream->pool);
			if(!rtp_stream->rtp_r_sockaddr) {
				status = FALSE;
			}

			/* update RTCP port */
			rtp_stream->rtcp_r_sockaddr = NULL;
			apr_sockaddr_info_get(
				&rtp_stream->rtcp_r_sockaddr,
				media->ip.buf,
				APR_INET,
				media->port+1,
				0,
				rtp_stream->pool);
		}
	}

	rtp_stream->remote_media = media;
	return status;
}

static apt_bool_t mpf_rtp_stream_media_negotiate(mpf_rtp_stream_t *rtp_stream)
{
	mpf_rtp_media_descriptor_t *local_media = rtp_stream->local_media;
	mpf_rtp_media_descriptor_t *remote_media = rtp_stream->remote_media;
	if(!local_media || !remote_media) {
		return FALSE;
	}

	local_media->id = remote_media->id;
	local_media->mid = remote_media->mid;
	local_media->ptime = remote_media->ptime;

	if(rtp_stream->state == MPF_MEDIA_DISABLED && remote_media->state == MPF_MEDIA_ENABLED) {
		/* enable RTP/RTCP session */
		rtp_stream->state = MPF_MEDIA_ENABLED;
		if(rtp_stream->rtp_l_sockaddr) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Enable RTP Session %s:%hu",
				rtp_stream->rtp_l_sockaddr->hostname,
				rtp_stream->rtp_l_sockaddr->port);
		}

		if(rtp_stream->rtcp_tx_timer) {
			mpf_timer_set(rtp_stream->rtcp_tx_timer,rtp_stream->config->rtcp_tx_interval);
		}
		if(rtp_stream->rtcp_rx_timer) {
			mpf_timer_set(rtp_stream->rtcp_rx_timer,rtp_stream->config->rtcp_rx_resolution);
		}
	}
	else if(rtp_stream->state == MPF_MEDIA_ENABLED && remote_media->state == MPF_MEDIA_DISABLED) {
		/* disable RTP/RTCP session */
		rtp_stream->state = MPF_MEDIA_DISABLED;
		if(rtp_stream->rtp_l_sockaddr) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Disable RTP Session %s:%hu",
				rtp_stream->rtp_l_sockaddr->hostname,
				rtp_stream->rtp_l_sockaddr->port);
		}

		if(rtp_stream->rtcp_tx_timer) {
			mpf_timer_kill(rtp_stream->rtcp_tx_timer);
		}
		if(rtp_stream->rtcp_rx_timer) {
			mpf_timer_kill(rtp_stream->rtcp_rx_timer);
		}
		if(rtp_stream->config->rtcp == TRUE && rtp_stream->config->rtcp_bye_policy != RTCP_BYE_DISABLE) {
			apt_str_t reason = {RTCP_BYE_SESSION_ENDED, sizeof(RTCP_BYE_SESSION_ENDED)-1};
			mpf_rtcp_bye_send(rtp_stream,&reason);
		}
	}

	local_media->state = remote_media->state;
	local_media->direction = mpf_stream_reverse_direction_get(remote_media->direction);
	rtp_stream->base->direction = local_media->direction;

	if(remote_media->state == MPF_MEDIA_ENABLED) {
		if(mpf_codec_list_is_empty(&remote_media->codec_list) == TRUE) {
			/* no remote codecs available, initialize them according to the local codecs  */
			mpf_codec_list_copy(&remote_media->codec_list,
								&local_media->codec_list,
								rtp_stream->pool);
		}

		/* intersect local and remote codecs */
		if(rtp_stream->config->own_preferrence == TRUE) {
			mpf_codec_lists_intersect(
				&local_media->codec_list,
				&remote_media->codec_list);
		}
		else {
			mpf_codec_lists_intersect(
				&remote_media->codec_list,
				&local_media->codec_list);
		}
	}

	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_rtp_stream_add(mpf_audio_stream_t *stream)
{
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_rtp_stream_remove(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	
	if(rtp_stream->state == MPF_MEDIA_ENABLED) {
		/* disable RTP/RTCP session */
		rtp_stream->state = MPF_MEDIA_DISABLED;
		if(rtp_stream->rtp_l_sockaddr) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove RTP Session %s:%hu",
				rtp_stream->rtp_l_sockaddr->hostname,
				rtp_stream->rtp_l_sockaddr->port);
		}

		if(rtp_stream->rtcp_tx_timer) {
			mpf_timer_kill(rtp_stream->rtcp_tx_timer);
		}
		if(rtp_stream->rtcp_rx_timer) {
			mpf_timer_kill(rtp_stream->rtcp_rx_timer);
		}
		if(rtp_stream->config->rtcp == TRUE && rtp_stream->config->rtcp_bye_policy != RTCP_BYE_DISABLE) {
			apt_str_t reason = {RTCP_BYE_SESSION_ENDED, sizeof(RTCP_BYE_SESSION_ENDED)-1};
			mpf_rtcp_bye_send(rtp_stream,&reason);
		}
	}
	
	mpf_rtp_socket_pair_close(rtp_stream);
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_rtp_stream_modify(mpf_audio_stream_t *stream, mpf_rtp_stream_descriptor_t *descriptor)
{
	apt_bool_t status = TRUE;
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	if(!rtp_stream) {
		return FALSE;
	}

	if(!rtp_stream->local_media) {
		/* create local media */
		status = mpf_rtp_stream_local_media_create(rtp_stream,descriptor->local,descriptor->remote,descriptor->capabilities);
	}
	else if(descriptor->local) {
		/* update local media */
		status = mpf_rtp_stream_local_media_update(rtp_stream,descriptor->local,descriptor->capabilities);
	}
	
	if(descriptor->remote && status == TRUE) {
		/* update remote media */
		mpf_rtp_stream_remote_media_update(rtp_stream,descriptor->remote);

		/* negotiate local and remote media */
		mpf_rtp_stream_media_negotiate(rtp_stream);
	}

	if((rtp_stream->base->direction & STREAM_DIRECTION_SEND) == STREAM_DIRECTION_SEND) {
		mpf_codec_list_t *codec_list = &rtp_stream->remote_media->codec_list;
		rtp_stream->base->tx_descriptor = codec_list->primary_descriptor;
		if(rtp_stream->base->tx_descriptor) {
			rtp_stream->transmitter.samples_per_frame = 
				(apr_uint32_t)mpf_codec_frame_samples_calculate(rtp_stream->base->tx_descriptor);
		}
		if(codec_list->event_descriptor) {
			rtp_stream->base->tx_event_descriptor = codec_list->event_descriptor;
		}
	}
	if((rtp_stream->base->direction & STREAM_DIRECTION_RECEIVE) == STREAM_DIRECTION_RECEIVE) {
		mpf_codec_list_t *codec_list = &rtp_stream->local_media->codec_list;
		rtp_stream->base->rx_descriptor = codec_list->primary_descriptor;
		if(codec_list->event_descriptor) {
			rtp_stream->base->rx_event_descriptor = codec_list->event_descriptor;
		}
	}

	if(!descriptor->local) {
		descriptor->local = rtp_stream->local_media;
	}
	return status;
}


static apt_bool_t mpf_rtp_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_receiver_t *receiver = &rtp_stream->receiver;
	if(!rtp_stream->rtp_socket || !rtp_stream->rtp_l_sockaddr || !rtp_stream->rtp_r_sockaddr) {
		return FALSE;
	}

	receiver->jb = mpf_jitter_buffer_create(
						&rtp_stream->config->jb_config,
						stream->rx_descriptor,
						codec,
						rtp_stream->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open RTP Receiver %s:%hu <- %s:%hu playout [%"APR_SIZE_T_FMT" ms]",
			rtp_stream->rtp_l_sockaddr->hostname,
			rtp_stream->rtp_l_sockaddr->port,
			rtp_stream->rtp_r_sockaddr->hostname,
			rtp_stream->rtp_r_sockaddr->port,
			rtp_stream->config->jb_config.initial_playout_delay);
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_close(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_receiver_t *receiver = &rtp_stream->receiver;

	if(!rtp_stream->rtp_l_sockaddr || !rtp_stream->rtp_r_sockaddr) {
		return FALSE;
	}

	receiver->stat.lost_packets = 0;
	if(receiver->stat.received_packets) {
		apr_uint32_t expected_packets = receiver->history.seq_cycles + 
			receiver->history.seq_num_max - receiver->history.seq_num_base + 1;
		if(expected_packets > receiver->stat.received_packets) {
			receiver->stat.lost_packets = expected_packets - receiver->stat.received_packets;
		}
	}

	mpf_jitter_buffer_destroy(receiver->jb);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close RTP Receiver %s:%hu <- %s:%hu [r:%lu l:%lu j:%lu]",
			rtp_stream->rtp_l_sockaddr->hostname,
			rtp_stream->rtp_l_sockaddr->port,
			rtp_stream->rtp_r_sockaddr->hostname,
			rtp_stream->rtp_r_sockaddr->port,
			receiver->stat.received_packets,
			receiver->stat.lost_packets,
			receiver->rr_stat.jitter);
	return TRUE;
}


static APR_INLINE void rtp_rx_overall_stat_reset(rtp_receiver_t *receiver)
{
	memset(&receiver->stat,0,sizeof(receiver->stat));
	memset(&receiver->history,0,sizeof(receiver->history));
	memset(&receiver->periodic_history,0,sizeof(receiver->periodic_history));
}

static APR_INLINE void rtp_rx_stat_init(rtp_receiver_t *receiver, rtp_header_t *header, apr_time_t *time)
{
	receiver->rr_stat.ssrc = header->ssrc;
	receiver->history.seq_num_base = receiver->history.seq_num_max = (apr_uint16_t)header->sequence;
	receiver->history.ts_last = header->timestamp;
	receiver->history.time_last = *time;
}

static APR_INLINE void rtp_rx_restart(rtp_receiver_t *receiver)
{
	apr_byte_t restarts = ++receiver->stat.restarts;
	rtp_rx_overall_stat_reset(receiver);
	mpf_jitter_buffer_restart(receiver->jb);
	receiver->stat.restarts = restarts;
}

static rtp_header_t* rtp_rx_header_skip(void **buffer, apr_size_t *size)
{
	apr_size_t offset = 0;
	rtp_header_t *header = (rtp_header_t*)*buffer;

    /* RTP header validity check */
	if(header->version != RTP_VERSION) {
		return NULL;
	}

    /* calculate payload offset */
	offset = sizeof(rtp_header_t) + (header->count * sizeof(apr_uint32_t));

	/* additional offset in case of RTP extension */
	if(header->extension) {
		rtp_extension_header_t *ext_header = (rtp_extension_header_t*)(((apr_byte_t*)*buffer)+offset);
		offset += (ntohs(ext_header->length) * sizeof(apr_uint32_t));
	}

	if (offset >= *size) {
		return NULL;
	}

	/* skip to payload */
	*buffer = (apr_byte_t*)*buffer + offset;
	*size = *size - offset;

	return header;
}

static APR_INLINE void rtp_periodic_history_update(rtp_receiver_t *receiver)
{
	apr_uint32_t expected_packets;
	apr_uint32_t expected_interval;
	apr_uint32_t received_interval;
	apr_uint32_t lost_interval;

	/* calculate expected packets */
	expected_packets = receiver->history.seq_cycles + 
		receiver->history.seq_num_max - receiver->history.seq_num_base + 1;

	/* calculate expected interval */
	expected_interval = expected_packets - receiver->periodic_history.expected_prior;
	/* update expected prior */
	receiver->periodic_history.expected_prior = expected_packets;

	/* calculate received interval */
	received_interval = receiver->stat.received_packets - receiver->periodic_history.received_prior;
	/* update received prior */
	receiver->periodic_history.received_prior = receiver->stat.received_packets;
	/* calculate lost interval */
	if(expected_interval > received_interval) {
		lost_interval = expected_interval - received_interval;
	}
	else {
		lost_interval = 0;
	}

	/* update lost fraction */
	if(expected_interval == 0 || lost_interval == 0) {
		receiver->rr_stat.fraction = 0;
	}
	else {
		receiver->rr_stat.fraction = (lost_interval << 8) / expected_interval;
	}

	if(expected_packets > receiver->stat.received_packets) {
		receiver->rr_stat.lost = expected_packets - receiver->stat.received_packets;
	}
	else {
		receiver->rr_stat.lost = 0;
	}

	receiver->periodic_history.discarded_prior = receiver->stat.discarded_packets;
	receiver->periodic_history.jitter_min = receiver->rr_stat.jitter;
	receiver->periodic_history.jitter_max = receiver->rr_stat.jitter;
}

typedef enum {
	RTP_SSRC_UPDATE,
	RTP_SSRC_PROBATION,
	RTP_SSRC_RESTART
} rtp_ssrc_result_e;

static APR_INLINE rtp_ssrc_result_e rtp_rx_ssrc_update(rtp_receiver_t *receiver, apr_uint32_t ssrc)
{
	if(receiver->rr_stat.ssrc == ssrc) {
		/* known ssrc */
		if(receiver->history.ssrc_probation) {
			/* reset the probation for new ssrc */
			receiver->history.ssrc_probation = 0;
			receiver->history.ssrc_new = 0;
		}
	}
	else {
		if(receiver->history.ssrc_new == ssrc) {
			if(--receiver->history.ssrc_probation == 0) {
				/* restart with new ssrc */
				receiver->rr_stat.ssrc = ssrc;
				return RTP_SSRC_RESTART;
			}
			else {
				return RTP_SSRC_PROBATION;
			}
		}
		else {
			/* start probation for new ssrc */
			receiver->history.ssrc_new = ssrc;
			receiver->history.ssrc_probation = 5;
			return RTP_SSRC_PROBATION;
		}
	}
	return RTP_SSRC_UPDATE;
}

typedef enum {
	RTP_SEQ_UPDATE,
	RTP_SEQ_MISORDER,
	RTP_SEQ_DRIFT
} rtp_seq_result_e;

static APR_INLINE rtp_seq_result_e rtp_rx_seq_update(rtp_receiver_t *receiver, apr_uint16_t seq_num)
{
	rtp_seq_result_e result = RTP_SEQ_UPDATE;
	apr_uint16_t seq_delta = seq_num - receiver->history.seq_num_max;
	if(seq_delta < MAX_DROPOUT) {
		if(seq_num < receiver->history.seq_num_max) {
			/* sequence number wrapped */
			receiver->history.seq_cycles += RTP_SEQ_MOD;
		}
		receiver->history.seq_num_max = seq_num;
	}
	else if(seq_delta <= RTP_SEQ_MOD - MAX_MISORDER) {
		/* sequence number made a very large jump */
		result = RTP_SEQ_DRIFT;
	}
	else {
		/* duplicate or misordered packet */
		result = RTP_SEQ_MISORDER;
	}
	receiver->stat.received_packets++;

	return result;
}

typedef enum {
	RTP_TS_UPDATE,
	RTP_TS_DRIFT
} rtp_ts_result_e;

static APR_INLINE rtp_ts_result_e rtp_rx_ts_update(rtp_receiver_t *receiver, mpf_codec_descriptor_t *descriptor, apr_time_t *time, apr_uint32_t ts)
{
	apr_int32_t deviation;

	/* arrival time diff in samples */
	deviation = (apr_int32_t)apr_time_as_msec(*time - receiver->history.time_last) * 
		descriptor->channel_count * descriptor->sampling_rate / 1000;
	/* arrival timestamp diff */
	deviation -= ts - receiver->history.ts_last;

	if(deviation < 0) {
		deviation = -deviation;
	}

	if(deviation > DEVIATION_THRESHOLD) {
		return RTP_TS_DRIFT;
	}

	receiver->rr_stat.jitter += deviation - ((receiver->rr_stat.jitter + 8) >> 4);
	RTP_TRACE("jitter=%d deviation=%d\n",receiver->rr_stat.jitter,deviation);
	receiver->history.time_last = *time;
	receiver->history.ts_last = ts;

	if(receiver->rr_stat.jitter < receiver->periodic_history.jitter_min) {
		receiver->periodic_history.jitter_min = receiver->rr_stat.jitter;
	}
	if(receiver->rr_stat.jitter > receiver->periodic_history.jitter_max) {
		receiver->periodic_history.jitter_max = receiver->rr_stat.jitter;
	}
	return RTP_TS_UPDATE;
}

static APR_INLINE void rtp_rx_failure_threshold_check(rtp_receiver_t *receiver)
{
	apr_uint32_t received;
	apr_uint32_t discarded;
	received = receiver->stat.received_packets - receiver->periodic_history.received_prior;
	discarded = receiver->stat.discarded_packets - receiver->periodic_history.discarded_prior;

	if(discarded * 100 > received * DISCARDED_TO_RECEIVED_RATIO_THRESHOLD) {
		/* failure threshold hired, restart */
		rtp_rx_restart(receiver);
	}
}

static apt_bool_t rtp_rx_packet_receive(mpf_rtp_stream_t *rtp_stream, void *buffer, apr_size_t size)
{
	rtp_receiver_t *receiver = &rtp_stream->receiver;
	mpf_codec_descriptor_t *descriptor = rtp_stream->base->rx_descriptor;
	apr_time_t time;
	rtp_ssrc_result_e ssrc_result;
	rtp_header_t *header = rtp_rx_header_skip(&buffer,&size);
	if(!header) {
		/* invalid RTP packet */
		receiver->stat.invalid_packets++;
		return FALSE;
	}

	header->sequence = ntohs((apr_uint16_t)header->sequence);
	header->timestamp = ntohl(header->timestamp);
	header->ssrc = ntohl(header->ssrc);

	time = apr_time_now();

	RTP_TRACE("RTP time=%6lu ssrc=%8lx pt=%3u %cts=%9lu seq=%5u size=%lu\n",
					(apr_uint32_t)apr_time_usec(time),
					header->ssrc, header->type, (header->marker == 1) ? '*' : ' ',
					header->timestamp, header->sequence, size);
	if(!receiver->stat.received_packets) {
		/* initialization */
		rtp_rx_stat_init(receiver,header,&time);
	}

	ssrc_result = rtp_rx_ssrc_update(receiver,header->ssrc);
	if(ssrc_result == RTP_SSRC_PROBATION) {
		receiver->stat.invalid_packets++;
		return FALSE;
	}
	else if(ssrc_result == RTP_SSRC_RESTART) {
		rtp_rx_restart(receiver);
		rtp_rx_stat_init(receiver,header,&time);
	}

	rtp_rx_seq_update(receiver,(apr_uint16_t)header->sequence);
	
	if(header->type == descriptor->payload_type) {
		/* codec */
		if(rtp_rx_ts_update(receiver,descriptor,&time,header->timestamp) == RTP_TS_DRIFT) {
			rtp_rx_restart(receiver);
			return FALSE;
		}
	
		if(mpf_jitter_buffer_write(receiver->jb,buffer,size,header->timestamp) != JB_OK) {
			receiver->stat.discarded_packets++;
			rtp_rx_failure_threshold_check(receiver);
		}
	}
	else if(rtp_stream->base->rx_event_descriptor && 
		header->type == rtp_stream->base->rx_event_descriptor->payload_type) {
		/* named event */
		mpf_named_event_frame_t *named_event = (mpf_named_event_frame_t *)buffer;
		named_event->duration = ntohs((apr_uint16_t)named_event->duration);
		if(mpf_jitter_buffer_event_write(receiver->jb,named_event,header->timestamp,(apr_byte_t)header->marker) != JB_OK) {
			receiver->stat.discarded_packets++;
		}
	}
	else if(header->type == RTP_PT_CN) {
		/* CN packet */
		receiver->stat.ignored_packets++;
	}
	else {
		/* invalid payload type */
		receiver->stat.ignored_packets++;
	}
	
	return TRUE;
}

static apt_bool_t rtp_rx_process(mpf_rtp_stream_t *rtp_stream)
{
	char buffer[MAX_RTP_PACKET_SIZE];
	apr_size_t size = sizeof(buffer);
	apr_size_t max_count = 5;
	while(max_count && apr_socket_recv(rtp_stream->rtp_socket,buffer,&size) == APR_SUCCESS) {
		rtp_rx_packet_receive(rtp_stream,buffer,size);

		size = sizeof(buffer);
		max_count--;
	}
	return TRUE;
}

static apt_bool_t mpf_rtp_stream_receive(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_rx_process(rtp_stream);

	return mpf_jitter_buffer_read(rtp_stream->receiver.jb,frame);
}


static apt_bool_t mpf_rtp_tx_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	apr_size_t frame_size;
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_transmitter_t *transmitter = &rtp_stream->transmitter;

	if(!rtp_stream->rtp_socket || !rtp_stream->rtp_l_sockaddr || !rtp_stream->rtp_r_sockaddr) {
		return FALSE;
	}

	if(!codec) {
		return FALSE;
	}

	if(!transmitter->ptime) {
		if(rtp_stream->config && rtp_stream->config->ptime) {
			transmitter->ptime = rtp_stream->config->ptime;
		}
		else {
			transmitter->ptime = 20;
		}
	}
	transmitter->packet_frames = transmitter->ptime / CODEC_FRAME_TIME_BASE;
	transmitter->current_frames = 0;

	frame_size = mpf_codec_frame_size_calculate(
							stream->tx_descriptor,
							codec->attribs);
	transmitter->packet_data = apr_palloc(
							rtp_stream->pool,
							sizeof(rtp_header_t) + transmitter->packet_frames * frame_size);
	
	transmitter->inactivity = 1;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open RTP Transmitter %s:%hu -> %s:%hu",
			rtp_stream->rtp_l_sockaddr->hostname,
			rtp_stream->rtp_l_sockaddr->port,
			rtp_stream->rtp_r_sockaddr->hostname,
			rtp_stream->rtp_r_sockaddr->port);
	return TRUE;
}

static apt_bool_t mpf_rtp_tx_stream_close(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	if(!rtp_stream->rtp_l_sockaddr || !rtp_stream->rtp_r_sockaddr) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close RTP Transmitter %s:%hu -> %s:%hu [s:%lu o:%lu]",
			rtp_stream->rtp_l_sockaddr->hostname,
			rtp_stream->rtp_l_sockaddr->port,
			rtp_stream->rtp_l_sockaddr->hostname,
			rtp_stream->rtp_r_sockaddr->port,
			rtp_stream->transmitter.sr_stat.sent_packets,
			rtp_stream->transmitter.sr_stat.sent_octets);
	return TRUE;
}


static APR_INLINE void rtp_header_prepare(
					rtp_transmitter_t *transmitter,
					rtp_header_t *header,
					apr_byte_t payload_type,
					apr_byte_t marker,
					apr_uint32_t timestamp)
{
	header->version = RTP_VERSION;
	header->padding = 0;
	header->extension = 0;
	header->count = 0;
	header->marker = marker;
	header->type = payload_type;
	header->timestamp = timestamp;
	header->ssrc = htonl(transmitter->sr_stat.ssrc);
}

static APR_INLINE apt_bool_t mpf_rtp_data_send(mpf_rtp_stream_t *rtp_stream, rtp_transmitter_t *transmitter, const mpf_frame_t *frame)
{
	apt_bool_t status = TRUE;
	memcpy(
		transmitter->packet_data + transmitter->packet_size,
		frame->codec_frame.buffer,
		frame->codec_frame.size);
	transmitter->packet_size += frame->codec_frame.size;

	if(++transmitter->current_frames == transmitter->packet_frames) {
		rtp_header_t *header = (rtp_header_t*)transmitter->packet_data;
		header->sequence = htons(++transmitter->last_seq_num);
		RTP_TRACE("> RTP time=%6lu ssrc=%8lx pt=%3u %cts=%9lu seq=%5u\n",
			(apr_uint32_t)apr_time_usec(apr_time_now()),
			transmitter->sr_stat.ssrc, header->type, 
			(header->marker == 1) ? '*' : ' ',
			header->timestamp, transmitter->last_seq_num);
		header->timestamp = htonl(header->timestamp);
		if(apr_socket_sendto(
					rtp_stream->rtp_socket,
					rtp_stream->rtp_r_sockaddr,
					0,
					transmitter->packet_data,
					&transmitter->packet_size) == APR_SUCCESS) {
			transmitter->sr_stat.sent_packets++;
			transmitter->sr_stat.sent_octets += (apr_uint32_t)transmitter->packet_size - sizeof(rtp_header_t);
		}
		else {
			status = FALSE;
		}
		transmitter->current_frames = 0;
	}
	return status;
}

static APR_INLINE apt_bool_t mpf_rtp_event_send(mpf_rtp_stream_t *rtp_stream, rtp_transmitter_t *transmitter, const mpf_frame_t *frame)
{
	char packet_data[20];
	apr_size_t packet_size = sizeof(rtp_header_t) + sizeof(mpf_named_event_frame_t);
	rtp_header_t *header = (rtp_header_t*) packet_data;
	mpf_named_event_frame_t *named_event = (mpf_named_event_frame_t*)(header+1);
	rtp_header_prepare(
		transmitter,
		header,
		rtp_stream->base->tx_event_descriptor->payload_type,
		(frame->marker == MPF_MARKER_START_OF_EVENT) ? 1 : 0,
		transmitter->timestamp_base);

	*named_event = frame->event_frame;
	named_event->edge = (frame->marker == MPF_MARKER_END_OF_EVENT) ? 1 : 0;
	
	header->sequence = htons(++transmitter->last_seq_num);
	RTP_TRACE("> RTP time=%6lu ssrc=%8lx pt=%3u %cts=%9lu seq=%5u event=%2u dur=%3u %c\n",
		(apr_uint32_t)apr_time_usec(apr_time_now()),
		transmitter->sr_stat.ssrc, 
		header->type, (header->marker == 1) ? '*' : ' ',
		header->timestamp, transmitter->last_seq_num,
		named_event->event_id, named_event->duration,
		(named_event->edge == 1) ? '*' : ' ');

	header->timestamp = htonl(header->timestamp);
	named_event->duration = htons((apr_uint16_t)named_event->duration);
	if(apr_socket_sendto(
				rtp_stream->rtp_socket,
				rtp_stream->rtp_r_sockaddr,
				0,
				packet_data,
				&packet_size) != APR_SUCCESS) {
		return FALSE;
	}
	transmitter->sr_stat.sent_packets++;
	transmitter->sr_stat.sent_octets += sizeof(mpf_named_event_frame_t);
	return TRUE;
}

static apt_bool_t mpf_rtp_stream_transmit(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	apt_bool_t status = TRUE;
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_transmitter_t *transmitter = &rtp_stream->transmitter;

	transmitter->timestamp += transmitter->samples_per_frame;

	if(frame->type == MEDIA_FRAME_TYPE_NONE) {
		if(!transmitter->inactivity) {
			if(transmitter->current_frames == 0) {
				/* set inactivity (ptime alligned) */
				transmitter->inactivity = 1;
				if(rtp_stream->config->rtcp == TRUE && rtp_stream->config->rtcp_bye_policy == RTCP_BYE_PER_TALKSPURT) {
					apt_str_t reason = {RTCP_BYE_TALKSPURT_ENDED, sizeof(RTCP_BYE_TALKSPURT_ENDED)-1};
					mpf_rtcp_bye_send(rtp_stream,&reason);
				}
			}
			else {
				/* ptime allignment */
				status = mpf_rtp_data_send(rtp_stream,transmitter,frame);
			}
		}
		return status;
	}

	if((frame->type & MEDIA_FRAME_TYPE_EVENT) == MEDIA_FRAME_TYPE_EVENT){
		/* transmit event as soon as received */
		if(stream->tx_event_descriptor) {
			if(frame->marker == MPF_MARKER_START_OF_EVENT) {
				/* store start time (base) of the event */
				transmitter->timestamp_base = transmitter->timestamp;
			}
			else if(frame->marker == MPF_MARKER_NEW_SEGMENT) {
				/* update base in case of long-lasting events */
				transmitter->timestamp_base = transmitter->timestamp;
			}

			status = mpf_rtp_event_send(rtp_stream,transmitter,frame);
		}
	}

	if((frame->type & MEDIA_FRAME_TYPE_AUDIO) == MEDIA_FRAME_TYPE_AUDIO){
		if(transmitter->current_frames == 0) {
			rtp_header_t *header = (rtp_header_t*)transmitter->packet_data;
			rtp_header_prepare(
					transmitter,
					header,
					stream->tx_descriptor->payload_type,
					transmitter->inactivity,
					transmitter->timestamp);
			transmitter->packet_size = sizeof(rtp_header_t);
			if(transmitter->inactivity) {
				transmitter->inactivity = 0;
			}
		}
		status = mpf_rtp_data_send(rtp_stream,transmitter,frame);
	}

	return status;
}

static apr_socket_t* mpf_socket_create(apr_sockaddr_t **l_sockaddr, const char *ip, apr_port_t port, apr_pool_t *pool)
{
	apr_socket_t *socket = NULL;
	*l_sockaddr = NULL;
	apr_sockaddr_info_get(
		l_sockaddr,
		ip,
		APR_INET,
		port,
		0,
		pool);
	if(!*l_sockaddr) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Failed to Get Sockaddr %s:%hu",ip,port);
		return NULL;
	}
	if(apr_socket_create(&socket,(*l_sockaddr)->family,SOCK_DGRAM,0,pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Failed to Create Socket %s:%hu", ip,port);
		return NULL;
	}
	
	apr_socket_opt_set(socket,APR_SO_NONBLOCK,1);
	apr_socket_timeout_set(socket,0);
	apr_socket_opt_set(socket,APR_SO_REUSEADDR,1);

	if(apr_socket_bind(socket,*l_sockaddr) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Failed to Bind Socket to %s:%hu", ip,port);
		apr_socket_close(socket);
		return NULL;
	}
	return socket;
}

static apt_bool_t mpf_rtp_socket_pair_create(mpf_rtp_stream_t *stream, mpf_rtp_media_descriptor_t *local_media)
{
	stream->rtp_socket = mpf_socket_create(&stream->rtp_l_sockaddr,local_media->ip.buf,local_media->port,stream->pool);
	if(!stream->rtp_socket) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Failed to Create RTP Socket");
		return FALSE;
	}

	stream->rtcp_socket = mpf_socket_create(&stream->rtcp_l_sockaddr,local_media->ip.buf,local_media->port+1,stream->pool);
	if(!stream->rtcp_socket) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Failed to Create RTCP Socket");
	}
	return TRUE;
}

static void mpf_rtp_socket_pair_close(mpf_rtp_stream_t *stream)
{
	if(stream->rtp_socket) {
		apr_socket_close(stream->rtp_socket);
		stream->rtp_socket = NULL;
	}
	if(stream->rtcp_socket) {
		apr_socket_close(stream->rtcp_socket);
		stream->rtcp_socket = NULL;
	}
}



static APR_INLINE void rtcp_sr_generate(mpf_rtp_stream_t *rtp_stream, rtcp_sr_stat_t *sr_stat)
{
	*sr_stat = rtp_stream->transmitter.sr_stat;
	apt_ntp_time_get(&sr_stat->ntp_sec, &sr_stat->ntp_frac);
	sr_stat->rtp_ts = rtp_stream->transmitter.timestamp;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Generate RTCP SR [ssrc:%lu s:%lu o:%lu ts:%lu]",
				sr_stat->ssrc,
				sr_stat->sent_packets,
				sr_stat->sent_octets,
				sr_stat->rtp_ts);
	rtcp_sr_hton(sr_stat);
}

static APR_INLINE void rtcp_rr_generate(mpf_rtp_stream_t *rtp_stream, rtcp_rr_stat_t *rr_stat)
{
	*rr_stat = rtp_stream->receiver.rr_stat;
	rr_stat->last_seq =	rtp_stream->receiver.history.seq_num_max;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Generate RTCP RR [ssrc:%lu last_seq:%lu j:%lu lost:%lu frac:%d]",
				rr_stat->ssrc,
				rr_stat->last_seq,
				rr_stat->jitter,
				rr_stat->lost,
				rr_stat->fraction);
	rtcp_rr_hton(rr_stat);
}

/* Generate either RTCP SR or RTCP RR packet */
static APR_INLINE apr_size_t rtcp_report_generate(mpf_rtp_stream_t *rtp_stream, rtcp_packet_t *rtcp_packet, apr_size_t length)
{
	apr_size_t offset = 0;
	rtcp_header_init(&rtcp_packet->header,RTCP_RR);
	if(rtp_stream->base->direction & STREAM_DIRECTION_SEND) {
		rtcp_packet->header.pt = RTCP_SR;
	}
	if(rtp_stream->base->direction & STREAM_DIRECTION_RECEIVE) {
		rtcp_packet->header.count = 1;
	}
	offset += sizeof(rtcp_header_t);

	if(rtcp_packet->header.pt == RTCP_SR) {
		rtcp_sr_generate(rtp_stream,&rtcp_packet->r.sr.sr_stat);
		offset += sizeof(rtcp_sr_stat_t);
		if(rtcp_packet->header.count) {
			rtcp_rr_generate(rtp_stream,rtcp_packet->r.sr.rr_stat);
			offset += sizeof(rtcp_rr_stat_t);
		}
	}
	else if(rtcp_packet->header.pt == RTCP_RR) {
		rtcp_packet->r.rr.ssrc = htonl(rtp_stream->transmitter.sr_stat.ssrc);
		rtcp_rr_generate(rtp_stream,rtcp_packet->r.rr.rr_stat);
		offset += sizeof(rtcp_packet->r.rr);
	}
	rtcp_header_length_set(&rtcp_packet->header,offset);
	return offset;
}

/* Generate RTCP SDES packet */
static APR_INLINE apr_size_t rtcp_sdes_generate(mpf_rtp_stream_t *rtp_stream, rtcp_packet_t *rtcp_packet, apr_size_t length)
{
	rtcp_sdes_item_t *item;
	apr_size_t offset = 0;
	apr_size_t padding;
	rtcp_header_init(&rtcp_packet->header,RTCP_SDES);
	offset += sizeof(rtcp_header_t);

	rtcp_packet->header.count ++;
	rtcp_packet->r.sdes.ssrc = htonl(rtp_stream->transmitter.sr_stat.ssrc);
	offset += sizeof(apr_uint32_t);

	/* insert SDES CNAME item */
	item = &rtcp_packet->r.sdes.item[0];
	item->type = RTCP_SDES_CNAME;
	item->length = (apr_byte_t)rtp_stream->local_media->ip.length;
	memcpy(item->data,rtp_stream->local_media->ip.buf,item->length);
	offset += sizeof(rtcp_sdes_item_t) - 1 + item->length;
	
	/* terminate with end marker and pad to next 4-octet boundary */
	padding = 4 - (offset & 0x3);
	while(padding--) {
		item = (rtcp_sdes_item_t*) ((char*)rtcp_packet + offset);
		item->type = RTCP_SDES_END;
		offset++;
	}

	rtcp_header_length_set(&rtcp_packet->header,offset);
	return offset;
}

/* Generate RTCP BYE packet */
static APR_INLINE apr_size_t rtcp_bye_generate(mpf_rtp_stream_t *rtp_stream, rtcp_packet_t *rtcp_packet, apr_size_t length, apt_str_t *reason)
{
	apr_size_t offset = 0;
	rtcp_header_init(&rtcp_packet->header,RTCP_BYE);
	offset += sizeof(rtcp_header_t);

	rtcp_packet->r.bye.ssrc[0] = htonl(rtp_stream->transmitter.sr_stat.ssrc);
	rtcp_packet->header.count++;
	offset += rtcp_packet->header.count * sizeof(apr_uint32_t);

	if(reason->length) {
		apr_size_t padding;

		memcpy(rtcp_packet->r.bye.data,reason->buf,reason->length);
		rtcp_packet->r.bye.length = (apr_byte_t)reason->length;
		offset += rtcp_packet->r.bye.length;
	
		/* terminate with end marker and pad to next 4-octet boundary */
		padding = 4 - (reason->length & 0x3);
		if(padding) {
			char *end = rtcp_packet->r.bye.data + reason->length;
			memset(end,0,padding);
			offset += padding;
		}
	}

	rtcp_header_length_set(&rtcp_packet->header,offset);
	return offset;
}

/* Send compound RTCP packet (SR/RR + SDES) */
static apt_bool_t mpf_rtcp_report_send(mpf_rtp_stream_t *rtp_stream)
{
	char buffer[MAX_RTCP_PACKET_SIZE];
	apr_size_t length = 0;
	rtcp_packet_t *rtcp_packet;

	if(!rtp_stream->rtcp_socket || !rtp_stream->rtcp_l_sockaddr || !rtp_stream->rtcp_r_sockaddr) {
		/* session is not initialized */
		return FALSE;
	}

	if(rtp_stream->base->direction != STREAM_DIRECTION_NONE) {
		/* update periodic (prior) history */
		rtp_periodic_history_update(&rtp_stream->receiver);
	}

	rtcp_packet = (rtcp_packet_t*) (buffer + length);
	length += rtcp_report_generate(rtp_stream,rtcp_packet,sizeof(buffer)-length);

	rtcp_packet = (rtcp_packet_t*) (buffer + length);
	length += rtcp_sdes_generate(rtp_stream,rtcp_packet,sizeof(buffer)-length);
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send Compound RTCP Packet [%"APR_SIZE_T_FMT" bytes] %s:%hu -> %s:%hu",
		length,
		rtp_stream->rtcp_l_sockaddr->hostname,
		rtp_stream->rtcp_l_sockaddr->port,
		rtp_stream->rtcp_r_sockaddr->hostname,
		rtp_stream->rtcp_r_sockaddr->port);
	if(apr_socket_sendto(
				rtp_stream->rtcp_socket,
				rtp_stream->rtcp_r_sockaddr,
				0,
				buffer,
				&length) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send Compound RTCP Packet [%"APR_SIZE_T_FMT" bytes] %s:%hu -> %s:%hu",
			length,
			rtp_stream->rtcp_l_sockaddr->hostname,
			rtp_stream->rtcp_l_sockaddr->port,
			rtp_stream->rtcp_r_sockaddr->hostname,
			rtp_stream->rtcp_r_sockaddr->port);
		return FALSE;
	}
	return TRUE;
}

/* Send compound RTCP packet (SR/RR + SDES + BYE) */
static apt_bool_t mpf_rtcp_bye_send(mpf_rtp_stream_t *rtp_stream, apt_str_t *reason)
{
	char buffer[MAX_RTCP_PACKET_SIZE];
	apr_size_t length = 0;
	rtcp_packet_t *rtcp_packet;

	if(!rtp_stream->rtcp_socket || !rtp_stream->rtcp_l_sockaddr || !rtp_stream->rtcp_r_sockaddr) {
		/* session is not initialized */
		return FALSE;
	}

	if(rtp_stream->base->direction != STREAM_DIRECTION_NONE) {
		/* update periodic (prior) history */
		rtp_periodic_history_update(&rtp_stream->receiver);
	}

	rtcp_packet = (rtcp_packet_t*) (buffer + length);
	length += rtcp_report_generate(rtp_stream,rtcp_packet,sizeof(buffer)-length);

	rtcp_packet = (rtcp_packet_t*) (buffer + length);
	length += rtcp_sdes_generate(rtp_stream,rtcp_packet,sizeof(buffer)-length);

	rtcp_packet = (rtcp_packet_t*) (buffer + length);
	length += rtcp_bye_generate(rtp_stream,rtcp_packet,sizeof(buffer)-length,reason);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send Compound RTCP Packet [BYE] [%"APR_SIZE_T_FMT" bytes] %s:%hu -> %s:%hu",
		length,
		rtp_stream->rtcp_l_sockaddr->hostname,
		rtp_stream->rtcp_l_sockaddr->port,
		rtp_stream->rtcp_r_sockaddr->hostname,
		rtp_stream->rtcp_r_sockaddr->port);
	if(apr_socket_sendto(
				rtp_stream->rtcp_socket,
				rtp_stream->rtcp_r_sockaddr,
				0,
				buffer,
				&length) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send Compound RTCP Packet [BYE] [%"APR_SIZE_T_FMT" bytes] %s:%hu -> %s:%hu",
			length,
			rtp_stream->rtcp_l_sockaddr->hostname,
			rtp_stream->rtcp_l_sockaddr->port,
			rtp_stream->rtcp_r_sockaddr->hostname,
			rtp_stream->rtcp_r_sockaddr->port);
		return FALSE;
	}
	return TRUE;
}

static APR_INLINE void rtcp_sr_get(mpf_rtp_stream_t *rtp_stream, rtcp_sr_stat_t *sr_stat)
{
	rtcp_sr_ntoh(sr_stat);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get RTCP SR [ssrc:%lu s:%lu o:%lu ts:%lu]",
				sr_stat->ssrc,
				sr_stat->sent_packets,
				sr_stat->sent_octets,
				sr_stat->rtp_ts);
}

static APR_INLINE void rtcp_rr_get(mpf_rtp_stream_t *rtp_stream, rtcp_rr_stat_t *rr_stat)
{
	rtcp_rr_ntoh(rr_stat);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get RTCP RR [ssrc:%lu last_seq:%lu j:%lu lost:%lu frac:%d]",
				rr_stat->ssrc,
				rr_stat->last_seq,
				rr_stat->jitter,
				rr_stat->lost,
				rr_stat->fraction);
}

static apt_bool_t mpf_rtcp_compound_packet_receive(mpf_rtp_stream_t *rtp_stream, char *buffer, apr_size_t length)
{
	rtcp_packet_t *rtcp_packet = (rtcp_packet_t*) buffer;
	rtcp_packet_t *rtcp_packet_end;

	rtcp_packet_end = (rtcp_packet_t*)(buffer + length);

	while(rtcp_packet < rtcp_packet_end && rtcp_packet->header.version == RTP_VERSION) {
		rtcp_packet->header.length = ntohs((apr_uint16_t)rtcp_packet->header.length);
		
		if(rtcp_packet->header.pt == RTCP_SR) {
			/* RTCP SR */
			rtcp_sr_get(rtp_stream,&rtcp_packet->r.sr.sr_stat);
			if(rtcp_packet->header.count) {
				rtcp_rr_get(rtp_stream,rtcp_packet->r.sr.rr_stat);
			}
		}
		else if(rtcp_packet->header.pt == RTCP_RR) {
			/* RTCP RR */
			rtcp_packet->r.rr.ssrc = ntohl(rtcp_packet->r.rr.ssrc);
			if(rtcp_packet->header.count) {
				rtcp_rr_get(rtp_stream,rtcp_packet->r.rr.rr_stat);
			}
		}
		else if(rtcp_packet->header.pt == RTCP_SDES) {
			/* RTCP SDES */
		}
		else if(rtcp_packet->header.pt == RTCP_BYE) {
			/* RTCP BYE */
		}
		else {
			/* unknown RTCP packet */
		}

		/* get next RTCP packet */
		rtcp_packet = (rtcp_packet_t*)((apr_uint32_t*)rtcp_packet + rtcp_packet->header.length + 1);
	}

	if(rtcp_packet != rtcp_packet_end) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Malformed Compound RTCP Packet");
		return FALSE;
	}

	return TRUE;
}

static void mpf_rtcp_tx_timer_proc(mpf_timer_t *timer, void *obj)
{
	mpf_rtp_stream_t *rtp_stream = obj;

	/* generate and send RTCP compound report (SR/RR + SDES) */
	mpf_rtcp_report_send(rtp_stream);

	/* re-schedule timer */
	mpf_timer_set(timer,rtp_stream->config->rtcp_tx_interval);
}

static void mpf_rtcp_rx_timer_proc(mpf_timer_t *timer, void *obj)
{
	mpf_rtp_stream_t *rtp_stream = obj;
	if(rtp_stream->rtcp_socket && rtp_stream->rtcp_l_sockaddr && rtp_stream->rtcp_r_sockaddr) {
		char buffer[MAX_RTCP_PACKET_SIZE];
		apr_size_t length = sizeof(buffer);
		
		if(apr_socket_recv(rtp_stream->rtcp_socket,buffer,&length) == APR_SUCCESS) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive Compound RTCP Packet [%"APR_SIZE_T_FMT" bytes] %s:%hu <- %s:%hu",
					length,
					rtp_stream->rtcp_l_sockaddr->hostname,
					rtp_stream->rtcp_l_sockaddr->port,
					rtp_stream->rtcp_r_sockaddr->hostname,
					rtp_stream->rtcp_r_sockaddr->port);
			mpf_rtcp_compound_packet_receive(rtp_stream,buffer,length);
		}
	}

	/* re-schedule timer */
	mpf_timer_set(timer,rtp_stream->config->rtcp_rx_resolution);
}
