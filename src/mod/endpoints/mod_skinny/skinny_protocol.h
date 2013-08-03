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
#ifndef _SKINNY_PROTOCOL_H
#define _SKINNY_PROTOCOL_H

#include <switch.h>
/* mod_skinny.h should be loaded first */
#include "mod_skinny.h"

/*****************************************************************************/
/* SKINNY TYPES */
/*****************************************************************************/
typedef enum {
	SKINNY_CODEC_NONE = 0,
	SKINNY_CODEC_NONSTANDARD = 1,
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
	SKINNY_CODEC_G722_1_32K = 40,
	SKINNY_CODEC_G722_1_24K = 41,
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
	SKINNY_CODEC_H264 = 103,
	SKINNY_CODEC_T120 = 105,
	SKINNY_CODEC_H224 = 106,
	SKINNY_CODEC_RFC2833_DYNPAYLOAD = 257
} skinny_codecs;

char* skinny_codec2string(skinny_codecs skinnycodec);

/*****************************************************************************/
/* SKINNY MESSAGE DATA */
/*****************************************************************************/

#define skinny_create_message(message,msgtype,field) \
    message = calloc(12 + sizeof(message->data.field), 1); \
    message->type = msgtype; \
    message->length = 4 + sizeof(message->data.field)

#define skinny_create_empty_message(message,msgtype) \
    message = calloc(12, 1); \
    message->type = msgtype; \
    message->length = 4


/* KeepAliveMessage */
#define KEEP_ALIVE_MESSAGE 0x0000

/* RegisterMessage */
#define REGISTER_MESSAGE 0x0001

#ifdef _MSC_VER
#pragma pack(push, r1, 1)
#endif

struct PACKED register_message {
	char device_name[16];
	uint32_t user_id;
	uint32_t instance;
	struct in_addr ip;
	uint32_t device_type;
	uint32_t max_streams;
};

/* PortMessage */
#define PORT_MESSAGE 0x0002
struct PACKED port_message {
	uint16_t port;
};

/* KeypadButtonMessage */
#define KEYPAD_BUTTON_MESSAGE 0x0003
struct PACKED keypad_button_message {
	uint32_t button;
	uint32_t line_instance;
	uint32_t call_id;
};

/* EnblocCallMessage */
#define ENBLOC_CALL_MESSAGE 0x0004
struct PACKED enbloc_call_message {
	char called_party[24];
	uint32_t line_instance;
};

/* StimulusMessage */
#define STIMULUS_MESSAGE 0x0005
struct PACKED stimulus_message {
	uint32_t instance_type; /* See enum skinny_button_definition */
	uint32_t instance;
	uint32_t call_id;
};

/* OffHookMessage */
#define OFF_HOOK_MESSAGE 0x0006
struct PACKED off_hook_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* OnHookMessage */
#define ON_HOOK_MESSAGE 0x0007
struct PACKED on_hook_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* ForwardStatReqMessage */
#define FORWARD_STAT_REQ_MESSAGE 0x0009
struct PACKED forward_stat_req_message {
	uint32_t line_instance;
};

/* SpeedDialStatReqMessage */
#define SPEED_DIAL_STAT_REQ_MESSAGE 0x000A
struct PACKED speed_dial_stat_req_message {
	uint32_t number;
};

/* LineStatReqMessage */
#define LINE_STAT_REQ_MESSAGE 0x000B
struct PACKED line_stat_req_message {
	uint32_t number;
};

/* ConfigStatReqMessage */
#define CONFIG_STAT_REQ_MESSAGE 0x000C

/* TimeDateReqMessage */
#define TIME_DATE_REQ_MESSAGE 0x000D

/* ButtonTemplateReqMessage */
#define BUTTON_TEMPLATE_REQ_MESSAGE 0x000E

/* VersionReqMessage */
#define VERSION_REQ_MESSAGE 0x000F

/* CapabilitiesResMessage */
#define CAPABILITIES_RES_MESSAGE 0x0010
struct PACKED station_capabilities {
	uint32_t codec;
	uint16_t max_frames_per_packet;
	char reserved[10];
};

struct PACKED capabilities_res_message {
	uint32_t count;
	struct station_capabilities caps[SWITCH_MAX_CODECS];
};

#define SERVER_REQ_MESSAGE 0x0012

/* AlarmMessage */
#define ALARM_MESSAGE 0x0020
struct PACKED alarm_message {
	uint32_t alarm_severity;
	char display_message[80];
	uint32_t alarm_param1;
	uint32_t alarm_param2;
};

/* OpenReceiveChannelAck */
#define OPEN_RECEIVE_CHANNEL_ACK_MESSAGE 0x0022
struct PACKED open_receive_channel_ack_message {
	uint32_t status;
	struct in_addr ip;
	uint32_t port;
	uint32_t pass_thru_party_id;
};

