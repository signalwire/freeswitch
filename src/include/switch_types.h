/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Bret McDanel <trixter AT 0xdecafbad dot com>
 * Joseph Sullivan <jossulli@amazon.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
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
#include <switch_json.h>

SWITCH_BEGIN_EXTERN_C
#define SWITCH_ENT_ORIGINATE_DELIM ":_:"
#define SWITCH_BLANK_STRING ""
#define SWITCH_TON_UNDEF 255
#define SWITCH_NUMPLAN_UNDEF 255
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
#define SWITCH_DEFAULT_CLID_NAME ""
#define SWITCH_DEFAULT_CLID_NUMBER "0000000000"
#define SWITCH_DEFAULT_DTMF_DURATION 2000
#define SWITCH_MIN_DTMF_DURATION 400
#define SWITCH_MAX_DTMF_DURATION 192000
#define SWITCH_DEFAULT_DIR_PERMS SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE | SWITCH_FPROT_UEXECUTE | SWITCH_FPROT_GREAD | SWITCH_FPROT_GEXECUTE
#ifdef WIN32
#define SWITCH_PATH_SEPARATOR "/"
#else
#define SWITCH_PATH_SEPARATOR "/"
#endif
#define SWITCH_URL_SEPARATOR "://"
#define SWITCH_IGNORE_DISPLAY_UPDATES_VARIABLE "ignore_display_updates"
#define SWITCH_AUDIO_SPOOL_PATH_VARIABLE "audio_spool_path"
#define SWITCH_BRIDGE_HANGUP_CAUSE_VARIABLE "bridge_hangup_cause"
#define SWITCH_READ_TERMINATOR_USED_VARIABLE "read_terminator_used"
#define SWITCH_SEND_SILENCE_WHEN_IDLE_VARIABLE "send_silence_when_idle"
#define SWITCH_CURRENT_APPLICATION_VARIABLE "current_application"
#define SWITCH_CURRENT_APPLICATION_DATA_VARIABLE "current_application_data"
#define SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE "current_application_response"
#define SWITCH_PASSTHRU_PTIME_MISMATCH_VARIABLE "passthru_ptime_mismatch"
#define SWITCH_ENABLE_HEARTBEAT_EVENTS_VARIABLE "enable_heartbeat_events"
#define SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE "bypass_media_after_bridge"
#define SWITCH_READ_RESULT_VARIABLE "read_result"
#define SWITCH_ATT_XFER_RESULT_VARIABLE "att_xfer_result"
#define SWITCH_COPY_XML_CDR_VARIABLE "copy_xml_cdr"
#define SWITCH_COPY_JSON_CDR_VARIABLE "copy_json_cdr"
#define SWITCH_CURRENT_APPLICATION_VARIABLE "current_application"
#define SWITCH_PROTO_SPECIFIC_HANGUP_CAUSE_VARIABLE "proto_specific_hangup_cause"
#define SWITCH_TRANSFER_HISTORY_VARIABLE "transfer_history"
#define SWITCH_TRANSFER_SOURCE_VARIABLE "transfer_source"
#define SWITCH_SENSITIVE_DTMF_VARIABLE "sensitive_dtmf"
#define SWITCH_RECORD_POST_PROCESS_EXEC_APP_VARIABLE "record_post_process_exec_app"
#define SWITCH_RECORD_POST_PROCESS_EXEC_API_VARIABLE "record_post_process_exec_api"

#define SWITCH_CHANNEL_EXECUTE_ON_ANSWER_VARIABLE "execute_on_answer"
#define SWITCH_CHANNEL_EXECUTE_ON_PRE_ANSWER_VARIABLE "execute_on_pre_answer"
#define SWITCH_CHANNEL_EXECUTE_ON_MEDIA_VARIABLE "execute_on_media"
#define SWITCH_CHANNEL_EXECUTE_ON_RING_VARIABLE "execute_on_ring"
#define SWITCH_CHANNEL_EXECUTE_ON_TONE_DETECT_VARIABLE "execute_on_tone_detect"
#define SWITCH_CHANNEL_EXECUTE_ON_ORIGINATE_VARIABLE "execute_on_originate"
#define SWITCH_CHANNEL_EXECUTE_ON_POST_ORIGINATE_VARIABLE "execute_on_post_originate"
#define SWITCH_CHANNEL_EXECUTE_ON_PRE_ORIGINATE_VARIABLE "execute_on_pre_originate"

#define SWITCH_CHANNEL_EXECUTE_ON_PRE_BRIDGE_VARIABLE "execute_on_pre_bridge"
#define SWITCH_CHANNEL_EXECUTE_ON_POST_BRIDGE_VARIABLE "execute_on_post_bridge"

#define SWITCH_CHANNEL_API_ON_ANSWER_VARIABLE "api_on_answer"
#define SWITCH_CHANNEL_API_ON_PRE_ANSWER_VARIABLE "api_on_pre_answer"
#define SWITCH_CHANNEL_API_ON_MEDIA_VARIABLE "api_on_media"
#define SWITCH_CHANNEL_API_ON_RING_VARIABLE "api_on_ring"
#define SWITCH_CHANNEL_API_ON_TONE_DETECT_VARIABLE "api_on_tone_detect"
#define SWITCH_CHANNEL_API_ON_ORIGINATE_VARIABLE "api_on_originate"
#define SWITCH_CHANNEL_API_ON_POST_ORIGINATE_VARIABLE "api_on_post_originate"
#define SWITCH_CHANNEL_API_ON_PRE_ORIGINATE_VARIABLE "api_on_pre_originate"

#define SWITCH_CALL_TIMEOUT_VARIABLE "call_timeout"
#define SWITCH_HOLDING_UUID_VARIABLE "holding_uuid"
#define SWITCH_SOFT_HOLDING_UUID_VARIABLE "soft_holding_uuid"
#define SWITCH_API_BRIDGE_END_VARIABLE "api_after_bridge"
#define SWITCH_API_BRIDGE_START_VARIABLE "api_before_bridge"
#define SWITCH_API_HANGUP_HOOK_VARIABLE "api_hangup_hook"
#define SWITCH_API_REPORTING_HOOK_VARIABLE "api_reporting_hook"
#define SWITCH_SESSION_IN_HANGUP_HOOK_VARIABLE "session_in_hangup_hook"
#define SWITCH_PROCESS_CDR_VARIABLE "process_cdr"
#define SWITCH_SKIP_CDR_CAUSES_VARIABLE "skip_cdr_causes"
#define SWITCH_FORCE_PROCESS_CDR_VARIABLE "force_process_cdr"
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
#define SWITCH_ZRTP_PASSTHRU_VARIABLE "zrtp_passthru"
#define SWITCH_ENDPOINT_DISPOSITION_VARIABLE "endpoint_disposition"
#define SWITCH_HOLD_MUSIC_VARIABLE "hold_music"
#define SWITCH_TEMP_HOLD_MUSIC_VARIABLE "temp_hold_music"
#define SWITCH_EXPORT_VARS_VARIABLE "export_vars"
#define SWITCH_BRIDGE_EXPORT_VARS_VARIABLE "bridge_export_vars"
#define SWITCH_R_SDP_VARIABLE "switch_r_sdp"
#define SWITCH_L_SDP_VARIABLE "switch_l_sdp"
#define SWITCH_B_SDP_VARIABLE "switch_m_sdp"
#define SWITCH_BRIDGE_VARIABLE "bridge_to"
#define SWITCH_LAST_BRIDGE_VARIABLE "last_bridge_to"
#define SWITCH_SIGNAL_BRIDGE_VARIABLE "signal_bridge_to"
#define SWITCH_SIGNAL_BOND_VARIABLE "signal_bond"
#define SWITCH_ORIGINATE_SIGNAL_BOND_VARIABLE "originate_signal_bond"
#define SWITCH_ORIGINATOR_VARIABLE "originator"
#define SWITCH_ORIGINATOR_CODEC_VARIABLE "originator_codec"
#define SWITCH_ORIGINATOR_VIDEO_CODEC_VARIABLE "originator_video_codec"
#define SWITCH_LOCAL_MEDIA_IP_VARIABLE "local_media_ip"
#define SWITCH_LOCAL_MEDIA_PORT_VARIABLE "local_media_port"
#define SWITCH_ADVERTISED_MEDIA_IP_VARIABLE "advertised_media_ip"
#define SWITCH_REMOTE_MEDIA_IP_VARIABLE "remote_media_ip"
#define SWITCH_REMOTE_MEDIA_PORT_VARIABLE "remote_media_port"
#define SWITCH_REMOTE_VIDEO_IP_VARIABLE "remote_video_ip"
#define SWITCH_REMOTE_VIDEO_PORT_VARIABLE "remote_video_port"
#define SWITCH_LOCAL_VIDEO_IP_VARIABLE "local_video_ip"
#define SWITCH_LOCAL_VIDEO_PORT_VARIABLE "local_video_port"
#define SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE "hangup_after_bridge"
#define SWITCH_PARK_AFTER_BRIDGE_VARIABLE "park_after_bridge"
#define SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE "transfer_after_bridge"
#define SWITCH_EXEC_AFTER_BRIDGE_APP_VARIABLE "exec_after_bridge_app"
#define SWITCH_EXEC_AFTER_BRIDGE_ARG_VARIABLE "exec_after_bridge_arg"
#define SWITCH_MAX_FORWARDS_VARIABLE "max_forwards"
#define SWITCH_DISABLE_APP_LOG_VARIABLE "disable_app_log"
#define SWITCH_SPEECH_KEY "speech"
#define SWITCH_UUID_BRIDGE "uuid_bridge"
#define SWITCH_BITS_PER_BYTE 8
#define SWITCH_DEFAULT_FILE_BUFFER_LEN 65536
#define SWITCH_DTMF_LOG_LEN 1000
#define SWITCH_MAX_TRANS 2000
#define SWITCH_CORE_SESSION_MAX_PRIVATES 2

