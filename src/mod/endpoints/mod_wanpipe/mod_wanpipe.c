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
 * mod_wanpipe.c -- WANPIPE PRI Channel Module
 *
 */

#include <switch.h>
#include <libsangoma.h>
#include <sangoma_pri.h>
#include <libteletone.h>
#include <ss7boost_client.h>

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE -1
#endif

//#define DOTRACE
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_wanpipe_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_wanpipe_load);
SWITCH_MODULE_DEFINITION(mod_wanpipe, mod_wanpipe_load, mod_wanpipe_shutdown, NULL);

#define STRLEN 15

switch_endpoint_interface_t *wanpipe_endpoint_interface;
static switch_memory_pool_t *module_pool = NULL;

typedef enum {
	PFLAG_ANSWER = (1 << 0),
	PFLAG_HANGUP = (1 << 1),
} PFLAGS;


typedef enum {
	PPFLAG_RING = (1 << 0),
} PPFLAGS;

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
	TFLAG_NOSIG = (1 << 10),
	TFLAG_BYE = (1 << 11),
	TFLAG_CODEC = (1 << 12),
	TFLAG_HANGUP = (1 << 13)
} TFLAGS;


#define DEFAULT_SAMPLES_PER_FRAME 160
#define MAX_SPANS 128

struct channel_map {
	char map[SANGOMA_MAX_CHAN_PER_SPAN][SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_mutex_t *mutex;
};

unsigned int txseq=0;
unsigned int rxseq=0;

#define SETUP_LEN CORE_MAX_CHAN_PER_SPAN*CORE_MAX_SPANS+1

struct ss7boost_handle {
	char *local_ip;
	char *remote_ip;
	int local_port;
	int remote_port;
	ss7boost_client_connection_t mcon;
	switch_mutex_t *mutex;
	struct channel_map span_chanmap[MAX_SPANS];
	char setup_array[SETUP_LEN][SWITCH_UUID_FORMATTED_LENGTH + 1];
	uint32_t setup_index;
};

typedef struct ss7boost_handle ss7boost_handle_t;

static int isup_exec_command(ss7boost_handle_t *ss7boost_handle, int span, int chan, int id, int cmd, int cause);

static struct {
	int debug;
	int panic;
	uint32_t samples_per_frame;
	int dtmf_on;
	int dtmf_off;
	int suppress_dtmf_tone;
	int ignore_dtmf_tone;
	int configured_spans;
	int configured_boost_spans;
	char *dialplan;
	char *context;
	switch_hash_t *call_hash;
	switch_mutex_t *hash_mutex;
	switch_mutex_t *channel_mutex;
	ss7boost_handle_t *ss7boost_handle;
	uint32_t fxo_index;
	uint32_t fxs_index;
} globals;

struct wanpipe_pri_span {
	int span;
	int dchan;
	unsigned int bchans;
	int node;
	int pswitch;
	char *dialplan;
	unsigned int l1;
	unsigned int dp;
	struct sangoma_pri spri;
};

struct wpsock {
	sng_fd_t fd;
	char *name;
};

typedef struct wpsock wpsock_t;

typedef enum {
	ANALOG_TYPE_UNKNOWN,
	ANALOG_TYPE_PHONE_FXS,
	ANLOG_TYPE_LINE_FXO
} analog_type_t;

typedef enum {
	ANALOG_STATE_DOWN,
	ANALOG_STATE_ONHOOK,
	ANALOG_STATE_OFFHOOK,
	ANALOG_STATE_RING
} analog_state_t;

struct analog_channel {
	analog_type_t a_type;
	analog_state_t state;
	wpsock_t *sock;
	int chan;
	int span;
	char *device;
	char *user;
	char *domain;
	char *cid_name;
	char *cid_num;
};
typedef struct analog_channel analog_channel_t;

#define MAX_ANALOG_CHANNELS 128
static struct analog_channel *FXS_ANALOG_CHANNELS[MAX_ANALOG_CHANNELS];
static struct analog_channel *FXO_ANALOG_CHANNELS[MAX_ANALOG_CHANNELS];

static struct wanpipe_pri_span *SPANS[MAX_SPANS];

struct private_object {
	unsigned int flags;			/* FLAGS */
	switch_frame_t read_frame;	/* Frame for Writing */
	switch_core_session_t *session;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	unsigned char auxbuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	struct sangoma_pri *spri;
	sangoma_api_hdr_t hdrframe;
	switch_caller_profile_t *caller_profile;
	wpsock_t *wpsock;
	int callno;
	int span;
	int cause;
	q931_call *call;
	teletone_dtmf_detect_state_t dtmf_detect;
	teletone_generation_session_t tone_session;
	switch_buffer_t *dtmf_buffer;
	unsigned int skip_read_frames;
	unsigned int skip_write_frames;
	switch_mutex_t *flag_mutex;
	int frame_size;
	ss7boost_handle_t *ss7boost_handle;
	int boost_chan_number;
	int boost_span_number;
	int boost_trunk_group;
	uint32_t setup_index;
	uint32_t boost_pres;
#ifdef DOTRACE
	int fd;
	int fd2;
#endif
};
typedef struct private_object private_object_t;



static void wp_logger(char *file, const char *func, int line, int level, char *fmt, ...)
{
	va_list ap;
	char *data = NULL;
	int ret;

	va_start(ap, fmt);
	if ((ret = switch_vasprintf(&data, fmt, ap)) != -1) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, level, "%s", data);
	}
	va_end(ap);
}

static int local_sangoma_tdm_read_event(sng_fd_t fd, wp_tdm_api_rx_hdr_t *rx_event)
{
	wanpipe_tdm_api_t tdm_api[1];
	
#if defined(WIN32)
    rx_event = &last_tdm_api_event_buffer;
#else
    int err;

    tdm_api->wp_tdm_cmd.cmd = SIOC_WP_TDM_READ_EVENT;

    if ((err = sangoma_tdm_cmd_exec(fd, tdm_api))) {
        return err;
    }

    rx_event = &tdm_api->wp_tdm_cmd.event;
#endif

	return 0;
}

static int analog_set_state(analog_channel_t *alc, analog_state_t state)
{
	alc->state = state;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Changing State to %d\n", state);
}

static void analog_check_state(analog_channel_t *alc)
{
	wanpipe_tdm_api_t tdm_api;

	switch(alc->state) {
	case ANALOG_STATE_DOWN:
		sangoma_tdm_enable_rxhook_events(alc->sock->fd, &tdm_api);
		analog_set_state(alc, ANALOG_STATE_ONHOOK);
		break;
	default:
		break;
	}
}

static void analog_parse_event(analog_channel_t *alc)
{
	wp_tdm_api_rx_hdr_t rx_event;
	int err = local_sangoma_tdm_read_event(alc->sock->fd, &rx_event);
	
	if (err < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error reading event!\n");
		return;
	}

	switch (rx_event.wp_tdm_api_event_type) {
	case WP_TDM_EVENT_RXHOOK:
		printf("hook\n");
		break;
	}

}

static void *SWITCH_THREAD_FUNC fxs_thread_run(switch_thread_t *thread, void *obj)
{

	for(;;) {
		int i = 0, sel_on = -1;
		fd_set oob;
		FD_ZERO(&oob);
		
		for(i = 0; i < globals.fxs_index; i++) {
			int fd;
			assert(FXS_ANALOG_CHANNELS[i]);
			assert(FXS_ANALOG_CHANNELS[i]->sock);

			fd = FXS_ANALOG_CHANNELS[i]->sock->fd;
			
			analog_check_state(FXS_ANALOG_CHANNELS[i]);

			FD_SET(fd, &oob);

			if (fd > sel_on) {
				sel_on = fd;
			}
		}

		if (sel_on > -1) {
			if (select(++sel_on, NULL, NULL, &oob, NULL)) {
				for(i = 0; i < globals.fxs_index; i++) {
					int fd = FXS_ANALOG_CHANNELS[i]->sock->fd;
					if (FD_ISSET(fd, &oob)) {
						analog_parse_event(FXS_ANALOG_CHANNELS[i]);
					}
				}
			}
		}
	}
}


