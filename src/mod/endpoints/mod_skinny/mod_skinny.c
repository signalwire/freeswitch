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
#define SKINNY_EVENT_UNREGISTER "skinny::unregister"
#define SKINNY_EVENT_EXPIRE "skinny::expire"
#define SKINNY_EVENT_ALARM "skinny::alarm"


switch_endpoint_interface_t *skinny_endpoint_interface;
static switch_memory_pool_t *module_pool = NULL;

struct skinny_profile {
	/* prefs */
	char *name;
	char *domain;
	char *ip;
	unsigned int port;
	char *dialplan;
	uint32_t keep_alive;
	char date_format[6];
	/* db */
	char *dbname;
	char *odbc_dsn;
	char *odbc_user;
	char *odbc_pass;
	switch_odbc_handle_t *master_odbc;
	/* stats */
	uint32_t ib_calls;
	uint32_t ob_calls;
	uint32_t ib_failed_calls;
	uint32_t ob_failed_calls;	
	/* listener */
	int listener_threads;
	switch_mutex_t *listener_mutex;	
	switch_socket_t *sock;
	switch_mutex_t *sock_mutex;
	struct listener *listeners;
	uint8_t listener_ready;
	/* sessions */
	switch_hash_t *session_hash;
	switch_mutex_t *sessions_mutex;
};
typedef struct skinny_profile skinny_profile_t;

struct skinny_globals {
	/* prefs */
	int debug;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	unsigned int flags;
	/* data */
	int calls;
	switch_mutex_t *calls_mutex;
	switch_hash_t *profile_hash;
	switch_event_node_t *heartbeat_node;
	int running;
};
typedef struct skinny_globals skinny_globals_t;

static skinny_globals_t globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_rates_string, globals.codec_rates_string);

/*****************************************************************************/
/* SQL TYPES */
/*****************************************************************************/
static char devices_sql[] =
	"CREATE TABLE skinny_devices (\n"
	"   name           VARCHAR(16),\n"
	"   user_id          INTEGER,\n"
	"   instance         INTEGER,\n"
	"   ip               VARCHAR(255),\n"
	"   type             INTEGER,\n"
	"   max_streams      INTEGER,\n"
	"   port             INTEGER,\n"
	"   codec_string     VARCHAR(255)\n"
	");\n";

static char buttons_sql[] =
	"CREATE TABLE skinny_buttons (\n"
	"   device_name      VARCHAR(16),\n"
	"   position         INTEGER,\n"
	"   type             VARCHAR(10),\n"
	"   label            VARCHAR(40),\n"
	"   value            VARCHAR(24),\n"
	"   settings         VARCHAR(44)\n"
	");\n";

/*****************************************************************************/
/* CHANNEL TYPES */
/*****************************************************************************/

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
	char *dest;
	/* identification */
	skinny_profile_t *profile;
	uint32_t call_id;
};

typedef struct private_object private_t;

/*****************************************************************************/
/* SKINNY MESSAGE TYPES */
/*****************************************************************************/

/* KeepAliveMessage */
#define KEEP_ALIVE_MESSAGE 0x0000

/* RegisterMessage */
#define REGISTER_MESSAGE 0x0001
struct register_message {
	char device_name[16];
	uint32_t user_id;
	uint32_t instance;
	struct in_addr ip;
	uint32_t device_type;
	uint32_t max_streams;
};

/* PortMessage */
#define PORT_MESSAGE 0x0002

/* KeypadButtonMessage */
#define KEYPAD_BUTTON_MESSAGE 0x0003
struct keypad_button_message {
	uint32_t button;
	uint32_t line_instance;
	uint32_t call_id;
};

/* StimulusMessage */
#define STIMULUS_MESSAGE 0x0005
struct stimulus_message {
	uint32_t instance_type; /* See enum skinny_button_definition */
	uint32_t instance;
	uint32_t call_reference;
};

/* OffHookMessage */
#define OFF_HOOK_MESSAGE 0x0006
struct off_hook_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* OnHookMessage */
#define ON_HOOK_MESSAGE 0x0007
struct on_hook_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* SpeedDialStatReqMessage */
#define SPEED_DIAL_STAT_REQ_MESSAGE 0x000A
struct speed_dial_stat_req_message {
	uint32_t number;
};

/* LineStatReqMessage */
#define LINE_STAT_REQ_MESSAGE 0x000B
struct line_stat_req_message {
	uint32_t number;
};

/* ConfigStatReqMessage */
#define CONFIG_STAT_REQ_MESSAGE 0x000C

/* TimeDateReqMessage */
#define TIME_DATE_REQ_MESSAGE 0x000D

/* ButtonTemplateReqMessage */
#define BUTTON_TEMPLATE_REQ_MESSAGE 0x000E

/* CapabilitiesResMessage */
#define CAPABILITIES_RES_MESSAGE 0x0010
struct station_capabilities {
	uint32_t codec;
	uint16_t frames;
	char reserved[10];
};

struct capabilities_res_message {
	uint32_t count;
	struct station_capabilities caps[SWITCH_MAX_CODECS];
};

/* AlarmMessage */
#define ALARM_MESSAGE 0x0020
struct alarm_message {
	uint32_t alarm_severity;
	char display_message[80];
	uint32_t alarm_param1;
	uint32_t alarm_param2;
};

/* OpenReceiveChannelAck */
#define OPEN_RECEIVE_CHANNEL_ACK_MESSAGE 0x0022
struct open_receive_channel_ack_message {
	uint32_t status;
	struct in_addr ip;
	uint32_t port;
	uint32_t pass_thru_party_id;
};

/* SoftKeySetReqMessage */
#define SOFT_KEY_SET_REQ_MESSAGE 0x0025

/* SoftKeyEventMessage */
#define SOFT_KEY_EVENT_MESSAGE 0x0026
struct soft_key_event_message {
	uint32_t soft_key_event;
	uint32_t line_instance;
	uint32_t callreference;
};

/* UnregisterMessage */
#define UNREGISTER_MESSAGE 0x0027

/* SoftKeyTemplateReqMessage */
#define SOFT_KEY_TEMPLATE_REQ_MESSAGE 0x0028

/* HeadsetStatusMessage */
#define HEADSET_STATUS_MESSAGE 0x002B
struct headset_status_message {
	uint32_t mode;
};

/* RegisterAvailableLinesMessage */
#define REGISTER_AVAILABLE_LINES_MESSAGE 0x002D
struct register_available_lines_message {
	uint32_t count;
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

/* StartToneMessage */
#define START_TONE_MESSAGE 0x0082
struct start_tone_message {
	uint32_t tone; /* see enum skinny_tone */
	uint32_t reserved;
	uint32_t line_instance;
	uint32_t call_id;
};

enum skinny_tone {
	SKINNY_TONE_SILENCE = 0x00,
	SKINNY_TONE_DIALTONE = 0x21,
	SKINNY_TONE_BUSYTONE = 0x23,
	SKINNY_TONE_ALERT = 0x24,
	SKINNY_TONE_REORDER = 0x25,
	SKINNY_TONE_CALLWAITTONE = 0x2D,
	SKINNY_TONE_NOTONE = 0x7F,
};

/* StopToneMessage */
#define STOP_TONE_MESSAGE 0x0083
struct stop_tone_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* SetRingerMessage */
#define SET_RINGER_MESSAGE 0x0085
struct set_ringer_message {
	uint32_t ring_type; /* See enum skinny_ring_type */
	uint32_t ring_mode; /* See enum skinny_ring_mode */
	uint32_t unknown; /* ?? */
};

enum skinny_ring_type {
	SKINNY_RING_OFF = 1,
	SKINNY_RING_INSIDE = 2,
	SKINNY_RING_OUTSIDE = 3,
	SKINNY_RING_FEATURE = 4
};

enum skinny_ring_mode {
	SKINNY_RING_FOREVER = 1,
	SKINNY_RING_ONCE = 2,
};

/* SetLampMessage */
#define SET_LAMP_MESSAGE 0x0086
struct set_lamp_message {
	uint32_t stimulus;
	uint32_t stimulus_instance;
	uint32_t mode; /* See enum skinny_lamp_mode */
};

enum skinny_lamp_mode {
	SKINNY_LAMP_OFF = 1,
	SKINNY_LAMP_ON = 2,
	SKINNY_LAMP_WINK = 3,
	SKINNY_LAMP_FLASH = 4,
	SKINNY_LAMP_BLINK = 5,
};

/* SetSpeakerModeMessage */
#define SET_SPEAKER_MODE_MESSAGE 0x0088
struct set_speaker_mode_message {
	uint32_t mode; /* See enum skinny_speaker_mode */
};

enum skinny_speaker_mode {
	SKINNY_SPEAKER_ON = 1,
	SKINNY_SPEAKER_OFF = 2,
};

/* StartMediaTransmissionMessage */
#define START_MEDIA_TRANSMISSION_MESSAGE 0x008A
struct start_media_transmission_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t remote_ip;
	uint32_t remote_port;
	uint32_t ms_per_packet;
	uint32_t payload_capacity;
	uint32_t precedence;
	uint32_t silence_suppression;
	uint16_t max_frames_per_packet;
	uint32_t g723_bitrate;
	/* ... */
};

/* StopMediaTransmissionMessage */
#define STOP_MEDIA_TRANSMISSION_MESSAGE 0x008B
struct stop_media_transmission_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t conference_id2;
	/* ... */
};

