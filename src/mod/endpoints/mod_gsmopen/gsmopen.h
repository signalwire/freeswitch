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
 * mod_gsmopen.cpp -- GSM Modem compatible Endpoint Module
 *
 */


#define __STDC_LIMIT_MACROS

#ifdef WIN32
#define HAVE_VSNPRINTF
#pragma warning(disable: 4290)
#endif //WIN32

#define MY_EVENT_INCOMING_SMS "gsmopen::incoming_sms"
#define MY_EVENT_DUMP "gsmopen::dump_event"
#define MY_EVENT_ALARM "gsmopen::alarm"

#define ALARM_FAILED_INTERFACE 0
#define ALARM_NO_NETWORK_REGISTRATION 1
#define ALARM_ROAMING_NETWORK_REGISTRATION 2
#define ALARM_NETWORK_NO_SERVICE 3
#define ALARM_NETWORK_NO_SIGNAL 4
#define ALARM_NETWORK_LOW_SIGNAL 5

#undef GIOVA48

#ifndef GIOVA48
#define SAMPLES_PER_FRAME 160
#else // GIOVA48
#define SAMPLES_PER_FRAME 960
#endif // GIOVA48

#ifndef GIOVA48
#define     GSMOPEN_FRAME_SIZE   160
#else //GIOVA48
#define     GSMOPEN_FRAME_SIZE   960
#endif //GIOVA48
#define     SAMPLERATE_GSMOPEN   8000

#include <switch.h>
#ifndef WIN32
#include <termios.h>
#include <sys/ioctl.h>
#include <iconv.h>
#include <dirent.h>
#endif //WIN32

#ifndef WIN32
#include <sys/time.h>
#endif //WIN32

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>
#include <spandsp/version.h>

#ifdef _MSC_VER
//Windows macro  for FD_SET includes a warning C4127: conditional expression is constant
#pragma warning(push)
#pragma warning(disable:4127)
#endif

#define 	PROTOCOL_ALSA_VOICEMODEM   4
#define 	PROTOCOL_AT   2
#define 	PROTOCOL_FBUS2   1
#define 	PROTOCOL_NO_SERIAL   3

#define AT_MESG_MAX_LENGTH 2048	/* much more than 10 SMSs */
#define		AT_BUFSIZ AT_MESG_MAX_LENGTH
#define AT_MESG_MAX_LINES 20	/* 256 lines, so it can contains the results of AT+CLAC, that gives all the AT commands the phone supports */

#ifndef GSMOPEN_SVN_VERSION
#define GSMOPEN_SVN_VERSION switch_version_full()
#endif /* GSMOPEN_SVN_VERSION */

#include "ctb-0.16/ctb.h"

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

#define DEBUGA_GSMOPEN(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"rev %s [%p|%-7lx][DEBUG_GSMOPEN  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_CALL(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"rev %s [%p|%-7lx][DEBUG_CALL  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_PBX(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 		"rev %s [%p|%-7lx][DEBUG_PBX  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define ERRORA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 		"rev %s [%p|%-7lx][ERRORA  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define WARNINGA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 		"rev %s [%p|%-7lx][WARNINGA  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define NOTICA(...)  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, 		"rev %s [%p|%-7lx][NOTICA  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );

#define GSMOPEN_P_LOG GSMOPEN_SVN_VERSION, (void *)NULL, (unsigned long)55, __LINE__, tech_pvt ? tech_pvt->name ? tech_pvt->name : "none" : "none", -1, tech_pvt ? tech_pvt->interface_state : -1, tech_pvt ? tech_pvt->phone_callflow : -1

/*********************************/
#define GSMOPEN_CAUSE_NORMAL		1
#define GSMOPEN_CAUSE_FAILURE		2
#define GSMOPEN_CAUSE_NO_ANSWER		3
/*********************************/
#define GSMOPEN_FRAME_DTMF		1
/*********************************/
#define GSMOPEN_CONTROL_RINGING		1
#define GSMOPEN_CONTROL_ANSWER		2
#define GSMOPEN_CONTROL_HANGUP		3
#define GSMOPEN_CONTROL_BUSY		4

