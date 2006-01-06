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
 * mod_iaxchan.c -- IAX2 Endpoint Module
 *
 */
#include <switch.h>

#ifdef WIN32
#include <iax2.h>
#include <iax-client.h>
#include <iax2-parser.h>
#include <sys/timeb.h>
#else
#include <iax/iax2.h>
#include <iax/iax-client.h>
#include <iax/iax2-parser.h>
#endif

static const char modname[] = "mod_iaxchan";

static switch_memory_pool *module_pool;
static int running = 1;


typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_LINEAR = (1 << 6)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

static struct {
	int debug;
	int port;
	char *dialplan;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	unsigned int flags;
} globals;

struct private_object {
	unsigned int flags;
	switch_codec read_codec;
	switch_codec write_codec;
	struct switch_frame read_frame;
	unsigned char databuf[1024];
	switch_core_session *session;
	struct iax_session *iax_session;
	switch_caller_profile *caller_profile;	
	unsigned int codec;
	unsigned int codecs;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
};

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string)


static char *IAXNAMES[] = {"IAX_EVENT_CONNECT","IAX_EVENT_ACCEPT","IAX_EVENT_HANGUP","IAX_EVENT_REJECT","IAX_EVENT_VOICE",
						   "IAX_EVENT_DTMF","IAX_EVENT_TIMEOUT","IAX_EVENT_LAGRQ","IAX_EVENT_LAGRP","IAX_EVENT_RINGA",
						   "IAX_EVENT_PING","IAX_EVENT_PONG","IAX_EVENT_BUSY","IAX_EVENT_ANSWER","IAX_EVENT_IMAGE",
						   "IAX_EVENT_AUTHRQ","IAX_EVENT_AUTHRP","IAX_EVENT_REGREQ","IAX_EVENT_REGACK",
						   "IAX_EVENT_URL","IAX_EVENT_LDCOMPLETE","IAX_EVENT_TRANSFER","IAX_EVENT_DPREQ",
						   "IAX_EVENT_DPREP","IAX_EVENT_DIAL","IAX_EVENT_QUELCH","IAX_EVENT_UNQUELCH",
						   "IAX_EVENT_UNLINK","IAX_EVENT_LINKREJECT","IAX_EVENT_TEXT","IAX_EVENT_REGREJ",
						   "IAX_EVENT_LINKURL","IAX_EVENT_CNG","IAX_EVENT_REREQUEST","IAX_EVENT_TXREPLY",
						   "IAX_EVENT_TXREJECT","IAX_EVENT_TXACCEPT","IAX_EVENT_TXREADY"};


struct ast_iana {
	const unsigned int ast;
	const int iana;
	char *name;
};

//999 means it's wrong nad i dont know the real one 
static struct ast_iana AST_IANA[] =
	{{AST_FORMAT_G723_1, 4, "g723.1"},
	 {AST_FORMAT_GSM, 3, "gsm"},
	 {AST_FORMAT_ULAW, 0, "ulaw"},
	 {AST_FORMAT_ALAW, 8, "alaw"},
	 {AST_FORMAT_G726, 999, "g726"},
	 {AST_FORMAT_ADPCM, 999, "adpcm"},
	 {AST_FORMAT_SLINEAR, 10, "slinear"},
	 {AST_FORMAT_LPC10, 7, "lpc10"},
	 {AST_FORMAT_G729A, 18, "g729"},
	 {AST_FORMAT_SPEEX, 98, "speex"},
	 {AST_FORMAT_ILBC, 999, "ilbc"},
	 {AST_FORMAT_MAX_AUDIO, 999, ""},
	 {AST_FORMAT_JPEG, 999, ""},
	 {AST_FORMAT_PNG, 999, ""},
	 {AST_FORMAT_H261, 999, ""},
	 {AST_FORMAT_H263, 999, ""},
	 {AST_FORMAT_MAX_VIDEO, 999, ""},
	 {0,0}
	};

static char *ast2str(unsigned int ast)
{
	int x;
	for(x = 0; x < 32; x++) {
		if ((1 << x) == ast) {
			return AST_IANA[x].name;
		}
	}
	return "";
}

