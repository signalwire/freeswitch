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
 * This module (mod_skypiax) has been contributed by:
 *
 * Giovanni Maruzzelli (gmaruzz@gmail.com)
 *
 *
 * Further Contributors:
 *
 *
 * mod_skypiax.c -- Skype compatible Endpoint Module
 *
 */

#include <switch.h>
#include <switch_version.h>

#ifndef WIN32
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#endif //WIN32

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>
#include <spandsp/version.h>

#ifndef WIN32
#include <netinet/tcp.h>
#endif

#ifdef _MSC_VER
//Windows macro  for FD_SET includes a warning C4127: conditional expression is constant
#pragma warning(push)
#pragma warning(disable:4127)
#endif

#define MY_EVENT_INCOMING_CHATMESSAGE "skypiax::incoming_chatmessage"

#define SAMPLERATE_SKYPIAX 16000
#define SAMPLES_PER_FRAME SAMPLERATE_SKYPIAX/50

#ifndef SKYPIAX_SVN_VERSION
#define SKYPIAX_SVN_VERSION SWITCH_VERSION_REVISION
#endif /* SKYPIAX_SVN_VERSION */

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

#define DEBUGA_SKYPE(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_SKYPE  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_CALL(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_CALL  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_PBX(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_PBX  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define ERRORA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][ERRORA  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define WARNINGA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][WARNINGA  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define NOTICA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][NOTICA  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );

#define SKYPIAX_P_LOG NULL, (unsigned long)55, __LINE__, tech_pvt ? tech_pvt->name ? tech_pvt->name : "none" : "none", -1, tech_pvt ? tech_pvt->interface_state : -1, tech_pvt ? tech_pvt->skype_callflow : -1

/*********************************/
#define SKYPIAX_CAUSE_NORMAL		1
/*********************************/
#define SKYPIAX_FRAME_DTMF			1
/*********************************/
#define SKYPIAX_CONTROL_RINGING		1
#define SKYPIAX_CONTROL_ANSWER		2

/*********************************/
#define		SKYPIAX_STATE_IDLE					0
#define		SKYPIAX_STATE_DOWN					1
#define		SKYPIAX_STATE_RING					2
#define		SKYPIAX_STATE_DIALING				3
#define		SKYPIAX_STATE_BUSY					4
#define		SKYPIAX_STATE_UP					5
#define		SKYPIAX_STATE_RINGING				6
#define		SKYPIAX_STATE_PRERING				7
#define		SKYPIAX_STATE_ERROR_DOUBLE_CALL		8
#define		SKYPIAX_STATE_SELECTED				9
#define 	SKYPIAX_STATE_HANGUP_REQUESTED		10
#define		SKYPIAX_STATE_PREANSWER				11
/*********************************/
/* call flow from the device */
#define 	CALLFLOW_CALL_IDLE					0
#define 	CALLFLOW_CALL_DOWN					1
#define 	CALLFLOW_INCOMING_RING				2
#define 	CALLFLOW_CALL_DIALING				3
#define 	CALLFLOW_CALL_LINEBUSY				4
#define 	CALLFLOW_CALL_ACTIVE				5
#define 	CALLFLOW_INCOMING_HANGUP			6
#define 	CALLFLOW_CALL_RELEASED				7
#define 	CALLFLOW_CALL_NOCARRIER				8
#define 	CALLFLOW_CALL_INFLUX				9
#define 	CALLFLOW_CALL_INCOMING				10
#define 	CALLFLOW_CALL_FAILED				11
#define 	CALLFLOW_CALL_NOSERVICE				12
#define 	CALLFLOW_CALL_OUTGOINGRESTRICTED	13
#define 	CALLFLOW_CALL_SECURITYFAIL			14
#define 	CALLFLOW_CALL_NOANSWER				15
#define 	CALLFLOW_STATUS_FINISHED			16
#define 	CALLFLOW_STATUS_CANCELLED			17
#define 	CALLFLOW_STATUS_FAILED				18
#define 	CALLFLOW_STATUS_REFUSED				19
#define 	CALLFLOW_STATUS_RINGING				20
#define 	CALLFLOW_STATUS_INPROGRESS			21
#define 	CALLFLOW_STATUS_UNPLACED			22
#define 	CALLFLOW_STATUS_ROUTING				23
#define 	CALLFLOW_STATUS_EARLYMEDIA			24
#define 	CALLFLOW_INCOMING_CALLID			25
#define 	CALLFLOW_STATUS_REMOTEHOLD			26

/*********************************/

#define SKYPIAX_MAX_INTERFACES 64

#ifndef WIN32
struct SkypiaxHandles {
	Window skype_win;
	Display *disp;
	Window win;
	int currentuserhandle;
	int api_connected;
	int fdesc[2];
};
#else //WIN32

struct SkypiaxHandles {
	HWND win32_hInit_MainWindowHandle;
	HWND win32_hGlobal_SkypeAPIWindowHandle;
	HINSTANCE win32_hInit_ProcessHandle;
	char win32_acInit_WindowClassName[128];
	UINT win32_uiGlobal_MsgID_SkypeControlAPIAttach;
	UINT win32_uiGlobal_MsgID_SkypeControlAPIDiscover;
	int currentuserhandle;
	int api_connected;
	switch_file_t *fdesc[2];
};
#endif //WIN32

#define MAX_CHATS 10

struct chat {
	char chatname[256];
	char dialog_partner[256];
};
typedef struct chat chat_t;

#define MAX_CHATMESSAGES 10

struct chatmessage {
	char id[256];
	char type[256];
	char chatname[256];
	char from_handle[256];
	char from_dispname[256];
	char body[512];
};
typedef struct chatmessage chatmessage_t;
struct private_object {
	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	char session_uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_caller_profile_t *caller_profile;
	switch_mutex_t *mutex;
	switch_mutex_t *flag_mutex;

