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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * mod_openh264 -- H264 Codec Using Cisco OpenH264
 *
 */

// #define DEBUG_H264
#include <switch.h>

#define EPSN (0.000001f) // (1e-6)	// desired float precision
#define PESN (0.000001f) // (1e-6)	// desired float precision
#define MT_ENABLED

#include "wels/codec_api.h"
//#include "inc/logging.h"     // for debug
#ifndef NAL_HEADER_ADD_0X30BYTES
#define NAL_HEADER_ADD_0X30BYTES 50
#endif

#define FPS 15.0f // frame rate
#define H264_NALU_BUFFER_SIZE 65536
#define SLICE_SIZE SWITCH_DEFAULT_VIDEO_SIZE //NALU Slice Size

SWITCH_MODULE_LOAD_FUNCTION(mod_openh264_load);
SWITCH_MODULE_DEFINITION(mod_openh264, mod_openh264_load, NULL, NULL);

typedef struct h264_codec_context_s {
	ISVCEncoder *encoder;
	switch_bool_t encoder_initialized;
	SEncParamExt encoder_params;
	SFrameBSInfo bit_stream_info;
	EVideoFrameType last_frame_type;
	int cur_layer;
	int cur_nalu_index;
	uint8_t last_nalu_type;
	uint8_t last_nri;
	int last_nalu_data_pos;
	int nalu_eat;
	int nalu_28_start;
	int change_bandwidth;
	SSourcePicture pic;

	ISVCDecoder *decoder;
	SDecodingParam decoder_params;
	switch_buffer_t *nalu_buffer;
	switch_image_t *img;
	int got_sps;
	int64_t pts;
	int need_key_frame;
	switch_size_t last_received_timestamp;
	switch_bool_t last_received_complete_picture;
	switch_codec_settings_t codec_settings;
	unsigned int bandwidth;
} h264_codec_context_t;

