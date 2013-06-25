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

#include "gsmopen.h"

#if 0
#ifdef WIN32
/***************/
// from http://www.openasthra.com/c-tidbits/gettimeofday-function-for-windows/

#include <time.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else /*  */
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif /*  */
struct sk_timezone {
	int tz_minuteswest;			/* minutes W of Greenwich */
	int tz_dsttime;				/* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct sk_timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;
	if (NULL != tv) {
		GetSystemTimeAsFileTime(&ft);
		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch */
		tmpres /= 10;			/*convert into microseconds */
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long) (tmpres / 1000000UL);
		tv->tv_usec = (long) (tmpres % 1000000UL);
	}
	if (NULL != tz) {
		if (!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
	return 0;
}

/***************/
#endif /* WIN32 */
#endif //0
SWITCH_BEGIN_EXTERN_C SWITCH_MODULE_LOAD_FUNCTION(mod_gsmopen_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_gsmopen_shutdown);
SWITCH_MODULE_DEFINITION(mod_gsmopen, mod_gsmopen_load, mod_gsmopen_shutdown, NULL);
SWITCH_END_EXTERN_C
#define GSMOPEN_CHAT_PROTO "sms"
#if 1
					  SWITCH_STANDARD_API(gsm_function);
/* BEGIN: Changes here */
#define GSM_SYNTAX "list [full] || console || AT_command || remove < interface_name | interface_id > || reload"
/* END: Changes heres */
SWITCH_STANDARD_API(gsmopen_function);
#define GSMOPEN_SYNTAX "interface_name AT_command"
#endif //0

SWITCH_STANDARD_API(gsmopen_boost_audio_function);
#define GSMOPEN_BOOST_AUDIO_SYNTAX "interface_name [<play|capt> <in decibels: -40 ... 0 ... +40>]"
SWITCH_STANDARD_API(sendsms_function);
#define SENDSMS_SYNTAX "gsmopen_sendsms interface_name destination_number SMS_text"
SWITCH_STANDARD_API(gsmopen_dump_function);
#define GSMOPEN_DUMP_SYNTAX "gsmopen_dump <interface_name|list>"
/* BEGIN: Changes here */
#define FULL_RELOAD 0
#define SOFT_RELOAD 1
/* END: Changes heres */

const char *interface_status[] = {	/* should match GSMOPEN_STATE_xxx in gsmopen.h */
	"IDLE",
	"DOWN",
	"RING",
	"DIALING",
	"BUSY",
	"UP",
	"RINGING",
	"PRERING",
	"DOUBLE",
	"SELECTD",
	"HANG_RQ",
	"PREANSW"
};

const char *phone_callflow[] = {	/* should match CALLFLOW_XXX in gsmopen.h */
	"CALL_IDLE",
	"CALL_DOWN",
	"INCOMING_RNG",
	"CALL_DIALING",
	"CALL_LINEBUSY",
	"CALL_ACTIVE",
	"INCOMING_HNG",
	"CALL_RLEASD",
	"CALL_NOCARR",
	"CALL_INFLUX",
	"CALL_INCOMING",
	"CALL_FAILED",
	"CALL_NOSRVC",
	"CALL_OUTRESTR",
	"CALL_SECFAIL",
	"CALL_NOANSWER",
	"STATUS_FNSHED",
	"STATUS_CANCLED",
	"STATUS_FAILED",
	"STATUS_REFUSED",
	"STATUS_RINGING",
	"STATUS_INPROGRS",
	"STATUS_UNPLACD",
	"STATUS_ROUTING",
	"STATUS_EARLYMD",
	"INCOMING_CLID",
	"STATUS_RMTEHOLD"
};

static struct {
	int debug;
	char *ip;
	int port;
	char *dialplan;
	char *destination;
	char *context;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	unsigned int flags;
	int fd;
	int calls;
	int real_interfaces;
	int next_interface;
	char hold_music[256];
	private_t GSMOPEN_INTERFACES[GSMOPEN_MAX_INTERFACES];
	switch_mutex_t *mutex;
	private_t *gsm_console;
} globals;

switch_endpoint_interface_t *gsmopen_endpoint_interface;
switch_memory_pool_t *gsmopen_module_pool = NULL;
int running = 0;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_context, globals.context);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_destination, globals.destination);
//SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string);
//SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_rates_string, globals.codec_rates_string);

/* BEGIN: Changes here */
static switch_status_t interface_exists(char *the_interface);
#if 1
static switch_status_t remove_interface(char *the_interface);
#endif //0
/* END: Changes here */

static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_hangup(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);
static switch_status_t channel_on_routing(switch_core_session_t *session);
static switch_status_t channel_on_exchange_media(switch_core_session_t *session);
static switch_status_t channel_on_consume_media(switch_core_session_t *session);
static switch_status_t channel_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session,
													switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);
static switch_status_t gsmopen_tech_init(private_t *tech_pvt, switch_core_session_t *session);