static wpsock_t *wp_open(int span, int chan)
{
	sng_fd_t fd;
	wpsock_t *sock;
	char name[25];
	
	snprintf(name, sizeof(name), "s%dc%d", span, chan);

	switch_mutex_lock(globals.hash_mutex);
	if ((sock = switch_core_hash_find(globals.call_hash, name))) {
		fd = sock->fd;
	} else {
		if ((fd = sangoma_open_tdmapi_span_chan(span, chan)) != INVALID_HANDLE_VALUE) {
			if ((sock = malloc(sizeof(*sock)))) {
				memset(sock, 0, sizeof(*sock));
				sock->fd = fd;
				sock->name = strdup(name);
				switch_core_hash_insert(globals.call_hash, sock->name, sock);
			}
		}
	}

	switch_mutex_unlock(globals.hash_mutex);

	return sock;
}

static int wp_restart(int span, int chan)
{
	wpsock_t *sock;
	sng_fd_t fd;
	char name[25];
	
	snprintf(name, sizeof(name), "s%dc%d", span, chan);

	switch_mutex_lock(globals.hash_mutex);
	if ((sock = switch_core_hash_find(globals.call_hash, name))) {
		switch_core_hash_delete(globals.call_hash, sock->name);
		fd = sock->fd;
	} else {
		fd = sangoma_open_tdmapi_span_chan(span, chan);
	}
	switch_mutex_unlock(globals.hash_mutex);

	if (fd != INVALID_HANDLE_VALUE) {
		sangoma_socket_close(&fd);
		return 0;
	}
	
	return -1;
}


static void set_global_dialplan(char *dialplan)
{
	if (globals.dialplan) {
		free(globals.dialplan);
		globals.dialplan = NULL;
	}

	globals.dialplan = strdup(dialplan);
}

static void set_global_context(char *context)
{
	if (globals.context) {
		free(globals.context);
		globals.context = NULL;
	}

	globals.context = strdup(context);
}


static int str2node(char *node)
{
	if (!strcasecmp(node, "cpe"))
		return PRI_CPE;
	if (!strcasecmp(node, "network"))
		return PRI_NETWORK;
	return -1;
}

static int str2switch(char *swtype)
{
	if (!strcasecmp(swtype, "ni1"))
		return PRI_SWITCH_NI1;
	if (!strcasecmp(swtype, "ni2"))
		return PRI_SWITCH_NI2;
	if (!strcasecmp(swtype, "dms100"))
		return PRI_SWITCH_DMS100;
	if (!strcasecmp(swtype, "lucent5e"))
		return PRI_SWITCH_LUCENT5E;
	if (!strcasecmp(swtype, "att4ess"))
		return PRI_SWITCH_ATT4ESS;
	if (!strcasecmp(swtype, "euroisdn"))
		return PRI_SWITCH_EUROISDN_E1;
	if (!strcasecmp(swtype, "gr303eoc"))
		return PRI_SWITCH_GR303_EOC;
	if (!strcasecmp(swtype, "gr303tmc"))
		return PRI_SWITCH_GR303_TMC;
	return -1;
}


static int str2l1(char *l1)
{
	if (!strcasecmp(l1, "alaw"))
		return PRI_LAYER_1_ALAW;

	return PRI_LAYER_1_ULAW;
}

static int str2dp(char *dp)
{
	if (!strcasecmp(dp, "international"))
		return PRI_INTERNATIONAL_ISDN;
	if (!strcasecmp(dp, "national"))
		return PRI_NATIONAL_ISDN;
	if (!strcasecmp(dp, "local"))
		return PRI_LOCAL_ISDN;
	if (!strcasecmp(dp, "private"))
		return PRI_PRIVATE;		
	if (!strcasecmp(dp, "unknown"))
		return PRI_UNKNOWN;

	return PRI_UNKNOWN;
}



static void set_global_dialplan(char *dialplan);
static void set_global_context(char *context);
static int str2node(char *node);
static int str2switch(char *swtype);
static switch_status_t wanpipe_on_init(switch_core_session_t *session);
static switch_status_t wanpipe_on_hangup(switch_core_session_t *session);
static switch_status_t wanpipe_on_exchange_media(switch_core_session_t *session);
static switch_status_t wanpipe_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t wanpipe_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags);
static switch_status_t wanpipe_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
										switch_io_flag_t flags, int stream_id);
static switch_status_t wanpipe_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
										 switch_io_flag_t flags, int stream_id);
static int on_info(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent);
static int on_hangup(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent);
static int on_routing(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent);
static int check_flags(struct sangoma_pri *spri);
static int on_restart(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent);
static int on_anything(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent);
static void *SWITCH_THREAD_FUNC pri_thread_run(switch_thread_t *thread, void *obj);
static switch_status_t config_wanpipe(int reload);


