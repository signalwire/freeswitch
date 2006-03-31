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
 * switch_module_interfaces.h -- Module Interface Definitions
 *
 */
/*! \file switch_module_interfaces.h
    \brief Module Interface Definitions

	This module holds the definition of data abstractions used to implement various pluggable 
	interfaces and pluggable event handlers.

*/
#ifndef SWITCH_MODULE_INTERFACES_H
#define SWITCH_MODULE_INTERFACES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

/*! \brief A table of functions to execute at various states 
*/
struct switch_state_handler_table {
	/*! executed when the state changes to init */
	switch_state_handler on_init;
	/*! executed when the state changes to ring */
	switch_state_handler on_ring;
	/*! executed when the state changes to execute */
	switch_state_handler on_execute;
	/*! executed when the state changes to hangup */
	switch_state_handler on_hangup;
	/*! executed when the state changes to loopback*/
	switch_state_handler on_loopback;
	/*! executed when the state changes to transmit*/
	switch_state_handler on_transmit;
};

/*! \brief Node in which to store custom outgoing channel callback hooks */
struct switch_io_event_hook_outgoing_channel {
	/*! the outgoing channel callback hook*/
	switch_outgoing_channel_hook outgoing_channel;
	struct switch_io_event_hook_outgoing_channel *next;
};

/*! \brief Node in which to store custom answer channel callback hooks */
struct switch_io_event_hook_answer_channel {
	/*! the answer channel callback hook*/
	switch_answer_channel_hook answer_channel;
	struct switch_io_event_hook_answer_channel *next;
};

/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_receive_message {
	/*! the answer channel callback hook*/
	switch_receive_message_hook receive_message;
	struct switch_io_event_hook_receive_message *next;
};

/*! \brief Node in which to store custom read frame channel callback hooks */
struct switch_io_event_hook_read_frame {
	/*! the read frame channel callback hook*/
	switch_read_frame_hook read_frame;
	struct switch_io_event_hook_read_frame *next;
};

/*! \brief Node in which to store custom write_frame channel callback hooks */
struct switch_io_event_hook_write_frame {
	/*! the write_frame channel callback hook*/
	switch_write_frame_hook write_frame;
	struct switch_io_event_hook_write_frame *next;
};

/*! \brief Node in which to store custom kill channel callback hooks */
struct switch_io_event_hook_kill_channel {
	/*! the kill channel callback hook*/
	switch_kill_channel_hook kill_channel;
	struct switch_io_event_hook_kill_channel *next;
};

/*! \brief Node in which to store custom waitfor read channel callback hooks */
struct switch_io_event_hook_waitfor_read {
	/*! the waitfor read channel callback hook*/
	switch_waitfor_read_hook waitfor_read;
	struct switch_io_event_hook_waitfor_read *next;
};

/*! \brief Node in which to store custom waitfor write channel callback hooks */
struct switch_io_event_hook_waitfor_write {
	/*! the waitfor write channel callback hook*/
	switch_waitfor_write_hook waitfor_write;
	struct switch_io_event_hook_waitfor_write *next;
};

/*! \brief Node in which to store custom send dtmf channel callback hooks */
struct switch_io_event_hook_send_dtmf {
	/*! the send dtmf channel callback hook*/
	switch_send_dtmf_hook send_dtmf;
	struct switch_io_event_hook_send_dtmf *next;
};

/*! \brief A table of lists of io_event_hooks to store the event hooks associated with a session */
struct switch_io_event_hooks {
	/*! a list of outgoing channel hooks */
	struct switch_io_event_hook_outgoing_channel *outgoing_channel;
	/*! a list of answer channel hooks */
	struct switch_io_event_hook_answer_channel *answer_channel;
	/*! a list of receive message hooks */
	struct switch_io_event_hook_receive_message *receive_message;
	/*! a list of read frame hooks */
	struct switch_io_event_hook_read_frame *read_frame;
	/*! a list of write frame hooks */
	struct switch_io_event_hook_write_frame *write_frame;
	/*! a list of kill channel hooks */
	struct switch_io_event_hook_kill_channel *kill_channel;
	/*! a list of wait for read hooks */
	struct switch_io_event_hook_waitfor_read *waitfor_read;
	/*! a list of wait for write hooks */
	struct switch_io_event_hook_waitfor_write *waitfor_write;
	/*! a list of send dtmf hooks */
	struct switch_io_event_hook_send_dtmf *send_dtmf;
};

