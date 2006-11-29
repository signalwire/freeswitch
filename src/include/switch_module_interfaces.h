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

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

/*! \brief A table of functions to execute at various states 
*/
struct switch_state_handler_table {
	/*! executed when the state changes to init */
	switch_state_handler_t on_init;
	/*! executed when the state changes to ring */
	switch_state_handler_t on_ring;
	/*! executed when the state changes to execute */
	switch_state_handler_t on_execute;
	/*! executed when the state changes to hangup */
	switch_state_handler_t on_hangup;
	/*! executed when the state changes to loopback*/
	switch_state_handler_t on_loopback;
	/*! executed when the state changes to transmit*/
	switch_state_handler_t on_transmit;
	/*! executed when the state changes to hold*/
	switch_state_handler_t on_hold;
	/*! executed when the state changes to hibernate*/
	switch_state_handler_t on_hibernate;
};

struct switch_stream_handle {
	switch_stream_handle_write_function_t write_function;
	void *data;
	void *end;
	switch_size_t data_size;
	switch_size_t data_len;
	switch_size_t alloc_len;
	switch_size_t alloc_chunk;
	switch_event_t *event;
};

/*! \brief Node in which to store custom outgoing channel callback hooks */
struct switch_io_event_hook_outgoing_channel {
	/*! the outgoing channel callback hook*/
	switch_outgoing_channel_hook_t outgoing_channel;
	struct switch_io_event_hook_outgoing_channel *next;
};

/*! \brief Node in which to store custom answer channel callback hooks */
struct switch_io_event_hook_answer_channel {
	/*! the answer channel callback hook*/
	switch_answer_channel_hook_t answer_channel;
	struct switch_io_event_hook_answer_channel *next;
};

/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_receive_message {
	/*! the answer channel callback hook*/
	switch_receive_message_hook_t receive_message;
	struct switch_io_event_hook_receive_message *next;
};

/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_receive_event {
	/*! the answer channel callback hook*/
	switch_receive_event_hook_t receive_event;
	struct switch_io_event_hook_receive_event *next;
};

/*! \brief Node in which to store custom read frame channel callback hooks */
struct switch_io_event_hook_read_frame {
	/*! the read frame channel callback hook*/
	switch_read_frame_hook_t read_frame;
	struct switch_io_event_hook_read_frame *next;
};

/*! \brief Node in which to store custom write_frame channel callback hooks */
struct switch_io_event_hook_write_frame {
	/*! the write_frame channel callback hook*/
	switch_write_frame_hook_t write_frame;
	struct switch_io_event_hook_write_frame *next;
};

/*! \brief Node in which to store custom kill channel callback hooks */
struct switch_io_event_hook_kill_channel {
	/*! the kill channel callback hook*/
	switch_kill_channel_hook_t kill_channel;
	struct switch_io_event_hook_kill_channel *next;
};

/*! \brief Node in which to store custom waitfor read channel callback hooks */
struct switch_io_event_hook_waitfor_read {
	/*! the waitfor read channel callback hook*/
	switch_waitfor_read_hook_t waitfor_read;
	struct switch_io_event_hook_waitfor_read *next;
};

/*! \brief Node in which to store custom waitfor write channel callback hooks */
struct switch_io_event_hook_waitfor_write {
	/*! the waitfor write channel callback hook*/
	switch_waitfor_write_hook_t waitfor_write;
	struct switch_io_event_hook_waitfor_write *next;
};

/*! \brief Node in which to store custom send dtmf channel callback hooks */
struct switch_io_event_hook_send_dtmf {
	/*! the send dtmf channel callback hook*/
	switch_send_dtmf_hook_t send_dtmf;
	struct switch_io_event_hook_send_dtmf *next;
};

/*! \brief A table of lists of io_event_hooks to store the event hooks associated with a session */
struct switch_io_event_hooks {
	/*! a list of outgoing channel hooks */
	switch_io_event_hook_outgoing_channel_t *outgoing_channel;
	/*! a list of answer channel hooks */
	switch_io_event_hook_answer_channel_t *answer_channel;
	/*! a list of receive message hooks */
	switch_io_event_hook_receive_message_t *receive_message;
	/*! a list of queue message hooks */
	switch_io_event_hook_receive_event_t *receive_event;
	/*! a list of read frame hooks */
	switch_io_event_hook_read_frame_t *read_frame;
	/*! a list of write frame hooks */
	switch_io_event_hook_write_frame_t *write_frame;
	/*! a list of kill channel hooks */
	switch_io_event_hook_kill_channel_t *kill_channel;
	/*! a list of wait for read hooks */
	switch_io_event_hook_waitfor_read_t *waitfor_read;
	/*! a list of wait for write hooks */
	switch_io_event_hook_waitfor_write_t *waitfor_write;
	/*! a list of send dtmf hooks */
	switch_io_event_hook_send_dtmf_t *send_dtmf;
};

