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
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter AT 0xdecafbad dot com>
 * Luke Dashjr <luke@openmethods.com> (OpenMethods, LLC)
 *
 * switch_ivr.h -- IVR Library
 *
 */
/** 
 * @file switch_ivr.h
 * @brief IVR Library
 * @see switch_ivr
 */

#ifndef SWITCH_IVR_H
#define SWITCH_IVR_H

#include <switch.h>
#include "switch_json.h"

SWITCH_BEGIN_EXTERN_C struct switch_unicast_conninfo {
	switch_core_session_t *session;
	switch_codec_t read_codec;
	switch_frame_t write_frame;
	switch_byte_t write_frame_data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_socket_t *socket;
	char *local_ip;
	switch_port_t local_port;
	char *remote_ip;
	switch_port_t remote_port;
	switch_sockaddr_t *local_addr;
	switch_sockaddr_t *remote_addr;
	switch_mutex_t *flag_mutex;
	int32_t flags;
	int type;
	int transport;
	int stream_id;
};
typedef struct switch_unicast_conninfo switch_unicast_conninfo_t;

#define SWITCH_IVR_VERIFY_SILENCE_DIVISOR(divisor) \
	{ \
		if ((divisor) <= 0 && (divisor) != -1) { \
			divisor = 400; \
		} \
	}

/**
 * @defgroup switch_ivr IVR Library
 * @ingroup core1
 *	A group of core functions to do IVR related functions designed to be 
 *	building blocks for a higher level IVR interface.
 * @{
 */

