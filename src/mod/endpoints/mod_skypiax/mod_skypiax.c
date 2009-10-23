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

#include "skypiax.h"
#define MDL_CHAT_PROTO "skype"

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
SWITCH_MODULE_LOAD_FUNCTION(mod_skypiax_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skypiax_shutdown);
SWITCH_MODULE_DEFINITION(mod_skypiax, mod_skypiax_load, mod_skypiax_shutdown, NULL);
SWITCH_STANDARD_API(sk_function);
/* BEGIN: Changes here */
#define SK_SYNTAX "list [full] || console || skype_API_msg || remove < skypeusername | #interface_name | #interface_id > || reload"
/* END: Changes heres */
SWITCH_STANDARD_API(skypiax_function);
#define SKYPIAX_SYNTAX "interface_name skype_API_msg"

SWITCH_STANDARD_API(skypiax_chat_function);
#define SKYPIAX_CHAT_SYNTAX "interface_name remote_skypename TEXT"
/* BEGIN: Changes here */
#define FULL_RELOAD 0
#define SOFT_RELOAD 1
/* END: Changes heres */

char *interface_status[] = {	/* should match SKYPIAX_STATE_xxx in skypiax.h */
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
char *skype_callflow[] = {		/* should match CALLFLOW_XXX in skypiax.h */
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
	private_t SKYPIAX_INTERFACES[SKYPIAX_MAX_INTERFACES];
	switch_mutex_t *mutex;
	private_t *sk_console;
} globals;

switch_endpoint_interface_t *skypiax_endpoint_interface;
switch_memory_pool_t *skypiax_module_pool = NULL;
int running = 0;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_context, globals.context);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_destination, globals.destination);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_rates_string, globals.codec_rates_string);

/* BEGIN: Changes here */
static switch_status_t interface_exists(char *the_interface);
static switch_status_t remove_interface(char *the_interface);
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
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);
static switch_status_t skypiax_tech_init(private_t * tech_pvt, switch_core_session_t *session);

static switch_status_t skypiax_codec(private_t * tech_pvt, int sample_rate, int codec_ms)
{
	switch_core_session_t *session = NULL;

	if (switch_core_codec_init
		(&tech_pvt->read_codec, "L16", NULL, sample_rate, codec_ms, 1,
		 SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		ERRORA("Can't load codec?\n", SKYPIAX_P_LOG);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init
		(&tech_pvt->write_codec, "L16", NULL, sample_rate, codec_ms, 1,
		 SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		ERRORA("Can't load codec?\n", SKYPIAX_P_LOG);
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
		ERRORA("no session\n", SKYPIAX_P_LOG);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t skypiax_tech_init(private_t * tech_pvt, switch_core_session_t *session)
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
		ERRORA("no tech_pvt->session_uuid_str\n", SKYPIAX_P_LOG);
		return SWITCH_STATUS_FALSE;
	}
	if (skypiax_codec(tech_pvt, SAMPLERATE_SKYPIAX, 20) != SWITCH_STATUS_SUCCESS) {
		ERRORA("skypiax_codec FAILED\n", SKYPIAX_P_LOG);
		return SWITCH_STATUS_FALSE;
	}
	DEBUGA_SKYPE("skypiax_codec SUCCESS\n", SKYPIAX_P_LOG);
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
			if (strlen(globals.SKYPIAX_INTERFACES[interface_id].name)) {
				return SWITCH_STATUS_SUCCESS;
			}
		} else {
			/* interface name */
			for (interface_id = 0; interface_id < SKYPIAX_MAX_INTERFACES; interface_id++) {
				if (strcmp(globals.SKYPIAX_INTERFACES[interface_id].name, the_interface) == 0) {
					return SWITCH_STATUS_SUCCESS;
					break;
				}
			}
		}
	} else {					/* look by skype_user */


		for (i = 0; i < SKYPIAX_MAX_INTERFACES; i++) {
			if (strlen(globals.SKYPIAX_INTERFACES[i].skype_user)) {
				if (strcmp(globals.SKYPIAX_INTERFACES[i].skype_user, the_interface) == 0) {
					return SWITCH_STATUS_SUCCESS;
				}
			}
		}
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status_t remove_interface(char *the_interface)
{
	int x = 10;
	unsigned int howmany = 8;
	int interface_id = -1;
	private_t *tech_pvt = NULL;
	switch_status_t status;

	//running = 0;


	if (*the_interface == '#') {	/* remove by interface id or interface name */
		the_interface++;
		switch_assert(the_interface);
		interface_id = atoi(the_interface);

		if (interface_id > 0 || (interface_id == 0 && strcmp(the_interface, "0") == 0)) {
			/* take a number as interface id */
			tech_pvt = &globals.SKYPIAX_INTERFACES[interface_id];
		} else {

			for (interface_id = 0; interface_id < SKYPIAX_MAX_INTERFACES; interface_id++) {
				if (strcmp(globals.SKYPIAX_INTERFACES[interface_id].name, the_interface) == 0) {
					tech_pvt = &globals.SKYPIAX_INTERFACES[interface_id];
					break;
				}
			}
		}
	} else {					/* remove by skype_user */
		for (interface_id = 0; interface_id < SKYPIAX_MAX_INTERFACES; interface_id++) {
			if (strcmp(globals.SKYPIAX_INTERFACES[interface_id].skype_user, the_interface) == 0) {
				tech_pvt = &globals.SKYPIAX_INTERFACES[interface_id];
				break;
			}
		}
	}

	if (!tech_pvt) {
		DEBUGA_SKYPE("interface '%s' does not exist\n", SKYPIAX_P_LOG, the_interface);
		goto end;
	}

	if (strlen(globals.SKYPIAX_INTERFACES[interface_id].session_uuid_str)) {
		DEBUGA_SKYPE("interface '%s' is busy\n", SKYPIAX_P_LOG, the_interface);
		goto end;
	}

	globals.SKYPIAX_INTERFACES[interface_id].running = 0;

	if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_signaling_thread) {
#ifdef WIN32
		switch_file_write(tech_pvt->SkypiaxHandles.fdesc[1], "sciutati", &howmany);	// let's the controldev_thread die
#else /* WIN32 */
		howmany = write(tech_pvt->SkypiaxHandles.fdesc[1], "sciutati", howmany);
#endif /* WIN32 */
	}

	if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread) {
#ifdef WIN32
		if (SendMessage(tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle, WM_DESTROY, 0, 0) == FALSE) {	// let's the skypiax_api_thread_func die
			DEBUGA_SKYPE("got FALSE here, thread probably was already dead. GetLastError returned: %d\n", SKYPIAX_P_LOG, GetLastError());
			globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread = NULL;
		}
#else
		if(tech_pvt->running && tech_pvt->SkypiaxHandles.disp){
			XEvent e;
			Atom atom1 = XInternAtom(tech_pvt->SkypiaxHandles.disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False);
			memset(&e, 0, sizeof(e));
			e.xclient.type = ClientMessage;
			e.xclient.message_type = atom1;	/*  leading message */
			e.xclient.display = tech_pvt->SkypiaxHandles.disp;
			e.xclient.window = tech_pvt->SkypiaxHandles.skype_win;
			e.xclient.format = 8;

			XSendEvent(tech_pvt->SkypiaxHandles.disp, tech_pvt->SkypiaxHandles.win, False, 0, &e);
			XSync(tech_pvt->SkypiaxHandles.disp, False);
		}
#endif
	}

	while (x) {
		x--;
		switch_yield(50000);
	}

	if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_signaling_thread) {
		switch_thread_join(&status, globals.SKYPIAX_INTERFACES[interface_id].skypiax_signaling_thread);
	}

	if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread) {
		switch_thread_join(&status, globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread);
	}

	switch_mutex_lock(globals.mutex);
	if (globals.sk_console == &globals.SKYPIAX_INTERFACES[interface_id]) {
		DEBUGA_SKYPE("interface '%s' no more console\n", SKYPIAX_P_LOG, the_interface);
		globals.sk_console = NULL;
	} else {
		DEBUGA_SKYPE("interface '%s' STILL console\n", SKYPIAX_P_LOG, the_interface);
	}
	memset(&globals.SKYPIAX_INTERFACES[interface_id], '\0', sizeof(private_t));
	globals.real_interfaces--;
	switch_mutex_unlock(globals.mutex);

	DEBUGA_SKYPE("interface '%s' deleted successfully\n", SKYPIAX_P_LOG, the_interface);
	globals.SKYPIAX_INTERFACES[interface_id].running = 1;
  end:
	//running = 1;
	return SWITCH_STATUS_SUCCESS;
}

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

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);
	//ERRORA("%s CHANNEL INIT\n", SKYPIAX_P_LOG, tech_pvt->name);
	switch_set_flag(tech_pvt, TFLAG_IO);

	/* Move channel's state machine to ROUTING. This means the call is trying
	   to get from the initial start where the call because, to the point
	   where a destination has been identified. If the channel is simply
	   left in the initial state, nothing will happen. */
	switch_channel_set_state(channel, CS_ROUTING);
	switch_mutex_lock(globals.mutex);
	globals.calls++;

	switch_mutex_unlock(globals.mutex);
	DEBUGA_SKYPE("%s CHANNEL INIT %s\n", SKYPIAX_P_LOG, tech_pvt->name, switch_core_session_get_uuid(session));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);


	if (tech_pvt) {
		DEBUGA_SKYPE("%s CHANNEL DESTROY %s\n", SKYPIAX_P_LOG, tech_pvt->name, switch_core_session_get_uuid(session));

		if (switch_core_codec_ready(&tech_pvt->read_codec)) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->write_codec)) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
		*tech_pvt->session_uuid_str = '\0';
		tech_pvt->interface_state = SKYPIAX_STATE_IDLE;
		if (tech_pvt->skype_callflow == CALLFLOW_STATUS_FINISHED) {
			tech_pvt->skype_callflow = CALLFLOW_CALL_IDLE;
		}
		switch_core_session_set_private(session, NULL);
	} else {
		DEBUGA_SKYPE("!!!!!!NO tech_pvt!!!! CHANNEL DESTROY %s\n", SKYPIAX_P_LOG, switch_core_session_get_uuid(session));
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	char msg_to_skype[256];


	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_channel_test_flag(channel, CF_ANSWERED)) {
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			tech_pvt->ob_failed_calls++;
		} else {
			tech_pvt->ib_failed_calls++;
		}
	}

	switch_clear_flag(tech_pvt, TFLAG_IO);
	switch_clear_flag(tech_pvt, TFLAG_VOICE);
	//switch_set_flag(tech_pvt, TFLAG_HANGUP);

	if (strlen(tech_pvt->skype_call_id)) {
		//switch_thread_cond_signal(tech_pvt->cond);
		DEBUGA_SKYPE("hanging up skype call: %s\n", SKYPIAX_P_LOG, tech_pvt->skype_call_id);
		sprintf(msg_to_skype, "ALTER CALL %s HANGUP", tech_pvt->skype_call_id);
		skypiax_signaling_write(tech_pvt, msg_to_skype);
	}
	//memset(tech_pvt->session_uuid_str, '\0', sizeof(tech_pvt->session_uuid_str));
	//*tech_pvt->session_uuid_str = '\0';
	DEBUGA_SKYPE("%s CHANNEL HANGUP\n", SKYPIAX_P_LOG, tech_pvt->name);
	switch_mutex_lock(globals.mutex);
	globals.calls--;
	if (globals.calls < 0) {
		globals.calls = 0;
	}

	tech_pvt->interface_state = SKYPIAX_STATE_IDLE;
	if (tech_pvt->skype_callflow == CALLFLOW_STATUS_FINISHED) {
		tech_pvt->skype_callflow = CALLFLOW_CALL_IDLE;
	}
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_SKYPE("%s CHANNEL ROUTING\n", SKYPIAX_P_LOG, tech_pvt->name);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_SKYPE("%s CHANNEL EXECUTE\n", SKYPIAX_P_LOG, tech_pvt->name);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_SKYPE("%s CHANNEL KILL_CHANNEL\n", SKYPIAX_P_LOG, tech_pvt->name);
	switch (sig) {
	case SWITCH_SIG_KILL:
		DEBUGA_SKYPE("%s CHANNEL got SWITCH_SIG_KILL\n", SKYPIAX_P_LOG, switch_channel_get_name(channel));
		//switch_mutex_lock(tech_pvt->flag_mutex);
		switch_clear_flag(tech_pvt, TFLAG_IO);
		switch_clear_flag(tech_pvt, TFLAG_VOICE);
		switch_set_flag(tech_pvt, TFLAG_HANGUP);
		if (tech_pvt->skype_callflow == CALLFLOW_STATUS_REMOTEHOLD) {
			ERRORA("FYI %s CHANNEL in CALLFLOW_STATUS_REMOTEHOLD got SWITCH_SIG_KILL\n", SKYPIAX_P_LOG, switch_channel_get_name(channel));
			channel_on_hangup(session);
		}
		if (switch_channel_get_state(channel) == CS_NEW) {
			ERRORA("FYI %s CHANNEL in CS_NEW state got SWITCH_SIG_KILL\n", SKYPIAX_P_LOG, switch_channel_get_name(channel));
			channel_on_hangup(session);
		}
		if ( switch_channel_get_state(channel) != CS_NEW && switch_channel_get_state(channel) < CS_EXECUTE) {
			ERRORA("FYI %s CHANNEL in %d state got SWITCH_SIG_KILL\n", SKYPIAX_P_LOG, switch_channel_get_name(channel), switch_channel_get_state(channel));
			channel_on_hangup(session);
		}
		//switch_mutex_unlock(tech_pvt->flag_mutex);
		break;
	case SWITCH_SIG_BREAK:
		DEBUGA_SKYPE("%s CHANNEL got SWITCH_SIG_BREAK\n", SKYPIAX_P_LOG, switch_channel_get_name(channel));
		//switch_set_flag(tech_pvt, TFLAG_BREAK);
		//switch_mutex_lock(tech_pvt->flag_mutex);
		switch_set_flag(tech_pvt, TFLAG_BREAK);
		//switch_mutex_unlock(tech_pvt->flag_mutex);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}
static switch_status_t channel_on_consume_media(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);

	DEBUGA_SKYPE("%s CHANNEL CONSUME_MEDIA\n", SKYPIAX_P_LOG, tech_pvt->name);
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;
	tech_pvt = switch_core_session_get_private(session);
	DEBUGA_SKYPE("%s CHANNEL EXCHANGE_MEDIA\n", SKYPIAX_P_LOG, tech_pvt->name);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;
	tech_pvt = switch_core_session_get_private(session);
	DEBUGA_SKYPE("%s CHANNEL SOFT_EXECUTE\n", SKYPIAX_P_LOG, tech_pvt->name);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_SKYPE("%s CHANNEL SEND_DTMF\n", SKYPIAX_P_LOG, tech_pvt->name);
	DEBUGA_SKYPE("DTMF: %c\n", SKYPIAX_P_LOG, dtmf->digit);

	skypiax_senddigit(tech_pvt, dtmf->digit);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	switch_byte_t *data;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_channel_ready(channel) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
		ERRORA("channel not ready \n", SKYPIAX_P_LOG);
		//TODO: kill the bastard
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->read_frame.flags = SFF_NONE;
	*frame = NULL;

	if (!skypiax_audio_read(tech_pvt)) {

		ERRORA("skypiax_audio_read ERROR\n", SKYPIAX_P_LOG);

	} else {
		switch_set_flag(tech_pvt, TFLAG_VOICE);
	}

	while (switch_test_flag(tech_pvt, TFLAG_IO)) {
		if (switch_test_flag(tech_pvt, TFLAG_BREAK)) {
			switch_clear_flag(tech_pvt, TFLAG_BREAK);
			DEBUGA_SKYPE("CHANNEL READ FRAME goto CNG\n", SKYPIAX_P_LOG);
			goto cng;
		}

		if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
			DEBUGA_SKYPE("CHANNEL READ FRAME not IO\n", SKYPIAX_P_LOG);
			return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(tech_pvt, TFLAG_IO) && switch_test_flag(tech_pvt, TFLAG_VOICE)) {
			switch_clear_flag(tech_pvt, TFLAG_VOICE);
			if (!tech_pvt->read_frame.datalen) {
				DEBUGA_SKYPE("CHANNEL READ CONTINUE\n", SKYPIAX_P_LOG);
				continue;
			}
			*frame = &tech_pvt->read_frame;
#ifdef BIGENDIAN
			if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
				switch_swap_linear((*frame)->data, (int) (*frame)->datalen / 2);
			}
#endif
			return SWITCH_STATUS_SUCCESS;
		}

		DEBUGA_SKYPE("CHANNEL READ no TFLAG_IO\n", SKYPIAX_P_LOG);
		return SWITCH_STATUS_FALSE;

	}

	DEBUGA_SKYPE("CHANNEL READ FALSE\n", SKYPIAX_P_LOG);
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

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_channel_ready(channel) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
		ERRORA("channel not ready \n", SKYPIAX_P_LOG);
		//TODO: kill the bastard
		return SWITCH_STATUS_FALSE;
	}
