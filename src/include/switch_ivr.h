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

SWITCH_BEGIN_EXTERN_C

static const switch_state_handler_table_t noop_state_handler = {0};

/**
 * @defgroup switch_ivr IVR Library
 * @ingroup core1
 *	A group of core functions to do IVR related functions designed to be 
 *	building blocks for a higher level IVR interface.
 * @{
 */


/*!
  \brief Wait for time to pass for a specified number of milliseconds
  \param session the session to wait for.
  \param ms the number of milliseconds
  \return SWITCH_STATUS_SUCCESS if the channel is still up
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_sleep(switch_core_session_t *session, uint32_t ms);

SWITCH_DECLARE(switch_status_t) switch_ivr_park(switch_core_session_t *session);

/*!
  \brief Wait for DTMF digits calling a pluggable callback function when digits are collected.
  \param session the session to read.
  \param dtmf_callback code to execute if any dtmf is dialed during the recording
  \param buf an object to maintain across calls
  \param buflen the size of buf
  \param timeout a timeout in milliseconds
  \return SWITCH_STATUS_SUCCESS to keep the collection moving.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_callback(switch_core_session_t *session,
																   switch_input_callback_function_t dtmf_callback,
																   void *buf,
																   unsigned int buflen,
																   unsigned int timeout);

/*!
  \brief Wait for specified number of DTMF digits, untile terminator is received or until the channel hangs up.
  \param session the session to read.
  \param buf strig to write to
  \param buflen max size of buf
  \param maxdigits max number of digits to read
  \param terminators digits to end the collection
  \param terminator actual digit that caused the collection to end (if any)
  \param timeout timeout in ms
  \return SWITCH_STATUS_SUCCESS to keep the collection moving.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_count(switch_core_session_t *session,
																char *buf,
																unsigned int buflen,
																unsigned int maxdigits,
																const char *terminators,
																char *terminator,
																unsigned int timeout);

/*!
  \brief Engage background Speech detection on a session
  \param session the session to attach
  \param mod_name the module name of the ASR library
  \param grammar the grammar name
  \param path the path to the grammar file
  \param dest the destination address
  \param ah an ASR handle to use (NULL to create one)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech(switch_core_session_t *session,
														 char *mod_name,
														 char *grammar,
														 char *path,
														 char *dest,
														 switch_asr_handle_t *ah);

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
  \param grammar the grammar name
  \param path the grammar path
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_load_grammar(switch_core_session_t *session, char *grammar, char *path);

/*!
  \brief Unload a grammar on a background speech detection handle
  \param session The session to change the grammar on
  \param grammar the grammar name
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_unload_grammar(switch_core_session_t *session, char *grammar);

/*!
  \brief Record a session to disk
  \param session the session to record
  \param file the path to the file
  \param fh file handle to use (NULL for builtin one)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_record_session(switch_core_session_t *session, char *file,  switch_file_handle_t *fh);

/*!
  \brief Stop Recording a session
  \param session the session to stop recording
  \param file the path to the file
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_stop_record_session(switch_core_session_t *session, char *file);

/*!
  \brief play a file from the disk to the session
  \param session the session to play the file too
  \param fh file handle to use (NULL for builtin one)
  \param file the path to the file
  \param timer_name the name of a timer to use input will be absorbed (NULL to time off the session input).
  \param dtmf_callback code to execute if any dtmf is dialed during the playback
  \param buf an object to maintain across calls
  \param buflen the size of buf
  \return SWITCH_STATUS_SUCCESS if all is well
  \note passing a NULL dtmf_callback nad a not NULL buf indicates to copy any dtmf to buf and stop playback.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_play_file(switch_core_session_t *session,
												   switch_file_handle_t *fh,
												   char *file,
												   char *timer_name,
												   switch_input_callback_function_t dtmf_callback,
												   void *buf,
												   unsigned int buflen);



/*!
  \brief record a file from the session to a file
  \param session the session to record from
  \param fh file handle to use
  \param file the path to the file
  \param dtmf_callback code to execute if any dtmf is dialed during the recording
  \param buf an object to maintain across calls
  \param buflen the size of buf
  \return SWITCH_STATUS_SUCCESS if all is well
  \note passing a NULL dtmf_callback nad a not NULL buf indicates to copy any dtmf to buf and stop recording.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_record_file(switch_core_session_t *session,
													 switch_file_handle_t *fh,
													 char *file,
													 switch_input_callback_function_t dtmf_callback,
													 void *buf,
													 unsigned int buflen);

/*!
 \brief Function to evaluate an expression against a string
 \param target The string to find a match in
 \param expression The regular expression to run against the string
 \return Boolean if a match was found or not
*/
SWITCH_DECLARE(switch_status_t) switch_regex_match(char *target, char *expression);

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
  \param digit_buffer variable digits captured will be put back into (empty if capture failed)
  \param digit_buffer_length length of the buffer for digits (should be the same or larger than max_digits)
  \return switch status, used to note status of channel (will still return success if digit capture failed)
  \note to test for digit capture failure look for \0 in the first position of the buffer
