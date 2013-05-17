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
 *
 * sofia_media.c -- SOFIA SIP Endpoint (sofia media code)
 *
 */
#include "mod_sofia.h"




uint8_t sofia_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, switch_sdp_type_t type)
{
	uint8_t t, p = 0;
	private_object_t *tech_pvt = switch_core_session_get_private(session);

	if ((t = switch_core_media_negotiate_sdp(session, r_sdp, &p, type))) {
		sofia_set_flag_locked(tech_pvt, TFLAG_SDP);
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



switch_status_t sofia_media_tech_media(private_object_t *tech_pvt, const char *r_sdp)
{
	uint8_t match = 0;

	switch_assert(tech_pvt != NULL);
	switch_assert(r_sdp != NULL);

	if (zstr(r_sdp)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((match = sofia_media_negotiate_sdp(tech_pvt->session, r_sdp, SDP_TYPE_REQUEST))) {
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


char *sofia_media_get_multipart(switch_core_session_t *session, const char *prefix, const char *sdp, char **mp_type)
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hi = NULL;
	int x = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *boundary = switch_core_session_get_uuid(session);

	SWITCH_STANDARD_STREAM(stream);
	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			const char *name = (char *) hi->name;
			char *value = (char *) hi->value;

			if (!strncasecmp(name, prefix, strlen(prefix))) {
				const char *hname = name + strlen(prefix);
				if (*value == '~') {
					stream.write_function(&stream, "--%s\nContent-Type: %s\nContent-Length: %d\n%s\n", boundary, hname, strlen(value), value + 1);
				} else {
					stream.write_function(&stream, "--%s\nContent-Type: %s\nContent-Length: %d\n\n%s\n", boundary, hname, strlen(value) + 1, value);
				}
				x++;
			}
		}
		switch_channel_variable_last(channel);
	}

	if (x) {
		*mp_type = switch_core_session_sprintf(session, "multipart/mixed; boundary=%s", boundary);
		if (sdp) {
			stream.write_function(&stream, "--%s\nContent-Type: application/sdp\nContent-Length: %d\n\n%s\n", boundary, strlen(sdp) + 1, sdp);
		}
		stream.write_function(&stream, "--%s--\n", boundary);
	}

	if (!zstr((char *) stream.data)) {
		extra_headers = stream.data;
	} else {
		switch_safe_free(stream.data);
	}

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

