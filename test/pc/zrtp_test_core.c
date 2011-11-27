/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#include "zrtp_test_core.h"
#include "zrtp_test_queue.h"

#if (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
#include <windows.h>
#elif (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_LINUX)
#include <pthread.h>
#include <string.h>
#endif

#define	_ZTU_ "TEST"
#define	ZRTP_TEST_PACKET_MAX_SIZE	256
#define	ZRTP_TEST_STREAMS_COUNT		1

#define ZRTP_ENABLE_TEST			0

extern uint8_t hash_word_list_odd[256][12];
extern uint8_t hash_word_list_even[256][10];

/*! Global data storage */
typedef struct zrtp_test_global
{
	mlist_t				channels_head;	/*! List of test channels*/
	zrtp_mutex_t*		channels_protector;	/*! Protector from list modifications */	
	unsigned char		is_log_enabled;	/*! Allows debug messages logging */
	unsigned			packets_rate;	/*! Delay in miliseconds between RTP injection */	
	unsigned			channels_count;	/*! Active channels count */
	unsigned			secure_count;	/*! Active channels count */
	zrtp_queue_t*		queue;			/*! Queue which emulate communication channel between ZRTP endpoints */
	unsigned char		is_running;		/*! Main test loop work while this is not 0 */
} zrtp_test_global_t;

zrtp_test_global_t		test_global;	/*! zrtp test global data */
zrtp_global_t*			zrtp_global;	/*! libzrtp global data */


typedef struct zrtp_test_session	zrtp_test_session_t;
typedef struct zrtp_test_stream		zrtp_test_stream_t;
typedef struct zrtp_test_channel	zrtp_test_channel_t;

#define	ZRTP_TEST_PACKET_HEADER_LENGTH 16


typedef struct zrtp_test_packet
{
	uint32_t			is_rtp;			/*! Defines is packet RTP or RTCP */
	uint32_t			length;			/*! Packet Length in bytes */
	char				body[1024];		/*! Packet body */
} zrtp_test_packet_t;

struct zrtp_test_stream
{
	zrtp_stream_t*		zrtp_stream;	/*! ZRTP stream associated test stream */
	zrtp_test_session_t* session;		/*! Pointer to the parent test session */
	uint16_t			seq;			/*! RTP sequence number for media packets construction */
	uint32_t			ssrc;			/*! RTP local SSRC for media packets exchanging */
	uint32_t			peer_ssrc;		/*! RTP remote SSRC for media packets exchanging */
};

struct zrtp_test_session
{	
	zrtp_test_stream_t	streams[ZRTP_MAX_STREAMS_PER_SESSION]; /*! Streams array */
	unsigned			streams_count;	/*! Active streams counter */	
	zrtp_session_t*		zrtp_session;	/*! ZRTP session associated test session */
	zrtp_test_channel_t *channel;
	mlist_t				_list;
};

struct zrtp_test_channel
{
	zrtp_test_channel_id_t	id;			/*! Channel ID*/
	zrtp_test_session_t		ses_left;	/*! Left test session */
	zrtp_test_session_t		ses_right;	/*! Right test session */
	mlist_t					_mlist;
};


zrtp_test_channel_t* find_channel_by_index(zrtp_test_channel_id_t chan_id);
zrtp_test_channel_t* find_channel_with_closest_index(zrtp_test_channel_id_t chan_id);
zrtp_test_stream_t*  find_peer_stream_by_ssrc(uint32_t ssrc);


/*============================================================================*/
/*     ZRTP Interfaces														  */
/*============================================================================*/

static void on_zrtp_protocol_event(zrtp_stream_t *ctx, zrtp_protocol_event_t event)
{
}

static void on_zrtp_security_event(zrtp_stream_t *ctx, zrtp_security_event_t event)
{
}
								   