/*********************************/
#define		GSMOPEN_STATE_IDLE				0
#define		GSMOPEN_STATE_DOWN				1
#define		GSMOPEN_STATE_RING				2
#define		GSMOPEN_STATE_DIALING				3
#define		GSMOPEN_STATE_BUSY				4
#define		GSMOPEN_STATE_UP				5
#define		GSMOPEN_STATE_RINGING				6
#define		GSMOPEN_STATE_PRERING				7
#define		GSMOPEN_STATE_ERROR_DOUBLE_CALL			8
#define		GSMOPEN_STATE_SELECTED				9
#define 	GSMOPEN_STATE_HANGUP_REQUESTED			10
#define		GSMOPEN_STATE_PREANSWER				11
/*********************************/
/* call flow from the device */
#define 	CALLFLOW_CALL_IDLE				0
#define 	CALLFLOW_CALL_DOWN				1
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
#define 	CALLFLOW_CALL_OUTGOINGRESTRICTED		13
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
#define 	CALLFLOW_CALL_REMOTEANSWER			27
#define 	CALLFLOW_CALL_HANGUP_REQUESTED			28

/*********************************/

#define 	AT_OK   0
#define 	AT_ERROR   1

#define GSMOPEN_MAX_INTERFACES 64

#define USSD_ENCODING_AUTO 		0
#define USSD_ENCODING_PLAIN 	1
#define USSD_ENCODING_HEX_7BIT	2
#define USSD_ENCODING_HEX_8BIT	3
#define USSD_ENCODING_UCS2		4

#ifndef WIN32
struct GSMopenHandles {
	int currentuserhandle;
	int api_connected;
	int fdesc[2];
};
#else //WIN32

struct GSMopenHandles {
	HWND win32_hInit_MainWindowHandle;
	HWND win32_hGlobal_GSMAPIWindowHandle;
	HINSTANCE win32_hInit_ProcessHandle;
	char win32_acInit_WindowClassName[128];
	UINT win32_uiGlobal_MsgID_GSMControlAPIAttach;
	UINT win32_uiGlobal_MsgID_GSMControlAPIDiscover;
	int currentuserhandle;
	int api_connected;
	switch_file_t *fdesc[2];
};

#endif //WIN32

/*! 
 * \brief structure for storing the results of AT commands, in an array of AT_MESG_MAX_LINES * AT_MESG_MAX_LENGTH chars
 */
struct s_result {
	int elemcount;
	char result[AT_MESG_MAX_LINES][AT_MESG_MAX_LENGTH];
};

struct ciapa_struct {
	int state;
	int hangupcause;
};
typedef struct ciapa_struct ciapa_t;

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

	char id[80];
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
	struct GSMopenHandles GSMopenHandles;

	int interface_state;		/*!< \brief 'state' of the interface (channel) */
	char language[80];			/*!< \brief default Asterisk dialplan language for this interface */
	char exten[80];				/*!< \brief default Asterisk dialplan extension for this interface */
	int gsmopen_sound_rate;		/*!< \brief rate of the sound device, in Hz, eg: 8000 */
	char callid_name[50];
	char callid_number[50];
	double playback_boost;
	double capture_boost;
	int stripmsd;
	char gsmopen_call_id[512];
	int gsmopen_call_ongoing;
	char gsmopen_friends[4096];
	char gsmopen_fullname[512];
	char gsmopen_displayname[512];
	int phone_callflow;			/*!< \brief 'callflow' of the gsmopen interface (as opposed to phone interface) */
	int gsmopen;				/*!< \brief config flag, bool, GSM support on this interface (0 if false, -1 if true) */
	int control_to_send;
#ifdef WIN32
	switch_file_t *audiopipe[2];
	switch_file_t *audiogsmopenpipe[2];
	switch_file_t *gsmopen_sound_capt_fd;	/*!< \brief file descriptor for sound capture dev */
#else							/* WIN32 */
	int audiopipe[2];
	int audiogsmopenpipe[2];
	int gsmopen_sound_capt_fd;	/*!< \brief file descriptor for sound capture dev */
