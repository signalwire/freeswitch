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

#ifndef MRCP_SOFIASIP_LOGGER_H
#define MRCP_SOFIASIP_LOGGER_H

/**
 * @file mrcp_sofiasip_logger.h
 * @brief Sofia-SIP Logger
 */ 

#include "apt_log.h"
#include "mrcp.h"

APT_BEGIN_EXTERN_C

/** Sofia-SIP log source */
APT_LOG_SOURCE_DECLARE(MRCP, sip_log_source)

/** Sofia-SIP log mark providing log source, file and line information */
#define SIP_LOG_MARK   APT_LOG_MARK_DECLARE(sip_log_source)

/**
 * Initialize Sofia-SIP log source.
 */
MRCP_DECLARE(void) mrcp_sofiasip_logsource_init();

/**
 * Initialize logger of the Sofia-SIP library.
 */
MRCP_DECLARE(apt_bool_t) mrcp_sofiasip_log_init(const char *name, const char *level_str, apt_bool_t redirect);

APT_END_EXTERN_C

#endif /* MRCP_SOFIASIP_LOGGER_H */
