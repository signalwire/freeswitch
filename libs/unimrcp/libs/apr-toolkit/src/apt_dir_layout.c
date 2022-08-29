/*
 * Copyright 2008-2015 Arsen Chaloyan
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
#include <apr_xml.h>
#include "apt_dir_layout.h"

/** Directories layout */
struct apt_dir_layout_t {
	/** Array of the directory paths the layout is composed of */
	const char **paths;
	/** Number of directories in the layout */
	apr_size_t   count;
};

/** Default labels matching the entries in configuration */
static const char *default_labels[APT_LAYOUT_DIR_COUNT] = {
	"confdir",    /* APT_LAYOUT_CONF_DIR */
	"plugindir",  /* APT_LAYOUT_PLUGIN_DIR */
	"logdir",     /* APT_LAYOUT_LOG_DIR */
	"datadir",    /* APT_LAYOUT_DATA_DIR */
	"vardir",     /* APT_LAYOUT_VAR_DIR */
};

static const char* apt_default_root_dir_path_get(apr_pool_t *pool)
{
	char *root_dir_path;
	char *cur_dir_path;
	/* Get the current directory */
	if(apr_filepath_get(&cur_dir_path,APR_FILEPATH_NATIVE,pool) != APR_SUCCESS)
		return NULL;

	/* Root directory is supposed to be one level up by default */
	if(apr_filepath_merge(&root_dir_path,cur_dir_path,"../",APR_FILEPATH_NATIVE,pool) != APR_SUCCESS)
		return FALSE;

	return root_dir_path;
}

static apt_bool_t apt_dir_layout_path_set_internal(apt_dir_layout_t *dir_layout, apr_size_t dir_entry_id, const char *path)
{
	if(dir_entry_id >= dir_layout->count)
		return FALSE;

	dir_layout->paths[dir_entry_id] = path;
	return TRUE;
}

APT_DECLARE(apt_dir_layout_t*) apt_dir_layout_create(apr_pool_t *pool)
{
	return apt_dir_layout_create_ext(APT_LAYOUT_DIR_COUNT,pool);
}

APT_DECLARE(apt_dir_layout_t*) apt_dir_layout_create_ext(apr_size_t count, apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = (apt_dir_layout_t*) apr_palloc(pool,sizeof(apt_dir_layout_t));
	dir_layout->count = count;
	dir_layout->paths = apr_pcalloc(pool,count*sizeof(char*));
	return dir_layout;
}

APT_DECLARE(apt_dir_layout_t*) apt_default_dir_layout_create(const char *root_dir_path, apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = apt_dir_layout_create_ext(APT_LAYOUT_DIR_COUNT,pool);

	if(!root_dir_path) {
		/* If root dir path is not specified, get the default one */
		root_dir_path = apt_default_root_dir_path_get(pool);
	}

	if(root_dir_path) {
		char *path;
		
		apr_filepath_merge(&path,root_dir_path,"conf",APR_FILEPATH_NATIVE,pool);
		apt_dir_layout_path_set_internal(dir_layout,APT_LAYOUT_CONF_DIR,path);

		apr_filepath_merge(&path,root_dir_path,"plugin",APR_FILEPATH_NATIVE,pool);
		apt_dir_layout_path_set_internal(dir_layout,APT_LAYOUT_PLUGIN_DIR,path);

		apr_filepath_merge(&path,root_dir_path,"log",APR_FILEPATH_NATIVE,pool);
		apt_dir_layout_path_set_internal(dir_layout,APT_LAYOUT_LOG_DIR,path);

		apr_filepath_merge(&path,root_dir_path,"data",APR_FILEPATH_NATIVE,pool);
		apt_dir_layout_path_set_internal(dir_layout,APT_LAYOUT_DATA_DIR,path);

		apr_filepath_merge(&path,root_dir_path,"var",APR_FILEPATH_NATIVE,pool);
		apt_dir_layout_path_set_internal(dir_layout,APT_LAYOUT_VAR_DIR,path);
	}
	return dir_layout;
}