SWITCH_DECLARE(switch_status_t) switch_ivr_deactivate_unicast(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_ivr_activate_unicast(switch_core_session_t *session,
															char *local_ip,
															switch_port_t local_port,
															char *remote_ip, switch_port_t remote_port, char *transport, char *flags);
/*!
  \brief Generate an JSON CDR report.
  \param session the session to get the data from.
  \param json_cdr pointer to the json object
  \return SWITCH_STATUS_SUCCESS if successful
  \note on success the json object must be freed
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_generate_json_cdr(switch_core_session_t *session, cJSON **json_cdr, switch_bool_t urlencode);

/*!
  \brief Generate an XML CDR report.
  \param session the session to get the data from.
  \param xml_cdr pointer to the xml_record
  \return SWITCH_STATUS_SUCCESS if successful
  \note on success the xml object must be freed
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_generate_xml_cdr(switch_core_session_t *session, switch_xml_t *xml_cdr);
SWITCH_DECLARE(int) switch_ivr_set_xml_profile_data(switch_xml_t xml, switch_caller_profile_t *caller_profile, int off);
SWITCH_DECLARE(int) switch_ivr_set_xml_chan_vars(switch_xml_t xml, switch_channel_t *channel, int off);

/*!
  \brief Parse command from an event
  \param session the session on which to parse the event
  \param event the event to parse
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_parse_event(_In_ switch_core_session_t *session, _In_ switch_event_t *event);

/*!
  \brief Parse all commands from an event
  \param session the session on which to parse the events
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_events(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_ivr_parse_next_event(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_messages(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_signal_data(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_ivr_process_indications(switch_core_session_t *session, switch_core_session_message_t *message);

/*!
  \brief Wait for time to pass for a specified number of milliseconds
  \param session the session to wait for.
  \param ms the number of milliseconds
  \param sync synchronize the channel's audio before waiting
  \param args arguements to pass for callbacks etc
  \return SWITCH_STATUS_SUCCESS if the channel is still up
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_sleep(switch_core_session_t *session, uint32_t ms, switch_bool_t sync, switch_input_args_t *args);

SWITCH_DECLARE(switch_status_t) switch_ivr_park(switch_core_session_t *session, switch_input_args_t *args);

/*!
  \brief Wait for DTMF digits calling a pluggable callback function when digits are collected.
  \param session the session to read.
  \param args arguements to pass for callbacks etc
  \param timeout a timeout in milliseconds
  \return SWITCH_STATUS_SUCCESS to keep the collection moving.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_callback(switch_core_session_t *session, switch_input_args_t *args, uint32_t digit_timeout,
																   uint32_t abs_timeout);

/*!
  \brief Wait for specified number of DTMF digits, untile terminator is received or until the channel hangs up.
  \param session the session to read.
  \param buf strig to write to
  \param buflen max size of buf
  \param maxdigits max number of digits to read
  \param terminators digits to end the collection
  \param terminator actual digit that caused the collection to end (if any)
  \param first_timeout timeout in ms
  \param digit_timeout digit timeout in ms
  \param abs_timeout abs timeout in ms
  \return SWITCH_STATUS_SUCCESS to keep the collection moving.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_count(switch_core_session_t *session,
																char *buf,
																switch_size_t buflen,
																switch_size_t maxdigits,
																const char *terminators, char *terminator,
																uint32_t first_timeout, uint32_t digit_timeout, uint32_t abs_timeout);

/*!
  \brief play a file to the session while doing speech recognition.
  \param session the session to play and detect on
  \param file the path to the file
  \param mod_name the module name of the ASR library
  \param grammar the grammar text, URI, or local file name
  \param result of speech recognition, allocated from the session pool
  \param input_timeout time to wait for input
  \param args arguements to pass for callbacks etc
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_play_and_detect_speech(switch_core_session_t *session, 
																  const char *file, 
																  const char *mod_name,
																  const char *grammar,
																  char **result,
																  uint32_t input_timeout,
																  switch_input_args_t *args);


/*!
  \brief Engage background Speech detection on a session
  \param session the session to attach
  \param mod_name the module name of the ASR library
  \param grammar the grammar text, URI, or local file name
  \param name the grammar name
  \param dest the destination address
  \param ah an ASR handle to use (NULL to create one)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech(switch_core_session_t *session,
														 const char *mod_name,
														 const char *grammar, const char *name, const char *dest, switch_asr_handle_t *ah);

/*!
  \brief Stop background Speech detection on a session
  \param session The session to stop detection on
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_detect_speech(switch_core_session_t *session);

/*!
  \brief Pause background Speech detection on a session
  \param session The session to pause detection on
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_pause_detect_speech(switch_core_session_t *session);

/*!
  \brief Resume background Speech detection on a session
  \param session The session to resume detection on
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_resume_detect_speech(switch_core_session_t *session);

/*!
  \brief Load a grammar on a background speech detection handle
  \param session The session to change the grammar on
  \param grammar the grammar text, URI, or local file name
  \param name the grammar name
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_load_grammar(switch_core_session_t *session, char *grammar, char *name);

/*!
  \brief Unload a grammar on a background speech detection handle
  \param session The session to change the grammar on
  \param name the grammar name
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_unload_grammar(switch_core_session_t *session, const char *name);

/*!
  \brief Enable a grammar on a background speech detection handle
  \param session The session to change the grammar on
  \param name the grammar name
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_enable_grammar(switch_core_session_t *session, const char *name);

/*!
  \brief Disable a grammar on a background speech detection handle
  \param session The session to change the grammar on
  \param name the grammar name
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_disable_grammar(switch_core_session_t *session, const char *name);

/*!
  \brief Disable all grammars on a background speech detection handle
  \param session The session to change the grammar on
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_disable_all_grammars(switch_core_session_t *session);

SWITCH_DECLARE(switch_status_t) switch_ivr_set_param_detect_speech(switch_core_session_t *session, const char *name, const char *val);

/*!
  \brief Start input timers on a background speech detection handle
  \param session The session to start the timers on
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_start_input_timers(switch_core_session_t *session);

/*!
  \brief Record a session to disk
  \param session the session to record
  \param file the path to the file
  \param limit stop recording after this amount of time (in ms, 0 = never stop)
  \param fh file handle to use (NULL for builtin one)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_record_session(switch_core_session_t *session, char *file, uint32_t limit, switch_file_handle_t *fh);
SWITCH_DECLARE(switch_status_t) switch_ivr_transfer_recordings(switch_core_session_t *orig_session, switch_core_session_t *new_session);


SWITCH_DECLARE(switch_status_t) switch_ivr_eavesdrop_pop_eavesdropper(switch_core_session_t *session, switch_core_session_t **sessionp);
SWITCH_DECLARE(switch_status_t) switch_ivr_eavesdrop_exec_all(switch_core_session_t *session, const char *app, const char *arg);
SWITCH_DECLARE(switch_status_t) switch_ivr_eavesdrop_update_display(switch_core_session_t *session, const char *name, const char *number);

/*!
  \brief Eavesdrop on a another session
  \param session our session
  \param uuid the uuid of the session to spy on
  \param require_group group name to use to limit by group
  \param flags tweak read-mux, write-mux and dtmf
  \return SWITCH_STATUS_SUCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_eavesdrop_session(switch_core_session_t *session,
															 const char *uuid, const char *require_group, switch_eavesdrop_flag_t flags);

/*!
  \brief displace the media for a session with the audio from a file
  \param session the session to displace
  \param file filename
  \param limit time limit in ms
  \param flags m (mux) l (loop) or r(read session instead of write session)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_displace_session(switch_core_session_t *session, const char *file, uint32_t limit, const char *flags);

/*!
  \brief Stop displacing a session
  \param session the session
  \param file file name from the switch_ivr_displace_session call
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_displace_session(switch_core_session_t *session, const char *file);

/*!
  \brief Stop Recording a session
  \param session the session to stop recording
  \param file the path to the file
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_record_session(switch_core_session_t *session, const char *file);


SWITCH_DECLARE(switch_status_t) switch_ivr_session_audio(switch_core_session_t *session, const char *cmd, const char *direction, int level);
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_session_audio(switch_core_session_t *session);

/*!
  \brief Start looking for DTMF inband
  \param session the session to start looking
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_inband_dtmf_session(switch_core_session_t *session);

/*!
  \brief Stop looking for DTMF inband
  \param session the session to stop looking
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_inband_dtmf_session(switch_core_session_t *session);


/*!
  \brief Start generating DTMF inband
  \param session the session to generate on
  \param read_stream true to use the session we are reading from, false for the session we are writing to.
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_inband_dtmf_generate_session(switch_core_session_t *session, switch_bool_t read_stream);

/*!
  \brief Stop generating DTMF inband
  \param session the session to stop generating
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_inband_dtmf_generate_session(switch_core_session_t *session);

/*!
  \brief - NEEDDESC -
  \param session the session to act on
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_session_echo(switch_core_session_t *session, switch_input_args_t *args);

/*!
  \brief Stop looking for TONES
  \param session the session to stop looking
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_tone_detect_session(switch_core_session_t *session);

/*!
  \brief Start looking for TONES
  \param session the session to start looking
  \param key the name of the tone.
  \param tone_spec comma sep list of tone freqs
  \param flags one or both of 'r' and 'w'
  \param timeout timeout
  \param app optional application to execute when tone is found
  \param data optional data for appliaction
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_tone_detect_session(switch_core_session_t *session,
															   const char *key, const char *tone_spec,
															   const char *flags, time_t timeout, int hits,
															   const char *app, const char *data, switch_tone_detect_callback_t callback);




/*!
  \brief play a file from the disk to the session
  \param session the session to play the file too
  \param fh file handle to use (NULL for builtin one)
  \param file the path to the file
  \param args arguements to pass for callbacks etc
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_play_file(switch_core_session_t *session, switch_file_handle_t *fh, const char *file,
													 switch_input_args_t *args);

SWITCH_DECLARE(switch_status_t) switch_ivr_wait_for_silence(switch_core_session_t *session, uint32_t thresh, uint32_t silence_hits,
															uint32_t listen_hits, uint32_t timeout_ms, const char *file);

SWITCH_DECLARE(switch_status_t) switch_ivr_gentones(switch_core_session_t *session, const char *script, int32_t loops, switch_input_args_t *args);

/*!
  \brief record a file from the session to a file
  \param session the session to record from
  \param fh file handle to use
  \param file the path to the file
  \param args arguements to pass for callbacks etc
  \param limit max limit to record for (0 for infinite)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_record_file(_In_ switch_core_session_t *session,
													   _In_ switch_file_handle_t *fh,
													   _In_z_ const char *file, _In_opt_ switch_input_args_t *args, _In_ uint32_t limit);


/*!
  \brief Play a sound and gather digits with the number of retries specified if the user doesn't give digits in the set time
  \param session the current session to play sound to and collect digits
  \param min_digits the fewest digits allowed for the response to be valid
  \param max_digits the max number of digits to accept
  \param max_tries number of times to replay the sound and capture digits
  \param timeout time to wait for input (this is per iteration, so total possible time = max_tries * (timeout + audio playback length)
  \param valid_terminators for input that can include # or * (useful for variable length prompts)
  \param audio_file file to play
  \param bad_input_audio_file file to play if the input from the user was invalid
  \param var_name variable name to put results in
  \param digit_buffer variable digits captured will be put back into (empty if capture failed)
  \param digit_buffer_length length of the buffer for digits (should be the same or larger than max_digits)
  \param digits_regex the qualifying regex
  \return switch status, used to note status of channel (will still return success if digit capture failed)
  \note to test for digit capture failure look for \\0 in the first position of the buffer
*/
SWITCH_DECLARE(switch_status_t) switch_play_and_get_digits(switch_core_session_t *session,
														   uint32_t min_digits,
														   uint32_t max_digits,
														   uint32_t max_tries,
														   uint32_t timeout,
														   const char *valid_terminators,
														   const char *audio_file,
														   const char *bad_input_audio_file,
														   const char *var_name, char *digit_buffer, uint32_t digit_buffer_length,
														   const char *digits_regex,
														   uint32_t digit_timeout,
														   const char *transfer_on_failure);

SWITCH_DECLARE(switch_status_t) switch_ivr_speak_text_handle(switch_core_session_t *session,
															 switch_speech_handle_t *sh,
															 switch_codec_t *codec, switch_timer_t *timer, char *text, switch_input_args_t *args);
SWITCH_DECLARE(void) switch_ivr_clear_speech_cache(switch_core_session_t *session);
/*!
  \brief Speak given text with given tts engine
  \param session the session to speak on
  \param tts_name the desired tts module
  \param voice_name the desired voice
  \param text the text to speak
  \param args arguements to pass for callbacks etc
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_speak_text(switch_core_session_t *session,
													  const char *tts_name, const char *voice_name, char *text, switch_input_args_t *args);

/*!
  \brief Make an outgoing call
  \param session originating session
  \param bleg B leg session
  \param cause a pointer to hold call cause
  \param bridgeto the desired remote callstring
  \param timelimit_sec timeout in seconds for outgoing call
  \param table optional state handler table to install on the channel
  \param cid_name_override override the caller id name
  \param cid_num_override override the caller id number
  \param caller_profile_override override the entire calling caller profile
  \param ovars variables to be set on the outgoing channel
  \param flags flags to pass
  \return SWITCH_STATUS_SUCCESS if bleg is a running session.
  \note bleg will be read locked which must be unlocked with switch_core_session_rwunlock() before losing scope
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_originate(switch_core_session_t *session,
													 switch_core_session_t **bleg,
													 switch_call_cause_t *cause,
													 const char *bridgeto,
													 uint32_t timelimit_sec,
													 const switch_state_handler_table_t *table,
													 const char *cid_name_override,
													 const char *cid_num_override,
													 switch_caller_profile_t *caller_profile_override,
													 switch_event_t *ovars, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);

SWITCH_DECLARE(switch_status_t) switch_ivr_enterprise_originate(switch_core_session_t *session,
																switch_core_session_t **bleg,
																switch_call_cause_t *cause,
																const char *bridgeto,
																uint32_t timelimit_sec,
																const switch_state_handler_table_t *table,
																const char *cid_name_override,
																const char *cid_num_override,
																switch_caller_profile_t *caller_profile_override,
																switch_event_t *ovars, switch_originate_flag_t flags,
																switch_call_cause_t *cancel_cause);

SWITCH_DECLARE(void) switch_ivr_bridge_display(switch_core_session_t *session, switch_core_session_t *peer_session);

/*!
  \brief Bridge Audio from one session to another
  \param session one session
  \param peer_session the other session
  \param dtmf_callback a callback for messages and dtmf
  \param session_data data to pass to the DTMF callback for session
  \param peer_session_data data to pass to the DTMF callback for peer_session
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_multi_threaded_bridge(_In_ switch_core_session_t *session,
																 _In_ switch_core_session_t *peer_session,
																 switch_input_callback_function_t dtmf_callback, void *session_data,
																 void *peer_session_data);

/*!
  \brief Bridge Signalling from one session to another
  \param session one session
  \param peer_session the other session
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_signal_bridge(switch_core_session_t *session, switch_core_session_t *peer_session);

/*!
  \brief Transfer an existing session to another location
  \param session the session to transfer
  \param extension the new extension
  \param dialplan the new dialplan (OPTIONAL, may be NULL)
  \param context the new context (OPTIONAL, may be NULL)
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_session_transfer(_In_ switch_core_session_t *session, const char *extension, const char *dialplan,
															const char *context);

/*!
  \brief Transfer an existing session to another location in the future
  \param runtime the time (int epoch seconds) to transfer the call
  \param uuid the uuid of the session to transfer
  \param extension the new extension
  \param dialplan the new dialplan (OPTIONAL, may be NULL)
  \param context the new context (OPTIONAL, may be NULL)
  \return the id of the task
*/
SWITCH_DECLARE(uint32_t) switch_ivr_schedule_transfer(time_t runtime, const char *uuid, char *extension, char *dialplan, char *context);


/*!
  \brief Hangup an existing session in the future
  \param runtime the time (int epoch seconds) to transfer the call
  \param uuid the uuid of the session to hangup
  \param cause the hanup cause code
  \param bleg hangup up the B-Leg if possible
  \return the id of the task
*/
SWITCH_DECLARE(uint32_t) switch_ivr_schedule_hangup(time_t runtime, const char *uuid, switch_call_cause_t cause, switch_bool_t bleg);

/*!
  \brief Bridge two existing sessions
  \param originator_uuid the uuid of the originator
  \param originatee_uuid the uuid of the originator
  \remark Any custom state handlers on both channels will be deleted
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_uuid_bridge(const char *originator_uuid, const char *originatee_uuid);

/*!
  \brief Signal a session to request direct media access to it's remote end
  \param uuid the uuid of the session to request
  \param flags flags to influence behaviour (SMF_REBRIDGE to rebridge the call in media mode)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_media(const char *uuid, switch_media_flag_t flags);

/*!
  \brief Signal a session to request indirect media allowing it to exchange media directly with another device
  \param uuid the uuid of the session to request
  \param flags flags to influence behaviour (SMF_REBRIDGE to rebridge the call in no_media mode)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_nomedia(const char *uuid, switch_media_flag_t flags);

/*!
  \brief Signal the session with a protocol specific hold message.
  \param uuid the uuid of the session to hold
  \param message optional message
  \param moh play music-on-hold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_hold_uuid(const char *uuid, const char *message, switch_bool_t moh);

/*!
  \brief Toggles channel hold state of session
  \param uuid the uuid of the session to hold
  \param message optional message
  \param moh play music-on-hold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_hold_toggle_uuid(const char *uuid, const char *message, switch_bool_t moh);

/*!
  \brief Signal the session with a protocol specific unhold message.
  \param uuid the uuid of the session to hold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_unhold_uuid(const char *uuid);

/*!
  \brief Signal the session with a protocol specific hold message.
  \param session the session to hold
  \param message optional message
  \param moh play music-on-hold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_hold(switch_core_session_t *session, const char *message, switch_bool_t moh);

/*!
  \brief Signal the session with a protocol specific unhold message.
  \param session the session to unhold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_unhold(switch_core_session_t *session);

/*!
  \brief Signal the session to broadcast audio in the future
  \param runtime when (in epoch time) to run the broadcast
  \param uuid the uuid of the session to broadcast on
  \param path the path data of the broadcast "/path/to/file.wav [<timer name>]" or "speak:<engine>|<voice>|<Text to say>"
  \param flags flags to send to the request (SMF_ECHO_BRIDGED to send the broadcast to both sides of the call)
  \return the id of the task
*/
SWITCH_DECLARE(uint32_t) switch_ivr_schedule_broadcast(time_t runtime, const char *uuid, const char *path, switch_media_flag_t flags);

/*!
  \brief Signal the session to broadcast audio
  \param uuid the uuid of the session to broadcast on
  \param path the path data of the broadcast "/path/to/file.wav [<timer name>]" or "speak:<engine>|<voice>|<Text to say>"
  \param flags flags to send to the request (SMF_ECHO_BRIDGED to send the broadcast to both sides of the call)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_broadcast(const char *uuid, const char *path, switch_media_flag_t flags);
SWITCH_DECLARE(void) switch_ivr_broadcast_in_thread(switch_core_session_t *session, const char *app, int flags);

/*!
  \brief Transfer variables from one session to another 
  \param sessa the original session
  \param sessb the new session
  \param var the name of the variable to transfer (NULL for all)
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_transfer_variable(switch_core_session_t *sessa, switch_core_session_t *sessb, char *var);


/******************************************************************************************************/

	 struct switch_ivr_digit_stream_parser;
	 typedef struct switch_ivr_digit_stream_parser switch_ivr_digit_stream_parser_t;
	 struct switch_ivr_digit_stream;
	 typedef struct switch_ivr_digit_stream switch_ivr_digit_stream_t;
/*!
  \brief Create a digit stream parser object
  \param pool the pool to use for the new hash
  \param parser a pointer to the object pointer
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_new(switch_memory_pool_t *pool, switch_ivr_digit_stream_parser_t ** parser);

/*!
  \brief Destroy a digit stream parser object
  \param parser a pointer to the parser object
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_destroy(switch_ivr_digit_stream_parser_t *parser);

/*!
  \brief Create a new digit stream object
  \param parser a pointer to the parser object created by switch_ivr_digit_stream_parser_new
  \param stream a pointer to the stream object pointer
  \return NULL if no match found or consumer data that was associated with a given digit string when matched
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_new(switch_ivr_digit_stream_parser_t *parser, switch_ivr_digit_stream_t ** stream);

/*!
  \brief Destroys a digit stream object
  \param stream a pointer to the stream object
  \return NULL if no match found or consumer data that was associated with a given digit string when matched
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_destroy(switch_ivr_digit_stream_t ** stream);

/*!
  \brief Set a digit string to action mapping
  \param parser a pointer to the parser object created by switch_ivr_digit_stream_parser_new
  \param digits a string of digits to associate with an action
  \param data consumer data attached to this digit string
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_set_event(switch_ivr_digit_stream_parser_t *parser, char *digits, void *data);

/*!
  \brief Delete a string to action mapping
  \param parser a pointer to the parser object created by switch_ivr_digit_stream_parser_new
  \param digits the digit string to be removed from the map
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_del_event(switch_ivr_digit_stream_parser_t *parser, char *digits);

/*!
  \brief Feed digits collected into the stream for event match testing
  \param parser a pointer to the parser object created by switch_ivr_digit_stream_parser_new
  \param stream a stream to write data to
  \param digit a digit to collect and test against the map of digit strings
  \return NULL if no match found or consumer data that was associated with a given digit string when matched
*/
SWITCH_DECLARE(void *) switch_ivr_digit_stream_parser_feed(switch_ivr_digit_stream_parser_t *parser, switch_ivr_digit_stream_t *stream, char digit);

/*!
  \brief Reset the collected digit stream to nothing
  \param stream a pointer to the parser stream object created by switch_ivr_digit_stream_new
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_reset(switch_ivr_digit_stream_t *stream);

/*!
  \brief Set a digit string terminator
  \param parser a pointer to the parser object created by switch_ivr_digit_stream_parser_new
  \param digit the terminator digit
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_set_terminator(switch_ivr_digit_stream_parser_t *parser, char digit);


/******************************************************************************************************/


/** @} */

/**
 * @defgroup switch_ivr_menu IVR Menu Library
 * @ingroup switch_ivr
 *	IVR menu functions
 *	
 * @{
 */

	 typedef enum {
		 SWITCH_IVR_MENU_FLAG_FALLTOMAIN = (1 << 0),
		 SWITCH_IVR_MENU_FLAG_FREEPOOL = (1 << 1),
		 SWITCH_IVR_MENU_FLAG_STACK = (1 << 2)
	 } switch_ivr_menu_flags;
/* Actions are either set in switch_ivr_menu_bind_function or returned by a callback */
	 typedef enum {
		 SWITCH_IVR_ACTION_DIE,	/* Exit the menu.                  */
		 SWITCH_IVR_ACTION_EXECMENU,	/* Goto another menu in the stack. */
		 SWITCH_IVR_ACTION_EXECAPP,	/* Execute an application.         */
		 SWITCH_IVR_ACTION_PLAYSOUND,	/* Play a sound.                   */
		 SWITCH_IVR_ACTION_BACK,	/* Go back 1 menu.                 */
		 SWITCH_IVR_ACTION_TOMAIN,	/* Go back to the top level menu.  */
		 SWITCH_IVR_ACTION_NOOP	/* No operation                    */
	 } switch_ivr_action_t;
	 struct switch_ivr_menu;
	 typedef switch_ivr_action_t switch_ivr_menu_action_function_t(struct switch_ivr_menu *, char *, char *, size_t, void *);
	 typedef struct switch_ivr_menu switch_ivr_menu_t;
	 typedef struct switch_ivr_menu_action switch_ivr_menu_action_t;
/******************************************************************************************************/

/*!
 *\brief Create a new menu object.
 *\param new_menu the pointer to the new menu
 *\param main The top level menu, (NULL if this is the top level one).
 *\param name A pointer to the name of this menu.
 *\param greeting_sound Optional pointer to a main sound (press 1 for this 2 for that).
 *\param short_greeting_sound Optional pointer to a shorter main sound for subsequent loops.
 *\param invalid_sound Optional pointer to a sound to play after invalid input.
 *\param exit_sound Optional pointer to a sound to play upon exiting the menu.
 *\param confirm_macro phrase macro name to confirm input
 *\param confirm_key the dtmf key required for positive confirmation
 *\param tts_engine the tts engine to use for this menu
 *\param tts_voice the tts voice to use for this menu
 *\param confirm_attempts number of times to prompt to confirm input before failure
 *\param inter_timeout inter-digit timeout
 *\param digit_len max number of digits
 *\param timeout A number of milliseconds to pause before looping.
 *\param max_failures Maximum number of failures to withstand before hangingup This resets everytime you enter the menu.
 *\param pool memory pool (NULL to create one).
 *\return SWITCH_STATUS_SUCCESS if the menu was created.
 */

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_init(switch_ivr_menu_t ** new_menu,
													 switch_ivr_menu_t *main,
													 const char *name,
													 const char *greeting_sound,
													 const char *short_greeting_sound,
													 const char *invalid_sound,
													 const char *exit_sound,
													 const char *confirm_macro,
													 const char *confirm_key,
													 const char *tts_engine,
													 const char *tts_voice,
													 int confirm_attempts,
													 int inter_timeout, int digit_len, int timeout, int max_failures,
													 int max_timeouts, switch_memory_pool_t *pool);

/*!
 *\brief switch_ivr_menu_bind_action: Bind a keystroke to an action.
 *\param menu The menu obj you wish to bind to.
 *\param ivr_action switch_ivr_action_t enum of what you want to do.
 *\param arg Optional (sometimes necessary) string arguement.
 *\param bind KeyStrokes to bind the action to.
 *\return SWUTCH_STATUS_SUCCESS if the action was binded
 */
SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_action(switch_ivr_menu_t *menu, switch_ivr_action_t ivr_action, const char *arg, const char *bind);


/*!
 *\brief Bind a keystroke to a callback function.
 *\param menu The menu obj you wish to bind to.
 *\param function The function to call [int proto(struct switch_ivr_menu *, char *, size_t, void *)]
 *\param arg Optional (sometimes necessary) string arguement.
 *\param bind KeyStrokes to bind the action to.
 *\note The function is passed a buffer to fill in with any required argument data.
 *\note The function is also passed an optional void pointer to an object set upon menu execution. (think threads)
 *\note The function returns an switch_ivr_action_t enum of what you want to do. and looks to your buffer for args.
 *\return SWUTCH_STATUS_SUCCESS if the function was binded
 */
SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_function(switch_ivr_menu_t *menu,
															  switch_ivr_menu_action_function_t *function, const char *arg, const char *bind);


/*!
 *\brief Execute a menu.
 *\param session The session running the menu.
 *\param stack The top-level menu object (the first one you created.)
 *\param name A pointer to the name of the menu.
 *\param obj A void pointer to an object you want to make avaliable to your callback functions that you may have binded with switch_ivr_menu_bind_function.
 *\return SWITCH_STATUS_SUCCESS if all is well
 */
SWITCH_DECLARE(switch_status_t) switch_ivr_menu_execute(switch_core_session_t *session, switch_ivr_menu_t *stack, char *name, void *obj);

/*!
 *\brief free a stack of menu objects.
 *\param stack The top level menu you wish to destroy.
 *\return SWITCH_STATUS_SUCCESS if the object was a top level menu and it was freed
 */
SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_free(switch_ivr_menu_t *stack);

	 struct switch_ivr_menu_xml_ctx;
	 typedef struct switch_ivr_menu_xml_ctx switch_ivr_menu_xml_ctx_t;
/*!
 *\brief Build a menu stack from an xml source
 *\param xml_menu_ctx The XML menu parser context previously created by switch_ivr_menu_stack_xml_init
 *\param menu_stack The menu stack object that will be created for you
 *\param xml_menus The xml Menus source
 *\param xml_menu The xml Menu source of the menu to be created
 *\return SWITCH_STATUS_SUCCESS if all is well
 */
SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_build(switch_ivr_menu_xml_ctx_t *xml_menu_ctx,
																switch_ivr_menu_t ** menu_stack, switch_xml_t xml_menus, switch_xml_t xml_menu);

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_str2action(const char *action_name, switch_ivr_action_t *action);

/*!
 *\param xml_menu_ctx The XML menu parser context previously created by switch_ivr_menu_stack_xml_init
 *\param name The xml tag name to add to the parser engine
 *\param function The menu function callback that will be executed when menu digits are bound to this name
 *\return SWITCH_STATUS_SUCCESS if all is well
 */
SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_add_custom(switch_ivr_menu_xml_ctx_t *xml_menu_ctx,
																	 const char *name, switch_ivr_menu_action_function_t *function);

/*!
 *\param xml_menu_ctx A pointer of a XML menu parser context to be created
 *\param pool memory pool (NULL to create one)
 *\return SWITCH_STATUS_SUCCESS if all is well
 */
SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_init(switch_ivr_menu_xml_ctx_t ** xml_menu_ctx, switch_memory_pool_t *pool);

SWITCH_DECLARE(switch_status_t) switch_ivr_phrase_macro_event(switch_core_session_t *session, const char *macro_name, const char *data, switch_event_t *event, const char *lang,
														switch_input_args_t *args);
#define switch_ivr_phrase_macro(session, macro_name, data, lang, args) switch_ivr_phrase_macro_event(session, macro_name, data, NULL, lang, args)
SWITCH_DECLARE(void) switch_ivr_delay_echo(switch_core_session_t *session, uint32_t delay_ms);
SWITCH_DECLARE(switch_status_t) switch_ivr_find_bridged_uuid(const char *uuid, char *b_uuid, switch_size_t blen);
SWITCH_DECLARE(void) switch_ivr_intercept_session(switch_core_session_t *session, const char *uuid, switch_bool_t bleg);
SWITCH_DECLARE(void) switch_ivr_park_session(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_ivr_wait_for_answer(switch_core_session_t *session, switch_core_session_t *peer_session);

SWITCH_DECLARE(switch_status_t) switch_ivr_read(switch_core_session_t *session,
												uint32_t min_digits,
												uint32_t max_digits,
												const char *prompt_audio_file,
												const char *var_name,
												char *digit_buffer, 
												switch_size_t digit_buffer_length, 
												uint32_t timeout, 
												const char *valid_terminators,
												uint32_t digit_timeout);


SWITCH_DECLARE(switch_status_t) switch_ivr_block_dtmf_session(switch_core_session_t *session);
SWITCH_DECLARE(switch_status_t) switch_ivr_unblock_dtmf_session(switch_core_session_t *session);

SWITCH_DECLARE(switch_status_t) switch_ivr_bind_dtmf_meta_session(switch_core_session_t *session, uint32_t key,
																  switch_bind_flag_t bind_flags, const char *app);
SWITCH_DECLARE(switch_status_t) switch_ivr_unbind_dtmf_meta_session(switch_core_session_t *session, uint32_t key);
SWITCH_DECLARE(switch_status_t) switch_ivr_soft_hold(switch_core_session_t *session, const char *unhold_key, const char *moh_a, const char *moh_b);
SWITCH_DECLARE(switch_status_t) switch_ivr_say(switch_core_session_t *session,
											   const char *tosay,
											   const char *module_name,
											   const char *say_type,
											   const char *say_method,
											   const char *say_gender,
											   switch_input_args_t *args);

SWITCH_DECLARE(switch_status_t) switch_ivr_say_string(switch_core_session_t *session,
													  const char *lang,
													  const char *ext,
													  const char *tosay,
													  const char *module_name,
													  const char *say_type,
													  const char *say_method,
													  const char *say_gender,
													  char **rstr);

SWITCH_DECLARE(switch_say_method_t) switch_ivr_get_say_method_by_name(const char *name);
SWITCH_DECLARE(switch_say_gender_t) switch_ivr_get_say_gender_by_name(const char *name);
SWITCH_DECLARE(switch_say_type_t) switch_ivr_get_say_type_by_name(const char *name);
SWITCH_DECLARE(switch_status_t) switch_ivr_say_spell(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args);
SWITCH_DECLARE(switch_status_t) switch_ivr_say_ip(switch_core_session_t *session,
												  char *tosay,
												  switch_say_callback_t number_func,
												  switch_say_args_t *say_args,
												  switch_input_args_t *args);

SWITCH_DECLARE(switch_status_t) switch_ivr_set_user(switch_core_session_t *session, const char *data);
SWITCH_DECLARE(switch_status_t) switch_ivr_sound_test(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_process_import(switch_core_session_t *session, switch_channel_t *peer_channel, const char *varname, const char *prefix);
SWITCH_DECLARE(switch_bool_t) switch_ivr_uuid_exists(const char *uuid);
SWITCH_DECLARE(switch_bool_t) switch_ivr_uuid_force_exists(const char *uuid);



SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_last_ping(switch_ivr_dmachine_t *dmachine);
SWITCH_DECLARE(const char *) switch_ivr_dmachine_get_name(switch_ivr_dmachine_t *dmachine);
SWITCH_DECLARE(void) switch_ivr_dmachine_set_match_callback(switch_ivr_dmachine_t *dmachine, switch_ivr_dmachine_callback_t match_callback);
SWITCH_DECLARE(void) switch_ivr_dmachine_set_nonmatch_callback(switch_ivr_dmachine_t *dmachine, switch_ivr_dmachine_callback_t nonmatch_callback);
SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_create(switch_ivr_dmachine_t **dmachine_p, 
														   const char *name,
														   switch_memory_pool_t *pool,
														   uint32_t digit_timeout, uint32_t input_timeout,
														   switch_ivr_dmachine_callback_t match_callback,
                                                           switch_ivr_dmachine_callback_t nonmatch_callback,
														   void *user_data);

SWITCH_DECLARE(void) switch_ivr_dmachine_destroy(switch_ivr_dmachine_t **dmachine);

SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_bind(switch_ivr_dmachine_t *dmachine, 
														 const char *realm,
														 const char *digits, 
														 int32_t key,
														 switch_ivr_dmachine_callback_t callback,
														 void *user_data);

SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_feed(switch_ivr_dmachine_t *dmachine, const char *digits, switch_ivr_dmachine_match_t **match);
SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_clear(switch_ivr_dmachine_t *dmachine);
SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_ping(switch_ivr_dmachine_t *dmachine, switch_ivr_dmachine_match_t **match_p);
SWITCH_DECLARE(switch_ivr_dmachine_match_t *) switch_ivr_dmachine_get_match(switch_ivr_dmachine_t *dmachine);
SWITCH_DECLARE(const char *) switch_ivr_dmachine_get_failed_digits(switch_ivr_dmachine_t *dmachine);
SWITCH_DECLARE(void) switch_ivr_dmachine_set_digit_timeout_ms(switch_ivr_dmachine_t *dmachine, uint32_t digit_timeout_ms);
SWITCH_DECLARE(void) switch_ivr_dmachine_set_input_timeout_ms(switch_ivr_dmachine_t *dmachine, uint32_t input_timeout_ms);
SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_clear_realm(switch_ivr_dmachine_t *dmachine, const char *realm);
SWITCH_DECLARE(switch_status_t) switch_ivr_dmachine_set_realm(switch_ivr_dmachine_t *dmachine, const char *realm);


SWITCH_DECLARE(switch_status_t) switch_ivr_get_file_handle(switch_core_session_t *session, switch_file_handle_t **fh);
SWITCH_DECLARE(switch_status_t) switch_ivr_release_file_handle(switch_core_session_t *session, switch_file_handle_t **fh);
SWITCH_DECLARE(switch_status_t) switch_ivr_process_fh(switch_core_session_t *session, const char *cmd, switch_file_handle_t *fhp);
SWITCH_DECLARE(switch_status_t) switch_ivr_insert_file(switch_core_session_t *session, const char *file, const char *insert_file, switch_size_t sample_point);

SWITCH_DECLARE(switch_status_t) switch_ivr_create_message_reply(switch_event_t **reply, switch_event_t *message, const char *new_proto);
SWITCH_DECLARE(char *) switch_ivr_check_presence_mapping(const char *exten_name, const char *domain_name);
SWITCH_DECLARE(switch_status_t) switch_ivr_kill_uuid(const char *uuid, switch_call_cause_t cause);
SWITCH_DECLARE(switch_status_t) switch_ivr_blind_transfer_ack(switch_core_session_t *session, switch_bool_t success);
SWITCH_DECLARE(switch_status_t) switch_ivr_record_session_mask(switch_core_session_t *session, const char *file, switch_bool_t on);

/** @} */

SWITCH_END_EXTERN_C
#endif
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