/* SoftKeySetReqMessage */
#define SOFT_KEY_SET_REQ_MESSAGE 0x0025

/* SoftKeyEventMessage */
#define SOFT_KEY_EVENT_MESSAGE 0x0026
struct PACKED soft_key_event_message {
	uint32_t event; /* See enum skinny_soft_key_event */
	uint32_t line_instance;
	uint32_t call_id;
};

/* UnregisterMessage */
#define UNREGISTER_MESSAGE 0x0027

/* SoftKeyTemplateReqMessage */
#define SOFT_KEY_TEMPLATE_REQ_MESSAGE 0x0028

/* HeadsetStatusMessage */
#define HEADSET_STATUS_MESSAGE 0x002B
struct PACKED headset_status_message {
	uint32_t mode; /* 1=HeadsetOn; 2=HeadsetOff */
};

/* RegisterAvailableLinesMessage */
#define REGISTER_AVAILABLE_LINES_MESSAGE 0x002D
struct PACKED register_available_lines_message {
	uint32_t count;
};

/* DeviceToUserDataMessage */
#define DEVICE_TO_USER_DATA_MESSAGE 0x002E
struct PACKED data_message {
	uint32_t application_id;
	uint32_t line_instance;
	uint32_t call_id;
	uint32_t transaction_id;
	uint32_t data_length;
	char data[1];
};

/* DeviceToUserDataResponseMessage */
#define DEVICE_TO_USER_DATA_RESPONSE_MESSAGE 0x002F
/* See struct PACKED data_message */

#define UPDATE_CAPABILITIES_MESSAGE 0x0030

#define MAX_CUSTOM_PICTURES						6
#define MAX_LAYOUT_WITH_SAME_SERVICE			5
#define MAX_SERVICE_TYPE						4
#define SKINNY_MAX_CAPABILITIES					18	/*!< max capabilities allowed in Cap response message */
#define SKINNY_MAX_VIDEO_CAPABILITIES			10
#define SKINNY_MAX_DATA_CAPABILITIES			5
#define MAX_LEVEL_PREFERENCE					4

/*!
 * \brief Picture Format Structure
 */
typedef struct {
	uint32_t custom_picture_format_width;                                      /*!< Picture Width */
	uint32_t custom_picture_format_height;                                     /*!< Picture Height */
	uint32_t custom_picture_format_pixelAspectRatio;                           /*!< Picture Pixel Aspect Ratio */
	uint32_t custom_picture_format_pixelclockConversionCode;                   /*!< Picture Pixel Conversion Code  */
	uint32_t custom_picture_format_pixelclockDivisor;                          /*!< Picture Pixel Divisor */
} custom_picture_format_t;


/*!
 * \brief Video Level Preference Structure
 */
typedef struct {
	uint32_t transmitPreference;                                            /*!< Transmit Preference */
	uint32_t format;                                                        /*!< Format / Codec */
	uint32_t maxBitRate;                                                    /*!< Maximum BitRate */
	uint32_t minBitRate;                                                    /*!< Minimum BitRate */
	uint32_t MPI;                                                           /*!<  */
	uint32_t serviceNumber;                                                 /*!< Service Number */
} levelPreference_t;                                                            /*!< Level Preference Structure */

/*! 
 * \brief Layout Config Structure (Update Capabilities Message Struct)
 * \since 20080111
 */
typedef struct {
	uint32_t layout;                                                        /*!< Layout \todo what is layout? */
} layoutConfig_t;                                                               /*!< Layout Config Structure */


/*!
 * \brief Service Resource Structure
 */
typedef struct {
	uint32_t layoutCount;                                                   /*!< Layout Count */
	layoutConfig_t layout[MAX_LAYOUT_WITH_SAME_SERVICE];                    /*!< Layout */
	uint32_t serviceNum;                                                    /*!< Service Number */
	uint32_t maxStreams;                                                    /*!< Maximum number of Streams */
	uint32_t maxConferences;                                                /*!< Maximum number of Conferences */
	uint32_t activeConferenceOnRegistration;                                /*!< Active Conference On Registration */
} serviceResource_t;



/*!
 * \brief Audio Capabilities Structure
 */
typedef struct {
	skinny_codecs payload_capability;                                   /*!< PayLoad Capability */
	uint32_t maxFramesPerPacket;                                        /*!< Maximum Number of Frames per IP Packet */
	uint32_t unknown[2];                                                /*!< this are related to G.723 */
} audioCap_t;  

/*!
 * \brief Video Capabilities Structure
 */
