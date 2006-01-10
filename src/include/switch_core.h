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

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

#define SWITCH_MAX_CORE_THREAD_SESSION_OBJS 128
#define SWITCH_MAX_STREAMS 128

/*! \brief A message object designed to allow unlike technologies to exchange data */
struct switch_core_session_message {
	/*! uuid of the sender (for replies)*/
	char *from;
	/*! enumeration of the type of message */
	switch_core_session_message_t message_id;

	/*! optional numeric arg*/
	int numeric_arg;
	/*! optional string arg*/
	char *string_arg;
	/*! optional string arg*/
	size_t string_arg_size;
	/*! optional arbitrary pointer arg */
	void *pointer_arg;
	/*! optional arbitrary pointer arg's size */
	size_t pointer_arg_size;

	/*! optional numeric reply*/
	int numeric_reply;
	/*! optional string reply*/
	char *string_reply;
	/*! optional string reply*/
	size_t string_reply_size;
	/*! optional arbitrary pointer reply */
	void *pointer_reply;
	/*! optional arbitrary pointer reply's size */
	size_t pointer_reply_size;

};

/*! \brief A generic object to pass as a thread's session object to allow mutiple arguements and a pool */
struct switch_core_thread_session {
	/*! status of the thread */
	int running;
	/*! array of void pointers to pass mutiple data objects */
	void *objs[SWITCH_MAX_CORE_THREAD_SESSION_OBJS];
	/*! a pointer to a memory pool if the thread has it's own pool */
	switch_memory_pool *pool;
};

struct switch_core_session;
struct switch_core_runtime;


/*!
  \defgroup core1 Core Library 
  \ingroup FREESWITCH
  \{ 
*/

///\defgroup ss Startup/Shutdown
///\ingroup core1
///\{
/*! 
  \brief Initilize the core
  \note to be called at application startup
*/
SWITCH_DECLARE(switch_status) switch_core_init(void);

/*! 
  \brief Destroy the core
  \note to be called at application shutdown
*/
SWITCH_DECLARE(switch_status) switch_core_destroy(void);
///\}


///\defgroup memp Memory Pooling/Allocation
///\ingroup core1
///\{
/*! 
  \brief Create a new sub memory pool from the core's master pool
  \return SWITCH_STATUS_SUCCESS on success
*/
SWITCH_DECLARE(switch_status) switch_core_new_memory_pool(switch_memory_pool **pool);

/*! 
  \brief Returns a subpool back to the main pool
  \return SWITCH_STATUS_SUCCESS on success
*/
SWITCH_DECLARE(switch_status) switch_core_destroy_memory_pool(switch_memory_pool **pool);

/*! 
  \brief Start the session's state machine
  \param session the session on which to start the state machine
*/
SWITCH_DECLARE(void) switch_core_session_run(switch_core_session *session);

/*! 
  \brief Allocate memory from the main pool with no intention of returning it
  \param memory the number of bytes to allocate
  \return a void pointer to the allocated memory
  \note this memory never goes out of scope until the core is destroyed
*/
SWITCH_DECLARE(void *) switch_core_permenant_alloc(size_t memory);

/*! 
  \brief Allocate memory directly from a memory pool
  \param pool the memory pool to allocate from
  \param memory the number of bytes to allocate
  \return a void pointer to the allocated memory
*/
SWITCH_DECLARE(void *) switch_core_alloc(switch_memory_pool *pool, size_t memory);

/*! 
  \brief Allocate memory from a session's pool
  \param session the session to request memory from
  \param memory the amount of memory to allocate
  \return a void pointer to the newly allocated memory
  \note the memory will be in scope as long as the session exists
*/
SWITCH_DECLARE(void *) switch_core_session_alloc(switch_core_session *session, size_t memory);

/*! 
  \brief Copy a string using permenant memory allocation
  \param todup the string to duplicate
  \return a pointer to the newly duplicated string
*/
SWITCH_DECLARE(char *) switch_core_permenant_strdup(char *todup);

/*! 
  \brief Copy a string using memory allocation from a session's pool
  \param session a session to use for allocation
  \param todup the string to duplicate
  \return a pointer to the newly duplicated string
*/
SWITCH_DECLARE(char *) switch_core_session_strdup(switch_core_session *session, char *todup);

