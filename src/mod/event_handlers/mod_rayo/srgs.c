/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2014, Grasshopper
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
 * srgs.c -- Parses / converts / matches SRGS grammars
 *
 */
#include <switch.h>
#include <iksemel.h>
#include <pcre.h>

#include "srgs.h"

#define MAX_RECURSION 100
#define MAX_TAGS 30

/** function to handle tag attributes */
typedef int (* tag_attribs_fn)(struct srgs_grammar *, char **);
/** function to handle tag CDATA */
typedef int (* tag_cdata_fn)(struct srgs_grammar *, char *, size_t);

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
} srgs_globals;
static srgs_globals globals = { 0 };

/**
 * SRGS node types
 */
enum srgs_node_type {
	/** anything */
	SNT_ANY,
	/** <grammar> */
	SNT_GRAMMAR,
	/** <rule> */
	SNT_RULE,
	/** <one-of> */
	SNT_ONE_OF,
	/** <item> */
	SNT_ITEM,
	/** <ruleref> unresolved reference to node */
	SNT_UNRESOLVED_REF,
	/** <ruleref> resolved reference to node */
	SNT_REF,
	/** <item> string */
	SNT_STRING,
	/** <tag> */
	SNT_TAG,
	/** <lexicon> */
	SNT_LEXICON,
	/** <example> */
	SNT_EXAMPLE,
	/** <token> */
	SNT_TOKEN,
	/** <meta> */
	SNT_META,
	/** <metadata> */
	SNT_METADATA
};

/**
 * <rule> value
 */
struct rule_value {
	char is_public;
	char *id;
	char *regex;
};

/**
 * <item> value
 */
struct item_value {
	int repeat_min;
	int repeat_max;
	const char *weight;
	int tag;
};

/**
 * <ruleref> value
 */
union ref_value {
	struct srgs_node *node;
	char *uri;
};

/**
 * A node in the SRGS parse tree
 */
struct srgs_node {
	/** Name of node */
	const char *name;
	/** Type of node */
	enum srgs_node_type type;
	/** True if node has been inspected for loops */
	char visited;
	/** Node value */
	union {
		char *root;
		const char *string;
		union ref_value ref;
		struct rule_value rule;
		struct item_value item;
	} value;
	/** parent node */
	struct srgs_node *parent;
	/** child node */
	struct srgs_node *child;
	/** sibling node */
	struct srgs_node *next;
	/** number of child nodes */
	int num_children;
	/** tag handling data */
	struct tag_def *tag_def;
};

/**
 * A parsed grammar
 */
struct srgs_grammar {
	/** grammar memory pool */
	switch_memory_pool_t *pool;
	/** current node being parsed */
	struct srgs_node *cur;
	/** rule names mapped to node */
	switch_hash_t *rules;
	/** possible matching tags */
	const char *tags[MAX_TAGS + 1];
	/** number of tags */
	int tag_count;
	/** grammar encoding */
	char *encoding;
	/** grammar language */
	char *language;
	/** true if digit grammar */
	int digit_mode;
	/** grammar parse tree root */
	struct srgs_node *root;
	/** root rule */
	struct srgs_node *root_rule;
	/** compiled grammar regex */
	pcre *compiled_regex;
	/** grammar in regex format */
	char *regex;
	/** grammar in JSGF format */
	char *jsgf;
	/** grammar as JSGF file */
	char *jsgf_file_name;
	/** synchronizes access to this grammar */
	switch_mutex_t *mutex;
	/** optional uuid for logging */
	const char *uuid;
};

/**
 * The SRGS SAX parser
 */
struct srgs_parser {
	/** parser memory pool */
	switch_memory_pool_t *pool;
	/** grammar cache */
	switch_hash_t *cache;
	/** cache mutex */
	switch_mutex_t *mutex;
	/** optional uuid for logging */
	const char *uuid;
};

/**
 * Convert entity name to node type
 * @param name of entity
 * @return the type or ANY
 */
static enum srgs_node_type string_to_node_type(char *name)
{
	if (!strcmp("grammar", name)) {
		return SNT_GRAMMAR;
	}
	if (!strcmp("item", name)) {
		return SNT_ITEM;
	}
	if (!strcmp("one-of", name)) {
		return SNT_ONE_OF;
	}
	if (!strcmp("ruleref", name)) {
		return SNT_UNRESOLVED_REF;
	}
	if (!strcmp("rule", name)) {
		return SNT_RULE;
	}
	if (!strcmp("tag", name)) {
		return SNT_TAG;
	}
	if (!strcmp("lexicon", name)) {
		return SNT_LEXICON;
	}
	if (!strcmp("example", name)) {
		return SNT_EXAMPLE;
	}
	if (!strcmp("token", name)) {
		return SNT_TOKEN;
	}
	if (!strcmp("meta", name)) {
		return SNT_META;
	}
	if (!strcmp("metadata", name)) {
		return SNT_METADATA;
	}
	return SNT_ANY;
}

/**
 * Log node
 */
static void sn_log_node_open(struct srgs_node *node)
{
	switch (node->type) {
		case SNT_ANY:
		case SNT_METADATA:
		case SNT_META:
		case SNT_TOKEN:
		case SNT_EXAMPLE:
		case SNT_LEXICON:
		case SNT_TAG:
		case SNT_ONE_OF:
		case SNT_GRAMMAR:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "<%s>\n", node->name);
			return;
		case SNT_RULE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "<rule id='%s' scope='%s'>\n", node->value.rule.id, node->value.rule.is_public ? "public" : "private");
			return;
		case SNT_ITEM:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "<item repeat='%i'>\n", node->value.item.repeat_min);
			return;
		case SNT_UNRESOLVED_REF:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "<ruleref (unresolved) uri='%s'\n", node->value.ref.uri);
			return;
		case SNT_REF:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "<ruleref uri='#%s'>\n", node->value.ref.node->value.rule.id);
			return;
		case SNT_STRING:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s\n", node->value.string);
			return;
	}
}

