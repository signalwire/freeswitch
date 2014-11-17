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
 * $Id: apt_log.h 1792 2011-01-10 21:08:52Z achaloyan $
 */

#ifndef MPF_TRACE_H
#define MPF_TRACE_H

/**
 * @file mpf_trace.h
 * @brief MPF Tracer
 */ 

#include <stdio.h>
#include "mpf.h"

APT_BEGIN_EXTERN_C

#ifdef WIN32
static void mpf_debug_output_trace(const char* format, ...)
{
	char buf[1024];
	va_list arg;
	va_start(arg, format);
	apr_vsnprintf(buf, sizeof(buf), format, arg);
	va_end(arg);

	OutputDebugStringA(buf);
}
#else
static APR_INLINE void mpf_debug_output_trace() {}
#endif

static APR_INLINE void mpf_null_trace() {}

APT_END_EXTERN_C

#endif /* MPF_TRACE_H */