#endif							/* WIN32 */
	switch_thread_t *tcp_srv_thread;
	switch_thread_t *tcp_cli_thread;
	switch_thread_t *gsmopen_signaling_thread;
	switch_thread_t *gsmopen_api_thread;
	int gsmopen_dir_entry_extension_prefix;
	char gsmopen_user[256];
	char gsmopen_password[256];
	char destination[256];
	struct timeval answer_time;

	struct timeval transfer_time;
	char transfer_callid_number[50];
	char gsmopen_transfer_call_id[512];
	int running;
	unsigned long ib_calls;
	unsigned long ob_calls;
	unsigned long ib_failed_calls;
	unsigned long ob_failed_calls;

	char controldevice_name[512];	/*!< \brief name of the serial device controlling the interface, possibly none */
	int controldevprotocol;		/*!< \brief which protocol is used for serial control of this interface */
	char controldevprotocolname[50];	/*!< \brief name of the serial device controlling protocol, one of "at" "fbus2" "no_serial" "alsa_voicemodem" */
	int controldevfd;			/*!< \brief serial controlling file descriptor for this interface */
#ifdef WIN32
	int controldevice_speed;
#else
	speed_t controldevice_speed;
#endif							// WIN32
	int controldev_dead;

	char at_dial_pre_number[64];
	char at_dial_post_number[64];
	char at_dial_expect[64];
	unsigned int at_early_audio;
	char at_hangup[64];
	char at_hangup_expect[64];
	char at_answer[64];
	char at_answer_expect[64];
	unsigned int at_initial_pause;
	char at_preinit_1[64];
	char at_preinit_1_expect[64];
	char at_preinit_2[64];
	char at_preinit_2_expect[64];
	char at_preinit_3[64];
	char at_preinit_3_expect[64];
	char at_preinit_4[64];
	char at_preinit_4_expect[64];
	char at_preinit_5[64];
	char at_preinit_5_expect[64];
	unsigned int at_after_preinit_pause;

	char at_postinit_1[64];
	char at_postinit_1_expect[64];
	char at_postinit_2[64];
	char at_postinit_2_expect[64];
	char at_postinit_3[64];
	char at_postinit_3_expect[64];
	char at_postinit_4[64];
	char at_postinit_4_expect[64];
	char at_postinit_5[64];
	char at_postinit_5_expect[64];

	char at_send_dtmf[64];

	char at_query_battchg[64];
	char at_query_battchg_expect[64];
	char at_query_signal[64];
	char at_query_signal_expect[64];
	char at_call_idle[64];
	char at_call_incoming[64];
	char at_call_active[64];
	char at_call_failed[64];
	char at_call_calling[64];

#define CIEV_STRING_SIZE 64
	char at_indicator_noservice_string[64];
	char at_indicator_nosignal_string[64];
	char at_indicator_lowsignal_string[64];
	char at_indicator_lowbattchg_string[64];
	char at_indicator_nobattchg_string[64];
	char at_indicator_callactive_string[64];
	char at_indicator_nocallactive_string[64];
	char at_indicator_nocallsetup_string[64];
	char at_indicator_callsetupincoming_string[64];
	char at_indicator_callsetupoutgoing_string[64];
	char at_indicator_callsetupremoteringing_string[64];

	int at_indicator_callp;
	int at_indicator_callsetupp;
	int at_indicator_roamp;
	int at_indicator_battchgp;
	int at_indicator_servicep;
	int at_indicator_signalp;

	int at_has_clcc;
	int at_has_ecam;

	char at_cmgw[16];
	int no_ucs2;
	time_t gsmopen_serial_sync_period;

	time_t gsmopen_serial_synced_timestamp;
	struct s_result line_array;

	int unread_sms_msg_id;
	int reading_sms_msg;
	char sms_message[4800];
	char sms_sender[256];
	char sms_date[256];
	char sms_userdataheader[256];
	char sms_body[4800];
	char sms_datacodingscheme[256];
	char sms_servicecentreaddress[256];
	int sms_messagetype;
	int sms_cnmi_not_supported;
	int sms_pdu_not_supported;

	int  ussd_request_encoding;
	int  ussd_response_encoding;
	int  ussd_request_hex;
	int  ussd_received;
	int  ussd_status;
	char ussd_message[1024];
	char ussd_dcs[256];

	struct timeval call_incoming_time;
	switch_mutex_t *controldev_lock;

	int phonebook_listing;
	int phonebook_querying;
	int phonebook_listing_received_calls;

	int phonebook_first_entry;
	int phonebook_last_entry;
	int phonebook_number_lenght;
	int phonebook_text_lenght;
	FILE *phonebook_writing_fp;

	struct timeval ringtime;
	ciapa_t *owner;

	time_t audio_play_reset_timestamp;
	int audio_play_reset_period;

	switch_timer_t timer_read;
	switch_timer_t timer_write;
	teletone_dtmf_detect_state_t dtmf_detect;
	switch_time_t old_dtmf_timestamp;

	int no_sound;

	dtmf_rx_state_t dtmf_state;
	int active;
	int home_network_registered;
	int roaming_registered;
	int not_registered;
	int got_signal;
	char imei[128];
	int requesting_imei;
	char imsi[128];
	int requesting_imsi;
	char operator_name[128];
	int requesting_operator_name;
	char subscriber_number[128];
	int requesting_subscriber_number;
	char device_mfg[128];
	int requesting_device_mfg;
	char device_model[128];
	int requesting_device_model;
	char device_firmware[128];
	int requesting_device_firmware;
	int network_creg_not_supported;
	char creg[128];

	char controldevice_audio_name[512];
	int controldev_audio_fd;
	int controldevice_audio_speed;
	int controldev_audio_dead;
	switch_mutex_t *controldev_audio_lock;
	               ctb::SerialPort * serialPort_serial_audio;

	               ctb::SerialPort * serialPort_serial_control;

	char buffer2[320];
	int buffer2_full;
	int serialPort_serial_audio_opened;

};

