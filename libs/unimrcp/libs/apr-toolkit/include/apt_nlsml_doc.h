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

#ifndef __APT_NLSML_DOC_H__
#define __APT_NLSML_DOC_H__

/**
 * @file apt_nlsml_doc.h
 * @brief Basic NLSML Routine
 */ 

#include "apr_xml.h"
#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** Load NLSML document */
APT_DECLARE(apr_xml_doc*) nlsml_doc_load(const apt_str_t *data, apr_pool_t *pool);

/** Get the first interpretation element */
APT_DECLARE(apr_xml_elem*) nlsml_first_interpret_get(const apr_xml_doc *doc);

/** Get the next interpretation element */
APT_DECLARE(apr_xml_elem*) nlsml_next_interpret_get(const apr_xml_elem *interpret);

/** Get instance and input elements of interpretation element */
APT_DECLARE(apt_bool_t) nlsml_interpret_results_get(const apr_xml_elem *interpret, apr_xml_elem **instance, apr_xml_elem **input);

/** Get specified atrribute of input element */
APT_DECLARE(const char *) nlsml_input_attrib_get(const apr_xml_elem *input, const char *attrib, apt_bool_t recursive);


APT_END_EXTERN_C

#endif /*__APT_NLSML_DOC_H__*/