/**
 * Log node
 */
static void sn_log_node_close(struct srgs_node *node)
{
	switch (node->type) {
		case SNT_GRAMMAR:
		case SNT_RULE:
		case SNT_ONE_OF:
		case SNT_ITEM:
		case SNT_REF:
		case SNT_TAG:
		case SNT_LEXICON:
		case SNT_EXAMPLE:
		case SNT_TOKEN:
		case SNT_META:
		case SNT_METADATA:
		case SNT_ANY:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "</%s>\n", node->name);
			return;
		case SNT_UNRESOLVED_REF:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "</ruleref (unresolved)>\n");
			return;
		case SNT_STRING:
			return;
	}
}

/**
 * Create a new node
 * @param pool to use
 * @param name of node
 * @param type of node
 * @return the node
 */
static struct srgs_node *sn_new(switch_memory_pool_t *pool, const char *name, enum srgs_node_type type)
{
	struct srgs_node *node = switch_core_alloc(pool, sizeof(*node));
	node->name = switch_core_strdup(pool, name);
	node->type = type;
	return node;
}

/**
 * @param node to search
 * @return the last sibling of node
 */
static struct srgs_node *sn_find_last_sibling(struct srgs_node *node)
{
	if (node && node->next) {
		return sn_find_last_sibling(node->next);
	}
	return node;
}

/**
 * Add child node
 * @param pool to use
 * @param parent node to add child to
 * @param name the child node name
 * @param type the child node type
 * @return the child node
 */
static struct srgs_node *sn_insert(switch_memory_pool_t *pool, struct srgs_node *parent, const char *name, enum srgs_node_type type)
{
	struct srgs_node *sibling = parent ? sn_find_last_sibling(parent->child) : NULL;
	struct srgs_node *child = sn_new(pool, name, type);
	if (parent) {
		parent->num_children++;
		child->parent = parent;
	}
	if (sibling) {
		sibling->next = child;
	} else if (parent) {
		parent->child = child;
	}
	return child;
}

/**
 * Add string child node
 * @param pool to use
 * @param parent node to add string to
 * @param string to add - this function does not copy the string
 * @return the string child node
 */
static struct srgs_node *sn_insert_string(switch_memory_pool_t *pool, struct srgs_node *parent, char *string)
{
	struct srgs_node *child = sn_insert(pool, parent, string, SNT_STRING);
	child->value.string = string;
	return child;
}

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
static int process_tag(struct srgs_grammar *grammar, const char *name, char **atts)
{
	struct srgs_node *cur = grammar->cur;
	if (cur->tag_def->is_root && cur->parent == NULL) {
		/* no parent for ROOT tags */
		return cur->tag_def->attribs_fn(grammar, atts);
	} else if (!cur->tag_def->is_root && cur->parent) {
		/* check if this child is allowed by parent node */
		struct tag_def *parent_def = cur->parent->tag_def;
		if (switch_core_hash_find(parent_def->children_tags, "ANY") ||
			switch_core_hash_find(parent_def->children_tags, name)) {
			return cur->tag_def->attribs_fn(grammar, atts);
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<%s> cannot be a child of <%s>\n", name, cur->parent->name);
		}
	} else if (cur->tag_def->is_root && cur->parent != NULL) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<%s> must be the root element\n", name);
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<%s> cannot be a root element\n", name);
	}
	return IKS_BADXML;
}

/**
 * Handle tag attributes that are ignored
 * @param grammar the grammar
 * @param atts the attributes
 * @return IKS_OK
 */
static int process_attribs_ignore(struct srgs_grammar *grammar, char **atts)
{
	return IKS_OK;
}

/**
 * Handle CDATA that is ignored
 * @param grammar the grammar
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int process_cdata_ignore(struct srgs_grammar *grammar, char *data, size_t len)
{
	return IKS_OK;
}

/**
 * Handle CDATA that is not allowed
 * @param grammar the grammar
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_BADXML if any printable characters
 */
static int process_cdata_bad(struct srgs_grammar *grammar, char *data, size_t len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (isgraph(data[i])) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Unexpected CDATA for <%s>\n", grammar->cur->name);
			return IKS_BADXML;
		}
	}
	return IKS_OK;
}

/**
 * Process <rule> attributes
 * @param grammar the grammar state
 * @param atts the attributes
 * @return IKS_OK if ok
 */
static int process_rule(struct srgs_grammar *grammar, char **atts)
{
	struct srgs_node *rule = grammar->cur;
	rule->value.rule.is_public = 0;
	rule->value.rule.id = NULL;
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("scope", atts[i])) {
				rule->value.rule.is_public = !zstr(atts[i + 1]) && !strcmp("public", atts[i + 1]);
			} else if (!strcmp("id", atts[i])) {
				if (!zstr(atts[i + 1])) {
					rule->value.rule.id = switch_core_strdup(grammar->pool, atts[i + 1]);
				}
			}
			i += 2;
		}
	}

	if (zstr(rule->value.rule.id)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Missing rule ID: %s\n", rule->value.rule.id);
		return IKS_BADXML;
	}

	if (switch_core_hash_find(grammar->rules, rule->value.rule.id)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Duplicate rule ID: %s\n", rule->value.rule.id);
		return IKS_BADXML;
	}
	switch_core_hash_insert(grammar->rules, rule->value.rule.id, rule);

	return IKS_OK;
}

/**
 * Process <ruleref> attributes
 * @param grammar the grammar state
 * @param atts the attributes
 * @return IKS_OK if ok
 */