static switch_status_t wanpipe_codec_init(private_object_t *tech_pvt)
{
	int err = 0;
	wanpipe_tdm_api_t tdm_api = {{0}};
	unsigned int rate = 8000;
	switch_channel_t *channel = NULL;

	if (switch_test_flag(tech_pvt, TFLAG_CODEC)) {
		return SWITCH_STATUS_SUCCESS;
	}


	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	err = sangoma_tdm_set_codec(tech_pvt->wpsock->fd, &tdm_api, WP_SLINEAR);
	
	sangoma_tdm_set_usr_period(tech_pvt->wpsock->fd, &tdm_api, globals.samples_per_frame / 8);
	tech_pvt->frame_size = sangoma_tdm_get_usr_mtu_mru(tech_pvt->wpsock->fd, &tdm_api);

	if (switch_core_codec_init
		(&tech_pvt->read_codec, "L16", NULL, rate, globals.samples_per_frame / 8, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
		 switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Cannot set read codec\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init
		(&tech_pvt->write_codec, "L16", NULL, rate, globals.samples_per_frame / 8, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
		 switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Cannot set read codec\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}
	tech_pvt->read_frame.rate = rate;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);

#ifdef DOTRACE	
						tech_pvt->fd = open("/tmp/wp-in.raw", O_WRONLY | O_TRUNC | O_CREAT);
						tech_pvt->fd2 = open("/tmp/wp-out.raw", O_WRONLY | O_TRUNC | O_CREAT);
#endif

	/* Setup artificial DTMF stuff */
	memset(&tech_pvt->tone_session, 0, sizeof(tech_pvt->tone_session));
	teletone_init_session(&tech_pvt->tone_session, 1024, NULL, NULL);
	
	if (globals.debug) {
		tech_pvt->tone_session.debug = globals.debug;
		tech_pvt->tone_session.debug_stream = stdout;
	}
	
	tech_pvt->tone_session.rate = rate;
	tech_pvt->tone_session.duration = globals.dtmf_on * (tech_pvt->tone_session.rate / 1000);
	tech_pvt->tone_session.wait = globals.dtmf_off * (tech_pvt->tone_session.rate / 1000);

	if (!globals.ignore_dtmf_tone) {	
		teletone_dtmf_detect_init (&tech_pvt->dtmf_detect, rate);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Audio init %s\n", switch_channel_get_name(channel));

	switch_set_flag(tech_pvt, TFLAG_CODEC);

	return SWITCH_STATUS_SUCCESS;

}


/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t wanpipe_on_init(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_status_t status;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	tech_pvt->read_frame.data = tech_pvt->databuf;

	if (tech_pvt->ss7boost_handle)  {
		if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
			ss7boost_client_event_t event;

			event.calling_number_presentation = tech_pvt->boost_pres;
			event.trunk_group = tech_pvt->boost_trunk_group;
		
			switch_mutex_lock(tech_pvt->ss7boost_handle->mutex);
			tech_pvt->setup_index = ++tech_pvt->ss7boost_handle->setup_index;
			if (tech_pvt->ss7boost_handle->setup_index == SETUP_LEN - 1) {
				tech_pvt->ss7boost_handle->setup_index = 0;
			}
			switch_mutex_unlock(tech_pvt->ss7boost_handle->mutex);
		
			switch_copy_string(tech_pvt->ss7boost_handle->setup_array[tech_pvt->setup_index],
							   switch_core_session_get_uuid(session), 
							   sizeof(tech_pvt->ss7boost_handle->setup_array[tech_pvt->setup_index]));
		
			
			ss7boost_client_call_init(&event, tech_pvt->caller_profile->caller_id_number, tech_pvt->caller_profile->destination_number, tech_pvt->setup_index);
			
			if (ss7boost_client_connection_write(&tech_pvt->ss7boost_handle->mcon, &event) <= 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Critical System Error: Failed to tx on ISUP socket [%s]\n", strerror(errno));
			}
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Call Called Event TG=%d\n", tech_pvt->boost_trunk_group);
			goto done;
		}
	} 

	if ((status = wanpipe_codec_init(tech_pvt)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return status;
	}
	
	if (switch_test_flag(tech_pvt, TFLAG_NOSIG)) {
		switch_channel_mark_answered(channel);
	}

 done:

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(channel, CS_ROUTING);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t wanpipe_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WANPIPE RING\n");

	

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t wanpipe_on_hangup(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	struct channel_map *chanmap = NULL;

	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_core_codec_destroy(&tech_pvt->read_codec);
	switch_core_codec_destroy(&tech_pvt->write_codec);


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WANPIPE HANGUP\n");

	if (tech_pvt->ss7boost_handle) {
		switch_mutex_lock(tech_pvt->ss7boost_handle->mutex);
		*tech_pvt->ss7boost_handle->span_chanmap[tech_pvt->boost_span_number].map[tech_pvt->boost_chan_number] = '\0';
		switch_mutex_unlock(tech_pvt->ss7boost_handle->mutex);
		if (!switch_test_flag(tech_pvt, TFLAG_BYE)) {
			isup_exec_command(tech_pvt->ss7boost_handle,
							  tech_pvt->boost_span_number,
							  tech_pvt->boost_chan_number,
							  -1,
							  SIGBOOST_EVENT_CALL_STOPPED,
							  SIGBOOST_RELEASE_CAUSE_NORMAL);
		}
	} else if (tech_pvt->spri) {
		chanmap = tech_pvt->spri->private_info;

		if (!switch_test_flag(tech_pvt, TFLAG_HANGUP)) {
			switch_set_flag_locked(tech_pvt, TFLAG_HANGUP);
			switch_mutex_lock(chanmap->mutex);
			pri_hangup(tech_pvt->spri->pri, tech_pvt->call, switch_channel_get_cause(channel));
			pri_destroycall(tech_pvt->spri->pri, tech_pvt->call);
			switch_mutex_unlock(chanmap->mutex);
		}


		switch_mutex_lock(globals.channel_mutex);
		*chanmap->map[tech_pvt->callno] = '\0';
		switch_mutex_unlock(globals.channel_mutex);
		
	}

	switch_set_flag_locked(tech_pvt, TFLAG_BYE);

	teletone_destroy_session(&tech_pvt->tone_session);

	switch_buffer_destroy(&tech_pvt->dtmf_buffer);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t wanpipe_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WANPIPE LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t wanpipe_on_soft_execute(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);



	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WANPIPE TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t wanpipe_answer_channel(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (tech_pvt->spri) {
		struct channel_map *chanmap = tech_pvt->spri->private_info;
			
		if (switch_test_flag(tech_pvt, TFLAG_INBOUND)) {
			switch_mutex_lock(chanmap->mutex);
			pri_answer(tech_pvt->spri->pri, tech_pvt->call, 0, 1);
			switch_mutex_unlock(chanmap->mutex);
		}
	} else if (tech_pvt->ss7boost_handle) {
		isup_exec_command(tech_pvt->ss7boost_handle,
						  tech_pvt->boost_span_number,
						  tech_pvt->boost_chan_number,
						  -1,
						  SIGBOOST_EVENT_CALL_ANSWERED,
						  0);
	}

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t wanpipe_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
										switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	uint8_t *bp;
	uint32_t bytes = 0;
	int bread = 0;
	char digit_str[80];

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_BYE) || tech_pvt->wpsock->fd < 0) {
		return SWITCH_STATUS_GENERR;
	}

	bp = tech_pvt->databuf;

	*frame = NULL;
	memset(tech_pvt->databuf, 0, sizeof(tech_pvt->databuf));
	while (bytes < globals.samples_per_frame * 2) {
		if (switch_test_flag(tech_pvt, TFLAG_BYE) || tech_pvt->wpsock->fd < 0) {
			return SWITCH_STATUS_GENERR;
		}

		if (sangoma_socket_waitfor(tech_pvt->wpsock->fd, 1000, POLLIN | POLLERR | POLLHUP) <= 0) {
			return SWITCH_STATUS_GENERR;
		}

		if ((bread = sangoma_readmsg_socket(tech_pvt->wpsock->fd,
										  &tech_pvt->hdrframe,
										  sizeof(tech_pvt->hdrframe), bp, sizeof(tech_pvt->databuf) - bytes, 0)) < 0) {
			if (errno == EBUSY) {
				continue;
			} else {
				return SWITCH_STATUS_GENERR;
			}

		}
		bytes += bread;
		bp += bytes;
	}

	if (!bytes || switch_test_flag(tech_pvt, TFLAG_BYE) || tech_pvt->wpsock->fd < 0) {
		return SWITCH_STATUS_GENERR;
	}

	tech_pvt->read_frame.datalen = bytes;
	tech_pvt->read_frame.samples = bytes / 2;
	
	if (!globals.ignore_dtmf_tone) {
		teletone_dtmf_detect (&tech_pvt->dtmf_detect, tech_pvt->read_frame.data, tech_pvt->read_frame.samples);
		teletone_dtmf_get(&tech_pvt->dtmf_detect, digit_str, sizeof(digit_str));
	
		if (digit_str[0]) {
			char *p = digit_str;
			switch_dtmf_t dtmf = {0, globals.dtmf_on};
			while(p && *p) {
				dtmf.digit = *p;
				switch_channel_queue_dtmf(channel, &dtmf);
				p++;
			}
			if (globals.debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DTMF DETECTED: [%s]\n", digit_str);
			}
			if (globals.suppress_dtmf_tone) {
				tech_pvt->skip_read_frames = 20;
			}
		}

		if (tech_pvt->skip_read_frames > 0) {
			memset(tech_pvt->read_frame.data, 0, tech_pvt->read_frame.datalen);
			tech_pvt->skip_read_frames--;
		}
	}
#ifdef DOTRACE	
	write(tech_pvt->fd2, tech_pvt->read_frame.data, (int) tech_pvt->read_frame.datalen);
#endif
	//printf("read %d\n", tech_pvt->read_frame.datalen);
	*frame = &tech_pvt->read_frame;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t wanpipe_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
										 switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt;
	uint32_t dtmf_blen;
	void *data = frame->data;
	int result = 0;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_BYE) || tech_pvt->wpsock->fd < 0) {
		return SWITCH_STATUS_GENERR;
	}

	if (tech_pvt->dtmf_buffer && (dtmf_blen = switch_buffer_inuse(tech_pvt->dtmf_buffer))) {
		uint32_t len = dtmf_blen > frame->datalen ? frame->datalen : dtmf_blen;

		switch_buffer_read(tech_pvt->dtmf_buffer, tech_pvt->auxbuf, len);		
		if (len < frame->datalen) {
			uint8_t *data = frame->data;
			memcpy(data + len, tech_pvt->auxbuf + len, frame->datalen - len);
		}
		data= tech_pvt->auxbuf;
	} 
	
	if (tech_pvt->skip_write_frames) {
		tech_pvt->skip_write_frames--;
		return SWITCH_STATUS_SUCCESS;
	}

#ifdef DOTRACE
	write(tech_pvt->fd, data, frame->datalen);
#endif

	result = sangoma_sendmsg_socket(tech_pvt->wpsock->fd, &tech_pvt->hdrframe, sizeof(tech_pvt->hdrframe), data, frame->datalen, 0);
	
	if (result < 0 && errno != EBUSY) {
		return SWITCH_STATUS_GENERR;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t wanpipe_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int wrote = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_BYE) || tech_pvt->wpsock->fd < 0) {
		return SWITCH_STATUS_GENERR;
	}

	if (!tech_pvt->dtmf_buffer) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate DTMF Buffer....");
		if (switch_buffer_create_dynamic(&tech_pvt->dtmf_buffer, 1024, 3192, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "Failed to allocate DTMF Buffer!\n");
			return SWITCH_STATUS_FALSE;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "SUCCESS!\n");
		}
	}

	tech_pvt->tone_session.duration = dtmf.duration * (tech_pvt->tone_session.rate / 1000);
	if ((wrote = teletone_mux_tones(&tech_pvt->tone_session, &tech_pvt->tone_session.TONES[(int)dtmf->digit]))) {
		switch_buffer_write(tech_pvt->dtmf_buffer, tech_pvt->tone_session.buffer, wrote * 2);
	}


	tech_pvt->skip_read_frames = 200;
	
	return status;
}

