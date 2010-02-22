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
#include "unimrcp_server.h"
#include "uni_version.h"
#include "mrcp_resource_loader.h"
#include "mpf_engine.h"
#include "mpf_codec_manager.h"
#include "mpf_rtp_termination_factory.h"
#include "mrcp_sofiasip_server_agent.h"
#include "mrcp_unirtsp_server_agent.h"
#include "mrcp_server_connection.h"
#include "apt_net.h"
#include "apt_log.h"

#define CONF_FILE_NAME            "unimrcpserver.xml"
#define DEFAULT_CONF_DIR_PATH     "../conf"
#define DEFAULT_PLUGIN_DIR_PATH   "../plugin"
#ifdef WIN32
#define DEFAULT_PLUGIN_EXT        "dll"
#else
#define DEFAULT_PLUGIN_EXT        "so"
#endif

#define DEFAULT_IP_ADDRESS        "127.0.0.1"
#define DEFAULT_SIP_PORT          8060
#define DEFAULT_RTSP_PORT         1554
#define DEFAULT_MRCP_PORT         1544
#define DEFAULT_RTP_PORT_MIN      5000
#define DEFAULT_RTP_PORT_MAX      6000

#define DEFAULT_SOFIASIP_UA_NAME  "UniMRCP SofiaSIP"
#define DEFAULT_SDP_ORIGIN        "UniMRCPServer"

#define XML_FILE_BUFFER_LENGTH    2000

static apr_xml_doc* unimrcp_server_config_parse(const char *path, apr_pool_t *pool);
static apt_bool_t unimrcp_server_config_load(mrcp_server_t *server, const char *plugin_dir_path, const apr_xml_doc *doc, apr_pool_t *pool);

/** Start UniMRCP server */
MRCP_DECLARE(mrcp_server_t*) unimrcp_server_start(apt_dir_layout_t *dir_layout)
{
	apr_pool_t *pool;
	apr_xml_doc *doc;
	mrcp_server_t *server;

	if(!dir_layout) {
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"UniMRCP Server ["UNI_VERSION_STRING"]");
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"APR ["APR_VERSION_STRING"]");
	server = mrcp_server_create(dir_layout);
	if(!server) {
		return NULL;
	}
	pool = mrcp_server_memory_pool_get(server);
	if(!pool) {
		return NULL;
	}

	doc = unimrcp_server_config_parse(dir_layout->conf_dir_path,pool);
	if(doc) {
		unimrcp_server_config_load(server,dir_layout->plugin_dir_path,doc,pool);
	}

	mrcp_server_start(server);
	return server;
}

/** Shutdown UniMRCP server */
MRCP_DECLARE(apt_bool_t) unimrcp_server_shutdown(mrcp_server_t *server)
{
	if(mrcp_server_shutdown(server) == FALSE) {
		return FALSE;
	}
	return mrcp_server_destroy(server);
}


/** Parse config file */
static apr_xml_doc* unimrcp_server_config_parse(const char *dir_path, apr_pool_t *pool)
{
	apr_xml_parser *parser = NULL;
	apr_xml_doc *doc = NULL;
	apr_file_t *fd = NULL;
	apr_status_t rv;
	const char *file_path;

	if(!dir_path) {
		dir_path = DEFAULT_CONF_DIR_PATH;
	}
	if(*dir_path == '\0') {
		file_path = CONF_FILE_NAME;
	}
	else {
		file_path = apr_psprintf(pool,"%s/%s",dir_path,CONF_FILE_NAME);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Open Config File [%s]",file_path);
	rv = apr_file_open(&fd,file_path,APR_READ|APR_BINARY,0,pool);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open Config File [%s]",file_path);
		return NULL;
	}

	rv = apr_xml_parse_file(pool,&parser,&doc,fd,XML_FILE_BUFFER_LENGTH);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse Config File [%s]",file_path);
		doc = NULL;
	}

	apr_file_close(fd);
	return doc;
}

static apt_bool_t param_name_value_get(const apr_xml_elem *elem, const apr_xml_attr **name, const apr_xml_attr **value)
{
	const apr_xml_attr *attr;
	if(!name || !value) {
		return FALSE;
	}

	*name = NULL;
	*value = NULL;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"name") == 0) {
			*name = attr;
		}
		else if(strcasecmp(attr->name,"value") == 0) {
			*value = attr;
		}
	}
	return (*name && *value) ? TRUE : FALSE;
}