static int process_ruleref(struct srgs_grammar *grammar, char **atts)
{
	struct srgs_node *ruleref = grammar->cur;
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("uri", atts[i])) {
				char *uri = atts[i + 1];
				if (zstr(uri)) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Empty <ruleref> uri\n");
					return IKS_BADXML;
				}
				/* only allow local reference */
				if (uri[0] != '#' || strlen(uri) < 2) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Only local rule refs allowed\n");
					return IKS_BADXML;
				}
				ruleref->value.ref.uri = switch_core_strdup(grammar->pool, uri);
				return IKS_OK;
			}
			i += 2;
		}
	}
	return IKS_OK;
}

/**
 * Process <item> attributes
 * @param grammar the grammar state
 * @param atts the attributes
 * @return IKS_OK if ok
 */
static int process_item(struct srgs_grammar *grammar, char **atts)
{
	struct srgs_node *item = grammar->cur;
	item->value.item.repeat_min = 1;
	item->value.item.repeat_max = 1;
	item->value.item.weight = NULL;
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("repeat", atts[i])) {
				/* repeats of 0 are not supported by this code */
				char *repeat = atts[i + 1];
				if (zstr(repeat)) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Empty <item> repeat atribute\n");
					return IKS_BADXML;
				}
				if (switch_is_number(repeat)) {
					/* single number */
					int repeat_val = atoi(repeat);
					if (repeat_val < 1) {
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<item> repeat must be >= 0\n");
						return IKS_BADXML;
					}
					item->value.item.repeat_min = repeat_val;
					item->value.item.repeat_max = repeat_val;
				} else {
					/* range */
					char *min = switch_core_strdup(grammar->pool, repeat);
					char *max = strchr(min, '-');
					if (max) {
						*max = '\0';
						max++;
					} else {
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<item> repeat must be a number or range\n");
						return IKS_BADXML;
					}
					if (switch_is_number(min) && (switch_is_number(max) || zstr(max))) {
						int min_val = atoi(min);
						int max_val = zstr(max) ? INT_MAX : atoi(max);
						/* max must be >= min and > 0
						   min must be >= 0 */
						if ((max_val <= 0) || (max_val < min_val) || (min_val < 0)) {
							switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<item> repeat range invalid\n");
							return IKS_BADXML;
						}
						item->value.item.repeat_min = min_val;
						item->value.item.repeat_max = max_val;
					} else {
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<item> repeat range is not a number\n");
						return IKS_BADXML;
					}
				}
			} else if (!strcmp("weight", atts[i])) {
				const char *weight = atts[i + 1];
				if (zstr(weight) || !switch_is_number(weight) || atof(weight) < 0) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<item> weight is not a number >= 0\n");
					return IKS_BADXML;
				}
				item->value.item.weight = switch_core_strdup(grammar->pool, weight);
			}
			i += 2;
		}
	}
	return IKS_OK;
}

/**
 * Process <grammar> attributes
 * @param grammar the grammar state
 * @param atts the attributes
 * @return IKS_OK if ok
 */
static int process_grammar(struct srgs_grammar *grammar, char **atts)
{
	if (grammar->root) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Only one <grammar> tag allowed\n");
		return IKS_BADXML;
	}
	grammar->root = grammar->cur;
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("mode", atts[i])) {
				char *mode = atts[i + 1];
				if (zstr(mode)) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<grammar> mode is missing\n");
					return IKS_BADXML;
				}
				grammar->digit_mode = !strcasecmp(mode, "dtmf");
			} else if(!strcmp("encoding", atts[i])) {
				char *encoding = atts[i + 1];
				if (zstr(encoding)) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<grammar> encoding is empty\n");
					return IKS_BADXML;
				}
				grammar->encoding = switch_core_strdup(grammar->pool, encoding);
			} else if (!strcmp("language", atts[i])) {
				char *language = atts[i + 1];
				if (zstr(language)) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<grammar> language is empty\n");
					return IKS_BADXML;
				}
				grammar->language = switch_core_strdup(grammar->pool, language);
			} else if (!strcmp("root", atts[i])) {
				char *root = atts[i + 1];
				if (zstr(root)) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "<grammar> root is empty\n");
					return IKS_BADXML;
				}
				grammar->cur->value.root = switch_core_strdup(grammar->pool, root);
			}
			i += 2;
		}
	}
	return IKS_OK;
}

/**
 * Process a tag
 */
static int tag_hook(void *user_data, char *name, char **atts, int type)
{
	int result = IKS_OK;
	struct srgs_grammar *grammar = (struct srgs_grammar *)user_data;

	if (type == IKS_OPEN || type == IKS_SINGLE) {
		enum srgs_node_type ntype = string_to_node_type(name);
		grammar->cur = sn_insert(grammar->pool, grammar->cur, name, ntype);
		grammar->cur->tag_def = switch_core_hash_find(globals.tag_defs, name);
		if (!grammar->cur->tag_def) {
			grammar->cur->tag_def = switch_core_hash_find(globals.tag_defs, "ANY");
		}
		result = process_tag(grammar, name, atts);
		sn_log_node_open(grammar->cur);
	}

	if (type == IKS_CLOSE || type == IKS_SINGLE) {
		sn_log_node_close(grammar->cur);
		grammar->cur = grammar->cur->parent;
	}

	return result;
}

/**
 * Process <tag> CDATA
 * @param grammar the grammar
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int process_cdata_tag(struct srgs_grammar *grammar, char *data, size_t len)
{
	struct srgs_node *item = grammar->cur->parent;
	if (item && item->type == SNT_ITEM) {
		if (grammar->tag_count < MAX_TAGS) {
			/* grammar gets the tag name, item gets the unique tag number */
			char *tag = switch_core_alloc(grammar->pool, sizeof(char) * (len + 1));
			tag[len] = '\0';
			strncpy(tag, data, len);
			grammar->tags[++grammar->tag_count] = tag;
			item->value.item.tag = grammar->tag_count;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "too many <tag>s\n");
			return IKS_BADXML;
		}
	}
	return IKS_OK;
}

