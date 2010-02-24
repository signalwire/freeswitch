/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 *
 * mod_skinny.c -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_skinny_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skinny_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_skinny_runtime);

SWITCH_MODULE_DEFINITION(mod_skinny, mod_skinny_load, mod_skinny_shutdown, mod_skinny_runtime);
#define SKINNY_EVENT_REGISTER "skinny::register"


switch_endpoint_interface_t *skinny_endpoint_interface;
static switch_memory_pool_t *module_pool = NULL;
static int running = 1;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_LINEAR = (1 << 6),
	TFLAG_CODEC = (1 << 7),
	TFLAG_BREAK = (1 << 8)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

static struct {
	/* prefs */
	int debug;
	char *domain;
	char *ip;
	unsigned int port;
	char *dialplan;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	uint32_t keep_alive;
	char date_format[6];
	/* data */
	switch_event_node_t *heartbeat_node;
	unsigned int flags;
	int calls;
	switch_mutex_t *mutex;
	switch_mutex_t *listener_mutex;	
	int listener_threads;
} globals;

struct private_object {
	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
	switch_mutex_t *mutex;
	switch_mutex_t *flag_mutex;
	//switch_thread_cond_t *cond;
};

typedef struct private_object private_t;


SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_domain, globals.domain);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ip, globals.ip);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_rates_string, globals.codec_rates_string);

/*****************************************************************************/
/* SKINNY MESSAGES */
/*****************************************************************************/

/* KeepAliveMessage */
#define KEEP_ALIVE_MESSAGE 0x0000

/* RegisterMessage */
#define REGISTER_MESSAGE 0x0001
struct register_message {
	char deviceName[16];
	uint32_t userId;
	uint32_t instance;
	struct in_addr ip;
	uint32_t deviceType;
	uint32_t maxStreams;
};

/* IpPortMessage */
#define PORT_MESSAGE 0x0002

/* LineStatReqMessage */
#define LINE_STAT_REQ_MESSAGE 0x000B
struct line_stat_req_message {
	uint32_t number;
};

/* CapabilitiesResMessage */
struct station_capabilities {
	uint32_t codec;
	uint16_t frames;
	char reserved[10];
};

#define CAPABILITIES_RES_MESSAGE 0x0010
struct capabilities_res_message {
	uint32_t count;
	struct station_capabilities caps[SWITCH_MAX_CODECS];
};

/* RegisterAckMessage */
#define REGISTER_ACK_MESSAGE 0x0081
struct register_ack_message {
	uint32_t keepAlive;
	char dateFormat[6];
	char reserved[2];
	uint32_t secondaryKeepAlive;
	char reserved2[4];
};

/* LineStatMessage */
#define LINE_STAT_RES_MESSAGE 0x0092
struct line_stat_res_message {
	uint32_t number;
	char name[24];
	char shortname[40];
	char displayname[44];
};

/* CapabilitiesReqMessage */
#define CAPABILITIES_REQ_MESSAGE 0x009B

/* RegisterRejectMessage */
#define REGISTER_REJ_MESSAGE 0x009D
struct register_rej_message {
	char error[33];
};

/* KeepAliveAckMessage */
#define KEEP_ALIVE_ACK_MESSAGE 0x0100

/* Message */
#define SKINNY_MESSAGE_FIELD_SIZE 4 /* 4-bytes field */
#define SKINNY_MESSAGE_HEADERSIZE 12 /* three 4-bytes fields */
#define SKINNY_MESSAGE_MAXSIZE 1000

union skinny_data {
	struct register_message reg;
	struct register_ack_message reg_ack;
	struct line_stat_req_message line_req;
	struct capabilities_res_message cap_res;
	struct line_stat_res_message line_res;
	struct register_rej_message reg_rej;
	
	uint16_t as_uint16;
	char as_char;
	void *raw;
};

struct skinny_message {
	int length;
	int reserved;
	int type;
	union skinny_data data;
};
typedef struct skinny_message skinny_message_t;

/*****************************************************************************/
/* SKINNY TYPES */
/*****************************************************************************/

#define SKINNY_MAX_LINES 10
struct skinny_line {
	struct skinny_device_t *device;
	char name[24];
	char shortname[40];
	char displayname[44];
};
typedef struct skinny_line skinny_line_t;

#define SKINNY_MAX_SPEEDDIALS 20
struct skinny_speeddial {
	struct skinny_device_t *device;
	char number[24];
	char displayname[40];
};
typedef struct skinny_speeddial skinny_speeddial_t;

struct skinny_device {
	char deviceName[16];
	uint32_t userId;
	uint32_t instance;
	struct in_addr ip;
	uint32_t deviceType;
	uint32_t maxStreams;

	uint16_t port;

	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;

