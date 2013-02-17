/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Eliot Gable <egable@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Eliot Gable <egable@gmail.com>
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_dispatcher.h -- Threaded message dispatcher.
 */

/*! \file switch_dispatcher.h
    \brief Threaded Message Dispatcher

	The dispatcher is a general purpose programming pattern consisting of a FIFO message queue and a processing thread
	which reads the queue and processes the messages. When you create a dispatcher, you specify a callback function which
	is called for each message sent to the queue. Your callback function must examine the message and decide how to
	process it. The processor thread keeps calling your callback until all messages pending in the queue have been
	processed. Then, the processor thread goes to sleep until the next message arrives. 

	Each time a message is pushed into the queue, a wake-up signal is broadcast to the processor thread. If the processor
	thread is already awake, it simply keeps doing what it is already doing. If the processor thread is asleep, it wakes and
	starts processing messages.

	This dispatcher system also contains core event handling through a double binding mechanism which intercepts events from
	the core eventing thread and pushes them through the dispatcher's queue and message processing thread before being
	delivered to your registered callback function for the event. This ensures that your callback function cannot block the
	core event thread in FreeSWITCH.

 */

/*!
  \defgroup dispatcher Threaded Message Dispatcher
  \ingroup core1
  \{
*/

//#define BUILD_DISPATCHER
#ifndef SWITCH_DISPATCHER_H
#define SWITCH_DISPATCHER_H

#ifdef BUILD_DISPATCHER

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

typedef int (*switch_dispatcher_func_t)(void *message, void *thread_data);

/*! \brief A dispatcher message. */
struct switch_dispatcher_message {
	/*! The message */
	void *data;
	/*! A flag indicating whether the message is an event intercepted from the core event thread */
	switch_bool_t is_switch_event;
};
typedef struct switch_dispatcher_message switch_dispatcher_message_t;

/*! \brief Representation of a dispatcher */
struct switch_dispatcher {
	/*! The dispatcher's memory pool */
	switch_memory_pool_t *pool;
	/*! The callback function to call for processing a message */
	switch_dispatcher_func_t callback;
	/*! The condition on which to wait for wakeup signals when messages are added to the queue */
	switch_thread_cond_t *cond;
	/*! A mutex to protect access to the condition object */
	switch_mutex_t *mutex;
	/*! The dispatcher's message queue */
	switch_queue_t *queue;
	/*! The dispatcher's processing thread */
	switch_thread_t *thread;
	/*! The processing thread's attributes */
	switch_threadattr_t *thread_attr;
	/*! The data you want passed to all calls of your callback function */
	void *thread_data;
	/*! A mutex to protect access to the event bindings for core event interception */
	switch_mutex_t *event_mutex;
	/*! A hash table of the event bindings for core event interception */
	switch_hash_t *event_bindings;
	/*! A flag indicating whether the dispatcher is running */
	switch_atomic_t running;
	/*! A flag indicating whether the dispatcher should accept new messages */
	switch_atomic_t accept_messages;
	/*! A flag indicating whether the dispatcher should finish processing messages and terminate */
	switch_atomic_t halt;
	/*! A flag indicating whether messages should be dropped if the queue overflows */
	switch_bool_t drop_overflow;
	/*! A flag indicating whether statistics should be collected for the dispatcher */
	switch_bool_t collect_stats;
	/*! The requested size of the dispatcher's queue */
	switch_size_t queue_size;

	/* Local message dispatcher statistics */
	/*! A mutex to protect access to the statistics */
	switch_mutex_t *process_loop_stats_mutex;
	/*! How many messages are in the queue */
	switch_atomic_t queue_count;
	/*! The maximum number of messages which were in the queue */
	switch_atomic_t max_queue_count;
	/*! Maximum number of messages processed per second */
    switch_atomic_t max_per_sec;
	/*! Number of messages processed in the last second */
    switch_atomic_t count_past_sec;
	/*! Number of messages processed so far this second */
    switch_atomic_t count_cur_sec;
	/*! Total number of messages processed */
    uint64_t total_count;
	/*! Total time used to process the messages */
    uint64_t total_time;
	/*! Processing duration of the last message processed */
    uint64_t last_duration;
	/*! Maximum processing duration of any message processed */
    uint64_t max_duration;
	/*! When the last second started */
    switch_time_t last_sec_start;
	/* End of message dispatcher statistics */
};
typedef struct switch_dispatcher switch_dispatcher_t;


/*! \brief Representation of a dispatcher event binding */
struct switch_dispatcher_event_binding_data {
	/*! The binding node */
	switch_event_node_t *node;
	/*! The callback to call for processing the event */
	switch_event_callback_t callback;
	/*! The data to pass to all calls to the callback */
	void *binding_data;
};
typedef struct switch_dispatcher_event_binding_data switch_dispatcher_event_binding_data_t;


/*
 * \brief Create a message dispatcher.
 * \param dispatcher_out A pointer to the pointer where you want to store the created dispatcher.
 * \param queue_size The size of the queue for the dispatcher.
 * \param drop_overflow Set to SWITCH_TRUE if you want to drop messages when the queue is full. Set to SWITCH_FALSE if you want to block until the message
 *                      can be safely delivered to the queue.
 * \param collect_stas Collect message processing statistics for the dispatcher.
 * \param callback The function to call when a message is available for processing.
 * \param thread_data An object to pass to the callback function for all calls to the function.
 * \post The pointer refenced by dispatcher_out is will point to the new dispatcher structure.
 * \post A new thread is created which immediately enters a sleeping state.
 */
