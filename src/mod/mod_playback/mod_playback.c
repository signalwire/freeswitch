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
 * mod_playback.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


static const char modname[] = "mod_playback";


void playback_function(switch_core_session *session, char *data)
{
	switch_channel *channel;
	short buf[960];
	char dtmf[128];
	int interval = 0, samples = 0;
	size_t len = 0, ilen = 0;
	switch_frame write_frame;
	switch_timer timer;
	switch_core_thread_session thread_session;
	switch_codec codec;
	switch_memory_pool *pool = switch_core_session_get_pool(session);
	switch_file_handle fh;
	char *codec_name;
	int x;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_core_file_open(&fh,
							  data,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel);
		return;
	}

	switch_channel_answer(channel);

	write_frame.data = buf;
	write_frame.buflen = sizeof(buf);

    
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OPEN FILE %s %dkhz %d channels\n", data, fh.samplerate, fh.channels);
	
	interval = 20;
	samples = (fh.samplerate / 50) * fh.channels;
	len = samples * 2;
	
	codec_name = "L16";
	
	if (switch_core_codec_init(&codec,
							   codec_name,
							   fh.samplerate,
							   interval,
							   fh.channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL,
							   pool) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activated\n");
		write_frame.codec = &codec;
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activation Failed %s@%dhz %d\n", codec_name, fh.samplerate, interval);
		switch_core_file_close(&fh);
		switch_channel_hangup(channel);
		return;
	}

	if (switch_core_timer_init(&timer, "soft", interval, samples, pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer failed!\n");
		switch_core_codec_destroy(&codec);
		switch_core_file_close(&fh);
		switch_channel_hangup(channel);
		return;
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer success %d bytes per %d ms!\n", len, interval);

	/* start a thread to absorb incoming audio */
	switch_core_service_session(session, &thread_session);
	ilen = samples;
	while(switch_channel_get_state(channel) == CS_EXECUTE) {
		int done = 0;

		if (switch_channel_has_dtmf(channel)) {
			switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "DTMF [%s]\n", dtmf);
			
			switch (*dtmf) {
			case '*':
				done = 1;
				break;
			default:
				break;
			}
		}
		
		if (done) {
			break;
		}
		
		switch_core_file_read(&fh, buf, &ilen);

		if (ilen <= 0) {
			break;
		}
		
		write_frame.datalen = ilen * 2;
		write_frame.samples = ilen;
#ifdef SWAP_LINEAR
		switch_swap_linear(write_frame.data, (int)write_frame.datalen / 2);
#endif
		if (switch_core_session_write_frame(session, &write_frame, -1) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bad Write\n");
			break;
		}

		if ((x = switch_core_timer_next(&timer)) < 0) {
			break;
		}
	}
	
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "done playing file\n");
	switch_core_file_close(&fh);

	/* End the audio absorbing thread */
	switch_core_thread_session_end(&thread_session);

	switch_core_timer_destroy(&timer);

	switch_core_codec_destroy(&codec);

	switch_channel_hangup(channel);
}

static const switch_application_interface playback_application_interface = {
	/*.interface_name*/ "playback",
	/*.application_function*/ playback_function
};

static const switch_loadable_module_interface mod_playback_module_interface = {
	/*.module_name = */			modname,
	/*.endpoint_interface = */	NULL,
	/*.timer_interface = */		NULL,
	/*.dialplan_interface = */	NULL,
	/*.codec_interface = */		NULL,
	/*.application_interface*/	&playback_application_interface
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {
	
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &mod_playback_module_interface;


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* 'switch_module_runtime' will start up in a thread by itself just by having it exist 
   if it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
 */


//switch_status switch_module_runtime(void)




