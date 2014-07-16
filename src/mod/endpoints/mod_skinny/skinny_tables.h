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
 * skinny_tables.h -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#ifndef _SKINNY_TABLES_H
#define _SKINNY_TABLES_H

/*****************************************************************************/
/* SKINNY TABLES */
/*****************************************************************************/
struct skinny_table {
	uint32_t id;
	const char *name;
};

#define SKINNY_DECLARE_ID2STR(func, TABLE, DEFAULT_STR) \
	const char *func(uint32_t id) \
{ \
	const char *str = DEFAULT_STR; \
	uint8_t x; \
	\
	for (x = 0; x < (sizeof(TABLE) / sizeof(struct skinny_table)) - 1; x++) {\
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
	uint32_t id = (uint32_t) DEFAULT_ID;\
	\
	if (*str > 47 && *str < 58) {\
		id = atoi(str);\
	} else {\
		uint8_t x;\
		for (x = 0; x < (sizeof(TABLE) / sizeof(struct skinny_table)) - 1 && TABLE[x].name; x++) {\
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
uint8_t x;\
for (x = 0; x < (sizeof(TABLE) / sizeof(struct skinny_table)) - 1; x++) {\
	switch_console_push_match(&my_matches, TABLE[x].name);\
}\
if (my_matches) {\
	*matches = my_matches;\
	status = SWITCH_STATUS_SUCCESS;\
}


extern struct skinny_table SKINNY_MESSAGE_TYPES[76];
const char *skinny_message_type2str(uint32_t id);
uint32_t skinny_str2message_type(const char *str);
#define SKINNY_PUSH_MESSAGE_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_MESSAGE_TYPES)

extern struct skinny_table SKINNY_DEVICE_TYPES[75];
const char *skinny_device_type2str(uint32_t id);
uint32_t skinny_str2device_type(const char *str);
#define SKINNY_PUSH_DEVICE_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_DEVICE_TYPES)

enum skinny_tone {
	SKINNY_TONE_SILENCE = 0x00,
	SKINNY_TONE_DIALTONE = 0x21,
	SKINNY_TONE_BUSYTONE = 0x23,
	SKINNY_TONE_ALERT = 0x24,
	SKINNY_TONE_REORDER = 0x25,
	SKINNY_TONE_CALLWAITTONE = 0x2D,
	SKINNY_TONE_NOTONE = 0x7F,
};
extern struct skinny_table SKINNY_TONES[9];
const char *skinny_tone2str(uint32_t id);
uint32_t skinny_str2tone(const char *str);
#define SKINNY_PUSH_TONES SKINNY_DECLARE_PUSH_MATCH(SKINNY_TONES)

enum skinny_ring_type {
	SKINNY_RING_OFF = 1,
	SKINNY_RING_INSIDE = 2,
	SKINNY_RING_OUTSIDE = 3,
	SKINNY_RING_FEATURE = 4,
	SKINNY_RING_FLASHONLY = 5
};
extern struct skinny_table SKINNY_RING_TYPES[6];
const char *skinny_ring_type2str(uint32_t id);
uint32_t skinny_str2ring_type(const char *str);
#define SKINNY_PUSH_RING_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_RING_TYPES)

enum skinny_ring_mode {
	SKINNY_RING_FOREVER = 1,
	SKINNY_RING_ONCE = 2,
};
extern struct skinny_table SKINNY_RING_MODES[3];
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
extern struct skinny_table SKINNY_LAMP_MODES[6];
const char *skinny_lamp_mode2str(uint32_t id);
uint32_t skinny_str2lamp_mode(const char *str);
#define SKINNY_PUSH_LAMP_MODES SKINNY_DECLARE_PUSH_MATCH(SKINNY_LAMP_MODES)

enum skinny_speaker_mode {
	SKINNY_SPEAKER_ON = 1,
	SKINNY_SPEAKER_OFF = 2,
};
extern struct skinny_table SKINNY_SPEAKER_MODES[3];
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
	SKINNY_BUTTON_HOLD = 0x03,
	SKINNY_BUTTON_TRANSFER = 0x04,
	SKINNY_BUTTON_FORWARDALL = 0x05,
	SKINNY_BUTTON_LINE = 0x09,
	SKINNY_BUTTON_VOICEMAIL = 0x0F,
	SKINNY_BUTTON_PRIVACY = 0x13,
	SKINNY_BUTTON_SERVICE_URL = 0x14,
	SKINNY_BUTTON_UNDEFINED = 0xFF,
};
extern struct skinny_table SKINNY_BUTTONS[12];
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
	SOFTKEY_CONF = 0x0D,
	SOFTKEY_PARK = 0x0E,
	SOFTKEY_JOIN = 0x0F,
	SOFTKEY_MEETME = 0x10,
	SOFTKEY_CALLPICKUP = 0x11,
	SOFTKEY_GRPCALLPICKUP = 0x12,
	SOFTKEY_DND = 0x13,
	SOFTKEY_IDIVERT = 0x14,
};
extern struct skinny_table SKINNY_SOFT_KEY_EVENTS[21];
const char *skinny_soft_key_event2str(uint32_t id);
uint32_t skinny_str2soft_key_event(const char *str);
#define SKINNY_PUSH_SOFT_KEY_EVENTS SKINNY_DECLARE_PUSH_MATCH(SOFT_KEY_EVENTS)

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
	SKINNY_KEY_SET_IN_USE_HINT = 10,
};
extern struct skinny_table SKINNY_KEY_SETS[12];
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
	SKINNY_LINE_IN_USE = 7,
	SKINNY_HOLD = 8,
	SKINNY_CALL_WAITING = 9,
	SKINNY_CALL_TRANSFER = 10,
	SKINNY_CALL_PARK = 11,
	SKINNY_PROCEED = 12,
	SKINNY_IN_USE_REMOTELY = 13,
	SKINNY_INVALID_NUMBER = 14
};
extern struct skinny_table SKINNY_CALL_STATES[15];
const char *skinny_call_state2str(uint32_t id);
uint32_t skinny_str2call_state(const char *str);
#define SKINNY_PUSH_CALL_STATES SKINNY_DECLARE_PUSH_MATCH(SKINNY_CALL_STATES)