#ifdef BIGENDIAN
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
		switch_swap_linear(frame->data, (int) frame->datalen / 2);
	}
#endif

	sent = frame->datalen;
#ifdef WIN32
	switch_file_write(tech_pvt->audioskypepipe[1], frame->data, &sent);
#else /* WIN32 */
	sent = write(tech_pvt->audioskypepipe[1], frame->data, sent);
#endif /* WIN32 */
	if (sent != frame->datalen && sent != -1) {
		DEBUGA_SKYPE("CLI PIPE write %d\n", SKYPIAX_P_LOG, sent);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	private_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	DEBUGA_SKYPE("ANSWERED! \n", SKYPIAX_P_LOG);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	//int i;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (msg->message_id) {
		case SWITCH_MESSAGE_INDICATE_ANSWER:
			{
				DEBUGA_SKYPE("MSG_ID=%d, TO BE ANSWERED!\n", SKYPIAX_P_LOG, msg->message_id);
				channel_answer_channel(session);
			}
			break;
		case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:

			DEBUGA_SKYPE("%s CHANNEL got SWITCH_MESSAGE_INDICATE_AUDIO_SYNC\n", SKYPIAX_P_LOG, switch_channel_get_name(channel));

			//for (i=0; i<50; i++) {
			//skypiax_audio_read(tech_pvt);
			//WARNINGA("read samples\n", SKYPIAX_P_LOG);
			//}
			//switch_core_timer_sync(&tech_pvt->timer_read);
			//switch_core_timer_sync(&tech_pvt->timer_write);

			break;
		default:
			{
				DEBUGA_SKYPE("MSG_ID=%d\n", SKYPIAX_P_LOG, msg->message_id);
			}
			break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	struct private_object *tech_pvt = switch_core_session_get_private(session);
	char *body = switch_event_get_body(event);
	switch_assert(tech_pvt != NULL);

	if (!body) {
		body = "";
	}

	WARNINGA("event: |||%s|||\n", SKYPIAX_P_LOG, body);

	return SWITCH_STATUS_SUCCESS;
}

