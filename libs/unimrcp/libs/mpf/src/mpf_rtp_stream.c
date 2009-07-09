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
#include "mpf_rtp_stream.h"
#include "mpf_termination.h"
#include "mpf_codec_manager.h"
#include "mpf_rtp_header.h"
#include "mpf_rtp_defs.h"
#include "apt_log.h"


/** RTP stream */
typedef struct mpf_rtp_stream_t mpf_rtp_stream_t;
struct mpf_rtp_stream_t {
	mpf_audio_stream_t         *base;

	mpf_rtp_media_descriptor_t *local_media;
	mpf_rtp_media_descriptor_t *remote_media;

	rtp_transmitter_t           transmitter;
	rtp_receiver_t              receiver;

	mpf_rtp_config_t           *config;

	apr_socket_t               *socket;
	apr_sockaddr_t             *local_sockaddr;
	apr_sockaddr_t             *remote_sockaddr;
	
	apr_pool_t                 *pool;
};

static apt_bool_t mpf_rtp_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t mpf_rtp_rx_stream_open(mpf_audio_stream_t *stream);
static apt_bool_t mpf_rtp_rx_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t mpf_rtp_stream_receive(mpf_audio_stream_t *stream, mpf_frame_t *frame);
static apt_bool_t mpf_rtp_tx_stream_open(mpf_audio_stream_t *stream);
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

static apt_bool_t mpf_rtp_socket_create(mpf_rtp_stream_t *stream, mpf_rtp_media_descriptor_t *local_media);


MPF_DECLARE(mpf_audio_stream_t*) mpf_rtp_stream_create(mpf_termination_t *termination, mpf_rtp_config_t *config, apr_pool_t *pool)
{
	mpf_rtp_stream_t *rtp_stream = apr_palloc(pool,sizeof(mpf_rtp_stream_t));
	rtp_stream->pool = pool;
	rtp_stream->config = config;
	rtp_stream->local_media = NULL;
	rtp_stream->remote_media = NULL;
	rtp_stream->socket = NULL;
	rtp_stream->local_sockaddr = NULL;
	rtp_stream->remote_sockaddr = NULL;
	rtp_stream->base = mpf_audio_stream_create(rtp_stream,&vtable,STREAM_MODE_NONE,pool);
	rtp_stream->base->termination = termination;
	rtp_receiver_init(&rtp_stream->receiver);
	rtp_transmitter_init(&rtp_stream->transmitter);
	rtp_stream->transmitter.ssrc = (apr_uint32_t)apr_time_now();

	return rtp_stream->base;
}

static apt_bool_t mpf_rtp_stream_local_media_create(mpf_rtp_stream_t *rtp_stream, mpf_rtp_media_descriptor_t *local_media, mpf_rtp_media_descriptor_t *remote_media)
{
	apt_bool_t status = TRUE;
	if(!local_media) {
		/* local media is not specified, create the default one */
		local_media = apr_palloc(rtp_stream->pool,sizeof(mpf_rtp_media_descriptor_t));
		mpf_rtp_media_descriptor_init(local_media);
		local_media->base.state = MPF_MEDIA_ENABLED;
		local_media->mode = STREAM_MODE_SEND_RECEIVE;
	}
	if(remote_media) {
		local_media->base.id = remote_media->base.id;
	}
	if(local_media->base.ip.length == 0) {
		local_media->base.ip = rtp_stream->config->ip;
		local_media->base.ext_ip = rtp_stream->config->ext_ip;
	}
	if(local_media->base.port == 0) {
		/* RTP port management */
		apr_port_t first_port_in_search = rtp_stream->config->rtp_port_cur;
		apt_bool_t is_port_ok = FALSE;

		do {
			local_media->base.port = rtp_stream->config->rtp_port_cur;
			rtp_stream->config->rtp_port_cur += 2;
			if(rtp_stream->config->rtp_port_cur == rtp_stream->config->rtp_port_max) {
				rtp_stream->config->rtp_port_cur = rtp_stream->config->rtp_port_min;
			}
			if(mpf_rtp_socket_create(rtp_stream,local_media) == TRUE) {
				is_port_ok = TRUE;
			}
		} while((is_port_ok == FALSE) && (first_port_in_search != rtp_stream->config->rtp_port_cur));
		if(is_port_ok == FALSE) {
			local_media->base.state = MPF_MEDIA_DISABLED;
			status = FALSE;
		}
	}
	else if(mpf_rtp_socket_create(rtp_stream,local_media) == FALSE) {
		local_media->base.state = MPF_MEDIA_DISABLED;
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
	}

	rtp_stream->local_media = local_media;
	return status;
}

