/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Bret McDanel <trixter AT 0xdecafbad dot com>
 *
 * switch_types.h -- Data Types
 *
 */
/*! \file switch_types.h
    \brief Data Types
*/
#ifndef SWITCH_TYPES_H
#define SWITCH_TYPES_H

#include <switch.h>
SWITCH_BEGIN_EXTERN_C
#define SWITCH_BLANK_STRING ""
#ifdef WIN32
#define SWITCH_SEQ_FWHITE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define SWITCH_SEQ_FRED FOREGROUND_RED | FOREGROUND_INTENSITY
#define SWITCH_SEQ_FMAGEN FOREGROUND_BLUE | FOREGROUND_RED
#define SWITCH_SEQ_FCYAN FOREGROUND_GREEN | FOREGROUND_BLUE
#define SWITCH_SEQ_FGREEN FOREGROUND_GREEN
#define SWITCH_SEQ_FYELLOW FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define SWITCH_SEQ_DEFAULT_COLOR SWITCH_SEQ_FWHITE
#else
#define SWITCH_SEQ_ESC "\033["
/* Ansi Control character suffixes */
#define SWITCH_SEQ_HOME_CHAR 'H'
#define SWITCH_SEQ_HOME_CHAR_STR "H"
#define SWITCH_SEQ_CLEARLINE_CHAR '1'
#define SWITCH_SEQ_CLEARLINE_CHAR_STR "1"
#define SWITCH_SEQ_CLEARLINEEND_CHAR "K"
#define SWITCH_SEQ_CLEARSCR_CHAR0 '2'
#define SWITCH_SEQ_CLEARSCR_CHAR1 'J'
#define SWITCH_SEQ_CLEARSCR_CHAR "2J"
#define SWITCH_SEQ_DEFAULT_COLOR SWITCH_SEQ_ESC SWITCH_SEQ_END_COLOR	/* Reset to Default fg/bg color */
#define SWITCH_SEQ_AND_COLOR ";"	/* To add multiple color definitions */
#define SWITCH_SEQ_END_COLOR "m"	/* To end color definitions */
/* Foreground colors values */
#define SWITCH_SEQ_F_BLACK "30"
#define SWITCH_SEQ_F_RED "31"
#define SWITCH_SEQ_F_GREEN "32"
#define SWITCH_SEQ_F_YELLOW "33"
#define SWITCH_SEQ_F_BLUE "34"
#define SWITCH_SEQ_F_MAGEN "35"
#define SWITCH_SEQ_F_CYAN "36"
#define SWITCH_SEQ_F_WHITE "37"
/* Background colors values */
#define SWITCH_SEQ_B_BLACK "40"
#define SWITCH_SEQ_B_RED "41"
#define SWITCH_SEQ_B_GREEN "42"
#define SWITCH_SEQ_B_YELLOW "43"
#define SWITCH_SEQ_B_BLUE "44"
#define SWITCH_SEQ_B_MAGEN "45"
#define SWITCH_SEQ_B_CYAN "46"
#define SWITCH_SEQ_B_WHITE "47"
/* Preset escape sequences - Change foreground colors only */
#define SWITCH_SEQ_FBLACK SWITCH_SEQ_ESC SWITCH_SEQ_F_BLACK SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_FRED SWITCH_SEQ_ESC SWITCH_SEQ_F_RED SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_FGREEN SWITCH_SEQ_ESC SWITCH_SEQ_F_GREEN SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_FYELLOW SWITCH_SEQ_ESC SWITCH_SEQ_F_YELLOW SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_FBLUE SWITCH_SEQ_ESC SWITCH_SEQ_F_BLUE SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_FMAGEN SWITCH_SEQ_ESC SWITCH_SEQ_F_MAGEN SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_FCYAN SWITCH_SEQ_ESC SWITCH_SEQ_F_CYAN SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_FWHITE SWITCH_SEQ_ESC SWITCH_SEQ_F_WHITE SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BBLACK SWITCH_SEQ_ESC SWITCH_SEQ_B_BLACK SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BRED SWITCH_SEQ_ESC SWITCH_SEQ_B_RED SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BGREEN SWITCH_SEQ_ESC SWITCH_SEQ_B_GREEN SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BYELLOW SWITCH_SEQ_ESC SWITCH_SEQ_B_YELLOW SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BBLUE SWITCH_SEQ_ESC SWITCH_SEQ_B_BLUE SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BMAGEN SWITCH_SEQ_ESC SWITCH_SEQ_B_MAGEN SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BCYAN SWITCH_SEQ_ESC SWITCH_SEQ_B_CYAN SWITCH_SEQ_END_COLOR
#define SWITCH_SEQ_BWHITE SWITCH_SEQ_ESC SWITCH_SEQ_B_WHITE SWITCH_SEQ_END_COLOR
/* Preset escape sequences */
#define SWITCH_SEQ_HOME SWITCH_SEQ_ESC SWITCH_SEQ_HOME_CHAR_STR
#define SWITCH_SEQ_CLEARLINE SWITCH_SEQ_ESC SWITCH_SEQ_CLEARLINE_CHAR_STR
#define SWITCH_SEQ_CLEARLINEEND SWITCH_SEQ_ESC SWITCH_SEQ_CLEARLINEEND_CHAR
#define SWITCH_SEQ_CLEARSCR SWITCH_SEQ_ESC SWITCH_SEQ_CLEARSCR_CHAR SWITCH_SEQ_HOME
#endif
#define SWITCH_DEFAULT_DTMF_DURATION 2000
#define SWITCH_MAX_DTMF_DURATION 192000
#define SWITCH_DEFAULT_DIR_PERMS SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE | SWITCH_FPROT_UEXECUTE | SWITCH_FPROT_GREAD | SWITCH_FPROT_GEXECUTE
#ifdef WIN32
#define SWITCH_PATH_SEPARATOR "\\"
#else
#define SWITCH_PATH_SEPARATOR "/"
#endif
#define SWITCH_URL_SEPARATOR "://"
#define SWITCH_ENABLE_HEARTBEAT_EVENTS_VARIABLE "enable_heartbeat_events"
#define SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE "bypass_media_after_bridge"
#define SWITCH_READ_RESULT_VARIABLE "read_result"
#define SWITCH_COPY_XML_CDR_VARIABLE "copy_xml_cdr"
#define SWITCH_CURRENT_APPLICATION_VARIABLE "current_application"
#define SWITCH_PROTO_SPECIFIC_HANGUP_CAUSE_VARIABLE "proto_specific_hangup_cause"
#define SWITCH_CHANNEL_EXECUTE_ON_ANSWER_VARIABLE "execute_on_answer"
#define SWITCH_CHANNEL_EXECUTE_ON_RING_VARIABLE "execute_on_ring"
#define SWITCH_CALL_TIMEOUT_VARIABLE "call_timeout"
#define SWITCH_HOLDING_UUID_VARIABLE "holding_uuid"
#define SWITCH_API_BRIDGE_END_VARIABLE "api_after_bridge"
#define SWITCH_API_HANGUP_HOOK_VARIABLE "api_hangup_hook"
#define SWITCH_PROCESS_CDR_VARIABLE "process_cdr"
#define SWITCH_BRIDGE_CHANNEL_VARIABLE "bridge_channel"
#define SWITCH_CHANNEL_NAME_VARIABLE "channel_name"
#define SWITCH_BRIDGE_UUID_VARIABLE "bridge_uuid"
#define SWITCH_CONTINUE_ON_FAILURE_VARIABLE "continue_on_fail"
#define SWITCH_PLAYBACK_TERMINATORS_VARIABLE "playback_terminators"
#define SWITCH_PLAYBACK_TERMINATOR_USED "playback_terminator_used"
#define SWITCH_CACHE_SPEECH_HANDLES_VARIABLE "cache_speech_handles"
#define SWITCH_CACHE_SPEECH_HANDLES_OBJ_NAME "__cache_speech_handles_obj__"
#define SWITCH_BYPASS_MEDIA_VARIABLE "bypass_media"
#define SWITCH_PROXY_MEDIA_VARIABLE "proxy_media"
#define SWITCH_ENDPOINT_DISPOSITION_VARIABLE "endpoint_disposition"
#define SWITCH_HOLD_MUSIC_VARIABLE "hold_music"
#define SWITCH_EXPORT_VARS_VARIABLE "export_vars"
#define SWITCH_R_SDP_VARIABLE "switch_r_sdp"
#define SWITCH_L_SDP_VARIABLE "switch_l_sdp"
#define SWITCH_B_SDP_VARIABLE "switch_m_sdp"
#define SWITCH_BRIDGE_VARIABLE "bridge_to"
#define SWITCH_SIGNAL_BRIDGE_VARIABLE "signal_bridge_to"
#define SWITCH_SIGNAL_BOND_VARIABLE "signal_bond"
#define SWITCH_ORIGINATOR_VARIABLE "originator"
#define SWITCH_ORIGINATOR_CODEC_VARIABLE "originator_codec"
#define SWITCH_LOCAL_MEDIA_IP_VARIABLE "local_media_ip"
#define SWITCH_LOCAL_MEDIA_PORT_VARIABLE "local_media_port"
#define SWITCH_REMOTE_MEDIA_IP_VARIABLE "remote_media_ip"
#define SWITCH_REMOTE_MEDIA_PORT_VARIABLE "remote_media_port"
#define SWITCH_REMOTE_VIDEO_IP_VARIABLE "remote_video_ip"
#define SWITCH_REMOTE_VIDEO_PORT_VARIABLE "remote_video_port"
#define SWITCH_LOCAL_VIDEO_IP_VARIABLE "local_video_ip"
#define SWITCH_LOCAL_VIDEO_PORT_VARIABLE "local_video_port"
#define SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE "hangup_after_bridge"
#define SWITCH_PARK_AFTER_BRIDGE_VARIABLE "park_after_bridge"
#define SWITCH_EXEC_AFTER_BRIDGE_APP_VARIABLE "exec_after_bridge_app"
#define SWITCH_EXEC_AFTER_BRIDGE_ARG_VARIABLE "exec_after_bridge_arg"
#define SWITCH_MAX_FORWARDS_VARIABLE "max_forwards"
#define SWITCH_DISABLE_APP_LOG_VARIABLE "disable_app_log"
#define SWITCH_SPEECH_KEY "speech"
#define SWITCH_UUID_BRIDGE "uuid_bridge"
#define SWITCH_BITS_PER_BYTE 8
	typedef uint8_t switch_byte_t;

