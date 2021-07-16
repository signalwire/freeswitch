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

#ifndef MRCP_ENGINE_TYPES_H
#define MRCP_ENGINE_TYPES_H

/**
 * @file mrcp_engine_types.h
 * @brief MRCP Engine Types
 */ 

#include <apr_tables.h>
#include <apr_hash.h>
#include "mrcp_state_machine.h"
#include "mpf_types.h"
#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** MRCP engine declaration */
typedef struct mrcp_engine_t mrcp_engine_t;
/** MRCP engine config declaration */
typedef struct mrcp_engine_config_t mrcp_engine_config_t;
/** MRCP engine profile settings declaration */
typedef struct mrcp_engine_settings_t mrcp_engine_settings_t;
/** MRCP engine vtable declaration */
typedef struct mrcp_engine_method_vtable_t mrcp_engine_method_vtable_t;
/** MRCP engine event vtable declaration */
typedef struct mrcp_engine_event_vtable_t mrcp_engine_event_vtable_t;
/** MRCP engine channel declaration */
typedef struct mrcp_engine_channel_t mrcp_engine_channel_t;
/** MRCP engine channel virtual method table declaration */
typedef struct mrcp_engine_channel_method_vtable_t mrcp_engine_channel_method_vtable_t;
/** MRCP engine channel virtual event table declaration */
typedef struct mrcp_engine_channel_event_vtable_t mrcp_engine_channel_event_vtable_t;

/** Table of channel virtual methods */
struct mrcp_engine_channel_method_vtable_t {
	/** Virtual destroy */
	apt_bool_t (*destroy)(mrcp_engine_channel_t *channel);
	/** Virtual open */
	apt_bool_t (*open)(mrcp_engine_channel_t *channel);
	/** Virtual close */
	apt_bool_t (*close)(mrcp_engine_channel_t *channel);
	/** Virtual process_request */
	apt_bool_t (*process_request)(mrcp_engine_channel_t *channel, mrcp_message_t *request);
};

/** Table of channel virtual event handlers */
struct mrcp_engine_channel_event_vtable_t {
	/** Open event handler */
	apt_bool_t (*on_open)(mrcp_engine_channel_t *channel, apt_bool_t status);
	/** Close event handler */
	apt_bool_t (*on_close)(mrcp_engine_channel_t *channel);
	/** Message event handler */
	apt_bool_t (*on_message)(mrcp_engine_channel_t *channel, mrcp_message_t *message);
};

/** MRCP engine channel declaration */
struct mrcp_engine_channel_t {
	/** Table of virtual methods */
	const mrcp_engine_channel_method_vtable_t *method_vtable;
	/** External object used with virtual methods */
	void                                      *method_obj;
	/** Table of virtual event handlers */
	const mrcp_engine_channel_event_vtable_t  *event_vtable;
	/** External object used with event handlers */
	void                                      *event_obj;
	/** Media termination */
	mpf_termination_t                         *termination;
	/** Back pointer to engine */
	mrcp_engine_t                             *engine;
	/** Unique identifier to be used in traces */
	apt_str_t                                  id;
	/** MRCP version */
	mrcp_version_e                             mrcp_version;
	/** Is channel successfully opened */
	apt_bool_t                                 is_open;
	/** Pool to allocate memory from */
	apr_pool_t                                *pool;
	/** Name/value attributes */
	apr_table_t                               *attribs;
};

/** Table of MRCP engine virtual methods */
struct mrcp_engine_method_vtable_t {
	/** Virtual destroy */
	apt_bool_t (*destroy)(mrcp_engine_t *engine);
	/** Virtual open */
	apt_bool_t (*open)(mrcp_engine_t *engine);
	/** Virtual close */
	apt_bool_t (*close)(mrcp_engine_t *engine);
	/** Virtual channel create */
	mrcp_engine_channel_t* (*create_channel)(mrcp_engine_t *engine, apr_pool_t *pool);
};

/** Table of MRCP engine virtual event handlers */
struct mrcp_engine_event_vtable_t {
	/** Open event handler */
	apt_bool_t (*on_open)(mrcp_engine_t *channel, apt_bool_t status);
	/** Close event handler */
	apt_bool_t (*on_close)(mrcp_engine_t *channel);
};

/** MRCP engine */
struct mrcp_engine_t {
	/** Identifier of the engine */
	const char                        *id;
	/** Resource identifier */
	mrcp_resource_id                   resource_id;
	/** External object associated with engine */
	void                              *obj;
	/** Table of virtual methods */
	const mrcp_engine_method_vtable_t *method_vtable;
	/** Table of virtual event handlers */
	const mrcp_engine_event_vtable_t  *event_vtable;
	/** External object used with event handlers */
	void                              *event_obj;
	/** Codec manager */
	const mpf_codec_manager_t         *codec_manager;
	/** Dir layout structure */
	const apt_dir_layout_t            *dir_layout;
	/** Config of engine */
	mrcp_engine_config_t              *config;
	/** Number of simultaneous channels currently in use */
	apr_size_t                         cur_channel_count;
	/** Is engine successfully opened */
	apt_bool_t                         is_open;
	/** Pool to allocate memory from */
	apr_pool_t                        *pool;

	/** Create state machine */
	mrcp_state_machine_t* (*create_state_machine)(void *obj, mrcp_version_e version, apr_pool_t *pool);
};

/** MRCP engine config */
struct mrcp_engine_config_t {
	/** Max number of simultaneous channels */
	apr_size_t   max_channel_count;
	/** Table of name/value string params */
	apr_table_t *params;
};

/** MRCP engine profile settings */
struct mrcp_engine_settings_t {
	/** Identifier of the resource loaded from configuration */
	const char    *resource_id;
	/** Identifier of the engine loaded from configuration */
	const char    *engine_id;
	/** Table of name/value string attributes loaded from configuration */
	apr_table_t   *attribs;
	/** Associated engine */
	mrcp_engine_t *engine;
};

APT_END_EXTERN_C

#endif /* MRCP_ENGINE_TYPES_H */
