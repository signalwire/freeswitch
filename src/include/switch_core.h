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
 * switch_core.h -- Core Library
 *
 */
/*! \file switch_core.h
  \brief Core Library

  This module is the main core library and is the intended location of all fundamental operations.
*/

#ifndef SWITCH_CORE_H
#define SWITCH_CORE_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

#define SWITCH_MAX_CORE_THREAD_SESSION_OBJS 128
#define SWITCH_MAX_STREAMS 128

struct switch_core_time_duration {
    uint32_t mms;
    uint32_t ms;
    uint32_t sec;
    uint32_t min;
    uint32_t hr;
	uint32_t day;
	uint32_t yr;
};

/*! \brief A message object designed to allow unlike technologies to exchange data */
struct switch_core_session_message {
	/*! uuid of the sender (for replies)*/
	char *from;
	/*! enumeration of the type of message */
	switch_core_session_message_types_t message_id;

	/*! optional numeric arg*/
	int numeric_arg;
	/*! optional string arg*/
	char *string_arg;
	/*! optional string arg*/
	switch_size_t string_arg_size;
	/*! optional arbitrary pointer arg */
	void *pointer_arg;
	/*! optional arbitrary pointer arg's size */
	switch_size_t pointer_arg_size;

	/*! optional numeric reply*/
	int numeric_reply;
	/*! optional string reply*/
	char *string_reply;
	/*! optional string reply*/
	switch_size_t string_reply_size;
	/*! optional arbitrary pointer reply */
	void *pointer_reply;
	/*! optional arbitrary pointer reply's size */
	switch_size_t pointer_reply_size;
	/*! message flags */
	switch_core_session_message_flag_t flags;
};

/*! \brief A generic object to pass as a thread's session object to allow mutiple arguements and a pool */
struct switch_core_thread_session {
	/*! status of the thread */
	int running;
	/*! mutex */
	switch_mutex_t *mutex;
	/*! array of void pointers to pass mutiple data objects */
	void *objs[SWITCH_MAX_CORE_THREAD_SESSION_OBJS];
	/*! a pointer to a memory pool if the thread has it's own pool */
	switch_memory_pool_t *pool;
};

struct switch_core_session;
struct switch_core_runtime;
struct switch_core_port_allocator;

/*!
  \defgroup core1 Core Library 
  \ingroup FREESWITCH
  \{ 
*/


///\defgroup mb1 Media Bugs
///\ingroup core1
///\{

/*!
  \brief Add a media bug to the session
  \param session the session to add the bug to
  \param callback a callback for events
  \param user_data arbitrary user data
  \param flags flags to choose the stream
  \param new_bug pointer to assign new bug to
  \return SWITCH_STATUS_SUCCESS if the operation was a success
*/
SWITCH_DECLARE(switch_status_t) switch_core_media_bug_add(switch_core_session_t *session,
														  switch_media_bug_callback_t callback,
														  void *user_data,
														  switch_media_bug_flag_t flags,
														  switch_media_bug_t **new_bug);
/*!
  \brief Obtain private data from a media bug
  \param bug the bug to get the data from
  \return the private data
*/
SWITCH_DECLARE(void *) switch_core_media_bug_get_user_data(switch_media_bug_t *bug);

/*!
  \brief Remove a media bug from the session
  \param session the session to remove the bug from
  \param bug bug to remove
  \return SWITCH_STATUS_SUCCESS if the operation was a success
*/
SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove(switch_core_session_t *session, switch_media_bug_t **bug);

/*!
  \brief Remove all media bugs from the session
  \param session the session to remove the bugs from
  \return SWITCH_STATUS_SUCCESS if the operation was a success
*/
SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove_all(switch_core_session_t *session);

/*!
  \brief Read a frame from the bug
  \param bug the bug to read from
  \param frame the frame to write the data to
  \return the amount of data 
*/
SWITCH_DECLARE(switch_status_t) switch_core_media_bug_read(switch_media_bug_t *bug, switch_frame_t *frame);

///\}

///\defgroup pa1 Port Allocation
///\ingroup core1
///\{

/*!
  \brief Initilize the port allocator
  \param start the starting port
  \param end the ending port
  \param inc the amount to increment each port
  \param new_allocator new pointer for the return value
  \return SWITCH_STATUS_SUCCESS if the operation was a success
*/
SWITCH_DECLARE(switch_status_t) switch_core_port_allocator_new(switch_port_t start,
															   switch_port_t end,
															   uint8_t inc,
															   switch_core_port_allocator_t **new_allocator);

/*!
  \brief Get a port from the port allocator
  \param alloc the allocator object
  \return the port
*/
SWITCH_DECLARE(switch_port_t) switch_core_port_allocator_request_port(switch_core_port_allocator_t *alloc);

/*!
  \brief destroythe port allocator
  \param alloc the allocator object
*/
SWITCH_DECLARE(void) switch_core_port_allocator_destroy(switch_core_port_allocator_t **alloc);
///\}

///\defgroup ss Startup/Shutdown
///\ingroup core1
///\{
/*! 
  \brief Initilize the core
  \param console optional FILE stream for output
  \param err a pointer to set any errors to
  \note to be called at application startup
*/
SWITCH_DECLARE(switch_status_t) switch_core_init(char *console, const char **err);