typedef struct {
	skinny_codecs payload_capability;                                   /*!< PayLoad Capability */
	uint32_t transmitOreceive;                                          /*!< Transmit of Receive */
	uint32_t levelPreferenceCount;                                      /*!< Level of Preference Count */

	levelPreference_t levelPreference[MAX_LEVEL_PREFERENCE];                /*!< Level Preference */

//      uint32_t codec_options[2];                                          /*!< Codec Options */

	union {
                struct {
                        uint32_t unknown1;
                        uint32_t unknown2;
                } h263;
                struct {
                        uint32_t profile;                                       /*!< H264 profile */
                        uint32_t level;                                         /*!< H264 level */
                } h264;
        } codec_options;

        /**
         * Codec options contains data specific for every codec
         *
         * Here is a list of known parameters per codec
        // H.261
        uint32_t                temporalSpatialTradeOffCapability;
        uint32_t                stillImageTransmission;

        // H.263
        uint32_t                h263_capability_bitfield;
        uint32_t                annexNandWFutureUse;

        // Video
        uint32_t                modelNumber;
        uint32_t                bandwidth;
        */
} videoCap_t;                                                                   /*!< Video Capabilities Structure */

/*!
 * \brief Data Capabilities Structure
 */
typedef struct {
	uint32_t payload_capability;                                             /*!< Payload Capability */
	uint32_t transmitOrReceive;                                             /*!< Transmit or Receive */
	uint32_t protocolDependentData;                                         /*!< Protocol Dependent Data */
	uint32_t maxBitRate;                                                    /*!< Maximum BitRate */
} dataCap_t;                                                                    /*!< Data Capabilities Structure */


struct PACKED update_capabilities_message {
	uint32_t audio_cap_count;                                     /*!< Audio Capability Count */
	uint32_t videoCapCount;                                     /*!< Video Capability Count */
	uint32_t dataCapCount;                                      /*!< Data Capability Count */
	uint32_t RTPPayloadFormat;                                      /*!< RTP Payload Format */
	uint32_t custom_picture_formatCount;                              /*!< Custom Picture Format Count */

	custom_picture_format_t custom_picture_format[MAX_CUSTOM_PICTURES]; /*!< Custom Picture Format */

	uint32_t activeStreamsOnRegistration;                           /*!< Active Streams on Registration */
	uint32_t maxBW;                                                 /*!< Max BW ?? */

	uint32_t serviceResourceCount;                                  /*!< Service Resource Count */
	serviceResource_t serviceResource[MAX_SERVICE_TYPE];            /*!< Service Resource */

	audioCap_t audioCaps[SKINNY_MAX_CAPABILITIES];                  /*!< Audio Capabilities */
	videoCap_t videoCaps[SKINNY_MAX_VIDEO_CAPABILITIES];            /*!< Video Capabilities */
	dataCap_t dataCaps[SKINNY_MAX_DATA_CAPABILITIES];               /*!< Data Capabilities */

	uint32_t unknown;                                               /*!< Unknown */
}; 


/* ServiceUrlStatReqMessage */
#define SERVICE_URL_STAT_REQ_MESSAGE 0x0033
struct PACKED service_url_stat_req_message {
	uint32_t service_url_index;
};

/* FeatureStatReqMessage */
#define FEATURE_STAT_REQ_MESSAGE 0x0034
struct PACKED feature_stat_req_message {
	uint32_t feature_index;
};

/* DeviceToUserDataVersion1Message */
#define DEVICE_TO_USER_DATA_VERSION1_MESSAGE 0x0041
struct PACKED extended_data_message {
	uint32_t application_id;
	uint32_t line_instance;
	uint32_t call_id;
	uint32_t transaction_id;
	uint32_t data_length;
	uint32_t sequence_flag;
	uint32_t display_priority;
	uint32_t conference_id;
	uint32_t app_instance_id;
	uint32_t routing_id;
	char data[1];
};

/* DeviceToUserDataResponseVersion1Message */
#define DEVICE_TO_USER_DATA_RESPONSE_VERSION1_MESSAGE 0x0042
/* See struct PACKED extended_data_message */

/* DialedPhoneBookMessage */
#define DIALED_PHONE_BOOK_MESSAGE 0x0048
struct PACKED dialed_phone_book_message {
	uint32_t number_index; /* must be shifted 4 bits right */
	uint32_t line_instance;
	uint32_t unknown;
	char phone_number[256];
};

/* AccessoryStatusMessage */
#define ACCESSORY_STATUS_MESSAGE 0x0049
struct PACKED accessory_status_message {
	uint32_t accessory_id;
	uint32_t accessory_status;
	uint32_t unknown;
};

