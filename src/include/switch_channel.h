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
 * switch_channel.h -- Media Channel Interface
 *
 */
/** 
 * @file switch_channel.h
 * @brief Media Channel Interface
 * @see switch_channel
 */

#ifndef SWITCH_CHANNEL_H
#define SWITCH_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

/**
 * @defgroup switch_channel Channel Functions
 * @ingroup FREESWITCH
 *	The switch_channel object is a private entity that belongs to a session that contains the call
 *	specific information such as the call state, variables, caller profiles and DTMF queue
 * @{
 */

/*!
  \brief Get the current state of a channel in the state engine
  \param channel channel to retrieve state from
  \return current state of channel
*/
SWITCH_DECLARE(switch_channel_state) switch_channel_get_state(switch_channel *channel);

/*!
  \brief Set the current state of a channel
  \param channel channel to set state of
  \param state new state
  \return current state of channel after application of new state
*/	
SWITCH_DECLARE(switch_channel_state) switch_channel_set_state(switch_channel *channel, switch_channel_state state);

/*!
  \brief Allocate a new channel
  \param channel NULL pointer to allocate channel to
  \param pool memory_pool to use for allocation
  \returns SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status) switch_channel_alloc(switch_channel **channel, switch_memory_pool *pool);

/*!
  \brief Connect a newly allocated channel to a session object and setup it's initial state
  \param channel the channel to initilize
  \param session the session to connect the channel to
  \param state the initial state of the channel
  \param flags the initial channel flags
*/
SWITCH_DECLARE(switch_status) switch_channel_init(switch_channel *channel,
								switch_core_session *session,
								switch_channel_state state,
								switch_channel_flag flags);

/*!
  \brief Set the given channel's caller profile
  \param channel channel to assign the profile to
  \param caller_profile the profile to assign
*/
SWITCH_DECLARE(void) switch_channel_set_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile);

/*!
  \brief Retrive the given channel's caller profile
  \param channel channel to retrive the profile from
  \return the requested profile
*/
SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_caller_profile(switch_channel *channel);

/*!
  \brief Set the given channel's originator caller profile
  \param channel channel to assign the profile to
  \param caller_profile the profile to assign
*/
SWITCH_DECLARE(void) switch_channel_set_originator_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile);

/*!
  \brief Retrive the given channel's originator caller profile
  \param channel channel to retrive the profile from
  \return the requested profile
*/
SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_originator_caller_profile(switch_channel *channel);

/*!
  \brief Set the given channel's originatee caller profile
  \param channel channel to assign the profile to
  \param caller_profile the profile to assign
*/
SWITCH_DECLARE(void) switch_channel_set_originatee_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile);

/*!
  \brief Retrive the given channel's originatee caller profile
  \param channel channel to retrive the profile from
  \return the requested profile
*/
SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_originatee_caller_profile(switch_channel *channel);

/*!
  \brief Set a variable on a given channel
  \param channel channel to set variable on
  \param varname the name of the variable
  \param value the vaule of the variable
  \returns SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status) switch_channel_set_variable(switch_channel *channel, char *varname, char *value);

/*!
  \brief Retrieve a variable from a given channel
  \param channel channel to retrieve variable from
  \param varname the name of the variable
  \return the value of the requested variable
*/
SWITCH_DECLARE(char *) switch_channel_get_variable(switch_channel *channel, char *varname);

/*!
  \brief Assign a caller extension to a given channel
  \param channel channel to assign extension to
  \param caller_extension extension to assign
*/
SWITCH_DECLARE(void) switch_channel_set_caller_extension(switch_channel *channel, switch_caller_extension *caller_extension);

/*!
  \brief Retrieve caller extension from a given channel
  \param channel channel to retrieve extension from
  \return the requested extension
*/
SWITCH_DECLARE(switch_caller_extension *) switch_channel_get_caller_extension(switch_channel *channel);

/*!
  \brief Test for presence of given flag(s) on a given channel
  \param channel channel to test 
  \param flags or'd list of channel flags to test
  \return SWITCH_STATUS_SUCCESS if provided flags are set
*/
SWITCH_DECLARE(switch_status) switch_channel_test_flag(switch_channel *channel, int flags);