/*! 
  \brief Initilize the core and load modules
  \param console optional FILE stream for output
  \param err a pointer to set any errors to
  \note to be called at application startup instead of switch_core_init.  Includes module loading.
*/
SWITCH_DECLARE(switch_status_t) switch_core_init_and_modload(char *console, const char **err);

/*! 
  \brief Set/Get Session Limit
  \param new_limit new value (if > 0)
  \return the current session limit
*/
SWITCH_DECLARE(uint32_t) switch_core_session_limit(uint32_t new_limit);

/*! 
  \brief Destroy the core
  \param vg nonzero to skip core uninitilize for memory debugging
  \note to be called at application shutdown
*/
SWITCH_DECLARE(switch_status_t) switch_core_destroy(int vg);
///\}


///\defgroup rwl Read/Write Locking
///\ingroup core1
///\{
/*! 
  \brief Acquire a read lock on the session
  \param session the session to acquire from
  \return success if it is safe to read from the session
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_read_lock(switch_core_session_t *session);

/*! 
  \brief Acquire a write lock on the session
  \param session the session to acquire from
*/
SWITCH_DECLARE(void) switch_core_session_write_lock(switch_core_session_t *session);

/*! 
  \brief Unlock a read or write lock on as given session
  \param session the session
*/
SWITCH_DECLARE(void) switch_core_session_rwunlock(switch_core_session_t *session);
///\}

///\defgroup sh State Handlers
///\ingroup core1
///\{
/*! 
  \brief Add a global state handler
  \param state_handler a state handler to add
  \return the current index/priority of this handler
*/
SWITCH_DECLARE(int) switch_core_add_state_handler(const switch_state_handler_table_t *state_handler);

/*! 
  \brief Access a state handler
  \param index the desired index to access
  \return the desired state handler table or NULL when it does not exist.
*/
SWITCH_DECLARE(const switch_state_handler_table_t *) switch_core_get_state_handler(int index);
///\}


///\defgroup memp Memory Pooling/Allocation
///\ingroup core1
///\{
/*! 
  \brief Create a new sub memory pool from the core's master pool
  \return SWITCH_STATUS_SUCCESS on success
*/
SWITCH_DECLARE(switch_status_t) switch_core_new_memory_pool(switch_memory_pool_t **pool);

/*! 
  \brief Returns a subpool back to the main pool
  \return SWITCH_STATUS_SUCCESS on success
*/
SWITCH_DECLARE(switch_status_t) switch_core_destroy_memory_pool(switch_memory_pool_t **pool);

/*! 
  \brief Start the session's state machine
  \param session the session on which to start the state machine
*/
SWITCH_DECLARE(void) switch_core_session_run(switch_core_session_t *session);

/*! 
  \brief determine if the session's state machine is running
  \param session the session on which to check
*/
SWITCH_DECLARE(unsigned int) switch_core_session_running(switch_core_session_t *session);

/*! 
  \brief Allocate memory from the main pool with no intention of returning it
  \param memory the number of bytes to allocate
  \return a void pointer to the allocated memory
  \note this memory never goes out of scope until the core is destroyed
*/
SWITCH_DECLARE(void *) switch_core_permanent_alloc(switch_size_t memory);

/*! 
  \brief Allocate memory directly from a memory pool
  \param pool the memory pool to allocate from
  \param memory the number of bytes to allocate
  \return a void pointer to the allocated memory
*/
SWITCH_DECLARE(void *) switch_core_alloc(switch_memory_pool_t *pool, switch_size_t memory);

/*! 
  \brief Allocate memory from a session's pool
  \param session the session to request memory from
  \param memory the amount of memory to allocate
  \return a void pointer to the newly allocated memory
  \note the memory will be in scope as long as the session exists
*/
SWITCH_DECLARE(void *) switch_core_session_alloc(switch_core_session_t *session, switch_size_t memory);

/*! 
  \brief Copy a string using permanent memory allocation
  \param todup the string to duplicate
  \return a pointer to the newly duplicated string
*/
SWITCH_DECLARE(char *) switch_core_permanent_strdup(char *todup);

/*! 
  \brief Copy a string using memory allocation from a session's pool
  \param session a session to use for allocation
  \param todup the string to duplicate
  \return a pointer to the newly duplicated string
*/
SWITCH_DECLARE(char *) switch_core_session_strdup(switch_core_session_t *session, char *todup);

/*! 
  \brief Copy a string using memory allocation from a given pool
  \param pool the pool to use for allocation
  \param todup the string to duplicate
  \return a pointer to the newly duplicated string
*/
SWITCH_DECLARE(char *) switch_core_strdup(switch_memory_pool_t *pool, char *todup);

/*! 
  \brief Retrieve the memory pool from a session
  \param session the session to retrieve the pool from
  \return the session's pool
  \note to be used sparingly
*/
SWITCH_DECLARE(switch_memory_pool_t *) switch_core_session_get_pool(switch_core_session_t *session);
///\}

///\defgroup sessm Session Creation / Management
///\ingroup core1
///\{
/*! 
  \brief Allocate and return a new session from the core
  \param endpoint_interface the endpoint interface the session is to be based on
  \param pool the pool to use for the allocation (a new one will be used if NULL)
  \return the newly created session
*/
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request(const switch_endpoint_interface_t *endpoint_interface, switch_memory_pool_t *pool);