/**
 * Process CDATA grammar tokens
 * @param grammar the grammar
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int process_cdata_tokens(struct srgs_grammar *grammar, char *data, size_t len)
{
	struct srgs_node *string = grammar->cur;
	int i;
	if (grammar->digit_mode) {
		for (i = 0; i < len; i++) {
			if (isdigit(data[i]) || data[i] == '#' || data[i] == '*') {
				char *digit = switch_core_alloc(grammar->pool, sizeof(char) * 2);
				digit[0] = data[i];
				digit[1] = '\0';
				string = sn_insert_string(grammar->pool, string, digit);
				sn_log_node_open(string);
			}
		}
	} else {
		char *data_dup = switch_core_alloc(grammar->pool, sizeof(char) * (len + 1));
		char *start = data_dup;
		char *end = start + len - 1;
		memcpy(data_dup, data, len);
		/* remove start whitespace */
		for (; start && *start && !isgraph(*start); start++) {
		}
		if (!zstr(start)) {
			/* remove end whitespace */
			for (; end != start && *end && !isgraph(*end); end--) {
				*end = '\0';
			}
			if (!zstr(start)) {
				sn_insert_string(grammar->pool, string, start);
			}
		}
	}
	return IKS_OK;
}

/**
 * Process cdata
 * @param user_data the grammar
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int cdata_hook(void *user_data, char *data, size_t len)
{
	struct srgs_grammar *grammar = (struct srgs_grammar *)user_data;
	if (!grammar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing grammar\n");
		return IKS_BADXML;
	}
	if (grammar->cur) {
		if (grammar->cur->tag_def) {
			return grammar->cur->tag_def->cdata_fn(grammar, data, len);
		}
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Missing definition for <%s>\n", grammar->cur->name);
		return IKS_BADXML;
	}
	return IKS_OK;
}

/**
 * Create a new parsed grammar
 * @param parser
 * @return the grammar
 */
struct srgs_grammar *srgs_grammar_new(struct srgs_parser *parser)
{
	switch_memory_pool_t *pool = NULL;
	struct srgs_grammar *grammar = NULL;
	switch_core_new_memory_pool(&pool);
	grammar = switch_core_alloc(pool, sizeof (*grammar));
	grammar->pool = pool;
	grammar->root = NULL;
	grammar->cur = NULL;
	grammar->uuid = (parser && !zstr(parser->uuid)) ? switch_core_strdup(pool, parser->uuid) : "";
	switch_core_hash_init(&grammar->rules);
	switch_mutex_init(&grammar->mutex, SWITCH_MUTEX_NESTED, pool);
	return grammar;
}

/**
 * Destroy a parsed grammar
 * @param grammar the grammar
 */
static void srgs_grammar_destroy(struct srgs_grammar *grammar)
{
	switch_memory_pool_t *pool = grammar->pool;
	if (grammar->compiled_regex) {
		pcre_free(grammar->compiled_regex);
	}
	if (grammar->jsgf_file_name) {
		switch_file_remove(grammar->jsgf_file_name, pool);
	}
	switch_core_destroy_memory_pool(&pool);
}

/**
 * Create a new parser.
 * @param uuid optional uuid for logging
 * @return the created parser
 */
struct srgs_parser *srgs_parser_new(const char *uuid)
{
	switch_memory_pool_t *pool = NULL;
	struct srgs_parser *parser = NULL;
	switch_core_new_memory_pool(&pool);
	if (pool) {
		parser = switch_core_alloc(pool, sizeof(*parser));
		parser->pool = pool;
		parser->uuid = zstr(uuid) ? "" : switch_core_strdup(pool, uuid);
		switch_core_hash_init(&parser->cache);
		switch_mutex_init(&parser->mutex, SWITCH_MUTEX_NESTED, pool);
	}
	return parser;
}

/**
 * Destroy the parser.
 * @param parser to destroy
 */
void srgs_parser_destroy(struct srgs_parser *parser)
{
	switch_memory_pool_t *pool = parser->pool;
	switch_hash_index_t *hi = NULL;

	if (parser->cache) {
		/* clean up all cached grammars */
		for (hi = switch_core_hash_first(parser->cache); hi; hi = switch_core_hash_next(&hi)) {
			struct srgs_grammar *grammar = NULL;
			const void *key;
			void *val;
			switch_core_hash_this(hi, &key, NULL, &val);
			grammar = (struct srgs_grammar *)val;
			switch_assert(grammar);
			srgs_grammar_destroy(grammar);
		}
		switch_core_hash_destroy(&parser->cache);
	}
	switch_core_destroy_memory_pool(&pool);
}

/**
 * Create regexes
 * @param grammar the grammar
 * @param node root node
 * @param stream set to NULL
 * @return 1 if successful
 */
