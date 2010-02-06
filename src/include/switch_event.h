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
 * switch_event.h -- Event System
 *
 */
/*! \file switch_event.h
    \brief Event System
	
	The event system uses a backend thread and an APR threadsafe FIFO queue to accept event objects from various threads
	and allow the backend to take control and deliver the events to registered callbacks.

	The typical usage would be to bind to one or all of the events and use a callback function to react in various ways
	(see the more_xmpp_event_handler or mod_event_test modules for examples).

	Builtin events are fired by the core at various points in the execution of the application and custom events can be 
	reserved and registered so events from an external module can be rendered and handled by an another even handler module.

	If the work time to process an event in a callback is anticipated to grow beyond a very small amount of time it is reccommended
	that you impelment your own handler thread and FIFO queue so you can accept the events int the callback and queue them 
	into your own thread rather than tie up the delivery agent.  It is in to opinion of the author that such a necessity 
	should be judged on a per-use basis and therefore does not fall within the scope of this system to provide that 
	functionality at a core level.

*/

/*!
  \defgroup events Eventing Engine
  \ingroup core1
  \{ 
*/

#ifndef SWITCH_EVENT_H
#define SWITCH_EVENT_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C
/*! \brief An event Header */
	struct switch_event_header {
	/*! the header name */
	char *name;
	/*! the header value */
	char *value;
	/*! hash of the header name */
	unsigned long hash;
	struct switch_event_header *next;
};

/*! \brief Representation of an event */
struct switch_event {
	/*! the event id (descriptor) */
	switch_event_types_t event_id;
	/*! the priority of the event */
	switch_priority_t priority;
	/*! the owner of the event */
	char *owner;
	/*! the subclass of the event */
	char *subclass_name;
	/*! the event headers */
	switch_event_header_t *headers;
	/*! the event headers tail pointer */
	switch_event_header_t *last_header;
	/*! the body of the event */
	char *body;
	/*! user data from the subclass provider */
	void *bind_user_data;
	/*! user data from the event sender */
	void *event_user_data;
	/*! unique key */
	unsigned long key;
	struct switch_event *next;
	int flags;
};

typedef enum {
	EF_UNIQ_HEADERS = (1 << 0)
} switch_event_flag_t;


struct switch_event_node;

#define SWITCH_EVENT_SUBCLASS_ANY NULL

/*!
  \brief Start the eventing system
  \param pool the memory pool to use for the event system (creates a new one if NULL)
  \return SWITCH_STATUS_SUCCESS when complete
*/
SWITCH_DECLARE(switch_status_t) switch_event_init(switch_memory_pool_t *pool);

/*!
  \brief Stop the eventing system
  \return SWITCH_STATUS_SUCCESS when complete
*/
SWITCH_DECLARE(switch_status_t) switch_event_shutdown(void);

/*!
  \brief Create an event
  \param event a NULL pointer on which to create the event
  \param event_id the event id enumeration of the desired event
  \param subclass_name the subclass name for custom event (only valid when event_id is SWITCH_EVENT_CUSTOM)
  \return SWITCH_STATUS_SUCCESS on success
*/
SWITCH_DECLARE(switch_status_t) switch_event_create_subclass_detailed(const char *file, const char *func, int line,
																	  switch_event_t **event, switch_event_types_t event_id, const char *subclass_name);

#define switch_event_create_subclass(_e, _eid, _sn) switch_event_create_subclass_detailed(__FILE__, (const char * )__SWITCH_FUNC__, __LINE__, _e, _eid, _sn)

/*!
  \brief Set the priority of an event
  \param event the event to set the priority on
  \param priority the event priority
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_event_set_priority(switch_event_t *event, switch_priority_t priority);

/*!
  \brief Retrieve a header value from an event
  \param event the event to read the header from
  \param header_name the name of the header to read
  \return the value of the requested header
*/
	 _Ret_opt_z_ SWITCH_DECLARE(char *) switch_event_get_header(switch_event_t *event, const char *header_name);

#define switch_event_get_header_nil(e, h) switch_str_nil(switch_event_get_header(e,h))

/*!
  \brief Retrieve the body value from an event
  \param event the event to read the body from
  \return the value of the body or NULL
*/
SWITCH_DECLARE(char *) switch_event_get_body(switch_event_t *event);

#ifndef SWIG
/*!
  \brief Add a header to an event
  \param event the event to add the header to
  \param stack the stack sense (stack it on the top or on the bottom)
  \param header_name the name of the header to add
  \param fmt the value of the header (varargs see standard sprintf family)
  \return SWITCH_STATUS_SUCCESS if the header was added
*/
SWITCH_DECLARE(switch_status_t) switch_event_add_header(switch_event_t *event, switch_stack_t stack,
														const char *header_name, const char *fmt, ...) PRINTF_FUNCTION(4, 5);
