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

#ifndef DEMO_APPLICATION_H
#define DEMO_APPLICATION_H

/**
 * @file demo_application.h
 * @brief Demo MRCP Application
 */ 

#include "mrcp_application.h"

APT_BEGIN_EXTERN_C

/** Demo application declaration */
typedef struct demo_application_t demo_application_t;

/** Demo application */
struct demo_application_t {
	/** MRCP application */
	mrcp_application_t              *application;
	/** Demo framework */
	void                            *framework;

	/** Virtual run method */
	apt_bool_t (*run)(demo_application_t *application, const char *profile);
	/** Virtual app_message handler */
	apt_bool_t (*handler)(demo_application_t *application, const mrcp_app_message_t *app_message);
};


/** Create demo synthesizer application */
demo_application_t* demo_synth_application_create(apr_pool_t *pool);

/** Create demo recognizer application */
demo_application_t* demo_recog_application_create(apr_pool_t *pool);

/** Create demo bypass media application */
demo_application_t* demo_bypass_application_create(apr_pool_t *pool);

/** Create demo resource discover application */
demo_application_t* demo_discover_application_create(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* DEMO_APPLICATION_H */
