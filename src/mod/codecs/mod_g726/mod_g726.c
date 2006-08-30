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
 * Brian K. West <brian.west@mac.com>
 * 
 *
 * mod_g726.c -- G726 Codec Module
 *
 */  
#include "switch.h"
#include "g72x.h"
#define BITS_IN_A_BYTE 8

static const char modname[] = "mod_g726";


typedef struct {
	g726_state context;
	uint8_t buf[5];
	uint8_t *ptr;
	uint8_t bits_per_frame;
	uint8_t bits;
	uint8_t bbits;
	uint8_t ecount;
	uint8_t d_bits;
	uint8_t d_bbits;
	uint8_t dcount;
	uint8_t save;
} g726_handle_t;

static switch_status_t switch_g726_init(switch_codec_t *codec, switch_codec_flag_t flags,
									  const switch_codec_settings_t *codec_settings) 
{
	int encoding, decoding;
	g726_handle_t *handle;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(handle = switch_core_alloc(codec->memory_pool, sizeof(*handle))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		g726_init_state(&handle->context);
		codec->private_info = handle;
		handle->bits_per_frame = codec->implementation->bits_per_second / (codec->implementation->samples_per_second);
		handle->ptr = handle->buf;
		return SWITCH_STATUS_SUCCESS;
	}
}


static switch_status_t switch_g726_destroy(switch_codec_t *codec) 
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

typedef int (*encoder_t)(int, int, g726_state *);
typedef int (*decoder_t)(int, int, g726_state *);


static void print_bits(uint8_t byte)
{
	int i;

	for (i=7;i>=0;i--) {
		//for (i=0;i<=7;i++) {
		if(byte & (1 << i)) {
			printf("1");
		} else {
			printf("0");
		}
	}
}






static switch_status_t switch_g726_encode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *decoded_data,

										uint32_t decoded_data_len, 
										uint32_t decoded_rate, 
										void *encoded_data,

										uint32_t *encoded_data_len, 
										uint32_t *encoded_rate, 
										unsigned int *flag) 
{

	g726_handle_t *handle = codec->private_info;
	g726_state *context = &handle->context;
	uint32_t len = codec->implementation->bytes_per_frame;
	uint32_t elen = codec->implementation->encoded_bytes_per_frame;
	encoder_t encoder;

	switch(elen) {
	case 100:
		encoder = g726_40_encoder;
		break;
	case 80:
		encoder = g726_32_encoder;
		break;
	case 60:
		encoder = g726_24_encoder;
		break;
	case 40:
		encoder = g726_16_encoder;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid Encoding Size %d!\n", elen);
		return SWITCH_STATUS_FALSE;
		break;
	}

	if (!context) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (decoded_data_len % len == 0) {
		uint32_t new_len = 0;
		int16_t *ddp = decoded_data;
		uint8_t *edp = encoded_data;
		int x;
		uint32_t loops = decoded_data_len / (sizeof(*ddp));

		for (x = 0; x < loops && new_len < *encoded_data_len; x++) {
			int edata = encoder(*ddp, AUDIO_ENCODING_LINEAR, context);
			int bits = handle->bbits + handle->bits_per_frame;
			
			handle->ecount++;
			if (!handle->bbits) {
				//printf("new byte assign the %d bits\n", handle->bits_per_frame);
				*handle->ptr = edata;
			} else if (bits <= BITS_IN_A_BYTE) {
				int shift_by = ((handle->bits_per_frame * (handle->ecount)) - handle->bits_per_frame);
				//printf ("shift by %d and add %d bits\n", shift_by, handle->bits_per_frame);
				//*handle->ptr <<= shift_by;
				//*handle->ptr |= edata;
				if (shift_by);

				//printf("edata\n");
				//print_bits(edata);
				//printf("\n");

				*handle->ptr |= (edata << 4);

				//printf("handle\n");
				//print_bits(*handle->ptr);
				//printf("\n");				

			} else {
				int remain, next, rdata, ndata;

				remain = BITS_IN_A_BYTE - handle->bits_per_frame;
				next = handle->bits_per_frame - remain;
				rdata = edata;
				ndata = edata;

				*handle->ptr += (edata << remain);
				*handle->ptr = ndata;

				handle->bbits = 0;
				handle->ecount = 0;
			}
			handle->bits = bits;
			handle->bbits += handle->bits_per_frame;

			if (0) {
			for(x = 0; x < 5; x++) {
				print_bits(handle->buf[x]);
				printf(" ");
			}
			printf("\n");
			}

			if ((handle->bits % BITS_IN_A_BYTE) == 0) {
				int bytes = handle->bits / BITS_IN_A_BYTE, count;
				for(count = 0; count < bytes; count++) {
					edp[new_len++] = handle->buf[count];
				}
				handle->bits = handle->bbits = 0;
				handle->ptr = handle->buf;
				handle->ecount = 0;
				memset(handle->buf, 0, sizeof(handle->buf));
			}
			ddp++;
		}

		if (new_len <= *encoded_data_len) {
			*encoded_data_len = new_len;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!! %u >= %u\n", new_len, *encoded_data_len);
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t switch_g726_decode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *encoded_data,

										uint32_t encoded_data_len, 
										uint32_t encoded_rate, 
										void *decoded_data,

										uint32_t *decoded_data_len, 
										uint32_t *decoded_rate, 
										unsigned int *flag) 
{

	g726_handle_t *handle = codec->private_info;
	g726_state *context = &handle->context;
	//uint32_t len = codec->implementation->bytes_per_frame;
	uint32_t elen = codec->implementation->encoded_bytes_per_frame;
	decoder_t decoder;


	switch(elen) {
	case 100:
		decoder = g726_40_decoder;
		break;
	case 80:
		decoder = g726_32_decoder;
		break;
	case 60:
		decoder = g726_24_decoder;
		break;
	case 40:
		decoder = g726_16_decoder;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid Encoding Size %d!\n", elen);
		return SWITCH_STATUS_FALSE;
		break;
	}

	if (!context) {
		return SWITCH_STATUS_FALSE;

	}


	{

		int loops = ((int)encoded_data_len * BITS_IN_A_BYTE) / handle->bits_per_frame;
		int8_t *edp = encoded_data;
		int16_t *ddp = decoded_data;
		int x;
		uint32_t new_len = 0;

		for (x = 0; x < loops && new_len < *decoded_data_len; x++) {
			int in = 0;
			int bits = 0;
			int8_t over = 0;
			int8_t under = 0;

			if (handle->save) {
				in = handle->save;
				handle->save = 0;
			}
			
			handle->d_bits += handle->bits_per_frame;
			bits = handle->d_bbits + handle->bits_per_frame;

			if (bits > BITS_IN_A_BYTE) {
				int tmp;
				over = bits - BITS_IN_A_BYTE;
				under = handle->bits_per_frame - over;
				handle->dcount = 0;
				tmp = *edp >> (BITS_IN_A_BYTE - (handle->bits_per_frame * handle->dcount));
				in = tmp >> over;
				handle->save = tmp;
				handle->save  &= (1 << under) - 1;
				edp++;
			} else if (bits == BITS_IN_A_BYTE) {
				handle->d_bbits = 0;
				in = *edp;
				edp++;
				handle->dcount = 0;
			} else {
				in |= *edp >> (BITS_IN_A_BYTE - (handle->bits_per_frame * handle->dcount));
				handle->d_bbits = bits;
			}

			

			handle->dcount++;

			*ddp++ = decoder(in, AUDIO_ENCODING_LINEAR, context);
			new_len += 2;
		}

		if (new_len <= *decoded_data_len) {
			*decoded_data_len = new_len;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!!\n");
			return SWITCH_STATUS_FALSE;
		}

	}

	return SWITCH_STATUS_SUCCESS;

}



/* Registration */ 

static const switch_codec_implementation_t g726_16k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 127, 
	/*.iananame */ "G726-16", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 16000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 320, 
	/*.encoded_bytes_per_frame */ 40, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_encode, 
	/*.decode */ switch_g726_decode, 
	/*.destroy */ switch_g726_destroy, 
};


static const switch_codec_implementation_t g726_24k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 2, 
	/*.iananame */ "G726-24", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 24000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 320, 
	/*.encoded_bytes_per_frame */ 60, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_encode, 
	/*.decode */ switch_g726_decode, 
	/*.destroy */ switch_g726_destroy, 
};

