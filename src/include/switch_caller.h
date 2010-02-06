/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
	const char *username;
	/*! The name of the dialplan */
	const char *dialplan;
	/*! Caller ID Name */
	const char *caller_id_name;
	/*! Caller ID Number */
	const char *caller_id_number;
	/*! Callee ID Name */
	const char *callee_id_name;
	/*! Callee ID Number */
	const char *callee_id_number;
	uint8_t caller_ton;
	uint8_t caller_numplan;
	/*! Caller Network Address (when applicable) */
	const char *network_addr;
	/*! ANI (when applicable) */
	const char *ani;
	uint8_t ani_ton;
	uint8_t ani_numplan;
	/*! ANI II (when applicable) */
	const char *aniii;
	/*! RDNIS */
	const char *rdnis;
	uint8_t rdnis_ton;
	uint8_t rdnis_numplan;
	/*! Destination Number */
	char *destination_number;
	uint8_t destination_number_ton;
	uint8_t destination_number_numplan;
	/*! channel type */
	const char *source;
	/*! channel name */
	char *chan_name;
	/*! unique id */
	char *uuid;
	/*! context */
	const char *context;
	/*! profile index */
	const char *profile_index;
	/*! flags */
	switch_caller_profile_flag_t flags;
	struct switch_caller_profile *originator_caller_profile;
	struct switch_caller_profile *originatee_caller_profile;
	struct switch_caller_profile *hunt_caller_profile;
	struct switch_channel_timetable *times;
	struct switch_caller_extension *caller_extension;
	switch_memory_pool_t *pool;
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
	struct switch_caller_profile *children;
	struct switch_caller_extension *next;
};

/*!
  \brief Create a new extension with desired parameters
  \param session session associated with the extension (bound by scope)
  \param extension_name extension name
  \param extension_number extension number
  \return a new extension object allocated from the session's memory pool
*/
SWITCH_DECLARE(switch_caller_extension_t *) switch_caller_extension_new(_In_ switch_core_session_t *session,
																		_In_z_ const char *extension_name, _In_z_ const char *extension_number);

SWITCH_DECLARE(switch_status_t) switch_caller_extension_clone(switch_caller_extension_t **new_ext, switch_caller_extension_t *orig,
															  switch_memory_pool_t *pool);

/*!
  \brief Add an application (instruction) to the given extension
  \param session session associated with the extension (bound by scope)
  \param caller_extension extension to add the application to
  \param application_name the name of the application
  \param extra_data optional argument to the application
*/
SWITCH_DECLARE(void) switch_caller_extension_add_application(_In_ switch_core_session_t *session,
															 _In_ switch_caller_extension_t *caller_extension,
															 _In_z_ const char *application_name, _In_z_ const char *extra_data);

/*!
  \brief Add an application (instruction) to the given extension
  \param session session associated with the extension (bound by scope)
  \param caller_extension extension to add the application to
  \param application_name the name of the application
  \param fmt optional argument to the application (printf format string)
*/
SWITCH_DECLARE(void) switch_caller_extension_add_application_printf(_In_ switch_core_session_t *session,
																	_In_ switch_caller_extension_t *caller_extension,
																	_In_z_ const char *application_name, _In_z_ const char *fmt, ...);


/*!
  \brief Get the value of a field in a caller profile based on it's name
  \param caller_profile The caller profile
  \param name the name
  \note this function is meant for situations where the name paramater is the contents of the variable
*/
	 _Check_return_ _Ret_opt_z_ SWITCH_DECLARE(const char *) switch_caller_get_field_by_name(_In_ switch_caller_profile_t *caller_profile,
																							 _In_z_ const char *name);

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
SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_new(_In_ switch_memory_pool_t *pool,
																	_In_opt_z_ const char *username,
																	_In_opt_z_ const char *dialplan,
																	_In_opt_z_ const char *caller_id_name,
																	_In_opt_z_ const char *caller_id_number,
																	_In_opt_z_ const char *network_addr,
																	_In_opt_z_ const char *ani,
																	_In_opt_z_ const char *aniii,
																	_In_opt_z_ const char *rdnis,
																	_In_opt_z_ const char *source,
																	_In_opt_z_ const char *context, _In_opt_z_ const char *destination_number);

/*!
  \brief Clone an existing caller profile object
  \param session session associated with the profile (bound by scope)
  \param tocopy the existing profile
*/
SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_clone(_In_ switch_core_session_t *session, _In_ switch_caller_profile_t *tocopy);

/*!
  \brief Duplicate an existing caller profile object
  \param pool pool to duplicate with
  \param tocopy the existing profile
*/
SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_dup(_In_ switch_memory_pool_t *pool, _In_ switch_caller_profile_t *tocopy);

/*!
  \brief Add headers to an existing event in regards to a specific profile
  \param caller_profile the desired profile
  \param prefix a prefix string to all of the field names (for uniqueness)
  \param event the event to add the information to
*/

SWITCH_DECLARE(void) switch_caller_profile_event_set_data(_In_ switch_caller_profile_t *caller_profile,
														  _In_opt_z_ const char *prefix, _In_ switch_event_t *event);

SWITCH_END_EXTERN_C
/** @} */
#endif
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
