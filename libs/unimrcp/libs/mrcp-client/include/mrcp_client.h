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

#ifndef MRCP_CLIENT_H
#define MRCP_CLIENT_H

/**
 * @file mrcp_client.h
 * @brief MRCP Client
 */ 

#include "mrcp_client_types.h"
#include "mpf_rtp_descriptor.h"
#include "apt_task.h"

APT_BEGIN_EXTERN_C

/** Event handler used in case of asynchronous start */
typedef void (*mrcp_client_handler_f)(apt_bool_t status);

/**
 * Create MRCP client instance.
 * @return the created client instance
 */
MRCP_DECLARE(mrcp_client_t*) mrcp_client_create(apt_dir_layout_t *dir_layout);

/**
 * Set asynchronous start mode.
 * @param client the MRCP client to set mode for
 * @param handler the event handler to signal start completion
 */
MRCP_DECLARE(void) mrcp_client_async_start_set(mrcp_client_t *client, mrcp_client_handler_f handler);


/**
 * Start message processing loop.
 * @param client the MRCP client to start
 * @return the created client instance
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_start(mrcp_client_t *client);

/**
 * Shutdown message processing loop.
 * @param client the MRCP client to shutdown
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_shutdown(mrcp_client_t *client);

/**
 * Destroy MRCP client.
 * @param client the MRCP client to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_destroy(mrcp_client_t *client);


/**
 * Register MRCP resource factory.
 * @param client the MRCP client to set resource factory for
 * @param resource_factory the resource factory to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_resource_factory_register(mrcp_client_t *client, mrcp_resource_factory_t *resource_factory);

/**
 * Register codec manager.
 * @param client the MRCP client to set codec manager for
 * @param codec_manager the codec manager to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_codec_manager_register(mrcp_client_t *client, mpf_codec_manager_t *codec_manager);

/**
 * Get registered codec manager.
 * @param client the MRCP client to get codec manager from
 */
MRCP_DECLARE(const mpf_codec_manager_t*) mrcp_client_codec_manager_get(const mrcp_client_t *client);

/**
 * Register media engine.
 * @param client the MRCP client to set media engine for
 * @param media_engine the media engine to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_media_engine_register(mrcp_client_t *client, mpf_engine_t *media_engine);

/**
 * Register RTP termination factory.
 * @param client the MRCP client to set termination factory for
 * @param rtp_termination_factory the termination factory
 * @param name the name of the factory
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_rtp_factory_register(mrcp_client_t *client, mpf_termination_factory_t *rtp_termination_factory, const char *name);

/**
 * Register RTP settings.
 * @param client the MRCP client to set RTP settings for
 * @param rtp_settings the settings to set
 * @param name the name of the settings
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_rtp_settings_register(mrcp_client_t *client, mpf_rtp_settings_t *rtp_settings, const char *name);

/**
 * Register MRCP signaling agent.
 * @param client the MRCP client to set signaling agent for
 * @param signaling_agent the signaling agent to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_signaling_agent_register(mrcp_client_t *client, mrcp_sig_agent_t *signaling_agent);

/**
 * Register MRCP signaling settings.
 * @param client the MRCP client to set signaling settings for
 * @param signaling_settings the signaling settings to set
 * @param name the name of the agent
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_signaling_settings_register(mrcp_client_t *client, mrcp_sig_settings_t *signaling_settings, const char *name);

/**
 * Register MRCP connection agent (MRCPv2 only).
 * @param client the MRCP client to set connection agent for
 * @param connection_agent the connection agent to set
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_register(mrcp_client_t *client, mrcp_connection_agent_t *connection_agent);

/** Create MRCP profile */
MRCP_DECLARE(mrcp_client_profile_t*) mrcp_client_profile_create(
										mrcp_resource_factory_t *resource_factory,
										mrcp_sig_agent_t *signaling_agent,
										mrcp_connection_agent_t *connection_agent,
										mpf_engine_t *media_engine,
										mpf_termination_factory_t *rtp_factory,
										mpf_rtp_settings_t *rtp_settings,
										mrcp_sig_settings_t *signaling_settings,
										apr_pool_t *pool);