/* Jitter */
#define JITTER_VARIANCE_THRESHOLD 400.0
/* IPDV */
#define IPDV_THRESHOLD 1.0
/* Burst and Lost Rate */
#define LOST_BURST_ANALYZE 500
/* Burst */
#define LOST_BURST_CAPTURE 1024

typedef uint8_t switch_byte_t;

typedef enum {
	SWITCH_PVT_PRIMARY = 0,
	SWITCH_PVT_SECONDARY
} switch_pvt_class_t;


/*!
  \enum switch_dtmf_source_t
  \brief DTMF sources
<pre>
    SWITCH_DTMF_UNKNOWN             - Unknown source
    SWITCH_DTMF_INBAND_AUDIO        - From audio
    SWITCH_DTMF_RTP                 - From RTP as a telephone event
    SWITCH_DTMF_ENDPOINT            - From endpoint signaling
    SWITCH_DTMF_APP                 - From application
</pre>
 */
typedef enum {
	SWITCH_DTMF_UNKNOWN,
	SWITCH_DTMF_INBAND_AUDIO,
	SWITCH_DTMF_RTP,
	SWITCH_DTMF_ENDPOINT,
	SWITCH_DTMF_APP
} switch_dtmf_source_t;

typedef enum {
	DIGIT_TARGET_SELF,
	DIGIT_TARGET_PEER,
	DIGIT_TARGET_BOTH
} switch_digit_action_target_t;



typedef enum {
	DTMF_FLAG_SKIP_PROCESS = (1 << 0),
	DTMF_FLAG_SENSITIVE = (1 << 1)
} dtmf_flag_t;

typedef struct {
	char digit;
	uint32_t duration;
	int32_t flags;
	switch_dtmf_source_t source;
} switch_dtmf_t;

typedef enum {
	SWITCH_CALL_DIRECTION_INBOUND,
	SWITCH_CALL_DIRECTION_OUTBOUND
} switch_call_direction_t;

typedef enum {
	SBF_DIAL_ALEG = (1 << 0),
	SBF_EXEC_ALEG = (1 << 1),
	SBF_DIAL_BLEG = (1 << 2),
	SBF_EXEC_BLEG = (1 << 3),
	SBF_EXEC_OPPOSITE = (1 << 4),
	SBF_EXEC_SAME = (1 << 5),
	SBF_ONCE = (1 << 6),
	SBF_EXEC_INLINE = (1 << 7)
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
	SOF_NO_EFFECTIVE_ANI = (1 << 2),
	SOF_NO_EFFECTIVE_ANIII = (1 << 3),
	SOF_NO_EFFECTIVE_CID_NUM = (1 << 4),
	SOF_NO_EFFECTIVE_CID_NAME = (1 << 5),
	SOF_NO_LIMITS = (1 << 6)
} switch_originate_flag_enum_t;
typedef uint32_t switch_originate_flag_t;

typedef enum {
	SPF_NONE = 0,
	SPF_ODD = (1 << 0),
	SPF_EVEN = (1 << 1),
	SPF_ROBUST_TCP = (1 << 2),
	SPF_ROBUST_UDP = (1 << 3)
} switch_port_flag_enum_t;
typedef uint32_t switch_port_flag_t;

typedef enum {
	ED_NONE = 0,
	ED_MUX_READ = (1 << 0),
	ED_MUX_WRITE = (1 << 1),
	ED_DTMF = (1 << 2),
	ED_COPY_DISPLAY = (1 << 3)
} switch_eavesdrop_flag_enum_t;
typedef uint32_t switch_eavesdrop_flag_t;

typedef enum {
	SCF_NONE = 0,
	SCF_USE_SQL = (1 << 0),
	SCF_NO_NEW_OUTBOUND_SESSIONS = (1 << 1),
	SCF_NO_NEW_INBOUND_SESSIONS = (1 << 2),
	SCF_NO_NEW_SESSIONS = (SCF_NO_NEW_OUTBOUND_SESSIONS | SCF_NO_NEW_INBOUND_SESSIONS),
	SCF_SHUTTING_DOWN = (1 << 3),
	SCF_VG = (1 << 4),
	SCF_RESTART = (1 << 5),
	SCF_SHUTDOWN_REQUESTED = (1 << 6),
	SCF_USE_AUTO_NAT = (1 << 7),
	SCF_EARLY_HANGUP = (1 << 8),
	SCF_CALIBRATE_CLOCK = (1 << 9),
	SCF_USE_HEAVY_TIMING = (1 << 10),
	SCF_USE_CLOCK_RT = (1 << 11),
	SCF_VERBOSE_EVENTS = (1 << 12),
	SCF_USE_WIN32_MONOTONIC = (1 << 13),
	SCF_AUTO_SCHEMAS = (1 << 14),
	SCF_MINIMAL = (1 << 15),
	SCF_USE_NAT_MAPPING = (1 << 16),
	SCF_CLEAR_SQL = (1 << 17),
	SCF_THREADED_SYSTEM_EXEC = (1 << 18),
	SCF_SYNC_CLOCK_REQUESTED = (1 << 19),
	SCF_CORE_NON_SQLITE_DB_REQ = (1 << 20),
	SCF_DEBUG_SQL = (1 << 21),
	SCF_API_EXPANSION = (1 << 22),
	SCF_SESSION_THREAD_POOL = (1 << 23)
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
	SWITCH_MANAGEMENT_INTERFACE,
	SWITCH_LIMIT_INTERFACE,
	SWITCH_CHAT_APPLICATION_INTERFACE,
	SWITCH_JSON_API_INTERFACE,
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

/* WARNING, Do not forget to update *SAY_METHOD_NAMES[] in src/switch_ivr_play_say.c */
typedef enum {
	SSM_NA,
	SSM_PRONOUNCED,
	SSM_ITERATED,
	SSM_COUNTED,
	SSM_PRONOUNCED_YEAR
} switch_say_method_t;

/* WARNING, Do not forget to update *SAY_TYPE_NAMES[] in src/switch_ivr_say.c */
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
	SST_NAME_PHONETIC,
	SST_SHORT_DATE_TIME
} switch_say_type_t;

typedef enum {
	SSG_MASCULINE,
	SSG_FEMININE,
	SSG_NEUTER,
	SSG_UTRUM
} switch_say_gender_t;

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
	SMF_IMMEDIATE = (1 << 6),
	SMF_EXEC_INLINE = (1 << 7),
	SMF_PRIORITY = (1 << 8)
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
	SWITCH_ABC_TYPE_TAP_NATIVE_READ,
	SWITCH_ABC_TYPE_TAP_NATIVE_WRITE,
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
	char *run_dir;
	char *db_dir;
	char *script_dir;
	char *temp_dir;
	char *htdocs_dir;
	char *grammar_dir;
	char *storage_dir;
	char *recordings_dir;
	char *sounds_dir;
	char *lib_dir;
	char *certs_dir;
};

typedef struct switch_directories switch_directories;
SWITCH_DECLARE_DATA extern switch_directories SWITCH_GLOBAL_dirs;

struct switch_filenames {
    char *conf_name;
};

typedef struct switch_filenames switch_filenames;
SWITCH_DECLARE_DATA extern switch_filenames SWITCH_GLOBAL_filenames;