/* RegisterAckMessage */
#define REGISTER_ACK_MESSAGE 0x0081
struct PACKED register_ack_message {
	uint32_t keep_alive;
	char date_format[6];
	char reserved[2];
	uint32_t secondary_keep_alive;
	char reserved2[4];
};

/* StartToneMessage */
#define START_TONE_MESSAGE 0x0082
struct PACKED start_tone_message {
	uint32_t tone; /* see enum skinny_tone */
	uint32_t reserved;
	uint32_t line_instance;
	uint32_t call_id;
};

/* StopToneMessage */
#define STOP_TONE_MESSAGE 0x0083
struct PACKED stop_tone_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* SetRingerMessage */
#define SET_RINGER_MESSAGE 0x0085
struct PACKED set_ringer_message {
	uint32_t ring_type; /* See enum skinny_ring_type */
	uint32_t ring_mode; /* See enum skinny_ring_mode */
	uint32_t line_instance;
	uint32_t call_id;
};

/* SetLampMessage */
#define SET_LAMP_MESSAGE 0x0086
struct PACKED set_lamp_message {
	uint32_t stimulus; /* See enum skinny_button_definition */
	uint32_t stimulus_instance;
	uint32_t mode; /* See enum skinny_lamp_mode */
};

/* SetSpeakerModeMessage */
#define SET_SPEAKER_MODE_MESSAGE 0x0088
struct PACKED set_speaker_mode_message {
	uint32_t mode; /* See enum skinny_speaker_mode */
};

/* StartMediaTransmissionMessage */
#define START_MEDIA_TRANSMISSION_MESSAGE 0x008A
struct PACKED start_media_transmission_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t remote_ip;
	uint32_t remote_port;
	uint32_t ms_per_packet;
	uint32_t payload_capacity;
	uint32_t precedence;
	uint32_t silence_suppression;
	uint16_t max_frames_per_packet;
	uint16_t unknown1;
	uint32_t g723_bitrate;
	/* ... */
};

/* StopMediaTransmissionMessage */
#define STOP_MEDIA_TRANSMISSION_MESSAGE 0x008B
struct PACKED stop_media_transmission_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t conference_id2;
	/* ... */
};

