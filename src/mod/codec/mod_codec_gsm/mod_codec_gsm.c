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
 * Michael Jerris <mike@jerris.com>
 *
 * mod_codec_gsm.c -- gsm Codec Module
 *
 */
#include "switch.h"
#include "gsm.h"

static const char modname[] = "mod_codec_gsm";


static switch_status switch_gsm_init(switch_codec *codec, switch_codec_flag flags, const struct switch_codec_settings *codec_settings)
{
	gsm context;
	int encoding, decoding;
	
	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);
	
	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	} else {
			context = gsm_create();
		}

		codec->private = context;
		
		return SWITCH_STATUS_SUCCESS;
	
}

static switch_status switch_gsm_destroy(switch_codec *codec)
{
	gsm_destroy((gsm)codec->private);
	codec->private = NULL;
	return SWITCH_STATUS_SUCCESS;
}


static switch_status switch_gsm_encode(switch_codec *codec,
								   switch_codec *other_codec,
								   void *decoded_data,
								   size_t decoded_data_len,
								   void *encoded_data,
								   size_t *encoded_data_len,
								   unsigned int *flag)
{
	gsm context = codec->private;
	int cbret = 0;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}
	gsm_encode(context, (void *)decoded_data, (void *)encoded_data);
	*encoded_data_len = 33;

	return SWITCH_STATUS_SUCCESS;
}


static switch_status switch_gsm_decode(switch_codec *codec,
								   switch_codec *other_codec,
								   void *encoded_data,
								   size_t encoded_data_len,
								   void *decoded_data,
								   size_t *decoded_data_len,
								   unsigned int *flag) 
{
	struct gsm_context *context = codec->private;
	
	if (!context) {
		return SWITCH_STATUS_FALSE;
	}


	if (encoded_data_len % 33 == 0) {
		int loops = (int) encoded_data_len / 33;
		char *edp = encoded_data;
		short *ddp = decoded_data;
		int x;
		unsigned int new_len = 0;
		for(x = 0; x < loops && new_len < *decoded_data_len; x++) {
			gsm_decode(&context, ddp, edp, 33);
			ddp += 80;
			edp += 10;
			new_len += 320;
		}
		if (new_len <= *decoded_data_len) {
			*decoded_data_len = new_len;
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "buffer overflow!!!\n");
			return SWITCH_STATUS_FALSE;
		}
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "yo this frame is an odd size [%d]\n", encoded_data_len);
	}
	
	
	return SWITCH_STATUS_SUCCESS;
}

/* Registration */

static const switch_codec_implementation gsm_8k_implementation = {
	/*.samples_per_second*/				8000,
	/*.bits_per_second*/				13200,
	/*.microseconds_per_frame*/			20000,
	/*.samples_per_frame*/				160,
	/*.bytes_per_frame*/				320,
	/*.encoded_bytes_per_frame*/		33,
	/*.number_of_channels*/				1,
	/*.pref_frames_per_packet*/			1,
	/*.max_frames_per_packet*/			7,
	/*.init*/							switch_gsm_init,
	/*.encode*/							switch_gsm_encode,
	/*.decode*/							switch_gsm_decode,
	/*.destroy*/						switch_gsm_destroy,
};

static const switch_codec_interface gsm_codec_interface = {
	/*.interface_name*/					"gsm",
	/*.codec_type*/						SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode*/						3,
	/*.iananame*/						"gsm",
	/*.implementations*/				&gsm_8k_implementation,
};

static switch_loadable_module_interface gsm_module_interface = {
	/*.module_name*/					modname,
	/*.endpoint_interface*/				NULL,
	/*.timer_interface*/				NULL,
	/*.dialplan_interface*/				NULL,
	/*.codec_interface*/				&gsm_codec_interface,
	/*.application_interface*/			NULL
};


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &gsm_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}