static int create_regexes(struct srgs_grammar *grammar, struct srgs_node *node, switch_stream_handle_t *stream)
{
	sn_log_node_open(node);
	switch (node->type) {
		case SNT_GRAMMAR:
			if (node->child) {
				int num_rules = 0;
				struct srgs_node *child = node->child;
				if (grammar->root_rule) {
					if (!create_regexes(grammar, grammar->root_rule, NULL)) {
						return 0;
					}
					grammar->regex = switch_core_sprintf(grammar->pool, "^%s$", grammar->root_rule->value.rule.regex);
				} else {
					switch_stream_handle_t new_stream = { 0 };
					SWITCH_STANDARD_STREAM(new_stream);
					if (node->num_children > 1) {
						new_stream.write_function(&new_stream, "%s", "^(?:");
					} else {
						new_stream.write_function(&new_stream, "%s", "^");
					}
					for (; child; child = child->next) {
						if (!create_regexes(grammar, child, &new_stream)) {
							switch_safe_free(new_stream.data);
							return 0;
						}
						if (child->type == SNT_RULE && child->value.rule.is_public) {
							if (num_rules > 0) {
								new_stream.write_function(&new_stream, "%s", "|");
							}
							new_stream.write_function(&new_stream, "%s", child->value.rule.regex);
							num_rules++;
						}
					}
					if (node->num_children > 1) {
						new_stream.write_function(&new_stream, "%s", ")$");
					} else {
						new_stream.write_function(&new_stream, "%s", "$");
					}
					grammar->regex = switch_core_strdup(grammar->pool, new_stream.data);
					switch_safe_free(new_stream.data);
				}
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_DEBUG, "document regex = %s\n", grammar->regex);
			}
			break;
		case SNT_RULE:
			if (node->value.rule.regex) {
				return 1;
			} else if (node->child) {
				struct srgs_node *item = node->child;
				switch_stream_handle_t new_stream = { 0 };
				SWITCH_STANDARD_STREAM(new_stream);
				for (; item; item = item->next) {
					if (!create_regexes(grammar, item, &new_stream)) {
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_DEBUG, "%s regex failed = %s\n", node->value.rule.id, node->value.rule.regex);
						switch_safe_free(new_stream.data);
						return 0;
					}
				}
				node->value.rule.regex = switch_core_strdup(grammar->pool, new_stream.data);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_DEBUG, "%s regex = %s\n", node->value.rule.id, node->value.rule.regex);
				switch_safe_free(new_stream.data);
			}
			break;
		case SNT_STRING: {
			int i;
			for (i = 0; i < strlen(node->value.string); i++) {
				switch (node->value.string[i]) {
					case '[':
					case '\\':
					case '^':
					case '$':
					case '.':
					case '|':
					case '?':
					case '*':
					case '+':
					case '(':
					case ')':
						/* escape special PCRE regex characters */
						stream->write_function(stream, "\\%c", node->value.string[i]);
						break;
					default:
						stream->write_function(stream, "%c", node->value.string[i]);
						break;
				}
			}
			if (node->child) {
				if (!create_regexes(grammar, node->child, stream)) {
					return 0;
				}
			}
			break;
		}
		case SNT_ITEM:
			if (node->child) {
				struct srgs_node *item = node->child;
				if (node->value.item.repeat_min != 1 || node->value.item.repeat_max != 1 || node->value.item.tag) {
					if (node->value.item.tag) {
						stream->write_function(stream, "(?P<%d>", node->value.item.tag);
					} else {
						stream->write_function(stream, "%s", "(?:");
					}
				}
				for(; item; item = item->next) {
					if (!create_regexes(grammar, item, stream)) {
						return 0;
					}
				}
				if (node->value.item.repeat_min != 1 || node->value.item.repeat_max != 1) {
					if (node->value.item.repeat_min != node->value.item.repeat_max) {
						if (node->value.item.repeat_min == 0 && node->value.item.repeat_max == INT_MAX) {
								stream->write_function(stream, ")*");
						} else if (node->value.item.repeat_min == 0 && node->value.item.repeat_max == 1) {
								stream->write_function(stream, ")?");
						} else if (node->value.item.repeat_min == 1 && node->value.item.repeat_max == INT_MAX) {
							stream->write_function(stream, ")+");
						} else if (node->value.item.repeat_max == INT_MAX) {
							stream->write_function(stream, "){%i,1000}", node->value.item.repeat_min);
						} else {
							stream->write_function(stream, "){%i,%i}", node->value.item.repeat_min, node->value.item.repeat_max);
						}
					} else {
						stream->write_function(stream, "){%i}", node->value.item.repeat_min);
					}
				} else if (node->value.item.tag) {
					stream->write_function(stream, "%s", ")");
				}
			}
			break;
		case SNT_ONE_OF:
			if (node->child) {
				struct srgs_node *item = node->child;
				if (node->num_children > 1) {
					stream->write_function(stream, "%s", "(?:");
				}
				for (; item; item = item->next) {
					if (item != node->child) {
						stream->write_function(stream, "%s", "|");
					}
					if (!create_regexes(grammar, item, stream)) {
						return 0;
					}
				}
				if (node->num_children > 1) {
					stream->write_function(stream, "%s", ")");
				}
			}
			break;
		case SNT_REF: {
			struct srgs_node *rule = node->value.ref.node;
			if (!rule->value.rule.regex) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_DEBUG, "ruleref: create %s regex\n", rule->value.rule.id);
				if (!create_regexes(grammar, rule, NULL)) {
					return 0;
				}
			}
			if (!rule->value.rule.regex) {
				return 0;
			}
			stream->write_function(stream, "%s", rule->value.rule.regex);
			break;
		}
		case SNT_ANY:
		default:
			/* ignore */
			return 1;
	}
	sn_log_node_close(node);
	return 1;
}

/**
 * Compile regex
 */
static pcre *get_compiled_regex(struct srgs_grammar *grammar)
{
	int erroffset = 0;
	const char *errptr = "";
	int options = 0;
	const char *regex;

	if (!grammar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "grammar is NULL!\n");
		return NULL;
	}

	switch_mutex_lock(grammar->mutex);
	if (!grammar->compiled_regex && (regex = srgs_grammar_to_regex(grammar))) {
		if (!(grammar->compiled_regex = pcre_compile(regex, options, &errptr, &erroffset, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_WARNING, "Failed to compile grammar regex: %s\n", regex);
		}
	}
	switch_mutex_unlock(grammar->mutex);
	return grammar->compiled_regex;
}