typedef struct {
	char digit;
	uint32_t duration;
} switch_dtmf_t;

typedef enum {
	SBF_DIAL_ALEG = (1 << 0),
	SBF_EXEC_ALEG = (1 << 1),
	SBF_DIAL_BLEG = (1 << 2),
	SBF_EXEC_BLEG = (1 << 3),
	SBF_EXEC_OPPOSITE = (1 << 4),
	SBF_EXEC_SAME = (1 << 5)
} switch_bind_flag_enum_t;
typedef uint32_t switch_bind_flag_t;

typedef enum {
	SWITCH_DTMF_RECV = 0,
	SWITCH_DTMF_SEND = 1
} switch_dtmf_direction_t;

typedef enum {
	SOF_NONE = 0,
	SOF_NOBLOCK = (1 << 0),
	SOF_FORKED_DIAL = (1 << 1),
	SOF_NO_EFFECTIVE_CID_NUM = (1 << 2),
	SOF_NO_EFFECTIVE_CID_NAME = (1 << 3)
} switch_originate_flag_enum_t;
typedef uint32_t switch_originate_flag_t;

typedef enum {
	SPF_NONE = 0,
	SPF_ODD = (1 << 0),
	SPF_EVEN = (1 << 1)
} switch_port_flag_enum_t;
typedef uint32_t switch_port_flag_t;

typedef enum {
	ED_MUX_READ = (1 << 0),
	ED_MUX_WRITE = (1 << 1),
	ED_DTMF = (1 << 2)
} switch_eavesdrop_flag_enum_t;
typedef uint32_t switch_eavesdrop_flag_t;

typedef enum {
	SCF_NONE = 0,
	SCF_USE_SQL = (1 << 0),
	SCF_NO_NEW_SESSIONS = (1 << 1),
	SCF_SHUTTING_DOWN = (1 << 2),
	SCF_CRASH_PROT = (1 << 3),
	SCF_VG = (1 << 4),
	SCF_RESTART = (1 << 5),
	SCF_SHUTDOWN_REQUESTED = (1 << 6)
} switch_core_flag_enum_t;
typedef uint32_t switch_core_flag_t;

typedef enum {
	SWITCH_ENDPOINT_INTERFACE,
	SWITCH_TIMER_INTERFACE,
	SWITCH_DIALPLAN_INTERFACE,
	SWITCH_CODEC_INTERFACE,
	SWITCH_APPLICATION_INTERFACE,
	SWITCH_API_INTERFACE,
	SWITCH_FILE_INTERFACE,
	SWITCH_SPEECH_INTERFACE,
	SWITCH_DIRECTORY_INTERFACE,
	SWITCH_CHAT_INTERFACE,
	SWITCH_SAY_INTERFACE,
	SWITCH_ASR_INTERFACE,
	SWITCH_MANAGEMENT_INTERFACE
} switch_module_interface_name_t;

typedef enum {
	SUF_NONE = 0,
	SUF_THREAD_RUNNING = (1 << 0),
	SUF_READY = (1 << 1),
	SUF_NATIVE = (1 << 2)
} switch_unicast_flag_enum_t;
typedef uint32_t switch_unicast_flag_t;

typedef enum {
	SWITCH_FALSE = 0,
	SWITCH_TRUE = 1
} switch_bool_t;

typedef enum {
	SSM_NA,
	SSM_PRONOUNCED,
	SSM_ITERATED,
	SSM_COUNTED
} switch_say_method_t;

typedef enum {
	SST_NUMBER,
	SST_ITEMS,
	SST_PERSONS,
	SST_MESSAGES,
	SST_CURRENCY,
	SST_TIME_MEASUREMENT,
	SST_CURRENT_DATE,
	SST_CURRENT_TIME,
	SST_CURRENT_DATE_TIME,
	SST_TELEPHONE_NUMBER,
	SST_TELEPHONE_EXTENSION,
	SST_URL,
	SST_IP_ADDRESS,
	SST_EMAIL_ADDRESS,
	SST_POSTAL_ADDRESS,
	SST_ACCOUNT_NUMBER,
	SST_NAME_SPELLED,
	SST_NAME_PHONETIC
} switch_say_type_t;

typedef enum {
	SMA_NONE,
	SMA_GET,
	SMA_SET
} switch_management_action_t;

typedef enum {
	SSHF_NONE = 0,
	SSHF_OWN_THREAD = (1 << 0),
	SSHF_FREE_ARG = (1 << 1),
	SSHF_NO_DEL = (1 << 2)
} switch_scheduler_flag_enum_t;
typedef uint32_t switch_scheduler_flag_t;

typedef enum {
	SMF_NONE = 0,
	SMF_REBRIDGE = (1 << 0),
	SMF_ECHO_ALEG = (1 << 1),
	SMF_ECHO_BLEG = (1 << 2),
	SMF_FORCE = (1 << 3),
	SMF_LOOP = (1 << 4),
	SMF_HOLD_BLEG = (1 << 5),
	SMF_IMMEDIATE = (1 << 6)
} switch_media_flag_enum_t;
typedef uint32_t switch_media_flag_t;

typedef enum {
	SWITCH_BITPACK_MODE_RFC3551,
	SWITCH_BITPACK_MODE_AAL2
} switch_bitpack_mode_t;

typedef enum {
	SWITCH_ABC_TYPE_INIT,
	SWITCH_ABC_TYPE_READ,
	SWITCH_ABC_TYPE_WRITE,
	SWITCH_ABC_TYPE_WRITE_REPLACE,
	SWITCH_ABC_TYPE_READ_REPLACE,
	SWITCH_ABC_TYPE_READ_PING,
	SWITCH_ABC_TYPE_CLOSE
} switch_abc_type_t;

typedef struct {
	switch_byte_t *buf;
	uint32_t buflen;
	switch_byte_t *cur;
	uint32_t bytes;
	uint32_t bits_tot;
	switch_byte_t bits_cur;
	switch_byte_t bits_rem;
	switch_byte_t frame_bits;
	switch_byte_t shiftby;
	switch_byte_t this_byte;
	switch_byte_t under;
	switch_byte_t over;
	switch_bitpack_mode_t mode;
} switch_bitpack_t;


struct switch_directories {
	char *base_dir;
	char *mod_dir;
	char *conf_dir;
	char *log_dir;
	char *db_dir;
	char *script_dir;
	char *temp_dir;
	char *htdocs_dir;
	char *grammar_dir;
	char *storage_dir;
};

typedef struct switch_directories switch_directories;
SWITCH_DECLARE_DATA extern switch_directories SWITCH_GLOBAL_dirs;

#define SWITCH_MAX_STACKS 16
#define SWITCH_THREAD_STACKSIZE 240 * 1024
#define SWITCH_SYSTEM_THREAD_STACKSIZE 8192 * 1024
#define SWITCH_MAX_INTERVAL 120	/* we only do up to 120ms */
#define SWITCH_INTERVAL_PAD 10	/* A little extra buffer space to be safe */
#define SWITCH_MAX_SAMPLE_LEN 32
#define SWITCH_BYTES_PER_SAMPLE 2	/* slin is 2 bytes per sample */
#define SWITCH_RECOMMENDED_BUFFER_SIZE (SWITCH_BYTES_PER_SAMPLE * SWITCH_MAX_SAMPLE_LEN * (SWITCH_MAX_INTERVAL + SWITCH_INTERVAL_PAD))
#define SWITCH_MAX_CODECS 50
#define SWITCH_MAX_STATE_HANDLERS 30
#define SWITCH_CORE_QUEUE_LEN 100000
#define SWITCH_MAX_MANAGEMENT_BUFFER_LEN 1024 * 8