/** Create MRCP profile (extended version) */
MRCP_DECLARE(mrcp_client_profile_t*) mrcp_client_profile_create_ex(
										mrcp_version_e mrcp_version,
										mrcp_resource_factory_t *resource_factory,
										mrcp_sa_factory_t *sa_factory,
										mrcp_ca_factory_t *ca_factory,
										mpf_engine_factory_t *mpf_factory,
										mpf_termination_factory_t *rtp_factory,
										mpf_rtp_settings_t *rtp_settings,
										mrcp_sig_settings_t *signaling_settings,
										apr_pool_t *pool);

/**
 * Set a tag to the profile.
 * @param profile the profile to set a tag for
 * @param tag the tag to set
 */
MRCP_DECLARE(void) mrcp_client_profile_tag_set(mrcp_client_profile_t *profile, const char *tag);

/**
 * Register MRCP profile.
 * @param client the MRCP client to set profile for
 * @param profile the profile to set
 * @param name the name of the profile
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_profile_register(mrcp_client_t *client, mrcp_client_profile_t *profile, const char *name);

/**
 * Register MRCP application.
 * @param client the MRCP client to set application for
 * @param application the application to set
 * @param name the name of the application
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_application_register(mrcp_client_t *client, mrcp_application_t *application, const char *name);

/**
 * Get memory pool.
 * @param client the MRCP client to get memory pool from
 */
MRCP_DECLARE(apr_pool_t*) mrcp_client_memory_pool_get(const mrcp_client_t *client);

/**
 * Get media engine by name.
 * @param client the MRCP client to get media engine from
 * @param name the name of the media engine to lookup
 */
MRCP_DECLARE(mpf_engine_t*) mrcp_client_media_engine_get(const mrcp_client_t *client, const char *name);

/**
 * Get RTP termination factory by name.
 * @param client the MRCP client to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mpf_termination_factory_t*) mrcp_client_rtp_factory_get(const mrcp_client_t *client, const char *name);

/** 
 * Get RTP settings by name
 * @param client the MRCP client to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mpf_rtp_settings_t*) mrcp_client_rtp_settings_get(const mrcp_client_t *client, const char *name);

/**
 * Get signaling agent by name.
 * @param client the MRCP client to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_client_signaling_agent_get(const mrcp_client_t *client, const char *name);

/**
 * Get signaling settings by name.
 * @param client the MRCP client to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mrcp_sig_settings_t*) mrcp_client_signaling_settings_get(const mrcp_client_t *client, const char *name);

/**
 * Get connection agent by name.
 * @param client the MRCP client to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_client_connection_agent_get(const mrcp_client_t *client, const char *name);

/**
 * Get profile by name.
 * @param client the MRCP client to get from
 * @param name the name to lookup
 */
MRCP_DECLARE(mrcp_client_profile_t*) mrcp_client_profile_get(const mrcp_client_t *client, const char *name);

/**
 * Get available profiles.
 * @param client the MRCP client to get the profile from
 * @param profiles the array of profiles to be filled in
 * @param count the max number of profiles on entry; and the number of profiles filled in on exit
 * @param tag the tag to be used as a filter (no filter is specified if tag is NULL)
 * @return FALSE if the provided max number of profiles is less than the actual number of profiles
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_profiles_get(const mrcp_client_t *client, mrcp_client_profile_t *profiles[], apr_size_t *count, const char *tag);

/**
 * Get directory layout.
 * @param client the MRCP client to get from
 */
MRCP_DECLARE(apt_dir_layout_t*) mrcp_client_dir_layout_get(const mrcp_client_t *client);

APT_END_EXTERN_C

#endif /* MRCP_CLIENT_H */
