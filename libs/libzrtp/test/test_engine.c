/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 *
 * Viktor Krykun <v.krikun at zfoneproject.com>
 */

#include <stdio.h>	/* for sprintf(), remove() */
#include <string.h>	/* for string operations */

#include "test_engine.h"
#include "queue.h"

#define _ZTU_ "test engine"

#define K_ZRTP_TEST_MAX_ENDPOINTS 10
#define K_ZRTP_TEST_MAX_SESSIONS_PER_ENDPOINT 100
#define K_ZRTP_TEST_MAX_CHANNELS (K_ZRTP_TEST_MAX_ENDPOINTS * K_ZRTP_TEST_MAX_ENDPOINTS * ZRTP_MAX_STREAMS_PER_SESSION)

#define K_ZRTP_TEST_PROCESSORS_COUNT 2
#define K_ZRTP_TEST_RTP_RATE		200

extern uint8_t hash_word_list_odd[256][12];
extern uint8_t hash_word_list_even[256][10];

typedef struct {
	zrtp_test_id_t 		id;
	zrtp_test_id_t 		session_id;
	zrtp_test_id_t		channel_id;
	zrtp_test_id_t		endpoint_id;
	zrtp_stream_t 		*zrtp;
	uint16_t			seq;
	zrtp_queue_t 		*input;
	zrtp_queue_t 		*output;
	unsigned 			zrtp_events_queueu[128];
	unsigned 			zrtp_events_count;
} zrtp_test_stream_t;

typedef struct {
	zrtp_test_id_t id;
	zrtp_test_id_t endpoint_id;
	zrtp_test_session_cfg_t cfg;
	zrtp_session_t *zrtp;
	zrtp_test_stream_t streams[ZRTP_MAX_STREAMS_PER_SESSION];
	unsigned streams_count;
} zrtp_test_session_t;

typedef struct {
	zrtp_test_id_t id;
	char name[ZRTP_TEST_STR_LEN];
	zrtp_zid_t zid;
	zrtp_test_endpoint_cfg_t cfg;
	zrtp_test_session_t sessions[K_ZRTP_TEST_MAX_SESSIONS_PER_ENDPOINT];
	unsigned sessions_count;
	zrtp_global_t *zrtp;
	unsigned is_running;
	zrtp_queue_t *input_queue;
} zrtp_endpoint_t;


typedef struct {
	zrtp_test_id_t 		id;
	zrtp_test_stream_t	*left;
	zrtp_test_stream_t	*right;
	unsigned			is_attached;
	unsigned			is_secure;
} zrtp_test_channel_t;

typedef struct zrtp_test_packet {
	uint32_t			is_rtp;			/*! Defines is packet RTP or RTCP */
	uint32_t			length;			/*! Packet Length in bytes */
	char				body[1024];		/*! Packet body */
} zrtp_test_packet_t;


static zrtp_endpoint_t g_test_endpoints[K_ZRTP_TEST_MAX_ENDPOINTS];
static unsigned g_test_endpoints_count = 0;

static zrtp_test_channel_t g_test_channels[K_ZRTP_TEST_MAX_CHANNELS];
static unsigned g_test_channels_count = 0;

static int g_endpoints_counter = 7;
static int g_channels_counter = 7;
static int g_sessions_counter = 7;
static int g_streams_counter = 7;


zrtp_endpoint_t *zrtp_test_endpoint_by_id(zrtp_test_id_t id);
zrtp_test_stream_t *zrtp_test_stream_by_id(zrtp_test_id_t id);
zrtp_test_stream_t *zrtp_test_stream_by_peerid(zrtp_test_id_t id);
zrtp_test_session_t *zrtp_test_session_by_id(zrtp_test_id_t id);
zrtp_test_channel_t *zrtp_test_channel_by_id(zrtp_test_id_t id);


/******************************************************************************
 * libzrtp interface implementation
 */

static void on_zrtp_event(zrtp_stream_t *ctx, zrtp_protocol_event_t event) {
	zrtp_test_id_t *stream_id = zrtp_stream_get_userdata(ctx);
	zrtp_test_stream_t *stream = zrtp_test_stream_by_id(*stream_id);

	stream->zrtp_events_queueu[stream->zrtp_events_count++] = event;
}