#define SWITCH_ACCEPTABLE_INTERVAL(_i) (_i && _i <= SWITCH_MAX_INTERVAL && (_i % 10) == 0)

typedef enum {
	SWITCH_CPF_SCREEN = (1 << 0),
	SWITCH_CPF_HIDE_NAME = (1 << 1),
	SWITCH_CPF_HIDE_NUMBER = (1 << 2)
} switch_caller_profile_flag_enum_t;
typedef uint32_t switch_caller_profile_flag_t;

typedef enum {
	SWITCH_AUDIO_COL_STR_TITLE = 0x01,
	SWITCH_AUDIO_COL_STR_COPYRIGHT = 0x02,
	SWITCH_AUDIO_COL_STR_SOFTWARE = 0x03,
	SWITCH_AUDIO_COL_STR_ARTIST = 0x04,
	SWITCH_AUDIO_COL_STR_COMMENT = 0x05,
	SWITCH_AUDIO_COL_STR_DATE = 0x06
} switch_audio_col_t;

typedef enum {
	SWITCH_XML_SECTION_RESULT = 0,
	SWITCH_XML_SECTION_CONFIG = (1 << 0),
	SWITCH_XML_SECTION_DIRECTORY = (1 << 1),
	SWITCH_XML_SECTION_DIALPLAN = (1 << 2),
	SWITCH_XML_SECTION_PHRASES = (1 << 3)
} switch_xml_section_enum_t;
typedef uint32_t switch_xml_section_t;

/*!
  \enum switch_vad_flag_t
  \brief RTP Related Flags
<pre>
    SWITCH_VAD_FLAG_TALKING         - Currently Talking
    SWITCH_VAD_FLAG_EVENTS_TALK     - Fire events when talking is detected
	SWITCH_VAD_FLAG_EVENTS_NOTALK   - Fire events when not talking is detected
	SWITCH_VAD_FLAG_CNG				- Send CNG
</pre>
 */
typedef enum {
	SWITCH_VAD_FLAG_TALKING = (1 << 0),
	SWITCH_VAD_FLAG_EVENTS_TALK = (1 << 1),
	SWITCH_VAD_FLAG_EVENTS_NOTALK = (1 << 2),
	SWITCH_VAD_FLAG_CNG = (1 << 3)
} switch_vad_flag_enum_t;
typedef uint32_t switch_vad_flag_t;

#define SWITCH_RTP_CNG_PAYLOAD 13

/*!
  \enum switch_rtp_flag_t
  \brief RTP Related Flags
<pre>
    SWITCH_RTP_FLAG_NOBLOCK       - Do not block
    SWITCH_RTP_FLAG_IO            - IO is ready
	SWITCH_RTP_FLAG_USE_TIMER     - Timeout Reads and replace with a CNG Frame
	SWITCH_RTP_FLAG_TIMER_RECLOCK - Resync the timer to the current clock on slips
	SWITCH_RTP_FLAG_SECURE        - Secure RTP
	SWITCH_RTP_FLAG_AUTOADJ       - Auto-Adjust the dest based on the source
	SWITCH_RTP_FLAG_RAW_WRITE     - Try to forward packets unscathed
	SWITCH_RTP_FLAG_GOOGLEHACK    - Convert payload from 102 to 97
	SWITCH_RTP_FLAG_VAD           - Enable VAD
	SWITCH_RTP_FLAG_BREAK		  - Stop what you are doing and return SWITCH_STATUS_BREAK
	SWITCH_RTP_FLAG_MINI		  - Use mini RTP when possible
	SWITCH_RTP_FLAG_DATAWAIT	  - Do not return from reads unless there is data even when non blocking
	SWITCH_RTP_FLAG_BUGGY_2833    - Emulate the bug in cisco equipment to allow interop
	SWITCH_RTP_FLAG_PASS_RFC2833  - Pass 2833 (ignore it)
	SWITCH_RTP_FLAG_AUTO_CNG      - Generate outbound CNG frames when idle
</pre>
 */
typedef enum {
	SWITCH_RTP_FLAG_NOBLOCK = (1 << 0),
	SWITCH_RTP_FLAG_IO = (1 << 1),
	SWITCH_RTP_FLAG_USE_TIMER = (1 << 2),
	SWITCH_RTP_FLAG_TIMER_RECLOCK = (1 << 3),
	SWITCH_RTP_FLAG_SECURE_SEND = (1 << 4),
	SWITCH_RTP_FLAG_SECURE_RECV = (1 << 5),
	SWITCH_RTP_FLAG_AUTOADJ = (1 << 6),
	SWITCH_RTP_FLAG_RAW_WRITE = (1 << 7),
	SWITCH_RTP_FLAG_GOOGLEHACK = (1 << 8),
	SWITCH_RTP_FLAG_VAD = (1 << 9),
	SWITCH_RTP_FLAG_BREAK = (1 << 10),
	SWITCH_RTP_FLAG_MINI = (1 << 11),
	SWITCH_RTP_FLAG_DATAWAIT = (1 << 12),
	SWITCH_RTP_FLAG_BUGGY_2833 = (1 << 13),
	SWITCH_RTP_FLAG_PASS_RFC2833 = (1 << 14),
	SWITCH_RTP_FLAG_AUTO_CNG = (1 << 15),
	SWITCH_RTP_FLAG_SECURE_SEND_RESET = (1 << 16),
	SWITCH_RTP_FLAG_SECURE_RECV_RESET = (1 << 17),
	SWITCH_RTP_FLAG_PROXY_MEDIA = (1 << 18),
	SWITCH_RTP_FLAG_SHUTDOWN = (1 << 19)
} switch_rtp_flag_enum_t;
typedef uint32_t switch_rtp_flag_t;


#ifdef _MSC_VER
#pragma pack(push, r1, 1)
#endif

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
typedef struct {
	unsigned version:2;			/* protocol version       */
	unsigned p:1;				/* padding flag           */
	unsigned x:1;				/* header extension flag  */
	unsigned cc:4;				/* CSRC count             */
	unsigned m:1;				/* marker bit             */
	unsigned pt:7;				/* payload type           */
	unsigned seq:16;			/* sequence number        */
	unsigned ts:32;				/* timestamp              */
	unsigned ssrc:32;			/* synchronization source */
} switch_rtp_hdr_t;

#else /*  BIG_ENDIAN */

typedef struct {
	unsigned cc:4;				/* CSRC count             */
	unsigned x:1;				/* header extension flag  */
	unsigned p:1;				/* padding flag           */
	unsigned version:2;			/* protocol version       */
	unsigned pt:7;				/* payload type           */
	unsigned m:1;				/* marker bit             */
	unsigned seq:16;			/* sequence number        */
	unsigned ts:32;				/* timestamp              */
	unsigned ssrc:32;			/* synchronization source */
} switch_rtp_hdr_t;

#endif

#ifdef _MSC_VER
#pragma pack(pop, r1)
#endif

/*!
  \enum switch_priority_t
  \brief Priority Indication
<pre>
    SWITCH_PRIORITY_NORMAL  - Normal Priority
    SWITCH_PRIORITY_LOW     - Low Priority
    SWITCH_PRIORITY_HIGH    - High Priority
</pre>
 */
typedef enum {
	SWITCH_PRIORITY_NORMAL,
	SWITCH_PRIORITY_LOW,
	SWITCH_PRIORITY_HIGH
} switch_priority_t;

/*!
  \enum switch_ivr_option_t
  \brief Possible options related to ivr functions
<pre>
    SWITCH_IVR_OPTION_NONE  - nothing whatsoever
    SWITCH_IVR_OPTION_ASYNC - Asynchronous (do things in the background when applicable)
	SWITCH_IVR_OPTION_FILE  - string argument implies a filename
</pre>
 */
typedef enum {
	SWITCH_IVR_OPTION_NONE = 0,
	SWITCH_IVR_OPTION_ASYNC = (1 << 0),
	SWITCH_IVR_OPTION_FILE = (1 << 1)
} switch_ivr_option_enum_t;
typedef uint32_t switch_ivr_option_t;

