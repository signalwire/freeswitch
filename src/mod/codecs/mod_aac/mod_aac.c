/* 
 * FreeSWITCH Modular of AAC Codec
 * Copyright (C) 2005-2020, Seven Du <dujinfang@gmail.com>
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
 * The Original Code is FreeSWITCH Module of AAC Codec
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
 * mod_aac.c -- AAC Audio Codec
 *
 */

#include <switch.h>
#include <faac.h>
#include <mp4v2/mp4v2.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_aac_load);
SWITCH_MODULE_DEFINITION(mod_aac, mod_aac_load, NULL, NULL);

typedef struct aac_context_s {
	faacEncHandle *encoder;
	switch_size_t input_samples;
	switch_size_t max_output_bytes;
	switch_buffer_t *buffer;
	uint8_t *input_buff;
	uint8_t *out_buff;
	uint8_t *asc;
	unsigned long len;
} aac_context_t;

static switch_status_t switch_aac_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int encoding, decoding;
	aac_context_t *context;
	faacEncConfigurationPtr faac_cfg;
	//unsigned long input_samples, max_output_bytes;

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
	context->encoder = faacEncOpen(
		codec->implementation->samples_per_second,
		codec->implementation->number_of_channels,
		&context->input_samples, &context->max_output_bytes);

	if (!context->encoder) goto error;

	switch_buffer_create_dynamic(&context->buffer, context->input_samples * 2, context->input_samples * 2 * 8, 0);
	context->input_buff = (uint8_t *)switch_core_alloc(codec->memory_pool, context->input_samples * 2);
	context->out_buff = (uint8_t *)switch_core_alloc(codec->memory_pool, context->max_output_bytes);
	faac_cfg = faacEncGetCurrentConfiguration(context->encoder);
	if (faac_cfg->version != FAAC_CFG_VERSION) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "wrong libfaac version (compiled for: %d, using %d)\n", FAAC_CFG_VERSION, faac_cfg->version);
		goto error;
	}

	// faac_cfg->aacObjectType = MAIN;
	faac_cfg->aacObjectType = LOW;
	faac_cfg->mpegVersion = MPEG4;
	faac_cfg->useTns = 0;
	faac_cfg->allowMidside = 1;
	// faac_cfg->bitRate = avctx->bit_rate / avctx->channels;
	// faac_cfg->bandWidth = avctx->cutoff;
	faac_cfg->outputFormat = 0;		/* user raw stream */
	faac_cfg->inputFormat = FAAC_INPUT_16BIT;
	// avctx->frame_size = samples_input / avctx->channels;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "aac config:\n"
		"  input samples: %lu\n"
		"  max output bytes: %lu\n"
		"  bitrate:  %lu\n"
		"  bandwidth: %u\n",
		context->input_samples, context->max_output_bytes,
		faac_cfg->bitRate, faac_cfg->bandWidth);

	if (!faacEncSetConfiguration(context->encoder, faac_cfg)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "faac doesn't support this bitrate %lu\n", faac_cfg->bitRate);
		goto error;
	}

	{
		faacEncGetDecoderSpecificInfo(context->encoder, &context->asc, &context->len);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "len: %lu %x %x\n", context->len, *(context->asc), *(context->asc+1) );
	}

	return SWITCH_STATUS_SUCCESS;