static char* ip_addr_get(const char *value, apr_pool_t *pool)
{
	if(!value || strcasecmp(value,"auto") == 0) {
		char *addr = DEFAULT_IP_ADDRESS;
		apt_ip_get(&addr,pool);
		return addr;
	}
	return apr_pstrdup(pool,value);
}

/** Load map of MRCP resource names */
static apt_bool_t resource_map_load(apr_table_t *resource_map, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_attr *attr_name;
	const apr_xml_attr *attr_value;
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Resource Map");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				apr_table_set(resource_map,attr_name->value,attr_value->value);
			}
		}
	}    
	return TRUE;
}

/** Load map of plugins */
static apt_bool_t plugin_map_load(apr_table_t *plugin_map, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_attr *attr_name;
	const apr_xml_attr *attr_value;
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Plugin Map");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				apr_table_set(plugin_map,attr_name->value,attr_value->value);
			}
		}
	}    
	return TRUE;
}

/** Load SofiaSIP signaling agent */
static mrcp_sig_agent_t* unimrcp_server_sofiasip_agent_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	mrcp_sofia_server_config_t *config = mrcp_sofiasip_server_config_alloc(pool);
	config->local_ip = DEFAULT_IP_ADDRESS;
	config->local_port = DEFAULT_SIP_PORT;
	config->ext_ip = NULL;
	config->user_agent_name = DEFAULT_SOFIASIP_UA_NAME;
	config->origin = DEFAULT_SDP_ORIGIN;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading SofiaSIP Agent");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			const apr_xml_attr *attr_name;
			const apr_xml_attr *attr_value;
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				if(strcasecmp(attr_name->value,"sip-ip") == 0) {
					config->local_ip = ip_addr_get(attr_value->value,pool);
				}
				else if(strcasecmp(attr_name->value,"sip-ext-ip") == 0) {
					config->ext_ip = ip_addr_get(attr_value->value,pool);
				}
				else if(strcasecmp(attr_name->value,"sip-port") == 0) {
					config->local_port = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"sip-transport") == 0) {
					config->transport = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"ua-name") == 0) {
					config->user_agent_name = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"sdp-origin") == 0) {
					config->origin = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"force-destination") == 0) {
					config->force_destination = atoi(attr_value->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr_name->value);
				}
			}
		}
	}    
	return mrcp_sofiasip_server_agent_create(config,pool);
}

/** Load UniRTSP signaling agent */
static mrcp_sig_agent_t* unimrcp_server_rtsp_agent_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	rtsp_server_config_t *config = mrcp_unirtsp_server_config_alloc(pool);
	config->local_ip = DEFAULT_IP_ADDRESS;
	config->local_port = DEFAULT_RTSP_PORT;
	config->origin = DEFAULT_SDP_ORIGIN;


	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading UniRTSP Agent");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			const apr_xml_attr *attr_name;
			const apr_xml_attr *attr_value;
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				if(strcasecmp(attr_name->value,"rtsp-ip") == 0) {
					config->local_ip = ip_addr_get(attr_value->value,pool);
				}
				else if(strcasecmp(attr_name->value,"rtsp-port") == 0) {
					config->local_port = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"sdp-origin") == 0) {
					config->origin = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"max-connection-count") == 0) {
					config->max_connection_count = atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"force-destination") == 0) {
					config->force_destination = atoi(attr_value->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr_name->value);
				}
			}
		}
		else if(strcasecmp(elem->name,"resourcemap") == 0) {
			resource_map_load(config->resource_map,elem,pool);
		}
	}
	return mrcp_unirtsp_server_agent_create(config,pool);
}

/** Load signaling agents */
static apt_bool_t unimrcp_server_signaling_agents_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Signaling Agents");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"agent") == 0) {
			mrcp_sig_agent_t *sig_agent = NULL;
			const char *name = NULL;
			const apr_xml_attr *attr;
			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"name") == 0) {
					name = apr_pstrdup(pool,attr->value);
				}
				else if(strcasecmp(attr->name,"class") == 0) {
					if(strcasecmp(attr->value,"SofiaSIP") == 0) {
						sig_agent = unimrcp_server_sofiasip_agent_load(server,elem,pool);
					}
					else if(strcasecmp(attr->value,"UniRTSP") == 0) {
						sig_agent = unimrcp_server_rtsp_agent_load(server,elem,pool);
					}
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
				}
			}
			if(sig_agent) {
				mrcp_server_signaling_agent_register(server,sig_agent,name);
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}

