/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Travis Cross <tc@traviscross.com>
 *
 * mod_prefix.c -- Longest-prefix match in-memory data store
 *
 */

#include <switch.h>
#include <switch_json.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "trie.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_prefix_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_prefix_shutdown);
SWITCH_MODULE_DEFINITION(mod_prefix, mod_prefix_load, mod_prefix_shutdown, NULL);

static struct {
	switch_memory_pool_t *pool;
	switch_hash_t *trees;
	switch_thread_rwlock_t *trees_rwlock;
} globals;

struct prefix_tree {
	struct bit_trie_node *node;
	switch_thread_rwlock_t *rwlock;
};

static cJSON* parse_file(const char *file) {
	cJSON *root;
	char *buf;
	struct stat s;
	int fd = open(file, O_RDONLY);
	if (fd < 0) return NULL;
	fstat(fd, &s);
	buf = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (buf == MAP_FAILED) return NULL;
	root = cJSON_Parse(buf);
	if (munmap(buf, s.st_size) < 0) abort();
	return root;
}

static struct bit_trie_node *load_file(const char *file) {
	struct bit_trie_node *node;
	cJSON *item, *root = parse_file(file);
	if (!root) return NULL;
	node = bit_trie_create();
	item = root->child;
	while(item) {
		if (item->string && item->valuestring) {
			bit_trie_set(node, (unsigned char*)item->string, strlen(item->string),
						 strdup(item->valuestring), strlen(item->valuestring)+1);
		}
		item = item->next;
	}
	cJSON_Delete(root);
	return node;
}

static int search_prefix_tree(switch_stream_handle_t *stream,
							  const char *name, const char *key) {
	struct prefix_tree *tr;
	switch_thread_rwlock_rdlock(globals.trees_rwlock);
	if ((tr = switch_core_hash_find(globals.trees, name))) {
		struct bit_trie_node *res = NULL;
		switch_thread_rwlock_rdlock(tr->rwlock);
		bit_trie_get(&res, tr->node, (unsigned char*)key, strlen(key));
		if (res && res->value) {
			stream->write_function(stream, "%s", res->value);
		} else {
			stream->write_function(stream, "");
		}
		switch_thread_rwlock_unlock(tr->rwlock);
	}
	switch_thread_rwlock_unlock(globals.trees_rwlock);
	return 0;
}

static int load_prefix_tree(const char *name, const char *file) {
	struct prefix_tree *tr;
	struct bit_trie_node *node;
	node = load_file(file);
	if (node) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded prefix file %s\n", file);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading prefix file %s\n", file);
		return 2;
	}
	switch_thread_rwlock_rdlock(globals.trees_rwlock);
	if ((tr = switch_core_hash_find(globals.trees, name))) {
		switch_thread_rwlock_wrlock(tr->rwlock);
		bit_trie_free(tr->node);
		tr->node = node;
		switch_thread_rwlock_unlock(tr->rwlock);
		switch_thread_rwlock_unlock(globals.trees_rwlock);
	} else {
		switch_thread_rwlock_unlock(globals.trees_rwlock);
		switch_thread_rwlock_wrlock(globals.trees_rwlock);
		if (!(tr = malloc(sizeof(struct prefix_tree)))) abort();
		memset(tr, 0, sizeof(struct prefix_tree));
		tr->node = node;
		switch_thread_rwlock_create(&tr->rwlock, globals.pool);
		switch_core_hash_insert(globals.trees, name, tr);
		switch_thread_rwlock_unlock(globals.trees_rwlock);
	}
	return 0;
}

static int drop_prefix_tree(const char *name) {
	struct prefix_tree *tr;
	switch_thread_rwlock_wrlock(globals.trees_rwlock);
	if ((tr = switch_core_hash_find(globals.trees, name))) {
		switch_core_hash_delete(globals.trees, name);
		switch_thread_rwlock_wrlock(tr->rwlock);
		bit_trie_free(tr->node);
		switch_thread_rwlock_unlock(tr->rwlock);
		switch_thread_rwlock_destroy(tr->rwlock);
		free(tr);
	}
	switch_thread_rwlock_unlock(globals.trees_rwlock);
	return 0;
}

