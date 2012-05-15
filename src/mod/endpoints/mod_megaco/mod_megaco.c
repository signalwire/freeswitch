/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_megaco.h"

struct megaco_globals megaco_globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_megaco_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_megaco_shutdown);
SWITCH_MODULE_DEFINITION(mod_megaco, mod_megaco_load, mod_megaco_shutdown, NULL);


#define MEGACO_FUNCTION_SYNTAX "profile [name] [start | stop]"
SWITCH_STANDARD_API(megaco_function)
{
	int argc;
	char *argv[10];
	char *dup = NULL;
	
	if (zstr(cmd)) {
		goto usage;
	}
	
	dup = strdup(cmd);
	argc = switch_split(dup, ' ', argv);
	
	if (argc < 1 || zstr(argv[0])) {
		goto usage;
	}
	
	if (!strcmp(argv[0], "profile")) {
		if (zstr(argv[1]) || zstr(argv[2])) {
			goto usage;
		}
		if (!strcmp(argv[2], "start")) {
			megaco_profile_t *profile = megaco_profile_locate(argv[1]);
			if (profile) {
				megaco_profile_release(profile);
				stream->write_function(stream, "-ERR Profile %s is already started\n", argv[2]);
			} else {
				megaco_profile_start(argv[1]);
				stream->write_function(stream, "+OK\n");
			}
		} else if (!strcmp(argv[2], "stop")) {
			megaco_profile_t *profile = megaco_profile_locate(argv[1]);
			if (profile) {
				megaco_profile_release(profile);
				megaco_profile_destroy(&profile);
				stream->write_function(stream, "+OK\n");
			} else {
				stream->write_function(stream, "-ERR No such profile\n");
			}
		}
	}
	
	goto done;
	
	usage:
		stream->write_function(stream, "-ERR Usage: "MEGACO_FUNCTION_SYNTAX"\n");
		
	done:
		switch_safe_free(dup);
		return SWITCH_STATUS_SUCCESS;
}

static switch_status_t console_complete_hashtable(switch_hash_t *hash, const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	for (hi = switch_hash_first(NULL, hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &vvar, NULL, &val);
		switch_console_push_match(&my_matches, (const char *) vvar);
	}

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

static switch_status_t list_profiles(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_status_t status;
	switch_thread_rwlock_rdlock(megaco_globals.profile_rwlock);
	status = console_complete_hashtable(megaco_globals.profile_hash, line, cursor, matches);
	switch_thread_rwlock_unlock(megaco_globals.profile_rwlock);
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_megaco_load)
{
	switch_api_interface_t *api_interface;
	
	memset(&megaco_globals, 0, sizeof(megaco_globals));
	megaco_globals.pool = pool;
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
		
	switch_core_hash_init(&megaco_globals.profile_hash, pool);
	switch_thread_rwlock_create(&megaco_globals.profile_rwlock, pool);
	
	SWITCH_ADD_API(api_interface, "megaco", "megaco", megaco_function, MEGACO_FUNCTION_SYNTAX);
	
	switch_console_set_complete("add megaco profile ::megaco::list_profiles start");
	switch_console_set_complete("add megaco profile ::megaco::list_profiles stop");
	switch_console_add_complete_func("::megaco::list_profiles", list_profiles);
	
	/* TODO: Kapil: Insert stack global startup code here */
	
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_megaco_shutdown)
{
	/* TODO: Kapil: Insert stack global shutdown code here */	
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