/*! \brief A table of i/o routines that an endpoint interface can implement */
struct switch_io_routines {
	/*! creates an outgoing session from given session, caller profile */
	switch_status_t (*outgoing_channel)(switch_core_session_t *, switch_caller_profile_t *, switch_core_session_t **, switch_memory_pool_t *);
	/*! answers the given session's channel */
	switch_status_t (*answer_channel)(switch_core_session_t *);
	/*! read a frame from a session */
	switch_status_t (*read_frame)(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
	/*! write a frame to a session */
	switch_status_t (*write_frame)(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
	/*! send a kill signal to the session's channel */
	switch_status_t (*kill_channel)(switch_core_session_t *, int);
	/*! wait for the session's channel to be ready to read audio */
	switch_status_t (*waitfor_read)(switch_core_session_t *, int, int);
	/*! wait for the session's channel to be ready to write audio */
	switch_status_t (*waitfor_write)(switch_core_session_t *, int, int);
	/*! send a string of DTMF digits to a session's channel */
	switch_status_t (*send_dtmf)(switch_core_session_t *, char *);
	/*! receive a message from another session*/
	switch_status_t (*receive_message)(switch_core_session_t *, switch_core_session_message_t *);
	/*! queue a message for another session*/
	switch_status_t (*receive_event)(switch_core_session_t *, switch_event_t *);
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
	const switch_io_routines_t *io_routines;

	/*! state machine methods */
	const switch_state_handler_table_t *state_handler;

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
	switch_timer_interface_t *timer_interface;
	/*! the timer's memory pool */
	switch_memory_pool_t *memory_pool;
	/*! private data for loadable modules to store information */
	void *private_info;
};

/*! \brief A table of functions that a timer module implements */
struct switch_timer_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to allocate the timer */
	switch_status_t (*timer_init)(switch_timer_t *);
	/*! function to wait for one cycle to pass */
	switch_status_t (*timer_next)(switch_timer_t *);
	/*! function to step the timer one step */
	switch_status_t (*timer_step)(switch_timer_t *);
	/*! function to check if the current step has expired */
	switch_status_t (*timer_check)(switch_timer_t *);
	/*! function to deallocate the timer */
	switch_status_t (*timer_destroy)(switch_timer_t *);
	const struct switch_timer_interface *next;
};

/*! \brief Abstract interface to a dialplan module */
struct switch_dialplan_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! the function to read an extension and set a channels dialpan */
	switch_dialplan_hunt_function_t hunt_function;
	const struct switch_dialplan_interface *next;
};

/*! \brief Abstract interface to a file format module */
struct switch_file_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the file */
	switch_status_t (*file_open)(switch_file_handle_t *, char *file_path);
	/*! function to close the file */
	switch_status_t (*file_close)(switch_file_handle_t *);
	/*! function to read from the file */
	switch_status_t (*file_read)(switch_file_handle_t *, void *data, switch_size_t *len);
	/*! function to write from the file */
	switch_status_t (*file_write)(switch_file_handle_t *, void *data, switch_size_t *len);
	/*! function to seek to a certian position in the file */
	switch_status_t (*file_seek)(switch_file_handle_t *, unsigned int *cur_pos, int64_t samples, int whence);
	/*! function to set meta data */
	switch_status_t (*file_set_string)(switch_file_handle_t *fh, switch_audio_col_t col, const char *string);
	/*! function to get meta data */
	switch_status_t (*file_get_string)(switch_file_handle_t *fh, switch_audio_col_t col, const char **string);
	/*! list of supported file extensions */
	char **extens;
	const struct switch_file_interface *next;
};

