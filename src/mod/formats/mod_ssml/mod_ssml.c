/*
 * mod_ssml for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * The Original Code is mod_ssml for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * mod_ssml.c -- SSML audio rendering format
 *
 */
#include <switch.h>
#include <iksemel.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_ssml_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ssml_shutdown);
SWITCH_MODULE_DEFINITION(mod_ssml, mod_ssml_load, mod_ssml_shutdown, NULL);

#define MAX_VOICE_FILES 256
#define MAX_VOICE_PRIORITY 999
#define VOICE_NAME_PRIORITY 1000
#define VOICE_GENDER_PRIORITY 1000
#define VOICE_LANG_PRIORITY 1000000

struct ssml_parser;

/** function to handle tag attributes */
typedef int (* tag_attribs_fn)(struct ssml_parser *, char **);
/** function to handle tag CDATA */
typedef int (* tag_cdata_fn)(struct ssml_parser *, char *, size_t);

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
 * Module configuration
 */
static struct {
	/** Mapping of mod-name-language-gender to voice */
	switch_hash_t *voice_cache;
	/** Mapping of voice names */
	switch_hash_t *say_voice_map;
	/** Synchronizes access to say_voice_map */
	switch_mutex_t *say_voice_map_mutex;
	/** Mapping of voice names */
	switch_hash_t *tts_voice_map;
	/** Synchronizes access to tts_voice_map */
	switch_mutex_t *tts_voice_map_mutex;
	/** Mapping of interpret-as value to macro */
	switch_hash_t *interpret_as_map;
	/** Mapping of ISO language code to say-module */
	switch_hash_t *language_map;
	/** Mapping of tag name to definition */
	switch_hash_t *tag_defs;
	/** module memory pool */
	switch_memory_pool_t *pool;
} globals;

/**
 * A say language
 */
struct language {
	/** The ISO language code */
	char *iso;
	/** The FreeSWITCH language code */
	char *language;
	/** The say module name */
	char *say_module;
};

/**
 * A say macro
 */
struct macro {
	/** interpret-as name (cardinal...) */
	char *name;
	/** language (en-US, en-UK, ...) */
	char *language;
	/** type (number, items, persons, messages...) */
	char *type;
	/** method (pronounced, counted, iterated...) */
	char *method;
};

/**
 * A TTS voice
 */
struct voice {
	/** higher priority = more likely to pick */
	int priority;
	/** voice gender */
	char *gender;
	/** voice name / macro */
	char *name;
	/** voice language */
	char *language;
	/** internal file prefix */
	char *prefix;
};

#define TAG_LEN 32
#define NAME_LEN 128
#define LANGUAGE_LEN 6
#define GENDER_LEN 8

/**
 * SSML voice state
 */
struct ssml_node {
	/** tag name */
	char tag_name[TAG_LEN];
	/** requested name */
	char name[NAME_LEN];
	/** requested language */
	char language[LANGUAGE_LEN];
	/** requested gender */
	char gender[GENDER_LEN];
	/** voice to use */
	struct voice *tts_voice;
	/** say macro to use */
	struct macro *say_macro;
	/** tag handling data */
	struct tag_def *tag_def;
	/** previous node */
	struct ssml_node *parent_node;
};

/**
 * A file to play
 */
struct ssml_file {
	/** prefix to add to file handle */
	char *prefix;
	/** the file to play */
	const char *name;
};

/**
 * SSML parser state
 */
struct ssml_parser {
	/** current attribs */
	struct ssml_node *cur_node;
	/** files to play */
	struct ssml_file *files;
	/** number of files */
	int num_files;
	/** max files to play */
	int max_files;
	/** memory pool to use */
	switch_memory_pool_t *pool;
	/** desired sample rate */
	int sample_rate;
};

/**
 * SSML playback state
 */
