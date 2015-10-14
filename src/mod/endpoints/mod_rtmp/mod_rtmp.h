/*
 * mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2011, Barracuda Networks Inc.
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Barracuda Networks Inc.
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Mathieu Rene <mrene@avgs.ca>
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_rtmp.h -- RTMP Endpoint Module
 *
 */

#ifndef MOD_RTMP_H
#define MOD_RTMP_H
#include <switch.h>

/* AMF */
#include "amf0.h"
#include "io.h"
#include "types.h"

//#define RTMP_DEBUG_IO
#define RTMP_DONT_HOLD

#define RTMP_THREE_WAY_UUID_VARIABLE "rtmp_three_way_uuid"
#define RTMP_ATTACH_ON_HANGUP_VARIABLE "rtmp_attach_on_hangup"
#define RTMP_USER_VARIABLE_PREFIX "rtmp_u_"

#define RTMP_DEFAULT_PORT 1935
#define RTMP_TCP_READ_BUF 2048 * 16
#define AMF_MAX_SIZE      2048 * 16 * 2

#define SUPPORT_SND_NONE	0x0000
#define SUPPORT_SND_ADPCM	0x0002
#define SUPPORT_SND_MP3		0x0004
#define SUPPORT_SND_INTEL 	0x0008
#define SUPPORT_SND_UNUSED 	0x0010
#define SUPPORT_SND_NELLY8 	0x0020
#define SUPPORT_SND_NELLY 	0x0040
#define SUPPORT_SND_G711A 	0x0080
#define SUPPORT_SND_G711U 	0x0100
#define SUPPORT_SND_NELLY16 	0x0200
#define SUPPORT_SND_AAC 	0x0400
#define SUPPORT_SND_SPEEX 	0x0800
#define SUPPORT_SND_ALL 	0x0FFF

#define SUPPORT_VID_UNUSED 	0x0001
#define SUPPORT_VID_JPEG	0x0002
#define SUPPORT_VID_SORENSON	0x0004
#define SUPPORT_VID_HOMEBREW	0x0008
#define SUPPORT_VID_VP6		0x0010
#define SUPPORT_VID_VP6ALPHA	0x0020
#define SUPPORT_VID_HOMEBREWV	0x0040
#define SUPPORT_VID_H264	0x0080
#define SUPPORT_VID_ALL		0x00FF

#define SUPPORT_VID_CLIENT_SEEK	1

#define kAMF0 0
#define kAMF3 3

#define RTMP_DEFAULT_ACK_WINDOW 0x200000

#define RTMP_TYPE_CHUNKSIZE 0x01
#define RTMP_TYPE_ABORT 0x2
#define RTMP_TYPE_ACK 0x3
#define RTMP_TYPE_USERCTRL 0x04
#define RTMP_TYPE_WINDOW_ACK_SIZE 0x5
#define RTMP_TYPE_SET_PEER_BW 0x6
#define RTMP_TYPE_AUDIO  0x08
#define RTMP_TYPE_VIDEO  0x09
#define RTMP_TYPE_METADATA 0x12
#define RTMP_TYPE_INVOKE 0x14
#define RTMP_TYPE_NOTIFY 0x12

#define RTMP_CTRL_STREAM_BEGIN 0x00
#define RTMP_CTRL_STREAM_EOF 0x01
#define RTMP_CTRL_STREAM_DRY 0x02
#define RTMP_CTRL_SET_BUFFER_LENGTH 0x03
#define RTMP_CTRL_STREAM_IS_RECORDED 0x04
#define RTMP_CTRL_PING_REQUEST 0x06
#define RTMP_CTRL_PING_RESPONSE 0x07

#define RTMP_DEFAULT_STREAM_CONTROL 0x02
#define RTMP_DEFAULT_STREAM_INVOKE 0x03
#define RTMP_DEFAULT_STREAM_NOTIFY 0x05
#define RTMP_DEFAULT_STREAM_VIDEO 0x07
#define RTMP_DEFAULT_STREAM_AUDIO 0x06


#define RTMP_MSGSTREAM_DEFAULT 0x0
/* It seems everything media-related (play/onStatus and the actual audio data are using this stream) */
#define RTMP_MSGSTREAM_MEDIA 0x01