	skinny_line_t line[SKINNY_MAX_LINES];
	int line_last;
	skinny_speeddial_t speeddial[SKINNY_MAX_SPEEDDIALS];
	int speeddial_last;
};
typedef struct skinny_device skinny_device_t;

typedef switch_status_t (*skinny_command_t) (char **argv, int argc, switch_stream_handle_t *stream);

enum skinny_codecs {
	SKINNY_CODEC_ALAW_64K = 2,
	SKINNY_CODEC_ALAW_56K = 3,
	SKINNY_CODEC_ULAW_64K = 4,
	SKINNY_CODEC_ULAW_56K = 5,
	SKINNY_CODEC_G722_64K = 6,
	SKINNY_CODEC_G722_56K = 7,
	SKINNY_CODEC_G722_48K = 8,
	SKINNY_CODEC_G723_1 = 9,
	SKINNY_CODEC_G728 = 10,
	SKINNY_CODEC_G729 = 11,
	SKINNY_CODEC_G729A = 12,
	SKINNY_CODEC_IS11172 = 13,
	SKINNY_CODEC_IS13818 = 14,
	SKINNY_CODEC_G729B = 15,
	SKINNY_CODEC_G729AB = 16,
	SKINNY_CODEC_GSM_FULL = 18,
	SKINNY_CODEC_GSM_HALF = 19,
	SKINNY_CODEC_GSM_EFULL = 20,
	SKINNY_CODEC_WIDEBAND_256K = 25,
	SKINNY_CODEC_DATA_64K = 32,
	SKINNY_CODEC_DATA_56K = 33,
	SKINNY_CODEC_GSM = 80,
	SKINNY_CODEC_ACTIVEVOICE = 81,
	SKINNY_CODEC_G726_32K = 82,
	SKINNY_CODEC_G726_24K = 83,
	SKINNY_CODEC_G726_16K = 84,
	SKINNY_CODEC_G729B_BIS = 85,
	SKINNY_CODEC_G729B_LOW = 86,
	SKINNY_CODEC_H261 = 100,
	SKINNY_CODEC_H263 = 101,
	SKINNY_CODEC_VIDEO = 102,
	SKINNY_CODEC_T120 = 105,
	SKINNY_CODEC_H224 = 106,
	SKINNY_CODEC_RFC2833_DYNPAYLOAD = 257
};

/*****************************************************************************/
/* LISTENERS TYPES */
/*****************************************************************************/

typedef enum {
	LFLAG_RUNNING = (1 << 0),
} event_flag_t;

struct listener {
	skinny_device_t *device;
	switch_socket_t *sock;
	switch_memory_pool_t *pool;
	switch_core_session_t *session;
	switch_thread_rwlock_t *rwlock;
	switch_sockaddr_t *sa;
	char remote_ip[50];
	switch_mutex_t *flag_mutex;
	uint32_t flags;
	switch_port_t remote_port;
	uint32_t id;
	time_t expire_time;
	struct listener *next;
};

typedef struct listener listener_t;

typedef switch_status_t (*skinny_listener_callback_func_t) (listener_t *listener, void *pvt);

static struct {
	switch_socket_t *sock;
	switch_mutex_t *sock_mutex;
	listener_t *listeners;
	uint8_t ready;
} listen_list;

/*****************************************************************************/
/* FUNCTIONS */
/*****************************************************************************/

/* CHANNEL FUNCTIONS */
static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_hangup(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);
static switch_status_t channel_on_routing(switch_core_session_t *session);
static switch_status_t channel_on_exchange_media(switch_core_session_t *session);
static switch_status_t channel_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);



/* SKINNY FUNCTIONS */
static switch_status_t skinny_send_reply(listener_t *listener, skinny_message_t *reply);

/* LISTENER FUNCTIONS */
static switch_status_t keepalive_listener(listener_t *listener, void *pvt);

/*****************************************************************************/
/* CHANNEL FUNCTIONS */
/*****************************************************************************/

