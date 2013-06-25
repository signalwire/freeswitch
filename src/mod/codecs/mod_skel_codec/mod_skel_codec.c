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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / SKEL codec module
 *
 * The Initial Developer of the Original Code is
 * Moises Silva <moy@sangoma.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Moises Silva <moy@sangoma.com>
 *
 * mod_skel_codec -- FreeSWITCH skeleton codec
 *
 */

/*
 * Other source of reference for module building: 
 * src/mod/applications/mod_skel/mod_skel.c
 * http://files.freeswitch.org/cluecon_2009/presentations/Silva_FreeSWITCH_Modules_For_Asterisk_Devs.ppt
 * You can load this codec with this command:
 * freeswitch@localhost> load mod_skel_codec
 * 2009-09-23 12:56:55.616573 [CONSOLE] switch_loadable_module.c:889 Successfully Loaded [mod_skel_codec]
 * 2009-09-23 12:56:55.616573 [NOTICE] switch_loadable_module.c:182 Adding Codec 'SKEL' (SKEL 8.0k) 8000hz 20ms
 * 2009-09-23 12:56:55.616573 [NOTICE] switch_loadable_module.c:270 Adding API Function 'skel_sayhi'
 *
 * Then test the API with:
 * freeswitch@localhost.localdomain> skel_sayhi
 * API CALL [skel_sayhi()] output:
 * Hello, I am the skeleton codec and I am not useful for users ... I feel so depressed :-(
 * */
#include <switch.h>

/* prototype of the module loading function, this function will be called when loading the shared object/DLL into FS core */
SWITCH_MODULE_LOAD_FUNCTION(mod_skel_codec_load);

/* Module definition structure, this macro does something like
 * static const char modname[] = "mod_skel_codec";
 * switch_loadable_module_function_table_t mod_skel_codec_module_interface = { load, shutdown, runtime, flags };
 * */
SWITCH_MODULE_DEFINITION(mod_skel_codec, mod_skel_codec_load, NULL, NULL);

/* Typically you will have a context structure to track your resources */
struct skel_context {
	/* dummy counters to check how many times the encode/decode routines are called */
	int32_t encodes;
	int32_t decodes;
};

static switch_status_t switch_skel_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct skel_context *context = NULL;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Skel init called.\n");

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	/* alloc memory for your internal data structure, no need to free it on destroy, since is allocated from the pool and the core will take care of it */
	if (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context)))) {
		return SWITCH_STATUS_FALSE;
	}

	codec->private_info = context;
	context->encodes = 0;
	context->decodes = 0;

	return SWITCH_STATUS_SUCCESS;
}

/* see src/switch_core_io.c (search switch_core_codec_encode/decode) for more details in expected return codes for the codec interface */
/* sample return codes, success and error are the typical:
 * SWITCH_STATUS_SUCCESS, success
 * SWITCH_STATUS_FALSE, error
 * SWITCH_STATUS_RESAMPLE, resampling needed
 * SWITCH_STATUS_NOOP, NOOP indicates that the audio in is already the same as the audio out, so no conversion was necessary.
 * SWITCH_STATUS_NOT_INITALIZED, failure to init, you can use this to init on first usage and no in switch_skel_init?
 * */
static switch_status_t switch_skel_encode(switch_codec_t *codec, switch_codec_t *other_codec,	/* codec that was used by the other side */
										  void *decoded_data,	/* decoded data that we must encode */
										  uint32_t decoded_data_len /* decoded data length */ ,
										  uint32_t decoded_rate /* rate of the decoded data */ ,
										  void *encoded_data,	/* here we will store the encoded data */
										  uint32_t *encoded_data_len,	/* here we will set the length of the encoded data */
										  uint32_t *encoded_rate /* here we will set the rate of the encoded data */ ,
										  unsigned int *flag /* frame flag, see switch_frame_flag_enum_t */ )
{
	struct skel_context *context = codec->private_info;
	/* FS core checks the actual samples per second and microseconds per packet to determine the buffer size in the worst case scenario, no need to check
	 * whether the buffer passed in by the core (encoded_data) will be big enough */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Skel encode called.\n");
	memcpy(encoded_data, decoded_data, decoded_data_len);
	*encoded_data_len = decoded_data_len;
	*encoded_rate = decoded_rate;
	context->encodes++;
	return SWITCH_STATUS_SUCCESS;
}