static void on_zrtp_secure(zrtp_stream_t *ctx) {
	zrtp_test_id_t *stream_id = zrtp_stream_get_userdata(ctx);
	zrtp_test_stream_t *stream = zrtp_test_stream_by_id(*stream_id);
	zrtp_test_channel_t *channel = zrtp_test_channel_by_id(stream->channel_id);
	zrtp_test_stream_t *remote_stream = (channel->left == stream) ? channel->right : channel->left;

	if (stream->zrtp->state == ZRTP_STATE_SECURE &&
		remote_stream->zrtp->state == ZRTP_STATE_SECURE) {
		channel->is_secure = 1;
	}

}

static int on_send_packet(const zrtp_stream_t* ctx, char* message, unsigned int length) {
	zrtp_queue_elem_t* elem = zrtp_sys_alloc(sizeof(zrtp_queue_elem_t));
	if (elem) {
		zrtp_test_packet_t* packet = (zrtp_test_packet_t*) elem->data;
		elem->size = length;

		packet->is_rtp = 1;
		packet->length = length;
		zrtp_memcpy(packet->body, message, length);

		zrtp_test_id_t *stream_id = zrtp_stream_get_userdata(ctx);
		zrtp_test_stream_t *stream = zrtp_test_stream_by_id(*stream_id);
		if (stream) {
			zrtp_test_queue_push(stream->output, elem);
			return zrtp_status_ok;
		} else {
			return zrtp_status_fail;
		}
	} else {
		return zrtp_status_alloc_fail;
	}
}


/******************************************************************************
 * Processing Loop
 */

static zrtp_test_stream_t *get_stream_to_process_(zrtp_endpoint_t *endpoint) {
	zrtp_test_id_t all_streams[K_ZRTP_TEST_MAX_SESSIONS_PER_ENDPOINT*ZRTP_MAX_STREAMS_PER_SESSION];
	unsigned streams_count = 0;
	unsigned i, j;

	for (i=0; i<endpoint->sessions_count; i++) {
		for (j=0; j<endpoint->sessions[i].streams_count; j++) {
			zrtp_test_stream_t *stream = &endpoint->sessions[i].streams[j];
			if (stream->input && stream->output)
				all_streams[streams_count++] = stream->id;
		}
	}

	if (0 == streams_count)
		return NULL;

	zrtp_randstr(endpoint->zrtp, (unsigned char*)&i, sizeof(i));
	j = (unsigned)i;
	j = j % streams_count;

	//printf("trace>>> CHOOSE stream Endpoint=%u IDX=%u ID=%u\n", endpoint->id,  j, all_streams[j]);
	return zrtp_test_stream_by_id(all_streams[j]);
}