/**
 * Resolve all unresolved references and detect loops.
 * @param grammar the grammar
 * @param node the current node
 * @param level the recursion level
 */
static int resolve_refs(struct srgs_grammar *grammar, struct srgs_node *node, int level)
{
	sn_log_node_open(node);
	if (node->visited) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Loop detected.\n");
		return 0;
	}
	node->visited = 1;

	if (level > MAX_RECURSION) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Recursion too deep.\n");
		return 0;
	}

	if (node->type == SNT_GRAMMAR && node->value.root) {
		struct srgs_node *rule = (struct srgs_node *)switch_core_hash_find(grammar->rules, node->value.root);
		if (!rule) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Root rule not found: %s\n", node->value.root);
			return 0;
		}
		grammar->root_rule = rule;
	}

	if (node->type == SNT_UNRESOLVED_REF) {
		/* resolve reference to local rule- drop first character # from URI */
		struct srgs_node *rule = (struct srgs_node *)switch_core_hash_find(grammar->rules, node->value.ref.uri + 1);
		if (!rule) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(grammar->uuid), SWITCH_LOG_INFO, "Local rule not found: %s\n", node->value.ref.uri);
			return 0;
		}

		/* link to rule */
		node->type = SNT_REF;
		node->value.ref.node = rule;
	}

	/* travel through rule to detect loops */
	if (node->type == SNT_REF) {
		if (!resolve_refs(grammar, node->value.ref.node, level + 1)) {
			return 0;
		}
	}

	/* resolve children refs */
	if (node->child) {
		struct srgs_node *child = node->child;
		for (; child; child = child->next) {
			if (!resolve_refs(grammar, child, level + 1)) {
				return 0;
			}
		}
	}

	node->visited = 0;
	sn_log_node_close(node);
	return 1;
}

/**
 * Parse the document into rules to match
 * @param parser the parser
 * @param document the document to parse
 * @return the parsed grammar if successful
 */
struct srgs_grammar *srgs_parse(struct srgs_parser *parser, const char *document)
{
	struct srgs_grammar *grammar = NULL;
	if (!parser) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "NULL parser!!\n");
		return NULL;
	}

	if (zstr(document)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "Missing grammar document\n");
		return NULL;
	}

	/* check for cached grammar */
	switch_mutex_lock(parser->mutex);
	grammar = (struct srgs_grammar *)switch_core_hash_find(parser->cache, document);
	if (!grammar) {
		int result = 0;
		iksparser *p;
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_DEBUG, "Parsing new grammar\n");
		grammar = srgs_grammar_new(parser);
		p = iks_sax_new(grammar, tag_hook, cdata_hook);
		if (iks_parse(p, document, 0, 1) == IKS_OK) {
			if (grammar->root) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_DEBUG, "Resolving references\n");
				if (resolve_refs(grammar, grammar->root, 0)) {
					result = 1;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "Nothing to parse!\n");
			}
		}
		iks_parser_delete(p);
		if (result) {
			switch_core_hash_insert(parser->cache, document, grammar);
		} else {
			if (grammar) {
				srgs_grammar_destroy(grammar);
				grammar = NULL;
			}
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_INFO, "Failed to parse grammar\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(parser->uuid), SWITCH_LOG_DEBUG, "Using cached grammar\n");
	}
	switch_mutex_unlock(parser->mutex);

	return grammar;
}

#define MAX_INPUT_SIZE 128
#define OVECTOR_SIZE MAX_TAGS
#define WORKSPACE_SIZE 1024

/**
 * Check if no more digits can be added to input and match
 * @param compiled_regex the regex used in the initial match
 * @param input the input to check
 * @return true if end of match (no more input can be added)
 */
static int is_match_end(pcre *compiled_regex, const char *input)
{
	int ovector[OVECTOR_SIZE];
	int input_size = strlen(input);
	char search_input[MAX_INPUT_SIZE + 2];
	const char *search_set = "0123456789#*ABCD";
	const char *search = strchr(search_set, input[input_size - 1]); /* start with last digit in input */
	int i = 0;

	if (!search) {
		return 0;
	}

	/* For each digit in search_set, check if input + search_set digit is a potential match.
	   If so, then this is not a match end.
	 */
	sprintf(search_input, "%sZ", input);
	for (i = 0; i < 16; i++) {
		int result;
		if (!*search) {
			search = search_set;
		}
		search_input[input_size] = *search++;
		result = pcre_exec(compiled_regex, NULL, search_input, input_size + 1, 0, 0,
			ovector, sizeof(ovector) / sizeof(ovector[0]));
		if (result > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not match end\n");
			return 0;
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "is match end\n");
	return 1;
}

/**
 * Find a match
 * @param grammar the grammar to match
 * @param input the input to compare
 * @param interpretation the (optional) interpretation of the input result
 * @return the match result
 */
enum srgs_match_type srgs_grammar_match(struct srgs_grammar *grammar, const char *input, const char **interpretation)
{
	int result = 0;
	int ovector[OVECTOR_SIZE];
	pcre *compiled_regex;

	*interpretation = NULL;

	if (zstr(input)) {
		return SMT_NO_MATCH;
	}
	if (strlen(input) > MAX_INPUT_SIZE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "input too large: %s\n", input);
		return SMT_NO_MATCH;
	}

	if (!(compiled_regex = get_compiled_regex(grammar))) {
		return SMT_NO_MATCH;
	}
	result = pcre_exec(compiled_regex, NULL, input, strlen(input), 0, PCRE_PARTIAL,
		ovector, OVECTOR_SIZE);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "match = %i\n", result);
	if (result > 0) {
		int i;
		char buffer[MAX_INPUT_SIZE + 1];
		buffer[MAX_INPUT_SIZE] = '\0';

		/* find matching instance... */
		for (i = 1; i <= grammar->tag_count; i++) {
			char substring_name[16] = { 0 };
			buffer[0] = '\0';
			snprintf(substring_name, 16, "%d", i);
			if (pcre_copy_named_substring(compiled_regex, input, ovector, result, substring_name, buffer, MAX_INPUT_SIZE) != PCRE_ERROR_NOSUBSTRING && !zstr_buf(buffer)) {
				*interpretation = grammar->tags[i];
				break;
			}
		}

		if (is_match_end(compiled_regex, input)) {
			return SMT_MATCH_END;
		}
		return SMT_MATCH;
	}
	if (result == PCRE_ERROR_PARTIAL) {
		return SMT_MATCH_PARTIAL;
	}

	return SMT_NO_MATCH;
}