static apt_bool_t mpf_rtp_stream_local_media_update(mpf_rtp_stream_t *rtp_stream, mpf_rtp_media_descriptor_t *media)
{
	apt_bool_t status = TRUE;
	if(apt_string_compare(&rtp_stream->local_media->base.ip,&media->base.ip) == FALSE ||
		rtp_stream->local_media->base.port != media->base.port) {

		if(mpf_rtp_socket_create(rtp_stream,media) == FALSE) {
			media->base.state = MPF_MEDIA_DISABLED;
			status = FALSE;
		}
	}
	if(mpf_codec_list_is_empty(&media->codec_list) == TRUE) {
		mpf_codec_manager_codec_list_get(
							rtp_stream->base->termination->codec_manager,
							&media->codec_list,
							rtp_stream->pool);
	}

	rtp_stream->local_media = media;
	return status;
}

static apt_bool_t mpf_rtp_stream_remote_media_update(mpf_rtp_stream_t *rtp_stream, mpf_rtp_media_descriptor_t *media)
{
	apt_bool_t status = TRUE;
	if(!rtp_stream->remote_media || 
		apt_string_compare(&rtp_stream->remote_media->base.ip,&media->base.ip) == FALSE ||
		rtp_stream->remote_media->base.port != media->base.port) {

		rtp_stream->remote_sockaddr = NULL;
		apr_sockaddr_info_get(
			&rtp_stream->remote_sockaddr,
			media->base.ip.buf,
			APR_INET,
			media->base.port,
			0,
			rtp_stream->pool);
		if(!rtp_stream->remote_sockaddr) {
			status = FALSE;
		}
	}

	rtp_stream->remote_media = media;
	return status;
}

static apt_bool_t mpf_rtp_stream_media_negotiate(mpf_rtp_stream_t *rtp_stream)
{
	if(!rtp_stream->local_media || !rtp_stream->remote_media) {
		return FALSE;
	}

	rtp_stream->local_media->base.id = rtp_stream->remote_media->base.id;
	rtp_stream->local_media->base.state = rtp_stream->remote_media->base.state;

	rtp_stream->local_media->mid = rtp_stream->remote_media->mid;
	rtp_stream->local_media->ptime = rtp_stream->remote_media->ptime;
	rtp_stream->local_media->mode = mpf_stream_mode_negotiate(rtp_stream->remote_media->mode);

	if(mpf_codec_list_is_empty(&rtp_stream->remote_media->codec_list) == TRUE) {
		/* no remote codecs available, initialize them according to the local codecs  */
		mpf_codec_list_copy(&rtp_stream->remote_media->codec_list,
							&rtp_stream->local_media->codec_list,
							rtp_stream->pool);
	}

	/* intersect local and remote codecs */
	if(rtp_stream->config->own_preferrence == TRUE) {
		mpf_codec_list_intersect(&rtp_stream->local_media->codec_list,&rtp_stream->remote_media->codec_list);
	}
	else {
		mpf_codec_list_intersect(&rtp_stream->remote_media->codec_list,&rtp_stream->local_media->codec_list);
	}

	rtp_stream->base->mode = rtp_stream->local_media->mode;
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
		status = mpf_rtp_stream_local_media_create(rtp_stream,descriptor->local,descriptor->remote);
	}
	else if(descriptor->local) {
		/* update local media */
		status = mpf_rtp_stream_local_media_update(rtp_stream,descriptor->local);
	}
	
	if(descriptor->remote && status == TRUE) {
		/* update remote media */
		mpf_rtp_stream_remote_media_update(rtp_stream,descriptor->remote);

		/* negotiate local and remote media */
		mpf_rtp_stream_media_negotiate(rtp_stream);
	}

	if((rtp_stream->base->mode & STREAM_MODE_SEND) == STREAM_MODE_SEND) {
		rtp_stream->base->tx_codec = mpf_codec_manager_codec_get(
								rtp_stream->base->termination->codec_manager,
								rtp_stream->remote_media->codec_list.preffered,
								rtp_stream->pool);
		if(rtp_stream->base->tx_codec) {
			rtp_stream->transmitter.samples_per_frame = 
				(apr_uint32_t)mpf_codec_frame_samples_calculate(rtp_stream->base->tx_codec->descriptor);
		}
	}
	if((rtp_stream->base->mode & STREAM_MODE_RECEIVE) == STREAM_MODE_RECEIVE) {
		rtp_stream->base->rx_codec = mpf_codec_manager_codec_get(
								rtp_stream->base->termination->codec_manager,
								rtp_stream->local_media->codec_list.preffered,
								rtp_stream->pool);
	}

	if(!descriptor->local) {
		descriptor->local = rtp_stream->local_media;
	}
	return status;
}