static void tech_init(private_t *tech_pvt, switch_core_session_t *session)
{
	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_core_session_set_private(session, tech_pvt);
	tech_pvt->session = session;
}

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	/* Move channel's state machine to ROUTING. This means the call is trying
	   to get from the initial start where the call because, to the point
	   where a destination has been identified. If the channel is simply
	   left in the initial state, nothing will happen. */
	switch_channel_set_state(channel, CS_ROUTING);
	switch_mutex_lock(globals.mutex);
	globals.calls++;
	switch_mutex_unlock(globals.mutex);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL INIT\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);

	if (tech_pvt) {
		if (switch_core_codec_ready(&tech_pvt->read_codec)) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}
		
		if (switch_core_codec_ready(&tech_pvt->write_codec)) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL DESTROY\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
	//switch_thread_cond_signal(tech_pvt->cond);


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));
	switch_mutex_lock(globals.mutex);
	globals.calls--;
	if (globals.calls < 0) {
		globals.calls = 0;
	}
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (sig) {
	case SWITCH_SIG_KILL:
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		//switch_thread_cond_sigpnal(tech_pvt->cond);
		break;
	case SWITCH_SIG_BREAK:
		switch_set_flag_locked(tech_pvt, TFLAG_BREAK);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	//switch_time_t started = switch_time_now();
	//unsigned int elapsed;
	switch_byte_t *data;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	tech_pvt->read_frame.flags = SFF_NONE;
	*frame = NULL;

	while (switch_test_flag(tech_pvt, TFLAG_IO)) {

		if (switch_test_flag(tech_pvt, TFLAG_BREAK)) {
			switch_clear_flag(tech_pvt, TFLAG_BREAK);
			goto cng;
		}

		if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
			return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(tech_pvt, TFLAG_IO) && switch_test_flag(tech_pvt, TFLAG_VOICE)) {
			switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
			if (!tech_pvt->read_frame.datalen) {
				continue;
			}
			*frame = &tech_pvt->read_frame;
#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
			if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
				switch_swap_linear((*frame)->data, (int) (*frame)->datalen / 2);
			}
#endif
			return SWITCH_STATUS_SUCCESS;
		}

		switch_cond_next();
	}


	return SWITCH_STATUS_FALSE;

  cng:
	data = (switch_byte_t *) tech_pvt->read_frame.data;
	data[0] = 65;
	data[1] = 0;
	tech_pvt->read_frame.datalen = 2;
	tech_pvt->read_frame.flags = SFF_CNG;
	*frame = &tech_pvt->read_frame;
	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	//switch_frame_t *pframe;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_FALSE;
	}
#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
		switch_swap_linear(frame->data, (int) frame->datalen / 2);
	}
#endif


	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	private_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		{
			channel_answer_channel(session);
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	if ((*new_session = switch_core_session_request(skinny_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, pool)) != 0) {
		private_t *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *caller_profile;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t))) != 0) {
			channel = switch_core_session_get_channel(*new_session);
			tech_init(tech_pvt, *new_session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (outbound_profile) {
			char name[128];

			snprintf(name, sizeof(name), "SKINNY/%s", outbound_profile->destination_number);
			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_ERROR, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}



		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_CAUSE_SUCCESS;
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

}

static switch_status_t channel_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	struct private_object *tech_pvt = switch_core_session_get_private(session);
	char *body = switch_event_get_body(event);
	switch_assert(tech_pvt != NULL);

	if (!body) {
		body = "";
	}

	return SWITCH_STATUS_SUCCESS;
}



switch_state_handler_table_t skinny_state_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media*/ NULL,
	/*.on_hibernate*/ NULL,
	/*.on_reset*/ NULL,
	/*.on_park*/ NULL,
	/*.on_reporting*/ NULL,
	/*.on_destroy*/ channel_on_destroy

};

switch_io_routines_t skinny_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message,
	/*.receive_event */ channel_receive_event
};

/*****************************************************************************/
/* SKINNY FUNCTIONS */
/*****************************************************************************/

static char* skinny_codec2string(enum skinny_codecs skinnycodec)
{
	switch (skinnycodec) {
		case SKINNY_CODEC_ALAW_64K:
		case SKINNY_CODEC_ALAW_56K:
			return "ALAW";
		case SKINNY_CODEC_ULAW_64K:
		case SKINNY_CODEC_ULAW_56K:
			return "ULAW";
		case SKINNY_CODEC_G722_64K:
		case SKINNY_CODEC_G722_56K:
		case SKINNY_CODEC_G722_48K:
			return "G722";
		case SKINNY_CODEC_G723_1:
			return "G723";
		case SKINNY_CODEC_G728:
			return "G728";
		case SKINNY_CODEC_G729:
		case SKINNY_CODEC_G729A:
			return "G729";
		case SKINNY_CODEC_IS11172:
			return "IS11172";
		case SKINNY_CODEC_IS13818:
			return "IS13818";
		case SKINNY_CODEC_G729B:
		case SKINNY_CODEC_G729AB:
			return "G729";
		case SKINNY_CODEC_GSM_FULL:
		case SKINNY_CODEC_GSM_HALF:
		case SKINNY_CODEC_GSM_EFULL:
			return "GSM";
		case SKINNY_CODEC_WIDEBAND_256K:
			return "WIDEBAND";
		case SKINNY_CODEC_DATA_64K:
		case SKINNY_CODEC_DATA_56K:
			return "DATA";
		case SKINNY_CODEC_GSM:
			return "GSM";
		case SKINNY_CODEC_ACTIVEVOICE:
			return "ACTIVEVOICE";
		case SKINNY_CODEC_G726_32K:
		case SKINNY_CODEC_G726_24K:
		case SKINNY_CODEC_G726_16K:
			return "G726";
		case SKINNY_CODEC_G729B_BIS:
		case SKINNY_CODEC_G729B_LOW:
			return "G729";
		case SKINNY_CODEC_H261:
			return "H261";
		case SKINNY_CODEC_H263:
			return "H263";
		case SKINNY_CODEC_VIDEO:
			return "VIDEO";
		case SKINNY_CODEC_T120:
			return "T120";
		case SKINNY_CODEC_H224:
			return "H224";
		case SKINNY_CODEC_RFC2833_DYNPAYLOAD:
			return "RFC2833_DYNPAYLOAD";
		default:
			return "";
	}
}

