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

#ifndef __DEMO_FRAMEWORK_H__
#define __DEMO_FRAMEWORK_H__

/**
 * @file demo_framework.h
 * @brief Demo MRCP Application Framework
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Opaque demo framework declaration */
typedef struct demo_framework_t demo_framework_t;

/**
 * Create demo framework.
 */
demo_framework_t* demo_framework_create(apt_dir_layout_t *dir_layout);

/**
 * Run demo application.
 * @param framework the framework to run application for
 * @param app_name the name of the application to run
 * @param profile_name the name of the profile to use
 */
apt_bool_t demo_framework_app_run(demo_framework_t *framework, const char *app_name, const char *profile_name);

/**
 * Destroy demo framework.
 * @param framework the framework to destroy
 */
apt_bool_t demo_framework_destroy(demo_framework_t *framework);

APT_END_EXTERN_C

#endif /*__DEMO_FRAMEWORK_H__*/
