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

#include <stdlib.h>
#include <apr_xml.h>
#include "pocketsphinx_properties.h"
#include "apt_log.h"

static const apr_xml_elem* pocketsphinx_document_load(const char *file_path, apr_pool_t *pool)
{
	apr_xml_parser *parser = NULL;
	apr_xml_doc *doc = NULL;
	const apr_xml_elem *root;
	apr_file_t *fd = NULL;
	apr_status_t rv;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Open PocketSphinx Config File [%s]",file_path);
	rv = apr_file_open(&fd,file_path,APR_READ|APR_BINARY,0,pool);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open PocketSphinx Config File [%s]",file_path);
		return FALSE;
	}

	rv = apr_xml_parse_file(pool,&parser,&doc,fd,2000);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse PocketSphinx Config File [%s]",file_path);
		apr_file_close(fd);
		return FALSE;
	}

	root = doc->root;
	if(!root || strcasecmp(root->name,"pocketsphinx") != 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Document <%s>",root->name);
		apr_file_close(fd);
		return FALSE;
	}

	apr_file_close(fd);
	return root;
}

static apt_bool_t sensitivity_properties_load(pocketsphinx_properties_t *properties, const apr_xml_elem *elem, apr_pool_t *pool)
{
	const apr_xml_attr *attr;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"level") == 0) {
			properties->sensitivity_level = atol(attr->value);
		}
		else if(strcasecmp(attr->name,"timeout") == 0) {
			properties->sensitivity_timeout = atol(attr->value);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}
	return TRUE;
}

static apt_bool_t timer_properties_load(pocketsphinx_properties_t *properties, const apr_xml_elem *elem, apr_pool_t *pool)
{
	const apr_xml_attr *attr;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"noinput-timeout") == 0) {
			properties->no_input_timeout = atol(attr->value);
		}
		else if(strcasecmp(attr->name,"recognition-timeout") == 0) {
			properties->recognition_timeout = atol(attr->value);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}
	return TRUE;
}

static apt_bool_t model_properties_load(pocketsphinx_properties_t *properties, const apr_xml_elem *elem, apr_pool_t *pool)
{
	const apr_xml_attr *attr;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"dir") == 0) {
			properties->data_dir = apr_pstrdup(pool,attr->value);
		}
		else if(strcasecmp(attr->name,"narrowband") == 0) {
			properties->model_8k = apr_pstrdup(pool,attr->value);
		}
		else if(strcasecmp(attr->name,"wideband") == 0) {
			properties->model_16k = apr_pstrdup(pool,attr->value);
		}
		else if(strcasecmp(attr->name,"dictionary") == 0) {
			properties->dictionary = apr_pstrdup(pool,attr->value);
		}
		else if(strcasecmp(attr->name,"preferred") == 0) {
			if(strcasecmp(attr->value,"narrowband") == 0) {
				properties->preferred_model = POCKETSPHINX_MODEL_NARROWBAND;
			}
			else if(strcasecmp(attr->value,"wideband") == 0) {
				properties->preferred_model = POCKETSPHINX_MODEL_WIDEBAND;
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}
	return TRUE;
}

static apt_bool_t save_waveform_properties_load(pocketsphinx_properties_t *properties, const apr_xml_elem *elem, apr_pool_t *pool)
{
	const apr_xml_attr *attr;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"dir") == 0) {
			properties->save_waveform_dir = apr_pstrdup(pool,attr->value);
		}
		else if(strcasecmp(attr->name,"enable") == 0) {
			properties->save_waveform = atoi(attr->value);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}
	return TRUE;
}

apt_bool_t pocketsphinx_properties_load(pocketsphinx_properties_t *properties, 
										const char *file_path, 
										const apt_dir_layout_t *dir_layout, 
										apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	const apr_xml_elem *root;
	char *path = NULL;

	/* reset or set default properties */
	properties->data_dir = NULL;
	properties->dictionary = NULL;
	properties->model_8k = NULL;
	properties->model_16k = NULL;
	properties->preferred_model = POCKETSPHINX_MODEL_NARROWBAND;

	properties->no_input_timeout = 10000;
	properties->recognition_timeout = 15000;
	properties->partial_result_timeout = 100;

	properties->save_waveform = TRUE;
	properties->save_waveform_dir = NULL;

	root = pocketsphinx_document_load(file_path,pool);
	if(root) {
		for(elem = root->first_child; elem; elem = elem->next) {
			if(strcasecmp(elem->name,"sensitivity") == 0) {
				sensitivity_properties_load(properties,elem,pool);
			}
			else if(strcasecmp(elem->name,"timers") == 0) {
				timer_properties_load(properties,elem,pool);
			}
			else if(strcasecmp(elem->name,"model") == 0) {
				model_properties_load(properties,elem,pool);
			}
			else if(strcasecmp(elem->name,"save-waveform") == 0) {
				save_waveform_properties_load(properties,elem,pool);
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
			}
		}
	}

	/* verify loaded properties */
	if(!properties->data_dir || *properties->data_dir == '\0') {
		properties->data_dir = dir_layout->data_dir_path;
	}
	if(!properties->save_waveform_dir || *properties->save_waveform_dir == '\0') {
		properties->save_waveform_dir = dir_layout->data_dir_path;
	}

	if(!properties->dictionary) {
		properties->dictionary = "default.dic";
	}
	if(!properties->model_8k) {
		properties->model_8k = "communicator";
	}
	if(!properties->model_16k) {
		properties->model_16k = "wsj1";
	}

	if(apr_filepath_merge(&path,properties->data_dir,properties->dictionary,0,pool) == APR_SUCCESS) {
		properties->dictionary = path;
	}
	if(apr_filepath_merge(&path,properties->data_dir,properties->model_8k,0,pool) == APR_SUCCESS) {
		properties->model_8k = path;
	}
	if(apr_filepath_merge(&path,properties->data_dir,properties->model_16k,0,pool) == APR_SUCCESS) {
		properties->model_16k = path;
	}

	return TRUE;
}