/** Load MRCPv2 connection agent */
static mrcp_connection_agent_t* unimrcp_server_connection_agent_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	char *mrcp_ip = DEFAULT_IP_ADDRESS;
	apr_port_t mrcp_port = DEFAULT_MRCP_PORT;
	apr_size_t max_connection_count = 100;
	apt_bool_t force_new_connection = FALSE;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading MRCPv2 Agent");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			const apr_xml_attr *attr_name;
			const apr_xml_attr *attr_value;
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				if(strcasecmp(attr_name->value,"mrcp-ip") == 0) {
					mrcp_ip = ip_addr_get(attr_value->value,pool);
				}
				else if(strcasecmp(attr_name->value,"mrcp-port") == 0) {
					mrcp_port = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"max-connection-count") == 0) {
					max_connection_count = atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"force-new-connection") == 0) {
					force_new_connection = atoi(attr_value->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr_name->value);
				}
			}
		}
	}    
	return mrcp_server_connection_agent_create(mrcp_ip,mrcp_port,max_connection_count,force_new_connection,pool);
}

/** Load MRCPv2 conection agents */
static apt_bool_t unimrcp_server_connection_agents_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Connection Agents");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"agent") == 0) {
			mrcp_connection_agent_t *connection_agent;
			const char *name = NULL;
			const apr_xml_attr *attr;
			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"name") == 0) {
					name = apr_pstrdup(pool,attr->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
				}
			}
			connection_agent = unimrcp_server_connection_agent_load(server,elem,pool);
			if(connection_agent) {
				mrcp_server_connection_agent_register(server,connection_agent,name);
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}

/** Load RTP termination factory */
static mpf_termination_factory_t* unimrcp_server_rtp_factory_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	char *rtp_ip = DEFAULT_IP_ADDRESS;
	char *rtp_ext_ip = NULL;
	mpf_rtp_config_t *rtp_config = mpf_rtp_config_create(pool);
	rtp_config->rtp_port_min = DEFAULT_RTP_PORT_MIN;
	rtp_config->rtp_port_max = DEFAULT_RTP_PORT_MAX;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading RTP Termination Factory");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			const apr_xml_attr *attr_name;
			const apr_xml_attr *attr_value;
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				if(strcasecmp(attr_name->value,"rtp-ip") == 0) {
					rtp_ip = ip_addr_get(attr_value->value,pool);
				}
				else if(strcasecmp(attr_name->value,"rtp-ext-ip") == 0) {
					rtp_ext_ip = ip_addr_get(attr_value->value,pool);
				}
				else if(strcasecmp(attr_name->value,"rtp-port-min") == 0) {
					rtp_config->rtp_port_min = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtp-port-max") == 0) {
					rtp_config->rtp_port_max = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"playout-delay") == 0) {
					rtp_config->jb_config.initial_playout_delay = atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"min-playout-delay") == 0) {
					rtp_config->jb_config.min_playout_delay = atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"max-playout-delay") == 0) {
					rtp_config->jb_config.max_playout_delay = atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"codecs") == 0) {
					const mpf_codec_manager_t *codec_manager = mrcp_server_codec_manager_get(server);
					if(codec_manager) {
						mpf_codec_manager_codec_list_load(codec_manager,&rtp_config->codec_list,attr_value->value,pool);
					}
				}
				else if(strcasecmp(attr_name->value,"ptime") == 0) {
					rtp_config->ptime = (apr_uint16_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"own-preference") == 0) {
					rtp_config->own_preferrence = atoi(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtcp") == 0) {
					rtp_config->rtcp = atoi(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtcp-bye") == 0) {
					rtp_config->rtcp_bye_policy = atoi(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtcp-tx-interval") == 0) {
					rtp_config->rtcp_tx_interval = (apr_uint16_t)atoi(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtcp-rx-resolution") == 0) {
					rtp_config->rtcp_rx_resolution = (apr_uint16_t)atol(attr_value->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr_name->value);
				}
			}
		}
	}
	apt_string_set(&rtp_config->ip,rtp_ip);
	if(rtp_ext_ip) {
		apt_string_set(&rtp_config->ext_ip,rtp_ext_ip);
	}
	return mpf_rtp_termination_factory_create(rtp_config,pool);
}