/*! 
  \brief Destroy a session and return the memory pool to the core
  \param session pointer to a pointer of the session to destroy
  \return
*/
SWITCH_DECLARE(void) switch_core_session_destroy(switch_core_session_t **session);

/*! 
  \brief Provide the total number of sessions
  \return the total number of allocated sessions
*/
SWITCH_DECLARE(uint32_t) switch_core_session_count(void);

/*! 
  \brief Allocate and return a new session from the core based on a given endpoint module name
  \param endpoint_name the name of the endpoint module
  \param pool the pool to use
  \return the newly created session
*/
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request_by_name(char *endpoint_name, switch_memory_pool_t *pool);

/*! 
  \brief Launch the session thread (state machine) on a given session
  \param session the session to activate the state machine on
*/
SWITCH_DECLARE(void) switch_core_session_thread_launch(switch_core_session_t *session);

/*! 
  \brief Retrieve a pointer to the channel object associated with a given session
  \param session the session to retrieve from
  \return a pointer to the channel object
*/
SWITCH_DECLARE(switch_channel_t *) switch_core_session_get_channel(switch_core_session_t *session);

/*! 
  \brief Signal a session's state machine thread that a state change has occured
*/
SWITCH_DECLARE(void) switch_core_session_signal_state_change(switch_core_session_t *session);

/*! 
  \brief Retrieve the unique identifier from a session
  \param session the session to retrieve the uuid from
  \return a string representing the uuid
*/
SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session_t *session);

/*! 
  \brief Locate a session based on it's uuiid
  \param uuid_str the unique id of the session you want to find
  \return the session or NULL
  \note if the session was located it will have a read lock obtained which will need to be released with switch_core_session_rwunlock()
*/
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_locate(char *uuid_str);

/*! 
  \brief Retrieve a global variable from the core
  \param varname the name of the variable
  \return the value of the desired variable
*/
SWITCH_DECLARE(char *) switch_core_get_variable(char *varname);

/*! 
  \brief Hangup All Sessions
  \param cause the hangup cause to apply to the hungup channels
*/
SWITCH_DECLARE(void) switch_core_session_hupall(switch_call_cause_t cause);

/*! 
  \brief Send a message to another session using it's uuid
  \param uuid_str the unique id of the session you want to send a message to
  \param message the switch_core_session_message_t object to send
  \return the status returned by the message handler
*/
SWITCH_DECLARE (switch_status_t) switch_core_session_message_send(char *uuid_str, switch_core_session_message_t *message);

/*! 
  \brief Queue a message on a session
  \param session the session to queue the message to
  \param message the message to queue
  \return SWITCH_STATUS_SUCCESS if the message was queued
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_queue_message(switch_core_session_t *session, switch_core_session_message_t *message);

/*! 
  \brief DE-Queue an message on a given session
  \param session the session to de-queue the message on
  \param message the de-queued message
  \return the  SWITCH_STATUS_SUCCESS if the message was de-queued
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_message(switch_core_session_t *session, switch_core_session_message_t **message);

/*! 
  \brief Queue an event on another session using its uuid
  \param uuid_str the unique id of the session you want to send a message to
  \param event the event to send
  \return the status returned by the message handler
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_event_send(char *uuid_str, switch_event_t **event);

/*! 
  \brief Send an event to a session translating it to it's native message format
  \param session the session to receive the event
  \param event the event to receive
  \return the status returned by the handler
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_receive_event(switch_core_session_t *session, switch_event_t **event);

/*! 
  \brief Retrieve private user data from a session
  \param session the session to retrieve from
  \return a pointer to the private data
*/
SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session_t *session);

/*! 
  \brief Add private user data to a session
  \param session the session to add used data to
  \param private_info the used data to add
  \return SWITCH_STATUS_SUCCESS if data is added
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_set_private(switch_core_session_t *session, void *private_info);

/*!
  \brief Add a logical stream to a session
  \param session the session to add the stream to
  \param private_info an optional pointer to private data for the new stream
  \return the stream id of the new stream
*/
SWITCH_DECLARE(int) switch_core_session_add_stream(switch_core_session_t *session, void *private_info);

/*!
  \brief Retreive a logical stream from a session
  \param session the session to add the stream to
  \param index the index to retrieve
  \return the stream
*/
SWITCH_DECLARE(void *) switch_core_session_get_stream(switch_core_session_t *session, int index);

/*!
  \brief Determine the number of logical streams a session has
  \param session the session to query
  \return the total number of logical streams
*/
SWITCH_DECLARE(int) switch_core_session_get_stream_count(switch_core_session_t *session);

/*! 
  \brief Launch a thread designed to exist within the scope of a given session
  \param session a session to allocate the thread from
  \param func a function to execute in the thread
  \param obj an arguement
*/
SWITCH_DECLARE(void) switch_core_session_launch_thread(switch_core_session_t *session, void *(*func)(switch_thread_t *, void *), void *obj);

/*! 
  \brief Signal a thread using a thread session to terminate
  \param thread_session the thread_session to indicate to
*/
SWITCH_DECLARE(void) switch_core_thread_session_end(switch_core_thread_session_t *thread_session);

/*! 
  \brief Launch a service thread on a session to drop inbound data
  \param session the session the launch thread on
  \param stream_id which logical media channel to use
  \param thread_session the thread_session to use
*/
SWITCH_DECLARE(void) switch_core_service_session(switch_core_session_t *session, switch_core_thread_session_t *thread_session, int stream_id);

