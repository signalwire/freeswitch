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

#ifndef __MRCP_H__
#define __MRCP_H__

/**
 * @file mrcp.h
 * @brief MRCP Core Definitions
 */ 

#include <apt.h>
#include <apt_dir_layout.h>

/** Library export/import defines */
#ifdef WIN32
#ifdef MRCP_STATIC_LIB
#define MRCP_DECLARE(type)   type __stdcall
#else
#ifdef MRCP_LIB_EXPORT
#define MRCP_DECLARE(type)   __declspec(dllexport) type __stdcall
#else
#define MRCP_DECLARE(type)   __declspec(dllimport) type __stdcall
#endif
#endif
#else
#define MRCP_DECLARE(type) type
#endif

#endif /*__MRCP_H__*/
