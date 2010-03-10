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
 * skinny_protocol.h -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#ifndef _MOD_SKINNY_H
/* mod_skinny.h should be loaded first */
#include "mod_skinny.h"
#endif /* _MOD_SKINNY_H */

#ifndef _SKINNY_PROTOCOL_H
#define _SKINNY_PROTOCOL_H

#include <switch.h>

/*****************************************************************************/
/* SKINNY MESSAGE DATA */
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
	/* uint32_t call_reference; */
};

/* OffHookMessage */
#define OFF_HOOK_MESSAGE 0x0006
struct off_hook_message {
	uint32_t line_instance;
	/* uint32_t call_id; */
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
	uint32_t event;
	uint32_t line_instance;
	uint32_t callreference;
};

/* UnregisterMessage */
#define UNREGISTER_MESSAGE 0x0027

/* SoftKeyTemplateReqMessage */
#define SOFT_KEY_TEMPLATE_REQ_MESSAGE 0x0028

/* ServiceUrlStatReqMessage */
#define SERVICE_URL_STAT_REQ_MESSAGE 0x0033
struct service_url_stat_req_message {
	uint32_t service_url_index;
};

/* FeatureStatReqMessage */
#define FEATURE_STAT_REQ_MESSAGE 0x0034
struct feature_stat_req_message {
	uint32_t feature_index;
};

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

/* SetLampMessage */
#define SET_LAMP_MESSAGE 0x0086
struct set_lamp_message {
	uint32_t stimulus; /* See enum skinny_button_definition */
	uint32_t stimulus_instance;
	uint32_t mode; /* See enum skinny_lamp_mode */
};

/* SetSpeakerModeMessage */
#define SET_SPEAKER_MODE_MESSAGE 0x0088
struct set_speaker_mode_message {
	uint32_t mode; /* See enum skinny_speaker_mode */
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

#define SKINNY_MAX_BUTTON_COUNT 42
struct button_template_message {
	uint32_t button_offset;
	uint32_t button_count;
	uint32_t total_button_count;
	struct button_definition btn[SKINNY_MAX_BUTTON_COUNT];
};

/* CapabilitiesReqMessage */
#define CAPABILITIES_REQ_MESSAGE 0x009B

/* RegisterRejectMessage */
#define REGISTER_REJ_MESSAGE 0x009D
struct register_rej_message {
	char error[33];
};

/* ResetMessage */
#define RESET_MESSAGE 0x009F
struct reset_message {
	uint32_t reset_type; /* See enum skinny_device_reset_types */
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
	uint8_t soft_key_template_index[16]; /* See enum skinny_soft_key_event */
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

/* CallStateMessage */
#define CALL_STATE_MESSAGE 0x0111
struct call_state_message {
	uint32_t call_state; /* See enum skinny_call_state */
	uint32_t line_instance;
	uint32_t call_id;
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

/* UnregisterAckMessage */
#define UNREGISTER_ACK_MESSAGE 0x0118
struct unregister_ack_message {
	uint32_t unregister_status;
};

/* DialedNumberMessage */
#define DIALED_NUMBER_MESSAGE 0x011D
struct dialed_number_message {
	char called_party[24];
	uint32_t line_instance;
	uint32_t call_id;
};

/* FeatureStatMessage */
#define FEATURE_STAT_RES_MESSAGE 0x011F
struct feature_stat_res_message {
	uint32_t index;
	uint32_t id;
	char text_label[40];
	uint32_t status;
};

/* ServiceUrlStatMessage */
#define SERVICE_URL_STAT_RES_MESSAGE 0x012F
struct service_url_stat_res_message {
	uint32_t index;
	char url[256];
	char display_name[40];
};

/*****************************************************************************/
/* SKINNY MESSAGE */
/*****************************************************************************/
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
	struct service_url_stat_req_message service_url_req;
	struct feature_stat_req_message feature_req;
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
	struct reset_message reset;
	struct open_receive_channel_message open_receive_channel;
	struct close_receive_channel_message close_receive_channel;
	struct soft_key_template_res_message soft_key_template;
	struct soft_key_set_res_message soft_key_set;
	struct select_soft_keys_message select_soft_keys;
	struct call_state_message call_state;
	struct display_prompt_status_message display_prompt_status;
	struct clear_prompt_status_message clear_prompt_status;
	struct activate_call_plane_message activate_call_plane;
	struct unregister_ack_message unregister_ack;
	struct dialed_number_message dialed_number;
	struct feature_stat_res_message feature_res;
	struct service_url_stat_res_message service_url_res;
	