switch_state_handler_table_t skypiax_state_handlers = {
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

switch_io_routines_t skypiax_io_routines = {
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
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags)
{
	private_t *tech_pvt = NULL;
	if ((*new_session = switch_core_session_request(skypiax_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, pool)) != 0) {
		switch_channel_t *channel = NULL;
		switch_caller_profile_t *caller_profile;
		char *rdest;
		int found = 0;
		char interface_name[256];

		DEBUGA_SKYPE("1 SESSION_REQUEST %s\n", SKYPIAX_P_LOG, switch_core_session_get_uuid(*new_session));
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
				//DEBUGA_SKYPE("Finding one available skype interface\n", SKYPIAX_P_LOG);
				//tech_pvt = find_available_skypiax_interface(NULL);
				//if (tech_pvt)
				//found = 1;
				//} else if (strncmp("RR", interface_name, strlen(interface_name)) == 0) {
				/* Find the first idle interface using Round Robin */
				DEBUGA_SKYPE("Finding one available skype interface RR\n", SKYPIAX_P_LOG);
				tech_pvt = find_available_skypiax_interface_rr(NULL);
				if (tech_pvt)
					found = 1;
			}

			for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
				/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
				if (strlen(globals.SKYPIAX_INTERFACES[i].name)
					&& (strncmp(globals.SKYPIAX_INTERFACES[i].name, interface_name, strlen(interface_name)) == 0)) {
					if (strlen(globals.SKYPIAX_INTERFACES[i].session_uuid_str)) {
						DEBUGA_SKYPE
							("globals.SKYPIAX_INTERFACES[%d].name=|||%s||| session_uuid_str=|||%s||| is BUSY\n",
							 SKYPIAX_P_LOG, i, globals.SKYPIAX_INTERFACES[i].name, globals.SKYPIAX_INTERFACES[i].session_uuid_str);
						DEBUGA_SKYPE("1 SESSION_DESTROY %s\n", SKYPIAX_P_LOG, switch_core_session_get_uuid(*new_session));
						switch_core_session_destroy(new_session);
						switch_mutex_unlock(globals.mutex);
						return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
					}

					DEBUGA_SKYPE("globals.SKYPIAX_INTERFACES[%d].name=|||%s|||?\n", SKYPIAX_P_LOG, i, globals.SKYPIAX_INTERFACES[i].name);
					tech_pvt = &globals.SKYPIAX_INTERFACES[i];
					found = 1;
					break;
				}

			}

		} else {
			ERRORA("Doh! no destination number?\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (!found) {
			DEBUGA_SKYPE("Doh! no available interface for |||%s|||?\n", SKYPIAX_P_LOG, interface_name);
			DEBUGA_SKYPE("2 SESSION_DESTROY %s\n", SKYPIAX_P_LOG, switch_core_session_get_uuid(*new_session));
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			//return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			return SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
		}

		channel = switch_core_session_get_channel(*new_session);
		if (!channel) {
			ERRORA("Doh! no channel?\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}
		if (skypiax_tech_init(tech_pvt, *new_session) != SWITCH_STATUS_SUCCESS) {
			ERRORA("Doh! no tech_init?\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}


		if (outbound_profile) {
			char name[128];

			if (strncmp("ANY", outbound_profile->destination_number, 3 ) == 0) {
				snprintf(name, sizeof(name), "skypiax/ANY/%s%s", tech_pvt->name, outbound_profile->destination_number+3);
			} else if (strncmp("RR", outbound_profile->destination_number, 2) == 0) {
				snprintf(name, sizeof(name), "skypiax/RR/%s%s", tech_pvt->name, outbound_profile->destination_number+2);
			} else {
				snprintf(name, sizeof(name), "skypiax/%s", outbound_profile->destination_number);
			}

			//snprintf(name, sizeof(name), "skypiax/%s", outbound_profile->destination_number);
			//snprintf(name, sizeof(name), "skypiax/%s", tech_pvt->name);
			switch_channel_set_name(channel, name);
			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			ERRORA("Doh! no caller profile\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(new_session);
			switch_mutex_unlock(globals.mutex);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		tech_pvt->ob_calls++;

		rdest = strchr(caller_profile->destination_number, '/');
		*rdest++ = '\0';

		//skypiax_call(tech_pvt, rdest, 30);

		switch_copy_string(tech_pvt->session_uuid_str, switch_core_session_get_uuid(*new_session), sizeof(tech_pvt->session_uuid_str));
		caller_profile = tech_pvt->caller_profile;
		caller_profile->destination_number = rdest;

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		skypiax_call(tech_pvt, rdest, 30);
		switch_mutex_unlock(globals.mutex);
		return SWITCH_CAUSE_SUCCESS;
	}

	ERRORA("Doh! no new_session\n", SKYPIAX_P_LOG);
	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
}

/*!
 * \brief This thread runs during a call, and monitor the interface for signaling, like hangup, caller id, etc most of signaling is handled inside the skypiax_signaling_read function
 *
 */
static void *SWITCH_THREAD_FUNC skypiax_signaling_thread_func(switch_thread_t * thread, void *obj)
{
	private_t *tech_pvt = obj;
	int res;
	int forever = 1;

	if (!tech_pvt)
		return NULL;

	DEBUGA_SKYPE("In skypiax_signaling_thread_func: started, p=%p\n", SKYPIAX_P_LOG, (void *) tech_pvt);

	while (forever) {
		if (!(running && tech_pvt->running))
			break;
		res = skypiax_signaling_read(tech_pvt);
		if (res == CALLFLOW_INCOMING_HANGUP) {
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			//private_t *tech_pvt = NULL;
			DEBUGA_SKYPE("skype call ended\n", SKYPIAX_P_LOG);

			if (tech_pvt) {
				session = switch_core_session_locate(tech_pvt->session_uuid_str);
				if (session) {
					channel = switch_core_session_get_channel(session);
					if (channel) {
						switch_channel_state_t state = switch_channel_get_state(channel);
						if (state < CS_EXECUTE) {
							switch_sleep(10000);	//10 msec, let the state evolve from CS_NEW
						}
						switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
					} else {
						ERRORA("no channel?\n", SKYPIAX_P_LOG);
					}
					switch_core_session_rwunlock(session);
				} else {
					DEBUGA_SKYPE("no session\n", SKYPIAX_P_LOG);
				}
				switch_mutex_lock(globals.mutex);
				tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
				*tech_pvt->session_uuid_str = '\0';
				*tech_pvt->skype_call_id = '\0';
				switch_mutex_unlock(globals.mutex);

				//ERRORA("LET'S WAIT\n", SKYPIAX_P_LOG);
				switch_sleep(300000);	//0.3 sec
				//ERRORA("WAIT'S OVER\n", SKYPIAX_P_LOG);
				//tech_pvt->skype_callflow = CALLFLOW_STATUS_FINISHED;
				//switch_sleep(30000);    //0.03 sec
				switch_mutex_lock(globals.mutex);
				tech_pvt->skype_callflow = CALLFLOW_CALL_IDLE;
				tech_pvt->interface_state = SKYPIAX_STATE_IDLE;
				switch_mutex_unlock(globals.mutex);
			} else {
				ERRORA("no tech_pvt?\n", SKYPIAX_P_LOG);
			}
		}
	}
	return NULL;
}

/* BEGIN: Changes heres */
static switch_status_t load_config(int reload_type)
/* END: Changes heres */
{
	char *cf = "skypiax.conf";
	switch_xml_t cfg, xml, global_settings, param, interfaces, myinterface;
	private_t *tech_pvt = NULL;

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, skypiax_module_pool);
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		ERRORA("open of %s failed\n", SKYPIAX_P_LOG, cf);
		running = 0;
		switch_xml_free(xml);
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_lock(globals.mutex);
	if ((global_settings = switch_xml_child(cfg, "global_settings"))) {
		for (param = switch_xml_child(global_settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				DEBUGA_SKYPE("globals.debug=%d\n", SKYPIAX_P_LOG, globals.debug);
				globals.debug = atoi(val);
				DEBUGA_SKYPE("globals.debug=%d\n", SKYPIAX_P_LOG, globals.debug);
			} else if (!strcasecmp(var, "hold-music")) {
				switch_set_string(globals.hold_music, val);
				DEBUGA_SKYPE("globals.hold_music=%s\n", SKYPIAX_P_LOG, globals.hold_music);
			} else if (!strcmp(var, "port")) {
				globals.port = atoi(val);
				DEBUGA_SKYPE("globals.port=%d\n", SKYPIAX_P_LOG, globals.port);
			} else if (!strcmp(var, "codec-master")) {
				if (!strcasecmp(val, "us")) {
					switch_set_flag(&globals, GFLAG_MY_CODEC_PREFS);
				}
				DEBUGA_SKYPE("codec-master globals.debug=%d\n", SKYPIAX_P_LOG, globals.debug);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
				DEBUGA_SKYPE("globals.dialplan=%s\n", SKYPIAX_P_LOG, globals.dialplan);
			} else if (!strcmp(var, "destination")) {
				set_global_destination(val);
				DEBUGA_SKYPE("globals.destination=%s\n", SKYPIAX_P_LOG, globals.destination);
			} else if (!strcmp(var, "context")) {
				set_global_context(val);
				DEBUGA_SKYPE("globals.context=%s\n", SKYPIAX_P_LOG, globals.context);
			} else if (!strcmp(var, "codec-prefs")) {
				set_global_codec_string(val);
				DEBUGA_SKYPE("globals.codec_string=%s\n", SKYPIAX_P_LOG, globals.codec_string);
				globals.codec_order_last = switch_separate_string(globals.codec_string, ',', globals.codec_order, SWITCH_MAX_CODECS);
			} else if (!strcmp(var, "codec-rates")) {
				set_global_codec_rates_string(val);
				DEBUGA_SKYPE("globals.codec_rates_string=%s\n", SKYPIAX_P_LOG, globals.codec_rates_string);
				globals.codec_rates_last = switch_separate_string(globals.codec_rates_string, ',', globals.codec_rates, SWITCH_MAX_CODECS);
			}

		}
	}

	if ((interfaces = switch_xml_child(cfg, "per_interface_settings"))) {
		int i = 0;

		for (myinterface = switch_xml_child(interfaces, "interface"); myinterface; myinterface = myinterface->next) {
			char *id = (char *) switch_xml_attr(myinterface, "id");
			char *name = (char *) switch_xml_attr(myinterface, "name");
			char *context = "default";
			char *dialplan = "XML";
			char *destination = "5000";
			char *tonegroup = NULL;
			char *digit_timeout = NULL;
			char *max_digits = NULL;
			char *hotline = NULL;
			char *dial_regex = NULL;
			char *hold_music = NULL;
			char *fail_dial_regex = NULL;
			char *enable_callerid = "true";
			char *X11_display = NULL;
			char *tcp_cli_port = NULL;
			char *tcp_srv_port = NULL;
			char *skype_user = NULL;

			uint32_t interface_id = 0, to = 0, max = 0;

			tech_pvt = NULL;

			for (param = switch_xml_child(myinterface, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "tonegroup")) {
					tonegroup = val;
				} else if (!strcasecmp(var, "digit_timeout") || !strcasecmp(var, "digit-timeout")) {
					digit_timeout = val;
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else if (!strcasecmp(var, "destination")) {
					destination = val;
				} else if (!strcasecmp(var, "dial-regex")) {
					dial_regex = val;
				} else if (!strcasecmp(var, "enable-callerid")) {
					enable_callerid = val;
				} else if (!strcasecmp(var, "fail-dial-regex")) {
					fail_dial_regex = val;
				} else if (!strcasecmp(var, "hold-music")) {
					hold_music = val;
				} else if (!strcasecmp(var, "skype_user")) {
					skype_user = val;
				} else if (!strcasecmp(var, "tcp_cli_port")) {
					tcp_cli_port = val;
				} else if (!strcasecmp(var, "tcp_srv_port")) {
					tcp_srv_port = val;
				} else if (!strcasecmp(var, "X11-display") || !strcasecmp(var, "X11_display")) {
					X11_display = val;
				} else if (!strcasecmp(var, "max_digits") || !strcasecmp(var, "max-digits")) {
					max_digits = val;
				} else if (!strcasecmp(var, "hotline")) {
					hotline = val;
				}

			}
			if (!skype_user) {
				ERRORA("interface missing REQUIRED param 'skype_user'\n", SKYPIAX_P_LOG);
				continue;
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

			if (!X11_display) {
				ERRORA("interface missing REQUIRED param 'X11_display'\n", SKYPIAX_P_LOG);
				continue;
			}
			if (!tcp_cli_port) {
				ERRORA("interface missing REQUIRED param 'tcp_cli_port'\n", SKYPIAX_P_LOG);
				continue;
			}

			if (!tcp_srv_port) {
				ERRORA("interface missing REQUIRED param 'tcp_srv_port'\n", SKYPIAX_P_LOG);
				continue;
			}
			if (!id) {
				ERRORA("interface missing REQUIRED param 'id'\n", SKYPIAX_P_LOG);
				continue;
			}
			if (switch_is_number(id)) {
				interface_id = atoi(id);
				DEBUGA_SKYPE("interface_id=%d\n", SKYPIAX_P_LOG, interface_id);
			} else {
				ERRORA("interface param 'id' MUST be a number, now id='%s'\n", SKYPIAX_P_LOG, id);
				continue;
			}

			if (!name) {
				WARNINGA("interface missing param 'name', not nice, but works\n", SKYPIAX_P_LOG);
			}

			if (!tonegroup) {
				tonegroup = "us";
			}

			if (digit_timeout) {
				to = atoi(digit_timeout);
			}

			if (max_digits) {
				max = atoi(max_digits);
			}

			if (name) {
				DEBUGA_SKYPE("name=%s\n", SKYPIAX_P_LOG, name);
			}
#ifndef WIN32
			if (!XInitThreads()) {
				ERRORA("Not initialized XInitThreads!\n", SKYPIAX_P_LOG);
			} else {
				DEBUGA_SKYPE("Initialized XInitThreads!\n", SKYPIAX_P_LOG);
			}
			switch_sleep(100);
#endif /* WIN32 */

			if (interface_id && interface_id < SKYPIAX_MAX_INTERFACES) {
				private_t newconf;
				switch_threadattr_t *skypiax_api_thread_attr = NULL;
				switch_threadattr_t *skypiax_signaling_thread_attr = NULL;

				memset(&newconf, '\0', sizeof(newconf));
				globals.SKYPIAX_INTERFACES[interface_id] = newconf;
				globals.SKYPIAX_INTERFACES[interface_id].running = 1;


				tech_pvt = &globals.SKYPIAX_INTERFACES[interface_id];

				switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].interface_id, id);
				if (name) {
					switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].name, name);
				} else {
					switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].name, "N/A");
				}
				DEBUGA_SKYPE("CONFIGURING interface_id=%d\n", SKYPIAX_P_LOG, interface_id);
#ifdef WIN32
				globals.SKYPIAX_INTERFACES[interface_id].tcp_cli_port = (unsigned short) atoi(tcp_cli_port);
				globals.SKYPIAX_INTERFACES[interface_id].tcp_srv_port = (unsigned short) atoi(tcp_srv_port);
#else /* WIN32 */
				globals.SKYPIAX_INTERFACES[interface_id].tcp_cli_port = atoi(tcp_cli_port);
				globals.SKYPIAX_INTERFACES[interface_id].tcp_srv_port = atoi(tcp_srv_port);
#endif /* WIN32 */
				switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].X11_display, X11_display);
				switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].skype_user, skype_user);
				switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].context, context);
				switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].dialplan, dialplan);
				switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].destination, destination);
				switch_set_string(globals.SKYPIAX_INTERFACES[interface_id].context, context);

				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].X11_display=%s\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].X11_display);
				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].skype_user=%s\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].skype_user);
				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].tcp_cli_port=%d\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].tcp_cli_port);
				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].tcp_srv_port=%d\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].tcp_srv_port);
				DEBUGA_SKYPE("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].name=%s\n",
							 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].name);
				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].context=%s\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].context);
				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].dialplan=%s\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].dialplan);
				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].destination=%s\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].destination);
				DEBUGA_SKYPE
					("interface_id=%d globals.SKYPIAX_INTERFACES[interface_id].context=%s\n",
					 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].context);
				WARNINGA("STARTING interface_id=%d\n", SKYPIAX_P_LOG, interface_id);

				switch_threadattr_create(&skypiax_api_thread_attr, skypiax_module_pool);
				switch_threadattr_detach_set(skypiax_api_thread_attr, 1);
				switch_threadattr_stacksize_set(skypiax_api_thread_attr, SWITCH_THREAD_STACKSIZE);
				switch_thread_create(&globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread,
									 skypiax_api_thread_attr, skypiax_do_skypeapi_thread, &globals.SKYPIAX_INTERFACES[interface_id], skypiax_module_pool);

				switch_sleep(100000);

				switch_threadattr_create(&skypiax_signaling_thread_attr, skypiax_module_pool);
				switch_threadattr_detach_set(skypiax_signaling_thread_attr, 1);
				switch_threadattr_stacksize_set(skypiax_signaling_thread_attr, SWITCH_THREAD_STACKSIZE);
				switch_thread_create(&globals.SKYPIAX_INTERFACES[interface_id].
									 skypiax_signaling_thread, skypiax_signaling_thread_attr,
									 skypiax_signaling_thread_func, &globals.SKYPIAX_INTERFACES[interface_id], skypiax_module_pool);

				switch_sleep(100000);

				skypiax_audio_init(&globals.SKYPIAX_INTERFACES[interface_id]);

				NOTICA
					("WAITING roughly 10 seconds to find a running Skype client and connect to its SKYPE API for interface_id=%d\n",
					 SKYPIAX_P_LOG, interface_id);
				i = 0;
				while (globals.SKYPIAX_INTERFACES[interface_id].SkypiaxHandles.api_connected == 0 && running && i < 200) {	// 10 seconds! thanks Jeff Lenk
					switch_sleep(50000);
					i++;
				}
				if (globals.SKYPIAX_INTERFACES[interface_id].SkypiaxHandles.api_connected) {
					NOTICA
						("Found a running Skype client, connected to its SKYPE API for interface_id=%d, waiting 60 seconds for CURRENTUSERHANDLE==%s\n",
						 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].skype_user);
				} else {
					ERRORA
						("Failed to connect to a SKYPE API for interface_id=%d, no SKYPE client running, please (re)start Skype client. Skypiax exiting\n",
						 SKYPIAX_P_LOG, interface_id);
					running = 0;
					switch_mutex_unlock(globals.mutex);
					switch_xml_free(xml);
					return SWITCH_STATUS_FALSE;
				}

				i = 0;
				while (globals.SKYPIAX_INTERFACES[interface_id].SkypiaxHandles.currentuserhandle == 0 && running && i < 1200) {	// 60 seconds! thanks Jeff Lenk
					switch_sleep(50000);
					i++;
				}
				if (globals.SKYPIAX_INTERFACES[interface_id].SkypiaxHandles.currentuserhandle) {
					WARNINGA
						("Interface_id=%d is now STARTED, the Skype client to which we are connected gave us the correct CURRENTUSERHANDLE (%s)\n",
						 SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].skype_user);

					skypiax_signaling_write(&globals.SKYPIAX_INTERFACES[interface_id], "SET AUTOAWAY OFF");
				} else {
					ERRORA
						("The Skype client to which we are connected FAILED to gave us CURRENTUSERHANDLE=%s, interface_id=%d FAILED to start. No Skype client logged in as '%s' has been found. Please (re)launch a Skype client logged in as '%s'. Skypiax exiting now\n",
						 SKYPIAX_P_LOG, globals.SKYPIAX_INTERFACES[interface_id].skype_user,
						 interface_id, globals.SKYPIAX_INTERFACES[interface_id].skype_user, globals.SKYPIAX_INTERFACES[interface_id].skype_user);
					running = 0;
					switch_mutex_unlock(globals.mutex);
					switch_xml_free(xml);
					return SWITCH_STATUS_FALSE;
				}

			} else {
				ERRORA("interface id %d is higher than SKYPIAX_MAX_INTERFACES (%d)\n", SKYPIAX_P_LOG, interface_id, SKYPIAX_MAX_INTERFACES);
				continue;
			}

		}

		for (i = 0; i < SKYPIAX_MAX_INTERFACES; i++) {
			if (strlen(globals.SKYPIAX_INTERFACES[i].name)) {
				/* How many real intterfaces */
				globals.real_interfaces = i + 1;

				tech_pvt = &globals.SKYPIAX_INTERFACES[i];

				DEBUGA_SKYPE("i=%d globals.SKYPIAX_INTERFACES[%d].interface_id=%s\n", SKYPIAX_P_LOG, i, i, globals.SKYPIAX_INTERFACES[i].interface_id);
				DEBUGA_SKYPE("i=%d globals.SKYPIAX_INTERFACES[%d].X11_display=%s\n", SKYPIAX_P_LOG, i, i, globals.SKYPIAX_INTERFACES[i].X11_display);
				DEBUGA_SKYPE("i=%d globals.SKYPIAX_INTERFACES[%d].name=%s\n", SKYPIAX_P_LOG, i, i, globals.SKYPIAX_INTERFACES[i].name);
				DEBUGA_SKYPE("i=%d globals.SKYPIAX_INTERFACES[%d].context=%s\n", SKYPIAX_P_LOG, i, i, globals.SKYPIAX_INTERFACES[i].context);
				DEBUGA_SKYPE("i=%d globals.SKYPIAX_INTERFACES[%d].dialplan=%s\n", SKYPIAX_P_LOG, i, i, globals.SKYPIAX_INTERFACES[i].dialplan);
				DEBUGA_SKYPE("i=%d globals.SKYPIAX_INTERFACES[%d].destination=%s\n", SKYPIAX_P_LOG, i, i, globals.SKYPIAX_INTERFACES[i].destination);
				DEBUGA_SKYPE("i=%d globals.SKYPIAX_INTERFACES[%d].context=%s\n", SKYPIAX_P_LOG, i, i, globals.SKYPIAX_INTERFACES[i].context);
			}
		}
	}

	switch_mutex_unlock(globals.mutex);
	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}