/*! \brief A table of i/o routines that an endpoint interface can implement */
struct switch_io_routines {
	/*! creates an outgoing session from given session, caller profile */
	switch_status (*outgoing_channel)(switch_core_session *, switch_caller_profile *, switch_core_session **, switch_memory_pool *);
	/*! answers the given session's channel */
	switch_status (*answer_channel)(switch_core_session *);
	/*! read a frame from a session */
	switch_status (*read_frame)(switch_core_session *, switch_frame **, int, switch_io_flag, int);
	/*! write a frame to a session */
	switch_status (*write_frame)(switch_core_session *, switch_frame *, int, switch_io_flag, int);
	/*! send a kill signal to the session's channel */
	switch_status (*kill_channel)(switch_core_session *, int);
	/*! wait for the session's channel to be ready to read audio */
	switch_status (*waitfor_read)(switch_core_session *, int, int);
	/*! wait for the session's channel to be ready to write audio */
	switch_status (*waitfor_write)(switch_core_session *, int, int);
	/*! send a string of DTMF digits to a session's channel */
	switch_status (*send_dtmf)(switch_core_session *, char *);
	/*! receive a message from another session*/
	switch_status (*receive_message)(switch_core_session *, switch_core_session_message *);
};

/*! \brief Abstraction of an module endpoint interface
  This is the glue between the abstract idea of a "channel" and what is really going on under the
  hood.	 Each endpoint module fills out one of these tables and makes it available when a channel
  is created of it's paticular type.
*/

struct switch_endpoint_interface {
	/*! the interface's name */
	const char *interface_name;

	/*! channel abstraction methods */
	const switch_io_routines *io_routines;

	/*! state machine methods */
	const switch_state_handler_table *state_handler;

	/*! private information */
	void *private_info;

	/* to facilitate linking */
	const struct switch_endpoint_interface *next;
};

/*! \brief Abstract handler to a timer module */
struct switch_timer {
	/*! time interval expressed in milliseconds */
	int interval;
	/*! flags to control behaviour */
	uint32_t flags;
	/*! sample count to increment by on each cycle */
	unsigned int samples;
	/*! current sample count based on samples parameter */
	unsigned int samplecount;
	/*! the timer interface provided from a loadable module */
	struct switch_timer_interface *timer_interface;
	/*! the timer's memory pool */
	switch_memory_pool *memory_pool;
	/*! private data for loadable modules to store information */
	void *private_info;
};

/*! \brief A table of functions that a timer module implements */
struct switch_timer_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to allocate the timer */
	switch_status (*timer_init)(switch_timer *);
	/*! function to wait for one cycle to pass */
	switch_status (*timer_next)(switch_timer *);
	/*! function to deallocate the timer */
	switch_status (*timer_destroy)(switch_timer *);
	const struct switch_timer_interface *next;
};

/*! \brief Abstract interface to a dialplan module */
struct switch_dialplan_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! the function to read an extension and set a channels dialpan */
	switch_dialplan_hunt_function hunt_function;
	const struct switch_dialplan_interface *next;
};

/*! \brief Abstract interface to a file format module */
struct switch_file_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the file */
	switch_status (*file_open)(switch_file_handle *, char *file_path);
	/*! function to close the file */
	switch_status (*file_close)(switch_file_handle *);
	/*! function to read from the file */
	switch_status (*file_read)(switch_file_handle *, void *data, switch_size_t *len);
	/*! function to write from the file */
	switch_status (*file_write)(switch_file_handle *, void *data, switch_size_t *len);
	/*! function to seek to a certian position in the file */
	switch_status (*file_seek)(switch_file_handle *, unsigned int *cur_pos, int64_t samples, int whence);
	/*! list of supported file extensions */
	char **extens;
	const struct switch_file_interface *next;
};

