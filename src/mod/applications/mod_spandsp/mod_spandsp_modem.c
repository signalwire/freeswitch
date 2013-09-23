/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH mod_fax.
 *
 * The Initial Developer of the Original Code is
 * Massimo Cetra <devel@navynet.it>
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Brian West <brian@freeswitch.org>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Steve Underwood <steveu@coppice.org>
 * mod_spandsp_modem.c -- t31 Soft Modem 
 *
 */

#include "mod_spandsp.h"
#include "udptl.h"
#include "mod_spandsp_modem.h"

#if defined(MODEM_SUPPORT)
#ifndef WIN32
#include <poll.h>
#endif

#define LOCAL_FAX_MAX_DATAGRAM      400
#define MAX_FEC_ENTRIES             4
#define MAX_FEC_SPAN                4
#define DEFAULT_FEC_ENTRIES         3
#define DEFAULT_FEC_SPAN            3

static struct {
	int REF_COUNT;
	int THREADCOUNT;
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	modem_t MODEM_POOL[MAX_MODEMS];
	int SOFT_MAX_MODEMS;
} globals;

struct modem_state {
	int state;
	char *name;
};

static struct modem_state MODEM_STATE[] = {
	{MODEM_STATE_INIT, "INIT"},
	{MODEM_STATE_ONHOOK, "ONHOOK"},
	{MODEM_STATE_OFFHOOK, "OFFHOOK"},
	{MODEM_STATE_ACQUIRED, "ACQUIRED"},
	{MODEM_STATE_RINGING, "RINGING"},
	{MODEM_STATE_ANSWERED, "ANSWERED"},
	{MODEM_STATE_DIALING, "DIALING"},
	{MODEM_STATE_CONNECTED, "CONNECTED"},
	{MODEM_STATE_HANGUP, "HANGUP"},
	{MODEM_STATE_LAST, "UNKNOWN"}
};


static modem_t *acquire_modem(int index);

static int t38_tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
	return 0;
}

static int t31_at_tx_handler(at_state_t *s, void *user_data, const uint8_t *buf, size_t len)
{
	modem_t *modem = user_data;

#ifndef WIN32
	switch_size_t wrote;
	wrote = write(modem->master, buf, len);
#else
	DWORD wrote;
	OVERLAPPED o;
	o.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	/* Initialize the rest of the OVERLAPPED structure to zero. */
	o.Internal = 0;
	o.InternalHigh = 0;
	o.Offset = 0;
	o.OffsetHigh = 0;
	assert(o.hEvent);
	if (!WriteFile(modem->master, buf, (DWORD)len, &wrote, &o)) {
		GetOverlappedResult(modem->master, &o, &wrote, TRUE);
	}
	CloseHandle (o.hEvent);
#endif

	if (wrote != len) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to pass the full buffer onto the device file. " 
						  "%"SWITCH_SSIZE_T_FMT " bytes of " "%"SWITCH_SIZE_T_FMT " written: %s\n", wrote, len, strerror(errno));

		if (wrote == -1) wrote = 0;

#ifndef WIN32
		if (tcflush(modem->master, TCOFLUSH)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to flush pty master buffer: %s\n", strerror(errno));
		} else if (tcflush(modem->slave, TCOFLUSH)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to flush pty slave buffer: %s\n", strerror(errno));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Successfully flushed pty buffer\n");
		}
#endif
	}
	return wrote;
}

static int t31_call_control_handler(t31_state_t *s, void *user_data, int op, const char *num)
{
	modem_t *modem = user_data;
	int ret = 0;

	if (modem->control_handler) {
		ret = modem->control_handler(modem, num, op);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DOH! NO CONTROL HANDLER INSTALLED\n");
	}

	return ret;
}

static modem_state_t modem_get_state(modem_t *modem) 
{
	modem_state_t state;

	switch_mutex_lock(modem->mutex);
	state = modem->state;
	switch_mutex_unlock(modem->mutex);

	return state;
}

static void _modem_set_state(modem_t *modem, modem_state_t state, const char *file, const char *func, int line) 
{

	switch_mutex_lock(modem->mutex);
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG,"Modem %s [%s] - Changing state to %s\n", modem->devlink, 
					  modem_state2name(modem->state), modem_state2name(state));
	modem->state = state;
	switch_mutex_unlock(modem->mutex);
}
#define modem_set_state(_modem, _state) _modem_set_state(_modem, _state, __FILE__, __SWITCH_FUNC__, __LINE__)

char *modem_state2name(int state) 
{
	if (state > MODEM_STATE_LAST || state < 0) {
		state = MODEM_STATE_LAST;
	}

	return MODEM_STATE[state].name;
}