struct ssml_context {
	/** handle to current file */
	switch_file_handle_t fh;
	/** files to play */
	struct ssml_file *files;
	/** number of files */
	int num_files;
	/** current file being played */
	int index;
};

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
	switch_core_hash_insert(globals.tag_defs, tag, def);
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
static int process_tag(struct ssml_parser *parser, const char *name, char **atts)
{
	struct tag_def *def = switch_core_hash_find(globals.tag_defs, name);
	if (def) {
		parser->cur_node->tag_def = def;
		if (def->is_root && parser->cur_node->parent_node == NULL) {
			/* no parent for ROOT tags */
			return def->attribs_fn(parser, atts);
		} else if (!def->is_root && parser->cur_node->parent_node) {
			/* check if this child is allowed by parent node */
			struct tag_def *parent_def = parser->cur_node->parent_node->tag_def;
			if (switch_core_hash_find(parent_def->children_tags, "ANY") ||
				switch_core_hash_find(parent_def->children_tags, name)) {
				return def->attribs_fn(parser, atts);
			}
		}
	}
	return IKS_BADXML;
}

/**
 * Handle CDATA that is ignored
 * @param parser the parser
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_OK
 */
static int process_cdata_ignore(struct ssml_parser *parser, char *data, size_t len)
{
	return IKS_OK;
}

/**
 * Handle CDATA that is not allowed
 * @param parser the parser
 * @param data the CDATA
 * @param len the CDATA length
 * @return IKS_BADXML
 */
static int process_cdata_bad(struct ssml_parser *parser, char *data, size_t len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (isgraph(data[i])) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unexpected CDATA for <%s>\n", parser->cur_node->tag_name);
			return IKS_BADXML;
		}
	}
	return IKS_OK;
}

/**
 * Score the voice on how close it is to desired language, name, and gender
 * @param voice the voice to score
 * @param cur_node the desired voice attributes
 * @param lang_required if true, language must match
 * @return the score
 */
static int score_voice(struct voice *voice, struct ssml_node *cur_node, int lang_required)
{
	/* language > gender,name > priority */
	int score = voice->priority;
	if (!zstr_buf(cur_node->gender) && !strcmp(cur_node->gender, voice->gender)) {
		score += VOICE_GENDER_PRIORITY;
	}
	if (!zstr_buf(cur_node->name) && !strcmp(cur_node->name, voice->name)) {
		score += VOICE_NAME_PRIORITY;
	}
	if (!zstr_buf(cur_node->language) && !strcmp(cur_node->language, voice->language)) {
		score += VOICE_LANG_PRIORITY;
	} else if (lang_required) {
		score = 0;
	}
	return score;
}

/**
 * Search for best voice based on attributes
 * @param cur_node the desired voice attributes
 * @param map the map to search
 * @param type "say" or "tts"
 * @param lang_required if true, language must match
 * @return the voice or NULL
 */
static struct voice *find_voice(struct ssml_node *cur_node, switch_hash_t *map, char *type, int lang_required)
{
	switch_hash_index_t *hi = NULL;
	struct voice *voice = (struct voice *)switch_core_hash_find(map, cur_node->name);
	char *lang_name_gender = NULL;
	int best_score = 0;

	/* check cache */
	lang_name_gender = switch_mprintf("%s-%s-%s-%s", type, cur_node->language, cur_node->name, cur_node->gender);
	voice = (struct voice *)switch_core_hash_find(globals.voice_cache, lang_name_gender);
	if (voice) {
		/* that was easy! */
		goto done;
	}

	/* find best language, name, gender match */
	for (hi = switch_core_hash_first(map); hi; hi = switch_core_hash_next(hi)) {
		const void *key;
		void *val;
		struct voice *candidate;
		int candidate_score = 0;
		switch_core_hash_this(hi, &key, NULL, &val);
		candidate = (struct voice *)val;
		candidate_score = score_voice(candidate, cur_node, lang_required);
		if (candidate_score > 0 && candidate_score > best_score) {
			voice = candidate;
			best_score = candidate_score;
		}
	}

	/* remember for next time */
	if (voice) {
		switch_core_hash_insert(globals.voice_cache, lang_name_gender, voice);
	}

done:
	switch_safe_free(lang_name_gender);

	return voice;
}

/**
 * Search for best voice based on attributes
 * @param cur_node the desired voice attributes
 * @return the voice or NULL
 */
static struct voice *find_tts_voice(struct ssml_node *cur_node)
{
	struct voice *v;
	switch_mutex_lock(globals.tts_voice_map_mutex);
	v = find_voice(cur_node, globals.tts_voice_map, "tts", 0);
	switch_mutex_unlock(globals.tts_voice_map_mutex);
	return v;
}

