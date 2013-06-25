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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * mod_fsk.c -- FSK data transfer
 *
 */
#include <switch.h>
#include "fsk_callerid.h"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fsk_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_fsk_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_fsk_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_fsk, mod_fsk_load, mod_fsk_shutdown, NULL);

switch_status_t my_write_sample(int16_t *buf, size_t buflen, void *user_data)
{
	switch_buffer_t *buffer = (switch_buffer_t *) user_data;

	switch_buffer_write(buffer, buf, buflen * 2);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t write_fsk_data(uint32_t rate, int32_t db, switch_buffer_t *buffer, switch_event_t *event, const char *prefix)
{
	fsk_modulator_t fsk_trans;
	fsk_data_state_t fsk_data = {0};
	uint8_t databuf[1024] = "";
	char time_str[9];
	struct tm tm;
	time_t now;
	switch_event_header_t *hp;
	switch_size_t plen = 0;

	memset(&fsk_trans, 0, sizeof(fsk_trans));
	
	time(&now);
	localtime_r(&now, &tm);
	strftime(time_str, sizeof(time_str), "%m%d%H%M", &tm);

	fsk_data_init(&fsk_data, databuf, sizeof(databuf));
	fsk_data_add_mdmf(&fsk_data, MDMF_DATETIME, (uint8_t *)time_str, strlen(time_str));

	if (prefix) {
		plen = strlen(prefix);
	}


	if (event) {
		for (hp = event->headers; hp; hp = hp->next) {
			char *packed;
			char *name = hp->name;

			if (plen && strncasecmp(name, prefix, plen)) {
				continue;
			}

			name += plen;

			if (zstr(name)) {
				continue;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Encoding [%s][%s]\n", hp->name, hp->value);
			
			if (!strcasecmp(name, "phone_num")) {
				fsk_data_add_mdmf(&fsk_data, MDMF_PHONE_NUM, (uint8_t *)hp->value, strlen(hp->value));
			} else if (!strcasecmp(name, "phone_name")) {
				fsk_data_add_mdmf(&fsk_data, MDMF_PHONE_NAME, (uint8_t *)hp->value, strlen(hp->value));
			} else {
				packed = switch_mprintf("%q:%q", name, hp->value);
				fsk_data_add_mdmf(&fsk_data, MDMF_NAME_VALUE, (uint8_t *)packed, strlen(packed));
				free(packed);
			}
		}
	}

	fsk_data_add_checksum(&fsk_data);

	fsk_modulator_init(&fsk_trans, FSK_BELL202, rate, &fsk_data, db, 180, 5, 300, my_write_sample, buffer);
	fsk_modulator_send_all((&fsk_trans));

	fsk_demod_destroy(&fsk_data);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_APP(fsk_send_function) {
	switch_event_t *event = NULL;
	switch_buffer_t *buffer;
	switch_slin_data_t sdata = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_frame_t *read_frame;
	switch_status_t status;
	

	if (data) {
		switch_ivr_sleep(session, 1000, SWITCH_TRUE, NULL);
		switch_core_session_send_dtmf_string(session, (const char *) data);
		switch_ivr_sleep(session, 1500, SWITCH_TRUE, NULL);
	}
	
	if (switch_core_session_set_codec_slin(session, &sdata) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
						  SWITCH_LOG_ERROR, "FAILURE\n");
		return;
	}

	switch_buffer_create_dynamic(&buffer, 1024, 2048, 0);

	switch_channel_get_variables(channel, &event);
	
	write_fsk_data(sdata.codec.implementation->actual_samples_per_second, -14, buffer, event, "fsk_");

	while(switch_channel_ready(channel)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if ((sdata.write_frame.datalen = switch_buffer_read(buffer, sdata.write_frame.data, 
															 sdata.codec.implementation->decoded_bytes_per_packet)) <= 0) {
			break;
		}

		
		if (sdata.write_frame.datalen < sdata.codec.implementation->decoded_bytes_per_packet) {
			memset((char *)sdata.write_frame.data + sdata.write_frame.datalen, 255, 
				   sdata.codec.implementation->decoded_bytes_per_packet - sdata.write_frame.datalen);
			sdata.write_frame.datalen = sdata.codec.implementation->decoded_bytes_per_packet;
		}
		sdata.write_frame.samples = sdata.write_frame.datalen / 2;
		switch_core_session_write_frame(sdata.session, &sdata.write_frame, SWITCH_IO_FLAG_NONE, 0);
	}

	switch_buffer_destroy(&buffer);
	switch_core_codec_destroy(&sdata.codec);
	switch_core_session_set_read_codec(session, NULL);
	
}

typedef struct {
	switch_core_session_t *session;
	fsk_data_state_t fsk_data;
	uint8_t fbuf[512];
	int skip;
} switch_fsk_detect_t;




static switch_bool_t fsk_detect_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_fsk_detect_t *pvt = (switch_fsk_detect_t *) user_data;
	//switch_frame_t *frame = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT: {
		switch_codec_implementation_t read_impl = { 0 };
		switch_core_session_get_read_impl(pvt->session, &read_impl);
		
		if (fsk_demod_init(&pvt->fsk_data, read_impl.actual_samples_per_second, pvt->fbuf, sizeof(pvt->fbuf))) {
			return SWITCH_FALSE;
		}
		
		break;
	}
	case SWITCH_ABC_TYPE_CLOSE:
		{
			fsk_demod_destroy(&pvt->fsk_data);
		}
		break;

	case SWITCH_ABC_TYPE_WRITE_REPLACE:
	case SWITCH_ABC_TYPE_READ_REPLACE:
		{
			switch_frame_t *rframe;

			if (type == SWITCH_ABC_TYPE_READ_REPLACE) {
				rframe = switch_core_media_bug_get_read_replace_frame(bug);
			} else {
				rframe = switch_core_media_bug_get_write_replace_frame(bug);
			}

			if (!pvt->skip && fsk_demod_feed(&pvt->fsk_data, rframe->data, rframe->datalen / 2) != SWITCH_STATUS_SUCCESS) {
				char str[1024] = "";
				size_t type, mlen;
				char *sp;
				switch_event_t *event;
				const char *app_var;
				int total = 0;

				switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA);
				
				while(fsk_data_parse(&pvt->fsk_data, &type, &sp, &mlen) == SWITCH_STATUS_SUCCESS) {
					char *varname = NULL, *val, *p;
					
					switch_copy_string(str, sp, mlen+1);
					*(str+mlen) = '\0';
					switch_clean_string(str);
					//printf("TYPE %u LEN %u VAL [%s]\n", (unsigned)type, (unsigned)mlen, str);

					val = str;

					switch(type) {
					case MDMF_DATETIME:
						varname = "fsk_datetime";
						break;
					case MDMF_PHONE_NAME:
						varname = "fsk_phone_name";
						break;
					case MDMF_PHONE_NUM:
						varname = "fsk_phone_num";
						break;
					case MDMF_NAME_VALUE:
						varname = switch_core_session_sprintf(pvt->session, "fsk_%s", val);
						if ((p = strchr(varname, ':'))) {
							*p++ = '\0';
							val = p;
						}
						break;
					default:
						break;
					}

					if (varname && val) {
						total++;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "%s setting FSK var [%s][%s]\n", 
										  switch_channel_get_name(channel), varname, val);
						switch_channel_set_variable(channel, varname, val);
						if (event) {
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, varname, val);
						}
					}
				}

				if (event) {
					if (switch_core_session_queue_event(pvt->session, &event) != SWITCH_STATUS_SUCCESS) {
						switch_event_destroy(&event);
					}
				}
				
				if (total && (app_var = switch_channel_get_variable(channel, "execute_on_fsk"))) {
					char *app_arg;

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "%s processing execute_on_fsk [%s]\n", 
									  switch_channel_get_name(channel), app_var);
					if ((app_arg = strchr(app_var, ' '))) {
						*app_arg++ = '\0';
					}
					switch_core_session_execute_application(pvt->session, app_var, app_arg);
				}
				
				pvt->skip = 10;
			}
			
			memset(rframe->data, 255, rframe->datalen);

			if (type == SWITCH_ABC_TYPE_READ_REPLACE) {
				switch_core_media_bug_set_read_replace_frame(bug, rframe);
			} else {
				switch_core_media_bug_set_write_replace_frame(bug, rframe);
			}

			if (pvt->skip && !--pvt->skip) {
				return SWITCH_FALSE;
			}

		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