/*!
  \enum switch_core_session_message_types_t
  \brief Possible types of messages for inter-session communication
<pre>
	SWITCH_MESSAGE_REDIRECT_AUDIO     - Indication to redirect audio to another location if possible
	SWITCH_MESSAGE_TRANSMIT_TEXT      - A text message
	SWITCH_MESSAGE_INDICATE_ANSWER    - indicate answer
	SWITCH_MESSAGE_INDICATE_PROGRESS  - indicate progress 
	SWITCH_MESSAGE_INDICATE_BRIDGE    - indicate a bridge starting
	SWITCH_MESSAGE_INDICATE_UNBRIDGE  - indicate a bridge ending
	SWITCH_MESSAGE_INDICATE_TRANSFER  - indicate a transfer is taking place
	SWITCH_MESSAGE_INDICATE_MEDIA	  - indicate media is required
	SWITCH_MESSAGE_INDICATE_NOMEDIA	  - indicate no-media is required
	SWITCH_MESSAGE_INDICATE_HOLD      - indicate hold
	SWITCH_MESSAGE_INDICATE_UNHOLD    - indicate unhold
	SWITCH_MESSAGE_INDICATE_REDIRECT  - indicate redirect
	SWITCH_MESSAGE_INDICATE_RESPOND    - indicate reject
	SWITCH_MESSAGE_INDICATE_BROADCAST - indicate media broadcast
	SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT - indicate media broadcast
	SWITCH_MESSAGE_INDICATE_DEFLECT - indicate deflect
	SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ - indicate video refresh request
</pre>
 */
typedef enum {
	SWITCH_MESSAGE_REDIRECT_AUDIO,
	SWITCH_MESSAGE_TRANSMIT_TEXT,
	SWITCH_MESSAGE_INDICATE_ANSWER,
	SWITCH_MESSAGE_INDICATE_PROGRESS,
	SWITCH_MESSAGE_INDICATE_BRIDGE,
	SWITCH_MESSAGE_INDICATE_UNBRIDGE,
	SWITCH_MESSAGE_INDICATE_TRANSFER,
	SWITCH_MESSAGE_INDICATE_RINGING,
	SWITCH_MESSAGE_INDICATE_MEDIA,
	SWITCH_MESSAGE_INDICATE_NOMEDIA,
	SWITCH_MESSAGE_INDICATE_HOLD,
	SWITCH_MESSAGE_INDICATE_UNHOLD,
	SWITCH_MESSAGE_INDICATE_REDIRECT,
	SWITCH_MESSAGE_INDICATE_RESPOND,
	SWITCH_MESSAGE_INDICATE_BROADCAST,
	SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT,
	SWITCH_MESSAGE_INDICATE_DEFLECT,
	SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ,
	SWITCH_MESSAGE_INDICATE_DISPLAY,
	SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY,
	SWITCH_MESSAGE_INVALID
} switch_core_session_message_types_t;


/*!
  \enum switch_stack_t
  \brief Expression of how to stack a list
<pre>
SWITCH_STACK_BOTTOM - Stack on the bottom
SWITCH_STACK_TOP	- Stack on the top
</pre>
 */
typedef enum {
	SWITCH_STACK_BOTTOM,
	SWITCH_STACK_TOP
} switch_stack_t;

/*!
  \enum switch_status_t
  \brief Common return values
<pre>
    SWITCH_STATUS_SUCCESS	- General Success (common return value for most functions)
    SWITCH_STATUS_FALSE		- General Falsehood
    SWITCH_STATUS_TIMEOUT	- A Timeout has occured
    SWITCH_STATUS_RESTART	- An indication to restart the previous operation
    SWITCH_STATUS_TERM		- An indication to terminate
    SWITCH_STATUS_NOTIMPL	- An indication that requested resource is not impelemented
    SWITCH_STATUS_MEMERR	- General memory error
    SWITCH_STATUS_NOOP		- NOTHING
    SWITCH_STATUS_RESAMPLE	- An indication that a resample has occured
    SWITCH_STATUS_GENERR	- A general Error
    SWITCH_STATUS_INUSE		- An indication that requested resource is in use
	SWITCH_STATUS_BREAK     - A non-fatal break of an operation
    SWITCH_STATUS_SOCKERR   - A socket error
	SWITCH_STATUS_MORE_DATA - Need More Data
	SWITCH_STATUS_NOTFOUND  - Not Found
	SWITCH_STATUS_UNLOAD    - Unload
	SWITCH_STATUS_NOUNLOAD  - Never Unload
</pre>
 */
typedef enum {
	SWITCH_STATUS_SUCCESS,
	SWITCH_STATUS_FALSE,
	SWITCH_STATUS_TIMEOUT,
	SWITCH_STATUS_RESTART,
	SWITCH_STATUS_TERM,
	SWITCH_STATUS_NOTIMPL,
	SWITCH_STATUS_MEMERR,
	SWITCH_STATUS_NOOP,
	SWITCH_STATUS_RESAMPLE,
	SWITCH_STATUS_GENERR,
	SWITCH_STATUS_INUSE,
	SWITCH_STATUS_BREAK,
	SWITCH_STATUS_SOCKERR,
	SWITCH_STATUS_MORE_DATA,
	SWITCH_STATUS_NOTFOUND,
	SWITCH_STATUS_UNLOAD,
	SWITCH_STATUS_NOUNLOAD,
	SWITCH_STATUS_IGNORE,
	SWITCH_STATUS_TOO_SMALL,
	SWITCH_STATUS_NOT_INITALIZED
} switch_status_t;



/*!
\enum switch_log_level_t
\brief Log Level Enumeration
<pre>
	SWITCH_LOG_DEBUG            - Debug
	SWITCH_LOG_INFO             - Info
	SWITCH_LOG_NOTICE           - Notice
	SWITCH_LOG_WARNING          - Warning
	SWITCH_LOG_ERROR            - Error
	SWITCH_LOG_CRIT             - Critical
	SWITCH_LOG_ALERT            - Alert
	SWITCH_LOG_CONSOLE          - Console
</pre>
 */
typedef enum {
	SWITCH_LOG_DEBUG = 7,
	SWITCH_LOG_INFO = 6,
	SWITCH_LOG_NOTICE = 5,
	SWITCH_LOG_WARNING = 4,
	SWITCH_LOG_ERROR = 3,
	SWITCH_LOG_CRIT = 2,
	SWITCH_LOG_ALERT = 1,
	SWITCH_LOG_CONSOLE = 0,
	SWITCH_LOG_INVALID = 64
} switch_log_level_t;


/*!
\enum switch_text_channel_t
\brief A target to write log/debug info to
<pre>
SWITCH_CHANNEL_ID_LOG			- Write to the currently defined log
SWITCH_CHANNEL_ID_LOG_CLEAN		- Write to the currently defined log with no extra file/line/date information
SWITCH_CHANNEL_ID_EVENT			- Write to the event engine as a LOG event
</pre>
 */
typedef enum {
	SWITCH_CHANNEL_ID_LOG,
	SWITCH_CHANNEL_ID_LOG_CLEAN,
	SWITCH_CHANNEL_ID_EVENT
} switch_text_channel_t;

typedef enum {
	SCSMF_DYNAMIC = (1 << 0)
} switch_core_session_message_flag_enum_t;
typedef uint32_t switch_core_session_message_flag_t;

#define SWITCH_CHANNEL_LOG SWITCH_CHANNEL_ID_LOG, __FILE__, __SWITCH_FUNC__, __LINE__, NULL
#define SWITCH_CHANNEL_LOG_CLEAN SWITCH_CHANNEL_ID_LOG_CLEAN, __FILE__, __SWITCH_FUNC__, __LINE__, NULL
#define SWITCH_CHANNEL_EVENT SWITCH_CHANNEL_ID_EVENT, __FILE__, __SWITCH_FUNC__, __LINE__, NULL

/*!
  \enum switch_channel_state_t
  \brief Channel States (these are the defaults, CS_SOFT_EXECUTE, CS_EXCHANGE_MEDIA, and CS_CONSUME_MEDIA are often overridden by specific apps)
<pre>
CS_NEW       - Channel is newly created 
CS_INIT      - Channel has been initilized
CS_ROUTING   - Channel is looking for an extension to execute
CS_SOFT_EXECUTE  - Channel is ready to execute from 3rd party control
CS_EXECUTE   - Channel is executing it's dialplan 
CS_EXCHANGE_MEDIA  - Channel is exchanging media with another channel.
CS_PARK      - Channel is accepting media awaiting commands.
CS_CONSUME_MEDIA		 - Channel is consuming all media and dropping it.
CS_HIBERNATE - Channel is in a sleep state
CS_RESET 	 - Channel is in a reset state
CS_HANGUP    - Channel is flagged for hangup and ready to end
CS_DONE      - Channel is ready to be destroyed and out of the state machine
</pre>
 */
typedef enum {
	CS_NEW,
	CS_INIT,
	CS_ROUTING,
	CS_SOFT_EXECUTE,
	CS_EXECUTE,
	CS_EXCHANGE_MEDIA,
	CS_PARK,
	CS_CONSUME_MEDIA,
	CS_HIBERNATE,
	CS_RESET,
	CS_HANGUP,
	CS_DONE,
	CS_NONE
} switch_channel_state_t;


