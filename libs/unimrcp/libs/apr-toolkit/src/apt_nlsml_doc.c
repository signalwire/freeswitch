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

#include <stdlib.h>
#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h>

#include "apt_nlsml_doc.h"
#include "apt_log.h"

/** NLSML result */
struct nlsml_result_t
{
	/** List of interpretations */
	APR_RING_HEAD(apt_ir_head_t, nlsml_interpretation_t) interpretations;
	/** List of enrollment results */
	APR_RING_HEAD(apt_er_head_t, nlsml_enrollment_result_t) enrollment_results;
	/** List of verification results */
	APR_RING_HEAD(apt_vr_head_t, nlsml_verification_result_t) verification_results;

	/** Optional grammar attribute */
	const char *grammar;
};

/** NLSML instance */
struct nlsml_instance_t
{
	/** Ring entry */
	APR_RING_ENTRY(nlsml_instance_t) link;

	/** Instance element */
	apr_xml_elem *elem;
};

/** NLSML input */
struct nlsml_input_t
{
	/** Input element */
	apr_xml_elem *elem;
	/** Input mode attribute [default: "speech"] */
	const char *mode;
	/** Confidence attribute [default: 1.0] */
	float       confidence;
	/** Timestamp-start attribute */
	const char *timestamp_start;
	/** Timestamp-end attribute */
	const char *timestamp_end;
};

/** NLSML interpretation */
struct nlsml_interpretation_t
{
	/** Ring entry */
	APR_RING_ENTRY(nlsml_interpretation_t) link;

	/** List of instances */
	APR_RING_HEAD(apt_head_t, nlsml_instance_t) instances;
	/** Input [0..1] */
	nlsml_input_t *input;

	/** Confidence attribute [default: 1.0] */
	float       confidence;
	/** Optional grammar attribute */
	const char *grammar;
};

struct nlsml_enrollment_result_t
{
	/** Ring entry */
	APR_RING_ENTRY(nlsml_enrollment_result_t) link;
};

struct nlsml_verification_result_t
{
	/** Ring entry */
	APR_RING_ENTRY(nlsml_verification_result_t) link;
};

/** Load NLSML document */
static apr_xml_doc* nlsml_doc_load(const char *data, apr_size_t length, apr_pool_t *pool)
{
	apr_xml_parser *parser;
	apr_xml_doc *doc = NULL;
	const apr_xml_elem *root;

	if(!data || !length) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No NLSML data available");
		return NULL;
	}

	/* create XML parser */
	parser = apr_xml_parser_create(pool);
	if(apr_xml_parser_feed(parser,data,length) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to feed NLSML input to the parser");
		return NULL;
	}

	/* done with XML tree creation */
	if(apr_xml_parser_done(parser,&doc) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to terminate NLSML parsing");
		return NULL;
	}

	if(!doc || !doc->root) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No NLSML root element");
		return NULL;
	}
	root = doc->root;

	/* NLSML validity check: root element must be <result> */
	if(strcmp(root->name,"result") != 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unexpected NLSML root element <%s>",root->name);
		return NULL;
	}

	return doc;
}

/** Parse confidence value */
static float nlsml_confidence_parse(const char *str)
{
	float confidence = (float) atof(str);
	if(confidence > 1.0)
		confidence /= 100;

	return confidence;
}

/** Parse <instance> element */
static nlsml_instance_t* nlsml_instance_parse(apr_xml_elem *elem, apr_pool_t *pool)
{
	/* Initialize instance */
	nlsml_instance_t *instance = apr_palloc(pool, sizeof(*instance));
	APR_RING_ELEM_INIT(instance,link);
	instance->elem = elem;

	return instance;
}