int FillSpecificParameters(h264_codec_context_t *context) {
	SEncParamExt *param;
	
	param = &context->encoder_params;

	if (!context->codec_settings.video.width) {
		context->codec_settings.video.width = 1280;
	}

	if (!context->codec_settings.video.height) {
		context->codec_settings.video.height = 720;
	}

	if (context->codec_settings.video.bandwidth) {
		context->bandwidth = context->codec_settings.video.bandwidth;
	} else {
		context->bandwidth = switch_calc_bitrate(context->codec_settings.video.width, context->codec_settings.video.height, 1, 15);
	}

	if (context->bandwidth > 5120) {
		context->bandwidth = 5120;
	}

	/* Test for temporal, spatial, SNR scalability */
	param->iPicWidth             = context->codec_settings.video.width; // width of picture in samples
	param->iPicHeight            = context->codec_settings.video.height;		 // height of picture in samples
	param->iTargetBitrate        = context->bandwidth;
	param->iRCMode               = RC_QUALITY_MODE;         //  rc mode control
	param->iTemporalLayerNum     = 1;         // layer number at temporal level
	param->iSpatialLayerNum      = 1;         // layer number at spatial level
	param->bEnableDenoise        = 0;         // denoise control
	param->bEnableBackgroundDetection = 1;    // background detection control
	param->bEnableSceneChangeDetect= 1;
	param->bEnableFrameSkip = 1;
	param->iMultipleThreadIdc= 1;
	param->bEnableAdaptiveQuant       = 1;    // adaptive quantization control
	param->bEnableLongTermReference   = 0;    // long term reference control
	param->iLtrMarkPeriod        = 30;
	param->iLoopFilterAlphaC0Offset= 0;
	param->iLoopFilterBetaOffset= 0;
	param->iComplexityMode = MEDIUM_COMPLEXITY;
	param->uiIntraPeriod		    = FPS * 3;       // period of Intra frame
	param->iNumRefFrame = AUTO_REF_PIC_COUNT; // encoder selects the number of reference frame automatically
#ifdef MT_ENABLED
	param->eSpsPpsIdStrategy = INCREASING_ID;
#else
	param->eSpsPpsIdStrategy = CONSTANT_ID;
#endif
	param->bPrefixNalAddingCtrl    = 0;

	int iIndexLayer = 0;
	param->sSpatialLayers[iIndexLayer].iVideoWidth	= context->codec_settings.video.width;
	param->sSpatialLayers[iIndexLayer].iVideoHeight	= context->codec_settings.video.height;
	param->sSpatialLayers[iIndexLayer].fFrameRate	= (double) (FPS * 1.0f);
	// param->sSpatialLayers[iIndexLayer].iQualityLayerNum = 1;
	param->sSpatialLayers[iIndexLayer].iSpatialBitrate  = param->iTargetBitrate;
	//param->sSpatialLayers[iIndexLayer].iMaxSpatialBitrate  = param->iTargetBitrate;
	//param->sSpatialLayers[iIndexLayer].uiLevelIdc = LEVEL_3_1;
	param->sSpatialLayers[iIndexLayer].uiProfileIdc = PRO_BASELINE;


	param->iUsageType = CAMERA_VIDEO_REAL_TIME;
	param->bEnableFrameCroppingFlag = 1;
	//param->iMaxBitrate = 1250000;
	//param->iTargetBitrate = 1250000;

#ifdef MT_ENABLED
	param->sSpatialLayers[iIndexLayer].sSliceCfg.uiSliceMode = SM_DYN_SLICE;
	param->sSpatialLayers[iIndexLayer].sSliceCfg.sSliceArgument.uiSliceSizeConstraint = SLICE_SIZE;
	param->uiMaxNalSize = SLICE_SIZE + NAL_HEADER_ADD_0X30BYTES;
#else
	param->sSpatialLayers[iIndexLayer].sSliceCfg.uiSliceMode = SM_SINGLE_SLICE;
#endif

	float fMaxFr = param->sSpatialLayers[param->iSpatialLayerNum - 1].fFrameRate;
	for (int32_t i = param->iSpatialLayerNum - 2; i >= 0; --i) {
		if (param->sSpatialLayers[i].fFrameRate > fMaxFr + EPSN) {
			fMaxFr = param->sSpatialLayers[i].fFrameRate;
		}
	}
	param->fMaxFrameRate = fMaxFr;

	return 0;
}

// return none-zero on error
long set_decoder_options(ISVCDecoder *decoder)
{
	int32_t iColorFormat = videoFormatI420;
    // EBufferProperty	eOutputProperty = BUFFER_HOST;
    long ret = 0;

	ret += decoder->SetOption(DECODER_OPTION_DATAFORMAT, &iColorFormat);
	// ret += decoder->SetOption(DECODER_OPTION_OUTPUT_PROPERTY,  &eOutputProperty);

	return ret;
}

