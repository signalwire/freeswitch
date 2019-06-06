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
 * Anthony Minessale II <anthm@freeswitch.org>
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

#include <switch.h>

SWITCH_BEGIN_EXTERN_C struct switch_channel_timetable {
	switch_time_t profile_created;
	switch_time_t created;
	switch_time_t answered;
	switch_time_t progress;
	switch_time_t progress_media;
	switch_time_t hungup;
	switch_time_t transferred;
	switch_time_t resurrected;
	switch_time_t bridged;
	switch_time_t last_hold;
	switch_time_t hold_accum;
	struct switch_channel_timetable *next;
};

typedef struct switch_channel_timetable switch_channel_timetable_t;

/**
 * @defgroup switch_channel Channel Functions
 * @ingroup core1
 *	The switch_channel object is a private entity that belongs to a session that contains the call
 *	specific information such as the call state, variables, caller profiles and DTMF queue
 * @{
 */

/*!
  \brief Get the current state of a channel in the state engine
  \param channel channel to retrieve state from
  \return current state of channel
*/
SWITCH_DECLARE(switch_channel_state_t) switch_channel_get_state(switch_channel_t *channel);
SWITCH_DECLARE(switch_channel_state_t) switch_channel_get_running_state(switch_channel_t *channel);
SWITCH_DECLARE(int) switch_channel_check_signal(switch_channel_t *channel, switch_bool_t in_thread_only);

/*!
  \brief Determine if a channel is ready for io
  \param channel channel to test
  \return true if the channel is ready
*/
SWITCH_DECLARE(int) switch_channel_test_ready(switch_channel_t *channel, switch_bool_t check_ready, switch_bool_t check_media);

#define switch_channel_ready(_channel) switch_channel_test_ready(_channel, SWITCH_TRUE, SWITCH_FALSE)
#define switch_channel_media_ready(_channel) switch_channel_test_ready(_channel, SWITCH_TRUE, SWITCH_TRUE)
#define switch_channel_media_up(_channel) (switch_channel_test_flag(_channel, CF_ANSWERED) || switch_channel_test_flag(_channel, CF_EARLY_MEDIA))

#define switch_channel_up(_channel) (switch_channel_check_signal(_channel, SWITCH_TRUE) || switch_channel_get_state(_channel) < CS_HANGUP)
#define switch_channel_down(_channel) (switch_channel_check_signal(_channel, SWITCH_TRUE) || switch_channel_get_state(_channel) >= CS_HANGUP)

#define switch_channel_up_nosig(_channel) (switch_channel_get_state(_channel) < CS_HANGUP)
#define switch_channel_down_nosig(_channel) (switch_channel_get_state(_channel) >= CS_HANGUP)

#define switch_channel_media_ack(_channel) (!switch_channel_test_cap(_channel, CC_MEDIA_ACK) || switch_channel_test_flag(_channel, CF_MEDIA_ACK))

#define switch_channel_text_only(_channel) (switch_channel_test_flag(_channel, CF_HAS_TEXT) && !switch_channel_test_flag(_channel, CF_AUDIO))


SWITCH_DECLARE(void) switch_channel_wait_for_state(switch_channel_t *channel, switch_channel_t *other_channel, switch_channel_state_t want_state);
SWITCH_DECLARE(void) switch_channel_wait_for_state_timeout(switch_channel_t *other_channel, switch_channel_state_t want_state, uint32_t timeout);
SWITCH_DECLARE(switch_status_t) switch_channel_wait_for_flag(switch_channel_t *channel,
															 switch_channel_flag_t want_flag,
															 switch_bool_t pres, uint32_t to, switch_channel_t *super_channel);

SWITCH_DECLARE(switch_channel_state_t) switch_channel_perform_set_state(switch_channel_t *channel,
																		const char *file, const char *func, int line, switch_channel_state_t state);

SWITCH_DECLARE(switch_channel_state_t) switch_channel_perform_set_running_state(switch_channel_t *channel, switch_channel_state_t state,
																				const char *file, const char *func, int line);
#define switch_channel_set_running_state(channel, state) switch_channel_perform_set_running_state(channel, state, __FILE__, __SWITCH_FUNC__, __LINE__)

/*!
  \brief Set the current state of a channel
  \param channel channel to set state of
  \param state new state
  \return current state of channel after application of new state
*/
#define switch_channel_set_state(channel, state) switch_channel_perform_set_state(channel, __FILE__, __SWITCH_FUNC__, __LINE__, state)

/*!
  \brief return a cause code for a given string
  \param str the string to check
  \return the code
*/
SWITCH_DECLARE(switch_call_cause_t) switch_channel_str2cause(_In_ const char *str);