#define SWITCH_MAX_STACKS 16
#define SWITCH_THREAD_STACKSIZE 240 * 1024
#define SWITCH_SYSTEM_THREAD_STACKSIZE 8192 * 1024
#define SWITCH_MAX_INTERVAL 120	/* we only do up to 120ms */
#define SWITCH_INTERVAL_PAD 10	/* A little extra buffer space to be safe */
#define SWITCH_MAX_SAMPLE_LEN 48
#define SWITCH_BYTES_PER_SAMPLE 2	/* slin is 2 bytes per sample */
#define SWITCH_RECOMMENDED_BUFFER_SIZE 4096	/* worst case of 32khz @60ms we only do 48khz @10ms which is 960 */
#define SWITCH_MAX_CODECS 50
#define SWITCH_MAX_STATE_HANDLERS 30
#define SWITCH_CORE_QUEUE_LEN 100000
#define SWITCH_MAX_MANAGEMENT_BUFFER_LEN 1024 * 8

#define SWITCH_ACCEPTABLE_INTERVAL(_i) (_i && _i <= SWITCH_MAX_INTERVAL && (_i % 10) == 0)

typedef enum {
	SWITCH_CPF_NONE = 0,
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
	SWITCH_XML_SECTION_LANGUAGES = (1 << 3),
	SWITCH_XML_SECTION_CHATPLAN = (1 << 4),

	/* Nothing after this line */
	SWITCH_XML_SECTION_MAX = (1 << 4)
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

typedef struct {
	switch_size_t raw_bytes;
	switch_size_t media_bytes;
	switch_size_t packet_count;
	switch_size_t period_packet_count;
	switch_size_t media_packet_count;
	switch_size_t skip_packet_count;
	switch_size_t jb_packet_count;
	switch_size_t dtmf_packet_count;
	switch_size_t cng_packet_count;
	switch_size_t flush_packet_count;
	switch_size_t largest_jb_size;
	/* Jitter */
	int64_t last_proc_time;		
	int64_t jitter_n;
	int64_t jitter_add;
	int64_t jitter_addsq;

	double variance;
	double min_variance;
	double max_variance;
	double std_deviation;

	/* Burst and Packet Loss */
	double lossrate;
	double burstrate;
	double mean_interval;
	int loss[LOST_BURST_CAPTURE];
	int last_loss;
	int recved;	
	int last_processed_seq;
	switch_size_t flaws;
	switch_size_t last_flaw;
	double R;
	double mos;
} switch_rtp_numbers_t;


typedef struct {
	uint32_t packet_count;
	uint32_t octet_count; 
	uint32_t peer_ssrc;
} switch_rtcp_numbers_t;

typedef struct {
	switch_rtp_numbers_t inbound;
	switch_rtp_numbers_t outbound;
	switch_rtcp_numbers_t rtcp;
	uint32_t read_count;
} switch_rtp_stats_t;

typedef enum {
	SWITCH_RTP_FLUSH_ONCE,
	SWITCH_RTP_FLUSH_STICK,
	SWITCH_RTP_FLUSH_UNSTICK
} switch_rtp_flush_t;

#define SWITCH_RTP_CNG_PAYLOAD 13

/*!
  \enum switch_rtp_flag_t
  \brief RTP Related Flags
<pre>
    SWITCH_RTP_FLAG_NOBLOCK       - Do not block
    SWITCH_RTP_FLAG_IO            - IO is ready
	SWITCH_RTP_FLAG_USE_TIMER     - Timeout Reads and replace with a CNG Frame
	SWITCH_RTP_FLAG_SECURE        - Secure RTP
	SWITCH_RTP_FLAG_AUTOADJ       - Auto-Adjust the dest based on the source
	SWITCH_RTP_FLAG_RAW_WRITE     - Try to forward packets unscathed
	SWITCH_RTP_FLAG_GOOGLEHACK    - Convert payload from 102 to 97
	SWITCH_RTP_FLAG_VAD           - Enable VAD
	SWITCH_RTP_FLAG_BREAK		  - Stop what you are doing and return SWITCH_STATUS_BREAK
	SWITCH_RTP_FLAG_DATAWAIT	  - Do not return from reads unless there is data even when non blocking
	SWITCH_RTP_FLAG_BUGGY_2833    - Emulate the bug in cisco equipment to allow interop
	SWITCH_RTP_FLAG_PASS_RFC2833  - Pass 2833 (ignore it)
	SWITCH_RTP_FLAG_AUTO_CNG      - Generate outbound CNG frames when idle    
</pre>
 */
typedef enum {
	SWITCH_RTP_FLAG_NOBLOCK = 0,
	SWITCH_RTP_FLAG_DTMF_ON,
	SWITCH_RTP_FLAG_IO,
	SWITCH_RTP_FLAG_USE_TIMER,
	SWITCH_RTP_FLAG_RTCP_PASSTHRU,
	SWITCH_RTP_FLAG_SECURE_SEND,
	SWITCH_RTP_FLAG_SECURE_RECV,
	SWITCH_RTP_FLAG_AUTOADJ,
	SWITCH_RTP_FLAG_RAW_WRITE,
	SWITCH_RTP_FLAG_GOOGLEHACK,
	SWITCH_RTP_FLAG_VAD,
	SWITCH_RTP_FLAG_BREAK,
	SWITCH_RTP_FLAG_UDPTL,
	SWITCH_RTP_FLAG_DATAWAIT,
	SWITCH_RTP_FLAG_BYTESWAP,
	SWITCH_RTP_FLAG_PASS_RFC2833,
	SWITCH_RTP_FLAG_AUTO_CNG,
	SWITCH_RTP_FLAG_SECURE_SEND_RESET,
	SWITCH_RTP_FLAG_SECURE_RECV_RESET,
	SWITCH_RTP_FLAG_PROXY_MEDIA,
	SWITCH_RTP_FLAG_SHUTDOWN,
	SWITCH_RTP_FLAG_FLUSH,
	SWITCH_RTP_FLAG_AUTOFLUSH,
	SWITCH_RTP_FLAG_STICKY_FLUSH,
	SWITCH_ZRTP_FLAG_SECURE_SEND,
	SWITCH_ZRTP_FLAG_SECURE_RECV,
	SWITCH_ZRTP_FLAG_SECURE_MITM_SEND,
	SWITCH_ZRTP_FLAG_SECURE_MITM_RECV,
	SWITCH_RTP_FLAG_DEBUG_RTP_READ,
	SWITCH_RTP_FLAG_DEBUG_RTP_WRITE,
	SWITCH_RTP_FLAG_VIDEO,
	SWITCH_RTP_FLAG_ENABLE_RTCP,
	SWITCH_RTP_FLAG_RTCP_MUX,
	SWITCH_RTP_FLAG_KILL_JB,
	SWITCH_RTP_FLAG_VIDEO_BREAK,
	SWITCH_RTP_FLAG_PAUSE,
	SWITCH_RTP_FLAG_FIR,
	SWITCH_RTP_FLAG_PLI,
	SWITCH_RTP_FLAG_RESET,
	SWITCH_RTP_FLAG_INVALID
} switch_rtp_flag_t;


typedef enum {
	RTP_BUG_NONE = 0,			/* won't be using this one much ;) */

	RTP_BUG_CISCO_SKIP_MARK_BIT_2833 = (1 << 0),
	/* Some Cisco devices get mad when you send the mark bit on new 2833 because it makes
	   them flush their jitterbuffer and the dtmf along with it.

	   This flag will disable the sending of the mark bit on the first DTMF packet.
	 */


	RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833 = (1 << 1),
		/*
		   Sonus wrongly expects that, when sending a multi-packet 2833 DTMF event, The sender
		   should increment the RTP timestamp in each packet when, in reality, the sender should
		   send the same exact timestamp and increment the duration field in the 2833 payload.
		   This allows a reconstruction of the duration if any of the packets are lost.

		   final_duration - initial_timestamp = total_samples

		   However, if the duration value exceeds the space allocated (16 bits), The sender should increment
		   the timestamp one unit and reset the duration to 0. 

		   Always sending a duration of 0 with a new timestamp should be tolerated but is rarely intentional
		   and is mistakenly done by many devices.  
		   The issue is that the Sonus expects everyone to do it this way instead of tolerating either way.
		   Sonus will actually ignore every packet with the same timestamp before concluding if it's DTMF.

		   This flag will cause each packet to have a new timestamp.
		 */


	RTP_BUG_IGNORE_MARK_BIT = (1 << 2),

	/*
	  A Huawei SBC has been discovered that sends the mark bit on every single RTP packet.
	  Since this causes the RTP stack to flush it's buffers, it horribly messes up the timing on the channel.

	  This flag will do nothing when an inbound packet contains the mark bit.

	 */

	
	RTP_BUG_SEND_LINEAR_TIMESTAMPS = (1 << 3),

	/*
	  Our friends at Sonus get real mad when the timestamps are not in perfect sequence even during periods of silence.
	  With this flag, we will only increment the timestamp when write packets even if they are eons apart.
	  
	 */

	RTP_BUG_START_SEQ_AT_ZERO = (1 << 4),

	/*
	  Our friends at Sonus also get real mad if the sequence number does not start at 0.  
	  Typically, we set this to a random starting value for your saftey.
	  This is a security risk you take upon yourself when you enable this flag.
	 */


	RTP_BUG_NEVER_SEND_MARKER = (1 << 5),

	/*
	  Our friends at Sonus are on a roll, They also get easily dumbfounded by marker bits.
	  This flag will never send any. Sheesh....
	 */
	
	RTP_BUG_IGNORE_DTMF_DURATION = (1 << 6),
	
	/*
	  Guess Who? ... Yep, Sonus (and who know's who else) likes to interweave DTMF with the audio stream making it take
	  2X as long as it should and sending an incorrect duration making the DTMF very delayed.
	  This flag will treat every dtmf as if it were 50ms and queue it on recipt of the leading packet rather than at the end.
	 */


	RTP_BUG_ACCEPT_ANY_PACKETS = (1 << 7),

	/*
	  Oracle's Contact Center Anywhere (CCA) likes to use a single RTP socket to send all its outbound audio.
	  This messes up our ability to auto adjust to NATTED RTP and causes us to ignore its audio packets.
	  This flag will allow compatibility with this dying product.
	*/


	RTP_BUG_GEN_ONE_GEN_ALL = (1 << 8),

	/*
	  Some RTP endpoints (and by some we mean *cough* _SONUS_!) do not like it when the timestamps jump forward or backwards in time.
	  So say you are generating a file that says "please wait for me to complete your call, or generating ringback"
	  Now you place and outbound call and you are bridging.  Well, while you were playing the file, you were generating your own RTP timestamps.
	  But, now that you have a remote RTP stream, you'd rather send those timestamps as-is in case they will be fed to a remote jitter buffer......
	  Ok, so this causes the audio to completely fade out despite the fact that we send the mark bit which should give them heads up its happening.

	  Sigh, This flag will tell FreeSWITCH that if it ever generates even one RTP packet itself, to continue to generate all of them and ignore the
	  actual timestamps in the frames.

	 */

	RTP_BUG_CHANGE_SSRC_ON_MARKER = (1 << 9),

	/*
	  By default FS will change the SSRC when the marker is set and it detects a timestamp reset.
	  If this setting is enabled it will NOT do this (old behaviour).
	 */

	RTP_BUG_FLUSH_JB_ON_DTMF = (1 << 10),
	
	/* FLUSH JITTERBUFFER When getting RFC2833 to reduce bleed through */

	RTP_BUG_ACCEPT_ANY_PAYLOAD = (1 << 11)

	/* 
	  Make FS accept any payload type instead of dropping and returning CNG frame. Workaround while FS only supports a single payload per rtp session.
	  This can be used by endpoint modules to detect payload changes and act appropriately (ex: sofia could send a reINVITE with single codec).
	  This should probably be a flag, but flag enum is already full!
	*/

} switch_rtp_bug_flag_t;

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

typedef struct {
	unsigned length:16;			/* length                 */
	unsigned profile:16;		/* defined by profile     */
} switch_rtp_hdr_ext_t;

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

typedef struct {
	unsigned profile:16;		/* defined by profile     */
	unsigned length:16;			/* length                 */
} switch_rtp_hdr_ext_t;

#endif

#ifdef _MSC_VER
#pragma pack(pop, r1)
#endif

#ifdef _MSC_VER
#pragma pack(push, r1, 1)
#endif

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
typedef struct {
	unsigned version:2;			/* protocol version                  */
	unsigned p:1;				/* padding flag                      */
	unsigned count:5;			/* number of reception report blocks */
	unsigned type:8;			/* packet type                       */
	unsigned length:16;			/* length in 32-bit words - 1        */
} switch_rtcp_hdr_t;

#else /*  BIG_ENDIAN */

typedef struct {
	unsigned count:5;			/* number of reception report blocks */
	unsigned p:1;				/* padding flag                      */
	unsigned version:2;			/* protocol version                  */
	unsigned type:8;			/* packet type                       */
	unsigned length:16;			/* length in 32-bit words - 1        */
} switch_rtcp_hdr_t;

#endif

#ifdef _MSC_VER
#pragma pack(pop, r1)
#endif

typedef struct audio_buffer_header_s {
	uint32_t ts;
	uint32_t len;
} audio_buffer_header_t;


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
	SWITCH_MESSAGE_INDICATE_AUDIO_SYNC,
	SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA,
	SWITCH_MESSAGE_INDICATE_UUID_CHANGE,
	SWITCH_MESSAGE_INDICATE_SIMPLIFY,
	SWITCH_MESSAGE_INDICATE_DEBUG_MEDIA,
	SWITCH_MESSAGE_INDICATE_PROXY_MEDIA,
	SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC,
	SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC_COMPLETE,
	SWITCH_MESSAGE_INDICATE_PHONE_EVENT,
	SWITCH_MESSAGE_INDICATE_T38_DESCRIPTION,
	SWITCH_MESSAGE_INDICATE_UDPTL_MODE,
	SWITCH_MESSAGE_INDICATE_CLEAR_PROGRESS,
	SWITCH_MESSAGE_INDICATE_JITTER_BUFFER,
	SWITCH_MESSAGE_INDICATE_RECOVERY_REFRESH,
	SWITCH_MESSAGE_INDICATE_SIGNAL_DATA,
	SWITCH_MESSAGE_INDICATE_MESSAGE,
	SWITCH_MESSAGE_INDICATE_INFO,
	SWITCH_MESSAGE_INDICATE_AUDIO_DATA,
	SWITCH_MESSAGE_INDICATE_BLIND_TRANSFER_RESPONSE,
	SWITCH_MESSAGE_INDICATE_STUN_ERROR,
	SWITCH_MESSAGE_INDICATE_MEDIA_RENEG,
	SWITCH_MESSAGE_INDICATE_KEEPALIVE,
	SWITCH_MESSAGE_REFER_EVENT,
	SWITCH_MESSAGE_ANSWER_EVENT,
	SWITCH_MESSAGE_PROGRESS_EVENT,
	SWITCH_MESSAGE_RING_EVENT,
	SWITCH_MESSAGE_RESAMPLE_EVENT,
	SWITCH_MESSAGE_HEARTBEAT_EVENT,
	SWITCH_MESSAGE_INVALID
} switch_core_session_message_types_t;