#define RTMP_EVENT_CONNECT "rtmp::connect"
#define RTMP_EVENT_DISCONNECT "rtmp::disconnect"
#define RTMP_EVENT_REGISTER "rtmp::register"
#define RTMP_EVENT_UNREGISTER "rtmp::unregister"
#define RTMP_EVENT_LOGIN "rtmp::login"
#define RTMP_EVENT_LOGOUT "rtmp::logout"
#define RTMP_EVENT_ATTACH "rtmp::attach"
#define RTMP_EVENT_DETACH "rtmp::detach"
#define RTMP_EVENT_CUSTOM "rtmp::custom"
#define RTMP_EVENT_CLIENTCUSTOM "rtmp::clientcustom"

#define INT32_LE(x) (x) & 0xFF, ((x) >> 8) & 0xFF, ((x) >> 16) & 0xFF, ((x) >> 24) & 0xFF
#define INT32(x)	((x) >> 24) & 0xFF, ((x) >> 16) & 0xFF, ((x) >> 8) & 0xFF, (x) & 0xFF
#define INT24(x)	((x) >> 16) & 0xFF, ((x) >> 8) & 0xFF, (x) & 0xFF
#define INT16(x)	((x) >> 8) & 0xFF, (x) & 0xFF

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffffL
#endif

/* Media debug flags */
#define RTMP_MD_AUDIO_READ    (1 << 0)
#define RTMP_MD_AUDIO_WRITE   (1 << 1)
#define RTMP_MD_VIDEO_READ    (1 << 2)
#define RTMP_MD_VIDEO_WRITE   (1 << 3)

typedef enum {
	RTMP_AUDIO_PCM = 0,
	RTMP_AUDIO_ADPCM = 1,
	RTMP_AUDIO_MP3 = 2,
	RTMP_AUDIO_NELLYMOSER_8K_MONO= 5,
	RTMP_AUDIO_NELLYMOSER = 6,
	RTMP_AUDIO_SPEEX = 11
} rtmp_audio_format_t;

/*

From: http://osflash.org/flv

0x08: AUDIO
The first byte of an audio packet contains bitflags that describe the codec used, with the following layout:



Name	 Expression	 Description
soundType	 (byte & 0×01) » 0	 0: mono, 1: stereo
soundSize	 (byte & 0×02) » 1	 0: 8-bit, 1: 16-bit
soundRate	 (byte & 0x0C) » 2	 0: 5.5 kHz, 1: 11 kHz, 2: 22 kHz, 3: 44 kHz
soundFormat	 (byte & 0xf0) » 4	 0: Uncompressed, 1: ADPCM, 2: MP3, 5: Nellymoser 8kHz mono, 6: Nellymoser, 11: Speex

0x09: VIDEO
The first byte of a video packet describes contains bitflags that describe the codec used, and the type of frame

Name	 Expression	 Description
codecID	 (byte & 0x0f) » 0	 2: Sorensen H.263, 3: Screen video, 4: On2 VP6, 5: On2 VP6 Alpha, 6: ScreenVideo 2
frameType	 (byte & 0xf0) » 4	 1: keyframe, 2: inter frame, 3: disposable inter frame

0x12: META
The contents of a meta packet are two AMF packets.
The first is almost always a short uint16_be length-prefixed UTF-8 string (AMF type 0×02),
and the second is typically a mixed array (AMF type 0×08). However, the second chunk typically contains a variety of types,
so a full AMF parser should be used.
*/


static inline int rtmp_audio_codec_get_channels(uint8_t codec) {
	return (codec & 0x01) ? 2 : 1;
}

static inline int rtmp_audio_codec_get_sample_size(uint8_t codec) {
	return (codec & 0x02) ? 16 : 8;
}

static inline int rtmp_audio_codec_get_rate(uint8_t codec) {
	switch(codec & 0x0C) {
		case 0:
			return 5500;
		case 1:
			return 11000;
		case 2:
			return 22000;
		case 3:
			return 44000;
		default:
			return 0;
	}
}

static inline rtmp_audio_format_t rtmp_audio_codec_get_format(uint8_t codec) {
	return (rtmp_audio_format_t)(codec & 0xf0);
}