static switch_status_t skinny_read_packet(listener_t *listener, skinny_message_t **req)
{
	skinny_message_t *request;
	switch_size_t mlen, bytes = 0;
	char mbuf[SKINNY_MESSAGE_MAXSIZE] = "";
	char *ptr;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	request = switch_core_alloc(listener->pool, SKINNY_MESSAGE_MAXSIZE);

	if (!request) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to allocate memory.\n");
		return SWITCH_STATUS_MEMERR;
	}
	
	if (!running) {
		return SWITCH_STATUS_FALSE;
	}

	ptr = mbuf;

	while (listener->sock && running) {
		uint8_t do_sleep = 1;
		if(bytes < SKINNY_MESSAGE_FIELD_SIZE) {
			/* We have nothing yet, get length header field */
			mlen = SKINNY_MESSAGE_FIELD_SIZE - bytes;
		} else {
			/* We now know the message size */
			mlen = request->length + 2*SKINNY_MESSAGE_FIELD_SIZE - bytes;
		}

		status = switch_socket_recv(listener->sock, ptr, &mlen);
		
		if (!running || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Socket break.\n");
			return SWITCH_STATUS_FALSE;
		}
		
		if(mlen) {
			bytes += mlen;
			
			if(bytes >= SKINNY_MESSAGE_FIELD_SIZE) {
				do_sleep = 0;
				ptr += mlen;
				memcpy(request, mbuf, bytes);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					"Got request: length=%d,reserved=%x,type=%x\n",
					request->length,request->reserved,request->type);
				if(request->length < SKINNY_MESSAGE_FIELD_SIZE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						"Skinny client sent invalid data. Length should be greater than 4 but got %d.\n",
						request->length);
					return SWITCH_STATUS_FALSE;
				}
				if(request->length + 2*SKINNY_MESSAGE_FIELD_SIZE > SKINNY_MESSAGE_MAXSIZE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						"Skinny client sent too huge data. Got %d which is above threshold %d.\n",
						request->length, SKINNY_MESSAGE_MAXSIZE - 2*SKINNY_MESSAGE_FIELD_SIZE);
					return SWITCH_STATUS_FALSE;
				}
				if(bytes >= request->length + 2*SKINNY_MESSAGE_FIELD_SIZE) {
					/* Message body */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						"Got complete request: length=%d,reserved=%x,type=%x,data=%d\n",
						request->length,request->reserved,request->type,request->data.as_char);
					*req = request;
					return  SWITCH_STATUS_SUCCESS;
				}
			}
		}
		if (listener->expire_time && listener->expire_time < switch_epoch_time_now(NULL)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Listener timed out.\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			return SWITCH_STATUS_FALSE;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_device_event(listener_t *listener, switch_event_t **ev, switch_event_types_t event_id, const char *subclass_name)
{
	switch_event_t *event = NULL;
	skinny_device_t *device;
	switch_event_create_subclass(&event, event_id, subclass_name);
	switch_assert(event);
	if(listener->device) {
		device = listener->device;
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-Device-Name", switch_str_nil(device->deviceName));
		switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-User-Id", "%d", device->userId);
		switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Instance", "%d", device->instance);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-IP", inet_ntoa(device->ip));
		switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Device-Type", "%d", device->deviceType);
		switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Max-Streams", "%d", device->maxStreams);
		switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Port", "%d", device->port);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-Codecs", device->codec_string);
	}
	*ev = event;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_register(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	skinny_message_t *message;
	skinny_device_t *device;
	switch_event_t *event = NULL;
	switch_event_t *params = NULL;
	switch_xml_t xml = NULL, domain, group, users, user;

	if(listener->device) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"A device is already registred on this listener.\n");
		message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_rej));
		message->type = REGISTER_REJ_MESSAGE;
		message->length = 4 + sizeof(message->data.reg_rej);
		strcpy(message->data.reg_rej.error, "A device is already registred on this listener");
		skinny_send_reply(listener, message);
		return SWITCH_STATUS_FALSE;
	}
	/* Initialize device */
	device = switch_core_alloc(listener->pool, sizeof(skinny_device_t));
	memcpy(device->deviceName, request->data.reg.deviceName, 16);
	device->userId = request->data.reg.userId;
	device->instance = request->data.reg.instance;
	device->ip = request->data.reg.ip;
	device->deviceType = request->data.reg.deviceType;
	device->maxStreams = request->data.reg.maxStreams;
	device->codec_string = realloc(device->codec_string, 1);
	device->codec_string[0] = '\0';

	/* Check directory */
	skinny_device_event(listener, &params, SWITCH_EVENT_REQUEST_PARAMS, SWITCH_EVENT_SUBCLASS_ANY);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "skinny-auth");

	if (switch_xml_locate_group(device->deviceName, globals.domain, &xml, &domain, &group, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't find device [%s@%s]\n"
					  "You must define a domain called '%s' in your directory and add a group with the name=\"%s\" attribute.\n"
					  , device->deviceName, globals.domain, globals.domain, device->deviceName);
		message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_rej));
		message->type = REGISTER_REJ_MESSAGE;
		message->length = 4 + sizeof(message->data.reg_rej);
		strcpy(message->data.reg_rej.error, "Device not found");
		skinny_send_reply(listener, message);
		return SWITCH_STATUS_FALSE;
		goto end;
	}
	users = switch_xml_child(group, "users");
	device->line_last = 0;
	if(users) {
		for (user = switch_xml_child(users, "user"); user; user = user->next) {
			const char *id = switch_xml_attr_soft(user, "id");
			const char *type = switch_xml_attr_soft(user, "type");
			//TODO device->line[device->line_last].device = *device;
			strcpy(device->line[device->line_last].name, id);
			strcpy(device->line[device->line_last].shortname, id);
			strcpy(device->line[device->line_last].displayname, id);
			device->line_last++;
		}
	}

	listener->device = device;
	status = SWITCH_STATUS_SUCCESS;

	/* Reply with RegisterAckMessage */
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_ack));
	message->type = REGISTER_ACK_MESSAGE;
	message->length = 4 + sizeof(message->data.reg_ack);
	message->data.reg_ack.keepAlive = globals.keep_alive;
	memcpy(message->data.reg_ack.dateFormat, globals.date_format, 6);
	message->data.reg_ack.secondaryKeepAlive = globals.keep_alive;
	skinny_send_reply(listener, message);

	/* Send CapabilitiesReqMessage */
	message = switch_core_alloc(listener->pool, 12);
	message->type = CAPABILITIES_REQ_MESSAGE;
	message->length = 4;
	skinny_send_reply(listener, message);

	/* skinny::register event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_REGISTER);
	switch_event_fire(&event);
	
	keepalive_listener(listener, NULL);

end:
	if(params) {
		switch_event_destroy(&params);
	}
	
	return status;
}

static switch_status_t skinny_handle_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_device_t *device;
	uint32_t i = 0;
	uint32_t n = 0;
	size_t string_len, string_pos, pos;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Received message (type=%x,length=%d).\n", request->type, request->length);
	if(!listener->device && request->type != REGISTER_MESSAGE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"Device should send a register message first.\n");
		return SWITCH_STATUS_FALSE;
	}
	device = listener->device;
	switch(request->type) {
		case REGISTER_MESSAGE:
			return skinny_handle_register(listener, request);
		case CAPABILITIES_RES_MESSAGE:
			n = request->data.cap_res.count;
			if (n > SWITCH_MAX_CODECS) {
				n = SWITCH_MAX_CODECS;
			}
			string_len = -1;
			for (i = 0; i < n; i++) {
				char *codec = skinny_codec2string(request->data.cap_res.caps[i].codec);
				device->codec_order[i] = codec;
				string_len += strlen(codec)+1;
			}
			i = 0;
			pos = 0;
			device->codec_string = realloc(device->codec_string, string_len+1);
			for (string_pos = 0; string_pos < string_len; string_pos++) {
				char *codec = device->codec_order[i];
				switch_assert(i < n);
				if(pos == strlen(codec)) {
					device->codec_string[string_pos] = ',';
					i++;
					pos = 0;
				} else {
					device->codec_string[string_pos] = codec[pos++];
				}
			}
			device->codec_string[string_len] = '\0';
			device->codec_order_last = n;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
				"Codecs %s supported.\n", device->codec_string);
		case PORT_MESSAGE:
			device->port = request->data.as_uint16;
			break;
		case LINE_STAT_REQ_MESSAGE:
			message = switch_core_alloc(listener->pool, 12+sizeof(message->data.line_res));
			message->type = LINE_STAT_RES_MESSAGE;
			message->length = 4 + sizeof(message->data.line_res);
			i = request->data.line_req.number;
			if(i > 0 && i <= device->line_last) {
				message->data.line_res.number = i;
				strcpy(message->data.line_res.name, device->line[i-1].name);
				strcpy(message->data.line_res.shortname, device->line[i-1].shortname);
				strcpy(message->data.line_res.displayname, device->line[i-1].displayname);
			} else {
				strcpy(message->data.line_res.name, "");
				strcpy(message->data.line_res.shortname, "");
				strcpy(message->data.line_res.displayname, "");
			}
			skinny_send_reply(listener, message);
			break;
		case KEEP_ALIVE_MESSAGE:
			message = switch_core_alloc(listener->pool, 12);
			message->type = KEEP_ALIVE_ACK_MESSAGE;
			message->length = 4;
			keepalive_listener(listener, NULL);
			skinny_send_reply(listener, message);
			break;
		/* TODO */
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"Unknown request type: %d.\n", request->type);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_send_reply(listener_t *listener, skinny_message_t *reply)
{
	char *ptr;
	switch_size_t len;
	switch_assert(reply != NULL);
	len = reply->length+8;
	ptr = (char *) reply;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Sending reply (type=%x,length=%d).\n",
		reply->type, reply->length);
	switch_socket_send(listener->sock, ptr, &len);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_free_message(skinny_message_t *message)
{
	if(message) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Freeing message\n");
		/* TODO */
	}
	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************/
