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
 * mod_iax.c -- IAX2 Endpoint Module
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_iax_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_iax_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_iax_runtime);
SWITCH_MODULE_DEFINITION(mod_iax, mod_iax_load, mod_iax_shutdown, mod_iax_runtime);

#include <iax2.h>
#include <iax-client.h>
#include <iax2-parser.h>
#ifdef WIN32
#include <sys/timeb.h>
#endif

switch_endpoint_interface_t *iax_endpoint_interface;
static switch_memory_pool_t *module_pool = NULL;
static int running = 1;


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

static struct {
	int debug;
	char *ip;
	int port;
	char *dialplan;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	unsigned int flags;
	int fd;
	int calls;
	switch_mutex_t *mutex;
} globals;

struct private_object {
	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t cng_frame;
	unsigned char cng_databuf[10];
	switch_core_session_t *session;
	struct iax_session *iax_session;
	switch_caller_profile_t *caller_profile;
	unsigned int codec;
	unsigned int codecs;
	unsigned short samprate;
	switch_mutex_t *mutex;
	switch_mutex_t *flag_mutex;
	//switch_thread_cond_t *cond;
};

typedef struct private_object private_t;


SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_rates_string, globals.codec_rates_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ip, globals.ip);


static char *IAXNAMES[] = { "IAX_EVENT_CONNECT", "IAX_EVENT_ACCEPT", "IAX_EVENT_HANGUP", "IAX_EVENT_REJECT", "IAX_EVENT_VOICE",
							"IAX_EVENT_DTMF", "IAX_EVENT_TIMEOUT", "IAX_EVENT_LAGRQ", "IAX_EVENT_LAGRP", "IAX_EVENT_RINGA",
							"IAX_EVENT_PING", "IAX_EVENT_PONG", "IAX_EVENT_BUSY", "IAX_EVENT_ANSWER", "IAX_EVENT_IMAGE",
							"IAX_EVENT_AUTHRQ", "IAX_EVENT_AUTHRP", "IAX_EVENT_REGREQ", "IAX_EVENT_REGACK",
							"IAX_EVENT_URL", "IAX_EVENT_LDCOMPLETE", "IAX_EVENT_TRANSFER", "IAX_EVENT_DPREQ",
							"IAX_EVENT_DPREP", "IAX_EVENT_DIAL", "IAX_EVENT_QUELCH", "IAX_EVENT_UNQUELCH",
							"IAX_EVENT_UNLINK", "IAX_EVENT_LINKREJECT", "IAX_EVENT_TEXT", "IAX_EVENT_REGREJ",
							"IAX_EVENT_LINKURL", "IAX_EVENT_CNG", "IAX_EVENT_REREQUEST", "IAX_EVENT_TXREPLY",
							"IAX_EVENT_TXREJECT", "IAX_EVENT_TXACCEPT", "IAX_EVENT_TXREADY"
};


struct ast_iana {
	const unsigned int ast;
	const int iana;
	char *name;
};

//999 means it's wrong nad i dont know the real one 
static struct ast_iana AST_IANA[] = { {AST_FORMAT_G723_1, 4, "g723.1"},
									  {AST_FORMAT_GSM, 3, "gsm"},
									  {AST_FORMAT_ULAW, 0, "ulaw"},
									  {AST_FORMAT_ALAW, 8, "alaw"},
									  {AST_FORMAT_G726, 999, "g726"},
									  {AST_FORMAT_ADPCM, 999, "adpcm"},
									  {AST_FORMAT_SLINEAR, 10, "slinear"},
									  {AST_FORMAT_LPC10, 7, "lpc10"},
									  {AST_FORMAT_G729A, 18, "g729"},
									  {AST_FORMAT_SPEEX, 97, "speex"},
									  {AST_FORMAT_SPEEX, 98, "speex"},
									  {AST_FORMAT_ILBC, 102, "ilbc"},
									  {AST_FORMAT_MAX_AUDIO, 999, ""},
									  {AST_FORMAT_JPEG, 999, ""},
									  {AST_FORMAT_PNG, 999, ""},
									  {AST_FORMAT_H261, 999, ""},
									  {AST_FORMAT_H263, 999, ""},
									  {AST_FORMAT_MAX_VIDEO, 999, ""},
									  {0, 0}
};

