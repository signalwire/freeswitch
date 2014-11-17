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
 * $Id: mrcp_engine_plugin.h 2139 2014-07-07 05:06:19Z achaloyan@gmail.com $
 */

#ifndef MRCP_ENGINE_PLUGIN_H
#define MRCP_ENGINE_PLUGIN_H

/**
 * @file mrcp_engine_plugin.h
 * @brief MRCP Engine Plugin
 */ 

#include "apr_version.h"
#include "apt_log.h"
#include "mrcp_engine_types.h"

APT_BEGIN_EXTERN_C

/** Let the plugin symbols be always exported as C functions */
#ifdef __cplusplus
#define MRCP_PLUGIN_EXTERN_C extern "C"
#else
#define MRCP_PLUGIN_EXTERN_C extern
#endif

/** Plugin export defines */
#ifdef WIN32
#define MRCP_PLUGIN_DECLARE(type) MRCP_PLUGIN_EXTERN_C __declspec(dllexport) type
#else
#define MRCP_PLUGIN_DECLARE(type) MRCP_PLUGIN_EXTERN_C type
#endif

/** [REQUIRED] Symbol name of the main entry point in plugin DSO */
#define MRCP_PLUGIN_ENGINE_SYM_NAME "mrcp_plugin_create"
/** [REQUIRED] Symbol name of the vesrion number entry point in plugin DSO */
#define MRCP_PLUGIN_VERSION_SYM_NAME "mrcp_plugin_version"
/** [IMPLIED] Symbol name of the log accessor entry point in plugin DSO */
#define MRCP_PLUGIN_LOGGER_SYM_NAME "mrcp_plugin_logger_set"

/** Prototype of engine creator (entry point of plugin DSO) */
typedef mrcp_engine_t* (*mrcp_plugin_creator_f)(apr_pool_t *pool);

/** Prototype of log accessor (entry point of plugin DSO) */
typedef apt_bool_t (*mrcp_plugin_log_accessor_f)(apt_logger_t *logger);

/** Declare this macro in plugins to use log routine of the server */
#define MRCP_PLUGIN_LOGGER_IMPLEMENT \
	MRCP_PLUGIN_DECLARE(apt_bool_t) mrcp_plugin_logger_set(apt_logger_t *logger) \
		{ return apt_log_instance_set(logger); }

/** Declare this macro in plugins to set plugin version */
#define MRCP_PLUGIN_VERSION_DECLARE \
	MRCP_PLUGIN_DECLARE(mrcp_plugin_version_t) mrcp_plugin_version; \
	mrcp_plugin_version_t mrcp_plugin_version =  \
		{PLUGIN_MAJOR_VERSION, PLUGIN_MINOR_VERSION, PLUGIN_PATCH_VERSION};


/** major version 
 * Major API changes that could cause compatibility problems for older
 * plugins such as structure size changes.  No binary compatibility is
 * possible across a change in the major version.
 */
#define PLUGIN_MAJOR_VERSION   1

/** minor version
 * Minor API changes that do not cause binary compatibility problems.
 * Reset to 0 when upgrading PLUGIN_MAJOR_VERSION
 */
#define PLUGIN_MINOR_VERSION   2

/** patch level 
 * The Patch Level never includes API changes, simply bug fixes.
 * Reset to 0 when upgrading PLUGIN_MINOR_VERSION
 */
#define PLUGIN_PATCH_VERSION   0


/**
 * Check at compile time if the plugin version is at least a certain
 * level.
 */
#define PLUGIN_VERSION_AT_LEAST(major,minor,patch)                    \
(((major) < PLUGIN_MAJOR_VERSION)                                     \
 || ((major) == PLUGIN_MAJOR_VERSION && (minor) < PLUGIN_MINOR_VERSION) \
 || ((major) == PLUGIN_MAJOR_VERSION && (minor) == PLUGIN_MINOR_VERSION && (patch) <= PLUGIN_PATCH_VERSION))

/** The formatted string of plugin's version */
#define PLUGIN_VERSION_STRING \
     APR_STRINGIFY(PLUGIN_MAJOR_VERSION) "." \
     APR_STRINGIFY(PLUGIN_MINOR_VERSION) "." \
     APR_STRINGIFY(PLUGIN_PATCH_VERSION)

/** Plugin version */
typedef apr_version_t mrcp_plugin_version_t;

/** Get plugin version */
static APR_INLINE void mrcp_plugin_version_get(mrcp_plugin_version_t *version)
{
	version->major = PLUGIN_MAJOR_VERSION;
	version->minor = PLUGIN_MINOR_VERSION;
	version->patch = PLUGIN_PATCH_VERSION;
}

/** Check plugin version */
static APR_INLINE int mrcp_plugin_version_check(mrcp_plugin_version_t *version)
{
	return PLUGIN_VERSION_AT_LEAST(version->major,version->minor,version->patch);
}

APT_END_EXTERN_C

#endif /* MRCP_ENGINE_PLUGIN_H */