/*! 
  \brief Request an outgoing session spawned from an existing session using a desired endpoing module
  \param session the originating session
  \param endpoint_name the name of the module to use for the new session
  \param caller_profile the originator's caller profile
  \param new_session a NULL pointer to aim at the newly created session
  \param pool optional existing memory pool to donate to the session
  \return SWITCH_STATUS_SUCCESS if the session was created
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_outgoing_channel(switch_core_session_t *session,
																   char *endpoint_name,
																   switch_caller_profile_t *caller_profile,
																   switch_core_session_t **new_session,
																   switch_memory_pool_t *pool);

/*! 
  \brief Answer the channel of a given session
  \param session the session to answer the channel of
  \return SWITCH_STATUS_SUCCESS if the channel was answered
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_answer_channel(switch_core_session_t *session);

/*! 
  \brief Receive a message on a given session
  \param session the session to receive the message from
  \param message the message to recieve
  \return the status returned by the message handler
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_receive_message(switch_core_session_t *session, switch_core_session_message_t *message);

/*! 
  \brief Queue an event on a given session
  \param session the session to queue the message on
  \param event the event to queue
  \return the status returned by the message handler
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_queue_event(switch_core_session_t *session, switch_event_t **event);


/*! 
  \brief Indicate the number of waiting events on a session
  \param session the session to check
  \return the number of events
*/
SWITCH_DECLARE(int32_t) switch_core_session_event_count(switch_core_session_t *session);

/*! 
  \brief DE-Queue an event on a given session
  \param session the session to de-queue the message on
  \param event the de-queued event
  \return the  SWITCH_STATUS_SUCCESS if the event was de-queued
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_event(switch_core_session_t *session, switch_event_t **event);

/*! 
  \brief Queue a private event on a given session
  \param session the session to queue the message on
  \param event the event to queue
  \return the status returned by the message handler
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_queue_private_event(switch_core_session_t *session, switch_event_t **event);


/*! 
  \brief Indicate the number of waiting private events on a session
  \param session the session to check
  \return the number of events
*/
SWITCH_DECLARE(int32_t) switch_core_session_private_event_count(switch_core_session_t *session);

/*! 
  \brief DE-Queue a private event on a given session
  \param session the session to de-queue the message on
  \param event the de-queued event
  \return the  SWITCH_STATUS_SUCCESS if the event was de-queued
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_private_event(switch_core_session_t *session, switch_event_t **event);


/*! 
  \brief Read a frame from a session
  \param session the session to read from
  \param frame a NULL pointer to a frame to aim at the newly read frame
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS a the frame was read
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, int stream_id);

/*! 
  \brief Reset the buffers and resampler on a session
  \param session the session to reset
*/
SWITCH_DECLARE(void) switch_core_session_reset(switch_core_session_t *session);

/*! 
  \brief Write a frame to a session
  \param session the session to write to
  \param frame the frame to write
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS a the frame was written
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, int stream_id);


SWITCH_DECLARE(switch_status_t) switch_core_session_perform_kill_channel(switch_core_session_t *session, 
																	   const char *file, 
																	   const char *func, 
																	   int line, 
																	   switch_signal_t sig);
/*! 
  \brief Send a signal to a channel
  \param session session to send signal to
  \param sig signal to send
  \return status returned by the session's signal handler
*/
#define switch_core_session_kill_channel(session, sig) switch_core_session_perform_kill_channel(session, __FILE__, __FUNCTION__, __LINE__, sig)

/*! 
  \brief Wait for a session to be ready for input
  \param session session to wait for
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS if data is available for read within timeframe specified
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_waitfor_read(switch_core_session_t *session, int timeout, int stream_id);

/*! 
  \brief Wait for a session to be ready for output
  \param session session to wait for
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS if the session is available for write within timeframe specified
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_waitfor_write(switch_core_session_t *session, int timeout, int stream_id);

/*! 
  \brief Send DTMF to a session
  \param session session to send DTMF to
  \param dtmf string to send to the session
  \return SWITCH_STATUS_SUCCESS if the dtmf was written
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_send_dtmf(switch_core_session_t *session, char *dtmf);

/*! 
  \brief Add an event hook to be executed when a session requests an outgoing extension
  \param session session to bind hook to
  \param outgoing_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_outgoing(switch_core_session_t *session, switch_outgoing_channel_hook_t outgoing_channel);
///\}

///\defgroup shooks Session Hook Callbacks
///\ingroup core1
///\{
/*! 
  \brief Add an event hook to be executed when a session answers a channel
  \param session session to bind hook to
  \param answer_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_answer_channel(switch_core_session_t *session, switch_answer_channel_hook_t answer_channel);

/*! 
  \brief Add an event hook to be executed when a session sends a message
  \param session session to bind hook to
  \param receive_message hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_receive_message(switch_core_session_t *session, switch_receive_message_hook_t receive_message);

/*! 
  \brief Add an event hook to be executed when a session reads a frame
  \param session session to bind hook to
  \param  read_frame hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_read_frame(switch_core_session_t *session, switch_read_frame_hook_t read_frame);

/*! 
  \brief Add an event hook to be executed when a session writes a frame
  \param session session to bind hook to
  \param write_frame hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_write_frame(switch_core_session_t *session, switch_write_frame_hook_t write_frame);

/*! 
  \brief Add an event hook to be executed when a session kills a channel
  \param session session to bind hook to
  \param kill_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_kill_channel(switch_core_session_t *session, switch_kill_channel_hook_t kill_channel);

/*! 
  \brief Add an event hook to be executed when a session waits for a read event
  \param session session to bind hook to
  \param waitfor_read hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_waitfor_read(switch_core_session_t *session, switch_waitfor_read_hook_t waitfor_read);

/*! 
  \brief Add an event hook to be executed when a session waits for a write event
  \param session session to bind hook to
  \param waitfor_write hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_waitfor_write(switch_core_session_t *session, switch_waitfor_write_hook_t waitfor_write);

/*! 
  \brief Add an event hook to be executed when a session sends dtmf
  \param session session to bind hook to
  \param send_dtmf hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_send_dtmf(switch_core_session_t *session, switch_send_dtmf_hook_t send_dtmf);
///\}

///\defgroup hashf Hash Functions
///\ingroup core1
///\{
/*! 
  \brief Initilize a hash table
  \param hash a NULL pointer to a hash table to aim at the new hash
  \param pool the pool to use for the new hash
  \return SWITCH_STATUS_SUCCESS if the hash is created
*/
SWITCH_DECLARE(switch_status_t) switch_core_hash_init(switch_hash_t **hash, switch_memory_pool_t *pool);