static void on_zrtp_secure(zrtp_stream_t *ctx)
{
	zrtp_test_stream_t *stream = (zrtp_test_stream_t*) zrtp_stream_get_userdata(ctx);
	zrtp_test_channel_t *channel = stream->session->channel;
	unsigned i = 0;
	unsigned char not_secure = 0;
		
	ZRTP_LOG(1, (_ZTU_,"Stream is SECURE ssrc=%u ctx=%d\n", ctx->media_ctx.ssrc, ctx));
	
	for (i=0; i<channel->ses_left.streams_count; i++) {
		if (ZRTP_STATE_SECURE != channel->ses_left.streams[i].zrtp_stream->state) {
			not_secure = 1;
		}
	}		
	if (0 == not_secure) {
		for (i=0; i<channel->ses_right.streams_count; i++) {
			if (ZRTP_STATE_SECURE != channel->ses_right.streams[i].zrtp_stream->state) {
				not_secure = 1;
			}
		}
	}	
	
	if (0 == not_secure) {
		zrtp_session_info_t session_info;
		zrtp_stream_info_t a_stream_info;
		
		test_global.secure_count++;
		
		ZRTP_LOG(1, (_ZTU_,"===================================================\n"));
		ZRTP_LOG(1, (_ZTU_,"Entire Channel is SECURE. Total Secure count=%d\n", test_global.secure_count));
		ZRTP_LOG(1, (_ZTU_,"===================================================\n"));
		
		zrtp_session_get(channel->ses_left.zrtp_session, &session_info);		
		zrtp_stream_get(channel->ses_left.streams[0].zrtp_stream , &a_stream_info);
		
		zrtp_log_print_sessioninfo(&session_info);
		zrtp_log_print_streaminfo(&a_stream_info);		
	}
}

static int on_send_packet(const zrtp_stream_t* ctx, char* message, unsigned int length)
{
	zrtp_queue_elem_t* elem = zrtp_sys_alloc(sizeof(zrtp_queue_elem_t));
	if (elem) {		
		zrtp_test_packet_t* packet = (zrtp_test_packet_t*) elem->data;
		elem->size = length;
				
		packet->is_rtp = 1;
		packet->length = length;
		zrtp_memcpy(packet->body, message, length);
				
		ZRTP_LOG(1, (_ZTU_,"\tSend message to ssrc=%u length=%d\n", ctx->media_ctx.ssrc, length));
		zrtp_test_queue_push(test_global.queue, elem);
		return zrtp_status_ok;
	} else {
		return zrtp_status_alloc_fail;
	}
} 

void zrtp_def_get_cache_path(char *path, uint32_t length)
{
#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
	strncpy_s(path, length, "./zrtp_test_cache.dat", length);
#else
	strncpy(path, "./zrtp_test_cache.dat", length);
#endif
}