/*!
  \brief return the cause code for a given channel
  \param channel the channel
  \return the code
*/
SWITCH_DECLARE(switch_call_cause_t) switch_channel_get_cause(_In_ switch_channel_t *channel);

SWITCH_DECLARE(switch_call_cause_t) switch_channel_cause_q850(switch_call_cause_t cause);
SWITCH_DECLARE(switch_call_cause_t) switch_channel_get_cause_q850(switch_channel_t *channel);
SWITCH_DECLARE(switch_call_cause_t *) switch_channel_get_cause_ptr(switch_channel_t *channel);

/*!
  \brief return a cause string for a given cause
  \param cause the code to check
  \return the string
*/
SWITCH_DECLARE(const char *) switch_channel_cause2str(_In_ switch_call_cause_t cause);

/*!
  \brief View the timetable of a channel
  \param channel channel to retrieve timetable from
  \return a pointer to the channel's timetable (created, answered, etc..)
*/
SWITCH_DECLARE(switch_channel_timetable_t *) switch_channel_get_timetable(_In_ switch_channel_t *channel);

/*!
  \brief Allocate a new channel
  \param channel NULL pointer to allocate channel to
  \param pool memory_pool to use for allocation
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status_t) switch_channel_alloc(_In_ switch_channel_t **channel, _In_ switch_call_direction_t direction,
													 _In_ switch_memory_pool_t *pool);

/*!
  \brief Connect a newly allocated channel to a session object and setup it's initial state
  \param channel the channel to initilize
  \param session the session to connect the channel to
  \param state the initial state of the channel
  \param flags the initial channel flags
*/
SWITCH_DECLARE(switch_status_t) switch_channel_init(switch_channel_t *channel, switch_core_session_t *session, switch_channel_state_t state,
													switch_channel_flag_t flag);

/*!
  \brief Takes presence_data_cols as a parameter or as a channel variable and copies them to channel profile variables
  \param channel the channel on which to set the channel profile variables
  \param presence_data_cols is a colon separated list of channel variables to copy to channel profile variables
 */
SWITCH_DECLARE(void) switch_channel_set_presence_data_vals(switch_channel_t *channel, const char *presence_data_cols);

/*!
  \brief Fire A presence event for the channel
  \param channel the channel to initilize
  \param rpid the rpid if for the icon to use
  \param status the status message
  \param id presence id
*/
SWITCH_DECLARE(void) switch_channel_perform_presence(switch_channel_t *channel, const char *rpid, const char *status, const char *id,
											 const char *file, const char *func, int line);
#define switch_channel_presence(_a, _b, _c, _d) switch_channel_perform_presence(_a, _b, _c, _d, __FILE__, __SWITCH_FUNC__, __LINE__)
/*!
  \brief Uninitalize a channel
  \param channel the channel to uninit
*/
SWITCH_DECLARE(void) switch_channel_uninit(switch_channel_t *channel);

/*!
  \brief Set the given channel's caller profile
  \param channel channel to assign the profile to
  \param caller_profile the profile to assign
*/
SWITCH_DECLARE(void) switch_channel_set_caller_profile(switch_channel_t *channel, switch_caller_profile_t *caller_profile);
SWITCH_DECLARE(void) switch_channel_step_caller_profile(switch_channel_t *channel);

/*!
  \brief Retrieve the given channel's caller profile
  \param channel channel to retrieve the profile from
  \return the requested profile
*/
SWITCH_DECLARE(switch_caller_profile_t *) switch_channel_get_caller_profile(switch_channel_t *channel);

/*!
  \brief Set the given channel's originator caller profile
  \param channel channel to assign the profile to
  \param caller_profile the profile to assign
*/
SWITCH_DECLARE(void) switch_channel_set_originator_caller_profile(switch_channel_t *channel, switch_caller_profile_t *caller_profile);


SWITCH_DECLARE(void) switch_channel_set_hunt_caller_profile(switch_channel_t *channel, switch_caller_profile_t *caller_profile);

/*!
  \brief Retrieve the given channel's originator caller profile
  \param channel channel to retrieve the profile from
  \return the requested profile
*/
SWITCH_DECLARE(switch_caller_profile_t *) switch_channel_get_originator_caller_profile(switch_channel_t *channel);

/*!
  \brief Set the given channel's originatee caller profile
  \param channel channel to assign the profile to
  \param caller_profile the profile to assign
*/
SWITCH_DECLARE(void) switch_channel_set_originatee_caller_profile(switch_channel_t *channel, switch_caller_profile_t *caller_profile);


