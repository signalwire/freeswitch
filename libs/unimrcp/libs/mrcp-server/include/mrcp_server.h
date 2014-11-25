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
 * $Id: mrcp_server.h 2251 2014-11-21 02:36:44Z achaloyan@gmail.com $
 */

#ifndef MRCP_SERVER_H
#define MRCP_SERVER_H

/**
 * @file mrcp_server.h
 * @brief MRCP Server
 */ 

#include "mrcp_server_types.h"
#include "mrcp_engine_iface.h"
#include "mpf_rtp_descriptor.h"
#include "apt_task.h"

APT_BEGIN_EXTERN_C

/**
 * Create MRCP server instance.
 * @return the created server instance
 */
MRCP_DECLARE(mrcp_server_t*) mrcp_server_create(apt_dir_layout_t *dir_layout);

/**
 * Start message processing loop.
 * @param server the MRCP server to start
 * @return the created server instance
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_start(mrcp_server_t *server);

/**
 * Shutdown message processing loop.
 * @param server the MRCP server to shutdown
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_shutdown(mrcp_server_t *server);

/**
 * Destroy MRCP server.
 * @param server the MRCP server to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_destroy(mrcp_server_t *server);


/**
 * Register MRCP resource factory.
 * @param server the MRCP server to set resource factory for
 * @param resource_factory the resource factory to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_resource_factory_register(
								mrcp_server_t *server, 
								mrcp_resource_factory_t *resource_factory);

/**
 * Register MRCP engine.
 * @param server the MRCP server to set engine for
 * @param engine the engine to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_engine_register(
								mrcp_server_t *server, 
								mrcp_engine_t *engine);

/**
 * Register codec manager.
 * @param server the MRCP server to set codec manager for
 * @param codec_manager the codec manager to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_codec_manager_register(mrcp_server_t *server, mpf_codec_manager_t *codec_manager);

/**
 * Get registered codec manager.
 * @param server the MRCP server to get codec manager from
 */
MRCP_DECLARE(const mpf_codec_manager_t*) mrcp_server_codec_manager_get(const mrcp_server_t *server);

/**
 * Register media engine.
 * @param server the MRCP server to set media engine for
 * @param media_engine the media engine to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_media_engine_register(
								mrcp_server_t *server, 
								mpf_engine_t *media_engine);

/**
 * Register RTP termination factory.
 * @param server the MRCP server to set termination factory for
 * @param rtp_termination_factory the termination factory
 * @param name the name of the factory
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_rtp_factory_register(
								mrcp_server_t *server, 
								mpf_termination_factory_t *rtp_termination_factory, 
								const char *name);

/**
 * Register RTP settings.
 * @param server the MRCP server to set RTP settings for
 * @param rtp_settings the settings to set
 * @param name the name of the settings
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_rtp_settings_register(
								mrcp_server_t *server, 
								mpf_rtp_settings_t *rtp_settings, 
								const char *name);

/**
 * Register MRCP signaling agent.
 * @param server the MRCP server to set signaling agent for
 * @param signaling_agent the signaling agent to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_signaling_agent_register(
								mrcp_server_t *server, 
								mrcp_sig_agent_t *signaling_agent);

/**
 * Register MRCP connection agent (MRCPv2 only).
 * @param server the MRCP server to set connection agent for
 * @param connection_agent the connection agent to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_register(
								mrcp_server_t *server, 
								mrcp_connection_agent_t *connection_agent);

/** Create MRCP profile */
MRCP_DECLARE(mrcp_server_profile_t*) mrcp_server_profile_create(
										const char *id,
										mrcp_version_e mrcp_version,
										mrcp_resource_factory_t *resource_factory,
										mrcp_sig_agent_t *signaling_agent,
										mrcp_connection_agent_t *connection_agent,
										mpf_engine_t *media_engine,
										mpf_termination_factory_t *rtp_factory,
										mpf_rtp_settings_t *rtp_settings,
										apr_pool_t *pool);

/**
 * Register MRCP profile.
 * @param server the MRCP server to set profile for
 * @param profile the profile to set
 * @param plugin_map the map of engines (plugins)
 */
MRCP_DECLARE(apt_bool_t) mrcp_server_profile_register(
									mrcp_server_t *server, 
									mrcp_server_profile_t *profile,
									apr_table_t *plugin_map);

/**
 * Load MRCP engine as a plugin.
 * @param server the MRCP server to use
 * @param id the identifier of the plugin
 * @param path the path to the plugin to load
 * @param config the config of the engine
 */
MRCP_DECLARE(mrcp_engine_t*) mrcp_server_engine_load(
									mrcp_server_t *server, 
									const char *id,
									const char *path,
									mrcp_engine_config_t *config);

/**
 * Get memory pool.
 * @param server the MRCP server to get memory pool from
 */
MRCP_DECLARE(apr_pool_t*) mrcp_server_memory_pool_get(const mrcp_server_t *server);

/**
 * Get media engine by name.
 * @param server the MRCP server to get media engine from
 * @param name the name of the media engine to lookup
 */
MRCP_DECLARE(mpf_engine_t*) mrcp_server_media_engine_get(const mrcp_server_t *server, const char *name);

/**
 * Get RTP termination factory by name.
 * @param server the MRCP server to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mpf_termination_factory_t*) mrcp_server_rtp_factory_get(const mrcp_server_t *server, const char *name);

/** 
 * Get RTP settings by name
 * @param server the MRCP server to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mpf_rtp_settings_t*) mrcp_server_rtp_settings_get(const mrcp_server_t *server, const char *name);

/**
 * Get signaling agent by name.
 * @param server the MRCP server to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_server_signaling_agent_get(const mrcp_server_t *server, const char *name);

/**
 * Get connection agent by name.
 * @param server the MRCP server to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_server_connection_agent_get(const mrcp_server_t *server, const char *name);

/**
 * Get profile by name.
 * @param server the MRCP client to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mrcp_server_profile_t*) mrcp_server_profile_get(const mrcp_server_t *server, const char *name);

APT_END_EXTERN_C

#endif /* MRCP_SERVER_H */