/*! 
  \brief Copy a string using memory allocation from a given pool
  \param pool the pool to use for allocation
  \param todup the string to duplicate
  \return a pointer to the newly duplicated string
*/
SWITCH_DECLARE(char *) switch_core_strdup(switch_memory_pool *pool, char *todup);

/*! 
  \brief Retrieve the memory pool from a session
  \param session the session to retrieve the pool from
  \return the session's pool
  \note to be used sparingly
*/
SWITCH_DECLARE(switch_memory_pool *) switch_core_session_get_pool(switch_core_session *session);
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
SWITCH_DECLARE(switch_core_session *) switch_core_session_request(const switch_endpoint_interface *endpoint_interface, switch_memory_pool *pool);

/*! 
  \brief Destroy a session and return the memory pool to the core
  \param session pointer to a pointer of the session to destroy
  \return
*/
SWITCH_DECLARE(void) switch_core_session_destroy(switch_core_session **session);

/*! 
  \brief Allocate and return a new session from the core based on a given endpoint module name
  \param endpoint_name the name of the endpoint module
  \param pool the pool to use
  \return the newly created session
*/
SWITCH_DECLARE(switch_core_session *) switch_core_session_request_by_name(char *endpoint_name, switch_memory_pool *pool);

/*! 
  \brief Launch the session thread (state machine) on a given session
  \param session the session to activate the state machine on
*/
SWITCH_DECLARE(void) switch_core_session_thread_launch(switch_core_session *session);

/*! 
  \brief Retrieve a pointer to the channel object associated with a given session
  \param session the session to retrieve from
  \return a pointer to the channel object
*/
SWITCH_DECLARE(switch_channel *) switch_core_session_get_channel(switch_core_session *session);

/*! 
  \brief Signal a session's state machine thread that a state change has occured
*/
SWITCH_DECLARE(void) switch_core_session_signal_state_change(switch_core_session *session);

/*! 
  \brief Retrieve the unique identifier from a session
  \param session the session to retrieve the uuid from
  \return a string representing the uuid
*/
SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session *session);

/*! 
  \brief Send a message to another session using it's uuid
  \param uuid_str the unique id of the session you want to send a message to
  \param message the switch_core_session_message object to send
  \return the status returned by the message handler
*/
SWITCH_DECLARE (switch_status) switch_core_session_message_send(char *uuid_str, switch_core_session_message *message);

/*! 
  \brief Retrieve private user data from a session
  \param session the session to retrieve from
  \return a pointer to the private data
*/
SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session *session);

/*! 
  \brief Add private user data to a session
  \param session the session to add used data to
  \param private the used data to add
  \return SWITCH_STATUS_SUCCESS if data is added
*/
SWITCH_DECLARE(switch_status) switch_core_session_set_private(switch_core_session *session, void *private);

/*!
  \brief Add a logical stream to a session
  \param session the session to add the stream to
  \param private an optional pointer to private data for the new stream
  \return the stream id of the new stream
 */
SWITCH_DECLARE(int) switch_core_session_add_stream(switch_core_session *session, void *private);

/*!
  \brief Retreive a logical stream from a session
  \param session the session to add the stream to
  \param index the index to retrieve
  \return the private data (if any)
 */
SWITCH_DECLARE(void *) switch_core_session_get_stream(switch_core_session *session, int index);

/*!
  \brief Determine the number of logical streams a session has
  \param session the session to query
  \return the total number of logical streams
 */
SWITCH_DECLARE(int) switch_core_session_get_stream_count(switch_core_session *session);

/*! 
  \brief Launch a thread designed to exist within the scope of a given session
  \param session a session to allocate the thread from
  \param func a function to execute in the thread
  \param obj an arguement
*/
SWITCH_DECLARE(void) switch_core_session_launch_thread(switch_core_session *session, void *(*func)(switch_thread *, void *), void *obj);

/*! 
  \brief Signal a thread using a thread session to terminate
  \param thread_session the thread_session to indicate to
*/
SWITCH_DECLARE(void) switch_core_thread_session_end(switch_core_thread_session *thread_session);