/* LISTENER FUNCTIONS */
/*****************************************************************************/

static void add_listener(listener_t *listener)
{
	switch_mutex_lock(globals.listener_mutex);
	listener->next = listen_list.listeners;
	listen_list.listeners = listener;
	switch_mutex_unlock(globals.listener_mutex);
}

static void remove_listener(listener_t *listener)
{
	listener_t *l, *last = NULL;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (l == listener) {
			if (last) {
				last->next = l->next;
			} else {
				listen_list.listeners = l->next;
			}
		}
		last = l;
	}
	switch_mutex_unlock(globals.listener_mutex);
}


static void walk_listeners(skinny_listener_callback_func_t callback, void *pvt)
{
	listener_t *l;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		callback(l, pvt);
	}
	switch_mutex_unlock(globals.listener_mutex);

}

static void flush_listener(listener_t *listener, switch_bool_t flush_log, switch_bool_t flush_events)
{

	/* TODO */
}

static listener_t *find_listener(char *device_name)
{
	listener_t *l, *r = NULL;
	skinny_device_t *device;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (l->device) {
			device = l->device;
			if(!strcasecmp(device->deviceName,device_name)) {
				if (switch_thread_rwlock_tryrdlock(l->rwlock) == SWITCH_STATUS_SUCCESS) {
					r = l;
				}
				break;
			}
		}
	}
	switch_mutex_unlock(globals.listener_mutex);
	return r;
}