#endif

SWITCH_DECLARE(switch_status_t) switch_event_set_subclass_name(switch_event_t *event, const char *subclass_name);

/*!
  \brief Add a string header to an event
  \param event the event to add the header to
  \param stack the stack sense (stack it on the top or on the bottom)
  \param header_name the name of the header to add
  \param data the value of the header
  \return SWITCH_STATUS_SUCCESS if the header was added
*/
SWITCH_DECLARE(switch_status_t) switch_event_add_header_string(switch_event_t *event, switch_stack_t stack, const char *header_name, const char *data);

SWITCH_DECLARE(switch_status_t) switch_event_del_header_val(switch_event_t *event, const char *header_name, const char *val);
#define switch_event_del_header(_e, _h) switch_event_del_header_val(_e, _h, NULL)

/*!
  \brief Destroy an event
  \param event pointer to the pointer to event to destroy
*/
SWITCH_DECLARE(void) switch_event_destroy(switch_event_t **event);
#define switch_event_safe_destroy(_event) if (_event) switch_event_destroy(_event)

/*!
  \brief Duplicate an event
  \param event a NULL pointer on which to duplicate the event
  \param todup an event to duplicate
  \return SWITCH_STATUS_SUCCESS if the event was duplicated
*/
SWITCH_DECLARE(switch_status_t) switch_event_dup(switch_event_t **event, switch_event_t *todup);

/*!
  \brief Fire an event with full arguement list
  \param file the calling file
  \param func the calling function
  \param line the calling line number
  \param event the event to send (will be nulled on success)
  \param user_data optional private data to pass to the event handlers
  \return
*/
SWITCH_DECLARE(switch_status_t) switch_event_fire_detailed(const char *file, const char *func, int line, switch_event_t **event, void *user_data);

SWITCH_DECLARE(void) switch_event_prep_for_delivery_detailed(const char *file, const char *func, int line, switch_event_t *event);
#define switch_event_prep_for_delivery(_event) switch_event_prep_for_delivery_detailed(__FILE__, (const char * )__SWITCH_FUNC__, __LINE__, _event)


/*!
  \brief Bind an event callback to a specific event
  \param id an identifier token of the binder
  \param event the event enumeration to bind to
  \param subclass_name the event subclass to bind to in the case if SWITCH_EVENT_CUSTOM
  \param callback the callback functon to bind
  \param user_data optional user specific data to pass whenever the callback is invoked
  \return SWITCH_STATUS_SUCCESS if the event was binded
*/
SWITCH_DECLARE(switch_status_t) switch_event_bind(const char *id, switch_event_types_t event, const char *subclass_name, switch_event_callback_t callback,
												  void *user_data);

/*!
  \brief Bind an event callback to a specific event
  \param id an identifier token of the binder
  \param event the event enumeration to bind to
  \param subclass_name the event subclass to bind to in the case if SWITCH_EVENT_CUSTOM
  \param callback the callback functon to bind
  \param user_data optional user specific data to pass whenever the callback is invoked
  \param node bind handle to later remove the binding.
  \return SWITCH_STATUS_SUCCESS if the event was binded
*/
SWITCH_DECLARE(switch_status_t) switch_event_bind_removable(const char *id, switch_event_types_t event, const char *subclass_name,
															switch_event_callback_t callback, void *user_data, switch_event_node_t **node);
/*!
  \brief Unbind a bound event consumer
  \param node node to unbind
  \return SWITCH_STATUS_SUCCESS if the consumer was unbinded
*/
SWITCH_DECLARE(switch_status_t) switch_event_unbind(switch_event_node_t **node);
SWITCH_DECLARE(switch_status_t) switch_event_unbind_callback(switch_event_callback_t callback);

/*!
  \brief Render the name of an event id enumeration
  \param event the event id to render the name of
  \return the rendered name
*/
SWITCH_DECLARE(const char *) switch_event_name(switch_event_types_t event);

/*!
  \brief return the event id that matches a given event name
  \param name the name of the event
  \param type the event id to return
  \return SWITCH_STATUS_SUCCESS if there was a match
*/
SWITCH_DECLARE(switch_status_t) switch_name_event(const char *name, switch_event_types_t *type);

/*!
  \brief Reserve a subclass name for private use with a custom event
  \param owner the owner of the event name
  \param subclass_name the name to reserve
  \return SWITCH_STATUS_SUCCESS if the name was reserved
  \note There is nothing to enforce this but I reccommend using module::event_name for the subclass names

*/
SWITCH_DECLARE(switch_status_t) switch_event_reserve_subclass_detailed(const char *owner, const char *subclass_name);

SWITCH_DECLARE(switch_status_t) switch_event_free_subclass_detailed(const char *owner, const char *subclass_name);