static unsigned int iana2ast(int iana)
{
	int x = 0;
	unsigned int ast = 0;

	for(x = 0; AST_IANA[x].ast; x++) {
		if (AST_IANA[x].iana == iana) {
			ast = AST_IANA[x].ast;
			break;
		}
	}

	return ast;
}

typedef enum {
	IAX_SET = 1,
	IAX_QUERY = 2
} iax_io_t;

static switch_status iax_set_codec(struct private_object *tech_pvt, struct iax_session *iax_session, unsigned int *format, unsigned int *cababilities, iax_io_t io)
{
	char *dname = NULL;
	//int rate = 8000;
	//int codec_ms = 20;
	switch_channel *channel;
	switch_codec_interface *codecs[SWITCH_MAX_CODECS];
	int num_codecs = 0;
	unsigned int local_cap = 0, mixed_cap = 0, chosen = 0, leading = 0;
	int x;

	if (globals.codec_string) {
		if (!(num_codecs = switch_loadable_module_get_codecs_sorted(switch_core_session_get_pool(tech_pvt->session),
			codecs,
			SWITCH_MAX_CODECS,
			globals.codec_order,
			globals.codec_order_last)) > 0) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "NO codecs?\n");
				return SWITCH_STATUS_GENERR;
		}
	} else if (!(num_codecs = switch_loadable_module_get_codecs(switch_core_session_get_pool(tech_pvt->session), codecs, SWITCH_MAX_CODECS)) > 0) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "NO codecs?\n");
		return SWITCH_STATUS_GENERR;
	}

	for (x = 0; x < num_codecs; x++) {
		unsigned int codec = iana2ast(codecs[x]->ianacode);
		if (io == IAX_QUERY) {
			iax_pref_codec_add(iax_session, codec);
		}
		local_cap |= codec;
	}

	if (io == IAX_SET) {
		mixed_cap = (local_cap & *cababilities);
	} else {
		mixed_cap = local_cap;
	}

	leading = iana2ast(codecs[0]->ianacode);
	if (io == IAX_QUERY) {
		chosen = leading;
		*format = chosen;
		*cababilities = local_cap;
		return SWITCH_STATUS_SUCCESS;
	} else if (switch_test_flag(&globals, GFLAG_MY_CODEC_PREFS) && (leading & mixed_cap)) {
		chosen = leading;
		dname = codecs[0]->iananame;
	} else {
		unsigned int prefs[32];
		int len = 0;

		if (!switch_test_flag(&globals, GFLAG_MY_CODEC_PREFS)) {
			len = iax_pref_codec_get(iax_session, prefs, sizeof(prefs));
		}

		if (len) { /*they sent us a pref and we don't want to be codec master*/
			char pref_str[256] = "(";

			for (x = 0; x < len; x++) {
				strncat(pref_str, ast2str(prefs[x]), sizeof(pref_str));
				strncat(pref_str, x == len - 1 ? ")" : ",", sizeof(pref_str));
			}

			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec Prefs Detected: %s\n", pref_str);

			for (x = 0; x < len; x++) {
				if ((prefs[x] & mixed_cap)) {
					int z;
					chosen = prefs[x];
					for (z = 0; z < num_codecs; z++) {
						if (prefs[x] == iana2ast(codecs[z]->ianacode)) {
							dname = codecs[z]->iananame;
						}
					}
					break;
				}
			}
		} else {
			if (*format & mixed_cap) { /* is the one we asked for here? */
				chosen = *format;
				for (x = 0; x < num_codecs; x++) {
					unsigned int cap = iana2ast(codecs[x]->ianacode);
					if (cap == chosen) {
						dname = codecs[x]->iananame;
					}
				}
			} else { /* c'mon there has to be SOMETHING...*/
				for (x = 0; x < num_codecs; x++) {
					unsigned int cap = iana2ast(codecs[x]->ianacode);
					if (cap & mixed_cap) {
						chosen = cap;
						dname = codecs[x]->iananame;
					}
				}
			}
		}
	}

	if (!dname && !chosen) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "NO codecs?\n");
		return SWITCH_STATUS_GENERR;
	}

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (!strcasecmp(dname, "l16")) {
		switch_set_flag(tech_pvt, TFLAG_LINEAR);
	}
	if (switch_core_codec_init(&tech_pvt->read_codec,
		dname,
		0,
		0,
		1,
		SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
		NULL,
		switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
			return SWITCH_STATUS_GENERR;
	} else {
		if (switch_core_codec_init(&tech_pvt->write_codec,
			dname,
			0,
			0,
			1,
			SWITCH_CODEC_FLAG_ENCODE |SWITCH_CODEC_FLAG_DECODE, 
			NULL,
			switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
				switch_core_codec_destroy(&tech_pvt->read_codec);	
				return SWITCH_STATUS_GENERR;
		} else {
			int ms;
			int rate;
			ms = tech_pvt->write_codec.implementation->microseconds_per_frame / 1000;
			rate = tech_pvt->write_codec.implementation->samples_per_second;
			tech_pvt->read_frame.rate = rate;
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activate Codec %s/%d %d ms\n", dname, rate, ms);
			tech_pvt->read_frame.codec = &tech_pvt->read_codec;
			switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
			switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
		}
		tech_pvt->codec = chosen;
		tech_pvt->codecs = local_cap;
	}

	if (io == IAX_QUERY) {
		*format = tech_pvt->codec;
		*cababilities = local_cap;
	}

	return SWITCH_STATUS_SUCCESS;
}


static const switch_endpoint_interface channel_endpoint_interface;

static switch_status channel_on_init(switch_core_session *session);
static switch_status channel_on_hangup(switch_core_session *session);
static switch_status channel_on_ring(switch_core_session *session);
static switch_status channel_on_loopback(switch_core_session *session);
static switch_status channel_on_transmit(switch_core_session *session);
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile, switch_core_session **new_session);
static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout, switch_io_flag flags);
static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout, switch_io_flag flags);
static switch_status channel_kill_channel(switch_core_session *session, int sig);


