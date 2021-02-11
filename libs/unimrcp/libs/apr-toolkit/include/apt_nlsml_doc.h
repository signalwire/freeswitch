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

#ifndef APT_NLSML_DOC_H
#define APT_NLSML_DOC_H

/**
 * @file apt_nlsml_doc.h
 * @brief NLSML Result Handling
 * @remark This is an MRCP version independent and vendor consistent implementation
 *         of the NSLML parser. The interface reflects the NLSML schema defined in 
 *         http://tools.ietf.org/html/rfc6787#section-16.1.
 */

#include <apr_xml.h>
#include "apt.h"

APT_BEGIN_EXTERN_C

/* Forward declarations */
typedef struct nlsml_result_t nlsml_result_t;
typedef struct nlsml_interpretation_t nlsml_interpretation_t;
typedef struct nlsml_enrollment_result_t nlsml_enrollment_result_t;
typedef struct nlsml_verification_result_t nlsml_verification_result_t;
typedef struct nlsml_instance_t nlsml_instance_t;
typedef struct nlsml_input_t nlsml_input_t;

/**
 * Parse NLSML result
 * @param data the data to parse
 * @param length the length of the data
 * @param pool the memory pool to use
 * @return the parsed NLSML result.
 */
APT_DECLARE(nlsml_result_t*) nlsml_result_parse(const char *data, apr_size_t length, apr_pool_t *pool);

/**
 * Trace parsed NLSML result (for debug purposes only)
 * @param result the parsed result to output
 * @param pool the memory pool to use
 */
APT_DECLARE(void) nlsml_result_trace(const nlsml_result_t *result, apr_pool_t *pool);

/*
 * Accessors of the NLSML <result> element.
 * Each <result> element may contain one or more <interpretation>, <enrollment-result>,
 * <verification-result> elements, and an optional <grammar> attribute.
 */

/**
 * Get first interpretation
 * @param result the parsed NLSML result which holds the list of interpretation elements
 */
APT_DECLARE(nlsml_interpretation_t*) nlsml_first_interpretation_get(const nlsml_result_t *result);

/**
 * Get next interpretation
 * @param result the parsed NLSML result which holds the list of interpretation elements
 * @param interpretation the current interpretation element
 */
APT_DECLARE(nlsml_interpretation_t*) nlsml_next_interpretation_get(const nlsml_result_t *result, const nlsml_interpretation_t *interpretation);

/**
 * Get first enrollment result
 * @param result the parsed NLSML result which holds the list of enrollment-result elements
 */
APT_DECLARE(nlsml_enrollment_result_t*) nlsml_first_enrollment_result_get(const nlsml_result_t *result);

/**
 * Get next enrollment result
 * @param result the parsed NLSML result which holds the list of enrollment-result elements
 * @param enrollment_result the current enrollment-result element
 */
APT_DECLARE(nlsml_enrollment_result_t*) nlsml_next_enrollment_result_get(const nlsml_result_t *result, const nlsml_enrollment_result_t *enrollment_result);

/**
 * Get first verification result
 * @param result the parsed NLSML result which holds the list of verification-result elements
 */
APT_DECLARE(nlsml_verification_result_t*) nlsml_first_verification_result_get(const nlsml_result_t *result);

/**
 * Get next verification result
 * @param result the parsed NLSML result which holds the list of verification-result elements
 * @param verification_result the current verification-result element
 */
APT_DECLARE(nlsml_verification_result_t*) nlsml_next_verification_result_get(const nlsml_result_t *result, const nlsml_verification_result_t *verification_result);

/**
 * Get the grammar attribute of the NLSML result
 * @param result the parsed result
 */
APT_DECLARE(const char*) nlsml_result_grammar_get(const nlsml_result_t *result);

/*
 * Accessors of the <interpretation> element.
 */

/**
 * Get first instance
 * @param interpretation the parsed interpretation element which holds the list of instance elements
 */
APT_DECLARE(nlsml_instance_t*) nlsml_interpretation_first_instance_get(const nlsml_interpretation_t *interpretation);

/**
 * Get next instance
 * @param interpretation the parsed interpretation element which holds the list of instance elements
 * @param instance the current instance element
 */
APT_DECLARE(nlsml_instance_t*) nlsml_interpretation_next_instance_get(const nlsml_interpretation_t *interpretation, const nlsml_instance_t *instance);

/**
 * Get input
 * @param interpretation the parsed interpretation element which may have 0 or 1 input elements
 */
APT_DECLARE(nlsml_input_t*) nlsml_interpretation_input_get(const nlsml_interpretation_t *interpretation);

/**
 * Get interpretation confidence
 * @param interpretation the parsed interpretation element
 * @remark the confidence is stored and returned as a float value for both MRCPv2 and MRCPv1
 */
APT_DECLARE(float) nlsml_interpretation_confidence_get(const nlsml_interpretation_t *interpretation);

/**
 * Get interpretation grammar
 * @param interpretation the parsed interpretation element
 */
APT_DECLARE(const char*) nlsml_interpretation_grammar_get(const nlsml_interpretation_t *interpretation);

/*
 * Accessors of the <instance> and <input> elements.
 */

/**
 * Get an XML representation of the instance element
 * @param instance the parsed instance element
 */
APT_DECLARE(const apr_xml_elem*) nlsml_instance_elem_get(const nlsml_instance_t *instance);

/**
 * Suppress SWI elements (normalize instance)
 * @param instance the parsed instance to suppress SWI sub-elements from
 */
APT_DECLARE(apt_bool_t) nlsml_instance_swi_suppress(nlsml_instance_t *instance);

/**
 * Generate a plain text content of the instance element
 * @param instance the parsed instance to generate content of
 * @param pool the memory pool to use
 */
APT_DECLARE(const char*) nlsml_instance_content_generate(const nlsml_instance_t *instance, apr_pool_t *pool);

/**
 * Get an XML representation of the input element
 * @param input the parsed input element
 */
APT_DECLARE(const apr_xml_elem*) nlsml_input_elem_get(const nlsml_input_t *input);

/**
 * Generate a plain text content of the input element
 * @param input the parsed input to generate content of
 * @param pool the memory pool to use
 */
APT_DECLARE(const char*) nlsml_input_content_generate(const nlsml_input_t *input, apr_pool_t *pool);

/**
 * Get input mode
 * @param input the parsed input element
 * @remark the input mode is either "speech" or "dtmf"
 */
APT_DECLARE(const char*) nlsml_input_mode_get(const nlsml_input_t *input);

/**
 * Get input confidence
 * @param input the parsed input element
 * @remark the confidence is stored and returned as a float value for both MRCPv2 and MRCPv1
 */
APT_DECLARE(float) nlsml_input_confidence_get(const nlsml_input_t *input);

/**
 * Get start of input timestamp
 * @param input the parsed input element
 */
APT_DECLARE(const char*) nlsml_input_timestamp_start_get(const nlsml_input_t *input);

/**
 * Get end of input timestamp
 * @param input the parsed input element
 */
APT_DECLARE(const char*) nlsml_input_timestamp_end_get(const nlsml_input_t *input);

APT_END_EXTERN_C

#endif /* APT_NLSML_DOC_H */