static switch_status_t dump_listener(listener_t *listener, void *pvt)
{
	switch_stream_handle_t *stream = (switch_stream_handle_t *) pvt;
	const char *line = "=================================================================================================";
	skinny_device_t *device;
	if(listener->device) {
		device = listener->device;
		stream->write_function(stream, "%s\n", line);
		stream->write_function(stream, "DeviceName    \t%s\n", switch_str_nil(device->deviceName));
		stream->write_function(stream, "UserId        \t%d\n", device->userId);
		stream->write_function(stream, "Instance      \t%d\n", device->instance);
		stream->write_function(stream, "IP            \t%s\n", inet_ntoa(device->ip));
		stream->write_function(stream, "DeviceType    \t%d\n", device->deviceType);
		stream->write_function(stream, "MaxStreams    \t%d\n", device->maxStreams);
		stream->write_function(stream, "Port          \t%d\n", device->port);
		stream->write_function(stream, "Codecs        \t%s\n", device->codec_string);
		stream->write_function(stream, "%s\n", line);
	}
	return SWITCH_STATUS_SUCCESS;
}

static void close_socket(switch_socket_t **sock)
{
	switch_mutex_lock(listen_list.sock_mutex);
	if (*sock) {
		switch_socket_shutdown(*sock, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(*sock);
		*sock = NULL;
	}
	switch_mutex_unlock(listen_list.sock_mutex);
}

static switch_status_t kill_listener(listener_t *listener, void *pvt)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Killing listener.\n");
	switch_clear_flag(listener, LFLAG_RUNNING);
	close_socket(&listener->sock);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kill_expired_listener(listener_t *listener, void *pvt)
{
	if(listener->expire_time < switch_epoch_time_now(NULL)) {
		return kill_listener(listener, pvt);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t keepalive_listener(listener_t *listener, void *pvt)
{
	switch_assert(listener);
	
	listener->expire_time = switch_epoch_time_now(NULL)+globals.keep_alive*110/100;

	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj)
{
	listener_t *listener = (listener_t *) obj;
	switch_status_t status;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	skinny_message_t *request = NULL;
	skinny_message_t *reply = NULL;

	switch_mutex_lock(globals.listener_mutex);
	globals.listener_threads++;
	switch_mutex_unlock(globals.listener_mutex);
	
	switch_assert(listener != NULL);
	
	if ((session = listener->session)) {
		if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}
	}

	switch_socket_opt_set(listener->sock, SWITCH_SO_TCP_NODELAY, TRUE);
	switch_socket_opt_set(listener->sock, SWITCH_SO_NONBLOCK, TRUE);

	if (globals.debug > 0) {
		if (zstr(listener->remote_ip)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Connection Open\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Connection Open from %s:%d\n", listener->remote_ip, listener->remote_port);
		}
	}

	switch_socket_opt_set(listener->sock, SWITCH_SO_NONBLOCK, TRUE);
	switch_set_flag_locked(listener, LFLAG_RUNNING);
	keepalive_listener(listener, NULL);
	add_listener(listener);


	while (running && switch_test_flag(listener, LFLAG_RUNNING) && listen_list.ready) {
		status = skinny_read_packet(listener, &request);

		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Socket Error!\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			break;
		}

		if (!request) {
			continue;
		}

		if (skinny_handle_request(listener, request) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			break;
		}

		skinny_free_message(request);
	}

  done:
	
	skinny_free_message(request);
	skinny_free_message(reply);
	
	remove_listener(listener);

	if (globals.debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Session complete, waiting for children\n");
	}

	switch_thread_rwlock_wrlock(listener->rwlock);
	flush_listener(listener, SWITCH_TRUE, SWITCH_TRUE);

	if (listener->session) {
		channel = switch_core_session_get_channel(listener->session);
	}
	
	if (listener->sock) {
		close_socket(&listener->sock);
	}

	switch_thread_rwlock_unlock(listener->rwlock);

	if (globals.debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Connection Closed\n");
	}

	if (listener->session) {
		switch_channel_clear_flag(switch_core_session_get_channel(listener->session), CF_CONTROLLED);
		//TODO switch_clear_flag_locked(listener, LFLAG_SESSION);
		switch_core_session_rwunlock(listener->session);
	} else if (listener->pool) {
		switch_memory_pool_t *pool = listener->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(globals.listener_mutex);
	globals.listener_threads--;
	switch_mutex_unlock(globals.listener_mutex);

	return NULL;
}

/* Create a thread for the socket and launch it */
static void launch_listener_thread(listener_t *listener)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, listener->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, listener_run, listener, listener->pool);
}