	uint16_t as_uint16;
	char as_char;
	void *raw;
};

/*
 * header is length+reserved
 * body is type+data
 */
struct skinny_message {
	uint32_t length;
	uint32_t reserved;
	uint32_t type;
	union skinny_data data;
};
typedef struct skinny_message skinny_message_t;

/*****************************************************************************/
/* SKINNY TYPES */
/*****************************************************************************/
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

typedef switch_status_t (*skinny_command_t) (char **argv, int argc, switch_stream_handle_t *stream);

/*****************************************************************************/
/* SKINNY TABLES */
/*****************************************************************************/
struct skinny_table {
	const char *name;
	uint32_t id;
};

#define SKINNY_DECLARE_ID2STR(func, TABLE, DEFAULT_STR) \
const char *func(uint32_t id) \
{ \
	const char *str = DEFAULT_STR; \
	\
	for (uint8_t x = 0; x < (sizeof(TABLE) / sizeof(struct skinny_table)) - 1; x++) {\
		if (TABLE[x].id == id) {\
			str = TABLE[x].name;\
			break;\
		}\
	}\
	\
	return str;\
}

#define SKINNY_DECLARE_STR2ID(func, TABLE, DEFAULT_ID) \
uint32_t func(const char *str)\
{\
	uint32_t id = DEFAULT_ID;\
	\
	if (*str > 47 && *str < 58) {\
		id = atoi(str);\
	} else {\
		for (uint8_t x = 0; x < (sizeof(TABLE) / sizeof(struct skinny_table)) - 1 && TABLE[x].name; x++) {\
			if (!strcasecmp(TABLE[x].name, str)) {\
				id = TABLE[x].id;\
				break;\
			}\
		}\
	}\
	return id;\
}

#define SKINNY_DECLARE_PUSH_MATCH(TABLE) \
	switch_console_callback_match_t *my_matches = NULL;\
	for (uint8_t x = 0; x < (sizeof(TABLE) / sizeof(struct skinny_table)) - 1; x++) {\
		switch_console_push_match(&my_matches, TABLE[x].name);\
	}\
	if (my_matches) {\
		*matches = my_matches;\
		status = SWITCH_STATUS_SUCCESS;\
	}
	
struct skinny_table SKINNY_MESSAGE_TYPES[55];
const char *skinny_message_type2str(uint32_t id);
uint32_t skinny_str2message_type(const char *str);
#define SKINNY_PUSH_MESSAGE_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_MESSAGE_TYPES)

enum skinny_tone {
	SKINNY_TONE_SILENCE = 0x00,
	SKINNY_TONE_DIALTONE = 0x21,
	SKINNY_TONE_BUSYTONE = 0x23,
	SKINNY_TONE_ALERT = 0x24,
	SKINNY_TONE_REORDER = 0x25,
	SKINNY_TONE_CALLWAITTONE = 0x2D,
	SKINNY_TONE_NOTONE = 0x7F,
};

enum skinny_ring_type {
	SKINNY_RING_OFF = 1,
	SKINNY_RING_INSIDE = 2,
	SKINNY_RING_OUTSIDE = 3,
	SKINNY_RING_FEATURE = 4
};
struct skinny_table SKINNY_RING_TYPES[5];
const char *skinny_ring_type2str(uint32_t id);
uint32_t skinny_str2ring_type(const char *str);
#define SKINNY_PUSH_RING_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_RING_TYPES)

enum skinny_ring_mode {
	SKINNY_RING_FOREVER = 1,
	SKINNY_RING_ONCE = 2,
};
struct skinny_table SKINNY_RING_MODES[3];
const char *skinny_ring_mode2str(uint32_t id);
uint32_t skinny_str2ring_mode(const char *str);
#define SKINNY_PUSH_RING_MODES SKINNY_DECLARE_PUSH_MATCH(SKINNY_RING_MODES)


enum skinny_lamp_mode {
	SKINNY_LAMP_OFF = 1,
	SKINNY_LAMP_ON = 2,
	SKINNY_LAMP_WINK = 3,
	SKINNY_LAMP_FLASH = 4,
	SKINNY_LAMP_BLINK = 5,
};
struct skinny_table SKINNY_LAMP_MODES[6];
const char *skinny_lamp_mode2str(uint32_t id);
uint32_t skinny_str2lamp_mode(const char *str);
#define SKINNY_PUSH_LAMP_MODES SKINNY_DECLARE_PUSH_MATCH(SKINNY_LAMP_MODES)

enum skinny_speaker_mode {
	SKINNY_SPEAKER_ON = 1,
	SKINNY_SPEAKER_OFF = 2,
};
struct skinny_table SKINNY_SPEAKER_MODES[3];
const char *skinny_speaker_mode2str(uint32_t id);
uint32_t skinny_str2speaker_mode(const char *str);
#define SKINNY_PUSH_SPEAKER_MODES SKINNY_DECLARE_PUSH_MATCH(SKINNY_SPEAKER_MODES)

enum skinny_call_type {
	SKINNY_INBOUND_CALL = 1,
	SKINNY_OUTBOUND_CALL = 2,
	SKINNY_FORWARD_CALL = 3,
};

enum skinny_button_definition {
	SKINNY_BUTTON_UNKNOWN = 0x00,
	SKINNY_BUTTON_LAST_NUMBER_REDIAL = 0x01,
	SKINNY_BUTTON_SPEED_DIAL = 0x02,
	SKINNY_BUTTON_LINE = 0x09,
	SKINNY_BUTTON_VOICEMAIL = 0x0F,
	SKINNY_BUTTON_PRIVACY = 0x13,
	SKINNY_BUTTON_SERVICE_URL = 0x14,
	SKINNY_BUTTON_UNDEFINED = 0xFF,
};
struct skinny_table SKINNY_BUTTONS[9];
const char *skinny_button2str(uint32_t id);
uint32_t skinny_str2button(const char *str);
#define SKINNY_PUSH_STIMULI SKINNY_DECLARE_PUSH_MATCH(SKINNY_BUTTONS)

enum skinny_soft_key_event {
	SOFTKEY_REDIAL = 0x01,
	SOFTKEY_NEWCALL = 0x02,
	SOFTKEY_HOLD = 0x03,
	SOFTKEY_TRANSFER = 0x04,
	SOFTKEY_CFWDALL = 0x05,
	SOFTKEY_CFWDBUSY = 0x06,
	SOFTKEY_CFWDNOANSWER = 0x07,
	SOFTKEY_BACKSPACE = 0x08,
	SOFTKEY_ENDCALL = 0x09,
	SOFTKEY_RESUME = 0x0A,
	SOFTKEY_ANSWER = 0x0B,
	SOFTKEY_INFO = 0x0C,
	SOFTKEY_CONFRM = 0x0D,
	SOFTKEY_PARK = 0x0E,
	SOFTKEY_JOIN = 0x0F,
	SOFTKEY_MEETMECONFRM = 0x10,
	SOFTKEY_CALLPICKUP = 0x11,
	SOFTKEY_GRPCALLPICKUP = 0x12,
	SOFTKEY_DND = 0x13,
	SOFTKEY_IDIVERT = 0x14,
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
struct skinny_table SKINNY_KEY_SETS[11];
const char *skinny_soft_key_set2str(uint32_t id);
uint32_t skinny_str2soft_key_set(const char *str);
#define SKINNY_PUSH_SOFT_KEY_SETS SKINNY_DECLARE_PUSH_MATCH(SKINNY_KEY_SETS)


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
struct skinny_table SKINNY_CALL_STATES[15];
const char *skinny_call_state2str(uint32_t id);
uint32_t skinny_str2call_state(const char *str);
#define SKINNY_PUSH_CALL_STATES SKINNY_DECLARE_PUSH_MATCH(SKINNY_CALL_STATES)

enum skinny_device_reset_types {
  SKINNY_DEVICE_RESET = 1,
  SKINNY_DEVICE_RESTART = 2
};
struct skinny_table SKINNY_DEVICE_RESET_TYPES[3];
const char *skinny_device_reset_type2str(uint32_t id);
uint32_t skinny_str2device_reset_type(const char *str);
#define SKINNY_PUSH_DEVICE_RESET_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_DEVICE_RESET_TYPES)

/*****************************************************************************/
/* SKINNY FUNCTIONS */
/*****************************************************************************/
#define skinny_check_data_length(message, len) \
	if (message->length < len+4) {\
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received Too Short Skinny Message (Expected %zu, got %d).\n", len+4, message->length);\
		return SWITCH_STATUS_FALSE;\
	}

switch_status_t skinny_read_packet(listener_t *listener, skinny_message_t **req);

switch_status_t skinny_device_event(listener_t *listener, switch_event_t **ev, switch_event_types_t event_id, const char *subclass_name);

switch_status_t skinny_send_call_info(switch_core_session_t *session);

switch_status_t skinny_create_session(listener_t *listener, uint32_t line, uint32_t to_state);
switch_status_t skinny_process_dest(listener_t *listener, uint32_t line);
switch_status_t skinny_answer(switch_core_session_t *session);

void skinny_line_get(listener_t *listener, uint32_t instance, struct line_stat_res_message **button);
void skinny_speed_dial_get(listener_t *listener, uint32_t instance, struct speed_dial_stat_res_message **button);

switch_status_t skinny_perform_send_reply(listener_t *listener, const char *file, const char *func, int line, skinny_message_t *reply);
#define  skinny_send_reply(listener, reply)  skinny_perform_send_reply(listener, __FILE__, __SWITCH_FUNC__, __LINE__, reply)

switch_status_t skinny_handle_request(listener_t *listener, skinny_message_t *request);

/*****************************************************************************/
/* SKINNY MESSAGE HELPER */
/*****************************************************************************/
switch_status_t start_tone(listener_t *listener,
	uint32_t tone,
	uint32_t reserved,
	uint32_t line_instance,
	uint32_t call_id);
switch_status_t stop_tone(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id);
switch_status_t set_ringer(listener_t *listener,
	uint32_t ring_type,
	uint32_t ring_mode,
	uint32_t unknown);
switch_status_t set_lamp(listener_t *listener,
	uint32_t stimulus,
	uint32_t stimulus_instance,
	uint32_t mode);
switch_status_t set_speaker_mode(listener_t *listener,
	uint32_t mode);
switch_status_t start_media_transmission(listener_t *listener,
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
switch_status_t stop_media_transmission(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t conference_id2);
switch_status_t send_call_info(listener_t *listener,
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
switch_status_t open_receive_channel(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t packets,
	uint32_t payload_capacity,
	uint32_t echo_cancel_type,
	uint32_t g723_bitrate,
	uint32_t conference_id2,
	uint32_t reserved[10]);
switch_status_t close_receive_channel(listener_t *listener,
	uint32_t conference_id,
	uint32_t pass_thru_party_id,
	uint32_t conference_id2);
switch_status_t send_select_soft_keys(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id,
	uint32_t soft_key_set,
	uint32_t valid_key_mask);
switch_status_t send_call_state(listener_t *listener,
	uint32_t call_state,
	uint32_t line_instance,
	uint32_t call_id);
switch_status_t display_prompt_status(listener_t *listener,
	uint32_t timeout,
	char display[32],
	uint32_t line_instance,
	uint32_t call_id);
switch_status_t clear_prompt_status(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id);
switch_status_t activate_call_plane(listener_t *listener,
	uint32_t line_instance);
switch_status_t send_dialed_number(listener_t *listener,
	char called_party[24],
	uint32_t line_instance,
	uint32_t call_id);
switch_status_t send_reset(listener_t *listener,
	uint32_t reset_type);

#endif /* _SKINNY_PROTOCOL_H */