#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
DWORD WINAPI process_incoming(void *param)
#else
void *process_incoming(void *param)
#endif
{
	zrtp_endpoint_t *the_endpoint = (zrtp_endpoint_t *)param;

	while (the_endpoint->is_running) {
		zrtp_test_packet_t* packet = NULL;
		zrtp_queue_elem_t* elem = NULL;
		zrtp_status_t s = zrtp_status_fail;
		zrtp_test_stream_t *stream;
		int is_protocol = 0;

		// TODO: use peak to not to block processing if queue for this stream is empty
		elem = zrtp_test_queue_pop(the_endpoint->input_queue);
		if (!elem || elem->size <= 0) {
			if (elem) zrtp_sys_free(elem);
			break;
		}

		packet = (zrtp_test_packet_t*) elem->data;
		zrtp_test_id_t stream_id;
		{
			if (packet->is_rtp) {
				ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *rtp_hdr = (zrtp_rtp_hdr_t*)packet->body;
				stream_id = zrtp_ntoh32(rtp_hdr->ssrc); /* remember, we use stream Id as it's RTP SSRC */
			} else {
				ZRTP_UNALIGNED(zrtp_rtcp_hdr_t) *rtcp_hdr = (zrtp_rtcp_hdr_t*)packet->body;
				stream_id = zrtp_ntoh32(rtcp_hdr->ssrc); /* remember, we use stream Id as it's RTP SSRC */
			}
			stream = zrtp_test_stream_by_peerid(stream_id);
		}

		/*
		 * Process incoming packet by libzrtp. Is this a RTP media packet - copy it to the buffer
		 * to print out later.
		 */
		if (packet->is_rtp) {
			s = zrtp_process_srtp(stream->zrtp, packet->body, &packet->length);
		} else {
			s = zrtp_process_srtcp(stream->zrtp, packet->body, &packet->length);
		}

		if (!is_protocol) {
			char *body;
			if (packet->is_rtp) {
				body = packet->body + sizeof(zrtp_rtp_hdr_t);
				body[packet->length - sizeof(zrtp_rtp_hdr_t)] = 0;
			} else {
				body = packet->body + sizeof(zrtp_rtcp_hdr_t);
				body[packet->length - sizeof(zrtp_rtcp_hdr_t)] = 0;
			}

			switch (s)
			{
			case zrtp_status_ok: {
				ZRTP_LOG(1, (_ZTU_,"Incoming: (%s) [%p:ssrc=%u] OK. <%s> decrypted %d bytes.\n",
						zrtp_log_state2str(stream->zrtp->state), stream->zrtp,  stream->id, body, packet->length));
			} break;

			case zrtp_status_drop: {
				ZRTP_LOG(1, (_ZTU_,"Incoming: (%s) [%p:ssrc=%u] DROPPED. <%s>\n",
						zrtp_log_state2str(stream->zrtp->state), stream->zrtp, stream->id, body));
			} break;

			case zrtp_status_fail: {
				ZRTP_LOG(1, (_ZTU_,"Incoming: (%s) [%p:ssrc=%u] DECRYPT FAILED. <%s>\n",
						zrtp_log_state2str(stream->zrtp->state), stream->zrtp, stream->id, body));
			} break;

			default:
				break;
			}
		}

		zrtp_sys_free(elem);

		/*
		 * When zrtp_stream is in the pending clear state and other side wants to send plain
		 * traffic. We have to call zrtp_clear_stream().
		 */
		if (stream->zrtp->state == ZRTP_STATE_PENDINGCLEAR) {
			zrtp_stream_clear(stream->zrtp);
		}
	}
#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
	return 0;
#else
	return NULL;
#endif
}

