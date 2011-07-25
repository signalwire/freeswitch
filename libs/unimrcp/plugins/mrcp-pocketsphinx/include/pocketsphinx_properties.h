/*
 * Copyright 2008-2010 Arsen Chaloyan
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
 * 
 * $Id: pocketsphinx_properties.h 1743 2010-07-12 08:04:46Z achaloyan $
 */

#ifndef POCKETSPHINX_PROPERTIES_H
#define POCKETSPHINX_PROPERTIES_H

/**
 * @file pocketsphinx_properties.h
 * @brief PocketSphinx Properties
 */ 

#include "apt_dir_layout.h"

APT_BEGIN_EXTERN_C

/** Enumeration of PocketSphinx models */
typedef enum {
	POCKETSPHINX_MODEL_NARROWBAND, /**< narrowband model */
	POCKETSPHINX_MODEL_WIDEBAND    /**< wideband model */
} pocketsphinx_model_e;

/** Declaration of PocketSphinx properties */
typedef struct pocketsphinx_properties_t pocketsphinx_properties_t;

/** PocketSphinx properties */
struct pocketsphinx_properties_t {
	/** Data directory */
	const char          *data_dir;
	/** Path to dictionary file */
	const char          *dictionary;
	/** Path to narrowband model */
	const char          *model_8k;
	/** Path to wideband model */
	const char          *model_16k;
	/** Preferred (default) model */
	pocketsphinx_model_e preferred_model;

	/** Sensitivity level */
	apr_size_t           sensitivity_level;
	/** Activity timeout (timeout to be used to switch to the activity state) */
	apr_size_t           activity_timeout;
	/** Inactivity timeout (timeout to be used to switch to the inactivity state) */
	apr_size_t           inactivity_timeout;

	/** Noinput timeout */
	apr_size_t           no_input_timeout;
	/** Recognition timeout */
	apr_size_t           recognition_timeout;
	/** Partial result checking timeout */
	apr_size_t           partial_result_timeout;

	/** Whether to save waveform or not */
	apt_bool_t           save_waveform;
	/** Directory to save waveform in */
	const char          *save_waveform_dir;
};

/** Load PocketSphinx properties */
apt_bool_t pocketsphinx_properties_load(pocketsphinx_properties_t *properties, 
										const char *file_path, 
										const apt_dir_layout_t *dir_layout,
										apr_pool_t *pool);

APT_END_EXTERN_C

#endif /* POCKETSPHINX_PROPERTIES_H */
