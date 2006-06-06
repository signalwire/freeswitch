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
 * mod_rss.c -- RSS Browser
 *
 */
#include <switch.h>

static const char modname[] = "mod_rss";


/* helper object */
struct dtmf_buffer {
	char *data;
	char *front;
	uint32_t len;
	uint32_t size;
	switch_file_handle_t fh;
};

/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status_t on_dtmf(switch_core_session_t *session, char *dtmf, void *buf, unsigned int buflen)
{

	struct dtmf_buffer *dtb;
	uint32_t len, slen;
	dtb = (struct dtmf_buffer *) buf;
	uint32_t samps = 0, pos = 0;

	if (*dtmf == '#') {
		return SWITCH_STATUS_FALSE;
	}

	len = dtb->size - dtb->len;
	slen = strlen(dtmf);

	if (slen > len) {
		slen = len;
	}
	
	switch_copy_string(dtb->front, dtmf, len);
	dtb->front += slen;
	dtb->len += slen;

	if (dtb->len == 2) {
		if (*dtb->data == '*') {
			dtb->front = dtb->data;
			dtb->len = 0;
			*dtb->data = '\0';
			switch(*(dtb->data+1)) {
			case '0':
				dtb->fh.speed = 0;
				break;
			case '1':
				dtb->fh.speed++;
				break;
			case '2':
				dtb->fh.speed--;
				break;
			case '5':
				{
					switch_codec_t *codec = switch_core_session_get_read_codec(session);
					samps = 5000 * (codec->implementation->samples_per_second / 1000);
					switch_core_file_seek(&dtb->fh, &pos, samps, SEEK_CUR);
				}
				break;
			case '4':
				{
					int32_t lpos = 0;
					switch_codec_t *codec = switch_core_session_get_read_codec(session);
					
					samps = 5000 * (codec->implementation->samples_per_second / 1000);
					lpos = (int) dtb->fh.pos - samps;
					if (lpos < 0) {
						lpos = 0;
					}
					switch_core_file_seek(&dtb->fh, &pos, lpos, SEEK_SET);
				}
				break;
			case '*':
				if (switch_test_flag(&dtb->fh, SWITCH_FILE_PAUSE)) {
					switch_clear_flag(&dtb->fh, SWITCH_FILE_PAUSE);
				} else {
					switch_set_flag(&dtb->fh, SWITCH_FILE_PAUSE);
				}
				break;
			}
			
			return SWITCH_STATUS_SUCCESS;
		}
		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}


static void rss_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	uint8_t index = 0;
	char fname[512];
	switch_status_t status;
	char buf[10];
	struct dtmf_buffer dtb;


	dtb.data = buf;
	dtb.front = buf;
	dtb.len = 0;
	dtb.size = sizeof(buf);

    channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No Path Specified!\n");
		return;
	}

	switch_channel_answer(channel);
	
	while(switch_channel_ready(channel)) {
		snprintf(fname, sizeof(fname), "%s/%.2u.raw", data, index);
		memset(&dtb.fh, 0, sizeof(dtb.fh));
		if ((status = switch_ivr_play_file(session, &dtb.fh, fname, NULL, on_dtmf, &dtb, sizeof(dtb))) == SWITCH_STATUS_FALSE) {
			break;
		}

		index = atoi(buf);

		/* reset for next loop */
		*buf = '\0';
		dtb.front = buf;
		dtb.len = 0;
	}
	


}

static const switch_application_interface_t rss_application_interface = {
	/*.interface_name */ "rss",
	/*.application_function */ rss_function,
	NULL, NULL, NULL,
	/*.next*/ NULL
};


static switch_loadable_module_interface_t rss_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &rss_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};


SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &rss_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

