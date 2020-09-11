/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2018, Grasshopper
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * nlsml.c -- Parses / creates NLSML results
 *
 */
#include <switch.h>
#include <iksemel.h>

#include "nlsml.h"
#include "iks_helpers.h"

struct nlsml_parser;

/** function to handle tag attributes */
typedef int (* tag_attribs_fn)(struct nlsml_parser *, char **);
/** function to handle tag CDATA */
typedef int (* tag_cdata_fn)(struct nlsml_parser *, char *, size_t);

/**
 * Tag definition
 */
struct tag_def {
	tag_attribs_fn attribs_fn;
	tag_cdata_fn cdata_fn;
	switch_bool_t is_root;
	switch_hash_t *children_tags;
};

/**
 * library configuration
 */
typedef struct {
	/** true if initialized */
	switch_bool_t init;
	/** Mapping of tag name to definition */
	switch_hash_t *tag_defs;
	/** library memory pool */
	switch_memory_pool_t *pool;
} nlsml_globals;
static nlsml_globals globals = { 0 };

/**
 * The node in the XML tree
 */
struct nlsml_node {
	/** tag name */
	const char *name;
	/** tag definition */
	struct tag_def *tag_def;
	/** parent to this node */
	struct nlsml_node *parent;
};

/**
 * The SAX parser state
 */
struct nlsml_parser {
	/** current node */
	struct nlsml_node *cur;
	/** optional UUID for logging */
	const char *uuid;
	/** true if a match exists */
	int match;
	/** true if noinput */
	int noinput;
	/** true if nomatch */
	int nomatch;
};

/**
 * Tag def destructor
 */
static void destroy_tag_def(void *ptr)
{
    struct tag_def *tag = (struct tag_def *) ptr;
	if (tag->children_tags) {
		switch_core_hash_destroy(&tag->children_tags);
	}
}

/**
 * Add a definition for a tag
 * @param tag the name
 * @param attribs_fn the function to handle the tag attributes
 * @param cdata_fn the function to handler the tag CDATA
 * @param children_tags comma-separated list of valid child tag names
 * @return the definition
 */
static struct tag_def *add_tag_def(const char *tag, tag_attribs_fn attribs_fn, tag_cdata_fn cdata_fn, const char *children_tags)
{
	struct tag_def *def = switch_core_alloc(globals.pool, sizeof(*def));
	switch_core_hash_init(&def->children_tags);
	if (!zstr(children_tags)) {
		char *children_tags_dup = switch_core_strdup(globals.pool, children_tags);
		char *tags[32] = { 0 };
		int tag_count = switch_separate_string(children_tags_dup, ',', tags, sizeof(tags) / sizeof(tags[0]));
		if (tag_count) {
			int i;
			for (i = 0; i < tag_count; i++) {
				switch_core_hash_insert(def->children_tags, tags[i], tags[i]);
			}
		}
	}
	def->attribs_fn = attribs_fn;
	def->cdata_fn = cdata_fn;
	def->is_root = SWITCH_FALSE;
	switch_core_hash_insert_destructor(globals.tag_defs, tag, def, destroy_tag_def);
	return def;
}

/**
 * Add a definition for a root tag
 * @param tag the name
 * @param attribs_fn the function to handle the tag attributes
 * @param cdata_fn the function to handler the tag CDATA
 * @param children_tags comma-separated list of valid child tag names
 * @return the definition
 */
static struct tag_def *add_root_tag_def(const char *tag, tag_attribs_fn attribs_fn, tag_cdata_fn cdata_fn, const char *children_tags)
{
	struct tag_def *def = add_tag_def(tag, attribs_fn, cdata_fn, children_tags);
	def->is_root = SWITCH_TRUE;
	return def;
}

/**
 * Handle tag attributes
 * @param parser the parser
 * @param name the tag name
 * @param atts the attributes
 * @return IKS_OK if OK IKS_BADXML on parse failure
 */
static int process_tag(struct nlsml_parser *parser, const char *name, char **atts)
{
	struct nlsml_node *cur = parser->cur;
	if (cur->tag_def->is_root && cur->parent == NULL) {
		/* no parent for ROOT tags */
		return cur->tag_def->attribs_fn(parser, atts);
	} else if (!cur->tag_def->is_root && cur->parent) {
		/* check if this child is allowed by parent node */
		struct tag_def *parent_def = cur->parent->tag_def;
		if (switch_core_hash_find(parent_def->children_tags, "ANY") ||
			switch_core_hash_find(parent_def->children_tags, name)) {
			return cur->tag_def->attribs_fn(parser, atts);
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "<%s> cannot be a child of <%s>\n", name, cur->parent->name);
		}
	} else if (cur->tag_def->is_root && cur->parent != NULL) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "<%s> must be the root element\n", name);
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "<%s> cannot be a root element\n", name);
	}
	return IKS_BADXML;
}

