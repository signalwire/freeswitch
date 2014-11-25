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
 * $Id: unimrcp_server.c 2252 2014-11-21 02:45:15Z achaloyan@gmail.com $
 */

#include <stdlib.h>
#include <apr_xml.h>
#include <apr_version.h>
#include "uni_version.h"
#include "uni_revision.h"
#include "unimrcp_server.h"
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

#define XML_FILE_BUFFER_LENGTH    16000

/** UniMRCP server loader */
typedef struct unimrcp_server_loader_t unimrcp_server_loader_t;

/** UniMRCP server loader */
struct unimrcp_server_loader_t {
	/** MRCP server */
	mrcp_server_t    *server;
	/** Directory layout */
	apt_dir_layout_t *dir_layout;
	/** XML document */
	apr_xml_doc      *doc;
	/** Pool to allocate memory from */
	apr_pool_t       *pool;

	/** Default IP address (named property) */
	const char       *ip;
	/** Default external (NAT) IP address (named property) */
	const char       *ext_ip;
	
	/** Implicitly detected, cached IP address */
	const char      *auto_ip;
};

static apt_bool_t unimrcp_server_load(mrcp_server_t *mrcp_server, apt_dir_layout_t *dir_layout, apr_pool_t *pool);

/** Start UniMRCP server */
MRCP_DECLARE(mrcp_server_t*) unimrcp_server_start(apt_dir_layout_t *dir_layout)
{
	apr_pool_t *pool;
	mrcp_server_t *server;

	if(!dir_layout) {
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"UniMRCP Server ["UNI_VERSION_STRING"] [r"UNI_REVISION_STRING"]");
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"APR ["APR_VERSION_STRING"]");
	server = mrcp_server_create(dir_layout);
	if(!server) {
		return NULL;
	}
	pool = mrcp_server_memory_pool_get(server);
	if(!pool) {
		return NULL;
	}

	if(unimrcp_server_load(server,dir_layout,pool) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Load UniMRCP Server Document");
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


/** Check whether specified attribute is valid */
static APR_INLINE apt_bool_t is_attr_valid(const apr_xml_attr *attr)
{
	return (attr && attr->value && *attr->value != '\0');
}

/** Check whether specified attribute is enabled (true) */
static APR_INLINE apt_bool_t is_attr_enabled(const apr_xml_attr *attr)
{
	if(attr && strcasecmp(attr->value,"false") == 0) {
		return FALSE;
	}
	return TRUE;
}

/** Check whether cdata is valid */
static APR_INLINE apt_bool_t is_cdata_valid(const apr_xml_elem *elem)
{
	return (elem->first_cdata.first && elem->first_cdata.first->text);
}

/** Get text cdata */
static APR_INLINE const char* cdata_text_get(const apr_xml_elem *elem)
{
	return elem->first_cdata.first->text;
}

/** Get boolean cdata */
static APR_INLINE apt_bool_t cdata_bool_get(const apr_xml_elem *elem)
{
	return (strcasecmp(elem->first_cdata.first->text,"true") == 0) ? TRUE : FALSE;
}

/** Copy cdata */
static APR_INLINE char* cdata_copy(const apr_xml_elem *elem, apr_pool_t *pool)
{
	return apr_pstrdup(pool,elem->first_cdata.first->text);
}

/** Get generic "id" and "enable" attributes */
static apt_bool_t header_attribs_get(const apr_xml_elem *elem, const apr_xml_attr **id, const apr_xml_attr **enable)
{
	const apr_xml_attr *attr;
	if(!id || !enable) {
		return FALSE;
	}

	*id = NULL;
	*enable = NULL;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"id") == 0) {
			*id = attr;
		}
		else if(strcasecmp(attr->name,"enable") == 0) {
			*enable = attr;
		}
	}

	if(is_attr_valid(*id) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing Required Attribute <id> in Element <%s>",elem->name);
		return FALSE;
	}
	return TRUE;
}

/** Get generic "name" and "value" attributes */
static apt_bool_t name_value_attribs_get(const apr_xml_elem *elem, const apr_xml_attr **name, const apr_xml_attr **value)
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