int modem_close(modem_t *modem) 
{
	int r = 0;
	switch_status_t was_running = switch_test_flag(modem, MODEM_FLAG_RUNNING);

	switch_clear_flag(modem, MODEM_FLAG_RUNNING);

#ifndef WIN32
	if (modem->master > -1) {
		shutdown(modem->master, 2);
		close(modem->master);
		modem->master = -1;
#else
	if (modem->master) {
		SetEvent(modem->threadAbort);
		CloseHandle(modem->threadAbort);
		CloseHandle(modem->master);
		modem->master = 0;
#endif
		r++;
	}

	if (modem->slave > -1) {
		shutdown(modem->slave, 2);
		close(modem->slave);
		modem->slave = -1;
		r++;
	}

	if (modem->t31_state) {
		t31_free(modem->t31_state);
		modem->t31_state = NULL;
	}

	unlink(modem->devlink);

	if (was_running) {
		switch_mutex_lock(globals.mutex);
		globals.REF_COUNT--;
		switch_mutex_unlock(globals.mutex);
	}

	return r;
}

switch_status_t modem_init(modem_t *modem, modem_control_handler_t control_handler)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
#ifdef WIN32
	COMMTIMEOUTS timeouts = {0};
#endif
	logging_state_t *logging;

	modem->master = -1;
	modem->slave = -1;

	/* windows will have to try something like:
	   http://com0com.cvs.sourceforge.net/viewvc/com0com/com0com/ReadMe.txt?revision=RELEASED
	 */

#if USE_OPENPTY
	if (openpty(&modem->master, &modem->slave, NULL, NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: failed to initialize pty\n");
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	modem->stty = ttyname(modem->slave);
#else
#ifdef WIN32
	snprintf(modem->devlink, sizeof(modem->devlink), "COM%d", modem->slot);

	modem->master = CreateFile(modem->devlink,
					GENERIC_READ | GENERIC_WRITE,
					0,
					0,
					OPEN_EXISTING,
					FILE_FLAG_OVERLAPPED,
					0);
	if (modem->master == INVALID_HANDLE_VALUE) {
		status = SWITCH_STATUS_FALSE;
		if (GetLastError() == ERROR_FILE_NOT_FOUND) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: Serial port does not exist\n");
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: Serial port open error\n");
		goto end;
	}
#elif !defined(HAVE_POSIX_OPENPT)
	modem->master = open("/dev/ptmx", O_RDWR);
#else
	modem->master = posix_openpt(O_RDWR | O_NOCTTY);
#endif

#ifndef WIN32
	if (modem->master < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: failed to initialize UNIX98 master pty\n");
	}

	if (grantpt(modem->master) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: failed to grant access to slave pty\n");
	}

	if (unlockpt(modem->master) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: failed to unlock slave pty\n");
	}

	modem->stty = ptsname(modem->master);
	if (modem->stty == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: failed to obtain slave pty filename\n");
	}

	modem->slave = open(modem->stty, O_RDWR);
	if (modem->slave < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: failed to open slave pty %s\n", modem->stty);
	}
#endif

#ifdef SOLARIS
	ioctl(modem->slave, I_PUSH, "ptem");  /* push ptem */
	ioctl(modem->slave, I_PUSH, "ldterm");    /* push ldterm*/
#endif
#endif

#ifndef WIN32
	snprintf(modem->devlink, sizeof(modem->devlink), "%s/FS%d", spandsp_globals.modem_directory, modem->slot);
	
	unlink(modem->devlink);

	if (symlink(modem->stty, modem->devlink)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fatal error: failed to create %s symbolic link\n", modem->devlink);
		modem_close(modem);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if (fcntl(modem->master, F_SETFL, fcntl(modem->master, F_GETFL, 0) | O_NONBLOCK)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set up non-blocking read on %s\n", ttyname(modem->master));
		modem_close(modem);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}
#else
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;

	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	SetCommMask(modem->master, EV_RXCHAR);

	if (!SetCommTimeouts(modem->master, &timeouts)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set up non-blocking read on %s\n", modem->devlink);
		modem_close(modem);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}
	modem->threadAbort = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
	
	if (!(modem->t31_state = t31_init(NULL, t31_at_tx_handler, modem, t31_call_control_handler, modem, t38_tx_packet_handler, modem))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot initialize the T.31 modem\n");
		modem_close(modem);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}
	modem->t38_core = t31_get_t38_core_state(modem->t31_state);

	if (spandsp_globals.modem_verbose) {
		logging = t31_get_logging_state(modem->t31_state);
		span_log_set_message_handler(logging, mod_spandsp_log_message, NULL);
		span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

		logging = v17_rx_get_logging_state(&modem->t31_state->audio.modems.fast_modems.v17_rx);
		span_log_set_message_handler(logging, mod_spandsp_log_message, NULL);
		span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

		logging = v29_rx_get_logging_state(&modem->t31_state->audio.modems.fast_modems.v29_rx);
		span_log_set_message_handler(logging, mod_spandsp_log_message, NULL);
		span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

		logging = v27ter_rx_get_logging_state(&modem->t31_state->audio.modems.fast_modems.v27ter_rx);
		span_log_set_message_handler(logging, mod_spandsp_log_message, NULL);
		span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

		logging = t38_core_get_logging_state(modem->t38_core);
		span_log_set_message_handler(logging, mod_spandsp_log_message, NULL);
		span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
	}

	modem->control_handler = control_handler;
	modem->flags = 0;
	switch_set_flag(modem, MODEM_FLAG_RUNNING);

	switch_mutex_init(&modem->mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&modem->cond_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_thread_cond_create(&modem->cond, globals.pool);

	modem_set_state(modem, MODEM_STATE_INIT);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Modem [%s]->[%s] Ready\n", modem->devlink, modem->stty);

	switch_mutex_lock(globals.mutex);
	globals.REF_COUNT++;
	switch_mutex_unlock(globals.mutex);

end:
	return status;
}

static switch_endpoint_interface_t *modem_endpoint_interface = NULL;

struct private_object {
	switch_mutex_t *mutex;
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	udptl_state_t udptl_state;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_timer_t timer;
	modem_t *modem;
	switch_caller_profile_t *caller_profile;
	int dead;
};

typedef struct private_object private_t;

static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_hangup(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);
static switch_status_t channel_on_routing(switch_core_session_t *session);
static switch_status_t channel_on_exchange_media(switch_core_session_t *session);
static switch_status_t channel_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);


/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt = NULL;
	int to_ticks = 60, ring_ticks = 10, rt = ring_ticks;
	int rest = 500000;
	at_state_t *at_state;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
#ifndef WIN32
		int tioflags;
#endif
		char call_time[16];
		char call_date[16];
		switch_size_t retsize;
		switch_time_exp_t tm;

		switch_time_exp_lt(&tm, switch_micro_time_now());
		switch_strftime(call_date, &retsize, sizeof(call_date), "%m%d", &tm);
		switch_strftime(call_time, &retsize, sizeof(call_time), "%H%M", &tm);

#ifndef WIN32
		ioctl(tech_pvt->modem->slave, TIOCMGET, &tioflags);
		tioflags |= TIOCM_RI;
		ioctl(tech_pvt->modem->slave, TIOCMSET, &tioflags);
#endif

		at_state = t31_get_at_state(tech_pvt->modem->t31_state);
		at_reset_call_info(at_state);
		at_set_call_info(at_state, "DATE", call_date);
		at_set_call_info(at_state, "TIME", call_time);
		at_set_call_info(at_state, "NAME", tech_pvt->caller_profile->caller_id_name);
		at_set_call_info(at_state, "NMBR", tech_pvt->caller_profile->caller_id_number);
		at_set_call_info(at_state, "ANID", tech_pvt->caller_profile->ani);
		at_set_call_info(at_state, "USER", tech_pvt->caller_profile->username);
		at_set_call_info(at_state, "CDID", tech_pvt->caller_profile->context);
		at_set_call_info(at_state, "NDID", tech_pvt->caller_profile->destination_number);

		modem_set_state(tech_pvt->modem, MODEM_STATE_RINGING);
		t31_call_event(tech_pvt->modem->t31_state, AT_CALL_EVENT_ALERTING);

		while (to_ticks > 0 && switch_channel_up(channel) && modem_get_state(tech_pvt->modem) == MODEM_STATE_RINGING) {
			if (--rt <= 0) {
				t31_call_event(tech_pvt->modem->t31_state, AT_CALL_EVENT_ALERTING);
				rt = ring_ticks;
			}
			
			switch_yield(rest);
			to_ticks--;
		}
		
		if (to_ticks < 1 || modem_get_state(tech_pvt->modem) != MODEM_STATE_ANSWERED) {
			t31_call_event(tech_pvt->modem->t31_state, AT_CALL_EVENT_NO_ANSWER);
			switch_channel_hangup(channel, SWITCH_CAUSE_NO_ANSWER);
		} else {
			t31_call_event(tech_pvt->modem->t31_state, AT_CALL_EVENT_ANSWERED);
			modem_set_state(tech_pvt->modem, MODEM_STATE_CONNECTED);
			switch_channel_mark_answered(channel);
		}
	}

	switch_channel_set_state(channel, CS_ROUTING);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	//switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	//channel = switch_core_session_get_channel(session);
	//switch_assert(channel != NULL);
	
	if ((tech_pvt = switch_core_session_get_private(session))) {

		switch_core_timer_destroy(&tech_pvt->timer);

		if (tech_pvt->modem) {
			*tech_pvt->modem->uuid_str = '\0';
			*tech_pvt->modem->digits = '\0';
			modem_set_state(tech_pvt->modem, MODEM_STATE_ONHOOK);
			tech_pvt->modem = NULL;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	t31_call_event(tech_pvt->modem->t31_state, AT_CALL_EVENT_HANGUP);

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

	switch (sig) {
	case SWITCH_SIG_BREAK:
		break;
	case SWITCH_SIG_KILL:
		tech_pvt->dead = 1;
		break;
	default:
		break;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL KILL\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL MODEM\n");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_reset(switch_core_session_t *session)
{
	private_t *tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RESET\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hibernate(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s HIBERNATE\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_consume_media(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL CONSUME_MEDIA\n");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int r, samples_wanted, samples_read = 0;
	int16_t *data;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (tech_pvt->dead) return SWITCH_STATUS_FALSE;

	data = tech_pvt->read_frame.data;
	samples_wanted = tech_pvt->read_codec.implementation->samples_per_packet;

	tech_pvt->read_frame.flags = SFF_NONE;
	switch_core_timer_next(&tech_pvt->timer);
	
	do {
		r = t31_tx(tech_pvt->modem->t31_state, data + samples_read, samples_wanted - samples_read);
		if (r < 0) break;
		samples_read += r;
	} while (samples_read < samples_wanted && r > 0);

	if (r < 0) {
		return SWITCH_STATUS_FALSE;
	}
	if (samples_read < samples_wanted) {
		memset(data + samples_read, 0, sizeof(int16_t)*(samples_wanted - samples_read));
		samples_read = samples_wanted;
	}

	tech_pvt->read_frame.samples = samples_read;
	tech_pvt->read_frame.datalen = samples_read * 2;

	*frame = &tech_pvt->read_frame;

	return status;
}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (tech_pvt->dead) return SWITCH_STATUS_FALSE;

	if (t31_rx(tech_pvt->modem->t31_state, frame->data, frame->datalen / 2)) {
		status = SWITCH_STATUS_FALSE;
	}
	
	return status;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		t31_call_event(tech_pvt->modem->t31_state, AT_CALL_EVENT_CONNECTED);
		modem_set_state(tech_pvt->modem, MODEM_STATE_CONNECTED);
		mod_spandsp_indicate_data(session, SWITCH_FALSE, SWITCH_TRUE);
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		t31_call_event(tech_pvt->modem->t31_state, AT_CALL_EVENT_CONNECTED);
		modem_set_state(tech_pvt->modem, MODEM_STATE_CONNECTED);
		mod_spandsp_indicate_data(session, SWITCH_FALSE, SWITCH_TRUE);
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		break;
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		mod_spandsp_indicate_data(session, SWITCH_FALSE, SWITCH_TRUE);
		break;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		mod_spandsp_indicate_data(session, SWITCH_FALSE, SWITCH_TRUE);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tech_init(private_t *tech_pvt, switch_core_session_t *session)
{
	const char *iananame = "L16";
	int rate = 8000;
	int interval = 20;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const switch_codec_implementation_t *read_impl;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s setup codec %s/%d/%d\n", switch_channel_get_name(channel), iananame, rate,
					  interval);

	status = switch_core_codec_init(&tech_pvt->read_codec,
									iananame,
									NULL,
									rate, interval, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, switch_core_session_get_pool(session));

	if (status != SWITCH_STATUS_SUCCESS || !tech_pvt->read_codec.implementation || !switch_core_codec_ready(&tech_pvt->read_codec)) {
		goto end;
	}

	status = switch_core_codec_init(&tech_pvt->write_codec,
									iananame,
									NULL,
									rate, interval, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, switch_core_session_get_pool(session));

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
		goto end;
	}

	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	tech_pvt->read_frame.flags = SFF_NONE;

	switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(session, &tech_pvt->write_codec);

	read_impl = tech_pvt->read_codec.implementation;

	switch_core_timer_init(&tech_pvt->timer, "soft",
						   read_impl->microseconds_per_packet / 1000, read_impl->samples_per_packet, switch_core_session_get_pool(session));

	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_core_session_set_private(session, tech_pvt);
	tech_pvt->session = session;
	tech_pvt->channel = switch_core_session_get_channel(session);

end:

	return status;
}

static void tech_attach(private_t *tech_pvt, modem_t *modem)
{
	tech_pvt->modem = modem;
	switch_set_string(modem->uuid_str, switch_core_session_get_uuid(tech_pvt->session));
	switch_channel_set_variable_printf(tech_pvt->channel, "modem_slot", "%d", modem->slot);
	switch_channel_set_variable(tech_pvt->channel, "modem_devlink", modem->devlink);
	switch_channel_set_variable(tech_pvt->channel, "modem_digits", modem->digits);
	switch_channel_export_variable(tech_pvt->channel, "rtp_autoflush_during_bridge", "false", SWITCH_EXPORT_VARS_VARIABLE);
}


static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{
	char name[128];
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

	if ((*new_session = switch_core_session_request(modem_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool)) != 0) {
		private_t *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *caller_profile;
		char *dest = switch_core_session_strdup(*new_session, outbound_profile->destination_number);
		char *modem_id_string = NULL;
		char *number = NULL;
		int modem_id = 0;
		modem_t *modem = NULL;

		if ((modem_id_string = dest)) {
			if ((number = strchr(modem_id_string, '/'))) {
				*number++ = '\0';
			}
		}

		if (zstr(modem_id_string) || zstr(number)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_ERROR, "Invalid dial string.\n");
			cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT; goto fail;
		}

		if (!strcasecmp(modem_id_string, "a")) {
			modem_id = -1;
		} else {
			modem_id = atoi(modem_id_string);
		}

		if (!(modem = acquire_modem(modem_id))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_ERROR, "Cannot find a modem.\n");
			cause = SWITCH_CAUSE_USER_BUSY; goto fail;
		}
		
		switch_core_session_add_stream(*new_session, NULL);

		if ((tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t))) != 0) {
			channel = switch_core_session_get_channel(*new_session);
			switch_snprintf(name, sizeof(name), "modem/%d/%s", modem->slot, number);
			switch_channel_set_name(channel, name);

			if (tech_init(tech_pvt, *new_session) != SWITCH_STATUS_SUCCESS) {
				cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER; goto fail;
			}

			switch_set_string(modem->digits, number);
			tech_attach(tech_pvt, modem);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER; goto fail;
		}

		if (outbound_profile) {
			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			caller_profile->source = switch_core_strdup(caller_profile->pool, "mod_spandsp");
			caller_profile->destination_number = switch_core_strdup(caller_profile->pool, number);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_ERROR, "Doh! no caller profile\n");
			cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER; goto fail;
		}

		switch_channel_set_state(channel, CS_INIT);

		return SWITCH_CAUSE_SUCCESS;

	fail:

		if (new_session) {
			switch_core_session_destroy(new_session);
		}

		if (modem) {
			modem_set_state(modem, MODEM_STATE_ONHOOK);
		}
	}

	return cause;
}

