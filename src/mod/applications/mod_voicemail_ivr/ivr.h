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
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * ivr.h -- VoiceMail IVR Engine
 *
 */

#ifndef _IVRE_H_
#define _IVRE_H_

struct ivre_data {
	char dtmf_stored[128];
	int dtmf_received;
	char dtmf_accepted[128][16];
	int result;
	switch_bool_t audio_stopped;
	switch_bool_t recorded_audio;
	const char *potentialMatch;
	int potentialMatchCount;
	const char *completeMatch;
	char terminate_key;
	const char *record_tone;
};
typedef struct ivre_data ivre_data_t;

#define RES_WAITFORMORE 0
#define RES_FOUND 1
#define RES_INVALID 3
#define RES_TIMEOUT 4
#define RES_BREAK 5
#define RES_RECORD 6
#define RES_BUFFER_OVERFLOW 99

#define MAX_DTMF_SIZE_OPTION 32

switch_status_t ivre_init(ivre_data_t *loc, char **dtmf_accepted);
switch_status_t ivre_playback(switch_core_session_t *session, ivre_data_t *loc, const char *macro_name,  const char *data, switch_event_t *event, const char *lang, int timeout);
switch_status_t ivre_record(switch_core_session_t *session, ivre_data_t *loc, switch_event_t *event, const char *file_path, switch_file_handle_t *fh, int max_record_len, switch_size_t *record_len);

switch_status_t ivre_playback_dtmf_buffered(switch_core_session_t *session, const char *macro_name,  const char *data, switch_event_t *event, const char *lang, int timeout);
#endif