int skinny_socket_create_and_bind()
{
	switch_status_t rv;
	switch_sockaddr_t *sa;
	switch_socket_t *inbound_socket = NULL;
	listener_t *listener;
	switch_memory_pool_t *pool = NULL, *listener_pool = NULL;
	uint32_t errs = 0;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	while(running) {
		rv = switch_sockaddr_info_get(&sa, globals.ip, SWITCH_INET, globals.port, 0, pool);
		if (rv)
			goto fail;
		rv = switch_socket_create(&listen_list.sock, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, pool);
		if (rv)
			goto sock_fail;
		rv = switch_socket_opt_set(listen_list.sock, SWITCH_SO_REUSEADDR, 1);
		if (rv)
			goto sock_fail;
		rv = switch_socket_bind(listen_list.sock, sa);
		if (rv)
			goto sock_fail;
		rv = switch_socket_listen(listen_list.sock, 5);
		if (rv)
			goto sock_fail;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket up listening on %s:%u\n", globals.ip, globals.port);

		break;
	  sock_fail:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error! Could not listen on %s:%u\n", globals.ip, globals.port);
		switch_yield(100000);
	}

	listen_list.ready = 1;

	while(running) {

		if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			goto fail;
		}

		if ((rv = switch_socket_accept(&inbound_socket, listen_list.sock, listener_pool))) {
			if (!running) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
				goto end;
			} else {
				/* I wish we could use strerror_r here but its not defined everywhere =/ */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error [%s]\n", strerror(errno));
				if (++errs > 100) {
					goto end;
				}
			}
		} else {
			errs = 0;
		}

		
		if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
			break;
		}

		switch_thread_rwlock_create(&listener->rwlock, listener_pool);

		listener->sock = inbound_socket;
		listener->pool = listener_pool;
		listener_pool = NULL;
		listener->device = NULL;

		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);

		switch_socket_addr_get(&listener->sa, SWITCH_TRUE, listener->sock);
		switch_get_addr(listener->remote_ip, sizeof(listener->remote_ip), listener->sa);
		listener->remote_port = switch_sockaddr_get_port(listener->sa);
		launch_listener_thread(listener);

	}

 end:

	close_socket(&listen_list.sock);
	
	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	if (listener_pool) {
		switch_core_destroy_memory_pool(&listener_pool);
	}


  fail:
	return SWITCH_STATUS_TERM;
}