static switch_status_t gsmopen_codec(private_t *tech_pvt, int sample_rate, int codec_ms)
{
	switch_core_session_t *session = NULL;

	if (switch_core_codec_init
		(&tech_pvt->read_codec, "L16", NULL, sample_rate, codec_ms, 1,
		 SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		ERRORA("Can't load codec?\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init
		(&tech_pvt->write_codec, "L16", NULL, sample_rate, codec_ms, 1,
		 SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		ERRORA("Can't load codec?\n", GSMOPEN_P_LOG);
		switch_core_codec_destroy(&tech_pvt->read_codec);
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->read_frame.rate = sample_rate;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;

	session = switch_core_session_locate(tech_pvt->session_uuid_str);

	if (session) {
		switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
		switch_core_session_set_write_codec(session, &tech_pvt->write_codec);
		switch_core_session_rwunlock(session);
	} else {
		ERRORA("no session\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t gsmopen_tech_init(private_t *tech_pvt, switch_core_session_t *session)
{

	switch_assert(tech_pvt != NULL);
	switch_assert(session != NULL);
	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_core_session_set_private(session, tech_pvt);
	switch_copy_string(tech_pvt->session_uuid_str, switch_core_session_get_uuid(session), sizeof(tech_pvt->session_uuid_str));
	if (!strlen(tech_pvt->session_uuid_str)) {
		ERRORA("no tech_pvt->session_uuid_str\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;
	}
	if (gsmopen_codec(tech_pvt, SAMPLERATE_GSMOPEN, 20) != SWITCH_STATUS_SUCCESS) {
		ERRORA("gsmopen_codec FAILED\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;
	}
	//teletone_dtmf_detect_init(&tech_pvt->dtmf_detect, tech_pvt->read_codec.implementation->actual_samples_per_second);
	//teletone_dtmf_detect_init(&tech_pvt->dtmf_detect, 8000);
	dtmf_rx_init(&tech_pvt->dtmf_state, NULL, NULL);
	dtmf_rx_parms(&tech_pvt->dtmf_state, 0, 10, 10, -99);

	if (tech_pvt->no_sound == 0) {
		if (serial_audio_init(tech_pvt)) {
			ERRORA("serial_audio_init failed\n", GSMOPEN_P_LOG);
			return SWITCH_STATUS_FALSE;

		}
	}

	if (switch_core_timer_init(&tech_pvt->timer_read, "soft", 20, tech_pvt->read_codec.implementation->samples_per_packet, gsmopen_module_pool) !=
		SWITCH_STATUS_SUCCESS) {
		ERRORA("setup timer failed\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;
	}

	switch_core_timer_sync(&tech_pvt->timer_read);

	if (switch_core_timer_init(&tech_pvt->timer_write, "soft", 20, tech_pvt->write_codec.implementation->samples_per_packet, gsmopen_module_pool) !=
		SWITCH_STATUS_SUCCESS) {
		ERRORA("setup timer failed\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;
	}

	switch_core_timer_sync(&tech_pvt->timer_write);

	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_clear_flag(tech_pvt, TFLAG_HANGUP);
	switch_mutex_unlock(tech_pvt->flag_mutex);
	DEBUGA_GSMOPEN("gsmopen_codec SUCCESS\n", GSMOPEN_P_LOG);
	return SWITCH_STATUS_SUCCESS;
}

/* BEGIN: Changes here */
static switch_status_t interface_exists(char *the_interface)
{
	int i;
	int interface_id;

	if (*the_interface == '#') {	/* look by interface id or interface name */
		the_interface++;
		switch_assert(the_interface);
		interface_id = atoi(the_interface);

		/* take a number as interface id */
		if (interface_id > 0 || (interface_id == 0 && strcmp(the_interface, "0") == 0)) {
			if (strlen(globals.GSMOPEN_INTERFACES[interface_id].name)) {
				return SWITCH_STATUS_SUCCESS;
			}
		} else {
			/* interface name */
			for (interface_id = 0; interface_id < GSMOPEN_MAX_INTERFACES; interface_id++) {
				if (strcmp(globals.GSMOPEN_INTERFACES[interface_id].name, the_interface) == 0) {
					return SWITCH_STATUS_SUCCESS;
					break;
				}
			}
		}
	} else {					/* look by gsmopen_user */

		for (i = 0; i < GSMOPEN_MAX_INTERFACES; i++) {
			if (strlen(globals.GSMOPEN_INTERFACES[i].gsmopen_user)) {
				if (strcmp(globals.GSMOPEN_INTERFACES[i].gsmopen_user, the_interface) == 0) {
					return SWITCH_STATUS_SUCCESS;
				}
			}
		}
	}
	return SWITCH_STATUS_FALSE;
}

#if 1
static switch_status_t remove_interface(char *the_interface)
{
	int x = 10;
	int fd;
#ifdef WIN32
	switch_size_t howmany = 8;
#else
	unsigned int howmany = 8;
#endif

	int interface_id = -1;
	private_t *tech_pvt = NULL;
	switch_status_t status;

	//running = 0;

	//XXX if (*the_interface == '#') {  /* remove by interface id or interface name */
	//XXX the_interface++;
	switch_assert(the_interface);
	interface_id = atoi(the_interface);

	if (interface_id > 0 || (interface_id == 0 && strcmp(the_interface, "0") == 0)) {
		/* take a number as interface id */
		tech_pvt = &globals.GSMOPEN_INTERFACES[interface_id];
	} else {

		for (interface_id = 0; interface_id < GSMOPEN_MAX_INTERFACES; interface_id++) {
			if (strcmp(globals.GSMOPEN_INTERFACES[interface_id].name, the_interface) == 0) {
				tech_pvt = &globals.GSMOPEN_INTERFACES[interface_id];
				break;
			}
		}
	}
	//XXX } //else {                    /* remove by gsmopen_user */
	//for (interface_id = 0; interface_id < GSMOPEN_MAX_INTERFACES; interface_id++) {
	//if (strcmp(globals.GSMOPEN_INTERFACES[interface_id].gsmopen_user, the_interface) == 0) {
	//tech_pvt = &globals.GSMOPEN_INTERFACES[interface_id];
	//break;
	//}
	//}
	//}

	if (!tech_pvt) {
		DEBUGA_GSMOPEN("interface '%s' does not exist\n", GSMOPEN_P_LOG, the_interface);
		goto end;
	}

	if (strlen(globals.GSMOPEN_INTERFACES[interface_id].session_uuid_str)) {
		DEBUGA_GSMOPEN("interface '%s' is busy\n", GSMOPEN_P_LOG, the_interface);
		goto end;
	}

		LOKKA(tech_pvt->controldev_lock);

	globals.GSMOPEN_INTERFACES[interface_id].running = 0;

	if (globals.GSMOPEN_INTERFACES[interface_id].gsmopen_signaling_thread) {
#if 1
#ifdef WIN32
		switch_file_write(tech_pvt->GSMopenHandles.fdesc[1], "sciutati", &howmany);	// let's the controldev_thread die
#else /* WIN32 */
		howmany = write(tech_pvt->GSMopenHandles.fdesc[1], "sciutati", howmany);
#endif /* WIN32 */
#endif //0
		DEBUGA_GSMOPEN("HERE will shutdown gsmopen_signaling_thread of '%s'\n", GSMOPEN_P_LOG, the_interface);
	}

	if (globals.GSMOPEN_INTERFACES[interface_id].gsmopen_api_thread) {
#if 0
#ifdef WIN32
		if (SendMessage(tech_pvt->GSMopenHandles.win32_hInit_MainWindowHandle, WM_DESTROY, 0, 0) == FALSE) {	// let's the gsmopen_api_thread_func die
			DEBUGA_GSMOPEN("got FALSE here, thread probably was already dead. GetLastError returned: %d\n", GSMOPEN_P_LOG, GetLastError());
			globals.GSMOPEN_INTERFACES[interface_id].gsmopen_api_thread = NULL;
		}
#else
		XEvent e;
		Atom atom1 = XInternAtom(tech_pvt->GSMopenHandles.disp, "GSMOPENCONTROLAPI_MESSAGE_BEGIN", False);
		memset(&e, 0, sizeof(e));
		e.xclient.type = ClientMessage;
		e.xclient.message_type = atom1;	/*  leading message */
		e.xclient.display = tech_pvt->GSMopenHandles.disp;
		e.xclient.window = tech_pvt->GSMopenHandles.gsmopen_win;
		e.xclient.format = 8;

		XSendEvent(tech_pvt->GSMopenHandles.disp, tech_pvt->GSMopenHandles.win, False, 0, &e);
		XSync(tech_pvt->GSMopenHandles.disp, False);
#endif //WIN32
#endif //0

		DEBUGA_GSMOPEN("HERE will shutdown gsmopen_api_thread of '%s'\n", GSMOPEN_P_LOG, the_interface);
	}

	while (x) {
		x--;
		switch_yield(50000);
	}

	if (globals.GSMOPEN_INTERFACES[interface_id].gsmopen_signaling_thread) {
		switch_thread_join(&status, globals.GSMOPEN_INTERFACES[interface_id].gsmopen_signaling_thread);
	}

	if (globals.GSMOPEN_INTERFACES[interface_id].gsmopen_api_thread) {
		switch_thread_join(&status, globals.GSMOPEN_INTERFACES[interface_id].gsmopen_api_thread);
	}

	fd = tech_pvt->controldevfd;
	//DEBUGA_GSMOPEN("SHUTDOWN tech_pvt->controldevfd=%d\n", GSMOPEN_P_LOG, tech_pvt->controldevfd);
	if (fd) {
		//close(fd);
		tech_pvt->controldevfd = -1;
		DEBUGA_GSMOPEN("SHUTDOWN tech_pvt->controldevfd=%d\n", GSMOPEN_P_LOG, tech_pvt->controldevfd);
	}

	serial_audio_shutdown(tech_pvt);

	int res;
	res = tech_pvt->serialPort_serial_control->Close();
	DEBUGA_GSMOPEN("serial_shutdown res=%d (controldevfd is %d)\n", GSMOPEN_P_LOG, res, tech_pvt->controldevfd);

#ifndef WIN32
	shutdown(tech_pvt->audiogsmopenpipe[0], 2);
	close(tech_pvt->audiogsmopenpipe[0]);
	shutdown(tech_pvt->audiogsmopenpipe[1], 2);
	close(tech_pvt->audiogsmopenpipe[1]);
	shutdown(tech_pvt->audiopipe[0], 2);
	close(tech_pvt->audiopipe[0]);
	shutdown(tech_pvt->audiopipe[1], 2);
	close(tech_pvt->audiopipe[1]);
	shutdown(tech_pvt->GSMopenHandles.fdesc[0], 2);
	close(tech_pvt->GSMopenHandles.fdesc[0]);
	shutdown(tech_pvt->GSMopenHandles.fdesc[1], 2);
	close(tech_pvt->GSMopenHandles.fdesc[1]);
#endif /* WIN32 */

		UNLOCKA(tech_pvt->controldev_lock);
	switch_mutex_lock(globals.mutex);
	if (globals.gsm_console == &globals.GSMOPEN_INTERFACES[interface_id]) {
		DEBUGA_GSMOPEN("interface '%s' no more console\n", GSMOPEN_P_LOG, the_interface);
		globals.gsm_console = NULL;
	} else {
		DEBUGA_GSMOPEN("interface '%s' STILL console\n", GSMOPEN_P_LOG, the_interface);
	}
	memset(&globals.GSMOPEN_INTERFACES[interface_id], '\0', sizeof(private_t));
	globals.real_interfaces--;
	switch_mutex_unlock(globals.mutex);

	DEBUGA_GSMOPEN("interface '%s' deleted successfully\n", GSMOPEN_P_LOG, the_interface);
	globals.GSMOPEN_INTERFACES[interface_id].running = 1;

  end:
	//running = 1;
	return SWITCH_STATUS_SUCCESS;
}
#endif //0

/* END: Changes here */

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt = NULL;

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);
	//ERRORA("%s CHANNEL INIT\n", GSMOPEN_P_LOG, tech_pvt->name);
	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_set_flag(tech_pvt, TFLAG_IO);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	/* Move channel's state machine to ROUTING. This means the call is trying
	   to get from the initial start where the call because, to the point
	   where a destination has been identified. If the channel is simply
	   left in the initial state, nothing will happen. */
	switch_channel_set_state(channel, CS_ROUTING);
	switch_mutex_lock(globals.mutex);
	globals.calls++;

	switch_mutex_unlock(globals.mutex);
	DEBUGA_GSMOPEN("%s CHANNEL INIT %s\n", GSMOPEN_P_LOG, tech_pvt->name, switch_core_session_get_uuid(session));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;

	tech_pvt = (private_t *) switch_core_session_get_private(session);

	if (tech_pvt) {
		DEBUGA_GSMOPEN("%s CHANNEL DESTROY %s\n", GSMOPEN_P_LOG, tech_pvt->name, switch_core_session_get_uuid(session));

		if (switch_core_codec_ready(&tech_pvt->read_codec)) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->write_codec)) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}

		switch_core_timer_destroy(&tech_pvt->timer_read);
		switch_core_timer_destroy(&tech_pvt->timer_write);

		if (tech_pvt->no_sound == 0) {
			serial_audio_shutdown(tech_pvt);
		}

		*tech_pvt->session_uuid_str = '\0';
		tech_pvt->interface_state = GSMOPEN_STATE_IDLE;
		if (tech_pvt->phone_callflow == CALLFLOW_STATUS_FINISHED) {
			tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
		}
		switch_core_session_set_private(session, NULL);
	} else {
		DEBUGA_GSMOPEN("!!!!!!NO tech_pvt!!!! CHANNEL DESTROY %s\n", GSMOPEN_P_LOG, switch_core_session_get_uuid(session));
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	tech_pvt->phone_callflow = CALLFLOW_CALL_HANGUP_REQUESTED;

	if (!switch_channel_test_flag(channel, CF_ANSWERED)) {
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			tech_pvt->ob_failed_calls++;
		} else {
			tech_pvt->ib_failed_calls++;
		}
	}

	DEBUGA_GSMOPEN("%s CHANNEL HANGUP\n", GSMOPEN_P_LOG, tech_pvt->name);
	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_clear_flag(tech_pvt, TFLAG_IO);
	switch_clear_flag(tech_pvt, TFLAG_VOICE);
	switch_set_flag(tech_pvt, TFLAG_HANGUP);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	gsmopen_hangup(tech_pvt);

	//memset(tech_pvt->session_uuid_str, '\0', sizeof(tech_pvt->session_uuid_str));
	//*tech_pvt->session_uuid_str = '\0';
	DEBUGA_GSMOPEN("%s CHANNEL HANGUP\n", GSMOPEN_P_LOG, tech_pvt->name);
	switch_mutex_lock(globals.mutex);
	globals.calls--;
	if (globals.calls < 0) {
		globals.calls = 0;
	}

	tech_pvt->interface_state = GSMOPEN_STATE_IDLE;
	//FIXME if (tech_pvt->phone_callflow == CALLFLOW_STATUS_FINISHED) {
	tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
	//FIXME }
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_GSMOPEN("%s CHANNEL ROUTING\n", GSMOPEN_P_LOG, tech_pvt->name);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_GSMOPEN("%s CHANNEL EXECUTE\n", GSMOPEN_P_LOG, tech_pvt->name);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_GSMOPEN("%s CHANNEL KILL_CHANNEL\n", GSMOPEN_P_LOG, tech_pvt->name);
	switch (sig) {
	case SWITCH_SIG_KILL:
		DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_SIG_KILL\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
		switch_mutex_lock(tech_pvt->flag_mutex);
		switch_clear_flag(tech_pvt, TFLAG_IO);
		switch_clear_flag(tech_pvt, TFLAG_VOICE);
		switch_set_flag(tech_pvt, TFLAG_HANGUP);
		switch_mutex_unlock(tech_pvt->flag_mutex);
		break;
	case SWITCH_SIG_BREAK:
		DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_SIG_BREAK\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
		//switch_set_flag(tech_pvt, TFLAG_BREAK);
		switch_mutex_lock(tech_pvt->flag_mutex);
		switch_set_flag(tech_pvt, TFLAG_BREAK);
		switch_mutex_unlock(tech_pvt->flag_mutex);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_consume_media(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;

	tech_pvt = (private_t *) switch_core_session_get_private(session);

	DEBUGA_GSMOPEN("%s CHANNEL CONSUME_MEDIA\n", GSMOPEN_P_LOG, tech_pvt->name);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;
	tech_pvt = (private_t *) switch_core_session_get_private(session);
	DEBUGA_GSMOPEN("%s CHANNEL EXCHANGE_MEDIA\n", GSMOPEN_P_LOG, tech_pvt->name);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;
	tech_pvt = (private_t *) switch_core_session_get_private(session);
	DEBUGA_GSMOPEN("%s CHANNEL SOFT_EXECUTE\n", GSMOPEN_P_LOG, tech_pvt->name);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_GSMOPEN("%s CHANNEL SEND_DTMF\n", GSMOPEN_P_LOG, tech_pvt->name);
	DEBUGA_GSMOPEN("DTMF: %c\n", GSMOPEN_P_LOG, dtmf->digit);

	gsmopen_senddigit(tech_pvt, dtmf->digit);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	switch_byte_t *data;

	int samples;
	int samples2;
	char digit_str[256];
	char buffer2[640];

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_channel_ready(channel) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
		ERRORA("channel not ready \n", GSMOPEN_P_LOG);
		//TODO: kill the bastard
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->read_frame.flags = SFF_NONE;
	*frame = NULL;

	if (switch_test_flag(tech_pvt, TFLAG_HANGUP)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_core_timer_next(&tech_pvt->timer_read);

	if (tech_pvt->no_sound) {
		goto cng;
	}
	samples = tech_pvt->serialPort_serial_audio->Read(buffer2, 640);

	if (samples >= 320) {
		tech_pvt->buffer2_full = 0;

		if (samples >= 640) {
			DEBUGA_GSMOPEN("read more than 320, samples=%d\n", GSMOPEN_P_LOG, samples);
			memcpy(tech_pvt->buffer2, buffer2 + 320, 320);
			tech_pvt->buffer2_full = 1;
		}
		samples = 320;
		memcpy(tech_pvt->read_frame.data, buffer2, 320);
	} else {
		if (samples != 0) {
			DEBUGA_GSMOPEN("read less than 320, samples=%d\n", GSMOPEN_P_LOG, samples);
		}
		if (tech_pvt->buffer2_full) {
			memcpy(tech_pvt->read_frame.data, tech_pvt->buffer2, 320);
			tech_pvt->buffer2_full = 0;
			samples = 320;
			DEBUGA_GSMOPEN("samples=%d FROM BUFFER\n", GSMOPEN_P_LOG, samples);
		}

	}

	tech_pvt->read_frame.datalen = samples;
	tech_pvt->read_frame.samples = samples / 2;
	tech_pvt->read_frame.timestamp = tech_pvt->timer_read.samplecount;

	*frame = &tech_pvt->read_frame;

	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_set_flag(tech_pvt, TFLAG_VOICE);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	if (samples != 320) {
		if (samples != 0) {
			DEBUGA_GSMOPEN("samples=%d, goto cng\n", GSMOPEN_P_LOG, samples);
		}
		goto cng;
	}

	memset(digit_str, 0, sizeof(digit_str));
	dtmf_rx(&tech_pvt->dtmf_state, (int16_t *) tech_pvt->read_frame.data, tech_pvt->read_frame.samples);
	dtmf_rx_get(&tech_pvt->dtmf_state, digit_str, sizeof(digit_str));

	gsmopen_sound_boost(tech_pvt->read_frame.data, tech_pvt->read_frame.samples, tech_pvt->capture_boost);

	if (digit_str[0]) {
		switch_time_t new_dtmf_timestamp = switch_time_now();
		if ((new_dtmf_timestamp - tech_pvt->old_dtmf_timestamp) > 350000) {	//FIXME: make it configurable
			char *p = digit_str;
			switch_channel_t *channel = switch_core_session_get_channel(session);

			while (p && *p) {
				switch_dtmf_t dtmf = { 0 };
				dtmf.digit = *p;
				dtmf.duration = SWITCH_DEFAULT_DTMF_DURATION;
				switch_channel_queue_dtmf(channel, &dtmf);
				p++;
			}
			DEBUGA_GSMOPEN("DTMF DETECTED: [%s] new_dtmf_timestamp: %u, delta_t: %u\n", GSMOPEN_P_LOG, digit_str, (unsigned int) new_dtmf_timestamp,
						   (unsigned int) (new_dtmf_timestamp - tech_pvt->old_dtmf_timestamp));
			tech_pvt->old_dtmf_timestamp = new_dtmf_timestamp;
		}
	}
	while (switch_test_flag(tech_pvt, TFLAG_IO)) {
		if (switch_test_flag(tech_pvt, TFLAG_BREAK)) {
			switch_mutex_lock(tech_pvt->flag_mutex);
			switch_clear_flag(tech_pvt, TFLAG_BREAK);
			switch_mutex_unlock(tech_pvt->flag_mutex);
			DEBUGA_GSMOPEN("BREAK: CHANNEL READ FRAME goto CNG\n", GSMOPEN_P_LOG);
			goto cng;
		}

		if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
			DEBUGA_GSMOPEN("CHANNEL READ FRAME not IO\n", GSMOPEN_P_LOG);
			return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(tech_pvt, TFLAG_IO) && switch_test_flag(tech_pvt, TFLAG_VOICE)) {
			switch_mutex_lock(tech_pvt->flag_mutex);
			switch_clear_flag(tech_pvt, TFLAG_VOICE);
			switch_mutex_unlock(tech_pvt->flag_mutex);
			if (!tech_pvt->read_frame.datalen) {
				DEBUGA_GSMOPEN("CHANNEL READ CONTINUE\n", GSMOPEN_P_LOG);
				continue;
			}
			*frame = &tech_pvt->read_frame;
#ifdef BIGENDIAN
			if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
				switch_swap_linear((int16_t *) (*frame)->data, (int) (*frame)->datalen / 2);
			}
#endif
			//DEBUGA_GSMOPEN("HERE\n", GSMOPEN_P_LOG);
			return SWITCH_STATUS_SUCCESS;
		}

		DEBUGA_GSMOPEN("CHANNEL READ no TFLAG_VOICE\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;

	}

	DEBUGA_GSMOPEN("CHANNEL READ FALSE\n", GSMOPEN_P_LOG);
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

	unsigned int sent;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_channel_ready(channel) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
		ERRORA("channel not ready \n", GSMOPEN_P_LOG);
		//TODO: kill the bastard
		return SWITCH_STATUS_FALSE;
	}
#ifdef BIGENDIAN
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
#ifdef WIN32
		switch_swap_linear((int16_t *) frame->data, (int) frame->datalen / 2);
#else
		switch_swap_linear(frame->data, (int) frame->datalen / 2);
#endif //WIN32
	}
#endif

	//switch_core_timer_next(&tech_pvt->timer_write);
	gsmopen_sound_boost(frame->data, frame->samples, tech_pvt->playback_boost);
	if (!tech_pvt->no_sound) {
		sent = tech_pvt->serialPort_serial_audio->Write((char *) frame->data, (int) (frame->datalen));

		if (sent && sent != frame->datalen && sent != -1) {
			DEBUGA_GSMOPEN("sent %d\n", GSMOPEN_P_LOG, sent);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	private_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	//ERRORA("%s CHANNEL INIT\n", GSMOPEN_P_LOG, tech_pvt->name);
	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_set_flag(tech_pvt, TFLAG_IO);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	gsmopen_serial_answer(tech_pvt);

	switch_mutex_lock(globals.mutex);
	globals.calls++;

	switch_mutex_unlock(globals.mutex);
	DEBUGA_GSMOPEN("%s CHANNEL ANSWER %s\n", GSMOPEN_P_LOG, tech_pvt->name, switch_core_session_get_uuid(session));


	if (channel) {
		switch_channel_mark_answered(channel);
	}

	DEBUGA_GSMOPEN("ANSWERED! \n", GSMOPEN_P_LOG);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		{
			DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_MESSAGE_INDICATE_ANSWER\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
			if (tech_pvt->interface_state != GSMOPEN_STATE_UP && tech_pvt->phone_callflow != CALLFLOW_CALL_ACTIVE) {
				DEBUGA_GSMOPEN("MSG_ID=%d, TO BE ANSWERED!\n", GSMOPEN_P_LOG, msg->message_id);
				channel_answer_channel(session);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_MESSAGE_INDICATE_PROGRESS\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
			if (tech_pvt->interface_state != GSMOPEN_STATE_UP && tech_pvt->phone_callflow != CALLFLOW_CALL_ACTIVE) {
				DEBUGA_GSMOPEN("MSG_ID=%d, TO BE ANSWERED!\n", GSMOPEN_P_LOG, msg->message_id);
				channel_answer_channel(session);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:

		DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_MESSAGE_INDICATE_AUDIO_SYNC\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
		switch_core_timer_sync(&tech_pvt->timer_read);
		switch_core_timer_sync(&tech_pvt->timer_write);

		break;

	case SWITCH_MESSAGE_INDICATE_TRANSFER:
		DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_MESSAGE_INDICATE_TRANSFER\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
		break;
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_MESSAGE_INDICATE_BRIDGE\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
		break;
	case SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY:
		DEBUGA_GSMOPEN("%s CHANNEL got SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
		break;
	default:
		{
			if (msg->message_id != SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC && msg->message_id != SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC_COMPLETE) {
				DEBUGA_GSMOPEN("MSG_ID=%d\n", GSMOPEN_P_LOG, msg->message_id);
			}
		}
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	struct private_object *tech_pvt = (struct private_object *) switch_core_session_get_private(session);
	char *body = switch_event_get_body(event);
	switch_assert(tech_pvt != NULL);

	if (!body) {
		body = (char *) "";
	}

	WARNINGA("event: |||%s|||\n", GSMOPEN_P_LOG, body);

	return SWITCH_STATUS_SUCCESS;
}

switch_state_handler_table_t gsmopen_state_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media */ channel_on_consume_media,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ channel_on_destroy
};

switch_io_routines_t gsmopen_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message,
	/*.receive_event */ channel_receive_event
};

static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session,
													switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{
	private_t *tech_pvt = NULL;
	int result;

		if ((*new_session = switch_core_session_request_uuid(gsmopen_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool, switch_event_get_header(var_event, "origination_uuid"))) != 0) {

		switch_channel_t *channel = NULL;
		switch_caller_profile_t *caller_profile;
		char *rdest;
		int found = 0;
		char interface_name[256];

		DEBUGA_GSMOPEN("1 SESSION_REQUEST %s\n", GSMOPEN_P_LOG, switch_core_session_get_uuid(*new_session));
		switch_core_session_add_stream(*new_session, NULL);

		if (!zstr(outbound_profile->destination_number)) {
			int i;
			char *slash;

			switch_copy_string(interface_name, outbound_profile->destination_number, 255);
			slash = strrchr(interface_name, '/');
			*slash = '\0';

			switch_mutex_lock(globals.mutex);
			if (strncmp("ANY", interface_name, strlen(interface_name)) == 0 || strncmp("RR", interface_name, strlen(interface_name)) == 0) {
				/* we've been asked for the "ANY" interface, let's find the first idle interface */
				//DEBUGA_GSMOPEN("Finding one available gsmopen interface\n", GSMOPEN_P_LOG);
				//tech_pvt = find_available_gsmopen_interface(NULL);
				//if (tech_pvt)
				//found = 1;
				//} else if (strncmp("RR", interface_name, strlen(interface_name)) == 0) {
				/* Find the first idle interface using Round Robin */
				DEBUGA_GSMOPEN("Finding one available gsmopen interface RR\n", GSMOPEN_P_LOG);
				tech_pvt = find_available_gsmopen_interface_rr(NULL);
				if (tech_pvt) {
					found = 1;
					DEBUGA_GSMOPEN("FOUND one available gsmopen interface RR\n", GSMOPEN_P_LOG);
				}
			}

			for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
				/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
				if (strlen(globals.GSMOPEN_INTERFACES[i].name)
					&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, interface_name, strlen(interface_name)) == 0)) {
					if (strlen(globals.GSMOPEN_INTERFACES[i].session_uuid_str)) {
						DEBUGA_GSMOPEN
							("globals.GSMOPEN_INTERFACES[%d].name=|||%s||| session_uuid_str=|||%s||| is BUSY\n",
							 GSMOPEN_P_LOG, i, globals.GSMOPEN_INTERFACES[i].name, globals.GSMOPEN_INTERFACES[i].session_uuid_str);
						DEBUGA_GSMOPEN("1 SESSION_DESTROY %s\n", GSMOPEN_P_LOG, switch_core_session_get_uuid(*new_session));
						switch_core_session_destroy(new_session);
						switch_mutex_unlock(globals.mutex);
						return SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
					}

					DEBUGA_GSMOPEN("globals.GSMOPEN_INTERFACES[%d].name=|||%s|||?\n", GSMOPEN_P_LOG, i, globals.GSMOPEN_INTERFACES[i].name);
					tech_pvt = &globals.GSMOPEN_INTERFACES[i];
					found = 1;
					break;
				}

			}

		} else {
			ERRORA("Doh! no destination number?\n", GSMOPEN_P_LOG);
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (!found) {
			DEBUGA_GSMOPEN("Doh! no available interface for |||%s|||?\n", GSMOPEN_P_LOG, interface_name);
			DEBUGA_GSMOPEN("2 SESSION_DESTROY %s\n", GSMOPEN_P_LOG, switch_core_session_get_uuid(*new_session));
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			//return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			return SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
		}

		channel = switch_core_session_get_channel(*new_session);
		if (!channel) {
			ERRORA("Doh! no channel?\n", GSMOPEN_P_LOG);
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}
		if (gsmopen_tech_init(tech_pvt, *new_session) != SWITCH_STATUS_SUCCESS) {
			ERRORA("Doh! no tech_init?\n", GSMOPEN_P_LOG);
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (outbound_profile) {
			char name[128];

			snprintf(name, sizeof(name), "gsmopen/%s", outbound_profile->destination_number);
			//snprintf(name, sizeof(name), "gsmopen/%s", tech_pvt->name);
			switch_channel_set_name(channel, name);
			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			ERRORA("Doh! no caller profile\n", GSMOPEN_P_LOG);
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		tech_pvt->ob_calls++;

		rdest = strchr(caller_profile->destination_number, '/');
		*rdest++ = '\0';

		//gsmopen_call(tech_pvt, rdest, 30);

		switch_copy_string(tech_pvt->session_uuid_str, switch_core_session_get_uuid(*new_session), sizeof(tech_pvt->session_uuid_str));
		caller_profile = tech_pvt->caller_profile;
		caller_profile->destination_number = rdest;

		switch_mutex_lock(tech_pvt->flag_mutex);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		switch_mutex_unlock(tech_pvt->flag_mutex);
		switch_channel_set_state(channel, CS_INIT);
		result=gsmopen_call(tech_pvt, rdest, 30);
		switch_mutex_unlock(globals.mutex);
		if(result != 0){
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}
		return SWITCH_CAUSE_SUCCESS;
	}

	ERRORA("Doh! no new_session\n", GSMOPEN_P_LOG);
	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
}

/*!
 * \brief This thread runs during a call, and monitor the interface for signaling, like hangup, caller id, etc most of signaling is handled inside the gsmopen_signaling_read function
 *
 */

static switch_status_t load_config(int reload_type)
{
	const char *cf = "gsmopen.conf";
	switch_xml_t cfg, xml, global_settings, param, interfaces, myinterface;
	private_t *tech_pvt = NULL;

#ifdef WIN32
	DEBUGA_GSMOPEN("Windows CODEPAGE Input =%d\n", GSMOPEN_P_LOG, GetConsoleCP());
	SetConsoleCP(65001);
	DEBUGA_GSMOPEN("Windows CODEPAGE Input =%d\n", GSMOPEN_P_LOG, GetConsoleCP());
	DEBUGA_GSMOPEN("Windows CODEPAGE Output =%d\n", GSMOPEN_P_LOG, GetConsoleOutputCP());
	SetConsoleOutputCP(65001);
	DEBUGA_GSMOPEN("Windows CODEPAGE Output =%d\n", GSMOPEN_P_LOG, GetConsoleOutputCP());
	//let's hope to have unicode in console now. You need to use Lucida Console or, much better, Courier New font for the command prompt to show unicode
#endif // WIN32
	NOTICA("GSMOPEN Charset Output Test 0 %s\n", GSMOPEN_P_LOG, "èéòàù");
	NOTICA("GSMOPEN Charset Output Test 1 %s\n", GSMOPEN_P_LOG, "ç°§^£");
	NOTICA("GSMOPEN Charset Output Test 2 %s\n", GSMOPEN_P_LOG, "новости");
	NOTICA("GSMOPEN Charset Output Test 3 %s\n", GSMOPEN_P_LOG, "ﺎﻠﺠﻤﻋﺓ");
	NOTICA("GSMOPEN Charset Output Test 4 %s\n", GSMOPEN_P_LOG, "ראת");
	NOTICA("GSMOPEN Charset Output Test 5 %s\n", GSMOPEN_P_LOG, "לק");
	NOTICA("GSMOPEN Charset Output Test 6 %s\n", GSMOPEN_P_LOG, "人大");

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, gsmopen_module_pool);
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		ERRORA("open of %s failed\n", GSMOPEN_P_LOG, cf);
		running = 0;
		switch_xml_free(xml);
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_lock(globals.mutex);

	set_global_dialplan("XML");
	DEBUGA_GSMOPEN("Default globals.dialplan=%s\n", GSMOPEN_P_LOG, globals.dialplan);
	set_global_destination("5000");
	DEBUGA_GSMOPEN("Default globals.destination=%s\n", GSMOPEN_P_LOG, globals.destination);
	set_global_context("default");
	DEBUGA_GSMOPEN("Default globals.context=%s\n", GSMOPEN_P_LOG, globals.context);

	if ((global_settings = switch_xml_child(cfg, "global_settings"))) {
		for (param = switch_xml_child(global_settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				DEBUGA_GSMOPEN("globals.debug=%d\n", GSMOPEN_P_LOG, globals.debug);
				globals.debug = atoi(val);
				DEBUGA_GSMOPEN("globals.debug=%d\n", GSMOPEN_P_LOG, globals.debug);
			} else if (!strcasecmp(var, "hold-music")) {
				switch_set_string(globals.hold_music, val);
				DEBUGA_GSMOPEN("globals.hold_music=%s\n", GSMOPEN_P_LOG, globals.hold_music);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
				DEBUGA_GSMOPEN("globals.dialplan=%s\n", GSMOPEN_P_LOG, globals.dialplan);
			} else if (!strcmp(var, "destination")) {
				set_global_destination(val);
				DEBUGA_GSMOPEN("globals.destination=%s\n", GSMOPEN_P_LOG, globals.destination);
			} else if (!strcmp(var, "context")) {
				set_global_context(val);
				DEBUGA_GSMOPEN("globals.context=%s\n", GSMOPEN_P_LOG, globals.context);

			}

		}
	}

	if ((interfaces = switch_xml_child(cfg, "per_interface_settings"))) {
		int i = 0;

		for (myinterface = switch_xml_child(interfaces, "interface"); myinterface; myinterface = myinterface->next) {
			char *id = (char *) switch_xml_attr(myinterface, "id");
			char *name = (char *) switch_xml_attr(myinterface, "name");
			const char *context = globals.context;
			const char *dialplan = globals.dialplan;
			const char *destination = globals.destination;
			const char *controldevice_name = "/dev/ttyUSB3";
			const char *controldevice_audio_name = "/dev/ttyUSB2";
			char *digit_timeout = NULL;
			char *max_digits = NULL;
			char *hotline = NULL;
			char *dial_regex = NULL;
			char *hold_music = globals.hold_music;
			char *fail_dial_regex = NULL;
			const char *enable_callerid = "true";

			const char *at_dial_pre_number = "ATD";
			const char *at_dial_post_number = ";";
			const char *at_dial_expect = "OK";
			const char *at_hangup = "ATH";
			const char *at_hangup_expect = "OK";
			const char *at_answer = "ATA";
			const char *at_answer_expect = "OK";
			const char *at_send_dtmf = "AT^DTMF";
			const char *at_preinit_1 = "";
			const char *at_preinit_1_expect = "";
			const char *at_preinit_2 = "";
			const char *at_preinit_2_expect = "";
			const char *at_preinit_3 = "";
			const char *at_preinit_3_expect = "";
			const char *at_preinit_4 = "";
			const char *at_preinit_4_expect = "";
			const char *at_preinit_5 = "";
			const char *at_preinit_5_expect = "";
			const char *at_postinit_1 = "at+cmic=0,9";
			const char *at_postinit_1_expect = "OK";
			const char *at_postinit_2 = "AT+CKPD=\"EEE\"";
			const char *at_postinit_2_expect = "OK";
			const char *at_postinit_3 = "AT+CSSN=1,0";
			const char *at_postinit_3_expect = "OK";
			const char *at_postinit_4 = "at+sidet=0";
			const char *at_postinit_4_expect = "OK";
			const char *at_postinit_5 = "at+clvl=3";
			const char *at_postinit_5_expect = "OK";
			const char *at_query_battchg = "AT+CBC";
			const char *at_query_battchg_expect = "OK";
			const char *at_query_signal = "AT+CSQ";
			const char *at_query_signal_expect = "OK";
			const char *at_call_idle = "+MCST: 1";
			const char *at_call_incoming = "RING";
			const char *at_call_active = "^CONN:1,0";
			const char *at_call_failed = "+MCST: 65";
			const char *at_call_calling = "^ORIG:1,0";
			const char *at_indicator_noservice_string = "CIEV: 2;0";
			const char *at_indicator_nosignal_string = "CIEV: 5;0";
			const char *at_indicator_lowsignal_string = "CIEV: 5;1";
			const char *at_indicator_lowbattchg_string = "CIEV: 0;1";
			const char *at_indicator_nobattchg_string = "CIEV: 0;0";
			const char *at_indicator_callactive_string = "CIEV: 3;1";
			const char *at_indicator_nocallactive_string = "CIEV: 3;0";
			const char *at_indicator_nocallsetup_string = "CIEV: 6;0";
			const char *at_indicator_callsetupincoming_string = "CIEV: 6;1";
			const char *at_indicator_callsetupoutgoing_string = "CIEV: 6;2";
			const char *at_indicator_callsetupremoteringing_string = "CIEV: 6;3";
			//const char *sms_receiving_program = "/usr/local/bin/ciapalo";
			const char *at_early_audio = "0";
			const char *at_after_preinit_pause = "500000";
			const char *at_initial_pause = "500000";
			const char *at_has_clcc = "0";
			const char *at_has_ecam = "0";
			const char *gsmopen_sound_rate = "8000";
			const char *capture_boost = "0";
			const char *playback_boost = "0";
			const char *no_sound = "0";
			const char *portaudiocindex = "1";
			const char *portaudiopindex = "1";
			const char *speexecho = "1";
			const char *speexpreprocess = "1";

			uint32_t interface_id = 0;
			int controldevice_speed = 115200;	//FIXME TODO
			int controldevice_audio_speed = 115200;	//FIXME TODO
			uint32_t controldevprotocol = PROTOCOL_AT;	//FIXME TODO
			uint32_t running = 1;	//FIXME TODO
			const char *gsmopen_serial_sync_period = "300";	//FIXME TODO

			tech_pvt = NULL;

			for (param = switch_xml_child(myinterface, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "id")) {
					id = val;
				} else if (!strcasecmp(var, "name")) {
					name = val;
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else if (!strcasecmp(var, "destination")) {
					destination = val;
				} else if (!strcasecmp(var, "controldevice_name")) {
					controldevice_name = val;
				} else if (!strcasecmp(var, "controldevice_audio_name")) {
					controldevice_audio_name = val;
				} else if (!strcasecmp(var, "digit_timeout")) {
					digit_timeout = val;
				} else if (!strcasecmp(var, "max_digits")) {
					max_digits = val;
				} else if (!strcasecmp(var, "hotline")) {
					hotline = val;
				} else if (!strcasecmp(var, "dial_regex")) {
					dial_regex = val;
				} else if (!strcasecmp(var, SWITCH_HOLD_MUSIC_VARIABLE)) {
					hold_music = val;
				} else if (!strcasecmp(var, "fail_dial_regex")) {
					fail_dial_regex = val;
				} else if (!strcasecmp(var, "enable_callerid")) {
					enable_callerid = val;
				} else if (!strcasecmp(var, "at_dial_pre_number")) {
					at_dial_pre_number = val;
				} else if (!strcasecmp(var, "at_dial_post_number")) {
					at_dial_post_number = val;
				} else if (!strcasecmp(var, "at_dial_expect")) {
					at_dial_expect = val;
				} else if (!strcasecmp(var, "at_hangup")) {
					at_hangup = val;
				} else if (!strcasecmp(var, "at_hangup_expect")) {
					at_hangup_expect = val;
				} else if (!strcasecmp(var, "at_answer")) {
					at_answer = val;
				} else if (!strcasecmp(var, "at_answer_expect")) {
					at_answer_expect = val;
				} else if (!strcasecmp(var, "at_send_dtmf")) {
					at_send_dtmf = val;
				} else if (!strcasecmp(var, "at_preinit_1")) {
					at_preinit_1 = val;
				} else if (!strcasecmp(var, "at_preinit_1_expect")) {
					at_preinit_1_expect = val;
				} else if (!strcasecmp(var, "at_preinit_2")) {
					at_preinit_2 = val;
				} else if (!strcasecmp(var, "at_preinit_2_expect")) {
					at_preinit_2_expect = val;
				} else if (!strcasecmp(var, "at_preinit_3")) {
					at_preinit_3 = val;
				} else if (!strcasecmp(var, "at_preinit_3_expect")) {
					at_preinit_3_expect = val;
				} else if (!strcasecmp(var, "at_preinit_4")) {
					at_preinit_4 = val;
				} else if (!strcasecmp(var, "at_preinit_4_expect")) {
					at_preinit_4_expect = val;
				} else if (!strcasecmp(var, "at_preinit_5")) {
					at_preinit_5 = val;
				} else if (!strcasecmp(var, "at_preinit_5_expect")) {
					at_preinit_5_expect = val;
				} else if (!strcasecmp(var, "at_postinit_1")) {
					at_postinit_1 = val;
				} else if (!strcasecmp(var, "at_postinit_1_expect")) {
					at_postinit_1_expect = val;
				} else if (!strcasecmp(var, "at_postinit_2")) {
					at_postinit_2 = val;
				} else if (!strcasecmp(var, "at_postinit_2_expect")) {
					at_postinit_2_expect = val;
				} else if (!strcasecmp(var, "at_postinit_3")) {
					at_postinit_3 = val;
				} else if (!strcasecmp(var, "at_postinit_3_expect")) {
					at_postinit_3_expect = val;
				} else if (!strcasecmp(var, "at_postinit_4")) {
					at_postinit_4 = val;
				} else if (!strcasecmp(var, "at_postinit_4_expect")) {
					at_postinit_4_expect = val;
				} else if (!strcasecmp(var, "at_postinit_5")) {
					at_postinit_5 = val;
				} else if (!strcasecmp(var, "at_postinit_5_expect")) {
					at_postinit_5_expect = val;
				} else if (!strcasecmp(var, "at_query_battchg")) {
					at_query_battchg = val;
				} else if (!strcasecmp(var, "at_query_battchg_expect")) {
					at_query_battchg_expect = val;
				} else if (!strcasecmp(var, "at_query_signal")) {
					at_query_signal = val;
				} else if (!strcasecmp(var, "at_query_signal_expect")) {
					at_query_signal_expect = val;
				} else if (!strcasecmp(var, "at_call_idle")) {
					at_call_idle = val;
				} else if (!strcasecmp(var, "at_call_incoming")) {
					at_call_incoming = val;
				} else if (!strcasecmp(var, "at_call_active")) {
					at_call_active = val;
				} else if (!strcasecmp(var, "at_call_failed")) {
					at_call_failed = val;
				} else if (!strcasecmp(var, "at_call_calling")) {
					at_call_calling = val;
				} else if (!strcasecmp(var, "at_indicator_noservice_string")) {
					at_indicator_noservice_string = val;
				} else if (!strcasecmp(var, "at_indicator_nosignal_string")) {
					at_indicator_nosignal_string = val;
				} else if (!strcasecmp(var, "at_indicator_lowsignal_string")) {
					at_indicator_lowsignal_string = val;
				} else if (!strcasecmp(var, "at_indicator_lowbattchg_string")) {
					at_indicator_lowbattchg_string = val;
				} else if (!strcasecmp(var, "at_indicator_nobattchg_string")) {
					at_indicator_nobattchg_string = val;
				} else if (!strcasecmp(var, "at_indicator_callactive_string")) {
					at_indicator_callactive_string = val;
				} else if (!strcasecmp(var, "at_indicator_nocallactive_string")) {
					at_indicator_nocallactive_string = val;
				} else if (!strcasecmp(var, "at_indicator_nocallsetup_string")) {
					at_indicator_nocallsetup_string = val;
				} else if (!strcasecmp(var, "at_indicator_callsetupincoming_string")) {
					at_indicator_callsetupincoming_string = val;
				} else if (!strcasecmp(var, "at_indicator_callsetupoutgoing_string")) {
					at_indicator_callsetupoutgoing_string = val;
				} else if (!strcasecmp(var, "at_indicator_callsetupremoteringing_string")) {
					at_indicator_callsetupremoteringing_string = val;
					//} else if (!strcasecmp(var, "sms_receiving_program")) {
					//sms_receiving_program = val;
				} else if (!strcasecmp(var, "portaudiocindex")) {
					portaudiocindex = val;
				} else if (!strcasecmp(var, "portaudiopindex")) {
					portaudiopindex = val;
				} else if (!strcasecmp(var, "speexecho")) {
					speexecho = val;
				} else if (!strcasecmp(var, "speexpreprocess")) {
					speexpreprocess = val;
				} else if (!strcasecmp(var, "at_early_audio")) {
					at_early_audio = val;
				} else if (!strcasecmp(var, "at_after_preinit_pause")) {
					at_after_preinit_pause = val;
				} else if (!strcasecmp(var, "at_initial_pause")) {
					at_initial_pause = val;
				} else if (!strcasecmp(var, "at_has_clcc")) {
					at_has_clcc = val;
				} else if (!strcasecmp(var, "at_has_ecam")) {
					at_has_ecam = val;
				} else if (!strcasecmp(var, "gsmopen_sound_rate")) {
					gsmopen_sound_rate = val;
				} else if (!strcasecmp(var, "capture_boost")) {
					capture_boost = val;
				} else if (!strcasecmp(var, "playback_boost")) {
					playback_boost = val;
				} else if (!strcasecmp(var, "no_sound")) {
					no_sound = val;
				} else if (!strcasecmp(var, "gsmopen_serial_sync_period")) {
					gsmopen_serial_sync_period = val;
				}

			}

			/* BEGIN: Changes here */
			if (reload_type == SOFT_RELOAD) {
				char the_interface[256];
				sprintf(the_interface, "#%s", name);

				if (interface_exists(the_interface) == SWITCH_STATUS_SUCCESS) {
					continue;
				}
			}
			/* END: Changes here */

			if (!id) {
				ERRORA("interface missing REQUIRED param 'id'\n", GSMOPEN_P_LOG);
				continue;
			}

			if (switch_is_number(id)) {
				interface_id = atoi(id);
			} else {
				ERRORA("interface param 'id' MUST be a number, now id='%s'\n", GSMOPEN_P_LOG, id);
				continue;
			}

			if (!switch_is_number(at_early_audio)) {
				ERRORA("interface param 'at_early_audio' MUST be a number, now at_early_audio='%s'\n", GSMOPEN_P_LOG, at_early_audio);
				continue;
			}
			if (!switch_is_number(at_after_preinit_pause)) {
				ERRORA("interface param 'at_after_preinit_pause' MUST be a number, now at_after_preinit_pause='%s'\n", GSMOPEN_P_LOG,
					   at_after_preinit_pause);
				continue;
			}
			if (!switch_is_number(at_initial_pause)) {
				ERRORA("interface param 'at_initial_pause' MUST be a number, now at_initial_pause='%s'\n", GSMOPEN_P_LOG, at_initial_pause);
				continue;
			}
			if (!switch_is_number(at_has_clcc)) {
				ERRORA("interface param 'at_has_clcc' MUST be a number, now at_has_clcc='%s'\n", GSMOPEN_P_LOG, at_has_clcc);
				continue;
			}
			if (!switch_is_number(at_has_ecam)) {
				ERRORA("interface param 'at_has_ecam' MUST be a number, now at_has_ecam='%s'\n", GSMOPEN_P_LOG, at_has_ecam);
				continue;
			}
			if (!switch_is_number(gsmopen_sound_rate)) {
				ERRORA("interface param 'gsmopen_sound_rate' MUST be a number, now gsmopen_sound_rate='%s'\n", GSMOPEN_P_LOG, gsmopen_sound_rate);
				continue;
			}
			if (!switch_is_number(capture_boost)) {
				ERRORA("interface param 'capture_boost' MUST be a number, now capture_boost='%s'\n", GSMOPEN_P_LOG, capture_boost);
				continue;
			}
			if (!switch_is_number(playback_boost)) {
				ERRORA("interface param 'playback_boost' MUST be a number, now playback_boost='%s'\n", GSMOPEN_P_LOG, playback_boost);
				continue;
			}
			if (!switch_is_number(no_sound)) {
				ERRORA("interface param 'no_sound' MUST be a number, now no_sound='%s'\n", GSMOPEN_P_LOG, no_sound);
				continue;
			}
			if (!switch_is_number(gsmopen_serial_sync_period)) {
				ERRORA("interface param 'gsmopen_serial_sync_period' MUST be a number, now gsmopen_serial_sync_period='%s'\n", GSMOPEN_P_LOG,
					   gsmopen_serial_sync_period);
				continue;
			}

			if (interface_id && interface_id < GSMOPEN_MAX_INTERFACES) {
				private_t newconf;
				switch_threadattr_t *gsmopen_api_thread_attr = NULL;
				int res = 0;

				memset(&newconf, '\0', sizeof(newconf));
				globals.GSMOPEN_INTERFACES[interface_id] = newconf;

				tech_pvt = &globals.GSMOPEN_INTERFACES[interface_id];

				switch_mutex_init(&globals.GSMOPEN_INTERFACES[interface_id].controldev_lock, SWITCH_MUTEX_NESTED, gsmopen_module_pool);
				switch_mutex_init(&globals.GSMOPEN_INTERFACES[interface_id].controldev_audio_lock, SWITCH_MUTEX_NESTED, gsmopen_module_pool);

				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].id, id);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].name, name);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].context, context);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].dialplan, dialplan);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].destination, destination);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].controldevice_name, controldevice_name);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].controldevice_audio_name, controldevice_audio_name);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].dial_regex, dial_regex);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].hold_music, hold_music);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].fail_dial_regex, fail_dial_regex);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_dial_pre_number, at_dial_pre_number);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_dial_post_number, at_dial_post_number);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_dial_expect, at_dial_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_hangup, at_hangup);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_hangup_expect, at_hangup_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_answer, at_answer);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_answer_expect, at_answer_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_send_dtmf, at_send_dtmf);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_1, at_preinit_1);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_1_expect, at_preinit_1_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_2, at_preinit_2);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_2_expect, at_preinit_2_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_3, at_preinit_3);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_3_expect, at_preinit_3_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_4, at_preinit_4);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_4_expect, at_preinit_4_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_5, at_preinit_5);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_preinit_5_expect, at_preinit_5_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_1, at_postinit_1);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_1_expect, at_postinit_1_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_2, at_postinit_2);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_2_expect, at_postinit_2_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_3, at_postinit_3);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_3_expect, at_postinit_3_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_4, at_postinit_4);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_4_expect, at_postinit_4_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_5, at_postinit_5);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_postinit_5_expect, at_postinit_5_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_query_battchg, at_query_battchg);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_query_battchg_expect, at_query_battchg_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_query_signal, at_query_signal);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_query_signal_expect, at_query_signal_expect);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_call_idle, at_call_idle);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_call_incoming, at_call_incoming);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_call_active, at_call_active);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_call_failed, at_call_failed);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_call_calling, at_call_calling);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_noservice_string, at_indicator_noservice_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_nosignal_string, at_indicator_nosignal_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_lowsignal_string, at_indicator_lowsignal_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_lowbattchg_string, at_indicator_lowbattchg_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_nobattchg_string, at_indicator_nobattchg_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_callactive_string, at_indicator_callactive_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_nocallactive_string, at_indicator_nocallactive_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_nocallsetup_string, at_indicator_nocallsetup_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_callsetupincoming_string, at_indicator_callsetupincoming_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_callsetupoutgoing_string, at_indicator_callsetupoutgoing_string);
				switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].at_indicator_callsetupremoteringing_string,
								  at_indicator_callsetupremoteringing_string);
				//switch_set_string(globals.GSMOPEN_INTERFACES[interface_id].sms_receiving_program, sms_receiving_program);
				globals.GSMOPEN_INTERFACES[interface_id].at_early_audio = atoi(at_early_audio);
				globals.GSMOPEN_INTERFACES[interface_id].at_after_preinit_pause = atoi(at_after_preinit_pause);
				globals.GSMOPEN_INTERFACES[interface_id].at_initial_pause = atoi(at_initial_pause);
				globals.GSMOPEN_INTERFACES[interface_id].at_has_clcc = atoi(at_has_clcc);
				globals.GSMOPEN_INTERFACES[interface_id].at_has_ecam = atoi(at_has_ecam);
				globals.GSMOPEN_INTERFACES[interface_id].capture_boost = atoi(capture_boost);
				globals.GSMOPEN_INTERFACES[interface_id].playback_boost = atoi(playback_boost);
				globals.GSMOPEN_INTERFACES[interface_id].no_sound = atoi(no_sound);
				globals.GSMOPEN_INTERFACES[interface_id].gsmopen_serial_sync_period = atoi(gsmopen_serial_sync_period);

				globals.GSMOPEN_INTERFACES[interface_id].controldevice_speed = controldevice_speed;	//FIXME
				globals.GSMOPEN_INTERFACES[interface_id].controldevprotocol = controldevprotocol;	//FIXME
				globals.GSMOPEN_INTERFACES[interface_id].running = running;	//FIXME

				WARNINGA("STARTING interface_id=%d\n", GSMOPEN_P_LOG, interface_id);
				DEBUGA_GSMOPEN("id=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].id);
				DEBUGA_GSMOPEN("name=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].name);
				DEBUGA_GSMOPEN("hold-music=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].hold_music);
				DEBUGA_GSMOPEN("context=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].context);
				DEBUGA_GSMOPEN("dialplan=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].dialplan);
				DEBUGA_GSMOPEN("destination=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].destination);
				DEBUGA_GSMOPEN("controldevice_name=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].controldevice_name);
				DEBUGA_GSMOPEN("controldevice_audio_name=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[interface_id].controldevice_audio_name);
				DEBUGA_GSMOPEN("gsmopen_serial_sync_period=%d\n", GSMOPEN_P_LOG,
							   (int) globals.GSMOPEN_INTERFACES[interface_id].gsmopen_serial_sync_period);

				/* init the serial port */
				if (globals.GSMOPEN_INTERFACES[interface_id].controldevprotocol != PROTOCOL_NO_SERIAL) {
					globals.GSMOPEN_INTERFACES[interface_id].controldevfd =
						gsmopen_serial_init(&globals.GSMOPEN_INTERFACES[interface_id], globals.GSMOPEN_INTERFACES[interface_id].controldevice_speed);
					if (globals.GSMOPEN_INTERFACES[interface_id].controldevfd == -1) {
						ERRORA("gsmopen_serial_init failed\n", GSMOPEN_P_LOG);
						ERRORA("STARTING interface_id=%d FAILED\n", GSMOPEN_P_LOG, interface_id);
						//return SWITCH_STATUS_FALSE;
						globals.GSMOPEN_INTERFACES[interface_id].running = 0;
						alarm_event(&globals.GSMOPEN_INTERFACES[interface_id], ALARM_FAILED_INTERFACE, "gsmopen_serial_init failed");
						globals.GSMOPEN_INTERFACES[interface_id].active = 0;
						globals.GSMOPEN_INTERFACES[interface_id].name[0] = '\0';
						continue;
					}
				}

				/* config the phone/modem on the serial port */
				if (globals.GSMOPEN_INTERFACES[interface_id].controldevprotocol != PROTOCOL_NO_SERIAL) {
					res = gsmopen_serial_config(&globals.GSMOPEN_INTERFACES[interface_id]);
					if (res) {
						int count = 0;
						ERRORA("gsmopen_serial_config failed, let's try again\n", GSMOPEN_P_LOG);
						while (res && count < 5) {
							switch_sleep(100000);	//0.1 seconds
							res = gsmopen_serial_config(&globals.GSMOPEN_INTERFACES[interface_id]);
							count++;
							if (res) {
								ERRORA("%d: gsmopen_serial_config failed, let's try again\n", GSMOPEN_P_LOG, count);
							}
						}
						if (res) {
							ERRORA("STARTING interface_id=%d FAILED\n", GSMOPEN_P_LOG, interface_id);
							//return SWITCH_STATUS_FALSE;
							globals.GSMOPEN_INTERFACES[interface_id].running = 0;
							alarm_event(&globals.GSMOPEN_INTERFACES[interface_id], ALARM_FAILED_INTERFACE, "gsmopen_serial_config failed");
							globals.GSMOPEN_INTERFACES[interface_id].active = 0;
							globals.GSMOPEN_INTERFACES[interface_id].name[0] = '\0';
							continue;
						}
					}
				}

				if (globals.GSMOPEN_INTERFACES[interface_id].no_sound == 0) {
					if (serial_audio_init(&globals.GSMOPEN_INTERFACES[interface_id])) {
						ERRORA("serial_audio_init failed\n", GSMOPEN_P_LOG);
						ERRORA("STARTING interface_id=%d FAILED\n", GSMOPEN_P_LOG, interface_id);
						//return SWITCH_STATUS_FALSE;
						globals.GSMOPEN_INTERFACES[interface_id].running = 0;
						alarm_event(&globals.GSMOPEN_INTERFACES[interface_id], ALARM_FAILED_INTERFACE, "serial_audio_init failed");
						globals.GSMOPEN_INTERFACES[interface_id].active = 0;
						globals.GSMOPEN_INTERFACES[interface_id].name[0] = '\0';
						continue;

					}

					if (serial_audio_shutdown(&globals.GSMOPEN_INTERFACES[interface_id])) {
						ERRORA("serial_audio_shutdown failed\n", GSMOPEN_P_LOG);
						ERRORA("STARTING interface_id=%d FAILED\n", GSMOPEN_P_LOG, interface_id);
						//return SWITCH_STATUS_FALSE;
						globals.GSMOPEN_INTERFACES[interface_id].running = 0;
						alarm_event(&globals.GSMOPEN_INTERFACES[interface_id], ALARM_FAILED_INTERFACE, "serial_audio_shutdown failed");
						globals.GSMOPEN_INTERFACES[interface_id].active = 0;
						globals.GSMOPEN_INTERFACES[interface_id].name[0] = '\0';
						continue;

					}

				}

				globals.GSMOPEN_INTERFACES[interface_id].active = 1;

				//gsmopen_store_boost((char *)"5", &globals.GSMOPEN_INTERFACES[interface_id].capture_boost);    //FIXME
				//gsmopen_store_boost((char *)"10", &globals.GSMOPEN_INTERFACES[interface_id].playback_boost);  //FIXME
				gsmopen_store_boost((char *) capture_boost, &globals.GSMOPEN_INTERFACES[interface_id].capture_boost);	//FIXME
				gsmopen_store_boost((char *) playback_boost, &globals.GSMOPEN_INTERFACES[interface_id].playback_boost);	//FIXME

				switch_sleep(100000);
				switch_threadattr_create(&gsmopen_api_thread_attr, gsmopen_module_pool);
				switch_threadattr_stacksize_set(gsmopen_api_thread_attr, SWITCH_THREAD_STACKSIZE);
				switch_thread_create(&globals.GSMOPEN_INTERFACES[interface_id].gsmopen_api_thread, gsmopen_api_thread_attr, gsmopen_do_gsmopenapi_thread,
									 &globals.GSMOPEN_INTERFACES[interface_id], gsmopen_module_pool);

				switch_sleep(100000);
				WARNINGA("STARTED interface_id=%d\n", GSMOPEN_P_LOG, interface_id);

			} else {
				ERRORA("interface id %d is higher than GSMOPEN_MAX_INTERFACES (%d)\n", GSMOPEN_P_LOG, interface_id, GSMOPEN_MAX_INTERFACES);
				alarm_event(&globals.GSMOPEN_INTERFACES[interface_id], ALARM_FAILED_INTERFACE, "interface id is higher than GSMOPEN_MAX_INTERFACES");
				continue;
			}

		}

		for (i = 0; i < GSMOPEN_MAX_INTERFACES; i++) {
			if (strlen(globals.GSMOPEN_INTERFACES[i].name)) {
				/* How many real intterfaces */
				globals.real_interfaces = i + 1;

				tech_pvt = &globals.GSMOPEN_INTERFACES[i];

				DEBUGA_GSMOPEN("id=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].id);
				DEBUGA_GSMOPEN("name=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].name);
				DEBUGA_GSMOPEN("context=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].context);
				DEBUGA_GSMOPEN("hold-music=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].hold_music);
				DEBUGA_GSMOPEN("dialplan=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].dialplan);
				DEBUGA_GSMOPEN("destination=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].destination);
				DEBUGA_GSMOPEN("controldevice_name=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].controldevice_name);
				DEBUGA_GSMOPEN("gsmopen_serial_sync_period=%d\n", GSMOPEN_P_LOG, (int) globals.GSMOPEN_INTERFACES[i].gsmopen_serial_sync_period);
				DEBUGA_GSMOPEN("controldevice_audio_name=%s\n", GSMOPEN_P_LOG, globals.GSMOPEN_INTERFACES[i].controldevice_audio_name);
			}
		}
	}

	switch_mutex_unlock(globals.mutex);
	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

//static switch_status_t chat_send(const char *proto, const char *from, const char *to, const char *subject, const char *body, const char *type, const char *hint)
static switch_status_t chat_send(switch_event_t *message_event)
{
	char *user, *host, *f_user = NULL, *f_host = NULL, *f_resource = NULL;
	private_t *tech_pvt = NULL;
	int i = 0, found = 0;

	const char *proto;
	const char *from;
	const char *to;
	const char *subject;
	const char *body;
	//const char *type;
	const char *hint;

	proto = switch_event_get_header(message_event, "proto");
	from = switch_event_get_header(message_event, "from");
	to = switch_event_get_header(message_event, "to");
	subject = switch_event_get_header(message_event, "subject");
	body = switch_event_get_body(message_event);
	//type = switch_event_get_header(message_event, "type");
	hint = switch_event_get_header(message_event, "hint");

	switch_assert(proto != NULL);

	DEBUGA_GSMOPEN("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, hint=%s)\n", GSMOPEN_P_LOG, proto, from, to, subject, body,
				   hint ? hint : "NULL");

	if (!to || !strlen(to)) {
		ERRORA("Missing To: header.\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((!from && !hint) || (!strlen(from) && !strlen(hint))) {
		ERRORA("Missing From: AND Hint: headers.\n", GSMOPEN_P_LOG);
		return SWITCH_STATUS_SUCCESS;
	}

	if (from && (f_user = strdup(from))) {
		if ((f_host = strchr(f_user, '@'))) {
			*f_host++ = '\0';
			if ((f_resource = strchr(f_host, '/'))) {
				*f_resource++ = '\0';
			}
		}
	}

	if (hint == NULL || !strlen(hint)) {	//FIXME FIXME FIXME
		hint = from;
	}
	if (subject == NULL || !strlen(subject)) {	//FIXME FIXME FIXME
		subject = "SIMPLE MESSAGE";
	}
	if (to && (user = strdup(to))) {
		if ((host = strchr(user, '@'))) {
			*host++ = '\0';
		}

		DEBUGA_GSMOPEN("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, hint=%s)\n", GSMOPEN_P_LOG, proto, from, to, subject, body,
					   hint ? hint : "NULL");
		if (hint && strlen(hint)) {
			//in hint we receive the interface name to use
			for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
				if (strlen(globals.GSMOPEN_INTERFACES[i].name)
					&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, hint, strlen(hint)) == 0)) {
					tech_pvt = &globals.GSMOPEN_INTERFACES[i];
					DEBUGA_GSMOPEN("Using interface: globals.GSMOPEN_INTERFACES[%d].name=|||%s|||\n", GSMOPEN_P_LOG, i,
								   globals.GSMOPEN_INTERFACES[i].name);
					found = 1;
					break;
				}
			}
		}						/* FIXME add a tech_pvt member for the SIM telephone number //else {
								   //we have no a predefined interface name to use (hint is NULL), so let's choose an interface from the username (from)
								   for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
								   if (strlen(globals.GSMOPEN_INTERFACES[i].name)
								   && (strncmp(globals.GSMOPEN_INTERFACES[i].skype_user, from, strlen(from)) == 0)) {
								   tech_pvt = &globals.GSMOPEN_INTERFACES[i];
								   DEBUGA_GSMOPEN("Using interface: globals.GSMOPEN_INTERFACES[%d].name=|||%s|||\n", GSMOPEN_P_LOG, i, globals.GSMOPEN_INTERFACES[i].name);
								   found = 1;
								   break;
								   }
								   }
								   }
								 */
		if (!found) {
			ERRORA("ERROR: A GSMopen interface with name='%s' or one with SIM_number='%s' was not found\n", GSMOPEN_P_LOG, hint ? hint : "NULL",
				   from ? from : "NULL");
			goto end;
		} else {
			gsmopen_sendsms(tech_pvt, (char *) to, (char *) body);
		}
	}
  end:
	switch_safe_free(user);
	switch_safe_free(f_user);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t compat_chat_send(const char *proto, const char *from, const char *to,
										const char *subject, const char *body, const char *type, const char *hint)
{
	switch_event_t *message_event;
	switch_status_t status;

	if (switch_event_create(&message_event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "proto", proto);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "from", from);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "to", to);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "subject", subject);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "type", type);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "hint", hint);

		if (body) {
			switch_event_add_body(message_event, "%s", body);
		}
	} else {
		abort();
	}

	status = chat_send(message_event);
	switch_event_destroy(&message_event);

	return status;

}

SWITCH_MODULE_LOAD_FUNCTION(mod_gsmopen_load)
{
	switch_api_interface_t *commands_api_interface;
	switch_chat_interface_t *chat_interface;

	gsmopen_module_pool = pool;
	memset(&globals, '\0', sizeof(globals));

	running = 1;

	if (load_config(FULL_RELOAD) != SWITCH_STATUS_SUCCESS) {
		running = 0;
		return SWITCH_STATUS_FALSE;
	}

	if (switch_event_reserve_subclass(MY_EVENT_INCOMING_SMS) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	gsmopen_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	gsmopen_endpoint_interface->interface_name = "gsmopen";
	gsmopen_endpoint_interface->io_routines = &gsmopen_io_routines;
	gsmopen_endpoint_interface->state_handler = &gsmopen_state_handlers;

	if (running) {

#if 1
		SWITCH_ADD_API(commands_api_interface, "gsm", "gsm console AT_command", gsm_function, GSM_SYNTAX);
		SWITCH_ADD_API(commands_api_interface, "gsmopen", "gsmopen interface AT_command", gsmopen_function, GSMOPEN_SYNTAX);
#endif //0
		SWITCH_ADD_API(commands_api_interface, "gsmopen_boost_audio", "gsmopen_boost_audio interface AT_command", gsmopen_boost_audio_function,
					   GSMOPEN_BOOST_AUDIO_SYNTAX);
		SWITCH_ADD_API(commands_api_interface, "gsmopen_dump", "gsmopen_dump interface", gsmopen_dump_function, GSMOPEN_DUMP_SYNTAX);
		SWITCH_ADD_API(commands_api_interface, "gsmopen_sendsms", "gsmopen_sendsms interface destination_number SMS_text", sendsms_function,
					   SENDSMS_SYNTAX);
		SWITCH_ADD_CHAT(chat_interface, GSMOPEN_CHAT_PROTO, chat_send);

		/* indicate that the module should continue to be loaded */
		return SWITCH_STATUS_SUCCESS;
	} else
		return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_gsmopen_shutdown)
{
	int x;
	private_t *tech_pvt = NULL;
	switch_status_t status;
#ifdef WIN32
	switch_size_t howmany = 8;
#else
	unsigned int howmany = 8;
#endif
	int interface_id;
	int fd;

	running = 0;

	for (interface_id = 0; interface_id < GSMOPEN_MAX_INTERFACES; interface_id++) {
		tech_pvt = &globals.GSMOPEN_INTERFACES[interface_id];

		if (strlen(globals.GSMOPEN_INTERFACES[interface_id].name)) {
			WARNINGA("SHUTDOWN interface_id=%d\n", GSMOPEN_P_LOG, interface_id);
			globals.GSMOPEN_INTERFACES[interface_id].running = 0;
			if (globals.GSMOPEN_INTERFACES[interface_id].gsmopen_signaling_thread) {
#ifdef WIN32
				switch_file_write(tech_pvt->GSMopenHandles.fdesc[1], "sciutati", &howmany);	// let's the controldev_thread die
#else /* WIN32 */
				howmany = write(tech_pvt->GSMopenHandles.fdesc[1], "sciutati", howmany);
#endif /* WIN32 */
			}
			x = 10;
			while (x) {			//FIXME 0.5 seconds?
				x--;
				switch_yield(50000);
			}
			if (globals.GSMOPEN_INTERFACES[interface_id].gsmopen_signaling_thread) {
				switch_thread_join(&status, globals.GSMOPEN_INTERFACES[interface_id].gsmopen_signaling_thread);
			}
			if (globals.GSMOPEN_INTERFACES[interface_id].gsmopen_api_thread) {
				switch_thread_join(&status, globals.GSMOPEN_INTERFACES[interface_id].gsmopen_api_thread);
			}

			x = 10;
			while (x) {			//FIXME 0.5 seconds?
				x--;
				switch_yield(50000);
			}
			fd = tech_pvt->controldevfd;
			//DEBUGA_GSMOPEN("SHUTDOWN tech_pvt->controldevfd=%d\n", GSMOPEN_P_LOG, tech_pvt->controldevfd);
			if (fd) {
				//close(fd);
				tech_pvt->controldevfd = -1;
				DEBUGA_GSMOPEN("SHUTDOWN tech_pvt->controldevfd=%d\n", GSMOPEN_P_LOG, tech_pvt->controldevfd);
			}

			serial_audio_shutdown(tech_pvt);

			int res;
			res = tech_pvt->serialPort_serial_control->Close();
			DEBUGA_GSMOPEN("serial_shutdown res=%d (controldevfd is %d)\n", GSMOPEN_P_LOG, res, tech_pvt->controldevfd);

#ifndef WIN32
			shutdown(tech_pvt->audiogsmopenpipe[0], 2);
			close(tech_pvt->audiogsmopenpipe[0]);
			shutdown(tech_pvt->audiogsmopenpipe[1], 2);
			close(tech_pvt->audiogsmopenpipe[1]);
			shutdown(tech_pvt->audiopipe[0], 2);
			close(tech_pvt->audiopipe[0]);
			shutdown(tech_pvt->audiopipe[1], 2);
			close(tech_pvt->audiopipe[1]);
			shutdown(tech_pvt->GSMopenHandles.fdesc[0], 2);
			close(tech_pvt->GSMopenHandles.fdesc[0]);
			shutdown(tech_pvt->GSMopenHandles.fdesc[1], 2);
			close(tech_pvt->GSMopenHandles.fdesc[1]);
#endif /* WIN32 */
		}

	}

	switch_event_free_subclass(MY_EVENT_INCOMING_SMS);

	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.context);
	switch_safe_free(globals.destination);
	switch_safe_free(globals.codec_string);
	switch_safe_free(globals.codec_rates_string);

	return SWITCH_STATUS_SUCCESS;
}

void *SWITCH_THREAD_FUNC gsmopen_do_gsmopenapi_thread(switch_thread_t *thread, void *obj)
{
	return gsmopen_do_gsmopenapi_thread_func(obj);
}

int dtmf_received(private_t *tech_pvt, char *value)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	channel = switch_core_session_get_channel(session);

	if (channel) {

		if (!switch_channel_test_flag(channel, CF_BRIDGED)) {

			switch_dtmf_t dtmf = { (char) value[0], switch_core_default_dtmf_duration(0) };
			DEBUGA_GSMOPEN("received DTMF %c on channel %s\n", GSMOPEN_P_LOG, dtmf.digit, switch_channel_get_name(channel));
			switch_mutex_lock(tech_pvt->flag_mutex);
			//FIXME: why sometimes DTMFs from here do not seems to be get by FS?
			switch_channel_queue_dtmf(channel, &dtmf);
			switch_set_flag(tech_pvt, TFLAG_DTMF);
			switch_mutex_unlock(tech_pvt->flag_mutex);
		} else {
			DEBUGA_GSMOPEN
				("received a DTMF on channel %s, but we're BRIDGED, so let's NOT relay it out of band\n", GSMOPEN_P_LOG, switch_channel_get_name(channel));
		}
	} else {
		WARNINGA("received %c DTMF, but no channel?\n", GSMOPEN_P_LOG, value[0]);
	}
	switch_core_session_rwunlock(session);

	return 0;
}

int new_inbound_channel(private_t *tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	switch_assert(tech_pvt != NULL);
	tech_pvt->ib_calls++;
	if ((session = switch_core_session_request(gsmopen_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL)) != 0) {
		DEBUGA_GSMOPEN("2 SESSION_REQUEST %s\n", GSMOPEN_P_LOG, switch_core_session_get_uuid(session));
		switch_core_session_add_stream(session, NULL);
		channel = switch_core_session_get_channel(session);
		if (!channel) {
			ERRORA("Doh! no channel?\n", GSMOPEN_P_LOG);
			switch_core_session_destroy(&session);
			return 0;
		}
		if (gsmopen_tech_init(tech_pvt, session) != SWITCH_STATUS_SUCCESS) {
			ERRORA("Doh! no tech_init?\n", GSMOPEN_P_LOG);
			switch_core_session_destroy(&session);
			return 0;
		}

		if ((tech_pvt->caller_profile =
			 switch_caller_profile_new(switch_core_session_get_pool(session), "gsmopen",
									   tech_pvt->dialplan, tech_pvt->callid_name,
									   tech_pvt->callid_number, NULL, NULL, NULL, NULL, "mod_gsmopen", tech_pvt->context, tech_pvt->destination)) != 0) {
			char name[128];
			//switch_snprintf(name, sizeof(name), "gsmopen/%s/%s", tech_pvt->name, tech_pvt->caller_profile->destination_number);
			switch_snprintf(name, sizeof(name), "gsmopen/%s", tech_pvt->name);
			switch_channel_set_name(channel, name);
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		switch_channel_set_state(channel, CS_INIT);
		if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
			ERRORA("Error spawning thread\n", GSMOPEN_P_LOG);
			switch_core_session_destroy(&session);
			return 0;
		}
	}

	DEBUGA_GSMOPEN("EXITING new_inbound_channel\n", GSMOPEN_P_LOG);

	return 0;
}

int remote_party_is_ringing(private_t *tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	} else {
		ERRORA("No session???\n", GSMOPEN_P_LOG);
		goto done;
	}
	if (session) {
		channel = switch_core_session_get_channel(session);
	} else {
		ERRORA("No session???\n", GSMOPEN_P_LOG);
		goto done;
	}
	if (channel) {
		switch_channel_mark_ring_ready(channel);
		DEBUGA_GSMOPEN("gsmopen_call: REMOTE PARTY RINGING\n", GSMOPEN_P_LOG);
	} else {
		ERRORA("No channel???\n", GSMOPEN_P_LOG);
	}

	switch_core_session_rwunlock(session);

  done:
	return 0;
}

int remote_party_is_early_media(private_t *tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	} else {
		ERRORA("No session???\n\n\n", GSMOPEN_P_LOG);
		//TODO: kill the bastard
		goto done;
	}
	if (session) {
		channel = switch_core_session_get_channel(session);
		switch_core_session_add_stream(session, NULL);
	} else {
		ERRORA("No session???\n", GSMOPEN_P_LOG);
		//TODO: kill the bastard
		goto done;
	}
	if (channel) {
		switch_channel_mark_pre_answered(channel);
		DEBUGA_GSMOPEN("gsmopen_call: REMOTE PARTY EARLY MEDIA\n", GSMOPEN_P_LOG);
	} else {
		ERRORA("No channel???\n", GSMOPEN_P_LOG);
		//TODO: kill the bastard
	}

	switch_core_session_rwunlock(session);

  done:
	return 0;
}

int outbound_channel_answered(private_t *tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	} else {
		ERRORA("No session???\n", GSMOPEN_P_LOG);
		goto done;
	}
	if (session) {
		channel = switch_core_session_get_channel(session);
	} else {
		ERRORA("No channel???\n", GSMOPEN_P_LOG);
		goto done;
	}
	if (channel) {
		switch_channel_mark_answered(channel);
		tech_pvt->phone_callflow = GSMOPEN_STATE_UP;
		tech_pvt->interface_state = GSMOPEN_STATE_UP;
		//DEBUGA_GSMOPEN("gsmopen_call: %s, answered\n", GSMOPEN_P_LOG, id);
	} else {
		ERRORA("No channel???\n", GSMOPEN_P_LOG);
	}

	switch_core_session_rwunlock(session);

  done:
	DEBUGA_GSMOPEN("outbound_channel_answered!\n", GSMOPEN_P_LOG);

	return 0;
}

private_t *find_available_gsmopen_interface_rr(private_t *tech_pvt_calling)
{
	private_t *tech_pvt = NULL;
	int i;
	//int num_interfaces = GSMOPEN_MAX_INTERFACES; 
	//int num_interfaces = globals.real_interfaces;

	switch_mutex_lock(globals.mutex);

	/* Fact is the real interface start from 1 */
	//XXX no, is just a convention, but you can have it start from 0. I do not, for aestetic reasons :-)  
	//if (globals.next_interface == 0) globals.next_interface = 1;

	for (i = 0; i < GSMOPEN_MAX_INTERFACES; i++) {
		int interface_id;

		interface_id = globals.next_interface;
		//interface_id = interface_id < GSMOPEN_MAX_INTERFACES ? interface_id : interface_id - GSMOPEN_MAX_INTERFACES + 1;
		globals.next_interface = interface_id + 1 < GSMOPEN_MAX_INTERFACES ? interface_id + 1 : 0;

		if (strlen(globals.GSMOPEN_INTERFACES[interface_id].name)) {
			int gsmopen_state = 0;

			tech_pvt = &globals.GSMOPEN_INTERFACES[interface_id];
			gsmopen_state = tech_pvt->interface_state;
			DEBUGA_GSMOPEN("gsmopen interface: %d, name: %s, state: %d\n", GSMOPEN_P_LOG, interface_id, globals.GSMOPEN_INTERFACES[interface_id].name,
						   gsmopen_state);
			if ((tech_pvt_calling ? strcmp(tech_pvt->gsmopen_user, tech_pvt_calling->gsmopen_user) : 1)
				&& (GSMOPEN_STATE_DOWN == gsmopen_state || 0 == gsmopen_state) && (tech_pvt->phone_callflow == CALLFLOW_STATUS_FINISHED
																				   || 0 == tech_pvt->phone_callflow)) {
				DEBUGA_GSMOPEN("returning as available gsmopen interface name: %s, state: %d callflow: %d\n", GSMOPEN_P_LOG, tech_pvt->name, gsmopen_state,
							   tech_pvt->phone_callflow);
				/*set to Dialing state to avoid other thread fint it, don't know if it is safe */
				//XXX no, it's not safe 
				if (tech_pvt_calling == NULL) {
					tech_pvt->interface_state = GSMOPEN_STATE_SELECTED;
				}

				switch_mutex_unlock(globals.mutex);
				return tech_pvt;
			}
		}						// else {
		//DEBUGA_GSMOPEN("GSM interface: %d blank!! A hole here means we cannot hunt the last interface.\n", GSMOPEN_P_LOG, interface_id);
		//}
	}

	switch_mutex_unlock(globals.mutex);
	return NULL;
}

#if 1
SWITCH_STANDARD_API(gsm_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (globals.gsm_console)
		stream->write_function(stream, "gsm console is: |||%s|||\n", globals.gsm_console->name);
	else
		stream->write_function(stream, "gsm console is NOT yet assigned\n");

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc || !argv[0]) {
		stream->write_function(stream, "%s", GSM_SYNTAX);
		goto end;
	}

	if (!strcasecmp(argv[0], "list")) {
		int i;
		unsigned int ib = 0;
		unsigned int ib_failed = 0;
		unsigned int ob = 0;
		unsigned int ob_failed = 0;
		char next_flag_char = ' ';

		stream->write_function(stream, "F ID\t    Name    \tIB (F/T)    OB (F/T)\tState\tCallFlw\t\tUUID\n");
		stream->write_function(stream, "= ====\t  ========  \t=======     =======\t======\t============\t======\n");

		for (i = 0; i < GSMOPEN_MAX_INTERFACES; i++) {

			if (strlen(globals.GSMOPEN_INTERFACES[i].name)) {
				next_flag_char = i == globals.next_interface ? '*' : ' ';
				ib += globals.GSMOPEN_INTERFACES[i].ib_calls;
				ib_failed += globals.GSMOPEN_INTERFACES[i].ib_failed_calls;
				ob += globals.GSMOPEN_INTERFACES[i].ob_calls;
				ob_failed += globals.GSMOPEN_INTERFACES[i].ob_failed_calls;


				stream->write_function(stream,
						//"%c %d\t[%s]\t%3ld/%ld\t%6ld/%ld\t%s\t%s\t%s\n",
						"%c %d\t[%6s]\t%3u/%u\t%6u/%u\t%s\t%s\t%s\n",
						next_flag_char,
						i, globals.GSMOPEN_INTERFACES[i].name,
						globals.GSMOPEN_INTERFACES[i].ib_failed_calls,
						globals.GSMOPEN_INTERFACES[i].ib_calls,
						globals.GSMOPEN_INTERFACES[i].ob_failed_calls,
						globals.GSMOPEN_INTERFACES[i].ob_calls,
						interface_status[globals.GSMOPEN_INTERFACES[i].interface_state],
						phone_callflow[globals.GSMOPEN_INTERFACES[i].phone_callflow], globals.GSMOPEN_INTERFACES[i].session_uuid_str);
			} else if (argc > 1 && !strcasecmp(argv[1], "full")) {
				stream->write_function(stream, "%c %d\n", next_flag_char, i);
				//stream->write_function(stream, "%c\t%d\n", next_flag_char, i);
			}

		}
		//stream->write_function(stream, "\nTotal: %d\n", globals.real_interfaces - 1);
		stream->write_function(stream, "\nTotal Interfaces: %d  IB Calls(Failed/Total): %u/%u  OB Calls(Failed/Total): %u/%u\n",
							   globals.real_interfaces > 0 ? globals.real_interfaces - 1 : 0, ib_failed, ib, ob_failed, ob);

	} else if (!strcasecmp(argv[0], "console")) {
		int i;
		int found = 0;

		if (argc == 2) {
			for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
				/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
				if (strlen(globals.GSMOPEN_INTERFACES[i].name)
					&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, argv[1], strlen(argv[1])) == 0)) {
					globals.gsm_console = &globals.GSMOPEN_INTERFACES[i];
					stream->write_function(stream, "gsm console is now: globals.GSMOPEN_INTERFACES[%d].name=|||%s|||\n", i,
										   globals.GSMOPEN_INTERFACES[i].name);
					stream->write_function(stream, "gsm console is: |||%s|||\n", globals.gsm_console->name);
					found = 1;
					break;
				}

			}
			if (!found)
				stream->write_function(stream, "ERROR: A GSMopen interface with name='%s' was not found\n", argv[1]);
		} else {

			stream->write_function(stream, "-ERR Usage: gsm console interface_name\n");
			goto end;
		}

	} else if (!strcasecmp(argv[0], "ciapalino")) {

/* BEGIN: Changes heres */
	} else if (!strcasecmp(argv[0], "reload")) {
		if (load_config(SOFT_RELOAD) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "gsm reload failed\n");
		} else {
			stream->write_function(stream, "gsm reload success\n");
		}
	} else if (!strcasecmp(argv[0], "remove")) {
		if (argc == 2) {
			if (remove_interface(argv[1]) == SWITCH_STATUS_SUCCESS) {
				if (interface_exists(argv[1]) == SWITCH_STATUS_SUCCESS) {
					stream->write_function(stream, "gsm remove '%s' failed\n", argv[1]);
				} else {
					stream->write_function(stream, "gsm remove '%s' success\n", argv[1]);
				}
			}
		} else {
			stream->write_function(stream, "-ERR Usage: gsm remove interface_name\n");
			goto end;
		}
/* END: Changes heres */

	} else {
		if (globals.gsm_console)
			gsmopen_serial_write_AT_noack(globals.gsm_console, (char *) cmd);
		else
			stream->write_function(stream, "gsm console is NOT yet assigned\n");
	}
  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(gsmopen_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	private_t *tech_pvt = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc) {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_SYNTAX);
		goto end;
	}

	if (argc < 2) {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_SYNTAX);
		goto end;
	}

	if (argv[0]) {
		int i;
		int found = 0;

		for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
			/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
			if (strlen(globals.GSMOPEN_INTERFACES[i].name)
				&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, argv[0], strlen(argv[0])) == 0)) {
				tech_pvt = &globals.GSMOPEN_INTERFACES[i];
				stream->write_function(stream, "Using interface: globals.GSMOPEN_INTERFACES[%d].name=|||%s|||\n", i, globals.GSMOPEN_INTERFACES[i].name);
				found = 1;
				break;
			}

		}
		if (!found) {
			stream->write_function(stream, "ERROR: A GSMopen interface with name='%s' was not found\n", argv[0]);
			switch_safe_free(mycmd);

			return SWITCH_STATUS_SUCCESS;
		} else {
			gsmopen_serial_write_AT_noack(tech_pvt, (char *) &cmd[strlen(argv[0]) + 1]);
		}
	} else {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_SYNTAX);
	}
  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}
