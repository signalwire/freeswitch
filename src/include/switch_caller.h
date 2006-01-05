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
/*! \file switch_caller.h
    \brief Caller Identification

	This file implements a caller profile which is a group of information about a connected endpoint
	such as common caller id and other useful information such as ip address and destination number.
	A connected session's channel has up to 3 profiles: It's own, that of the session who spawned it
	and that of the session it has spawned.

	In addition, this file implements an abstract interface for extensions and applications.
	A connected session's channel has one extension object which may have one or more applications
	linked into a stack which will be executed in order by the session's state machine when the 
	current state is CS_EXECUTE.
*/

#ifndef SWITCH_CALLER_H
#define SWITCH_CALLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

	/*! \brief Call Specific Data
	 */
	struct switch_caller_profile {
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
		char *ani2;
		/*! Destination Number */
		char *destination_number;
	};

	/*! \brief An Abstract Representation of a dialplan Application */
	struct switch_caller_application {
		/*! The name of the registered application to call */
		char *application_name;
		/*! An optional argument string to pass to the application */
		char *application_data;
		/*! A function pointer to the application */
		switch_application_function application_function;
		struct switch_caller_application *next;
	};

	/*! \brief An Abstract Representation of a dialplan extension */
	struct switch_caller_extension {
		/*! The name of the extension */
		char *extension_name;
		/*! The number of the extension */
		char *extension_number;
		/*! Pointer to the current application for this extension */
		struct switch_caller_application *current_application;
		/*! Pointer to the last application for this extension */
		struct switch_caller_application *last_application;
		/*! Pointer to the entire stack of applications for this extension */
		struct switch_caller_application *applications;
	};

	/*!
	  \brief Create a new extension with desired parameters
	  \param session session associated with the extension (bound by scope)
	  \param extension_name extension name
	  \param extension_number extension number
	  \return a new extension object allocated from the session's memory pool
	*/
	SWITCH_DECLARE(switch_caller_extension *) switch_caller_extension_new(switch_core_session *session,
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
	SWITCH_DECLARE(void) switch_caller_extension_add_application(switch_core_session *session,
																 switch_caller_extension *caller_extension,
																 char *application_name,
																 char *extra_data);


	/*!
      \brief Create a new caller profile object
	  \param session session associated with the profile (bound by scope)
	  \param dialplan name of the dialplan module in use
	  \param caller_id_name caller ID name
	  \param caller_id_number caller ID number
	  \param network_addr network address
	  \param ani ANI information
	  \param ani2 ANI II information
	  \param destination_number destination number
	  \return a new profile object allocated from the session's memory pool
	*/
	SWITCH_DECLARE(switch_caller_profile *) switch_caller_profile_new(switch_core_session *session,
																	  char *dialplan,
																	  char *caller_id_name,
																	  char *caller_id_number,
																	  char *network_addr,
																	  char *ani,
																	  char *ani2,
																	  char *destination_number);

	/*!
      \brief Clone an existing caller profile object
	  \param session session associated with the profile (bound by scope)
	  \param tocopy the existing profile
	*/
	
	SWITCH_DECLARE(switch_caller_profile *) switch_caller_profile_clone(switch_core_session *session,
																		switch_caller_profile *tocopy);

	/*!
      \brief Add headers to an existing event in regards to a specific profile
	  \param caller_profile the desired profile
	  \param prefix a prefix string to all of the field names (for uniqueness)
	  \param event the event to add the information to
	*/

	SWITCH_DECLARE(void) switch_caller_profile_event_set_data(switch_caller_profile *caller_profile, char *prefix, switch_event *event);


#ifdef __cplusplus
}
#endif


#endif