/*! an abstract representation of a file handle (some parameters based on compat with libsndfile) */
struct switch_file_handle {
	/*! the interface of the module that implemented the current file type */
	const switch_file_interface_t *file_interface;
	/*! flags to control behaviour */
	uint32_t flags;
	/*! a file descriptor if neceessary */
	switch_file_t *fd;
	/*! samples position of the handle */
	unsigned int samples;
	/*! the current samplerate */
	uint32_t samplerate;
	/*! the number of channels */
	uint8_t channels;
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
	switch_memory_pool_t *memory_pool;
	/*! private data for the format module to store handle specific info */
	void *private_info;
	int64_t pos;
	switch_buffer_t *audio_buffer;
    uint32_t thresh;
    uint32_t silence_hits;
};

/*! \brief Abstract interface to an asr module */
struct switch_asr_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the asr interface */
	switch_status_t (*asr_open)(switch_asr_handle_t *ah,
								char *codec,
								int rate,
								char *dest,
								switch_asr_flag_t *flags);
	/*! function to load a grammar to the asr interface */
	switch_status_t (*asr_load_grammar)(switch_asr_handle_t *ah, char *grammar, char *path);
	/*! function to unload a grammar to the asr interface */
	switch_status_t (*asr_unload_grammar)(switch_asr_handle_t *ah, char *grammar);
	/*! function to close the asr interface */
	switch_status_t (*asr_close)(switch_asr_handle_t *ah, switch_asr_flag_t *flags);
	/*! function to feed audio to the ASR*/
	switch_status_t (*asr_feed)(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags);
	/*! function to resume the ASR*/
	switch_status_t (*asr_resume)(switch_asr_handle_t *ah);
	/*! function to pause the ASR*/
	switch_status_t (*asr_pause)(switch_asr_handle_t *ah);
	/*! function to read results from the ASR*/
	switch_status_t (*asr_check_results)(switch_asr_handle_t *ah, switch_asr_flag_t *flags);
	/*! function to read results from the ASR*/
	switch_status_t (*asr_get_results)(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags);
	const struct switch_asr_interface *next;
};

/*! an abstract representation of an asr speech interface. */
struct switch_asr_handle {
	/*! the interface of the module that implemented the current speech interface */
	const switch_asr_interface_t *asr_interface;
	/*! flags to control behaviour */
	uint32_t flags;
	/*! The Name*/
	char *name;
	/*! The Codec*/
	char *codec;
	/*! The Rate*/
	uint32_t rate;
	char *grammar;
	/*! the handle's memory pool */
	switch_memory_pool_t *memory_pool;
	/*! private data for the format module to store handle specific info */
	void *private_info;
};

/*! \brief Abstract interface to a speech module */
struct switch_speech_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the speech interface */
	switch_status_t (*speech_open)(switch_speech_handle_t *sh,
								 char *voice_name, 
								 int rate,
								 switch_speech_flag_t *flags);
	/*! function to close the speech interface */
	switch_status_t (*speech_close)(switch_speech_handle_t *, switch_speech_flag_t *flags);
	/*! function to feed audio to the ASR*/
	switch_status_t (*speech_feed_tts)(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags);
	/*! function to read audio from the TTS*/
	switch_status_t (*speech_read_tts)(switch_speech_handle_t *sh,
									 void *data,
									 switch_size_t *datalen,
									 uint32_t *rate,
									 switch_speech_flag_t *flags);
	void (*speech_flush_tts)(switch_speech_handle_t *sh);
	void (*speech_text_param_tts)(switch_speech_handle_t *sh, char *param, char *val);
	void (*speech_numeric_param_tts)(switch_speech_handle_t *sh, char *param, int val);
	void (*speech_float_param_tts)(switch_speech_handle_t *sh, char *param, double val);

	const struct switch_speech_interface *next;
};


/*! an abstract representation of a asr/tts speech interface. */
struct switch_speech_handle {
	/*! the interface of the module that implemented the current speech interface */
	const switch_speech_interface_t *speech_interface;
	/*! flags to control behaviour */
	uint32_t flags;
	/*! The Name*/
	char *name;
	/*! The Rate*/
	uint32_t rate;
	uint32_t speed;
	char voice[80];
	char engine[80];
	/*! the handle's memory pool */
	switch_memory_pool_t *memory_pool;
	/*! private data for the format module to store handle specific info */
	void *private_info;
};


/*! \brief Abstract interface to a chat module */
struct switch_chat_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the directory interface */
	switch_status_t (*chat_send)(char *proto, char *from, char *to, char *subject, char *body, char *hint);
	const struct switch_chat_interface *next;
};