#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
DWORD WINAPI process_outgoing(void *param)
#else
void *process_outgoing(void *param)
#endif
{
	unsigned packets_counter = 0;
	zrtp_endpoint_t *the_endpoint = (zrtp_endpoint_t *)param;

	while (the_endpoint->is_running) {
		zrtp_test_stream_t* stream = NULL;
		unsigned i;

		zrtp_status_t s = zrtp_status_fail;
		zrtp_test_packet_t* packet;
		zrtp_queue_elem_t* elem;
		char* word = NULL;

		zrtp_sleep(K_ZRTP_TEST_RTP_RATE);

		/* Get random channel to operate with and select random peer */
		stream = get_stream_to_process_(the_endpoint);
		if (!stream) {
			continue;
		}

		elem = zrtp_sys_alloc(sizeof(zrtp_queue_elem_t));
		if (!elem) {
			break;
		}
		packet = (zrtp_test_packet_t*) elem->data;
		packet->is_rtp = (packets_counter++ % 20); /* Every 20-th packet is RTCP */

		/*
		 * Construct RTP/RTCP Packet
		 */
		if (packet->is_rtp)
		{
			ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *rtp_hdr = (zrtp_rtp_hdr_t*)packet->body;

			/* Fill RTP Header according to the specification */
			zrtp_memset(rtp_hdr, 0, sizeof(zrtp_rtp_hdr_t));
			rtp_hdr->version = 2;			/* Current RTP version 2 */
			rtp_hdr->pt = 0;				/* PCMU padding type */
			rtp_hdr->ssrc = zrtp_hton32(stream->id);		/* Use stream Identifier as it's SSRC */
			if (stream->seq >= 0xFFFF) {
				stream->seq = 0;
			}
			rtp_hdr->seq = zrtp_hton16(stream->seq++);
			rtp_hdr->ts = zrtp_hton32((uint32_t)(zrtp_time_now()/1000));

			/* Get RTP body from PGP words lists */
			word = (char*)(i ? hash_word_list_odd[packets_counter % 256] : hash_word_list_even[packets_counter % 256]);

			zrtp_memcpy(packet->body + sizeof(zrtp_rtp_hdr_t), word, (uint32_t)strlen(word));
			packet->length = sizeof(zrtp_rtp_hdr_t) + (uint32_t)strlen(word);

			/* Process RTP media with libzrtp */
			s = zrtp_process_rtp(stream->zrtp, packet->body, &packet->length);
		}
		else {
			ZRTP_UNALIGNED(zrtp_rtcp_hdr_t) *rtcp_hdr = (zrtp_rtcp_hdr_t*)packet->body;

			/* Fill RTCP Header according to the specification */
			rtcp_hdr->rc = 0;
			rtcp_hdr->version = 2;
			rtcp_hdr->ssrc = stream->id;

			/* Get RTP body from PGP words lists. Put RTCP marker at the beginning */
			zrtp_memcpy(packet->body + sizeof(zrtp_rtcp_hdr_t), "RTCP", 4);
			word = (char*)( i ? hash_word_list_odd[packets_counter % 256] : hash_word_list_even[packets_counter % 256]);

			zrtp_memcpy(packet->body + sizeof(zrtp_rtcp_hdr_t) + 4, word, (uint32_t)strlen(word));
			packet->length = sizeof(zrtp_rtcp_hdr_t) + (uint32_t)strlen(word) + 4;
			/* RTCP packets sould be 32 byes aligned */
			packet->length += (packet->length % 4) ? (4 - packet->length % 4) : 0;

			/* Process RTCP control with libzrtp */
			s = zrtp_process_rtcp(stream->zrtp, packet->body, &packet->length);
		}

		elem->size = packet->length;

		/* Handle zrtp_process_xxx() instructions */
		switch (s) {
		/* Put the packet to the queue ==> send packet to the other side pear */
		case zrtp_status_ok: {
			ZRTP_LOG(3, (_ZTU_,"Outgoing: (%s) [%p:ssrc=%u] OK. <%s%s> encrypted %d bytes.\n",
					zrtp_log_state2str(stream->zrtp->state), stream->zrtp, stream->id, packet->is_rtp ? "" : "RTCP", word, packet->length));
			zrtp_test_queue_push(stream->output, elem);
		} break;

		case zrtp_status_drop: {
			ZRTP_LOG(1, (_ZTU_,"Outgoing: (%s) [%p:ssrc=%u] DROPPED.\n",
					zrtp_log_state2str(stream->zrtp->state), stream->zrtp, stream->id));
		} break;

		case zrtp_status_fail: {
			ZRTP_LOG(1, (_ZTU_,"Outgoing: (%s) [%p:ssrc=%u] ENCRYPT FAILED.\n",
					zrtp_log_state2str(stream->zrtp->state), stream->zrtp, stream->id));
		}	break;

		default:
			break;
		}

		if (zrtp_status_ok != s) {
			zrtp_sys_free(packet);
		}
	}
#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
	return 0;
#else
	return NULL;
#endif
}


/******************************************************************************
 * Test Engine Public API
 */

void zrtp_test_endpoint_config_defaults(zrtp_test_endpoint_cfg_t* cfg) {

	zrtp_memset(cfg, 0, sizeof(zrtp_test_endpoint_cfg_t));

	cfg->generate_traffic = 0;

	/* It's always a good idea to start with default values */
	zrtp_config_defaults(&cfg->zrtp);

	/* Set ZRTP client id */
	strcpy(cfg->zrtp.client_id, "zrtp-test-engine");

	cfg->zrtp.is_mitm = 0;
	cfg->zrtp.lic_mode = ZRTP_LICENSE_MODE_ACTIVE;

	cfg->zrtp.cb.event_cb.on_zrtp_secure			= &on_zrtp_secure;
	cfg->zrtp.cb.event_cb.on_zrtp_security_event	= &on_zrtp_event;
	cfg->zrtp.cb.event_cb.on_zrtp_protocol_event	= &on_zrtp_event;
	cfg->zrtp.cb.misc_cb.on_send_packet				= &on_send_packet;
}

