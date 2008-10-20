/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
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

/* 
 * Note: This is a Windows specific version of apu.h. It is renamed to
 * apu.h at the start of a Windows build.
 */
/* @file apu.h
 * @brief APR-Utility main file
 */

#ifdef WIN32
#ifndef APU_H
#define APU_H
/**
 * @defgroup APR_Util APR Utility Functions
 * @{
 */


/**
 * APU_DECLARE_EXPORT is defined when building the APR-UTIL dynamic library,
 * so that all public symbols are exported.
 *
 * APU_DECLARE_STATIC is defined when including the APR-UTIL public headers,
 * to provide static linkage when the dynamic library may be unavailable.
 *
 * APU_DECLARE_STATIC and APU_DECLARE_EXPORT are left undefined when
 * including the APR-UTIL public headers, to import and link the symbols from 
 * the dynamic APR-UTIL library and assure appropriate indirection and calling
 * conventions at compile time.
 */

#if defined(DOXYGEN) || !defined(WIN32)
/**
 * The public APR-UTIL functions are declared with APU_DECLARE(), so they may
 * use the most appropriate calling convention.  Public APR functions with 
 * variable arguments must use APU_DECLARE_NONSTD().
 *
 * @deffunc APU_DECLARE(rettype) apr_func(args);
 */
#define APU_DECLARE(type)            type
/**
 * The public APR-UTIL functions using variable arguments are declared with 
 * APU_DECLARE_NONSTD(), as they must use the C language calling convention.
 *
 * @deffunc APU_DECLARE_NONSTD(rettype) apr_func(args, ...);
 */
#define APU_DECLARE_NONSTD(type)     type
/**
 * The public APR-UTIL variables are declared with APU_DECLARE_DATA.
 * This assures the appropriate indirection is invoked at compile time.
 *
 * @deffunc APU_DECLARE_DATA type apr_variable;
 * @tip extern APU_DECLARE_DATA type apr_variable; syntax is required for
 * declarations within headers to properly import the variable.
 */
#define APU_DECLARE_DATA
#elif defined(APU_DECLARE_STATIC)
#define APU_DECLARE(type)            type __stdcall
#define APU_DECLARE_NONSTD(type)     type __cdecl
#define APU_DECLARE_DATA
#elif defined(APU_DECLARE_EXPORT)
#define APU_DECLARE(type)            __declspec(dllexport) type __stdcall
#define APU_DECLARE_NONSTD(type)     __declspec(dllexport) type __cdecl
#define APU_DECLARE_DATA             __declspec(dllexport)
#else
#define APU_DECLARE(type)            __declspec(dllimport) type __stdcall
#define APU_DECLARE_NONSTD(type)     __declspec(dllimport) type __cdecl
#define APU_DECLARE_DATA             __declspec(dllimport)
#endif
/** @} */
/*
 * we always have SDBM (it's in our codebase)
 */
#define APU_HAVE_SDBM   1
#define APU_HAVE_GDBM   0

/* Allow external override */
#if !defined(APU_HAVE_DB)
#define APU_HAVE_DB     0
#endif


#define APU_HAVE_APR_ICONV     0
#define APU_HAVE_ICONV         0
#define APR_HAS_XLATE          (APU_HAVE_APR_ICONV || APU_HAVE_ICONV)

#if !defined(APU_HAVE_PGSQL)
#define APU_HAVE_PGSQL      0
#endif
#if !defined(APU_HAVE_SQLITE2)
#define APU_HAVE_SQLITE2    0
#endif
#if !defined(APU_HAVE_SQLITE3)
#define APU_HAVE_SQLITE3    0
#endif

#endif /* APU_H */
#endif /* WIN32 */
