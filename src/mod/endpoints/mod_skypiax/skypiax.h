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

#ifdef _MSC_VER
//Windows macro  for FD_SET includes a warning C4127: conditional expression is constant
#pragma warning(push)
#pragma warning(disable:4127)
#endif

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
#define		SKYPIAX_STATE_DOWN		1
#define		SKYPIAX_STATE_RING		2
#define		SKYPIAX_STATE_DIALING	3
#define		SKYPIAX_STATE_BUSY		4
#define		SKYPIAX_STATE_UP		5
#define		SKYPIAX_STATE_RINGING	6
#define		SKYPIAX_STATE_PRERING	7
/*********************************/
/* call flow from the device */
#define 	CALLFLOW_CALL_IDLE  SKYPIAX_STATE_DOWN
#define 	CALLFLOW_INCOMING_RING  SKYPIAX_STATE_RING
#define 	CALLFLOW_CALL_DIALING   SKYPIAX_STATE_DIALING
#define 	CALLFLOW_CALL_LINEBUSY   SKYPIAX_STATE_BUSY
#define 	CALLFLOW_CALL_ACTIVE   300
#define 	CALLFLOW_INCOMING_HANGUP   100
#define 	CALLFLOW_CALL_RELEASED   101
#define 	CALLFLOW_CALL_NOCARRIER   102
#define 	CALLFLOW_CALL_INFLUX   103
#define 	CALLFLOW_CALL_INCOMING   104
#define 	CALLFLOW_CALL_FAILED   105
#define 	CALLFLOW_CALL_NOSERVICE   106
#define 	CALLFLOW_CALL_OUTGOINGRESTRICTED   107
#define 	CALLFLOW_CALL_SECURITYFAIL   108
#define 	CALLFLOW_CALL_NOANSWER   109
#define 	CALLFLOW_STATUS_FINISHED   110
#define 	CALLFLOW_STATUS_CANCELLED   111
#define 	CALLFLOW_STATUS_FAILED   112
#define 	CALLFLOW_STATUS_REFUSED   113
#define 	CALLFLOW_STATUS_RINGING   114
#define 	CALLFLOW_STATUS_INPROGRESS   115
#define 	CALLFLOW_STATUS_UNPLACED   116
#define 	CALLFLOW_STATUS_ROUTING   117
#define 	CALLFLOW_STATUS_EARLYMEDIA   118
#define 	SKYPIAX_STATE_HANGUP_REQUESTED   200
  //FIXME CALLFLOW_INCOMING_CALLID to be removed
#define 	CALLFLOW_INCOMING_CALLID   1019
#define 	CALLFLOW_STATUS_REMOTEHOLD   201

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

	int interface_state;		/*!< \brief 'state' of the interface (channel) */
	char language[80];			/*!< \brief default Asterisk dialplan language for this interface */
	char exten[80];				/*!< \brief default Asterisk dialplan extension for this interface */
	int skypiax_sound_rate;		/*!< \brief rate of the sound device, in Hz, eg: 8000 */
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
	int skype_callflow;			/*!< \brief 'callflow' of the skype interface (as opposed to phone interface) */
	int skype;					/*!< \brief config flag, bool, Skype support on this interface (0 if false, -1 if true) */
	int control_to_send;
#ifdef WIN32
	switch_file_t *audiopipe[2];
	switch_file_t *audioskypepipe[2];
	switch_file_t *skypiax_sound_capt_fd;	/*!< \brief file descriptor for sound capture dev */
#else							/* WIN32 */
	int audiopipe[2];
	int audioskypepipe[2];
	int skypiax_sound_capt_fd;	/*!< \brief file descriptor for sound capture dev */
#endif							/* WIN32 */
	switch_thread_t *tcp_srv_thread;
	switch_thread_t *tcp_cli_thread;
	switch_thread_t *skypiax_signaling_thread;
	switch_thread_t *skypiax_api_thread;
	short audiobuf[SAMPLES_PER_FRAME];
	int audiobuf_is_loaded;

	//int phonebook_listing;
	//int phonebook_querying;
	//int phonebook_listing_received_calls;

	//int phonebook_first_entry;
	//int phonebook_last_entry;
	//int phonebook_number_lenght;
	//int phonebook_text_lenght;
	FILE *phonebook_writing_fp;
	int skypiax_dir_entry_extension_prefix;
	char skype_user[256];
	char skype_password[256];
	char destination[256];
	struct timeval answer_time;

	struct timeval transfer_time;
	char transfer_callid_number[50];
	char skype_transfer_call_id[512];
	int running;
};

typedef struct private_object private_t;

void *SWITCH_THREAD_FUNC skypiax_api_thread_func(switch_thread_t * thread, void *obj);
void skypiax_tech_init(private_t * tech_pvt, switch_core_session_t *session);
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
private_t *find_available_skypiax_interface(private_t * tech_pvt);
private_t *find_available_skypiax_interface_rr(void);
int remote_party_is_ringing(private_t * tech_pvt);
int remote_party_is_early_media(private_t * tech_pvt);
int skypiax_answer(private_t * tech_pvt, char *id, char *value);
int skypiax_transfer(private_t * tech_pvt, char *id, char *value);