/*! \brief Abstract interface to a directory module */
struct switch_directory_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to open the directory interface */
	switch_status_t (*directory_open)(switch_directory_handle_t *dh, char *source, char *dsn, char *passwd);
	/*! function to close the directory interface */
	switch_status_t (*directory_close)(switch_directory_handle_t *dh);
	/*! function to query the directory interface */
	switch_status_t (*directory_query)(switch_directory_handle_t *dh, char *base, char *query);
	/*! function to advance to the next record */
	switch_status_t (*directory_next)(switch_directory_handle_t *dh);
	/*! function to advance to the next name/value pair in the current record */
	switch_status_t (*directory_next_pair)(switch_directory_handle_t *dh, char **var, char **val);
	
	const struct switch_directory_interface *next;
};

/*! an abstract representation of a directory interface. */
struct switch_directory_handle {
	/*! the interface of the module that implemented the current directory interface */
	const switch_directory_interface_t *directory_interface;
	/*! flags to control behaviour */
	uint32_t flags;

	/*! the handle's memory pool */
	switch_memory_pool_t *memory_pool;
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
	const switch_codec_interface_t *codec_interface;
	/*! the specific implementation of the above codec */
	const switch_codec_implementation_t *implementation;
	/*! fmtp line from remote sdp */
	char *fmtp_in;
	/*! fmtp line for local sdp */
	char *fmtp_out;
	/*! codec settings for this handle */
	switch_codec_settings_t codec_settings;
	/*! flags to modify behaviour */
	uint32_t flags;
	/*! the handle's memory pool*/
	switch_memory_pool_t *memory_pool;
	/*! private data for the codec module to store handle specific info */
	void *private_info;
};

/*! \brief A table of settings and callbacks that define a paticular implementation of a codec */
struct switch_codec_implementation {
	/*! enumeration defining the type of the codec */
	const switch_codec_type_t codec_type;
	/*! the IANA code number */
	switch_payload_t ianacode;
	/*! the IANA code name */
	char *iananame;
	/*! default fmtp to send (can be overridden by the init function) */
	char *fmtp;
	/*! samples transferred per second */
	uint32_t samples_per_second;
	/*! bits transferred per second */
	int bits_per_second;
	/*! number of microseconds that denote one frame */
	int microseconds_per_frame;
	/*! number of samples that denote one frame */
	uint32_t samples_per_frame;
	/*! number of bytes that denote one frame decompressed */
	uint32_t bytes_per_frame;
	/*! number of bytes that denote one frame compressed */
	uint32_t encoded_bytes_per_frame;
	/*! number of channels represented */
	uint8_t number_of_channels;
	/*! number of frames to send in one netowrk packet */
	int pref_frames_per_packet;
	/*! max number of frames to send in one network packet */
	int max_frames_per_packet;
	/*! function to initialize a codec handle using this implementation */
	switch_status_t (*init)(switch_codec_t *, switch_codec_flag_t, const switch_codec_settings_t *codec_settings);
	/*! function to encode raw data into encoded data */
	switch_status_t (*encode)(switch_codec_t *codec,
						 switch_codec_t *other_codec,
						 void *decoded_data,
						 uint32_t decoded_data_len,
						 uint32_t decoded_rate,
						 void *encoded_data,
						 uint32_t *encoded_data_len,
						 uint32_t *encoded_rate,
						 unsigned int *flag);
	/*! function to decode encoded data into raw data */
	switch_status_t (*decode)(switch_codec_t *codec,
						 switch_codec_t *other_codec,
						 void *encoded_data,
						 uint32_t encoded_data_len,
						 uint32_t encoded_rate,
						 void *decoded_data,
						 uint32_t *decoded_data_len,
						 uint32_t *decoded_rate,
						 unsigned int *flag);
	/*! deinitalize a codec handle using this implementation */
	switch_status_t (*destroy)(switch_codec_t *);
	const struct switch_codec_implementation *next;
};

/*! \brief Top level module interface to implement a series of codec implementations */
struct switch_codec_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! a list of codec implementations related to the codec */
	const switch_codec_implementation_t *implementations;
	const struct switch_codec_interface *next;
};

/*! \brief A module interface to implement an application */
struct switch_application_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function the application implements */
	switch_application_function_t application_function;
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
	switch_api_function_t function;
	/*! an example of the application syntax */
	const char *syntax;
	const struct switch_api_interface *next;
};

SWITCH_END_EXTERN_C

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
