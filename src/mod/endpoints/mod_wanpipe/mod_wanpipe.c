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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <libsangoma.h>
#include <sangoma_pri.h>

static const char modname[] = "mod_wanpipe";
#define STRLEN 15

static switch_memory_pool *module_pool;

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
} TFLAGS;

#define PACKET_LEN 160
#define DEFAULT_BYTES_PER_FRAME 160

static struct {
	int debug;
	int panic;
	int bytes_per_frame;
	char *dialplan;
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

#define MAX_SPANS 128
static struct wanpipe_pri_span *SPANS[MAX_SPANS];


struct private_object {
	unsigned int flags;			/* FLAGS */
	switch_frame read_frame;	/* Frame for Writing */
	switch_core_session *session;
	switch_codec read_codec;
	switch_codec write_codec;
	unsigned char databuf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	struct sangoma_pri *spri;
	sangoma_api_hdr_t hdrframe;
	switch_caller_profile *caller_profile;
	int socket;
	int callno;
	int cause;
	q931_call *call;
};

struct channel_map {
	switch_core_session *map[36];
};


static void set_global_dialplan(char *dialplan)
{
	if (globals.dialplan) {
		free(globals.dialplan);
		globals.dialplan = NULL;
	}

	globals.dialplan = strdup(dialplan);
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

static const switch_endpoint_interface wanpipe_endpoint_interface;

static void set_global_dialplan(char *dialplan);
static int str2node(char *node);
static int str2switch(char *swtype);
static switch_status wanpipe_on_init(switch_core_session *session);
static switch_status wanpipe_on_hangup(switch_core_session *session);
static switch_status wanpipe_on_loopback(switch_core_session *session);
static switch_status wanpipe_on_transmit(switch_core_session *session);
static switch_status wanpipe_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile,
											  switch_core_session **new_session);
static switch_status wanpipe_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
										switch_io_flag flags, int stream_id);
static switch_status wanpipe_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
										 switch_io_flag flags, int stream_id);
static int on_info(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event);
static int on_hangup(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event);
static int on_ring(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event);
static int check_flags(struct sangoma_pri *spri);
static int on_restart(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event);
static int on_anything(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event);
static void *pri_thread_run(switch_thread *thread, void *obj);
static int config_wanpipe(int reload);



/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status wanpipe_on_init(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	wanpipe_tdm_api_t tdm_api;
	int err = 0;
	unsigned int rate = 8000;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	tech_pvt->read_frame.data = tech_pvt->databuf;

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "WANPIPE INIT\n");

	err = sangoma_tdm_set_codec(tech_pvt->socket, &tdm_api, WP_SLINEAR);	
	

	if (switch_core_codec_init
		(&tech_pvt->read_codec, "L16", rate, 20, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
		 switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s Cannot set read codec\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init
		(&tech_pvt->write_codec, "L16", rate, 20, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
		 switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s Cannot set read codec\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel);
		return SWITCH_STATUS_FALSE;
	}
	tech_pvt->read_frame.rate = rate;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(session, &tech_pvt->write_codec);

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(channel, CS_RING);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status wanpipe_on_ring(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "WANPIPE RING\n");

	

	return SWITCH_STATUS_SUCCESS;
}

static switch_status wanpipe_on_hangup(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	struct channel_map *chanmap;

	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	chanmap = tech_pvt->spri->private;

	sangoma_socket_close(&tech_pvt->socket);

	switch_core_codec_destroy(&tech_pvt->read_codec);
	switch_core_codec_destroy(&tech_pvt->write_codec);


	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "WANPIPE HANGUP\n");

	pri_hangup(tech_pvt->spri->pri, tech_pvt->call, tech_pvt->cause);
	pri_destroycall(tech_pvt->spri->pri, tech_pvt->call);

	if (chanmap->map[tech_pvt->callno]) {
		chanmap->map[tech_pvt->callno] = NULL;
	}

	/*
	pri_hangup(tech_pvt->spri->pri,
			   tech_pvt->hangup_event.hangup.call ? tech_pvt->hangup_event.hangup.call : tech_pvt->ring_event.ring.call,
			   tech_pvt->cause);
	pri_destroycall(tech_pvt->spri->pri,
					tech_pvt->hangup_event.hangup.call ? tech_pvt->hangup_event.hangup.call : tech_pvt->ring_event.ring.call);
	*/

	return SWITCH_STATUS_SUCCESS;
}

