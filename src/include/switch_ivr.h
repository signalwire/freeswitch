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

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

/**
 * @defgroup switch_ivr IVR Library
 * @ingroup FREESWITCH
 *	A group of core functions to do IVR related functions designed to be 
 *	building blocks for a higher level IVR interface.
 * @{
 */

/*!
  \brief Wait for DTMF digits calling a pluggable callback function when digits are collected.
  \param session the session to read.
  \param dtmf_callback code to execute if any dtmf is dialed during the recording
  \param buf an object to maintain across calls
  \param buflen the size of buf
  \return SWITCH_STATUS_SUCCESS to keep the collection moving.
*/
SWITCH_DECLARE(switch_status) switch_ivr_collect_digits_callback(switch_core_session *session,
																 switch_dtmf_callback_function dtmf_callback,
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
  \return SWITCH_STATUS_SUCCESS to keep the collection moving.
*/
SWITCH_DECLARE(switch_status) switch_ivr_collect_digits_count(switch_core_session *session,
															  char *buf,
															  unsigned int buflen,
															  unsigned int maxdigits,
															  const char *terminators,
															  char *terminator);
	
/*!
  \brief play a file from the disk to the session
  \param session the session to play the file too
  \param pointer to file handle to use (NULL for builtin one)
  \param file the path to the file
  \param timer_name the name of a timer to use input will be absorbed (NULL to time off the session input).
  \param dtmf_callback code to execute if any dtmf is dialed during the playback
  \param buf an object to maintain across calls
  \param buflen the size of buf
  \return SWITCH_STATUS_SUCCESS if all is well
  \note passing a NULL dtmf_callback nad a not NULL buf indicates to copy any dtmf to buf and stop playback.
*/
SWITCH_DECLARE(switch_status) switch_ivr_play_file(switch_core_session *session,
												   switch_file_handle *fh,
												   char *file,
												   char *timer_name,
												   switch_dtmf_callback_function dtmf_callback,
												   void *buf,
												   unsigned int buflen);



/*!
  \brief record a file from the session to a file
  \param session the session to record from
  \param file the path to the file
  \param dtmf_callback code to execute if any dtmf is dialed during the recording
  \param buf an object to maintain across calls
  \param buflen the size of buf
  \return SWITCH_STATUS_SUCCESS if all is well
  \note passing a NULL dtmf_callback nad a not NULL buf indicates to copy any dtmf to buf and stop recording.
*/
SWITCH_DECLARE(switch_status) switch_ivr_record_file(switch_core_session *session,
													 char *file,
													 switch_dtmf_callback_function dtmf_callback,
													 void *buf,
													 unsigned int buflen);


SWITCH_DECLARE(switch_status) switch_ivr_speak_text(switch_core_session *session, 
													char *tts_name,
													char *voice_name,
													char *timer_name,
													size_t rate,
													switch_dtmf_callback_function dtmf_callback,
													char *text,
													void *buf,
													unsigned int buflen);

/** @} */

#ifdef __cplusplus
}
#endif

#endif