/* CallInfoMessage */
#define CALL_INFO_MESSAGE 0x008F
struct call_info_message {
	char calling_party_name[40];
	char calling_party[24];
	char called_party_name[40];
	char called_party[24];
	uint32_t line_instance;
	uint32_t call_id;
	uint32_t call_type; /* See enum skinny_call_type */
	char original_called_party_name[40];
	char original_called_party[24];
	char last_redirecting_party_name[40];
	char last_redirecting_party[24];
	uint32_t original_called_party_redirect_reason;
	uint32_t last_redirecting_reason;
	char calling_party_voice_mailbox[24];
	char called_party_voice_mailbox[24];
	char original_called_party_voice_mailbox[24];
	char last_redirecting_voice_mailbox[24];
	uint32_t call_instance;
	uint32_t call_security_status;
	uint32_t party_pi_restriction_bits;
};

enum skinny_call_type {
	SKINNY_INBOUND_CALL = 1,
	SKINNY_OUTBOUND_CALL = 2,
	SKINNY_FORWARD_CALL = 3,
};

/* SpeedDialStatMessage */
#define SPEED_DIAL_STAT_RES_MESSAGE 0x0091
struct speed_dial_stat_res_message {
	uint32_t number;
	char line[24];
	char label[40];
};

/* LineStatMessage */
#define LINE_STAT_RES_MESSAGE 0x0092
struct line_stat_res_message {
	uint32_t number;
	char name[24];
	char shortname[40];
	char displayname[44];
};

/* ConfigStatMessage */
#define CONFIG_STAT_RES_MESSAGE 0x0093
struct config_stat_res_message {
	char device_name[16];
	uint32_t user_id;
	uint32_t instance;
	char user_name[40];
	char server_name[40];
	uint32_t number_lines;
	uint32_t number_speed_dials;
};

/* DefineTimeDate */
#define DEFINE_TIME_DATE_MESSAGE 0x0094
struct define_time_date_message {
	uint32_t year;
	uint32_t month;
	uint32_t day_of_week; /* monday = 1 */
	uint32_t day;
	uint32_t hour;
	uint32_t minute;
	uint32_t seconds;
	uint32_t milliseconds;
	uint32_t timestamp;
};

/* ButtonTemplateMessage */
#define BUTTON_TEMPLATE_RES_MESSAGE 0x0097
struct button_definition {
	uint8_t instance_number;
	uint8_t button_definition; /* See enum skinny_button_definition */
};

enum skinny_button_definition {
	SKINNY_BUTTON_SPEED_DIAL = 0x02,
	SKINNY_BUTTON_LINE = 0x09,
	SKINNY_BUTTON_VOICEMAIL = 0x0F,
	SKINNY_BUTTON_UNDEFINED = 0xFF,
};

struct button_template_message {
	uint32_t button_offset;
	uint32_t button_count;
	uint32_t total_button_count;
	struct button_definition btn[42];
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

/* OpenReceiveChannelMessage */
#define OPEN_RECEIVE_CHANNEL_MESSAGE 0x0105
struct open_receive_channel_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t packets;
	uint32_t payload_capacity;
	uint32_t echo_cancel_type;
	uint32_t g723_bitrate;
	uint32_t conference_id2;
	uint32_t reserved[10];
};

/* CloseReceiveChannelMessage */
#define CLOSE_RECEIVE_CHANNEL_MESSAGE 0x0106
struct close_receive_channel_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t conference_id2;
};

/* SoftKeyTemplateResMessage */
#define SOFT_KEY_TEMPLATE_RES_MESSAGE 0x0108

struct soft_key_template_definition {
	char soft_key_label[16];
	uint32_t soft_key_event;
};

struct soft_key_template_res_message {
	uint32_t soft_key_offset;
	uint32_t soft_key_count;
	uint32_t total_soft_key_count;
	struct soft_key_template_definition soft_key[32];
};

/* SoftKeySetResMessage */
#define SOFT_KEY_SET_RES_MESSAGE 0x0109
struct soft_key_set_definition {
	uint8_t soft_key_template_index[16];
	uint16_t soft_key_info_index[16];
};

struct soft_key_set_res_message {
	uint32_t soft_key_set_offset;
	uint32_t soft_key_set_count;
	uint32_t total_soft_key_set_count;
	struct soft_key_set_definition soft_key_set[16];
	uint32_t res;
};

/* SelectSoftKeysMessage */
#define SELECT_SOFT_KEYS_MESSAGE 0x0110
struct select_soft_keys_message {
	uint32_t line_instance;
	uint32_t call_id;
	uint32_t soft_key_set; /* See enum skinny_key_set */
	uint32_t valid_key_mask;
};

enum skinny_key_set {
	SKINNY_KEY_SET_ON_HOOK = 0,
	SKINNY_KEY_SET_CONNECTED = 1,
	SKINNY_KEY_SET_ON_HOLD = 2,
	SKINNY_KEY_SET_RING_IN = 3,
	SKINNY_KEY_SET_OFF_HOOK = 4,
	SKINNY_KEY_SET_CONNECTED_WITH_TRANSFER = 5,
	SKINNY_KEY_SET_DIGITS_AFTER_DIALING_FIRST_DIGIT = 6,
	SKINNY_KEY_SET_CONNECTED_WITH_CONFERENCE = 7,
	SKINNY_KEY_SET_RING_OUT = 8,
	SKINNY_KEY_SET_OFF_HOOK_WITH_FEATURES = 9,
};

/* CallStateMessage */
#define CALL_STATE_MESSAGE 0x0111
struct call_state_message {
	uint32_t call_state; /* See enum skinny_call_state */
	uint32_t line_instance;
	uint32_t call_id;
};

enum skinny_call_state {
	SKINNY_OFF_HOOK = 1,
	SKINNY_ON_HOOK = 2,
	SKINNY_RING_OUT = 3,
	SKINNY_RING_IN = 4,
	SKINNY_CONNECTED = 5,
	SKINNY_BUSY = 6,
	SKINNY_CONGESTION = 7,
	SKINNY_HOLD = 8,
	SKINNY_CALL_WAITING = 9,
	SKINNY_CALL_TRANSFER = 10,
	SKINNY_CALL_PARK = 11,
	SKINNY_PROCEED = 12,
	SKINNY_CALL_REMOTE_MULTILINE = 13,
	SKINNY_INVALID_NUMBER = 14
};

/* DisplayPromptStatusMessage */
#define DISPLAY_PROMPT_STATUS_MESSAGE 0x0112
struct display_prompt_status_message {
	uint32_t timeout;
	char display[32];
	uint32_t line_instance;
	uint32_t call_id;
};

/* ClearPromptStatusMessage */
#define CLEAR_PROMPT_STATUS_MESSAGE  0x0113
struct clear_prompt_status_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* ActivateCallPlaneMessage */
#define ACTIVATE_CALL_PLANE_MESSAGE 0x0116
struct activate_call_plane_message {
	uint32_t line_instance;
};

/* DialedNumberMessage */
#define DIALED_NUMBER_MESSAGE 0x011D
struct dialed_number_message {
	char called_party[24];
	uint32_t line_instance;
	uint32_t call_id;
};

/* Message */
#define SKINNY_MESSAGE_FIELD_SIZE 4 /* 4-bytes field */
#define SKINNY_MESSAGE_HEADERSIZE 12 /* three 4-bytes fields */
#define SKINNY_MESSAGE_MAXSIZE 1000

union skinny_data {
	struct register_message reg;
	struct keypad_button_message keypad_button;
	struct stimulus_message stimulus;
	struct off_hook_message off_hook;
	struct on_hook_message on_hook;
	struct speed_dial_stat_req_message speed_dial_req;
	struct line_stat_req_message line_req;
	struct capabilities_res_message cap_res;
	struct alarm_message alarm;
	struct open_receive_channel_ack_message open_receive_channel_ack;
	struct soft_key_event_message soft_key_event;
	struct headset_status_message headset_status;
	struct register_available_lines_message reg_lines;
	struct register_ack_message reg_ack;
	struct start_tone_message start_tone;
	struct stop_tone_message stop_tone;
	struct set_ringer_message ringer;
	struct set_lamp_message lamp;
	struct set_speaker_mode_message speaker_mode;
	struct start_media_transmission_message start_media;
	struct stop_media_transmission_message stop_media;
	struct call_info_message call_info;
	struct speed_dial_stat_res_message speed_dial_res;
	struct line_stat_res_message line_res;
	struct config_stat_res_message config_res;
	struct define_time_date_message define_time_date;
	struct button_template_message button_template;
	struct register_rej_message reg_rej;
	struct open_receive_channel_message open_receive_channel;
	struct close_receive_channel_message close_receive_channel;
	struct soft_key_template_res_message soft_key_template;
	struct soft_key_set_res_message soft_key_set;
	struct select_soft_keys_message select_soft_keys;
	struct call_state_message call_state;
	struct display_prompt_status_message display_prompt_status;
	struct clear_prompt_status_message clear_prompt_status;
	struct activate_call_plane_message activate_call_plane;
	struct dialed_number_message dialed_number;
	
	uint16_t as_uint16;
	char as_char;
	void *raw;
};

