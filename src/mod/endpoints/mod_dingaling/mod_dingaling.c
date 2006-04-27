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
 * mod_dingaling.c -- Jingle Endpoint Module
 *
 */
#include <switch.h>
#include <libdingaling.h>

#define DL_CAND_WAIT 10000000
#define DL_CAND_INITIAL_WAIT 2000000

static const char modname[] = "mod_dingaling";

static switch_memory_pool *module_pool = NULL;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_READING = (1 << 3),
	TFLAG_WRITING = (1 << 4),
	TFLAG_BYE = (1 << 5),
	TFLAG_VOICE = (1 << 6),
	TFLAG_RTP_READY = (1 << 7),
	TFLAG_CODEC_READY = (1 << 8),
	TFLAG_TRANSPORT = (1 << 9),
	TFLAG_ANSWER = (1 << 10),
	TFLAG_VAD_IN = ( 1 << 11),
	TFLAG_VAD_OUT = ( 1 << 12),
	TFLAG_VAD = ( 1 << 13),
	TFLAG_DO_CAND = ( 1 << 14),
	TFLAG_DO_DESC = (1 << 15),
	TFLAG_LANADDR = (1 << 16)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

static struct {
	int debug;
	char *dialplan;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	unsigned int flags;
	unsigned int init;
	switch_hash *profile_hash;
	int running;
	int handles;
} globals;

struct mdl_profile {
	char *name;
	char *login;
	char *password;
	char *message;
	char *dialplan;
	char *ip;
	char *extip;
	char *lanaddr;
	char *exten;
	ldl_handle_t *handle;
	unsigned int flags;
};

struct private_object {
	unsigned int flags;
	switch_codec read_codec;
	switch_codec write_codec;
	struct switch_frame read_frame;
	struct mdl_profile *profile;
	switch_core_session *session;
	switch_caller_profile *caller_profile;
	unsigned short samprate;
	switch_mutex_t *mutex;
	switch_codec_interface *codecs[SWITCH_MAX_CODECS];
	unsigned int num_codecs;
	int codec_index;
	struct switch_rtp *rtp_session;
	ldl_session_t *dlsession;
	char *remote_ip;
	switch_port_t local_port;
	switch_port_t remote_port;
	char local_user[17];
	char local_pass[17];
	char *remote_user;
	unsigned int cand_id;
	unsigned int desc_id;
	char last_digit;
	unsigned int dc;
	time_t last_digit_time;
	switch_queue_t *dtmf_queue;
	char out_digit;
	unsigned char out_digit_packet[4];
	unsigned int out_digit_sofar;
	unsigned int out_digit_dur;
	uint16_t out_digit_seq;
	int32_t timestamp_send;
	int32_t timestamp_recv;
	int32_t timestamp_dtmf;
	uint32_t last_read;
	char *codec_name;
	switch_payload_t codec_num;
	switch_time_t next_desc;
	switch_time_t next_cand;
	char *stun_ip;
	uint16_t stun_port;
};

struct rfc2833_digit {
	char digit;
	int duration;
};


SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan)
	 SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string)
	 SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_rates_string, globals.codec_rates_string)


	 static switch_status channel_on_init(switch_core_session *session);
	 static switch_status channel_on_hangup(switch_core_session *session);
	 static switch_status channel_on_ring(switch_core_session *session);
	 static switch_status channel_on_loopback(switch_core_session *session);
	 static switch_status channel_on_transmit(switch_core_session *session);
	 static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile,
												   switch_core_session **new_session, switch_memory_pool *pool);
	 static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
											 switch_io_flag flags, int stream_id);
	 static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
											  switch_io_flag flags, int stream_id);
	 static switch_status channel_kill_channel(switch_core_session *session, int sig);
	 static ldl_status handle_signalling(ldl_handle_t *handle, ldl_session_t *dlsession, ldl_signal_t signal, char *msg);
	 static ldl_status handle_response(ldl_handle_t *handle, char *id);
	 static switch_status load_config(void);

	 static void dl_logger(char *file, const char *func, int line, int level, char *fmt, ...)
{
	va_list ap;
	char data[1024];

	va_start(ap, fmt);
	
	vsnprintf(data, sizeof(data), fmt, ap);
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, SWITCH_LOG_DEBUG, data);

	va_end(ap);
}

static void get_codecs(struct private_object *tech_pvt)
{
	assert(tech_pvt != NULL);
	assert(tech_pvt->session != NULL);

	if (globals.codec_string) {
		if ((tech_pvt->num_codecs = switch_loadable_module_get_codecs_sorted(tech_pvt->codecs,
																			 SWITCH_MAX_CODECS,
																			 globals.codec_order,
																			 globals.codec_order_last)) <= 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO codecs?\n");
			return;
		}
	} else if (((tech_pvt->num_codecs =
				 switch_loadable_module_get_codecs(switch_core_session_get_pool(tech_pvt->session), tech_pvt->codecs, SWITCH_MAX_CODECS))) <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO codecs?\n");
		return;
	}
}