#endif //0
SWITCH_STANDARD_API(gsmopen_dump_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	private_t *tech_pvt = NULL;
	char value[512];

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc) {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_DUMP_SYNTAX);
		goto end;
	}
	if (argc == 1) {
		int i;
		int found = 0;

		for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
			/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
			if (strlen(globals.GSMOPEN_INTERFACES[i].name)
				&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, argv[0], strlen(argv[0])) == 0)) {
				tech_pvt = &globals.GSMOPEN_INTERFACES[i];
				//stream->write_function(stream, "Using interface: globals.GSMOPEN_INTERFACES[%d].name=|||%s|||\n", i, globals.GSMOPEN_INTERFACES[i].name);
				found = 1;
				break;
			}

		}
		if (!found && (strcmp("list", argv[0]) == 0)) {
			int i;
			stream->write_function(stream, "gsmopen_dump LIST\n\n");
			for (i = 0; i < GSMOPEN_MAX_INTERFACES; i++) {
				if (strlen(globals.GSMOPEN_INTERFACES[i].name)) {
					stream->write_function(stream, "dumping interface '%s'\n\n", globals.GSMOPEN_INTERFACES[i].name);
					tech_pvt = &globals.GSMOPEN_INTERFACES[i];

					stream->write_function(stream, "interface_name = %s\n", tech_pvt->name);
					stream->write_function(stream, "interface_id = %s\n", tech_pvt->id);
					snprintf(value, sizeof(value) - 1, "%d", tech_pvt->active);
					stream->write_function(stream, "active = %s\n", value);
					if (!tech_pvt->network_creg_not_supported) {
						snprintf(value, sizeof(value) - 1, "%d", tech_pvt->not_registered);
						stream->write_function(stream, "not_registered = %s\n", value);
						snprintf(value, sizeof(value) - 1, "%d", tech_pvt->home_network_registered);
						stream->write_function(stream, "home_network_registered = %s\n", value);
						snprintf(value, sizeof(value) - 1, "%d", tech_pvt->roaming_registered);
						stream->write_function(stream, "roaming_registered = %s\n", value);
					} else {
						stream->write_function(stream, "not_registered = %s\n", "N/A");
						stream->write_function(stream, "home_network_registered = %s\n", "N/A");
						stream->write_function(stream, "roaming_registered = %s\n", "N/A");
					}
					snprintf(value, sizeof(value) - 1, "%d", tech_pvt->got_signal);
					stream->write_function(stream, "got_signal = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%d", tech_pvt->running);
					stream->write_function(stream, "running = %s\n", value);
					stream->write_function(stream, "imei = %s\n", tech_pvt->imei);
					stream->write_function(stream, "imsi = %s\n", tech_pvt->imsi);
					snprintf(value, sizeof(value) - 1, "%d", tech_pvt->controldev_dead);
					stream->write_function(stream, "controldev_dead = %s\n", value);
					stream->write_function(stream, "controldevice_name = %s\n", tech_pvt->controldevice_name);
					snprintf(value, sizeof(value) - 1, "%d", tech_pvt->no_sound);
					stream->write_function(stream, "no_sound = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%f", tech_pvt->playback_boost);
					stream->write_function(stream, "playback_boost = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%f", tech_pvt->capture_boost);
					stream->write_function(stream, "capture_boost = %s\n", value);
					stream->write_function(stream, "dialplan = %s\n", tech_pvt->dialplan);
					stream->write_function(stream, "context = %s\n", tech_pvt->context);
					stream->write_function(stream, "destination = %s\n", tech_pvt->destination);
					snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ib_calls);
					stream->write_function(stream, "ib_calls = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ob_calls);
					stream->write_function(stream, "ob_calls = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ib_failed_calls);
					stream->write_function(stream, "ib_failed_calls = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ob_failed_calls);
					stream->write_function(stream, "ob_failed_calls = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%d", tech_pvt->interface_state);
					stream->write_function(stream, "interface_state = %s\n", value);
					snprintf(value, sizeof(value) - 1, "%d", tech_pvt->phone_callflow);
					stream->write_function(stream, "phone_callflow = %s\n", value);
					stream->write_function(stream, "session_uuid_str = %s\n", tech_pvt->session_uuid_str);
					stream->write_function(stream, "\n");

					dump_event(tech_pvt);
				}

			}

		} else if (found) {
			stream->write_function(stream, "dumping interface '%s'\n\n", argv[0]);
			tech_pvt = &globals.GSMOPEN_INTERFACES[i];

			stream->write_function(stream, "interface_name = %s\n", tech_pvt->name);
			stream->write_function(stream, "interface_id = %s\n", tech_pvt->id);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->active);
			stream->write_function(stream, "active = %s\n", value);
			if (!tech_pvt->network_creg_not_supported) {
				snprintf(value, sizeof(value) - 1, "%d", tech_pvt->not_registered);
				stream->write_function(stream, "not_registered = %s\n", value);
				snprintf(value, sizeof(value) - 1, "%d", tech_pvt->home_network_registered);
				stream->write_function(stream, "home_network_registered = %s\n", value);
				snprintf(value, sizeof(value) - 1, "%d", tech_pvt->roaming_registered);
				stream->write_function(stream, "roaming_registered = %s\n", value);
			} else {
				stream->write_function(stream, "not_registered = %s\n", "N/A");
				stream->write_function(stream, "home_network_registered = %s\n", "N/A");
				stream->write_function(stream, "roaming_registered = %s\n", "N/A");
			}
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->got_signal);
			stream->write_function(stream, "got_signal = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->running);
			stream->write_function(stream, "running = %s\n", value);
			stream->write_function(stream, "imei = %s\n", tech_pvt->imei);
			stream->write_function(stream, "imsi = %s\n", tech_pvt->imsi);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->controldev_dead);
			stream->write_function(stream, "controldev_dead = %s\n", value);
			stream->write_function(stream, "controldevice_name = %s\n", tech_pvt->controldevice_name);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->no_sound);
			stream->write_function(stream, "no_sound = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%f", tech_pvt->playback_boost);
			stream->write_function(stream, "playback_boost = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%f", tech_pvt->capture_boost);
			stream->write_function(stream, "capture_boost = %s\n", value);
			stream->write_function(stream, "dialplan = %s\n", tech_pvt->dialplan);
			stream->write_function(stream, "context = %s\n", tech_pvt->context);
			stream->write_function(stream, "destination = %s\n", tech_pvt->destination);
			snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ib_calls);
			stream->write_function(stream, "ib_calls = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ob_calls);
			stream->write_function(stream, "ob_calls = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ib_failed_calls);
			stream->write_function(stream, "ib_failed_calls = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ob_failed_calls);
			stream->write_function(stream, "ob_failed_calls = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->interface_state);
			stream->write_function(stream, "interface_state = %s\n", value);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->phone_callflow);
			stream->write_function(stream, "phone_callflow = %s\n", value);
			stream->write_function(stream, "session_uuid_str = %s\n", tech_pvt->session_uuid_str);
			stream->write_function(stream, "\n");

			dump_event(tech_pvt);
		} else {
			stream->write_function(stream, "interface '%s' was not found\n", argv[0]);
		}
	} else {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_DUMP_SYNTAX);
	}
  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(gsmopen_boost_audio_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	private_t *tech_pvt = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc == 1 || argc == 3) {
		int i;
		int found = 0;

		for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
			/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
			if (strlen(globals.GSMOPEN_INTERFACES[i].name)
				&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, argv[0], strlen(argv[0])) == 0)) {
				tech_pvt = &globals.GSMOPEN_INTERFACES[i];
				stream->write_function(stream, "Using interface: globals.GSMOPEN_INTERFACES[%d].name=|||%s|||\n", i, globals.GSMOPEN_INTERFACES[i].name);
				found = 1;
				break;
			}

		}
		if (!found) {
			stream->write_function(stream, "ERROR: A GSMopen interface with name='%s' was not found\n", argv[0]);

		} else {
			if (argc == 1) {
				stream->write_function(stream, "[%s] capture boost is %f\n", globals.GSMOPEN_INTERFACES[i].name,
									   globals.GSMOPEN_INTERFACES[i].capture_boost);
				stream->write_function(stream, "[%s] playback boost is %f\n", globals.GSMOPEN_INTERFACES[i].name,
									   globals.GSMOPEN_INTERFACES[i].playback_boost);
				stream->write_function(stream, "%s usage: %s", argv[0], GSMOPEN_BOOST_AUDIO_SYNTAX);
				goto end;
			} else if ((strncmp("play", argv[1], strlen(argv[1])) == 0)) {
				if (switch_is_number(argv[2])) {
					stream->write_function(stream, "[%s] playback boost was %f\n", globals.GSMOPEN_INTERFACES[i].name,
										   globals.GSMOPEN_INTERFACES[i].playback_boost);
					gsmopen_store_boost(argv[2], &globals.GSMOPEN_INTERFACES[i].playback_boost);	//FIXME
					stream->write_function(stream, "[%s] playback boost is now %f\n", globals.GSMOPEN_INTERFACES[i].name,
										   globals.GSMOPEN_INTERFACES[i].playback_boost);
				}
			} else if ((strncmp("capt", argv[1], strlen(argv[1])) == 0)) {
				if (switch_is_number(argv[2])) {
					stream->write_function(stream, "[%s] capture boost was %f\n", globals.GSMOPEN_INTERFACES[i].name,
										   globals.GSMOPEN_INTERFACES[i].capture_boost);
					gsmopen_store_boost(argv[2], &globals.GSMOPEN_INTERFACES[i].capture_boost);	//FIXME
					stream->write_function(stream, "[%s] capture boost is now %f\n", globals.GSMOPEN_INTERFACES[i].name,
										   globals.GSMOPEN_INTERFACES[i].capture_boost);
				}
			} else {
				stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_BOOST_AUDIO_SYNTAX);
			}
		}
	} else {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_BOOST_AUDIO_SYNTAX);
	}
  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