/**
 * Search for best voice based on attributes
 * @param cur_node the desired voice attributes
 * @return the voice or NULL
 */
static struct voice *find_say_voice(struct ssml_node *cur_node)
{
	struct voice *v;
	switch_mutex_lock(globals.say_voice_map_mutex);
	v = find_voice(cur_node, globals.say_voice_map, "say", 1);
	switch_mutex_unlock(globals.say_voice_map_mutex);
	return v;
}

/**
 * Handle tag attributes that are ignored
 * @param parser the parser
 * @param atts the attributes
 * @return IKS_OK
 */
static int process_attribs_ignore(struct ssml_parser *parsed_data, char **atts)
{
	struct ssml_node *cur_node = parsed_data->cur_node;
	cur_node->tts_voice = find_tts_voice(cur_node);
	return IKS_OK;
}

/**
 * open next file for reading
 * @param handle the file handle
 */
static switch_status_t next_file(switch_file_handle_t *handle)
{
	struct ssml_context *context = handle->private_info;
	const char *file;

  top:

	context->index++;

	if (switch_test_flag((&context->fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&context->fh);
	}

	if (context->index >= context->num_files) {
		return SWITCH_STATUS_FALSE;
	}


	file = context->files[context->index].name;
	context->fh.prefix = context->files[context->index].prefix;

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		/* unsupported */
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_file_open(&context->fh, file, handle->channels, handle->samplerate, handle->flags, NULL) != SWITCH_STATUS_SUCCESS) {
		goto top;
	}

	handle->samples = context->fh.samples;
	handle->format = context->fh.format;
	handle->sections = context->fh.sections;
	handle->seekable = context->fh.seekable;
	handle->speed = context->fh.speed;
	handle->interval = context->fh.interval;

	if (switch_test_flag((&context->fh), SWITCH_FILE_NATIVE)) {
		switch_set_flag(handle, SWITCH_FILE_NATIVE);
	} else {
		switch_clear_flag(handle, SWITCH_FILE_NATIVE);
	}

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Process xml:lang attribute
 */
static int process_xml_lang(struct ssml_parser *parsed_data, char **atts)
{
	struct ssml_node *cur_node = parsed_data->cur_node;

	/* only allow language change in <speak>, <p>, and <s> */
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("xml:lang", atts[i])) {
				if (!zstr(atts[i + 1])) {
					strncpy(cur_node->language, atts[i + 1], LANGUAGE_LEN);
					cur_node->language[LANGUAGE_LEN - 1] = '\0';
				}
			}
			i += 2;
		}
	}
	cur_node->tts_voice = find_tts_voice(cur_node);
	return IKS_OK;
}

/**
 * Process <voice>
 */
static int process_voice(struct ssml_parser *parsed_data, char **atts)
{
	struct ssml_node *cur_node = parsed_data->cur_node;
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("xml:lang", atts[i])) {
				if (!zstr(atts[i + 1])) {
					strncpy(cur_node->language, atts[i + 1], LANGUAGE_LEN);
					cur_node->language[LANGUAGE_LEN - 1] = '\0';
				}
			} else if (!strcmp("name", atts[i])) {
				if (!zstr(atts[i + 1])) {
					strncpy(cur_node->name, atts[i + 1], NAME_LEN);
					cur_node->name[NAME_LEN - 1] = '\0';
				}
			} else if (!strcmp("gender", atts[i])) {
				if (!zstr(atts[i + 1])) {
					strncpy(cur_node->gender, atts[i + 1], GENDER_LEN);
					cur_node->gender[GENDER_LEN - 1] = '\0';
				}
			}
			i += 2;
		}
	}
	cur_node->tts_voice = find_tts_voice(cur_node);
	return IKS_OK;
}

/**
 * Process <say-as>
 */
static int process_say_as(struct ssml_parser *parsed_data, char **atts)
{
	struct ssml_node *cur_node = parsed_data->cur_node;
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("interpret-as", atts[i])) {
				char *interpret_as = atts[i + 1];
				if (!zstr(interpret_as)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "interpret-as: %s\n", atts[i + 1]);
					cur_node->say_macro = (struct macro *)switch_core_hash_find(globals.interpret_as_map, interpret_as);
				}
				break;
			}
			i += 2;
		}
	}
	cur_node->tts_voice = find_tts_voice(cur_node);
	return IKS_OK;
}

