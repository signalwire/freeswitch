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

BEGIN_EXTERN_C

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

/*!
  \brief Wait for DTMF digits calling a pluggable callback function when digits are collected.
  \param session the session to read.
  \param dtmf_callback code to execute if any dtmf is dialed during the recording
  \param buf an object to maintain across calls
  \param buflen the size of buf
  \return SWITCH_STATUS_SUCCESS to keep the collection moving.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_callback(switch_core_session_t *session,
																 switch_input_callback_function_t dtmf_callback,
																 void *buf,
																 unsigned int buflen);

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
  \param bridgeto the desired remote callstring
  \return SWITCH_STATUS_SUCCESS if bleg is a running session.
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_outcall(switch_core_session_t *session,
                                                   switch_core_session_t **bleg,
                                                   char *bridgeto);

/*!
  \brief Bridge Audio from one session to another
  \param session one session
  \param peer_session the other session
  \param timelimit maximum number of seconds to wait for both channels to be answered
  \param dtmf_callback code to execute if any dtmf is dialed during the bridge
  \param session_data data to pass to the DTMF callback for session
  \param peer_session_data data to pass to the DTMF callback for peer_session
  \return SWITCH_STATUS_SUCCESS if all is well
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_multi_threaded_bridge(switch_core_session_t *session, 
															   switch_core_session_t *peer_session,
															   unsigned int timelimit,
															   switch_input_callback_function_t dtmf_callback,
															   void *session_data,
															   void *peer_session_data);


/*!
  \brief Transfer an existing session to another location
  \param session the session to transfer
  \param extension the new extension
  \param dialplan the new dialplan (OPTIONAL, may be NULL)
  \param context the new context (OPTIONAL, may be NULL)
*/
SWITCH_DECLARE(switch_status_t) switch_ivr_session_transfer(switch_core_session_t *session, char *extension, char *dialplan, char *context);

/** @} */

END_EXTERN_C

#endif