/*!
  \brief Retrieve the given channel's originatee caller profile
  \param channel channel to retrieve the profile from
  \return the requested profile
*/
SWITCH_DECLARE(switch_caller_profile_t *) switch_channel_get_originatee_caller_profile(switch_channel_t *channel);

/*!
  \brief Set the given channel's origination caller profile
  \param channel channel to assign the profile to
  \param caller_profile the profile to assign
*/
SWITCH_DECLARE(void) switch_channel_set_origination_caller_profile(switch_channel_t *channel, switch_caller_profile_t *caller_profile);

/*!
  \brief Retrieve the given channel's origination caller profile
  \param channel channel to retrieve the profile from
  \return the requested profile
*/
SWITCH_DECLARE(switch_caller_profile_t *) switch_channel_get_origination_caller_profile(switch_channel_t *channel);


/*!
  \brief Retrieve the given channel's unique id
  \param channel channel to retrieve the unique id from
  \return the unique id
*/
SWITCH_DECLARE(char *) switch_channel_get_uuid(switch_channel_t *channel);

/*!
  \brief Set a variable on a given channel
  \param channel channel to set variable on
  \param varname the name of the variable
  \param value the value of the variable
  \returns SWITCH_STATUS_SUCCESS if successful
*/

SWITCH_DECLARE(switch_status_t) switch_channel_set_profile_var(switch_channel_t *channel, const char *name, const char *val);

SWITCH_DECLARE(switch_status_t) switch_channel_set_log_tag(switch_channel_t *channel, const char *tagname, const char *tagvalue);
SWITCH_DECLARE(switch_status_t) switch_channel_get_log_tags(switch_channel_t *channel, switch_event_t **log_tags);

SWITCH_DECLARE(switch_status_t) switch_channel_set_variable_var_check(switch_channel_t *channel,
																	  const char *varname, const char *value, switch_bool_t var_check);
SWITCH_DECLARE(switch_status_t) switch_channel_add_variable_var_check(switch_channel_t *channel,
																	  const char *varname, const char *value, switch_bool_t var_check, switch_stack_t stack);
SWITCH_DECLARE(switch_status_t) switch_channel_set_variable_printf(switch_channel_t *channel, const char *varname, const char *fmt, ...);
SWITCH_DECLARE(switch_status_t) switch_channel_set_variable_name_printf(switch_channel_t *channel, const char *val, const char *fmt, ...);

SWITCH_DECLARE(switch_status_t) switch_channel_set_variable_partner_var_check(switch_channel_t *channel,
																			  const char *varname, const char *value, switch_bool_t var_check);
SWITCH_DECLARE(const char *) switch_channel_get_variable_partner(switch_channel_t *channel, const char *varname);

SWITCH_DECLARE(const char *) switch_channel_get_hold_music(switch_channel_t *channel);
SWITCH_DECLARE(const char *) switch_channel_get_hold_music_partner(switch_channel_t *channel);

SWITCH_DECLARE(uint32_t) switch_channel_del_variable_prefix(switch_channel_t *channel, const char *prefix);
SWITCH_DECLARE(switch_status_t) switch_channel_transfer_variable_prefix(switch_channel_t *orig_channel, switch_channel_t *new_channel, const char *prefix);

#define switch_channel_set_variable_safe(_channel, _var, _val) switch_channel_set_variable_var_check(_channel, _var, _val, SWITCH_FALSE)
#define switch_channel_set_variable(_channel, _var, _val) switch_channel_set_variable_var_check(_channel, _var, _val, SWITCH_TRUE)
#define switch_channel_set_variable_partner(_channel, _var, _val) switch_channel_set_variable_partner_var_check(_channel, _var, _val, SWITCH_TRUE)


SWITCH_DECLARE(switch_status_t) switch_channel_export_variable_var_check(switch_channel_t *channel,
																		 const char *varname, const char *val,
																		 const char *export_varname,
																		 switch_bool_t var_check);

SWITCH_DECLARE(void) switch_channel_process_export(switch_channel_t *channel, switch_channel_t *peer_channel,
												   switch_event_t *var_event, const char *export_varname);

#define switch_channel_export_variable(_channel, _varname, _value, _ev) switch_channel_export_variable_var_check(_channel, _varname, _value, _ev, SWITCH_TRUE)
SWITCH_DECLARE(switch_status_t) switch_channel_export_variable_printf(switch_channel_t *channel, const char *varname,
																	  const char *export_varname, const char *fmt, ...);