static const switch_codec_implementation_t g726_32k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 2, 
	/*.iananame */ "G726-32", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 32000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 320, 
	/*.encoded_bytes_per_frame */ 80, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_encode, 
	/*.decode */ switch_g726_decode, 
	/*.destroy */ switch_g726_destroy, 
};

static const switch_codec_implementation_t g726_40k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 2, 
	/*.iananame */ "G726-40", 
	/*.samples_per_second */ 8000,
	/*.bits_per_second */ 40000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 320, 
	/*.encoded_bytes_per_frame */ 100, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_encode, 
	/*.decode */ switch_g726_decode, 
	/*.destroy */ switch_g726_destroy, 
};

const switch_codec_interface_t g726_16k_codec_interface = { 
	/*.interface_name */ "G.726 16k", 
	/*.implementations */ &g726_16k_implementation, 
};
const switch_codec_interface_t g726_24k_codec_interface = { 
	/*.interface_name */ "G.726 24k", 
	/*.implementations */ &g726_24k_implementation, 
	/*.next */ &g726_16k_codec_interface
};
const switch_codec_interface_t g726_32k_codec_interface = { 
	/*.interface_name */ "G.726 32k", 
	/*.implementations */ &g726_32k_implementation, 
	/*.next */ &g726_24k_codec_interface
};
const switch_codec_interface_t g726_40k_codec_interface = { 
	/*.interface_name */ "G.726 40k", 
	/*.implementations */ &g726_40k_implementation, 
	/*.next */ &g726_32k_codec_interface
};

static switch_loadable_module_interface_t g726_module_interface = { 
	/*.module_name */ modname, 
	/*.endpoint_interface */ NULL, 
	/*.timer_interface */ NULL, 
	/*.dialplan_interface */ NULL, 
	/*.codec_interface */ &g726_40k_codec_interface, 
	/*.application_interface */ NULL 
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface,
													 char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */ 
	*module_interface = &g726_module_interface;

	/* indicate that the module should continue to be loaded */ 
	return SWITCH_STATUS_SUCCESS;
}