/**
 * Generate regex from SRGS document.  Call this after parsing SRGS document.
 * @param parser the parser
 * @return the regex or NULL
 */
const char *srgs_grammar_to_regex(struct srgs_grammar *grammar)
{
	if (!grammar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "grammar is NULL!\n");
		return NULL;
	}
	switch_mutex_lock(grammar->mutex);
	if (!grammar->regex && !create_regexes(grammar, grammar->root, NULL)) {
		switch_mutex_unlock(grammar->mutex);
		return NULL;
	}
	switch_mutex_unlock(grammar->mutex);
	return grammar->regex;
}

/**
 * Create JSGF grammar
 * @param parser the parser
 * @param node root node
 * @param stream set to NULL
 * @return 1 if successful
 */
static int create_jsgf(struct srgs_grammar *grammar, struct srgs_node *node, switch_stream_handle_t *stream)
{
	sn_log_node_open(node);
	switch (node->type) {
		case SNT_GRAMMAR:
			if (node->child) {
				struct srgs_node *child;
				switch_stream_handle_t new_stream = { 0 };
				SWITCH_STANDARD_STREAM(new_stream);

				new_stream.write_function(&new_stream, "#JSGF V1.0");
				if (!zstr(grammar->encoding)) {
					new_stream.write_function(&new_stream, " %s", grammar->encoding);
					if (!zstr(grammar->language)) {
						new_stream.write_function(&new_stream, " %s", grammar->language);
					}
				}

				new_stream.write_function(&new_stream,
					";\ngrammar org.freeswitch.srgs_to_jsgf;\n"
					"public ");

				/* output root rule */
				if (grammar->root_rule) {
					if (!create_jsgf(grammar, grammar->root_rule, &new_stream)) {
						switch_safe_free(new_stream.data);
						return 0;
					}
				} else {
					int num_rules = 0;
					int first = 1;

					for (child = node->child; child; child = child->next) {
						if (child->type == SNT_RULE && child->value.rule.is_public) {
							num_rules++;
						}
					}

					if (num_rules > 1) {
						new_stream.write_function(&new_stream, "<root> =");
						for (child = node->child; child; child = child->next) {
							if (child->type == SNT_RULE && child->value.rule.is_public) {
								if (!first) {
									new_stream.write_function(&new_stream, "%s", " |");
								}
								first = 0;
								new_stream.write_function(&new_stream, " <%s>", child->value.rule.id);
							}
						}
						new_stream.write_function(&new_stream, ";\n");
					} else {
						for (child = node->child; child; child = child->next) {
							if (child->type == SNT_RULE && child->value.rule.is_public) {
								grammar->root_rule = child;
								if (!create_jsgf(grammar, child, &new_stream)) {
									switch_safe_free(new_stream.data);
									return 0;
								} else {
									break;
								}
							}
						}
					}
				}

				/* output all rule definitions */
				for (child = node->child; child; child = child->next) {
					if (child->type == SNT_RULE && child != grammar->root_rule) {
						if (!create_jsgf(grammar, child, &new_stream)) {
							switch_safe_free(new_stream.data);
							return 0;
						}
					}
				}
				grammar->jsgf = switch_core_strdup(grammar->pool, new_stream.data);
				switch_safe_free(new_stream.data);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "document jsgf = %s\n", grammar->jsgf);
			}
			break;
		case SNT_RULE:
			if (node->child) {
				struct srgs_node *item = node->child;
				stream->write_function(stream, "<%s> =", node->value.rule.id);
				for (; item; item = item->next) {
					if (!create_jsgf(grammar, item, stream)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s jsgf rule failed\n", node->value.rule.id);
						return 0;
					}
				}
				stream->write_function(stream, ";\n");
			}
			break;
		case SNT_STRING: {
			int len = strlen(node->value.string);
			int i;
			stream->write_function(stream, " ");
			for (i = 0; i < len; i++) {
				switch (node->value.string[i]) {
					case '\\':
					case '*':
					case '+':
					case '/':
					case '(':
					case ')':
					case '[':
					case ']':
					case '{':
					case '}':
					case '=':
					case '<':
					case '>':
					case ';':
					case '|':
						stream->write_function(stream, "\\");
						break;
					default:
						break;
				}
				stream->write_function(stream, "%c", node->value.string[i]);
			}
			if (node->child) {
				if (!create_jsgf(grammar, node->child, stream)) {
					return 0;
				}
			}
			break;
		}
		case SNT_ITEM:
			if (node->child) {
				struct srgs_node *item;
				if (node->value.item.repeat_min == 0 && node->value.item.repeat_max == 1) {
					/* optional item */
					stream->write_function(stream, " [");
					for(item = node->child; item; item = item->next) {
						if (!create_jsgf(grammar, item, stream)) {
							return 0;
						}
					}
					stream->write_function(stream, " ]");
				} else {
					/* minimum repeats */
					int i;
					for (i = 0; i < node->value.item.repeat_min; i++) {
						if (node->value.item.repeat_min != 1 && node->value.item.repeat_max != 1) {
							stream->write_function(stream, " (");
						}
						for(item = node->child; item; item = item->next) {
							if (!create_jsgf(grammar, item, stream)) {
								return 0;
							}
						}
						if (node->value.item.repeat_min != 1 && node->value.item.repeat_max != 1) {
							stream->write_function(stream, " )");
						}
					}
					if (node->value.item.repeat_max == INT_MAX) {
						stream->write_function(stream, "*");
					} else {
						for (;i < node->value.item.repeat_max; i++) {
							stream->write_function(stream, " [");
							for(item = node->child; item; item = item->next) {
								if (!create_jsgf(grammar, item, stream)) {
									return 0;
								}
							}
							stream->write_function(stream, " ]");
						}
					}
				}
			}
			break;
		case SNT_ONE_OF:
			if (node->child) {
				struct srgs_node *item = node->child;
				if (node->num_children > 1) {
					stream->write_function(stream, " (");
				}
				for (; item; item = item->next) {
					if (item != node->child) {
						stream->write_function(stream, " |");
					}
					stream->write_function(stream, " (");
					if (!create_jsgf(grammar, item, stream)) {
						return 0;
					}
					stream->write_function(stream, " )");
				}
				if (node->num_children > 1) {
					stream->write_function(stream, " )");
				}
			}
			break;
		case SNT_REF: {
			struct srgs_node *rule = node->value.ref.node;
			stream->write_function(stream, " <%s>", rule->value.rule.id);
			break;
		}
		case SNT_ANY:
		default:
			/* ignore */
			return 1;
	}
	sn_log_node_close(node);
	return 1;
}