error:
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_aac_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	aac_context_t *context = (aac_context_t *)codec->private_info;
	switch_buffer_t *buffer = context->buffer;
	int ret = 0;
	switch_size_t size = 0;
	uint8_t *aac_input_buff = context->input_buff;

	/* encode buffer data */
	if (decoded_data_len == 0) {
		switch_size_t remain_size = 0;
		//remain_size = switch_buffer_len(buffer);
		remain_size = switch_buffer_write(buffer, NULL, 0);
		memset(aac_input_buff, 0, context->input_samples * 2);
		if (remain_size > 0) {
			switch_buffer_read(buffer, aac_input_buff, remain_size);
		}

		ret = faacEncEncode(context->encoder, (int32_t *)aac_input_buff, remain_size, encoded_data, context->max_output_bytes);
	} else {
		size = switch_buffer_write(buffer, decoded_data, decoded_data_len);
		if (size < context->input_samples * 2) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "buffer size %lu \n", size);
			goto end;
		}

		memset(aac_input_buff, 0, context->input_samples * 2);
		switch_buffer_read(buffer, aac_input_buff, context->input_samples * 2);
		ret = faacEncEncode(context->encoder, (int32_t *)aac_input_buff, context->input_samples, encoded_data, context->max_output_bytes);
	}

	if (ret < 0) {
		*encoded_data_len = 0;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encode failed!\n");
		return SWITCH_STATUS_FALSE;
		// *flag |= SFF_CNG;
		// return SWITCH_STATUS_SUCCESS;
	}

  end:
	//if (ret > 0) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ret: %d\n", ret);
	*encoded_data_len = ret;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_aac_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_aac_destroy(switch_codec_t *codec)
{
	aac_context_t *context = (aac_context_t *)codec->private_info;

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "destroy:====\n");
	switch_safe_free(context->asc);
	switch_buffer_destroy(&context->buffer);
	if (context->encoder) faacEncClose(context->encoder);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(aac_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_frame_t *frame;
	switch_status_t status;
	switch_codec_t codec = { 0 };
	//uint8_t buffer[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
	//unsigned int flag;
	switch_codec_t *read_codec = NULL;
	switch_codec_t L16_codec = { 0 };
	uint8_t *L16_out_buff = NULL;
	uint32_t L16_out_buff_datalen = 0;
	switch_size_t PCMU_packet_datalen;
	aac_context_t *context = NULL;
	uint8_t *encoded_data = NULL;
	uint32_t encoded_data_len = 0;
	//uint32_t len = 0;
	uint32_t rate = 0;
	MP4FileHandle aacfile = MP4_INVALID_FILE_HANDLE;
	MP4TrackId MP4track = 0;
	const char *file_path = data;

	switch_channel_answer(channel);

	read_codec = switch_core_session_get_read_codec(session);
	status = switch_core_codec_init(&codec, "AAC", NULL,"fmtp", 8000, 20, 1, SWITCH_CODEC_FLAG_ENCODE, NULL, switch_core_session_get_pool(session));

	if (switch_core_codec_init(&L16_codec, "L16", NULL, NULL,8000, 20, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "init L16 error!\n");
		return;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't initialize codec");
		return;
	}

	//PCMU_packet_datalen = read_codec->implementation->bits_per_second / (1000000 / read_codec->implementation->bits_per_secondmicroseconds_per_packet);
	PCMU_packet_datalen = 160;
	L16_out_buff_datalen = PCMU_packet_datalen * 2;
	L16_out_buff = (uint8_t *)malloc(L16_out_buff_datalen);
	context = (aac_context_t *)codec.private_info;
	encoded_data = context->out_buff;

	if (!file_path) file_path = "/tmp/test_aac.m4a";

	aacfile = MP4CreateEx(file_path, 0, 1, 1, NULL, 0, NULL, 0);
	MP4SetTimeScale(aacfile, 90000);
	MP4track = MP4AddAudioTrack(aacfile, 8000, 1024, MP4_MPEG4_AUDIO_TYPE);
	MP4SetAudioProfileLevel(aacfile, 0x0F);
	MP4SetTrackESConfiguration(aacfile, MP4track, context->asc, context->len);

	while(switch_channel_ready(channel)) {
		status = switch_core_session_read_frame(session, &frame, SWITCH_IO_FLAG_SINGLE_READ, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (!frame) continue;

		if (switch_test_flag(frame, SFF_CNG)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "this is cng\n");
			continue;
		}

		/* do decode first */
		memset(L16_out_buff, 0, 320);
		switch_core_codec_decode(read_codec, &L16_codec, frame->data, frame->datalen, 8000, L16_out_buff, &L16_out_buff_datalen, &rate, &frame->flags);

		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "decode success len %d ----> len %d\n", frame->datalen, L16_out_buff_datalen);

		switch_core_codec_encode(&codec, NULL, L16_out_buff, L16_out_buff_datalen, 8000, encoded_data, &encoded_data_len, &rate, &frame->flags);

		if (encoded_data_len > 0) {
			MP4WriteSample(aacfile, MP4track, encoded_data, encoded_data_len, MP4_INVALID_DURATION, 0, 1);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "read %d, encoded: %d\n", L16_out_buff_datalen, encoded_data_len);
		}
	}

	/* encode buffer data */
	while(1) {
		memset(L16_out_buff, 0, 320);
		switch_core_codec_encode(&codec, NULL, L16_out_buff, 0, 8000, encoded_data, &encoded_data_len, &rate, NULL);

		if (encoded_data_len > 0) {
			MP4WriteSample(aacfile, MP4track, encoded_data, encoded_data_len, MP4_INVALID_DURATION, 0, 1);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "encoded: %d\n", encoded_data_len);
		}
		if (encoded_data_len == 0) break;
	}

	MP4Close(aacfile, 0);
	switch_safe_free(L16_out_buff);
	if (codec.implementation) switch_core_codec_destroy(&codec);
	if (L16_codec.implementation) switch_core_codec_destroy(&L16_codec);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "aac app end!\n");
}

SWITCH_MODULE_LOAD_FUNCTION(mod_aac_load)
{
	switch_application_interface_t *app_interface;
	switch_codec_interface_t *codec_interface;
	int mpf = 20000, spf = 160, bpf = 320, counta, countb;
	int rates[] = {0, 8000, 16000, 32000, 44100, 48000, 88200, 96000, 176400, 192000};
	switch_payload_t ianacode[4] = { 0, 99, 99, 99 };

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "AAC");

	for (counta = 1; counta <= 5; counta++) {
		for (countb = 1; countb > 0; countb--) {
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
												 ianacode[counta],	/* the IANA code number */
												 "AAC",	/* the IANA code name */
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
												 switch_aac_init,	/* function to initialize a codec handle using this implementation */
												 switch_aac_encode,	/* function to encode raw data into encoded data */
												 switch_aac_decode,	/* function to decode encoded data into raw data */
												 switch_aac_destroy);	/* deinitalize a codec handle using this implementation */
		}
		spf = spf * 2;
	}

	mpf = 20000, spf = 160, bpf = 320;
	for (counta = 1; counta <= 5; counta++) {
		for (countb = 1; countb > 0; countb--) {
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
												 ianacode[counta],	/* the IANA code number */
												 "AAC",	/* the IANA code name */
												 NULL,	/* default fmtp to send (can be overridden by the init function) */
												 rates[counta],	/* samples transferred per second */
												 rates[counta],	/* actual samples transferred per second */
												 0,	/* bits transferred per second */
												 mpf * countb,	/* number of microseconds per frame */
												 spf * countb * 2,	/* number of samples per frame */
												 bpf * countb * 2,	/* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 2,	/* number of channels represented */
												 1,	/* number of frames per network packet */
												 switch_aac_init,	/* function to initialize a codec handle using this implementation */
												 switch_aac_encode,	/* function to encode raw data into encoded data */
												 switch_aac_decode,	/* function to decode encoded data into raw data */
												 switch_aac_destroy);	/* deinitalize a codec handle using this implementation */
		}
		spf = spf * 2;
	}

	SWITCH_ADD_APP(app_interface, "aac", "aac", "AAC test", aac_function, "", SAF_NONE);

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