static switch_status_t wanpipe_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_object_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_BYE) || tech_pvt->wpsock->fd < 0) {
		return SWITCH_STATUS_GENERR;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_NOMEDIA:
		break;
	case SWITCH_MESSAGE_INDICATE_MEDIA:
		break;
	case SWITCH_MESSAGE_INDICATE_HOLD:
		break;
	case SWITCH_MESSAGE_INDICATE_UNHOLD:
		break;
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		break;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		break;
	case SWITCH_MESSAGE_INDICATE_REDIRECT:
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		wanpipe_answer_channel(session);
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t wanpipe_kill_channel(switch_core_session_t *session, int sig)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch(sig) {
	case SWITCH_SIG_KILL:
		switch_clear_flag_locked(tech_pvt, TFLAG_MEDIA);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;

}


switch_io_routines_t wanpipe_io_routines = {
	/*.outgoing_channel */ wanpipe_outgoing_channel,
	/*.read_frame */ wanpipe_read_frame,
	/*.write_frame */ wanpipe_write_frame,
	/*.kill_channel */ wanpipe_kill_channel,
	/*.waitfor_read */ NULL,
	/*.waitfor_read */ NULL,
	/*.send_dtmf*/ wanpipe_send_dtmf,
	/*.receive_message*/ wanpipe_receive_message
};

switch_state_handler_table_t wanpipe_state_handlers = {
	/*.on_init */ wanpipe_on_init,
	/*.on_routing */ wanpipe_on_routing,
	/*.on_execute */ NULL,
	/*.on_hangup */ wanpipe_on_hangup,
	/*.on_exchange_media */ wanpipe_on_exchange_media,
	/*.on_soft_execute */ wanpipe_on_soft_execute
};

static switch_call_cause_t wanpipe_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags)
{
	char *bchan = NULL;
	char name[128] = "";
	char *protocol = NULL;
	char *dest;
	int ready = 0, is_pri = 0, is_boost = 0, is_raw = 0;
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	private_object_t *tech_pvt;
	switch_channel_t *channel;
	switch_caller_profile_t *caller_profile = NULL;
	int callno = 0;
	struct sangoma_pri *spri;
	int span = 0, autospan = 0, autochan = 0;
	char *num, *p;
	struct channel_map *chanmap = NULL;
	
	if (!outbound_profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh! no caller profile\n");
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		goto error;
	}

	protocol = strdup(outbound_profile->destination_number);
	assert(protocol != NULL);

	if (!(dest = strchr(protocol, '/'))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error No protocol specified!\n");
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		goto error;
	}
	
	*dest++ = '\0';

	if (!strcasecmp(protocol, "raw")) {
		bchan = dest;
		is_raw = ready = 1;
	} else if (!strcasecmp(protocol, "pri")) {
		if ((is_pri = globals.configured_spans)) {
			ready = is_pri;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error No PRI Spans Configured.\n");
		}
	} else if (!strcasecmp(protocol, "ss7boost")) {
		if ((is_boost = globals.configured_boost_spans)) {
			ready = is_boost;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error No SS7BOOST Spans Configured.\n");
		}
	}
	
	if (!ready) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Continue!\n");
		cause = SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
		goto error;
	}

	outbound_profile->destination_number = dest;

	if (!(*new_session = switch_core_session_request(wanpipe_endpoint_interface, pool))) {
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
        goto error;
	}

	switch_core_session_add_stream(*new_session, NULL);
	if ((tech_pvt = (private_object_t *) switch_core_session_alloc(*new_session, sizeof(private_object_t)))) {
		memset(tech_pvt, 0, sizeof(*tech_pvt));
		switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(*new_session));
		channel = switch_core_session_get_channel(*new_session);
		switch_core_session_set_private(*new_session, tech_pvt);
		tech_pvt->session = *new_session;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		switch_core_session_destroy(new_session);
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		goto error;
	}

		
	caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);

	if (is_pri) {
		num = caller_profile->destination_number;
		if ((p = strchr(num, '/'))) {
			*p++ = '\0';

			if (*num == 'a') {
				span = 1;
				autospan = 1;
			} else if (*num == 'A') {
				span = MAX_SPANS - 1;
				autospan = -1;
			} else {
				if (num && *num > 47 && *num < 58) {
					span = atoi(num);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Syntax\n");
					switch_core_session_destroy(new_session);
					cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
					goto error;
				}
			}
			num = p;
			if ((p = strchr(num, '/'))) {
				*p++ = '\0';
				if (*num == 'a') {
					autochan = 1;
				} else if (*num == 'A') {
					autochan = -1;
				} else if (num && *num > 47 && *num < 58) {
					callno = atoi(num);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Syntax\n");
					switch_core_session_destroy(new_session);
					cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
					goto error;
				}
				caller_profile->destination_number = p;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Syntax\n");
				switch_core_session_destroy(new_session);
				cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
				goto error;
			}
		}
	}

	tech_pvt->caller_profile = caller_profile;

	if (is_raw) {
		int chan, span;

		if (sangoma_span_chan_fromif (bchan, &span, &chan)) {
			if (!(tech_pvt->wpsock = wp_open(span, chan))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open fd for s%dc%d! [%s]\n", span, chan, strerror(errno));
				switch_core_session_destroy(new_session);
				cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
				goto error;
			}
			switch_set_flag_locked(tech_pvt, TFLAG_NOSIG);
			snprintf(name, sizeof(name), "wanpipe/%s/nosig", bchan);
			switch_channel_set_name(channel, name);			
			switch_channel_set_caller_profile(channel, caller_profile);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid address\n");
			switch_core_session_destroy(new_session);
			cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
			goto error;
		}
	} else if (is_pri) {
		switch_mutex_lock(globals.channel_mutex);
		callno = 0;
		while (!callno) {
			if (autospan > 0 && span == MAX_SPANS - 1) {
				break;
			}

			if (autospan < 0 && span == 0) {
				break;
			}

			if (SPANS[span] && (spri = &SPANS[span]->spri) && switch_test_flag(spri, SANGOMA_PRI_READY)) {
				chanmap = spri->private_info;
						
				if (autochan > 0) {
					for(callno = 1; callno < SANGOMA_MAX_CHAN_PER_SPAN; callno++) {
						if ((SPANS[span]->bchans & (1 << callno)) && ! *chanmap->map[callno]) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Choosing channel s%dc%d\n", span, callno);
							goto done;
						}
					}
					callno = 0;
				} else if (autochan < 0) {
					for(callno = SANGOMA_MAX_CHAN_PER_SPAN; callno > 0; callno--) {
						if ((SPANS[span]->bchans & (1 << callno)) && ! *chanmap->map[callno]) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Choosing channel s%dc%d\n", span, callno);
							goto done;
						}
					}
					callno = 0;
				}
			}

			if (autospan > 0) {
				span++;
			} else if (autospan < 0) {
				span--;
			}
		}
	done:
		switch_mutex_unlock(globals.channel_mutex);

		if (!spri || callno == 0 || callno == (SANGOMA_MAX_CHAN_PER_SPAN)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No Free Channels!\n");
			switch_core_session_destroy(new_session);
			cause = SWITCH_CAUSE_SWITCH_CONGESTION;
			goto error;
		}
				
		tech_pvt->callno = callno;
				
		if (spri) {
			struct channel_map *chanmap = spri->private_info;
			switch_mutex_lock(chanmap->mutex);
			if (tech_pvt->call = pri_new_call(spri->pri)) {
				struct pri_sr *sr;
					
				snprintf(name, sizeof(name), "wanpipe/pri/s%dc%d/%s", spri->span, callno, caller_profile->destination_number);
				switch_channel_set_name(channel, name);			
				switch_channel_set_caller_profile(channel, caller_profile);
				sr = pri_sr_new();
				pri_sr_set_channel(sr, callno, 0, 0);
				pri_sr_set_bearer(sr, 0, SPANS[span]->l1);
				pri_sr_set_called(sr, caller_profile->destination_number, SPANS[span]->dp, 1);
				pri_sr_set_caller(sr,
								  caller_profile->caller_id_number,
								  caller_profile->caller_id_name,
								  SPANS[span]->dp,
								  PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN);
				pri_sr_set_redirecting(sr,
									   caller_profile->caller_id_number,
									   SPANS[span]->dp,
									   PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN,
									   PRI_REDIR_UNCONDITIONAL);
				
				if (pri_setup(spri->pri, tech_pvt->call , sr)) {
					switch_core_session_destroy(new_session);
					pri_sr_free(sr);
					cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
					switch_mutex_unlock(chanmap->mutex);
					goto error;
				}
				
				if (!(tech_pvt->wpsock = wp_open(spri->span, callno))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open fd!\n");
					switch_core_session_destroy(new_session);
					pri_sr_free(sr);
					cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
					switch_mutex_unlock(chanmap->mutex);
					goto error;
				}
				pri_sr_free(sr);
				switch_copy_string(chanmap->map[callno],
								   switch_core_session_get_uuid(*new_session),
								   sizeof(chanmap->map[callno]));
				tech_pvt->spri = spri;
			}
			switch_mutex_unlock(chanmap->mutex);
		}
	} else if (is_boost) {
		char *p;

		if ((p = strchr(caller_profile->destination_number, '/'))) {
			char *grp = caller_profile->destination_number;
			*p = '\0';
			caller_profile->destination_number = p+1;
			tech_pvt->boost_trunk_group = atoi(grp+1) - 1;
			if (tech_pvt->boost_trunk_group < 0) {
				tech_pvt->boost_trunk_group = 0;
			}
		}
		sprintf(name, "wanpipe/ss7boost/%s", caller_profile->destination_number);
		switch_channel_set_name(channel, name);
		tech_pvt->ss7boost_handle = globals.ss7boost_handle;

		if (session && switch_core_session_compare(session, *new_session)) {
			private_object_t *otech_pvt = switch_core_session_get_private(session);
			tech_pvt->boost_pres = otech_pvt->boost_pres;
		}

	}

	tech_pvt->caller_profile = caller_profile;
	switch_channel_set_flag(channel, CF_OUTBOUND);
	switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	switch_channel_set_state(channel, CS_INIT);
	cause = SWITCH_CAUSE_SUCCESS;
	

 error:
	switch_safe_free(protocol);
	return cause;
}


