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

#ifndef __APT_DIR_LAYOUT_H__
#define __APT_DIR_LAYOUT_H__

/**
 * @file apt_dir_layout.h
 * @brief Directory Layout
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/** Directory layout declaration */
typedef struct apt_dir_layout_t apt_dir_layout_t;

/** Directory layout */
struct apt_dir_layout_t {
	/** Path to config dir */
	char *conf_dir_path;
	/** Path to plugin dir */
	char *plugin_dir_path;
	/** Path to log dir */
	char *log_dir_path;
	/** Path to data dir */
	char *data_dir_path;
};

/**
 * Create (allocate) the structure of default directories layout.
 */
APT_DECLARE(apt_dir_layout_t*) apt_default_dir_layout_create(const char *root_dir_path, apr_pool_t *pool);

/**
 * Create (allocate) the structure of custom directories layout.
 */
APT_DECLARE(apt_dir_layout_t*) apt_custom_dir_layout_create(
									const char *conf_dir_path,
									const char *plugin_dir_path,
									const char *log_dir_path,
									const char *data_dir_path,
									apr_pool_t *pool);

/**
 * Construct file path with the given file name relative to data dir.
 */
APT_DECLARE(char*) apt_datadir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__APT_DIR_LAYOUT_H__*/