/* CallInfoMessage */
#define CALL_INFO_MESSAGE 0x008F
struct PACKED call_info_message {
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

/* ForwardStatMessage */
#define FORWARD_STAT_MESSAGE 0x0090
struct PACKED forward_stat_message {
	uint32_t active_forward;
	uint32_t line_instance;
	uint32_t forward_all_active;
	char forward_all_number[24];
	uint32_t forward_busy_active;
	char forward_busy_number[24];
	uint32_t forward_noanswer_active;
	char forward_noanswer_number[24];
};

/* SpeedDialStatMessage */
#define SPEED_DIAL_STAT_RES_MESSAGE 0x0091
struct PACKED speed_dial_stat_res_message {
	uint32_t number;
	char line[24];
	char label[40];
};

/* LineStatMessage */
#define LINE_STAT_RES_MESSAGE 0x0092
struct PACKED line_stat_res_message {
	uint32_t number;
	char name[24];
	char shortname[40];
	char displayname[44];
};

/* ConfigStatMessage */
#define CONFIG_STAT_RES_MESSAGE 0x0093
struct PACKED config_stat_res_message {
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
struct PACKED define_time_date_message {
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
struct PACKED button_definition {
	uint8_t instance_number;
	uint8_t button_definition; /* See enum skinny_button_definition */
};

#define SKINNY_MAX_BUTTON_COUNT 42
struct PACKED button_template_message {
	uint32_t button_offset;
	uint32_t button_count;
	uint32_t total_button_count;
	struct button_definition btn[SKINNY_MAX_BUTTON_COUNT];
};

/* VersionMessage */
#define VERSION_MESSAGE 0x0098
struct PACKED version_message {
	char version[16];
};

/* CapabilitiesReqMessage */
#define CAPABILITIES_REQ_MESSAGE 0x009B

/* RegisterRejectMessage */
#define REGISTER_REJECT_MESSAGE 0x009D
struct PACKED register_reject_message {
	char error[33];
};

#define SERVER_RESPONSE_MESSAGE 0x009E
#define ServerMaxNameSize                       48
#define StationMaxServers                       5
/*!
 * \brief Station Identifier Structure
 */
typedef struct {
	char serverName[ServerMaxNameSize];                                     /*!< Server Name */
} ServerIdentifier;

struct PACKED server_response_message {
	ServerIdentifier server[StationMaxServers];                     /*!< Server Identifier */
	uint32_t serverListenPort[StationMaxServers];                   /*!< Server is Listening on Port */
	uint32_t serverIpAddr[StationMaxServers];                       /*!< Server IP Port */
};                                                     /*!< Server Result Message Structure */


/* ResetMessage */
#define RESET_MESSAGE 0x009F
struct PACKED reset_message {
	uint32_t reset_type; /* See enum skinny_device_reset_types */
};

/* KeepAliveAckMessage */
#define KEEP_ALIVE_ACK_MESSAGE 0x0100

/* OpenReceiveChannelMessage */
#define OPEN_RECEIVE_CHANNEL_MESSAGE 0x0105
struct PACKED open_receive_channel_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t ms_per_packet;
	uint32_t payload_capacity;
	uint32_t echo_cancel_type;
	uint32_t g723_bitrate;
	uint32_t conference_id2;
	uint32_t reserved[10];
};

/* CloseReceiveChannelMessage */
#define CLOSE_RECEIVE_CHANNEL_MESSAGE 0x0106
struct PACKED close_receive_channel_message {
	uint32_t conference_id;
	uint32_t pass_thru_party_id;
	uint32_t conference_id2;
};

/* SoftKeyTemplateResMessage */
#define SOFT_KEY_TEMPLATE_RES_MESSAGE 0x0108

struct PACKED soft_key_template_definition {
	char soft_key_label[16];
	uint32_t soft_key_event;
};

struct PACKED soft_key_template_res_message {
	uint32_t soft_key_offset;
	uint32_t soft_key_count;
	uint32_t total_soft_key_count;
	struct soft_key_template_definition soft_key[32];
};

/* SoftKeySetResMessage */
#define SOFT_KEY_SET_RES_MESSAGE 0x0109
struct PACKED soft_key_set_definition {
	uint8_t soft_key_template_index[16]; /* See enum skinny_soft_key_event */
	uint16_t soft_key_info_index[16];
};

struct PACKED soft_key_set_res_message {
	uint32_t soft_key_set_offset;
	uint32_t soft_key_set_count;
	uint32_t total_soft_key_set_count;
	struct soft_key_set_definition soft_key_set[16];
	uint32_t res;
};

/* SelectSoftKeysMessage */
#define SELECT_SOFT_KEYS_MESSAGE 0x0110
struct PACKED select_soft_keys_message {
	uint32_t line_instance;
	uint32_t call_id;
	uint32_t soft_key_set; /* See enum skinny_key_set */
	uint32_t valid_key_mask;
};

/* CallStateMessage */
#define CALL_STATE_MESSAGE 0x0111
struct PACKED call_state_message {
	uint32_t call_state; /* See enum skinny_call_state */
	uint32_t line_instance;
	uint32_t call_id;
};

/* DisplayPromptStatusMessage */
#define DISPLAY_PROMPT_STATUS_MESSAGE 0x0112
struct PACKED display_prompt_status_message {
	uint32_t timeout;
	char display[32];
	uint32_t line_instance;
	uint32_t call_id;
};

/* ClearPromptStatusMessage */
#define CLEAR_PROMPT_STATUS_MESSAGE 0x0113
struct PACKED clear_prompt_status_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* ActivateCallPlaneMessage */
#define ACTIVATE_CALL_PLANE_MESSAGE 0x0116
struct PACKED activate_call_plane_message {
	uint32_t line_instance;
};

/* UnregisterAckMessage */
#define UNREGISTER_ACK_MESSAGE 0x0118
struct PACKED unregister_ack_message {
	uint32_t unregister_status;
};

/* BackSpaceReqMessage */
#define BACK_SPACE_REQ_MESSAGE 0x0119
struct PACKED back_space_req_message {
	uint32_t line_instance;
	uint32_t call_id;
};

/* DialedNumberMessage */
#define DIALED_NUMBER_MESSAGE 0x011D
struct PACKED dialed_number_message {
	char called_party[24];
	uint32_t line_instance;
	uint32_t call_id;
};

/* UserToDeviceDataMessage */
#define USER_TO_DEVICE_DATA_MESSAGE 0x011E
/* See struct PACKED data_message */

/* FeatureStatMessage */
#define FEATURE_STAT_RES_MESSAGE 0x011F
struct PACKED feature_stat_res_message {
	uint32_t index;
	uint32_t id;
	char text_label[40];
	uint32_t status;
};

/* DisplayPriNotifyMessage */
#define DISPLAY_PRI_NOTIFY_MESSAGE 0x0120
struct PACKED display_pri_notify_message {
	uint32_t message_timeout;
	uint32_t priority;
	char notify[32];
};

/* ServiceUrlStatMessage */
#define SERVICE_URL_STAT_RES_MESSAGE 0x012F
struct PACKED service_url_stat_res_message {
	uint32_t index;
	char url[256];
	char display_name[40];
};

/* UserToDeviceDataVersion1Message */
#define USER_TO_DEVICE_DATA_VERSION1_MESSAGE 0x013F
/* See struct PACKED extended_data_message */

/* DialedPhoneBookAckMessage */
#define DIALED_PHONE_BOOK_ACK_MESSAGE 0x0152
struct PACKED dialed_phone_book_ack_message {
	uint32_t number_index; /* must be shifted 4 bits right */
	uint32_t line_instance;
	uint32_t unknown;
	uint32_t unknown2;
};

#ifdef _MSC_VER
#pragma pack(pop, r1)
#endif

/* XMLAlarmMessage */
#define XML_ALARM_MESSAGE 0x015A

/*****************************************************************************/
/* SKINNY MESSAGE */
/*****************************************************************************/
#define SKINNY_MESSAGE_FIELD_SIZE 4 /* 4-bytes field */
#define SKINNY_MESSAGE_HEADERSIZE 12 /* three 4-bytes fields */
#define SKINNY_MESSAGE_MAXSIZE 2048

union skinny_data {
	/* no data for KEEP_ALIVE_MESSAGE */
	struct register_message reg;
	struct port_message port;
	struct keypad_button_message keypad_button;
	struct enbloc_call_message enbloc_call;
	struct stimulus_message stimulus;
	struct off_hook_message off_hook;
	struct on_hook_message on_hook;
	struct forward_stat_req_message forward_stat_req;
	struct speed_dial_stat_req_message speed_dial_req;
	struct line_stat_req_message line_req;
	/* no data for CONFIG_STAT_REQ_MESSAGE */
	/* no data for TIME_DATE_REQ_MESSAGE */
	/* no data for BUTTON_TEMPLATE_REQ_MESSAGE */
	/* no data for VERSION_REQ_MESSAGE */
	struct capabilities_res_message cap_res;
	struct alarm_message alarm;
	struct open_receive_channel_ack_message open_receive_channel_ack;
	/* no data for SOFT_KEY_SET_REQ_MESSAGE */
	struct soft_key_event_message soft_key_event;
	/* no data for UNREGISTER_MESSAGE */
	/* no data for SOFT_KEY_TEMPLATE_REQ_MESSAGE */
	struct headset_status_message headset_status;
	struct register_available_lines_message reg_lines;
	/* see field "data" for DEVICE_TO_USER_DATA_MESSAGE */
	/* see field "data" for DEVICE_TO_USER_DATA_RESPONSE_MESSAGE */
	struct service_url_stat_req_message service_url_req;
	struct feature_stat_req_message feature_req;
	/* see field "extended_data" for DEVICE_TO_USER_DATA_VERSION1_MESSAGE */
	/* see field "extended_data" for DEVICE_TO_USER_DATA_RESPONSE_VERSION1_MESSAGE */
	struct dialed_phone_book_message dialed_phone_book;
	struct accessory_status_message accessory_status;
	struct register_ack_message reg_ack;
	struct start_tone_message start_tone;
	struct stop_tone_message stop_tone;
	struct set_ringer_message ringer;
	struct set_lamp_message lamp;
	struct set_speaker_mode_message speaker_mode;
	struct start_media_transmission_message start_media;
	struct stop_media_transmission_message stop_media;
	struct call_info_message call_info;
	struct forward_stat_message forward_stat;
	struct speed_dial_stat_res_message speed_dial_res;
	struct line_stat_res_message line_res;
	struct config_stat_res_message config_res;
	struct define_time_date_message define_time_date;
	struct button_template_message button_template;
	struct version_message version;
	/* no data for CAPABILITIES_REQ_MESSAGE */
	struct register_reject_message reg_rej;
	struct reset_message reset;
	struct server_response_message serv_res_mess;
	/* no data for KEEP_ALIVE_ACK_MESSAGE */
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
	struct back_space_req_message back_space_req;
	struct dialed_number_message dialed_number;
	/* see field "data" for USER_TO_DEVICE_DATA_MESSAGE */
	struct feature_stat_res_message feature_res;
	struct display_pri_notify_message display_pri_notify;
	struct service_url_stat_res_message service_url_res;
	/* see field "extended_data" for USER_TO_DEVICE_DATA_VERSION1_MESSAGE */
	struct dialed_phone_book_ack_message dialed_phone_book_ack;