*/
SWITCH_DECLARE(switch_status_t) switch_play_and_get_digits(switch_core_session_t *session,
                                                           unsigned int min_digits,
                                                           unsigned int max_digits,
                                                           unsigned int max_tries,
                                                           unsigned int timeout,
                                                           char* valid_terminators,
                                                           char* audio_file,
                                                           char* bad_input_audio_file,
                                                           void* digit_buffer,
                                                           unsigned int digit_buffer_length,
                                                           char* digits_regex);

SWITCH_DECLARE(switch_status_t) switch_ivr_speak_text_handle(switch_core_session_t *session,
                                                             switch_speech_handle_t *sh,
                                                             switch_codec_t *codec,
                                                             switch_timer_t *timer,
                                                             switch_input_callback_function_t dtmf_callback,
                                                             char *text,
                                                             void *buf,
                                                             unsigned int buflen);

/*!
  \brief Speak given text with given tts engine
  \param session the session to speak on
  \param tts_name the desired tts module
  \param voice_name the desired voice
  \param timer_name optional timer to use for async behaviour
  \param rate the sample rate
  \param dtmf_callback code to execute if any dtmf is dialed during the audio
  \param text the text to speak
  \param buf an option data pointer to pass to the callback or a string to put encountered digits in
  \param buflen the len of buf
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_speak_text(switch_core_session_t *session, 
													  char *tts_name,
													  char *voice_name,
													  char *timer_name,
													  uint32_t rate,
													  switch_input_callback_function_t dtmf_callback,
													  char *text,
													  void *buf,
													  unsigned int buflen);

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
  \return SWITCH_STATUS_SUCCESS if bleg is a running session.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_originate(switch_core_session_t *session,
													 switch_core_session_t **bleg,
													 switch_call_cause_t *cause,
													 char *bridgeto,
													 uint32_t timelimit_sec,
													 const switch_state_handler_table_t *table,
													 char *cid_name_override,
													 char *cid_num_override,
													 switch_caller_profile_t *caller_profile_override);

/*!
  \brief Bridge Audio from one session to another
  \param session one session
  \param peer_session the other session
  \param dtmf_callback code to execute if any dtmf is dialed during the bridge
  \param session_data data to pass to the DTMF callback for session
  \param peer_session_data data to pass to the DTMF callback for peer_session
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_multi_threaded_bridge(switch_core_session_t *session, 
																 switch_core_session_t *peer_session,
																 switch_input_callback_function_t dtmf_callback,
																 void *session_data,
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
SWITCH_DECLARE(switch_status_t) switch_ivr_session_transfer(switch_core_session_t *session, char *extension, char *dialplan, char *context);

/*!
  \brief Bridge two existing sessions
  \param originator_uuid the uuid of the originator
  \param originatee_uuid the uuid of the originator
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_uuid_bridge(char *originator_uuid, char *originatee_uuid);

/*!
  \brief Signal a session to request direct media access to it's remote end
  \param uuid the uuid of the session to request
  \param flags flags to influence behaviour (SMF_REBRIDGE to rebridge the call in media mode)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_media(char *uuid, switch_media_flag_t flags);

/*!
  \brief Signal a session to request indirect media allowing it to exchange media directly with another device
  \param uuid the uuid of the session to request
  \param flags flags to influence behaviour (SMF_REBRIDGE to rebridge the call in no_media mode)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_nomedia(char *uuid, switch_media_flag_t flags);

/*!
  \brief Signal the session with a protocol specific hold message.
  \param uuid the uuid of the session to hold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_hold_uuid(char *uuid);

/*!
  \brief Signal the session with a protocol specific unhold message.
  \param uuid the uuid of the session to hold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_unhold_uuid(char *uuid);

/*!
  \brief Signal the session with a protocol specific hold message.
  \param session the session to hold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_hold(switch_core_session_t *session);

/*!
  \brief Signal the session with a protocol specific unhold message.
  \param uuid the uuid of the session to unhold
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_unhold(switch_core_session_t *session);

/*!
  \brief Signal the session to broadcast audio
  \param uuid the uuid of the session to broadcast on
  \param path the path data of the broadcast "/path/to/file.wav [<timer name>]" or "speak:<engine>|<voice>|<Text to say>"
  \param flags flags to send to the request (SMF_ECHO_BRIDGED to send the broadcast to both sides of the call)
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_broadcast(char *uuid, char *path, switch_media_flag_t flags);

/*!
  \brief Transfer variables from one session to another 
  \param sessa the original session
  \param sessb the new session
  \param var the name of the variable to transfer (NULL for all)
  \return SWITCH_STATUS_SUCCESS if all is well 
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_transfer_variable(switch_core_session_t *sessa, switch_core_session_t *sessb, char *var);

/** @} */

SWITCH_END_EXTERN_C

#endif


