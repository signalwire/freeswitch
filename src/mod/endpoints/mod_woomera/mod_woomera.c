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
 *
 *
 * mod_woomera.c -- Woomera Endpoint Module
 *
 */
#include <switch.h>

#define WOOMERA_STRLEN 256
#define WOOMERA_ARRAY_LEN 50
#define WOOMERA_MIN_PORT 9900
#define WOOMERA_MAX_PORT 9999
#define WOOMERA_BODYLEN 2048
#define WOOMERA_LINE_SEPERATOR "\r\n"
#define WOOMERA_RECORD_SEPERATOR "\r\n\r\n"
#define WOOMERA_DEBUG_PREFIX "**[DEBUG]** "
#define WOOMERA_DEBUG_LINE "--------------------------------------------------------------------------------"
#define WOOMERA_HARD_TIMEOUT -10000
#define WOOMERA_QLEN 10
#define WOOMERA_RECONNECT_TIME 5000000
#define MEDIA_ANSWER "ANSWER"
// THE ONE ABOVE OR THE 2 BELOW BUT NOT BOTH
//#define MEDIA_ANSWER "ANSWER"
//#define USE_ANSWER 1

SWITCH_MODULE_LOAD_FUNCTION(mod_woomera_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_woomera_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_woomera_runtime);
SWITCH_MODULE_DEFINITION(mod_woomera, mod_woomera_load, mod_woomera_shutdown, mod_woomera_runtime);

static switch_memory_pool_t *module_pool = NULL;
switch_endpoint_interface_t *woomera_endpoint_interface;

#define STRLEN 15
#define FRAME_LEN 480
//static int WFORMAT = AST_FORMAT_SLINEAR;

typedef enum {
	WFLAG_EXISTS = (1 << 0),
	WFLAG_EVENT = (1 << 1),
	WFLAG_CONTENT = (1 << 2),
} WFLAGS;


typedef enum {
	WCFLAG_NOWAIT = (1 << 0)
} WCFLAGS;


typedef enum {
	PFLAG_INBOUND = (1 << 0),
	PFLAG_OUTBOUND = (1 << 1),
	PFLAG_DYNAMIC = (1 << 2),
	PFLAG_DISABLED = (1 << 3)
} PFLAGS;

typedef enum {
	TFLAG_MEDIA = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_INCOMING = (1 << 3),
	TFLAG_PARSE_INCOMING = (1 << 4),
	TFLAG_ACTIVATE = (1 << 5),
	TFLAG_DTMF = (1 << 6),
	TFLAG_DESTROY = (1 << 7),
	TFLAG_ABORT = (1 << 8),
	TFLAG_SWITCH = (1 << 9),
	TFLAG_ANSWER = (1 << 10)
} TFLAGS;

struct woomera_message {
	char callid[WOOMERA_STRLEN];
	int mval;
	char command[WOOMERA_STRLEN];
	char command_args[WOOMERA_STRLEN];
	char names[WOOMERA_STRLEN][WOOMERA_ARRAY_LEN];
	char values[WOOMERA_STRLEN][WOOMERA_ARRAY_LEN];
	char body[WOOMERA_BODYLEN];
	unsigned int flags;
	int last;
	struct woomera_message *next;
};


static struct {
	switch_port_t next_woomera_port;
	int debug;
	int panic;
	int rtpmode;
} globals;

struct woomera_event_queue {
	struct woomera_message *head;
};

struct woomera_profile {
	char *name;
	switch_socket_t *woomera_socket;
	switch_mutex_t *iolock;
	char woomera_host[WOOMERA_STRLEN];
	switch_port_t woomera_port;
	char audio_ip[WOOMERA_STRLEN];
	char dialplan[WOOMERA_STRLEN];
	unsigned int flags;
	int thread_running;
	struct woomera_event_queue event_queue;
};


struct private_object {
	char *name;
	switch_frame_t frame;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_core_session_t *session;
	switch_pollfd_t *read_poll;
	switch_pollfd_t *write_poll;
	switch_pollfd_t *command_poll;
	char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_mutex_t *iolock;
	switch_sockaddr_t *udpread;
	switch_sockaddr_t *udpwrite;
	switch_socket_t *command_channel;
	switch_socket_t *udp_socket;
	unsigned int flags;
	short fdata[FRAME_LEN];
	struct woomera_message call_info;
	struct woomera_profile *profile;
	char dest[WOOMERA_STRLEN];
	switch_port_t port;
	switch_time_t started;
	int timeout;
	char dtmfbuf[WOOMERA_STRLEN];
	switch_caller_profile_t *caller_profile;
	struct woomera_event_queue event_queue;
	switch_mutex_t *flag_mutex;
};

typedef struct private_object private_object;
typedef struct woomera_message woomera_message;
typedef struct woomera_profile woomera_profile;
typedef struct woomera_event_queue woomera_event_queue;

static woomera_profile default_profile;