	struct update_capabilities_message upd_cap;
	struct data_message data;
	struct extended_data_message extended_data;

	uint16_t as_uint16;
	char as_char[1];
};

#ifdef _MSC_VER
#pragma pack(push, r1, 1)
#endif

/*
 * header is length+version
 * body is type+data
 * length is length of body
 */
struct PACKED skinny_message {
	uint32_t length;
	uint32_t version;
	uint32_t type;
	union skinny_data data;
};

#ifdef _MSC_VER
#pragma pack(pop, r1)
#endif

typedef struct skinny_message skinny_message_t;



/*****************************************************************************/
/* SKINNY FUNCTIONS */
/*****************************************************************************/
#define skinny_check_data_length(message, len) \
	if (message->length < len+4) {\
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,\
				"Received Too Short Skinny Message %s (type=%x,length=%d), expected %" SWITCH_SIZE_T_FMT ".\n",\
				skinny_message_type2str(request->type), request->type, request->length,\
				len+4);\
		return SWITCH_STATUS_FALSE;\
	}
#define skinny_check_data_length_soft(message, len) \
	(message->length >= len+4)

switch_status_t skinny_read_packet(listener_t *listener, skinny_message_t **req);

switch_status_t skinny_device_event(listener_t *listener, switch_event_t **ev, switch_event_types_t event_id, const char *subclass_name);

switch_status_t skinny_session_walk_lines(skinny_profile_t *profile, char *channel_uuid, switch_core_db_callback_func_t callback, void *data);

void skinny_line_get(listener_t *listener, uint32_t instance, struct line_stat_res_message **button);
void skinny_speed_dial_get(listener_t *listener, uint32_t instance, struct speed_dial_stat_res_message **button);
void skinny_service_url_get(listener_t *listener, uint32_t instance, struct service_url_stat_res_message **button);
void skinny_feature_get(listener_t *listener, uint32_t instance, struct feature_stat_res_message **button);

switch_status_t skinny_perform_send_reply(listener_t *listener, const char *file, const char *func, int line, skinny_message_t *reply, switch_bool_t discard);
#define  skinny_send_reply(listener, reply, discard)  skinny_perform_send_reply(listener, __FILE__, __SWITCH_FUNC__, __LINE__, reply, discard)

switch_status_t skinny_perform_send_reply_quiet(listener_t *listener, const char *file, const char *func, int line, skinny_message_t *reply, switch_bool_t discard);
#define  skinny_send_reply_quiet(listener, reply, discard)  skinny_perform_send_reply_quiet(listener, __FILE__, __SWITCH_FUNC__, __LINE__, reply, discard)

switch_status_t skinny_handle_request(listener_t *listener, skinny_message_t *request);

/*****************************************************************************/
/* SKINNY MESSAGE HELPER */
/*****************************************************************************/
switch_status_t perform_send_keep_alive_ack(listener_t *listener,
		const char *file, const char *func, int line);
#define send_keep_alive_ack(listener) perform_send_keep_alive_ack(listener, __FILE__, __SWITCH_FUNC__, __LINE__);

switch_status_t perform_send_register_ack(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t keep_alive,
		char *date_format,
		char *reserved,
		uint32_t secondary_keep_alive,
		char *reserved2);
#define send_register_ack(listener, ...) perform_send_register_ack(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_speed_dial_stat_res(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t number,
		char *speed_line,
		char *speed_label);