/*****************************************************************************/
/* MODULE FUNCTIONS */
/*****************************************************************************/
static switch_status_t load_skinny_config(void)
{
	char *cf = "skinny.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "domain")) {
				set_global_domain(val);
			} else if (!strcmp(var, "ip")) {
				set_global_ip(val);
			} else if (!strcmp(var, "port")) {
				globals.port = atoi(val);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "codec-prefs")) {
				set_global_codec_string(val);
				globals.codec_order_last = switch_separate_string(globals.codec_string, ',', globals.codec_order, SWITCH_MAX_CODECS);
			} else if (!strcmp(var, "codec-master")) {
				if (!strcasecmp(val, "us")) {
					switch_set_flag(&globals, GFLAG_MY_CODEC_PREFS);
				}
			} else if (!strcmp(var, "codec-rates")) {
				set_global_codec_rates_string(val);
				globals.codec_rates_last = switch_separate_string(globals.codec_rates_string, ',', globals.codec_rates, SWITCH_MAX_CODECS);
			} else if (!strcmp(var, "keep-alive")) {
				globals.keep_alive = atoi(val);
			} else if (!strcmp(var, "date-format")) {
				memcpy(globals.date_format, val, 6);
			}
		}
	}
	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	if (!globals.port) {
		globals.port = 2000;
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t cmd_device(char **argv, int argc, switch_stream_handle_t *stream)
{
	listener_t *listener;
	if (argc != 1) {
		stream->write_function(stream, "Invalid Args!\n");
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (argv[0] && !strcasecmp(argv[0], "*")) {
		walk_listeners(dump_listener, stream);
	} else {
		listener=find_listener(argv[0]);
		if(listener) {
			dump_listener(listener, stream);
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(skinny_function)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	skinny_command_t func = NULL;
	const char *usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"skinny help\n"
		"skinny device *\n"
		"skinny device <device_name>\n"
		"--------------------------------------------------------------------------------\n";
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!(mycmd = strdup(cmd))) {
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!strcasecmp(argv[0], "device")) {
		func = cmd_device;
	} else if (!strcasecmp(argv[0], "help")) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}
	
	if (func) {
		status = func(&argv[1], argc - 1, stream);
	} else {
		stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
	}

  done:
	switch_safe_free(mycmd);
	return status;
}

static void event_handler(switch_event_t *event)
{
	if (event->event_id == SWITCH_EVENT_HEARTBEAT) {
		walk_listeners(kill_expired_listener, NULL);
	}
}

static switch_status_t skinny_list_devices(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	listener_t *l;
	skinny_device_t *device;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if(l->device) {
			device = l->device;
			switch_console_push_match(&my_matches, device->deviceName);
		}
	}
	switch_mutex_unlock(globals.listener_mutex);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}
	
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_skinny_load)
{
	switch_api_interface_t *api_interface;

	module_pool = pool;

	memset(&globals, 0, sizeof(globals));

	load_skinny_config();

	switch_mutex_init(&globals.listener_mutex, SWITCH_MUTEX_NESTED, module_pool);

	memset(&listen_list, 0, sizeof(listen_list));
	switch_mutex_init(&listen_list.sock_mutex, SWITCH_MUTEX_NESTED, module_pool);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_HEARTBEAT, NULL, event_handler, NULL, &globals.heartbeat_node) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind our heartbeat handler!\n");
		/* Not such severe to prevent loading */
	}

	if (switch_event_reserve_subclass(SKINNY_EVENT_REGISTER) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", SKINNY_EVENT_REGISTER);
		return SWITCH_STATUS_TERM;
	}


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	skinny_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	skinny_endpoint_interface->interface_name = "skinny";
	skinny_endpoint_interface->io_routines = &skinny_io_routines;
	skinny_endpoint_interface->state_handler = &skinny_state_handlers;


	SWITCH_ADD_API(api_interface, "skinny", "Skinny Controls", skinny_function, "<cmd> <args>");
	switch_console_set_complete("add skinny help");
	switch_console_set_complete("add skinny device *");
	switch_console_set_complete("add skinny device ::skinny::list_devices");

	switch_console_add_complete_func("::skinny::list_devices", skinny_list_devices);
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_skinny_runtime)
{
	return skinny_socket_create_and_bind();
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skinny_shutdown)
{
	int sanity = 0;

	switch_event_free_subclass(SKINNY_EVENT_REGISTER);
	switch_event_unbind(&globals.heartbeat_node);

	running = 0;

	walk_listeners(kill_listener, NULL);

	close_socket(&listen_list.sock);

	while (globals.listener_threads) {
		switch_yield(100000);
		walk_listeners(kill_listener, NULL);
		if (++sanity >= 200) {
			break;
		}
	}

	/* Free dynamically allocated strings */
	switch_safe_free(globals.domain);
	switch_safe_free(globals.ip);
	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.codec_string);
	switch_safe_free(globals.codec_rates_string);
	
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