static switch_status_t chat_send(const char *proto, const char *from, const char *to, const char *subject, const char *body, const char *type, const char *hint)
{
	//char *user, *host, *f_user = NULL, *ffrom = NULL, *f_host = NULL, *f_resource = NULL;
	char *user, *host, *f_user = NULL, *f_host = NULL, *f_resource = NULL;
	//mdl_profile_t *profile = NULL;
	private_t * tech_pvt=NULL;
	int i=0, found=0, tried=0;
	char skype_msg[1024];

	switch_assert(proto != NULL);

	DEBUGA_SKYPE("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, type=%s, hint=%s)\n", SKYPIAX_P_LOG, proto, from, to, subject, body, type, hint?hint:"NULL");

	if (!to || !strlen(to)) {
		ERRORA("Missing To: header.\n", SKYPIAX_P_LOG);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((!from && !hint) || (!strlen(from) && !strlen(hint)) ) {
		ERRORA("Missing From: AND Hint: headers.\n", SKYPIAX_P_LOG);
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

	if (to && (user = strdup(to))) {
		if ((host = strchr(user, '@'))) {
			*host++ = '\0';
		}

		//if (!strcmp(proto, MDL_CHAT_PROTO)) {

	DEBUGA_SKYPE("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, type=%s, hint=%s)\n", SKYPIAX_P_LOG, proto, from, to, subject, body, type, hint?hint:"NULL");
			if (hint && strlen(hint)) {
				//in hint we receive the interface name to use
				for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
					if (strlen(globals.SKYPIAX_INTERFACES[i].name)
							&& (strncmp(globals.SKYPIAX_INTERFACES[i].name, hint, strlen(hint)) == 0)) {
						tech_pvt = &globals.SKYPIAX_INTERFACES[i];
						DEBUGA_SKYPE("Using interface: globals.SKYPIAX_INTERFACES[%d].name=|||%s|||\n", SKYPIAX_P_LOG, i, globals.SKYPIAX_INTERFACES[i].name);
						found = 1;
						break;
					}
				}
			} else {
				//we have no a predefined interface name to use (hint is NULL), so let's choose an interface from the username (from)
				for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
					if (strlen(globals.SKYPIAX_INTERFACES[i].name)
							&& (strncmp(globals.SKYPIAX_INTERFACES[i].skype_user, from, strlen(from)) == 0)) {
						tech_pvt = &globals.SKYPIAX_INTERFACES[i];
						DEBUGA_SKYPE("Using interface: globals.SKYPIAX_INTERFACES[%d].name=|||%s|||\n", SKYPIAX_P_LOG, i, globals.SKYPIAX_INTERFACES[i].name);
						found = 1;
						break;
					}
				}
			}
			if (!found) {
				ERRORA("ERROR: A Skypiax interface with name='%s' or one with skypeuser='%s' was not found\n", SKYPIAX_P_LOG, hint?hint:"NULL", from?from:"NULL");
				goto end;
			} else {

				snprintf(skype_msg, sizeof(skype_msg), "CHAT CREATE %s", to);
				skypiax_signaling_write(tech_pvt, skype_msg);
				switch_sleep(100);
			}
		//} else {
			//FIXME don't know how to do here, let's hope this is correct
			//char *p;
			//ffrom = switch_mprintf("%s+%s", proto, from);
			//from = ffrom;
			//if ((p = strchr(from, '/'))) {
				//*p = '\0';
			//}
	//NOTICA("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, type=%s, hint=%s)\n", SKYPIAX_P_LOG, proto, from, to, subject, body, type, hint?hint:"NULL");
			//switch_core_chat_send(proto, proto, from, to, subject, body, type, hint);
			//return SWITCH_STATUS_SUCCESS;
		//}

		found=0;

		while(!found){
			for(i=0; i<MAX_CHATS; i++){
				if(!strcmp(tech_pvt->chats[i].dialog_partner, to) ){
					snprintf(skype_msg, sizeof(skype_msg), "CHATMESSAGE %s %s", tech_pvt->chats[i].chatname, body);
					skypiax_signaling_write(tech_pvt, skype_msg);
					found=1;
					break;
				}
			}
			if(found){
				break;
			}
			if(tried > 1000){
				ERRORA("No chat with dialog_partner='%s' was found\n", SKYPIAX_P_LOG, to);
				break;
			}
			switch_sleep(1000);
		}

	}
end:
	switch_safe_free(user);
	switch_safe_free(f_user);
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_skypiax_load)
{
	switch_api_interface_t *commands_api_interface;
	switch_chat_interface_t *chat_interface;

	skypiax_module_pool = pool;
	memset(&globals, '\0', sizeof(globals));

	running = 1;

	if (load_config(FULL_RELOAD) != SWITCH_STATUS_SUCCESS) {
		running = 0;
		return SWITCH_STATUS_FALSE;
	}

        if (switch_event_reserve_subclass(MY_EVENT_INCOMING_CHATMESSAGE) != SWITCH_STATUS_SUCCESS) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
                return SWITCH_STATUS_GENERR;
        }

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	skypiax_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	skypiax_endpoint_interface->interface_name = "skypiax";
	skypiax_endpoint_interface->io_routines = &skypiax_io_routines;
	skypiax_endpoint_interface->state_handler = &skypiax_state_handlers;

	if (running) {

		SWITCH_ADD_API(commands_api_interface, "sk", "Skypiax console commands", sk_function, SK_SYNTAX);
		SWITCH_ADD_API(commands_api_interface, "skypiax", "Skypiax interface commands", skypiax_function, SKYPIAX_SYNTAX);
		SWITCH_ADD_API(commands_api_interface, "skypiax_chat", "Skypiax_chat interface remote_skypename TEXT", skypiax_chat_function, SKYPIAX_CHAT_SYNTAX);
		SWITCH_ADD_CHAT(chat_interface, MDL_CHAT_PROTO, chat_send);


		/* indicate that the module should continue to be loaded */
		return SWITCH_STATUS_SUCCESS;
	} else
		return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_skypiax_shutdown)
{
	int x;
	private_t *tech_pvt = NULL;
	switch_status_t status;
	unsigned int howmany = 8;
	int interface_id;

	running = 0;

	for (interface_id = 0; interface_id < SKYPIAX_MAX_INTERFACES; interface_id++) {
		tech_pvt = &globals.SKYPIAX_INTERFACES[interface_id];

		if (strlen(globals.SKYPIAX_INTERFACES[interface_id].name)) {
			if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_signaling_thread) {
#ifdef WIN32
				switch_file_write(tech_pvt->SkypiaxHandles.fdesc[1], "sciutati", &howmany);	// let's the controldev_thread die
#else /* WIN32 */
				howmany = write(tech_pvt->SkypiaxHandles.fdesc[1], "sciutati", howmany);
#endif /* WIN32 */
			}

			if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread) {
#ifdef WIN32
				if (SendMessage(tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle, WM_DESTROY, 0, 0) == FALSE) {	// let's the skypiax_api_thread_func die
					DEBUGA_SKYPE("got FALSE here, thread probably was already dead. GetLastError returned: %d\n", SKYPIAX_P_LOG, GetLastError());
					globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread = NULL;
				}
#else
				if(tech_pvt->SkypiaxHandles.disp){
					XEvent e;
					Atom atom1 = XInternAtom(tech_pvt->SkypiaxHandles.disp, "SKYPECONTROLAPI_MESSAGE_BEGIN",
							False);
					memset(&e, 0, sizeof(e));
					e.xclient.type = ClientMessage;
					e.xclient.message_type = atom1;	/*  leading message */
					e.xclient.display = tech_pvt->SkypiaxHandles.disp;
					e.xclient.window = tech_pvt->SkypiaxHandles.skype_win;
					e.xclient.format = 8;

					XSendEvent(tech_pvt->SkypiaxHandles.disp, tech_pvt->SkypiaxHandles.win, False, 0, &e);
					XSync(tech_pvt->SkypiaxHandles.disp, False);
				}
#endif
			}
			x = 10;
			while (x) {			//FIXME 0.5 seconds?
				x--;
				switch_yield(50000);
			}
			if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_signaling_thread) {
				switch_thread_join(&status, globals.SKYPIAX_INTERFACES[interface_id].skypiax_signaling_thread);
			}
			if (globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread) {
				switch_thread_join(&status, globals.SKYPIAX_INTERFACES[interface_id].skypiax_api_thread);
			}