/*============================================================================*/
/*     Sessions Life-Cycle Logic											  */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
DWORD WINAPI process_incoming(void *param)
#else
void *process_incoming(void *param)
#endif
{
	ZRTP_LOG(1, (_ZTU_,"======> STARTING Incoming processor\n"));
	while (test_global.is_running)
	{
		zrtp_test_packet_t* packet = NULL;
		zrtp_queue_elem_t* elem = NULL;
		zrtp_status_t s = zrtp_status_fail;
		zrtp_test_stream_t *stream = NULL;
		int is_protocol = 0;
		
		/* Get packet from the "Network" and find aapropriate stream to process it */
		elem = zrtp_test_queue_pop(test_global.queue);
		packet = (zrtp_test_packet_t*) elem->data;
		
		/* We have ssrc in the packet, which indetifies stream that had send it to us.
		 * so now we have to find appropriate stream which has to process that packet.
		 */
		if (packet->is_rtp) {
			ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *hdr = (zrtp_rtp_hdr_t*) (packet->body);
			if (ZRTP_PACKETS_MAGIC == zrtp_ntoh32(hdr->ts))  {
				is_protocol	 = 1;
				ZRTP_LOG(1, (_ZTU_,"\n"));
			}	
			stream = find_peer_stream_by_ssrc(hdr->ssrc);
		} else {
			ZRTP_UNALIGNED(zrtp_rtcp_hdr_t) *hdr = (zrtp_rtcp_hdr_t*) (packet->body);
			stream = find_peer_stream_by_ssrc(hdr->ssrc);		
		}
				
		if (!stream) {
			zrtp_sys_free(elem);
			ZRTP_LOG(1, (_ZTU_,"process_incoming(): ERROR! can't found peer stream!\n"));
			continue;
		}			
		
		/*
		 * Process incoming packet by libzrtp. Is this a RTP media packet - copy it to the buffer
		 * to print out later.
		 */
		if (packet->is_rtp) {		
			s = zrtp_process_srtp(stream->zrtp_stream, packet->body, &packet->length);			
		} else {	
			s = zrtp_process_srtcp(stream->zrtp_stream, packet->body, &packet->length);
		}
		
		
		if (!is_protocol)
		{
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
				ZRTP_LOG(1, (_ZTU_,"Incoming: (%s) [%p:ssrc=%u] INPUT. <%s> decrypted %d bytes.\n",
							zrtp_log_state2str(stream->zrtp_stream->state), stream->zrtp_stream,  stream->ssrc, body, packet->length));
			} break;
			
			case zrtp_status_drop: {
				ZRTP_LOG(1, (_ZTU_,"Incoming: (%s) [%p:ssrc=%u] INPUT DROPPED. <%s>\n",
							zrtp_log_state2str(stream->zrtp_stream->state), stream->zrtp_stream, stream->ssrc, body));
			} break;
			
			case zrtp_status_fail: {
				ZRTP_LOG(1, (_ZTU_,"Incoming: (%s) [%p:ssrc=%u] DECRYPT FAILED. <%s>\n",
							zrtp_log_state2str(stream->zrtp_stream->state), stream->zrtp_stream, stream->ssrc, body));
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
		if (stream->zrtp_stream->state == ZRTP_STATE_PENDINGCLEAR) {
			zrtp_stream_clear(stream->zrtp_stream);
		}		
	} /* while is running */
	
#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
	return 0;
#else
	return NULL;
#endif
}

/*----------------------------------------------------------------------------*/
#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
DWORD WINAPI process_outgoing(void *param)
#else
void *process_outgoing(void *param)
#endif
{	
	unsigned packets_counter = 0;
	
	ZRTP_LOG(1, (_ZTU_,"======> STARTING Outgoing processor\n"));
	while (test_global.is_running)
	{		
		zrtp_test_channel_id_t channel_id = 0;
		zrtp_test_channel_t* channel = NULL;
		zrtp_test_session_t* session = NULL;		
		unsigned i;
				
		zrtp_sleep(test_global.packets_rate);

		if ((0 == test_global.channels_count) || (0 == test_global.secure_count)) {
			continue;
		}
		
		/* Get random channel to operate with and select random peer */
		zrtp_randstr(zrtp_global, (unsigned char*)&channel_id, sizeof(zrtp_test_channel_id_t));
		channel = find_channel_with_closest_index(channel_id);
		if (!channel) {
			continue;
		}
		
		ZRTP_LOG(1, (_ZTU_,"\n"));
		ZRTP_LOG(1, (_ZTU_,"Out. Generate packet by channel N=%u and %s session.\n",
					 channel->id, ((channel_id % 100) > 50) ? "RIGHT": "LEFT"));
		
		session = ((channel_id % 100) > 50) ? &channel->ses_right : &channel->ses_left;
		
		/* Generate RTP or RTCP packet for every stream within the session */
		for (i=0; i<session->streams_count; i++)
		{
			zrtp_status_t s = zrtp_status_fail;
			zrtp_test_packet_t* packet;
			zrtp_queue_elem_t* elem;
			zrtp_test_stream_t* stream = &session->streams[i];
			char* word = NULL;
			
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
				rtp_hdr->ssrc = stream->ssrc;	/* Use ssrc for addressing like in real RTP */
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
				s = zrtp_process_rtp(stream->zrtp_stream, packet->body, &packet->length);
			}
			else
			{
				ZRTP_UNALIGNED(zrtp_rtcp_hdr_t) *rtcp_hdr = (zrtp_rtcp_hdr_t*)packet->body;
				
				/* Fill RTCP Header according to the specification */
				rtcp_hdr->rc = 0;
				rtcp_hdr->version = 2;
				rtcp_hdr->ssrc = stream->ssrc;
				
				/* Get RTP body from PGP words lists. Put RTCP marker at the beginning */
				zrtp_memcpy(packet->body + sizeof(zrtp_rtcp_hdr_t), "RTCP", 4);
				word = (char*)( i ? hash_word_list_odd[packets_counter % 256] : hash_word_list_even[packets_counter % 256]);
				
				zrtp_memcpy(packet->body + sizeof(zrtp_rtcp_hdr_t) + 4, word, (uint32_t)strlen(word));
				packet->length = sizeof(zrtp_rtcp_hdr_t) + (uint32_t)strlen(word) + 4;
				/* RTCP packets sould be 32 byes aligned */
				packet->length += (packet->length % 4) ? (4 - packet->length % 4) : 0;

				/* Process RTCP control with libzrtp */
				s = zrtp_process_rtcp(stream->zrtp_stream, packet->body, &packet->length);
			}

			/* Handle zrtp_process_xxx() instructions */
			switch (s)
			{
			/* Put the packet to the queue ==> send packet to the other side pear */
			case zrtp_status_ok: {							
				ZRTP_LOG(3, (_ZTU_,"Outgoing: (%s) [%p:ssrc=%u] OUTPUT. <%s%s> encrypted %d bytes.\n",
							zrtp_log_state2str(stream->zrtp_stream->state), stream->zrtp_stream, stream->ssrc, packet->is_rtp ? "" : "RTCP", word, packet->length));
				zrtp_test_queue_push(test_global.queue, elem);											
			} break;
			
			case zrtp_status_drop: {
				ZRTP_LOG(1, (_ZTU_,"Outgoing: (%s) [%p:ssrc=%u] OUTPUT DROPPED.\n",
							zrtp_log_state2str(stream->zrtp_stream->state), stream->zrtp_stream, stream->ssrc));
			} break;
				
			case zrtp_status_fail: {
				ZRTP_LOG(1, (_ZTU_,"Outgoing: (%s) [%p:ssrc=%u] ENCRYPT FAILED.\n",
							zrtp_log_state2str(stream->zrtp_stream->state), stream->zrtp_stream, stream->ssrc));
			}	break;
			
			default:
				break;
			}
						
			if (zrtp_status_ok != s) {
				zrtp_sys_free(packet);
			}
			
		} /* for (every stream within the session) */
	} /* while is running */
	
#if   (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
	return 0;
#else
	return NULL;
#endif
}

