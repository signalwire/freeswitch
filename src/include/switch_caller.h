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
 * switch_caller.h -- Caller Identification
 *
 */
/**
 * @file switch_caller.h
 * @brief Caller Identification
 * @see caller
 */
/**
 * @defgroup caller Caller Identity / Dialplan
 * @ingroup core1
 *
 *	This module implements a caller profile which is a group of information about a connected endpoint
 *	such as common caller id and other useful information such as ip address and destination number.
 *	A connected session's channel has up to 3 profiles: It's own, that of the session who spawned it
 *	and that of the session it has spawned.
 *
 *	In addition, this module implements an abstract interface for extensions and applications.
 *	A connected session's channel has one extension object which may have one or more applications
 *	linked into a stack which will be executed in order by the session's state machine when the 
 *	current state is CS_EXECUTE.
 * @{
 */

#ifndef SWITCH_CALLER_H
#define SWITCH_CALLER_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

/*! \brief Call Specific Data
 */
struct switch_caller_profile {
	/*! The Call's User Name */
	char *username;
	/*! The name of the dialplan */
	char *dialplan;
	/*! Caller ID Name */
	char *caller_id_name;
	/*! Caller ID Number */
	char *caller_id_number;
	/*! Caller Network Address (when applicable) */
	char *network_addr;
	/*! ANI (when applicable) */
	char *ani;
	/*! ANI II (when applicable) */
	char *aniii;
	/*! RDNIS */
	char *rdnis;
	/*! Destination Number */
	char *destination_number;
	/*! channel type */
	char *source;
	/*! channel name */
	char *chan_name;
	/*! unique id */
	char *uuid;
	/*! context */
	char *context;
	/*! flags */
	switch_caller_profile_flag_t flags;
	struct switch_caller_profile *next;
};

/*! \brief An Abstract Representation of a dialplan Application */
struct switch_caller_application {
	/*! The name of the registered application to call */
	char *application_name;
	/*! An optional argument string to pass to the application */
	char *application_data;
	/*! A function pointer to the application */
	switch_application_function_t application_function;
	struct switch_caller_application *next;
};

/*! \brief An Abstract Representation of a dialplan extension */
struct switch_caller_extension {
	/*! The name of the extension */
	char *extension_name;
	/*! The number of the extension */
	char *extension_number;
	/*! Pointer to the current application for this extension */
	switch_caller_application_t *current_application;
	/*! Pointer to the last application for this extension */
	switch_caller_application_t *last_application;
	/*! Pointer to the entire stack of applications for this extension */
	switch_caller_application_t *applications;
	struct switch_caller_extension *next;
};

/*!
  \brief Create a new extension with desired parameters
  \param session session associated with the extension (bound by scope)
  \param extension_name extension name
  \param extension_number extension number
  \return a new extension object allocated from the session's memory pool
*/
SWITCH_DECLARE(switch_caller_extension_t *) switch_caller_extension_new(switch_core_session_t *session,
																	  char *extension_name,
																	  char *extension_number
																	  );

/*!
  \brief Add an application (instruction) to the given extension
  \param session session associated with the extension (bound by scope)
  \param caller_extension extension to add the application to
  \param application_name the name of the application
  \param extra_data optional argument to the application
*/
SWITCH_DECLARE(void) switch_caller_extension_add_application(switch_core_session_t *session,
															 switch_caller_extension_t *caller_extension,
															 char *application_name,
															 char *extra_data);


/*!
  \brief Get the value of a field in a caller profile based on it's name
  \param caller_profile The caller profile
  \param name the name
  \note this function is meant for situations where the name paramater is the contents of the variable
*/
SWITCH_DECLARE(char *) switch_caller_get_field_by_name(switch_caller_profile_t *caller_profile, char *name);

/*!
  \brief Create a new caller profile object
  \param pool memory pool to use
  \param username tne username of the caller
  \param dialplan name of the dialplan module in use
  \param caller_id_name caller ID name
  \param caller_id_number caller ID number
  \param network_addr network address
  \param ani ANI information
  \param aniii ANI II information
  \param rdnis RDNIS
  \param source the source 
  \param context a logical context
  \param destination_number destination number
  \return a new profile object allocated from the session's memory pool
*/
SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_new(switch_memory_pool_t *pool,
																	char *username,
																	char *dialplan,
																	char *caller_id_name,
																	char *caller_id_number,
																	char *network_addr,
																	char *ani,
																	char *aniii,
																	char *rdnis,
																	char *source,
																	char *context,
																	char *destination_number);

/*!
  \brief Clone an existing caller profile object
  \param session session associated with the profile (bound by scope)
  \param tocopy the existing profile
*/
	
SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_clone(switch_core_session_t *session,
																	switch_caller_profile_t *tocopy);

/*!
  \brief Add headers to an existing event in regards to a specific profile
  \param caller_profile the desired profile
  \param prefix a prefix string to all of the field names (for uniqueness)
  \param event the event to add the information to
*/

SWITCH_DECLARE(void) switch_caller_profile_event_set_data(switch_caller_profile_t *caller_profile, char *prefix, switch_event_t *event);

SWITCH_END_EXTERN_C

/** @} */

#endif