static void iax_err_cb(const char *s)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "IAX  ERR: %s", s);
}

static void iax_out_cb(const char *s)
{
	if (globals.debug) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "IAX INFO: %s", s);
	}
}


/* 
State methods they get called when the state changes to the specific state 
returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status channel_on_init(switch_core_session *session)
{
	switch_channel *channel;
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	iax_set_private(tech_pvt->iax_session, tech_pvt);

	switch_set_flag(tech_pvt, TFLAG_IO);

	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_thread_cond_create(&tech_pvt->cond, switch_core_session_get_pool(session));	
	switch_mutex_lock(tech_pvt->mutex);

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(channel, CS_RING);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_ring(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL RING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_execute(switch_core_session *session)
{

	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_hangup(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag(tech_pvt, TFLAG_IO);
	switch_thread_cond_signal(tech_pvt->cond);

	if (tech_pvt->read_codec.implementation) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
	}

	if (tech_pvt->write_codec.implementation) {
		switch_core_codec_destroy(&tech_pvt->write_codec);
	}

	if (tech_pvt->iax_session) {
		if (!switch_test_flag(tech_pvt, TFLAG_HANGUP)) {
			iax_hangup(tech_pvt->iax_session, "Hangup");
			switch_set_flag(tech_pvt, TFLAG_HANGUP);
		}
		iax_session_destroy(&tech_pvt->iax_session);
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_kill_channel(switch_core_session *session, int sig)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag(tech_pvt, TFLAG_IO);
	switch_clear_flag(tech_pvt, TFLAG_VOICE);
	switch_channel_hangup(channel);
	switch_thread_cond_signal(tech_pvt->cond);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL KILL\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_loopback(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_transmit(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}


/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile, switch_core_session **new_session) 
{
	if ((*new_session = switch_core_session_request(&channel_endpoint_interface, NULL))) {
		struct private_object *tech_pvt;
		switch_channel *channel;
		switch_caller_profile *caller_profile;
		unsigned int req = 0, cap = 0;

		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object)))) {
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
			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
			snprintf(name, sizeof(name), "IAX/%s-%04x", caller_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		if (!(tech_pvt->iax_session = iax_session_new())) {
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}


		if (iax_set_codec(tech_pvt, tech_pvt->iax_session, &req, &cap, IAX_QUERY) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}



		iax_call(tech_pvt->iax_session,
			caller_profile->caller_id_number,
			caller_profile->caller_id_name,
			caller_profile->destination_number,
			NULL, 0, req, cap);

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;

}

static switch_status channel_waitfor_read(switch_core_session *session, int ms)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_waitfor_write(switch_core_session *session, int ms)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status channel_send_dtmf(switch_core_session *session, char *dtmf)
{
	struct private_object *tech_pvt = NULL;
	char *digit;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	if (tech_pvt->iax_session) {
		for(digit = dtmf; *digit; digit++) {
			iax_send_dtmf(tech_pvt->iax_session, *digit);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout, switch_io_flag flags) 
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	for(;;) {
		if (switch_test_flag(tech_pvt, TFLAG_IO)) {
			switch_thread_cond_wait(tech_pvt->cond, tech_pvt->mutex);
			if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
				return SWITCH_STATUS_FALSE;			
			}

			if (switch_test_flag(tech_pvt, TFLAG_IO)) {
				switch_clear_flag(tech_pvt, TFLAG_VOICE);
				if(!tech_pvt->read_frame.datalen) {
					continue;
				}

				*frame = &tech_pvt->read_frame;
				return SWITCH_STATUS_SUCCESS;
			}
		}
		break;
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout, switch_io_flag flags)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	//switch_frame *pframe;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if(!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_FALSE;
	}

#ifndef BIGENDIAN
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
		switch_swap_linear(frame->data, (int)frame->datalen / 2);
	}
#endif
	iax_send_voice(tech_pvt->iax_session, tech_pvt->codec, frame->data, (int)frame->datalen, tech_pvt->write_codec.implementation->samples_per_frame);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status channel_answer_channel(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	iax_answer(tech_pvt->iax_session);

	return SWITCH_STATUS_SUCCESS;
}

static const switch_event_handler_table channel_event_handlers = {
	/*.on_init*/			channel_on_init,
	/*.on_ring*/			channel_on_ring,
	/*.on_execute*/			channel_on_execute,
	/*.on_hangup*/			channel_on_hangup,
	/*.on_loopback*/		channel_on_loopback,
	/*.on_transmit*/		channel_on_transmit
};