/*----------------------------------------------------------------------------*/
static zrtp_status_t init_test_session( zrtp_test_session_t *session,
									    zrtp_profile_t *zrtp_profile,
									    unsigned nstreams)
{
	unsigned i = 0;
	g_zrtp_cfg zid;
	zrtp_status_t s = zrtp_status_fail;
	
	session->streams_count = nstreams;
	
	/* Allocate ZRTP session */
	zrtp_randstr(zrtp_global, (unsigned char*)&zid, sizeof(g_zrtp_cfg));
	
	ZRTP_LOG(3, (_ZTU_,"INITIALIZE NEW SESSION ctx=%p:\n", session));
	ZRTP_LOG(3, (_ZTU_,"---------------------------------------------------\n"));
	
	s = zrtp_session_init(zrtp_global, zrtp_profile, zid, 1, &session->zrtp_session);	
	if (zrtp_status_ok != s) {	
		ZRTP_LOG(3, (_ZTU_,"ERROR! can't initalize ZRTP session d=%d.\n", s));
		return s;
	}
	
	zrtp_session_set_userdata(session->zrtp_session, session);
	
	ZRTP_LOG(3, (_ZTU_,"Attach %d ZRTP streams.\n", session->streams_count));
	/* ZRTP session is ready for use. Now it's time to attach streams to it */
	for (i=0; i<session->streams_count; i++)
	{
		zrtp_test_stream_t *stream = &session->streams[i];
		
		/* Create random sequence number and ssrc for packets generation */
		zrtp_randstr(zrtp_global, (unsigned char*)&stream->seq, sizeof(stream->seq));
		zrtp_randstr(zrtp_global, (unsigned char*)&stream->ssrc, sizeof(stream->ssrc));
		
		ZRTP_LOG(3, (_ZTU_,"Created stream N%d ssrc=%u seq=%d ctx=%p\n", i, stream->ssrc, stream->seq, stream));
		
		/* Attach zrtp_stream to zrtp_session */
		s = zrtp_stream_attach(session->zrtp_session, &stream->zrtp_stream);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(3, (_ZTU_,"ERROR! can't attach ZRTP stream.\n"));
			break;
		}
		zrtp_stream_set_userdata(stream->zrtp_stream, stream);
		stream->session = session;
	}
	
	if (i != session->streams_count) {
		zrtp_session_down(session->zrtp_session);
		return zrtp_status_fail;		
	}
	
	return zrtp_status_ok;
};