/** Parse <input> element */
static nlsml_input_t* nlsml_input_parse(apr_xml_elem *elem, apr_pool_t *pool)
{
	const apr_xml_attr *xml_attr;
	/* Initialize input */
	nlsml_input_t *input = apr_palloc(pool, sizeof(*input));
	input->elem = elem;
	input->mode = "speech";
	input->confidence = 1.0;
	input->timestamp_start = NULL;
	input->timestamp_end = NULL;

	/* Find input attributes */
	for(xml_attr = elem->attr; xml_attr; xml_attr = xml_attr->next) {
		if(strcasecmp(xml_attr->name, "mode") == 0) {
			input->mode = xml_attr->value;
		}
		else if(strcasecmp(xml_attr->name, "confidence") == 0) {
			input->confidence = nlsml_confidence_parse(xml_attr->value);
		}
		else if(strcasecmp(xml_attr->name, "timestamp-start") == 0) {
			input->timestamp_start = xml_attr->value;
		}
		else if(strcasecmp(xml_attr->name, "timestamp-end") == 0) {
			input->timestamp_end = xml_attr->value;
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown attribute '%s' for <%s>", xml_attr->name, elem->name);
		}
	}

	return input;
}

/** Parse <interpretation> element */
static nlsml_interpretation_t* nlsml_interpretation_parse(apr_xml_elem *elem, apr_pool_t *pool)
{
	apr_xml_elem *child_elem;
	const apr_xml_attr *xml_attr;
	nlsml_instance_t *instance;
	nlsml_input_t *input;

	/* Initialize interpretation */
	nlsml_interpretation_t *interpretation = apr_palloc(pool, sizeof(*interpretation));
	APR_RING_ELEM_INIT(interpretation,link);
	interpretation->grammar = NULL;
	interpretation->confidence = 1.0;
	interpretation->input = NULL;
	APR_RING_INIT(&interpretation->instances, nlsml_instance_t, link);

	/* Find optional grammar and confidence attributes */
	for(xml_attr = elem->attr; xml_attr; xml_attr = xml_attr->next) {
		if(strcasecmp(xml_attr->name, "grammar") == 0) {
			interpretation->grammar = xml_attr->value;
		}
		else if(strcasecmp(xml_attr->name, "confidence") == 0) {
			interpretation->confidence = nlsml_confidence_parse(xml_attr->value);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown attribute '%s' for <%s>", xml_attr->name, elem->name);
		}
	}

	/* Find input and instance elements */
	for(child_elem = elem->first_child; child_elem; child_elem = child_elem->next) {
		if(strcasecmp(child_elem->name, "input") == 0) {
			input = nlsml_input_parse(child_elem, pool);
			if(input) {
				interpretation->input = input;
			}
		}
		else if(strcasecmp(child_elem->name, "instance") == 0) {
			instance = nlsml_instance_parse(child_elem, pool);
			if(instance) {
				APR_RING_INSERT_TAIL(&interpretation->instances, instance, nlsml_instance_t, link);
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown child element <%s> for <%s>", child_elem->name, elem->name);
		}
	}

	return interpretation;
}

/** Parse <enrollment-result> element */
static nlsml_enrollment_result_t* nlsml_enrollment_result_parse(const apr_xml_elem *elem, apr_pool_t *pool)
{
	/* To be done */
	return NULL;
}

/** Parse <verification-result> element */
static nlsml_verification_result_t* nlsml_verification_result_parse(const apr_xml_elem *elem, apr_pool_t *pool)
{
	/* To be done */
	return NULL;
}

/** Parse NLSML result */
APT_DECLARE(nlsml_result_t*) nlsml_result_parse(const char *data, apr_size_t length, apr_pool_t *pool)
{
	nlsml_result_t *result;
	apr_xml_elem *root;
	apr_xml_elem *child_elem;
	const apr_xml_attr *xml_attr;
	nlsml_interpretation_t *interpretation;
	nlsml_enrollment_result_t *enrollment_result;
	nlsml_verification_result_t *verification_result;
	apr_xml_doc *doc;
	/* Load XML document */
	doc = nlsml_doc_load(data, length, pool);
	if(!doc)
		return NULL;

	root = doc->root;

	/* Initialize result */
	result = apr_palloc(pool, sizeof(*result));
	APR_RING_INIT(&result->interpretations, nlsml_interpretation_t, link);
	APR_RING_INIT(&result->enrollment_results, nlsml_enrollment_result_t, link);
	APR_RING_INIT(&result->verification_results, nlsml_verification_result_t, link);
	result->grammar = NULL;

	/* Find optional grammar attribute */
	for(xml_attr = root->attr; xml_attr; xml_attr = xml_attr->next) {
		if(strcasecmp(xml_attr->name, "grammar") == 0) {
			result->grammar = xml_attr->value;
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown attribute '%s' for <%s>", xml_attr->name, root->name);
		}
	}

	/* Find interpretation, enrollment-result, or verification-result elements */
	for(child_elem = root->first_child; child_elem; child_elem = child_elem->next) {
		if(strcasecmp(child_elem->name, "interpretation") == 0) {
			interpretation = nlsml_interpretation_parse(child_elem, pool);
			if(interpretation) {
				APR_RING_INSERT_TAIL(&result->interpretations, interpretation, nlsml_interpretation_t, link);
			}
		}
		else if(strcasecmp(child_elem->name, "enrollment-result") == 0) {
			enrollment_result = nlsml_enrollment_result_parse(child_elem, pool);
			if(enrollment_result) {
				APR_RING_INSERT_TAIL(&result->enrollment_results, enrollment_result, nlsml_enrollment_result_t, link);
			}
		}
		else if(strcasecmp(child_elem->name, "verification-result") == 0) {
			verification_result = nlsml_verification_result_parse(child_elem, pool);
			if(verification_result) {
				APR_RING_INSERT_TAIL(&result->verification_results, verification_result, nlsml_verification_result_t, link);
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown child element <%s> for <%s>", child_elem->name, root->name);
		}
	}

	if(APR_RING_EMPTY(&result->interpretations, nlsml_interpretation_t, link) && 
		APR_RING_EMPTY(&result->enrollment_results, nlsml_enrollment_result_t, link) &&
			APR_RING_EMPTY(&result->verification_results, nlsml_verification_result_t, link)) {
		/* at least one of <interpretation>, <enrollment-result>, <verification-result> MUST be specified */
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid NLSML document: at least one child element MUST be specified for <%s>", root->name);
	}

	return result;
}

/** Trace NLSML result (for debug purposes only) */
APT_DECLARE(void) nlsml_result_trace(const nlsml_result_t *result, apr_pool_t *pool)
{
	int interpretation_count;
	nlsml_interpretation_t *interpretation;
	int instance_count;
	nlsml_instance_t *instance;
	nlsml_input_t *input;
	const char *instance_data;
	const char *input_data;
	const char *timestamp_start;
	const char *timestamp_end;

	if(result->grammar)
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Result.grammar: %s", result->grammar);

	interpretation_count = 0;
	interpretation = nlsml_first_interpretation_get(result);
	while(interpretation) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].confidence: %.2f", interpretation_count, nlsml_interpretation_confidence_get(interpretation));
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].grammar: %s", interpretation_count, nlsml_interpretation_grammar_get(interpretation));

		instance_count = 0;
		instance = nlsml_interpretation_first_instance_get(interpretation);
		while(instance) {
			nlsml_instance_swi_suppress(instance);
			instance_data = nlsml_instance_content_generate(instance,pool);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].instance[%d]: %s", interpretation_count, instance_count, instance_data);

			instance_count++;
			instance = nlsml_interpretation_next_instance_get(interpretation, instance);
		}

		input = nlsml_interpretation_input_get(interpretation);
		if(input) {
			input_data = nlsml_input_content_generate(input,pool);
			timestamp_start = nlsml_input_timestamp_start_get(input);
			timestamp_end = nlsml_input_timestamp_end_get(input);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].input: %s", interpretation_count, input_data);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].input.mode: %s", interpretation_count, nlsml_input_mode_get(input));
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].input.confidence: %.2f", interpretation_count, nlsml_input_confidence_get(input));
			if(timestamp_start)
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].input.timestamp-start: %s", interpretation_count, timestamp_start);
			if(timestamp_end)
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpretation[%d].input.timestamp-end: %s", interpretation_count, timestamp_end);
		}

		interpretation_count++;
		interpretation = nlsml_next_interpretation_get(result, interpretation);
	}
}