typedef struct private_object private_t;

void *SWITCH_THREAD_FUNC gsmopen_api_thread_func(switch_thread_t *thread, void *obj);
int gsmopen_audio_read(private_t *tech_pvt);
int gsmopen_audio_init(private_t *tech_pvt);
int gsmopen_signaling_read(private_t *tech_pvt);

int gsmopen_call(private_t *tech_pvt, char *idest, int timeout);
int gsmopen_senddigit(private_t *tech_pvt, char digit);

void *gsmopen_do_tcp_srv_thread_func(void *obj);
void *SWITCH_THREAD_FUNC gsmopen_do_tcp_srv_thread(switch_thread_t *thread, void *obj);

void *gsmopen_do_tcp_cli_thread_func(void *obj);
void *SWITCH_THREAD_FUNC gsmopen_do_tcp_cli_thread(switch_thread_t *thread, void *obj);

void *gsmopen_do_gsmopenapi_thread_func(void *obj);
void *SWITCH_THREAD_FUNC gsmopen_do_gsmopenapi_thread(switch_thread_t *thread, void *obj);
int dtmf_received(private_t *tech_pvt, char *value);
int start_audio_threads(private_t *tech_pvt);
int new_inbound_channel(private_t *tech_pvt);
int outbound_channel_answered(private_t *tech_pvt);
#if defined(WIN32) && !defined(__CYGWIN__)
int gsmopen_pipe_read(switch_file_t *pipe, short *buf, int howmany);
int gsmopen_pipe_write(switch_file_t *pipe, short *buf, int howmany);
/* Visual C do not have strsep ? */
char *strsep(char **stringp, const char *delim);
#else
int gsmopen_pipe_read(int pipe, short *buf, int howmany);
int gsmopen_pipe_write(int pipe, short *buf, int howmany);
#endif /* WIN32 */
int gsmopen_close_socket(unsigned int fd);
private_t *find_available_gsmopen_interface_rr(private_t *tech_pvt_calling);
int remote_party_is_ringing(private_t *tech_pvt);
int remote_party_is_early_media(private_t *tech_pvt);
int gsmopen_socket_create_and_bind(private_t *tech_pvt, int *which_port);

void *gsmopen_do_controldev_thread(void *data);
int gsmopen_serial_init(private_t *tech_pvt, int controldevice_speed);
int gsmopen_serial_monitor(private_t *tech_pvt);
int gsmopen_serial_sync(private_t *tech_pvt);
int gsmopen_serial_sync_AT(private_t *tech_pvt);
int gsmopen_serial_config(private_t *tech_pvt);
int gsmopen_serial_config_AT(private_t *tech_pvt);

