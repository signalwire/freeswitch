/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * This file contains the revision base number and other relevant information.
 */

/** Revision base number. */
#define UNI_REVISION            2208

/** Revision base string. */
#define UNI_REVISION_STRING     "2208"

/** Revision base date. */
#define UNI_REVISION_DATE       "2014-10-31"

/** Revision base stamp. */
#define UNI_REVISION_STAMP      20141031L


/** Check at compile time if the revision base number is at least a certain level. */
#define UNI_REVISION_AT_LEAST(rev)   ((rev) < UNI_REVISION)

#endif /* UNI_REVISION_H */
