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
 * $Id: rtsp.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef RTSP_H
#define RTSP_H

/**
 * @file rtsp.h
 * @brief RTSP Core Definitions
 */ 

#include <apt.h>
#include <apr_network_io.h>

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

#endif /* RTSP_H */