#if 0
int gsmopen_transfer(private_t *tech_pvt, char *id, char *value)
{
	char msg_to_gsmopen[1024];
	int i;
	int found = 0;
	private_t *giovatech;
	struct timeval timenow;

	switch_mutex_lock(globals.mutex);

	gettimeofday(&timenow, NULL);
	for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
		if (strlen(globals.GSMOPEN_INTERFACES[i].name)) {

			giovatech = &globals.GSMOPEN_INTERFACES[i];
			//NOTICA("gsmopen interface: %d, name: %s, state: %d, value=%s, giovatech->callid_number=%s, giovatech->gsmopen_user=%s\n", GSMOPEN_P_LOG, i, giovatech->name, giovatech->interface_state, value, giovatech->callid_number, giovatech->gsmopen_user);
			//FIXME check a timestamp here
			if (strlen(giovatech->gsmopen_call_id) && (giovatech->interface_state != GSMOPEN_STATE_DOWN) && (!strcmp(giovatech->gsmopen_user, tech_pvt->gsmopen_user)) && (!strcmp(giovatech->callid_number, value)) && ((((timenow.tv_sec - giovatech->answer_time.tv_sec) * 1000000) + (timenow.tv_usec - giovatech->answer_time.tv_usec)) < 500000)) {	//0.5sec
				found = 1;
				DEBUGA_GSMOPEN
					("FOUND  (name=%s, giovatech->interface_state=%d != GSMOPEN_STATE_DOWN) && (giovatech->gsmopen_user=%s == tech_pvt->gsmopen_user=%s) && (giovatech->callid_number=%s == value=%s)\n",
					 GSMOPEN_P_LOG, giovatech->name, giovatech->interface_state, giovatech->gsmopen_user, tech_pvt->gsmopen_user, giovatech->callid_number,
					 value)
					break;
			}
		}
	}

	if (found) {
		//tech_pvt->callid_number[0]='\0';
		//sprintf(msg_to_gsmopen, "ALTER CALL %s END HANGUP", id);
		//gsmopen_signaling_write(tech_pvt, msg_to_gsmopen);
		switch_mutex_unlock(globals.mutex);
		return 0;
	}
	DEBUGA_GSMOPEN("NOT FOUND\n", GSMOPEN_P_LOG);

	if (!tech_pvt || !tech_pvt->gsmopen_call_id || !strlen(tech_pvt->gsmopen_call_id)) {
		/* we are not inside an active call */
		DEBUGA_GSMOPEN("We're NO MORE in a call now %s\n", GSMOPEN_P_LOG, (tech_pvt && tech_pvt->gsmopen_call_id) ? tech_pvt->gsmopen_call_id : "");
		switch_mutex_unlock(globals.mutex);

	} else {

		/* we're owned, we're in a call, let's try to transfer */
		/************************** TODO
		  Checking here if it is possible to transfer this call to Test2
		  -> GET CALL 288 CAN_TRANSFER Test2
		  <- CALL 288 CAN_TRANSFER test2 TRUE
		 **********************************/

		private_t *available_gsmopen_interface = NULL;

		gettimeofday(&timenow, NULL);
		for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
			if (strlen(globals.GSMOPEN_INTERFACES[i].name)) {

				giovatech = &globals.GSMOPEN_INTERFACES[i];
				//NOTICA("gsmopen interface: %d, name: %s, state: %d, value=%s, giovatech->callid_number=%s, giovatech->gsmopen_user=%s\n", GSMOPEN_P_LOG, i, giovatech->name, giovatech->interface_state, value, giovatech->callid_number, giovatech->gsmopen_user);
				//FIXME check a timestamp here
				if (strlen(giovatech->gsmopen_transfer_call_id) && (giovatech->interface_state != GSMOPEN_STATE_DOWN) && (!strcmp(giovatech->gsmopen_user, tech_pvt->gsmopen_user)) && (!strcmp(giovatech->transfer_callid_number, value)) && ((((timenow.tv_sec - giovatech->transfer_time.tv_sec) * 1000000) + (timenow.tv_usec - giovatech->transfer_time.tv_usec)) < 1000000)) {	//1.0 sec
					found = 1;
					DEBUGA_GSMOPEN
						("FOUND  (name=%s, giovatech->interface_state=%d != GSMOPEN_STATE_DOWN) && (giovatech->gsmopen_user=%s == tech_pvt->gsmopen_user=%s) && (giovatech->transfer_callid_number=%s == value=%s)\n",
						 GSMOPEN_P_LOG, giovatech->name, giovatech->interface_state,
						 giovatech->gsmopen_user, tech_pvt->gsmopen_user, giovatech->transfer_callid_number, value)
						break;
				}
			}
		}

		if (found) {
			//tech_pvt->callid_number[0]='\0';
			//sprintf(msg_to_gsmopen, "ALTER CALL %s END HANGUP", id);
			//gsmopen_signaling_write(tech_pvt, msg_to_gsmopen);
			switch_mutex_unlock(globals.mutex);
			return 0;
		}
		DEBUGA_GSMOPEN("NOT FOUND\n", GSMOPEN_P_LOG);

		available_gsmopen_interface = find_available_gsmopen_interface_rr(tech_pvt);
		if (available_gsmopen_interface) {
			/* there is a gsmopen interface idle, let's transfer the call to it */

			//FIXME write a timestamp here
			gettimeofday(&tech_pvt->transfer_time, NULL);
			switch_copy_string(tech_pvt->gsmopen_transfer_call_id, id, sizeof(tech_pvt->gsmopen_transfer_call_id) - 1);

			switch_copy_string(tech_pvt->transfer_callid_number, value, sizeof(tech_pvt->transfer_callid_number) - 1);

			DEBUGA_GSMOPEN
				("Let's transfer the gsmopen_call %s to %s interface (with gsmopen_user: %s), because we are already in a gsmopen call(%s)\n",
				 GSMOPEN_P_LOG, tech_pvt->gsmopen_call_id, available_gsmopen_interface->name, available_gsmopen_interface->gsmopen_user, id);

			//FIXME why this? the inbound call will come, eventually, on that other interface
			//available_gsmopen_interface->ib_calls++;

			sprintf(msg_to_gsmopen, "ALTER CALL %s TRANSFER %s", id, available_gsmopen_interface->gsmopen_user);
			//gsmopen_signaling_write(tech_pvt, msg_to_gsmopen);
			if (tech_pvt->interface_state == GSMOPEN_STATE_SELECTED) {
				tech_pvt->interface_state = GSMOPEN_STATE_IDLE;	//we marked it GSMOPEN_STATE_SELECTED just in case it has to make an outbound call
			}
		} else {
			/* no gsmopen interfaces idle, do nothing */
			DEBUGA_GSMOPEN
				("Not answering the gsmopen_call %s, because we are already in a gsmopen call(%s) and not transferring, because no other gsmopen interfaces are available\n",
				 GSMOPEN_P_LOG, id, tech_pvt->gsmopen_call_id);
			sprintf(msg_to_gsmopen, "ALTER CALL %s END HANGUP", id);
			//gsmopen_signaling_write(tech_pvt, msg_to_gsmopen);
		}
		switch_sleep(10000);
		DEBUGA_GSMOPEN
			("We have NOT answered a GSM RING from gsmopen_call %s, because we are already in a gsmopen call (%s)\n",
			 GSMOPEN_P_LOG, id, tech_pvt->gsmopen_call_id);

		switch_mutex_unlock(globals.mutex);
	}
	return 0;
}
#endif //0