/**
 * Process <break>- this is a period of silence
 */
static int process_break(struct ssml_parser *parsed_data, char **atts)
{
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("time", atts[i])) {
				char *t = atts[i + 1];
				if (!zstr(t) && parsed_data->num_files < parsed_data->max_files) {
					int timeout_ms = 0;
					char *unit;
					if ((unit = strstr(t, "ms"))) {
						*unit = '\0';
						if (switch_is_number(t)) {
							timeout_ms = atoi(t);
						}
					} else if ((unit = strstr(t, "s"))) {
						*unit = '\0';
						if (switch_is_number(t)) {
							timeout_ms = atoi(t) * 1000;
						}
					}
					if (timeout_ms > 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding <break>: \"%s\"\n", t);
						parsed_data->files[parsed_data->num_files].name = switch_core_sprintf(parsed_data->pool, "silence_stream://%i", timeout_ms);
						parsed_data->files[parsed_data->num_files++].prefix = NULL;
					}
				}
				return IKS_OK;
			}
			i += 2;
		}
	}
	return IKS_OK;
}

/**
 * Process <audio>- this is a URL to play
 */
static int process_audio(struct ssml_parser *parsed_data, char **atts)
{
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("src", atts[i])) {
				char *src = atts[i + 1];
				ikstack *stack = NULL;
				if (!zstr(src) && parsed_data->num_files < parsed_data->max_files) {
					/* get the URI */
					if (strchr(src, '&')) {
						stack = iks_stack_new(256, 0);
						src = iks_unescape(stack, src, strlen(src));
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding <audio>: \"%s\"\n", src);
					parsed_data->files[parsed_data->num_files].name = switch_core_strdup(parsed_data->pool, src);
					parsed_data->files[parsed_data->num_files++].prefix = NULL;
					if (stack) {
						iks_stack_delete(&stack);
					}
				}
				return IKS_OK;
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
	struct ssml_parser *parsed_data = (struct ssml_parser *)user_data;
	struct ssml_node *new_node = malloc(sizeof *new_node);
	struct ssml_node *parent_node = parsed_data->cur_node;

	if (type == IKS_OPEN || type == IKS_SINGLE) {
		if (parent_node) {
			/* inherit parent attribs */
			*new_node = *parent_node;
			new_node->parent_node = parent_node;
		} else {
			new_node->name[0] = '\0';
			new_node->language[0] = '\0';
			new_node->gender[0] = '\0';
			new_node->parent_node = NULL;
		}
		new_node->tts_voice = NULL;
		new_node->say_macro = NULL;
		strncpy(new_node->tag_name, name, TAG_LEN);
		new_node->tag_name[TAG_LEN - 1] = '\0';
		parsed_data->cur_node = new_node;
		result = process_tag(parsed_data, name, atts);
	}

	if (type == IKS_CLOSE || type == IKS_SINGLE) {
		if (parsed_data->cur_node) {
			struct ssml_node *parent_node = parsed_data->cur_node->parent_node;
			free(parsed_data->cur_node);
			parsed_data->cur_node = parent_node;
		}
	}

	return result;
}

/**
 * Try to get file(s) from say module
 * @param parsed_data
 * @param to_say
 * @return 1 if successful
 */