static switch_size_t buffer_h264_nalu(h264_codec_context_t *context, switch_frame_t *frame)
{
	uint8_t nalu_idc = 0;
	uint8_t nalu_type = 0;
	uint8_t *data = (uint8_t *)frame->data;
	uint8_t nalu_hdr = *data;
	uint8_t sync_bytes[] = {0, 0, 0, 1};
	switch_buffer_t *buffer = context->nalu_buffer;
	switch_size_t size = switch_buffer_inuse(buffer);

	switch_assert(frame);

	nalu_idc = (nalu_hdr & 0x60) >> 5;
	nalu_type = nalu_hdr & 0x1f;

	if (context->got_sps <= 0) {
		context->got_sps--;
		if ((abs(context->got_sps) % 30) == 0) {
			switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
		}
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "waiting pps\n");
		//return SWITCH_STATUS_RESTART;
	}

	if (context->got_sps <= 0 && nalu_type == 7) context->got_sps = 1;


	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "XXX GOT %d len:%d\n", nalu_type, frame->datalen);

	if (!context->got_sps) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "===============Found SPS/PPS===============\n");
		context->got_sps = 1;
	}

	/* hack for phones sending sps/pps with frame->m = 1 such as grandstream */
	if ((nalu_type == 7 || nalu_type == 8) && frame->m) frame->m = SWITCH_FALSE;
	
	if (nalu_type == 28) { // 0x1c FU-A
		int start = *(data + 1) & 0x80;
		int end = *(data + 1) & 0x40;
		
		nalu_type = *(data + 1) & 0x1f;

		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "WTF start:%d end:%d mark:%d\n", start, end, frame->m);

		if (frame->m) end = 1;

		if (start && end) return 1;

		if (start) {
			if (context->nalu_28_start) {
				context->nalu_28_start = 0;
				switch_buffer_zero(buffer);
			}
		} else if (end) {
			context->nalu_28_start = 0;
		} else if (!context->nalu_28_start) {
			return 0;
		}

		if (start) {
			//uint8_t nalu_idc = (nalu_hdr & 0x60) >> 5;
			nalu_type |= (nalu_idc << 5);
			size = switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
			size = switch_buffer_write(buffer, &nalu_type, 1);
			context->nalu_28_start = 1;
		}

		size = switch_buffer_write(buffer, (void *)(data + 2), frame->datalen - 2);
	} else if (nalu_type == 24) { // 0x18 STAP-A
		uint16_t nalu_size;
		int left = frame->datalen - 1;

		data++;

	again:
		if (left > 2) {
			nalu_size = ntohs(*(uint16_t *)data);
			data += 2;
			left -= 2;

			if (nalu_size > left) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID PACKET\n");
				context->got_sps = 0;
				switch_buffer_zero(buffer);
				return 0;
			}

			nalu_hdr = *data;
			nalu_type = nalu_hdr & 0x1f;

			if (context->got_sps <= 0 && nalu_type == 7) context->got_sps = 1;

			size += switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
			size += switch_buffer_write(buffer, (void *)data, nalu_size);
			data += nalu_size;
			left -= nalu_size;
			goto again;
		}
	} else {
		size = switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
		size = switch_buffer_write(buffer, frame->data, frame->datalen);
		context->nalu_28_start = 0;
	}

	if (frame->m) context->nalu_28_start = 0;

#ifdef DEBUG_H264
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "seq: %u ts: %u len: %4d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x mark=%d size=%" SWITCH_SIZE_T_FMT "\n",
		(frame)->seq, (frame)->timestamp, (frame)->datalen,
		*((uint8_t *)(frame)->data), *((uint8_t *)(frame)->data + 1),
		*((uint8_t *)(frame)->data + 2), *((uint8_t *)(frame)->data + 3),
		*((uint8_t *)(frame)->data + 4), *((uint8_t *)(frame)->data + 5),
		*((uint8_t *)(frame)->data + 6), *((uint8_t *)(frame)->data + 7),
		*((uint8_t *)(frame)->data + 8), *((uint8_t *)(frame)->data + 9),
		*((uint8_t *)(frame)->data + 10), (frame)->m, size);
#endif

	return size;
}

static switch_status_t nalu_slice(h264_codec_context_t *context, switch_frame_t *frame)
{
	int nalu_len;
	uint8_t *buffer;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	frame->m = SWITCH_FALSE;

	if (context->cur_nalu_index >= context->bit_stream_info.sLayerInfo[context->cur_layer].iNalCount) {
		context->cur_nalu_index = 0;
		context->cur_layer++;
		context->last_nalu_data_pos = 0;
	}

#if 0
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "uiTemporalId:%d uiSpatialId:%d uiQualityId:%d uiLayerType:%d FrameType: %d\n",
		context->bit_stream_info.sLayerInfo[context->cur_layer].uiTemporalId,
		context->bit_stream_info.sLayerInfo[context->cur_layer].uiSpatialId,
		context->bit_stream_info.sLayerInfo[context->cur_layer].uiQualityId,
		context->bit_stream_info.sLayerInfo[context->cur_layer].uiLayerType,
		context->last_frame_type);
