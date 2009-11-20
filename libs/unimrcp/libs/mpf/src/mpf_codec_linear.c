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

#define APR_WANT_BYTEFUNC
#include <apr_want.h>
#include "mpf_codec.h"
#include "mpf_rtp_pt.h"

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
	const apr_int16_t *buf_in = frame_in->buffer;
	apr_int16_t *buf_out = frame_out->buffer;
	apr_size_t samples = frame_in->size / sizeof(apr_int16_t);

	frame_out->size = frame_in->size;

	for(i=0; i<samples; i++) {
		buf_out[i] = htons(buf_in[i]);
	}

	return TRUE;
}

static apt_bool_t l16_decode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	apr_uint32_t i;
	const apr_int16_t *buf_in = frame_in->buffer;
	apr_int16_t *buf_out = frame_out->buffer;
	apr_size_t samples = frame_in->size / sizeof(apr_int16_t);

	frame_out->size = frame_in->size;

	for(i=0; i<samples; i++) {
		buf_out[i] = ntohs(buf_in[i]);
	}

	return TRUE;
}

static const mpf_codec_vtable_t l16_vtable = {
	l16_open,
	l16_close,
	l16_encode,
	l16_decode,
	NULL
};

static const mpf_codec_attribs_t l16_attribs = {
	{L16_CODEC_NAME, L16_CODEC_NAME_LENGTH},      /* codec name */
	16,                                           /* bits per sample */
	MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000 |
	MPF_SAMPLE_RATE_32000 | MPF_SAMPLE_RATE_48000 /* supported sampling rates */
};

mpf_codec_t* mpf_codec_l16_create(apr_pool_t *pool)
{
	return mpf_codec_create(&l16_vtable,&l16_attribs,NULL,pool);
}