typedef struct {
	uint16_t T38FaxVersion;
	uint32_t T38MaxBitRate;
	switch_bool_t T38FaxFillBitRemoval;
	switch_bool_t T38FaxTranscodingMMR;
	switch_bool_t T38FaxTranscodingJBIG;
	const char *T38FaxRateManagement;
	uint32_t T38FaxMaxBuffer;
	uint32_t T38FaxMaxDatagram;
	const char *T38FaxUdpEC;
	const char *T38VendorInfo;
	const char *remote_ip;
	uint16_t remote_port;
	const char *local_ip;
	uint16_t local_port;
	const char *sdp_o_line;
} switch_t38_options_t;

/*!
  \enum switch_stack_t
  \brief Expression of how to stack a list
<pre>
SWITCH_STACK_BOTTOM - Stack on the bottom
SWITCH_STACK_TOP	- Stack on the top
</pre>
 */
typedef enum {
	SWITCH_STACK_BOTTOM = (1 << 0),
	SWITCH_STACK_TOP = (1 << 1),
	SWITCH_STACK_NODUP = (1 << 2),
	SWITCH_STACK_UNSHIFT = (1 << 3),
	SWITCH_STACK_PUSH = (1 << 4),
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
	SWITCH_STATUS_INTR,
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
	SWITCH_STATUS_FOUND,
	SWITCH_STATUS_CONTINUE,
	SWITCH_STATUS_TERM,
	SWITCH_STATUS_NOT_INITALIZED,
	SWITCH_STATUS_XBREAK = 35,
	SWITCH_STATUS_WINBREAK = 730035
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
	SWITCH_LOG_DEBUG10 = 110,
	SWITCH_LOG_DEBUG9 = 109,
	SWITCH_LOG_DEBUG8 = 108,
	SWITCH_LOG_DEBUG7 = 107,
	SWITCH_LOG_DEBUG6 = 106,
	SWITCH_LOG_DEBUG5 = 105,
	SWITCH_LOG_DEBUG4 = 104,
	SWITCH_LOG_DEBUG3 = 103,
	SWITCH_LOG_DEBUG2 = 102,
	SWITCH_LOG_DEBUG1 = 101,
	SWITCH_LOG_DEBUG = 7,
	SWITCH_LOG_INFO = 6,
	SWITCH_LOG_NOTICE = 5,
	SWITCH_LOG_WARNING = 4,
	SWITCH_LOG_ERROR = 3,
	SWITCH_LOG_CRIT = 2,
	SWITCH_LOG_ALERT = 1,
	SWITCH_LOG_CONSOLE = 0,
	SWITCH_LOG_INVALID = 64,
	SWITCH_LOG_UNINIT = 1000,
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
	SWITCH_CHANNEL_ID_EVENT,
	SWITCH_CHANNEL_ID_SESSION
} switch_text_channel_t;

