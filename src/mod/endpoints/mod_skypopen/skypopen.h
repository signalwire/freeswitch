/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * This module (mod_gsmopen) has been contributed by:
 *
 * Giovanni Maruzzelli <gmaruzz@gmail.com>
 *
 * Maintainer: Giovanni Maruzzelli <gmaruzz@gmail.com>
 *
 * mod_skypopen.c -- Skype compatible Endpoint Module
 *
 */


#include <switch.h>

#ifndef WIN32
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>

// CLOUDTREE (Thomas Hazel)
#define XIO_ERROR_BY_SETJMP
//#define XIO_ERROR_BY_UCONTEXT

// CLOUDTREE (Thomas Hazel)
#ifdef XIO_ERROR_BY_SETJMP
#include "setjmp.h"
#endif
// CLOUDTREE (Thomas Hazel)
#ifdef XIO_ERROR_BY_UCONTEXT
#include "ucontext.h"
#endif

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

#define MY_EVENT_INCOMING_CHATMESSAGE "skypopen::incoming_chatmessage"
#define MY_EVENT_INCOMING_RAW "skypopen::incoming_raw"

#define SAMPLERATE_SKYPOPEN 16000
#define MS_SKYPOPEN 20
#define SAMPLES_PER_FRAME (SAMPLERATE_SKYPOPEN/(1000/MS_SKYPOPEN))
#define BYTES_PER_FRAME (SAMPLES_PER_FRAME * sizeof(short))

#ifndef SKYPOPEN_SVN_VERSION
#define SKYPOPEN_SVN_VERSION switch_version_full()
#endif /* SKYPOPEN_SVN_VERSION */

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_LINEAR = (1 << 6),
	TFLAG_PROGRESS = (1 << 7),
	TFLAG_BREAK = (1 << 8)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

#define DEBUGA_SKYPE(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"%-*s  [%s ] [DEBUG_SKYPE  %-5d][%-15s][%s,%s] " __VA_ARGS__ );
#define DEBUGA_CALL(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"%-*s  [%s ] [DEBUG_CALL  %-5d][%-15s][%s,%s] " __VA_ARGS__ );
#define DEBUGA_PBX(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"%-*s  [%s ] [DEBUG_PBX  %-5d][%-15s][%s,%s] " __VA_ARGS__ );
#define ERRORA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 		"%-*s   [%s ] [ERRORA       %-5d][%-15s][%s,%s] " __VA_ARGS__ );
#define WARNINGA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 		"%-*s[%s ] [WARNINGA     %-5d][%-15s][%s,%s] " __VA_ARGS__ );
#define NOTICA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, 		"%-*s [%s ] [NOTICA       %-5d][%-15s][%s,%s] " __VA_ARGS__ );

#define SKYPOPEN_P_LOG (int)((20 - (strlen(__FILE__))) + ((__LINE__ - 1000) < 0) + ((__LINE__ - 100) < 0)), " ", SKYPOPEN_SVN_VERSION, __LINE__, tech_pvt ? tech_pvt->name ? tech_pvt->name : "none" : "none", tech_pvt ? interface_status[tech_pvt->interface_state] : "N/A", tech_pvt ? skype_callflow[tech_pvt->skype_callflow] : "N/A"

/*********************************/
#define SKYPOPEN_CAUSE_NORMAL		1
/*********************************/
#define SKYPOPEN_FRAME_DTMF			1
/*********************************/
#define SKYPOPEN_CONTROL_RINGING		1
#define SKYPOPEN_CONTROL_ANSWER		2

/*********************************/
// CLOUDTREE (Thomas Hazel)
#define SKYPOPEN_RINGING_INIT		0
#define SKYPOPEN_RINGING_PRE		1

/*********************************/
#define		SKYPOPEN_STATE_IDLE					0
#define		SKYPOPEN_STATE_DOWN					1
#define		SKYPOPEN_STATE_RING					2
#define		SKYPOPEN_STATE_DIALING				3
#define		SKYPOPEN_STATE_BUSY					4
#define		SKYPOPEN_STATE_UP					5
#define		SKYPOPEN_STATE_RINGING				6
#define		SKYPOPEN_STATE_PRERING				7
#define		SKYPOPEN_STATE_ERROR_DOUBLE_CALL		8
#define		SKYPOPEN_STATE_SELECTED				9
#define 	SKYPOPEN_STATE_HANGUP_REQUESTED		10
#define		SKYPOPEN_STATE_PREANSWER				11
#define		SKYPOPEN_STATE_DEAD				12
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

#define SKYPOPEN_MAX_INTERFACES 64

#ifndef WIN32
struct SkypopenHandles {
	Window skype_win;
	Display *disp;
	Window win;
	int currentuserhandle;
	int api_connected;
	int fdesc[2];

	// CLOUDTREE (Thomas Hazel)
#ifdef XIO_ERROR_BY_SETJMP
	jmp_buf ioerror_context;
#endif
#ifdef XIO_ERROR_BY_UCONTEXT
	ucontext_t ioerror_context;
#endif

	// CLOUDTREE (Thomas Hazel) - is there a capable freeswitch list?
	switch_bool_t managed;
	void *prev;
	void *next;
};

// CLOUDTREE (Thomas Hazel) - is there a capable freeswitch list?
struct SkypopenList {
	int entries;
	void *head;
	void *tail;
};