#ifdef WIN32
static void s_pri_error(char *s)
#else
static void s_pri_error(struct pri *pri, char *s)
#endif
{
	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, s);
	}
}

#ifdef WIN32
static void s_pri_message(char *s)
{
	s_pri_error(s);
}
#else
static void s_pri_message(struct pri *pri, char *s)
{
	s_pri_error(pri, s);
}
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_wanpipe_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	sangoma_set_logger(wp_logger);

	memset(SPANS, 0, sizeof(SPANS));

	pri_set_error(s_pri_error);
	pri_set_message(s_pri_message);

	module_pool = pool;

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.call_hash, module_pool);
	switch_mutex_init(&globals.hash_mutex, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.channel_mutex, SWITCH_MUTEX_NESTED, module_pool);

	/* start the pri's */
	if ((status = config_wanpipe(0)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	wanpipe_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	wanpipe_endpoint_interface->interface_name = "wanpipe";
	wanpipe_endpoint_interface->io_routines = &wanpipe_io_routines;
	wanpipe_endpoint_interface->state_handler = &wanpipe_state_handlers;

	/* indicate that the module should continue to be loaded */
	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_wanpipe_shutdown)
{
	switch_core_hash_destroy(&globals.call_hash);
	return SWITCH_STATUS_SUCCESS;
}

/*event Handlers */

static int on_info(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "number is: %s\n", pevent->ring.callednum);
	if (strlen(pevent->ring.callednum) > 3) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "final number is: %s\n", pevent->ring.callednum);
		pri_answer(spri->pri, pevent->ring.call, 0, 1);
	}
	return 0;
}

static int on_hangup(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{
	struct channel_map *chanmap;
	switch_core_session_t *session;
	private_object_t *tech_pvt;

	chanmap = spri->private_info;
	if ((session = switch_core_session_locate(chanmap->map[pevent->hangup.channel]))) {
		switch_channel_t *channel = NULL;

		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		if (switch_channel_get_state(channel) < CS_HANGUP) {


			tech_pvt = switch_core_session_get_private(session);
			assert(tech_pvt != NULL);
			chanmap = tech_pvt->spri->private_info;
			if (!tech_pvt->call) {
				tech_pvt->call = pevent->hangup.call;
			}

			tech_pvt->cause = pevent->hangup.cause;
			switch_set_flag_locked(tech_pvt, TFLAG_HANGUP);
			switch_channel_hangup(channel, tech_pvt->cause);
			switch_mutex_lock(chanmap->mutex);
			pri_destroycall(tech_pvt->spri->pri, tech_pvt->call);
			switch_mutex_unlock(chanmap->mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Hanging up channel s%dc%d\n", spri->span, pevent->hangup.channel);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Duplicate Hang up on channel s%dc%d\n", spri->span, pevent->hangup.channel);
		}
		switch_core_session_rwunlock(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Hanging up nonexistant channel s%dc%d\n", spri->span, pevent->hangup.channel);
	}


	return 0;
}

static int on_answer(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	struct channel_map *chanmap;


	chanmap = spri->private_info;

	if ((session = switch_core_session_locate(chanmap->map[pevent->answer.channel]))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Answer on channel s%dc%d\n", spri->span, pevent->answer.channel);
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);
		switch_channel_mark_answered(channel);
		switch_core_session_rwunlock(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Answer on channel s%dc%d but it's not in use?\n", spri->span, pevent->answer.channel);
	}
	
	return 0;
}


static int on_proceed(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	struct channel_map *chanmap;

	chanmap = spri->private_info;

	if ((session = switch_core_session_locate(chanmap->map[pevent->proceeding.channel]))) {
		switch_core_session_message_t *msg;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Proceeding on channel s%dc%d\n", spri->span, pevent->proceeding.channel);
		
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);
		
		switch_core_session_pass_indication(session, SWITCH_MESSAGE_INDICATE_PROGRESS);
		switch_channel_mark_pre_answered(channel);
		
		switch_core_session_rwunlock(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Proceeding on channel s%dc%d but it's not in use?\n", 
						  spri->span, pevent->proceeding.channel);
	}

	return 0;
}


static int on_routinging(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	struct channel_map *chanmap;


	chanmap = spri->private_info;

	if ((session = switch_core_session_locate(chanmap->map[pevent->ringing.channel]))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "-- Ringing on channel s%dc%d\n", spri->span, pevent->ringing.channel);
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		switch_core_session_pass_indication(session, SWITCH_MESSAGE_INDICATE_RINGING);
		switch_channel_mark_ring_ready(channel);

		switch_core_session_rwunlock(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "-- Ringing on channel s%dc%d %s but it's not in use?\n", spri->span, pevent->ringing.channel, chanmap->map[pevent->ringing.channel]);
	}

	return 0;
}


static int on_routing(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{
	char name[128];
	switch_core_session_t *session = NULL;
	switch_channel_t *channel;
	struct channel_map *chanmap;
	int ret = 0;
	
	switch_mutex_lock(globals.channel_mutex);

	chanmap = spri->private_info;
	if ((session = switch_core_session_locate(chanmap->map[pevent->ring.channel]))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "--Duplicate Ring on channel s%dc%d (ignored)\n",
							  spri->span, pevent->ring.channel);
		switch_core_session_rwunlock(session);
		ret = 0;
		goto done;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Ring on channel s%dc%d (from %s to %s)\n", spri->span, pevent->ring.channel,
						  pevent->ring.callingnum, pevent->ring.callednum);

	switch_mutex_unlock(chanmap->mutex);
	pri_proceeding(spri->pri, pevent->ring.call, pevent->ring.channel, 0);
	pri_acknowledge(spri->pri, pevent->ring.call, pevent->ring.channel, 0);
	switch_mutex_unlock(chanmap->mutex);

	if ((session = switch_core_session_request(wanpipe_endpoint_interface, NULL))) {
		private_object_t *tech_pvt;
		char ani2str[4] = "";
		//wanpipe_tdm_api_t tdm_api;

		switch_core_session_add_stream(session, NULL);
		if ((tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t)))) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			sprintf(name, "wanpipe/pri/s%dc%d", spri->span, pevent->ring.channel);
			switch_channel_set_name(channel, name);
			tech_pvt->session = session;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(&session);
			ret = 0;
			goto done;
		}

		if (pevent->ring.ani2 >= 0) {
			snprintf(ani2str, 5, "%.2d", pevent->ring.ani2);
		}

		if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																  NULL,
																  globals.dialplan,
																  "FreeSWITCH",
																  pevent->ring.callingnum,
#ifdef WIN32
																  NULL,
#else
																  pevent->ring.callingani,