static switch_state_handler_table_t channel_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media */ channel_on_consume_media,
	/*.on_hibernate */ channel_on_hibernate,
	/*.on_reset */ channel_on_reset,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ channel_on_destroy
};

static switch_io_routines_t channel_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message
};

static switch_status_t create_session(switch_core_session_t **new_session, modem_t *modem)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_core_session_t *session;
	switch_channel_t *channel;
	private_t *tech_pvt = NULL;
	char name[1024];
	switch_caller_profile_t *caller_profile;
	char *ani = NULL, *p, *digits = NULL;
	
	if (!(session = switch_core_session_request(modem_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Failure.\n");
		goto end;
	}

	switch_core_session_add_stream(session, NULL);
	channel = switch_core_session_get_channel(session);
	tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(*tech_pvt));
	
	p = switch_core_session_strdup(session, modem->digits);

	if (*p == '*') {
		ani = p + 1;
		if ((digits = strchr(ani, '*'))) {
			*digits++ = '\0';
		} else {
			ani = NULL;
		}
	}
	
	if (zstr(digits)) {
		digits = p;
	}

	if (zstr(ani)) {
		ani = modem->devlink + 5;
	}

	switch_snprintf(name, sizeof(name), "modem/%d/%s", modem->slot, digits);
	switch_channel_set_name(channel, name);

	if (tech_init(tech_pvt, session) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_core_session_destroy(&session);
		goto end;
	}

	caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
											   modem->devlink,
											   spandsp_globals.modem_dialplan,
											   "FSModem", 
											   ani,
											   NULL, 
											   ani,
											   NULL, 
											   NULL, 
											   "mod_spandsp", 
											   spandsp_globals.modem_context, 
											   digits);

	caller_profile->source = switch_core_strdup(caller_profile->pool, "mod_spandsp");
	switch_channel_set_caller_profile(channel, caller_profile);
	tech_pvt->caller_profile = caller_profile;
	switch_channel_set_state(channel, CS_INIT);

	if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error spawning thread\n");
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto end;
	}

	status = SWITCH_STATUS_SUCCESS;
	tech_attach(tech_pvt, modem);
	*new_session = session;

 end:

	return status;
}