zrtp_status_t zrtp_test_endpoint_create(zrtp_test_endpoint_cfg_t* cfg,
										const char *name,
										zrtp_test_id_t* id) {
	zrtp_status_t s;
	unsigned i;
	char cache_file_path[ZRTP_TEST_STR_LEN];
	zrtp_endpoint_t *new_endpoint;

	if (g_test_endpoints_count >= K_ZRTP_TEST_MAX_ENDPOINTS)
		return zrtp_status_alloc_fail;

	new_endpoint = &g_test_endpoints[g_test_endpoints_count++];
	zrtp_memset(new_endpoint, 0, sizeof(zrtp_endpoint_t));

	/* Copy configuration, we will use it later to clean up after ourselves */
	zrtp_memcpy(&new_endpoint->cfg, cfg, sizeof(zrtp_test_endpoint_cfg_t));

	/* Remember endpoint name */
	strcpy(new_endpoint->name, name);

	new_endpoint->id = g_endpoints_counter++;

	/* Adjust cache file path so each endpoint will use it's own file. */
	sprintf(cache_file_path, "./%s_cache.dat", name);
	zrtp_zstrcpyc(ZSTR_GV(new_endpoint->cfg.zrtp.def_cache_path), cache_file_path);

	/* Initialize libzrtp engine for this endpoint */
	s = zrtp_init(&new_endpoint->cfg.zrtp, &new_endpoint->zrtp);
	if (zrtp_status_ok == s) {
		*id = new_endpoint->id;

		/* Generate random ZID */
		zrtp_randstr(new_endpoint->zrtp, new_endpoint->zid, sizeof(new_endpoint->zid));
	}

	/* Create Input queue*/
	s = zrtp_test_queue_create(&new_endpoint->input_queue);
	if (zrtp_status_ok != s) {
		return s;
	}

	/* Start processing loop */
	new_endpoint->is_running = 1;

	for (i = 0; i<K_ZRTP_TEST_PROCESSORS_COUNT; i++) {
		if (0 != zrtp_thread_create(process_incoming, new_endpoint)) {
			return zrtp_status_fail;
		}

		if (cfg->generate_traffic) {
			if (0 != zrtp_thread_create(process_outgoing, new_endpoint)) {
				return zrtp_status_fail;
			}
		}
	}

	return s;
}

zrtp_status_t zrtp_test_endpoint_destroy(zrtp_test_id_t id) {
	unsigned i;
	zrtp_status_t s = zrtp_status_ok;
	zrtp_endpoint_t *endpoint = zrtp_test_endpoint_by_id(id);

	endpoint->is_running = 0;

	if (endpoint->input_queue) {
		/* Push faked element to the queue to unlock incoming threads */
		for (i=0; i<K_ZRTP_TEST_PROCESSORS_COUNT; i++) {
			zrtp_queue_elem_t *elem = malloc(sizeof(zrtp_queue_elem_t));
			elem->size = 0;
			zrtp_test_queue_push(endpoint->input_queue, elem);
		}
		zrtp_sleep(0.5*1000);

		zrtp_test_queue_destroy(endpoint->input_queue);
	}

	for (i=0; i<20; i++) zrtp_sleep(100);

	if (endpoint) {
		/* Shut down libzrtp */
		if (endpoint->zrtp)
			s = zrtp_down(endpoint->zrtp);

		/* Clean-up ZRTP cache after ourselves */
		remove(endpoint->cfg.zrtp.def_cache_path.buffer);
	} else {
		s = zrtp_status_fail;
	}

	return s;
}

