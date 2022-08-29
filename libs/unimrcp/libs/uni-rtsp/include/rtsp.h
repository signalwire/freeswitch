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

#ifndef RTSP_H
#define RTSP_H

/**
 * @file rtsp.h
 * @brief RTSP Core Definitions
 */ 

#include <apt.h>
#include <apr_network_io.h>
#include <apt_log.h>

/** Library export/import defines */
#ifdef WIN32
#ifdef RTSP_STATIC_LIB
#define RTSP_DECLARE(type)   type __stdcall
#else
#ifdef RTSP_LIB_EXPORT
#define RTSP_DECLARE(type)   __declspec(dllexport) type __stdcall
#else
#define RTSP_DECLARE(type)   __declspec(dllimport) type __stdcall
#endif
#endif
#else
#define RTSP_DECLARE(type) type
#endif

/** RTSP log source */
APT_LOG_SOURCE_DECLARE(RTSP,rtsp_log_source)

/** RTSP log mark providing log source, file and line information */
#define RTSP_LOG_MARK   APT_LOG_MARK_DECLARE(rtsp_log_source)

#endif /* RTSP_H */