/*
 * header is length+reserved
 * body is type+data
 */
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
	skinny_profile_t *profile;
	char device_name[16];

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
#define skinny_check_data_length(message, len) \
	if (message->length < len+4) {\
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received Too Short Skinny Message (Expected %d, got %d).\n", len+4, message->length);\
		return SWITCH_STATUS_FALSE;\
	}

static switch_status_t start_tone(listener_t *listener,
	uint32_t tone,
	uint32_t reserved,
	uint32_t line_instance,
	uint32_t call_id);
static switch_status_t stop_tone(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id);
static switch_status_t set_ringer(listener_t *listener,
	uint32_t ring_type,
	uint32_t ring_mode,
	uint32_t unknown);
static switch_status_t set_lamp(listener_t *listener,
	uint32_t stimulus,
	uint32_t stimulus_instance,
	uint32_t mode);
static switch_status_t set_speaker_mode(listener_t *listener,
	uint32_t mode);
static switch_status_t start_media_transmission(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t remote_ip,
	uint32_t remote_port,
	uint32_t ms_per_packet,
	uint32_t payload_capacity,
	uint32_t precedence,
	uint32_t silence_suppression,
	uint16_t max_frames_per_packet,
	uint32_t g723_bitrate);
static switch_status_t stop_media_transmission(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t conference_id2);
static switch_status_t send_call_info(listener_t *listener,
	char calling_party_name[40],
	char calling_party[24],
	char called_party_name[40],
	char called_party[24],
	uint32_t line_instance,
	uint32_t call_id,
	uint32_t call_type,
	char original_called_party_name[40],
	char original_called_party[24],
	char last_redirecting_party_name[40],
	char last_redirecting_party[24],
	uint32_t original_called_party_redirect_reason,
	uint32_t last_redirecting_reason,
	char calling_party_voice_mailbox[24],
	char called_party_voice_mailbox[24],
	char original_called_party_voice_mailbox[24],
	char last_redirecting_voice_mailbox[24],
	uint32_t call_instance,
	uint32_t call_security_status,
	uint32_t party_pi_restriction_bits);
static switch_status_t open_receive_channel(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t packets,
	uint32_t payload_capacity,
	uint32_t echo_cancel_type,
	uint32_t g723_bitrate,
	uint32_t conference_id2,
	uint32_t reserved[10]);
static switch_status_t close_receive_channel(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t conference_id2);
static switch_status_t send_select_soft_keys(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id,
	uint32_t soft_key_set,
	uint32_t valid_key_mask);
static switch_status_t send_call_state(listener_t *listener,
	uint32_t call_state,
	uint32_t line_instance,
	uint32_t call_id);
static switch_status_t display_prompt_status(listener_t *listener,
	uint32_t timeout,
	char display[32],
	uint32_t line_instance,
	uint32_t call_id);
static switch_status_t clear_prompt_status(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id);
static switch_status_t activate_call_plane(listener_t *listener,
	uint32_t line_instance);
static switch_status_t send_dialed_number(listener_t *listener,
	char called_party[24],
	uint32_t line_instance,
	uint32_t call_id);

static switch_status_t skinny_send_reply(listener_t *listener, skinny_message_t *reply);

/* LISTENER FUNCTIONS */
static switch_status_t keepalive_listener(listener_t *listener, void *pvt);

/*****************************************************************************/
/* PROFILES FUNCTIONS */
/*****************************************************************************/
static switch_status_t dump_profile(const skinny_profile_t *profile, switch_stream_handle_t *stream)
{
	const char *line = "=================================================================================================";
	switch_assert(profile);
	stream->write_function(stream, "%s\n", line);
	/* prefs */
	stream->write_function(stream, "Name             \t%s\n", profile->name);
	stream->write_function(stream, "Domain Name      \t%s\n", profile->domain);
	stream->write_function(stream, "IP               \t%s\n", profile->ip);
	stream->write_function(stream, "Port             \t%d\n", profile->port);
	stream->write_function(stream, "Dialplan         \t%s\n", profile->dialplan);
	stream->write_function(stream, "Keep-Alive       \t%d\n", profile->keep_alive);
	stream->write_function(stream, "Date-Format      \t%s\n", profile->date_format);
	/* db */
	stream->write_function(stream, "DBName           \t%s\n", profile->dbname ? profile->dbname : switch_str_nil(profile->odbc_dsn));
	/* stats */
	stream->write_function(stream, "CALLS-IN         \t%d\n", profile->ib_calls);
	stream->write_function(stream, "FAILED-CALLS-IN  \t%d\n", profile->ib_failed_calls);
	stream->write_function(stream, "CALLS-OUT        \t%d\n", profile->ob_calls);
	stream->write_function(stream, "FAILED-CALLS-OUT \t%d\n", profile->ob_failed_calls);
	/* listener */
	stream->write_function(stream, "Listener-Threads \t%d\n", profile->listener_threads);
	stream->write_function(stream, "%s\n", line);

	return SWITCH_STATUS_SUCCESS;
}


static skinny_profile_t *skinny_find_profile(const char *profile_name)
{
	return (skinny_profile_t *) switch_core_hash_find(globals.profile_hash, profile_name);
}

/*****************************************************************************/
/* SQL FUNCTIONS */
/*****************************************************************************/
static void skinny_execute_sql(skinny_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_core_db_t *db;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (switch_odbc_available() && profile->odbc_dsn) {
		switch_odbc_statement_handle_t stmt;
		if (switch_odbc_handle_exec(profile->master_odbc, sql, &stmt, NULL) != SWITCH_ODBC_SUCCESS) {
			char *err_str;
			err_str = switch_odbc_handle_get_error(profile->master_odbc, stmt);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
			switch_safe_free(err_str);
		}
		switch_odbc_statement_handle_free(&stmt);
	} else {
		if (!(db = switch_core_db_open_file(profile->dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SQL: %s\n", sql);
		switch_core_db_persistant_execute(db, sql, 1);
		switch_core_db_close(db);
	}

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}
}


static switch_bool_t skinny_execute_sql_callback(skinny_profile_t *profile,
											  switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	switch_core_db_t *db;
	char *errmsg = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (switch_odbc_available() && profile->odbc_dsn) {
		switch_odbc_handle_callback_exec(profile->master_odbc, sql, callback, pdata, NULL);
	} else {
		if (!(db = switch_core_db_open_file(profile->dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SQL: %s\n", sql);
		switch_core_db_exec(db, sql, callback, pdata, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
			free(errmsg);
		}

		if (db) {
			switch_core_db_close(db);
		}
	}

  end:

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}

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
	switch_mutex_lock(globals.calls_mutex);
	globals.calls++;
	switch_mutex_unlock(globals.calls_mutex);

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
	switch_mutex_lock(globals.calls_mutex);
	globals.calls--;
	if (globals.calls < 0) {
		globals.calls = 0;
	}
	switch_mutex_unlock(globals.calls_mutex);

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
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	switch_core_session_t *nsession = NULL;
	private_t *tech_pvt;
	
	char *profile_name, *dest;
	skinny_profile_t *profile = NULL;
	char name[128];
	switch_channel_t *channel;
	switch_caller_profile_t *caller_profile;

	if (!outbound_profile || zstr(outbound_profile->destination_number)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Invalid Destination\n");
		goto error;
	}

	if (!(nsession = switch_core_session_request(skinny_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, pool))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	if (!(tech_pvt = (struct private_object *) switch_core_session_alloc(nsession, sizeof(*tech_pvt)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error Creating Session private object\n");
		goto error;
	}

	tech_init(tech_pvt, nsession);

	if(!(profile_name = switch_core_session_strdup(nsession, outbound_profile->destination_number))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error Creating Session Info\n");
		goto error;
	}

	if (!(dest = strchr(profile_name, '/'))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Skinny URL. Should be skinny/<profile>/<number>.\n");
		cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
		goto error;
	}
	*dest++ = '\0';

	profile = skinny_find_profile(profile_name);
	if (!(profile = skinny_find_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Profile %s\n", profile_name);
		cause = SWITCH_CAUSE_UNALLOCATED_NUMBER;
		goto error;
	}
	
	tech_pvt->profile = profile;
	tech_pvt->dest = switch_core_session_strdup(nsession, dest);
	snprintf(name, sizeof(name), "SKINNY/%s/%s", profile->name, dest);

	channel = switch_core_session_get_channel(nsession);
	switch_channel_set_name(channel, name);
	

	caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
	switch_channel_set_caller_profile(channel, caller_profile);
	tech_pvt->caller_profile = caller_profile;

	switch_channel_set_flag(channel, CF_OUTBOUND);
	switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	switch_channel_set_state(channel, CS_INIT);

	cause = SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	
	if (!(cause == SWITCH_CAUSE_SUCCESS)) {
		goto error;
	}
	
	*new_session = nsession;
	cause = SWITCH_CAUSE_SUCCESS;
	goto done;

  error:
	if (nsession) {
		switch_core_session_destroy(&nsession);
	}
	*pool = NULL;


  done:

	if (profile) {
		if (cause == SWITCH_CAUSE_SUCCESS) {
			profile->ob_calls++;
		} else {
			profile->ob_failed_calls++;
		}
	}
	return cause;
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
	
	if (!globals.running) {
		return SWITCH_STATUS_FALSE;
	}

	ptr = mbuf;

	while (listener->sock && globals.running) {
		uint8_t do_sleep = 1;
		if(bytes < SKINNY_MESSAGE_FIELD_SIZE) {
			/* We have nothing yet, get length header field */
			mlen = SKINNY_MESSAGE_FIELD_SIZE - bytes;
		} else {
			/* We now know the message size */
			mlen = request->length + 2*SKINNY_MESSAGE_FIELD_SIZE - bytes;
		}

		status = switch_socket_recv(listener->sock, ptr, &mlen);
		
		if (!globals.running || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Socket break.\n");
			return SWITCH_STATUS_FALSE;
		}
		
		if(mlen) {
			bytes += mlen;
			
			if(bytes >= SKINNY_MESSAGE_FIELD_SIZE) {
				do_sleep = 0;
				ptr += mlen;
				memcpy(request, mbuf, bytes);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
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

static int skinny_device_event_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_event_t *event = (switch_event_t *) pArg;

	char *device_name = argv[0];
	char *user_id = argv[1];
	char *instance = argv[2];
	char *ip = argv[3];
	char *device_type = argv[4];
	char *max_streams = argv[5];
	char *port = argv[6];
	char *codec_string = argv[7];

	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-Device-Name", device_name);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-User-Id", "%s", user_id);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Instance", "%s", instance);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-IP", ip);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Device-Type", "%s", device_type);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Max-Streams", "%s", max_streams);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Port", "%s", port);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-Codecs", codec_string);

	return 0;
}

static switch_status_t skinny_device_event(listener_t *listener, switch_event_t **ev, switch_event_types_t event_id, const char *subclass_name)
{
	switch_event_t *event = NULL;
	char *sql;
	skinny_profile_t *profile;
	assert(listener->profile);
	profile = listener->profile;

	switch_event_create_subclass(&event, event_id, subclass_name);
	switch_assert(event);
	if ((sql = switch_mprintf("SELECT * FROM skinny_devices WHERE name='%s'", listener->device_name))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_device_event_callback, event);
		switch_safe_free(sql);
	}

	*ev = event;
	return SWITCH_STATUS_SUCCESS;
}

/* Message helpers */
static switch_status_t start_tone(listener_t *listener,
	uint32_t tone,
	uint32_t reserved,
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.start_tone));
	message->type = START_TONE_MESSAGE;
	message->length = 4 + sizeof(message->data.start_tone);
	message->data.start_tone.tone = tone;
	message->data.start_tone.reserved = reserved;
	message->data.start_tone.line_instance = line_instance;
	message->data.start_tone.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t stop_tone(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.stop_tone));
	message->type = STOP_TONE_MESSAGE;
	message->length = 4 + sizeof(message->data.stop_tone);
	message->data.stop_tone.line_instance = line_instance;
	message->data.stop_tone.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t set_ringer(listener_t *listener,
	uint32_t ring_type,
	uint32_t ring_mode,
	uint32_t unknown)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.ringer));
	message->type = SET_RINGER_MESSAGE;
	message->length = 4 + sizeof(message->data.ringer);
	message->data.ringer.ring_type = ring_type;
	message->data.ringer.ring_mode = ring_mode;
	message->data.ringer.unknown = unknown;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t set_lamp(listener_t *listener,
	uint32_t stimulus,
	uint32_t stimulus_instance,
	uint32_t mode)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.lamp));
	message->type = SET_LAMP_MESSAGE;
	message->length = 4 + sizeof(message->data.lamp);
	message->data.lamp.stimulus = stimulus;
	message->data.lamp.stimulus_instance = stimulus_instance;
	message->data.lamp.mode = mode;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t set_speaker_mode(listener_t *listener,
	uint32_t mode)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.speaker_mode));
	message->type = SET_SPEAKER_MODE_MESSAGE;
	message->length = 4 + sizeof(message->data.speaker_mode);
	message->data.speaker_mode.mode = mode;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t start_media_transmission(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t remote_ip,
	uint32_t remote_port,
	uint32_t ms_per_packet,
	uint32_t payload_capacity,
	uint32_t precedence,
	uint32_t silence_suppression,
	uint16_t max_frames_per_packet,
	uint32_t g723_bitrate)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.start_media));
	message->type = START_MEDIA_TRANSMISSION_MESSAGE;
	message->length = 4 + sizeof(message->data.start_media);
	message->data.start_media.conference_id = conference_id;
	message->data.start_media.pass_thru_party_id = pass_thru_party_id;
	message->data.start_media.remote_ip = remote_ip;
	message->data.start_media.remote_port = remote_port;
	message->data.start_media.ms_per_packet = ms_per_packet;
	message->data.start_media.payload_capacity = payload_capacity;
	message->data.start_media.precedence = precedence;
	message->data.start_media.silence_suppression = silence_suppression;
	message->data.start_media.max_frames_per_packet = max_frames_per_packet;
	message->data.start_media.g723_bitrate = g723_bitrate;
	/* ... */
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t stop_media_transmission(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t conference_id2)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.stop_media));
	message->type = STOP_MEDIA_TRANSMISSION_MESSAGE;
	message->length = 4 + sizeof(message->data.stop_media);
	message->data.stop_media.conference_id = conference_id;
	message->data.stop_media.pass_thru_party_id = pass_thru_party_id;
	message->data.stop_media.conference_id2 = conference_id2;
	/* ... */
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t send_call_info(listener_t *listener,
	char calling_party_name[40],
	char calling_party[24],
	char called_party_name[40],
	char called_party[24],
	uint32_t line_instance,
	uint32_t call_id,
	uint32_t call_type,
	char original_called_party_name[40],
	char original_called_party[24],
	char last_redirecting_party_name[40],
	char last_redirecting_party[24],
	uint32_t original_called_party_redirect_reason,
	uint32_t last_redirecting_reason,
	char calling_party_voice_mailbox[24],
	char called_party_voice_mailbox[24],
	char original_called_party_voice_mailbox[24],
	char last_redirecting_voice_mailbox[24],
	uint32_t call_instance,
	uint32_t call_security_status,
	uint32_t party_pi_restriction_bits)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.call_info));
	message->type = CALL_INFO_MESSAGE;
	message->length = 4 + sizeof(message->data.call_info);
	strcpy(message->data.call_info.calling_party_name, calling_party_name);
	strcpy(message->data.call_info.calling_party, calling_party);
	strcpy(message->data.call_info.called_party_name, called_party_name);
	strcpy(message->data.call_info.called_party, called_party);
	message->data.call_info.line_instance = line_instance;
	message->data.call_info.call_id = call_id;
	message->data.call_info.call_type = call_type;
	strcpy(message->data.call_info.original_called_party_name, original_called_party_name);
	strcpy(message->data.call_info.original_called_party, original_called_party);
	strcpy(message->data.call_info.last_redirecting_party_name, last_redirecting_party_name);
	strcpy(message->data.call_info.last_redirecting_party, last_redirecting_party);
	message->data.call_info.original_called_party_redirect_reason = original_called_party_redirect_reason;
	message->data.call_info.last_redirecting_reason = last_redirecting_reason;
	strcpy(message->data.call_info.calling_party_voice_mailbox, calling_party_voice_mailbox);
	strcpy(message->data.call_info.called_party_voice_mailbox, called_party_voice_mailbox);
	strcpy(message->data.call_info.original_called_party_voice_mailbox, original_called_party_voice_mailbox);
	strcpy(message->data.call_info.last_redirecting_voice_mailbox, last_redirecting_voice_mailbox);
	message->data.call_info.call_instance = call_instance;
	message->data.call_info.call_security_status = call_security_status;
	message->data.call_info.party_pi_restriction_bits = party_pi_restriction_bits;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t open_receive_channel(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t packets,
	uint32_t payload_capacity,
	uint32_t echo_cancel_type,
	uint32_t g723_bitrate,
	uint32_t conference_id2,
	uint32_t reserved[10])
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.open_receive_channel));
	message->type = OPEN_RECEIVE_CHANNEL_MESSAGE;
	message->length = 4 + sizeof(message->data.open_receive_channel);
	message->data.open_receive_channel.conference_id = conference_id;
	message->data.open_receive_channel.pass_thru_party_id = pass_thru_party_id;
	message->data.open_receive_channel.packets = packets;
	message->data.open_receive_channel.payload_capacity = payload_capacity;
	message->data.open_receive_channel.echo_cancel_type = echo_cancel_type;
	message->data.open_receive_channel.g723_bitrate = g723_bitrate;
	message->data.open_receive_channel.conference_id2 = conference_id2;
	/*
	message->data.open_receive_channel.reserved[0] = reserved[0];
	message->data.open_receive_channel.reserved[1] = reserved[1];
	message->data.open_receive_channel.reserved[2] = reserved[2];
	message->data.open_receive_channel.reserved[3] = reserved[3];
	message->data.open_receive_channel.reserved[4] = reserved[4];
	message->data.open_receive_channel.reserved[5] = reserved[5];
	message->data.open_receive_channel.reserved[6] = reserved[6];
	message->data.open_receive_channel.reserved[7] = reserved[7];
	message->data.open_receive_channel.reserved[8] = reserved[8];
	message->data.open_receive_channel.reserved[9] = reserved[9];
	*/
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t close_receive_channel(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t conference_id2)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.close_receive_channel));
	message->type = CLOSE_RECEIVE_CHANNEL_MESSAGE;
	message->length = 4 + sizeof(message->data.close_receive_channel);
	message->data.close_receive_channel.conference_id = conference_id;
	message->data.close_receive_channel.pass_thru_party_id = pass_thru_party_id;
	message->data.close_receive_channel.conference_id2 = conference_id2;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t send_select_soft_keys(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id,
	uint32_t soft_key_set,
	uint32_t valid_key_mask)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.select_soft_keys));
	message->type = SELECT_SOFT_KEYS_MESSAGE;
	message->length = 4 + sizeof(message->data.select_soft_keys);
	message->data.select_soft_keys.line_instance = line_instance;
	message->data.select_soft_keys.call_id = call_id;
	message->data.select_soft_keys.soft_key_set = soft_key_set;
	message->data.select_soft_keys.valid_key_mask = valid_key_mask;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t send_call_state(listener_t *listener,
	uint32_t call_state,
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.call_state));
	message->type = CALL_STATE_MESSAGE;
	message->length = 4 + sizeof(message->data.call_state);
	message->data.call_state.call_state = call_state;
	message->data.call_state.line_instance = line_instance;
	message->data.call_state.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t display_prompt_status(listener_t *listener,
	uint32_t timeout,
	char display[32],
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.display_prompt_status));
	message->type = DISPLAY_PROMPT_STATUS_MESSAGE;
	message->length = 4 + sizeof(message->data.display_prompt_status);
	message->data.display_prompt_status.timeout = timeout;
	strcpy(message->data.display_prompt_status.display, display);
	message->data.display_prompt_status.line_instance = line_instance;
	message->data.display_prompt_status.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t clear_prompt_status(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.clear_prompt_status));
	message->type = CLEAR_PROMPT_STATUS_MESSAGE;
	message->length = 4 + sizeof(message->data.clear_prompt_status);
	message->data.clear_prompt_status.line_instance = line_instance;
	message->data.clear_prompt_status.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t activate_call_plane(listener_t *listener,
	uint32_t line_instance)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.activate_call_plane));
	message->type = ACTIVATE_CALL_PLANE_MESSAGE;
	message->length = 4 + sizeof(message->data.activate_call_plane);
	message->data.activate_call_plane.line_instance = line_instance;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t send_dialed_number(listener_t *listener,
	char called_party[24],
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.dialed_number));
	message->type = DIALED_NUMBER_MESSAGE;
	message->length = 4 + sizeof(message->data.dialed_number);
	strcpy(message->data.dialed_number.called_party, called_party);
	message->data.dialed_number.line_instance = line_instance;
	message->data.dialed_number.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

