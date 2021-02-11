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

#ifndef MPF_OBJECT_H
#define MPF_OBJECT_H

/**
 * @file mpf_object.h
 * @brief Media Processing Object Base (bridge, multiplexor, mixer, ...)
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** MPF object declaration */
typedef struct mpf_object_t mpf_object_t;

/** Media processing objects base */
struct mpf_object_t {
	/** Informative name used for debugging */
	const char *name;
	/** Virtual destroy */
	apt_bool_t (*destroy)(mpf_object_t *object);
	/** Virtual process */
	apt_bool_t (*process)(mpf_object_t *object);
	/** Virtual trace of media path */
	void (*trace)(mpf_object_t *object);
};

/** Initialize object */
static APR_INLINE void mpf_object_init(mpf_object_t *object, const char *name)
{
	object->name = name;
	object->destroy = NULL;
	object->process = NULL;
	object->trace = NULL;
}

/** Destroy object */
static APR_INLINE void mpf_object_destroy(mpf_object_t *object)
{
	if(object->destroy)
		object->destroy(object);
}

/** Process object */
static APR_INLINE void mpf_object_process(mpf_object_t *object)
{
	if(object->process)
		object->process(object);
}

/** Trace media path */
static APR_INLINE void mpf_object_trace(mpf_object_t *object)
{
	if(object->trace)
		object->trace(object);
}


APT_END_EXTERN_C

#endif /* MPF_OBJECT_H */