typedef enum {
	SCSMF_DYNAMIC = (1 << 0),
	SCSMF_FREE_STRING_REPLY = (1 << 1),
	SCSMF_FREE_POINTER_REPLY = (1 << 2)
} switch_core_session_message_flag_enum_t;
typedef uint32_t switch_core_session_message_flag_t;

#define SWITCH_CHANNEL_LOG SWITCH_CHANNEL_ID_LOG, __FILE__, __SWITCH_FUNC__, __LINE__, NULL
#define SWITCH_CHANNEL_LOG_CLEAN SWITCH_CHANNEL_ID_LOG_CLEAN, __FILE__, __SWITCH_FUNC__, __LINE__, NULL
#define SWITCH_CHANNEL_SESSION_LOG_CLEAN(x) SWITCH_CHANNEL_ID_LOG_CLEAN, __FILE__, __SWITCH_FUNC__, __LINE__, switch_core_session_get_uuid((x))
#define SWITCH_CHANNEL_EVENT SWITCH_CHANNEL_ID_EVENT, __FILE__, __SWITCH_FUNC__, __LINE__, NULL
#define SWITCH_CHANNEL_SESSION_LOG(x) SWITCH_CHANNEL_ID_SESSION, __FILE__, __SWITCH_FUNC__, __LINE__, (const char*)(x)
#define SWITCH_CHANNEL_CHANNEL_LOG(x) SWITCH_CHANNEL_ID_SESSION, __FILE__, __SWITCH_FUNC__, __LINE__, (const char*)switch_channel_get_session(x)
#define SWITCH_CHANNEL_UUID_LOG(x) SWITCH_CHANNEL_ID_LOG, __FILE__, __SWITCH_FUNC__, __LINE__, (x)

typedef enum {
	CCS_DOWN,
	CCS_DIALING,
	CCS_RINGING,
	CCS_EARLY,
	CCS_ACTIVE,
	CCS_HELD,
	CCS_RING_WAIT,
	CCS_HANGUP,
	CCS_UNHELD
} switch_channel_callstate_t;

typedef enum {
	SDS_DOWN,
	SDS_RINGING,
	SDS_ACTIVE,
	SDS_ACTIVE_MULTI,
	SDS_HELD,
	SDS_UNHELD,
	SDS_HANGUP
} switch_device_state_t;


/*!
  \enum switch_channel_state_t
  \brief Channel States (these are the defaults, CS_SOFT_EXECUTE, CS_EXCHANGE_MEDIA, and CS_CONSUME_MEDIA are often overridden by specific apps)
<pre>
CS_NEW       - Channel is newly created.
CS_INIT      - Channel has been initilized.
CS_ROUTING   - Channel is looking for an extension to execute.
CS_SOFT_EXECUTE  - Channel is ready to execute from 3rd party control.
CS_EXECUTE   - Channel is executing it's dialplan.
CS_EXCHANGE_MEDIA  - Channel is exchanging media with another channel.
CS_PARK      - Channel is accepting media awaiting commands.
CS_CONSUME_MEDIA		 - Channel is consuming all media and dropping it.
CS_HIBERNATE - Channel is in a sleep state.
CS_RESET 	 - Channel is in a reset state.
CS_HANGUP    - Channel is flagged for hangup and ready to end.
CS_REPORTING - Channel is ready to collect call detail.
CS_DESTROY      - Channel is ready to be destroyed and out of the state machine
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
	CS_REPORTING,
	CS_DESTROY,
	CS_NONE
} switch_channel_state_t;

typedef enum {
	SWITCH_RING_READY_NONE,
	SWITCH_RING_READY_RINGING,
	SWITCH_RING_READY_QUEUED
}  switch_ring_ready_t;


/*!
  \enum switch_channel_flag_t
  \brief Channel Flags

<pre>
CF_ANSWERED			- Channel is answered
CF_OUTBOUND			- Channel is an outbound channel
CF_EARLY_MEDIA		- Channel is ready for audio before answer 
CF_ORIGINATOR		- Channel is an originator
CF_TRANSFER			- Channel is being transfered
CF_ACCEPT_CNG		- Channel will accept CNG frames
CF_REDIRECT 		- Channel is being redirected
CF_BRIDGED			- Channel in a bridge
CF_HOLD				- Channel is on hold
CF_SERVICE			- Channel has a service thread
CF_TAGGED			- Channel is tagged
CF_WINNER			- Channel is the winner
CF_CONTROLLED		- Channel is under control
CF_PROXY_MODE		- Channel has no media
CF_SUSPEND			- Suspend i/o
CF_EVENT_PARSE		- Suspend control events
CF_GEN_RINGBACK		- Channel is generating it's own ringback
CF_RING_READY		- Channel is ready to send ringback
CF_BREAK			- Channel should stop what it's doing
CF_BROADCAST		- Channel is broadcasting
CF_UNICAST			- Channel has a unicast connection
CF_VIDEO			- Channel has video
CF_EVENT_LOCK		- Don't parse events
CF_RESET			- Tell extension parser to reset
CF_ORIGINATING		- Channel is originating
CF_STOP_BROADCAST	- Signal to stop broadcast

CF_AUDIO_PAUSE      - Audio is not ready to read/write
CF_VIDEO_PAUSE      - Video is not ready to read/write

</pre>
 */

typedef enum {
	CC_MEDIA_ACK = 1,
	CC_BYPASS_MEDIA,
	CC_PROXY_MEDIA,
	CC_JITTERBUFFER,
	CC_FS_RTP,
	CC_QUEUEABLE_DTMF_DELAY,
	/* WARNING: DO NOT ADD ANY FLAGS BELOW THIS LINE */
	CC_FLAG_MAX
} switch_channel_cap_t;