/*! 
  \brief Destroy an existing hash table
  \param hash the hash to destroy
  \return SWITCH_STATUS_SUCCESS if the hash is destroyed
*/
SWITCH_DECLARE(switch_status_t) switch_core_hash_destroy(switch_hash_t *hash);

/*! 
  \brief Insert data into a hash
  \param hash the hash to add data to
  \param key the name of the key to add the data to
  \param data the data to add
  \return SWITCH_STATUS_SUCCESS if the data is added
  \note the string key must be a constant or a dynamic string
*/
SWITCH_DECLARE(switch_status_t) switch_core_hash_insert(switch_hash_t *hash, char *key, void *data);

/*! 
  \brief Insert data into a hash with dynamicly allocated key name
  \param hash the hash to add data to
  \param key the name of the key to add the data to
  \param data the data to add
  \return SWITCH_STATUS_SUCCESS if the data is added
*/
SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_dup(switch_hash_t *hash, char *key, void *data);

/*! 
  \brief Delete data from a hash based on desired key
  \param hash the hash to delete from
  \param key the key from which to delete the data
  \return SWITCH_STATUS_SUCCESS if the data is deleted
*/
SWITCH_DECLARE(switch_status_t) switch_core_hash_delete(switch_hash_t *hash, char *key);

/*! 
  \brief Retrieve data from a given hash
  \param hash the hash to retrieve from
  \param key the key to retrieve
  \return a pointer to the data held in the key
*/
SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash_t *hash, char *key);
///\}

///\defgroup timer Timer Functions
///\ingroup core1
///\{
/*! 
  \brief Request a timer handle using given time module
  \param timer a timer object to allocate to
  \param timer_name the name of the timer module to use
  \param interval desired interval
  \param samples the number of samples to increment on each cycle
  \param pool the memory pool to use for allocation
  \return
*/
SWITCH_DECLARE(switch_status_t) switch_core_timer_init(switch_timer_t *timer, char *timer_name, int interval, int samples, switch_memory_pool_t *pool);

/*! 
  \brief Wait for one cycle on an existing timer
  \param timer the timer to wait on
  \return the newest sample count
*/
SWITCH_DECLARE(int) switch_core_timer_next(switch_timer_t *timer);

/*! 
  \brief Step the timer one step
  \param timer the timer to wait on
  \return the newest sample count
*/
SWITCH_DECLARE(switch_status_t) switch_core_timer_step(switch_timer_t *timer);

/*! 
  \brief Check if the current step has been exceeded
  \param timer the timer to wait on
  \return the newest sample count
*/
SWITCH_DECLARE(switch_status_t) switch_core_timer_check(switch_timer_t *timer);

/*! 
  \brief Destroy an allocated timer
  \param timer timer to destroy
  \return SWITCH_STATUS_SUCCESS after destruction
*/
SWITCH_DECLARE(switch_status_t) switch_core_timer_destroy(switch_timer_t *timer);
///\}

///\defgroup codecs Codec Functions
///\ingroup core1
///\{
/*! 
  \brief Initialize a codec handle
  \param codec the handle to initilize
  \param codec_name the name of the codec module to use
  \param fmtp codec parameters to send
  \param rate the desired rate (0 for any)
  \param ms the desired number of milliseconds (0 for any)
  \param channels the desired number of channels (0 for any)
  \param flags flags to alter behaviour
  \param codec_settings desired codec settings
  \param pool the memory pool to use
  \return SWITCH_STATUS_SUCCESS if the handle is allocated
*/
SWITCH_DECLARE(switch_status_t) switch_core_codec_init(switch_codec_t *codec, 
													   char *codec_name, 
													   char *fmtp,
													   uint32_t rate, 
													   int ms, 
													   int channels, 
													   uint32_t flags,
													   const switch_codec_settings_t *codec_settings, 
													   switch_memory_pool_t *pool);

