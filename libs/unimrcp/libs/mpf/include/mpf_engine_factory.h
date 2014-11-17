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
 * $Id: mpf_engine_factory.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MPF_ENGINE_FACTORY_H
#define MPF_ENGINE_FACTORY_H

/**
 * @file mpf_engine.h
 * @brief Factory of Media Processing Engines
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** Create factory of media engines. */
MPF_DECLARE(mpf_engine_factory_t*) mpf_engine_factory_create(apr_pool_t *pool);

/** Add media engine to factory. */
MPF_DECLARE(apt_bool_t) mpf_engine_factory_engine_add(mpf_engine_factory_t *mpf_factory, mpf_engine_t *media_engine);

/** Determine whether factory is empty. */
MPF_DECLARE(apt_bool_t) mpf_engine_factory_is_empty(const mpf_engine_factory_t *mpf_factory);

/** Select next available media engine. */
MPF_DECLARE(mpf_engine_t*) mpf_engine_factory_engine_select(mpf_engine_factory_t *mpf_factory);

/** Associate media engines with RTP termination factory. */
MPF_DECLARE(apt_bool_t) mpf_engine_factory_rtp_factory_assign(mpf_engine_factory_t *mpf_factory, mpf_termination_factory_t *rtp_factory);

APT_END_EXTERN_C

#endif /* MPF_ENGINE_FACTORY_H */
