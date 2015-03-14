/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * mod_vorbis.c -- Vorbis Audio Codec
 *
 */

#include <switch.h>
#include <vorbis/vorbisenc.h>

// SWITCH_MODULE_LOAD_FUNCTION(mod_vorbis_load);
// SWITCH_MODULE_DEFINITION(mod_vorbis, mod_vorbis_load, NULL, NULL);

#define SCC_GET_CODEC_PRIVATE SCC_VIDEO_BANDWIDTH
// #define OGG_DEBUG // dump to a file

typedef struct vorbis_context_s {
	ogg_stream_state os; /* take physical pages, weld into a logical stream of packets */
	ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
	ogg_packet       op; /* one raw packet of data for decode */
	vorbis_info      vi; /* struct that stores all the static vorbis bitstream settings */
	vorbis_comment   vc; /* struct that stores all the user comments */
	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block     vb; /* local working space for packet->PCM decode */

	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;

	uint8_t *codec_private_data;
	uint16_t codec_private_data_len;

#ifdef OGG_DEBUG
	int fd;
#endif

} vorbis_context_t;

static void xiph_lacing(uint x, uint8_t **buffer)
{
	uint8_t *p = *buffer;

	while (x >= 255) {
		*p++ = 255;
		x -= 255;
	}

	*p++ = x;
	*buffer = p;
}

static switch_status_t switch_vorbis_control(switch_codec_t *codec,
										  switch_codec_control_command_t cmd,
										  switch_codec_control_type_t ctype,
										  void *cmd_data,
										  switch_codec_control_type_t *rtype,
										  void **ret_data)
{
	vorbis_context_t *context = (vorbis_context_t *)codec->private_info;

	switch(cmd) {
	case SCC_GET_CODEC_PRIVATE:
	{
		uint32_t len = 0;
		uint8_t *p;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%ld %ld %ld\n",
			context->header.bytes, context->header_comm.bytes, context->header_code.bytes);

		len = context->header.bytes + context->header_comm.bytes + context->header_code.bytes;
		len += 20;

		if (!context->codec_private_data) {
			// uint16_t size;

			p = context->codec_private_data = switch_core_alloc(codec->memory_pool, len);
			switch_assert(p);

#if 0
			// ffmpeg also can decode this one
			size = context->header.bytes;
			*(uint16_t *)p = htons(size);
			p+=2;
			memcpy(p, context->header.packet, context->header.bytes);
			p += context->header.bytes;

			size = context->header_comm.bytes;
			*(uint16_t *)p = htons(size);
			p+=2;
			memcpy(p, context->header_comm.packet, context->header_comm.bytes);
			p += context->header_comm.bytes;

			size = context->header_code.bytes;
			*(uint16_t *)p = htons(size);
			p+=2;
			memcpy(p, context->header_code.packet, context->header_code.bytes);
			p += context->header_code.bytes;
#else
			// SPEC
			// http://www.matroska.org/technical/specs/codecid/index.html
			// A_VORBIS
			*p++ = 2;
			xiph_lacing(context->header.bytes, &p);
			xiph_lacing(context->header_comm.bytes, &p);
			memcpy(p, context->header.packet, context->header.bytes);
			p += context->header.bytes;
			memcpy(p, context->header_comm.packet, context->header_comm.bytes);
			p += context->header_comm.bytes;
			memcpy(p, context->header_code.packet, context->header_code.bytes);
			p += context->header_code.bytes;
#endif

			context->codec_private_data_len = p - context->codec_private_data;
		}

		switch_assert(context->codec_private_data_len < 20000);

		*(int16_t *)cmd_data = context->codec_private_data_len;
		*ret_data = (void *)context->codec_private_data;
	}
	break;
	default: break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_vorbis_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int encoding, decoding;
	vorbis_context_t *context;
	int ret = 0;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (codec->fmtp_in) {
			codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
		}
	}

	context = switch_core_alloc(codec->memory_pool, sizeof(*context));
	memset(context, 0, sizeof(*context));
	codec->private_info = context;

	vorbis_info_init(&context->vi);

	// https://xiph.org/vorbis/doc/vorbisenc/examples.html
	ret=vorbis_encode_init_vbr(&context->vi,
		codec->implementation->number_of_channels,
		codec->implementation->actual_samples_per_second, 0.4);

#if 0
	ret = vorbis_encode_init(&context->vi,
		codec->implementation->number_of_channels,
		codec->implementation->actual_samples_per_second,
		-1,128000,-1);
#endif

	if (ret) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VORBIS codec init error, ret = %d\n", ret);
		goto error;
	}

	vorbis_comment_init(&context->vc);
	vorbis_comment_add_tag(&context->vc, "ENCODER", "FreeSWITCH");

	ret = vorbis_analysis_init(&context->vd, &context->vi);
	if (ret) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VORBIS analysis init error, ret = %d\n", ret);
		goto error;
	}

	ret = vorbis_block_init(&context->vd, &context->vb);
	if (ret) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VORBIS block init error, ret = %d\n", ret);
		goto error;
	}

	// headers
	vorbis_analysis_headerout(&context->vd, &context->vc, &context->header, &context->header_comm, &context->header_code);

	{//hack so we can use the codec control
		switch_codec_implementation_t **impl = (switch_codec_implementation_t **)&codec->implementation;
		(*impl)->codec_control = switch_vorbis_control;
	}