static void wake_modem_thread(modem_t *modem)
{
	if (switch_mutex_trylock(modem->cond_mutex) == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(modem->cond);
		switch_mutex_unlock(modem->cond_mutex);
	}
}

static int control_handler(modem_t *modem, const char *num, int op)
{
	switch_core_session_t *session = NULL;
	at_state_t *at_state;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Control Handler op:%d state:[%s] %s\n", 
					  op, modem_state2name(modem_get_state(modem)), modem->devlink);

	switch (op) {
	case AT_MODEM_CONTROL_ANSWER:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - Answering\n", modem->devlink, modem_state2name(modem_get_state(modem)));
		modem_set_state(modem, MODEM_STATE_ANSWERED);
		break;
	case AT_MODEM_CONTROL_CALL:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "Modem %s [%s] - Dialing '%s'\n", modem->devlink, modem_state2name(modem_get_state(modem)), num);
			modem_set_state(modem, MODEM_STATE_DIALING);
			switch_clear_flag(modem, MODEM_FLAG_XOFF);
			wake_modem_thread(modem);

			switch_set_string(modem->digits, num);
			
			if (create_session(&session, modem) != SWITCH_STATUS_SUCCESS) {
				t31_call_event(modem->t31_state, AT_CALL_EVENT_HANGUP);
			} else {
				switch_core_session_thread_launch(session);
			}
		}
		break;
	case AT_MODEM_CONTROL_OFFHOOK:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - Going off hook\n", modem->devlink, modem_state2name(modem_get_state(modem)));
		modem_set_state(modem, MODEM_STATE_OFFHOOK);
		break;
	case AT_MODEM_CONTROL_ONHOOK:
	case AT_MODEM_CONTROL_HANGUP: 
		{
			if (modem_get_state(modem) != MODEM_STATE_RINGING) {
				int set_state = 1;
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								  "Modem %s [%s] - Hanging up\n", modem->devlink, modem_state2name(modem_get_state(modem)));
				switch_clear_flag(modem, MODEM_FLAG_XOFF);
				wake_modem_thread(modem);

				modem_set_state(modem, MODEM_STATE_HANGUP);

				if (!zstr(modem->uuid_str)) {
					switch_core_session_t *session;
					
					if ((session = switch_core_session_force_locate(modem->uuid_str))) {
						switch_channel_t *channel = switch_core_session_get_channel(session);
						
						if (switch_channel_up(channel)) {
							switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
							set_state = 0;
						}
						switch_core_session_rwunlock(session);
					}
				}

				if (set_state) {
					modem_set_state(modem, MODEM_STATE_ONHOOK);
				}
			}
		}
		break;
	case AT_MODEM_CONTROL_DTR:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - DTR %d\n", modem->devlink, modem_state2name(modem_get_state(modem)), (int) (intptr_t) num);
		break;
	case AT_MODEM_CONTROL_RTS:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - RTS %d\n", modem->devlink, modem_state2name(modem_get_state(modem)), (int) (intptr_t) num);
		break;
	case AT_MODEM_CONTROL_CTS:
		{
			u_char x[1];
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1,
							  "Modem %s [%s] - CTS %s\n", modem->devlink, modem_state2name(modem_get_state(modem)), (int) (intptr_t) num ? "XON" : "XOFF");

			at_state = t31_get_at_state(modem->t31_state);
			if (num) {
				x[0] = 0x11;
				t31_at_tx_handler(at_state, modem, x, 1);
				switch_clear_flag(modem, MODEM_FLAG_XOFF);
				wake_modem_thread(modem);
			} else {
				x[0] = 0x13;
				t31_at_tx_handler(at_state, modem, x, 1);
				switch_set_flag(modem, MODEM_FLAG_XOFF);
			}
		}
		break;
	case AT_MODEM_CONTROL_CAR:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - CAR %d\n", modem->devlink, modem_state2name(modem_get_state(modem)), (int) (intptr_t) num);
		break;
	case AT_MODEM_CONTROL_RNG:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - RNG %d\n", modem->devlink, modem_state2name(modem_get_state(modem)), (int) (intptr_t) num);
		break;
	case AT_MODEM_CONTROL_DSR:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - DSR %d\n", modem->devlink, modem_state2name(modem_get_state(modem)), (int) (intptr_t) num);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Modem %s [%s] - operation %d\n", modem->devlink, modem_state2name(modem_get_state(modem)), op);
		break;
	}
	/*endswitch*/
	return 0;
}

