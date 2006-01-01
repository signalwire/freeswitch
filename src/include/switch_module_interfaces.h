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
*/
#ifndef SWITCH_MODULE_INTERFACES_H
#define SWITCH_MODULE_INTERFACES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

/* A table of functions to execute at various states */
struct switch_event_handler_table {
	switch_event_handler on_init;
	switch_event_handler on_ring;
	switch_event_handler on_execute;
	switch_event_handler on_hangup;
	switch_event_handler on_loopback;
	switch_event_handler on_transmit;
};

struct switch_io_event_hook_outgoing_channel {
	switch_outgoing_channel_hook outgoing_channel;
	struct switch_io_event_hook_outgoing_channel *next;
};

struct switch_io_event_hook_answer_channel {
	switch_answer_channel_hook answer_channel;
	struct switch_io_event_hook_answer_channel *next;
};

struct switch_io_event_hook_read_frame {
	switch_read_frame_hook read_frame;
	struct switch_io_event_hook_read_frame *next;
};

struct switch_io_event_hook_write_frame {
	switch_write_frame_hook write_frame;
	struct switch_io_event_hook_write_frame *next;
};

struct switch_io_event_hook_kill_channel {
	switch_kill_channel_hook kill_channel;
	struct switch_io_event_hook_kill_channel *next;
};

struct switch_io_event_hook_waitfor_read {
	switch_waitfor_read_hook waitfor_read;
	struct switch_io_event_hook_waitfor_read *next;
};

struct switch_io_event_hook_waitfor_write {
	switch_waitfor_write_hook waitfor_write;
	struct switch_io_event_hook_waitfor_write *next;
};

struct switch_io_event_hook_send_dtmf {
	switch_send_dtmf_hook send_dtmf;
	struct switch_io_event_hook_send_dtmf *next;
};

struct switch_io_event_hooks {
	struct switch_io_event_hook_outgoing_channel *outgoing_channel;
	struct switch_io_event_hook_answer_channel *answer_channel;
	struct switch_io_event_hook_read_frame *read_frame;
	struct switch_io_event_hook_write_frame *write_frame;
	struct switch_io_event_hook_kill_channel *kill_channel;
	struct switch_io_event_hook_waitfor_read *waitfor_read;
	struct switch_io_event_hook_waitfor_write *waitfor_write;
	struct switch_io_event_hook_send_dtmf *send_dtmf;
};

struct switch_io_routines {
	switch_status (*outgoing_channel)(switch_core_session *, switch_caller_profile *, switch_core_session **);
	switch_status (*answer_channel)(switch_core_session *);
	switch_status (*read_frame)(switch_core_session *, switch_frame **, int, switch_io_flag);
	switch_status (*write_frame)(switch_core_session *, switch_frame *, int, switch_io_flag);
	switch_status (*kill_channel)(switch_core_session *, int);
	switch_status (*waitfor_read)(switch_core_session *, int);
	switch_status (*waitfor_write)(switch_core_session *, int);
	switch_status (*send_dtmf)(switch_core_session *, char *);
};

/*
  This is the glue between the abstract idea of a "channel" and what is really going on under the
  hood.	 Each endpoint module fills out one of these tables and makes it available when a channel
  is created of it's paticular type.
*/

struct switch_endpoint_interface {
	/* the interface's name */
	const char *interface_name;

	/* channel abstraction methods */
	const switch_io_routines *io_routines;

	/* state machine methods */
	const switch_event_handler_table *event_handlers;

	/* private information */
	void *private;

	/* to facilitate linking */
	const struct switch_endpoint_interface *next;
};

struct switch_timer {
	int interval;
	unsigned int flags;
	unsigned int samples;
	unsigned int samplecount;
	struct switch_timer_interface *timer_interface;
	switch_memory_pool *memory_pool;
	void *private;
};

struct switch_timer_interface {
	const char *interface_name;
	switch_status (*timer_init)(switch_timer *);
	switch_status (*timer_next)(switch_timer *);
	switch_status (*timer_destroy)(switch_timer *);
	const struct switch_timer_interface *next;
};

struct switch_dialplan_interface {
	const char *interface_name;
	switch_dialplan_hunt_function hunt_function;
	const struct switch_dialplan_interface *next;
};

struct switch_file_interface {
	const char *interface_name;
	switch_status (*file_open)(switch_file_handle *, char *file_path);
	switch_status (*file_close)(switch_file_handle *);
	switch_status (*file_read)(switch_file_handle *, void *data, unsigned int *len);
	switch_status (*file_write)(switch_file_handle *, void *data, unsigned int *len);
	switch_status (*file_seek)(switch_file_handle *, unsigned int *cur_pos, unsigned int samples, int whence);
	char **extens;
	const struct switch_file_interface *next;
};

struct switch_file_handle {
	const struct switch_file_interface *file_interface;
	unsigned int flags;
	switch_file_t *fd;
	unsigned int samples;
	unsigned int samplerate;
	unsigned int channels;
	unsigned int format;
	unsigned int sections;
	int seekable;
	unsigned int sample_count;
	switch_memory_pool *memory_pool;
	void *private;
};


/* nobody has more setting than speex so we will let them set the standard */
struct switch_codec_settings {
	int quality;
	int complexity;
	int enhancement;
	int vad;
	int vbr;
	float vbr_quality;
	int abr;
	int dtx;
	int preproc;
	int pp_vad;
	int pp_agc;
	float pp_agc_level;
	int pp_denoise;
	int pp_dereverb;
	float pp_dereverb_decay;
	float pp_dereverb_level;
};

struct switch_codec {
	const struct switch_codec_interface *codec_interface;
	const struct switch_codec_implementation *implementation;
	struct switch_codec_settings codec_settings;
	switch_codec_flag flags;
	switch_memory_pool *memory_pool;
	void *private;
};

struct switch_codec_implementation {
	int samples_per_second;
	int bits_per_second;
	int microseconds_per_frame;
	int samples_per_frame;
	int bytes_per_frame;
	int encoded_bytes_per_frame;
	int number_of_channels;
	int pref_frames_per_packet;
	int max_frames_per_packet;
	switch_status (*init)(switch_codec *, switch_codec_flag, const switch_codec_settings *codec_settings);
	switch_status (*encode)(switch_codec *codec,
						 switch_codec *other_codec,
						 void *decoded_data,
						 size_t decoded_data_len,
						 int decoded_rate,
						 void *encoded_data,
						 size_t *encoded_data_len,
						 int *encoded_rate,
						 unsigned int *flag);
	switch_status (*decode)(switch_codec *codec,
						 switch_codec *other_codec,
						 void *encoded_data,
						 size_t encoded_data_len,
						 int encoded_rate,
						 void *decoded_data,
						 size_t *decoded_data_len,
						 int *decoded_rate,
						 unsigned int *flag);
	switch_status (*destroy)(switch_codec *);
	const struct switch_codec_implementation *next;
};

struct switch_codec_interface {
	const char *interface_name;
	const switch_codec_type codec_type;
	int ianacode;
	char *iananame;
	const switch_codec_implementation *implementations;
	const struct switch_codec_interface *next;
};


struct switch_application_interface {
	const char *interface_name;
	switch_application_function application_function;
	const char *long_desc;
	const char *short_desc;
	const char *syntax;
	const struct switch_application_interface *next;
};

struct switch_api_interface {
	const char *interface_name;
	const char *desc;
	switch_api_function function;
	const struct switch_api_interface *next;
};

#endif