/*!
  \enum switch_channel_flag_t
  \brief Channel Flags

<pre>
CF_ANSWERED     = (1 <<  0) - Channel is answered
CF_OUTBOUND     = (1 <<  1) - Channel is an outbound channel
CF_EARLY_MEDIA  = (1 <<  2) - Channel is ready for audio before answer 
CF_ORIGINATOR	= (1 <<  3) - Channel is an originator
CF_TRANSFER		= (1 <<  4) - Channel is being transfered
CF_ACCEPT_CNG	= (1 <<  5) - Channel will accept CNG frames
CF_WAIT_FOR_ME	= (1 <<  6) - Channel wants you to wait
CF_BRIDGED		= (1 <<  7) - Channel in a bridge
CF_HOLD			= (1 <<  8) - Channel is on hold
CF_SERVICE		= (1 <<  9) - Channel has a service thread
CF_TAGGED		= (1 << 10) - Channel is tagged
CF_WINNER		= (1 << 11) - Channel is the winner
CF_CONTROLLED	= (1 << 12) - Channel is under control
CF_PROXY_MODE		= (1 << 13) - Channel has no media
CF_SUSPEND		= (1 << 14) - Suspend i/o
CF_EVENT_PARSE  = (1 << 15) - Suspend control events
CF_REPEAT_STATE = (1 << 16) - Tell the state machine to repeat a state
CF_GEN_RINGBACK = (1 << 17) - Channel is generating it's own ringback
CF_RING_READY   = (1 << 18) - Channel is ready to send ringback
CF_BREAK        = (1 << 19) - Channel should stop what it's doing
CF_BROADCAST    = (1 << 20) - Channel is broadcasting
CF_UNICAST      = (1 << 21) - Channel has a unicast connection
CF_VIDEO		= (1 << 22) - Channel has video
CF_EVENT_LOCK   = (1 << 23) - Don't parse events
CF_RESET        = (1 << 24) - Tell extension parser to reset
CF_ORIGINATING  = (1 << 25) - Channel is originating
CF_STOP_BROADCAST = (1 << 26) - Signal to stop broadcast
</pre>
 */

typedef enum {
	CF_ANSWERED = (1 << 0),
	CF_OUTBOUND = (1 << 1),
	CF_EARLY_MEDIA = (1 << 2),
	CF_ORIGINATOR = (1 << 3),
	CF_TRANSFER = (1 << 4),
	CF_ACCEPT_CNG = (1 << 5),
	CF_WAIT_FOR_ME = (1 << 6),
	CF_BRIDGED = (1 << 7),
	CF_HOLD = (1 << 8),
	CF_SERVICE = (1 << 9),
	CF_TAGGED = (1 << 10),
	CF_WINNER = (1 << 11),
	CF_CONTROLLED = (1 << 12),
	CF_PROXY_MODE = (1 << 13),
	CF_SUSPEND = (1 << 14),
	CF_EVENT_PARSE = (1 << 15),
	CF_REPEAT_STATE = (1 << 16),
	CF_GEN_RINGBACK = (1 << 17),
	CF_RING_READY = (1 << 18),
	CF_BREAK = (1 << 19),
	CF_BROADCAST = (1 << 20),
	CF_UNICAST = (1 << 21),
	CF_VIDEO = (1 << 22),
	CF_EVENT_LOCK = (1 << 23),
	CF_RESET = (1 << 24),
	CF_ORIGINATING = (1 << 25),
	CF_STOP_BROADCAST = (1 << 26),
	CF_PROXY_MEDIA = (1 << 27),
	CF_INNER_BRIDGE = (1 << 28),
	CF_REQ_MEDIA = (1 << 29),
	CF_VERBOSE_EVENTS = (1 << 30)
} switch_channel_flag_enum_t;
typedef uint32_t switch_channel_flag_t;


/*!
  \enum switch_frame_flag_t
  \brief Frame Flags

<pre>
SFF_CNG        = (1 <<  0) - Frame represents comfort noise
SFF_RAW_RTP    = (1 <<  1) - Frame has raw rtp accessible
SFF_RTP_HEADER = (1 << 2)  - Get the rtp header from the frame header
SFF_PLC        = (1 << 3)  - Frame has generated PLC data
SFF_RFC2833    = (1 << 4)  - Frame has rfc2833 dtmf data
</pre>
 */
typedef enum {
	SFF_NONE = 0,
	SFF_CNG = (1 << 0),
	SFF_RAW_RTP = (1 << 1),
	SFF_RTP_HEADER = (1 << 2),
	SFF_PLC = (1 << 3),
	SFF_RFC2833 = (1 << 4),
	SFF_PROXY_PACKET = (1 << 5)
} switch_frame_flag_enum_t;
typedef uint32_t switch_frame_flag_t;


typedef enum {
	SAF_NONE = 0,
	SAF_SUPPORT_NOMEDIA = (1 << 0)
} switch_application_flag_enum_t;
typedef uint32_t switch_application_flag_t;

/*!
  \enum switch_signal_t
  \brief Signals to send to channels
<pre>
SWITCH_SIG_KILL - Kill the channel
SWITCH_SIG_XFER - Stop the current io but leave it viable
</pre>
 */

typedef enum {
	SWITCH_SIG_NONE,
	SWITCH_SIG_KILL,
	SWITCH_SIG_XFER,
	SWITCH_SIG_BREAK
} switch_signal_t;

/*!
  \enum switch_codec_flag_t
  \brief Codec related flags
<pre>
SWITCH_CODEC_FLAG_ENCODE =			(1 <<  0) - Codec can encode
SWITCH_CODEC_FLAG_DECODE =			(1 <<  1) - Codec can decode
SWITCH_CODEC_FLAG_SILENCE_START =	(1 <<  2) - Start period of silence
SWITCH_CODEC_FLAG_SILENCE_STOP =	(1 <<  3) - End period of silence
SWITCH_CODEC_FLAG_SILENCE =			(1 <<  4) - Silence
SWITCH_CODEC_FLAG_FREE_POOL =		(1 <<  5) - Free codec's pool on destruction
SWITCH_CODEC_FLAG_AAL2 =			(1 <<  6) - USE AAL2 Bitpacking
SWITCH_CODEC_FLAG_PASSTHROUGH =		(1 <<  7) - Passthrough only
</pre>
*/
typedef enum {
	SWITCH_CODEC_FLAG_ENCODE = (1 << 0),
	SWITCH_CODEC_FLAG_DECODE = (1 << 1),
	SWITCH_CODEC_FLAG_SILENCE_START = (1 << 2),
	SWITCH_CODEC_FLAG_SILENCE_STOP = (1 << 3),
	SWITCH_CODEC_FLAG_SILENCE = (1 << 4),
	SWITCH_CODEC_FLAG_FREE_POOL = (1 << 5),
	SWITCH_CODEC_FLAG_AAL2 = (1 << 6),
	SWITCH_CODEC_FLAG_PASSTHROUGH = (1 << 7)
} switch_codec_flag_enum_t;
typedef uint32_t switch_codec_flag_t;


/*!
  \enum switch_speech_flag_t
  \brief Speech related flags
<pre>
SWITCH_SPEECH_FLAG_HASTEXT =		(1 <<  0) - Interface is has text to read.
SWITCH_SPEECH_FLAG_PEEK =			(1 <<  1) - Read data but do not erase it.
SWITCH_SPEECH_FLAG_FREE_POOL =		(1 <<  2) - Free interface's pool on destruction.
SWITCH_SPEECH_FLAG_BLOCKING =       (1 <<  3) - Indicate that a blocking call is desired 
SWITCH_SPEECH_FLAG_PAUSE = 			(1 <<  4) - Pause toggle for playback
</pre>
*/
typedef enum {
	SWITCH_SPEECH_FLAG_NONE = 0,
	SWITCH_SPEECH_FLAG_HASTEXT = (1 << 0),
	SWITCH_SPEECH_FLAG_PEEK = (1 << 1),
	SWITCH_SPEECH_FLAG_FREE_POOL = (1 << 2),
	SWITCH_SPEECH_FLAG_BLOCKING = (1 << 3),
	SWITCH_SPEECH_FLAG_PAUSE = (1 << 4)
} switch_speech_flag_enum_t;
typedef uint32_t switch_speech_flag_t;

/*!
  \enum switch_asr_flag_t
  \brief Asr related flags
<pre>
SWITCH_ASR_FLAG_DATA =			(1 <<  0) - Interface has data
SWITCH_ASR_FLAG_FREE_POOL =		(1 <<  1) - Pool needs to be freed
SWITCH_ASR_FLAG_CLOSED = 		(1 <<  2) - Interface has been closed
SWITCH_ASR_FLAG_FIRE_EVENTS =	(1 <<  3) - Fire all speech events
SWITCH_ASR_FLAG_AUTO_RESUME =   (1 <<  4) - Auto Resume
</pre>
*/
typedef enum {
	SWITCH_ASR_FLAG_NONE = 0,
	SWITCH_ASR_FLAG_DATA = (1 << 0),
	SWITCH_ASR_FLAG_FREE_POOL = (1 << 1),
	SWITCH_ASR_FLAG_CLOSED = (1 << 2),
	SWITCH_ASR_FLAG_FIRE_EVENTS = (1 << 3),
	SWITCH_ASR_FLAG_AUTO_RESUME = (1 << 4)

} switch_asr_flag_enum_t;
typedef uint32_t switch_asr_flag_t;