/*! an abstract representation of a file handle (some parameters based on compat with libsndfile) */
struct switch_file_handle {
	/*! the interface of the module that implemented the current file type */
	const struct switch_file_interface *file_interface;
	/*! flags to control behaviour */
	uint32_t flags;
	/*! a file descriptor if neceessary */
	switch_file_t *fd;
	/*! samples position of the handle */
	unsigned int samples;
	/*! the current samplerate */
	unsigned int samplerate;
	/*! the number of channels */
	unsigned int channels;
	/*! integer representation of the format */
	unsigned int format;
	/*! integer representation of the sections */
	unsigned int sections;
	/*! is the file seekable */
	int seekable;
	/*! the sample count of the file */
	unsigned int sample_count;
	/*! the speed of the file playback*/
	int speed;
	/*! the handle's memory pool */
	switch_memory_pool *memory_pool;
	/*! private data for the format module to store handle specific info */
	void *private_info;
	int64_t pos;
	switch_buffer *audio_buffer;
};


/*! \brief Abstract interface to a speech module */
struct switch_speech_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the speech interface */
	switch_status (*speech_open)(switch_speech_handle *sh,
								 char *voice_name, 
								 int rate,
								 switch_speech_flag *flags);
	/*! function to close the speech interface */
	switch_status (*speech_close)(switch_speech_handle *, switch_speech_flag *flags);
	/*! function to feed audio to the ASR*/
	switch_status (*speech_feed_asr)(switch_speech_handle *sh, void *data, unsigned int *len, int rate, switch_speech_flag *flags);
	/*! function to read text from the ASR*/
	switch_status (*speech_interpret_asr)(switch_speech_handle *sh, char *buf, unsigned int buflen, switch_speech_flag *flags);
	/*! function to feed text to the TTS*/
	switch_status (*speech_feed_tts)(switch_speech_handle *sh, char *text, switch_speech_flag *flags);
	/*! function to read audio from the TTS*/
	switch_status (*speech_read_tts)(switch_speech_handle *sh,
									 void *data,
									 switch_size_t *datalen,
									 switch_size_t *rate,
									 switch_speech_flag *flags);

	const struct switch_speech_interface *next;
};


/*! an abstract representation of a asr/tts speech interface. */
struct switch_speech_handle {
	/*! the interface of the module that implemented the current speech interface */
	const struct switch_speech_interface *speech_interface;
	/*! flags to control behaviour */
	uint32_t flags;

	/*! the handle's memory pool */
	switch_memory_pool *memory_pool;
	/*! private data for the format module to store handle specific info */
	void *private_info;
};



/*! \brief Abstract interface to a directory module */
struct switch_directory_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the directory interface */
	switch_status (*directory_open)(switch_directory_handle *dh, char *source, char *dsn, char *passwd);
	/*! function to close the directory interface */
	switch_status (*directory_close)(switch_directory_handle *dh);
	/*! function to query the directory interface */
	switch_status (*directory_query)(switch_directory_handle *dh, char *base, char *query);
	/*! function to advance to the next record */
	switch_status (*directory_next)(switch_directory_handle *dh);
	/*! function to advance to the next name/value pair in the current record */
	switch_status (*directory_next_pair)(switch_directory_handle *dh, char **var, char **val);
	
	const struct switch_directory_interface *next;
};

/*! an abstract representation of a directory interface. */
struct switch_directory_handle {
	/*! the interface of the module that implemented the current directory interface */
	const struct switch_directory_interface *directory_interface;
	/*! flags to control behaviour */
	uint32_t flags;

	/*! the handle's memory pool */
	switch_memory_pool *memory_pool;
	/*! private data for the format module to store handle specific info */
	void *private_info;
};