static char* unimrcp_server_ip_address_get(unimrcp_server_loader_t *loader, const apr_xml_elem *elem)
{
	const apr_xml_attr *attr = NULL;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"type") == 0) {
			break;
		}
	}

	if(attr && strcasecmp(attr->value,"auto") == 0) {
		/* implicitly detect IP address, if not already detected */
		if(!loader->auto_ip) {
			char *auto_addr = DEFAULT_IP_ADDRESS;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Detecting IP Address");
			apt_ip_get(&auto_addr,loader->pool);
			loader->auto_ip = auto_addr;
		}
		return apr_pstrdup(loader->pool,loader->auto_ip);
	}
	else if(attr && strcasecmp(attr->value,"iface") == 0) {
		/* get IP address by network interface name */
		char *ip_addr = DEFAULT_IP_ADDRESS;
		if(is_cdata_valid(elem) == TRUE) {
			const char *iface_name = cdata_text_get(elem);
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Get IP Address by Interface [%s]", iface_name);
			apt_ip_get_by_iface(iface_name,&ip_addr,loader->pool);
		}
		return ip_addr;
	}

	if(is_cdata_valid(elem)) {
		/* use provided IP address */
		return cdata_copy(elem,loader->pool);
	}

	/* use default IP address */
	return apr_pstrdup(loader->pool,loader->ip);
}

/** Load resource */
static apt_bool_t unimrcp_server_resource_load(mrcp_resource_loader_t *resource_loader, const apr_xml_elem *root, apr_pool_t *pool)
{
	apt_str_t resource_class;
	const apr_xml_attr *id_attr;
	const apr_xml_attr *enable_attr;
	apt_string_reset(&resource_class);

	if(header_attribs_get(root,&id_attr,&enable_attr) == FALSE) {
		return FALSE;
	}
	
	if(is_attr_enabled(enable_attr) == FALSE) {
		return TRUE;
	}

	apt_string_set(&resource_class,id_attr->value);
	return mrcp_resource_load(resource_loader,&resource_class);
}

/** Load resource factory */
static apt_bool_t unimrcp_server_resource_factory_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;
	mrcp_resource_factory_t *resource_factory;
	mrcp_resource_loader_t *resource_loader = mrcp_resource_loader_create(FALSE,loader->pool);
	if(!resource_loader) {
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Resources");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"resource") == 0) {
			unimrcp_server_resource_load(resource_loader,elem,loader->pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	resource_factory = mrcp_resource_factory_get(resource_loader);
	return mrcp_server_resource_factory_register(loader->server,resource_factory);
}

/** Load SofiaSIP signaling agent */
static apt_bool_t unimrcp_server_sip_uas_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	mrcp_sig_agent_t *agent;
	mrcp_sofia_server_config_t *config;

	config = mrcp_sofiasip_server_config_alloc(loader->pool);
	config->local_port = DEFAULT_SIP_PORT;
	config->user_agent_name = DEFAULT_SOFIASIP_UA_NAME;
	config->origin = DEFAULT_SDP_ORIGIN;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading SofiaSIP Agent <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"sip-ip") == 0) {
			config->local_ip = unimrcp_server_ip_address_get(loader,elem);
		}
		else if(strcasecmp(elem->name,"sip-ext-ip") == 0) {
			config->ext_ip = unimrcp_server_ip_address_get(loader,elem);
		}
		else if(strcasecmp(elem->name,"sip-port") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->local_port = (apr_port_t)atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"sip-transport") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->transport = cdata_copy(elem,loader->pool);
			}
		}
		else if(strcasecmp(elem->name,"ua-name") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				const apr_xml_attr *attr = NULL;
				for(attr = elem->attr; attr; attr = attr->next) {
					if(strcasecmp(attr->name,"appendversion") == 0) {
						break;
					}
				}
				if(is_attr_enabled(attr)) {
					config->user_agent_name = apr_psprintf(loader->pool,"%s "UNI_VERSION_STRING,cdata_text_get(elem));
				}
				else {
					config->user_agent_name = cdata_copy(elem,loader->pool);
				}
			}
		}
		else if(strcasecmp(elem->name,"sdp-origin") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->origin = cdata_copy(elem,loader->pool);
			}
		}
		else if(strcasecmp(elem->name,"force-destination") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->force_destination = cdata_bool_get(elem);
			}
		}
		else if(strcasecmp(elem->name,"sip-t1") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->sip_t1 = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"sip-t2") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->sip_t2 = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"sip-t4") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->sip_t4 = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"sip-t1x64") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->sip_t1x64 = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"sip-message-output") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->tport_log = cdata_bool_get(elem);
			}
		}
		else if(strcasecmp(elem->name,"sip-message-dump") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				const char *root_path;
				const char *path = cdata_text_get(elem);
				if(loader->dir_layout && apr_filepath_root(&root_path,&path,0,loader->pool) == APR_ERELATIVE)
					config->tport_dump_file = apt_dir_layout_path_compose(
													loader->dir_layout,
													APT_LAYOUT_LOG_DIR,
													path,
													loader->pool);
				else
					config->tport_dump_file = cdata_copy(elem,loader->pool);
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	if(!config->local_ip) {
		/* use default IP address if not specified */
		config->local_ip = apr_pstrdup(loader->pool,loader->ip);
	}
	if(!config->ext_ip && loader->ext_ip) {
		/* use default ext IP address if not specified */
		config->ext_ip = apr_pstrdup(loader->pool,loader->ext_ip);
	}

	agent = mrcp_sofiasip_server_agent_create(id,config,loader->pool);
	return mrcp_server_signaling_agent_register(loader->server,agent);
}