static int get_file_from_macro(struct ssml_parser *parsed_data, char *to_say)
{
	struct ssml_node *cur_node = parsed_data->cur_node;
	struct macro *say_macro = cur_node->say_macro;
	struct voice *say_voice = find_say_voice(cur_node);
	struct language *language;
	char *file_string = NULL;
	char *gender = NULL;
	switch_say_interface_t *si;

	/* voice is required */
	if (!say_voice) {
		return 0;
	}

	language = switch_core_hash_find(globals.language_map, say_voice->language);
	/* language is required */
	if (!language) {
		return 0;
	}

	/* TODO need to_say gender, not voice gender */
	gender = "neuter";

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Trying macro: %s, %s, %s, %s, %s\n", language->language, to_say, say_macro->type, say_macro->method, gender);

	if ((si = switch_loadable_module_get_say_interface(language->say_module)) && si->say_string_function) {
		switch_say_args_t say_args = {0};
		say_args.type = switch_ivr_get_say_type_by_name(say_macro->type);
		say_args.method = switch_ivr_get_say_method_by_name(say_macro->method);
		say_args.gender = switch_ivr_get_say_gender_by_name(gender);
		say_args.ext = "wav";
		si->say_string_function(NULL, to_say, &say_args, &file_string);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding macro: \"%s\", prefix=\"%s\"\n", file_string, say_voice->prefix);
	if (!zstr(file_string)) {
		parsed_data->files[parsed_data->num_files].name = switch_core_strdup(parsed_data->pool, file_string);
		parsed_data->files[parsed_data->num_files++].prefix = switch_core_strdup(parsed_data->pool, say_voice->prefix);
		return 1;
	}
	switch_safe_free(file_string);

	return 0;
}

/**
 * Get TTS file for voice
 */
static int get_file_from_voice(struct ssml_parser *parsed_data, char *to_say)
{
	struct ssml_node *cur_node = parsed_data->cur_node;
	if (cur_node->tts_voice) {
		char *file = switch_core_sprintf(parsed_data->pool, "%s%s", cur_node->tts_voice->prefix, to_say);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding <%s>: \"%s\"\n", cur_node->tag_name, file);
		parsed_data->files[parsed_data->num_files].name = file;
		parsed_data->files[parsed_data->num_files++].prefix = NULL;
		return 1;
	}
	return 0;
}

/**
 * Get TTS from CDATA
 */
static int process_cdata_tts(struct ssml_parser *parsed_data, char *data, size_t len)
{
	struct ssml_node *cur_node = parsed_data->cur_node;
	if (!len) {
		return IKS_OK;
	}
	if (cur_node && parsed_data->num_files < parsed_data->max_files) {
		int i = 0;
		int empty = 1;
		char *to_say;

		/* is CDATA empty? */
		for (i = 0; i < len && empty; i++) {
			empty &= !isgraph(data[i]);
		}
		if (empty) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Skipping empty tts\n");
			return IKS_OK;
		}

		/* try macro */
		to_say = malloc(len + 1);
		strncpy(to_say, data, len);
		to_say[len] = '\0';
		if (!cur_node->say_macro || !get_file_from_macro(parsed_data, to_say)) {
			/* use voice instead */
			if (!get_file_from_voice(parsed_data, to_say)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No TTS voices available to render text!\n");
			}
		}
		free(to_say);
		return IKS_OK;
	}
	return IKS_BADXML;
}

/**
 * Process <sub>- this is an alias for text to speak
 */
static int process_sub(struct ssml_parser *parsed_data, char **atts)
{
	if (atts) {
		int i = 0;
		while (atts[i]) {
			if (!strcmp("alias", atts[i])) {
				char *alias = atts[i + 1];
				if (!zstr(alias)) {
					return process_cdata_tts(parsed_data, alias, strlen(alias));
				}
				return IKS_BADXML;
			}
			i += 2;
		}
	}
	return IKS_OK;
}

/**
 * Process cdata
 */
static int cdata_hook(void *user_data, char *data, size_t len)
{
	struct ssml_parser *parsed_data = (struct ssml_parser *)user_data;
	if (!parsed_data) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing parser\n");
		return IKS_BADXML;
	}
	if (parsed_data->cur_node) {
		struct tag_def *handler = switch_core_hash_find(globals.tag_defs, parsed_data->cur_node->tag_name);
		if (handler) {
			return handler->cdata_fn(parsed_data, data, len);
		}
		return IKS_BADXML;
	}
	return IKS_OK;
}

/**
 * Transforms SSML into file_string format and opens file_string.
 * @param handle
 * @param path the inline SSML
 * @return SWITCH_STATUS_SUCCESS if opened
 */