/** Get first interpretation */
APT_DECLARE(nlsml_interpretation_t*) nlsml_first_interpretation_get(const nlsml_result_t *result)
{
	nlsml_interpretation_t *first_interpretation = APR_RING_FIRST(&result->interpretations);
	if(first_interpretation == APR_RING_SENTINEL(&result->interpretations, nlsml_interpretation_t, link))
		return NULL;
	return first_interpretation;
}

/** Get next interpretation */
APT_DECLARE(nlsml_interpretation_t*) nlsml_next_interpretation_get(const nlsml_result_t *result, const nlsml_interpretation_t *interpretation)
{
	nlsml_interpretation_t *next_interpretation = APR_RING_NEXT(interpretation, link);
	if(next_interpretation == APR_RING_SENTINEL(&result->interpretations, nlsml_interpretation_t, link))
		return NULL;
	return next_interpretation;
}

/** Get first enrollment result */
APT_DECLARE(nlsml_enrollment_result_t*) nlsml_first_enrollment_result_get(const nlsml_result_t *result)
{
	nlsml_enrollment_result_t *first_enrollment_result = APR_RING_FIRST(&result->enrollment_results);
	if(first_enrollment_result == APR_RING_SENTINEL(&result->enrollment_results, nlsml_enrollment_result_t, link))
		return NULL;
	return first_enrollment_result;
}