SWITCH_DECLARE(void) switch_channel_set_scope_variables(switch_channel_t *channel, switch_event_t **event);
SWITCH_DECLARE(switch_status_t) switch_channel_get_scope_variables(switch_channel_t *channel, switch_event_t **event);

/*!
  \brief Retrieve a variable from a given channel
  \param channel channel to retrieve variable from
  \param varname the name of the variable
  \return the value of the requested variable
*/
SWITCH_DECLARE(const char *) switch_channel_get_variable_dup(switch_channel_t *channel, const char *varname, switch_bool_t dup, int idx);
#define switch_channel_get_variable(_c, _v) switch_channel_get_variable_dup(_c, _v, SWITCH_TRUE, -1)

SWITCH_DECLARE(switch_status_t) switch_channel_get_variables(switch_channel_t *channel, switch_event_t **event);

SWITCH_DECLARE(switch_status_t) switch_channel_pass_callee_id(switch_channel_t *channel, switch_channel_t *other_channel);

static inline int switch_channel_var_false(switch_channel_t *channel, const char *variable) {
	return switch_false(switch_channel_get_variable_dup(channel, variable, SWITCH_FALSE, -1));
}

static inline int switch_channel_var_true(switch_channel_t *channel, const char *variable) {
	return switch_true(switch_channel_get_variable_dup(channel, variable, SWITCH_FALSE, -1));
}

/*!
 * \brief Start iterating over the entries in the channel variable list.
 * \param channel the channel to iterate the variables for
 * \remark This function locks the profile mutex, use switch_channel_variable_last to unlock
 */
SWITCH_DECLARE(switch_event_header_t *) switch_channel_variable_first(switch_channel_t *channel);

/*!
 * \brief Stop iterating over channel variables.
 * \remark Unlocks the profile mutex initially locked in switch_channel_variable_first
 */
SWITCH_DECLARE(void) switch_channel_variable_last(switch_channel_t *channel);


SWITCH_DECLARE(void) switch_channel_restart(switch_channel_t *channel);

SWITCH_DECLARE(switch_status_t) switch_channel_caller_extension_masquerade(switch_channel_t *orig_channel, switch_channel_t *new_channel, uint32_t offset);

/*!
  \brief Assign a caller extension to a given channel
  \param channel channel to assign extension to
  \param caller_extension extension to assign
*/
SWITCH_DECLARE(void) switch_channel_set_caller_extension(switch_channel_t *channel, switch_caller_extension_t *caller_extension);