enum skinny_device_reset_types {
	SKINNY_DEVICE_RESET = 1,
	SKINNY_DEVICE_RESTART = 2
};
extern struct skinny_table SKINNY_DEVICE_RESET_TYPES[3];
const char *skinny_device_reset_type2str(uint32_t id);
uint32_t skinny_str2device_reset_type(const char *str);
#define SKINNY_PUSH_DEVICE_RESET_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_DEVICE_RESET_TYPES)

enum skinny_accessory_types {
	SKINNY_ACCESSORY_NONE = 0x00,
	SKINNY_ACCESSORY_HEADSET = 0x01,
	SKINNY_ACCESSORY_HANDSET = 0x02,
	SKINNY_ACCESSORY_SPEAKER = 0x03
};
extern struct skinny_table SKINNY_ACCESSORY_TYPES[5];
const char *skinny_accessory_type2str(uint32_t id);
uint32_t skinny_str2accessory_type(const char *str);
#define SKINNY_PUSH_ACCESSORY_TYPES SKINNY_DECLARE_PUSH_MATCH(SKINNY_ACCESSORY_TYPES)

enum skinny_accessory_states {
	SKINNY_ACCESSORY_STATE_NONE = 0x00,
	SKINNY_ACCESSORY_STATE_OFFHOOK = 0x01,
	SKINNY_ACCESSORY_STATE_ONHOOK = 0x02
};
extern struct skinny_table SKINNY_ACCESSORY_STATES[4];
const char *skinny_accessory_state2str(uint32_t id);
uint32_t skinny_str2accessory_state(const char *str);
#define SKINNY_PUSH_ACCESSORY_STATES SKINNY_DECLARE_PUSH_MATCH(SKINNY_ACCESSORY_STATES)