/** Get next enrollment result */
APT_DECLARE(nlsml_enrollment_result_t*) nlsml_next_enrollment_result_get(const nlsml_result_t *result, const nlsml_enrollment_result_t *enrollment_result)
{
	nlsml_enrollment_result_t *next_enrollment_result = APR_RING_NEXT(enrollment_result, link);
	if(next_enrollment_result == APR_RING_SENTINEL(&result->enrollment_results, nlsml_enrollment_result_t, link))
		return NULL;
	return next_enrollment_result;
}

/** Get first verification result */
APT_DECLARE(nlsml_verification_result_t*) nlsml_first_verification_result_get(const nlsml_result_t *result)
{
	nlsml_verification_result_t *first_verification_result = APR_RING_FIRST(&result->verification_results);
	if(first_verification_result == APR_RING_SENTINEL(&result->verification_results, nlsml_verification_result_t, link))
		return NULL;
	return first_verification_result;
}

/** Get next verification result */
APT_DECLARE(nlsml_verification_result_t*) nlsml_next_verification_result_get(const nlsml_result_t *result, const nlsml_verification_result_t *verification_result)
{
	nlsml_verification_result_t *next_verification_result = APR_RING_NEXT(verification_result, link);
	if(next_verification_result == APR_RING_SENTINEL(&result->verification_results, nlsml_verification_result_t, link))
		return NULL;
	return next_verification_result;
}

/** Get grammar attribute of NLSML result */
APT_DECLARE(const char*) nlsml_result_grammar_get(const nlsml_result_t *result)
{
	return result->grammar;
}

/** Get first instance */
APT_DECLARE(nlsml_instance_t*) nlsml_interpretation_first_instance_get(const nlsml_interpretation_t *interpretation)
{
	nlsml_instance_t *first_instance = APR_RING_FIRST(&interpretation->instances);
	if(first_instance == APR_RING_SENTINEL(&interpretation->instances, nlsml_instance_t, link))
		return NULL;
	return first_instance;
}