#endif
																  switch_strlen_zero(ani2str) ? NULL : ani2str,
																  NULL,
																  NULL,
																  (char *)modname,
																  globals.context,
																  pevent->ring.callednum))) {
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}

		switch_set_flag_locked(tech_pvt, TFLAG_INBOUND);
		tech_pvt->spri = spri;
		tech_pvt->cause = -1;

		if (!tech_pvt->call) {
			tech_pvt->call = pevent->ring.call;
		}
		
		tech_pvt->callno = pevent->ring.channel;
		tech_pvt->span = spri->span;

		if (!(tech_pvt->wpsock = wp_open(spri->span, pevent->ring.channel))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open fd!\n");
		}

		switch_copy_string(chanmap->map[pevent->ring.channel], switch_core_session_get_uuid(session), sizeof(chanmap->map[pevent->ring.channel]));
		
		switch_channel_set_state(channel, CS_INIT);
		if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
			switch_core_session_destroy(&session);
			ret = 0;
			goto done;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create new Inbound Channel!\n");
	}

 done:
	switch_mutex_unlock(globals.channel_mutex);
	
	return ret;
}

static int check_flags(struct sangoma_pri *spri)
{

	return 0;
}

static int on_restart(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{
	switch_core_session_t *session;
	struct channel_map *chanmap;


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Restarting s%dc%d\n", spri->span, pevent->restart.channel);

	if (pevent->restart.channel < 1) {
		return 0;
	}


	chanmap = spri->private_info;
	
	if ((session = switch_core_session_locate(chanmap->map[pevent->restart.channel]))) {
		switch_channel_t *channel;
		channel = switch_core_session_get_channel(session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hanging Up channel %s\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_core_session_rwunlock(session);
	}
	
	wp_restart(spri->span, pevent->restart.channel);

	return 0;
}

static int on_dchan_up(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{

	if (!switch_test_flag(spri, SANGOMA_PRI_READY)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Span %d D-Chan UP!\n", spri->span);
		switch_set_flag(spri, SANGOMA_PRI_READY);
	}

	return 0;
}

static int on_dchan_down(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{

	if (switch_test_flag(spri, SANGOMA_PRI_READY)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Span %d D-Chan DOWN!\n", spri->span);
		switch_clear_flag(spri, SANGOMA_PRI_READY);
	}
	
	return 0;
}

static int on_anything(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *pevent)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Caught Event span %d %u (%s)\n", spri->span, event_type,
						  sangoma_pri_event_str(event_type));
	return 0;
}


static void *SWITCH_THREAD_FUNC pri_thread_run(switch_thread_t *thread, void *obj)
{
	struct sangoma_pri *spri = obj;
	struct channel_map chanmap;

	memset(&chanmap, 0, sizeof(chanmap));

	switch_event_t *s_event;


	switch_mutex_init(&chanmap.mutex, SWITCH_MUTEX_NESTED, module_pool);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_ANY, on_anything);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_RING, on_routing);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_RINGING, on_routinging);
	//SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_SETUP_ACK, on_proceed);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_PROCEEDING, on_proceed);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_ANSWER, on_answer);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_DCHAN_UP, on_dchan_up);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_DCHAN_DOWN, on_dchan_down);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_HANGUP_REQ, on_hangup);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_HANGUP, on_hangup);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_INFO_RECEIVED, on_info);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_RESTART, on_restart);

	spri->on_loop = check_flags;
	spri->private_info = &chanmap;

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_pri._tcp");
		switch_event_fire(&s_event);
	}

	sangoma_run_pri(spri);

	free(spri);
	return NULL;
}

static void pri_thread_launch(struct sangoma_pri *spri)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, pri_thread_run, spri, module_pool);

}


static int isup_exec_command(ss7boost_handle_t *ss7boost_handle, int span, int chan, int id, int cmd, int cause)
{
	ss7boost_client_event_t oevent;
	int r = 0;

	switch_mutex_lock(ss7boost_handle->mutex);
	ss7boost_client_event_init(&oevent, cmd, chan, span);
	oevent.release_cause = cause;
	
	if (id >= 0) {
		oevent.call_setup_id = id;
	}
	
	if (ss7boost_client_connection_write(&ss7boost_handle->mcon, &oevent) <= 0){
		r = -1;
	}

	switch_mutex_unlock(ss7boost_handle->mutex);

	return r;
}

#ifdef USE_WAITFOR_SOCKET
static int waitfor_socket(int fd, int timeout, int flags)
{
    struct pollfd pfds[1];
    int res;
 
    memset(&pfds[0], 0, sizeof(pfds[0]));
    pfds[0].fd = fd;
    pfds[0].events = flags;
    res = poll(pfds, 1, timeout);

    if (res > 0) {
	if (pfds[0].revents & POLLIN) {
		res = 1;
	} else if ((pfds[0].revents & POLLERR)) {
		res = -1;
    	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"System Error: Poll Event Error no event!\n");
		res = -1;
	}
    }

    return res;
}
#endif


static void validate_number(unsigned char *s)
{
	unsigned char *p;
	for (p = s; *p; p++) {
		if (*p < 48 || *p > 57) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Encountered a non-numeric character [%c]!\n", *p);
			*p = '\0';
			break;
		}
	}
}


static void handle_call_stop(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	char *uuid = ss7boost_handle->span_chanmap[event->span].map[event->chan];

	if (*uuid) {
		switch_core_session_t *session;

		if ((session = switch_core_session_locate(uuid))) {
			private_object_t *tech_pvt;
			switch_channel_t *channel;
	
			channel = switch_core_session_get_channel(session);
			assert(channel != NULL);

			tech_pvt = switch_core_session_get_private(session);
			assert(tech_pvt != NULL);
			switch_set_flag_locked(tech_pvt, TFLAG_BYE);
			switch_channel_hangup(channel, event->release_cause);
			switch_core_session_rwunlock(session);
		}
		*uuid = '\0';

	} 

	isup_exec_command(ss7boost_handle,
					  event->span, 
					  event->chan, 
					  -1,
					  SIGBOOST_EVENT_CALL_STOPPED_ACK,
					  0);


}

static void handle_call_start(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	char name[128];

	if (*ss7boost_handle->span_chanmap[event->span].map[event->chan]) {
		isup_exec_command(ss7boost_handle,
						  event->span, 
						  event->chan, 
						  -1,
						  SIGBOOST_EVENT_CALL_START_NACK,
						  SIGBOOST_RELEASE_CAUSE_BUSY);		
		return;
	}

	
	if ((session = switch_core_session_request(wanpipe_endpoint_interface, NULL))) {
		private_object_t *tech_pvt;

		switch_core_session_add_stream(session, NULL);
		if ((tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t)))) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			sprintf(name, "wanpipe/ss7boost/s%dc%d", event->span+1, event->chan+1);
			switch_channel_set_name(channel, name);
			tech_pvt->session = session;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			goto fail;
		}


		if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																  NULL,
																  globals.dialplan,
																  "FreeSWITCH(boost)",
																  (char *)event->calling_number_digits,
#ifdef WIN32
																  NULL,
#else
																  (char *)event->calling_number_digits,
#endif
																  NULL,
																  NULL,
																  NULL,
																  (char *)modname,
																  NULL,
																  (char *)event->called_number_digits))) {
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}

		switch_set_flag_locked(tech_pvt, TFLAG_INBOUND);

		tech_pvt->cause = -1;

		tech_pvt->ss7boost_handle = ss7boost_handle;
		tech_pvt->boost_span_number = event->span;
		tech_pvt->boost_chan_number = event->chan;
		tech_pvt->boost_pres = event->calling_number_presentation;
		
		if (!(tech_pvt->wpsock = wp_open(event->span+1, event->chan+1))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open channel %d:%d\n", event->span+1, event->chan+1);
			goto fail;
		}

		switch_copy_string(ss7boost_handle->span_chanmap[event->span].map[event->chan], switch_core_session_get_uuid(session), 
						   sizeof(ss7boost_handle->span_chanmap[event->span].map[event->chan]));
		
		switch_channel_set_state(channel, CS_INIT);
		isup_exec_command(ss7boost_handle,
						  event->span, 
						  event->chan, 
						  -1,
						  SIGBOOST_EVENT_CALL_START_ACK,
						  0);
		switch_core_session_thread_launch(session);
		return;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create new Inbound Channel!\n");
	}

	
 fail:
	if (session) {
		switch_core_session_destroy(&session);
	}

	isup_exec_command(ss7boost_handle,
					  event->span, 
					  event->chan, 
					  -1,
					  SIGBOOST_EVENT_CALL_STOPPED,
					  SIGBOOST_RELEASE_CAUSE_BUSY);

}


static void handle_heartbeat(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Heartbeat!\n");

	isup_exec_command(ss7boost_handle,
					  event->span, 
					  event->chan, 
					  -1,
					  SIGBOOST_EVENT_HEARTBEAT,
					  0);
}