static switch_status_t woomera_on_init(switch_core_session_t *session);
static switch_status_t woomera_on_hangup(switch_core_session_t *session);
static switch_status_t woomera_on_ring(switch_core_session_t *session);
static switch_status_t woomera_on_loopback(switch_core_session_t *session);
static switch_status_t woomera_on_transmit(switch_core_session_t *session);
static switch_call_cause_t woomera_outgoing_channel(switch_core_session_t *session,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool);
static switch_status_t woomera_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id);
static switch_status_t woomera_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id);
static switch_status_t woomera_kill_channel(switch_core_session_t *session, int sig);
static void tech_destroy(private_object * tech_pvt);
static void woomera_printf(woomera_profile * profile, switch_socket_t * socket, char *fmt, ...);
static char *woomera_message_header(woomera_message * wmsg, char *key);
static int woomera_enqueue_event(woomera_event_queue * event_queue, woomera_message * wmsg);
static int woomera_dequeue_event(woomera_event_queue * event_queue, woomera_message * wmsg);
static int woomera_message_parse(switch_socket_t * fd, woomera_message * wmsg, int timeout, woomera_profile * profile, woomera_event_queue * event_queue);
static int connect_woomera(switch_socket_t ** new_sock, woomera_profile * profile, int flags);
static int woomera_profile_thread_running(woomera_profile * profile, int set, int new);
static int woomera_locate_socket(woomera_profile * profile, switch_socket_t ** woomera_socket);
static int tech_create_read_socket(private_object * tech_pvt);
static void *woomera_channel_thread_run(switch_thread_t * thread, void *obj);
static void *woomera_thread_run(void *obj);
static int tech_activate(private_object * tech_pvt);

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t woomera_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	struct private_object *tech_pvt = NULL;
	int rate = 8000;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt->frame.data = tech_pvt->databuf;

	if (switch_core_codec_init
		(&tech_pvt->read_codec, "L16", NULL, rate, 30, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
		 switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Cannot set read codec\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init
		(&tech_pvt->write_codec, "L16", NULL, rate, 30, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
		 switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Cannot set read codec\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}
	tech_pvt->frame.rate = rate;
	tech_pvt->frame.codec = &tech_pvt->read_codec;
	switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(session, &tech_pvt->write_codec);


	switch_set_flag_locked(tech_pvt, TFLAG_ACTIVATE);

	switch_core_session_launch_thread(session, woomera_channel_thread_run, session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s WOOMERA INIT\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t woomera_on_ring(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s WOOMERA RING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t woomera_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s WOOMERA EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t woomera_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_core_codec_destroy(&tech_pvt->read_codec);
	switch_core_codec_destroy(&tech_pvt->write_codec);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s WOOMERA HANGUP\n", switch_channel_get_name(channel));
	tech_destroy(tech_pvt);

	return SWITCH_STATUS_SUCCESS;
}

static void woomera_socket_close(switch_socket_t ** socket)
{
	if (*socket) {
		switch_socket_close(*socket);
		*socket = NULL;
	}
}


static void udp_socket_close(struct private_object *tech_pvt)
{
	if (tech_pvt->udp_socket) {
		switch_socket_shutdown(tech_pvt->udp_socket, SWITCH_SHUTDOWN_READWRITE);
		woomera_socket_close(&tech_pvt->udp_socket);
	}
}


static switch_status_t woomera_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!tech_pvt->udp_socket) {
		return SWITCH_STATUS_FALSE;
	}

	switch (sig) {
	case SWITCH_SIG_KILL:
		udp_socket_close(tech_pvt);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s WOOMERA KILL\n", switch_channel_get_name(channel));
		break;
	case SWITCH_SIG_BREAK:
		{
			const char p = 0;
			switch_size_t len = sizeof(p);
			if (tech_pvt->udp_socket && tech_pvt->udpwrite) {
				switch_socket_sendto(tech_pvt->udp_socket, tech_pvt->udpwrite, 0, &p, &len);
			}
		}
		break;
	}


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t woomera_on_loopback(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WOOMERA LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t woomera_on_transmit(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WOOMERA TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t woomera_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	return switch_socket_waitfor(tech_pvt->read_poll, ms) ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

static switch_status_t woomera_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
//  return switch_socket_waitfor(tech_pvt->write_poll, ms);
}

static switch_status_t woomera_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;
	switch_frame_t *pframe;
	switch_size_t len;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	for(;;) {
		if (switch_test_flag(tech_pvt, TFLAG_ABORT) || !tech_pvt->udp_socket) {
			return SWITCH_STATUS_GENERR;
		}

		if (switch_test_flag(tech_pvt, TFLAG_MEDIA)) {
			break;
		}
		switch_yield(1000);
	}


	if (!tech_pvt->udp_socket) {
		return SWITCH_STATUS_GENERR;
	}
	/*
	   if ((status = woomera_waitfor_read(session, -1)) != SWITCH_STATUS_SUCCESS) {
	   return status;
	   }1<
	 */
	pframe = &tech_pvt->frame;
	*frame = pframe;

	len = sizeof(tech_pvt->databuf);
	if (switch_socket_recvfrom(tech_pvt->udpread, tech_pvt->udp_socket, 0, tech_pvt->databuf, &len) == SWITCH_STATUS_SUCCESS) {
		pframe->datalen = (uint32_t) len;
		pframe->samples = (int) pframe->datalen / 2;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t woomera_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;
	switch_size_t len;
	//switch_frame_t *pframe;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);
	
	if (switch_test_flag(tech_pvt, TFLAG_ABORT) || !tech_pvt->udp_socket) {
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_MEDIA)) {
		return SWITCH_STATUS_SUCCESS;
	}

	//pframe = &tech_pvt->frame;
	len = frame->datalen;
	if (switch_socket_sendto(tech_pvt->udp_socket, tech_pvt->udpwrite, 0, frame->data, &len) == SWITCH_STATUS_SUCCESS) {
		frame->datalen = (uint32_t) len;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

static switch_state_handler_table_t woomera_event_handlers = {
	/*.on_init */ woomera_on_init,
	/*.on_ring */ woomera_on_ring,
	/*.on_execute */ woomera_on_execute,
	/*.on_hangup */ woomera_on_hangup,
	/*.on_loopback */ woomera_on_loopback,
	/*.on_transmit */ woomera_on_transmit
};

static switch_io_routines_t woomera_io_routines = {
	/*.outgoing_channel */ woomera_outgoing_channel,
	/*.read_frame */ woomera_read_frame,
	/*.write_frame */ woomera_write_frame,
	/*.kill_channel */ woomera_kill_channel,
	/*.waitfor_read */ woomera_waitfor_read,
	/*.waitfor_write */ woomera_waitfor_write
};

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t woomera_outgoing_channel(switch_core_session_t *session,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool)
{
	if ((*new_session = switch_core_session_request(woomera_endpoint_interface, pool)) != 0) {
		struct private_object *tech_pvt;
		switch_channel_t *channel;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(*new_session));
			tech_pvt->profile = &default_profile;
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (outbound_profile) {
			char name[128];
			switch_caller_profile_t *caller_profile;

			switch_snprintf(name, sizeof(name), "Woomera/%s-%04x", outbound_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_CAUSE_SUCCESS;
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

}

static void tech_destroy(private_object * tech_pvt)
{
	woomera_message wmsg;

	if (globals.debug > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, WOOMERA_DEBUG_PREFIX "+++DESTROY\n");
	}


	woomera_printf(tech_pvt->profile, tech_pvt->command_channel, "hangup %s%s", tech_pvt->call_info.callid, WOOMERA_RECORD_SEPERATOR);
	if (woomera_message_parse(tech_pvt->command_channel, &wmsg, WOOMERA_HARD_TIMEOUT, tech_pvt->profile, &tech_pvt->event_queue) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "{%s} Already Disconnected\n", tech_pvt->profile->name);
	}

	woomera_printf(tech_pvt->profile, tech_pvt->command_channel, "bye%s", WOOMERA_RECORD_SEPERATOR);
	woomera_socket_close(&tech_pvt->command_channel);
	udp_socket_close(tech_pvt);
}


static void woomera_printf(woomera_profile * profile, switch_socket_t * socket, char *fmt, ...)
{
	char *stuff;
	size_t res = 0, len = 0;

	va_list ap;
	va_start(ap, fmt);
#ifndef vasprintf
	stuff = (char *) malloc(10240);
	vsnprintf(stuff, 10240, fmt, ap);
#else
	res = vasprintf(&stuff, fmt, ap);
#endif
	va_end(ap);
	if (res == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Out of memory\n");
	} else {
		if (profile && globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send Message: {%s} [%s/%d]\n%s\n%s\n", profile->name,
							  profile->woomera_host, profile->woomera_port, WOOMERA_DEBUG_LINE, stuff);
		}
		len = strlen(stuff);
		switch_socket_send(socket, stuff, &len);

		free(stuff);
	}

}

static char *woomera_message_header(woomera_message * wmsg, char *key)
{
	int x = 0;
	char *value = NULL;

	for (x = 0; x < wmsg->last; x++) {
		if (!strcasecmp(wmsg->names[x], key)) {
			value = wmsg->values[x];
			break;
		}
	}

	return value;
}

static int woomera_enqueue_event(woomera_event_queue * event_queue, woomera_message * wmsg)
{
	woomera_message *new, *mptr;

	if ((new = malloc(sizeof(woomera_message))) != 0) {
		memcpy(new, wmsg, sizeof(woomera_message));
		new->next = NULL;

		if (!event_queue->head) {
			event_queue->head = new;
		} else {
			for (mptr = event_queue->head; mptr && mptr->next; mptr = mptr->next);
			mptr->next = new;
		}
		return 1;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Allocation Error!\n");
	}

	return 0;
}

static int woomera_dequeue_event(woomera_event_queue * event_queue, woomera_message * wmsg)
{
	woomera_message *mptr = NULL;

	if (event_queue->head) {
		mptr = event_queue->head;
		event_queue->head = mptr->next;
	}

	if (mptr) {
		memcpy(wmsg, mptr, sizeof(woomera_message));
		free(mptr);
		return 1;
	} else {
		memset(wmsg, 0, sizeof(woomera_message));
	}

	return 0;
}

static int woomera_message_parse(switch_socket_t * fd, woomera_message * wmsg, int timeout, woomera_profile * profile, woomera_event_queue * event_queue)
{
	char *cur, *cr, *next = NULL;
	char buf[2048] = "", *ptr;
	int bytes = 0;

	memset(wmsg, 0, sizeof(woomera_message));

	if (!fd) {
		return -1;
	}

	if (timeout < 0) {
		timeout = abs(timeout);
	} else if (timeout == 0) {
		timeout = -1;
	}

	ptr = buf;
	bytes = 0;
	while (!strstr(buf, WOOMERA_RECORD_SEPERATOR)) {
		size_t len = 1;
		switch_status_t status;

		if (!profile->thread_running) {
			return -1;
		}

		status = switch_socket_recv(fd, ptr, &len);

		if (status == 70007) {
			char bbuf = '\n';
			switch_size_t blen = sizeof(bbuf);
			switch_socket_send(fd, &bbuf, &blen);
			continue;
		}

		if (status != SWITCH_STATUS_SUCCESS) {
			return -1;
		}

		ptr++;
		bytes++;
	}
	//*eor = '\0';
	next = buf;

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Receive Message: {%s} [%s/%d]\n%s\n%s\n", profile->name,
						  profile->woomera_host, profile->woomera_port, WOOMERA_DEBUG_LINE, buf);
	}

	while ((cur = next) != 0) {
		if ((cr = strstr(cur, WOOMERA_LINE_SEPERATOR)) != 0) {
			*cr = '\0';
			next = cr + (sizeof(WOOMERA_LINE_SEPERATOR) - 1);
			if (!strcmp(next, WOOMERA_RECORD_SEPERATOR)) {
				break;
			}
		}

		if (!cur || !cur[0]) {
			break;
		}

		if (!wmsg->last) {
			switch_set_flag(wmsg, WFLAG_EXISTS);
			if (!strncasecmp(cur, "EVENT", 5)) {
				cur += 6;
				switch_set_flag(wmsg, WFLAG_EVENT);

				if (cur && (cr = strchr(cur, ' ')) != 0) {
					char *id;

					*cr = '\0';
					cr++;
					id = cr;
					if (cr && (cr = strchr(cr, ' ')) != 0) {
						*cr = '\0';
						cr++;
						strncpy(wmsg->command_args, cr, WOOMERA_STRLEN);
					}
					if (id) {
						strncpy(wmsg->callid, id, sizeof(wmsg->callid) - 1);
					}
				}
			} else {
				if (cur && (cur = strchr(cur, ' ')) != 0) {
					*cur = '\0';
					cur++;
					wmsg->mval = atoi(buf);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Malformed Message!\n");
					break;
				}
			}
			if (cur) {
				strncpy(wmsg->command, cur, WOOMERA_STRLEN);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Malformed Message!\n");
				break;
			}
		} else {
			char *name, *val;
			name = cur;
			if ((val = strchr(name, ':')) != 0) {
				*val = '\0';
				val++;
				while (*val == ' ') {
					*val = '\0';
					val++;
				}
				strncpy(wmsg->values[wmsg->last - 1], val, WOOMERA_STRLEN);
			}
			strncpy(wmsg->names[wmsg->last - 1], name, WOOMERA_STRLEN);
			if (name && val && !strcasecmp(name, "content-type")) {
				switch_set_flag(wmsg, WFLAG_CONTENT);
				bytes = atoi(val);
			}

		}
		wmsg->last++;
	}

	wmsg->last--;

	if (bytes && switch_test_flag(wmsg, WFLAG_CONTENT)) {
		size_t len = (bytes > sizeof(wmsg->body)) ? sizeof(wmsg->body) : bytes;
		switch_socket_recv(fd, wmsg->body, &len);

		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s\n", wmsg->body);
		}
	}

	if (event_queue && switch_test_flag(wmsg, WFLAG_EVENT)) {
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Queue Event: {%s} [%s]\n", profile->name, wmsg->command);
		}
		/* we don't want events we want a reply so we will stash them for later */
		woomera_enqueue_event(event_queue, wmsg);

		/* call ourself recursively to find the reply. we'll keep doing this as long we get events.
		 * wmsg will be overwritten but it's ok we just queued it.
		 */
		return woomera_message_parse(fd, wmsg, timeout, profile, event_queue);

	} else if (wmsg->mval > 99 && wmsg->mval < 200) {
		/* reply in the 100's are nice but we need to wait for another reply 
		   call ourself recursively to find the reply > 199 and forget this reply.
		 */
		return woomera_message_parse(fd, wmsg, timeout, profile, event_queue);
	} else {
		return switch_test_flag(wmsg, WFLAG_EXISTS);
	}
}