/**
 * Handle tag attributes that are ignored
 * @param parser the parser
 * @param atts the attributes
 * @return IKS_OK
 */
static int process_attribs_ignore(struct nlsml_parser *parser, char **atts)
{
	return IKS_OK;
}

/**
 * Handle CDATA that is ignored
 * @param parser the parser
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int process_cdata_ignore(struct nlsml_parser *parser, char *data, size_t len)
{
	return IKS_OK;
}

/**
 * Handle CDATA that is not allowed
 * @param parser the parser
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_BADXML if any printable characters
 */
static int process_cdata_bad(struct nlsml_parser *parser, char *data, size_t len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (isgraph(data[i])) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "Unexpected CDATA for <%s>\n", parser->cur->name);
			return IKS_BADXML;
		}
	}
	return IKS_OK;
}

/**
 * Handle CDATA with match text
 * @param parser the parser
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int process_cdata_match(struct nlsml_parser *parser, char *data, size_t len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (isgraph(data[i])) {
			parser->match++;
			return IKS_OK;
		}
	}
	return IKS_OK;
}

/**
 * Handle nomatch
 * @param parser the parser
 * @param atts the attributes
 * @return IKS_OK
 */
static int process_nomatch(struct nlsml_parser *parser, char **atts)
{
	parser->nomatch++;
	return IKS_OK;
}

/**
 * Handle noinput
 * @param parser the parser
 * @param atts the attributes
 * @return IKS_OK
 */
static int process_noinput(struct nlsml_parser *parser, char **atts)
{
	parser->noinput++;
	return IKS_OK;
}

/**
 * Process a tag
 */
static int tag_hook(void *user_data, char *name, char **atts, int type)
{
	int result = IKS_OK;
	struct nlsml_parser *parser = (struct nlsml_parser *)user_data;

	if (type == IKS_OPEN || type == IKS_SINGLE) {
		struct nlsml_node *child_node = malloc(sizeof(*child_node));
		switch_assert(child_node);
		child_node->name = name;
		child_node->tag_def = switch_core_hash_find(globals.tag_defs, name);
		if (!child_node->tag_def) {
			child_node->tag_def = switch_core_hash_find(globals.tag_defs, "ANY");
		}
		child_node->parent = parser->cur;
		parser->cur = child_node;
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_DEBUG1, "<%s>\n", name);
		result = process_tag(parser, name, atts);
	}

	if (type == IKS_CLOSE || type == IKS_SINGLE) {
		struct nlsml_node *node = parser->cur;
		parser->cur = node->parent;
		free(node);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_DEBUG1, "</%s>\n", name);
	}

	return result;
}

/**
 * Process cdata
 * @param user_data the parser
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int cdata_hook(void *user_data, char *data, size_t len)
{
	struct nlsml_parser *parser = (struct nlsml_parser *)user_data;
	if (!parser) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing parser\n");
		return IKS_BADXML;
	}
	if (parser->cur) {
		struct tag_def *def = parser->cur->tag_def;
		if (def) {
			return def->cdata_fn(parser, data, len);
		}
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "Missing definition for <%s>\n", parser->cur->name);
		return IKS_BADXML;
	}
	return IKS_OK;
}

/**
 * Parse the result, looking for noinput/nomatch/match
 * @param nlsml_result the NLSML result to parse
 * @param uuid optional UUID for logging
 * @return true if successful
 */
enum nlsml_match_type nlsml_parse(const char *nlsml_result, const char *uuid)
{
	struct nlsml_parser parser = { 0 };
	int result = NMT_BAD_XML;
	iksparser *p = NULL;
	parser.uuid = uuid;

	if (!zstr(nlsml_result)) {
		p = iks_sax_new(&parser, tag_hook, cdata_hook);
		if (iks_parse(p, nlsml_result, 0, 1) == IKS_OK) {
			/* check result */
			if (parser.match) {
				result = NMT_MATCH;
				goto end;
			}
			if (parser.nomatch) {
				result = NMT_NOMATCH;
				goto end;
			}
			if (parser.noinput) {
				result = NMT_NOINPUT;
				goto end;
			}
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser.uuid), SWITCH_LOG_INFO, "NLSML result does not have match/noinput/nomatch!\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser.uuid), SWITCH_LOG_INFO, "Failed to parse NLSML!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser.uuid), SWITCH_LOG_INFO, "Missing NLSML result\n");
	}
 end:

	while (parser.cur) {
		struct nlsml_node *node = parser.cur;
		parser.cur = node->parent;
		free(node);
	}

	if ( p ) {
		iks_parser_delete(p);
	}
	return result;
}

#define NLSML_NS "http://www.ietf.org/xml/ns/mrcpv2"

