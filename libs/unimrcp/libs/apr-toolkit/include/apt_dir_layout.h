/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * $Id: apt_dir_layout.h 2204 2014-10-31 01:01:42Z achaloyan@gmail.com $
 */

#ifndef APT_DIR_LAYOUT_H
#define APT_DIR_LAYOUT_H

/**
 * @file apt_dir_layout.h
 * @brief Directories Layout
 */

#include "apt.h"

APT_BEGIN_EXTERN_C

/*
 * This define allows user applications to support both the old interface,
 * where members of apt_dir_layout_t structure were accessable to the
 * application, and the new opaque interface, where OPAQUE_DIR_LAYOUT
 * is defined.
 */
#define OPAQUE_DIR_LAYOUT

/** Directories layout declaration */
typedef struct apt_dir_layout_t apt_dir_layout_t;

/** Enumeration of directories the layout is composed of */
typedef enum {
	APT_LAYOUT_CONF_DIR,     /**< configuration directory */
	APT_LAYOUT_PLUGIN_DIR,   /**< plugin directory */
	APT_LAYOUT_LOG_DIR,      /**< log directory */
	APT_LAYOUT_DATA_DIR,     /**< data directory */
	APT_LAYOUT_VAR_DIR,      /**< var directory */

	APT_LAYOUT_DIR_COUNT,     /**< number of directories in the default layout */

	APT_LAYOUT_EXT_DIR = APT_LAYOUT_DIR_COUNT
} apt_dir_entry_id;

/**
 * Create the default directories layout based on the specified root directory.
 * @param root_dir_path the path to the root directory
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_dir_layout_t*) apt_default_dir_layout_create(const char *root_dir_path, apr_pool_t *pool);

/**
 * Create a custom directories layout based on the specified individual directories.
 * @param conf_dir_path the path to the config dir
 * @param plugin_dir_path the path to the plugin dir
 * @param log_dir_path the path to the log dir
 * @param var_dir_path the path to the var dir
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_dir_layout_t*) apt_custom_dir_layout_create(
									const char *conf_dir_path,
									const char *plugin_dir_path,
									const char *log_dir_path,
									const char *data_dir_path,
									const char *var_dir_path,
									apr_pool_t *pool);

/**
 * Create a bare directories layout.
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_dir_layout_t*) apt_dir_layout_create(apr_pool_t *pool);

/**
 * Create am extended bare directories layout.
 * @param count the number of directories in the layout
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_dir_layout_t*) apt_dir_layout_create_ext(apr_size_t count, apr_pool_t *pool);

/**
 * Load directories layout from the specified configuration file.
 * @param dir_layout the directory layout
 * @param config_file the path to the configuration file
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_dir_layout_load(apt_dir_layout_t *dir_layout, const char *config_file, apr_pool_t *pool);

/**
 * Load directories layout from the specified configuration file using the provided labels.
 * @param dir_layout the directory layout
 * @param config_file the path to the configuration file
 * @param labels the array of directory labels (configuration entries)
 * @param count the number of labels (normally equals the number of directories in the layout)
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_dir_layout_load_ext(apt_dir_layout_t *dir_layout, const char *config_file, const char **labels, apr_size_t count, apr_pool_t *pool);

/**
 * Set the path to the individual directory in the layout.
 * @param dir_layout the directory layout
 * @param dir_entry_id the directory id (apt_dir_entry_id)
 * @param path the directory path
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_dir_layout_path_set(apt_dir_layout_t *dir_layout, apr_size_t dir_entry_id, const char *path, apr_pool_t *pool);

/**
 * Get the path to the individual directory in the layout.
 * @param dir_layout the directory layout
 * @param dir_entry_id the directory id (apt_dir_entry_id)
 */
APT_DECLARE(const char*) apt_dir_layout_path_get(const apt_dir_layout_t *dir_layout, apr_size_t dir_entry_id);

/**
 * Compose a file path relative to the specified directory in the layout.
 * @param dir_layout the directory layout
 * @param dir_entry_id the directory id (apt_dir_entry_id)
 * @param file_name the file name to append to the directory path
 * @param pool the memory pool to use
 */
APT_DECLARE(char*) apt_dir_layout_path_compose(const apt_dir_layout_t *dir_layout, apr_size_t dir_entry_id, const char *file_name, apr_pool_t *pool);


/**
 * Compose a file path relative to config dir.
 * @param dir_layout the directory layout
 * @param file_name the file name
 * @param pool the memory pool to use
 */
APT_DECLARE(char*) apt_confdir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool);

/**
 * Compose a file path relative to data dir.
 * @param dir_layout the directory layout
 * @param file_name the file name
 * @param pool the memory pool to use
 */
APT_DECLARE(char*) apt_datadir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool);

/**
 * Compose a file path relative to var dir.
 * @param dir_layout the directory layout
 * @param file_name the file name
 * @param pool the memory pool to use
 */
APT_DECLARE(char*) apt_vardir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* APT_DIR_LAYOUT_H */