typedef enum {
	MODEM_POLL_READ = (1 << 0),
	MODEM_POLL_WRITE = (1 << 1),
	MODEM_POLL_ERROR = (1 << 2)
} modem_poll_t;

#ifndef WIN32
static int modem_wait_sock(int sock, uint32_t ms, modem_poll_t flags)
{
	struct pollfd pfds[2] = { { 0 } };
	int s = 0, r = 0;
	
	pfds[0].fd = sock;

	if ((flags & MODEM_POLL_READ)) {
		pfds[0].events |= POLLIN;
	}

	if ((flags & MODEM_POLL_WRITE)) {
		pfds[0].events |= POLLOUT;
	}

	if ((flags & MODEM_POLL_ERROR)) {
		pfds[0].events |= POLLERR;
	}
	
	s = poll(pfds, 1, ms);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((pfds[0].revents & POLLIN)) {
			r |= MODEM_POLL_READ;
		}
		if ((pfds[0].revents & POLLOUT)) {
			r |= MODEM_POLL_WRITE;
		}
		if ((pfds[0].revents & POLLERR)) {
			r |= MODEM_POLL_ERROR;
		}
	}

	return r;
}
#else
static int modem_wait_sock(modem_t *modem, int ms, modem_poll_t flags)
{
/* this method ignores ms and waits infinitely */
	DWORD dwEvtMask, dwWait;
	OVERLAPPED o;
	BOOL result;
	int ret = MODEM_POLL_ERROR;
	HANDLE arHandles[2];

	arHandles[0] = modem->threadAbort;

	o.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	arHandles[1] = o.hEvent;

	/* Initialize the rest of the OVERLAPPED structure to zero. */
	o.Internal = 0;
	o.InternalHigh = 0;
	o.Offset = 0;
	o.OffsetHigh = 0;
	assert(o.hEvent);

	result = WaitCommEvent(modem->master, &dwEvtMask, &o);

	if (result == 0)
	{
		if (GetLastError() != ERROR_IO_PENDING) {
			/* something went horribly wrong with WaitCommEvent(), so 
			clear all errors and try again */
			DWORD comerrors;
			ClearCommError(modem->master, &comerrors, 0);
		} else {
			/* IO is pending, wait for it to finish */
			dwWait = WaitForMultipleObjects(2, arHandles, FALSE, INFINITE);
			if (dwWait == WAIT_OBJECT_0 + 1) {
				ret = MODEM_POLL_READ;
			}
		}
	}
	else {
		ret = MODEM_POLL_READ;
	}

	CloseHandle (o.hEvent);
	return ret;
}
#endif