void *gsmopen_do_gsmopenapi_thread_func(void *obj)
{

	private_t *tech_pvt = (private_t *) obj;
	time_t now_timestamp;

	//if (gsmopen_present(GSMopenHandles)) 
	while (running && tech_pvt->running) {
		int res;
		//gsmopen_sleep(1000000); //1 sec
		//DEBUGA_GSMOPEN("ciao!\n", GSMOPEN_P_LOG);
		res = gsmopen_serial_read(tech_pvt);
		if (res == -1) {		//manage the graceful interface shutdown
			tech_pvt->controldev_dead = 1;
			close(tech_pvt->controldevfd);
			ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
			tech_pvt->running = 0;
			alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
			tech_pvt->active = 0;
			tech_pvt->name[0] = '\0';
			switch_sleep(1000000);
		} else if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL && tech_pvt->interface_state == GSMOPEN_STATE_RING
				   && tech_pvt->phone_callflow != CALLFLOW_CALL_HANGUP_REQUESTED) {
			//WARNINGA("INCOMING RING\n", GSMOPEN_P_LOG);

			gsmopen_ring(tech_pvt);

			//FIXME gsmopen_answer(tech_pvt);
			//new_inbound_channel(tech_pvt);
			//FIXME if (!gsmopen_new(p, AST_STATE_RING, tech_pvt->context)) {
			//FIXME ERRORA("gsmopen_new failed! BAD BAD BAD\n", GSMOPEN_P_LOG);
			//FIXME }

		} else if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL && tech_pvt->interface_state == GSMOPEN_STATE_DIALING) {
			DEBUGA_GSMOPEN("WE'RE DIALING, let's take the earlymedia\n", GSMOPEN_P_LOG);
			tech_pvt->interface_state = CALLFLOW_STATUS_EARLYMEDIA;
			remote_party_is_early_media(tech_pvt);
			//new_inbound_channel(tech_pvt);
			//FIXME if (!gsmopen_new(p, AST_STATE_RING, tech_pvt->context)) {
			//FIXME ERRORA("gsmopen_new failed! BAD BAD BAD\n", GSMOPEN_P_LOG);
			//FIXME }

		} else if (tech_pvt->interface_state == CALLFLOW_CALL_REMOTEANSWER) {
			DEBUGA_GSMOPEN("REMOTE PARTY ANSWERED\n", GSMOPEN_P_LOG);
			outbound_channel_answered(tech_pvt);
			//new_inbound_channel(tech_pvt);
			//FIXME if (!gsmopen_new(p, AST_STATE_RING, tech_pvt->context)) {
			//FIXME ERRORA("gsmopen_new failed! BAD BAD BAD\n", GSMOPEN_P_LOG);
			//FIXME }
		}
		switch_sleep(100);		//give other threads a chance
		time(&now_timestamp);

		if ((now_timestamp - tech_pvt->gsmopen_serial_synced_timestamp) > tech_pvt->gsmopen_serial_sync_period) {	//TODO find a sensible period. 5min? in config?
			gsmopen_serial_sync(tech_pvt);
			gsmopen_serial_getstatus_AT(tech_pvt);
		}
	}
	DEBUGA_GSMOPEN("EXIT\n", GSMOPEN_P_LOG);
	//running = 0;
	return NULL;

}