/*!
  \enum switch_directory_flag_t
  \brief Directory Handle related flags
<pre>
SWITCH_DIRECTORY_FLAG_FREE_POOL =		(1 <<  0) - Free interface's pool on destruction.
</pre>
*/
typedef enum {
	SWITCH_DIRECTORY_FLAG_FREE_POOL = (1 << 0)

} switch_directory_flag_enum_t;
typedef uint32_t switch_directory_flag_t;

/*!
  \enum switch_codec_type_t
  \brief Codec types
<pre>
SWITCH_CODEC_TYPE_AUDIO - Audio Codec
SWITCH_CODEC_TYPE_VIDEO - Video Codec
SWITCH_CODEC_TYPE_T38   - T38 Codec
SWITCH_CODEC_TYPE_APP   - Application Codec
</pre>
 */
typedef enum {
	SWITCH_CODEC_TYPE_AUDIO,
	SWITCH_CODEC_TYPE_VIDEO,
	SWITCH_CODEC_TYPE_T38,
	SWITCH_CODEC_TYPE_APP
} switch_codec_type_t;


/*!
  \enum switch_timer_flag_t
  \brief Timer related flags
<pre>
SWITCH_TIMER_FLAG_FREE_POOL =		(1 <<  0) - Free timer's pool on destruction
</pre>
*/
typedef enum {
	SWITCH_TIMER_FLAG_FREE_POOL = (1 << 0)
} switch_timer_flag_enum_t;
typedef uint32_t switch_timer_flag_t;


/*!
  \enum switch_timer_flag_t
  \brief Timer related flags
<pre>
SMBF_READ_STREAM - Include the Read Stream
SMBF_WRITE_STREAM - Include the Write Stream
SMBF_WRITE_REPLACE - Replace the Write Stream
SMBF_READ_REPLACE - Replace the Read Stream
SMBF_STEREO - Record in stereo
SMBF_ANSWER_RECORD_REQ - Don't record until the channel is answered
SMBF_THREAD_LOCK - Only let the same thread who created the bug remove it.
</pre>
*/
typedef enum {
	SMBF_BOTH = 0,
	SMBF_READ_STREAM = (1 << 0),
	SMBF_WRITE_STREAM = (1 << 1),
	SMBF_WRITE_REPLACE = (1 << 2),
	SMBF_READ_REPLACE = (1 << 3),
	SMBF_READ_PING = (1 << 4),
	SMBF_STEREO = (1 << 5),
	SMBF_RECORD_ANSWER_REQ = (1 << 6),
	SMBF_THREAD_LOCK = (1 << 7)
} switch_media_bug_flag_enum_t;
typedef uint32_t switch_media_bug_flag_t;

/*!
  \enum switch_file_flag_t
  \brief File flags
<pre>
SWITCH_FILE_FLAG_READ =         (1 <<  0) - Open for read
SWITCH_FILE_FLAG_WRITE =        (1 <<  1) - Open for write
SWITCH_FILE_FLAG_FREE_POOL =    (1 <<  2) - Free file handle's pool on destruction
SWITCH_FILE_DATA_SHORT =        (1 <<  3) - Read data in shorts
SWITCH_FILE_DATA_INT =          (1 <<  4) - Read data in ints
SWITCH_FILE_DATA_FLOAT =        (1 <<  5) - Read data in floats
SWITCH_FILE_DATA_DOUBLE =       (1 <<  6) - Read data in doubles
SWITCH_FILE_DATA_RAW =          (1 <<  7) - Read data as is
SWITCH_FILE_PAUSE =             (1 <<  8) - Pause
SWITCH_FILE_NATIVE =            (1 <<  9) - File is in native format (no transcoding)
SWITCH_FILE_SEEK = 				(1 << 10) - File has done a seek
SWITCH_FILE_OPEN =              (1 << 11) - File is open
</pre>
 */
typedef enum {
	SWITCH_FILE_FLAG_READ = (1 << 0),
	SWITCH_FILE_FLAG_WRITE = (1 << 1),
	SWITCH_FILE_FLAG_FREE_POOL = (1 << 2),
	SWITCH_FILE_DATA_SHORT = (1 << 3),
	SWITCH_FILE_DATA_INT = (1 << 4),
	SWITCH_FILE_DATA_FLOAT = (1 << 5),
	SWITCH_FILE_DATA_DOUBLE = (1 << 6),
	SWITCH_FILE_DATA_RAW = (1 << 7),
	SWITCH_FILE_PAUSE = (1 << 8),
	SWITCH_FILE_NATIVE = (1 << 9),
	SWITCH_FILE_SEEK = (1 << 10),
	SWITCH_FILE_OPEN = (1 << 11),
	SWITCH_FILE_CALLBACK = (1 << 12)
} switch_file_flag_enum_t;
typedef uint32_t switch_file_flag_t;

typedef enum {
	SWITCH_IO_FLAG_NONE = 0,
	SWITCH_IO_FLAG_NOBLOCK = (1 << 0)
} switch_io_flag_enum_t;
typedef uint32_t switch_io_flag_t;

/* make sure this is synced with the EVENT_NAMES array in switch_event.c
   also never put any new ones before EVENT_ALL
*/
/*!
  \enum switch_event_types_t
  \brief Built-in Events

<pre>
    SWITCH_EVENT_CUSTOM				- A custom event
    SWITCH_EVENT_CHANNEL_CREATE		- A channel has been created
    SWITCH_EVENT_CHANNEL_DESTROY	- A channel has been destroyed
    SWITCH_EVENT_CHANNEL_STATE		- A channel has changed state
    SWITCH_EVENT_CHANNEL_ANSWER		- A channel has been answered
    SWITCH_EVENT_CHANNEL_HANGUP		- A channel has been hungup
    SWITCH_EVENT_CHANNEL_EXECUTE	- A channel has executed a module's application
    SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE	- A channel has finshed executing a module's application
	SWITCH_EVENT_CHANNEL_BRIDGE     - A channel has bridged to another channel
	SWITCH_EVENT_CHANNEL_UNBRIDGE   - A channel has unbridged from another channel
    SWITCH_EVENT_CHANNEL_PROGRESS	- A channel has started ringing
    SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA	- A channel has started early media
    SWITCH_EVENT_CHANNEL_OUTGOING	- A channel has been unparked
	SWITCH_EVENT_CHANNEL_PARK 		- A channel has been parked
	SWITCH_EVENT_CHANNEL_UNPARK 	- A channel has been unparked
	SWITCH_EVENT_CHANNEL_APPLICATION- A channel has called and event from an application
	SWITCH_EVENT_CHANNEL_ORIGINATE  - A channel has been originated
	SWITCH_EVENT_CHANNEL_UUID       - A channel has changed uuid
    SWITCH_EVENT_API				- An API call has been executed
    SWITCH_EVENT_LOG				- A LOG event has been triggered
    SWITCH_EVENT_INBOUND_CHAN		- A new inbound channel has been created
    SWITCH_EVENT_OUTBOUND_CHAN		- A new outbound channel has been created
    SWITCH_EVENT_STARTUP			- The system has been started
    SWITCH_EVENT_SHUTDOWN			- The system has been shutdown
	SWITCH_EVENT_PUBLISH			- Publish
	SWITCH_EVENT_UNPUBLISH			- UnPublish
	SWITCH_EVENT_TALK				- Talking Detected
	SWITCH_EVENT_NOTALK				- Not Talking Detected
	SWITCH_EVENT_SESSION_CRASH		- Session Crashed
	SWITCH_EVENT_MODULE_LOAD		- Module was loaded
	SWITCH_EVENT_MODULE_UNLOAD		- Module was unloaded
	SWITCH_EVENT_DTMF				- DTMF was sent
	SWITCH_EVENT_MESSAGE			- A Basic Message
	SWITCH_EVENT_PRESENCE_IN		- Presence in
	SWITCH_EVENT_PRESENCE_OUT		- Presence out
	SWITCH_EVENT_PRESENCE_PROBE		- Presence probe
	SWITCH_EVENT_MESSAGE_WAITING	- A message is waiting
	SWITCH_EVENT_MESSAGE_QUERY		- A query for MESSAGE_WAITING events
	SWITCH_EVENT_CODEC				- Codec Change
	SWITCH_EVENT_BACKGROUND_JOB		- Background Job
	SWITCH_EVENT_DETECTED_SPEECH	- Detected Speech
	SWITCH_EVENT_DETECTED_TONE      - Detected Tone
	SWITCH_EVENT_PRIVATE_COMMAND	- A private command event 
	SWITCH_EVENT_HEARTBEAT			- Machine is alive
	SWITCH_EVENT_TRAP				- Error Trap
	SWITCH_EVENT_ADD_SCHEDULE		- Something has been scheduled
	SWITCH_EVENT_DEL_SCHEDULE		- Something has been unscheduled
	SWITCH_EVENT_EXE_SCHEDULE		- Something scheduled has been executed
	SWITCH_EVENT_RE_SCHEDULE		- Something scheduled has been rescheduled
	SWITCH_EVENT_RELOADXML			- XML registry has been reloaded
	SWITCH_EVENT_NOTIFY				- Notification
	SWITCH_EVENT_SEND_MESSAGE		- Message
	SWITCH_EVENT_RECV_MESSAGE		- Message
    SWITCH_EVENT_ALL				- All events at once
</pre>

 */
