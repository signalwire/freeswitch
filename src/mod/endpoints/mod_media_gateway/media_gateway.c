/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"
#include "media_gateway_stack.h"

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

megaco_profile_t*  megaco_get_profile_by_suId(SuId suId)
{
	megaco_profile_t*    profile = NULL;
	void 		*val = NULL;
	switch_hash_index_t *hi = NULL;
	int found = 0x00;
	const void *var;

	/*iterate through profile list to get requested suID profile */

    switch_thread_rwlock_rdlock(megaco_globals.profile_rwlock);
	for (hi = switch_hash_first(NULL, megaco_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		profile = (megaco_profile_t *) val;
		if (profile->idx == suId) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got profile[%s] associated with suId[%d]\n",profile->name, suId);
			found = 0x01;
			break;
		}
	}

	if(!found){
        profile = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Not able to find profile associated with suId[%d]\n",suId);
	}
    
    switch_thread_rwlock_unlock(megaco_globals.profile_rwlock);

	return profile;
}


static switch_status_t mg_on_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf, switch_dtmf_direction_t direction)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	mg_termination_t *term = switch_channel_get_private(channel, PVT_MG_TERM);
	//char digit[2] = { dtmf->digit };
	if (!term) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find termination structure for session [%s]\n",
			switch_core_session_get_uuid(session));
		return SWITCH_STATUS_SUCCESS;
	}
	
	//switch_status_t  mg_send_dtmf_notify(megaco_profile_t* mg_profile, const char* term_name, char* digits, int num_of_collected_digits);
	//mg_send_dtmf_notify(term->profile, term->name, digit, 1);
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MEGACO DTMF Signaling NOT Sending notify to MGC for dtmf:%c\n",dtmf->digit);
	return SWITCH_STATUS_SUCCESS;
}

/*
 * Creates a freeswitch channel for the specified termination. 
 * The channel will be parked until future actions are taken 
 */
switch_status_t megaco_activate_termination(mg_termination_t *term)
{
    switch_event_t *var_event = NULL;
    switch_core_session_t *session = NULL;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char dialstring[100];
    switch_call_cause_t cause;

    switch_event_create(&var_event, SWITCH_EVENT_CLONE);
    
    if (term->type == MG_TERM_RTP) {
        switch_snprintf(dialstring, sizeof dialstring, "rtp/%s", term->name);
        
        switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, kLOCALADDR, term->u.rtp.local_addr);
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kLOCALPORT, "%d", term->u.rtp.local_port);
        switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, kREMOTEADDR, term->u.rtp.remote_addr);
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kREMOTEPORT, "%d", term->u.rtp.remote_port);
        
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kPTIME, "%d", term->u.rtp.ptime);
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kPT, "%d", term->u.rtp.pt);
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kRFC2833PT, "%d", term->u.rtp.rfc2833_pt);
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kRATE, "%d", term->u.rtp.rate);
        switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, kCODEC, term->u.rtp.codec);
        
        switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, kMEDIATYPE, mg_media_type2str(term->u.rtp.media_type));
        switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "fax_enable_t38", "true");
        switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "rtp_execute_on_image", "t38_gateway self nocng");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s local_add[%s]\n",__FUNCTION__, term->u.rtp.local_addr); 
    } else if (term->type == MG_TERM_TDM) {
        switch_snprintf(dialstring, sizeof dialstring, "tdm/%s", term->name);
        
        switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, kSPAN_NAME,  term->u.tdm.span_name);
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kCHAN_ID, "%d", term->u.tdm.channel);
        switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, kPREBUFFER_LEN, "%d", term->profile->tdm_pre_buffer_size);
    }
    
    /* Set common variables on the channel */
    switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, SWITCH_PARK_AFTER_BRIDGE_VARIABLE, "true");
    
    if (!zstr(term->uuid)) {
        /* A UUID is present, check if the channel still exists */
        switch_core_session_t *session;
        if ((session = switch_core_session_locate(term->uuid))) {
            switch_channel_t *channel = switch_core_session_get_channel(session);
            switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "command", "media_modify");
            
            if (term->type == MG_TERM_RTP) {
                if (term->u.rtp.t38_options) {
                    switch_channel_set_private(channel, "t38_options", term->u.rtp.t38_options);
                }
                
                if (term->u.rtp.media_type == MGM_IMAGE) {
                    mg_term_set_pre_buffer_size(term, 0);
                }
            }
            
            switch_core_session_receive_event(session, &var_event);

            switch_core_session_rwunlock(session);
            
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sent refresh to channel [%s], for termination [%s]\n", term->uuid, term->name);
            
            return SWITCH_STATUS_SUCCESS;
        }
        
        /* The referenced channel doesn't exist anymore, clear it */
        term->uuid = NULL;
    }
    
	if (zstr(term->uuid)) {    
		switch_channel_t *channel;
		if (switch_ivr_originate(NULL, &session, &cause, dialstring, 0, NULL, NULL, NULL, NULL, var_event, 0, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to instanciate termination [%s]: %s\n", term->name, switch_channel_cause2str(cause));   
			status = SWITCH_STATUS_FALSE;
			goto done;
		}

		term->uuid = switch_core_strdup(term->pool, switch_core_session_get_uuid(session));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Termination [%s] successfully instanciated as [%s] [%s]\n", term->name, dialstring, switch_core_session_get_uuid(session));   
		channel = switch_core_session_get_channel(session);
		switch_channel_set_private(channel, PVT_MG_TERM, term);

		if (term->type == MG_TERM_RTP && term->u.rtp.t38_options) {
			switch_channel_set_private(channel, "t38_options", term->u.rtp.t38_options);
		}

		switch_core_event_hook_add_recv_dtmf(session, mg_on_dtmf);
		if (term->type == MG_TERM_TDM){
			switch_core_session_execute_application_async(session, "spandsp_start_fax_detect", "mg_notify ced 120 ced");
			switch_core_session_execute_application_async(session, "spandsp_start_fax_detect", "mg_notify cng 120 cng");
		}

	}
    
    switch_set_flag(term, MGT_ACTIVE);
    
done:
    if (session) {
        switch_core_session_rwunlock(session);
    }
    switch_event_destroy(&var_event);

    return status;
}