SWITCH_STANDARD_API(sendsms_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;
	private_t *tech_pvt = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc) {
		stream->write_function(stream, "ERROR, usage: %s", SENDSMS_SYNTAX);
		goto end;
	}

	if (argc < 3) {
		stream->write_function(stream, "ERROR, usage: %s", SENDSMS_SYNTAX);
		goto end;
	}

	if (argv[0]) {
		int i;
		int found = 0;

		for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
			/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
			if (strlen(globals.GSMOPEN_INTERFACES[i].name)
				&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, argv[0], strlen(argv[0])) == 0)) {
				tech_pvt = &globals.GSMOPEN_INTERFACES[i];
				stream->write_function(stream, "Trying to send your SMS: interface=%s, dest=%s, text=%s\n", argv[0], argv[1], argv[2]);
				found = 1;
				break;
			}

		}
		if (!found) {
			stream->write_function(stream, "ERROR: A GSMopen interface with name='%s' was not found\n", argv[0]);
			switch_safe_free(mycmd);

			return SWITCH_STATUS_SUCCESS;
		} else {
			//gsmopen_sendsms(tech_pvt, (char *) argv[1], (char *) argv[2]);
			NOTICA("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, type=NULL, hint=%s)\n", GSMOPEN_P_LOG, GSMOPEN_CHAT_PROTO, tech_pvt->name,
				   argv[1], "SIMPLE MESSAGE", switch_str_nil(argv[2]), tech_pvt->name);

			compat_chat_send(GSMOPEN_CHAT_PROTO, tech_pvt->name, argv[1], "SIMPLE MESSAGE", switch_str_nil(argv[2]), NULL, tech_pvt->name);
		}
	} else {
		stream->write_function(stream, "ERROR, usage: %s", SENDSMS_SYNTAX);
	}
  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

