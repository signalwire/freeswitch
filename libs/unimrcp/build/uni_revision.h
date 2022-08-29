/*
 * Copyright 2008-2020 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0 
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UNI_REVISION_H
#define UNI_REVISION_H

/**
 * @file uni_revision.h
 * @brief UniMRCP Revision
 *
 * This file contains the revision number and other relevant information.
 * The revision indicates the number of commits since the last release, 
 * and is supposed to be set by the maintainer on certain milestones between 
 * releases. The revision number is also reset to 0 on every new release.
 */

#include "uni_version.h"

/** Revision (number of commits since last release). */
#define UNI_REVISION            0

/** Revision string. */
#define UNI_REVISION_STRING     "0"

/** Revision date. */
#define UNI_REVISION_DATE       "2020-03-30"

/** Revision stamp. */
#define UNI_REVISION_STAMP      20200330L

/** Check at compile time if the revision number is at least a certain level. */
#define UNI_REVISION_AT_LEAST(rev)   ((rev) < UNI_REVISION)

/** Check at compile time if the full version of UniMRCP is at least a certain level. */
#define UNI_FULL_VERSION_AT_LEAST(major,minor,patch,rev)          \
(((major) < UNI_MAJOR_VERSION)                                    \
 || ((major) == UNI_MAJOR_VERSION && (minor) < UNI_MINOR_VERSION) \
 || ((major) == UNI_MAJOR_VERSION && (minor) == UNI_MINOR_VERSION && (patch) < UNI_PATCH_VERSION) \
 || ((major) == UNI_MAJOR_VERSION && (minor) == UNI_MINOR_VERSION && (patch) == UNI_PATCH_VERSION && (rev) <= UNI_REVISION))

/** 
 * The formatted string of UniMRCP's full version. 
 * For example:
 *    release version string: 1.3.0
 *    development version string: 1.3.0-r33
 */
#define UNI_FULL_VERSION_STRING \
	(UNI_REVISION > 0) ? UNI_VERSION_STRING "-r" UNI_REVISION_STRING : UNI_VERSION_STRING

#endif /* UNI_REVISION_H */
