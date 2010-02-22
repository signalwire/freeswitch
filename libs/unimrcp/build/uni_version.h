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

#ifndef __UNI_VERSION_H__
#define __UNI_VERSION_H__

/**
 * @file uni_version.h
 * @brief UniMRCP Version Numbering
 * 
 * UniMRCP version numbering is derived from APR project specified in:
 *
 *     http://apr.apache.org/versioning.html
 */

#include <apr_version.h>

/** major version 
 * Major API changes that could cause compatibility problems for older
 * programs such as structure size changes.  No binary compatibility is
 * possible across a change in the major version.
 */
#define UNI_MAJOR_VERSION   0

/** minor version
 * Minor API changes that do not cause binary compatibility problems.
 * Reset to 0 when upgrading UNI_MAJOR_VERSION
 */
#define UNI_MINOR_VERSION   9

/** patch level 
 * The Patch Level never includes API changes, simply bug fixes.
 * Reset to 0 when upgrading UNI_MINOR_VERSION
 */
#define UNI_PATCH_VERSION   0


/**
 * Check at compile time if the UNI version is at least a certain
 * level.
 */
#define UNI_VERSION_AT_LEAST(major,minor,patch)                    \
(((major) < UNI_MAJOR_VERSION)                                     \
 || ((major) == UNI_MAJOR_VERSION && (minor) < UNI_MINOR_VERSION) \
 || ((major) == UNI_MAJOR_VERSION && (minor) == UNI_MINOR_VERSION && (patch) <= UNI_PATCH_VERSION))

/** The formatted string of UniMRCP's version */
#define UNI_VERSION_STRING \
     APR_STRINGIFY(UNI_MAJOR_VERSION) "." \
     APR_STRINGIFY(UNI_MINOR_VERSION) "." \
     APR_STRINGIFY(UNI_PATCH_VERSION)

#endif /* __UNI_VERSION_H__ */