switch_status_t megaco_prepare_tdm_termination(mg_termination_t *term)
{
	switch_event_t *event = NULL;
	if (switch_event_create(&event, SWITCH_EVENT_TRAP) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to create NOTIFY event\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "span-name", "%s",  term->u.tdm.span_name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "chan-number", "%d", term->u.tdm.channel);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "condition", "mg-tdm-prepare");

	switch_event_fire(&event);
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t megaco_check_tdm_termination(mg_termination_t *term)
{
	switch_event_t *event = NULL;

	if(!term || !term->profile) return SWITCH_STATUS_FALSE;

	if (switch_event_create(&event, SWITCH_EVENT_TRAP) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to create NOTIFY event\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "span-name", "%s",  term->u.tdm.span_name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "chan-number", "%d", term->u.tdm.channel);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "condition", "mg-tdm-check");

	switch_event_fire(&event);
	return SWITCH_STATUS_SUCCESS;
}

mg_termination_t *megaco_choose_termination(megaco_profile_t *profile, const char *prefix)
{
    mg_termination_type_t termtype;
    switch_memory_pool_t *pool;
    mg_termination_t *term = NULL;
    char name[100];
    int term_id;
    size_t prefixlen = strlen(prefix);
    
    /* Check the termination type by prefix */
    if (strncasecmp(prefix, profile->rtp_termination_id_prefix, strlen(profile->rtp_termination_id_prefix)) == 0) {
        termtype = MG_TERM_RTP;
        term_id = mg_rtp_request_id(profile);
        switch_snprintf(name, sizeof name, "%s/%d", profile->rtp_termination_id_prefix, term_id);
    } else {
        for (term = profile->physical_terminations; term; term = term->next) {
            if (!switch_test_flag(term, MGT_ALLOCATED) && !strncasecmp(prefix, term->name, prefixlen)) {
                switch_set_flag(term, MGT_ALLOCATED);
                return term;
            }
        }
        
        return NULL;
    }
    
    switch_core_new_memory_pool(&pool);
    term = switch_core_alloc(pool, sizeof *term);
    term->pool = pool;
    term->type = termtype;
    term->active_events = NULL;
    term->profile = profile;
    switch_set_flag(term, MGT_ALLOCATED);
    
    if (termtype == MG_TERM_RTP) {
        /* Fill in local address and reserve an rtp port */
        //term->u.rtp.local_addr = profile->my_ipaddr;
        term->u.rtp.local_addr = profile->rtp_ipaddr;
        term->u.rtp.local_port = switch_rtp_request_port(term->u.rtp.local_addr);
        term->u.rtp.codec = megaco_codec_str(profile->default_codec);
        term->u.rtp.term_id = term_id;
        term->u.rtp.ptime = 20;
        term->name = switch_core_strdup(term->pool, name);
    }
    
    switch_core_hash_insert_wrlock(profile->terminations, term->name, term, profile->terminations_rwlock);
    
    return term;
}