static void *SWITCH_THREAD_FUNC modem_thread(switch_thread_t *thread, void *obj)
{
	modem_t *modem = obj;
	int r, avail;
#ifdef WIN32
	DWORD readBytes;
	OVERLAPPED o;
#endif
	char buf[T31_TX_BUF_LEN], tmp[80];

	switch_mutex_lock(globals.mutex);
	modem_init(modem, control_handler);
	globals.THREADCOUNT++;
	switch_mutex_unlock(globals.mutex);

	if (switch_test_flag(modem, MODEM_FLAG_RUNNING)) {
		switch_mutex_lock(modem->cond_mutex);

		while (switch_test_flag(modem, MODEM_FLAG_RUNNING)) {

#ifndef WIN32
			r = modem_wait_sock(modem->master, -1, MODEM_POLL_READ | MODEM_POLL_ERROR);
#else
			r = modem_wait_sock(modem, -1, MODEM_POLL_READ | MODEM_POLL_ERROR);
#endif

			if (!switch_test_flag(modem, MODEM_FLAG_RUNNING)) {
				break;
			}

			if (r < 0 || !(r & MODEM_POLL_READ) || (r & MODEM_POLL_ERROR)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Bad Read on master [%s] [%d]\n", modem->devlink, r);
				break;
			}

			modem->last_event = switch_time_now();

			if (switch_test_flag(modem, MODEM_FLAG_XOFF)) {
				switch_thread_cond_wait(modem->cond, modem->cond_mutex);
				modem->last_event = switch_time_now();
			}

			avail = t31_at_rx_free_space(modem->t31_state);
			if (avail == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Buffer Full, retrying....\n");
				switch_yield(10000);
				continue;
			}

#ifndef WIN32		
			r = read(modem->master, buf, avail);
#else
			o.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

			/* Initialize the rest of the OVERLAPPED structure to zero. */
			o.Internal = 0;
			o.InternalHigh = 0;
			o.Offset = 0;
			o.OffsetHigh = 0;
			assert(o.hEvent);
			if (!ReadFile(modem->master, buf, avail, &readBytes, &o)) {
				GetOverlappedResult(modem->master, &o, &readBytes,TRUE);
			}
			CloseHandle (o.hEvent);
			r = readBytes;
#endif
			t31_at_rx(modem->t31_state, buf, r);

			memset(tmp, 0, sizeof(tmp));
			if (!strncasecmp(buf, "AT", 2)) {
				int x;

				strncpy(tmp, buf, r);
				for (x = 0; x < r; x++) {
					if (tmp[x] == '\r' || tmp[x] == '\n') {
						tmp[x] = '\0';
					}
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Command on %s [%s]\n", modem->devlink, tmp);
			}
		}

		switch_mutex_unlock(modem->cond_mutex);

		if (switch_test_flag(modem, MODEM_FLAG_RUNNING)) {
			modem_close(modem);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Thread ended for %s\n", modem->devlink);

	switch_mutex_lock(globals.mutex);
	globals.THREADCOUNT--;
	switch_mutex_unlock(globals.mutex);

	return NULL;
}

static void launch_modem_thread(modem_t *modem) 
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, modem_thread, modem, globals.pool);
}

static void activate_modems(void)
{
	int max = globals.SOFT_MAX_MODEMS;
	int x;

	switch_mutex_lock(globals.mutex);
	memset(globals.MODEM_POOL, 0, sizeof(globals.MODEM_POOL));
	for (x = 0; x < max; x++) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Starting Modem SLOT %d\n", x);

		globals.MODEM_POOL[x].slot = x;
		launch_modem_thread(&globals.MODEM_POOL[x]);
	}
	switch_mutex_unlock(globals.mutex);
}

