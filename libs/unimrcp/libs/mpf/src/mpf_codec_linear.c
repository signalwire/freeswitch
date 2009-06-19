/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mpf_codec.h"

/* linear 16-bit PCM (host horder) */
#define LPCM_CODEC_NAME        "LPCM"
#define LPCM_CODEC_NAME_LENGTH (sizeof(LPCM_CODEC_NAME)-1)

/* linear 16-bit PCM (RFC3551) */
#define L16_CODEC_NAME        "L16"
#define L16_CODEC_NAME_LENGTH (sizeof(L16_CODEC_NAME)-1)


static apt_bool_t l16_open(mpf_codec_t *codec)
{
	return TRUE;
}

static apt_bool_t l16_close(mpf_codec_t *codec)
{
	return TRUE;
}

static apt_bool_t l16_encode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	apr_uint32_t i;
	const short *buf_in = frame_in->buffer;
	short *buf_out = frame_out->buffer;

	frame_out->size = frame_in->size;

	for(i=0; i<frame_in->size; ) {
		buf_out[i] = htons(buf_in[i]);
		i += sizeof(short);
	}

	return TRUE;
}

static apt_bool_t l16_decode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	apr_uint32_t i;
	const short *buf_in = frame_in->buffer;
	short *buf_out = frame_out->buffer;

	frame_out->size = frame_in->size;

	for(i=0; i<frame_in->size; ) {
		buf_out[i] = ntohs(buf_in[i]);
		i += sizeof(short);
	}

	return TRUE;
}



static const mpf_codec_vtable_t lpcm_vtable = {
	NULL
};

static const mpf_codec_vtable_t l16_vtable = {
	l16_open,
	l16_close,
	l16_encode,
	l16_decode,
	NULL
};

static const mpf_codec_attribs_t lpcm_attribs = {
	{LPCM_CODEC_NAME, LPCM_CODEC_NAME_LENGTH},   /* codec name */
	16,                                          /* bits per sample */
	MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000 /* sampling rates */
};

static const mpf_codec_attribs_t l16_attribs = {
	{L16_CODEC_NAME, L16_CODEC_NAME_LENGTH},     /* codec name */
	16,                                          /* bits per sample */
	MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000 /* sampling rates */
};

mpf_codec_descriptor_t* mpf_codec_lpcm_descriptor_create(apr_uint16_t sampling_rate, apr_byte_t channel_count, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor = apr_palloc(pool,sizeof(mpf_codec_descriptor_t));
	mpf_codec_descriptor_init(descriptor);
	descriptor->payload_type = 96;
	descriptor->name.buf = LPCM_CODEC_NAME;
	descriptor->name.length = LPCM_CODEC_NAME_LENGTH;
	descriptor->sampling_rate = sampling_rate;
	descriptor->channel_count = channel_count;
	return descriptor;
}

mpf_codec_t* mpf_codec_lpcm_create(apr_pool_t *pool)
{
	return mpf_codec_create(&lpcm_vtable,&lpcm_attribs,NULL,pool);
}

mpf_codec_t* mpf_codec_l16_create(apr_pool_t *pool)
{
	return mpf_codec_create(&l16_vtable,&l16_attribs,NULL,pool);
}