APT_DECLARE(apt_dir_layout_t*) apt_custom_dir_layout_create(
									const char *conf_dir_path,
									const char *plugin_dir_path,
									const char *log_dir_path,
									const char *data_dir_path,
									const char *var_dir_path,
									apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = apt_dir_layout_create_ext(APT_LAYOUT_DIR_COUNT,pool);

	apt_dir_layout_path_set(dir_layout,APT_LAYOUT_CONF_DIR,conf_dir_path,pool);
	apt_dir_layout_path_set(dir_layout,APT_LAYOUT_PLUGIN_DIR,plugin_dir_path,pool);
	apt_dir_layout_path_set(dir_layout,APT_LAYOUT_LOG_DIR,log_dir_path,pool);
	apt_dir_layout_path_set(dir_layout,APT_LAYOUT_DATA_DIR,data_dir_path,pool);
	apt_dir_layout_path_set(dir_layout,APT_LAYOUT_VAR_DIR,log_dir_path,pool);

	return dir_layout;
}

static apt_bool_t apt_dir_entry_id_by_label(const char **labels, apr_size_t count, const char *name, apr_size_t *id)
{
	apr_size_t i;
	for(i=0; i<count; i++) {
		if(strcasecmp(labels[i],name) == 0) {
			if(id)
				*id = i;
			return TRUE;
		}
	}
	return FALSE;
}

static apr_xml_doc* apt_dir_layout_doc_parse(const char *file_path, apr_pool_t *pool)
{
	apr_xml_parser *parser = NULL;
	apr_xml_doc *xml_doc = NULL;
	apr_file_t *fd = NULL;
	apr_status_t rv;

	rv = apr_file_open(&fd,file_path,APR_READ|APR_BINARY,0,pool);
	if(rv != APR_SUCCESS) {
		return NULL;
	}

	rv = apr_xml_parse_file(pool,&parser,&xml_doc,fd,2000);
	if(rv != APR_SUCCESS) {
		xml_doc = NULL;
	}

	apr_file_close(fd);
	return xml_doc;
}

static APR_INLINE apr_status_t apt_dir_is_path_absolute(const char *path, apr_pool_t *pool)
{
	const char *root_path;
	const char *file_path = path;
	return apr_filepath_root(&root_path,&file_path,0,pool);
}

static char* apt_dir_layout_subdir_parse(const char *root_dir_path, const apr_xml_elem *elem, apr_pool_t *pool)
{
	char *path;
	char *full_path = NULL;
	apr_status_t status;

	if(!elem || !elem->first_cdata.first || !elem->first_cdata.first->text) {
		return NULL;
	}

	path = apr_pstrdup(pool,elem->first_cdata.first->text);
	apr_collapse_spaces(path,path);

	/* Check if path is absolute or relative */
	status = apt_dir_is_path_absolute(path,pool);
	if(status == APR_SUCCESS) {
		/* Absolute path specified */
		return path;
	}
	else if (status == APR_ERELATIVE) {
		/* Relative path specified -> merge it with the root path */
		if(apr_filepath_merge(&full_path,root_dir_path,path,APR_FILEPATH_NATIVE,pool) == APR_SUCCESS) {
			return full_path;
		}
	}

	/* WARNING: invalid path specified */
	return NULL;
}

APT_DECLARE(apt_bool_t) apt_dir_layout_load(apt_dir_layout_t *dir_layout, const char *config_file, apr_pool_t *pool)
{
	return apt_dir_layout_load_ext(dir_layout,config_file,default_labels,APT_LAYOUT_DIR_COUNT,pool);
}