/*----------------------------------------------------------------------------*/
int zrtp_test_channel_create( const zrtp_test_channel_config_t* config,
							  zrtp_test_channel_id_t* chan_id)
{
	zrtp_status_t s = zrtp_status_fail;
	zrtp_test_channel_t* channel = NULL;
	unsigned streams_number = 1;
	
	
	/*
	 * Create two connection for each side of the call.
	 * They will have the same id in order to simplify calls managing.
	 */
	channel = zrtp_sys_alloc(sizeof(zrtp_test_channel_t));
	if (!channel) {
		return -1;
	}	
	zrtp_memset(channel, 0, sizeof(zrtp_test_channel_t));
	
	/* Generate new unique ID for the channel */
	zrtp_randstr(zrtp_global, (unsigned char*)chan_id, sizeof(zrtp_test_channel_id_t));
	
	ZRTP_LOG(1, (_ZTU_,"=====> CREATE NEW CHANNEL ID=%u ctx=%p.\n", chan_id, channel));
	
	/*
	 * Alloacte and initalize ZRTP crypto stuffs
	 */
	do {
		zrtp_profile_t zrtp_profile;
		unsigned i = 0;
		
		/*
		 * Use default ZRTP configuration with slitely changes:
		 * - enable "allowclear" to be able to test more scenarios
		 */
		zrtp_profile_defaults(&zrtp_profile, zrtp_global);
		zrtp_profile.allowclear = 1;
		zrtp_profile.autosecure = 1;
		
		channel->ses_left.streams_count = streams_number;
#if (ZRTP_ENABLE_TEST == 1)
		zrtp_profile.pk_schemes[0] = ZRTP_PKTYPE_EC384P;
		zrtp_profile.pk_schemes[1] = ZRTP_PKTYPE_DH3072;
		zrtp_profile.pk_schemes[2] = ZRTP_PKTYPE_MULT;
		zrtp_profile.pk_schemes[3] = 0;		
#endif		
		s = init_test_session(&channel->ses_left, &zrtp_profile, 1);
		if (zrtp_status_ok != s) {
			break;
		}
		channel->ses_left.channel = channel;
		
		channel->ses_right.streams_count = streams_number;
#if (ZRTP_ENABLE_TEST == 1)	
		zrtp_profile.autosecure = 1;
		zrtp_profile.pk_schemes[0] = ZRTP_PKTYPE_EC384P;
		zrtp_profile.pk_schemes[1] = ZRTP_PKTYPE_DH2048;
		zrtp_profile.pk_schemes[2] = ZRTP_PKTYPE_DH3072;
		zrtp_profile.pk_schemes[3] = ZRTP_PKTYPE_MULT;
		zrtp_profile.pk_schemes[4] = 0;
#endif
		s = init_test_session(&channel->ses_right, &zrtp_profile, 1);
		if (zrtp_status_ok != s) {
			break;
		}
		channel->ses_right.channel = channel;
		
		/* Make cross-references to allow left stream to communicate with the right one. */
		for (i=0; i<streams_number; i++) {
			channel->ses_left.streams[i].peer_ssrc = channel->ses_right.streams[i].ssrc;
			channel->ses_right.streams[i].peer_ssrc = channel->ses_left.streams[i].ssrc;
		}
	} while (0);
	
	if (zrtp_status_ok != s) {
		zrtp_sys_free(channel);
	} else {
		channel->id = *chan_id;		
		zrtp_mutex_lock(test_global.channels_protector);
		mlist_add(&test_global.channels_head, &channel->_mlist);
		zrtp_mutex_unlock(test_global.channels_protector);
	}
	
	test_global.channels_count++;
	 
	return s;
}