#endif

	if (context->last_frame_type == videoFrameTypeSkip ||
		context->cur_layer >= context->bit_stream_info.iLayerNum) {
		frame->datalen = 0;
		frame->m = SWITCH_TRUE;
		context->cur_layer = 0;
		context->cur_nalu_index = 0;
		return status;
	}

	nalu_len = context->bit_stream_info.sLayerInfo[context->cur_layer].pNalLengthInByte[context->cur_nalu_index] - 4; // NALU w/o sync bits
	buffer = context->bit_stream_info.sLayerInfo[context->cur_layer].pBsBuf;

#if 0
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "layer: %d/%d nalu:%d/%d nalu_len:%d/%d\n",
		context->cur_layer, context->bit_stream_info.iLayerNum,
		context->cur_nalu_index, context->bit_stream_info.sLayerInfo[context->cur_layer].iNalCount,
		nalu_len, context->last_nalu_data_pos);
#endif

	switch_assert(nalu_len > 0);

#ifdef MT_ENABLED
	if (nalu_len > SLICE_SIZE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "LARGE SLICE OVER MTU %d BYTES\n", nalu_len);
	}
#endif

	if (nalu_len <= SLICE_SIZE) {
		uint8_t nalu_type;

		context->last_nalu_data_pos += 4;
		nalu_type = *(buffer + context->last_nalu_data_pos) & 0x1f;

		// if (nalu_type == 7) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Got SPS\n");

		memcpy(frame->data, (buffer + context->last_nalu_data_pos), nalu_len);
		frame->datalen = nalu_len;

		if ((context->cur_nalu_index == context->bit_stream_info.sLayerInfo[context->cur_layer].iNalCount - 1) &&
				 (context->cur_layer == context->bit_stream_info.iLayerNum - 1)) {
			frame->m = SWITCH_TRUE;
		} else {
			status = SWITCH_STATUS_MORE_DATA;
		}
		context->cur_nalu_index++;
		context->last_nalu_data_pos += nalu_len;
		context->last_nalu_type = nalu_type;
		goto end;
	} else {
		int left = nalu_len;
		uint8_t *p = (uint8_t *) frame->data;

		if (context->nalu_eat) {
			left = nalu_len + 4 - context->nalu_eat;
			switch_assert(left > 0);
		}

		if (left > (SLICE_SIZE - 2)) {
			uint8_t start_bit;

			if (context->nalu_eat) {
				start_bit = 0;
			} else {
				start_bit = 0x80;
				context->last_nalu_data_pos += 4;
				context->last_nalu_type = *(buffer + context->last_nalu_data_pos) & 0x1f;
				context->last_nri = *(buffer + context->last_nalu_data_pos) & 0x60;
				context->last_nalu_data_pos++;
				context->nalu_eat = 5;
			}

			p[0] = context->last_nri | 28; // FU-A
			p[1] = start_bit | context->last_nalu_type;

			memcpy(p + 2, buffer + context->last_nalu_data_pos, SLICE_SIZE - 2);
			context->last_nalu_data_pos += (SLICE_SIZE - 2);
			context->nalu_eat += (SLICE_SIZE - 2);
			frame->datalen = SLICE_SIZE;
			status = SWITCH_STATUS_MORE_DATA;
			goto end;
		} else {
			p[0] = context->last_nri | 28; // FU-A
			p[1] = 0x40 | context->last_nalu_type;
			memcpy(p + 2, buffer + context->last_nalu_data_pos, left);
			context->last_nalu_data_pos += left;
			frame->datalen = left + 2;
			frame->m = SWITCH_TRUE;
			context->nalu_eat = 0;
			context->cur_nalu_index++;
			status = SWITCH_STATUS_SUCCESS;
			goto end;
		}
	}

