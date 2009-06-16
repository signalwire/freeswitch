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

#ifndef __MPF_CODEC_MANAGER_H__
#define __MPF_CODEC_MANAGER_H__

/**
 * @file mpf_codec_manager.h
 * @brief MPF Codec Manager
 */ 

#include "mpf_types.h"
#include "mpf_codec.h"

APT_BEGIN_EXTERN_C

/** Create codec manager */
MPF_DECLARE(mpf_codec_manager_t*) mpf_codec_manager_create(apr_size_t codec_count, apr_pool_t *pool);

/** Destroy codec manager */
MPF_DECLARE(void) mpf_codec_manager_destroy(mpf_codec_manager_t *codec_manager);

/** Register codec in codec manager */
MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_register(mpf_codec_manager_t *codec_manager, mpf_codec_t *codec);

/** Get (allocate) codec by codec descriptor */
MPF_DECLARE(mpf_codec_t*) mpf_codec_manager_codec_get(const mpf_codec_manager_t *codec_manager, mpf_codec_descriptor_t *descriptor, apr_pool_t *pool);

/** Get (allocate) list of available codecs */
MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_list_get(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, apr_pool_t *pool);

/** Load (allocate) list of codecs  */
MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_list_load(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, const char *str, apr_pool_t *pool);

/** Find codec by name  */
MPF_DECLARE(const mpf_codec_t*) mpf_codec_manager_codec_find(const mpf_codec_manager_t *codec_manager, const apt_str_t *codec_name);

APT_END_EXTERN_C

#endif /*__MPF_CODEC_MANAGER_H__*/
