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

#ifndef MRCP_UNIRTSP_LOGGER_H
#define MRCP_UNIRTSP_LOGGER_H

/**
 * @file mrcp_unirtsp_logger.h
 * @brief UniRTSP Logger
 */ 

#include "mrcp.h"

APT_BEGIN_EXTERN_C

/**
 * Initialize log source of the UniRTSP library.
 */
MRCP_DECLARE(void) mrcp_unirtsp_logsource_init();

APT_END_EXTERN_C

#endif /* MRCP_UNIRTSP_LOGGER_H */