static const switch_io_routines channel_io_routines = {
	/*.outgoing_channel*/	channel_outgoing_channel,
	/*.answer_channel*/		channel_answer_channel,
	/*.read_frame*/			channel_read_frame,
	/*.write_frame*/		channel_write_frame,
	/*.kill_channel*/		channel_kill_channel,
	/*.waitfor_read*/		channel_waitfor_read,
	/*.waitfor_write*/		channel_waitfor_write,
	/*.send_dtmf*/			channel_send_dtmf
};

static const switch_endpoint_interface channel_endpoint_interface = {
	/*.interface_name*/		"iax",
	/*.io_routines*/		&channel_io_routines,
	/*.event_handlers*/		&channel_event_handlers,
	/*.private*/			NULL,
	/*.next*/				NULL
};

static const switch_loadable_module_interface channel_module_interface = {
	/*.module_name*/			modname,
	/*.endpoint_interface*/		&channel_endpoint_interface,
	/*.timer_interface*/		NULL,
	/*.dialplan_interface*/		NULL,
	/*.codec_interface*/		NULL,
	/*.application_interface*/	NULL
};


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &channel_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

#define PHONE_FREED		0
#define PHONE_CALLACCEPTED	1
#define PHONE_RINGING		2
#define PHONE_ANSWERED		3
#define PHONE_CONNECTED		4

#define UNREGISTERED		0
#define REGISTERED		1


