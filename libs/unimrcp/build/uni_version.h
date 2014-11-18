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
 * $Id: uni_version.h 2139 2014-07-07 05:06:19Z achaloyan@gmail.com $
 */

#ifndef UNI_VERSION_H
#define UNI_VERSION_H

/**
 * @file uni_version.h
 * @brief UniMRCP Version
 * 
 * UniMRCP uses a version numbering scheme derived from the APR project.
 *
 * <a href="http://apr.apache.org/versioning.html"> APR's Version Numbering </a>
 */

/** major version 
 * Major API changes that could cause compatibility problems for older
 * programs such as structure size changes.  No binary compatibility is
 * possible across a change in the major version.
 */
#define UNI_MAJOR_VERSION   1

/** minor version
 * Minor API changes that do not cause binary compatibility problems.
 * Reset to 0 when upgrading UNI_MAJOR_VERSION.
 */
#define UNI_MINOR_VERSION   2

/** patch level 
 * The Patch Level never includes API changes, simply bug fixes.
 * Reset to 0 when upgrading UNI_MINOR_VERSION.
 */
#define UNI_PATCH_VERSION   0


/** Check at compile time if the version of UniMRCP is at least a certain level. */
#define UNI_VERSION_AT_LEAST(major,minor,patch)                    \
(((major) < UNI_MAJOR_VERSION)                                     \
 || ((major) == UNI_MAJOR_VERSION && (minor) < UNI_MINOR_VERSION) \
 || ((major) == UNI_MAJOR_VERSION && (minor) == UNI_MINOR_VERSION && (patch) <= UNI_PATCH_VERSION))

/** Properly quote a value as a string in the C preprocessor. */
#define UNI_STRINGIFY(n) UNI_STRINGIFY_HELPER(n)
/** Helper macro for UNI_STRINGIFY. */
#define UNI_STRINGIFY_HELPER(n) #n

/** The formatted string of UniMRCP's version. */
#define UNI_VERSION_STRING \
     UNI_STRINGIFY(UNI_MAJOR_VERSION) "." \
     UNI_STRINGIFY(UNI_MINOR_VERSION) "." \
     UNI_STRINGIFY(UNI_PATCH_VERSION)

/** An alternative formatted string of UniMRCP's version
    macro for Win32 .rc files using numeric CSV representation. */
#define UNI_VERSION_STRING_CSV UNI_MAJOR_VERSION ##, \
                             ##UNI_MINOR_VERSION ##, \
                             ##UNI_PATCH_VERSION

/** The Copyright. */
#define UNI_COPYRIGHT "Copyright 2008-2014 Arsen Chaloyan"

/*
 * Use the brief description of the license for Win32 .rc files;
 * otherwise, use the full description.
 */
#if defined(APSTUDIO_INVOKED) || defined(RC_INVOKED)
/** The License (brief description). */
#define UNI_LICENSE "The Apache License, Version 2.0"
#else
/** The License (full description). */
#define UNI_LICENSE \
 " * Licensed under the Apache License, Version 2.0 (the ""License"");\n" \
 " * you may not use this file except in compliance with the License.\n" \
 " * You may obtain a copy of the License at\n" \
 " * \n" \
 " *     http://www.apache.org/licenses/LICENSE-2.0 \n" \
 " * \n" \
 " * Unless required by applicable law or agreed to in writing, software\n" \
 " * distributed under the License is distributed on an ""AS IS"" BASIS,\n" \
 " * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n" \
 " * See the License for the specific language governing permissions and\n" \
 " * limitations under the License.\n"
#endif /* APSTUDIO_INVOKED || RC_INVOKED */

#endif /* UNI_VERSION_H */