end:
#if 0
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%d mark=%d BYTES datalen:%d %02x %02x\n", nalu_len, frame->m, frame->datalen, *((uint8_t *)frame->data), *((uint8_t *)frame->data + 1));
#endif
	return status;
}

static switch_status_t switch_h264_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	h264_codec_context_t *context = NULL;
	int encoding, decoding;
	int ret;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	if (codec->fmtp_in) {
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
	}

	context = (h264_codec_context_t*)switch_core_alloc(codec->memory_pool, sizeof(h264_codec_context_t));
	memset(context, 0, sizeof(*context));

	if (codec_settings) {
		context->codec_settings = *codec_settings;
	}

	if (decoding) {
		WelsCreateDecoder(&context->decoder);

		if (!context->decoder) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CreateDecoder Error\n");
			return SWITCH_STATUS_FALSE;
		}

		context->decoder_params.eOutputColorFormat	= videoFormatI420;
		context->decoder_params.uiTargetDqLayer	= (uint8_t) -1;
		context->decoder_params.eEcActiveIdc	= ERROR_CON_SLICE_COPY;
		context->decoder_params.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
		context->decoder_params.sVideoProperty.size = sizeof(context->decoder_params.sVideoProperty);

		if (context->decoder->Initialize(&context->decoder_params)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Initialize failed\n");
			goto error;
		}

		if (set_decoder_options(context->decoder)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Set Decoder Options Error\n");
		}
	}

	if (encoding) {
		ret = WelsCreateSVCEncoder(&context->encoder);
		if (ret) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create encoder, error: %d\n", ret);
			goto error;
		}

		FillSpecificParameters(context);
	}

	//if (encoding | decoding) WelsStderrSetTraceLevel(10);

	switch_buffer_create_dynamic(&(context->nalu_buffer), H264_NALU_BUFFER_SIZE, H264_NALU_BUFFER_SIZE * 8, 0);
	codec->private_info = context;

	return SWITCH_STATUS_SUCCESS;

error:
	// TODO, do some proper clean up
	return SWITCH_STATUS_FALSE;
}


static switch_status_t init_encoder(h264_codec_context_t *context, uint32_t width, uint32_t height)
{
	int i;

	if (width) context->codec_settings.video.width = width;
	if (height) context->codec_settings.video.height = height;
	//context->encoder_params.iTargetBitrate = width * height * 8;
	for (int i=0; i<context->encoder_params.iSpatialLayerNum; i++) {
		context->encoder_params.sSpatialLayers[i].iVideoWidth = width;
		context->encoder_params.sSpatialLayers[i].iVideoHeight = height;
	}
	FillSpecificParameters(context);

	/* just do it, the encoder will Uninitialize first by itself if already initialized */
	if (cmResultSuccess != context->encoder->InitializeExt(&context->encoder_params)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoder Init Error\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Codec ready; picture size %dx%d Bandwidth: %d\n",
					  context->encoder_params.iPicWidth, context->encoder_params.iPicHeight, context->codec_settings.video.bandwidth);


	context->encoder_initialized = SWITCH_TRUE;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_h264_encode(switch_codec_t *codec, switch_frame_t *frame)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;
	int width = 0;
	int height = 0;
	long enc_ret;
	long result;

	frame->m = SWITCH_FALSE;

	if (context->need_key_frame) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "H264 KEYFRAME GENERATED\n");
		context->encoder->ForceIntraFrame(1);
		context->need_key_frame = 0;
	}

	if (frame->flags & SFF_SAME_IMAGE) {
		return nalu_slice(context, frame);
	}


	if (frame->img->d_h > 1) {
		width = frame->img->d_w;
		height = frame->img->d_h;
	} else {
		width = frame->img->w;
		height = frame->img->h;
	}

	//switch_assert(width > 0 && (width % 2 == 0));
	//switch_assert(height > 0 && (height % 2 == 0));

	if (!context->encoder_initialized) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "initializing encoder %dx%d\n", width, height);
		init_encoder(context, width, height);
	}


	if (context->change_bandwidth) {
		context->codec_settings.video.bandwidth = context->change_bandwidth;
		context->change_bandwidth = 0;
		init_encoder(context, 0, 0);
	}

	if (width != context->encoder_params.iPicWidth || height != context->encoder_params.iPicHeight ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "picture size changed from %dx%d to %dx%d, reinitializing encoder\n",
			context->encoder_params.iPicWidth, context->encoder_params.iPicHeight, width, height);

		init_encoder(context, width, height);
	}

	context->pic.iColorFormat = videoFormatI420;
	context->pic.iPicHeight = height;
	context->pic.iPicWidth = width;
	context->pic.iStride[0] = frame->img->stride[0];
	context->pic.iStride[1] = frame->img->stride[1];
	context->pic.iStride[2] = frame->img->stride[2];

	context->pic.pData[0] = frame->img->planes[0];
	context->pic.pData[1] = frame->img->planes[1];
	context->pic.pData[2] = frame->img->planes[2];

	result = (EVideoFrameType)context->encoder->EncodeFrame(&context->pic, &context->bit_stream_info);

	if (result != cmResultSuccess ) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "EncodeFrame() failed, result = %ld\n", result);
	  goto error;
	}

	context->cur_layer = 0;
	context->cur_nalu_index = 0;
	context->last_nalu_data_pos = 0;

	return nalu_slice(context, frame);