static inline uint8_t rtmp_audio_codec(int channels, int bits, int rate, rtmp_audio_format_t format) {
	uint8_t codec = 0;

	switch (channels) {
		case 1:
			break;
		case 2:
			codec |= 1;
		default:
			return 0;
	}

	switch (bits) {
		case 8:
			break;
		case 16:
			codec |= 2;
		default:
			return 0;
	}

	switch (rate) {
		case 0:
		case 5500:
			break;
		case 11000:
			codec |= 0x4;
			break;
		case 22000:
			codec |= 0x8;
			break;
		case 44000:
			codec |= 0xC;
		default:
			return 0;
	}

	switch(format) {
		case RTMP_AUDIO_PCM:
			break;
		case RTMP_AUDIO_ADPCM:
			codec |= 0x10;
			break;
		case RTMP_AUDIO_MP3:
			codec |= 0x20;
			break;
		case RTMP_AUDIO_NELLYMOSER_8K_MONO:
			codec |= 0x50;
			break;
		case RTMP_AUDIO_NELLYMOSER:
			codec |= 0x60;
			break;
		case RTMP_AUDIO_SPEEX:
			codec |= 0x80;
			break;
		default:
			return 0;
	}

	return codec;
}


struct rtmp_session;
typedef struct rtmp_session rtmp_session_t;

struct rtmp_profile;
typedef struct rtmp_profile rtmp_profile_t;

typedef struct rtmp_state rtmp_state_t;

#define RTMP_INVOKE_FUNCTION_ARGS rtmp_session_t *rsession, rtmp_state_t *state, int amfnumber, int transaction_id, int argc, amf0_data *argv[]

typedef switch_status_t (*rtmp_invoke_function_t)(RTMP_INVOKE_FUNCTION_ARGS);

#define RTMP_INVOKE_FUNCTION(_x) switch_status_t _x (RTMP_INVOKE_FUNCTION_ARGS)

/* AMF Helpers */

#define amf0_is_string(_x) (_x && (_x)->type == AMF0_TYPE_STRING)
#define amf0_is_number(_x) (_x && (_x)->type == AMF0_TYPE_NUMBER)
#define amf0_is_boolean(_x) (_x && (_x)->type == AMF0_TYPE_BOOLEAN)
#define amf0_is_object(_x) (_x && (_x)->type == AMF0_TYPE_OBJECT)

static inline char *amf0_get_string(amf0_data *x)
{
	return (amf0_is_string(x) ? (char*)amf0_string_get_uint8_ts(x) : NULL);
}

static inline int amf0_get_number(amf0_data *x)
{
	return (amf0_is_number(x) ? amf0_number_get_value(x) : 0);
}

static inline switch_bool_t amf0_get_boolean(amf0_data *x)
{
	return (amf0_is_boolean(x) ? amf0_boolean_get_value(x) : SWITCH_FALSE);
}

struct rtmp_io {
	switch_status_t  (*read)(rtmp_session_t *rsession, unsigned char *buf, switch_size_t *len);
	switch_status_t (*write)(rtmp_session_t *rsession, const unsigned char *buf, switch_size_t *len);
	switch_status_t (*close)(rtmp_session_t *rsession);
	rtmp_profile_t *profile;
	switch_memory_pool_t *pool;
	int running;
	const char *name;
	const char *address;
};

typedef struct rtmp_io rtmp_io_t;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_DETACHED = (1 << 1),		/* Call isn't the current active call */
	TFLAG_BREAK = (1 << 2),
	TFLAG_THREE_WAY = (1 << 3),		/* In a three-way call */
	TFLAG_VID_WAIT_KEYFRAME = (1 << 4)	/* Wait for video keyframe */
} TFLAGS;


/* Session flags */
typedef enum {
	SFLAG_AUDIO = (1 << 0),		/* < Send audio */
	SFLAG_VIDEO = (1 << 1)		/* < Send video */
} SFLAGS;

typedef enum {
	PFLAG_RUNNING = (1 << 0)
} PFLAGS;

struct mod_rtmp_globals {
	switch_endpoint_interface_t *rtmp_endpoint_interface;
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	switch_hash_t *profile_hash;
	switch_thread_rwlock_t *profile_rwlock;
	switch_hash_t *session_hash;
	switch_thread_rwlock_t *session_rwlock;
	switch_hash_t *invoke_hash;
	int running;
};

extern struct mod_rtmp_globals rtmp_globals;

struct rtmp_profile {
	char *name;			/* < Profile name */
	switch_memory_pool_t *pool;	/* < Memory pool */
	rtmp_io_t *io;			/* < IO Module instance */
	switch_thread_rwlock_t *rwlock;	/* < Rwlock for reference counting */
	uint32_t flags;			/* < PFLAGS */
	switch_mutex_t *mutex;		/* < Mutex for call count */
	int calls;			/* < Active calls count */
	int clients;			/* < Number of connected clients */
	switch_hash_t *session_hash;	/* < Active rtmp sessions */
	switch_thread_rwlock_t *session_rwlock; /* < rwlock for session hashtable */
	const char *context;		/* < Default dialplan name */
	const char *dialplan;		/* < Default dialplan context */
	const char *bind_address;	/* < Bind address */
	const char *io_name;		/* < Name of I/O module (from config) */
	int chunksize;				/* < Override default chunksize (from config) */
	int buffer_len;				/* < Receive buffer length the flash clients should use */