SWITCH_DECLARE(void) switch_channel_invert_cid(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_flip_cid(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_sort_cid(switch_channel_t *channel);

/*!
  \brief Retrieve caller extension from a given channel
  \param channel channel to retrieve extension from
  \return the requested extension
*/
SWITCH_DECLARE(switch_caller_extension_t *) switch_channel_get_caller_extension(switch_channel_t *channel);

/*!
  \brief Test for presence of given flag on a given channel
  \param channel channel to test
  \param flag to test
  \return TRUE if flags were present
*/
SWITCH_DECLARE(uint32_t) switch_channel_test_flag(switch_channel_t *channel, switch_channel_flag_t flag);

/*!
  \brief Set given flag(s) on a given channel
  \param channel channel on which to set flag
  \param flag or'd list of flags to set
*/
SWITCH_DECLARE(void) switch_channel_set_flag_value(switch_channel_t *channel, switch_channel_flag_t flag, uint32_t value);
#define switch_channel_set_flag(_c, _f) switch_channel_set_flag_value(_c, _f, 1)

SWITCH_DECLARE(void) switch_channel_set_flag_recursive(switch_channel_t *channel, switch_channel_flag_t flag);

SWITCH_DECLARE(void) switch_channel_set_cap_value(switch_channel_t *channel, switch_channel_cap_t cap, uint32_t value);
#define switch_channel_set_cap(_c, _cc) switch_channel_set_cap_value(_c, _cc, 1)

SWITCH_DECLARE(void) switch_channel_clear_cap(switch_channel_t *channel, switch_channel_cap_t cap);
SWITCH_DECLARE(uint32_t) switch_channel_test_cap(switch_channel_t *channel, switch_channel_cap_t cap);
SWITCH_DECLARE(uint32_t) switch_channel_test_cap_partner(switch_channel_t *channel, switch_channel_cap_t cap);

/*!
  \brief Set given flag(s) on a given channel's bridge partner
  \param channel channel to derive the partner channel to set flag on
  \param flag to set
  \return true if the flag was set
*/
SWITCH_DECLARE(switch_bool_t) switch_channel_set_flag_partner(switch_channel_t *channel, switch_channel_flag_t flag);

/*!
  \brief Clears given flag(s) on a given channel's bridge partner
  \param channel channel to derive the partner channel to clear flag(s) from
  \param flag the flag to clear
  \return true if the flag was cleared
*/
SWITCH_DECLARE(switch_bool_t) switch_channel_clear_flag_partner(switch_channel_t *channel, switch_channel_flag_t flag);

SWITCH_DECLARE(uint32_t) switch_channel_test_flag_partner(switch_channel_t *channel, switch_channel_flag_t flag);

/*!
  \brief Set given flag(s) on a given channel to be applied on the next state change
  \param channel channel on which to set flag(s)
  \param flag flag to set
*/
SWITCH_DECLARE(void) switch_channel_set_state_flag(switch_channel_t *channel, switch_channel_flag_t flag);
SWITCH_DECLARE(void) switch_channel_clear_state_flag(switch_channel_t *channel, switch_channel_flag_t flag);

/*!
  \brief Clear given flag(s) from a channel
  \param channel channel to clear flags from
  \param flag flag to clear
*/
SWITCH_DECLARE(void) switch_channel_clear_flag(switch_channel_t *channel, switch_channel_flag_t flag);

SWITCH_DECLARE(void) switch_channel_clear_flag_recursive(switch_channel_t *channel, switch_channel_flag_t flag);

SWITCH_DECLARE(switch_status_t) switch_channel_perform_answer(switch_channel_t *channel, const char *file, const char *func, int line);

SWITCH_DECLARE(switch_status_t) switch_channel_perform_mark_answered(switch_channel_t *channel, const char *file, const char *func, int line);
SWITCH_DECLARE(void) switch_channel_check_zrtp(switch_channel_t *channel);

/*!
  \brief Answer a channel (initiate/acknowledge a successful connection)
  \param channel channel to answer
  \return SWITCH_STATUS_SUCCESS if channel was answered successfully
*/
#define switch_channel_answer(channel) switch_channel_perform_answer(channel, __FILE__, __SWITCH_FUNC__, __LINE__)

/*!
  \brief Mark a channel answered with no indication (for outbound calls)
  \param channel channel to mark answered
  \return SWITCH_STATUS_SUCCESS if channel was answered successfully
*/
#define switch_channel_mark_answered(channel) switch_channel_perform_mark_answered(channel, __FILE__, __SWITCH_FUNC__, __LINE__)

/*!
  \brief Mark a channel pre_answered (early media) with no indication (for outbound calls)
  \param channel channel to mark pre_answered
  \return SWITCH_STATUS_SUCCESS if channel was pre_answered successfully
*/
#define switch_channel_mark_pre_answered(channel) switch_channel_perform_mark_pre_answered(channel, __FILE__, __SWITCH_FUNC__, __LINE__)

SWITCH_DECLARE(switch_status_t) switch_channel_perform_acknowledge_call(switch_channel_t *channel,
																		const char *file, const char *func, int line);
#define switch_channel_acknowledge_call(channel) switch_channel_perform_acknowledge_call(channel, __FILE__, __SWITCH_FUNC__, __LINE__)

SWITCH_DECLARE(switch_status_t) switch_channel_perform_ring_ready_value(switch_channel_t *channel,
																		switch_ring_ready_t rv,
																		const char *file, const char *func, int line);
/*!
  \brief Send Ringing message to a channel
  \param channel channel to ring
  \return SWITCH_STATUS_SUCCESS if successful
*/
#define switch_channel_ring_ready(channel) switch_channel_perform_ring_ready_value(channel, SWITCH_RING_READY_RINGING, __FILE__, __SWITCH_FUNC__, __LINE__)
#define switch_channel_ring_ready_value(channel, _rv)					\
	switch_channel_perform_ring_ready_value(channel, _rv, __FILE__, __SWITCH_FUNC__, __LINE__)


SWITCH_DECLARE(switch_status_t) switch_channel_perform_pre_answer(switch_channel_t *channel, const char *file, const char *func, int line);

SWITCH_DECLARE(switch_status_t) switch_channel_perform_mark_pre_answered(switch_channel_t *channel, const char *file, const char *func, int line);

SWITCH_DECLARE(switch_status_t) switch_channel_perform_mark_ring_ready_value(switch_channel_t *channel,
																			 switch_ring_ready_t rv,
																			 const char *file, const char *func, int line);

/*!
  \brief Indicate progress on a channel to attempt early media
  \param channel channel to pre-answer
  \return SWITCH_STATUS_SUCCESS
*/
#define switch_channel_pre_answer(channel) switch_channel_perform_pre_answer(channel, __FILE__, __SWITCH_FUNC__, __LINE__)

/*!
  \brief Indicate a channel is ready to provide ringback
  \param channel channel
  \return SWITCH_STATUS_SUCCESS
*/
#define switch_channel_mark_ring_ready(channel) \
	switch_channel_perform_mark_ring_ready_value(channel, SWITCH_RING_READY_RINGING, __FILE__, __SWITCH_FUNC__, __LINE__)

#define switch_channel_mark_ring_ready_value(channel, _rv)					\
	switch_channel_perform_mark_ring_ready_value(channel, _rv, __FILE__, __SWITCH_FUNC__, __LINE__)

/*!
  \brief add a state handler table to a given channel
  \param channel channel on which to add the state handler table
  \param state_handler table of state handler functions
  \return the index number/priority of the table negative value indicates failure
*/
SWITCH_DECLARE(int) switch_channel_add_state_handler(switch_channel_t *channel, const switch_state_handler_table_t *state_handler);

/*!
  \brief clear a state handler table from a given channel
  \param channel channel from which to clear the state handler table
  \param state_handler table of state handler functions
*/
SWITCH_DECLARE(void) switch_channel_clear_state_handler(switch_channel_t *channel, const switch_state_handler_table_t *state_handler);

/*!
  \brief Retrieve an state handler tablefrom a given channel at given index level
  \param channel channel from which to retrieve the state handler table
  \param index the index of the state handler table (start from 0)
  \return given channel's state handler table at given index or NULL if requested index does not exist.
*/
SWITCH_DECLARE(const switch_state_handler_table_t *) switch_channel_get_state_handler(switch_channel_t *channel, int index);

/*!
  \brief Set private data on channel
  \param channel channel on which to set data
  \param key unique keyname to associate your private data to
  \param private_info void pointer to private data
  \return SWITCH_STATUS_SUCCESS if data was set
  \remarks set NULL to delete your private data
*/
SWITCH_DECLARE(switch_status_t) switch_channel_set_private(switch_channel_t *channel, const char *key, const void *private_info);

/*!
  \brief Retrieve private from a given channel
  \param channel channel to retrieve data from
  \param key unique keyname to retrieve your private data
  \return void pointer to channel's private data
*/
SWITCH_DECLARE(void *) switch_channel_get_private(switch_channel_t *channel, const char *key);
SWITCH_DECLARE(void *) switch_channel_get_private_partner(switch_channel_t *channel, const char *key);

/*!
  \brief Assign a name to a given channel
  \param channel channel to assign name to
  \param name name to assign
  \return SWITCH_STATUS_SUCCESS if name was assigned
*/
SWITCH_DECLARE(switch_status_t) switch_channel_set_name(switch_channel_t *channel, const char *name);

/*!
  \brief Retrieve the name of a given channel
  \param channel channel to get name of
  \return the channel's name
*/
SWITCH_DECLARE(char *) switch_channel_get_name(switch_channel_t *channel);


SWITCH_DECLARE(switch_channel_state_t) switch_channel_perform_hangup(switch_channel_t *channel,
																	 const char *file, const char *func, int line, switch_call_cause_t hangup_cause);

/*!
  \brief Hangup a channel flagging it's state machine to end
  \param channel channel to hangup
  \param hangup_cause the appropriate hangup cause
  \return the resulting channel state.
*/
#define switch_channel_hangup(channel, hangup_cause) switch_channel_perform_hangup(channel, __FILE__, __SWITCH_FUNC__, __LINE__, hangup_cause)

/*!
  \brief Test for presence of DTMF on a given channel
  \param channel channel to test
  \return number of digits in the queue
*/
SWITCH_DECLARE(switch_size_t) switch_channel_has_dtmf(_In_ switch_channel_t *channel);
SWITCH_DECLARE(switch_status_t) switch_channel_dtmf_lock(switch_channel_t *channel);
SWITCH_DECLARE(switch_status_t) switch_channel_try_dtmf_lock(switch_channel_t *channel);
SWITCH_DECLARE(switch_status_t) switch_channel_dtmf_unlock(switch_channel_t *channel);


/*!
  \brief Queue DTMF on a given channel
  \param channel channel to queue DTMF to
  \param dtmf digit
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status_t) switch_channel_queue_dtmf(_In_ switch_channel_t *channel, _In_ const switch_dtmf_t *dtmf);
SWITCH_DECLARE(switch_status_t) switch_channel_queue_dtmf_string(_In_ switch_channel_t *channel, _In_ const char *dtmf_string);

/*!
  \brief Retrieve DTMF digits from a given channel
  \param channel channel to retrieve digits from
  \param dtmf digit
  \return number of bytes read into the buffer
*/
SWITCH_DECLARE(switch_status_t) switch_channel_dequeue_dtmf(_In_ switch_channel_t *channel, _In_ switch_dtmf_t *dtmf);
SWITCH_DECLARE(void) switch_channel_flush_dtmf(_In_ switch_channel_t *channel);
SWITCH_DECLARE(switch_size_t) switch_channel_dequeue_dtmf_string(_In_ switch_channel_t *channel, _Out_opt_bytecapcount_(len)
																 char *dtmf_str, _In_ switch_size_t len);

/*!
  \brief Render the name of the provided state enum
  \param state state to get name of
  \return the string representation of the state
*/
SWITCH_DECLARE(const char *) switch_channel_state_name(_In_ switch_channel_state_t state);

/*!
  \brief Render the enum of the provided state name
  \param name the name of the state
  \return the enum value (numeric)
*/
SWITCH_DECLARE(switch_channel_state_t) switch_channel_name_state(_In_ const char *name);

/*!
  \brief Add information about a given channel to an event object
  \param channel channel to add information about
  \param event event to add information to
*/
SWITCH_DECLARE(void) switch_channel_event_set_data(_In_ switch_channel_t *channel, _In_ switch_event_t *event);

SWITCH_DECLARE(void) switch_channel_event_set_basic_data(_In_ switch_channel_t *channel, _In_ switch_event_t *event);
SWITCH_DECLARE(void) switch_channel_event_set_extended_data(_In_ switch_channel_t *channel, _In_ switch_event_t *event);

/*!
  \brief Expand varaibles in a string based on the variables in a paticular channel
  \param channel channel to expand the variables from
  \param in the original string
  \return the original string if no expansion takes place otherwise a new string that must be freed
  \note it's necessary to test if the return val is the same as the input and free the string if it is not.
*/
SWITCH_DECLARE(char *) switch_channel_expand_variables_check(switch_channel_t *channel, const char *in, switch_event_t *var_list, switch_event_t *api_list, uint32_t recur);
#define switch_channel_expand_variables(_channel, _in) switch_channel_expand_variables_check(_channel, _in, NULL, NULL, 0)

#define switch_channel_inbound_display(_channel) ((switch_channel_direction(_channel) == SWITCH_CALL_DIRECTION_INBOUND && !switch_channel_test_flag(_channel, CF_BLEG)) || (switch_channel_direction(_channel) == SWITCH_CALL_DIRECTION_OUTBOUND && switch_channel_test_flag(_channel, CF_DIALPLAN)))

#define switch_channel_outbound_display(_channel) ((switch_channel_direction(_channel) == SWITCH_CALL_DIRECTION_INBOUND && switch_channel_test_flag(_channel, CF_BLEG)) || (switch_channel_direction(_channel) == SWITCH_CALL_DIRECTION_OUTBOUND && !switch_channel_test_flag(_channel, CF_DIALPLAN)))

SWITCH_DECLARE(char *) switch_channel_build_param_string(_In_ switch_channel_t *channel, _In_opt_ switch_caller_profile_t *caller_profile,
														 _In_opt_ const char *prefix);
SWITCH_DECLARE(switch_status_t) switch_channel_set_timestamps(_In_ switch_channel_t *channel);

#define switch_channel_stop_broadcast(_channel)	for(;;) {if (switch_channel_test_flag(_channel, CF_BROADCAST)) {switch_channel_set_flag(_channel, CF_STOP_BROADCAST); switch_channel_set_flag(_channel, CF_BREAK); } break;}

SWITCH_DECLARE(void) switch_channel_perform_audio_sync(switch_channel_t *channel, const char *file, const char *func, int line);
#define switch_channel_audio_sync(_c)  switch_channel_perform_audio_sync(_c, __FILE__, __SWITCH_FUNC__, __LINE__)
SWITCH_DECLARE(void) switch_channel_perform_video_sync(switch_channel_t *channel, const char *file, const char *func, int line);
#define switch_channel_video_sync(_c)  switch_channel_perform_video_sync(_c, __FILE__, __SWITCH_FUNC__, __LINE__)

SWITCH_DECLARE(void) switch_channel_set_private_flag(switch_channel_t *channel, uint32_t flags);
SWITCH_DECLARE(void) switch_channel_clear_private_flag(switch_channel_t *channel, uint32_t flags);
SWITCH_DECLARE(int) switch_channel_test_private_flag(switch_channel_t *channel, uint32_t flags);

SWITCH_DECLARE(void) switch_channel_set_app_flag_key(const char *app, switch_channel_t *channel, uint32_t flags);
SWITCH_DECLARE(void) switch_channel_clear_app_flag_key(const char *app, switch_channel_t *channel, uint32_t flags);
SWITCH_DECLARE(int) switch_channel_test_app_flag_key(const char *app, switch_channel_t *channel, uint32_t flags);

#define switch_channel_set_app_flag(_c, _f) switch_channel_set_app_flag_key(__FILE__, _c, _f)
#define switch_channel_clear_app_flag(_c, _f) switch_channel_clear_app_flag_key(__FILE__, _c, _f)
#define switch_channel_test_app_flag(_c, _f) switch_channel_test_app_flag_key(__FILE__, _c, _f)

SWITCH_DECLARE(void) switch_channel_set_bridge_time(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_set_hangup_time(switch_channel_t *channel);
SWITCH_DECLARE(switch_call_direction_t) switch_channel_direction(switch_channel_t *channel);
SWITCH_DECLARE(switch_call_direction_t) switch_channel_logical_direction(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_set_direction(switch_channel_t *channel, switch_call_direction_t direction);

SWITCH_DECLARE(switch_core_session_t *) switch_channel_get_session(switch_channel_t *channel);
SWITCH_DECLARE(char *) switch_channel_get_flag_string(switch_channel_t *channel);
SWITCH_DECLARE(char *) switch_channel_get_cap_string(switch_channel_t *channel);
SWITCH_DECLARE(int) switch_channel_state_change_pending(switch_channel_t *channel);

SWITCH_DECLARE(void) switch_channel_perform_set_callstate(switch_channel_t *channel, switch_channel_callstate_t callstate,
														  const char *file, const char *func, int line);
#define switch_channel_set_callstate(channel, state) switch_channel_perform_set_callstate(channel, state, __FILE__, __SWITCH_FUNC__, __LINE__)
SWITCH_DECLARE(switch_channel_callstate_t) switch_channel_get_callstate(switch_channel_t *channel);
SWITCH_DECLARE(const char *) switch_channel_callstate2str(switch_channel_callstate_t callstate);
SWITCH_DECLARE(switch_channel_callstate_t) switch_channel_str2callstate(const char *str);
SWITCH_DECLARE(void) switch_channel_mark_hold(switch_channel_t *channel, switch_bool_t on);

/** @} */

SWITCH_DECLARE(switch_status_t) switch_channel_execute_on(switch_channel_t *channel, const char *variable_prefix);
SWITCH_DECLARE(switch_status_t) switch_channel_api_on(switch_channel_t *channel, const char *variable_prefix);
SWITCH_DECLARE(void) switch_channel_process_device_hangup(switch_channel_t *channel);
SWITCH_DECLARE(switch_caller_extension_t *) switch_channel_get_queued_extension(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_transfer_to_extension(switch_channel_t *channel, switch_caller_extension_t *caller_extension);
SWITCH_DECLARE(const char *) switch_channel_get_partner_uuid(switch_channel_t *channel);
SWITCH_DECLARE(const char *) switch_channel_get_partner_uuid_copy(switch_channel_t *channel, char *buf, switch_size_t blen);
SWITCH_DECLARE(switch_hold_record_t *) switch_channel_get_hold_record(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_state_thread_lock(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_state_thread_unlock(switch_channel_t *channel);
SWITCH_DECLARE(switch_status_t) switch_channel_state_thread_trylock(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_handle_cause(switch_channel_t *channel, switch_call_cause_t cause);
SWITCH_DECLARE(void) switch_channel_global_init(switch_memory_pool_t *pool);
SWITCH_DECLARE(void) switch_channel_global_uninit(void);
SWITCH_DECLARE(const char *) switch_channel_set_device_id(switch_channel_t *channel, const char *device_id);
SWITCH_DECLARE(void) switch_channel_clear_device_record(switch_channel_t *channel);
SWITCH_DECLARE(switch_device_record_t *) switch_channel_get_device_record(switch_channel_t *channel);
SWITCH_DECLARE(void) switch_channel_release_device_record(switch_device_record_t **dcdrp);
SWITCH_DECLARE(switch_status_t) switch_channel_bind_device_state_handler(switch_device_state_function_t function, void *user_data);
SWITCH_DECLARE(switch_status_t) switch_channel_unbind_device_state_handler(switch_device_state_function_t function);
SWITCH_DECLARE(const char *) switch_channel_device_state2str(switch_device_state_t device_state);
SWITCH_DECLARE(switch_status_t) switch_channel_pass_sdp(switch_channel_t *from_channel, switch_channel_t *to_channel, const char *sdp);

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