/* Message handling */
static switch_status_t skinny_handle_alarm(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;

	skinny_check_data_length(request, sizeof(request->data.alarm));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Received alarm: Severity=%d, DisplayMessage=%s, Param1=%d, Param2=%d.\n",
		request->data.alarm.alarm_severity, request->data.alarm.display_message,
		request->data.alarm.alarm_param1, request->data.alarm.alarm_param2);
	/* skinny::alarm event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_ALARM);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Severity", "%d", request->data.alarm.alarm_severity);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-DisplayMessage", "%s", request->data.alarm.display_message);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Param1", "%d", request->data.alarm.alarm_param1);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Param2", "%d", request->data.alarm.alarm_param2);
	switch_event_fire(&event);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_register(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	skinny_message_t *message;
	skinny_profile_t *profile;
	switch_event_t *event = NULL;
	switch_event_t *params = NULL;
	switch_xml_t xroot, xdomain, xgroup, xuser, xskinny, xbuttons, xbutton;
	char *sql;
	assert(listener->profile);
	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.reg));

	if(!zstr(listener->device_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"A device is already registred on this listener.\n");
		message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_rej));
		message->type = REGISTER_REJ_MESSAGE;
		message->length = 4 + sizeof(message->data.reg_rej);
		strcpy(message->data.reg_rej.error, "A device is already registred on this listener");
		skinny_send_reply(listener, message);
		return SWITCH_STATUS_FALSE;
	}

	/* Check directory */
	skinny_device_event(listener, &params, SWITCH_EVENT_REQUEST_PARAMS, SWITCH_EVENT_SUBCLASS_ANY);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "skinny-auth");

	if (switch_xml_locate_user("id", request->data.reg.device_name, profile->domain, "", &xroot, &xdomain, &xuser, &xgroup, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't find device [%s@%s]\n"
					  "You must define a domain called '%s' in your directory and add a user with id=\"%s\".\n"
					  , request->data.reg.device_name, profile->domain, profile->domain, request->data.reg.device_name);
		message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_rej));
		message->type = REGISTER_REJ_MESSAGE;
		message->length = 4 + sizeof(message->data.reg_rej);
		strcpy(message->data.reg_rej.error, "Device not found");
		skinny_send_reply(listener, message);
		status =  SWITCH_STATUS_FALSE;
		goto end;
	}

	if ((sql = switch_mprintf(
			"INSERT INTO skinny_devices "
				"(name, user_id, instance, ip, type, max_streams, codec_string) "
				"VALUES ('%s','%d','%d', '%s', '%d', '%d', '%s')",
			request->data.reg.device_name,
			request->data.reg.user_id,
			request->data.reg.instance,
			inet_ntoa(request->data.reg.ip),
			request->data.reg.device_type,
			request->data.reg.max_streams,
			"" /* codec_string */
			))) {
		skinny_execute_sql(profile, sql, profile->listener_mutex);
		switch_safe_free(sql);
	}


	strcpy(listener->device_name, request->data.reg.device_name);

	xskinny = switch_xml_child(xuser, "skinny");
	if (xskinny) {
		xbuttons = switch_xml_child(xskinny, "buttons");
		if (xbuttons) {
			for (xbutton = switch_xml_child(xbuttons, "button"); xbutton; xbutton = xbutton->next) {
				const char *position = switch_xml_attr_soft(xbutton, "position");
				const char *type = switch_xml_attr_soft(xbutton, "type");
				const char *label = switch_xml_attr_soft(xbutton, "label");
				const char *value = switch_xml_attr_soft(xbutton, "value");
				const char *settings = switch_xml_attr_soft(xbutton, "settings");
				if ((sql = switch_mprintf(
						"INSERT INTO skinny_buttons "
							"(device_name, position, type, label, value, settings) "
							"VALUES('%s', '%s', '%s', '%s', '%s', '%s')",
						request->data.reg.device_name,
						position,
						type,
						label,
						value,
						settings))) {
					skinny_execute_sql(profile, sql, profile->listener_mutex);
					switch_safe_free(sql);
				}
			}
		}
	}

	status = SWITCH_STATUS_SUCCESS;

	/* Reply with RegisterAckMessage */
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_ack));
	message->type = REGISTER_ACK_MESSAGE;
	message->length = 4 + sizeof(message->data.reg_ack);
	message->data.reg_ack.keepAlive = profile->keep_alive;
	memcpy(message->data.reg_ack.dateFormat, profile->date_format, 6);
	message->data.reg_ack.secondaryKeepAlive = profile->keep_alive;
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