	switch_hash_t *reg_hash;	/* < Registration hashtable */
	switch_thread_rwlock_t *reg_rwlock; /* < Registration hash rwlock */

	switch_bool_t auth_calls;	/* < Require authentiation */
};

typedef struct {
	unsigned ts:24;
	unsigned len:24;
	unsigned type:8;
	unsigned src:16;
	unsigned dst:16;
} rtmp_hdr_t;

#define RTMP_DEFAULT_CHUNKSIZE 128

struct rtmp_state {
	union {
		char sz[12];
		rtmp_hdr_t packed;
	} header;
	int remainlen;
	int origlen;

	uint32_t ts; /* 24 bits max */
	uint32_t ts_delta; /* 24 bits max */
	uint8_t type;
	uint32_t stream_id;
	unsigned char buf[AMF_MAX_SIZE];
	switch_size_t buf_pos;
};


typedef enum {
	RS_HANDSHAKE = 0,
	RS_HANDSHAKE2 = 1,
	RS_ESTABLISHED = 2,
	RS_DESTROY = 3
} rtmp_session_state_t;

struct rtmp_private;
typedef struct rtmp_private rtmp_private_t;


struct rtmp_account;
typedef struct rtmp_account rtmp_account_t;

struct rtmp_account {
	const char *user;
	const char *domain;
	rtmp_account_t *next;
};

typedef struct rtmp2rtp_helper_s
{
	amf0_data	*sps;
	amf0_data	*pps;
	amf0_data	*nal_list;
	uint32_t	lenSize;
} rtmp2rtp_helper_t;

typedef struct rtp2rtmp_helper_s
{
	amf0_data       *sps;
	amf0_data       *pps;
	amf0_data       *avc_conf;
	switch_bool_t   send;
	switch_bool_t   send_avc;
	switch_buffer_t *rtmp_buf;
	switch_buffer_t *fua_buf; //fu_a buf
	uint32_t        last_recv_ts;
	uint8_t         last_mark;
	uint16_t        last_seq;
	switch_bool_t   sps_changed;
} rtp2rtmp_helper_t;

struct rtmp_session {
	switch_memory_pool_t *pool;
	rtmp_profile_t *profile;
	char uuid[SWITCH_UUID_FORMATTED_LENGTH+1];
	void *io_private;

	rtmp_session_state_t state;
	int parse_state;
	uint16_t parse_remain; /* < Remaining bytes required before changing parse state */

	int hdrsize;	/* < The current header size */
	int amfnumber;	/* < The current AMF number */

	rtmp_state_t amfstate[64];
	rtmp_state_t amfstate_out[64];

	switch_mutex_t *socket_mutex;
	switch_mutex_t *count_mutex;
	int active_sessions;

	unsigned char hsbuf[2048];
	int hspos;
	uint16_t in_chunksize;
	uint16_t out_chunksize;

	/* Connect params */
	const char *flashVer;
	const char *swfUrl;
	const char *tcUrl;
	const char *app;
	const char *pageUrl;

	uint32_t capabilities;
	uint32_t audioCodecs;
	uint32_t videoCodecs;
	uint32_t videoFunction;

	switch_thread_rwlock_t *rwlock;

	rtmp_private_t *tech_pvt;		/* < Active call's tech_pvt */
#ifdef RTMP_DEBUG_IO
	FILE *io_debug_in;
	FILE *io_debug_out;
#endif

	const char *remote_address;
	switch_port_t remote_port;

	switch_hash_t *session_hash;		/* < Hash of call uuids and tech_pvt */
	switch_thread_rwlock_t *session_rwlock;	/* < RWLock protecting session_hash */

	rtmp_account_t *account;
	switch_thread_rwlock_t *account_rwlock;
	uint32_t flags;

	int8_t sendAudio, sendVideo;
	uint64_t recv_ack_window;			/* < ACK Window */
	uint64_t recv_ack_sent;				/* < Bytes ack'd */
	uint64_t recv;						/* < Bytes received */