/*----------------------------------------------------------------------------*/
static void down_test_session(zrtp_test_session_t *session)
{
	zrtp_session_down(session->zrtp_session);
};

int zrtp_test_channel_delete(zrtp_test_channel_id_t chan_id)
{
	zrtp_test_channel_t* channel = find_channel_by_index(chan_id);
	if (channel)
	{
		test_global.channels_count--;
		
		zrtp_mutex_lock(test_global.channels_protector);
		mlist_del(&channel->_mlist);
		zrtp_mutex_unlock(test_global.channels_protector);
		
		down_test_session(&channel->ses_left);
		down_test_session(&channel->ses_right);
		
		zrtp_sys_free(channel);
		return 0;
	} else {
		return -1;
	}
}

/*----------------------------------------------------------------------------*/
typedef enum
{
	TEST_ACTION_START = 0,
	TEST_ACTION_SECURE,
	TEST_ACTION_CLEAR,
	TEST_ACTION_STOP
} zrtp_test_session_action_t;

static void test_session_do_action(zrtp_test_session_t *session, zrtp_test_session_action_t action)
{
	unsigned i = 0;	
	if (session->zrtp_session)
	{
		for (i=0; i<session->streams_count; i++) {
			switch (action) {
			case TEST_ACTION_START:
			{
				char buff[65];
				ZRTP_LOG(1, (_ZTU_,"=====> START TEST SESSION ID=%u ctx=%p.\n", session->channel->id, session->channel));				
				zrtp_stream_start(session->streams[i].zrtp_stream, zrtp_ntoh32(session->streams[i].ssrc));					
				zrtp_signaling_hash_get(session->streams[i].zrtp_stream, buff, sizeof(buff));					
				break;
			}
			case TEST_ACTION_STOP:
				ZRTP_LOG(1, (_ZTU_,"=====> STOP TEST SESSION ID=%u ctx=%p.\n", session->channel->id, session->channel));
				zrtp_stream_stop(session->streams[i].zrtp_stream);
				break;
			case TEST_ACTION_SECURE:
				ZRTP_LOG(1, (_ZTU_,"=====> SECURE TEST SESSION ID=%u ctx=%p.\n", session->channel->id, session->channel));
				zrtp_stream_secure(session->streams[i].zrtp_stream);
				break;
			case TEST_ACTION_CLEAR:
				ZRTP_LOG(1, (_ZTU_,"=====> CLEAR TEST SESSION ID=%u ctx=%p.\n", session->channel->id, session->channel));
				zrtp_stream_clear(session->streams[i].zrtp_stream);
				break;
			}			
		}
	}
};

static int test_channel_do_action(zrtp_test_channel_id_t chan_id, zrtp_test_session_action_t action)
{
	zrtp_test_channel_t* channel = find_channel_by_index(chan_id);
	if (channel) {
		test_session_do_action(&channel->ses_left, action);
		test_session_do_action(&channel->ses_right, action);
		return 0;
	} else {
		return -1;
	}
}