static void *SWITCH_THREAD_FUNC handle_thread_run(switch_thread *thread, void *obj)
{
	ldl_handle_t *handle = obj;
	struct mdl_profile *profile = NULL;

	profile = ldl_handle_get_private(handle);
	globals.handles++;
	ldl_handle_run(handle);
	globals.handles--;
	ldl_handle_destroy(&handle);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Handle %s [%s] Destroyed\n", profile->name, profile->login);
	
	return NULL;
}

static void handle_thread_launch(ldl_handle_t *handle)
{
	switch_thread *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, handle_thread_run, handle, module_pool);

}


static int activate_rtp(struct private_object *tech_pvt)
{
	switch_channel *channel = switch_core_session_get_channel(tech_pvt->session);
	const char *err;
	int ms = 20;

	if (tech_pvt->rtp_session) {
		return 0;
	}

	if (!strncasecmp(tech_pvt->codec_name, "ilbc", 4)) {
		ms = 30;
	}

	if (switch_core_codec_init(&tech_pvt->read_codec,
							   tech_pvt->codec_name,
							   8000,
							   ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Can't load codec?\n");
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return -1;
	}
	tech_pvt->read_frame.rate = tech_pvt->read_codec.implementation->samples_per_second;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set Read Codec to %s\n", tech_pvt->codec_name);

	if (switch_core_codec_init(&tech_pvt->write_codec,
							   tech_pvt->codec_name,
							   8000,
							   ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Can't load codec?\n");
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return -1;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set Write Codec to %s\n",  tech_pvt->codec_name);
							
	switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
	

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SETUP RTP %s:%d -> %s:%d\n", tech_pvt->profile->ip, tech_pvt->local_port, tech_pvt->remote_ip, tech_pvt->remote_port);

	if (!(tech_pvt->rtp_session = switch_rtp_new(tech_pvt->profile->ip,
												 tech_pvt->local_port,
												 tech_pvt->remote_ip,
												 tech_pvt->remote_port,
												 tech_pvt->codec_num,
												 tech_pvt->read_codec.implementation->encoded_bytes_per_frame,
												 tech_pvt->read_codec.implementation->microseconds_per_frame,
												 SWITCH_RTP_FLAG_USE_TIMER | SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_GOOGLEHACK,
												 NULL,
												 &err, switch_core_session_get_pool(tech_pvt->session)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "RTP ERROR %s\n", err);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return -1;
	} else {
		uint8_t vad_in = switch_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = switch_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = switch_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;
		switch_rtp_activate_ice(tech_pvt->rtp_session, tech_pvt->remote_user, tech_pvt->local_user);
		if ((vad_in && inb) || (vad_out && !inb)) {
			switch_rtp_enable_vad(tech_pvt->rtp_session, tech_pvt->session, &tech_pvt->read_codec, SWITCH_VAD_FLAG_TALKING);
			switch_set_flag(tech_pvt, TFLAG_VAD);
		}
	}

	return 0;
}



static int do_candidates(struct private_object *tech_pvt, int force)
{
	switch_channel *channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DO_CAND)) {
		return 0;
	}



	tech_pvt->next_cand += DL_CAND_WAIT;
	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		return -1;
	}
	switch_set_flag(tech_pvt, TFLAG_DO_CAND);

	if (force || !switch_test_flag(tech_pvt, TFLAG_RTP_READY)) {
		ldl_candidate_t cand[1];
		char *advip = tech_pvt->profile->extip ? tech_pvt->profile->extip : tech_pvt->profile->ip;
		char *err;

		memset(cand, 0, sizeof(cand));
		switch_stun_random_string(tech_pvt->local_user, 16, NULL);
		switch_stun_random_string(tech_pvt->local_pass, 16, NULL);

		if (switch_test_flag(tech_pvt, TFLAG_LANADDR)) {
			advip = tech_pvt->profile->ip;
		}


		cand[0].port = tech_pvt->local_port;
		cand[0].address = advip;
				
		if (!strncasecmp(advip, "stun:", 5)) {
			char *stun_ip = advip + 5;
					
			if (tech_pvt->stun_ip) {
				cand[0].address = tech_pvt->stun_ip;
				cand[0].port = tech_pvt->stun_port;
			} else {
				if (!stun_ip) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! NO STUN SERVER!\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return -1;
				}

				cand[0].address = tech_pvt->profile->ip;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Stun Lookup Local %s:%d\n", cand[0].address, cand[0].port);
				if (switch_stun_lookup(&cand[0].address,
									   &cand[0].port,
									   stun_ip,
									   SWITCH_STUN_DEFAULT_PORT,
									   &err,
									   switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! %s:%d [%s]\n", stun_ip, SWITCH_STUN_DEFAULT_PORT, err);
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return -1;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stun Success %s:%d\n", cand[0].address, cand[0].port);
			}
			cand[0].type = "stun";
			tech_pvt->stun_ip = switch_core_session_strdup(tech_pvt->session, cand[0].address);
			tech_pvt->stun_port = cand[0].port;
		} else {
			cand[0].type = "local";
		}

		cand[0].name = "rtp";
		cand[0].username = tech_pvt->local_user;
		cand[0].password = tech_pvt->local_pass;
		cand[0].pref = 1;
		cand[0].protocol = "udp";
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send Candidate %s:%d [%s]\n", cand[0].address, cand[0].port, cand[0].username);
		tech_pvt->cand_id = ldl_session_candidates(tech_pvt->dlsession, cand, 1);
		switch_set_flag(tech_pvt, TFLAG_RTP_READY);
	}
	switch_clear_flag(tech_pvt, TFLAG_DO_CAND);
	return 0;
}

static char *lame(char *in)
{
	if (!strncasecmp(in, "ilbc", 4)) {
		return "iLBC";
	} else {
		return in;
	}
}

static int do_describe(struct private_object *tech_pvt, int force)
{
	ldl_payload_t payloads[5];
	switch_channel *channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DO_DESC)) {
		return 0;
	}

	tech_pvt->next_desc += DL_CAND_WAIT;

	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		return -1;
	}

	memset(payloads, 0, sizeof(payloads));
	switch_set_flag(tech_pvt, TFLAG_DO_CAND);
	if (!tech_pvt->num_codecs) {
		get_codecs(tech_pvt);
		if (!tech_pvt->num_codecs) {
			switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
			switch_set_flag(tech_pvt, TFLAG_BYE);
			switch_clear_flag(tech_pvt, TFLAG_IO);
			return -1;
		}
	}

	if (force || !switch_test_flag(tech_pvt, TFLAG_CODEC_READY)) {
		if (tech_pvt->codec_index < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Don't have my codec yet here's one\n");
			tech_pvt->codec_name = lame(tech_pvt->codecs[0]->iananame);
			tech_pvt->codec_num = tech_pvt->codecs[0]->ianacode;
			tech_pvt->codec_index = 0;
					
			payloads[0].name = lame(tech_pvt->codecs[0]->iananame);
			payloads[0].id = tech_pvt->codecs[0]->ianacode;
			
		} else {
			payloads[0].name = lame(tech_pvt->codecs[tech_pvt->codec_index]->iananame);
			payloads[0].id = tech_pvt->codecs[tech_pvt->codec_index]->ianacode;
		}

				
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send Describe [%s]\n", payloads[0].name);
		tech_pvt->desc_id = ldl_session_describe(tech_pvt->dlsession, payloads, 1,
												 switch_test_flag(tech_pvt, TFLAG_OUTBOUND) ? LDL_DESCRIPTION_INITIATE : LDL_DESCRIPTION_ACCEPT);
		switch_set_flag(tech_pvt, TFLAG_CODEC_READY);
	} 
	switch_clear_flag(tech_pvt, TFLAG_DO_CAND);
	return 0;
}