static switch_status_t ssml_file_open(switch_file_handle_t *handle, const char *path)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	struct ssml_context *context = switch_core_alloc(handle->memory_pool, sizeof(*context));
	struct ssml_parser *parsed_data = switch_core_alloc(handle->memory_pool, sizeof(*parsed_data));
	iksparser *parser = iks_sax_new(parsed_data, tag_hook, cdata_hook);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Open: %s\n", path);

	parsed_data->cur_node = NULL;
	parsed_data->files = switch_core_alloc(handle->memory_pool, sizeof(struct ssml_file) * MAX_VOICE_FILES);
	parsed_data->max_files = MAX_VOICE_FILES;
	parsed_data->num_files = 0;
	parsed_data->pool = handle->memory_pool;
	parsed_data->sample_rate = handle->samplerate;

	if (iks_parse(parser, path, 0, 1) == IKS_OK) {
		if (parsed_data->num_files) {
			context->files = parsed_data->files;
			context->num_files = parsed_data->num_files;
			context->index = -1;
			handle->private_info = context;
			status = next_file(handle);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No files to play: %s\n", path);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Parse error: %s, num_files = %i\n", path, parsed_data->num_files);
	}

	iks_parser_delete(parser);

	return status;
}

/**
 * Close SSML document.
 * @param handle
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t ssml_file_close(switch_file_handle_t *handle)
{
	struct ssml_context *context = (struct ssml_context *)handle->private_info;
	if (switch_test_flag((&context->fh), SWITCH_FILE_OPEN)) {
		return switch_core_file_close(&context->fh);
	}

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Read from SSML document
 * @param handle
 * @param data
 * @param len
 * @return
 */
static switch_status_t ssml_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_status_t status;
	struct ssml_context *context = (struct ssml_context *)handle->private_info;
	size_t llen = *len;

	status = switch_core_file_read(&context->fh, data, len);
	if (status != SWITCH_STATUS_SUCCESS) {
		if ((status = next_file(handle)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		*len = llen;
		status = switch_core_file_read(&context->fh, data, len);
	}
	return status;
}

/**
 * Seek file
 */
static switch_status_t ssml_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	struct ssml_context *context = handle->private_info;

	if (samples == 0 && whence == SWITCH_SEEK_SET) {
		/* restart from beginning */
		context->index = -1;
		return next_file(handle);
	}

	if (!handle->seekable) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File is not seekable\n");
		return SWITCH_STATUS_NOTIMPL;
	}

	return switch_core_file_seek(&context->fh, cur_sample, samples, whence);
}

/**
 * TTS playback state
 */
struct tts_context {
	/** handle to TTS engine */
	switch_speech_handle_t sh;
	/** TTS flags */
	switch_speech_flag_t flags;
	/** maximum number of samples to read at a time */
	int max_frame_size;
	/** done flag */
	int done;
};

/**
 * Do TTS as file format
 * @param handle
 * @param path the inline SSML
 * @return SWITCH_STATUS_SUCCESS if opened
 */
static switch_status_t tts_file_open(switch_file_handle_t *handle, const char *path)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	struct tts_context *context = switch_core_alloc(handle->memory_pool, sizeof(*context));
	char *arg_string = switch_core_strdup(handle->memory_pool, path);
	char *args[3] = { 0 };
	int argc = switch_separate_string(arg_string, '|', args, (sizeof(args) / sizeof(args[0])));
	char *module;
	char *voice;
	char *document;

	/* path is module:(optional)profile|voice|{param1=val1,param2=val2}TTS document */
	if (argc != 3) {
		return SWITCH_STATUS_FALSE;
	}
	module = args[0];
	voice = args[1];
	document = args[2];

	memset(context, 0, sizeof(*context));
	context->flags = SWITCH_SPEECH_FLAG_NONE;
	if ((status = switch_core_speech_open(&context->sh, module, voice, handle->samplerate, handle->interval, &context->flags, NULL)) == SWITCH_STATUS_SUCCESS) {
		if ((status = switch_core_speech_feed_tts(&context->sh, document, &context->flags)) == SWITCH_STATUS_SUCCESS) {
			handle->channels = 1;
			handle->samples = 0;
			handle->format = 0;
			handle->sections = 0;
			handle->seekable = 0;
			handle->speed = 0;
			context->max_frame_size = handle->samplerate / 1000 * SWITCH_MAX_INTERVAL;
		} else {
			switch_core_speech_close(&context->sh, &context->flags);
		}
	}
	handle->private_info = context;
	return status;
}

/**
 * Read audio from TTS engine
 * @param handle
 * @param data
 * @param len
 * @return
 */