#ifndef WIN32
			WARNINGA("SHUTDOWN interface_id=%d\n", SKYPIAX_P_LOG, interface_id);
			shutdown(tech_pvt->audioskypepipe[0], 2);
			close(tech_pvt->audioskypepipe[0]);
			shutdown(tech_pvt->audioskypepipe[1], 2);
			close(tech_pvt->audioskypepipe[1]);
			shutdown(tech_pvt->audiopipe[0], 2);
			close(tech_pvt->audiopipe[0]);
			shutdown(tech_pvt->audiopipe[1], 2);
			close(tech_pvt->audiopipe[1]);
			shutdown(tech_pvt->SkypiaxHandles.fdesc[0], 2);
			close(tech_pvt->SkypiaxHandles.fdesc[0]);
			shutdown(tech_pvt->SkypiaxHandles.fdesc[1], 2);
			close(tech_pvt->SkypiaxHandles.fdesc[1]);
#endif /* WIN32 */
		}

	}
        switch_event_free_subclass(MY_EVENT_INCOMING_CHATMESSAGE);

	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.context);
	switch_safe_free(globals.destination);
	switch_safe_free(globals.codec_string);
	switch_safe_free(globals.codec_rates_string);

	return SWITCH_STATUS_SUCCESS;
}

void *SWITCH_THREAD_FUNC skypiax_do_tcp_srv_thread(switch_thread_t * thread, void *obj)
{
	return skypiax_do_tcp_srv_thread_func(obj);
}

void *SWITCH_THREAD_FUNC skypiax_do_tcp_cli_thread(switch_thread_t * thread, void *obj)
{
	return skypiax_do_tcp_cli_thread_func(obj);
}

void *SWITCH_THREAD_FUNC skypiax_do_skypeapi_thread(switch_thread_t * thread, void *obj)
{
	return skypiax_do_skypeapi_thread_func(obj);
}