switch_status_t stop_fsk_detect_session(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if ((bug = switch_channel_get_private(channel, "fsk"))) {
		switch_channel_set_private(channel, "fsk", NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

switch_status_t fsk_detect_session(switch_core_session_t *session, const char *flags)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_fsk_detect_t *pvt = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	int bflags = SMBF_READ_REPLACE;

	if (strchr(flags, 'w')) {
		bflags = SMBF_WRITE_REPLACE;
	}

	switch_core_session_get_read_impl(session, &read_impl);

	if (!(pvt = switch_core_session_alloc(session, sizeof(*pvt)))) {
		return SWITCH_STATUS_MEMERR;
	}

   	pvt->session = session;


	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = switch_core_media_bug_add(session, "fsk_detect", NULL,
                                            fsk_detect_callback, pvt, 0, bflags | SMBF_NO_PAUSE, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_channel_set_private(channel, "fsk", bug);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_APP(fsk_recv_function) 
{
	fsk_detect_session(session, data);
}

SWITCH_STANDARD_APP(fsk_display_function) 
{
	/* expected to be called via 'execute_on_fsk' -- passes display update over FSK */

	const char *cid_name, *cid_num;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_core_session_message_t *msg;
	switch_core_session_t *psession = NULL, *usession = NULL;
	char *flags = (char *) data;

	cid_name = switch_channel_get_variable(channel, "fsk_phone_name");
	cid_num = switch_channel_get_variable(channel, "fsk_phone_num");

	if (zstr(cid_name)) {
		cid_name = cid_num;
	}
	
	if (zstr(cid_num)) {
		return;
	}

	if (strchr(flags, 'b')) {
		if (switch_core_session_get_partner(session, &psession) == SWITCH_STATUS_SUCCESS) {
			usession = psession;
		}
	}

	if (!usession) {
		usession = session;
	}

	msg = switch_core_session_alloc(usession, sizeof(*msg));
	MESSAGE_STAMP_FFL(msg);
	msg->message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;
	msg->string_array_arg[0] = switch_core_session_strdup(usession, cid_name);
	msg->string_array_arg[1] = switch_core_session_strdup(usession, cid_num);
	msg->from = __FILE__;
	switch_core_session_queue_message(usession, msg);

	if (psession) {
		switch_core_session_rwunlock(psession);
		psession = NULL;
	}
}

SWITCH_STANDARD_APP(fsk_simplify_function) 
{
	/* expected to be called via 'execute_on_fsk' -- redirects call to point-to-point and eliminates legs in the middle */
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *sip_uri, *fsk_simplify_profile, *fsk_simplify_context;
	char *bridgeto;
	
	if (!(sip_uri = switch_channel_get_variable(channel, "fsk_uri"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s Missing URI field!\n", switch_channel_get_name(channel));
	}

	if (!(fsk_simplify_profile = switch_channel_get_variable(channel, "fsk_simplify_profile"))) {
		fsk_simplify_profile = "internal";
	}

	fsk_simplify_context = switch_channel_get_variable(channel, "fsk_simplify_context");

	if (!zstr(sip_uri)) {
		switch_core_session_t *psession;
		switch_channel_t *pchannel;

		bridgeto = switch_core_session_sprintf(session, "bridge:sofia/%s/sip:%s", fsk_simplify_profile, sip_uri);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s transfering to [%s]\n", 
						  switch_channel_get_name(channel), bridgeto);
		

		if (switch_core_session_get_partner(session, &psession) == SWITCH_STATUS_SUCCESS) {
			pchannel = switch_core_session_get_channel(psession);
			switch_channel_set_flag(pchannel, CF_REDIRECT);
			switch_channel_set_flag(pchannel, CF_TRANSFER);
		}
		
		switch_ivr_session_transfer(session, bridgeto, "inline", fsk_simplify_context);

		if (psession) {
			switch_ivr_session_transfer(psession, "sleep:5000", "inline", NULL);
			switch_core_session_rwunlock(psession);			
		}
	}
}

/* Macro expands to: switch_status_t mod_fsk_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_fsk_load)
{
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "fsk_send", "fsk_send", NULL, fsk_send_function, NULL, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "fsk_recv", "fsk_recv", NULL, fsk_recv_function, NULL, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "fsk_simplify", "fsk_simplify", NULL, fsk_simplify_function, NULL, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "fsk_display", "fsk_display", NULL, fsk_display_function, NULL, SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_fsk_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fsk_shutdown)
{
	/* Cleanup dynamically allocated config settings */

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