/**
 * Makes NLSML result to conform to mrcpv2
 * @param result the potentially non-conforming result
 * @return the conforming result
 */
iks *nlsml_normalize(const char *result)
{
	iks *result_xml = NULL;
	iksparser *p = iks_dom_new(&result_xml);
	if (iks_parse(p, result, 0, 1) == IKS_OK && result_xml) {
		/* for now, all that is needed is to set the proper namespace */
		iks_insert_attrib(result_xml, "xmlns", NLSML_NS);
	} else {
		/* unexpected ... */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Failed to normalize NLSML result: %s\n", result);
		if (result_xml) {
			iks_delete(result_xml);
		}
	}
	iks_parser_delete(p);
	return result_xml;
}

/**
 * @return true if digit is a DTMF
 */
static int isdtmf(const char digit)
{
	switch(digit) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '*':
	case '#':
	case 'a':
	case 'A':
	case 'b':
	case 'B':
	case 'c':
	case 'C':
	case 'd':
	case 'D':
		return 1;
	}
	return 0;
}

/**
 * Construct an NLSML result for match
 * @param match the matching digits or text
 * @param interpretation the optional digit interpretation
 * @param mode dtmf or speech
 * @param confidence 0-100
 * @return the NLSML <result>
 */
iks *nlsml_create_match(const char *match, const char *interpretation, const char *mode, int confidence)
{
	iks *result = iks_new("result");
	iks_insert_attrib(result, "xmlns", NLSML_NS);
	iks_insert_attrib(result, "xmlns:xf", "http://www.w3.org/2000/xforms");
	if (!zstr(match)) {
		iks *interpretation_node = iks_insert(result, "interpretation");
		iks *input_node = iks_insert(interpretation_node, "input");
		iks *instance_node = iks_insert(interpretation_node, "instance");
		iks_insert_attrib(input_node, "mode", mode);
		iks_insert_attrib_printf(input_node, "confidence", "%d", confidence);
		iks_insert_cdata(input_node, match, strlen(match));
		if (zstr(interpretation)) {
			iks_insert_cdata(instance_node, match, strlen(match));
		} else {
			iks_insert_cdata(instance_node, interpretation, strlen(interpretation));
		}
	}
	return result;
}

/**
 * Construct an NLSML result for match
 * @param match the matching digits or text
 * @param interpretation the optional digit interpretation
 * @return the NLSML <result>
 */
iks *nlsml_create_dtmf_match(const char *digits, const char *interpretation)
{
	iks *result = NULL;
	int first = 1;
	int i;
	int num_digits = strlen(digits);
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	for (i = 0; i < num_digits; i++) {
		if (isdtmf(digits[i])) {
			if (first) {
				stream.write_function(&stream, "%c", digits[i]);
				first = 0;
			} else {
				stream.write_function(&stream, " %c", digits[i]);
			}
		}
	}
	result = nlsml_create_match((const char *)stream.data, interpretation, "dtmf", 100);
	switch_safe_free(stream.data);
	return result;
}

/**
 * Initialize NLSML parser.  This function is not thread safe.
 */
int nlsml_init(void)
{
	if (globals.init) {
		return 1;
	}

	globals.init = SWITCH_TRUE;
	switch_core_new_memory_pool(&globals.pool);
	switch_core_hash_init(&globals.tag_defs);

	add_root_tag_def("result", process_attribs_ignore, process_cdata_ignore, "interpretation");
	add_tag_def("interpretation", process_attribs_ignore, process_cdata_ignore, "input,model,xf:model,instance,xf:instance");
	add_tag_def("input", process_attribs_ignore, process_cdata_match, "input,nomatch,noinput");
	add_tag_def("noinput", process_noinput, process_cdata_bad, "");
	add_tag_def("nomatch", process_nomatch, process_cdata_ignore, "");
	add_tag_def("model", process_attribs_ignore, process_cdata_ignore, "ANY");
	add_tag_def("xf:model", process_attribs_ignore, process_cdata_ignore, "ANY");
	add_tag_def("instance", process_attribs_ignore, process_cdata_ignore, "ANY");
	add_tag_def("xf:instance", process_attribs_ignore, process_cdata_ignore, "ANY");
	add_tag_def("ANY", process_attribs_ignore, process_cdata_ignore, "ANY");

	return 1;
}

/**
 * Destruction of NLSML parser environment
 */
void nlsml_destroy(void)
{
	if (globals.init) {
		if (globals.tag_defs) {
			switch_core_hash_destroy(&globals.tag_defs);
			globals.tag_defs = NULL;
		}
		if (globals.pool) {
			switch_core_destroy_memory_pool(&globals.pool);
			globals.pool = NULL;
		}
		globals.init = SWITCH_FALSE;
	}
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