int dump_event_full(private_t *tech_pvt, int is_alarm, int alarm_code, const char *alarm_message)
{
	switch_event_t *event;
	char value[512];
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	switch_status_t status;

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	if (session) {
		channel = switch_core_session_get_channel(session);
	}

	if (is_alarm) {
		ERRORA("ALARM on interface %s: \n", GSMOPEN_P_LOG, tech_pvt->name);
		status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_ALARM);
	} else {
		DEBUGA_GSMOPEN("DUMP on interface %s: \n", GSMOPEN_P_LOG, tech_pvt->name);
		status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_DUMP);
	}
	if (status == SWITCH_STATUS_SUCCESS) {
		if (is_alarm) {
			snprintf(value, sizeof(value) - 1, "%d", alarm_code);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm_code", value);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm_message", alarm_message);
		}
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "interface_name", tech_pvt->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "interface_id", tech_pvt->id);
		snprintf(value, sizeof(value) - 1, "%d", tech_pvt->active);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "active", value);
		if (!tech_pvt->network_creg_not_supported) {
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->not_registered);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "not_registered", value);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->home_network_registered);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "home_network_registered", value);
			snprintf(value, sizeof(value) - 1, "%d", tech_pvt->roaming_registered);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "roaming_registered", value);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "not_registered", "N/A");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "home_network_registered", "N/A");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "roaming_registered", "N/A");
		}
		snprintf(value, sizeof(value) - 1, "%d", tech_pvt->got_signal);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "got_signal", value);
		snprintf(value, sizeof(value) - 1, "%d", tech_pvt->running);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "running", value);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "imei", tech_pvt->imei);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "imsi", tech_pvt->imsi);
		snprintf(value, sizeof(value) - 1, "%d", tech_pvt->controldev_dead);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "controldev_dead", value);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "controldevice_name", tech_pvt->controldevice_name);
		snprintf(value, sizeof(value) - 1, "%d", tech_pvt->no_sound);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "no_sound", value);
		snprintf(value, sizeof(value) - 1, "%f", tech_pvt->playback_boost);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "playback_boost", value);
		snprintf(value, sizeof(value) - 1, "%f", tech_pvt->capture_boost);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "capture_boost", value);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "dialplan", tech_pvt->dialplan);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "context", tech_pvt->context);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "destination", tech_pvt->destination);
		snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ib_calls);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ib_calls", value);
		snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ob_calls);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ob_calls", value);
		snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ib_failed_calls);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ib_failed_calls", value);
		snprintf(value, sizeof(value) - 1, "%lu", tech_pvt->ob_failed_calls);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ob_failed_calls", value);
		snprintf(value, sizeof(value) - 1, "%d", tech_pvt->interface_state);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "interface_state", value);
		snprintf(value, sizeof(value) - 1, "%d", tech_pvt->phone_callflow);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "phone_callflow", value);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "session_uuid_str", tech_pvt->session_uuid_str);
		if (strlen(tech_pvt->session_uuid_str)) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "true");
		} else {				//no session
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "false");
		}
		if (channel) {
			switch_channel_event_set_data(channel, event);
		}
		switch_event_fire(&event);
	} else {
		ERRORA("cannot create event on interface %s. WHY?????\n", GSMOPEN_P_LOG, tech_pvt->name);
	}

	if (session) {
		switch_core_session_rwunlock(session);
	}
	return 0;
}