typedef enum {
	SWITCH_EVENT_CUSTOM,
	SWITCH_EVENT_CHANNEL_CREATE,
	SWITCH_EVENT_CHANNEL_DESTROY,
	SWITCH_EVENT_CHANNEL_STATE,
	SWITCH_EVENT_CHANNEL_ANSWER,
	SWITCH_EVENT_CHANNEL_HANGUP,
	SWITCH_EVENT_CHANNEL_EXECUTE,
	SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE,
	SWITCH_EVENT_CHANNEL_BRIDGE,
	SWITCH_EVENT_CHANNEL_UNBRIDGE,
	SWITCH_EVENT_CHANNEL_PROGRESS,
	SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA,
	SWITCH_EVENT_CHANNEL_OUTGOING,
	SWITCH_EVENT_CHANNEL_PARK,
	SWITCH_EVENT_CHANNEL_UNPARK,
	SWITCH_EVENT_CHANNEL_APPLICATION,
	SWITCH_EVENT_CHANNEL_ORIGINATE,
	SWITCH_EVENT_CHANNEL_UUID,
	SWITCH_EVENT_API,
	SWITCH_EVENT_LOG,
	SWITCH_EVENT_INBOUND_CHAN,
	SWITCH_EVENT_OUTBOUND_CHAN,
	SWITCH_EVENT_STARTUP,
	SWITCH_EVENT_SHUTDOWN,
	SWITCH_EVENT_PUBLISH,
	SWITCH_EVENT_UNPUBLISH,
	SWITCH_EVENT_TALK,
	SWITCH_EVENT_NOTALK,
	SWITCH_EVENT_SESSION_CRASH,
	SWITCH_EVENT_MODULE_LOAD,
	SWITCH_EVENT_MODULE_UNLOAD,
	SWITCH_EVENT_DTMF,
	SWITCH_EVENT_MESSAGE,
	SWITCH_EVENT_PRESENCE_IN,
	SWITCH_EVENT_PRESENCE_OUT,
	SWITCH_EVENT_PRESENCE_PROBE,
	SWITCH_EVENT_MESSAGE_WAITING,
	SWITCH_EVENT_MESSAGE_QUERY,
	SWITCH_EVENT_ROSTER,
	SWITCH_EVENT_CODEC,
	SWITCH_EVENT_BACKGROUND_JOB,
	SWITCH_EVENT_DETECTED_SPEECH,
	SWITCH_EVENT_DETECTED_TONE,
	SWITCH_EVENT_PRIVATE_COMMAND,
	SWITCH_EVENT_HEARTBEAT,
	SWITCH_EVENT_TRAP,
	SWITCH_EVENT_ADD_SCHEDULE,
	SWITCH_EVENT_DEL_SCHEDULE,
	SWITCH_EVENT_EXE_SCHEDULE,
	SWITCH_EVENT_RE_SCHEDULE,
	SWITCH_EVENT_RELOADXML,
	SWITCH_EVENT_NOTIFY,
	SWITCH_EVENT_SEND_MESSAGE,
	SWITCH_EVENT_RECV_MESSAGE,
	SWITCH_EVENT_REQUEST_PARAMS,
	SWITCH_EVENT_CHANNEL_DATA,
	SWITCH_EVENT_GENERAL,
	SWITCH_EVENT_COMMAND,
	SWITCH_EVENT_SESSION_HEARTBEAT,
	SWITCH_EVENT_ALL
} switch_event_types_t;

typedef enum {
	SWITCH_INPUT_TYPE_DTMF,
	SWITCH_INPUT_TYPE_EVENT
} switch_input_type_t;

typedef enum {
	SWITCH_CAUSE_NONE = 0,
	SWITCH_CAUSE_UNALLOCATED_NUMBER = 1,
	SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET = 2,
	SWITCH_CAUSE_NO_ROUTE_DESTINATION = 3,
	SWITCH_CAUSE_CHANNEL_UNACCEPTABLE = 6,
	SWITCH_CAUSE_CALL_AWARDED_DELIVERED = 7,
	SWITCH_CAUSE_NORMAL_CLEARING = 16,
	SWITCH_CAUSE_USER_BUSY = 17,
	SWITCH_CAUSE_NO_USER_RESPONSE = 18,
	SWITCH_CAUSE_NO_ANSWER = 19,
	SWITCH_CAUSE_SUBSCRIBER_ABSENT = 20,
	SWITCH_CAUSE_CALL_REJECTED = 21,
	SWITCH_CAUSE_NUMBER_CHANGED = 22,
	SWITCH_CAUSE_REDIRECTION_TO_NEW_DESTINATION = 23,
	SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR = 25,
	SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER = 27,
	SWITCH_CAUSE_INVALID_NUMBER_FORMAT = 28,
	SWITCH_CAUSE_FACILITY_REJECTED = 29,
	SWITCH_CAUSE_RESPONSE_TO_STATUS_ENQUIRY = 30,
	SWITCH_CAUSE_NORMAL_UNSPECIFIED = 31,
	SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION = 34,
	SWITCH_CAUSE_NETWORK_OUT_OF_ORDER = 38,
	SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE = 41,
	SWITCH_CAUSE_SWITCH_CONGESTION = 42,
	SWITCH_CAUSE_ACCESS_INFO_DISCARDED = 43,
	SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL = 44,
	SWITCH_CAUSE_PRE_EMPTED = 45,
	SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED = 50,
	SWITCH_CAUSE_OUTGOING_CALL_BARRED = 52,
	SWITCH_CAUSE_INCOMING_CALL_BARRED = 54,
	SWITCH_CAUSE_BEARERCAPABILITY_NOTAUTH = 57,
	SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL = 58,
	SWITCH_CAUSE_SERVICE_UNAVAILABLE = 63,
	SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL = 65,
	SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED = 66,
	SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED = 69,
	SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED = 79,
	SWITCH_CAUSE_INVALID_CALL_REFERENCE = 81,
	SWITCH_CAUSE_INCOMPATIBLE_DESTINATION = 88,
	SWITCH_CAUSE_INVALID_MSG_UNSPECIFIED = 95,
	SWITCH_CAUSE_MANDATORY_IE_MISSING = 96,
	SWITCH_CAUSE_MESSAGE_TYPE_NONEXIST = 97,
	SWITCH_CAUSE_WRONG_MESSAGE = 98,
	SWITCH_CAUSE_IE_NONEXIST = 99,
	SWITCH_CAUSE_INVALID_IE_CONTENTS = 100,
	SWITCH_CAUSE_WRONG_CALL_STATE = 101,
	SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE = 102,
	SWITCH_CAUSE_MANDATORY_IE_LENGTH_ERROR = 103,
	SWITCH_CAUSE_PROTOCOL_ERROR = 111,
	SWITCH_CAUSE_INTERWORKING = 127,
	SWITCH_CAUSE_SUCCESS = 142,
	SWITCH_CAUSE_ORIGINATOR_CANCEL = 487,
	SWITCH_CAUSE_CRASH = 500,
	SWITCH_CAUSE_SYSTEM_SHUTDOWN = 501,
	SWITCH_CAUSE_LOSE_RACE = 502,
	SWITCH_CAUSE_MANAGER_REQUEST = 503,
	SWITCH_CAUSE_BLIND_TRANSFER = 600,
	SWITCH_CAUSE_ATTENDED_TRANSFER = 601,
	SWITCH_CAUSE_ALLOTTED_TIMEOUT = 602,
	SWITCH_CAUSE_USER_CHALLENGE = 603,
	SWITCH_CAUSE_MEDIA_TIMEOUT = 604,
	SWITCH_CAUSE_PICKED_OFF = 605,
	SWITCH_CAUSE_USER_NOT_REGISTERED = 606,
	SWITCH_CAUSE_PROGRESS_TIMEOUT = 607
} switch_call_cause_t;

