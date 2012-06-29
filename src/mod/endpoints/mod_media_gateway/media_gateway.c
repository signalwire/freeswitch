/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"

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

mg_peer_profile_t *megaco_peer_profile_locate(const char *name)
{
	mg_peer_profile_t *profile = switch_core_hash_find_rdlock(megaco_globals.peer_profile_hash, name, megaco_globals.peer_profile_rwlock);

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

void megaco_peer_profile_release(mg_peer_profile_t *profile) 
{
	switch_thread_rwlock_unlock(profile->rwlock);
}

mg_context_t *megaco_get_context(megaco_profile_t *profile, uint32_t context_id)
{
    mg_context_t *result = NULL;
    
    if (context_id > MG_MAX_CONTEXTS) {
        return NULL;
    }
    
    switch_thread_rwlock_rdlock(profile->contexts_rwlock);
    
    /* Context exists */
    if (profile->contexts_bitmap[context_id % 8] & (1 << (context_id / 8))) {
        for (result = profile->contexts[context_id % MG_CONTEXT_MODULO]; result; result = result->next) {
            if (result->context_id == context_id) {
                break;
            }
        }
    }
    
    switch_thread_rwlock_unlock(profile->contexts_rwlock);
    
    return result;
}

/* Returns a fresh new context */
mg_context_t *megaco_choose_context(megaco_profile_t *profile)
{
    mg_context_t *ctx;
    
    switch_thread_rwlock_wrlock(profile->contexts_rwlock);
    /* Try the next one */
    if (profile->next_context_id >= MG_MAX_CONTEXTS) {
        profile->next_context_id = 1;
    }
    
    /* Look for an available context */
    for (; profile->next_context_id < MG_MAX_CONTEXTS; profile->next_context_id++) {
        if ((profile->contexts_bitmap[profile->next_context_id % 8] & (1 << (profile->next_context_id / 8))) == 0) {
            /* Found! */
            profile->contexts_bitmap[profile->next_context_id % 8] |= 1 << (profile->next_context_id / 8);
            int i = profile->next_context_id % MG_CONTEXT_MODULO;
            ctx = malloc(sizeof *ctx);
            ctx->context_id = profile->next_context_id;
            ctx->profile = profile;
            
            if (!profile->contexts[i]) {
                profile->contexts[i] = ctx;
            } else {
                mg_context_t *it;
                for (it = profile->contexts[i]; it && it->next; it = it->next)
                    ;
                it->next = ctx;
            }
            
            profile->next_context_id++;
            break;
        }
    }
    
    switch_thread_rwlock_unlock(profile->contexts_rwlock);
    
    return ctx;
}

void megaco_release_context(mg_context_t *ctx)
{
    uint32_t context_id = ctx->context_id;
    megaco_profile_t *profile = ctx->profile;
    int i = context_id % MG_CONTEXT_MODULO;
    
    switch_thread_rwlock_wrlock(profile->contexts_rwlock);
    if (profile->contexts[i] == ctx) {
        profile->contexts[i] = ctx->next;
    } else {
        mg_context_t *it = profile->contexts[i]->next, *prev = profile->contexts[i];
        for (; it; prev = it, it = it->next) {
            if (it == ctx) {
                prev->next = it->next;
                break;
            }
        }
    }
    
    profile->contexts_bitmap[context_id % 8] &= ~(1 << (context_id / 8));
    
    memset(ctx, 0, sizeof *ctx);
    free(ctx);
    
    switch_thread_rwlock_unlock(profile->contexts_rwlock);
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
    profile->next_context_id++;
	
	switch_thread_rwlock_create(&profile->rwlock, pool);
    
    switch_thread_rwlock_create(&profile->contexts_rwlock, pool);

//    switch_core_hash_init(&profile->contexts_hash, pool);
    
	if (SWITCH_STATUS_SUCCESS != config_profile(profile, SWITCH_FALSE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error configuring profile %s\n", profile->name);
		goto fail;
	}
	
	if(SWITCH_STATUS_FALSE == sng_mgco_start(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error starting MEGACO Stack for profile  %s\n", profile->name);
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
	
	
	if(SWITCH_STATUS_FALSE == sng_mgco_stop((*profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error stopping MEGACO Stack for profile  %s\n", (*profile)->name); 
	}
    
    /* TODO: Cleanup contexts */

	switch_thread_rwlock_unlock((*profile)->rwlock);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stopped profile: %s\n", (*profile)->name);	
	switch_core_hash_delete_wrlock(megaco_globals.profile_hash, (*profile)->name, megaco_globals.profile_rwlock);
	
	mg_config_cleanup(*profile);

	switch_core_destroy_memory_pool(&(*profile)->pool);
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t megaco_peer_profile_destroy(mg_peer_profile_t **profile) 
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stopping peer profile: %s\n", (*profile)->name);	
	
	switch_core_hash_delete_wrlock(megaco_globals.peer_profile_hash, (*profile)->name, megaco_globals.peer_profile_rwlock);
	
	mg_peer_config_cleanup(*profile);

	switch_core_destroy_memory_pool(&(*profile)->pool);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stopped peer profile: %s\n", (*profile)->name);	
	
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
