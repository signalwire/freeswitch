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

#include <apr_file_info.h>
#include "apt_dir_layout.h"

static apt_dir_layout_t* apt_dir_layout_alloc(apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = (apt_dir_layout_t*) apr_palloc(pool,sizeof(apt_dir_layout_t));
	dir_layout->conf_dir_path = NULL;
	dir_layout->plugin_dir_path = NULL;
	dir_layout->log_dir_path = NULL;
	dir_layout->data_dir_path = NULL;
	return dir_layout;
}

APT_DECLARE(apt_dir_layout_t*) apt_default_dir_layout_create(const char *root_dir_path, apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = apt_dir_layout_alloc(pool);
	if(root_dir_path) {
		apr_filepath_merge(&dir_layout->conf_dir_path,root_dir_path,"conf",0,pool);
		apr_filepath_merge(&dir_layout->plugin_dir_path,root_dir_path,"plugin",0,pool);
		apr_filepath_merge(&dir_layout->log_dir_path,root_dir_path,"log",0,pool);
		apr_filepath_merge(&dir_layout->data_dir_path,root_dir_path,"data",0,pool);
	}
	return dir_layout;
}

APT_DECLARE(apt_dir_layout_t*) apt_custom_dir_layout_create(
									const char *conf_dir_path,
									const char *plugin_dir_path,
									const char *log_dir_path,
									const char *data_dir_path,
									apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = apt_dir_layout_alloc(pool);
	if(conf_dir_path) {
		dir_layout->conf_dir_path = apr_pstrdup(pool,conf_dir_path);
	}
	if(plugin_dir_path) {
		dir_layout->plugin_dir_path = apr_pstrdup(pool,plugin_dir_path);
	}
	if(log_dir_path) {
		dir_layout->log_dir_path = apr_pstrdup(pool,log_dir_path);
	}
	if(data_dir_path) {
		dir_layout->data_dir_path = apr_pstrdup(pool,data_dir_path);
	}
	return dir_layout;
}

APT_DECLARE(char*) apt_datadir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool)
{
	if(dir_layout && dir_layout->data_dir_path && file_name) {
		char *file_path = NULL;
		if(apr_filepath_merge(&file_path,dir_layout->data_dir_path,file_name,0,pool) == APR_SUCCESS) {
			return file_path;
		}
	}
	return NULL;
}