static void *SWITCH_THREAD_FUNC negotiate_thread_run(switch_thread *thread, void *obj)
{
	switch_core_session *session = obj;

	switch_channel *channel;
	struct private_object *tech_pvt = NULL;
	switch_time_t started;
	switch_time_t now;
	unsigned int elapsed;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_set_flag(tech_pvt, TFLAG_IO);

	started = switch_time_now();

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		tech_pvt->next_desc = switch_time_now();
	} else {
		tech_pvt->next_cand = switch_time_now() + DL_CAND_WAIT;
		tech_pvt->next_desc = switch_time_now() + DL_CAND_WAIT;
	}

	while(! (switch_test_flag(tech_pvt, TFLAG_CODEC_READY) && 
			 switch_test_flag(tech_pvt, TFLAG_RTP_READY) && 
			 switch_test_flag(tech_pvt, TFLAG_ANSWER) && 
			 switch_test_flag(tech_pvt, TFLAG_TRANSPORT))) {
		now = switch_time_now();
		elapsed = (unsigned int)((now - started) / 1000);

		if (switch_channel_get_state(channel) >= CS_HANGUP || switch_test_flag(tech_pvt, TFLAG_BYE)) {
			return NULL;
		}

		
		if (now >= tech_pvt->next_desc) {
			if (do_describe(tech_pvt, 0) < 0) {
				break;
			}
		}
		
		if (tech_pvt->next_cand && now >= tech_pvt->next_cand) {
			if (do_candidates(tech_pvt, 0) < 0) {
				break;
			}
		}
		if (elapsed > 60000) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			switch_set_flag(tech_pvt, TFLAG_BYE);
			switch_clear_flag(tech_pvt, TFLAG_IO);
			return NULL;
		}
		if (switch_test_flag(tech_pvt, TFLAG_BYE) || ! switch_test_flag(tech_pvt, TFLAG_IO)) {
			return NULL;
		}
		switch_yield(1000);
		//printf("WAIT %s %d %d %d %d\n", switch_channel_get_name(channel), switch_test_flag(tech_pvt, TFLAG_TRANSPORT), switch_test_flag(tech_pvt, TFLAG_CODEC_READY), switch_test_flag(tech_pvt, TFLAG_RTP_READY), switch_test_flag(tech_pvt, TFLAG_ANSWER));
	}
	
	if (switch_channel_get_state(channel) >= CS_HANGUP || switch_test_flag(tech_pvt, TFLAG_BYE)) {
		return NULL;
	}	

	activate_rtp(tech_pvt);
	
	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		do_candidates(tech_pvt, 0);
		switch_channel_answer(channel);
		//printf("***************************ANSWER\n");
	} else {
		switch_core_session_thread_launch(session);
	}
	switch_channel_set_state(channel, CS_INIT);
	return NULL;
}