typedef enum {
	SCSC_PAUSE_INBOUND,
	SCSC_HUPALL,
	SCSC_SHUTDOWN,
	SCSC_CHECK_RUNNING,
	SCSC_LOGLEVEL,
	SCSC_SPS,
	SCSC_LAST_SPS,
	SCSC_RECLAIM,
	SCSC_MAX_SESSIONS,
	SCSC_SYNC_CLOCK,
	SCSC_MAX_DTMF_DURATION,
	SCSC_DEFAULT_DTMF_DURATION,
	SCSC_SHUTDOWN_ELEGANT,
	SCSC_SHUTDOWN_ASAP,
	SCSC_CANCEL_SHUTDOWN,
	SCSC_SEND_SIGHUP
} switch_session_ctl_t;

typedef struct apr_pool_t switch_memory_pool_t;
typedef uint16_t switch_port_t;
typedef uint8_t switch_payload_t;
typedef struct switch_app_log switch_app_log_t;
typedef struct switch_rtp switch_rtp_t;
typedef struct switch_core_session_message switch_core_session_message_t;
typedef struct switch_event_header switch_event_header_t;
typedef struct switch_event switch_event_t;
typedef struct switch_event_subclass switch_event_subclass_t;
typedef struct switch_event_node switch_event_node_t;
typedef struct switch_loadable_module switch_loadable_module_t;
typedef struct switch_frame switch_frame_t;
typedef struct switch_channel switch_channel_t;
typedef struct switch_file_handle switch_file_handle_t;
typedef struct switch_core_session switch_core_session_t;
typedef struct switch_caller_profile switch_caller_profile_t;
typedef struct switch_caller_extension switch_caller_extension_t;
typedef struct switch_caller_application switch_caller_application_t;
typedef struct switch_state_handler_table switch_state_handler_table_t;
typedef struct switch_timer switch_timer_t;
typedef struct switch_codec switch_codec_t;
typedef struct switch_core_thread_session switch_core_thread_session_t;
typedef struct switch_codec_implementation switch_codec_implementation_t;
typedef struct switch_buffer switch_buffer_t;
typedef struct switch_codec_settings switch_codec_settings_t;
typedef struct switch_odbc_handle switch_odbc_handle_t;

typedef struct switch_io_routines switch_io_routines_t;
typedef struct switch_speech_handle switch_speech_handle_t;
typedef struct switch_asr_handle switch_asr_handle_t;
typedef struct switch_directory_handle switch_directory_handle_t;
typedef struct switch_loadable_module_interface switch_loadable_module_interface_t;
typedef struct switch_endpoint_interface switch_endpoint_interface_t;
typedef struct switch_timer_interface switch_timer_interface_t;
typedef struct switch_dialplan_interface switch_dialplan_interface_t;
typedef struct switch_codec_interface switch_codec_interface_t;
typedef struct switch_application_interface switch_application_interface_t;
typedef struct switch_api_interface switch_api_interface_t;
typedef struct switch_file_interface switch_file_interface_t;
typedef struct switch_speech_interface switch_speech_interface_t;
typedef struct switch_asr_interface switch_asr_interface_t;
typedef struct switch_directory_interface switch_directory_interface_t;
typedef struct switch_chat_interface switch_chat_interface_t;
typedef struct switch_management_interface switch_management_interface_t;
typedef struct switch_core_port_allocator switch_core_port_allocator_t;
typedef struct switch_media_bug switch_media_bug_t;
typedef switch_bool_t (*switch_media_bug_callback_t) (switch_media_bug_t *, void *, switch_abc_type_t);
typedef struct switch_xml_binding switch_xml_binding_t;

typedef switch_status_t (*switch_core_codec_encode_func_t) (switch_codec_t *codec,
															switch_codec_t *other_codec,
															void *decoded_data,
															uint32_t decoded_data_len,
															uint32_t decoded_rate,
															void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate, unsigned int *flag);


typedef switch_status_t (*switch_core_codec_decode_func_t) (switch_codec_t *codec,
															switch_codec_t *other_codec,
															void *encoded_data,
															uint32_t encoded_data_len,
															uint32_t encoded_rate,
															void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate, unsigned int *flag);

typedef switch_status_t (*switch_core_codec_init_func_t) (switch_codec_t *, switch_codec_flag_t, const switch_codec_settings_t *codec_settings);
typedef switch_status_t (*switch_core_codec_destroy_func_t) (switch_codec_t *);




typedef void (*switch_application_function_t) (switch_core_session_t *, const char *);
#define SWITCH_STANDARD_APP(name) static void name (switch_core_session_t *session, const char *data)

typedef void (*switch_event_callback_t) (switch_event_t *);
typedef switch_caller_extension_t *(*switch_dialplan_hunt_function_t) (switch_core_session_t *, void *, switch_caller_profile_t *);
#define SWITCH_STANDARD_DIALPLAN(name) static switch_caller_extension_t * name (switch_core_session_t *session, void *arg, switch_caller_profile_t *caller_profile)


typedef struct switch_scheduler_task switch_scheduler_task_t;

typedef void (*switch_scheduler_func_t) (switch_scheduler_task_t *task);

#define SWITCH_STANDARD_SCHED_FUNC(name) static void name (switch_scheduler_task_t *task)

typedef switch_status_t (*switch_state_handler_t) (switch_core_session_t *);
typedef struct switch_stream_handle switch_stream_handle_t;
typedef switch_status_t (*switch_stream_handle_write_function_t) (switch_stream_handle_t *handle, const char *fmt, ...);
typedef switch_status_t (*switch_stream_handle_raw_write_function_t) (switch_stream_handle_t *handle, uint8_t *data, switch_size_t datalen);

typedef switch_status_t (*switch_api_function_t) (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session,
												  _In_ switch_stream_handle_t *stream);

#define SWITCH_STANDARD_API(name) static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)

typedef switch_status_t (*switch_input_callback_function_t) (switch_core_session_t *session, void *input,
															 switch_input_type_t input_type, void *buf, unsigned int buflen);
typedef switch_status_t (*switch_read_frame_callback_function_t) (switch_core_session_t *session, switch_frame_t *frame, void *user_data);
typedef struct switch_say_interface switch_say_interface_t;
typedef struct {
	switch_input_callback_function_t input_callback;
	void *buf;
	uint32_t buflen;
	switch_read_frame_callback_function_t read_frame_callback;
	void *user_data;
} switch_input_args_t;
typedef switch_status_t (*switch_say_callback_t) (switch_core_session_t *session,
												  char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args);
typedef struct switch_xml *switch_xml_t;
typedef struct switch_core_time_duration switch_core_time_duration_t;
typedef switch_xml_t (*switch_xml_search_function_t) (const char *section,
													  const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
													  void *user_data);

typedef struct switch_hash switch_hash_t;
struct HashElem;
typedef struct HashElem switch_hash_index_t;

struct switch_network_list;
typedef struct switch_network_list switch_network_list_t;


#define SWITCH_API_VERSION 3
#define SWITCH_MODULE_LOAD_ARGS (switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
#define SWITCH_MODULE_RUNTIME_ARGS (void)
#define SWITCH_MODULE_SHUTDOWN_ARGS (void)
typedef switch_status_t (*switch_module_load_t) SWITCH_MODULE_LOAD_ARGS;
typedef switch_status_t (*switch_module_runtime_t) SWITCH_MODULE_RUNTIME_ARGS;
typedef switch_status_t (*switch_module_shutdown_t) SWITCH_MODULE_SHUTDOWN_ARGS;
#define SWITCH_MODULE_LOAD_FUNCTION(name) switch_status_t name SWITCH_MODULE_LOAD_ARGS
#define SWITCH_MODULE_RUNTIME_FUNCTION(name) switch_status_t name SWITCH_MODULE_RUNTIME_ARGS
#define SWITCH_MODULE_SHUTDOWN_FUNCTION(name) switch_status_t name SWITCH_MODULE_SHUTDOWN_ARGS

typedef enum {
	SMODF_NONE = 0,
	SMODF_GLOBAL_SYMBOLS = (1 << 0)
} switch_module_flag_enum_t;
typedef uint32_t switch_module_flag_t;

typedef struct switch_loadable_module_function_table {
	int switch_api_version;
	switch_module_load_t load;
	switch_module_shutdown_t shutdown;
	switch_module_runtime_t runtime;
	switch_module_flag_t flags;
} switch_loadable_module_function_table_t;

#define SWITCH_MODULE_DEFINITION_EX(name, load, shutdown, runtime, flags)					\
static const char modname[] =  #name ;														\
SWITCH_MOD_DECLARE_DATA switch_loadable_module_function_table_t name##_module_interface = {	\
	SWITCH_API_VERSION,																		\
	load,																					\
	shutdown,																				\
	runtime,																				\
	flags																					\
}

#define SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)								\
		SWITCH_MODULE_DEFINITION_EX(name, load, shutdown, runtime, SMODF_NONE)

/* things we don't deserve to know about */
/*! \brief A channel */
struct switch_channel;
/*! \brief A core session representing a call and all of it's resources */
struct switch_core_session;
/*! \brief An audio bug */
struct switch_media_bug;
/*! \brief A digit stream parser object */
struct switch_ivr_digit_stream_parser;

SWITCH_END_EXTERN_C
#endif
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
