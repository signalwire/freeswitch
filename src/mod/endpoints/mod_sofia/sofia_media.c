/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * sofia_media.c -- SOFIA SIP Endpoint (sofia media code)
 *
 */
#include "mod_sofia.h"




uint8_t sofia_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, switch_sdp_type_t type)
{
	uint8_t t = 0, p = 0;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!switch_channel_test_flag(channel, CF_REINVITE) || switch_core_media_validate_common_audio_sdp(session, r_sdp, type)) {
		if ((t = switch_core_media_negotiate_sdp(session, r_sdp, &p, type))) {
			sofia_set_flag_locked(tech_pvt, TFLAG_SDP);
		}
	}

	if (!p) {
		sofia_set_flag(tech_pvt, TFLAG_NOREPLY);
	}

	return t;
}

switch_status_t sofia_media_activate_rtp(private_object_t *tech_pvt)
{
	switch_status_t status;

	switch_mutex_lock(tech_pvt->sofia_mutex);
	status = switch_core_media_activate_rtp(tech_pvt->session);
	switch_mutex_unlock(tech_pvt->sofia_mutex);


	if (status == SWITCH_STATUS_SUCCESS) {
		sofia_set_flag(tech_pvt, TFLAG_RTP);
		sofia_set_flag(tech_pvt, TFLAG_IO);
	}

	return status;
}



switch_status_t sofia_media_tech_media(private_object_t *tech_pvt, const char *r_sdp, switch_sdp_type_t type)
{
	uint8_t match = 0;

	switch_assert(tech_pvt != NULL);
	switch_assert(r_sdp != NULL);

	if (zstr(r_sdp)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((match = sofia_media_negotiate_sdp(tech_pvt->session, r_sdp, type))) {
		if (switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "EARLY MEDIA");
		sofia_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
		switch_channel_mark_pre_answered(tech_pvt->channel);
		return SWITCH_STATUS_SUCCESS;
	}


	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE_NONSTD(switch_status_t) switch_string_stream_write(switch_stream_handle_t *handle, const char *fmt, ...)
{
	va_list ap;
	char *data = NULL;
	va_start(ap, fmt);
	//ret = switch_vasprintf(&data, fmt, ap);
	if (!(data = switch_vmprintf(fmt, ap))) {
		return SWITCH_STATUS_FALSE;
	}
	va_end(ap);
	return handle->raw_write_function(handle, (unsigned char*)data, strlen(data));
}


static void process_mp(switch_core_session_t *session, switch_stream_handle_t *stream, const char *boundary, const char *str, const char*isup, intptr_t isup_len, switch_bool_t drop_sdp) {
	char *dname = switch_core_session_strdup(session, str);
	char *dval;

	if ((dval = strchr(dname, ':'))) {
		*dval++ = '\0';
		
		if (drop_sdp && (strcasecmp(dname, "application/sdp") == 0)) {
			return;
		}

		if (strcasecmp(dname, "application/isup") == 0 && isup) {
			switch_string_stream_write(stream, "--%s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", boundary, dname, isup_len);
			stream->raw_write_function(stream, (unsigned char*)isup, isup_len);
			switch_string_stream_write(stream, "\r\n");
		} else if (*dval == '~') {
			switch_string_stream_write(stream, "--%s\r\nContent-Type: %s\r\nContent-Length: %d\r\n%s\r\n", boundary, dname, strlen(dval), dval + 1);
		} else {
			switch_string_stream_write(stream, "--%s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n%s\r\n", boundary, dname, strlen(dval) + 1, dval);
		}
	}
	
}

char *sofia_media_get_multipart(switch_core_session_t *session, const char *prefix, const char *sdp,  char **mp_type, switch_size_t * mp_data_len)
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hi = NULL;
	int x = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *boundary = switch_core_session_get_uuid(session);

	char * isup = (char*)switch_channel_get_private(channel, "_isup_payload");
	intptr_t isup_len = (intptr_t)switch_channel_get_private(channel, "_isup_payload_size");
	
	SWITCH_STANDARD_STREAM(stream);
	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			const char *name = (char *) hi->name;
			char *value = (char *) hi->value;

			if (!strcasecmp(name, prefix)) {
				if (hi->idx > 0) {
					int i = 0;

					for(i = 0; i < hi->idx; i++) {
						process_mp(session, &stream, boundary, hi->array[i], isup, isup_len, !!sdp);
						x++;
					}
				} else {
					process_mp(session, &stream, boundary, value, isup, isup_len, !!sdp);
					x++;
				}
			}
		}
		switch_channel_variable_last(channel);
	}

	if (x) {
		*mp_type = switch_core_session_sprintf(session, "multipart/mixed; boundary=%s", boundary);
		if (sdp) {
			switch_string_stream_write(&stream, "--%s\r\nContent-Type: application/sdp\r\nContent-Length: %d\r\n\r\n%s\r\n", boundary, strlen(sdp) + 1, sdp);
		}
		switch_string_stream_write(&stream, "--%s--\r\n", boundary);
	}

	if (!zstr((char *) stream.data)) {
		extra_headers = stream.data;
	} else {
		switch_safe_free(stream.data);
	}
	
	*mp_data_len = stream.data_len;
	return extra_headers;
}





/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