	uint32_t send_ack_window;
	uint32_t send_ack;
	uint32_t send;
	switch_time_t send_ack_ts;

	uint32_t send_bw;					/* < Current send bandwidth (in bytes/sec) */

	uint32_t next_streamid;				/* < The next stream id that will be used */
	uint32_t active_streamid;			/* < The stream id returned by the last call to createStream */

	uint32_t media_streamid;			/* < The stream id that was used for the last "play" command,
											where we should send media */
	switch_size_t dropped_video_frame;

	uint8_t media_debug;
};

struct rtmp_private {
	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;

	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];	/* < Buffer for read_frame */

	switch_caller_profile_t *caller_profile;

	switch_mutex_t *mutex;
	switch_mutex_t *flag_mutex;

	switch_core_session_t *session;
	switch_channel_t *channel;
	rtmp_session_t *rtmp_session;

	int read_channel; /* RTMP channel #s for read and write */
	int write_channel;
	uint8_t audio_codec;
	uint8_t video_codec;

	switch_time_t stream_start_ts;
	switch_time_t stream_last_ts;
	switch_timer_t timer;
	switch_buffer_t *readbuf;
	switch_mutex_t *readbuf_mutex;

	const char *display_callee_id_name;
	const char *display_callee_id_number;

	const char *auth_user;
	const char *auth_domain;
	const char *auth;

	uint16_t maxlen;
	int over_size;

	//video
	int has_video;
	char *video_max_bandwidth_out;
	switch_codec_t video_read_codec;
	switch_codec_t video_write_codec;
	rtp2rtmp_helper_t video_write_helper;
	rtmp2rtp_helper_t video_read_helper;
	switch_frame_t video_read_frame;
	uint32_t video_read_ts;
	uint16_t seq;
	unsigned char video_databuf[SWITCH_RTP_MAX_BUF_LEN];	/* < Buffer for read_frame */
	switch_buffer_t *video_readbuf;
	switch_mutex_t *video_readbuf_mutex;
	uint16_t video_maxlen;
	int video_over_size;

	switch_core_media_params_t mparams;
	switch_media_handle_t *media_handle;
};

struct rtmp_reg;
typedef struct rtmp_reg rtmp_reg_t;

struct rtmp_reg {
	const char *uuid;	/* < The rtmp session id */
	const char *nickname;	/* < This instance's nickname, optional */
	const char *user;
	const char *domain;
	rtmp_reg_t *next;	/* < Next entry */
};


typedef enum {
	MSG_FULLHEADER = 1
} rtmp_message_send_flag_t;

/* Invokable functions from flash */
RTMP_INVOKE_FUNCTION(rtmp_i_connect);
RTMP_INVOKE_FUNCTION(rtmp_i_createStream);
RTMP_INVOKE_FUNCTION(rtmp_i_initStream);
RTMP_INVOKE_FUNCTION(rtmp_i_noop);
RTMP_INVOKE_FUNCTION(rtmp_i_play);
RTMP_INVOKE_FUNCTION(rtmp_i_publish);
RTMP_INVOKE_FUNCTION(rtmp_i_makeCall);
RTMP_INVOKE_FUNCTION(rtmp_i_fcSubscribe);
RTMP_INVOKE_FUNCTION(rtmp_i_sendDTMF);
RTMP_INVOKE_FUNCTION(rtmp_i_login);
RTMP_INVOKE_FUNCTION(rtmp_i_logout);
RTMP_INVOKE_FUNCTION(rtmp_i_register);
RTMP_INVOKE_FUNCTION(rtmp_i_unregister);
RTMP_INVOKE_FUNCTION(rtmp_i_answer);
RTMP_INVOKE_FUNCTION(rtmp_i_attach);
RTMP_INVOKE_FUNCTION(rtmp_i_hangup);
RTMP_INVOKE_FUNCTION(rtmp_i_transfer);
RTMP_INVOKE_FUNCTION(rtmp_i_three_way);
RTMP_INVOKE_FUNCTION(rtmp_i_join);
RTMP_INVOKE_FUNCTION(rtmp_i_sendevent);
RTMP_INVOKE_FUNCTION(rtmp_i_receiveaudio);
RTMP_INVOKE_FUNCTION(rtmp_i_receivevideo);
RTMP_INVOKE_FUNCTION(rtmp_i_log);

/*** RTMP Sessions ***/
rtmp_session_t *rtmp_session_locate(const char *uuid);
void rtmp_session_rwunlock(rtmp_session_t *rsession);