static switch_status wanpipe_on_loopback(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "WANPIPE LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status wanpipe_on_transmit(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "WANPIPE TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status wanpipe_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile,
											  switch_core_session **new_session)
{
	if ((*new_session = switch_core_session_request(&wanpipe_endpoint_interface, NULL))) {
		struct private_object *tech_pvt;
		switch_channel *channel;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt =
			 (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object)))) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		if (outbound_profile) {
			char name[128];
			switch_caller_profile *caller_profile;
			struct sangoma_pri *spri;
			int span = 0, autospan = 0, autochan = 0;
			char *num, *p;
			int channo = 0;
			struct channel_map *chanmap = NULL;

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			num = caller_profile->destination_number;
			if ((p = strchr(num, '/'))) {
				*p++ = '\0';
				if (*num != 'a') {
					span = atoi(num);
				} else {
					span = 1;
					autospan = 1;
				}
				num = p;
				if ((p = strchr(num, '/'))) {
					*p++ = '\0';
					if (*num == 'a') {
						autochan = 1;
					} else if (*num == 'A') {
						autochan = -1;
					} else {
						channo = atoi(num);
					}
					caller_profile->destination_number = p;
				}
				
			}
			
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
			snprintf(name, sizeof(name), "WanPipe/%s-%04x", caller_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);

			do {
				if ((spri = &SPANS[span]->spri)) {
					chanmap = spri->private;
					if (channo == 0) {
						if (autochan > 0) {
							for(channo = 1; channo < SANGOMA_MAX_CHAN_PER_SPAN; channo++) {
								if ((SPANS[span]->bchans & (1 << channo)) && !chanmap->map[channo]) {
									switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Choosing channel %d\n", channo);
									break;
								}
							}
						} else if (autochan < 0) {
							for(channo = SANGOMA_MAX_CHAN_PER_SPAN; channo > 0; channo--) {
								if ((SPANS[span]->bchans & (1 << channo)) && !chanmap->map[channo]) {
									switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Choosing channel %d\n", channo);
									break;
								}
							}
						}

						if (channo <= 0 || channo == (SANGOMA_MAX_CHAN_PER_SPAN)) {
							switch_console_printf(SWITCH_CHANNEL_CONSOLE, "No Free Channels!\n");
							channo = 0;
						}
					}
					if (channo) {
						break;
					}
				}
			} while(autospan && span < MAX_SPANS && !spri && !channo);


			if (!spri || channo == 0 || channo == (SANGOMA_MAX_CHAN_PER_SPAN)) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "No Free Channels!\n");
				switch_core_session_destroy(new_session);
				return SWITCH_STATUS_GENERR;
			}
			
			if (spri && (tech_pvt->call = pri_new_call(spri->pri))) {
				struct pri_sr *sr;
				int mtu_mru;
				wanpipe_tdm_api_t tdm_api;

				sr = pri_sr_new();
				pri_sr_set_channel(sr, channo, 0, 0);
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
					return SWITCH_STATUS_GENERR;
				}
				if ((tech_pvt->socket = sangoma_create_socket_intr(spri->span, channo)) < 0) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't open fd!\n");
					switch_core_session_destroy(new_session);
					pri_sr_free(sr);
                    return SWITCH_STATUS_GENERR;
				}
				pri_sr_free(sr);
				chanmap->map[channo] = *new_session;
				tech_pvt->spri = spri;

				mtu_mru = sangoma_tdm_get_usr_mtu_mru(tech_pvt->socket, &tdm_api);	
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "MTU is %d\n", mtu_mru);
			}
			
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