#ifdef OGG_DEBUG
	ogg_stream_init(&context->os, rand());

	context->fd = open("/tmp/test.ogg", O_CREAT | O_RDWR | O_TRUNC, 0644);

	if (context->fd < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error open file to write\n");
		goto error;
	}

	ogg_stream_packetin(&context->os,&context->header); /* automatically placed in its own page */
	ogg_stream_packetin(&context->os,&context->header_comm);
	ogg_stream_packetin(&context->os,&context->header_code);

	/* This ensures the actual
	 * audio data will start on a new page, as per spec
	 */
	while(1){
		int result = ogg_stream_flush(&context->os,&context->og);
		if (result == 0) break;

		write(context->fd, context->og.header, context->og.header_len);
		write(context->fd, context->og.body, context->og.body_len);
	}

#endif

	return SWITCH_STATUS_SUCCESS;

error:
	vorbis_block_clear(&context->vb);
	vorbis_dsp_clear(&context->vd);
	vorbis_comment_clear(&context->vc);
	vorbis_info_clear(&context->vi);

	return SWITCH_STATUS_GENERR;
}

static switch_status_t switch_vorbis_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	vorbis_context_t *context = (vorbis_context_t *)codec->private_info;
	int i, j;
	uint32_t len = 0;
	int16_t *data = (int16_t *)decoded_data;
	int channels = codec->implementation->number_of_channels;
	uint32_t samples = decoded_data_len / 2 / channels;
	float **buffer = vorbis_analysis_buffer(&context->vd, samples);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%u\n", decoded_data_len);

	/* uninterleave samples */
	for (i = 0; i < samples; i++) {
		for (j = 0; j < channels; j++) {
			buffer[j][i] = *(data + channels * i + j) / 32768.f;
		}
	}

	vorbis_analysis_wrote(&context->vd, samples);

	while (vorbis_analysis_blockout(&context->vd, &context->vb) == 1) {
		/* analysis, assume we want to use bitrate management */
		vorbis_analysis(&context->vb, NULL);
		vorbis_bitrate_addblock(&context->vb);

		while (vorbis_bitrate_flushpacket(&context->vd, &context->op)){
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "got %ld bytes pts: %lld\n", context->op.bytes, context->op.granulepos);

#ifdef OGG_DEBUG
			ogg_stream_packetin(&context->os, &context->op);

			/* write out pages (if any) */
			while(1){
				int result=ogg_stream_pageout(&context->os, &context->og);
				if (result == 0) break;

				write(context->fd, context->og.header,context->og.header_len);
				write(context->fd, context->og.body, context->og.body_len);
			}
#endif

			if (len + context->op.bytes > *encoded_data_len) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "buffer overflow %u\n", *encoded_data_len);
				break;
			}

			memcpy((uint8_t *)encoded_data + len, context->op.packet, context->op.bytes);
			len += context->op.bytes;
		}
	}

	*encoded_data_len = len;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_vorbis_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_vorbis_destroy(switch_codec_t *codec)
{
	vorbis_context_t *context = (vorbis_context_t *)codec->private_info;

	vorbis_block_clear(&context->vb);
	vorbis_dsp_clear(&context->vd);
	vorbis_comment_clear(&context->vc);
	vorbis_info_clear(&context->vi);

#ifdef OGG_DEBUG
	if (context->fd > -1) close(context->fd);
#endif

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_vorbis_load)
{
	switch_codec_interface_t *codec_interface;
	int mpf = 20000, spf = 160, bpf = 320, counta, countb;
	int rates[] = {0, 8000, 44100, 48000, 88200, 96000, 176400, 192000};
	switch_payload_t ianacode[4] = { 0, 99, 99, 99 };

	/* connect my internal structure to the blank pointer passed to me */
	// *module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "VORBIS");

	for (counta = 1; counta <= 3; counta++) {
		for (countb = 1; countb > 0; countb--) {
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
												 ianacode[counta],	/* the IANA code number */
												 "VORBIS",	/* the IANA code name */
												 NULL,	/* default fmtp to send (can be overridden by the init function) */
												 rates[counta],	/* samples transferred per second */
												 rates[counta],	/* actual samples transferred per second */
												 0,	/* bits transferred per second */
												 mpf * countb,	/* number of microseconds per frame */
												 spf * countb,	/* number of samples per frame */
												 bpf * countb,	/* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 1,	/* number of channels represented */
												 1,	/* number of frames per network packet */
												 switch_vorbis_init,	/* function to initialize a codec handle using this implementation */
												 switch_vorbis_encode,	/* function to encode raw data into encoded data */
												 switch_vorbis_decode,	/* function to decode encoded data into raw data */
												 switch_vorbis_destroy);	/* deinitalize a codec handle using this implementation */
		}
		spf = spf * 2;
	}

	for (counta = 1; counta <= 3; counta++) {
		for (countb = 1; countb > 0; countb--) {
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
												 ianacode[counta],	/* the IANA code number */
												 "vorbis",	/* the IANA code name */
												 NULL,	/* default fmtp to send (can be overridden by the init function) */
												 rates[counta],	/* samples transferred per second */
												 rates[counta],	/* actual samples transferred per second */
												 0,	/* bits transferred per second */
												 mpf * countb,	/* number of microseconds per frame */
												 spf * countb,	/* number of samples per frame */
												 bpf * countb,	/* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 2,	/* number of channels represented */
												 1,	/* number of frames per network packet */
												 switch_vorbis_init,	/* function to initialize a codec handle using this implementation */
												 switch_vorbis_encode,	/* function to encode raw data into encoded data */
												 switch_vorbis_decode,	/* function to decode encoded data into raw data */
												 switch_vorbis_destroy);	/* deinitalize a codec handle using this implementation */
		}
		spf = spf * 2;
	}

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