static void negotiate_thread_launch(switch_core_session *session)
{
	switch_thread *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
	switch_threadattr_detach_set(thd_attr, 1);
	switch_thread_create(&thread, thd_attr, negotiate_thread_run, session, switch_core_session_get_pool(session));

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

	tech_pvt->read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;

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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL RING\n", switch_channel_get_name(channel));

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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


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
	switch_clear_flag(tech_pvt, TFLAG_VOICE);
	switch_set_flag(tech_pvt, TFLAG_BYE);
	
	if (tech_pvt->dlsession) {
		ldl_session_terminate(tech_pvt->dlsession);
		ldl_session_destroy(&tech_pvt->dlsession);
	}

	if (tech_pvt->rtp_session) {
		switch_rtp_destroy(&tech_pvt->rtp_session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NUKE RTP\n");
		tech_pvt->rtp_session = NULL;
	}

	if (tech_pvt->read_codec.implementation) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
	}

	if (tech_pvt->write_codec.implementation) {
		switch_core_codec_destroy(&tech_pvt->write_codec);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_kill_channel(switch_core_session *session, int sig)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	if ((channel = switch_core_session_get_channel(session))) {
		if ((tech_pvt = switch_core_session_get_private(session))) {
			switch_clear_flag(tech_pvt, TFLAG_IO);
			switch_clear_flag(tech_pvt, TFLAG_VOICE);
			switch_set_flag(tech_pvt, TFLAG_BYE);
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			if (tech_pvt->dlsession) {
				ldl_session_terminate(tech_pvt->dlsession);
			}
			if (tech_pvt->rtp_session) {
				switch_rtp_kill_socket(tech_pvt->rtp_session);
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL KILL\n", switch_channel_get_name(channel));
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_loopback(switch_core_session *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_transmit(switch_core_session *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_waitfor_read(switch_core_session *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_waitfor_write(switch_core_session *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status channel_send_dtmf(switch_core_session *session, char *dtmf)
{
	struct private_object *tech_pvt = NULL;
	//char *digit;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
										switch_io_flag flags, int stream_id)
{
	struct private_object *tech_pvt = NULL;
	uint32_t bytes = 0;
	switch_size_t samples = 0, frames = 0, ms = 0;
	switch_channel *channel = NULL;
	switch_payload_t payload = 0;
	switch_status status;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	if (!tech_pvt->rtp_session) {
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->read_frame.datalen = 0;
	switch_set_flag(tech_pvt, TFLAG_READING);

	bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
	samples = tech_pvt->read_codec.implementation->samples_per_frame;
	ms = tech_pvt->read_codec.implementation->microseconds_per_frame;
	tech_pvt->read_frame.datalen = 0;

	
	while (!switch_test_flag(tech_pvt, TFLAG_BYE) && switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->read_frame.datalen == 0) {
		tech_pvt->read_frame.flags = 0;
		status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame);
		
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			return SWITCH_STATUS_FALSE;
		}
		payload = tech_pvt->read_frame.payload;

		/* RFC2833 ... TBD try harder to honor the duration etc.*/
		if (payload == 101) {
			unsigned char *packet = tech_pvt->read_frame.data;
			int end = packet[1]&0x80;
			int duration = (packet[2]<<8) + packet[3];
			char key = switch_rfc2833_to_char(packet[0]);

			/* SHEESH.... Curse you RFC2833 inventors!!!!*/
			if ((time(NULL) - tech_pvt->last_digit_time) > 2) {
				tech_pvt->last_digit = 0;
				tech_pvt->dc = 0;
			}
			if (duration && end) {
				if (key != tech_pvt->last_digit) {
					char digit_str[] = {key, 0};
					time(&tech_pvt->last_digit_time);
					switch_channel_queue_dtmf(channel, digit_str);
				}
				if (++tech_pvt->dc >= 3) {
					tech_pvt->last_digit = 0;
					tech_pvt->dc = 0;
				} else {
					tech_pvt->last_digit = key;
				}
			} 
		}

		if (switch_test_flag(&tech_pvt->read_frame, SFF_CNG)) {
			tech_pvt->read_frame.datalen = tech_pvt->last_read ? tech_pvt->last_read : tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
		}

		if (tech_pvt->read_frame.datalen > 0) {
			bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
			frames = (tech_pvt->read_frame.datalen / bytes);
			samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
			ms = frames * tech_pvt->read_codec.implementation->microseconds_per_frame;
			tech_pvt->timestamp_recv += (int32_t) samples;
			tech_pvt->read_frame.samples = (int) samples;
			tech_pvt->last_read = tech_pvt->read_frame.datalen;
			//printf("READ bytes=%d payload=%d frames=%d samples=%d ms=%d ts=%d sampcount=%d\n", (int)tech_pvt->read_frame.datalen, (int)payload, (int)frames, (int)samples, (int)ms, (int)tech_pvt->timestamp_recv, (int)tech_pvt->read_frame.samples);
			break;
		}

		switch_yield(1000);
	}


	switch_clear_flag(tech_pvt, TFLAG_READING);

	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return SWITCH_STATUS_FALSE;
	}

	*frame = &tech_pvt->read_frame;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
										 switch_io_flag flags, int stream_id)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	switch_status status = SWITCH_STATUS_SUCCESS;
	int bytes = 0, samples = 0, frames = 0;


	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!tech_pvt->rtp_session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_RTP_READY)) {
		return SWITCH_STATUS_SUCCESS;
	}


	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return SWITCH_STATUS_FALSE;
	}

	switch_set_flag(tech_pvt, TFLAG_WRITING);


	bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
	frames = ((int) frame->datalen / bytes);
	samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;

	if (tech_pvt->out_digit_dur > 0) {
		int x, ts, loops = 1, duration;

		tech_pvt->out_digit_sofar += samples;

		if (tech_pvt->out_digit_sofar >= tech_pvt->out_digit_dur) {
			duration = tech_pvt->out_digit_dur;
			tech_pvt->out_digit_packet[1] |= 0x80;
			tech_pvt->out_digit_dur = 0;
			loops = 3;
		} else {
			duration = tech_pvt->out_digit_sofar;
		}

		ts = tech_pvt->timestamp_dtmf += samples;
		tech_pvt->out_digit_packet[2] = (unsigned char) (duration >> 8);
		tech_pvt->out_digit_packet[3] = (unsigned char) duration;
		

		for (x = 0; x < loops; x++) {
			switch_rtp_write_manual(tech_pvt->rtp_session, tech_pvt->out_digit_packet, 4, 0, 101, ts, tech_pvt->out_digit_seq, &frame->flags);
			/*
			  printf("Send %s packet for [%c] ts=%d sofar=%u dur=%d\n", loops == 1 ? "middle" : "end", tech_pvt->out_digit, ts, 
			  tech_pvt->out_digit_sofar, duration);
			*/
		}
	}

	if (!tech_pvt->out_digit_dur && tech_pvt->dtmf_queue && switch_queue_size(tech_pvt->dtmf_queue)) {
		void *pop;

		if (switch_queue_trypop(tech_pvt->dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			int x, ts;
			struct rfc2833_digit *rdigit = pop;
			
			memset(tech_pvt->out_digit_packet, 0, 4);
			tech_pvt->out_digit_sofar = 0;
			tech_pvt->out_digit_dur = rdigit->duration;
			tech_pvt->out_digit = rdigit->digit;
			tech_pvt->out_digit_packet[0] = (unsigned char)switch_char_to_rfc2833(rdigit->digit);
			tech_pvt->out_digit_packet[1] = 7;

			ts = tech_pvt->timestamp_dtmf += samples;
			tech_pvt->out_digit_seq++;
			for (x = 0; x < 3; x++) {
				switch_rtp_write_manual(tech_pvt->rtp_session, tech_pvt->out_digit_packet, 4, 1, 101, ts, tech_pvt->out_digit_seq, &frame->flags);
				/*
				  printf("Send start packet for [%c] ts=%d sofar=%u dur=%d\n", tech_pvt->out_digit, ts, 
				  tech_pvt->out_digit_sofar, 0);
				*/
			}

			free(rdigit);
		}
	}





	//printf("%s send %d bytes %d samples in %d frames ts=%d\n", switch_channel_get_name(channel), frame->datalen, samples, frames, tech_pvt->timestamp_send);

	if (switch_rtp_write_frame(tech_pvt->rtp_session, frame, samples) < 0) {
		return SWITCH_STATUS_FALSE;
	}
	tech_pvt->timestamp_send += (int) samples;

	switch_clear_flag(tech_pvt, TFLAG_WRITING);
	//switch_mutex_unlock(tech_pvt->rtp_lock);
	return status;
}

static switch_status channel_answer_channel(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	

	//if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {

	//}
	return SWITCH_STATUS_SUCCESS;
}


static switch_status channel_receive_message(switch_core_session *session, switch_core_session_message *msg)
{
	switch_channel *channel;
	struct private_object *tech_pvt;
			
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		if (tech_pvt->rtp_session) {
			switch_rtp_clear_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "De-activate timed RTP!\n");
			//switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_TIMER_RECLOCK);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		if (tech_pvt->rtp_session) {
			switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Re-activate timed RTP!\n");
			//switch_rtp_clear_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_TIMER_RECLOCK);
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}
static const switch_state_handler_table channel_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_ring */ channel_on_ring,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_loopback */ channel_on_loopback,
	/*.on_transmit */ channel_on_transmit
};

static const switch_io_routines channel_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.answer_channel */ channel_answer_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.waitfor_read */ channel_waitfor_read,
	/*.waitfor_write */ channel_waitfor_write,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message*/ channel_receive_message
};

static const switch_endpoint_interface channel_endpoint_interface = {
	/*.interface_name */ "dingaling",
	/*.io_routines */ &channel_io_routines,
	/*.event_handlers */ &channel_event_handlers,
	/*.private */ NULL,
	/*.next */ NULL
};

static const switch_loadable_module_interface channel_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ &channel_endpoint_interface,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};


/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile,
											  switch_core_session **new_session, switch_memory_pool *pool)
{
	if ((*new_session = switch_core_session_request(&channel_endpoint_interface, pool)) != 0) {
		struct private_object *tech_pvt;
		switch_channel *channel;
		switch_caller_profile *caller_profile = NULL;
		struct mdl_profile *mdl_profile = NULL;
		ldl_session_t *dlsession = NULL;
		char *profile_name;
		char *callto;
		char idbuf[1024];
		char *full_id;
		char sess_id[11] = "";
		char workspace[1024];



		switch_copy_string(workspace, outbound_profile->destination_number, sizeof(workspace));
		profile_name = workspace;
		if ((callto = strchr(profile_name, '/'))) {
			*callto++ = '\0';
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Invalid URL!\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}
		

		if ((mdl_profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
			if (!ldl_handle_ready(mdl_profile->handle)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doh! we are not logged in yet!\n");
				switch_core_session_destroy(new_session);
				return SWITCH_STATUS_GENERR;
			}
			if (!(full_id = ldl_handle_probe(mdl_profile->handle, callto, idbuf, sizeof(idbuf)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unknown Recipient!\n");
				switch_core_session_destroy(new_session);
				return SWITCH_STATUS_GENERR;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unknown Profile!\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			tech_pvt->flags |= globals.flags;
			tech_pvt->flags |= mdl_profile->flags;
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
			tech_pvt->codec_index = -1;
			tech_pvt->local_port = switch_rtp_request_port();
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		if (outbound_profile) {
			char name[128];
			
			snprintf(name, sizeof(name), "DingaLing/%s-%04x", outbound_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		
		switch_stun_random_string(sess_id, 10, "0123456789");

		ldl_session_create(&dlsession, mdl_profile->handle, sess_id, full_id, mdl_profile->login);
		tech_pvt->profile = mdl_profile;
		ldl_session_set_private(dlsession, *new_session);
		tech_pvt->dlsession = dlsession;
		get_codecs(tech_pvt);
		//tech_pvt->desc_id = ldl_session_describe(dlsession, NULL, 0, LDL_DESCRIPTION_INITIATE);
		negotiate_thread_launch(*new_session);
		return SWITCH_STATUS_SUCCESS;

	}

	return SWITCH_STATUS_GENERR;

}

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	load_config();

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &channel_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static ldl_status handle_loop(ldl_handle_t *handle)
{
	if (!globals.running) {
		return LDL_STATUS_FALSE;
	}
	return LDL_STATUS_SUCCESS;
}
static void init_profile(struct mdl_profile *profile)
{
	if (profile &&
		profile->login &&
		profile->password &&
		profile->dialplan &&
		profile->message &&
		profile->ip &&
		profile->name &&
		profile->exten) {
		ldl_handle_t *handle;


		if (ldl_handle_init(&handle,
							profile->login,
							profile->password,
							profile->message,
							handle_loop,
							handle_signalling,
							handle_response,
							profile) == LDL_STATUS_SUCCESS) {
			profile->handle = handle;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Started Thread for %s@%s\n", profile->login, profile->dialplan);
			switch_core_hash_insert(globals.profile_hash, profile->name, profile);
			handle_thread_launch(handle);
		} 
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Invalid Profile\n");
	}
}


SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	if (globals.running) {
		int x = 0;
		globals.running = 0;
		while (globals.handles > 0) {
			switch_yield(100000);
			x++;
			if(x > 10) {
				break;
			}
		}
		ldl_global_destroy();
	}
	return SWITCH_STATUS_SUCCESS;
}


static switch_status load_config(void)
{
	switch_config cfg;
	char *var, *val;
	char *cf = "dingaling.conf";
	struct mdl_profile *profile = NULL;
	int lastcat = -1;

	memset(&globals, 0, sizeof(globals));
	globals.running = 1;

	switch_core_hash_init(&globals.profile_hash, module_pool);	
	if (!switch_config_open_file(&cfg, cf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "codec_prefs")) {
				set_global_codec_string(val);
				globals.codec_order_last =
					switch_separate_string(globals.codec_string, ',', globals.codec_order, SWITCH_MAX_CODECS);
			} else if (!strcmp(var, "codec_rates")) {
				set_global_codec_rates_string(val);
				globals.codec_rates_last =
					switch_separate_string(globals.codec_rates_string, ',', globals.codec_rates, SWITCH_MAX_CODECS);
			}

		} else if (!strcasecmp(cfg.category, "interface")) {
			if (!globals.init) {
				ldl_global_init(globals.debug);
				ldl_global_set_logger(dl_logger);
				globals.init = 1;
			}

			if (cfg.catno != lastcat) {
				if (profile) {
					init_profile(profile);
					profile = NULL;
				}
				lastcat = cfg.catno;
			}

			if(!profile) {
				profile = switch_core_alloc(module_pool, sizeof(*profile));
			}

			if (!strcmp(var, "login")) {
				profile->login = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "password")) {
				profile->password = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "dialplan")) {
				profile->dialplan = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "name")) {
				profile->name = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "message")) {
				profile->message = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "rtp-ip")) {
				profile->ip = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "ext-rtp-ip")) {
				profile->extip = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "lanaddr")) {
				profile->lanaddr = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "exten")) {
				profile->exten = switch_core_strdup(module_pool, val);
			} else if (!strcmp(var, "vad")) {
				if (!strcasecmp(val, "in")) {
					switch_set_flag(profile, TFLAG_VAD_IN);
				} else if (!strcasecmp(val, "out")) {
					switch_set_flag(profile, TFLAG_VAD_OUT);
				} else if (!strcasecmp(val, "both")) {
					switch_set_flag(profile, TFLAG_VAD_IN);
					switch_set_flag(profile, TFLAG_VAD_OUT);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invald option %s for VAD\n", val);
				}
			}
		}
	}

	if (profile) {
		init_profile(profile);
		profile = NULL;
	}

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	switch_config_close_file(&cfg);
	return SWITCH_STATUS_SUCCESS;
}



