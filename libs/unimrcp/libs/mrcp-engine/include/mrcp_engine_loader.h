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

#ifndef __MRCP_ENGINE_LOADER_H__
#define __MRCP_ENGINE_LOADER_H__

/**
 * @file mrcp_engine_loader.h
 * @brief Loader of plugins for MRCP engines
 */ 

#include "mrcp_engine_iface.h"

APT_BEGIN_EXTERN_C

/** Opaque engine loader declaration */
typedef struct mrcp_engine_loader_t mrcp_engine_loader_t;

/** Create engine loader */
MRCP_DECLARE(mrcp_engine_loader_t*) mrcp_engine_loader_create(apr_pool_t *pool);

/** Destroy engine loader */
MRCP_DECLARE(apt_bool_t) mrcp_engine_loader_destroy(mrcp_engine_loader_t *loader);

/** Unload loaded plugins */
MRCP_DECLARE(apt_bool_t) mrcp_engine_loader_plugins_unload(mrcp_engine_loader_t *loader);


/** Load engine plugin */
MRCP_DECLARE(mrcp_engine_t*) mrcp_engine_loader_plugin_load(mrcp_engine_loader_t *loader, const char *path, const char *name);


APT_END_EXTERN_C

#endif /*__MRCP_ENGINE_LOADER_H__*/