static apt_bool_t mpf_rtp_stream_destroy(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	if(rtp_stream->socket) {
		apr_socket_close(rtp_stream->socket);
		rtp_stream->socket = NULL;
	}
	
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_open(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_receiver_t *receiver = &rtp_stream->receiver;
	if(!rtp_stream->socket || !rtp_stream->local_media) {
		return FALSE;
	}

	receiver->jb = mpf_jitter_buffer_create(
						&rtp_stream->config->jb_config,
						stream->rx_codec,
						rtp_stream->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open RTP Receive %s:%hu <- %s:%hu playout [%d ms]",
			rtp_stream->local_media->base.ip.buf,
			rtp_stream->local_media->base.port,
			rtp_stream->remote_media->base.ip.buf,
			rtp_stream->remote_media->base.port,
			rtp_stream->config->jb_config.initial_playout_delay);
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_close(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_receiver_t *receiver = &rtp_stream->receiver;
	receiver->stat.lost_packets = 0;
	if(receiver->stat.received_packets) {
		apr_uint32_t expected_packets = receiver->history.seq_cycles + 
			receiver->history.seq_num_max - receiver->history.seq_num_base + 1;
		if(expected_packets > receiver->stat.received_packets) {
			receiver->stat.lost_packets = expected_packets - receiver->stat.received_packets;
		}
	}

	mpf_jitter_buffer_destroy(receiver->jb);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close RTP Receive %s:%hu <- %s:%hu [r:%lu l:%lu j:%lu]",
			rtp_stream->local_media->base.ip.buf,
			rtp_stream->local_media->base.port,
			rtp_stream->remote_media->base.ip.buf,
			rtp_stream->remote_media->base.port,
			receiver->stat.received_packets,
			receiver->stat.lost_packets,
			receiver->stat.jitter);
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
	receiver->stat.ssrc = header->ssrc;
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

typedef enum {
	RTP_SSRC_UPDATE,
	RTP_SSRC_PROBATION,
	RTP_SSRC_RESTART
} rtp_ssrc_result_e;

static APR_INLINE rtp_ssrc_result_e rtp_rx_ssrc_update(rtp_receiver_t *receiver, apr_uint32_t ssrc)
{
	if(receiver->stat.ssrc == ssrc) {
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
				receiver->stat.ssrc = ssrc;
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

	if(receiver->stat.received_packets - receiver->periodic_history.received_prior >= 50) {
		receiver->periodic_history.received_prior = receiver->stat.received_packets;
		receiver->periodic_history.discarded_prior = receiver->stat.discarded_packets;
		receiver->periodic_history.jitter_min = receiver->stat.jitter;
		receiver->periodic_history.jitter_max = receiver->stat.jitter;
	}
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

	receiver->stat.jitter += deviation - ((receiver->stat.jitter + 8) >> 4);
#if PRINT_RTP_PACKET_STAT
	printf("jitter=%d deviation=%d\n",receiver->stat.jitter,deviation);
#endif
	receiver->history.time_last = *time;
	receiver->history.ts_last = ts;

	if(receiver->stat.jitter < receiver->periodic_history.jitter_min) {
		receiver->periodic_history.jitter_min = receiver->stat.jitter;
	}
	if(receiver->stat.jitter > receiver->periodic_history.jitter_max) {
		receiver->periodic_history.jitter_max = receiver->stat.jitter;
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

static apt_bool_t rtp_rx_packet_receive(rtp_receiver_t *receiver, mpf_codec_t *codec, void *buffer, apr_size_t size)
{
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

#if PRINT_RTP_PACKET_STAT
	printf("RTP time=%6lu ssrc=%8lx pt=%3u %cts=%9lu seq=%5u size=%lu\n",
					(apr_uint32_t)apr_time_usec(time),
					header->ssrc, header->type, header->marker ? '*' : ' ',
					header->timestamp, header->sequence, size);
#endif
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
	
	if(rtp_rx_ts_update(receiver,codec->descriptor,&time,header->timestamp) == RTP_TS_DRIFT) {
		rtp_rx_restart(receiver);
		return FALSE;
	}
	
	if(header->type == codec->descriptor->payload_type) {
		/* codec */
		if(mpf_jitter_buffer_write(receiver->jb,codec,buffer,size,header->timestamp) != JB_OK) {
			receiver->stat.discarded_packets++;
			rtp_rx_failure_threshold_check(receiver);
		}
	}
	else if(header->type == receiver->event_pt && receiver->event_pt != 0) {
		/* named event */
		mpf_named_event_frame_t *named_event = (mpf_named_event_frame_t *)buffer;
		if(mpf_jitter_buffer_write_named_event(receiver->jb,named_event,header->timestamp) != JB_OK) {
			receiver->stat.discarded_packets++;
			rtp_rx_failure_threshold_check(receiver);
		}
	}
	else if(header->type == 13 || header->type == 19) {
		/* CN packet*/
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
	char buffer[1500];
	apr_size_t size = sizeof(buffer);
	apr_size_t max_count = 5;
	while(max_count && apr_socket_recvfrom(rtp_stream->remote_sockaddr,rtp_stream->socket,0,buffer,&size) == APR_SUCCESS) {
		rtp_rx_packet_receive(&rtp_stream->receiver,rtp_stream->base->rx_codec,buffer,size);

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





static apt_bool_t mpf_rtp_tx_stream_open(mpf_audio_stream_t *stream)
{
	apr_size_t frame_size;
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_transmitter_t *transmitter = &rtp_stream->transmitter;
	if(!rtp_stream->socket || !rtp_stream->remote_media) {
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
							stream->tx_codec->descriptor,
							stream->tx_codec->attribs);
	transmitter->packet_data = apr_palloc(
							rtp_stream->pool,
							sizeof(rtp_header_t) + transmitter->packet_frames * frame_size);
	
	transmitter->inactivity = 1;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open RTP Transmit %s:%hu -> %s:%hu",
			rtp_stream->local_media->base.ip.buf,
			rtp_stream->local_media->base.port,
			rtp_stream->remote_media->base.ip.buf,
			rtp_stream->remote_media->base.port);
	return TRUE;
}

static apt_bool_t mpf_rtp_tx_stream_close(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close RTP Transmit %s:%hu -> %s:%hu [s:%lu]",
			rtp_stream->local_media->base.ip.buf,
			rtp_stream->local_media->base.port,
			rtp_stream->remote_media->base.ip.buf,
			rtp_stream->remote_media->base.port,
			rtp_stream->transmitter.stat.sent_packets);
	return TRUE;
}


static APR_INLINE void rtp_header_prepare(rtp_transmitter_t *transmitter, apr_byte_t payload_type)
{
	rtp_header_t *header = (rtp_header_t*)transmitter->packet_data;

#if PRINT_RTP_PACKET_STAT
	printf("> RTP time=%6lu ssrc=%8lx pt=%3u %cts=%9lu seq=%5u\n",
		(apr_uint32_t)apr_time_usec(apr_time_now()),
		transmitter->ssrc, payload_type, transmitter->inactivity ? '*' : ' ',
		transmitter->timestamp, transmitter->last_seq_num);
#endif	
	header->version = RTP_VERSION;
	header->padding = 0;
	header->extension = 0;
	header->count = 0;
	header->marker = transmitter->inactivity;
	header->type = payload_type;
	header->sequence = htons(++transmitter->last_seq_num);
	header->timestamp = htonl(transmitter->timestamp);
	header->ssrc = htonl(transmitter->ssrc);

	if(transmitter->inactivity) {
		transmitter->inactivity = 0;
	}

	transmitter->packet_size = sizeof(rtp_header_t);
}

static apt_bool_t mpf_rtp_stream_transmit(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	apt_bool_t status = TRUE;
	mpf_rtp_stream_t *rtp_stream = stream->obj;
	rtp_transmitter_t *transmitter = &rtp_stream->transmitter;

	transmitter->timestamp += transmitter->samples_per_frame;

	if(transmitter->current_frames == 0) {
		if(frame->type == MEDIA_FRAME_TYPE_NONE) {
			transmitter->inactivity = 1;
		}
		else {
			rtp_header_prepare(transmitter,stream->tx_codec->descriptor->payload_type);
		}
	}
	
	if(!transmitter->inactivity) {
		memcpy(
			transmitter->packet_data + transmitter->packet_size,
			frame->codec_frame.buffer,
			frame->codec_frame.size);
		transmitter->packet_size += frame->codec_frame.size;

		if(++transmitter->current_frames == transmitter->packet_frames) {
			if(apr_socket_sendto(
								rtp_stream->socket,
								rtp_stream->remote_sockaddr,
								0,
								transmitter->packet_data,
								&transmitter->packet_size) == APR_SUCCESS) {
				transmitter->stat.sent_packets++;
			}
			else {
				status = FALSE;
			}
			transmitter->current_frames = 0;
		}
	}

	return status;
}

static apt_bool_t mpf_rtp_socket_create(mpf_rtp_stream_t *stream, mpf_rtp_media_descriptor_t *local_media)
{
	if(stream->socket) {
		apr_socket_close(stream->socket);
		stream->socket = NULL;
	}
	
	stream->local_sockaddr = NULL;
	apr_sockaddr_info_get(
		&stream->local_sockaddr,
		local_media->base.ip.buf,
		APR_INET,
		local_media->base.port,
		0,
		stream->pool);
	if(!stream->local_sockaddr) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Failed to Get Sockaddr %s:%hu",
				local_media->base.ip.buf,
				local_media->base.port);
		return FALSE;
	}
	if(apr_socket_create(&stream->socket,stream->local_sockaddr->family,SOCK_DGRAM,0,stream->pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Failed to Create Socket %s:%hu",
				local_media->base.ip.buf,
				local_media->base.port);
		return FALSE;
	}
	
	apr_socket_opt_set(stream->socket,APR_SO_NONBLOCK,1);
	apr_socket_timeout_set(stream->socket,0);
	apr_socket_opt_set(stream->socket,APR_SO_REUSEADDR,1);

	if(apr_socket_bind(stream->socket,stream->local_sockaddr) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Failed to Bind Socket to %s:%hu",
				local_media->base.ip.buf,
				local_media->base.port);
		apr_socket_close(stream->socket);
		stream->socket = NULL;
		return FALSE;
	}
	return TRUE;
}