static switch_status_t tts_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	struct tts_context *context = (struct tts_context *)handle->private_info;
	switch_size_t rlen;

	if (*len > context->max_frame_size) {
		*len = context->max_frame_size;
	}
	rlen = *len * 2; /* rlen (bytes) = len (samples) * 2 */

	if (!context->done) {
		context->flags = SWITCH_SPEECH_FLAG_BLOCKING;
		if ((status = switch_core_speech_read_tts(&context->sh, data, &rlen, &context->flags))) {
			context->done = 1;
		}
	} else {
		switch_core_speech_flush_tts(&context->sh);
		memset(data, 0, rlen);
		status = SWITCH_STATUS_FALSE;
	}
	*len = rlen / 2; /* len (samples) = rlen (bytes) / 2 */
	return status;
}

/**
 * Close TTS engine
 * @param handle
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t tts_file_close(switch_file_handle_t *handle)
{
	struct tts_context *context = (struct tts_context *)handle->private_info;
	switch_core_speech_close(&context->sh, &context->flags);
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Configure voices
 * @param pool memory pool to use
 * @param map voice map to load
 * @param type type of voices (for logging)
 */
static void do_config_voices(switch_memory_pool_t *pool, switch_xml_t voices, switch_hash_t *map, const char *type)
{
	if (voices) {
		int priority = MAX_VOICE_PRIORITY;
		switch_xml_t voice;
		for (voice = switch_xml_child(voices, "voice"); voice; voice = voice->next) {
			const char *name = switch_xml_attr_soft(voice, "name");
			const char *language = switch_xml_attr_soft(voice, "language");
			const char *gender = switch_xml_attr_soft(voice, "gender");
			const char *prefix = switch_xml_attr_soft(voice, "prefix");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s map (%s, %s, %s) = %s\n", type, name, language, gender, prefix);
			if (!zstr(name) && !zstr(prefix)) {
				struct voice *v = (struct voice *)switch_core_alloc(pool, sizeof(*v));
				v->name = switch_core_strdup(pool, name);
				v->language = switch_core_strdup(pool, language);
				v->gender = switch_core_strdup(pool, gender);
				v->prefix = switch_core_strdup(pool, prefix);
				v->priority = priority--;
				switch_core_hash_insert(map, name, v);
			}
		}
	}
}

/**
 * Configure module
 * @param pool memory pool to use
 * @return SWITCH_STATUS_SUCCESS if module is configured
 */