static switch_status wanpipe_answer_channel(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_INBOUND)) {
		pri_answer(tech_pvt->spri->pri, tech_pvt->call, 0, 1);
	}
	return SWITCH_STATUS_SUCCESS;
}



static switch_status wanpipe_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
										switch_io_flag flags, int stream_id)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	void *bp;
	int bytes = 0, res = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (tech_pvt->socket <= 0) {
		return SWITCH_STATUS_GENERR;
	}

	bp = tech_pvt->databuf;

	*frame = NULL;
	memset(tech_pvt->databuf, 0, sizeof(tech_pvt->databuf));
	while (bytes < globals.bytes_per_frame) {
		if ((res = sangoma_socket_waitfor(tech_pvt->socket, timeout, POLLIN | POLLERR)) < 0) {
			return SWITCH_STATUS_GENERR;
		} else if (res == 0) {
			tech_pvt->read_frame.datalen = 0;
			return SWITCH_STATUS_SUCCESS;
		}

		if ((res = sangoma_readmsg_socket(tech_pvt->socket,
										  &tech_pvt->hdrframe,
										  sizeof(tech_pvt->hdrframe), bp, sizeof(tech_pvt->databuf) - bytes, 0)) < 0) {
			if (errno == EBUSY) {
				continue;
			} else {
				return SWITCH_STATUS_GENERR;
			}
		}
		bytes += res;
		bp += bytes;
	}
	tech_pvt->read_frame.datalen = bytes;

	*frame = &tech_pvt->read_frame;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status wanpipe_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
										 switch_io_flag flags, int stream_id)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	int res = 0;
	int bytes = frame->datalen;
	void *bp = frame->data;
	switch_status status = SWITCH_STATUS_SUCCESS;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	while (bytes > 0) {
		sangoma_socket_waitfor(tech_pvt->socket, -1, POLLOUT | POLLERR | POLLHUP);
		res = sangoma_sendmsg_socket(tech_pvt->socket,
									 &tech_pvt->hdrframe, sizeof(tech_pvt->hdrframe), bp, PACKET_LEN, 0);
		if (res < 0) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE,
								  "Bad Write frame len %d write %d bytes returned %d errno %d!\n", frame->datalen,
								  PACKET_LEN, res, errno);
			if (errno == EBUSY) {
				continue;
			}
			status = SWITCH_STATUS_GENERR;
			break;
		} else {
			bytes -= res;
			bp += res;
			res = 0;
		}
	}

	return status;
}

static const switch_io_routines wanpipe_io_routines = {
	/*.outgoing_channel */ wanpipe_outgoing_channel,
	/*.answer_channel */ wanpipe_answer_channel,
	/*.read_frame */ wanpipe_read_frame,
	/*.write_frame */ wanpipe_write_frame
};

static const switch_state_handler_table wanpipe_state_handlers = {
	/*.on_init */ wanpipe_on_init,
	/*.on_ring */ wanpipe_on_ring,
	/*.on_execute */ NULL,
	/*.on_hangup */ wanpipe_on_hangup,
	/*.on_loopback */ wanpipe_on_loopback,
	/*.on_transmit */ wanpipe_on_transmit
};

static const switch_endpoint_interface wanpipe_endpoint_interface = {
	/*.interface_name */ "wanpipe",
	/*.io_routines */ &wanpipe_io_routines,
	/*.state_handlers */ &wanpipe_state_handlers,
	/*.private */ NULL,
	/*.next */ NULL
};

static const switch_loadable_module_interface wanpipe_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ &wanpipe_endpoint_interface,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	memset(SPANS, 0, sizeof(SPANS));

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	/* start the pri's */
	config_wanpipe(0);

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &wanpipe_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}





/*event Handlers */

static int on_info(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "number is: %s\n", event->ring.callednum);
	if (strlen(event->ring.callednum) > 3) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "final number is: %s\n", event->ring.callednum);
		pri_answer(spri->pri, event->ring.call, 0, 1);
	}
	return 0;
}

