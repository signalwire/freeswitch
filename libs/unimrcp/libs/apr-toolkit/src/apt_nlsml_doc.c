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

#include "apt_nlsml_doc.h"

/** Load NLSML document */
APT_DECLARE(apr_xml_doc*) nlsml_doc_load(const apt_str_t *data, apr_pool_t *pool)
{
	apr_xml_parser *parser;
	apr_xml_doc *doc = NULL;
	const apr_xml_elem *root;

	/* create XML parser */
	parser = apr_xml_parser_create(pool);
	if(apr_xml_parser_feed(parser,data->buf,data->length) != APR_SUCCESS) {
		return NULL;
	}

	/* done with XML tree creation */
	if(apr_xml_parser_done(parser,&doc) != APR_SUCCESS) {
		return NULL;
	}

	if(!doc || !doc->root) {
		return NULL;
	}
	root = doc->root;

	/* NLSML validity check: root element must be <result> */
	if(strcmp(root->name,"result") != 0) {
		return NULL;
	}

	return doc;
}

/** Get the first <interpretation> element */
APT_DECLARE(apr_xml_elem*) nlsml_first_interpret_get(const apr_xml_doc *doc)
{
	apr_xml_elem *child_elem;
	for(child_elem = doc->root->first_child; child_elem; child_elem = child_elem->next) {
		if(strcmp(child_elem->name,"interpretation") == 0) {
			return child_elem;
		}
	}

	return NULL;
}

/** Get the next <interpretation> element */
APT_DECLARE(apr_xml_elem*) nlsml_next_interpret_get(const apr_xml_elem *elem)
{
	apr_xml_elem *child_elem;
	for(child_elem = elem->next; child_elem; child_elem = child_elem->next) {
		if(strcmp(child_elem->name,"interpretation") == 0) {
			return child_elem;
		}
	}

	return NULL;
}

/** Get <instance> and <input> elements of <interpretation> element */
APT_DECLARE(apt_bool_t) nlsml_interpret_results_get(const apr_xml_elem *interpret, apr_xml_elem **instance, apr_xml_elem **input)
{
	apr_xml_elem *child_elem;
	*input = NULL;
	*instance = NULL;
	for(child_elem = interpret->first_child; child_elem; child_elem = child_elem->next) {
		if(strcmp(child_elem->name,"input") == 0) {
			*input = child_elem;
		}
		else if(strcmp(child_elem->name,"instance") == 0) {
			*instance = child_elem;
		}
	}
	return TRUE;
}

/** Get specified atrribute of <input> */
APT_DECLARE(const char *) nlsml_input_attrib_get(const apr_xml_elem *input, const char *attrib, apt_bool_t recursive)
{
	const apr_xml_attr *xml_attr;
	for(xml_attr = input->attr; xml_attr; xml_attr = xml_attr->next) {
		if(strcasecmp(xml_attr->name,attrib) == 0) {
			return xml_attr->value;
		}
	}

	if(recursive && input->parent) {
		return nlsml_input_attrib_get(input->parent,attrib,recursive);
	}

	return NULL;
}