/** Load UniRTSP signaling agent */
static apt_bool_t unimrcp_server_rtsp_uas_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	mrcp_sig_agent_t *agent;
	rtsp_server_config_t *config;

	config = mrcp_unirtsp_server_config_alloc(loader->pool);
	config->origin = DEFAULT_SDP_ORIGIN;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading UniRTSP Agent <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"rtsp-ip") == 0) {
			config->local_ip = unimrcp_server_ip_address_get(loader,elem);
		}
		else if(strcasecmp(elem->name,"rtsp-port") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->local_port = (apr_port_t)atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"sdp-origin") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->origin = cdata_copy(elem,loader->pool);
			}
		}
		else if(strcasecmp(elem->name,"max-connection-count") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->max_connection_count = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"force-destination") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				config->force_destination = cdata_bool_get(elem);
			}
		}
		else if(strcasecmp(elem->name,"resource-map") == 0) {
			const apr_xml_attr *name_attr;
			const apr_xml_attr *value_attr;
			const apr_xml_elem *child_elem;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Resource Map");
			for(child_elem = elem->first_child; child_elem; child_elem = child_elem->next) {
				if(name_value_attribs_get(child_elem,&name_attr,&value_attr) == TRUE) {
					apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",name_attr->value,value_attr->value);
					apr_table_set(config->resource_map,name_attr->value,value_attr->value);
				}
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	if(!config->local_ip) {
		/* use default IP address if not specified */
		config->local_ip = apr_pstrdup(loader->pool,loader->ip);
	}

	agent = mrcp_unirtsp_server_agent_create(id,config,loader->pool);
	return mrcp_server_signaling_agent_register(loader->server,agent);
}

/** Load MRCPv2 connection agent */
static apt_bool_t unimrcp_server_mrcpv2_uas_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	mrcp_connection_agent_t *agent;
	char *mrcp_ip = NULL;
	apr_port_t mrcp_port = DEFAULT_MRCP_PORT;
	apr_size_t max_connection_count = 100;
	apt_bool_t force_new_connection = FALSE;
	apr_size_t rx_buffer_size = 0;
	apr_size_t tx_buffer_size = 0;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading MRCPv2 Agent <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"mrcp-ip") == 0) {
			mrcp_ip = unimrcp_server_ip_address_get(loader,elem);
		}
		else if(strcasecmp(elem->name,"mrcp-port") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				mrcp_port = (apr_port_t)atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"max-connection-count") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				max_connection_count = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"force-new-connection") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				force_new_connection = cdata_bool_get(elem);
			}
		}
		else if(strcasecmp(elem->name,"rx-buffer-size") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				rx_buffer_size = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"tx-buffer-size") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				tx_buffer_size = atol(cdata_text_get(elem));
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	if(!mrcp_ip) {
		/* use default IP address if not specified */
		mrcp_ip = apr_pstrdup(loader->pool,loader->ip);
	}

	agent = mrcp_server_connection_agent_create(id,mrcp_ip,mrcp_port,max_connection_count,force_new_connection,loader->pool);
	if(agent) {
		if(rx_buffer_size) {
			mrcp_server_connection_rx_size_set(agent,rx_buffer_size);
		}
		if(tx_buffer_size) {
			mrcp_server_connection_tx_size_set(agent,tx_buffer_size);
		}
	}
	return mrcp_server_connection_agent_register(loader->server,agent);
}