/*! 
  \brief Launch a service thread on a session to drop inbound data
  \param session the session the launch thread on
  \param stream_id which logical media channel to use
  \param thread_session the thread_session to use
*/
SWITCH_DECLARE(void) switch_core_service_session(switch_core_session *session, switch_core_thread_session *thread_session, int stream_id);

/*! 
  \brief Request an outgoing session spawned from an existing session using a desired endpoing module
  \param session the originating session
  \param endpoint_name the name of the module to use for the new session
  \param caller_profile the originator's caller profile
  \param new_session a NULL pointer to aim at the newly created session
  \return SWITCH_STATUS_SUCCESS if the session was created
*/
SWITCH_DECLARE(switch_status) switch_core_session_outgoing_channel(switch_core_session *session,
											 char *endpoint_name,
											 switch_caller_profile *caller_profile,
											 switch_core_session **new_session);

/*! 
  \brief Answer the channel of a given session
  \param session the session to answer the channel of
  \return SWITCH_STATUS_SUCCESS if the channel was answered
*/
SWITCH_DECLARE(switch_status) switch_core_session_answer_channel(switch_core_session *session);

/*! 
  \brief Receive a message on a given session
  \param session the session to receive the message from
  \param message the message to recieve
  \return the status returned by the message handler
*/
SWITCH_DECLARE(switch_status) switch_core_session_receive_message(switch_core_session *session, switch_core_session_message *message);

/*! 
  \brief Read a frame from a session
  \param session the session to read from
  \param frame a NULL pointer to a frame to aim at the newly read frame
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS a the frame was read
*/
SWITCH_DECLARE(switch_status) switch_core_session_read_frame(switch_core_session *session, switch_frame **frame, int timeout, int stream_id);

/*! 
  \brief Write a frame to a session
  \param session the session to write to
  \param frame the frame to write
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS a the frame was written
*/
SWITCH_DECLARE(switch_status) switch_core_session_write_frame(switch_core_session *session, switch_frame *frame, int timeout, int stream_id);

/*! 
  \brief Send a signal to a channel
  \param session session to send signal to
  \param sig signal to send
  \return status returned by the session's signal handler
*/
SWITCH_DECLARE(switch_status) switch_core_session_kill_channel(switch_core_session *session, switch_signal sig);

/*! 
  \brief Wait for a session to be ready for input
  \param session session to wait for
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS if data is available for read within timeframe specified
*/
SWITCH_DECLARE(switch_status) switch_core_session_waitfor_read(switch_core_session *session, int timeout, int stream_id);

/*! 
  \brief Wait for a session to be ready for output
  \param session session to wait for
  \param timeout number of milliseconds to wait for data
  \param stream_id which logical media channel to use
  \return SWITCH_STATUS_SUCCESS if the session is available for write within timeframe specified
*/
SWITCH_DECLARE(switch_status) switch_core_session_waitfor_write(switch_core_session *session, int timeout, int stream_id);

/*! 
  \brief Send DTMF to a session
  \param session session to send DTMF to
  \param dtmf string to send to the session
  \return SWITCH_STATUS_SUCCESS if the dtmf was written
*/
SWITCH_DECLARE(switch_status) switch_core_session_send_dtmf(switch_core_session *session, char *dtmf);