static int connect_woomera(switch_socket_t ** new_sock, woomera_profile * profile, int flags)
{

	switch_sockaddr_t *sa;

	if (switch_sockaddr_info_get(&sa, profile->woomera_host, AF_INET, profile->woomera_port, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}

	if (switch_socket_create(new_sock, AF_INET, SOCK_STREAM, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}

	switch_socket_timeout_set((*new_sock), 10000000);
	switch_socket_opt_set((*new_sock), SWITCH_SO_KEEPALIVE, 1);

	/*
	   status = switch_socket_bind((*new_sock), sa);
	   if (0 && status != SWITCH_STATUS_SUCCESS) {
	   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Can't Bind to %s:%d!\n", profile->woomera_host, profile->woomera_port);
	   return -1;
	   }
	 */
	if (switch_socket_connect((*new_sock), sa) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}


	return 1;
}

static int woomera_profile_thread_running(woomera_profile * profile, int set, int new)
{
	int running = 0;

	switch_mutex_lock(profile->iolock);
	if (set) {
		profile->thread_running = new;
	}
	running = profile->thread_running;
	switch_mutex_unlock(profile->iolock);
	return running;

}

static int woomera_locate_socket(woomera_profile * profile, switch_socket_t ** woomera_socket)
{
	woomera_message wmsg;

	for (;;) {

		while (connect_woomera(woomera_socket, profile, 0) < 0) {
			if (!woomera_profile_thread_running(profile, 0, 0)) {
				break;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "{%s} Cannot Reconnect to Woomera! retry in 5 seconds\n", profile->name);
			switch_sleep(WOOMERA_RECONNECT_TIME);
		}

		if (*woomera_socket) {
			if (switch_test_flag(profile, PFLAG_INBOUND)) {
				woomera_printf(profile, *woomera_socket, "LISTEN%s", WOOMERA_RECORD_SEPERATOR);
				if (woomera_message_parse(*woomera_socket, &wmsg, WOOMERA_HARD_TIMEOUT, profile, &profile->event_queue) < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "{%s} HELP! Woomera is broken!\n", profile->name);
					globals.panic = 1;
					woomera_profile_thread_running(&default_profile, 1, 0);
					switch_sleep(WOOMERA_RECONNECT_TIME);
					if (*woomera_socket) {
						woomera_socket_close(woomera_socket);
					}

					continue;
				}
			}

		}
		switch_sleep(100);
		break;
	}
	return *woomera_socket ? 1 : 0;
}



static int tech_create_read_socket(private_object * tech_pvt)
{
	switch_memory_pool_t *pool = switch_core_session_get_pool(tech_pvt->session);

	if ((tech_pvt->port = globals.next_woomera_port++) >= WOOMERA_MAX_PORT) {
		tech_pvt->port = globals.next_woomera_port = WOOMERA_MIN_PORT;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "connect %s:%d\n", tech_pvt->profile->audio_ip, tech_pvt->port);
	//tech_pvt->udp_socket = create_udp_socket(tech_pvt->profile->audio_ip, tech_pvt->port, &tech_pvt->udpread, 0);

	switch_sockaddr_info_get(&tech_pvt->udpread, tech_pvt->profile->audio_ip, SWITCH_UNSPEC, tech_pvt->port, 0, pool);
	if (switch_socket_create(&tech_pvt->udp_socket, AF_INET, SOCK_DGRAM, 0, pool) == SWITCH_STATUS_SUCCESS) {
		switch_socket_bind(tech_pvt->udp_socket, tech_pvt->udpread);
		switch_socket_create_pollfd(&tech_pvt->read_poll, tech_pvt->udp_socket, SWITCH_POLLIN | SWITCH_POLLERR, pool);
		switch_socket_create_pollfd(&tech_pvt->write_poll, tech_pvt->udp_socket, SWITCH_POLLOUT | SWITCH_POLLERR, pool);
		switch_set_flag_locked(tech_pvt, TFLAG_MEDIA);
	} else {
		switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
	}
	
	return 0;
}


static int tech_activate(private_object * tech_pvt)
{
	woomera_message wmsg;

	if (tech_pvt) {
		if ((connect_woomera(&tech_pvt->command_channel, tech_pvt->profile, 0))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "connected to woomera!\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't connect to woomera!\n");
			switch_sleep(WOOMERA_RECONNECT_TIME);
			return -1;
		}

		if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {

			woomera_printf(tech_pvt->profile,
						   tech_pvt->command_channel,
						   "CALL %s%sRaw-Audio: %s/%d%sLocal-Name: %s!%s%s",
						   tech_pvt->caller_profile->destination_number,
						   WOOMERA_LINE_SEPERATOR,
						   tech_pvt->profile->audio_ip,
						   tech_pvt->port,
						   WOOMERA_LINE_SEPERATOR,
						   tech_pvt->caller_profile->caller_id_name, tech_pvt->caller_profile->caller_id_number, WOOMERA_RECORD_SEPERATOR);

			woomera_message_parse(tech_pvt->command_channel, &wmsg, WOOMERA_HARD_TIMEOUT, tech_pvt->profile, &tech_pvt->event_queue);
		} else {
			switch_set_flag_locked(tech_pvt, TFLAG_PARSE_INCOMING);
			woomera_printf(tech_pvt->profile, tech_pvt->command_channel, "LISTEN%s", WOOMERA_RECORD_SEPERATOR);
			if (woomera_message_parse(tech_pvt->command_channel, &wmsg, WOOMERA_HARD_TIMEOUT, tech_pvt->profile, &tech_pvt->event_queue) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "{%s} HELP! Woomera is broken!\n", tech_pvt->profile->name);
				switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
				globals.panic = 1;
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Where's my tech_pvt?\n");
	}

	return 0;
}



static void *woomera_channel_thread_run(switch_thread_t * thread, void *obj)
{
	switch_core_session_t *session = obj;
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;
	woomera_message wmsg;
	int res = 0;

	switch_assert(session != NULL);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!tech_pvt->udp_socket) {
		tech_create_read_socket(tech_pvt);
	}


	for (;;) {
		if (globals.panic) {
			switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
		}

		if (switch_test_flag(tech_pvt, TFLAG_ABORT)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			udp_socket_close(tech_pvt);
			break;
		}

		if (switch_test_flag(tech_pvt, TFLAG_ACTIVATE)) {
			switch_clear_flag_locked(tech_pvt, TFLAG_ACTIVATE);
			tech_activate(tech_pvt);
		}

		if (switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
			switch_clear_flag_locked(tech_pvt, TFLAG_ANSWER);
#ifdef USE_ANSWER
			woomera_printf(tech_pvt->profile, tech_pvt->command_channel, "ANSWER %s%s", tech_pvt->call_info.callid, WOOMERA_RECORD_SEPERATOR);
			if (woomera_message_parse(tech_pvt->command_channel, &wmsg, WOOMERA_HARD_TIMEOUT, tech_pvt->profile, &tech_pvt->event_queue) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "{%s} HELP! Woomera is broken!\n", tech_pvt->profile->name);
				switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
				globals.panic = 1;
				continue;
			}
#endif
		}

		if (switch_test_flag(tech_pvt, TFLAG_DTMF)) {
			switch_mutex_lock(tech_pvt->iolock);
			woomera_printf(tech_pvt->profile, tech_pvt->command_channel, "DTMF %s %s%s", tech_pvt->call_info.callid,
						   tech_pvt->dtmfbuf, WOOMERA_RECORD_SEPERATOR);
			if (woomera_message_parse(tech_pvt->command_channel, &wmsg, WOOMERA_HARD_TIMEOUT, tech_pvt->profile, &tech_pvt->event_queue) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "{%s} HELP! Woomera is broken!\n", tech_pvt->profile->name);
				switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
				globals.panic = 1;
				continue;
			}
			switch_clear_flag_locked(tech_pvt, TFLAG_DTMF);
			memset(tech_pvt->dtmfbuf, 0, sizeof(tech_pvt->dtmfbuf));
			switch_mutex_unlock(tech_pvt->iolock);
		}