static int on_hangup(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	struct channel_map *chanmap;
	switch_core_session *session;
	struct private_object *tech_pvt;

	chanmap = spri->private;
	if ((session = chanmap->map[event->hangup.channel])) {
		switch_channel *channel = NULL;

		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		tech_pvt = switch_core_session_get_private(session);
		assert(tech_pvt != NULL);

		if (!tech_pvt->call) {
			tech_pvt->call = event->hangup.call;
		}

		tech_pvt->cause = event->hangup.cause;

		switch_channel_set_state(channel, CS_HANGUP);
		chanmap->map[event->hangup.channel] = NULL;
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "-- Hanging up channel %d\n", event->hangup.channel);
	return 0;
}

static int on_answer(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	switch_core_session *session;
	switch_channel *channel;
	struct channel_map *chanmap;

	chanmap = spri->private;

	if ((session = chanmap->map[event->answer.channel])) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "-- Answer on channel %d\n", event->answer.channel);
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);
		switch_channel_answer(channel);
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "-- Answer on channel %d but it's not in use?\n", event->answer.channel);
	}

	return 0;
}

static int on_ringing(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	switch_core_session *session;
	switch_channel *channel;
	struct channel_map *chanmap;
	struct private_object *tech_pvt;

	chanmap = spri->private;

	if ((session = chanmap->map[event->ringing.channel])) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "-- Ringing on channel %d\n", event->ringing.channel);
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		pri_proceeding(spri->pri, event->ringing.call, event->ringing.channel, 0);
		pri_acknowledge(spri->pri, event->ringing.call, event->ringing.channel, 0);

		tech_pvt = switch_core_session_get_private(session);
		if (!tech_pvt->call) {
			tech_pvt->call = event->ringing.call;
		}
		tech_pvt->callno = event->ring.channel;

	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "-- Ringing on channel %d but it's not in use?\n", event->ringing.channel);
	}

	return 0;
}

static int on_ring(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	char name[128];
	switch_core_session *session;
	switch_channel *channel;
	struct channel_map *chanmap;



	chanmap = spri->private;
	if (chanmap->map[event->ring.channel]) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "--Duplicate Ring on channel %d (ignored)\n",
							  event->ring.channel);
		return 0;
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "-- Ring on channel %d (from %s to %s)\n", event->ring.channel,
						  event->ring.callingnum, event->ring.callednum);


	pri_proceeding(spri->pri, event->ring.call, event->ring.channel, 0);
	pri_acknowledge(spri->pri, event->ring.call, event->ring.channel, 0);

	if ((session = switch_core_session_request(&wanpipe_endpoint_interface, NULL))) {
		struct private_object *tech_pvt;
		int fd;
		char ani2str[4] = "";
		//wanpipe_tdm_api_t tdm_api;

		switch_core_session_add_stream(session, NULL);
		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object)))) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			sprintf(name, "w%dg%d", spri->span, event->ring.channel);
			switch_channel_set_name(channel, name);
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hey where is my memory pool?\n");
			switch_core_session_destroy(&session);
			return 0;
		}

		if (event->ring.ani2 >= 0) {
			snprintf(ani2str, 5, "%.2d", event->ring.ani2);
		}

		if ((tech_pvt->caller_profile = switch_caller_profile_new(session,
																  globals.dialplan,
																  "wanpipe fixme",
																  event->ring.callingnum,
																  event->ring.callingani,
																  switch_strlen_zero(ani2str) ? NULL : ani2str,
																  NULL,
																  event->ring.callednum))) {
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}

		switch_set_flag(tech_pvt, TFLAG_INBOUND);
		tech_pvt->spri = spri;
		tech_pvt->cause = -1;

		if (!tech_pvt->call) {
			tech_pvt->call = event->ring.call;
		}

		tech_pvt->callno = event->ring.channel;

		if ((fd = sangoma_create_socket_intr(spri->span, event->ring.channel)) < 0) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't open fd!\n");
		}
		//sangoma_tdm_set_hw_period(fd, &tdm_api, 480);

		tech_pvt->socket = fd;
		chanmap->map[event->ring.channel] = session;

		switch_channel_set_state(channel, CS_INIT);
		switch_core_session_thread_launch(session);
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cannot Create new Inbound Channel!\n");
	}


	return 0;
}