/**
 * Generate JSGF from SRGS document.  Call this after parsing SRGS document.
 * @param grammar the grammar
 * @return the JSGF document or NULL
 */
const char *srgs_grammar_to_jsgf(struct srgs_grammar *grammar)
{
	if (!grammar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "grammar is NULL!\n");
		return NULL;
	}
	switch_mutex_lock(grammar->mutex);
	if (!grammar->jsgf && !create_jsgf(grammar, grammar->root, NULL)) {
		switch_mutex_unlock(grammar->mutex);
		return NULL;
	}
	switch_mutex_unlock(grammar->mutex);
	return grammar->jsgf;
}

/**
 * Generate JSGF file from SRGS document.  Call this after parsing SRGS document.
 * @param grammar the grammar
 * @param basedir the base path to use if file does not already exist
 * @param ext the extension to use
 * @return the path or NULL
 */
const char *srgs_grammar_to_jsgf_file(struct srgs_grammar *grammar, const char *basedir, const char *ext)
{
	if (!grammar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "grammar is NULL!\n");
		return NULL;
	}
	switch_mutex_lock(grammar->mutex);
	if (!grammar->jsgf_file_name) {
		char file_name_buf[SWITCH_UUID_FORMATTED_LENGTH + 1];
		switch_file_t *file;
		switch_size_t len;
		const char *jsgf = srgs_grammar_to_jsgf(grammar);
                switch_uuid_str(file_name_buf, sizeof(file_name_buf));
		grammar->jsgf_file_name = switch_core_sprintf(grammar->pool, "%s%s%s.%s", basedir, SWITCH_PATH_SEPARATOR, file_name_buf, ext);
		if (!jsgf) {
			switch_mutex_unlock(grammar->mutex);
			return NULL;
		}

		/* write grammar to file */
		if (switch_file_open(&file, grammar->jsgf_file_name, SWITCH_FOPEN_WRITE | SWITCH_FOPEN_TRUNCATE | SWITCH_FOPEN_CREATE, SWITCH_FPROT_OS_DEFAULT, grammar->pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to create jsgf file: %s!\n", grammar->jsgf_file_name);
			grammar->jsgf_file_name = NULL;
			switch_mutex_unlock(grammar->mutex);
			return NULL;
		}
		len = strlen(jsgf);
		switch_file_write(file, jsgf, &len);
		switch_file_close(file);
	}
	switch_mutex_unlock(grammar->mutex);
	return grammar->jsgf_file_name;
}

/**
 * Initialize SRGS parser.  This function is not thread safe.
 */
int srgs_init(void)
{
	if (globals.init) {
		return 1;
	}

	globals.init = SWITCH_TRUE;
	switch_core_new_memory_pool(&globals.pool);
	switch_core_hash_init(&globals.tag_defs);

	add_root_tag_def("grammar", process_grammar, process_cdata_bad, "meta,metadata,lexicon,tag,rule");
	add_tag_def("ruleref", process_ruleref, process_cdata_bad, "");
	add_tag_def("token", process_attribs_ignore, process_cdata_ignore, "");
	add_tag_def("tag", process_attribs_ignore, process_cdata_tag, "");
	add_tag_def("one-of", process_attribs_ignore, process_cdata_tokens, "item");
	add_tag_def("item", process_item, process_cdata_tokens, "token,ruleref,item,one-of,tag");
	add_tag_def("rule", process_rule, process_cdata_tokens, "token,ruleref,item,one-of,tag,example");
	add_tag_def("example", process_attribs_ignore, process_cdata_ignore, "");
	add_tag_def("lexicon", process_attribs_ignore, process_cdata_bad, "");
	add_tag_def("meta", process_attribs_ignore, process_cdata_bad, "");
	add_tag_def("metadata", process_attribs_ignore, process_cdata_ignore, "ANY");
	add_tag_def("ANY", process_attribs_ignore, process_cdata_ignore, "ANY");

	return 1;
}

/**
 * Destruction of SRGS parser environment
 */
void srgs_destroy(void)
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