#define send_speed_dial_stat_res(listener, ...) perform_send_speed_dial_stat_res(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_start_tone(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t tone,
		uint32_t reserved,
		uint32_t line_instance,
		uint32_t call_id);
#define send_start_tone(listener, ...) perform_send_start_tone(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_stop_tone(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t line_instance,
		uint32_t call_id);
#define send_stop_tone(listener, ...) perform_send_stop_tone(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_set_ringer(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t ring_type,
		uint32_t ring_mode,
		uint32_t line_instance,
		uint32_t call_id);
#define send_set_ringer(listener, ...) perform_send_set_ringer(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_set_lamp(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t stimulus,
		uint32_t stimulus_instance,
		uint32_t mode);
#define send_set_lamp(listener, ...) perform_send_set_lamp(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_set_speaker_mode(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t mode);
#define send_set_speaker_mode(listener, ...) perform_send_set_speaker_mode(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_srvreq_response(listener_t *listener,
		const char *file, const char *func, int line,
		char *ip, uint32_t port);
#define send_srvreq_response(listener, ...) perform_send_srvreq_response(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_start_media_transmission(listener_t *listener,
		const char *file, const char *func, int line,
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
#define send_start_media_transmission(listener,...) perform_send_start_media_transmission(listener,__FILE__, __SWITCH_FUNC__, __LINE__,__VA_ARGS__)

switch_status_t perform_send_stop_media_transmission(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t conference_id,
		uint32_t pass_thru_party_id,
		uint32_t conference_id2);