static ldl_status handle_signalling(ldl_handle_t *handle, ldl_session_t *dlsession, ldl_signal_t signal, char *msg)
{
	struct mdl_profile *profile = NULL;
	switch_core_session *session = NULL;
	switch_channel *channel = NULL;
    struct private_object *tech_pvt = NULL;

	assert(dlsession != NULL);
	assert(handle != NULL);

	if (!(profile = ldl_handle_get_private(handle))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR NO PROFILE!\n");
		return LDL_STATUS_FALSE;
	}

	if ((session = ldl_session_get_private(dlsession))) {
		tech_pvt = switch_core_session_get_private(session);
		assert(tech_pvt != NULL);

		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "using Existing session for %s\n", ldl_session_get_id(dlsession));

		if (switch_channel_get_state(channel) >= CS_HANGUP) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Call %s is already over\n", switch_channel_get_name(channel));
			return LDL_STATUS_FALSE;
		}

	} else {
		if (signal != LDL_SIGNAL_INITIATE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session is already dead\n");
			return LDL_STATUS_FALSE;
		}
		if ((session = switch_core_session_request(&channel_endpoint_interface, NULL)) != 0) {
			switch_core_session_add_stream(session, NULL);
			if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object))) != 0) {
				memset(tech_pvt, 0, sizeof(*tech_pvt));
				tech_pvt->flags |= globals.flags;
				tech_pvt->flags |= profile->flags;
				channel = switch_core_session_get_channel(session);
				switch_core_session_set_private(session, tech_pvt);
				tech_pvt->session = session;
				tech_pvt->codec_index = -1;
				tech_pvt->profile = profile;
				tech_pvt->local_port = switch_rtp_request_port();
				switch_set_flag(tech_pvt, TFLAG_ANSWER);
				
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Hey where is my memory pool?\n");
				switch_core_session_destroy(&session);
				return LDL_STATUS_FALSE;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Creating a session for %s\n", ldl_session_get_id(dlsession));
		
			if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																	  profile->dialplan,
																	  ldl_session_get_caller(dlsession),
																	  ldl_session_get_caller(dlsession),
																	  ldl_session_get_ip(dlsession),
																	  NULL,
																	  NULL,
																	  NULL,
																	  (char *)modname,
																	  profile->exten)) != 0) {
				char name[128];
				snprintf(name, sizeof(name), "DingaLing/%s-%04x", tech_pvt->caller_profile->destination_number,
						 rand() & 0xffff);
				switch_channel_set_name(channel, name);
				switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

			}
		
			ldl_session_set_private(dlsession, session);
			tech_pvt->dlsession = dlsession;
			negotiate_thread_launch(session);
		}
	}
	
	switch(signal) {
	case LDL_SIGNAL_NONE:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR\n");
		break;
	case LDL_SIGNAL_MSG:
		if (msg) { 
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "MSG [%s]\n", msg);
			if (*msg == '+') {
				switch_channel_queue_dtmf(channel, msg + 1);
			}
		}

		break;
	case LDL_SIGNAL_INITIATE:
		if (signal) {
			ldl_payload_t *payloads;
			unsigned int len = 0;

			if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
				if (!strcasecmp(msg, "accept")) {
					switch_set_flag(tech_pvt, TFLAG_ANSWER);
					do_candidates(tech_pvt, 0);
				}
			}

			if (tech_pvt->codec_index > -1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Already decided on a codec\n");
				break;
			}

			if (!tech_pvt->num_codecs) {
				get_codecs(tech_pvt);
				if (!tech_pvt->num_codecs) {
					return LDL_STATUS_FALSE;
				}
			}

			
			if (ldl_session_get_payloads(dlsession, &payloads, &len) == LDL_STATUS_SUCCESS) {
                unsigned int x, y;
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%u payloads\n", len);
				for(x = 0; x < len; x++) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Available Payload %s %u\n", payloads[x].name, payloads[x].id);
					for(y = 0; y < tech_pvt->num_codecs; y++) {
						int match = 0;
						char *name = tech_pvt->codecs[y]->iananame;

						if (!strncasecmp(name, "ilbc", 4)) {
							name = "ilbc";
						}
						if (tech_pvt->codecs[y]->ianacode > 96) {
							match = strcasecmp(name, payloads[x].name) ? 0 : 1;
						} else {
							match = (payloads[x].id == tech_pvt->codecs[y]->ianacode) ? 1 : 0;
						}
						
						if (match) {
							tech_pvt->codec_index = y;
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Choosing Payload index %u %s %u\n", y, payloads[x].name, payloads[x].id);
							tech_pvt->codec_name = tech_pvt->codecs[y]->iananame;
							tech_pvt->codec_num = tech_pvt->codecs[y]->ianacode;
							if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
								do_describe(tech_pvt, 0);
							}
							return LDL_STATUS_SUCCESS;
						}
					}
				}
			}
		}

		
		break;
	case LDL_SIGNAL_CANDIDATES:
		if (signal) {
			ldl_candidate_t *candidates;
			unsigned int len = 0;

			if (ldl_session_get_candidates(dlsession, &candidates, &len) == LDL_STATUS_SUCCESS) {
				unsigned int x;

				
				if (tech_pvt->remote_ip) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Already picked an IP [%s]\n", tech_pvt->remote_ip);
					break;
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%u candidates\n", len);
				for(x = 0; x < len; x++) {
					uint8_t lanaddr = strncasecmp(candidates[x].address, profile->lanaddr, strlen(profile->lanaddr)) ? 0 : 1;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "candidates %s:%d\n", candidates[x].address, candidates[x].port);
					
					if (!strcasecmp(candidates[x].protocol, "udp") && (!strcasecmp(candidates[x].type, "local") || !strcasecmp(candidates[x].type, "stun")) && 
						((profile->lanaddr && lanaddr) ||
						 (strncasecmp(candidates[x].address, "10.", 3) && 
						  strncasecmp(candidates[x].address, "192.168.", 8) &&
						  strncasecmp(candidates[x].address, "127.", 4) &&
						  strncasecmp(candidates[x].address, "1.", 2) &&
						  strncasecmp(candidates[x].address, "2.", 2) &&
						  strncasecmp(candidates[x].address, "172.16.", 7) &&
						  strncasecmp(candidates[x].address, "172.17.", 7) &&
						  strncasecmp(candidates[x].address, "172.18.", 7) &&
						  strncasecmp(candidates[x].address, "172.19.", 7) &&
						  strncasecmp(candidates[x].address, "172.2", 5) &&
						  strncasecmp(candidates[x].address, "172.30.", 7) &&
						  strncasecmp(candidates[x].address, "172.31.", 7)
						  ))) {
						ldl_payload_t payloads[5];

						memset(payloads, 0, sizeof(payloads));

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Acceptable Candidate %s:%d\n", candidates[x].address, candidates[x].port);
						if (lanaddr) {
							switch_set_flag(tech_pvt, TFLAG_LANADDR);
						}

						if (!tech_pvt->num_codecs) {
							get_codecs(tech_pvt);
							if (!tech_pvt->num_codecs) {
								return LDL_STATUS_FALSE;
							}
						}

						tech_pvt->remote_ip = switch_core_session_strdup(session, candidates[x].address);
						ldl_session_set_ip(dlsession, tech_pvt->remote_ip);
						tech_pvt->remote_port = candidates[x].port;
						tech_pvt->remote_user = switch_core_session_strdup(session, candidates[x].username);
						if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
							do_candidates(tech_pvt, 0);
						}
						switch_set_flag(tech_pvt, TFLAG_TRANSPORT);
						
						return LDL_STATUS_SUCCESS;
					}
				}
			}
		}
		break;
	case LDL_SIGNAL_ERROR:
	case LDL_SIGNAL_TERMINATE:
		if (channel) {
			switch_channel_state state = switch_channel_get_state(channel);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hungup %s %u %d\n", switch_channel_get_name(channel), state, CS_INIT);
			switch_set_flag(tech_pvt, TFLAG_BYE);
			switch_clear_flag(tech_pvt, TFLAG_IO);
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

			if (state <= CS_INIT && !switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroy unused Session\n");
				switch_core_session_destroy(&session);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "End Call\n");
			}

		}
		break;
	}

	return LDL_STATUS_SUCCESS;
}

static ldl_status handle_response(ldl_handle_t *handle, char *id)
{
	return LDL_STATUS_SUCCESS;
}