int dtmf_received(private_t * tech_pvt, char *value)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	channel = switch_core_session_get_channel(session);

	if (channel) {

		if (!switch_channel_test_flag(channel, CF_BRIDGED)) {

			switch_dtmf_t dtmf = { (char) value[0], switch_core_default_dtmf_duration(0) };
			DEBUGA_SKYPE("received DTMF %c on channel %s\n", SKYPIAX_P_LOG, dtmf.digit, switch_channel_get_name(channel));
			switch_mutex_lock(tech_pvt->flag_mutex);
			//FIXME: why sometimes DTMFs from here do not seems to be get by FS?
			switch_channel_queue_dtmf(channel, &dtmf);
			switch_set_flag(tech_pvt, TFLAG_DTMF);
			switch_mutex_unlock(tech_pvt->flag_mutex);
		} else {
			NOTICA
				("received a DTMF on channel %s, but we're BRIDGED, so let's NOT relay it out of band\n", SKYPIAX_P_LOG, switch_channel_get_name(channel));
		}
	} else {
		WARNINGA("received %c DTMF, but no channel?\n", SKYPIAX_P_LOG, value[0]);
	}
	switch_core_session_rwunlock(session);

	return 0;
}

int start_audio_threads(private_t * tech_pvt)
{
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, skypiax_module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (switch_thread_create(&tech_pvt->tcp_srv_thread, thd_attr, skypiax_do_tcp_srv_thread, tech_pvt, skypiax_module_pool) == SWITCH_STATUS_SUCCESS) {
		DEBUGA_SKYPE("started tcp_srv_thread thread.\n", SKYPIAX_P_LOG);
	} else {
		ERRORA("failed to start tcp_srv_thread thread.\n", SKYPIAX_P_LOG);
		return -1;
	}

	switch_threadattr_create(&thd_attr, skypiax_module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (switch_thread_create(&tech_pvt->tcp_cli_thread, thd_attr, skypiax_do_tcp_cli_thread, tech_pvt, skypiax_module_pool) == SWITCH_STATUS_SUCCESS) {
		DEBUGA_SKYPE("started tcp_cli_thread thread.\n", SKYPIAX_P_LOG);
	} else {
		ERRORA("failed to start tcp_cli_thread thread.\n", SKYPIAX_P_LOG);
		return -1;
	}
	switch_sleep(100000);

	if (tech_pvt->tcp_cli_thread == NULL || tech_pvt->tcp_srv_thread == NULL) {
		ERRORA("tcp_cli_thread or tcp_srv_thread exited\n", SKYPIAX_P_LOG);
		return -1;
	}

	return 0;
}

int new_inbound_channel(private_t * tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if ((session = switch_core_session_request(skypiax_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, NULL)) != 0) {
		DEBUGA_SKYPE("2 SESSION_REQUEST %s\n", SKYPIAX_P_LOG, switch_core_session_get_uuid(session));
		switch_core_session_add_stream(session, NULL);
		channel = switch_core_session_get_channel(session);
		if (!channel) {
			ERRORA("Doh! no channel?\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(&session);
			return 0;
		}
		if (skypiax_tech_init(tech_pvt, session) != SWITCH_STATUS_SUCCESS) {
			ERRORA("Doh! no tech_init?\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(&session);
			return 0;
		}

		if ((tech_pvt->caller_profile =
			 switch_caller_profile_new(switch_core_session_get_pool(session), "skypiax",
									   tech_pvt->dialplan, tech_pvt->callid_name,
									   tech_pvt->callid_number, NULL, NULL, NULL, NULL, "mod_skypiax", tech_pvt->context, tech_pvt->destination)) != 0) {
			char name[128];
			//switch_snprintf(name, sizeof(name), "skypiax/%s/%s", tech_pvt->name, tech_pvt->caller_profile->destination_number);
			switch_snprintf(name, sizeof(name), "skypiax/%s", tech_pvt->name);
			switch_channel_set_name(channel, name);
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		switch_channel_set_state(channel, CS_INIT);
		if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
			ERRORA("Error spawning thread\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(&session);
			return 0;
		}
	}
	if (channel) {
		switch_channel_mark_answered(channel);
	}

	DEBUGA_SKYPE("new_inbound_channel\n", SKYPIAX_P_LOG);

	return 0;
}

int remote_party_is_ringing(private_t * tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	} else {
		ERRORA("No session???\n", SKYPIAX_P_LOG);
		goto done;
	}
	if (session) {
		channel = switch_core_session_get_channel(session);
	} else {
		ERRORA("No session???\n", SKYPIAX_P_LOG);
		goto done;
	}
	if (channel) {
		switch_channel_mark_ring_ready(channel);
		DEBUGA_SKYPE("skype_call: REMOTE PARTY RINGING\n", SKYPIAX_P_LOG);
	} else {
		ERRORA("No channel???\n", SKYPIAX_P_LOG);
	}

	switch_core_session_rwunlock(session);

  done:
	return 0;
}

int remote_party_is_early_media(private_t * tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	} else {
		ERRORA("No session???\n\n\n", SKYPIAX_P_LOG);
		//TODO: kill the bastard
		goto done;
	}
	if (session) {
		channel = switch_core_session_get_channel(session);
		switch_core_session_add_stream(session, NULL);
	} else {
		ERRORA("No session???\n", SKYPIAX_P_LOG);
		//TODO: kill the bastard
		goto done;
	}
	if (channel) {
		switch_channel_mark_pre_answered(channel);
		DEBUGA_SKYPE("skype_call: REMOTE PARTY EARLY MEDIA\n", SKYPIAX_P_LOG);
	} else {
		ERRORA("No channel???\n", SKYPIAX_P_LOG);
		//TODO: kill the bastard
	}

	switch_core_session_rwunlock(session);

  done:
	return 0;
}

int outbound_channel_answered(private_t * tech_pvt)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	} else {
		ERRORA("No session???\n", SKYPIAX_P_LOG);
		goto done;
	}
	if (session) {
		channel = switch_core_session_get_channel(session);
	} else {
		ERRORA("No channel???\n", SKYPIAX_P_LOG);
		goto done;
	}
	if (channel) {
		switch_channel_mark_answered(channel);
		//DEBUGA_SKYPE("skype_call: %s, answered\n", SKYPIAX_P_LOG, id);
	} else {
		ERRORA("No channel???\n", SKYPIAX_P_LOG);
	}

	switch_core_session_rwunlock(session);

  done:
	DEBUGA_SKYPE("outbound_channel_answered!\n", SKYPIAX_P_LOG);

	return 0;
}

private_t *find_available_skypiax_interface_rr(private_t * tech_pvt_calling)
{
	private_t *tech_pvt = NULL;
	int i;
	//int num_interfaces = SKYPIAX_MAX_INTERFACES; 
	//int num_interfaces = globals.real_interfaces;

	switch_mutex_lock(globals.mutex);

	/* Fact is the real interface start from 1 */
	//XXX no, is just a convention, but you can have it start from 0. I do not, for aestetic reasons :-)  
	//if (globals.next_interface == 0) globals.next_interface = 1;

	for (i = 0; i < SKYPIAX_MAX_INTERFACES; i++) {
		int interface_id;

		interface_id = globals.next_interface;
		//interface_id = interface_id < SKYPIAX_MAX_INTERFACES ? interface_id : interface_id - SKYPIAX_MAX_INTERFACES + 1;
		globals.next_interface = interface_id + 1 < SKYPIAX_MAX_INTERFACES ? interface_id + 1 : 0;

		if (strlen(globals.SKYPIAX_INTERFACES[interface_id].name)) {
			int skype_state = 0;

			tech_pvt = &globals.SKYPIAX_INTERFACES[interface_id];
			skype_state = tech_pvt->interface_state;
			//DEBUGA_SKYPE("skype interface: %d, name: %s, state: %d\n", SKYPIAX_P_LOG, interface_id, globals.SKYPIAX_INTERFACES[interface_id].name, skype_state);
			if ((tech_pvt_calling ? strcmp(tech_pvt->skype_user, tech_pvt_calling->skype_user) : 1)
				&& (SKYPIAX_STATE_DOWN == skype_state || 0 == skype_state) && (tech_pvt->skype_callflow == CALLFLOW_STATUS_FINISHED
																			   || 0 == tech_pvt->skype_callflow)) {
				DEBUGA_SKYPE("returning as available skype interface name: %s, state: %d callflow: %d\n", SKYPIAX_P_LOG, tech_pvt->name, skype_state,
							 tech_pvt->skype_callflow);
				/*set to Dialing state to avoid other thread fint it, don't know if it is safe */
				//XXX no, it's not safe 
				if (tech_pvt_calling == NULL) {
					tech_pvt->interface_state = SKYPIAX_STATE_SELECTED;
				}

				switch_mutex_unlock(globals.mutex);
				return tech_pvt;
			}
		}						// else {
		//DEBUGA_SKYPE("Skype interface: %d blank!! A hole here means we cannot hunt the last interface.\n", SKYPIAX_P_LOG, interface_id);
		//}
	}

	switch_mutex_unlock(globals.mutex);
	return NULL;
}