zrtp_status_t zrtp_test_stream_get(zrtp_test_id_t id,
								   zrtp_test_stream_info_t* info) {

	zrtp_test_stream_t *stream = zrtp_test_stream_by_id(id);
	if (stream) {
		zrtp_status_t s;
		zrtp_memset(info, 0, sizeof(zrtp_test_stream_info_t));

		zrtp_memcpy(info->zrtp_events_queueu, stream->zrtp_events_queueu, sizeof(info->zrtp_events_queueu));
		info->zrtp_events_count = stream->zrtp_events_count;

		s = zrtp_stream_get(stream->zrtp, &info->zrtp);
		return s;
	} else {
		return zrtp_status_bad_param;
	}
}

void zrtp_test_session_config_defaults(zrtp_test_session_cfg_t* cfg) {
	cfg->streams_count = 1;
	cfg->role = ZRTP_SIGNALING_ROLE_UNKNOWN;
	cfg->is_enrollment = 0;

	zrtp_profile_defaults(&cfg->zrtp, NULL);
}

zrtp_status_t zrtp_test_session_create(zrtp_test_id_t endpoint_id,
									   zrtp_test_session_cfg_t* cfg,
									   zrtp_test_id_t* id) {
	zrtp_status_t s;
	unsigned i;
	zrtp_test_session_t *the_session;
	zrtp_endpoint_t *the_endpoint = zrtp_test_endpoint_by_id(endpoint_id);

	if (!the_endpoint)
		return zrtp_status_fail;

	if (the_endpoint->sessions_count >= K_ZRTP_TEST_MAX_SESSIONS_PER_ENDPOINT)
		return zrtp_status_fail;

	the_session = &the_endpoint->sessions[the_endpoint->sessions_count++];

	zrtp_memset(the_session, 0, sizeof(zrtp_test_session_t));

	zrtp_memcpy(&the_session->cfg, cfg, sizeof(zrtp_test_session_cfg_t));

	the_session->id = g_sessions_counter++;
	the_session->endpoint_id = endpoint_id;

	s = zrtp_session_init(the_endpoint->zrtp,
						  &cfg->zrtp,
						  the_endpoint->zid,
						  cfg->role,
						  &the_session->zrtp);

	if (zrtp_status_ok == s) {

		zrtp_session_set_userdata(the_session->zrtp, &the_session->id);

		for (i=0; i<cfg->streams_count; i++) {
			zrtp_test_stream_t *the_stream = &the_session->streams[i];
			zrtp_memset(the_stream, 0, sizeof(zrtp_test_stream_t));

			the_stream->id = g_streams_counter++;
			the_stream->session_id = the_session->id;
			the_stream->endpoint_id = endpoint_id;

			s = zrtp_stream_attach(the_session->zrtp, &the_stream->zrtp);
			if (zrtp_status_ok == s) {
				zrtp_stream_set_userdata(the_stream->zrtp, &the_stream->id);
				the_session->streams_count++;
			} else {
				break;
			}
		}
	}

	if (zrtp_status_ok == s) {
		*id = the_session->id;
	}

	return s;
}

zrtp_status_t zrtp_test_session_destroy(zrtp_test_id_t id) {
	zrtp_test_session_t *session = zrtp_test_session_by_id(id);
	if (session) {
		/* NOTE: we don't release session slots here due to nature of testing
		 * engine: test configuration constructed from scratch for every single test.
		 */
		zrtp_session_down(session->zrtp);
	}
	return zrtp_status_ok;
}

zrtp_status_t zrtp_test_session_get(zrtp_test_id_t id, zrtp_test_session_info_t* info) {
	zrtp_status_t s;
	zrtp_test_session_t *session = zrtp_test_session_by_id(id);
	if (session) {
		s = zrtp_session_get(session->zrtp, &info->zrtp);
		if (zrtp_status_ok == s) {
			unsigned i;
			for (i=0; i<session->streams_count; i++) {
				s = zrtp_test_stream_get(session->streams[i].id, &info->streams[i]);
				if (zrtp_status_ok != s)
					break;
			}
		}

		return s;
	} else {
		return zrtp_status_bad_param;
	}
}