APT_DECLARE(apt_bool_t) apt_dir_layout_load_ext(apt_dir_layout_t *dir_layout, const char *config_file, const char **labels, apr_size_t count, apr_pool_t *pool)
{
	apr_xml_doc *doc;
	const apr_xml_elem *elem;
	const apr_xml_elem *root;
	const apr_xml_attr *xml_attr;
	char *path;
	const char *root_dir_path = NULL;
	apr_size_t id;

	if(!dir_layout || !config_file || !labels || !count) {
		return FALSE;
	}

	/* Parse XML document */
	doc = apt_dir_layout_doc_parse(config_file,pool);
	if(!doc) {
		return FALSE;
	}

	root = doc->root;

	/* Match document name */
	if(!root || strcasecmp(root->name,"dirlayout") != 0) {
		/* Unknown document */
		return FALSE;
	}

	/* Find rootdir attribute */
	for(xml_attr = root->attr; xml_attr; xml_attr = xml_attr->next) {
		if(strcasecmp(xml_attr->name, "rootdir") == 0) {
			root_dir_path = xml_attr->value;
			break;
		}
	}

	if(root_dir_path) {
		/* If root dir path is specified, check if it is absolute or relative */
		apr_status_t status = apt_dir_is_path_absolute(root_dir_path,pool);
		if(status == APR_ERELATIVE) {
			/* Relative path specified -> make it absolute */
			char *full_path;
			char *cur_dir_path;
			/* Get the current directory */
			if(apr_filepath_get(&cur_dir_path,APR_FILEPATH_NATIVE,pool) != APR_SUCCESS)
				return FALSE;

			/* Merge it with path specified */
			if(apr_filepath_merge(&full_path,cur_dir_path,root_dir_path,APR_FILEPATH_NATIVE,pool) != APR_SUCCESS)
				return FALSE;
			root_dir_path = full_path;
		}
	}
	else {
		/* If root dir path is not specified, get the default one */
		root_dir_path = apt_default_root_dir_path_get(pool);
	}

	/* Navigate through document */
	for(elem = root->first_child; elem; elem = elem->next) {
		if(apt_dir_entry_id_by_label(labels,dir_layout->count,elem->name,&id) == TRUE) {
			path = apt_dir_layout_subdir_parse(root_dir_path,elem,pool);
			if(path) {
				apt_dir_layout_path_set_internal(dir_layout,id,path);
			}
		}
		else {
			/* Unknown element */
		}
	}
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_dir_layout_path_set(apt_dir_layout_t *dir_layout, apr_size_t dir_entry_id, const char *path, apr_pool_t *pool)
{
	if(!dir_layout || dir_entry_id >= dir_layout->count || !path)
		return FALSE;

	dir_layout->paths[dir_entry_id] = apr_pstrdup(pool,path);
	return TRUE;
}

APT_DECLARE(const char*) apt_dir_layout_path_get(const apt_dir_layout_t *dir_layout, apr_size_t dir_entry_id)
{
	if(!dir_layout || dir_entry_id >= dir_layout->count)
		return NULL;

	return dir_layout->paths[dir_entry_id];
}

APT_DECLARE(char*) apt_dir_layout_path_compose(const apt_dir_layout_t *dir_layout, apr_size_t dir_entry_id, const char *file_name, apr_pool_t *pool)
{
	char *file_path;
	if(!dir_layout || dir_entry_id >= dir_layout->count)
		return NULL;

	if(apr_filepath_merge(&file_path,dir_layout->paths[dir_entry_id],file_name,APR_FILEPATH_NATIVE,pool) == APR_SUCCESS) {
		return file_path;
	}
	return NULL;
}

APT_DECLARE(char*) apt_confdir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool)
{
	return apt_dir_layout_path_compose(dir_layout,APT_LAYOUT_CONF_DIR,file_name,pool);
}

APT_DECLARE(char*) apt_datadir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool)
{
	return apt_dir_layout_path_compose(dir_layout,APT_LAYOUT_DATA_DIR,file_name,pool);
}

APT_DECLARE(char*) apt_vardir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool)
{
	return apt_dir_layout_path_compose(dir_layout,APT_LAYOUT_VAR_DIR,file_name,pool);
}