/** Load media engines */
static apt_bool_t unimrcp_server_media_engines_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;

	/* create codec manager first */
	mpf_codec_manager_t *codec_manager = mpf_engine_codec_manager_create(pool);
	if(codec_manager) {
		mrcp_server_codec_manager_register(server,codec_manager);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Media Engines");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"engine") == 0) {
			mpf_engine_t *media_engine;
			unsigned long realtime_rate = 1;
			const char *name = NULL;
			const apr_xml_attr *attr;
			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"name") == 0) {
					name = apr_pstrdup(pool,attr->value);
				}
				else if(strcasecmp(attr->name,"realtime-rate") == 0) {
					realtime_rate = atol(attr->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
				}
			}
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Media Engine");
			media_engine = mpf_engine_create(pool);
			if(media_engine) {
				mpf_engine_scheduler_rate_set(media_engine,realtime_rate);
				mrcp_server_media_engine_register(server,media_engine,name);
			}
		}
		else if(strcasecmp(elem->name,"rtp") == 0) {
			mpf_termination_factory_t *rtp_factory;
			const char *name = NULL;
			const apr_xml_attr *attr;
			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"name") == 0) {
					name = apr_pstrdup(pool,attr->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
				}
			}
			rtp_factory = unimrcp_server_rtp_factory_load(server,elem,pool);
			if(rtp_factory) {
				mrcp_server_rtp_factory_register(server,rtp_factory,name);
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}

/** Load resource */
static apt_bool_t unimrcp_server_resource_load(mrcp_server_t *server, mrcp_resource_loader_t *resource_loader, const apr_xml_elem *root, apr_pool_t *pool)
{
	apt_str_t resource_class;
	apt_bool_t resource_enabled = TRUE;
	const apr_xml_attr *attr;
	apt_string_reset(&resource_class);
	for(attr = root->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"class") == 0) {
			apt_string_set(&resource_class,attr->value);
		}
		else if(strcasecmp(attr->name,"enable") == 0) {
			resource_enabled = atoi(attr->value);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}

	if(!resource_class.buf || !resource_enabled) {
		return FALSE;
	}

	return mrcp_resource_load(resource_loader,&resource_class);
}

/** Load resources */
static apt_bool_t unimrcp_server_resources_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	mrcp_resource_factory_t *resource_factory;
	mrcp_resource_loader_t *resource_loader = mrcp_resource_loader_create(FALSE,pool);
	if(!resource_loader) {
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Resources");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"resource") == 0) {
			unimrcp_server_resource_load(server,resource_loader,elem,pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	
	resource_factory = mrcp_resource_factory_get(resource_loader);
	mrcp_server_resource_factory_register(server,resource_factory);
	return TRUE;
}

/** Load plugin */
static apt_bool_t unimrcp_server_plugin_load(mrcp_server_t *server, const char *plugin_dir_path, const apr_xml_elem *root, apr_pool_t *pool)
{
	mrcp_engine_config_t *config;
	const char *plugin_class = NULL;
	const char *plugin_ext = NULL;
	const char *plugin_path = NULL;
	apt_bool_t plugin_enabled = TRUE;
	const apr_xml_attr *attr;
	config = mrcp_engine_config_alloc(pool);
	for(attr = root->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"name") == 0) {
			config->name = apr_pstrdup(pool,attr->value);
		}
		else if(strcasecmp(attr->name,"class") == 0) {
			plugin_class = attr->value;
		}
		else if(strcasecmp(attr->name,"ext") == 0) {
			plugin_ext = attr->value;
		}
		else if(strcasecmp(attr->name,"enable") == 0) {
			plugin_enabled = atoi(attr->value);
		}
		else if(strcasecmp(attr->name,"max-channel-count") == 0) {
			config->max_channel_count = atol(attr->value);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}

	if(!plugin_class || !plugin_enabled) {
		return FALSE;
	}
	if(!plugin_dir_path) {
		plugin_dir_path = DEFAULT_PLUGIN_DIR_PATH;
	}
	if(!plugin_ext) {
		plugin_ext = DEFAULT_PLUGIN_EXT;
	}

	if(*plugin_dir_path == '\0') {
		plugin_path = apr_psprintf(pool,"%s.%s",plugin_class,plugin_ext);
	}
	else {
		plugin_path = apr_psprintf(pool,"%s/%s.%s",plugin_dir_path,plugin_class,plugin_ext);
	}

	/* load optional name/value params */
	if(root->first_child){
		const apr_xml_attr *attr_name;
		const apr_xml_attr *attr_value;
		const apr_xml_elem *elem;
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Engine Params");
		config->params = apr_table_make(pool,1);
		for(elem = root->first_child; elem; elem = elem->next) {
			if(strcasecmp(elem->name,"param") == 0) {
				if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
					apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
					apr_table_set(config->params,attr_name->value,attr_value->value);
				}
			}
		}
	}

	return mrcp_server_plugin_register(server,plugin_path,config);
}