int zrtp_test_channel_start(zrtp_test_channel_id_t chan_id) {
	return test_channel_do_action(chan_id, TEST_ACTION_START);
}

int zrtp_test_channel_secure(zrtp_test_channel_id_t chan_id) {
	return test_channel_do_action(chan_id, TEST_ACTION_SECURE);
}

int zrtp_test_channel_clear(zrtp_test_channel_id_t chan_id) {
	return test_channel_do_action(chan_id, TEST_ACTION_CLEAR);
}

int zrtp_test_channel_stop(zrtp_test_channel_id_t chan_id) {
	return test_channel_do_action(chan_id, TEST_ACTION_STOP);
}


/*============================================================================*/
/*     Test Routine															  */
/*============================================================================*/

/*---------------------------------------------------------------------------*/
static int start_processors(int count)
{
	int32_t i;
	for (i = 0; i<count; i++)
	{
		if (0 != zrtp_thread_create(process_incoming, NULL)) {
			return -1;
		}
		if (0 != zrtp_thread_create(process_outgoing, NULL)) {
			return -1;
		}
	}
	
	return 0;
}

int zrtp_test_zrtp_init()
{
	int result = -1;
	zrtp_status_t s = zrtp_status_ok;
	zrtp_config_t zrtp_config;
	
	ZRTP_LOG(1, (_ZTU_,"=====>Start ZRTP Test-Unite Core initalization.\n"));
	
	zrtp_memset(&test_global, 0, sizeof(test_global));
	
	ZRTP_LOG(1, (_ZTU_,"Create internal components...\n"));
	do {	
		/* Create global objects: channels list and communication queue */
		init_mlist(&test_global.channels_head);
		
		s = zrtp_mutex_init(&test_global.channels_protector);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1, (_ZTU_,"ERROR! Can't create channels protector %d\n", s));
			break;
		}
		
		s = zrtp_test_queue_create(&test_global.queue);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1, (_ZTU_,"ERROR! Can't create global queue: %s\n", s));
			break;
		}
		
		/* Start several threads to process test zrtp channels */
		test_global.is_running = 1;
		result = start_processors(ZRTP_TEST_STREAMS_COUNT);
		if (0 != result) {
			break;
		}
		
		ZRTP_LOG(1, (_ZTU_,"Configuring and Initalizing ZRTP engine...\n"));
		/* Reset  global values to defaults */
		test_global.is_log_enabled	= 1;
		test_global.packets_rate	= 500;
		test_global.channels_count	= 0;
		
		/* Initalize ZRTP Engine */
		zrtp_config_defaults(&zrtp_config);
		zrtp_config.lic_mode = ZRTP_LICENSE_MODE_ACTIVE;
		zrtp_memcpy(zrtp_config.client_id, "ZRTP Test Unite!", 16);
		
		zrtp_config.cb.event_cb.on_zrtp_secure			= on_zrtp_secure;
		zrtp_config.cb.event_cb.on_zrtp_security_event	= on_zrtp_security_event;
		zrtp_config.cb.event_cb.on_zrtp_protocol_event	= on_zrtp_protocol_event;
		zrtp_config.cb.misc_cb.on_send_packet			= on_send_packet;
		
		s = zrtp_init(&zrtp_config, &zrtp_global);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1, (_ZTU_,"ERROR! zrtp_init() failed with status=%d.\n", s));
			break;
		}
		
		result = 0;
	} while (0);
	
	if (0 != result) {
		if (!test_global.channels_protector) {
			zrtp_mutex_destroy(test_global.channels_protector);
		}
		if (!test_global.queue) {
			zrtp_test_queue_destroy(test_global.queue);
		}
	}
	
	return result;
}

