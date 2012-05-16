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
	switch_xml_t cfg, xml, mg_interfaces, mg_interface, tpt_interfaces, tpt_interface, peer_interfaces, peer_interface;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;
	const char *file = "megaco.conf";
	const char* mg_profile_tpt_id = NULL;
	const char* mg_profile_peer_id = NULL;

	if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
		goto done;
	}

	if (!(mg_interfaces = switch_xml_child(cfg, "sng_mg_interfaces"))) {
		goto done;
	}

	/* iterate through MG Interface list to build all MG profiles */
	for (mg_interface = switch_xml_child(mg_interfaces, "sng_mg_interface"); mg_interface; mg_interface = mg_interface->next) {

		const char *name = switch_xml_attr_soft(mg_interface, "name");
		if (strcmp(name, profile->name)) {
			continue;
		}

		/* parse MG profile */
		if(SWITCH_STATUS_FALSE == sng_parse_mg_profile(mg_interface)) {
			goto done;
		}

		mg_profile_tpt_id = switch_xml_attr_soft(mg_interface, "id");

		/* Now get required transport profile against mg_profile_tpt_id*/
		if (!(tpt_interfaces = switch_xml_child(cfg, "sng_transport_interfaces"))) {
			goto done;
		}

		for (tpt_interface = switch_xml_child(tpt_interfaces, "sng_transport_interface"); tpt_interface; tpt_interface = tpt_interface->next) {
			const char *id = switch_xml_attr_soft(tpt_interface, "id");
			if (strcmp(id, mg_profile_tpt_id)) {
				continue;
			}

			/* parse MG transport profile */
			if(SWITCH_STATUS_FALSE == sng_parse_mg_tpt_profile(tpt_interface)) {
				goto done;
			}
		}

		/* as of now supporting only one peer */
		mg_profile_peer_id = switch_xml_attr_soft(mg_interface, "peerId");
		/* Now get required peer profile against mg_profile_peer_id*/
		if (!(peer_interfaces = switch_xml_child(cfg, "sng_mg_peer_interfaces"))) {
			goto done;
		}

		for (peer_interface = switch_xml_child(peer_interfaces, "sng_mg_peer_interface"); peer_interface; peer_interface = peer_interface->next) {
			const char *id = switch_xml_attr_soft(peer_interface, "id");
			if (strcmp(id, mg_profile_peer_id)) {
				continue;
			}

			/* parse MG Peer profile */
			if(SWITCH_STATUS_FALSE == sng_parse_mg_peer_profile(peer_interface)) {
				goto done;
			}
		}

		status = SWITCH_STATUS_SUCCESS;
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
	
	if (config_profile(profile, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error configuring profile %s\n", profile->name);
		goto fail;
	}
	
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