	char interface_id[80];
	char name[80];
	char dialplan[80];
	char context[80];
	char dial_regex[256];
	char fail_dial_regex[256];
	char hold_music[256];
	char type[256];
	char X11_display[256];
#ifdef WIN32
	unsigned short tcp_cli_port;
	unsigned short tcp_srv_port;
#else
	int tcp_cli_port;
	int tcp_srv_port;
#endif
	struct SkypiaxHandles SkypiaxHandles;

	int interface_state;
	char language[80];
	char exten[80];
	int skypiax_sound_rate;
	char callid_name[50];
	char callid_number[50];
	double playback_boost;
	double capture_boost;
	int stripmsd;
	char skype_call_id[512];
	int skype_call_ongoing;
	char skype_friends[4096];
	char skype_fullname[512];
	char skype_displayname[512];
	int skype_callflow;
	int skype;
	int control_to_send;
#ifdef WIN32
	switch_file_t *audiopipe_srv[2];
	switch_file_t *audiopipe_cli[2];
	switch_file_t *skypiax_sound_capt_fd;
#else							/* WIN32 */
	int audiopipe_srv[2];
	int audiopipe_cli[2];
	int skypiax_sound_capt_fd;
#endif							/* WIN32 */
	switch_thread_t *tcp_srv_thread;
	switch_thread_t *tcp_cli_thread;
	switch_thread_t *skypiax_signaling_thread;
	switch_thread_t *skypiax_api_thread;
	short audiobuf[SAMPLES_PER_FRAME];
	int audiobuf_is_loaded;
	short audiobuf_cli[SAMPLES_PER_FRAME];
	switch_mutex_t *mutex_audio_cli;
	int flag_audio_cli;
	short audiobuf_srv[SAMPLES_PER_FRAME];
	switch_mutex_t *mutex_audio_srv;
	int flag_audio_srv;

	FILE *phonebook_writing_fp;
	int skypiax_dir_entry_extension_prefix;
	char skype_user[256];
	char initial_skype_user[256];
	char skype_password[256];
	char destination[256];
	struct timeval answer_time;

	struct timeval transfer_time;
	char transfer_callid_number[50];
	char skype_transfer_call_id[512];
	int running;
	uint32_t ib_calls;
	uint32_t ob_calls;
	uint32_t ib_failed_calls;
	uint32_t ob_failed_calls;

	chatmessage_t chatmessages[MAX_CHATMESSAGES];
	chat_t chats[MAX_CHATS];
	uint32_t report_incoming_chatmessages;
	switch_timer_t timer_read;
	switch_timer_t timer_write;
	int begin_to_write;
	int begin_to_read;
	dtmf_rx_state_t dtmf_state;
	switch_time_t old_dtmf_timestamp;
	switch_buffer_t *write_buffer;
	switch_buffer_t *read_buffer;
	int silent_mode;
	int write_silence_when_idle;

};

typedef struct private_object private_t;

void *SWITCH_THREAD_FUNC skypiax_api_thread_func(switch_thread_t * thread, void *obj);
int skypiax_audio_read(private_t * tech_pvt);
int skypiax_audio_init(private_t * tech_pvt);
int skypiax_signaling_write(private_t * tech_pvt, char *msg_to_skype);
int skypiax_signaling_read(private_t * tech_pvt);

int skypiax_call(private_t * tech_pvt, char *idest, int timeout);
int skypiax_senddigit(private_t * tech_pvt, char digit);

void *skypiax_do_tcp_srv_thread_func(void *obj);
void *SWITCH_THREAD_FUNC skypiax_do_tcp_srv_thread(switch_thread_t * thread, void *obj);

void *skypiax_do_tcp_cli_thread_func(void *obj);
void *SWITCH_THREAD_FUNC skypiax_do_tcp_cli_thread(switch_thread_t * thread, void *obj);

void *skypiax_do_skypeapi_thread_func(void *obj);
void *SWITCH_THREAD_FUNC skypiax_do_skypeapi_thread(switch_thread_t * thread, void *obj);
int dtmf_received(private_t * tech_pvt, char *value);
int start_audio_threads(private_t * tech_pvt);
int new_inbound_channel(private_t * tech_pvt);
int outbound_channel_answered(private_t * tech_pvt);
int skypiax_signaling_write(private_t * tech_pvt, char *msg_to_skype);
#if defined(WIN32) && !defined(__CYGWIN__)
int skypiax_pipe_read(switch_file_t * pipe, short *buf, int howmany);
int skypiax_pipe_write(switch_file_t * pipe, short *buf, int howmany);
/* Visual C do not have strsep ? */
char *strsep(char **stringp, const char *delim);
#else
int skypiax_pipe_read(int pipe, short *buf, int howmany);
int skypiax_pipe_write(int pipe, short *buf, int howmany);
#endif /* WIN32 */
int skypiax_close_socket(unsigned int fd);
private_t *find_available_skypiax_interface_rr(private_t * tech_pvt_calling);
int remote_party_is_ringing(private_t * tech_pvt);
int remote_party_is_early_media(private_t * tech_pvt);
int skypiax_answer(private_t * tech_pvt, char *id, char *value);
int skypiax_transfer(private_t * tech_pvt, char *id, char *value);
#ifndef WIN32
int skypiax_socket_create_and_bind(private_t * tech_pvt, int *which_port);
#else
int skypiax_socket_create_and_bind(private_t * tech_pvt, unsigned short *which_port);
#endif //WIN32
int incoming_chatmessage(private_t * tech_pvt, int which);
int next_port(void);