#if 1==0						/*convert to use switch_time_now */
		if (tech_pvt->timeout) {
			struct timeval now;
			int elapsed;
			gettimeofday(&now, NULL);
			elapsed = (((now.tv_sec * 1000) + now.tv_usec / 1000) - ((tech_pvt->started.tv_sec * 1000) + tech_pvt->started.tv_usec / 1000));
			if (elapsed > tech_pvt->timeout) {
				/* call timed out! */
				switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
			}
		}
#endif

		if (!tech_pvt->command_channel) {
			break;
		}
		/* Check for events */
		if ((res = woomera_dequeue_event(&tech_pvt->event_queue, &wmsg)) != 0 ||
			(res = woomera_message_parse(tech_pvt->command_channel, &wmsg, 100, tech_pvt->profile, NULL)) != 0) {

			if (res < 0 || !strcasecmp(wmsg.command, "HANGUP")) {
				switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
				continue;
			} else if (!strcasecmp(wmsg.command, "DTMF")) {
				/*
				   struct ast_frame dtmf_frame = {AST_FRAME_DTMF};
				   int x = 0;
				   for (x = 0; x < strlen(wmsg.command_args); x++) {
				   dtmf_frame.subclass = wmsg.command_args[x];
				   ast_queue_frame(tech_pvt->owner, ast_frdup(&dtmf_frame));
				   if (globals.debug > 1) {
				   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, WOOMERA_DEBUG_PREFIX "SEND DTMF [%c] to %s\n", dtmf_frame.subclass, tech_pvt->owner->name);
				   }
				   }
				 */
			} else if (!strcasecmp(wmsg.command, "PROCEED")) {
				/* This packet has lots of info so well keep it */
				tech_pvt->call_info = wmsg;
				switch_core_session_queue_indication(tech_pvt->session, SWITCH_MESSAGE_INDICATE_RINGING);
				switch_channel_mark_ring_ready(channel);
			} else if (switch_test_flag(tech_pvt, TFLAG_PARSE_INCOMING) && !strcasecmp(wmsg.command, "INCOMING")) {
				char *exten;
				char cid_name[512];
				char *cid_num;
				char *ip;
				char *p;
				switch_clear_flag_locked(tech_pvt, TFLAG_PARSE_INCOMING);
				switch_set_flag_locked(tech_pvt, TFLAG_INCOMING);
				tech_pvt->call_info = wmsg;

				exten = woomera_message_header(&wmsg, "Local-Number");
				if (switch_strlen_zero(exten)) {
					exten = "s";
				}

				if ((p = woomera_message_header(&wmsg, "Remote-Name")) != 0) {
					strncpy(cid_name, p, sizeof(cid_name));
				}

				if ((cid_num = strchr(cid_name, '!')) != 0) {
					*cid_num = '\0';
					cid_num++;
				} else {
					cid_num = woomera_message_header(&wmsg, "Remote-Number");
				}
				ip = woomera_message_header(&wmsg, "Remote-Address");

				if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																		  NULL,
																		  tech_pvt->profile->dialplan,
																		  cid_name,
																		  cid_num,
																		  ip,
																		  NULL,
																		  NULL,
																		  NULL,
																		  modname,
																		  NULL,
																		  exten)) != 0) {
					char name[128];
					switch_snprintf(name, sizeof(name), "Woomera/%s-%04x", tech_pvt->caller_profile->destination_number, rand() & 0xffff);
					switch_channel_set_name(channel, name);
					switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

				}

				woomera_printf(tech_pvt->profile, tech_pvt->command_channel,
							   "%s %s%s"
							   "Raw-Audio: %s/%d%s",
							   MEDIA_ANSWER, wmsg.callid, WOOMERA_LINE_SEPERATOR, tech_pvt->profile->audio_ip, tech_pvt->port, WOOMERA_RECORD_SEPERATOR);

				if (woomera_message_parse(tech_pvt->command_channel, &wmsg, WOOMERA_HARD_TIMEOUT, tech_pvt->profile, &tech_pvt->event_queue) < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "{%s} HELP! Woomera is broken!\n", tech_pvt->profile->name);
					switch_set_flag_locked(tech_pvt, TFLAG_ABORT);
					globals.panic = 1;
					continue;
				}

			} else if (!strcasecmp(wmsg.command, "CONNECT")) {

			} else if (!strcasecmp(wmsg.command, "MEDIA")) {
				char *raw_audio_header;

				if ((raw_audio_header = woomera_message_header(&wmsg, "Raw-Audio")) != 0) {
					char ip[25];
					char *ptr;
					switch_port_t port = 0;

					strncpy(ip, raw_audio_header, sizeof(ip) - 1);
					if ((ptr = strchr(ip, '/')) != 0) {
						*ptr = '\0';
						ptr++;
						port = (switch_port_t) atoi(ptr);
					}
					/* Move Channel's State Machine to RING */
					switch_channel_answer(channel);
					switch_channel_set_state(channel, CS_RING);

					if (switch_sockaddr_info_get(&tech_pvt->udpwrite,
												 ip, SWITCH_UNSPEC, port, 0, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
						if (globals.debug) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
											  WOOMERA_DEBUG_PREFIX "{%s} Cannot resolve %s\n", tech_pvt->profile->name, ip);
						}
						switch_channel_hangup(channel, SWITCH_CAUSE_NETWORK_OUT_OF_ORDER);
					}
				}
			}
		}
		if (globals.debug > 2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, WOOMERA_DEBUG_PREFIX "CHECK {%s}(%d)\n", tech_pvt->profile->name, res);
		}
	}
	if (globals.debug > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, WOOMERA_DEBUG_PREFIX "Monitor thread for %s done.\n", tech_pvt->profile->name);
	}

	return NULL;
}




