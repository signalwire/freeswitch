/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DSO_H
#define DSO_H

#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_pools.h"
#include "fspr_dso.h"
#include "fspr.h"

#if APR_HAS_DSO

#ifdef HAVE_MACH_O_DYLD_H
#include <mach-o/dyld.h>
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifdef HAVE_DL_H
#include <dl.h>
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 1
#endif

#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0
#endif

#if (defined(__DragonFly__) ||\
     defined(__FreeBSD__) ||\
     defined(__OpenBSD__) ||\
     defined(__NetBSD__)     ) && !defined(__ELF__)
#define DLSYM_NEEDS_UNDERSCORE
#endif

struct fspr_dso_handle_t {
    fspr_pool_t    *pool;
    void          *handle;
    const char    *errormsg;
};

#endif

#endif