/*----------------------------------------------------------------------------*/
int zrtp_test_zrtp_down()
{
	mlist_t* node = NULL,* tmp = NULL;
	
	ZRTP_LOG(1, (_ZTU_,"=====> Destroying ZRTP Test Application:\n"));
	ZRTP_LOG(1, (_ZTU_,"Stop all running threads.\n"));
	
	/* Stop Main Processing Loop */
	test_global.is_running = 0;
	
	ZRTP_LOG(1, (_ZTU_,"Destroy all active connections.\n"));
	/* Stop and destroy all active sessions */	
	zrtp_mutex_lock(test_global.channels_protector);
	mlist_for_each_safe(node, tmp, &test_global.channels_head) {
		zrtp_test_channel_t* result = (zrtp_test_channel_t*) mlist_get_struct(zrtp_test_channel_t, _mlist, node);
		zrtp_test_channel_delete(result->id);		
	}
	zrtp_mutex_unlock(test_global.channels_protector);
	
	ZRTP_LOG(1, (_ZTU_,"Destroy ZRTP Engine.\n"));
	/* Destroy libzrtp and all utility components */
	zrtp_down(zrtp_global);
	
	 if (!test_global.channels_protector) {
		zrtp_mutex_destroy(test_global.channels_protector);
	}
	if (!test_global.queue) {
		zrtp_test_queue_destroy(test_global.queue);
	}
	
	return 0;
}

/*----------------------------------------------------------------------------*/
zrtp_test_channel_t* find_channel_by_index(zrtp_test_channel_id_t chan_id)
{
	unsigned char is_found = 0;
	mlist_t* node = NULL,* tmp = NULL;
	zrtp_test_channel_t* result = NULL;
	
	zrtp_mutex_lock(test_global.channels_protector);
	mlist_for_each_safe(node, tmp, &test_global.channels_head) {
		result = (zrtp_test_channel_t*) mlist_get_struct(zrtp_test_channel_t, _mlist, node);
		if (result->id == chan_id) {
			is_found = 1;
			break;
		}
	}
	zrtp_mutex_unlock(test_global.channels_protector);
	
	return (is_found) ? result : NULL;
}

zrtp_test_channel_t* find_channel_with_closest_index(zrtp_test_channel_id_t chan_id)
{
	unsigned char is_found = 0;
	mlist_t* node = NULL,* tmp = NULL;
	zrtp_test_channel_t* result = NULL;
	zrtp_test_channel_t* left = NULL;
	
	zrtp_mutex_lock(test_global.channels_protector);
	mlist_for_each_safe(node, tmp, &test_global.channels_head) {
		result = (zrtp_test_channel_t*) mlist_get_struct(zrtp_test_channel_t, _mlist, node);
		if (result->id > chan_id) {
			is_found = 1;
			break;
		} else {
			left = result;
		}
	}
	zrtp_mutex_unlock(test_global.channels_protector);
	
	return (is_found) ? result : left;
}

zrtp_test_stream_t*  find_peer_stream_by_ssrc(uint32_t ssrc)
{
	unsigned char is_found = 0;
	mlist_t *node = NULL, *tmp = NULL;
	zrtp_test_stream_t *result = NULL;
	
	zrtp_mutex_lock(test_global.channels_protector);
	mlist_for_each_safe(node, tmp, &test_global.channels_head)
	{
		zrtp_test_channel_t *channel = (zrtp_test_channel_t*) mlist_get_struct(zrtp_test_channel_t, _mlist, node);
		unsigned i = 0;

		for (i=0; i<channel->ses_left.streams_count; i++) {
			zrtp_test_stream_t *stream = &channel->ses_left.streams[i];
			if (stream->ssrc == ssrc) {
				is_found = 1;
				result = &channel->ses_right.streams[i];
				break;
			}
		}
		
		for (i=0; i<channel->ses_right.streams_count; i++) {
			zrtp_test_stream_t *stream = &channel->ses_right.streams[i];
			if (stream->ssrc == ssrc) {
				is_found = 1;
				result = &channel->ses_left.streams[i];
				break;
			}
		}
		
		if (is_found) {
			break;
		}
	}
	zrtp_mutex_unlock(test_global.channels_protector);
	
	return (is_found) ? result : NULL;
}