static void handle_call_start_ack(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	char *uuid = ss7boost_handle->setup_array[event->call_setup_id];
	
	if (*uuid) {
		switch_core_session_t *session;

		if ((session = switch_core_session_locate(uuid))) {
			private_object_t *tech_pvt;
			switch_channel_t *channel;
	
			channel = switch_core_session_get_channel(session);
			assert(channel != NULL);
			
			tech_pvt = switch_core_session_get_private(session);
			assert(tech_pvt != NULL);
			
			tech_pvt->ss7boost_handle = ss7boost_handle;
			tech_pvt->boost_span_number = event->span;
			tech_pvt->boost_chan_number = event->chan;
			
			switch_copy_string(ss7boost_handle->span_chanmap[event->span].map[event->chan], switch_core_session_get_uuid(session), 
							   sizeof(ss7boost_handle->span_chanmap[event->span].map[event->chan]));



			if (!tech_pvt->wpsock) {
				if (!(tech_pvt->wpsock = wp_open(tech_pvt->boost_span_number+1, tech_pvt->boost_chan_number+1))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open fd for s%dc%d! [%s]\n", 
									  tech_pvt->boost_span_number+1, tech_pvt->boost_chan_number+1, strerror(errno));
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return;
				}
				if (wanpipe_codec_init(tech_pvt) != SWITCH_STATUS_SUCCESS) {
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return;
				}
			}

			switch_channel_mark_pre_answered(channel);
			
			switch_core_session_rwunlock(session);
		}
		*uuid = '\0';
	}
}

static void handle_call_start_nack_ack(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	// WTF IS THIS! ?
}

static void handle_call_answer(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	char *uuid = ss7boost_handle->span_chanmap[event->span].map[event->chan];

	if (*uuid) {
		switch_core_session_t *session;
		
		if ((session = switch_core_session_locate(uuid))) {
			private_object_t *tech_pvt;
			switch_channel_t *channel;
	
			channel = switch_core_session_get_channel(session);
			assert(channel != NULL);

			tech_pvt = switch_core_session_get_private(session);
			assert(tech_pvt != NULL);

			if (!tech_pvt->wpsock) {
				if (!(tech_pvt->wpsock=wp_open(tech_pvt->boost_span_number+1, tech_pvt->boost_chan_number+1))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open fd for s%dc%d! [%s]\n", 
									  tech_pvt->boost_span_number+1, tech_pvt->boost_chan_number+1, strerror(errno));
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return;
				}
				if (wanpipe_codec_init(tech_pvt) != SWITCH_STATUS_SUCCESS) {
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return;
				}
			}
			
			switch_channel_mark_answered(channel);
			switch_core_session_rwunlock(session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Session %s missing!\n", uuid);
			*uuid = '\0';
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No UUID?\n");
	}
}

static void handle_call_stop_ack(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	// TODO anything here?
}


static void handle_call_start_nack(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	char *uuid = ss7boost_handle->setup_array[event->call_setup_id];
	
	if (*uuid) {
		switch_core_session_t *session;

		if ((session = switch_core_session_locate(uuid))) {
			private_object_t *tech_pvt;
			switch_channel_t *channel;
	
			channel = switch_core_session_get_channel(session);
			assert(channel != NULL);
			
			tech_pvt = switch_core_session_get_private(session);
			assert(tech_pvt != NULL);
			
			tech_pvt->ss7boost_handle = ss7boost_handle;
			tech_pvt->boost_span_number = event->span;
			tech_pvt->boost_chan_number = event->chan;
			
			switch_channel_hangup(channel, event->release_cause);
			
			isup_exec_command(ss7boost_handle,
							  event->span,
							  event->chan, 
							  event->call_setup_id,
							  SIGBOOST_EVENT_CALL_START_NACK_ACK,
							  0);

			switch_core_session_rwunlock(session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Session %s missing!\n", uuid);
		}
		*uuid = '\0';
	}
}

static int parse_ss7_event(ss7boost_handle_t *ss7boost_handle, ss7boost_client_event_t *event)
{
	int ret = 0;

	switch_mutex_lock(ss7boost_handle->mutex);
		
	validate_number((unsigned char*)event->called_number_digits);
	validate_number((unsigned char*)event->calling_number_digits);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
					  "\nRX EVENT\n"
					  "===================================\n"
					  "           rType: %s (%0x HEX)\n"
					  "           rSpan: [%d]\n"
					  "           rChan: [%d]\n"
					  "  rCalledNum: %s\n"
					  " rCallingNum: %s\n"
					  "      rCause: %s\n"
					  " rInterface : [w%dg%d]\n"
					  "  rEvent ID : [%d]\n"
					  "   rSetup ID: [%d]\n"
					  "        rSeq: [%d]\n"
					  "===================================\n"
					  "\n",
					  ss7boost_client_event_id_name(event->event_id),
					  event->event_id,
					  event->span+1,
					  event->chan+1,
					  (event->called_number_digits_count ? (char *) event->called_number_digits : "N/A"),
					  (event->calling_number_digits_count ? (char *) event->calling_number_digits : "N/A"),
					  switch_channel_cause2str(event->release_cause),
					  event->span+1,
					  event->chan+1,
					  event->event_id,
					  event->call_setup_id,
					  event->seqno
					  );
	

	switch(event->event_id) {
	
	case SIGBOOST_EVENT_CALL_START:
		handle_call_start(ss7boost_handle, event);
		break;
	case SIGBOOST_EVENT_CALL_STOPPED:
		handle_call_stop(ss7boost_handle, event);
		break;
	case SIGBOOST_EVENT_CALL_START_ACK:
		handle_call_start_ack(ss7boost_handle, event);
		break;
	case SIGBOOST_EVENT_CALL_START_NACK:
		handle_call_start_nack(ss7boost_handle, event);
		break;
	case SIGBOOST_EVENT_CALL_ANSWERED:
		handle_call_answer(ss7boost_handle, event);
		break;
	case SIGBOOST_EVENT_HEARTBEAT:
		handle_heartbeat(ss7boost_handle, event);
		break;
	case SIGBOOST_EVENT_CALL_START_NACK_ACK:
		handle_call_start_nack_ack(ss7boost_handle, event);
		break;
	case SIGBOOST_EVENT_CALL_STOPPED_ACK:
		handle_call_stop_ack(ss7boost_handle, event);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Warning no handler implemented for [%s]\n", 
						  ss7boost_client_event_id_name(event->event_id));
		break;
	}
	switch_mutex_unlock(ss7boost_handle->mutex);
	return ret;
}

static void *SWITCH_THREAD_FUNC boost_thread_run(switch_thread_t *thread, void *obj)
{
	ss7boost_handle_t *ss7boost_handle = (ss7boost_handle_t *) obj;
	ss7boost_client_event_t *event;

	if (ss7boost_client_connection_open(&ss7boost_handle->mcon,
									ss7boost_handle->local_ip, 
									ss7boost_handle->local_port,
									ss7boost_handle->remote_ip,
									ss7boost_handle->remote_port,
									module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "FATAL ERROR CREATING CLIENT CONNECTION\n");
		return NULL;
	}
			
	isup_exec_command(ss7boost_handle,
					  0,
					  0, 
					  -1,
					  SIGBOOST_EVENT_SYSTEM_RESTART,
					  0);
	
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Monitor Thread Started\n");
	
	switch_mutex_lock(globals.hash_mutex);
	globals.configured_boost_spans++;
	switch_mutex_unlock(globals.hash_mutex);

	globals.ss7boost_handle = ss7boost_handle;
	
	for(;;) {
		if (ss7boost_client_connection_read(&ss7boost_handle->mcon, &event) == SWITCH_STATUS_SUCCESS) {
				struct timeval current;
				struct timeval difftime;
				gettimeofday(&current,NULL);
				timersub (&current, &event->tv, &difftime);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket Event [%s] T=%d:%d\n",
								  ss7boost_client_event_id_name(event->event_id),
								  (int)difftime.tv_sec, (int)difftime.tv_usec);
				
				parse_ss7_event(ss7boost_handle, event);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: Reading from Boost Socket! %s\n", strerror(errno));
			break;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Close udp socket\n");
	ss7boost_client_connection_close(&ss7boost_handle->mcon);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Monitor Thread Ended\n");	
	

	return NULL;
}

static void launch_ss7boost_handle(ss7boost_handle_t *ss7boost_handle)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, boost_thread_run, ss7boost_handle, module_pool);
}