error:

	frame->datalen = 0;

	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_h264_decode(switch_codec_t *codec, switch_frame_t *frame)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;
	switch_size_t size = 0;
	uint32_t error_code;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

#ifdef DEBUG_H264
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "len: %d seq: %u ts: %u mark:%d nalu:0x%x\n", frame->datalen, frame->seq, frame->timestamp, frame->m, *(uint8_t *)frame->data);
#endif

	if (context->last_received_timestamp && context->last_received_timestamp != frame->timestamp &&
		(!context->last_received_complete_picture)) {
		// possible packet loss
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Packet Loss, skip privousely received packets\n");
		if (frame->m) {
			context->last_received_complete_picture = SWITCH_TRUE;
		}
		switch_goto_status(SWITCH_STATUS_RESTART, end);
	}

	context->last_received_timestamp = frame->timestamp;
	context->last_received_complete_picture = frame->m ? SWITCH_TRUE : SWITCH_FALSE;

	size = buffer_h264_nalu(context, frame);

#ifdef DEBUG_H264
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "READ buf:%ld got_key:%d st:%d m:%d size:%" SWITCH_SIZE_T_FMT "\n", size, context->got_sps, status, frame->m, size);
#endif

	if (size == 1) {
		switch_goto_status(SWITCH_STATUS_RESTART, end);
	}

	if (frame->m && size) {
		int got_picture = 0;
		int i;
		const void *nalu = NULL;
		int width, height;
		SBufferInfo dest_buffer_info;
		switch_buffer_peek_zerocopy(context->nalu_buffer, &nalu);
		uint8_t* pData[3] = { 0 };

		frame->m = SWITCH_FALSE;
		frame->flags = 0;

		pData[0] = NULL;
		pData[1] = NULL;
		pData[2] = NULL;
		memset(&dest_buffer_info, 0, sizeof(dest_buffer_info));

		error_code = context->decoder->DecodeFrame2((uint8_t *)nalu, size, (uint8_t **)pData, &dest_buffer_info);

		if (error_code == dsErrorFree && dest_buffer_info.iBufferStatus == 1) {
			width  = dest_buffer_info.UsrData.sSystemBuffer.iWidth;
			height = dest_buffer_info.UsrData.sSystemBuffer.iHeight;

#ifdef DEBUG_H264
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "got pic: [%dx%d]\n", width, height);
#endif
			if (!context->img) {
				context->img = switch_img_wrap(NULL, SWITCH_IMG_FMT_I420, width, height, 0, pData[0]);
				assert(context->img);
			}

			context->img->w = dest_buffer_info.UsrData.sSystemBuffer.iStride[0];
			context->img->h = dest_buffer_info.UsrData.sSystemBuffer.iStride[1];
			context->img->d_w = width;
			context->img->d_h = height;
			context->img->planes[0] = pData[0];
			context->img->planes[1] = pData[1];
			context->img->planes[2] = pData[2];
			context->img->stride[0] = dest_buffer_info.UsrData.sSystemBuffer.iStride[0];
			context->img->stride[1] = dest_buffer_info.UsrData.sSystemBuffer.iStride[1];
			context->img->stride[2] = dest_buffer_info.UsrData.sSystemBuffer.iStride[1];

			frame->img = context->img;
			// TODO: keep going and see if more picture available
			// pDecoder->DecodeFrame (NULL, 0, pData, &sDstBufInfo);
		} else {
			if (error_code) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Decode error: 0x%x\n", error_code);
				switch_goto_status(SWITCH_STATUS_RESTART, end);
			}
		}
		//switch_set_flag(frame, SFF_USE_VIDEO_TIMESTAMP);
		switch_buffer_zero(context->nalu_buffer);
		context->nalu_28_start = 0;
		status = SWITCH_STATUS_SUCCESS;
	}