static void *woomera_thread_run(void *obj)
{

	int res = 0;
	woomera_message wmsg;
	woomera_profile *profile;

	profile = obj;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Started Woomera Thread {%s}.\n", profile->name);

	profile->thread_running = 1;
	profile->woomera_socket = NULL;


	while (woomera_profile_thread_running(profile, 0, 0)) {
		/* listen on socket and handle events */
		if (globals.panic == 2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Woomera is disabled!\n");
			switch_sleep(WOOMERA_RECONNECT_TIME);
			continue;
		}

		if (!profile->woomera_socket) {
			if (woomera_locate_socket(profile, &profile->woomera_socket)) {
				globals.panic = 0;
			}
			if (!woomera_profile_thread_running(profile, 0, 0)) {
				break;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Woomera Thread Up {%s} %s/%d\n", profile->name,
							  profile->woomera_host, profile->woomera_port);

		}

		if (globals.panic) {
			if (globals.panic != 2) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "Help I'm in a state of panic!\n");
				globals.panic = 0;	//fix
			}
			woomera_socket_close(&profile->woomera_socket);


			continue;
		}

		if ((((res = woomera_dequeue_event(&profile->event_queue, &wmsg)) != 0) || ((res = woomera_message_parse(profile->woomera_socket, &wmsg,
																												 /* if we are not stingy with threads we can block forever */
																												 0, profile, NULL))) != 0)) {
			if (res < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "{%s} HELP! I lost my connection to woomera!\n", profile->name);
				woomera_socket_close(&profile->woomera_socket);

				//global_set_flag(TFLAG_ABORT);
				globals.panic = 1;
				continue;

				/* Can't get to the following code --Commented out for now. */
/*				if (profile->woomera_socket) 
					if (switch_test_flag(profile, PFLAG_INBOUND)) {
						woomera_printf(profile, profile->woomera_socket, "LISTEN%s", WOOMERA_RECORD_SEPERATOR);
						if (woomera_message_parse(profile->woomera_socket,
												  &wmsg, WOOMERA_HARD_TIMEOUT, profile, &profile->event_queue) < 0) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "{%s} HELP! Woomera is broken!\n",
												  profile->name);
							globals.panic = 1;
							woomera_socket_close(&profile->woomera_socket);
						}
					}
					if (profile->woomera_socket) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Woomera Thread Up {%s} %s/%d\n", profile->name,
											  profile->woomera_host, profile->woomera_port);
					}
				}*/
				//continue;
			}

			if (!strcasecmp(wmsg.command, "INCOMING")) {
				char *name;
				switch_core_session_t *session;

				if ((name = woomera_message_header(&wmsg, "Remote-Address")) == 0) {
					name = woomera_message_header(&wmsg, "Channel-Name");
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "New Inbound Channel %s!\n", name);
				if ((session = switch_core_session_request(woomera_endpoint_interface, NULL)) != 0) {
					struct private_object *tech_pvt;
					switch_channel_t *channel;

					switch_core_session_add_stream(session, NULL);

					if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object))) != 0) {
						memset(tech_pvt, 0, sizeof(*tech_pvt));
						switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
						tech_pvt->profile = &default_profile;
						channel = switch_core_session_get_channel(session);
						switch_core_session_set_private(session, tech_pvt);
						tech_pvt->session = session;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
						switch_core_session_destroy(&session);
						break;
					}
					switch_channel_set_state(channel, CS_INIT);
					if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
						switch_core_session_destroy(&session);
						break;
					}
				}
			}
		}

		if (globals.debug > 2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Main Thread {%s} Select Return %d\n", profile->name, res);
		}

		switch_yield(100);
	}


	if (profile->woomera_socket) {
		woomera_printf(profile, profile->woomera_socket, "BYE%s", WOOMERA_RECORD_SEPERATOR);
		woomera_socket_close(&profile->woomera_socket);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ended Woomera Thread {%s}.\n", profile->name);
	woomera_profile_thread_running(profile, 1, -1);
	return NULL;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_woomera_runtime)
{

	woomera_thread_run(&default_profile);

	return SWITCH_STATUS_TERM;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_woomera_shutdown)
{
	int x = 0;
	woomera_profile_thread_running(&default_profile, 1, 0);
	while (!woomera_profile_thread_running(&default_profile, 0, 0)) {
		woomera_socket_close(&default_profile.woomera_socket);
		if (x++ > 10) {
			break;
		}
		switch_yield(1);
	}
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_woomera_load)
{
	struct woomera_profile *profile = &default_profile;
	char *cf = "woomera.conf";
	switch_xml_t cfg, xml, settings, param, xmlp;

	memset(&globals, 0, sizeof(globals));
	globals.next_woomera_port = WOOMERA_MIN_PORT;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	switch_set_flag(profile, PFLAG_INBOUND | PFLAG_OUTBOUND);
	profile->name = "main";
	strncpy(profile->dialplan, "default", sizeof(profile->dialplan) - 1);
	strncpy(profile->audio_ip, "127.0.0.1", sizeof(profile->audio_ip) - 1);
	strncpy(profile->woomera_host, "127.0.0.1", sizeof(profile->woomera_host) - 1);
	profile->woomera_port = (switch_port_t) 42420;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "noload") && atoi(val)) {
				return SWITCH_STATUS_TERM;
			}
			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			}
		}
	}

	for (xmlp = switch_xml_child(cfg, "interface"); xmlp; xmlp = xmlp->next) {
		for (param = switch_xml_child(xmlp, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcmp(var, "audio-ip")) {
				strncpy(profile->audio_ip, val, sizeof(profile->audio_ip) - 1);
			} else if (!strcmp(var, "host")) {
				strncpy(profile->woomera_host, val, sizeof(profile->woomera_host) - 1);
			} else if (!strcmp(var, "port")) {
				profile->woomera_port = (switch_port_t) atoi(val);
			} else if (!strcmp(var, "disabled")) {
				if (atoi(val) > 0) {
					switch_set_flag(profile, PFLAG_DISABLED);
				}
			} else if (!strcmp(var, "inbound")) {
				if (atoi(val) < 1) {
					switch_clear_flag(profile, PFLAG_INBOUND);
				}
			} else if (!strcmp(var, "outbound")) {
				if (atoi(val) < 1) {
					switch_clear_flag(profile, PFLAG_OUTBOUND);
				}
			} else if (!strcmp(var, "dialplan")) {
				strncpy(profile->dialplan, val, sizeof(profile->dialplan) - 1);
			}
		}
	}

	switch_xml_free(xml);

	module_pool = pool;

	if (switch_mutex_init(&default_profile.iolock, SWITCH_MUTEX_NESTED, module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no lock\n");
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	woomera_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	woomera_endpoint_interface->interface_name = "woomera";
	woomera_endpoint_interface->io_routines = &woomera_io_routines;
	woomera_endpoint_interface->state_handler = &woomera_event_handlers;

	/* indicate that the module should continue to be loaded */
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
