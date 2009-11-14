/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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
 * Brian K. West <brian@freeswitch.org>
 *
 * mod_bv16.c -- BroadVoice32 (BV16) audio codec (http://www.broadcom.com/support/broadvoice/)
 *
 */

#include "switch.h"
#include "bv16.h"
#include "bitpack.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_bv16_load);
SWITCH_MODULE_DEFINITION(mod_bv16, mod_bv16_load, NULL, NULL);

#define FRAME_SIZE 40
#define CODE_SIZE 5

struct bv16_context {
	struct BV16_Encoder_State cs;
	struct BV16_Decoder_State ds;
	struct BV16_Bit_Stream ebs;
	struct BV16_Bit_Stream dbs;	
};

static switch_status_t switch_bv16_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct bv16_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	} 

	if (encoding) {
		Reset_BV16_Encoder(&context->cs);
	}
	
	if (decoding) {
		Reset_BV16_Decoder(&context->ds);
	}
	
	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv16_destroy(switch_codec_t *codec)
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv16_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct bv16_context *context = codec->private_info;
	int16_t *cur_frame = decoded_data;
	uint16_t *target = encoded_data;
	int i, frames = decoded_data_len / (FRAME_SIZE * 2);

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}
	
	*encoded_data_len = 0;
	for (i = 0; i < frames; i++) {
		BV16_Encode(&context->ebs, &context->cs, cur_frame);
		BV16_BitPack(target, &context->ebs);
		*encoded_data_len += CODE_SIZE * 2;
		cur_frame += FRAME_SIZE;
		target += CODE_SIZE;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv16_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct bv16_context *context = codec->private_info;
	uint16_t *cur_frame = encoded_data;
	int16_t *target = decoded_data;
	int i, frames = encoded_data_len / (CODE_SIZE * 2);

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = 0;
	for (i = 0; i < frames; i++) {
		if ((*flag & SFF_PLC)) {
			BV16_PLC(&context->ds, target);
		} else {
			BV16_BitUnPack(cur_frame, &context->dbs);
			BV16_Decode(&context->dbs, &context->ds, target);
		}
		cur_frame += CODE_SIZE;
		target += FRAME_SIZE;
		*decoded_data_len += FRAME_SIZE * 2;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_bv16_load)
{
	switch_codec_interface_t *codec_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	SWITCH_ADD_CODEC(codec_interface, "BroadVoice16 (BV16)"); 

	switch_core_codec_add_implementation(pool,
										 codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 106,						/* the IANA code number */
										 "BV16",					/* the IANA code name */
										 NULL,						/* default fmtp to send (can be overridden by the init function) */
										 8000,						/* samples transferred per second */
										 8000,						/* actual samples transferred per second */
										 16000,						/* bits transferred per second */
										 20000,						/* number of microseconds per frame */
										 160,						/* number of samples per frame */
										 320,						/* number of bytes per frame decompressed */
										 40,						/* number of bytes per frame compressed */
										 1,							/* number of channels represented */
										 1,							/* number of frames per network packet */
										 switch_bv16_init,			/* function to initialize a codec handle using this implementation */
										 switch_bv16_encode,		/* function to encode raw data into encoded data */
										 switch_bv16_decode,		/* function to decode encoded data into raw data */
										 switch_bv16_destroy);		/* deinitalize a codec handle using this implementation */

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