SWITCH_STANDARD_API(sk_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (globals.sk_console)
		stream->write_function(stream, "sk console is: |||%s|||\n", globals.sk_console->name);
	else
		stream->write_function(stream, "sk console is NOT yet assigned\n");

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc || !argv[0]) {
		stream->write_function(stream, "%s", SK_SYNTAX);
		goto end;
	}

	if (!strcasecmp(argv[0], "list")) {
		int i;
		int ib = 0;
		int ib_failed = 0;
		int ob = 0;
		int ob_failed = 0;
		char next_flag_char = ' ';

		stream->write_function(stream, "F ID\t    Name    \tIB (F/T)    OB (F/T)\tState\tCallFlw\t\tUUID\n");
		stream->write_function(stream, "= ====\t  ========  \t=======     =======\t======\t============\t======\n");

		for (i = 0; i < SKYPIAX_MAX_INTERFACES; i++) {
			next_flag_char = i == globals.next_interface ? '*' : ' ';
			ib += globals.SKYPIAX_INTERFACES[i].ib_calls;
			ib_failed += globals.SKYPIAX_INTERFACES[i].ib_failed_calls;
			ob += globals.SKYPIAX_INTERFACES[i].ob_calls;
			ob_failed += globals.SKYPIAX_INTERFACES[i].ob_failed_calls;

			if (strlen(globals.SKYPIAX_INTERFACES[i].name)) {
				stream->write_function(stream,
									   "%c %d\t[%s]\t%3u/%u\t%6u/%u\t%s\t%s\t%s\n",
									   next_flag_char,
									   i, globals.SKYPIAX_INTERFACES[i].name,
									   globals.SKYPIAX_INTERFACES[i].ib_failed_calls,
									   globals.SKYPIAX_INTERFACES[i].ib_calls,
									   globals.SKYPIAX_INTERFACES[i].ob_failed_calls,
									   globals.SKYPIAX_INTERFACES[i].ob_calls,
									   interface_status[globals.SKYPIAX_INTERFACES[i].interface_state],
									   skype_callflow[globals.SKYPIAX_INTERFACES[i].skype_callflow], globals.SKYPIAX_INTERFACES[i].session_uuid_str);
			} else if (argc > 1 && !strcasecmp(argv[1], "full")) {
				stream->write_function(stream, "%c\t%d\n", next_flag_char, i);
			}

		}
		stream->write_function(stream, "\nTotal Interfaces: %d  IB Calls(Failed/Total): %ld/%ld  OB Calls(Failed/Total): %ld/%ld\n",
		       globals.real_interfaces > 0 ? globals.real_interfaces - 1 : 0, ib_failed, ib, ob_failed, ob);

	} else if (!strcasecmp(argv[0], "console")) {
		int i;
		int found = 0;

		if (argc == 2) {
			for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
				/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
				if (strlen(globals.SKYPIAX_INTERFACES[i].name)
					&& (strncmp(globals.SKYPIAX_INTERFACES[i].name, argv[1], strlen(argv[1])) == 0)) {
					globals.sk_console = &globals.SKYPIAX_INTERFACES[i];
					stream->write_function(stream,
										   "sk console is now: globals.SKYPIAX_INTERFACES[%d].name=|||%s|||\n", i, globals.SKYPIAX_INTERFACES[i].name);
					stream->write_function(stream, "sk console is: |||%s|||\n", globals.sk_console->name);
					found = 1;
					break;
				}

			}
			if (!found)
				stream->write_function(stream, "ERROR: A Skypiax interface with name='%s' was not found\n", argv[1]);
		} else {

			stream->write_function(stream, "-ERR Usage: sk console interface_name\n");
			goto end;
		}

	} else if (!strcasecmp(argv[0], "ciapalino")) {

/* BEGIN: Changes heres */
	} else if (!strcasecmp(argv[0], "reload")) {
		if (load_config(SOFT_RELOAD) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "sk reload failed\n");
		} else {
			stream->write_function(stream, "sk reload success\n");
		}
	} else if (!strcasecmp(argv[0], "remove")) {
		if (argc == 2) {
			if (remove_interface(argv[1]) == SWITCH_STATUS_SUCCESS) {
				if (interface_exists(argv[1]) == SWITCH_STATUS_SUCCESS) {
					stream->write_function(stream, "sk remove '%s' failed\n", argv[1]);
				} else {
					stream->write_function(stream, "sk remove '%s' success\n", argv[1]);
				}
			}
		} else {
			stream->write_function(stream, "-ERR Usage: sk remove interface_name\n");
			goto end;
		}
/* END: Changes heres */

	} else {
		if (globals.sk_console)
			skypiax_signaling_write(globals.sk_console, (char *) cmd);
		else
			stream->write_function(stream, "sk console is NOT yet assigned\n");
	}
  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(skypiax_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	private_t *tech_pvt = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc) {
		stream->write_function(stream, "ERROR, usage: %s", SKYPIAX_SYNTAX);
		goto end;
	}

	if (argc < 2) {
		stream->write_function(stream, "ERROR, usage: %s", SKYPIAX_SYNTAX);
		goto end;
	}

	if (argv[0]) {
		int i;
		int found = 0;

		for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
			/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
			if (strlen(globals.SKYPIAX_INTERFACES[i].name)
				&& (strncmp(globals.SKYPIAX_INTERFACES[i].name, argv[0], strlen(argv[0])) == 0)) {
				tech_pvt = &globals.SKYPIAX_INTERFACES[i];
				stream->write_function(stream, "Using interface: globals.SKYPIAX_INTERFACES[%d].name=|||%s|||\n", i, globals.SKYPIAX_INTERFACES[i].name);
				found = 1;
				break;
			}

		}
		if (!found) {
			stream->write_function(stream, "ERROR: A Skypiax interface with name='%s' was not found\n", argv[0]);
			switch_safe_free(mycmd);

			return SWITCH_STATUS_SUCCESS;
		} else {
			skypiax_signaling_write(tech_pvt, (char *) &cmd[strlen(argv[0]) + 1]);
		}
	} else {
		stream->write_function(stream, "ERROR, usage: %s", SKYPIAX_SYNTAX);
	}
  end:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

int skypiax_answer(private_t * tech_pvt, char *id, char *value)
{
	char msg_to_skype[1024];
	int i;
	int found = 0;
	private_t *giovatech;
	struct timeval timenow;

	switch_mutex_lock(globals.mutex);

	gettimeofday(&timenow, NULL);
	for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
		if (strlen(globals.SKYPIAX_INTERFACES[i].name)) {

			giovatech = &globals.SKYPIAX_INTERFACES[i];
			//NOTICA("interface=%d, name=%s, giovatech->skype_call_id=%s, giovatech->interface_state=%d, giovatech->skype_user=%s, tech_pvt->skype_user=%s, giovatech->callid_number=%s, value=%s, delta=%ld  500000\n", SKYPIAX_P_LOG, i, giovatech->name, giovatech->skype_call_id, giovatech->interface_state, giovatech->skype_user, tech_pvt->skype_user, giovatech->callid_number, value, (((timenow.tv_sec - giovatech->answer_time.tv_sec) * 1000000) + (timenow.tv_usec - giovatech->answer_time.tv_usec)) );
			if (strlen(giovatech->skype_call_id) && (giovatech->interface_state != SKYPIAX_STATE_DOWN) && (!strcmp(giovatech->skype_user, tech_pvt->skype_user)) && (!strcmp(giovatech->callid_number, value)) && ((((timenow.tv_sec - giovatech->answer_time.tv_sec) * 1000000) + (timenow.tv_usec - giovatech->answer_time.tv_usec)) < 1000000)) {	//XXX 1.5sec - can have a max of 1 call coming from the same skypename to the same skypename each 1.5 seconds
				found = 1;
				DEBUGA_SKYPE
					("FOUND  (name=%s, giovatech->interface_state=%d != SKYPIAX_STATE_DOWN) && (giovatech->skype_user=%s == tech_pvt->skype_user=%s) && (giovatech->callid_number=%s == value=%s)\n",
					 SKYPIAX_P_LOG, giovatech->name, giovatech->interface_state,
					 giovatech->skype_user, tech_pvt->skype_user, giovatech->callid_number, value)
					if (tech_pvt->interface_state == SKYPIAX_STATE_PRERING) {
					tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
				} else if (tech_pvt->interface_state != 0 && tech_pvt->interface_state != SKYPIAX_STATE_DOWN) {
					WARNINGA("Why an interface_state %d HERE?\n", SKYPIAX_P_LOG, tech_pvt->interface_state);
					tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
				}

				break;
			}
		}
	}

	if (found) {
		//tech_pvt->callid_number[0]='\0';
		//sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
		//skypiax_signaling_write(tech_pvt, msg_to_skype);
		switch_mutex_unlock(globals.mutex);
		return 0;
	}
	DEBUGA_SKYPE("NOT FOUND\n", SKYPIAX_P_LOG);

	if (tech_pvt && tech_pvt->skype_call_id && !strlen(tech_pvt->skype_call_id)) {
		/* we are not inside an active call */

		tech_pvt->ib_calls++;

		sprintf(msg_to_skype, "GET CALL %s PARTNER_DISPNAME", id);
		skypiax_signaling_write(tech_pvt, msg_to_skype);
		switch_sleep(10000);
		tech_pvt->interface_state = SKYPIAX_STATE_PREANSWER;
		sprintf(msg_to_skype, "ALTER CALL %s ANSWER", id);
		skypiax_signaling_write(tech_pvt, msg_to_skype);
		DEBUGA_SKYPE("We answered a Skype RING on skype_call %s\n", SKYPIAX_P_LOG, id);
		//FIXME write a timestamp here
		gettimeofday(&tech_pvt->answer_time, NULL);
		switch_copy_string(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);

		switch_copy_string(tech_pvt->callid_number, value, sizeof(tech_pvt->callid_number) - 1);

		DEBUGA_SKYPE
			("NEW!  name: %s, state: %d, value=%s, tech_pvt->callid_number=%s, tech_pvt->skype_user=%s\n",
			 SKYPIAX_P_LOG, tech_pvt->name, tech_pvt->interface_state, value, tech_pvt->callid_number, tech_pvt->skype_user);
	} else if (!tech_pvt || !tech_pvt->skype_call_id) {
		ERRORA("No Call ID?\n", SKYPIAX_P_LOG);
	} else {
		DEBUGA_SKYPE("We're in a call now (%s), let's refuse this one (%s)\n", SKYPIAX_P_LOG, tech_pvt->skype_call_id, id);
		sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
		skypiax_signaling_write(tech_pvt, msg_to_skype);
	}

	switch_mutex_unlock(globals.mutex);
	return 0;
}
int skypiax_transfer(private_t * tech_pvt, char *id, char *value)
{
	char msg_to_skype[1024];
	int i;
	int found = 0;
	private_t *giovatech;
	struct timeval timenow;

	switch_mutex_lock(globals.mutex);

	gettimeofday(&timenow, NULL);
	for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
		if (strlen(globals.SKYPIAX_INTERFACES[i].name)) {

			giovatech = &globals.SKYPIAX_INTERFACES[i];
			//NOTICA("skype interface: %d, name: %s, state: %d, value=%s, giovatech->callid_number=%s, giovatech->skype_user=%s\n", SKYPIAX_P_LOG, i, giovatech->name, giovatech->interface_state, value, giovatech->callid_number, giovatech->skype_user);
			//FIXME check a timestamp here
			if (strlen(giovatech->skype_call_id) && (giovatech->interface_state != SKYPIAX_STATE_DOWN) && (!strcmp(giovatech->skype_user, tech_pvt->skype_user)) && (!strcmp(giovatech->callid_number, value)) && ((((timenow.tv_sec - giovatech->answer_time.tv_sec) * 1000000) + (timenow.tv_usec - giovatech->answer_time.tv_usec)) < 500000)) {	//0.5sec
				found = 1;
				DEBUGA_SKYPE
					("FOUND  (name=%s, giovatech->interface_state=%d != SKYPIAX_STATE_DOWN) && (giovatech->skype_user=%s == tech_pvt->skype_user=%s) && (giovatech->callid_number=%s == value=%s)\n",
					 SKYPIAX_P_LOG, giovatech->name, giovatech->interface_state,
					 giovatech->skype_user, tech_pvt->skype_user, giovatech->callid_number, value)
					break;
			}
		}
	}

	if (found) {
		//tech_pvt->callid_number[0]='\0';
		//sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
		//skypiax_signaling_write(tech_pvt, msg_to_skype);
		switch_mutex_unlock(globals.mutex);
		return 0;
	}
	DEBUGA_SKYPE("NOT FOUND\n", SKYPIAX_P_LOG);

	if (!tech_pvt || !tech_pvt->skype_call_id || !strlen(tech_pvt->skype_call_id)) {
		/* we are not inside an active call */
		DEBUGA_SKYPE("We're NO MORE in a call now %s\n", SKYPIAX_P_LOG, (tech_pvt && tech_pvt->skype_call_id) ? tech_pvt->skype_call_id : "");
		switch_mutex_unlock(globals.mutex);

	} else {

		/* we're owned, we're in a call, let's try to transfer */
		/************************** TODO
		  Checking here if it is possible to transfer this call to Test2
		  -> GET CALL 288 CAN_TRANSFER Test2
		  <- CALL 288 CAN_TRANSFER test2 TRUE
		 **********************************/

		private_t *available_skypiax_interface = NULL;

		gettimeofday(&timenow, NULL);
		for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
			if (strlen(globals.SKYPIAX_INTERFACES[i].name)) {

				giovatech = &globals.SKYPIAX_INTERFACES[i];
				//NOTICA("skype interface: %d, name: %s, state: %d, value=%s, giovatech->callid_number=%s, giovatech->skype_user=%s\n", SKYPIAX_P_LOG, i, giovatech->name, giovatech->interface_state, value, giovatech->callid_number, giovatech->skype_user);
				//FIXME check a timestamp here
				if (strlen(giovatech->skype_transfer_call_id) && (giovatech->interface_state != SKYPIAX_STATE_DOWN) && (!strcmp(giovatech->skype_user, tech_pvt->skype_user)) && (!strcmp(giovatech->transfer_callid_number, value)) && ((((timenow.tv_sec - giovatech->transfer_time.tv_sec) * 1000000) + (timenow.tv_usec - giovatech->transfer_time.tv_usec)) < 1000000)) {	//1.0 sec
					found = 1;
					DEBUGA_SKYPE
						("FOUND  (name=%s, giovatech->interface_state=%d != SKYPIAX_STATE_DOWN) && (giovatech->skype_user=%s == tech_pvt->skype_user=%s) && (giovatech->transfer_callid_number=%s == value=%s)\n",
						 SKYPIAX_P_LOG, giovatech->name, giovatech->interface_state,
						 giovatech->skype_user, tech_pvt->skype_user, giovatech->transfer_callid_number, value)
						break;
				}
			}
		}

		if (found) {
			//tech_pvt->callid_number[0]='\0';
			//sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
			//skypiax_signaling_write(tech_pvt, msg_to_skype);
			switch_mutex_unlock(globals.mutex);
			return 0;
		}
		DEBUGA_SKYPE("NOT FOUND\n", SKYPIAX_P_LOG);

		available_skypiax_interface = find_available_skypiax_interface_rr(tech_pvt);
		if (available_skypiax_interface) {
			/* there is a skypiax interface idle, let's transfer the call to it */

			//FIXME write a timestamp here
			gettimeofday(&tech_pvt->transfer_time, NULL);
			switch_copy_string(tech_pvt->skype_transfer_call_id, id, sizeof(tech_pvt->skype_transfer_call_id) - 1);

			switch_copy_string(tech_pvt->transfer_callid_number, value, sizeof(tech_pvt->transfer_callid_number) - 1);

			DEBUGA_SKYPE
				("Let's transfer the skype_call %s to %s interface (with skype_user: %s), because we are already in a skypiax call(%s)\n",
				 SKYPIAX_P_LOG, tech_pvt->skype_call_id, available_skypiax_interface->name, available_skypiax_interface->skype_user, id);

			//FIXME why this? the inbound call will come, eventually, on that other interface
			//available_skypiax_interface->ib_calls++;

			sprintf(msg_to_skype, "ALTER CALL %s TRANSFER %s", id, available_skypiax_interface->skype_user);
			skypiax_signaling_write(tech_pvt, msg_to_skype);
			if (tech_pvt->interface_state == SKYPIAX_STATE_SELECTED) {
				tech_pvt->interface_state = SKYPIAX_STATE_IDLE;	//we marked it SKYPIAX_STATE_SELECTED just in case it has to make an outbound call
			}
		} else {
			/* no skypiax interfaces idle, do nothing */
			DEBUGA_SKYPE
				("Not answering the skype_call %s, because we are already in a skypiax call(%s) and not transferring, because no other skypiax interfaces are available\n",
				 SKYPIAX_P_LOG, id, tech_pvt->skype_call_id);
			sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
			skypiax_signaling_write(tech_pvt, msg_to_skype);
		}
		switch_sleep(10000);
		DEBUGA_SKYPE
			("We have NOT answered a Skype RING from skype_call %s, because we are already in a skypiax call (%s)\n",
			 SKYPIAX_P_LOG, id, tech_pvt->skype_call_id);

		switch_mutex_unlock(globals.mutex);
	}
	return 0;
}