zrtp_status_t zrtp_test_channel_create(zrtp_test_id_t left_id, zrtp_test_id_t right_id, zrtp_test_id_t* id) {
	zrtp_test_channel_t *the_channel;
	zrtp_test_stream_t *left = zrtp_test_stream_by_id(left_id);
	zrtp_test_stream_t *right = zrtp_test_stream_by_id(right_id);

	if (!left || !right)
		return zrtp_status_bad_param;

	if (g_test_channels_count >= K_ZRTP_TEST_MAX_CHANNELS)
		return zrtp_status_bad_param;

	zrtp_endpoint_t *left_endpoint = zrtp_test_endpoint_by_id(left->endpoint_id);
	zrtp_endpoint_t *right_endpoint = zrtp_test_endpoint_by_id(right->endpoint_id);

	the_channel = &g_test_channels[g_test_channels_count++];
	zrtp_memset(the_channel, 0, sizeof(zrtp_test_channel_t));

	the_channel->id = g_channels_counter++;
	the_channel->left = left;
	the_channel->right = right;

	left->output = right_endpoint->input_queue;
	left->input = left_endpoint->input_queue;
	right->output = left_endpoint->input_queue;
	right->input = right_endpoint->input_queue;

	right->channel_id = the_channel->id;
	left->channel_id = the_channel->id;

	the_channel->is_attached = 1;

	*id = the_channel->id;

	return zrtp_status_ok;
}

zrtp_status_t zrtp_test_channel_create2(zrtp_test_id_t left_session,
										zrtp_test_id_t right_session,
										unsigned stream_idx,
										zrtp_test_id_t *id) {
	zrtp_test_session_t *left = zrtp_test_session_by_id(left_session);
	zrtp_test_session_t *right = zrtp_test_session_by_id(right_session);

	if (!left || !right)
		return zrtp_status_bad_param;

	if (left->streams_count <= stream_idx || right->streams_count <= stream_idx)
		return zrtp_status_bad_param;

	return zrtp_test_channel_create(left->streams[stream_idx].id, right->streams[stream_idx].id, id);
}

zrtp_status_t zrtp_test_channel_destroy(zrtp_test_id_t id) {
	zrtp_test_channel_t *channel = zrtp_test_channel_by_id(id);
	if (!channel)
		return zrtp_status_bad_param;

	return zrtp_status_ok;
}

zrtp_status_t zrtp_test_channel_start(zrtp_test_id_t id) {
	zrtp_status_t s1, s2;
	zrtp_test_channel_t *the_channel = zrtp_test_channel_by_id(id);
	zrtp_test_session_t *the_session;

	the_session = zrtp_test_session_by_id(the_channel->left->session_id);
	if (the_session->cfg.is_enrollment)
		s1 = zrtp_stream_registration_start(the_channel->left->zrtp, the_channel->left->id); /* use stream Id as ssrc */
	else
		s1 = zrtp_stream_start(the_channel->left->zrtp, the_channel->left->id); /* use stream Id as ssrc */
	if (s1 == zrtp_status_ok) {
		the_session = zrtp_test_session_by_id(the_channel->right->session_id);
		if (the_session->cfg.is_enrollment)
			s2 = zrtp_stream_registration_start(the_channel->right->zrtp, the_channel->right->id);
		else
			s2 = zrtp_stream_start(the_channel->right->zrtp, the_channel->right->id);
	} else {
		return s1;
	}

	return s2;
}

zrtp_status_t zrtp_test_channel_get(zrtp_test_id_t id,
									zrtp_test_channel_info_t* info) {

	zrtp_test_channel_t *channel = zrtp_test_channel_by_id(id);
	if (channel) {
		zrtp_status_t s;

		zrtp_memset(info, 0, sizeof(zrtp_test_channel_info_t));

		s = zrtp_test_stream_get(channel->left->id, &info->left);
		if (zrtp_status_ok == s) {
			s = zrtp_test_stream_get(channel->right->id, &info->right);
			if (zrtp_status_ok == s) {
				info->is_secure = channel->is_secure;
			}
		}

		return s;
	} else {
		return zrtp_status_bad_param;
	}
}