typedef enum {
	CF_ANSWERED = 1,
	CF_OUTBOUND,
	CF_EARLY_MEDIA,
	CF_BRIDGE_ORIGINATOR,
	CF_UUID_BRIDGE_ORIGINATOR,
	CF_TRANSFER,
	CF_ACCEPT_CNG,
	CF_REDIRECT,
	CF_BRIDGED,
	CF_HOLD,
	CF_SERVICE,
	CF_TAGGED,
	CF_WINNER,
	CF_CONTROLLED,
	CF_PROXY_MODE,
	CF_SUSPEND,
	CF_EVENT_PARSE,
	CF_GEN_RINGBACK,
	CF_RING_READY,
	CF_BREAK,
	CF_BROADCAST,
	CF_UNICAST,
	CF_VIDEO,
	CF_EVENT_LOCK,
	CF_EVENT_LOCK_PRI,
	CF_RESET,
	CF_ORIGINATING,
	CF_STOP_BROADCAST,
	CF_PROXY_MEDIA,
	CF_INNER_BRIDGE,
	CF_REQ_MEDIA,
	CF_VERBOSE_EVENTS,
	CF_PAUSE_BUGS,
	CF_DIVERT_EVENTS,
	CF_BLOCK_STATE,
	CF_FS_RTP,
	CF_REPORTING,
	CF_PARK,
	CF_TIMESTAMP_SET,
	CF_ORIGINATOR,
	CF_XFER_ZOMBIE,
	CF_MEDIA_ACK,
	CF_THREAD_SLEEPING,
	CF_DISABLE_RINGBACK,
	CF_NOT_READY,
	CF_SIGNAL_BRIDGE_TTL,
	CF_MEDIA_BRIDGE_TTL,
	CF_BYPASS_MEDIA_AFTER_BRIDGE,
	CF_LEG_HOLDING,
	CF_BROADCAST_DROP_MEDIA,
	CF_EARLY_HANGUP,
	CF_MEDIA_SET,
	CF_CONSUME_ON_ORIGINATE,
	CF_PASSTHRU_PTIME_MISMATCH,
	CF_BRIDGE_NOWRITE,
	CF_RECOVERED,
	CF_JITTERBUFFER,
	CF_JITTERBUFFER_PLC,
	CF_DIALPLAN,
	CF_BLEG,
	CF_BLOCK_BROADCAST_UNTIL_MEDIA,
	CF_CNG_PLC,
	CF_ATTENDED_TRANSFER,
	CF_LAZY_ATTENDED_TRANSFER,
	CF_SIGNAL_DATA,
	CF_SIMPLIFY,
	CF_ZOMBIE_EXEC,
	CF_INTERCEPT,
	CF_INTERCEPTED,
	CF_VIDEO_REFRESH_REQ,
	CF_SERVICE_AUDIO,
	CF_SERVICE_VIDEO,
	CF_ZRTP_PASSTHRU_REQ,
	CF_ZRTP_PASSTHRU,
	CF_ZRTP_HASH,
	CF_CHANNEL_SWAP,
	CF_DEVICE_LEG,
	CF_FINAL_DEVICE_LEG,
	CF_PICKUP,
	CF_CONFIRM_BLIND_TRANSFER,
	CF_NO_PRESENCE,
	CF_CONFERENCE,
	CF_CONFERENCE_ADV,
	CF_RECOVERING,
	CF_RECOVERING_BRIDGE,
	CF_TRACKED,
	CF_TRACKABLE,
	CF_NO_CDR,
	CF_EARLY_OK,
	CF_MEDIA_TRANS,
	CF_HOLD_ON_BRIDGE,
	CF_SECURE,
	CF_LIBERAL_DTMF,
	CF_SLA_BARGE,
	CF_SLA_BARGING,
	CF_PROTO_HOLD, //TFLAG_SIP_HOLD
	CF_HOLD_LOCK,
	CF_VIDEO_POSSIBLE,//TFLAG_VIDEO
	CF_NOTIMER_DURING_BRIDGE,
	CF_PASS_RFC2833,
	CF_T38_PASSTHRU,
	CF_DROP_DTMF,
	CF_REINVITE,
	CF_AUTOFLUSH_DURING_BRIDGE,
	CF_RTP_NOTIMER_DURING_BRIDGE,
	CF_WEBRTC,
	CF_WEBRTC_MOZ,
	CF_ICE,
	CF_DTLS,
	CF_VERBOSE_SDP,
	CF_DTLS_OK,
	CF_3PCC,
	CF_VIDEO_PASSIVE,
	CF_NOVIDEO,
	CF_VIDEO_ECHO,
	CF_SLA_INTERCEPT,
	CF_VIDEO_BREAK,
	CF_AUDIO_PAUSE,
	CF_VIDEO_PAUSE,
	CF_BYPASS_MEDIA_AFTER_HOLD,
	CF_HANGUP_HELD,
	CF_CONFERENCE_RESET_MEDIA,
	/* WARNING: DO NOT ADD ANY FLAGS BELOW THIS LINE */
	/* IF YOU ADD NEW ONES CHECK IF THEY SHOULD PERSIST OR ZERO THEM IN switch_core_session.c switch_core_session_request_xml() */
	CF_FLAG_MAX
} switch_channel_flag_t;


typedef enum {
	CF_APP_TAGGED = (1 << 0),
	CF_APP_T38 = (1 << 1),
	CF_APP_T38_REQ = (1 << 2),
	CF_APP_T38_FAIL = (1 << 3),
	CF_APP_T38_NEGOTIATED = (1 << 4)
} switch_channel_app_flag_t;


/*!
  \enum switch_frame_flag_t
  \brief Frame Flags

<pre>
SFF_CNG        = (1 <<  0) - Frame represents comfort noise
SFF_RAW_RTP    = (1 <<  1) - Frame has raw rtp accessible
SFF_RTP_HEADER = (1 << 2)  - Get the rtp header from the frame header
SFF_PLC        = (1 << 3)  - Frame has generated PLC data
SFF_RFC2833    = (1 << 4)  - Frame has rfc2833 dtmf data
SFF_DYNAMIC    = (1 << 5)  - Frame is dynamic and should be freed
</pre>
 */
typedef enum {
	SFF_NONE = 0,
	SFF_CNG = (1 << 0),
	SFF_RAW_RTP = (1 << 1),
	SFF_RTP_HEADER = (1 << 2),
	SFF_PLC = (1 << 3),
	SFF_RFC2833 = (1 << 4),
	SFF_PROXY_PACKET = (1 << 5),
	SFF_DYNAMIC = (1 << 6),
	SFF_ZRTP = (1 << 7),
	SFF_UDPTL_PACKET = (1 << 8),
	SFF_NOT_AUDIO = (1 << 9),
	SFF_RTCP = (1 << 10)
} switch_frame_flag_enum_t;
typedef uint32_t switch_frame_flag_t;


typedef enum {
	SAF_NONE = 0,
	SAF_SUPPORT_NOMEDIA = (1 << 0),
	SAF_ROUTING_EXEC = (1 << 1),
	SAF_MEDIA_TAP = (1 << 2),
	SAF_ZOMBIE_EXEC = (1 << 3),
	SAF_NO_LOOPBACK = (1 << 4)
} switch_application_flag_enum_t;
typedef uint32_t switch_application_flag_t;

typedef enum {
	SCAF_NONE = 0
} switch_chat_application_flag_enum_t;
typedef uint32_t switch_chat_application_flag_t;


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
	SWITCH_CODEC_FLAG_PASSTHROUGH = (1 << 7),
	SWITCH_CODEC_FLAG_READY = (1 << 8)
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
	SWITCH_SPEECH_FLAG_PAUSE = (1 << 4),
	SWITCH_SPEECH_FLAG_OPEN = (1 << 5),
	SWITCH_SPEECH_FLAG_DONE = (1 << 6)
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

typedef enum {
	SWITCH_MEDIA_TYPE_AUDIO,
	SWITCH_MEDIA_TYPE_VIDEO
} switch_media_type_t;
#define SWITCH_MEDIA_TYPE_TOTAL 2


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
SMBF_ANSWER_REQ - Don't record until the channel is answered
SMBF_BRIDGE_REQ - Don't record until the channel is bridged
SMBF_THREAD_LOCK - Only let the same thread who created the bug remove it.
SMBF_PRUNE - 
SMBF_NO_PAUSE - 
SMBF_STEREO_SWAP - Record in stereo: Write Stream - left channel, Read Stream - right channel
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
	SMBF_ANSWER_REQ = (1 << 6),
	SMBF_BRIDGE_REQ = (1 << 7),
	SMBF_THREAD_LOCK = (1 << 8),
	SMBF_PRUNE = (1 << 9),
	SMBF_NO_PAUSE = (1 << 10),
	SMBF_STEREO_SWAP = (1 << 11),
	SMBF_LOCK = (1 << 12),
	SMBF_TAP_NATIVE_READ = (1 << 13),
	SMBF_TAP_NATIVE_WRITE = (1 << 14),
	SMBF_ONE_ONLY = (1 << 15),
	SMBF_MASK = (1 << 16)
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
	SWITCH_FILE_CALLBACK = (1 << 12),
	SWITCH_FILE_DONE = (1 << 13),
	SWITCH_FILE_BUFFER_DONE = (1 << 14),
	SWITCH_FILE_WRITE_APPEND = (1 << 15),
	SWITCH_FILE_WRITE_OVER = (1 << 16),
	SWITCH_FILE_NOMUX = (1 << 17),
	SWITCH_FILE_BREAK_ON_CHANGE = (1 << 18)
} switch_file_flag_enum_t;
typedef uint32_t switch_file_flag_t;