static void deactivate_modems(void)
{
	int max = globals.SOFT_MAX_MODEMS;
	int x;
	
	switch_mutex_lock(globals.mutex);

	for (x = 0; x < max; x++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stopping Modem SLOT %d\n", x);
		modem_close(&globals.MODEM_POOL[x]);
	}

	switch_mutex_unlock(globals.mutex);

	/* Wait for Threads to die */
	while (globals.THREADCOUNT) {
		switch_yield(100000);
	}
}

static modem_t *acquire_modem(int index)
{
	modem_t *modem = NULL;
	switch_time_t now = switch_time_now();
	int64_t idle_debounce = 2000000;

	switch_mutex_lock(globals.mutex);
	if (index > -1 && index < globals.SOFT_MAX_MODEMS) {
		modem = &globals.MODEM_POOL[index];
	} else {
		int x;

		for (x = 0; x < globals.SOFT_MAX_MODEMS; x++) {
			if (globals.MODEM_POOL[x].state == MODEM_STATE_ONHOOK && (now - globals.MODEM_POOL[x].last_event) > idle_debounce) {
				modem = &globals.MODEM_POOL[x];
				break;
			}
		}
	}

	if (modem && (modem->state != MODEM_STATE_ONHOOK || (now - modem->last_event) < idle_debounce)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Modem %s In Use!\n", modem->devlink);
		modem = NULL;
	}

	if (modem) {
		modem_set_state(modem, MODEM_STATE_ACQUIRED);
		modem->last_event = switch_time_now();
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Modems Available!\n");
	}

	switch_mutex_unlock(globals.mutex);

	return modem;
}

switch_status_t modem_global_init(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
{
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	globals.SOFT_MAX_MODEMS = spandsp_globals.modem_count;
	
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);

	modem_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	modem_endpoint_interface->interface_name = "modem";
	modem_endpoint_interface->io_routines = &channel_io_routines;
	modem_endpoint_interface->state_handler = &channel_event_handlers;

	activate_modems();

	return SWITCH_STATUS_SUCCESS;
}

void modem_global_shutdown(void)
{
	deactivate_modems();
}

#else

void modem_global_init(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Modem support disabled\n");
}

void modem_global_shutdown(void)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Modem support disabled\n");
}

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