/*!
  \brief Set given flag(s) on a given channel
  \param channel channel on which to set flag(s)
  \param flags or'd list of flags to set
  \return SWITCH_STATUS_SUCCESS if flags were set
*/
SWITCH_DECLARE(switch_status) switch_channel_set_flag(switch_channel *channel, int flags);

/*!
  \brief Clear given flag(s) from a channel
  \param channel channel to clear flags from
  \param flags or'd list of flags to clear
  \return SWITCH_STATUS_SUCCESS if flags were cleared
*/
SWITCH_DECLARE(switch_status) switch_channel_clear_flag(switch_channel *channel, int flags);

/*!
  \brief Answer a channel (initiate/acknowledge a successful connection)
  \param channel channel to answer
  \return SWITCH_STATUS_SUCCESS if channel was answered successfully
*/
SWITCH_DECLARE(switch_status) switch_channel_answer(switch_channel *channel);

/*!
  \brief Assign an event handler table to a given channel
  \param channel channel on which to assign the event handler table
  \param event_handlers table of event handler functions
*/
SWITCH_DECLARE(void) switch_channel_set_event_handlers(switch_channel *channel, const struct switch_event_handler_table *event_handlers);

/*!
  \brief Retrieve an event handler tablefrom a given channel
  \param channel channel from which to retrieve the event handler table
  \return given channel's event handler table
*/
SWITCH_DECLARE(const struct switch_event_handler_table *) switch_channel_get_event_handlers(switch_channel *channel);

/*!
  \brief Set private data on channel
  \param channel channel on which to set data
  \param private void pointer to private data
  \return SWITCH_STATUS_SUCCESS if data was set
*/
SWITCH_DECLARE(switch_status) switch_channel_set_private(switch_channel *channel, void *private);

/*!
  \brief Retrieve private from a given channel
  \param channel channel to retrieve data from
  \return void pointer to channel's private data
*/
SWITCH_DECLARE(void *) switch_channel_get_private(switch_channel *channel);

/*!
  \brief Assign a name to a given channel
  \param channel channel to assign name to
  \param name name to assign
  \return SWITCH_STATUS_SUCCESS if name was assigned
*/
SWITCH_DECLARE(switch_status) switch_channel_set_name(switch_channel *channel, char *name);

/*!
  \brief Retrieve the name of a given channel
  \param channel channel to get name of
  \return the channel's name
*/
SWITCH_DECLARE(char *) switch_channel_get_name(switch_channel *channel);

/*!
  \brief Hangup a channel flagging it's state machine to end
  \param channel channel to hangup
  \return SWITCH_STATUS_SUCCESS if channel state was set to hangup
*/
SWITCH_DECLARE(switch_status) switch_channel_hangup(switch_channel *channel);

/*!
  \brief Test for presence of DTMF on a given channel
  \param channel channel to test
  \return number of digits in the queue
*/
SWITCH_DECLARE(int) switch_channel_has_dtmf(switch_channel *channel);

/*!
  \brief Queue DTMF on a given channel
  \param channel channel to queue DTMF to
  \param dtmf string of digits to queue
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status) switch_channel_queue_dtmf(switch_channel *channel, char *dtmf);

/*!
  \brief Retrieve DTMF digits from a given channel
  \param channel channel to retrieve digits from
  \param dtmf buffer to write dtmf to
  \param len max size in bytes of the buffer
  \return number of bytes read into the buffer
*/
SWITCH_DECLARE(int) switch_channel_dequeue_dtmf(switch_channel *channel, char *dtmf, size_t len);

/*!
  \brief Render the name of the provided state enum
  \param state state to get name of
  \return the string representation of the state
*/
SWITCH_DECLARE(const char *) switch_channel_state_name(switch_channel_state state);

/*!
  \brief Add information about a given channel to an event object
  \param channel channel to add information about
  \param event event to add information to
*/
SWITCH_DECLARE(void) switch_channel_event_set_data(switch_channel *channel, switch_event *event);


// These may go away
SWITCH_DECLARE(switch_status) switch_channel_set_raw_mode (switch_channel *channel, int freq, int bits, int channels, int ms, int kbps);
SWITCH_DECLARE(switch_status) switch_channel_get_raw_mode (switch_channel *channel, int *freq, int *bits, int *channels, int *ms, int *kbps);
/** @} */

#ifdef __cplusplus
}
#endif

#endif