static switch_status_t skinny_headset_status_message(listener_t *listener, skinny_message_t *request)
{
	skinny_check_data_length(request, sizeof(request->data.headset_status));
	
	/* Nothing to do */
	return SWITCH_STATUS_SUCCESS;
}

static int skinny_config_stat_res_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	skinny_message_t *message = pArg;
	char *device_name = argv[0];
	int user_id = atoi(argv[1]);
	int instance = atoi(argv[2]);
	char *user_name = argv[3];
	char *server_name = argv[4];
	int number_lines = atoi(argv[5]);
	int number_speed_dials = atoi(argv[6]);
	
	strcpy(message->data.config_res.device_name, device_name);
	message->data.config_res.user_id = user_id;
	message->data.config_res.instance = instance;
	strcpy(message->data.config_res.user_name, user_name);
	strcpy(message->data.config_res.server_name, server_name);
	message->data.config_res.number_lines = number_lines;
	message->data.config_res.number_speed_dials = number_speed_dials;

	return 0;
}

static switch_status_t skinny_handle_config_stat_request(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_message_t *message;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.config_res));
	message->type = CONFIG_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.config_res);

	if ((sql = switch_mprintf(
			"SELECT name, user_id, instance, '' AS user_name, '' AS server_name, "
				"(SELECT COUNT(*) FROM skinny_buttons WHERE device_name='%s' AND type='line') AS number_lines, "
				"(SELECT COUNT(*) FROM skinny_buttons WHERE device_name='%s' AND type='speed-dial') AS number_speed_dials "
				"FROM skinny_devices WHERE name='%s' ",
			listener->device_name,
			listener->device_name,
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_config_stat_res_callback, message);
		switch_safe_free(sql);
	}
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_capabilities_response(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_profile_t *profile;

	uint32_t i = 0;
	uint32_t n = 0;
	char *codec_order[SWITCH_MAX_CODECS];
	char *codec_string;
	
	size_t string_len, string_pos, pos;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.cap_res.count));

	n = request->data.cap_res.count;
	if (n > SWITCH_MAX_CODECS) {
		n = SWITCH_MAX_CODECS;
	}
	string_len = -1;

	skinny_check_data_length(request, sizeof(request->data.cap_res.count) + n * sizeof(request->data.cap_res.caps[0]));

	for (i = 0; i < n; i++) {
		char *codec = skinny_codec2string(request->data.cap_res.caps[i].codec);
		codec_order[i] = codec;
		string_len += strlen(codec)+1;
	}
	i = 0;
	pos = 0;
	codec_string = switch_core_alloc(listener->pool, string_len+1);
	for (string_pos = 0; string_pos < string_len; string_pos++) {
		char *codec = codec_order[i];
		switch_assert(i < n);
		if(pos == strlen(codec)) {
			codec_string[string_pos] = ',';
			i++;
			pos = 0;
		} else {
			codec_string[string_pos] = codec[pos++];
		}
	}
	codec_string[string_len] = '\0';
	if ((sql = switch_mprintf(
			"UPDATE skinny_devices SET codec_string='%s' WHERE name='%s'",
			codec_string,
			listener->device_name
			))) {
		skinny_execute_sql(profile, sql, profile->listener_mutex);
		switch_safe_free(sql);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Codecs %s supported.\n", codec_string);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_port_message(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.as_uint16));

	if ((sql = switch_mprintf(
			"UPDATE skinny_devices SET port='%d' WHERE name='%s'",
			request->data.as_uint16,
			listener->device_name
			))) {
		skinny_execute_sql(profile, sql, profile->listener_mutex);
		switch_safe_free(sql);
	}
	return SWITCH_STATUS_SUCCESS;
}