/** Load media engine */
static apt_bool_t unimrcp_server_media_engine_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	mpf_engine_t *media_engine;
	unsigned long realtime_rate = 1;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Media Engine <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"realtime-rate") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				realtime_rate = atol(cdata_text_get(elem));
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	
	media_engine = mpf_engine_create(id,loader->pool);
	if(media_engine) {
		mpf_engine_scheduler_rate_set(media_engine,realtime_rate);
	}
	return mrcp_server_media_engine_register(loader->server,media_engine);
}

/** Load RTP factory */
static apt_bool_t unimrcp_server_rtp_factory_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	char *rtp_ip = NULL;
	char *rtp_ext_ip = NULL;
	mpf_termination_factory_t *rtp_factory;
	mpf_rtp_config_t *rtp_config;

	rtp_config = mpf_rtp_config_alloc(loader->pool);
	rtp_config->rtp_port_min = DEFAULT_RTP_PORT_MIN;
	rtp_config->rtp_port_max = DEFAULT_RTP_PORT_MAX;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading RTP Factory <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"rtp-ip") == 0) {
			rtp_ip = unimrcp_server_ip_address_get(loader,elem);
		}
		else if(strcasecmp(elem->name,"rtp-ext-ip") == 0) {
			rtp_ext_ip = unimrcp_server_ip_address_get(loader,elem);
		}
		else if(strcasecmp(elem->name,"rtp-port-min") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				rtp_config->rtp_port_min = (apr_port_t)atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"rtp-port-max") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				rtp_config->rtp_port_max = (apr_port_t)atol(cdata_text_get(elem));
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	if(rtp_ip) {
		apt_string_set(&rtp_config->ip,rtp_ip);
	}
	else {
		apt_string_set(&rtp_config->ip,loader->ip);
	}
	if(rtp_ext_ip) {
		apt_string_set(&rtp_config->ext_ip,rtp_ext_ip);
	}
	else if(loader->ext_ip){
		apt_string_set(&rtp_config->ext_ip,loader->ext_ip);
	}

	rtp_factory = mpf_rtp_termination_factory_create(rtp_config,loader->pool);
	return mrcp_server_rtp_factory_register(loader->server,rtp_factory,id);
}