#define send_stop_media_transmission(listener, ...) perform_send_stop_media_transmission(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_call_info(listener_t *listener,
		const char *file, const char *func, int line,
		const char *calling_party_name,
		const char *calling_party,
		const char *called_party_name,
		const char *called_party,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t call_type,
		const char *original_called_party_name,
		const char *original_called_party,
		const char *last_redirecting_party_name,
		const char *last_redirecting_party,
		uint32_t original_called_party_redirect_reason,
		uint32_t last_redirecting_reason,
		const char *calling_party_voice_mailbox,
		const char *called_party_voice_mailbox,
		const char *original_called_party_voice_mailbox,
		const char *last_redirecting_voice_mailbox,
		uint32_t call_instance,
		uint32_t call_security_status,
		uint32_t party_pi_restriction_bits);
#define send_call_info(listener, ...) perform_send_call_info(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_define_time_date(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t year,
		uint32_t month,
		uint32_t day_of_week, /* monday = 1 */
		uint32_t day,
		uint32_t hour,
		uint32_t minute,
		uint32_t seconds,
		uint32_t milliseconds,
		uint32_t timestamp);
#define send_define_time_date(listener, ...) perform_send_define_time_date(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_define_current_time_date(listener_t *listener,
		const char *file, const char *func, int line);
#define send_define_current_time_date(listener) perform_send_define_current_time_date(listener, __FILE__, __SWITCH_FUNC__, __LINE__)

switch_status_t perform_send_version(listener_t *listener,
		const char *file, const char *func, int line,
		char *version);
#define send_version(listener, ...) perform_send_version(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_capabilities_req(listener_t *listener,
		const char *file, const char *func, int line);
#define send_capabilities_req(listener) perform_send_capabilities_req(listener, __FILE__, __SWITCH_FUNC__, __LINE__)

switch_status_t perform_send_register_reject(listener_t *listener,
		const char *file, const char *func, int line,
		char *error);
#define send_register_reject(listener, ...) perform_send_register_reject(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_open_receive_channel(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t conference_id,
		uint32_t pass_thru_party_id,
		uint32_t ms_per_packet,
		uint32_t payload_capacity,
		uint32_t echo_cancel_type,
		uint32_t g723_bitrate,
		uint32_t conference_id2,
		uint32_t reserved[10]);
#define send_open_receive_channel(listener, ...) perform_send_open_receive_channel(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_close_receive_channel(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t conference_id,
		uint32_t pass_thru_party_id,
		uint32_t conference_id2);
#define send_close_receive_channel(listener, ...) perform_send_close_receive_channel(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_select_soft_keys(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t soft_key_set,
		uint32_t valid_key_mask);
#define send_select_soft_keys(listener, ...) perform_send_select_soft_keys(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_call_state(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t call_state,
		uint32_t line_instance,
		uint32_t call_id);
#define send_call_state(listener, ...) perform_send_call_state(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_display_prompt_status(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t timeout,
		const char *display,
		uint32_t line_instance,
		uint32_t call_id);
#define send_display_prompt_status(listener, ...) perform_send_display_prompt_status(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_display_prompt_status_textid(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t timeout,
		uint32_t display_textid,
		uint32_t line_instance,
		uint32_t call_id);
#define send_display_prompt_status_textid(listener, ...) perform_send_display_prompt_status_textid(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_clear_prompt_status(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t line_instance,
		uint32_t call_id);
#define send_clear_prompt_status(listener, ...) perform_send_clear_prompt_status(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_activate_call_plane(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t line_instance);
#define send_activate_call_plane(listener, ...) perform_send_activate_call_plane(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_back_space_request(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t line_instance,
		uint32_t call_id);
#define send_back_space_request(listener, ...) perform_send_back_space_request(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_dialed_number(listener_t *listener,
		const char *file, const char *func, int line,
		char called_party[24],
		uint32_t line_instance,
		uint32_t call_id);
#define send_dialed_number(listener, ...) perform_send_dialed_number(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_display_pri_notify(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t message_timeout,
		uint32_t priority,
		char *notify);
#define send_display_pri_notify(listener, ...) perform_send_display_pri_notify(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_reset(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t reset_type);
#define send_reset(listener, ...) perform_send_reset(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_data(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t message_type,
		uint32_t application_id,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t transaction_id,
		uint32_t data_length,
		const char *data);
#define send_data(listener, ...) perform_send_data(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

switch_status_t perform_send_extended_data(listener_t *listener,
		const char *file, const char *func, int line,
		uint32_t message_type,
		uint32_t application_id,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t transaction_id,
		uint32_t data_length,
		uint32_t sequence_flag,
		uint32_t display_priority,
		uint32_t conference_id,
		uint32_t app_instance_id,
		uint32_t routing_id,
		const char *data);
#define send_extended_data(listener, ...) perform_send_extended_data(listener, __FILE__, __SWITCH_FUNC__, __LINE__, __VA_ARGS__)

#endif /* _SKINNY_PROTOCOL_H */

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