typedef enum {
	SWITCH_IO_FLAG_NONE = 0,
	SWITCH_IO_FLAG_NOBLOCK = (1 << 0),
	SWITCH_IO_FLAG_SINGLE_READ = (1 << 1)
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
    SWITCH_EVENT_CLONE				- A cloned event
    SWITCH_EVENT_CHANNEL_CREATE		- A channel has been created
    SWITCH_EVENT_CHANNEL_DESTROY	- A channel has been destroyed
    SWITCH_EVENT_CHANNEL_STATE		- A channel has changed state
    SWITCH_EVENT_CHANNEL_CALLSTATE	- A channel has changed call state
    SWITCH_EVENT_CHANNEL_ANSWER		- A channel has been answered
    SWITCH_EVENT_CHANNEL_HANGUP		- A channel has been hungup
    SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE	- A channel has completed the hangup
    SWITCH_EVENT_CHANNEL_EXECUTE	- A channel has executed a module's application
    SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE	- A channel has finshed executing a module's application
    SWITCH_EVENT_CHANNEL_HOLD		- A channel has been put on hold
    SWITCH_EVENT_CHANNEL_UNHOLD		- A channel has been unheld
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
    SWITCH_EVENT_NOTIFY_IN			- Received incoming NOTIFY from gateway subscription
    SWITCH_EVENT_PRESENCE_OUT		- Presence out
    SWITCH_EVENT_PRESENCE_PROBE		- Presence probe
    SWITCH_EVENT_MESSAGE_WAITING	- A message is waiting
    SWITCH_EVENT_MESSAGE_QUERY		- A query for MESSAGE_WAITING events
    SWITCH_EVENT_ROSTER				- ?
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
    SWITCH_EVENT_PHONE_FEATURE		- Notification (DND/CFWD/etc)
    SWITCH_EVENT_PHONE_FEATURE_SUBSCRIBE - Phone feature subscription
    SWITCH_EVENT_SEND_MESSAGE		- Message
    SWITCH_EVENT_RECV_MESSAGE		- Message
    SWITCH_EVENT_REQUEST_PARAMS
    SWITCH_EVENT_CHANNEL_DATA
    SWITCH_EVENT_GENERAL
    SWITCH_EVENT_COMMAND
    SWITCH_EVENT_SESSION_HEARTBEAT
    SWITCH_EVENT_CLIENT_DISCONNECTED
    SWITCH_EVENT_SERVER_DISCONNECTED
    SWITCH_EVENT_SEND_INFO
    SWITCH_EVENT_RECV_INFO
    SWITCH_EVENT_RECV_RTCP_MESSAGE
    SWITCH_EVENT_CALL_SECURE
    SWITCH_EVENT_NAT            	- NAT Management (new/del/status)
    SWITCH_EVENT_RECORD_START
    SWITCH_EVENT_RECORD_STOP
    SWITCH_EVENT_PLAYBACK_START
    SWITCH_EVENT_PLAYBACK_STOP
    SWITCH_EVENT_CALL_UPDATE
    SWITCH_EVENT_FAILURE            - A failure occurred which might impact the normal functioning of the switch
    SWITCH_EVENT_SOCKET_DATA
    SWITCH_EVENT_MEDIA_BUG_START
    SWITCH_EVENT_MEDIA_BUG_STOP
    SWITCH_EVENT_CONFERENCE_DATA_QUERY
    SWITCH_EVENT_CONFERENCE_DATA
    SWITCH_EVENT_CALL_SETUP_REQ
    SWITCH_EVENT_CALL_SETUP_RESULT
    SWITCH_EVENT_CALL_DETAIL
    SWITCH_EVENT_DEVICE_STATE
    SWITCH_EVENT_ALL				- All events at once
</pre>

 */
typedef enum {
	SWITCH_EVENT_CUSTOM,
	SWITCH_EVENT_CLONE,
	SWITCH_EVENT_CHANNEL_CREATE,
	SWITCH_EVENT_CHANNEL_DESTROY,
	SWITCH_EVENT_CHANNEL_STATE,
	SWITCH_EVENT_CHANNEL_CALLSTATE,
	SWITCH_EVENT_CHANNEL_ANSWER,
	SWITCH_EVENT_CHANNEL_HANGUP,
	SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE,
	SWITCH_EVENT_CHANNEL_EXECUTE,
	SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE,
	SWITCH_EVENT_CHANNEL_HOLD,
	SWITCH_EVENT_CHANNEL_UNHOLD,
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
	SWITCH_EVENT_NOTIFY_IN,
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
	SWITCH_EVENT_PHONE_FEATURE,
	SWITCH_EVENT_PHONE_FEATURE_SUBSCRIBE,
	SWITCH_EVENT_SEND_MESSAGE,
	SWITCH_EVENT_RECV_MESSAGE,
	SWITCH_EVENT_REQUEST_PARAMS,
	SWITCH_EVENT_CHANNEL_DATA,
	SWITCH_EVENT_GENERAL,
	SWITCH_EVENT_COMMAND,
	SWITCH_EVENT_SESSION_HEARTBEAT,
	SWITCH_EVENT_CLIENT_DISCONNECTED,
	SWITCH_EVENT_SERVER_DISCONNECTED,
	SWITCH_EVENT_SEND_INFO,
	SWITCH_EVENT_RECV_INFO,
	SWITCH_EVENT_RECV_RTCP_MESSAGE,
	SWITCH_EVENT_CALL_SECURE,
	SWITCH_EVENT_NAT,
	SWITCH_EVENT_RECORD_START,
	SWITCH_EVENT_RECORD_STOP,
	SWITCH_EVENT_PLAYBACK_START,
	SWITCH_EVENT_PLAYBACK_STOP,
	SWITCH_EVENT_CALL_UPDATE,
	SWITCH_EVENT_FAILURE,
	SWITCH_EVENT_SOCKET_DATA,
	SWITCH_EVENT_MEDIA_BUG_START,
	SWITCH_EVENT_MEDIA_BUG_STOP,
	SWITCH_EVENT_CONFERENCE_DATA_QUERY,
	SWITCH_EVENT_CONFERENCE_DATA,
	SWITCH_EVENT_CALL_SETUP_REQ,
	SWITCH_EVENT_CALL_SETUP_RESULT,
	SWITCH_EVENT_CALL_DETAIL,
	SWITCH_EVENT_DEVICE_STATE,
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
	SWITCH_CAUSE_PROGRESS_TIMEOUT = 607,
	SWITCH_CAUSE_INVALID_GATEWAY = 608,
	SWITCH_CAUSE_GATEWAY_DOWN = 609,
	SWITCH_CAUSE_INVALID_URL = 610,
	SWITCH_CAUSE_INVALID_PROFILE = 611,
	SWITCH_CAUSE_NO_PICKUP = 612,
	SWITCH_CAUSE_SRTP_READ_ERROR = 613
} switch_call_cause_t;

typedef enum {
	SCSC_PAUSE_INBOUND,
	SCSC_PAUSE_OUTBOUND,
	SCSC_PAUSE_ALL,
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
	SCSC_MIN_DTMF_DURATION,
	SCSC_DEFAULT_DTMF_DURATION,
	SCSC_SHUTDOWN_ELEGANT,
	SCSC_SHUTDOWN_ASAP,
	SCSC_CANCEL_SHUTDOWN,
	SCSC_SEND_SIGHUP,
	SCSC_DEBUG_LEVEL,
	SCSC_FLUSH_DB_HANDLES,
	SCSC_SHUTDOWN_NOW,
	SCSC_REINCARNATE_NOW,
	SCSC_CALIBRATE_CLOCK,
	SCSC_SAVE_HISTORY,
	SCSC_CRASH,
	SCSC_MIN_IDLE_CPU,
	SCSC_VERBOSE_EVENTS,
	SCSC_SHUTDOWN_CHECK,
	SCSC_PAUSE_INBOUND_CHECK,
	SCSC_PAUSE_OUTBOUND_CHECK,
	SCSC_PAUSE_CHECK,
	SCSC_READY_CHECK,
	SCSC_THREADED_SYSTEM_EXEC,
	SCSC_SYNC_CLOCK_WHEN_IDLE,
	SCSC_DEBUG_SQL,
	SCSC_SQL,
	SCSC_API_EXPANSION,
	SCSC_RECOVER,
	SCSC_SPS_PEAK,
	SCSC_SPS_PEAK_FIVEMIN,
	SCSC_SESSIONS_PEAK,
	SCSC_SESSIONS_PEAK_FIVEMIN
} switch_session_ctl_t;

typedef enum {
	SSH_FLAG_STICKY = (1 << 0)
} switch_state_handler_flag_t;

#ifdef WIN32
typedef SOCKET switch_os_socket_t;
#else
typedef int switch_os_socket_t;
#endif

typedef struct apr_pool_t switch_memory_pool_t;
typedef uint16_t switch_port_t;
typedef uint8_t switch_payload_t;
typedef struct switch_app_log switch_app_log_t;
typedef struct switch_rtp switch_rtp_t;
typedef struct switch_rtcp switch_rtcp_t;
typedef struct switch_core_session_message switch_core_session_message_t;
typedef struct switch_event_header switch_event_header_t;
typedef struct switch_event switch_event_t;
typedef struct switch_event_subclass switch_event_subclass_t;
typedef struct switch_event_node switch_event_node_t;
typedef struct switch_loadable_module switch_loadable_module_t;
typedef struct switch_frame switch_frame_t;
typedef struct switch_rtcp_frame switch_rtcp_frame_t;
typedef struct switch_channel switch_channel_t;
typedef struct switch_sql_queue_manager switch_sql_queue_manager_t;
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
typedef struct switch_codec_fmtp switch_codec_fmtp_t;
typedef struct switch_odbc_handle switch_odbc_handle_t;
typedef struct switch_pgsql_handle switch_pgsql_handle_t;
typedef struct switch_pgsql_result switch_pgsql_result_t;

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
typedef struct switch_chat_application_interface switch_chat_application_interface_t;
typedef struct switch_api_interface switch_api_interface_t;
typedef struct switch_json_api_interface switch_json_api_interface_t;
typedef struct switch_file_interface switch_file_interface_t;
typedef struct switch_speech_interface switch_speech_interface_t;
typedef struct switch_asr_interface switch_asr_interface_t;
typedef struct switch_directory_interface switch_directory_interface_t;
typedef struct switch_chat_interface switch_chat_interface_t;
typedef struct switch_management_interface switch_management_interface_t;
typedef struct switch_core_port_allocator switch_core_port_allocator_t;
typedef struct switch_media_bug switch_media_bug_t;
typedef struct switch_limit_interface switch_limit_interface_t;

typedef void (*hashtable_destructor_t)(void *ptr);

struct switch_console_callback_match_node {
	char *val;
	struct switch_console_callback_match_node *next;
};
typedef struct switch_console_callback_match_node switch_console_callback_match_node_t;

struct switch_console_callback_match {
	struct switch_console_callback_match_node *head;
	struct switch_console_callback_match_node *end;
	int count;
	int dynamic;
};
typedef struct switch_console_callback_match switch_console_callback_match_t;

typedef void (*switch_media_bug_exec_cb_t)(switch_media_bug_t *bug, void *user_data);

typedef void (*switch_cap_callback_t) (const char *var, const char *val, void *user_data);
typedef switch_status_t (*switch_console_complete_callback_t) (const char *, const char *, switch_console_callback_match_t **matches);
typedef switch_bool_t (*switch_media_bug_callback_t) (switch_media_bug_t *, void *, switch_abc_type_t);
typedef switch_bool_t (*switch_tone_detect_callback_t) (switch_core_session_t *, const char *, const char *);
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
typedef switch_status_t (*switch_core_codec_fmtp_parse_func_t) (const char *fmtp, switch_codec_fmtp_t *codec_fmtp);
typedef switch_status_t (*switch_core_codec_destroy_func_t) (switch_codec_t *);


typedef switch_status_t (*switch_chat_application_function_t) (switch_event_t *, const char *);
#define SWITCH_STANDARD_CHAT_APP(name) static switch_status_t name (switch_event_t *message, const char *data)

typedef void (*switch_application_function_t) (switch_core_session_t *, const char *);
#define SWITCH_STANDARD_APP(name) static void name (switch_core_session_t *session, const char *data)

typedef int (*switch_core_recover_callback_t)(switch_core_session_t *session);
typedef void (*switch_event_callback_t) (switch_event_t *);
typedef switch_caller_extension_t *(*switch_dialplan_hunt_function_t) (switch_core_session_t *, void *, switch_caller_profile_t *);
#define SWITCH_STANDARD_DIALPLAN(name) static switch_caller_extension_t *name (switch_core_session_t *session, void *arg, switch_caller_profile_t *caller_profile)

typedef switch_bool_t (*switch_hash_delete_callback_t) (_In_ const void *key, _In_ const void *val, _In_opt_ void *pData);
#define SWITCH_HASH_DELETE_FUNC(name) static switch_bool_t name (const void *key, const void *val, void *pData)

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


typedef switch_status_t (*switch_json_api_function_t) (const cJSON *json, _In_opt_ switch_core_session_t *session, cJSON **json_reply);


#define SWITCH_STANDARD_JSON_API(name) static switch_status_t name (const cJSON *json, _In_opt_ switch_core_session_t *session, cJSON **json_reply)

typedef switch_status_t (*switch_input_callback_function_t) (switch_core_session_t *session, void *input,
															 switch_input_type_t input_type, void *buf, unsigned int buflen);
typedef switch_status_t (*switch_read_frame_callback_function_t) (switch_core_session_t *session, switch_frame_t *frame, void *user_data);
typedef struct switch_say_interface switch_say_interface_t;

#define DMACHINE_MAX_DIGIT_LEN 512

typedef enum {
	DM_MATCH_POSITIVE,
	DM_MATCH_NEGATIVE
} dm_match_type_t;

struct switch_ivr_dmachine;
typedef struct switch_ivr_dmachine switch_ivr_dmachine_t;

struct switch_ivr_dmachine_match {
	switch_ivr_dmachine_t *dmachine;
	const char *match_digits;
	int32_t match_key;
	dm_match_type_t type;
	void *user_data;
};

typedef struct switch_ivr_dmachine_match switch_ivr_dmachine_match_t;
typedef switch_status_t (*switch_ivr_dmachine_callback_t) (switch_ivr_dmachine_match_t *match);

#define MAX_ARG_RECURSION 25

#define arg_recursion_check_start(_args) if (_args) {					\
		if (_args->loops >= MAX_ARG_RECURSION) {						\
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,		\
							  "RECURSION ERROR!  It's not the best idea to call things that collect input recursively from an input callback.\n"); \
			return SWITCH_STATUS_GENERR;								\
		} else {_args->loops++;}										\
	}