mg_termination_t *megaco_term_locate_by_span_chan_id(const char *span_name, const char *chan_number)
{
	void                	*val = NULL;
	switch_hash_index_t 	*hi = NULL;
	mg_termination_t 	*term = NULL;
	megaco_profile_t 	*profile = NULL;
	const void 		*var;

	switch_assert(span_name);
	switch_assert(chan_number);

	/* span + chan will be unique across all the mg_profiles  *
	 * loop through all profiles and then all terminations to *
	 * get the mg termination associated with input span+chan */

	switch_thread_rwlock_rdlock(megaco_globals.profile_rwlock);
	for (hi = switch_hash_first(NULL, megaco_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		profile = (megaco_profile_t *) val;

		if(NULL != (term = megaco_find_termination_by_span_chan(profile, span_name, chan_number))) break;
	}
	switch_thread_rwlock_unlock(megaco_globals.profile_rwlock);

	return term;
}

mg_termination_t* megaco_find_termination_by_span_chan(megaco_profile_t *profile, const char *span_name, const char *chan_number)
{
	void                *val = NULL;
	switch_hash_index_t *hi = NULL;
	mg_termination_t *term = NULL;
	int found = 0x00;
	const void *var;

	if(!span_name || !chan_number){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Invalid span_name/chan_number \n");
		return NULL;
	}

	for (hi = switch_hash_first(NULL, profile->terminations); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		term = (mg_termination_t *) val;
		if(!term) continue;
		if(MG_TERM_TDM != term->type) continue;

		if ((!strcasecmp(span_name, term->u.tdm.span_name))&& (atoi(chan_number) == term->u.tdm.channel)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					"Got term[%s] associated with span[%s], channel[%s] in MG profile[%s]\n",
					 term->name, span_name, chan_number, profile->name);
			found = 0x01;
			break;
		}
	}

	if(!found){
		term = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				" MG profile[%s] does not have termination associated with span[%s], channel[%s]\n", 
				 profile->name, span_name, chan_number); 
	}

	return term;
}

mg_termination_t *megaco_find_termination(megaco_profile_t *profile, const char *name)
{
    mg_termination_t *term = switch_core_hash_find_rdlock(profile->terminations, name, profile->terminations_rwlock);
    return term;

}

void megaco_termination_destroy(mg_termination_t *term)
{
    /* Lookup the FS session and hang it up */
    switch_core_session_t *session;
    switch_channel_t *channel;
    
    if ((session = switch_core_session_locate(term->uuid))) {
        channel = switch_core_session_get_channel(session);
        switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
        switch_core_session_rwunlock(session);
        term->uuid = NULL;
    }
    
    if (term->type == MG_TERM_RTP ){
	    if(term->u.rtp.local_port != 0) {
		    switch_rtp_release_port(term->u.rtp.local_addr, term->u.rtp.local_port);
	    }
	    if(term->u.rtp.term_id != 0) {
		    mg_rtp_release_id(term->profile,term->u.rtp.term_id);
	    }
    }

    if(term->active_events){
	    mgUtlDelMgMgcoReqEvtDesc(term->active_events);
	    MG_STACK_MEM_FREE(term->active_events, sizeof(MgMgcoReqEvtDesc));
    }
    term->context = NULL;


    switch_clear_flag(term, MGT_ALLOCATED);
    switch_clear_flag(term, MGT_ACTIVE);
    switch_clear_flag(term, MG_FAX_NOTIFIED);
    
    if (term->type == MG_TERM_RTP) {
        switch_core_hash_delete_wrlock(term->profile->terminations, term->name, term->profile->terminations_rwlock);
        switch_core_destroy_memory_pool(&term->pool);   
    }
}

switch_status_t megaco_context_is_term_present(mg_context_t *ctx, mg_termination_t *term)
{

    switch_assert(ctx != NULL);
    switch_assert(term != NULL);

    if (ctx->terminations[0] && (term == ctx->terminations[0])) {
        return SWITCH_STATUS_SUCCESS;
    }

    if (ctx->terminations[1] && (term == ctx->terminations[1])) {
        return SWITCH_STATUS_SUCCESS;
    }

    return SWITCH_STATUS_FALSE;
}