struct button_template_helper {
	skinny_message_t *message;
	int count[0xff+1];
};

static int skinny_handle_button_template_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct button_template_helper *helper = pArg;
	skinny_message_t *message = helper->message;
	char *device_name = argv[0];
	int position = atoi(argv[1]);
	char *type = argv[2];
	int i;
	
	/* fill buttons between previous one and current one */
	for(i = message->data.button_template.button_count; i+1 < position; i++) {
		message->data.button_template.btn[i].instance_number = ++helper->count[0xff];
		message->data.button_template.btn[i].button_definition = 0xff; /* None */
		message->data.button_template.button_count++;
		message->data.button_template.total_button_count++;
	}


	if (!strcasecmp(type, "line")) {
		message->data.button_template.btn[i].instance_number = ++helper->count[0x09];
		message->data.button_template.btn[position-1].button_definition = 0x09; /* Line */
	} else if (!strcasecmp(type, "speed-dial")) {
		message->data.button_template.btn[i].instance_number = ++helper->count[0x02];
		message->data.button_template.btn[position-1].button_definition = 0x02; /* speeddial */
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
			"Unknown button type %s for device %s.\n", type, device_name);
	}
	message->data.button_template.button_count++;
	message->data.button_template.total_button_count++;

	return 0;
}

static switch_status_t skinny_handle_button_template_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct button_template_helper helper = {0};
	skinny_profile_t *profile;
	char *sql;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.button_template));
	message->type = BUTTON_TEMPLATE_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.button_template);

	message->data.button_template.button_offset = 0;
	message->data.button_template.button_count = 0;
	message->data.button_template.total_button_count = 0;
	
	helper.message = message;
	/* Add buttons */
	if ((sql = switch_mprintf(
			"SELECT device_name, position, type "
				"FROM skinny_buttons WHERE device_name='%s' ORDER BY position",
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_handle_button_template_request_callback, &helper);
		switch_safe_free(sql);
	}
	
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_soft_key_template_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.soft_key_template));
	message->type = SOFT_KEY_TEMPLATE_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.soft_key_template);

	message->data.soft_key_template.soft_key_offset = 0;
	message->data.soft_key_template.soft_key_count = 21;
	message->data.soft_key_template.total_soft_key_count = 21;
	
	/* TODO fill the template */
	strcpy(message->data.soft_key_template.soft_key[0].soft_key_label, "\200\001");
	message->data.soft_key_template.soft_key[0].soft_key_event = 1; /* Redial */
	
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_soft_key_set_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.soft_key_set));
	message->type = SOFT_KEY_SET_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.soft_key_set);

	message->data.soft_key_set.soft_key_set_offset = 0;
	message->data.soft_key_set.soft_key_set_count = 11;
	message->data.soft_key_set.total_soft_key_set_count = 11;
	
	/* TODO fill the set */
	
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

static int skinny_line_stat_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	skinny_message_t *message = pArg;

	message->data.line_res.number++;
	if (message->data.line_res.number == atoi(argv[0])) { /* wanted_position */
		strcpy(message->data.line_res.name, argv[3]); /* value */
		strcpy(message->data.line_res.shortname,  argv[2]); /* label */
		strcpy(message->data.line_res.displayname,  argv[4]); /* settings */
	}
	return 0;
}

static switch_status_t skinny_handle_line_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;
	char *sql;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.line_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.line_res));
	message->type = LINE_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.line_res);
	message->data.line_res.number = 0;

	if ((sql = switch_mprintf(
			"SELECT '%d' AS wanted_position, position, label, value, settings "
				"FROM skinny_buttons WHERE device_name='%s' AND type='line' "
				"ORDER BY position",
			request->data.line_req.number,
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_line_stat_request_callback, message);
		switch_safe_free(sql);
	}
	message->data.line_res.number = request->data.line_req.number;
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}


static int skinny_handle_speed_dial_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	skinny_message_t *message = pArg;

	message->data.speed_dial_res.number++;
	if (message->data.speed_dial_res.number == atoi(argv[0])) { /* wanted_position */
		message->data.speed_dial_res.number = atoi(argv[3]); /* value */
		strcpy(message->data.speed_dial_res.label,  argv[2]); /* label */
	}
	return 0;
}

static switch_status_t skinny_handle_speed_dial_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;
	char *sql;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.speed_dial_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.speed_dial_res));
	message->type = SPEED_DIAL_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.speed_dial_res);
	message->data.speed_dial_res.number = 0;
	if ((sql = switch_mprintf(
			"SELECT '%d' AS wanted_position, position, label, value, settings "
				"FROM skinny_buttons WHERE device_name='%s' AND type='speed-dial' "
				"ORDER BY position",
			request->data.speed_dial_req.number,
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_handle_speed_dial_request_callback, message);
		switch_safe_free(sql);
	}
	message->data.speed_dial_res.number = request->data.speed_dial_req.number;
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_register_available_lines_message(listener_t *listener, skinny_message_t *request)
{
	skinny_check_data_length(request, sizeof(request->data.reg_lines));

	/* Do nothing */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_time_date_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	switch_time_t ts;
	switch_time_exp_t tm;
	
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.define_time_date));
	message->type = DEFINE_TIME_DATE_MESSAGE;
	message->length = 4+sizeof(message->data.define_time_date);
	ts = switch_micro_time_now();
	switch_time_exp_lt(&tm, ts);
	message->data.define_time_date.year = tm.tm_year + 1900;
	message->data.define_time_date.month = tm.tm_mon + 1;
	message->data.define_time_date.day_of_week = tm.tm_wday;
	message->data.define_time_date.day = tm.tm_yday + 1;
	message->data.define_time_date.hour = tm.tm_hour;
	message->data.define_time_date.minute = tm.tm_min;
	message->data.define_time_date.seconds = tm.tm_sec + 1;
	message->data.define_time_date.milliseconds = tm.tm_usec / 1000;
	message->data.define_time_date.timestamp = ts / 1000000;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_keep_alive_message(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;

	message = switch_core_alloc(listener->pool, 12);
	message->type = KEEP_ALIVE_ACK_MESSAGE;
	message->length = 4;
	keepalive_listener(listener, NULL);
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_unregister(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;
	/* skinny::unregister event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_UNREGISTER);
	switch_event_fire(&event);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t skinny_handle_request(listener_t *listener, skinny_message_t *request)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Received message (type=%x,length=%d).\n", request->type, request->length);
	if(zstr(listener->device_name) && request->type != REGISTER_MESSAGE && request->type != ALARM_MESSAGE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"Device should send a register message first.\n");
		return SWITCH_STATUS_FALSE;
	}
	switch(request->type) {
		case ALARM_MESSAGE:
			return skinny_handle_alarm(listener, request);
		/* registering phase */
		case REGISTER_MESSAGE:
			return skinny_handle_register(listener, request);
		case HEADSET_STATUS_MESSAGE:
			return skinny_headset_status_message(listener, request);
		case CONFIG_STAT_REQ_MESSAGE:
			return skinny_handle_config_stat_request(listener, request);
		case CAPABILITIES_RES_MESSAGE:
			return skinny_handle_capabilities_response(listener, request);
		case PORT_MESSAGE:
			return skinny_handle_port_message(listener, request);
		case BUTTON_TEMPLATE_REQ_MESSAGE:
			return skinny_handle_button_template_request(listener, request);
		case SOFT_KEY_TEMPLATE_REQ_MESSAGE:
			return skinny_handle_soft_key_template_request(listener, request);
		case SOFT_KEY_SET_REQ_MESSAGE:
			return skinny_handle_soft_key_set_request(listener, request);
		case LINE_STAT_REQ_MESSAGE:
			return skinny_handle_line_stat_request(listener, request);
		case SPEED_DIAL_STAT_REQ_MESSAGE:
			return skinny_handle_speed_dial_request(listener, request);
		case REGISTER_AVAILABLE_LINES_MESSAGE:
			return skinny_handle_register_available_lines_message(listener, request);
		case TIME_DATE_REQ_MESSAGE:
			return skinny_handle_time_date_request(listener, request);
		/* live phase */
		case KEEP_ALIVE_MESSAGE:
			return skinny_handle_keep_alive_message(listener, request);
		/* end phase */
		case UNREGISTER_MESSAGE:
			return skinny_handle_unregister(listener, request);
		case 0xABCDEF: /* the following commands are to avoid compile warnings (which are errors) */
activate_call_plane(listener, 1 /* line */);
send_select_soft_keys(listener, 1 /* line */, 0 /* call_id */, SKINNY_KEY_SET_RING_OUT, 0xffff);
send_dialed_number(listener, 0 /* called_party */, 1 /* line */, 0 /* call_id */);
send_call_state(listener, SKINNY_PROCEED, 1 /* line */, 0 /* call_id */);
open_receive_channel(listener,
	0, /* uint32_t conference_id, */
	0, /* uint32_t pass_thru_party_id, */
	20, /* uint32_t packets, */
	SKINNY_CODEC_ULAW_64K, /* uint32_t payload_capacity, */
	0, /* uint32_t echo_cancel_type, */
	0, /* uint32_t g723_bitrate, */
	0, /* uint32_t conference_id2, */
	0 /* uint32_t reserved[10] */
);
start_media_transmission(listener,
	0, /* uint32_t conference_id, */
	0, /* uint32_t pass_thru_party_id, */
	0, /* uint32_t remote_ip, */
	0, /* uint32_t remote_port, */
	20, /* uint32_t ms_per_packet, */
	SKINNY_CODEC_ULAW_64K, /* uint32_t payload_capacity, */
	184, /* uint32_t precedence, */
	0, /* uint32_t silence_suppression, */
	0, /* uint16_t max_frames_per_packet, */
	0 /* uint32_t g723_bitrate */
);
close_receive_channel(listener,
	0, /* uint32_t conference_id, */
	0, /* uint32_t pass_thru_party_id, */
	0 /* uint32_t conference_id2, */
);
stop_media_transmission(listener,
	0, /* uint32_t conference_id, */
	0, /* uint32_t pass_thru_party_id, */
	0 /* uint32_t conference_id2, */
);
start_tone(listener, SKINNY_TONE_DIALTONE, 0, 0, 0);
stop_tone(listener, 0, 0);
clear_prompt_status(listener, 0, 0);
set_speaker_mode(listener, SKINNY_SPEAKER_OFF);

send_call_state(listener, SKINNY_RING_IN, 0, 0);
send_select_soft_keys(listener, 0, 0,
	SKINNY_KEY_SET_RING_IN, 0xffff);
display_prompt_status(listener, 0, "\200\027tel", 0, 0);
/* displayprinotifiymessage */
send_call_info(listener,
	"TODO", /* char calling_party_name[40], */
	"TODO", /* char calling_party[24], */
	"TODO", /* char called_party_name[40], */
	"TODO", /* char called_party[24], */
	0, /* uint32_t line_instance, */
	0, /* uint32_t call_id, */
	SKINNY_OUTBOUND_CALL, /* uint32_t call_type, */
	"TODO", /* char original_called_party_name[40], */
	"TODO", /* char original_called_party[24], */
	"TODO", /* char last_redirecting_party_name[40], */
	"TODO", /* char last_redirecting_party[24], */
	0, /* uint32_t original_called_party_redirect_reason, */
	0, /* uint32_t last_redirecting_reason, */
	"TODO", /* char calling_party_voice_mailbox[24], */
	"TODO", /* char called_party_voice_mailbox[24], */
	"TODO", /* char original_called_party_voice_mailbox[24], */
	"TODO", /* char last_redirecting_voice_mailbox[24], */
	1, /* uint32_t call_instance, */
	1, /* uint32_t call_security_status, */
	0 /* uint32_t party_pi_restriction_bits */
);
set_lamp(listener, SKINNY_BUTTON_LINE, 0, SKINNY_LAMP_BLINK);
set_ringer(listener, SKINNY_RING_OUTSIDE, SKINNY_RING_FOREVER, 0);



		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"Unknown request type: %x (length=%d).\n", request->type, request->length);
			return SWITCH_STATUS_SUCCESS;
	}
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