/** Get next instance */
APT_DECLARE(nlsml_instance_t*) nlsml_interpretation_next_instance_get(const nlsml_interpretation_t *interpretation, const nlsml_instance_t *instance)
{
	nlsml_instance_t *next_instance = APR_RING_NEXT(instance, link);
	if(next_instance == APR_RING_SENTINEL(&interpretation->instances, nlsml_instance_t, link))
		return NULL;
	return next_instance;
}

/** Get input */
APT_DECLARE(nlsml_input_t*) nlsml_interpretation_input_get(const nlsml_interpretation_t *interpretation)
{
	return interpretation->input;
}

/** Get interpretation confidence */
APT_DECLARE(float) nlsml_interpretation_confidence_get(const nlsml_interpretation_t *interpretation)
{
	return interpretation->confidence;
}

/** Get interpretation grammar */
APT_DECLARE(const char*) nlsml_interpretation_grammar_get(const nlsml_interpretation_t *interpretation)
{
	return interpretation->grammar;
}

/** Get instance element */
APT_DECLARE(const apr_xml_elem*) nlsml_instance_elem_get(const nlsml_instance_t *instance)
{
	return instance->elem;
}

/** Suppress SWI elements (normalize instance) */
APT_DECLARE(apt_bool_t) nlsml_instance_swi_suppress(nlsml_instance_t *instance)
{
	apr_xml_elem *child_elem;
	apr_xml_elem *prev_elem = NULL;
	apr_xml_elem *swi_literal = NULL;
	apt_bool_t remove;
	if(!instance->elem)
		return FALSE;

	for(child_elem = instance->elem->first_child; child_elem; child_elem = child_elem->next) {
		remove = FALSE;
		if(strcasecmp(child_elem->name,"SWI_literal") == 0) {
			swi_literal = child_elem;
			remove = TRUE;
		}
		else if(strcasecmp(child_elem->name,"SWI_meaning") == 0) {
			remove = TRUE;
		}

		if(remove == TRUE) {
			if(child_elem == instance->elem->first_child) {
				instance->elem->first_child = child_elem->next;
			}
			else if(prev_elem) {
				prev_elem->next = child_elem->next;
			}
		}

		prev_elem = child_elem;
	}

	if(APR_XML_ELEM_IS_EMPTY(instance->elem) && swi_literal) {
		instance->elem->first_cdata = swi_literal->first_cdata;
	}

	return TRUE;
}

/** Generate a plain text content of the instance element */
APT_DECLARE(const char*) nlsml_instance_content_generate(const nlsml_instance_t *instance, apr_pool_t *pool)
{
	const char *buf = NULL;
	if(instance->elem) {
		apr_size_t size;
		apr_xml_to_text(pool, instance->elem, APR_XML_X2T_INNER, NULL, NULL, &buf, &size);
	}
	return buf;
}

/** Get input element */
APT_DECLARE(const apr_xml_elem*) nlsml_input_elem_get(const nlsml_input_t *input)
{
	return input->elem;
}

/** Generate a plain text content of the input element */
APT_DECLARE(const char*) nlsml_input_content_generate(const nlsml_input_t *input, apr_pool_t *pool)
{
	const char *buf = NULL;
	if(input->elem) {
		apr_size_t size;
		apr_xml_to_text(pool, input->elem, APR_XML_X2T_INNER, NULL, NULL, &buf, &size);
	}
	return buf;
}

/** Get input mode */
APT_DECLARE(const char*) nlsml_input_mode_get(const nlsml_input_t *input)
{
	return input->mode;
}

/** Get input confidence */
APT_DECLARE(float) nlsml_input_confidence_get(const nlsml_input_t *input)
{
	return input->confidence;
}

/** Get start of input timestamp */
APT_DECLARE(const char*) nlsml_input_timestamp_start_get(const nlsml_input_t *input)
{
	return input->timestamp_start;
}

/** Get end of input timestamp */
APT_DECLARE(const char*) nlsml_input_timestamp_end_get(const nlsml_input_t *input)
{
	return input->timestamp_end;
}
