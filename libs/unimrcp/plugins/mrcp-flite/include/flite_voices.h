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

#ifndef __FLITE_VOICES_H__
#define __FLITE_VOICES_H__

/**
 * @file flite_voices.h
 * @brief Flite Voices
 */ 

#include <flite.h>
#include <apr_hash.h>
#include "mrcp_message.h"

APT_BEGIN_EXTERN_C


/** Opaque Flite voice declaration */
typedef struct flite_voices_t flite_voices_t;

/** Load Flite voices */
flite_voices_t* flite_voices_load(apr_pool_t *pool);
/** Unload Flite voices */
void flite_voices_unload(flite_voices_t *voices);

/** Get best matched voice */
cst_voice* flite_voices_best_match_get(flite_voices_t *voices, mrcp_message_t *message);

APT_END_EXTERN_C

#endif /*__FLITE_VOICES_H__*/