/*! 
  \brief Add an event hook to be executed when a session requests an outgoing extension
  \param session session to bind hook to
  \param outgoing_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_outgoing(switch_core_session *session, switch_outgoing_channel_hook outgoing_channel);
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
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_answer_channel(switch_core_session *session, switch_answer_channel_hook answer_channel);

/*! 
  \brief Add an event hook to be executed when a session sends a message
  \param session session to bind hook to
  \param receive_message hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_receive_message(switch_core_session *session, switch_receive_message_hook receive_message);

/*! 
  \brief Add an event hook to be executed when a session reads a frame
  \param session session to bind hook to
  \param  read_frame hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_read_frame(switch_core_session *session, switch_read_frame_hook read_frame);

/*! 
  \brief Add an event hook to be executed when a session writes a frame
  \param session session to bind hook to
  \param write_frame hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_write_frame(switch_core_session *session, switch_write_frame_hook write_frame);

/*! 
  \brief Add an event hook to be executed when a session kills a channel
  \param session session to bind hook to
  \param kill_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_kill_channel(switch_core_session *session, switch_kill_channel_hook kill_channel);

/*! 
  \brief Add an event hook to be executed when a session waits for a read event
  \param session session to bind hook to
  \param waitfor_read hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_waitfor_read(switch_core_session *session, switch_waitfor_read_hook waitfor_read);

/*! 
  \brief Add an event hook to be executed when a session waits for a write event
  \param session session to bind hook to
  \param waitfor_write hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_waitfor_write(switch_core_session *session, switch_waitfor_write_hook waitfor_write);

/*! 
  \brief Add an event hook to be executed when a session sends dtmf
  \param session session to bind hook to
  \param send_dtmf hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_send_dtmf(switch_core_session *session, switch_send_dtmf_hook send_dtmf);
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
SWITCH_DECLARE(switch_status) switch_core_hash_init(switch_hash **hash, switch_memory_pool *pool);

/*! 
  \brief Destroy an existing hash table
  \param hash the hash to destroy
  \return SWITCH_STATUS_SUCCESS if the hash is destroyed
*/
SWITCH_DECLARE(switch_status) switch_core_hash_destroy(switch_hash *hash);

/*! 
  \brief Insert data into a hash
  \param hash the hash to add data to
  \param key the name of the key to add the data to
  \param data the data to add
  \return SWITCH_STATUS_SUCCESS if the data is added
  \note the string key must be a constant or a dynamic string
*/
SWITCH_DECLARE(switch_status) switch_core_hash_insert(switch_hash *hash, char *key, void *data);

/*! 
  \brief Insert data into a hash with dynamicly allocated key name
  \param hash the hash to add data to
  \param key the name of the key to add the data to
  \param data the data to add
  \return SWITCH_STATUS_SUCCESS if the data is added
*/
SWITCH_DECLARE(switch_status) switch_core_hash_insert_dup(switch_hash *hash, char *key, void *data);

/*! 
  \brief Delete data from a hash based on desired key
  \param hash the hash to delete from
  \param key the key from which to delete the data
  \return SWITCH_STATUS_SUCCESS if the data is deleted
*/
SWITCH_DECLARE(switch_status) switch_core_hash_delete(switch_hash *hash, char *key);

/*! 
  \brief Retrieve data from a given hash
  \param hash the hash to retrieve from
  \param key the key to retrieve
  \return a pointer to the data held in the key
*/
SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash *hash, char *key);
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
SWITCH_DECLARE(switch_status) switch_core_timer_init(switch_timer *timer, char *timer_name, int interval, int samples, switch_memory_pool *pool);

/*! 
  \brief Wait for one cycle on an existing timer
  \param timer the timer to wait on
  \return the newest sample count
*/
SWITCH_DECLARE(int) switch_core_timer_next(switch_timer *timer);

/*! 
  \brief Destroy an allocated timer
  \param timer timer to destroy
  \return SWITCH_STATUS_SUCCESS after destruction
*/
SWITCH_DECLARE(switch_status) switch_core_timer_destroy(switch_timer *timer);
///\}