#define gsmopen_serial_write_AT_expect(P, D, S) gsmopen_serial_write_AT_expect1(P, D, S, 1, 0)
#define gsmopen_serial_write_AT_expect_noexpcr(P, D, S) gsmopen_serial_write_AT_expect1(P, D, S, 0, 0)
#define gsmopen_serial_write_AT_expect_noexpcr_tout(P, D, S, T) gsmopen_serial_write_AT_expect1(P, D, S, 0, T)
#define gsmopen_serial_write_AT_expect_longtime(P, D, S) gsmopen_serial_write_AT_expect1(P, D, S, 1, 5)
#define gsmopen_serial_write_AT_expect_longtime_noexpcr(P, D, S) gsmopen_serial_write_AT_expect1(P, D, S, 0, 5)
int gsmopen_serial_write_AT(private_t *tech_pvt, const char *data);
int gsmopen_serial_write_AT_nocr(private_t *tech_pvt, const char *data);
int gsmopen_serial_write_AT_ack(private_t *tech_pvt, const char *data);
int gsmopen_serial_write_AT_ack_nocr_longtime(private_t *tech_pvt, const char *data);
int gsmopen_serial_write_AT_noack(private_t *tech_pvt, const char *data);
int gsmopen_serial_write_AT_expect1(private_t *tech_pvt, const char *data, const char *expected_string, int expect_crlf, int seconds);
int gsmopen_serial_AT_expect(private_t *tech_pvt, const char *expected_string, int expect_crlf, int seconds);
int gsmopen_serial_read_AT(private_t *tech_pvt, int look_for_ack, int timeout_usec, int timeout_sec, const char *expected_string, int expect_crlf);
int gsmopen_serial_read(private_t *tech_pvt);
#define RESULT_FAILURE 0
#define RESULT_SUCCESS 1
int utf8_to_ucs2(private_t *tech_pvt, char *utf8_in, size_t inbytesleft, char *ucs2_out, size_t outbytesleft);
int ucs2_to_utf8(private_t *tech_pvt, char *ucs2_in, char *utf8_out, size_t outbytesleft);
int utf8_to_iso_8859_1(private_t *tech_pvt, char *utf8_in, size_t inbytesleft, char *iso_8859_1_out, size_t outbytesleft);
#define PUSHA_UNLOCKA(x)    if(option_debug > 100) ERRORA("PUSHA_UNLOCKA: %p\n", GSMOPEN_P_LOG, (void *)x);
#define POPPA_UNLOCKA(x)    if(option_debug > 100) ERRORA("POPPA_UNLOCKA: %p\n", GSMOPEN_P_LOG, (void *)x);
#define LOKKA(x)    switch_mutex_lock(x);
#define UNLOCKA(x)  switch_mutex_unlock(x);

#define gsmopen_queue_control(x, y) ERRORA("gsmopen_queue_control: %p, %d\n", GSMOPEN_P_LOG, (void *)x, y);

#define ast_setstate(x, y) ERRORA("ast_setstate: %p, %d\n", GSMOPEN_P_LOG, (void *)x, y);

int gsmopen_serial_read(private_t *tech_pvt);
int gsmopen_answer(private_t *tech_pvt);
int gsmopen_serial_answer(private_t *tech_pvt);
int gsmopen_serial_answer_AT(private_t *tech_pvt);
int gsmopen_serial_hangup(private_t *tech_pvt);
int gsmopen_serial_hangup_AT(private_t *tech_pvt);
int gsmopen_hangup(private_t *tech_pvt);
int gsmopen_serial_call(private_t *tech_pvt, char *dstr);
int gsmopen_serial_call_AT(private_t *tech_pvt, char *dstr);
int gsmopen_sendsms(private_t *tech_pvt, char *dest, char *text);

void gsmopen_store_boost(char *s, double *boost);
int gsmopen_sound_boost(void *data, int samples_num, double boost);
int sms_incoming(private_t *tech_pvt);
int gsmopen_ring(private_t *tech_pvt);

int iso_8859_1_to_utf8(private_t *tech_pvt, char *iso_8859_1_in, char *utf8_out, size_t outbytesleft);
int gsmopen_serial_getstatus_AT(private_t *tech_pvt);

int dump_event(private_t *tech_pvt);
int alarm_event(private_t *tech_pvt, int alarm_code, const char *alarm_message);
int dump_event_full(private_t *tech_pvt, int is_alarm, int alarm_code, const char *alarm_message);

int gsmopen_serial_init_audio_port(private_t *tech_pvt, int controldevice_audio_speed);
int serial_audio_init(private_t *tech_pvt);
int serial_audio_shutdown(private_t *tech_pvt);
#ifndef WIN32
void find_ttyusb_devices(private_t *tech_pvt, const char *dirname);
#endif// WIN32
int gsmopen_ussd(private_t *tech_pvt, char *ussd, int waittime);
int ussd_incoming(private_t *tech_pvt);