switch_status_t rtmp_session_login(rtmp_session_t *rsession, const char *user, const char *domain);
switch_status_t rtmp_session_logout(rtmp_session_t *rsession, const char *user, const char *domain);
switch_status_t rtmp_session_check_user(rtmp_session_t *rsession, const char *user, const char *domain);

switch_status_t rtmp_check_auth(rtmp_session_t *rsession, const char *user, const char *domain, const char *authmd5);
void rtmp_event_fill(rtmp_session_t *rsession, switch_event_t *event);
switch_status_t amf_object_to_event(amf0_data *obj, switch_event_t **event);
switch_status_t amf_event_to_object(amf0_data **obj, switch_event_t *event);

/*** Endpoint interface ***/
switch_call_cause_t rtmp_session_create_call(rtmp_session_t *rsession, switch_core_session_t **newsession, int read_channel, int write_channel, const char *number, const char *auth_user, const char *auth_domain, switch_event_t *event);

switch_status_t rtmp_on_execute(switch_core_session_t *session);
switch_status_t rtmp_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf);
switch_status_t rtmp_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg);
switch_status_t rtmp_receive_event(switch_core_session_t *session, switch_event_t *event);
switch_status_t rtmp_on_init(switch_core_session_t *session);
switch_status_t rtmp_on_hangup(switch_core_session_t *session);
switch_status_t rtmp_on_destroy(switch_core_session_t *session);
switch_status_t rtmp_on_routing(switch_core_session_t *session);
switch_status_t rtmp_on_exchange_media(switch_core_session_t *session);
switch_status_t rtmp_on_soft_execute(switch_core_session_t *session);
switch_call_cause_t rtmp_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
											switch_caller_profile_t *outbound_profile,
											switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
											switch_call_cause_t *cancel_cause);
switch_status_t rtmp_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
switch_status_t rtmp_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
switch_status_t rtmp_kill_channel(switch_core_session_t *session, int sig);

switch_status_t rtmp_tech_init(rtmp_private_t *tech_pvt, rtmp_session_t *rtmp_session, switch_core_session_t *session);
rtmp_profile_t *rtmp_profile_locate(const char *name);
void rtmp_profile_release(rtmp_profile_t *profile);

/**** I/O ****/
switch_status_t rtmp_tcp_init(rtmp_profile_t *profile, const char *bindaddr, rtmp_io_t **new_io, switch_memory_pool_t *pool);
switch_status_t rtmp_session_request(rtmp_profile_t *profile, rtmp_session_t **newsession);
switch_status_t rtmp_session_destroy(rtmp_session_t **session);
switch_status_t rtmp_real_session_destroy(rtmp_session_t **session);

/**** Protocol ****/
void rtmp_set_chunksize(rtmp_session_t *rsession, uint32_t chunksize);
switch_status_t rtmp_send_invoke(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...);
switch_status_t rtmp_send_invoke_free(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...);
switch_status_t rtmp_send_notify(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...);
switch_status_t rtmp_send_notify_free(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...);
switch_status_t rtmp_send_invoke_v(rtmp_session_t *rsession, uint8_t amfnumber, uint8_t type, uint32_t timestamp, uint32_t stream_id, va_list list, switch_bool_t freethem);
switch_status_t rtmp_send_message(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint8_t type, uint32_t stream_id, const unsigned char *message, switch_size_t len, uint32_t flags);

void rtmp_send_event(rtmp_session_t *rsession, switch_event_t *event);
void rtmp_notify_call_state(switch_core_session_t *session);
void rtmp_send_display_update(switch_core_session_t *session);
void rtmp_send_incoming_call(switch_core_session_t *session, switch_event_t *var_event);
void rtmp_send_onhangup(switch_core_session_t *session);
void rtmp_add_registration(rtmp_session_t *rsession, const char *auth, const char *nickname);
void rtmp_clear_registration(rtmp_session_t *rsession, const char *auth, const char *nickname);
/* Attaches an rtmp session to one of its calls, use NULL to hold everything */
void rtmp_attach_private(rtmp_session_t *rsession, rtmp_private_t *tech_pvt);
rtmp_private_t *rtmp_locate_private(rtmp_session_t *rsession, const char *uuid);
void rtmp_ping(rtmp_session_t *rsession);

void rtmp_session_send_onattach(rtmp_session_t *rsession);

/* Protocol handler */
switch_status_t rtmp_handle_data(rtmp_session_t *rsession);

#endif /* defined(MOD_RTMP_H) */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