/*****************************************************************************/
/* LISTENER FUNCTIONS */
/*****************************************************************************/

static void add_listener(listener_t *listener)
{
	skinny_profile_t *profile;
	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;

	switch_mutex_lock(profile->listener_mutex);
	listener->next = profile->listeners;
	profile->listeners = listener;
	switch_mutex_unlock(profile->listener_mutex);
}

static void remove_listener(listener_t *listener)
{
	listener_t *l, *last = NULL;
	skinny_profile_t *profile;
	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;

	switch_mutex_lock(profile->listener_mutex);
	for (l = profile->listeners; l; l = l->next) {
		if (l == listener) {
			if (last) {
				last->next = l->next;
			} else {
				profile->listeners = l->next;
			}
		}
		last = l;
	}
	switch_mutex_unlock(profile->listener_mutex);
}


static void walk_listeners(skinny_listener_callback_func_t callback, void *pvt)
{
	switch_hash_index_t *hi;
	void *val;
	skinny_profile_t *profile;
	listener_t *l;
	
	/* walk listeners */
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;

		switch_mutex_lock(profile->listener_mutex);
		for (l = profile->listeners; l; l = l->next) {
			callback(l, pvt);
		}
		switch_mutex_unlock(profile->listener_mutex);
	}
}

static void flush_listener(listener_t *listener, switch_bool_t flush_log, switch_bool_t flush_events)
{

	/* TODO */
}

static int dump_device_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_stream_handle_t *stream = (switch_stream_handle_t *) pArg;

	char *device_name = argv[0];
	char *user_id = argv[1];
	char *instance = argv[2];
	char *ip = argv[3];
	char *type = argv[4];
	char *max_streams = argv[5];
	char *port = argv[6];
	char *codec_string = argv[7];

	const char *line = "=================================================================================================";
	stream->write_function(stream, "%s\n", line);
	stream->write_function(stream, "DeviceName    \t%s\n", switch_str_nil(device_name));
	stream->write_function(stream, "UserId        \t%s\n", user_id);
	stream->write_function(stream, "Instance      \t%s\n", instance);
	stream->write_function(stream, "IP            \t%s\n", ip);
	stream->write_function(stream, "DeviceType    \t%s\n", type);
	stream->write_function(stream, "MaxStreams    \t%s\n", max_streams);
	stream->write_function(stream, "Port          \t%s\n", port);
	stream->write_function(stream, "Codecs        \t%s\n", codec_string);
	stream->write_function(stream, "%s\n", line);

	return 0;
}

static switch_status_t dump_device(skinny_profile_t *profile, const char *device_name, switch_stream_handle_t *stream)
{
	char *sql;
	if ((sql = switch_mprintf("SELECT * FROM skinny_devices WHERE name LIKE '%s'",
			device_name))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, dump_device_callback, stream);
		switch_safe_free(sql);
	}

	return SWITCH_STATUS_SUCCESS;
}


