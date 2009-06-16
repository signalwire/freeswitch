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

#define L16_CODEC_NAME        "L16"
#define L16_CODEC_NAME_LENGTH (sizeof(L16_CODEC_NAME)-1)

static const mpf_codec_vtable_t l16_vtable = {
	NULL
};

static const mpf_codec_descriptor_t l16_descriptor = {
	96, /* not specified */
	{L16_CODEC_NAME, L16_CODEC_NAME_LENGTH},
	8000,
	1,
	NULL,
	TRUE
};

static const mpf_codec_attribs_t l16_attribs = {
	{L16_CODEC_NAME, L16_CODEC_NAME_LENGTH},     /* codec name */
	16,                                          /* bits per sample */
	MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000 /* sampling rates */
};

mpf_codec_t* mpf_codec_l16_create(apr_pool_t *pool)
{
	return mpf_codec_create(&l16_vtable,&l16_attribs,NULL,pool);
}

const mpf_codec_descriptor_t* l16_descriptor_get()
{
	return &l16_descriptor;
}