SWITCH_DECLARE(switch_status_t) switch_dispatcher_create_real(switch_dispatcher_t **dispatcher_out, switch_size_t queue_size, switch_bool_t drop_overflow,
															  switch_bool_t collect_stats, switch_dispatcher_func_t callback, void *thread_data,
															  const char *file, const char *func, int line);
/*
 * \ref switch_dispatcher_create_real()
 */
#define switch_dispatcher_create(d, qs, drp, cs, f, td) switch_dispatcher_create_real(d, qs, drp, cs, f, td, __FILE__, __FUNCTION__, __LINE__)

/*
 * \brief Destroy a message dispatcher, blocking until all messages are processed and the thread is torn down.
 * \param dispatcher_out A pointer to a switch_dispatcher_t* referencing the dispatcher to destroy.
 * \post dispatcher_out is set to NULL.
 */
SWITCH_DECLARE(switch_status_t) switch_dispatcher_destroy_real(switch_dispatcher_t **dispatcher_out, const char *file, const char *func, int line);

/*
 * \ref switch_dispatcher_destroy_real()
 */
#define switch_dispatcher_destroy(d) switch_dispatcher_destroy_real(d, __FILE__, __FUNCTION__, __LINE__)

/*
 * \brief Add a message to the dispatcher queue.
 * \param dispatcher A pointer to the dispatcher structure which holds the queue receiving the message.
 * \param data The message to send to the queue.
 * \pre The dispatcher structure must already be created by a call to switch_dispatcher_create().
 * \pre The dispatcher thread must not be in the middle of a shutdown due to a previous call to switch_dispatcher_destroy().
 * \pre The message must not be NULL.
 * \post The message is added to the end of the dispatcher's queue.
 */
SWITCH_DECLARE(switch_status_t) switch_dispatcher_enqueue_real(switch_dispatcher_t *dispatcher, void *data, const char *file, const char *func, int line);

/*
 * \ref switch_dispatcher_enqueue_real()
 */
#define switch_dispatcher_enqueue(disp, data) switch_dispatcher_enqueue_real(disp, data, __FILE__, __FUNCTION__, __LINE__)


/*
 * \brief Dump the dispatcher's message processing statistics out to the specified stream.
 * \param dispatcher The dispatcher whose statistics you wish to dump.
 * \param stream The stream to which the dispatcher's statistics should be dumped.
 * \pre The dispatcher structure must already be created by a call to switch_dispatcher_create() with SWITCH_TRUE specified in the collect_stats parameter.
 * \pre The stream must be pre-initialized with SWITCH_STANDARD_STREAM() macro.
 * \post The stream will be updated with a formatted dump of the dispatcher's statistics.
 */
SWITCH_DECLARE(void) switch_dispatcher_dump_stats_real(switch_dispatcher_t *dispatcher, switch_stream_handle_t *stream, const char *file,
													   const char *func, int line);

/*
 * \ref switch_dispatcher_dump_stats_real()
 */
#define switch_dispatcher_dump_stats(d, s) switch_dispatcher_dump_stats_real(d, s, __FILE__, __FUNCTION__, __LINE__)


/*
 * \brief Bind the dispatcher to an event and route the events through the dispatcher to the specified callback.
 * \param dispatcher A pointer to the dispatcher structure which holds the queue for receiving the event as a dispatcher message.
 * \param id An unique identifier for the binding.
 * \param event The event enumeration to bind to.
 * \param subclass_name Event subclass to bind to in case of SWITCH_EVENT_CUSTOM.
 * \param callback The callback function to call when the event is received. This will be called by the dispatcher's thread.
 * \param user_data Optional user data to pass to the callback function each time it is called for this event.
 * \pre The dispatcher structure must already be created by a call to switch_dispatcher_create().
 * \pre The dispatcher must not be in the middle of shutdown due to a previous call to switch_dispatcher_destroy().
 * \post The dispatcher is bound to the event and will receive the event into its message queue. When the event is popped off the queue, it will be passed to the callback function specified by this function call.
 */
SWITCH_DECLARE(switch_status_t) switch_dispatcher_event_bind_real(switch_dispatcher_t *dispatcher, const char *id, switch_event_types_t event,
																  const char *subclass_name, switch_event_callback_t callback,
																  void *user_data, const char *file, const char *func, int line);

/*
 * \ref switch_dispatcher_event_bind_real()
 */
#define switch_dispatcher_event_bind(d, bd, id, e, sn, cb, ud) switch_dispatcher_event_bind_real(d, bd, id, e, sn, cb, ud, __FILE__, __FUNCTION__, __LINE__);


/*
 * \brief Unbind the dispatcher from an event.
 * \param dispatcher A pointer to the dispatcher structure which holds the queue for receiving the event.
 * \param event The event enumeration to unbind.
 * \param subclass_name The subclass name of the event to unbind.
 * \pre The dispatcher structure must already be created by a call to switch_dispatcher_create().
 * \pre The dispatcher must not be in the middle of a shutdown due to a previous call to switch_dispatcher_destroy().
 * \pre The dispatcher must be bound to the event and subclass specified.
 * \post The dispatcher is no longer bound to the event and subclass specified, but any messages for that event remaining in the queue will continue to be
 *       processed.
 */
SWITCH_DECLARE(switch_status_t) switch_dispatcher_event_unbind_real(switch_dispatcher_t *disp, switch_event_types_t event, const char *subclass_name, 
																	const char *file, const char *func, int line);

/*
 * \ref switch_dispatcher_event_unbind_real()
 */
#define switch_dispatcher_event_unbind(d, e, s) switch_dispatcher_event_unbind_real(d, e, s, __FILE__, __FUNCTION__, __LINE__)


///\}

SWITCH_END_EXTERN_C

#endif

#endif /* SWITCH_DISPATCHER_H */

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