static switch_status load_config(void)
{
	switch_config cfg;
	char *var, *val;
	char *cf = "iax.conf";

	memset(&globals, 0, sizeof(globals));

	if (!switch_config_open_file(&cfg, cf)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "port")) {
				globals.port = atoi(val);
			} else if (!strcmp(var, "codec_master")) {
				if (!strcasecmp(val, "us")) {
					switch_set_flag(&globals, GFLAG_MY_CODEC_PREFS);
				}
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "codec_prefs")) {
				set_global_codec_string(val);
				globals.codec_order_last = switch_separate_string(globals.codec_string, ',', globals.codec_order, SWITCH_MAX_CODECS);
			}
		}
	}

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}
	if (!globals.port) {
		globals.port = 4569;
	}

	switch_config_close_file(&cfg);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{
	int res;
	int netfd;
	int refresh;
	struct iax_event *iaxevent = NULL;

	load_config();

	if (globals.debug) {
		iax_enable_debug();
	}
	if ((res = iax_init(globals.port) < 0)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Error Binding Port!\n");
		return SWITCH_STATUS_TERM;
	}

	iax_set_error(iax_err_cb);
	iax_set_output(iax_out_cb);
	netfd = iax_get_fd();	

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "IAX Ready Port %d\n", globals.port);

	for(;;) {

		if (running == -1) {
			break;
		}

		/* Wait for an event.*/
		if ((iaxevent = iax_get_event(0)) == NULL) {
			switch_yield(1000);
			continue;
		} else if (iaxevent) {
			struct private_object *tech_pvt = iax_get_private(iaxevent->session);

			if (globals.debug && iaxevent->etype != IAX_EVENT_VOICE) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Event %d [%s]!\n",
					iaxevent->etype, IAXNAMES[iaxevent->etype]);
			}
			switch (iaxevent->etype) {
				case IAX_EVENT_REGACK:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Registration completed successfully.\n");
					if (iaxevent->ies.refresh) refresh = iaxevent->ies.refresh;
					break;
				case IAX_EVENT_REGREJ:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Registration failed.\n");
					break;
				case IAX_EVENT_TIMEOUT:
					break;
				case IAX_EVENT_ACCEPT:
					if (tech_pvt) {					
						unsigned int cap =  iax_session_get_capability(iaxevent->session);
						unsigned int format = iaxevent->ies.format;

						if (iax_set_codec(tech_pvt, iaxevent->session, &format, &cap, IAX_SET) != SWITCH_STATUS_SUCCESS) {
							switch_console_printf(SWITCH_CHANNEL_CONSOLE, "WTF? %d %d\n",iaxevent->ies.format, iaxevent->ies.capability);
						}
					}


					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Call accepted.\n");
					break;
				case IAX_EVENT_RINGA:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Ringing heard.\n");
					break;
				case IAX_EVENT_PONG:
					// informative only
					break;
				case IAX_EVENT_ANSWER:
					// the other side answered our call
					if (tech_pvt) {
						switch_channel *channel;
						if ((channel = switch_core_session_get_channel(tech_pvt->session))) {
							switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Answer %s\n", switch_channel_get_name(channel));
							switch_channel_answer(channel);
						}
					}

					break;
				case IAX_EVENT_CONNECT:
					// incoming call detected
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, 
						"Incoming call connected %s, %s, %s %d/%d\n",
						iaxevent->ies.called_number,
						iaxevent->ies.calling_number,
						iaxevent->ies.calling_name,
						iaxevent->ies.format,
						iaxevent->ies.capability);

					if (iaxevent) {
						switch_core_session *session;

						switch_console_printf(SWITCH_CHANNEL_CONSOLE, "New Inbound Channel %s!\n", iaxevent->ies.calling_name);
						if ((session = switch_core_session_request(&channel_endpoint_interface, NULL))) {
							struct private_object *tech_pvt;
							switch_channel *channel;

							if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object)))) {
								memset(tech_pvt, 0, sizeof(*tech_pvt));
								channel = switch_core_session_get_channel(session);
								switch_core_session_set_private(session, tech_pvt);
								tech_pvt->session = session;
							} else {
								switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hey where is my memory pool?\n");
								switch_core_session_destroy(&session);
								break;
							}


							if ((tech_pvt->caller_profile = switch_caller_profile_new(session,
								globals.dialplan,
								iaxevent->ies.calling_name,
								iaxevent->ies.calling_number,
								iax_get_peer_ip(iaxevent->session),
								iaxevent->ies.calling_ani,
								NULL,
								iaxevent->ies.called_number))) {
									char name[128];
									switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
									snprintf(name, sizeof(name), "IAX/%s-%04x", tech_pvt->caller_profile->destination_number, rand() & 0xffff);
									switch_channel_set_name(channel, name);
							}

							if (iax_set_codec(tech_pvt, iaxevent->session,
								&iaxevent->ies.format,
								&iaxevent->ies.capability,
								IAX_SET) != SWITCH_STATUS_SUCCESS) {
									iax_reject(iaxevent->session, "Codec Error!");
									switch_core_session_destroy(&session);
							} else {
								tech_pvt->iax_session = iaxevent->session;						
								tech_pvt->session = session;
								iax_accept(tech_pvt->iax_session, tech_pvt->codec);
								iax_ring_announce(tech_pvt->iax_session);
								switch_channel_set_state(channel, CS_INIT);
								switch_core_session_thread_launch(session);
							}
						}
					}
					break;
				case IAX_EVENT_REJECT:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Rejected call.\n");
				case IAX_EVENT_BUSY:
				case IAX_EVENT_HANGUP:
					if (tech_pvt) {
						switch_channel *channel;

						switch_clear_flag(tech_pvt, TFLAG_IO);
						switch_clear_flag(tech_pvt, TFLAG_VOICE);
						if ((channel = switch_core_session_get_channel(tech_pvt->session))) {
							switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hangup %s\n", switch_channel_get_name(channel));
							switch_set_flag(tech_pvt, TFLAG_HANGUP);
							switch_channel_hangup(channel);
							switch_thread_cond_signal(tech_pvt->cond);
							iaxevent->session = NULL;
						} else {
							switch_console_printf(SWITCH_CHANNEL_CONSOLE, "No Session? %s\n", switch_test_flag(tech_pvt, TFLAG_VOICE) ? "yes" : "no");
						}
					}
					break;
				case IAX_EVENT_CNG:
					// pseudo-silence
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "sending silence\n");
					break;
				case IAX_EVENT_VOICE:
					if (tech_pvt && (tech_pvt->read_frame.datalen = iaxevent->datalen)) {
						int bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
						int frames = (int)(tech_pvt->read_frame.datalen / bytes);
						tech_pvt->read_frame.samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
						memcpy(tech_pvt->read_frame.data, iaxevent->data, iaxevent->datalen);
						/* wake up the i/o thread*/
						switch_set_flag(tech_pvt, TFLAG_VOICE);
						switch_thread_cond_signal(tech_pvt->cond);
					}				
					break;
				case IAX_EVENT_TRANSFER:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Call transfer occurred.\n");
					//session[0] = iaxevent->session;
					break;
				case IAX_EVENT_DTMF:
					if (tech_pvt) {
						switch_channel *channel;
						if ((channel = switch_core_session_get_channel(tech_pvt->session))) {
							char str[2] = {iaxevent->subclass};
							if (globals.debug) {
								switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s DTMF %s\n", str, switch_channel_get_name(channel));
							}
							switch_channel_queue_dtmf(channel, str);
						}
					}

					break;
				default:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Don't know what to do with IAX event %d.\n", iaxevent->etype);
					break;
			}

			iax_event_free(iaxevent);
		}

	}

	running = 0;

	return SWITCH_STATUS_TERM;
}


SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	int x = 0;

	running = -1;

	iax_shutdown();

	while (running) {
		if (x++ > 100) {
			break;
		}
		switch_yield(20000);
	}
	return SWITCH_STATUS_SUCCESS;
}