switch_status_t megaco_context_add_termination(mg_context_t *ctx, mg_termination_t *term)
{
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    switch_assert(ctx != NULL);
    switch_assert(term != NULL);
    
    /* Check if the current context has existing terminations */
    if (ctx->terminations[0] && ctx->terminations[1]) {
        /* Context is full */
        return SWITCH_STATUS_FALSE;
    }
    
    term->context = ctx;
    if (ctx->terminations[0]) {
        ctx->terminations[1] = term;
    } else if (ctx->terminations[1]) {
        ctx->terminations[0] = term;
    } else {
        ctx->terminations[0] = term;
    }
    
    if (ctx->terminations[0] && ctx->terminations[1]) {
        if (zstr(ctx->terminations[0]->uuid)) {
            if(SWITCH_STATUS_SUCCESS != (status = megaco_activate_termination(ctx->terminations[0]))){
                return status;
            }
        }
        if (zstr(ctx->terminations[1]->uuid)) {
            if(SWITCH_STATUS_SUCCESS != (status = megaco_activate_termination(ctx->terminations[1]))){
                return status;
            }
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Bridging: %s (%s) <> %s (%s)\n", 
                          ctx->terminations[0]->name, ctx->terminations[0]->uuid,
                          ctx->terminations[1]->name, ctx->terminations[1]->uuid);
        
        switch_ivr_uuid_bridge(ctx->terminations[0]->uuid, ctx->terminations[1]->uuid);

         ctx->terminations[0]->profile->mg_stats->total_num_of_call_recvd++;
    }

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t megaco_context_sub_all_termination(mg_context_t *ctx)
{
    switch_assert(ctx != NULL);
    
    /* Channels will automatically go to park once the bridge ends */
    if (ctx->terminations[0]) {
        megaco_context_sub_termination(ctx, ctx->terminations[0]);
    }
    
    if (ctx->terminations[1]) {
        megaco_context_sub_termination(ctx, ctx->terminations[1]);
    }
    
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t megaco_context_sub_termination(mg_context_t *ctx, mg_termination_t *term)
{
    switch_assert(ctx != NULL);
    switch_assert(term != NULL);
    
    /* Channels will automatically go to park once the bridge ends */
    if (ctx->terminations[0] == term) {
        ctx->terminations[0] = NULL;
    } else if (ctx->terminations[1] == term) {
        ctx->terminations[1] = NULL;
    }
    
    megaco_termination_destroy(term);

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t megaco_context_move_termination(mg_context_t *dst, mg_termination_t *term) 
{

        return SWITCH_STATUS_SUCCESS;
}

mg_context_t *megaco_find_context_by_suid(SuId suId, uint32_t context_id)
{
    megaco_profile_t*    profile = NULL;
    
    if(NULL == (profile = megaco_get_profile_by_suId(suId))){
	    return NULL;
    }
    
    return megaco_get_context(profile, context_id);
}

mg_context_t *megaco_get_context(megaco_profile_t *profile, uint32_t context_id)
{
    mg_context_t *result = NULL;
    
    if (context_id > MG_MAX_CONTEXTS) {
        return NULL;
    }

    switch_thread_rwlock_rdlock(profile->contexts_rwlock);
    
    /* Context exists */
    if (profile->contexts_bitmap[context_id / 8] & (1 << (context_id % 8))) {
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
    mg_context_t *ctx=NULL;
    
    switch_thread_rwlock_wrlock(profile->contexts_rwlock);
    /* Try the next one */
    if (profile->next_context_id >= MG_MAX_CONTEXTS) {
        profile->next_context_id = 1;
    }
    
    /* Look for an available context */
    for (; profile->next_context_id < MG_MAX_CONTEXTS; profile->next_context_id++) {
        if ((profile->contexts_bitmap[profile->next_context_id / 8] & (1 << (profile->next_context_id % 8))) == 0) {
            /* Found! */
            int i = profile->next_context_id % MG_CONTEXT_MODULO;
            profile->contexts_bitmap[profile->next_context_id / 8] |= 1 << (profile->next_context_id % 8);
            ctx = malloc(sizeof *ctx);
            memset(ctx, 0, sizeof *ctx);
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
    
    profile->contexts_bitmap[context_id / 8] &= ~(1 << (context_id % 8));
    
    memset(ctx, 0, sizeof *ctx);
    free(ctx);
    
    switch_thread_rwlock_unlock(profile->contexts_rwlock);
}

uint32_t mg_rtp_request_id(megaco_profile_t *profile)
{
    uint32_t rtp_id = 0x00;

    if (profile->rtpid_next >= MG_MAX_RTPID || profile->rtpid_next == 0) {
        profile->rtpid_next = 1;
    }

    for (; profile->rtpid_next < MG_MAX_RTPID; profile->rtpid_next++) {
        if ((profile->rtpid_bitmap[profile->rtpid_next / 8] & (1 << (profile->rtpid_next % 8))) == 0) {
            profile->rtpid_bitmap[profile->rtpid_next / 8] |= 1 << (profile->rtpid_next % 8);
            rtp_id = profile->rtpid_next;
	    profile->rtpid_next++;
            return rtp_id;
        }
    }
    
    return 0;
}

void mg_rtp_release_id(megaco_profile_t *profile, uint32_t id)
{
    profile->rtpid_bitmap[id / 8] &= ~(1 << (id % 8));
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
	profile->physical_terminations = NULL;
	profile->name = switch_core_strdup(pool, profilename);
	profile->next_context_id++;
	profile->inact_tmr = 0x00;
	profile->total_cfg_term = 0x00;
	profile->peer_active = 0x00;
	profile->mg_stats = switch_core_alloc(pool, sizeof(mg_stats_t));
	profile->inact_tmr_task_id = 0x00;
	
	switch_thread_rwlock_create(&profile->rwlock, pool);
    
    switch_thread_rwlock_create(&profile->contexts_rwlock, pool);
    switch_thread_rwlock_create(&profile->terminations_rwlock, pool);

    switch_core_hash_init(&profile->terminations, pool);
    
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


switch_status_t mgco_init_ins_service_change(SuId suId)
{
    megaco_profile_t*    profile = NULL;
    void                *val = NULL;
    const void  *key = NULL;
    switch_ssize_t   keylen;
    switch_hash_index_t *hi = NULL;
    mg_termination_t *term = NULL;


    if(NULL == (profile = megaco_get_profile_by_suId(suId))){
        return SWITCH_STATUS_FALSE;
    }

    profile->peer_active = 0x01;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
            "mgco_init_ins_service_change : Initiating terminations service change for profile: %s\n", profile->name);

    /* loop through all termination and post get status Event  */
    for (hi = switch_hash_first(NULL,  profile->terminations); hi; hi = switch_hash_next(hi)) {
        switch_hash_this(hi, &key, &keylen, &val);
        term = (mg_termination_t *) val;
		if(!term) continue;
		if(MG_TERM_RTP == term->type) continue;
		megaco_check_tdm_termination(term);
    }

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

void mg_term_set_pre_buffer_size(mg_termination_t *term, int newval) 
{
    switch_event_t *event = NULL, *event2 = NULL;
    switch_core_session_t *session, *session2;
    
    if (!zstr(term->uuid) && (session = switch_core_session_locate(term->uuid))) {
        switch_event_create(&event, SWITCH_EVENT_CLONE);
        
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "command", kPREBUFFER_LEN);
        switch_event_add_header(event, SWITCH_STACK_BOTTOM, kPREBUFFER_LEN, "%d", newval);
    
        /* Propagate event to bridged session if there is one */
        if (switch_core_session_get_partner(session, &session2) == SWITCH_STATUS_SUCCESS) {
            switch_event_dup(&event2, event);
            switch_core_session_receive_event(session2, &event2);
            switch_core_session_rwunlock(session2);
        }
        
        switch_core_session_receive_event(session, &event);
        switch_core_session_rwunlock(session);
        
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sent prebuffer_size event to [%s] to [%d ms]\n", term->uuid, newval);
    }
    
    switch_event_destroy(&event);
}


void mg_term_set_ec(mg_termination_t *term, int enable) 
{
    switch_event_t *event = NULL, *event2 = NULL;
    switch_core_session_t *session, *session2;
    
    if (!zstr(term->uuid) && (session = switch_core_session_locate(term->uuid))) {
        switch_event_create(&event, SWITCH_EVENT_CLONE);
        
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "command", kECHOCANCEL);
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, kECHOCANCEL, enable ? "true" : "false");
    
        /* Propagate event to bridged session if there is one */
        if (switch_core_session_get_partner(session, &session2) == SWITCH_STATUS_SUCCESS) {
            switch_event_dup(&event2, event);
            switch_core_session_receive_event(session2, &event2);
            switch_core_session_rwunlock(session2);
        }
        
        switch_core_session_receive_event(session, &event);
        switch_core_session_rwunlock(session);
        
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sent echo_cancel event to [%s] to [%s]\n", term->uuid, enable ? "enable" : "disable");
    }
    
    switch_event_destroy(&event);
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
