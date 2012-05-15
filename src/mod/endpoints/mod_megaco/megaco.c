/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_megaco.h"

megaco_profile_t *megaco_profile_locate(const char *name) 
{
	megaco_profile_t *profile = switch_core_hash_find_rdlock(megaco_globals.profile_hash, name, megaco_globals.profile_rwlock);

	if (profile) {
		if (switch_thread_rwlock_tryrdlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile %s is locked\n", name);
			profile = NULL;
		}
	}

	return profile;
}

void megaco_profile_release(megaco_profile_t *profile) 
{
	switch_thread_rwlock_unlock(profile->rwlock);
}

static switch_status_t config_profile(megaco_profile_t *profile, switch_bool_t reload)
{
	switch_xml_t cfg, xml, x_profiles, x_profile, x_settings;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;
	int count;
	const char *file = "megaco.conf";

	if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
		goto done;
	}

	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto done;
	}

	for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
		const char *name = switch_xml_attr_soft(x_profile, "name");
		if (strcmp(name, profile->name)) {
			continue;
		}

		if (!(x_settings = switch_xml_child(x_profile, "settings"))) {
			goto done;
		}
		count = switch_event_import_xml(switch_xml_child(x_settings, "param"), "name", "value", &event);
		// status = switch_xml_config_parse_event(event, count, reload, instructions);
		
		/* TODO: Initialize stack configuration */
	}

done:
	if (xml) {
		switch_xml_free(xml);	
	}

	if (event) {
		switch_event_destroy(&event);
	}
	return status;
}

switch_status_t megaco_profile_start(const char *profilename)
{
	switch_memory_pool_t *pool;
	megaco_profile_t *profile;
	
	switch_assert(profilename);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Starting profile: %s\n", profilename);
	
	switch_core_new_memory_pool(&pool);
	profile = switch_core_alloc(pool, sizeof(*profile));
	profile->pool = pool;
	profile->name = switch_core_strdup(pool, profilename);
	
	switch_thread_rwlock_create(&profile->rwlock, pool);
	
	/* TODO: Kapil: Insert stack per-interface startup code here */
	
	switch_core_hash_insert_wrlock(megaco_globals.profile_hash, profile->name, profile, megaco_globals.profile_rwlock);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Started profile: %s\n", profile->name);
	
	return SWITCH_STATUS_SUCCESS;
fail:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_FALSE;	
}


switch_status_t megaco_profile_destroy(megaco_profile_t **profile) 
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stopping profile: %s\n", (*profile)->name);	
	switch_thread_rwlock_wrlock((*profile)->rwlock);
	
	
	/* TODO: Kapil: Insert stack per-interface shutdown code here */

	
	switch_thread_rwlock_unlock((*profile)->rwlock);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stopped profile: %s\n", (*profile)->name);	
	switch_core_hash_delete_wrlock(megaco_globals.profile_hash, (*profile)->name, megaco_globals.profile_rwlock);
	
	switch_core_destroy_memory_pool(&(*profile)->pool);
	
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
