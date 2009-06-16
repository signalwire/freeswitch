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

#include "mrcp_default_factory.h"
#include "mrcp_synth_resource.h"
#include "mrcp_recog_resource.h"
#include "apt_log.h"

/** String table of MRCPv2 resources (mrcp_resource_types_e) */
static const apt_str_table_item_t mrcp_resource_string_table[] = {
	{{"speechsynth",11},6},
	{{"speechrecog",11},6}
};

/** Create default MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_default_factory_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource;
	mrcp_resource_factory_t *resource_factory;
	/* create resource factory instance */
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create MRCP Resource Factory [%d]",MRCP_RESOURCE_TYPE_COUNT);
	resource_factory = mrcp_resource_factory_create(MRCP_RESOURCE_TYPE_COUNT,pool);
	if(!resource_factory) {
		return NULL;
	}
	
	/* set resource string table */
	mrcp_resource_string_table_set(resource_factory,mrcp_resource_string_table);

	/* create and register resources */
	resource = mrcp_synth_resource_create(pool);
	if(resource) {
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Register Synthesizer Resource");
		mrcp_resource_register(resource_factory,resource,MRCP_SYNTHESIZER_RESOURCE);
	}
	
	resource = mrcp_recog_resource_create(pool);
	if(resource) {
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Register Recognizer Resource");
		mrcp_resource_register(resource_factory,resource,MRCP_RECOGNIZER_RESOURCE);
	}

	return resource_factory;
}