/*!
  \brief Render a string representation of an event sutable for printing or network transport 
  \param event the event to render
  \param str a string pointer to point at the allocated data
  \param encode url encode the headers
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note you must free the resulting string when you are finished with it
*/
SWITCH_DECLARE(switch_status_t) switch_event_serialize(switch_event_t *event, char **str, switch_bool_t encode);

#ifndef SWIG
/*!
  \brief Render a XML representation of an event sutable for printing or network transport
  \param event the event to render
  \param fmt optional body of the event (varargs see standard sprintf family)
  \return the xml object if the operation was successful
  \note the body supplied by this function will supersede an existing body the event may have
*/
SWITCH_DECLARE(switch_xml_t) switch_event_xmlize(switch_event_t *event, const char *fmt, ...) PRINTF_FUNCTION(2, 3);
#endif

/*!
  \brief Determine if the event system has been initilized
  \return SWITCH_STATUS_SUCCESS if the system is running
*/
SWITCH_DECLARE(switch_status_t) switch_event_running(void);

#ifndef SWIG
/*!
  \brief Add a body to an event
  \param event the event to add to body to
  \param fmt optional body of the event (varargs see standard sprintf family)
  \return SWITCH_STATUS_SUCCESS if the body was added to the event
  \note the body parameter can be shadowed by the switch_event_reserve_subclass_detailed function
*/
SWITCH_DECLARE(switch_status_t) switch_event_add_body(switch_event_t *event, const char *fmt, ...) PRINTF_FUNCTION(2, 3);
#endif
SWITCH_DECLARE(char *) switch_event_expand_headers(switch_event_t *event, const char *in);

SWITCH_DECLARE(switch_status_t) switch_event_create_pres_in_detailed(_In_z_ char *file, _In_z_ char *func, _In_ int line,
																	 _In_z_ const char *proto, _In_z_ const char *login,
																	 _In_z_ const char *from, _In_z_ const char *from_domain,
																	 _In_z_ const char *status, _In_z_ const char *event_type,
																	 _In_z_ const char *alt_event_type, _In_ int event_count,
																	 _In_z_ const char *unique_id, _In_z_ const char *channel_state,
																	 _In_z_ const char *answer_state, _In_z_ const char *call_direction);
#define switch_event_create_pres_in(event) switch_event_create_pres_in_detailed(__FILE__, (const char * )__SWITCH_FUNC__, __LINE__, \
											proto, login, from, from_domain, status, event_type, alt_event_type, event_count, \
											unique_id, channel_state, answer_state, call_direction)


/*!
  \brief Reserve a subclass assuming the owner string is the current filename
  \param subclass_name the subclass name to reserve
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note the body supplied by this function will supersede an existing body the event may have
*/
#define switch_event_reserve_subclass(subclass_name) switch_event_reserve_subclass_detailed(__FILE__, subclass_name)
#define switch_event_free_subclass(subclass_name) switch_event_free_subclass_detailed(__FILE__, subclass_name)

/*!
  \brief Create a new event assuming it will not be custom event and therefore hiding the unused parameters
  \param event a NULL pointer on which to create the event
  \param id the event id enumeration of the desired event
  \return SWITCH_STATUS_SUCCESS on success
*/
#define switch_event_create(event, id) switch_event_create_subclass(event, id, SWITCH_EVENT_SUBCLASS_ANY)

	 static inline switch_status_t switch_event_create_plain(switch_event_t **event, switch_event_types_t event_id)
{
	switch_status_t status = switch_event_create(event, SWITCH_EVENT_CLONE);
	if (status == SWITCH_STATUS_SUCCESS) {
		(*event)->event_id = event_id;
	}

	return status;
}

/*!
  \brief Deliver an event to all of the registered event listeners
  \param event the event to send (will be nulled)
  \note normaly use switch_event_fire for delivering events (only use this when you wish to deliver the event blocking on your thread)
*/
SWITCH_DECLARE(void) switch_event_deliver(switch_event_t **event);

/*!
  \brief Fire an event filling in most of the arguements with obvious values
  \param event the event to send (will be nulled on success)
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note the body supplied by this function will supersede an existing body the event may have
*/
#define switch_event_fire(event) switch_event_fire_detailed(__FILE__, (const char * )__SWITCH_FUNC__, __LINE__, event, NULL)

/*!
  \brief Fire an event filling in most of the arguements with obvious values and allowing user_data to be sent
  \param event the event to send (will be nulled on success)
  \param data user data to send to the event handlers
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note the body supplied by this function will supersede an existing body the event may have
*/
#define switch_event_fire_data(event, data) switch_event_fire_detailed(__FILE__, (const char * )__SWITCH_FUNC__, __LINE__, event, data)

SWITCH_DECLARE(char *) switch_event_build_param_string(switch_event_t *event, const char *prefix, switch_hash_t *vars_map);

///\}

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