static void do_config(switch_bool_t reload);

#define PREFIX_API_USAGE "get <table> <key> | load <table> <file> | drop <table> | reload"
SWITCH_STANDARD_API(prefix_api_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1 || !argv[0]) {
		goto usage;
	}

	if (!strcasecmp(argv[0], "get")) {
		char *name, *key;
		int ret;
		if (argc < 3) goto usage;
		name = argv[1]; key = argv[2];
		ret = search_prefix_tree(stream, name, key);
		if (ret == 1) goto usage;
	} else if (!strcasecmp(argv[0], "load")) {
		char *name, *file;
		if (argc < 3) goto usage;
		name = argv[1]; file = argv[2];
		if (!load_prefix_tree(name, file)) {
			stream->write_function(stream, "+OK\n");
		} else {
			stream->write_function(stream, "-ERR\n");
		}
	} else if (!strcasecmp(argv[0], "drop")) {
		char *name;
		if (argc < 2) goto usage;
		name = argv[1];
		if (!drop_prefix_tree(name)) {
			stream->write_function(stream, "+OK\n");
		} else {
			stream->write_function(stream, "-ERR\n");
		}
	} else if (!strcasecmp(argv[0], "reload")) {
		do_config(1);
		stream->write_function(stream, "+OK\n");
	} else {
		goto usage;
	}
	goto done;

 usage:
	stream->write_function(stream, "-ERR Usage: prefix %s\n", PREFIX_API_USAGE);

 done:
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

static void do_config(switch_bool_t reload)
{
	switch_xml_t xml = NULL, x_lists = NULL, x_list = NULL, cfg = NULL;
	if ((xml = switch_xml_open_cfg("prefix.conf", &cfg, NULL))) {
		if ((x_lists = switch_xml_child(cfg, "tables"))) {
			for (x_list = switch_xml_child(x_lists, "table"); x_list; x_list = x_list->next) {
				const char *name = switch_xml_attr(x_list, "name");
				const char *file = switch_xml_attr(x_list, "file");
				load_prefix_tree(name, file);
			}
		}
		switch_xml_free(xml);
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_prefix_load)
{
	switch_api_interface_t *api_interface;
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	switch_thread_rwlock_create(&globals.trees_rwlock, globals.pool);
	switch_core_hash_init(&globals.trees);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "prefix", "prefix handling", prefix_api_function, PREFIX_API_USAGE);
	switch_console_set_complete("add prefix get");
	switch_console_set_complete("add prefix load");
	switch_console_set_complete("add prefix drop");
	switch_console_set_complete("add prefix reload");
	do_config(SWITCH_FALSE);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_prefix_shutdown)
{
	switch_hash_index_t *hi = NULL;
	switch_thread_rwlock_wrlock(globals.trees_rwlock);
	while ((hi = switch_core_hash_first_iter(globals.trees, hi))) {
		void *val = NULL;
		struct prefix_tree *tr = NULL;
		const void *key;
		switch_ssize_t keylen;
		switch_core_hash_this(hi, &key, &keylen, &val);
		tr = (struct prefix_tree*)val;
		switch_core_hash_delete(globals.trees, key);
		switch_thread_rwlock_wrlock(tr->rwlock);
		bit_trie_free(tr->node);
		switch_thread_rwlock_unlock(tr->rwlock);
		switch_thread_rwlock_destroy(tr->rwlock);
		free(tr);
	}
	switch_core_hash_destroy(&globals.trees);
	switch_thread_rwlock_unlock(globals.trees_rwlock);
	switch_thread_rwlock_destroy(globals.trees_rwlock);
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * show-trailing-whitespace:t
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