/** Load plugin */
static apt_bool_t unimrcp_server_plugin_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	mrcp_engine_t *engine;
	mrcp_engine_config_t *config;
	char *plugin_file_name;
	char *plugin_path;
	const char *plugin_id = NULL;
	const char *plugin_name = NULL;
	const char *plugin_ext = NULL;
	apt_bool_t plugin_enabled = TRUE;
	const apr_xml_attr *attr;
	for(attr = root->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"id") == 0) {
			plugin_id = apr_pstrdup(loader->pool,attr->value);
		}
		else if(strcasecmp(attr->name,"name") == 0) {
			plugin_name = attr->value;
		}
		else if(strcasecmp(attr->name,"ext") == 0) {
			plugin_ext = attr->value;
		}
		else if(strcasecmp(attr->name,"enable") == 0) {
			plugin_enabled = is_attr_enabled(attr);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
		}
	}

	if(!plugin_id || !plugin_name) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing plugin id or name");
		return FALSE;
	}

	if(!plugin_enabled) {
		/* disabled plugin, just skip it */
		return TRUE;
	}

	if(!plugin_ext) {
		plugin_ext = DEFAULT_PLUGIN_EXT;
	}

	plugin_file_name = apr_psprintf(loader->pool,"%s.%s",plugin_name,plugin_ext);
	plugin_path = apt_dir_layout_path_compose(loader->dir_layout,APT_LAYOUT_PLUGIN_DIR,plugin_file_name,loader->pool);
	if(!plugin_path) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to compose plugin path %s",plugin_file_name);
		return FALSE;
	}

	config = mrcp_engine_config_alloc(loader->pool);

	/* load optional named and generic name/value params */
	if(root->first_child){
		const apr_xml_attr *attr_name;
		const apr_xml_attr *attr_value;
		const apr_xml_elem *elem;
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Engine Params");
		config->params = apr_table_make(loader->pool,1);
		for(elem = root->first_child; elem; elem = elem->next) {
			if(strcasecmp(elem->name,"max-channel-count") == 0) {
				if(is_cdata_valid(elem) == TRUE) {
					config->max_channel_count = atol(cdata_text_get(elem));
				}
			}
			else if(strcasecmp(elem->name,"param") == 0) {
				if(name_value_attribs_get(elem,&attr_name,&attr_value) == TRUE) {
					apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
					apr_table_set(config->params,attr_name->value,attr_value->value);
				}
			}
		}
	}

	engine = mrcp_server_engine_load(loader->server,plugin_id,plugin_path,config);
	return mrcp_server_engine_register(loader->server,engine);
}

/** Load plugin (engine) factory */
static apt_bool_t unimrcp_server_plugin_factory_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Plugin Factory");
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"engine") == 0) {
			unimrcp_server_plugin_load(loader,elem);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Load jitter buffer settings */
static apt_bool_t unimrcp_server_jb_settings_load(unimrcp_server_loader_t *loader, mpf_jb_config_t *jb, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Jitter Buffer Settings");
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"playout-delay") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				jb->initial_playout_delay = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"min-playout-delay") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				jb->min_playout_delay = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"max-playout-delay") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				jb->max_playout_delay = atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"adaptive") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				jb->adaptive = (apr_byte_t) atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"time-skew-detection") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				jb->time_skew_detection = (apr_byte_t) atol(cdata_text_get(elem));
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Load RTCP settings */
static apt_bool_t unimrcp_server_rtcp_settings_load(unimrcp_server_loader_t *loader, mpf_rtp_settings_t *rtcp_settings, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;
	const apr_xml_attr *attr = NULL;
	for(attr = root->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"enable") == 0) {
			break;
		}
	}

	if(is_attr_enabled(attr) == FALSE) {
		/* RTCP is disabled, skip the rest */
		return TRUE;
	}

	rtcp_settings->rtcp = TRUE;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading RTCP Settings");
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"rtcp-bye") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				rtcp_settings->rtcp_bye_policy = atoi(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"tx-interval") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				rtcp_settings->rtcp_tx_interval = (apr_uint16_t)atoi(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"rx-resolution") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				rtcp_settings->rtcp_rx_resolution = (apr_uint16_t)atol(cdata_text_get(elem));
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Load RTP settings */
static apt_bool_t unimrcp_server_rtp_settings_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	mpf_rtp_settings_t *rtp_settings = mpf_rtp_settings_alloc(loader->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading RTP Settings <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"jitter-buffer") == 0) {
			unimrcp_server_jb_settings_load(loader,&rtp_settings->jb_config,elem);
		}
		else if(strcasecmp(elem->name,"ptime") == 0) {
			if(is_cdata_valid(elem) == TRUE) {
				rtp_settings->ptime = (apr_uint16_t)atol(cdata_text_get(elem));
			}
		}
		else if(strcasecmp(elem->name,"codecs") == 0) {
			const apr_xml_attr *attr;
			const mpf_codec_manager_t *codec_manager = mrcp_server_codec_manager_get(loader->server);
			if(is_cdata_valid(elem) == TRUE && codec_manager) {
				mpf_codec_manager_codec_list_load(
					codec_manager,
					&rtp_settings->codec_list,
					cdata_text_get(elem),
					loader->pool);
			}
			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"own-preference") == 0) {
					rtp_settings->own_preferrence = is_attr_enabled(attr);
					break;
				}
			}
		}
		else if(strcasecmp(elem->name,"rtcp") == 0) {
			unimrcp_server_rtcp_settings_load(loader,rtp_settings,elem);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	return mrcp_server_rtp_settings_register(loader->server,rtp_settings,id);
}

