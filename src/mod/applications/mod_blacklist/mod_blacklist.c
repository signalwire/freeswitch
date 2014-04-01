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
 * Mathieu Rene <mrene@avgs.ca>
 * Raymond Chandler <intralanman@freeswitch.org>
 *
 * mod_blacklist.c -- Blacklist module
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_blacklist_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_blacklist_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_blacklist_load);


SWITCH_MODULE_DEFINITION(mod_blacklist, mod_blacklist_load, mod_blacklist_shutdown, NULL);

typedef struct {
	switch_hash_t *list;
	switch_mutex_t *list_mutex;
	switch_memory_pool_t *pool;
} blacklist_t;

static struct {
	switch_hash_t *files;
	switch_hash_t *lists;
	switch_mutex_t *files_mutex;
	switch_mutex_t *lists_mutex;
	switch_memory_pool_t *pool;
} globals;

blacklist_t *blacklist_create(const char *name) 
{
	switch_memory_pool_t *pool = NULL;
	blacklist_t *bl = NULL;
	
	switch_core_new_memory_pool(&pool);
	bl = switch_core_alloc(pool, sizeof(*bl));
	switch_assert(bl);
	bl->pool = pool;
	
	switch_core_hash_init(&bl->list);
	switch_mutex_init(&bl->list_mutex, SWITCH_MUTEX_NESTED, pool);
	
	return bl;
}

void blacklist_free(blacklist_t *bl)
{
	switch_core_destroy_memory_pool(&bl->pool);
}


void trim(char *string)
{
	char *p;
	if ((p = strchr(string, '\n'))) {
		*p = '\0';
	}
	if ((p = strchr(string, '\r'))) {
		*p = '\0';
	}
}