int dump_event(private_t *tech_pvt)
{
	return dump_event_full(tech_pvt, 0, 0, NULL);
}

int alarm_event(private_t *tech_pvt, int alarm_code, const char *alarm_message)
{
	return dump_event_full(tech_pvt, 1, alarm_code, alarm_message);
}

int sms_incoming(private_t *tech_pvt)
{
	switch_event_t *event;
	switch_core_session_t *session = NULL;
	int event_sent_to_esl = 0;

	//DEBUGA_GSMOPEN("received SMS on interface %s: %s\n", GSMOPEN_P_LOG, tech_pvt->name, tech_pvt->sms_message);
	DEBUGA_GSMOPEN("received SMS on interface %s: DATE=%s, SENDER=%s, BODY=%s|\n", GSMOPEN_P_LOG, tech_pvt->name, tech_pvt->sms_date, tech_pvt->sms_sender,
				   tech_pvt->sms_body);

#ifdef NOTDEF
	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	}
	if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", GSMOPEN_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", tech_pvt->name);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", tech_pvt->chatmessages[which].from_dispname);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", tech_pvt->sms_sender);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "date", tech_pvt->sms_date);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "datacodingscheme", tech_pvt->sms_datacodingscheme);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "servicecentreaddress", tech_pvt->sms_servicecentreaddress);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "messagetype", "%d", tech_pvt->sms_messagetype);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "chatname", tech_pvt->chatmessages[which].chatname);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "id", tech_pvt->chatmessages[which].id);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to", tech_pvt->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", tech_pvt->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_proto", GSMOPEN_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_user", tech_pvt->sms_sender);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_host", "from_host");
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_full", "from_full");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_user", tech_pvt->name);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_host", "to_host");
		switch_event_add_body(event, "%s\n", tech_pvt->sms_body);
		if (session) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "true");
			if (switch_core_session_queue_event(session, &event) != SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
				switch_event_fire(&event);
			}
		} else {				//no session
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "false");
			switch_event_fire(&event);
			event_sent_to_esl = 1;
		}

	} else {
		ERRORA("cannot create event on interface %s. WHY?????\n", GSMOPEN_P_LOG, tech_pvt->name);
	}

	if (!event_sent_to_esl) {

		if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", GSMOPEN_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", tech_pvt->name);
			//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", tech_pvt->chatmessages[which].from_dispname);
			//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", tech_pvt->chatmessages[which].from_handle);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", tech_pvt->sms_sender);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "date", tech_pvt->sms_date);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "datacodingscheme", tech_pvt->sms_datacodingscheme);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "servicecentreaddress", tech_pvt->sms_servicecentreaddress);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "messagetype", "%d", tech_pvt->sms_messagetype);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
			//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "chatname", tech_pvt->chatmessages[which].chatname);
			//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "id", tech_pvt->chatmessages[which].id);
			switch_event_add_body(event, "%s\n", tech_pvt->sms_body);
			if (session) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "true");
			} else {			//no session
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "false");
			}
			switch_event_fire(&event);
		} else {
			ERRORA("cannot create event on interface %s. WHY?????\n", GSMOPEN_P_LOG, tech_pvt->name);
		}
	}

	if (session) {
		switch_core_session_rwunlock(session);
	}
#endif //NOTDEF
	/* mod_sms begin */
	if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", GSMOPEN_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", tech_pvt->name);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", tech_pvt->chatmessages[which].from_dispname);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", tech_pvt->sms_sender);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "date", tech_pvt->sms_date);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "datacodingscheme", tech_pvt->sms_datacodingscheme);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "servicecentreaddress", tech_pvt->sms_servicecentreaddress);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "messagetype", "%d", tech_pvt->sms_messagetype);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "chatname", tech_pvt->chatmessages[which].chatname);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "id", tech_pvt->chatmessages[which].id);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to", tech_pvt->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", tech_pvt->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_proto", GSMOPEN_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_user", tech_pvt->sms_sender);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_host", "from_host");
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_full", "from_full");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_user", tech_pvt->name);
		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_host", "to_host");
		switch_event_add_body(event, "%s\n", tech_pvt->sms_body);
		//switch_core_chat_send("GLOBAL", event); /* mod_sms */
		switch_core_chat_send("GLOBAL", event);	/* mod_sms */
	} else {

		ERRORA("cannot create event on interface %s. WHY?????\n", GSMOPEN_P_LOG, tech_pvt->name);
	}
	/* mod_sms end */


	//memset(&tech_pvt->chatmessages[which], '\0', sizeof(&tech_pvt->chatmessages[which]) );
	//memset(tech_pvt->sms_message, '\0', sizeof(tech_pvt->sms_message));
	return 0;
}

#ifdef NOTDEF
SWITCH_STANDARD_API(gsmopen_chat_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	private_t *tech_pvt = NULL;
	//int tried =0;
	int i;
	int found = 0;
	//char skype_msg[1024];

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc) {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_CHAT_SYNTAX);
		goto end;
	}

	if (argc < 3) {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_CHAT_SYNTAX);
		goto end;
	}

	if (argv[0]) {
		for (i = 0; !found && i < GSMOPEN_MAX_INTERFACES; i++) {
			/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
			if (strlen(globals.GSMOPEN_INTERFACES[i].name)
				&& (strncmp(globals.GSMOPEN_INTERFACES[i].name, argv[0], strlen(argv[0])) == 0)) {
				tech_pvt = &globals.GSMOPEN_INTERFACES[i];
				stream->write_function(stream, "Using interface: globals.GSMOPEN_INTERFACES[%d].name=|||%s|||\n", i, globals.GSMOPEN_INTERFACES[i].name);
				found = 1;
				break;
			}

		}
		if (!found) {
			stream->write_function(stream, "ERROR: A GSMopen interface with name='%s' was not found\n", argv[0]);
			goto end;
		} else {

			//chat_send(const char *proto, const char *from, const char *to, const char *subject, const char *body, const char *type, const char *hint);
			//chat_send(p*roto, const char *from, const char *to, const char *subject, const char *body, const char *type, const char *hint);
			//chat_send(GSMOPEN_CHAT_PROTO, tech_pvt->skype_user, argv[1], "SIMPLE MESSAGE", switch_str_nil((char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]), NULL, hint);

			NOTICA("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, type=NULL, hint=%s)\n", GSMOPEN_P_LOG, GSMOPEN_CHAT_PROTO,
				   tech_pvt->skype_user, argv[1], "SIMPLE MESSAGE", switch_str_nil((char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]),
				   tech_pvt->name);

			chat_send(GSMOPEN_CHAT_PROTO, tech_pvt->skype_user, argv[1], "SIMPLE MESSAGE",
					  switch_str_nil((char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]), NULL, tech_pvt->name);

			//NOTICA("TEXT is: %s\n", GSMOPEN_P_LOG, (char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1] );
			//snprintf(skype_msg, sizeof(skype_msg), "CHAT CREATE %s", argv[1]);
			//gsmopen_signaling_write(tech_pvt, skype_msg);
			//switch_sleep(100);
		}
	} else {
		stream->write_function(stream, "ERROR, usage: %s", GSMOPEN_CHAT_SYNTAX);
		goto end;
	}

#ifdef NOTDEF

	found = 0;

	while (!found) {
		for (i = 0; i < MAX_CHATS; i++) {
			if (!strcmp(tech_pvt->chats[i].dialog_partner, argv[1])) {
				snprintf(skype_msg, sizeof(skype_msg), "CHATMESSAGE %s %s", tech_pvt->chats[i].chatname,
						 (char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]);
				gsmopen_signaling_write(tech_pvt, skype_msg);
				found = 1;
				break;
			}
		}
		if (found) {
			break;
		}
		if (tried > 1000) {
			stream->write_function(stream, "ERROR: no chat with dialog_partner='%s' was found\n", argv[1]);
			break;
		}
		switch_sleep(1000);
	}
#endif //NOTDEF

  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}
#endif // NOTDEF

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet expandtab:
 */
