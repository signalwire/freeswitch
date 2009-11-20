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

#ifndef __MPF_FILE_TERMINATION_FACTORY_H__
#define __MPF_FILE_TERMINATION_FACTORY_H__

/**
 * @file mpf_file_termination_factory.h
 * @brief MPF File Termination Factory
 */ 

#include "mpf_termination_factory.h"

APT_BEGIN_EXTERN_C

/**
 * Create file termination factory.
 */
MPF_DECLARE(mpf_termination_factory_t*) mpf_file_termination_factory_create(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__MPF_RTP_TERMINATION_FACTORY_H__*/