/* sample return codes, success and error are the typical:
 * SWITCH_STATUS_SUCCESS, success
 * SWITCH_STATUS_FALSE, error
 * SWITCH_STATUS_RESAMPLE, resampling needed
 * SWITCH_STATUS_BREAK, do nothing else with the frame but dont report error, seems to be only useful for mod_g729
 * SWITCH_STATUS_NOOP, NOOP indicates that the audio in is already the same as the audio out, so no conversion was necessary.
 * SWITCH_STATUS_NOT_INITALIZED, failure to init, you can use this to init on first usage and no in switch_skel_init?
 * */
static switch_status_t switch_skel_decode(switch_codec_t *codec,	/* codec session handle */
										  switch_codec_t *other_codec,	/* what is this? */
										  void *encoded_data,	/* data that we must decode into slinear and put it in decoded_data */
										  uint32_t encoded_data_len,	/* length in bytes of the encoded data */
										  uint32_t encoded_rate,	/* at which rate was the data encoded */
										  void *decoded_data,	/* buffer where we must put the decoded data */
										  uint32_t *decoded_data_len,	/* we must set this value to the size of the decoded data */
										  uint32_t *decoded_rate,	/* rate of the decoded data */
										  unsigned int *flag /* frame flag, see switch_frame_flag_enum_t */ )
{
	struct skel_context *context = codec->private_info;
	/* FS core checks the actual samples per second and microseconds per packet to determine the buffer size in the worst case scenario, no need to check
	 * whether the buffer passed in by the core will be enough */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Skel decode called.\n");
	memcpy(decoded_data, encoded_data, encoded_data_len);
	*decoded_data_len = encoded_data_len;
	*decoded_rate = encoded_rate;
	context->decodes++;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_skel_destroy(switch_codec_t *codec)
{
	/* things that you may do here is closing files, sockets or other resources used during the codec session 
	 * no need to free memory allocated from the pool though, the owner of the pool takes care of that */
	struct skel_context *context = codec->private_info;
	context->encodes = 0;
	context->decodes = 0;		/* silly, context was allocated in the pool and therefore will be destroyed soon, exact time is not guaranteed though */
	return SWITCH_STATUS_SUCCESS;
}

/* A standard API registered by the this codec module, APIs can be executed from CLI, mod_event_socket, or many other interfaces 
 * any information or actions you want to publish for users on demand is typically exposed through an API */
SWITCH_STANDARD_API(skel_sayhi)
/* static switch_status_t skel_sayhi(const char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream) */
{
	stream->write_function(stream, "Hello, I am the skeleton codec and I am not useful for users ... I feel so depressed :-( \n");
	return SWITCH_STATUS_SUCCESS;
}

/* switch_status_t mod_skel_codec_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_skel_codec_load)
{
	/* the codec interface that will be registered with the core */
	switch_codec_interface_t *codec_interface;

	/* the API interface, only needed if you wish to register APIs 
	 * (like commands exposed through CLI, mod_event_socket or any other FS interface capable of executing APIs) */
	switch_api_interface_t *api_interface;

	/* The core gives us a blank module interface pointer and a pool of memory, we allocate memory from that pool to create our module interface 
	 * and set the pointer to that chunk of allocated memory */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* SWITCH_ADD_CODEC allocates a codec interface structure from the pool the core gave us and adds it to the internal interface list the core keeps, 
	 * gets a codec id and set the given codec name to it.
	 * At this point there is an empty shell codec interface registered, but not yet implementations */
	SWITCH_ADD_CODEC(codec_interface, "SKEL 8.0k");	/* 8.0kbit */

	/* Now add as many codec implementations as needed, typically this is done inside a for loop where the packetization size is 
	 * incremented (10ms, 20ms, 30ms etc) as needed */
	switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
										 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 0,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
										 "SKEL",	/* the IANA code name */
										 NULL,	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 8000,	/* bits transferred per second */
										 20000,	/* for 20ms (milliseconds) frames, 20,000 microseconds and so on, you can register the same codec with different packetization */
										 160,	/* number of samples per frame, for this dummy implementation is 160, which is the number of samples in 20ms at 8000 samples per second */
										 320,	/* number of bytes per frame decompressed */
										 320,	/* number of bytes per frame compressed, since we dont really compress anything, is the same number as per frame decompressed */
										 1,	/* number of channels represented */
										 20,	/* number of frames per network packet */
										 switch_skel_init,	/* function to initialize a codec session using this implementation */
										 switch_skel_encode,	/* function to encode slinear data into encoded data */
										 switch_skel_decode,	/* function to decode encoded data into slinear data */
										 switch_skel_destroy);	/* deinitalize a codec handle using this implementation */

	SWITCH_ADD_API(api_interface, "skel_sayhi", "Skel Codec Says Hi", skel_sayhi, NULL);
	switch_console_set_complete("add skel_sayhi");	/* For CLI completion */
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

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