#define arg_recursion_check_stop(_args) if (_args) _args->loops--

typedef struct {
	switch_input_callback_function_t input_callback;
	void *buf;
	uint32_t buflen;
	switch_read_frame_callback_function_t read_frame_callback;
	void *user_data;
	switch_ivr_dmachine_t *dmachine;
	int loops;
} switch_input_args_t;


typedef struct {
	switch_say_type_t type;
	switch_say_method_t method;
	switch_say_gender_t gender;
	const char *ext;
} switch_say_args_t;


typedef switch_status_t (*switch_say_callback_t) (switch_core_session_t *session,
												  char *tosay,
												  switch_say_args_t *say_args,
												  switch_input_args_t *args);

typedef switch_status_t (*switch_say_string_callback_t) (switch_core_session_t *session,
														 char *tosay,
														 switch_say_args_t *say_args, char **rstr);
														 
struct switch_say_file_handle;
typedef struct switch_say_file_handle switch_say_file_handle_t;

typedef switch_status_t (*switch_new_say_callback_t) (switch_say_file_handle_t *sh,
													  char *tosay,
													  switch_say_args_t *say_args);


typedef struct switch_xml *switch_xml_t;
typedef struct switch_core_time_duration switch_core_time_duration_t;
typedef switch_xml_t(*switch_xml_open_root_function_t) (uint8_t reload, const char **err, void *user_data);
typedef switch_xml_t(*switch_xml_search_function_t) (const char *section,
													 const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
													 void *user_data);

struct switch_hashtable;
struct switch_hashtable_iterator;
typedef struct switch_hashtable switch_hash_t;
typedef struct switch_hashtable_iterator switch_hash_index_t;

struct switch_network_list;
typedef struct switch_network_list switch_network_list_t;


#define SWITCH_API_VERSION 5
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
	SWITCH_PRI_LOW = 1,
	SWITCH_PRI_NORMAL = 10,
	SWITCH_PRI_IMPORTANT = 50,
	SWITCH_PRI_REALTIME = 99,
} switch_thread_priority_t;

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

typedef int (*switch_modulename_callback_func_t) (void *user_data, const char *module_name);

typedef struct switch_slin_data switch_slin_data_t;

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
struct sql_queue_manager;

struct switch_media_handle_s;
typedef struct switch_media_handle_s switch_media_handle_t;

typedef uint32_t switch_event_channel_id_t;
typedef void (*switch_event_channel_func_t)(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id);

struct switch_live_array_s;
typedef struct switch_live_array_s switch_live_array_t;

typedef enum {
	SDP_TYPE_REQUEST,
	SDP_TYPE_RESPONSE
} switch_sdp_type_t;


typedef enum {
	AEAD_AES_256_GCM_8,
	AEAD_AES_128_GCM_8,
	AES_CM_256_HMAC_SHA1_80,
	AES_CM_192_HMAC_SHA1_80,
	AES_CM_128_HMAC_SHA1_80,
	AES_CM_256_HMAC_SHA1_32,
	AES_CM_192_HMAC_SHA1_32,
	AES_CM_128_HMAC_SHA1_32,
	AES_CM_128_NULL_AUTH,
	CRYPTO_INVALID
} switch_rtp_crypto_key_type_t;

typedef struct payload_map_s {
	switch_media_type_t type;
	switch_sdp_type_t sdp_type;
	uint32_t ptime;
	uint32_t rate;
	uint8_t allocated;
	uint8_t negotiated;
	uint8_t current;
	unsigned long hash;

	char *rm_encoding;
	char *iananame;
	switch_payload_t pt;
	unsigned long rm_rate;
	unsigned long adv_rm_rate;
	uint32_t codec_ms;
	uint32_t bitrate;

	char *rm_fmtp;

	switch_payload_t agreed_pt;
	switch_payload_t recv_pt;

	char *fmtp_out;

	char *remote_sdp_ip;
	switch_port_t remote_sdp_port;

	int channels;
	int adv_channels;

	struct payload_map_s *next;

} payload_map_t;

typedef enum {
	ICE_GOOGLE_JINGLE = (1 << 0),
	ICE_VANILLA = (1 << 1),
	ICE_CONTROLLED = (1 << 2)
} switch_core_media_ice_type_t;



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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
