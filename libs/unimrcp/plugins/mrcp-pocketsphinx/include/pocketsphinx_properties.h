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

#ifndef __POCKETSPHINX_PROPERTIES_H__
#define __POCKETSPHINX_PROPERTIES_H__

/**
 * @file pocketsphinx_properties.h
 * @brief PocketSphinx Properties
 */ 

#include "apt_dir_layout.h"

APT_BEGIN_EXTERN_C

typedef enum {
	POCKETSPHINX_MODEL_NARROWBAND,
	POCKETSPHINX_MODEL_WIDEBAND

} pocketsphinx_model_e;

/** Declaration of pocketsphinx properties */
typedef struct pocketsphinx_properties_t pocketsphinx_properties_t;

/** Pocketsphinx properties */
struct pocketsphinx_properties_t {
	const char          *data_dir;
	const char          *dictionary;
	const char          *model_8k;
	const char          *model_16k;
	pocketsphinx_model_e preferred_model;

	apr_size_t           sensitivity_level;
	apr_size_t           sensitivity_timeout;

	apr_size_t           no_input_timeout;
	apr_size_t           recognition_timeout;
	apr_size_t           partial_result_timeout;

	apt_bool_t           save_waveform;
	const char          *save_waveform_dir;
};

apt_bool_t pocketsphinx_properties_load(pocketsphinx_properties_t *properties, 
										const char *file_path, 
										const apt_dir_layout_t *dir_layout,
										apr_pool_t *pool);

APT_END_EXTERN_C

#endif /*__POCKETSPHINX_PROPERTIES_H__*/