end:

#if 0
	if (size == 0) {
		status = SWITCH_STATUS_MORE_DATA;
	}
#endif

	if (status == SWITCH_STATUS_RESTART) {
		context->got_sps = 0;
		switch_buffer_zero(context->nalu_buffer);
		context->nalu_28_start = 0;

#if 0
		/* re-initialize decoder, trying to recover from really bad H264 bit streams */
		if (context->decoder->Initialize(&context->decoder_params)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Initialize failed\n");
		} else if (set_decoder_options(context->decoder)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Set Decoder Options Error\n");
		}
#endif
	}

	if (!context->got_sps) {
		switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
	}

	if (!frame->img) {
		//switch_set_flag(frame, SFF_USE_VIDEO_TIMESTAMP);
		//} else {
		status = SWITCH_STATUS_MORE_DATA;
	}

	return status;
}

static switch_status_t switch_h264_control(switch_codec_t *codec, 
										   switch_codec_control_command_t cmd, 
										   switch_codec_control_type_t ctype,
										   void *cmd_data,
										   switch_codec_control_type_t atype,
										   void *cmd_arg,
										   switch_codec_control_type_t *rtype,
										   void **ret_data) {



	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;

	switch(cmd) {
	case SCC_VIDEO_GEN_KEYFRAME:
		context->need_key_frame = 1;		
		break;
	case SCC_VIDEO_BANDWIDTH:
		{
			switch(ctype) {
			case SCCT_INT:
				context->change_bandwidth = *((int *) cmd_data);
				break;
			case SCCT_STRING:
				{
					char *bwv = (char *) cmd_data;
					context->change_bandwidth = switch_parse_bandwidth_string(bwv);
				}
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_h264_destroy(switch_codec_t *codec)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;

	if (!context) return SWITCH_STATUS_SUCCESS;

	if (context->nalu_buffer) switch_buffer_destroy(&context->nalu_buffer);

	if (context->encoder) {
		context->encoder->Uninitialize();
		WelsDestroySVCEncoder(context->encoder);
	}

	if (context->decoder) {
		switch_img_free(&context->img);
		context->decoder->Uninitialize();
		WelsDestroyDecoder(context->decoder);
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_openh264_load)
{
	switch_codec_interface_t *codec_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "H264 Video (with Cisco OpenH264)");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "H264", NULL,
											   switch_h264_init, switch_h264_encode, switch_h264_decode, switch_h264_control, switch_h264_destroy);


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