static void close_socket(switch_socket_t **sock, skinny_profile_t *profile)
{
	switch_mutex_lock(profile->sock_mutex);
	if (*sock) {
		switch_socket_shutdown(*sock, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(*sock);
		*sock = NULL;
	}
	switch_mutex_unlock(profile->sock_mutex);
}

static switch_status_t kill_listener(listener_t *listener, void *pvt)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Killing listener.\n");
	switch_clear_flag(listener, LFLAG_RUNNING);
	close_socket(&listener->sock, listener->profile);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kill_expired_listener(listener_t *listener, void *pvt)
{
	switch_event_t *event = NULL;

	if(listener->expire_time < switch_epoch_time_now(NULL)) {
		/* skinny::expire event */
		skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_EXPIRE);
		switch_event_fire(&event);
		return kill_listener(listener, pvt);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t keepalive_listener(listener_t *listener, void *pvt)
{
	skinny_profile_t *profile;
	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;
	
	listener->expire_time = switch_epoch_time_now(NULL)+profile->keep_alive*110/100;

	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj)
{
	listener_t *listener = (listener_t *) obj;
	switch_status_t status;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	skinny_message_t *request = NULL;
	skinny_profile_t *profile;
	switch_assert(listener);
	assert(listener->profile);
	profile = listener->profile;

	switch_mutex_lock(profile->listener_mutex);
	profile->listener_threads++;
	switch_mutex_unlock(profile->listener_mutex);
	
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


	while (globals.running && switch_test_flag(listener, LFLAG_RUNNING) && profile->listener_ready) {
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

	}

  done:
	
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
		close_socket(&listener->sock, profile);
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

	switch_mutex_lock(profile->listener_mutex);
	profile->listener_threads--;
	switch_mutex_unlock(profile->listener_mutex);

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

int skinny_socket_create_and_bind(skinny_profile_t *profile)
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

	while(globals.running) {
		rv = switch_sockaddr_info_get(&sa, profile->ip, SWITCH_INET, profile->port, 0, pool);
		if (rv)
			goto fail;
		rv = switch_socket_create(&profile->sock, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, pool);
		if (rv)
			goto sock_fail;
		rv = switch_socket_opt_set(profile->sock, SWITCH_SO_REUSEADDR, 1);
		if (rv)
			goto sock_fail;
		rv = switch_socket_bind(profile->sock, sa);
		if (rv)
			goto sock_fail;
		rv = switch_socket_listen(profile->sock, 5);
		if (rv)
			goto sock_fail;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Socket up listening on %s:%u\n", profile->ip, profile->port);

		break;
	  sock_fail:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error! Could not listen on %s:%u\n", profile->ip, profile->port);
		switch_yield(100000);
	}

	profile->listener_ready = 1;

	while(globals.running) {

		if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			goto fail;
		}

		if ((rv = switch_socket_accept(&inbound_socket, profile->sock, listener_pool))) {
			if (!globals.running) {
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
		strcpy(listener->device_name, "");
		listener->profile = profile;

		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);

		switch_socket_addr_get(&listener->sa, SWITCH_TRUE, listener->sock);
		switch_get_addr(listener->remote_ip, sizeof(listener->remote_ip), listener->sa);
		listener->remote_port = switch_sockaddr_get_port(listener->sa);
		launch_listener_thread(listener);

	}

 end:

	close_socket(&profile->sock, profile);
	
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
static void skinny_profile_set(skinny_profile_t *profile, char *var, char *val)
{
	if (!var)
		return;

	if (!strcasecmp(var, "domain")) {
		profile->domain = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "ip")) {
		profile->ip = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "dialplan")) {
		profile->dialplan = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "odbc-dsn") && !zstr(val)) {
		if (switch_odbc_available()) {
			profile->odbc_dsn = switch_core_strdup(module_pool, val);
			if ((profile->odbc_user = strchr(profile->odbc_dsn, ':'))) {
				*profile->odbc_user++ = '\0';
				if ((profile->odbc_pass = strchr(profile->odbc_user, ':'))) {
					*profile->odbc_pass++ = '\0';
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
		}
	}
}

static switch_status_t load_skinny_config(void)
{
	char *cf = "skinny.conf";
	switch_xml_t xcfg, xml, xsettings, xprofiles, xprofile, xparam;

	memset(&globals, 0, sizeof(globals));
	globals.running = 1;

	switch_core_hash_init(&globals.profile_hash, module_pool);

	switch_mutex_init(&globals.calls_mutex, SWITCH_MUTEX_NESTED, module_pool);
	
	if (!(xml = switch_xml_open_cfg(cf, &xcfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((xsettings = switch_xml_child(xcfg, "settings"))) {
		for (xparam = switch_xml_child(xsettings, "param"); xparam; xparam = xparam->next) {
			char *var = (char *) switch_xml_attr_soft(xparam, "name");
			char *val = (char *) switch_xml_attr_soft(xparam, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
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
			}
		} /* param */
	} /* settings */

	if ((xprofiles = switch_xml_child(xcfg, "profiles"))) {
		for (xprofile = switch_xml_child(xprofiles, "profile"); xprofile; xprofile = xprofile->next) {
			char *profile_name = (char *) switch_xml_attr_soft(xprofile, "name");
			switch_xml_t xsettings = switch_xml_child(xprofile, "settings");
			if (zstr(profile_name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
					"<profile> is missing name attribute\n");
				continue;
			}
			if (xsettings) {
				char dbname[256];
				switch_core_db_t *db;
				skinny_profile_t *profile = NULL;
				switch_xml_t param;
				
				profile = switch_core_alloc(module_pool, sizeof(skinny_profile_t));
				profile->name = profile_name;
				
				for (param = switch_xml_child(xsettings, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					char *val = (char *) switch_xml_attr_soft(param, "value");

					if (!strcmp(var, "domain")) {
						skinny_profile_set(profile, "domain", val);
					} else if (!strcmp(var, "ip")) {
						skinny_profile_set(profile, "ip", val);
					} else if (!strcmp(var, "port")) {
						profile->port = atoi(val);
					} else if (!strcmp(var, "dialplan")) {
						skinny_profile_set(profile, "dialplan", val);
					} else if (!strcmp(var, "keep-alive")) {
						profile->keep_alive = atoi(val);
					} else if (!strcmp(var, "date-format")) {
						memcpy(profile->date_format, val, 6);
					}
				} /* param */
				
				if (!profile->dialplan) {
					skinny_profile_set(profile, "dialplan","default");
				}

				if (!profile->port) {
					profile->port = 2000;
				}

				switch_snprintf(dbname, sizeof(dbname), "skinny_%s", profile->name);
				profile->dbname = switch_core_strdup(module_pool, dbname);

				if (switch_odbc_available() && profile->odbc_dsn) {
					if (!(profile->master_odbc = switch_odbc_handle_new(profile->odbc_dsn, profile->odbc_user, profile->odbc_pass))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
						continue;

					}
					if (switch_odbc_handle_connect(profile->master_odbc) != SWITCH_ODBC_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected ODBC DSN: %s\n", profile->odbc_dsn);
					switch_odbc_handle_exec(profile->master_odbc, devices_sql, NULL, NULL);
					switch_odbc_handle_exec(profile->master_odbc, buttons_sql, NULL, NULL);
				} else {
					if ((db = switch_core_db_open_file(profile->dbname))) {
						switch_core_db_test_reactive(db, "SELECT * FROM skinny_devices", NULL, devices_sql);
						switch_core_db_test_reactive(db, "SELECT * FROM skinny_buttons", NULL, buttons_sql);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database!\n");
						continue;
					}
					switch_core_db_close(db);
				}
				
				skinny_execute_sql_callback(profile, profile->listener_mutex, "DELETE FROM skinny_devices", NULL, NULL);
				skinny_execute_sql_callback(profile, profile->listener_mutex, "DELETE FROM skinny_buttons", NULL, NULL);

				switch_core_hash_init(&profile->session_hash, module_pool);
				
				switch_core_hash_insert(globals.profile_hash, profile->name, profile);
				profile = NULL;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
					"Settings are missing from profile %s.\n", profile_name);
			} /* settings */
		} /* profile */
	}
	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t cmd_status_profile(const char *profile_name, switch_stream_handle_t *stream)
{
	skinny_profile_t *profile;
	if ((profile = skinny_find_profile(profile_name))) {
		dump_profile(profile, stream);
	} else {
		stream->write_function(stream, "Profile not found!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t cmd_status_profile_device(const char *profile_name, const char *device_name, switch_stream_handle_t *stream)
{
	skinny_profile_t *profile;
	if ((profile = skinny_find_profile(profile_name))) {
		dump_device(profile, device_name, stream);
	} else {
		stream->write_function(stream, "Profile not found!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(skinny_function)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"skinny help\n"
		"skinny status profile <profile_name>\n"
		"skinny status profile <profile_name> device <device_name>\n"
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

	if (argc == 3 && !strcasecmp(argv[0], "status") && !strcasecmp(argv[1], "profile")) {
		status = cmd_status_profile(argv[2], stream);
	} else if (argc == 5 && !strcasecmp(argv[0], "status") && !strcasecmp(argv[1], "profile") && !strcasecmp(argv[3], "device")) {
		status = cmd_status_profile_device(argv[2], argv[4], stream);
	} else if (!strcasecmp(argv[0], "help")) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
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

static switch_status_t skinny_list_profiles(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_hash_index_t *hi;
	void *val;
	skinny_profile_t *profile;
	
	/* walk profiles */
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;

		switch_console_push_match(&my_matches, profile->name);
	}
	
	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}
	
	return status;
}

struct match_helper {
	switch_console_callback_match_t *my_matches;
};

static int skinny_list_devices_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct match_helper *h = (struct match_helper *) pArg;
	char *device_name = argv[0];

	switch_console_push_match(&h->my_matches, device_name);
	return 0;
}

static switch_status_t skinny_list_devices(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	struct match_helper h = { 0 };
	switch_status_t status = SWITCH_STATUS_FALSE;
	skinny_profile_t *profile;
	char *sql;

	char *myline;
	char *argv[1024] = { 0 };
	int argc = 0;

	if (!(myline = strdup(line))) {
		status = SWITCH_STATUS_MEMERR;
		return status;
	}
	if (!(argc = switch_separate_string(myline, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || argc != 5) {
		return status;
	}

	if((profile = skinny_find_profile(argv[3]))) {
		if ((sql = switch_mprintf("SELECT name FROM skinny_devices"))) {
			skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_list_devices_callback, &h);
			switch_safe_free(sql);
		}
	}
	
	if (h.my_matches) {
		*matches = h.my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}
	
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_skinny_load)
{
	switch_hash_index_t *hi;
	void *val;
	skinny_profile_t *profile;

	switch_api_interface_t *api_interface;

	module_pool = pool;

	load_skinny_config();

	/* init listeners */
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;
	
		switch_mutex_init(&profile->listener_mutex, SWITCH_MUTEX_NESTED, module_pool);
		switch_mutex_init(&profile->sock_mutex, SWITCH_MUTEX_NESTED, module_pool);

	}

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
	switch_console_set_complete("add skinny status profile ::skinny::list_profiles");
	switch_console_set_complete("add skinny status profile ::skinny::list_profiles device ::skinny::list_devices");

	switch_console_add_complete_func("::skinny::list_profiles", skinny_list_profiles);
	switch_console_add_complete_func("::skinny::list_devices", skinny_list_devices);
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_skinny_runtime)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_hash_index_t *hi;
	void *val;
	skinny_profile_t *profile;
	
	/* launch listeners */
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;
	
		status = skinny_socket_create_and_bind(profile);
		if(status != SWITCH_STATUS_SUCCESS) {
			return status;
		}
	}
	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skinny_shutdown)
{
	switch_hash_index_t *hi;
	void *val;
	skinny_profile_t *profile;
	int sanity = 0;

	switch_event_free_subclass(SKINNY_EVENT_REGISTER);
	switch_event_unbind(&globals.heartbeat_node);

	globals.running = 0;

	/* kill listeners */
	walk_listeners(kill_listener, NULL);

	/* close sockets */
	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (skinny_profile_t *) val;

		close_socket(&profile->sock, profile);

		while (profile->listener_threads) {
			switch_yield(100000);
			walk_listeners(kill_listener, NULL);
			if (++sanity >= 200) {
				break;
			}
		}
	}

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