static int check_flags(struct sangoma_pri *spri)
{

	return 0;
}

static int on_restart(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	int fd;
	switch_core_session *session;
	struct channel_map *chanmap;

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "-- Restarting channel %d\n", event->restart.channel);

	chanmap = spri->private;
	
	if ((session = chanmap->map[event->restart.channel])) {
		switch_channel *channel;
		channel = switch_core_session_get_channel(session);
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hanging Up %s\n", switch_channel_get_name(channel));
		switch_channel_hangup(channel);
	}
	
	if ((fd = sangoma_create_socket_intr(spri->span, event->restart.channel)) < 0) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't open fd!\n");
	} else {
		close(fd);
	}
	return 0;
}

static int on_anything(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Caught Event %d (%s)\n", event_type,
						  sangoma_pri_event_str(event_type));
	return 0;
}


static void *pri_thread_run(switch_thread *thread, void *obj)
{
	struct sangoma_pri *spri = obj;
	struct channel_map chanmap;
	switch_event *s_event;
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_ANY, on_anything);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_RING, on_ring);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_RINGING, on_ringing);
	//SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_SETUP_ACK, on_ringing);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_ANSWER, on_answer);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_HANGUP_REQ, on_hangup);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_INFO_RECEIVED, on_info);
	SANGOMA_MAP_PRI_EVENT((*spri), SANGOMA_PRI_EVENT_RESTART, on_restart);

	spri->on_loop = check_flags;
	spri->private = &chanmap;

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
	switch_thread *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_thread_create(&thread, thd_attr, pri_thread_run, spri, module_pool);

}

static int config_wanpipe(int reload)
{
	switch_config cfg;
	char *var, *val;
	int count = 0;
	char *cf = "wanpipe.conf";
	int current_span = 0;

	globals.bytes_per_frame = DEFAULT_BYTES_PER_FRAME;


	if (!switch_config_open_file(&cfg, cf)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "bpf")) {
				globals.bytes_per_frame = atoi(val);
			}
		} else if (!strcasecmp(cfg.category, "span")) {
			if (!strcmp(var, "span")) {
				current_span = atoi(val);
				if (current_span <= 0 || current_span > MAX_SPANS) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid SPAN!\n");
					current_span = 0;
					continue;
				}
				if (!SPANS[current_span]) {
					if (!(SPANS[current_span] = switch_core_alloc(module_pool, sizeof(*SPANS[current_span])))) {
						switch_console_printf(SWITCH_CHANNEL_CONSOLE, "MEMORY ERROR\n");
						break;;
					}
					SPANS[current_span]->span = current_span;
				}
				
			} else {
				if (!current_span) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid option %s when no span defined.\n", var);
					continue;
				}
				
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
							switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid bchan range!\n");
						}
					} else {
						int i = atoi(val);
						if (i > 0 && i < 31) {
							SPANS[current_span]->bchans |= (1 << i);
						} else {
							switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid bchan!\n");
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
				} else if (!strcmp(var, "dialplan")) {
					set_global_dialplan(val);
				}
			}
		}
	}
	switch_config_close_file(&cfg);

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	for(current_span = 1; current_span < MAX_SPANS; current_span++) {
		if (SPANS[current_span]) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Launch span %d\n", current_span);
			if (!SPANS[current_span]->l1) {
				SPANS[current_span]->l1 = PRI_LAYER_1_ULAW;
			}
			sangoma_init_pri(&SPANS[current_span]->spri,
							 current_span,
							 SPANS[current_span]->dchan,
							 SPANS[current_span]->pswitch,
							 SPANS[current_span]->node,
							 globals.debug);
			pri_thread_launch(&SPANS[current_span]->spri);
		}
	}



	return count;

}