///\defgroup codecs Codec Functions
///\ingroup core1
///\{
/*! 
  \brief Initialize a codec handle
  \param codec the handle to initilize
  \param codec_name the name of the codec module to use
  \param rate the desired rate (0 for any)
  \param ms the desired number of milliseconds (0 for any)
  \param channels the desired number of channels (0 for any)
  \param flags flags to alter behaviour
  \param codec_settings desired codec settings
  \param pool the memory pool to use
  \return SWITCH_STATUS_SUCCESS if the handle is allocated
*/
SWITCH_DECLARE(switch_status) switch_core_codec_init(switch_codec *codec, 
													 char *codec_name, 
													 int rate, 
													 int ms, 
													 int channels, 
													 switch_codec_flag flags, 
													 const switch_codec_settings *codec_settings, 
													 switch_memory_pool *pool);

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
SWITCH_DECLARE(switch_status) switch_core_codec_encode(switch_codec *codec,
								 switch_codec *other_codec,
								 void *decoded_data,
								 size_t decoded_data_len,
								 int decoded_rate,
								 void *encoded_data,
								 size_t *encoded_data_len,
								 int *encoded_rate,
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
SWITCH_DECLARE(switch_status) switch_core_codec_decode(switch_codec *codec,
								 switch_codec *other_codec,
								 void *encoded_data,
								 size_t encoded_data_len,
								 int encoded_rate,
								 void *decoded_data,
								 size_t *decoded_data_len,
								 int *decoded_rate,
								 unsigned int *flag);

/*! 
  \brief Destroy an initalized codec handle
  \param codec the codec handle to destroy
  \return SWITCH_STATUS_SUCCESS if the codec was destroyed
*/
SWITCH_DECLARE(switch_status) switch_core_codec_destroy(switch_codec *codec);

/*! 
  \brief Assign the read codec to a given session
  \param session session to add the codec to
  \param codec the codec to add
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status) switch_core_session_set_read_codec(switch_core_session *session, switch_codec *codec);

/*! 
  \brief Assign the write codec to a given session
  \param session session to add the codec to
  \param codec the codec to add
  \return SWITCH_STATUS_SUCCESS if successful
*/
SWITCH_DECLARE(switch_status) switch_core_session_set_write_codec(switch_core_session *session, switch_codec *codec);
///\}

///\defgroup db Database Functions
///\ingroup core1
///\{
/*! 
  \brief Open a core db (SQLite) file
  \param filename the path to the db file to open
  \return
*/
SWITCH_DECLARE(switch_core_db *) switch_core_db_open_file(char *filename);

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
SWITCH_DECLARE(switch_status) switch_core_file_open(switch_file_handle *fh, char *file_path, unsigned int flags, switch_memory_pool *pool);

/*! 
  \brief Read media from a file handle
  \param fh the file handle to read from
  \param data the buffer to read the data to
  \param len the max size of the buffer
  \return SWITCH_STATUS_SUCCESS with len adjusted to the bytes read if successful
*/
SWITCH_DECLARE(switch_status) switch_core_file_read(switch_file_handle *fh, void *data, size_t *len);

/*! 
  \brief Write media to a file handle
  \param fh the file handle to write to
  \param data the buffer to write
  \param len the amount of data to write from the buffer
  \return SWITCH_STATUS_SUCCESS with len adjusted to the bytes written if successful
*/
SWITCH_DECLARE(switch_status) switch_core_file_write(switch_file_handle *fh, void *data, size_t *len);

/*! 
  \brief Seek a position in a file
  \param fh the file handle to seek
  \param cur_pos the current position in the file
  \param samples the amount of samples to seek from the beginning of the file
  \param whence the indicator (see traditional seek)
  \return SWITCH_STATUS_SUCCESS with cur_pos adjusted to new position
*/
SWITCH_DECLARE(switch_status) switch_core_file_seek(switch_file_handle *fh, unsigned int *cur_pos, unsigned int samples, int whence);

/*! 
  \brief Close an open file handle
  \param fh the file handle to close
  \return SWITCH_STATUS_SUCCESS if the file handle was closed
*/
SWITCH_DECLARE(switch_status) switch_core_file_close(switch_file_handle *fh);
///\}

///\defgroup misc Misc
///\ingroup core1
///\{
/*! 
  \brief Retrieve a FILE stream of a given text channel name
  \param channel text channel enumeration
  \return a FILE stream
*/
SWITCH_DECLARE(FILE *) switch_core_data_channel(switch_text_channel channel);

/*! 
  \brief Launch a thread
*/
SWITCH_DECLARE(void) switch_core_launch_thread(void *(*func)(switch_thread *, void*), void *obj, switch_memory_pool *pool);
///\}

#ifdef USE_PERL
/*! 
  \brief Execute some perl when compiled with perl support
  \return SWITCH_STATUS_SUCCESS on success
*/
SWITCH_DECLARE(switch_status) switch_core_do_perl(char *txt);
#endif 

/*!
  \}
*/


#ifdef __cplusplus
}
#endif


#endif