/* nobody has more setting than speex so we will let them set the standard */
/*! \brief Various codec settings (currently only relevant to speex) */
struct switch_codec_settings {
	/*! desired quality */
	int quality;
	/*! desired complexity */
	int complexity;
	/*! desired enhancement */
	int enhancement;
	/*! desired vad level */
	int vad;
	/*! desired vbr level */
	int vbr;
	/*! desired vbr quality */
	float vbr_quality;
	/*! desired abr level */
	int abr;
	/*! desired dtx setting */
	int dtx;
	/*! desired preprocessor settings */
	int preproc;
	/*! preprocessor vad settings */
	int pp_vad;
	/*! preprocessor gain control settings */
	int pp_agc;
	/*! preprocessor gain level */
	float pp_agc_level;
	/*! preprocessor denoise level */
	int pp_denoise;
	/*! preprocessor dereverb settings */
	int pp_dereverb;
	/*! preprocessor dereverb decay level */
	float pp_dereverb_decay;
	/*! preprocessor dereverb level */
	float pp_dereverb_level;
};

/*! an abstract handle to a codec module */
struct switch_codec {
	/*! the codec interface table this handle uses */
	const struct switch_codec_interface *codec_interface;
	/*! the specific implementation of the above codec */
	const struct switch_codec_implementation *implementation;
	/*! codec settings for this handle */
	struct switch_codec_settings codec_settings;
	/*! flags to modify behaviour */
	uint32_t flags;
	/*! the handle's memory pool*/
	switch_memory_pool *memory_pool;
	/*! private data for the codec module to store handle specific info */
	void *private_info;
};

/*! \brief A table of settings and callbacks that define a paticular implementation of a codec */
struct switch_codec_implementation {
	/*! samples transferred per second */
	int samples_per_second;
	/*! bits transferred per second */
	int bits_per_second;
	/*! number of microseconds that denote one frame */
	int microseconds_per_frame;
	/*! number of samples that denote one frame */
	int samples_per_frame;
	/*! number of bytes that denote one frame decompressed */
	switch_size_t bytes_per_frame;
	/*! number of bytes that denote one frame compressed */
	int encoded_bytes_per_frame;
	/*! number of channels represented */
	int number_of_channels;
	/*! number of frames to send in one netowrk packet */
	int pref_frames_per_packet;
	/*! max number of frames to send in one network packet */
	int max_frames_per_packet;
	/*! function to initialize a codec handle using this implementation */
	switch_status (*init)(switch_codec *, switch_codec_flag, const switch_codec_settings *codec_settings);
	/*! function to encode raw data into encoded data */
	switch_status (*encode)(switch_codec *codec,
						 switch_codec *other_codec,
						 void *decoded_data,
						 switch_size_t decoded_data_len,
						 int decoded_rate,
						 void *encoded_data,
						 switch_size_t *encoded_data_len,
						 int *encoded_rate,
						 unsigned int *flag);
	/*! function to decode encoded data into raw data */
	switch_status (*decode)(switch_codec *codec,
						 switch_codec *other_codec,
						 void *encoded_data,
						 switch_size_t encoded_data_len,
						 int encoded_rate,
						 void *decoded_data,
						 switch_size_t *decoded_data_len,
						 int *decoded_rate,
						 unsigned int *flag);
	/*! deinitalize a codec handle using this implementation */
	switch_status (*destroy)(switch_codec *);
	const struct switch_codec_implementation *next;
};

/*! \brief Top level module interface to implement a series of codec implementations */
struct switch_codec_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! enumeration defining the type of the codec */
	const switch_codec_type codec_type;
	/*! the IANA code number */
	unsigned int ianacode;
	/*! the IANA code name */
	char *iananame;
	/*! a list of codec implementations related to the codec */
	const switch_codec_implementation *implementations;
	const struct switch_codec_interface *next;
};

/*! \brief A module interface to implement an application */
struct switch_application_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function the application implements */
	switch_application_function application_function;
	/*! the long winded description of the application */
	const char *long_desc;
	/*! the short and sweet description of the application */
	const char *short_desc;
	/*! an example of the application syntax */
	const char *syntax;
	const struct switch_application_interface *next;
};

/*! \brief A module interface to implement an api function */
struct switch_api_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! a description of the api function */
	const char *desc;
	/*! function the api call uses */
	switch_api_function function;
	const struct switch_api_interface *next;
};

#ifdef __cplusplus
}
#endif

#endif