static switch_status_t config_wanpipe(int reload)
{
	char *cf = "wanpipe.conf";
	int current_span = 0, min_span = 0, max_span = 0;
	switch_xml_t cfg, xml, settings, param, pri_spans, ss7boost_handles, span, analog_channels, channel;

	globals.samples_per_frame = DEFAULT_SAMPLES_PER_FRAME;
	globals.dtmf_on = 150;
	globals.dtmf_off = 50;



	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}


	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "ms-per-frame")) {
				globals.samples_per_frame = atoi(val) * 8;
			} else if (!strcmp(var, "dtmf-on")) {
				globals.dtmf_on = atoi(val);
			} else if (!strcmp(var, "dtmf-off")) {
				globals.dtmf_off = atoi(val);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "context")) {
				set_global_context(val);
			} else if (!strcmp(var, "suppress-dtmf-tone")) {
				globals.suppress_dtmf_tone = switch_true(val);
			} else if (!strcmp(var, "ignore-dtmf-tone")) {
				globals.ignore_dtmf_tone = switch_true(val);
			}
		}
	}

	ss7boost_handles = switch_xml_child(cfg, "ss7boost_handles");
	for (span = switch_xml_child(ss7boost_handles, "handle"); span; span = span->next) {
		char *local_ip = NULL, *remote_ip = NULL;
		int local_port = 0, remote_port = 0;
		ss7boost_handle_t *ss7boost_handle;

		for (param = switch_xml_child(span, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			
			if (!strcasecmp(var, "local-ip")) {
				local_ip = val;
			} else if (!strcasecmp(var, "local-port")) {
				local_port = atoi(val);
			} else if (!strcasecmp(var, "remote-ip")) {
				remote_ip = val;
			} else if (!strcasecmp(var, "remote-port")) {
				remote_port = atoi(val);
			}
		}


		if (!(local_ip && local_port && remote_ip && remote_port)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Config, skipping...\n");
			continue;
		}

		assert(ss7boost_handle = malloc(sizeof(*ss7boost_handle)));
		memset(ss7boost_handle, 0, sizeof(*ss7boost_handle));
		ss7boost_handle->local_ip = switch_core_strdup(module_pool, local_ip);
		ss7boost_handle->local_port = local_port;
		ss7boost_handle->remote_ip = switch_core_strdup(module_pool, remote_ip);
		ss7boost_handle->remote_port = remote_port;

		switch_mutex_init(&ss7boost_handle->mutex, SWITCH_MUTEX_NESTED, module_pool);
		launch_ss7boost_handle(ss7boost_handle);
		break;
	}

	analog_channels = switch_xml_child(cfg, "analog_channels");
	for(channel = switch_xml_child(analog_channels, "channel"); channel; channel = channel->next) {
		char *c_type = (char *) switch_xml_attr(channel, "type");
		char *c_dev = (char *) switch_xml_attr(channel, "device");
		char *user = NULL;
		char *domain = NULL;
		char *cid_name = NULL, *cid_num = NULL;
		analog_channel_t *alc;
		analog_type_t a_type = ANALOG_TYPE_UNKNOWN;
		wpsock_t *sock;
		int chan, span;
		
		if (!c_type) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required attribute 'type'\n");
			continue;
		}

		if (!strcasecmp(c_type, "phone") || !strcasecmp(c_type, "fxs")) {
			a_type = ANALOG_TYPE_PHONE_FXS;
		} else if (!strcasecmp(c_type, "line") || !strcasecmp(c_type, "fxo")) {
			a_type = ANLOG_TYPE_LINE_FXO;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid type '%s'\n", c_type);
			continue;
		}

		if (!c_dev) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required attribute 'device'\n");
			continue;
		}

		if (!sangoma_span_chan_fromif (c_dev, &span, &chan)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid device name '%s'\n", c_dev);
			continue;
		}
		
		if (!(sock = wp_open(span, chan))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open device '%s' (%s)\n", c_dev, strerror(errno));
			continue;
		}

		for (param = switch_xml_child(channel, "param"); param; param = param->next) {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");
			
			if (!strcasecmp(var, "user")) {
				user = var;
			} else if (!strcasecmp(var, "domain")) {
				domain = val;
			} else if (!strcasecmp(var, "caller-id-name")) {
				cid_name = val;
			} else if (!strcasecmp(var, "caller-id-number")) {
				cid_num = val;
			}
		}

		assert((alc = malloc(sizeof(*alc))));
		memset(alc, 0, sizeof(*alc));
		if (user) {
			alc->user = strdup(user);
		}
		if (domain) {
			alc->domain = strdup(domain);
		}
		if (cid_name) {
			alc->cid_name = strdup(cid_name);
		}
		if (cid_num) {
			alc->cid_name = strdup(cid_num);
		}
		
		alc->a_type = a_type;
		alc->sock = sock;
		alc->chan = chan;
		alc->span = span;

		if (a_type == ANALOG_TYPE_PHONE_FXS) {
			FXS_ANALOG_CHANNELS[globals.fxs_index++] = alc;
		} else {
			FXO_ANALOG_CHANNELS[globals.fxo_index++] = alc;
		}
	}


	if (globals.fxs_index) {
		switch_thread_t *thread;
		switch_threadattr_t *thd_attr = NULL;
	
		switch_threadattr_create(&thd_attr, module_pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, fxs_thread_run, NULL, module_pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Starting FXS Thread!\n");
	}


	pri_spans = switch_xml_child(cfg, "pri_spans");
	for (span = switch_xml_child(pri_spans, "span"); span; span = span->next) {
		char *id = (char *) switch_xml_attr(span, "id");
		int32_t i = 0;

		current_span = 0;
		
		if (id) {
			char *p;
			
			min_span = atoi(id);
			if ((p = strchr(id, '-'))) {
				p++;
				max_span = atoi(p);
			} else {
				max_span = min_span;
			}
			if (min_span < 1 || max_span < min_span) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Span Config! [%s]\n", id);
				continue;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing SPAN ID!\n");
			continue;
		}
		
		for (i = min_span; i <= max_span; i++) {
			current_span = i;
			
			if (current_span <= 0 || current_span > MAX_SPANS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid SPAN %d!\n", current_span);
				current_span = 0;
				continue;
			}
			
			if (!SPANS[current_span]) {
				if (!(SPANS[current_span] = switch_core_alloc(module_pool, sizeof(*SPANS[current_span])))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "MEMORY ERROR\n");
					break;
				}
				SPANS[current_span]->span = current_span;
			}
			

			for (param = switch_xml_child(span, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
			
				if (!strcmp(var, "dchan")) {
					SPANS[current_span]->dchan = atoi(val);
				} else if (!strcmp(var, "bchan")) {
					char from[128];
					char *to;
					switch_copy_string(from, val, sizeof(from));
					if ((to = strchr(from, '-'))) {
						int fromi, toi, x = 0;
						*to++ = '\0';
						fromi = atoi(from);
						toi = atoi(to);
						if (fromi > 0 && toi > 0 && fromi < toi && fromi < MAX_SPANS && toi < MAX_SPANS) {
							for(x = fromi; x <= toi; x++) {
								SPANS[current_span]->bchans |= (1 << x);
							}
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid bchan range!\n");
						}
					} else {
						int i = atoi(val);
						if (i > 0 && i < 31) {
							SPANS[current_span]->bchans |= (1 << i);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid bchan!\n");
						}
					}
				} else if (!strcmp(var, "node")) {
					SPANS[current_span]->node = str2node(val);
				} else if (!strcmp(var, "switch")) {
					SPANS[current_span]->pswitch = str2switch(val);
				} else if (!strcmp(var, "dp")) {
					SPANS[current_span]->dp = str2dp(val);
				} else if (!strcmp(var, "l1")) {
					SPANS[current_span]->l1 = str2l1(val);
				}
			}
		}
	}
	switch_xml_free(xml);

	if (!globals.dialplan) {
		set_global_dialplan("XML");
	}

	globals.configured_spans = 0;
	for(current_span = 1; current_span < MAX_SPANS; current_span++) {
		if (SPANS[current_span]) {

			if (!SPANS[current_span]->l1) {
				SPANS[current_span]->l1 = PRI_LAYER_1_ULAW;
			}
			if (sangoma_init_pri(&SPANS[current_span]->spri,
								 current_span,
								 SPANS[current_span]->dchan,
								 SPANS[current_span]->pswitch,
								 SPANS[current_span]->node,
								 globals.debug)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot launch span %d\n", current_span);
				continue;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Launch span %d\n", current_span);
			pri_thread_launch(&SPANS[current_span]->spri);
			switch_mutex_lock(globals.hash_mutex);
			globals.configured_spans++;
			switch_mutex_unlock(globals.hash_mutex);
		}
	}

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