/******************************************************************************
 * Helpers
 */

zrtp_endpoint_t *zrtp_test_endpoint_by_id(zrtp_test_id_t id) {
	int i;

	if (ZRTP_TEST_UNKNOWN_ID == id) return NULL;

	for (i=0; i<g_test_endpoints_count; i++) {
		if (g_test_endpoints[i].id == id) {
			return &g_test_endpoints[i];
		}
	}

	return NULL;
}

zrtp_test_session_t *zrtp_test_session_by_id(zrtp_test_id_t id) {
	int i, j;

	if (ZRTP_TEST_UNKNOWN_ID == id) return NULL;

	for (i=0; i<g_test_endpoints_count; i++) {
		zrtp_endpoint_t *endpoint = &g_test_endpoints[i];
		if (endpoint->id == ZRTP_TEST_UNKNOWN_ID)
			continue;

		for (j=0; j<endpoint->sessions_count; j++) {
			if (endpoint->sessions[j].id == id) {
				return  &endpoint->sessions[j];
			}
		}
	}

	return NULL;
}

zrtp_test_stream_t *zrtp_test_stream_by_id(zrtp_test_id_t id) {
	int i, j, k;

	if (ZRTP_TEST_UNKNOWN_ID == id) return NULL;

	for (i=0; i<g_test_endpoints_count; i++) {
		zrtp_endpoint_t *endpoint = &g_test_endpoints[i];
		if (endpoint->id == ZRTP_TEST_UNKNOWN_ID)
			continue;

		for (j=0; j<endpoint->sessions_count; j++) {
			zrtp_test_session_t *session = &endpoint->sessions[j];
			if (session->id == ZRTP_TEST_UNKNOWN_ID)
				continue;

			for (k=0; k<session->streams_count; k++) {
				if (session->streams[k].id  == id) {
					return &session->streams[k];
				}
			}
		}
	}

	return NULL;
}

zrtp_test_channel_t *zrtp_test_channel_by_id(zrtp_test_id_t id) {
	int i;
	zrtp_test_channel_t *channel = NULL;

	if (ZRTP_TEST_UNKNOWN_ID == id) return NULL;

	for (i=0; i<g_test_channels_count; i++) {
		if (g_test_channels[i].id != ZRTP_TEST_UNKNOWN_ID && g_test_channels[i].id == id) {
			channel = &g_test_channels[i];
			break;
		}
	}

	return channel;
}

zrtp_test_stream_t *zrtp_test_stream_by_peerid(zrtp_test_id_t id) {
	int i;
	if (ZRTP_TEST_UNKNOWN_ID == id) return NULL;

	for (i=0; i<g_test_channels_count; i++) {
		if (g_test_channels[i].id != ZRTP_TEST_UNKNOWN_ID) {
			if (g_test_channels[i].left->id == id)
				return g_test_channels[i].right;
			else if (g_test_channels[i].right->id == id)
				return g_test_channels[i].left;
		}
	}

	return NULL;
}

zrtp_test_id_t zrtp_test_session_get_stream_by_idx(zrtp_test_id_t session_id, unsigned idx) {
	zrtp_test_session_t *session = zrtp_test_session_by_id(session_id);
	if (session && session->streams_count > idx) {
		return session->streams[idx].id;
	} else {
		return ZRTP_TEST_UNKNOWN_ID;
	}
}

zrtp_stream_t *zrtp_stream_for_test_stream(zrtp_test_id_t stream_id) {
	zrtp_test_stream_t *stream = zrtp_test_stream_by_id(stream_id);
	if (stream) {
		return stream->zrtp;
	} else {
		return NULL;
	}
}

unsigned zrtp_stream_did_event_receive(zrtp_test_id_t stream_id, unsigned event) {
	unsigned i;
	zrtp_test_stream_info_t stream_info;

	zrtp_test_stream_get(stream_id, &stream_info);
	for (i=0; i<stream_info.zrtp_events_count; i++) {
		if (stream_info.zrtp_events_queueu[i] == event)
			break;
	}

	return (i != stream_info.zrtp_events_count);
}