static switch_status_t load_list(const char *name, const char *filename)
{
	FILE *f;

	if ((f = fopen(filename, "r"))) {
		char buf[1024] = {0};
		blacklist_t *bl = blacklist_create(name);	/* Create a hashtable + mutex for that list */

		while (fgets(buf, 1024, f)) {
			trim(buf);
			switch_core_hash_insert(bl->list, buf, (void *)SWITCH_TRUE);
		}

		switch_core_hash_insert(globals.files, name, filename);
		switch_core_hash_insert(globals.lists, name, bl);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded list [%s]\n", name);

		fclose(f);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open [%s] to load list [%s]\n", filename, name);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t do_config(switch_bool_t reload)
{
	/* Load up blacklists */
	switch_xml_t xml, cfg, lists, list;
	switch_hash_index_t *hi = NULL;
	
	if (!(xml = switch_xml_open_cfg("mod_blacklist.conf", &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load configuration section\n");
		return SWITCH_STATUS_FALSE;
	}
	
	switch_mutex_lock(globals.lists_mutex);
	
	/* Destroy any active lists */
	while ((hi = switch_core_hash_first_iter( globals.lists, hi))) {
		const void *key;
		void *val;
		switch_core_hash_this(hi, &key, NULL, &val);
		blacklist_free((blacklist_t*)val);
		switch_core_hash_delete(globals.lists, (const char*)key);
	}
	
	if ((lists = switch_xml_child(cfg, "lists"))) {
		for (list = switch_xml_child(lists, "list"); list; list = list->next) {
			const char *name = switch_xml_attr_soft(list, "name");
			const char *filename = switch_xml_attr_soft(list, "filename");

			if (zstr(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "list has no name\n");
				continue;
			}
			if (zstr(filename)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "list [%s] has no filename\n", name);
				continue;
			}

			load_list(name, filename);
		}
	}
	
	switch_mutex_unlock(globals.lists_mutex);
	
	if (xml) {
		switch_xml_free(xml);
		xml = NULL;
	}

	return SWITCH_STATUS_SUCCESS;
}

#define BLACKLIST_API_SYNTAX	\
	"blacklist check <listname> <item>\n"	\
	"blacklist add <listname> <item>\n"	\
	"blacklist del <listname> <item>\n"	\
	"blacklist save <listname>\n"   \
	"blacklist reload\n"			\
	"blacklist help\n"

SWITCH_STANDARD_API(blacklist_api_function)
{
	char *data;
	int argc;
	char *argv[3];
	
	data = strdup(cmd);
	trim(data);
	if (!(argc = switch_separate_string(data, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid usage\n");
		goto done;
	}
	
	if (!strcasecmp(argv[0], "check")) {
		blacklist_t *bl = NULL;
		switch_bool_t result;
		
		if (argc < 2 || zstr(argv[1]) || zstr(argv[2])) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Wrong syntax");
			goto done;
		}
		
		switch_mutex_lock(globals.lists_mutex);
		bl = switch_core_hash_find(globals.lists, argv[1]);
		switch_mutex_unlock(globals.lists_mutex);
		
		if (!bl) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown blacklist [%s]\n", argv[1]);
			stream->write_function(stream, "false");
			goto done;
		}
		
		switch_mutex_lock(bl->list_mutex);
		result = (switch_bool_t)(intptr_t)switch_core_hash_find(bl->list, argv[2]);
		stream->write_function(stream, "%s", result ? "true" : "false");
		switch_mutex_unlock(bl->list_mutex);
	} else if (!strcasecmp(argv[0], "add")) {
		blacklist_t *bl = NULL;
		if (argc < 2 || zstr(argv[1]) || zstr(argv[2])) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Wrong syntax");
			goto done;
		}
		
		switch_mutex_lock(globals.lists_mutex);
		bl = switch_core_hash_find(globals.lists, argv[1]);
		switch_mutex_unlock(globals.lists_mutex);
		
		if (!bl) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown blacklist [%s]\n", argv[1]);
			stream->write_function(stream, "-ERR Unknown blacklist\n");
			goto done;
		}
		
		switch_mutex_lock(bl->list_mutex);
		switch_core_hash_insert(bl->list, argv[2], (void*)SWITCH_TRUE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added [%s] to list [%s]\n", argv[2], argv[1]);
		switch_mutex_unlock(bl->list_mutex);
		stream->write_function(stream, "+OK\n");
	} else if (!strcasecmp(argv[0], "del")) {
		blacklist_t *bl = NULL;
		if (argc < 2 || zstr(argv[1]) || zstr(argv[2])) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Wrong syntax");
			goto done;
		}
		
		switch_mutex_lock(globals.lists_mutex);
		bl = switch_core_hash_find(globals.lists, argv[1]);
		switch_mutex_unlock(globals.lists_mutex);
		
		if (!bl) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown blacklist [%s]\n", argv[1]);
			stream->write_function(stream, "-ERR Unknown blacklist\n");
			goto done;
		}
		
		switch_mutex_lock(bl->list_mutex);
		switch_core_hash_insert(bl->list, argv[2], NULL);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Removed [%s] from list [%s]\n", argv[2], argv[1]);
		switch_mutex_unlock(bl->list_mutex);
		stream->write_function(stream, "+OK\n");
	} else if (!strcasecmp(argv[0], "save"))  {
		switch_hash_index_t *hi;
		void *val;
		const void *var;
		blacklist_t *bl = NULL;
		char *filename = NULL;
		switch_file_t *fd;

		if (argc < 1 || zstr(argv[1])) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Missing blacklist name");
			goto done;
		}

		switch_mutex_lock(globals.lists_mutex);
		bl = switch_core_hash_find(globals.lists, argv[1]);
		switch_mutex_unlock(globals.lists_mutex);

		switch_mutex_lock(globals.files_mutex);
		filename = switch_core_hash_find(globals.files, argv[1]);
		switch_mutex_unlock(globals.files_mutex);

		if (!bl || !filename) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown blacklist [%s]\n", argv[1]);
			stream->write_function(stream, "-ERR Unknown blacklist\n");
			goto done;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Saving %s to %s\n", argv[1], filename);
		
		switch_mutex_lock(globals.lists_mutex);
		if (switch_file_open(&fd, filename, SWITCH_FOPEN_WRITE | SWITCH_FOPEN_TRUNCATE | SWITCH_FOPEN_CREATE, SWITCH_FPROT_OS_DEFAULT, globals.pool)
			== SWITCH_STATUS_SUCCESS) {
			for (hi = switch_core_hash_first(bl->list); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &var, NULL, &val);
				switch_file_printf(fd, "%s\n", (char *)var);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "adding %s to the dump file\n", (char *)var);
			}
			stream->write_function(stream, "+OK\n");			
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "couldn't open %s for writing\n", filename);
		}
		switch_mutex_unlock(globals.lists_mutex);
	} else if (!strcasecmp(argv[0], "reload"))  {
		do_config(SWITCH_TRUE);
		stream->write_function(stream, "+OK\n");
	} else if (!strcasecmp(argv[0], "help")) {
		stream->write_function(stream, BLACKLIST_API_SYNTAX "+OK\n");
	} else if (!zstr(argv[0])) {
		stream->write_function(stream, "-ERR: No such command: %s (see 'blacklist help')\n", argv[0]);
	}
	
done:
	switch_safe_free(data);
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_blacklist_load)
{
	switch_api_interface_t *api_interface;
	//switch_application_interface_t *app_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	switch_core_hash_init(&globals.files);
	switch_mutex_init(&globals.files_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	switch_core_hash_init(&globals.lists);
	switch_mutex_init(&globals.lists_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	do_config(SWITCH_FALSE);

	SWITCH_ADD_API(api_interface, "blacklist", "Control blacklists", blacklist_api_function, "");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_blacklist_shutdown)
{
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