/*! 
  \brief Encode data using a codec handle
  \param codec the codec handle to use
  \param other_codec the codec handle of the last codec used
  \param decoded_data the raw data
  \param decoded_data_len then length of the raw buffer
  \param decoded_rate the rate of the decoded data
  \param encoded_data the buffer to write the encoded data to
  \param encoded_data_len the size of the encoded_data buffer
  \param encoded_rate the new rate of the encoded data
  \param flag flags to exchange
  \return SWITCH_STATUS_SUCCESS if the data was encoded
  \note encoded_data_len will be rewritten to the in-use size of encoded_data
*/
SWITCH_DECLARE(switch_status_t) switch_core_codec_encode(switch_codec_t *codec,
													   switch_codec_t *other_codec,
													   void *decoded_data,
													   uint32_t decoded_data_len,
													   uint32_t decoded_rate,
													   void *encoded_data,
													   uint32_t *encoded_data_len,
													   uint32_t *encoded_rate,
													   unsigned int *flag);

/*! 
  \brief Decode data using a codec handle
  \param codec the codec handle to use
  \param other_codec the codec handle of the last codec used
  \param encoded_data the buffer to read the encoded data from
  \param encoded_data_len the size of the encoded_data buffer
  \param encoded_rate the rate of the encoded data
  \param decoded_data the raw data buffer
  \param decoded_data_len then length of the raw buffer
  \param decoded_rate the new rate of the decoded data
  \param flag flags to exchange
  \return SWITCH_STATUS_SUCCESS if the data was decoded
  \note decoded_data_len will be rewritten to the in-use size of decoded_data
*/
SWITCH_DECLARE(switch_status_t) switch_core_codec_decode(switch_codec_t *codec,
													   switch_codec_t *other_codec,
													   void *encoded_data,
													   uint32_t encoded_data_len,
													   uint32_t encoded_rate,
													   void *decoded_data,
													   uint32_t *decoded_data_len,
													   uint32_t *decoded_rate,
													   unsigned int *flag);

/*! 
  \brief Destroy an initalized codec handle
  \param codec the codec handle to destroy
  \return SWITCH_STATUS_SUCCESS if the codec was destroyed
*/
SWITCH_DECLARE(switch_status_t) switch_core_codec_destroy(switch_codec_t *codec);

/*! 
  \brief Assign the read codec to a given session
  \param session session to add the codec to
  \param codec the codec to add
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_set_read_codec(switch_core_session_t *session, switch_codec_t *codec);

/*! 
  \brief Retrieve the read codec from a given session
  \param session session to retrieve from
  \return a pointer to the codec
*/
SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_read_codec(switch_core_session_t *session);

/*! 
  \brief Assign the write codec to a given session
  \param session session to add the codec to
  \param codec the codec to add
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status_t) switch_core_session_set_write_codec(switch_core_session_t *session, switch_codec_t *codec);

/*! 
  \brief Retrieve the write codec from a given session
  \param session session to retrieve from
  \return a pointer to the codec
*/
SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_write_codec(switch_core_session_t *session);

///\}
///\defgroup db Database Functions
///\ingroup core1
///\{
/*! 
  \brief Open a core db (SQLite) file
  \param filename the path to the db file to open
  \return the db handle
*/
SWITCH_DECLARE(switch_core_db_t *) switch_core_db_open_file(char *filename);

/*! 
  \brief Execute a sql stmt until it is accepted
  \param db the db handle
  \param sql the sql to execute
  \param retries the number of retries to use
  \return SWITCH_STATUS_SUCCESS if successful

*/
SWITCH_DECLARE(switch_status_t) switch_core_db_persistant_execute(switch_core_db_t *db, char *sql, uint32_t retries);

/*! 
  \brief perform a test query then perform a reactive query if the first one fails
  \param db the db handle
  \param test_sql the test sql
  \param reactive_sql the reactive sql
*/
SWITCH_DECLARE(void) switch_core_db_test_reactive(switch_core_db_t *db, char *test_sql, char *reactive_sql);

#define SWITCH_CORE_DB "core"
/*!
  \brief Open the default system database
*/
#define switch_core_db_handle() switch_core_db_open_file(SWITCH_CORE_DB)

///\}

///\defgroup Media File Functions
///\ingroup core1
///\{
/*! 
  \brief Open a media file using file format modules
  \param fh a file handle to use
  \param file_path the path to the file
  \param flags read/write flags
  \param pool the pool to use (NULL for new pool)
  \return SWITCH_STATUS_SUCCESS if the file is opened
  \note the loadable module used is chosen based on the file extension
*/
SWITCH_DECLARE(switch_status_t) switch_core_file_open(switch_file_handle_t *fh, char *file_path, unsigned int flags, switch_memory_pool_t *pool);

/*! 
  \brief Read media from a file handle
  \param fh the file handle to read from (must be initilized by you memset all 0 for read, fill in channels and rate for write)
  \param data the buffer to read the data to
  \param len the max size of the buffer
  \return SWITCH_STATUS_SUCCESS with len adjusted to the bytes read if successful
*/
SWITCH_DECLARE(switch_status_t) switch_core_file_read(switch_file_handle_t *fh, void *data, switch_size_t *len);