static switch_status_t do_config(switch_memory_pool_t *pool)
{
	char *cf = "ssml.conf";
	switch_xml_t cfg, xml;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	/* get voices */
	do_config_voices(pool, switch_xml_child(cfg, "tts-voices"), globals.tts_voice_map, "tts");
	do_config_voices(pool, switch_xml_child(cfg, "say-voices"), globals.say_voice_map, "say");

	/* get languages */
	{
		switch_xml_t languages = switch_xml_child(cfg, "language-map");
		if (languages) {
			switch_xml_t language;
			for (language = switch_xml_child(languages, "language"); language; language = language->next) {
				const char *iso = switch_xml_attr_soft(language, "iso");
				const char *say_module = switch_xml_attr_soft(language, "say-module");
				const char *lang = switch_xml_attr_soft(language, "language");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "language map: %s = (%s, %s) \n", iso, say_module, lang);
				if (!zstr(iso) && !zstr(say_module) && !zstr(lang)) {
					struct language *l = (struct language *)switch_core_alloc(pool, sizeof(*l));
					l->iso = switch_core_strdup(pool, iso);
					l->say_module = switch_core_strdup(pool, say_module);
					l->language = switch_core_strdup(pool, lang);
					switch_core_hash_insert(globals.language_map, iso, l);
				}
			}
		}
	}

	/* get macros */
	{
		switch_xml_t macros = switch_xml_child(cfg, "macros");
		if (macros) {
			switch_xml_t macro;
			for (macro = switch_xml_child(macros, "macro"); macro; macro = macro->next) {
				const char *name = switch_xml_attr_soft(macro, "name");
				const char *method = switch_xml_attr_soft(macro, "method");
				const char *type = switch_xml_attr_soft(macro, "type");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "macro: %s = (%s, %s) \n", name, method, type);
				if (!zstr(name) && !zstr(type)) {
					struct macro *m = (struct macro *)switch_core_alloc(pool, sizeof(*m));
					m->name = switch_core_strdup(pool, name);
					m->method = switch_core_strdup(pool, method);
					m->type = switch_core_strdup(pool, type);
					switch_core_hash_insert(globals.interpret_as_map, name, m);
				}
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static char *ssml_supported_formats[] = { "ssml", NULL };
static char *tts_supported_formats[] = { "tts", NULL };

SWITCH_MODULE_LOAD_FUNCTION(mod_ssml_load)
{
	switch_file_interface_t *file_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = ssml_supported_formats;
	file_interface->file_open = ssml_file_open;
	file_interface->file_close = ssml_file_close;
	file_interface->file_read = ssml_file_read;
	file_interface->file_seek = ssml_file_seek;

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = tts_supported_formats;
	file_interface->file_open = tts_file_open;
	file_interface->file_close = tts_file_close;
	file_interface->file_read = tts_file_read;
	/* TODO allow skip ahead if TTS supports it
	 * file_interface->file_seek = tts_file_seek;
	 */

	globals.pool = pool;
	switch_core_hash_init(&globals.voice_cache);
	switch_core_hash_init(&globals.tts_voice_map);
	switch_mutex_init(&globals.tts_voice_map_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.say_voice_map);
	switch_mutex_init(&globals.say_voice_map_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.interpret_as_map);
	switch_core_hash_init(&globals.language_map);
	switch_core_hash_init(&globals.tag_defs);

	add_root_tag_def("speak", process_xml_lang, process_cdata_tts, "audio,break,emphasis,mark,phoneme,prosody,say-as,voice,sub,p,s,lexicon,metadata,meta");
	add_tag_def("p", process_xml_lang, process_cdata_tts, "audio,break,emphasis,mark,phoneme,prosody,say-as,voice,sub,s");
	add_tag_def("s", process_xml_lang, process_cdata_tts, "audio,break,emphasis,mark,phoneme,prosody,say-as,voice,sub");
	add_tag_def("voice", process_voice, process_cdata_tts, "audio,break,emphasis,mark,phoneme,prosody,say-as,voice,sub,p,s");
	add_tag_def("prosody", process_attribs_ignore, process_cdata_tts, "audio,break,emphasis,mark,phoneme,prosody,say-as,voice,sub,p,s");
	add_tag_def("audio", process_audio, process_cdata_tts, "audio,break,emphasis,mark,phoneme,prosody,say-as,voice,sub,p,s,desc");
	add_tag_def("desc", process_attribs_ignore, process_cdata_ignore, "");
	add_tag_def("emphasis", process_attribs_ignore, process_cdata_tts, "audio,break,emphasis,mark,phoneme,prosody,say-as,voice,sub");
	add_tag_def("say-as", process_say_as, process_cdata_tts, "");
	add_tag_def("sub", process_sub, process_cdata_ignore, "");
	add_tag_def("phoneme", process_attribs_ignore, process_cdata_tts, "");
	add_tag_def("break", process_break, process_cdata_bad, "");
	add_tag_def("mark", process_attribs_ignore, process_cdata_bad, "");
	add_tag_def("lexicon", process_attribs_ignore, process_cdata_bad, "");
	add_tag_def("metadata", process_attribs_ignore, process_cdata_ignore, "ANY");
	add_tag_def("meta", process_attribs_ignore, process_cdata_bad, "");

	return do_config(pool);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ssml_shutdown)
{
	switch_core_hash_destroy(&globals.voice_cache);
	switch_core_hash_destroy(&globals.tts_voice_map);
	switch_core_hash_destroy(&globals.say_voice_map);
	switch_core_hash_destroy(&globals.interpret_as_map);
	switch_core_hash_destroy(&globals.language_map);
	{
		switch_hash_index_t *hi = NULL;
		for (hi = switch_core_hash_first(globals.tag_defs); hi; hi = switch_core_hash_next(hi)) {
			const void *key;
			struct tag_def *def;
			switch_core_hash_this(hi, &key, NULL, (void *)&def);
			switch_core_hash_destroy(&def->children_tags);
		}
	}
	switch_core_hash_destroy(&globals.tag_defs);

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
