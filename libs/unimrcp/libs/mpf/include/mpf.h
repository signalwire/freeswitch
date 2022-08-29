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

#ifndef MPF_H
#define MPF_H

/**
 * @file mpf.h
 * @brief Media Processing Framework Definitions
 */ 

#include <apt.h>
#include <apt_log.h>

/** lib export/import defines (win32) */
#ifdef WIN32
#ifdef MPF_STATIC_LIB
#define MPF_DECLARE(type)   type __stdcall
#else
#ifdef MPF_LIB_EXPORT
#define MPF_DECLARE(type)   __declspec(dllexport) type __stdcall
#else
#define MPF_DECLARE(type)   __declspec(dllimport) type __stdcall
#endif
#endif
#else
#define MPF_DECLARE(type) type
#endif

/** MPF log source */
APT_LOG_SOURCE_DECLARE(MPF, mpf_log_source)

/** MPF log mark providing log source, file and line information */
#define MPF_LOG_MARK   APT_LOG_MARK_DECLARE(mpf_log_source)

#endif /* MPF_H */