/** Load map of resources and engines */
static apr_table_t* resource_engine_map_load(const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_attr *attr_name;
	const apr_xml_attr *attr_value;
	const apr_xml_elem *elem;
	apr_table_t *plugin_map = apr_table_make(pool,2);
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Plugin Map");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			if(name_value_attribs_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				apr_table_set(plugin_map,attr_name->value,attr_value->value);
			}
		}
	}
	return plugin_map;
}

/** Load MRCPv2 profile */
static apt_bool_t unimrcp_server_mrcpv2_profile_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	mrcp_server_profile_t *profile;
	mrcp_sig_agent_t *sip_agent = NULL;
	mrcp_connection_agent_t *mrcpv2_agent = NULL;
	mpf_engine_t *media_engine = NULL;
	mpf_termination_factory_t *rtp_factory = NULL;
	mpf_rtp_settings_t *rtp_settings = NULL;
	apr_table_t *resource_engine_map = NULL;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading MRCPv2 Profile <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);

		if(is_cdata_valid(elem) == FALSE) {
			continue;
		}

		if(strcasecmp(elem->name,"sip-uas") == 0) {
			sip_agent = mrcp_server_signaling_agent_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"mrcpv2-uas") == 0) {
			mrcpv2_agent = mrcp_server_connection_agent_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"media-engine") == 0) {
			media_engine = mrcp_server_media_engine_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"rtp-factory") == 0) {
			rtp_factory = mrcp_server_rtp_factory_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"rtp-settings") == 0) {
			rtp_settings = mrcp_server_rtp_settings_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"resource-engine-map") == 0) {
			resource_engine_map = resource_engine_map_load(elem,loader->pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create MRCPv2 Profile [%s]",id);
	profile = mrcp_server_profile_create(
				id,
				MRCP_VERSION_2,
				NULL,
				sip_agent,
				mrcpv2_agent,
				media_engine,
				rtp_factory,
				rtp_settings,
				loader->pool);
	return mrcp_server_profile_register(loader->server,profile,resource_engine_map);
}

/** Load MRCPv1 profile */
static apt_bool_t unimrcp_server_mrcpv1_profile_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root, const char *id)
{
	const apr_xml_elem *elem;
	mrcp_server_profile_t *profile;
	mrcp_sig_agent_t *rtsp_agent = NULL;
	mpf_engine_t *media_engine = NULL;
	mpf_termination_factory_t *rtp_factory = NULL;
	mpf_rtp_settings_t *rtp_settings = NULL;
	apr_table_t *resource_engine_map = NULL;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading MRCPv1 Profile <%s>",id);
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);

		if(is_cdata_valid(elem) == FALSE) {
			continue;
		}

		if(strcasecmp(elem->name,"rtsp-uas") == 0) {
			rtsp_agent = mrcp_server_signaling_agent_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"media-engine") == 0) {
			media_engine = mrcp_server_media_engine_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"rtp-factory") == 0) {
			rtp_factory = mrcp_server_rtp_factory_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"rtp-settings") == 0) {
			rtp_settings = mrcp_server_rtp_settings_get(loader->server,cdata_text_get(elem));
		}
		else if(strcasecmp(elem->name,"resource-engine-map") == 0) {
			resource_engine_map = resource_engine_map_load(elem,loader->pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create MRCPv1 Profile [%s]",id);
	profile = mrcp_server_profile_create(
				id,
				MRCP_VERSION_1,
				NULL,
				rtsp_agent,
				NULL,
				media_engine,
				rtp_factory,
				rtp_settings,
				loader->pool);
	return mrcp_server_profile_register(loader->server,profile,resource_engine_map);
}


/** Load properties */
static apt_bool_t unimrcp_server_properties_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Properties");
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Element <%s>",elem->name);
		if(strcasecmp(elem->name,"ip") == 0) {
			loader->ip = unimrcp_server_ip_address_get(loader,elem);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Property ip:%s",loader->ip);
		}
		else if(strcasecmp(elem->name,"ext-ip") == 0) {
			loader->ext_ip = unimrcp_server_ip_address_get(loader,elem);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Property ext-ip:%s",loader->ext_ip);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Load components */
static apt_bool_t unimrcp_server_components_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;
	const apr_xml_attr *id_attr;
	const apr_xml_attr *enable_attr;
	const char *id;

	/* Create codec manager first (probably it should be loaded from config either) */
	mpf_codec_manager_t *codec_manager = mpf_engine_codec_manager_create(loader->pool);
	if(codec_manager) {
		mrcp_server_codec_manager_register(loader->server,codec_manager);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Components");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"resource-factory") == 0) {
			unimrcp_server_resource_factory_load(loader,elem);
			continue;
		}
		if(strcasecmp(elem->name,"plugin-factory") == 0) {
			unimrcp_server_plugin_factory_load(loader,elem);
			continue;
		}
		
		/* get common "id" and "enable" attributes */
		if(header_attribs_get(elem,&id_attr,&enable_attr) == FALSE) {
			/* invalid id */
			continue;
		}
		if(is_attr_enabled(enable_attr) == FALSE) {
			/* disabled element, just skip it */
			continue;
		}
		id = apr_pstrdup(loader->pool,id_attr->value);

		if(strcasecmp(elem->name,"sip-uas") == 0) {
			unimrcp_server_sip_uas_load(loader,elem,id);
		}
		else if(strcasecmp(elem->name,"rtsp-uas") == 0) {
			unimrcp_server_rtsp_uas_load(loader,elem,id);
		}
		else if(strcasecmp(elem->name,"mrcpv2-uas") == 0) {
			unimrcp_server_mrcpv2_uas_load(loader,elem,id);
		}
		else if(strcasecmp(elem->name,"media-engine") == 0) {
			unimrcp_server_media_engine_load(loader,elem,id);
		}
		else if(strcasecmp(elem->name,"rtp-factory") == 0) {
			unimrcp_server_rtp_factory_load(loader,elem,id);
		}
		else if(strcasecmp(elem->name,"plugin-factory") == 0) {
			unimrcp_server_plugin_factory_load(loader,elem);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Load settings */
static apt_bool_t unimrcp_server_settings_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;
	const apr_xml_attr *id_attr;
	const apr_xml_attr *enable_attr;
	const char *id;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Settings");
	for(elem = root->first_child; elem; elem = elem->next) {
		/* get common "id" and "enable" attributes */
		if(header_attribs_get(elem,&id_attr,&enable_attr) == FALSE) {
			/* invalid id */
			continue;
		}
		if(is_attr_enabled(enable_attr) == FALSE) {
			/* disabled element, just skip it */
			continue;
		}
		id = apr_pstrdup(loader->pool,id_attr->value);

		if(strcasecmp(elem->name,"rtp-settings") == 0) {
			unimrcp_server_rtp_settings_load(loader,elem,id);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Load profiles */
static apt_bool_t unimrcp_server_profiles_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;
	const apr_xml_attr *id_attr;
	const apr_xml_attr *enable_attr;
	const char *id;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Profiles");
	for(elem = root->first_child; elem; elem = elem->next) {
		/* get common "id" and "enable" attributes */
		if(header_attribs_get(elem,&id_attr,&enable_attr) == FALSE) {
			/* invalid id */
			continue;
		}
		if(is_attr_enabled(enable_attr) == FALSE) {
			/* disabled element, just skip it */
			continue;
		}
		id = apr_pstrdup(loader->pool,id_attr->value);

		if(strcasecmp(elem->name,"mrcpv2-profile") == 0) {
			unimrcp_server_mrcpv2_profile_load(loader,elem,id);
		}
		else if(strcasecmp(elem->name,"mrcpv1-profile") == 0) {
			unimrcp_server_mrcpv1_profile_load(loader,elem,id);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Load misc parameters */
static apt_bool_t unimrcp_server_misc_load(unimrcp_server_loader_t *loader, const apr_xml_elem *root)
{
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Loading Misc Parameters");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"sofiasip-logger") == 0) {
			char *logger_list_str;
			char *logger_name;
			char *state;
			apr_xml_attr *attr;
			apt_bool_t redirect = FALSE;
			const char *loglevel_str = NULL;
			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"redirect") == 0) {
					if(attr->value && strcasecmp(attr->value,"true") == 0)
						redirect = TRUE;
				}
				else if(strcasecmp(attr->name,"loglevel") == 0) {
					loglevel_str = attr->value;
				}
			}
			
			logger_list_str = apr_pstrdup(loader->pool,cdata_text_get(elem));
			do {
				logger_name = apr_strtok(logger_list_str, ",", &state);
				if(logger_name) {
					mrcp_sofiasip_server_logger_init(logger_name,loglevel_str,redirect);
				}
				logger_list_str = NULL; /* make sure we pass NULL on subsequent calls of apr_strtok() */
			}
			while(logger_name);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}

/** Parse XML document */
static apr_xml_doc* unimrcp_server_doc_parse(const char *file_path, apr_pool_t *pool)
{
	apr_xml_parser *parser = NULL;
	apr_xml_doc *xml_doc = NULL;
	apr_file_t *fd = NULL;
	apr_status_t rv;

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Open Config File [%s]",file_path);
	rv = apr_file_open(&fd,file_path,APR_READ|APR_BINARY,0,pool);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open Config File [%s]",file_path);
		return NULL;
	}

	rv = apr_xml_parse_file(pool,&parser,&xml_doc,fd,XML_FILE_BUFFER_LENGTH);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse Config File [%s]",file_path);
		xml_doc = NULL;
	}

	apr_file_close(fd);
	return xml_doc;
}

static apt_bool_t unimrcp_server_load(mrcp_server_t *mrcp_server, apt_dir_layout_t *dir_layout, apr_pool_t *pool)
{
	const char *file_path;
	apr_xml_doc *doc;
	const apr_xml_elem *elem;
	const apr_xml_elem *root;
	const apr_xml_attr *attr;
	unimrcp_server_loader_t *loader;
	const char *version = NULL;

	file_path = apt_confdir_filepath_get(dir_layout,CONF_FILE_NAME,pool);
	if(!file_path) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Path to Conf File [%s]",CONF_FILE_NAME);
		return FALSE;
	}

	/* Parse XML document */
	doc = unimrcp_server_doc_parse(file_path,pool);
	if(!doc) {
		return FALSE;
	}

	root = doc->root;

	/* Match document name */
	if(!root || strcasecmp(root->name,"unimrcpserver") != 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Document <%s>",root ? root->name : "null");
		return FALSE;
	}

	/* Read attributes */
	for(attr = root->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"version") == 0) {
			version = attr->value;
		}
	}

	/* Check version number first */
	if(!version) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Version");
		return FALSE;
	}

	loader = apr_palloc(pool,sizeof(unimrcp_server_loader_t));
	loader->doc = doc;
	loader->server = mrcp_server;
	loader->dir_layout = dir_layout;
	loader->pool = pool;
	loader->ip = DEFAULT_IP_ADDRESS;
	loader->ext_ip = NULL;
	loader->auto_ip = NULL;

	/* Navigate through document */
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"properties") == 0) {
			unimrcp_server_properties_load(loader,elem);
		}
		else if(strcasecmp(elem->name,"components") == 0) {
			unimrcp_server_components_load(loader,elem);
		}
		else if(strcasecmp(elem->name,"settings") == 0) {
			unimrcp_server_settings_load(loader,elem);
		}
		else if(strcasecmp(elem->name,"profiles") == 0) {
			unimrcp_server_profiles_load(loader,elem);
		}
		else if(strcasecmp(elem->name,"misc") == 0) {
			unimrcp_server_misc_load(loader,elem);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
	return TRUE;
}