/** Load plugins */
static apt_bool_t unimrcp_server_plugins_load(mrcp_server_t *server, const char *plugin_dir_path, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Engine Plugins");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"engine") == 0) {
			unimrcp_server_plugin_load(server,plugin_dir_path,elem,pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}


/** Load settings */
static apt_bool_t unimrcp_server_settings_load(mrcp_server_t *server, const char *plugin_dir_path, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Settings");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"signaling") == 0) {
			unimrcp_server_signaling_agents_load(server,elem,pool);
		}
		else if(strcasecmp(elem->name,"connection") == 0) {
			unimrcp_server_connection_agents_load(server,elem,pool);
		}
		else if(strcasecmp(elem->name,"media") == 0) {
			unimrcp_server_media_engines_load(server,elem,pool);
		}
		else if(strcasecmp(elem->name,"plugin") == 0) {
			unimrcp_server_plugins_load(server,plugin_dir_path,elem,pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}

/** Load profile */
static apt_bool_t unimrcp_server_profile_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const char *name = NULL;
	mrcp_profile_t *profile;
	mrcp_sig_agent_t *sig_agent = NULL;
	mrcp_connection_agent_t *cnt_agent = NULL;
	mpf_engine_t *media_engine = NULL;
	mpf_termination_factory_t *rtp_factory = NULL;
	apr_table_t *plugin_map = NULL;
	const apr_xml_elem *elem;
	const apr_xml_attr *attr;
	for(attr = root->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"name") == 0) {
			name = apr_pstrdup(pool,attr->value);
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Profile [%s]",name);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}
	if(!name) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Load Profile: no profile name specified");
		return FALSE;
	}
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			const apr_xml_attr *attr_name;
			const apr_xml_attr *attr_value;
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Loading Profile %s [%s]",attr_name->value,attr_value->value);
				if(strcasecmp(attr_name->value,"signaling-agent") == 0) {
					sig_agent = mrcp_server_signaling_agent_get(server,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"connection-agent") == 0) {
					cnt_agent = mrcp_server_connection_agent_get(server,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"media-engine") == 0) {
					media_engine = mrcp_server_media_engine_get(server,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtp-factory") == 0) {
					rtp_factory = mrcp_server_rtp_factory_get(server,attr_value->value);
				}
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr_name->value);
				}
			}
		}
		else if(strcasecmp(elem->name,"pluginmap") == 0) {
			plugin_map = apr_table_make(pool,2);
			plugin_map_load(plugin_map,elem,pool);
		}
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create Profile [%s]",name);
	profile = mrcp_server_profile_create(NULL,sig_agent,cnt_agent,media_engine,rtp_factory,pool);
	return mrcp_server_profile_register(server,profile,plugin_map,name);
}

/** Load profiles */
static apt_bool_t unimrcp_server_profiles_load(mrcp_server_t *server, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Profiles");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"profile") == 0) {
			unimrcp_server_profile_load(server,elem,pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
    return TRUE;
}

/** Load configuration (settings and profiles) */
static apt_bool_t unimrcp_server_config_load(mrcp_server_t *server, const char *plugin_dir_path, const apr_xml_doc *doc, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	const apr_xml_elem *root = doc->root;
	if(!root || strcasecmp(root->name,"unimrcpserver") != 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Document");
		return FALSE;
	}
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"resources") == 0) {
			unimrcp_server_resources_load(server,elem,pool);
		}
		else if(strcasecmp(elem->name,"settings") == 0) {
			unimrcp_server_settings_load(server,plugin_dir_path,elem,pool);
		}
		else if(strcasecmp(elem->name,"profiles") == 0) {
			unimrcp_server_profiles_load(server,elem,pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
    
	return TRUE;
}