/*! 
  \brief Write media to a file handle
  \param fh the file handle to write to
  \param data the buffer to write
  \param len the amount of data to write from the buffer
  \return SWITCH_STATUS_SUCCESS with len adjusted to the bytes written if successful
*/
SWITCH_DECLARE(switch_status_t) switch_core_file_write(switch_file_handle_t *fh, void *data, switch_size_t *len);

/*! 
  \brief Seek a position in a file
  \param fh the file handle to seek
  \param cur_pos the current position in the file
  \param samples the amount of samples to seek from the beginning of the file
  \param whence the indicator (see traditional seek)
  \return SWITCH_STATUS_SUCCESS with cur_pos adjusted to new position
*/
SWITCH_DECLARE(switch_status_t) switch_core_file_seek(switch_file_handle_t *fh, unsigned int *cur_pos, int64_t samples, int whence);

/*! 
  \brief Set metadata to the desired string
  \param fh the file handle to set data to
  \param col the enum of the col name
  \param string the string to add
  \return SWITCH_STATUS_SUCCESS with cur_pos adjusted to new position
*/
SWITCH_DECLARE(switch_status_t) switch_core_file_set_string(switch_file_handle_t *fh, switch_audio_col_t col, const char *string);

/*! 
  \brief get metadata of the desired string
  \param fh the file handle to get data from
  \param col the enum of the col name
  \param string pointer to the string to fetch
  \return SWITCH_STATUS_SUCCESS with cur_pos adjusted to new position
*/
SWITCH_DECLARE(switch_status_t) switch_core_file_get_string(switch_file_handle_t *fh, switch_audio_col_t col, const char **string);


/*! 
  \brief Close an open file handle
  \param fh the file handle to close
  \return SWITCH_STATUS_SUCCESS if the file handle was closed
*/
SWITCH_DECLARE(switch_status_t) switch_core_file_close(switch_file_handle_t *fh);
///\}

///\defgroup speech ASR/TTS Functions
///\ingroup core1
///\{
/*! 
  \brief Open a speech handle
  \param sh a speech handle to use
  \param module_name the speech module to use
  \param voice_name the desired voice name
  \param rate the sampling rate
  \param flags tts flags
  \param pool the pool to use (NULL for new pool)
  \return SWITCH_STATUS_SUCCESS if the handle is opened
*/
SWITCH_DECLARE(switch_status_t) switch_core_speech_open(switch_speech_handle_t *sh, 
													  char *module_name,
													  char *voice_name,
													  unsigned int rate,
													  switch_speech_flag_t *flags,
													  switch_memory_pool_t *pool);
/*! 
  \brief Feed text to the TTS module
  \param sh the speech handle to feed
  \param text the buffer to write
  \param flags flags in/out for fine tuning
  \return SWITCH_STATUS_SUCCESS with len adjusted to the bytes written if successful
*/
SWITCH_DECLARE(switch_status_t) switch_core_speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags);

/*! 
  \brief Flush TTS audio on a given handle
  \param sh the speech handle
*/
SWITCH_DECLARE(void) switch_core_speech_flush_tts(switch_speech_handle_t *sh);

/*! 
  \brief Set a text parameter on a TTS handle
  \param sh the speech handle
  \param param the parameter
  \param val the value
*/
SWITCH_DECLARE(void) switch_core_speech_text_param_tts(switch_speech_handle_t *sh, char *param, char *val);

/*! 
  \brief Set a numeric parameter on a TTS handle
  \param sh the speech handle
  \param param the parameter
  \param val the value
*/
SWITCH_DECLARE(void) switch_core_speech_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val);

/*! 
  \brief Set a float parameter on a TTS handle
  \param sh the speech handle
  \param param the parameter
  \param val the value
*/
SWITCH_DECLARE(void) switch_core_speech_float_param_tts(switch_speech_handle_t *sh, char *param, double val);

/*! 
  \brief Read rendered audio from the TTS module
  \param sh the speech handle to read
  \param data the buffer to read to
  \param datalen the max size / written size of the data
  \param rate the rate of the read audio
  \param flags flags in/out for fine tuning
  \return SWITCH_STATUS_SUCCESS with len adjusted to the bytes written if successful
*/
SWITCH_DECLARE(switch_status_t) switch_core_speech_read_tts(switch_speech_handle_t *sh, 
														  void *data,
														  switch_size_t *datalen,
														  uint32_t *rate,
														  switch_speech_flag_t *flags);
/*! 
  \brief Close an open speech handle
  \param sh the speech handle to close
  \param flags flags in/out for fine tuning
  \return SWITCH_STATUS_SUCCESS if the file handle was closed
*/
SWITCH_DECLARE(switch_status_t) switch_core_speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags);