static char *ast2str(int ast)
{
	int x;
	for (x = 0; x < 19; x++) {
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

	for (x = 0; AST_IANA[x].ast; x++) {
		if (AST_IANA[x].iana == iana) {
			ast = AST_IANA[x].ast;
			break;
		}
	}

	return ast;
}

static unsigned short iax_build_codec_rates(void)
{
	int x;
	unsigned short samples = 0;

	for (x = 0; x < globals.codec_rates_last; x++) {
		int rate = atoi(globals.codec_rates[x]);
		switch (rate) {
		case 8:
			samples |= IAX_RATE_8KHZ;
			break;
		case 16:
			samples |= IAX_RATE_16KHZ;
			break;
		case 22:
			samples |= IAX_RATE_22KHZ;
			break;
		case 32:
			samples |= IAX_RATE_32KHZ;
			break;
		case 44:
			samples |= IAX_RATE_44KHZ;
			break;
		case 48:
			samples |= IAX_RATE_48KHZ;
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "I don't know rate %d\n", rate);
			break;
		}
	}
	return samples;
}

typedef enum {
	IAX_SET = 1,
	IAX_QUERY = 2
} iax_io_t;

static switch_status_t iax_set_codec(private_t * tech_pvt, struct iax_session *iax_session,
									 unsigned int *format, unsigned int *cababilities, unsigned short *samprate, iax_io_t io)
{
	char *dname = NULL;
	switch_channel_t *channel;
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	int num_codecs = 0;
	unsigned int local_cap = 0, mixed_cap = 0, chosen = 0, leading = 0;
	int x, srate = 8000;
	uint32_t interval = 0;

	if (globals.codec_string) {
		num_codecs = switch_loadable_module_get_codecs_sorted(codecs, SWITCH_MAX_CODECS, globals.codec_order, globals.codec_order_last);
	} else {
		num_codecs = switch_loadable_module_get_codecs(codecs, SWITCH_MAX_CODECS);
	}

	if (num_codecs <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO codecs?\n");
		return SWITCH_STATUS_GENERR;
	}

	for (x = 0; x < num_codecs; x++) {
		static const switch_codec_implementation_t *imp;
		for (imp = codecs[x]; imp; imp = imp->next) {
			unsigned int codec = iana2ast(imp->ianacode);
			if (io == IAX_QUERY && !(codec & local_cap)) {
				iax_pref_codec_add(iax_session, codec);
			}
			local_cap |= codec;
		}
	}

	switch_assert(codecs[0]);

	if (io == IAX_SET) {
		mixed_cap = (local_cap & *cababilities);
	} else {
		mixed_cap = local_cap;
	}

	leading = iana2ast(codecs[0]->ianacode);
	interval = codecs[0]->microseconds_per_frame / 1000;
	if (io == IAX_QUERY) {
		chosen = leading;
		*format = chosen;
		*cababilities = local_cap;
		if (globals.codec_rates_last) {
			*samprate = iax_build_codec_rates();
			tech_pvt->samprate = *samprate;
		}
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

		if (len) {				/*they sent us a pref and we don't want to be codec master */
			char pref_str[256] = "(";

			for (x = 0; x < len; x++) {
				strncat(pref_str, ast2str(prefs[x]), sizeof(pref_str)-1);
				strncat(pref_str, x == len - 1 ? ")" : ",", sizeof(pref_str)-1);
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Codec Prefs Detected: %s\n", pref_str);

			for (x = 0; x < len; x++) {
				if ((prefs[x] & mixed_cap)) {
					int z;
					chosen = prefs[x];
					for (z = 0; z < num_codecs; z++) {
						static const switch_codec_implementation_t *imp;
						for (imp = codecs[z]; imp; imp = imp->next) {
							if (prefs[x] == iana2ast(imp->ianacode)) {
								dname = imp->iananame;
								interval = imp->microseconds_per_frame / 1000;
								break;
							}
						}
					}
					break;
				}
			}
		} else {
			if (*format & mixed_cap) {	/* is the one we asked for here? */
				chosen = *format;
				for (x = 0; x < num_codecs; x++) {
					static const switch_codec_implementation_t *imp;
					for (imp = codecs[x]; imp; imp = imp->next) {
						unsigned int cap = iana2ast(imp->ianacode);
						if (cap == chosen) {
							dname = imp->iananame;
							interval = imp->microseconds_per_frame / 1000;
							break;
						}
					}
				}
			} else {			/* c'mon there has to be SOMETHING... */
				for (x = 0; x < num_codecs; x++) {
					static const switch_codec_implementation_t *imp;
					for (imp = codecs[x]; imp; imp = imp->next) {
						unsigned int cap = iana2ast(imp->ianacode);
						if (cap & mixed_cap) {
							chosen = cap;
							dname = imp->iananame;
							interval = imp->microseconds_per_frame / 1000;
							break;
						}
					}
				}
			}
		}
	}

	if (!dname && !chosen) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO codecs?\n");
		return SWITCH_STATUS_GENERR;
	}

	channel = switch_core_session_get_channel(tech_pvt->session);

	if (tech_pvt->samprate || *samprate) {
		unsigned short samples = iax_build_codec_rates();
		unsigned short mixed = ((tech_pvt->samprate ? tech_pvt->samprate : *samprate) & samples);

		srate = 8000;

		if (mixed & IAX_RATE_16KHZ) {
			srate = 16000;
		}
		if (mixed & IAX_RATE_22KHZ) {
			srate = 22050;
		}
		if (mixed & IAX_RATE_32KHZ) {
			srate = 32000;
		}
		if (mixed & IAX_RATE_44KHZ) {
			srate = 44000;
		}
		if (mixed & IAX_RATE_48KHZ) {
			srate = 48000;
		}
	}

	if (!strcasecmp(dname, "l16")) {
		switch_set_flag_locked(tech_pvt, TFLAG_LINEAR);
	}
	if (switch_core_codec_init(&tech_pvt->read_codec,
							   dname,
							   NULL,
							   srate,
							   interval,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_GENERR;
	} else {
		if (switch_core_codec_init(&tech_pvt->write_codec,
								   dname,
								   NULL,
								   srate,
								   interval,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_core_codec_destroy(&tech_pvt->read_codec);
			return SWITCH_STATUS_GENERR;
		} else {
			int ms;
			int rate;
			ms = tech_pvt->write_codec.implementation->microseconds_per_frame / 1000;
			rate = tech_pvt->write_codec.implementation->samples_per_second;
			tech_pvt->read_frame.rate = rate;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activate Codec %s/%d %d ms\n", dname, rate, ms);
			tech_pvt->read_frame.codec = &tech_pvt->read_codec;
			switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
			switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
			switch_set_flag_locked(tech_pvt, TFLAG_CODEC);
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

static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_hangup(switch_core_session_t *session);
static switch_status_t channel_on_ring(switch_core_session_t *session);
static switch_status_t channel_on_loopback(switch_core_session_t *session);
static switch_status_t channel_on_transmit(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);


static void iax_err_cb(const char *s)
{
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "IAX  ERR: %s", s);
}

static void iax_out_cb(const char *s)
{
	if (globals.debug > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "IAX INFO: %s", s);
	}
}

static void tech_init(private_t * tech_pvt, switch_core_session_t *session)
{
	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	tech_pvt->cng_frame.data = tech_pvt->cng_databuf;
	tech_pvt->cng_frame.buflen = sizeof(tech_pvt->cng_databuf);
	switch_set_flag((&tech_pvt->cng_frame), SFF_CNG);
	tech_pvt->cng_frame.datalen = 2;
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_core_session_set_private(session, tech_pvt);
	tech_pvt->session = session;
}

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	iax_set_private(tech_pvt->iax_session, tech_pvt);

	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(switch_core_session_get_channel(session), CS_RING);
	switch_mutex_lock(globals.mutex);
	globals.calls++;
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_ring(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL RING\n", switch_channel_get_name(switch_core_session_get_channel(session)));
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(switch_core_session_get_channel(session)));
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	private_t *tech_pvt = switch_core_session_get_private(session);

	switch_assert(tech_pvt != NULL);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);

	if (tech_pvt->read_codec.implementation) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
	}

	if (tech_pvt->write_codec.implementation) {
		switch_core_codec_destroy(&tech_pvt->write_codec);
	}

	switch_mutex_lock(globals.mutex);
	if (tech_pvt->iax_session) {
		if (!switch_test_flag(tech_pvt, TFLAG_HANGUP)) {
			iax_hangup(tech_pvt->iax_session, "Hangup");
			switch_set_flag_locked(tech_pvt, TFLAG_HANGUP);
		}
		iax_session_destroy(&tech_pvt->iax_session);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(switch_core_session_get_channel(session)));
	globals.calls--;
	if (globals.calls < 0) {
		globals.calls = 0;
	}
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (sig) {
	case SWITCH_SIG_KILL:
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
		switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
		break;
	case SWITCH_SIG_BREAK:
		switch_set_flag_locked(tech_pvt, TFLAG_BREAK);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_loopback(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_transmit(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
	assert(switch_core_session_get_private(session));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
	assert(switch_core_session_get_private(session));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);
	if (tech_pvt->iax_session) {
		switch_mutex_lock(globals.mutex);
		iax_send_dtmf(tech_pvt->iax_session, dtmf->digit);
		switch_mutex_unlock(globals.mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_time_t started = switch_timestamp_now();
	unsigned int elapsed;
	switch_byte_t *data;

	switch_assert(tech_pvt != NULL);
	tech_pvt->read_frame.flags = SFF_NONE;
	*frame = NULL;

	while (switch_test_flag(tech_pvt, TFLAG_IO)) {

		if (!switch_test_flag(tech_pvt, TFLAG_CODEC)) {
			switch_yield(1000);
			continue;
		}
		if (switch_test_flag(tech_pvt, TFLAG_BREAK)) {
			switch_clear_flag(tech_pvt, TFLAG_BREAK);
			goto cng;
		}

		if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
			return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(tech_pvt, TFLAG_IO) && switch_test_flag(tech_pvt, TFLAG_DTMF)) {
			switch_clear_flag_locked(tech_pvt, TFLAG_DTMF);
			*frame = &tech_pvt->cng_frame;
			return SWITCH_STATUS_SUCCESS;
		}

		if (switch_test_flag(tech_pvt, TFLAG_IO) && switch_test_flag(tech_pvt, TFLAG_VOICE)) {
			switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
			if (!tech_pvt->read_frame.datalen) {
				continue;
			}
			*frame = &tech_pvt->read_frame;
#ifdef BIGENDIAN
			if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
				switch_swap_linear((*frame)->data, (int) (*frame)->datalen);
			}
#endif
			return SWITCH_STATUS_SUCCESS;
		}

		if (timeout > -1) {
			elapsed = (unsigned int) ((switch_timestamp_now() - started) / 1000);
			if (elapsed >= (unsigned int) timeout) {
				return SWITCH_STATUS_SUCCESS;
			}
		}
		switch_yield(1000);
	}

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

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_FALSE;
	}
#ifdef BIGENDIAN
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
		switch_swap_linear(frame->data, (int) frame->datalen / 2);
	}
#endif
	iax_send_voice(tech_pvt->iax_session, tech_pvt->codec, frame->data, (int) frame->datalen, tech_pvt->write_codec.implementation->samples_per_frame);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		switch_mutex_lock(globals.mutex);
		iax_answer(tech_pvt->iax_session);
		switch_mutex_unlock(globals.mutex);
	}
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	private_t *tech_pvt = (private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_RESPOND:
		{
			if (tech_pvt->iax_session) {
				switch_mutex_lock(globals.mutex);
				iax_reject(tech_pvt->iax_session, msg->string_arg ? msg->string_arg : "Call Rejected");
				switch_set_flag(tech_pvt, TFLAG_HANGUP);
				switch_mutex_unlock(globals.mutex);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		{
			channel_answer_channel(session);
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags)
{
	if ((*new_session = switch_core_session_request(iax_endpoint_interface, pool)) != 0) {
		private_t *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *caller_profile;
		unsigned int req = 0, cap = 0;
		unsigned short samprate = 0;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t))) != 0) {
			channel = switch_core_session_get_channel(*new_session);
			tech_init(tech_pvt, *new_session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (outbound_profile) {
			char name[128];

			switch_snprintf(name, sizeof(name), "IAX/%s-%04x", outbound_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		switch_mutex_lock(globals.mutex);
		tech_pvt->iax_session = iax_session_new();
		switch_mutex_unlock(globals.mutex);

		if (!tech_pvt->iax_session) {
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (iax_set_codec(tech_pvt, tech_pvt->iax_session, &req, &cap, &samprate, IAX_QUERY) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL;
		}

		if (samprate) {
			iax_set_samplerate(tech_pvt->iax_session, samprate);
		}

		iax_call(tech_pvt->iax_session,
				 caller_profile->caller_id_number, caller_profile->caller_id_name, caller_profile->destination_number, NULL, 0, req, cap);

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_CAUSE_SUCCESS;
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
}

static switch_status_t channel_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	struct private_object *tech_pvt = switch_core_session_get_private(session);
	char *body = switch_event_get_body(event);

	switch_assert(tech_pvt != NULL);

	if (!body) {
		body = "";
	}

	iax_send_text(tech_pvt->iax_session, body);

	return SWITCH_STATUS_SUCCESS;
}


switch_state_handler_table_t iax_state_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_ring */ channel_on_ring,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_loopback */ channel_on_loopback,
	/*.on_transmit */ channel_on_transmit
};

switch_io_routines_t iax_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.waitfor_read */ channel_waitfor_read,
	/*.waitfor_write */ channel_waitfor_write,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message*/ channel_receive_message,
	/*.receive_event */ channel_receive_event
};

SWITCH_MODULE_LOAD_FUNCTION(mod_iax_load)
{
	module_pool = pool;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	iax_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	iax_endpoint_interface->interface_name = "iax";
	iax_endpoint_interface->io_routines = &iax_io_routines;
	iax_endpoint_interface->state_handler = &iax_state_handlers;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

#define PHONE_FREED		0
#define PHONE_CALLACCEPTED	1
#define PHONE_RINGING		2
#define PHONE_ANSWERED		3
#define PHONE_CONNECTED		4

static switch_status_t load_config(void)
{
	char *cf = "iax.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);
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
			} else if (!strcmp(var, "port")) {
				globals.port = atoi(val);
			} else if (!strcmp(var, "ip")) {
				set_global_ip(val);
			} else if (!strcmp(var, "codec-master")) {
				if (!strcasecmp(val, "us")) {
					switch_set_flag(&globals, GFLAG_MY_CODEC_PREFS);
				}
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "codec-prefs")) {
				set_global_codec_string(val);
				globals.codec_order_last = switch_separate_string(globals.codec_string, ',', globals.codec_order, SWITCH_MAX_CODECS);
			} else if (!strcmp(var, "codec-rates")) {
				set_global_codec_rates_string(val);
				globals.codec_rates_last = switch_separate_string(globals.codec_rates_string, ',', globals.codec_rates, SWITCH_MAX_CODECS);
			}
		}
	}

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	if (!globals.port) {
		globals.port = 4569;
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tech_media(private_t * tech_pvt, struct iax_event *iaxevent)
{
	unsigned int cap = iax_session_get_capability(iaxevent->session);
	unsigned int format = iaxevent->ies.format;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!switch_test_flag(tech_pvt, TFLAG_CODEC) &&
		(status = iax_set_codec(tech_pvt, iaxevent->session, &format, &cap, &iaxevent->ies.samprate, IAX_SET)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec Error %u %u\n", iaxevent->ies.format, iaxevent->ies.capability);
	}

	return status;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_iax_runtime)
{
	struct iax_event *iaxevent = NULL;
	switch_event_t *s_event;
	if (load_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	if (globals.debug > 1) {
		iax_enable_debug();
	}
	if (iax_init(globals.ip, globals.port) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Binding Port!\n");
		return SWITCH_STATUS_TERM;
	}

	iax_set_error(iax_err_cb);
	iax_set_output(iax_out_cb);
	globals.fd = iax_get_fd();

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "IAX Ready Port %d\n", globals.port);

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_iax2._udp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", globals.port);
		switch_event_fire(&s_event);
	}

	for (;;) {
		if (running == -1) {
			break;
		}

		/* Wait for an event. */
		if ((iaxevent = iax_get_event(0)) == NULL) {
			int waitlen = 0;

			if (globals.calls == 0) {
				waitlen = 10000;
			} else if (globals.calls < 10) {
				waitlen = 1000;
			} else {
				waitlen = 500;
			}

			switch_yield(waitlen);
			continue;
		} else {
			private_t *tech_pvt = NULL;
			switch_channel_t *channel = NULL;

			if ((tech_pvt = iax_get_private(iaxevent->session))) {
				channel = switch_core_session_get_channel(tech_pvt->session);
				switch_core_session_signal_lock(tech_pvt->session);
			}

			if (globals.debug && iaxevent->etype != IAX_EVENT_VOICE) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Event %d [%s]!\n", iaxevent->etype, IAXNAMES[iaxevent->etype]);
			}

			switch_mutex_lock(globals.mutex);
			switch (iaxevent->etype) {
			case IAX_EVENT_REGACK:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Registration completed successfully.\n");
				//if (iaxevent->ies.refresh)
				//refresh = iaxevent->ies.refresh;
				break;
			case IAX_EVENT_REGREJ:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Registration failed.\n");
				break;
			case IAX_EVENT_REGREQ:
				/* what is the right way to handle this? */
				iax_reject_registration(iaxevent->session, NULL);
				break;
			case IAX_EVENT_TIMEOUT:
				break;
			case IAX_EVENT_ACCEPT:
				if (channel && !switch_channel_test_flag(channel, CF_ANSWERED)) {
					if (tech_media(tech_pvt, iaxevent) == SWITCH_STATUS_SUCCESS) {
						switch_channel_mark_pre_answered(channel);
					} else {
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					}
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Call accepted.\n");
				break;
			case IAX_EVENT_RINGA:
				if (channel) {
					switch_core_session_queue_indication(tech_pvt->session, SWITCH_MESSAGE_INDICATE_RINGING);
					switch_channel_mark_ring_ready(channel);
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Ringing heard.\n");
				break;
			case IAX_EVENT_PONG:
				// informative only
				break;
			case IAX_EVENT_ANSWER:
				// the other side answered our call
				if (channel) {
					if (tech_media(tech_pvt, iaxevent) == SWITCH_STATUS_SUCCESS) {
						if (switch_channel_test_flag(channel, CF_ANSWERED)) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "WTF Mutiple Answer %s?\n", switch_channel_get_name(channel));

						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Answer %s\n", switch_channel_get_name(channel));
							switch_channel_mark_answered(channel);
						}
					} else {
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					}
				}
				break;
			case IAX_EVENT_CONNECT:
				// incoming call detected
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
								  "Incoming call connected %s, %s, %s %u/%u\n",
								  iaxevent->ies.called_number,
								  iaxevent->ies.calling_number, iaxevent->ies.calling_name, iaxevent->ies.format, iaxevent->ies.capability);

				if (iaxevent) {
					switch_core_session_t *session;

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "New Inbound Channel %s!\n", iaxevent->ies.calling_name);
					if ((session = switch_core_session_request(iax_endpoint_interface, NULL)) != 0) {
						switch_core_session_add_stream(session, NULL);
						if ((tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(private_t))) != 0) {
							channel = switch_core_session_get_channel(session);
							tech_init(tech_pvt, session);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
							switch_core_session_destroy(&session);
							break;
						}

						if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																				  iaxevent->ies.username,
																				  globals.dialplan,
																				  iaxevent->ies.calling_name,
																				  iaxevent->ies.calling_number,
																				  iax_get_peer_ip(iaxevent->session),
																				  iaxevent->ies.calling_ani,
																				  NULL,
																				  NULL,
																				  modname,
																				  iaxevent->ies.called_context,
																				  iaxevent->ies.called_number)) != 0) {
							char name[128];
							switch_snprintf(name, sizeof(name), "IAX/%s-%04x", tech_pvt->caller_profile->destination_number, rand() & 0xffff);
							switch_channel_set_name(channel, name);
							switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
						}

						if (iax_set_codec(tech_pvt, iaxevent->session,
										  &iaxevent->ies.format, &iaxevent->ies.capability, &iaxevent->ies.samprate, IAX_SET) != SWITCH_STATUS_SUCCESS) {
							iax_reject(iaxevent->session, "Codec Error!");
							switch_core_session_destroy(&session);
						} else {
							tech_pvt->iax_session = iaxevent->session;
							tech_pvt->session = session;
							iax_accept(tech_pvt->iax_session, tech_pvt->codec);
							iax_ring_announce(tech_pvt->iax_session);
							switch_channel_set_state(channel, CS_INIT);
							if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
								switch_core_session_destroy(&session);
							}
						}
					}
				}
				break;
			case IAX_EVENT_REJECT:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rejected call.\n");
			case IAX_EVENT_BUSY:
			case IAX_EVENT_HANGUP:
				if (channel) {
					switch_mutex_lock(tech_pvt->flag_mutex);
					switch_clear_flag(tech_pvt, TFLAG_IO);
					switch_clear_flag(tech_pvt, TFLAG_VOICE);
					switch_mutex_unlock(tech_pvt->flag_mutex);


					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hangup %s\n", switch_channel_get_name(channel));
					switch_set_flag_locked(tech_pvt, TFLAG_HANGUP);
					switch_channel_hangup(channel, iaxevent->etype == IAX_EVENT_HANGUP ? SWITCH_CAUSE_NORMAL_CLEARING : SWITCH_CAUSE_FACILITY_REJECTED);
					iaxevent->session = NULL;
				}
				break;
			case IAX_EVENT_CNG:
				// pseudo-silence
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "sending silence\n");
				break;
			case IAX_EVENT_VOICE:
				if (tech_pvt && (tech_pvt->read_frame.datalen = iaxevent->datalen) != 0) {
					if (channel && switch_channel_get_state(channel) <= CS_HANGUP) {
						int bytes, frames;

						if (!switch_test_flag(tech_pvt, TFLAG_CODEC)) {
							break;
						}

						if (tech_pvt->read_codec.implementation->encoded_bytes_per_frame) {
							bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
							frames = (int) (tech_pvt->read_frame.datalen / bytes);
						} else {
							frames = 1;
						}

						tech_pvt->read_frame.samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
						memcpy(tech_pvt->read_frame.data, iaxevent->data, iaxevent->datalen);
						/* wake up the i/o thread */
						switch_set_flag_locked(tech_pvt, TFLAG_VOICE);
					}
				}
				break;
			case IAX_EVENT_TRANSFER:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Call transfer occurred.\n");
				break;
			case IAX_EVENT_DTMF:
				if (channel) {
					switch_dtmf_t dtmf = { (char) iaxevent->subclass , SWITCH_DEFAULT_DTMF_DURATION };
					if (globals.debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%c DTMF %s\n", dtmf.digit, switch_channel_get_name(channel));
					}
					switch_mutex_lock(tech_pvt->flag_mutex);
					switch_channel_queue_dtmf(channel, &dtmf);
					switch_set_flag(tech_pvt, TFLAG_DTMF);
					switch_mutex_unlock(tech_pvt->flag_mutex);
				}

				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Don't know what to do with IAX event %d.\n", iaxevent->etype);
				break;
			}
			iax_event_free(iaxevent);
			switch_mutex_unlock(globals.mutex);			
			if (tech_pvt && tech_pvt->session) {
				switch_core_session_signal_unlock(tech_pvt->session);
			}
		}
	}

	running = 0;

	return SWITCH_STATUS_TERM;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_iax_shutdown)
{
	int x = 0;

	running = -1;

	if (globals.fd) {
		/* Die Mutha $%#$@% Die Mutha $#%#$^ Die */
		shutdown(globals.fd, 2);
	}
	iax_shutdown();

	while (running) {
		if (x++ > 100) {
			break;
		}
		switch_yield(20000);
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