// CLOUDTREE (Thomas Hazel) - is there a capable freeswitch list?
struct SkypopenHandles *skypopen_list_add(struct SkypopenList *list, struct SkypopenHandles *x);
struct SkypopenHandles *skypopen_list_find(struct SkypopenList *list, struct SkypopenHandles *x);
struct SkypopenHandles *skypopen_list_remove_by_value(struct SkypopenList *list, Display * display);
struct SkypopenHandles *skypopen_list_remove_by_reference(struct SkypopenList *list, struct SkypopenHandles *x);
int skypopen_list_size(struct SkypopenList *list);

#else //WIN32

struct SkypopenHandles {
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
	struct SkypopenHandles SkypopenHandles;

	// CLOUDTREE (Thomas Hazel)
	char ringing_state;

	int interface_state;
	char language[80];
	char exten[80];
	int skypopen_sound_rate;
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
	switch_file_t *skypopen_sound_capt_fd;
#else							/* WIN32 */
	int audiopipe_srv[2];
	int audiopipe_cli[2];
	int skypopen_sound_capt_fd;
#endif							/* WIN32 */
	switch_thread_t *tcp_srv_thread;
	switch_thread_t *tcp_cli_thread;
	switch_thread_t *skypopen_signaling_thread;
	switch_thread_t *skypopen_api_thread;
	short audiobuf[SAMPLES_PER_FRAME];
	int audiobuf_is_loaded;
	short audiobuf_cli[SAMPLES_PER_FRAME];
	switch_mutex_t *mutex_audio_cli;
	int flag_audio_cli;
	short audiobuf_srv[SAMPLES_PER_FRAME];
	switch_mutex_t *mutex_audio_srv;
	int flag_audio_srv;
	switch_mutex_t *mutex_thread_audio_cli;
	switch_mutex_t *mutex_thread_audio_srv;

	FILE *phonebook_writing_fp;
	int skypopen_dir_entry_extension_prefix;
	char skype_user[256];
	char initial_skype_user[256];
	char skype_password[256];
	char destination[256];
	struct timeval answer_time;
	struct timeval ring_time;

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
	switch_timer_t timer_read_srv;
	switch_timer_t timer_write;
	int begin_to_write;
	int begin_to_read;
	dtmf_rx_state_t dtmf_state;
	switch_time_t old_dtmf_timestamp;
	switch_buffer_t *write_buffer;
	switch_buffer_t *read_buffer;
	int silent_mode;
	int write_silence_when_idle;
	int setsockopt;
	char answer_id[256];
	char answer_value[256];
	char ring_id[256];
	char ring_value[256];

	char message[4096];
	char skype_voicemail_id[512];
	char skype_voicemail_id_greeting[512];
};

typedef struct private_object private_t;

void *SWITCH_THREAD_FUNC skypopen_api_thread_func(switch_thread_t *thread, void *obj);
int skypopen_audio_read(private_t *tech_pvt);
int skypopen_audio_init(private_t *tech_pvt);
int skypopen_signaling_write(private_t *tech_pvt, char *msg_to_skype);
int skypopen_signaling_read(private_t *tech_pvt);

int skypopen_call(private_t *tech_pvt, char *idest, int timeout);
int skypopen_senddigit(private_t *tech_pvt, char digit);

void *skypopen_do_tcp_srv_thread_func(void *obj);
void *SWITCH_THREAD_FUNC skypopen_do_tcp_srv_thread(switch_thread_t *thread, void *obj);

void *skypopen_do_tcp_cli_thread_func(void *obj);
void *SWITCH_THREAD_FUNC skypopen_do_tcp_cli_thread(switch_thread_t *thread, void *obj);

void *skypopen_do_skypeapi_thread_func(void *obj);
void *SWITCH_THREAD_FUNC skypopen_do_skypeapi_thread(switch_thread_t *thread, void *obj);
int dtmf_received(private_t *tech_pvt, char *value);
int start_audio_threads(private_t *tech_pvt);
int new_inbound_channel(private_t *tech_pvt);
int outbound_channel_answered(private_t *tech_pvt);
int skypopen_signaling_write(private_t *tech_pvt, char *msg_to_skype);
#if defined(WIN32) && !defined(__CYGWIN__)
int skypopen_pipe_read(switch_file_t *pipe, short *buf, int howmany);
int skypopen_pipe_write(switch_file_t *pipe, short *buf, int howmany);
/* Visual C do not have strsep ? */
char *strsep(char **stringp, const char *delim);
#else
int skypopen_pipe_read(int pipe, short *buf, int howmany);
int skypopen_pipe_write(int pipe, short *buf, int howmany);
#endif /* WIN32 */
int skypopen_close_socket(unsigned int fd);
private_t *find_available_skypopen_interface_rr(private_t *tech_pvt_calling);
int remote_party_is_ringing(private_t *tech_pvt);
int remote_party_is_early_media(private_t *tech_pvt);
int skypopen_answer(private_t *tech_pvt);
int skypopen_transfer(private_t *tech_pvt);
#ifndef WIN32
int skypopen_socket_create_and_bind(private_t *tech_pvt, int *which_port);
#else
int skypopen_socket_create_and_bind(private_t *tech_pvt, unsigned short *which_port);
#endif //WIN32
int incoming_chatmessage(private_t *tech_pvt, int which);
int next_port(void);
int skypopen_partner_handle_ring(private_t *tech_pvt);
int skypopen_answered(private_t *tech_pvt);
int inbound_channel_answered(private_t *tech_pvt);