enum skinny_label {
	SKINNY_TEXTID_EMPTY = 0,
	SKINNY_TEXTID_REDIAL = 1,
	SKINNY_TEXTID_NEWCALL = 2,
	SKINNY_TEXTID_HOLD = 3,
	SKINNY_TEXTID_TRANSFER = 4,
	SKINNY_TEXTID_CFWDALL = 5,
	SKINNY_TEXTID_CFWDBUSY = 6,
	SKINNY_TEXTID_CFWDNOANSWER = 7,
	SKINNY_TEXTID_BACKSPACE = 8,
	SKINNY_TEXTID_ENDCALL = 9,
	SKINNY_TEXTID_RESUME = 10,
	SKINNY_TEXTID_ANSWER = 11,
	SKINNY_TEXTID_INFO = 12,
	SKINNY_TEXTID_CONF = 13,
	SKINNY_TEXTID_PARK = 14,
	SKINNY_TEXTID_JOIN = 15,
	SKINNY_TEXTID_MEETME = 16,
	SKINNY_TEXTID_CALLPICKUP = 17,
	SKINNY_TEXTID_GRPCALLPICKUP = 18,
	SKINNY_TEXTID_YOUR_CURRENT_OPTIONS = 19,
	SKINNY_TEXTID_OFF_HOOK = 20,
	SKINNY_TEXTID_ON_HOOK = 21,
	SKINNY_TEXTID_RING_OUT = 22,
	SKINNY_TEXTID_FROM = 23,
	SKINNY_TEXTID_CONNECTED = 24,
	SKINNY_TEXTID_BUSY = 25,
	SKINNY_TEXTID_LINE_IN_USE = 26,
	SKINNY_TEXTID_CALL_WAITING = 27,
	SKINNY_TEXTID_CALL_TRANSFER = 28,
	SKINNY_TEXTID_CALL_PARK = 29,
	SKINNY_TEXTID_CALL_PROCEED = 30,
	SKINNY_TEXTID_IN_USE_REMOTE = 31,
	SKINNY_TEXTID_ENTER_NUMBER = 32,
	SKINNY_TEXTID_CALL_PARK_AT = 33,
	SKINNY_TEXTID_PRIMARY_ONLY = 34,
	SKINNY_TEXTID_TEMP_FAIL = 35,
	SKINNY_TEXTID_YOU_HAVE_VOICEMAIL = 36,
	SKINNY_TEXTID_FORWARDED_TO = 37,
	SKINNY_TEXTID_CAN_NOT_COMPLETE_CONFERENCE = 38,
	SKINNY_TEXTID_NO_CONFERENCE_BRIDGE = 39,
	SKINNY_TEXTID_CAN_NOT_HOLD_PRIMARY_CONTROL = 40,
	SKINNY_TEXTID_INVALID_CONFERENCE_PARTICIPANT = 41,
	SKINNY_TEXTID_IN_CONFERENCE_ALREADY = 42,
	SKINNY_TEXTID_NO_PARTICIPANT_INFO = 43,
	SKINNY_TEXTID_EXCEED_MAXIMUM_PARTIES = 44,
	SKINNY_TEXTID_KEY_IS_NOT_ACTIVE = 45,
	SKINNY_TEXTID_ERROR_NO_LICENSE = 46,
	SKINNY_TEXTID_ERROR_DBCONFIG = 47,
	SKINNY_TEXTID_ERROR_DATABASE = 48,
	SKINNY_TEXTID_ERROR_PASS_LIMIT = 49,
	SKINNY_TEXTID_ERROR_UNKNOWN = 50,
	SKINNY_TEXTID_ERROR_MISMATCH = 51,
	SKINNY_TEXTID_CONFERENCE = 52,
	SKINNY_TEXTID_PARK_NUMBER = 53,
	SKINNY_TEXTID_PRIVATE = 54,
	SKINNY_TEXTID_NOT_ENOUGH_BANDWIDTH = 55,
	SKINNY_TEXTID_UNKNOWN_NUMBER = 56,
	SKINNY_TEXTID_RMLSTC = 57,
	SKINNY_TEXTID_VOICEMAIL = 58,
	SKINNY_TEXTID_IMMDIV = 59,
	SKINNY_TEXTID_INTRCPT = 60,
	SKINNY_TEXTID_SETWTCH = 61,
	SKINNY_TEXTID_TRNSFVM = 62,
	SKINNY_TEXTID_DND = 63,
	SKINNY_TEXTID_DIVALL = 64,
	SKINNY_TEXTID_CALLBACK = 65,
	SKINNY_TEXTID_NETWORK_CONGESTION_REROUTING = 66,
	SKINNY_TEXTID_BARGE = 67,
	SKINNY_TEXTID_FAILED_TO_SETUP_BARGE = 68,
	SKINNY_TEXTID_ANOTHER_BARGE_EXISTS = 69,
	SKINNY_TEXTID_INCOMPATIBLE_DEVICE_TYPE = 70,
	SKINNY_TEXTID_NO_PARK_NUMBER_AVAILABLE = 71,
	SKINNY_TEXTID_CALLPARK_REVERSION = 72,
	SKINNY_TEXTID_SERVICE_IS_NOT_ACTIVE = 73,
	SKINNY_TEXTID_HIGH_TRAFFIC_TRY_AGAIN_LATER = 74,
	SKINNY_TEXTID_QRT = 75,
	SKINNY_TEXTID_MCID = 76,
	SKINNY_TEXTID_DIRTRFR = 77,
	SKINNY_TEXTID_SELECT = 78,
	SKINNY_TEXTID_CONFLIST = 79,
	SKINNY_TEXTID_IDIVERT = 80,
	SKINNY_TEXTID_CBARGE = 81,
	SKINNY_TEXTID_CAN_NOT_COMPLETE_TRANSFER = 82,
	SKINNY_TEXTID_CAN_NOT_JOIN_CALLS = 83,
	SKINNY_TEXTID_MCID_SUCCESSFUL = 84,
	SKINNY_TEXTID_NUMBER_NOT_CONFIGURED = 85,
	SKINNY_TEXTID_SECURITY_ERROR = 86,
	SKINNY_TEXTID_VIDEO_BANDWIDTH_UNAVAILABLE = 87,
	SKINNY_TEXTID_VIDMODE = 88,
	SKINNY_TEXTID_MAX_CALL_DURATION_TIMEOUT = 89,
	SKINNY_TEXTID_MAX_HOLD_DURATION_TIMEOUT = 90,
	SKINNY_TEXTID_OPICKUP = 91,
	SKINNY_TEXTID_EXTERNAL_TRANSFER_RESTRICTED = 97,
	SKINNY_TEXTID_MAC_ADDRESS = 101,
	SKINNY_TEXTID_HOST_NAME = 102,
	SKINNY_TEXTID_DOMAIN_NAME = 103,
	SKINNY_TEXTID_IP_ADDRESS = 104,
	SKINNY_TEXTID_SUBNET_MASK = 105,
	SKINNY_TEXTID_TFTP_SERVER_1 = 106,
	SKINNY_TEXTID_DEFAULT_ROUTER_1 = 107,
	SKINNY_TEXTID_DEFAULT_ROUTER_2 = 108,
	SKINNY_TEXTID_DEFAULT_ROUTER_3 = 109,
	SKINNY_TEXTID_DEFAULT_ROUTER_4 = 110,
	SKINNY_TEXTID_DEFAULT_ROUTER_5 = 111,
	SKINNY_TEXTID_DNS_SERVER_1 = 112,
	SKINNY_TEXTID_DNS_SERVER_2 = 113,
	SKINNY_TEXTID_DNS_SERVER_3 = 114,
	SKINNY_TEXTID_DNS_SERVER_4 = 115,
	SKINNY_TEXTID_DNS_SERVER_5 = 116,
	SKINNY_TEXTID_OPERATIONAL_VLAN_ID = 117,
	SKINNY_TEXTID_ADMIN_VLAN_ID = 118,
	SKINNY_TEXTID_CALL_MANAGER_1 = 119,
	SKINNY_TEXTID_CALL_MANAGER_2 = 120,
	SKINNY_TEXTID_CALL_MANAGER_3 = 121,
	SKINNY_TEXTID_CALL_MANAGER_4 = 122,
	SKINNY_TEXTID_CALL_MANAGER_5 = 123,
	SKINNY_TEXTID_INFORMATION_URL = 124,
	SKINNY_TEXTID_DIRECTORIES_URL = 125,
	SKINNY_TEXTID_MESSAGES_URL = 126,
	SKINNY_TEXTID_SERVICES_URL = 127
};
extern struct skinny_table SKINNY_TEXTIDS[128];
const char *skinny_textid2str(uint32_t id);
uint32_t skinny_str2textid(const char *str);
#define SKINNY_PUSH_TEXTIDS SKINNY_DECLARE_PUSH_MATCH(SKINNY_TEXTIDS)


#endif /* _SKINNY_TABLES_H */

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