int incoming_chatmessage(private_t * tech_pvt, int which)
{
	switch_event_t *event;
	switch_core_session_t *session = NULL;
	int event_sent_to_esl = 0;

	DEBUGA_SKYPE("received CHATMESSAGE on interface %s\n", SKYPIAX_P_LOG, tech_pvt->name);

	if (!zstr(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
	}
	if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", tech_pvt->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", tech_pvt->chatmessages[which].from_dispname);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", tech_pvt->chatmessages[which].from_handle);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "chatname", tech_pvt->chatmessages[which].chatname);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "id", tech_pvt->chatmessages[which].id);
		switch_event_add_body(event, "%s", tech_pvt->chatmessages[which].body);
		if(session){
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "true");
			if (switch_core_session_queue_event(session, &event) != SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
				switch_event_fire(&event);
			}
		} else { //no session
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "false");
			switch_event_fire(&event);
			event_sent_to_esl=1;
		}

	}else{
		ERRORA("cannot create event on interface %s. WHY?????\n", SKYPIAX_P_LOG, tech_pvt->name);
	}

	if(!event_sent_to_esl){

		if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", tech_pvt->name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", tech_pvt->chatmessages[which].from_dispname);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", tech_pvt->chatmessages[which].from_handle);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "chatname", tech_pvt->chatmessages[which].chatname);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "id", tech_pvt->chatmessages[which].id);
			switch_event_add_body(event, "%s", tech_pvt->chatmessages[which].body);
			if(session){
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "true");
			} else { //no session
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "during-call", "false");
			}
			switch_event_fire(&event);
		} else{
			ERRORA("cannot create event on interface %s. WHY?????\n", SKYPIAX_P_LOG, tech_pvt->name);
		}
	}

	if(session){
		switch_core_session_rwunlock(session);
	}
	memset(&tech_pvt->chatmessages[which], '\0', sizeof(&tech_pvt->chatmessages[which]) );
	return 0;
}


SWITCH_STANDARD_API(skypiax_chat_function)
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
		stream->write_function(stream, "ERROR, usage: %s", SKYPIAX_CHAT_SYNTAX);
		goto end;
	}

	if (argc < 3) {
		stream->write_function(stream, "ERROR, usage: %s", SKYPIAX_CHAT_SYNTAX);
		goto end;
	}

	if (argv[0]) {
		for (i = 0; !found && i < SKYPIAX_MAX_INTERFACES; i++) {
			/* we've been asked for a normal interface name, or we have not found idle interfaces to serve as the "ANY" interface */
			if (strlen(globals.SKYPIAX_INTERFACES[i].name)
					&& (strncmp(globals.SKYPIAX_INTERFACES[i].name, argv[0], strlen(argv[0])) == 0)) {
				tech_pvt = &globals.SKYPIAX_INTERFACES[i];
				stream->write_function(stream, "Using interface: globals.SKYPIAX_INTERFACES[%d].name=|||%s|||\n", i, globals.SKYPIAX_INTERFACES[i].name);
				found = 1;
				break;
			}

		}
		if (!found) {
			stream->write_function(stream, "ERROR: A Skypiax interface with name='%s' was not found\n", argv[0]);
			goto end;
		} else {

			//chat_send(const char *proto, const char *from, const char *to, const char *subject, const char *body, const char *type, const char *hint);
			//chat_send(p*roto, const char *from, const char *to, const char *subject, const char *body, const char *type, const char *hint);
			//chat_send(MDL_CHAT_PROTO, tech_pvt->skype_user, argv[1], "SIMPLE MESSAGE", switch_str_nil((char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]), NULL, hint);

			NOTICA("chat_send(proto=%s, from=%s, to=%s, subject=%s, body=%s, type=NULL, hint=%s)\n", SKYPIAX_P_LOG, MDL_CHAT_PROTO, tech_pvt->skype_user, argv[1], "SIMPLE MESSAGE", switch_str_nil((char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]), tech_pvt->name);

			chat_send(MDL_CHAT_PROTO, tech_pvt->skype_user, argv[1], "SIMPLE MESSAGE", switch_str_nil((char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]), NULL, tech_pvt->name);

			//NOTICA("TEXT is: %s\n", SKYPIAX_P_LOG, (char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1] );
			//snprintf(skype_msg, sizeof(skype_msg), "CHAT CREATE %s", argv[1]);
			//skypiax_signaling_write(tech_pvt, skype_msg);
			//switch_sleep(100);
		}
	} else {
		stream->write_function(stream, "ERROR, usage: %s", SKYPIAX_CHAT_SYNTAX);
		goto end;
	}

#ifdef NOTDEF

	found=0;

	while(!found){
		for(i=0; i<MAX_CHATS; i++){
			if(!strcmp(tech_pvt->chats[i].dialog_partner, argv[1]) ){
				snprintf(skype_msg, sizeof(skype_msg), "CHATMESSAGE %s %s", tech_pvt->chats[i].chatname, (char *) &cmd[strlen(argv[0]) + 1 + strlen(argv[1]) + 1]);
				skypiax_signaling_write(tech_pvt, skype_msg);
				found=1;
				break;
			}
		}
		if(found){
			break;
		}
		if(tried > 1000){
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