/*! 
  \brief Open an asr handle
  \param ah the asr handle to open
  \param module_name the name of the asr module
  \param codec the preferred codec
  \param rate the preferred rate
  \param dest the destination address
  \param flags flags to influence behaviour
  \param pool the pool to use (NULL for new pool)
  \return SWITCH_STATUS_SUCCESS if the asr handle was opened
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_open(switch_asr_handle_t *ah,
													 char *module_name,
													 char *codec,
													 int rate,
													 char *dest,
													 switch_asr_flag_t *flags,
													 switch_memory_pool_t *pool);

/*!
  \brief Close an asr handle
  \param ah the handle to close
  \param flags flags to influence behaviour
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags);

/*!
  \brief Feed audio data to an asr handle
  \param ah the handle to feed data to
  \param data a pointer to the data
  \param len the size in bytes of the data
  \param flags flags to influence behaviour
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags);

/*!
  \brief Check an asr handle for results
  \param ah the handle to check
  \param flags flags to influence behaviour
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags);

/*!
  \brief Get results from an asr handle
  \param ah the handle to get results from
  \param xmlstr a pointer to dynamically allocate an xml result string to
  \param flags flags to influence behaviour
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags);

/*!
  \brief Load a grammar to an asr handle
  \param ah the handle to load to
  \param grammar the name of the grammar
  \param path the path to the grammaar file
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_load_grammar(switch_asr_handle_t *ah, char *grammar, char *path);

/*!
  \brief Unload a grammar from an asr handle
  \param ah the handle to unload the grammar from
  \param grammar the grammar to unload
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_unload_grammar(switch_asr_handle_t *ah, char *grammar);

/*!
  \brief Pause detection on an asr handle
  \param ah the handle to pause
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_pause(switch_asr_handle_t *ah);

/*!
  \brief Resume detection on an asr handle
  \param ah the handle to resume
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status_t) switch_core_asr_resume(switch_asr_handle_t *ah);

///\}


///\defgroup dir Directory Service Functions
///\ingroup core1
///\{
/*! 
  \brief Open a directory handle
  \param dh a direcotry handle to use
  \param module_name the directory module to use
  \param source the source of the db (ip, hostname, path etc)
  \param dsn the username or designation of the lookup
  \param passwd the password
  \param pool the pool to use (NULL for new pool)
  \return SWITCH_STATUS_SUCCESS if the handle is opened
*/
SWITCH_DECLARE(switch_status_t) switch_core_directory_open(switch_directory_handle_t *dh, 
														 char *module_name, 
														 char *source,
														 char *dsn,
														 char *passwd,
														 switch_memory_pool_t *pool);

/*! 
  \brief Query a directory handle
  \param dh a direcotry handle to use
  \param base the base to query against
  \param query a string of filters or query data
  \return SWITCH_STATUS_SUCCESS if the query is successful
*/
SWITCH_DECLARE(switch_status_t) switch_core_directory_query(switch_directory_handle_t *dh, char *base, char *query);

/*! 
  \brief Obtain the next record in a lookup
  \param dh a direcotry handle to use
  \return SWITCH_STATUS_SUCCESS if another record exists
*/
SWITCH_DECLARE(switch_status_t) switch_core_directory_next(switch_directory_handle_t *dh);

/*! 
  \brief Obtain the next name/value pair in the current record
  \param dh a direcotry handle to use
  \param var a pointer to pointer of the name to fill in
  \param val a pointer to poinbter of the value to fill in
  \return SWITCH_STATUS_SUCCESS if an item exists
*/
SWITCH_DECLARE(switch_status_t) switch_core_directory_next_pair(switch_directory_handle_t *dh, char **var, char **val);

/*! 
  \brief Close an open directory handle
  \param dh a direcotry handle to close
  \return SWITCH_STATUS_SUCCESS if handle was closed
*/
SWITCH_DECLARE(switch_status_t) switch_core_directory_close(switch_directory_handle_t *dh);
///\}


///\defgroup misc Misc
///\ingroup core1
///\{
/*! 
  \brief Retrieve a FILE stream of a given text channel name
  \param channel text channel enumeration
  \return a FILE stream
*/
SWITCH_DECLARE(FILE *) switch_core_data_channel(switch_text_channel_t channel);


/*! 
  \brief Set the maximum priority the process can obtain
  \return 0 on success
*/
SWITCH_DECLARE(int32_t) set_high_priority(void);

/*! 
  \brief Run endlessly until the system is shutdown
  \param bg divert console to the background
*/
SWITCH_DECLARE(void) switch_core_runtime_loop(int bg);

/*!
  \brief Set the output console to the desired file
  \param console the file path
*/
SWITCH_DECLARE(switch_status_t) switch_core_set_console(char *console);

/*!
  \brief Breakdown a number of milliseconds into various time spec
  \param total_ms a number of milliseconds
  \param duration an object to store the results
*/
SWITCH_DECLARE(void) switch_core_measure_time(switch_time_t total_ms, switch_core_time_duration_t *duration);

/*!
  \brief Number of microseconds the system has been up
  \return a number of microseconds
*/
SWITCH_DECLARE(switch_time_t) switch_core_uptime(void);

/*!
  \brief send a control message to the core
  \param cmd the command
  \param val the command arguement (if needed)
  \return 0 on success nonzero on error
*/
SWITCH_DECLARE(int32_t) switch_core_session_ctl(switch_session_ctl_t cmd, uint32_t *val);

/*!
  \brief Get the output console
  \return the FILE stream
*/
SWITCH_DECLARE(FILE *) switch_core_get_console(void);
/*! 
  \brief Launch a thread
*/
SWITCH_DECLARE(void) switch_core_launch_thread(void *(*func)(switch_thread_t *, void*), void *obj, switch_memory_pool_t *pool);

/*!
  \brief Initiate Globals
*/
SWITCH_DECLARE(void) switch_core_set_globals(void);
///\}

/*!
  \}
*/

SWITCH_END_EXTERN_C

#endif
