/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"
#include "media_gateway_stack.h"


/**************************************************************************************************************/
struct megaco_globals megaco_globals;
static sng_mg_event_interface_t sng_event;

/**************************************************************************************************************/
SWITCH_MODULE_LOAD_FUNCTION(mod_media_gateway_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_media_gateway_shutdown);
SWITCH_MODULE_DEFINITION(mod_media_gateway, mod_media_gateway_load, mod_media_gateway_shutdown, NULL);

/**************************************************************************************************************/

SWITCH_STANDARD_API(megaco_function)
{
	return mg_process_cli_cmd(cmd, stream);
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

static void mg_event_handler(switch_event_t *event)
{
	switch(event->event_id) {
		case SWITCH_EVENT_TRAP:
			{
				const char *span_name = NULL;
				const char *chan_number = NULL;
				const char *cond = NULL;

				cond = switch_event_get_header(event, "condition");
				if (zstr(cond)) {
					return;
				}

				span_name   = switch_event_get_header(event, "span-name");
				chan_number = switch_event_get_header(event, "chan-number");
				
				if (!strcmp(cond, "ftdm-alarm-trap")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					 "ftdm-alarm-trap for span_name[%s] chan_number[%s]\n", span_name,chan_number);
					mg_send_term_service_change((char*)span_name, (char*)chan_number, MG_TERM_SERVICE_STATE_OUT_OF_SERVICE);
				} else if (!strcmp(cond, "ftdm-alarm-clear")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					 "ftdm-alarm-clear for span_name[%s] chan_number[%s] \n", span_name,chan_number);
					mg_send_term_service_change( (char*)span_name, (char*)chan_number, MG_TERM_SERVICE_STATE_IN_SERVICE);
				}
			}
		break;
		default:
			break;
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_media_gateway_load)
{
	switch_api_interface_t *api_interface;
	
	memset(&megaco_globals, 0, sizeof(megaco_globals));
	megaco_globals.pool = pool;
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
		
	switch_core_hash_init(&megaco_globals.profile_hash, pool);
	switch_thread_rwlock_create(&megaco_globals.profile_rwlock, pool);

	switch_core_hash_init(&megaco_globals.peer_profile_hash, pool);
	switch_thread_rwlock_create(&megaco_globals.peer_profile_rwlock, pool);
	
	SWITCH_ADD_API(api_interface, "mg", "media_gateway", megaco_function, MEGACO_FUNCTION_SYNTAX);
	
	switch_console_set_complete("add mg profile ::mg::list_profiles start");
	switch_console_set_complete("add mg profile ::mg::list_profiles stop");
	switch_console_set_complete("add mg profile ::mg::list_profiles status");
	switch_console_set_complete("add mg profile ::mg::list_profiles xmlstatus");
	switch_console_set_complete("add mg profile ::mg::list_profiles peerxmlstatus");
	switch_console_set_complete("add mg logging ::mg::list_profiles enable");
	switch_console_set_complete("add mg logging ::mg::list_profiles disable");
	switch_console_add_complete_func("::mg::list_profiles", list_profiles);

	/* Initialize MEGACO Stack */
	sng_event.mg.sng_mgco_txn_ind  		= handle_mgco_txn_ind;
	sng_event.mg.sng_mgco_cmd_ind  		= handle_mgco_cmd_ind;
	sng_event.mg.sng_mgco_txn_sta_ind  	= handle_mgco_txn_sta_ind;
	sng_event.mg.sng_mgco_sta_ind  		= handle_mgco_sta_ind;
	sng_event.mg.sng_mgco_cntrl_cfm  	= handle_mgco_cntrl_cfm;
	sng_event.mg.sng_mgco_audit_cfm  	= handle_mgco_audit_cfm;
	/* Alarm CB */
	sng_event.sm.sng_mg_alarm  		= handle_mg_alarm;
	sng_event.sm.sng_tucl_alarm  		= handle_tucl_alarm;
	/* Log */
	sng_event.sm.sng_log  			= handle_sng_log;

	switch_event_bind("mod_media_gateway", SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, mg_event_handler, NULL);

	/* initualize MEGACO stack */
	return sng_mgco_init(&sng_event);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_media_gateway_shutdown)
{
    void 		*val = NULL;
    const void 	*key = NULL;
    switch_ssize_t   keylen;
    switch_hash_index_t *hi = NULL;
    megaco_profile_t*    profile = NULL;
    mg_peer_profile_t*    peer_profile = NULL;

    /* destroy all the mg profiles */
    while ((hi = switch_hash_first(NULL, megaco_globals.profile_hash))) {
        switch_hash_this(hi, &key, &keylen, &val);
        profile = (megaco_profile_t *) val;
        if(profile->inact_tmr_task_id){
            switch_scheduler_del_task_id(profile->inact_tmr_task_id);
            profile->inact_tmr_task_id = 0x00;
        }
        megaco_profile_destroy(&profile);
        profile = NULL;
    }

    hi = NULL;
    key = NULL;
    val = NULL;
    /* destroy all the mg peer profiles */
    while ((hi = switch_hash_first(NULL, megaco_globals.peer_profile_hash))) {
        switch_hash_this(hi, &key, &keylen, &val);
        peer_profile = (mg_peer_profile_t *) val;
        megaco_peer_profile_destroy(&peer_profile);
        peer_profile = NULL;
    }

    sng_mgco_stack_shutdown();


    return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************************/
void handle_sng_log(uint8_t level, char *fmt, ...)
{
	int log_level;
	char print_buf[1024];
	va_list ptr;

	memset(&print_buf[0],0,sizeof(1024));

	va_start(ptr, fmt);

	switch(level)
	{
		case SNG_LOGLEVEL_DEBUG:    log_level = SWITCH_LOG_DEBUG;       break;
		case SNG_LOGLEVEL_INFO:     log_level = SWITCH_LOG_INFO;        break;
		case SNG_LOGLEVEL_WARN:     log_level = SWITCH_LOG_WARNING;     break;
		case SNG_LOGLEVEL_ERROR:    log_level = SWITCH_LOG_ERROR;       break;
		case SNG_LOGLEVEL_CRIT:     log_level = SWITCH_LOG_CRIT;        break;
		default:                    log_level = SWITCH_LOG_DEBUG;       break;
	};

	vsprintf(&print_buf[0], fmt, ptr);

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, log_level, " MOD_MEGACO: %s \n", &print_buf[0]); 

	va_end(ptr);
}
/*****************************************************************************************************************************/

#if 0
static switch_status_t mgco_parse_local_sdp(mg_termination_t *term, CmSdpInfoSet *sdp)
{
    int i;
    CmSdpInfoSet *local_sdp;
    /* Parse the local SDP while copying the important bits over to our local structure,
     * while taking care of editing choose request and replacing them by real values */
    
    if (!term->u.rtp.local_sdp) {
        local_sdp = term->u.rtp.local_sdp = switch_core_alloc(term->context->pool, sizeof *term->u.rtp.local_sdp);
    }
    
    
    if (sdp->numComp.pres == NOTPRSNT) {
        return SWITCH_STATUS_FALSE;
    }
    
    for (i = 0; i < sdp->numComp.val; i++) {
        CmSdpInfo *s = sdp->info[i];
        int mediaId;
        
        local_sdp->info[i] = switch_core_alloc(term->context->pool, sizeof *(local_sdp->info[i]));
        *(local_sdp->info[i]) = *(sdp->info[i]);
        
        if (s->conn.addrType.pres && s->conn.addrType.val == CM_SDP_ADDR_TYPE_IPV4 &&
            s->conn.netType.type.val == CM_SDP_NET_TYPE_IN &&
            s->conn.u.ip4.addrType.val == CM_SDP_IPV4_IP_UNI) {
            
            if (s->conn.u.ip4.addrType.pres) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Local address: %d.%d.%d.%d\n",
                                  s->conn.u.ip4.u.uniIp.b[0].val,
                                  s->conn.u.ip4.u.uniIp.b[1].val,
                                  s->conn.u.ip4.u.uniIp.b[2].val,
                                  s->conn.u.ip4.u.uniIp.b[3].val);
                
                /* TODO: Double-check bind address for this profile */
                
            }
            if (s->attrSet.numComp.pres) {
                for (mediaId = 0; mediaId < s->attrSet.numComp.val; mediaId++) {
                    CmSdpAttr *a = s->attrSet.attr[mediaId];
                    local_sdp->info[i]->attrSet.attr[mediaId] = switch_core_alloc(term->context->pool, sizeof(CmSdpAttr));
                    *(local_sdp->info[i]->attrSet.attr[mediaId]) = *a;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Media %p\n", (void*)a);
                }
            }
            
            if (s->mediaDescSet.numComp.pres) {
                for (mediaId = 0; mediaId < s->mediaDescSet.numComp.val; mediaId++) {
                    CmSdpMediaDesc *desc = s->mediaDescSet.mediaDesc[mediaId];
                    local_sdp->info[i]->mediaDescSet.mediaDesc[mediaId] = switch_core_alloc(term->context->pool, sizeof(CmSdpMediaDesc));
                    *(local_sdp->info[i]->mediaDescSet.mediaDesc[mediaId]) = *desc;
                    
                    if (desc->field.mediaType.val == CM_SDP_MEDIA_AUDIO &&
                        desc->field.id.type.val ==  CM_SDP_VCID_PORT &&
                        desc->field.id.u.port.type.val == CM_SDP_PORT_INT &&
                        desc->field.id.u.port.u.portInt.port.type.val == CM_SDP_SPEC) {
                        int port = desc->field.id.u.port.u.portInt.port.val.val;
                        
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Port: %d\n", port);
                        
                    }
                }
            }
        }
    }

    return SWITCH_STATUS_SUCCESS;
}
#endif

/* KAPIL- NOTE : We are using Command mode operation of MEGACO stack, so we will always get command indication instead of transaction */
/* Below API is not useful ... just leaving as it is...*/

void handle_mgco_txn_ind(Pst *pst, SuId suId, MgMgcoMsg* msg)
{
    size_t txnIter;
    switch_memory_pool_t *pool;
    
    switch_core_new_memory_pool(&pool);
    
	/*TODO*/
    if(msg->body.type.val == MGT_TXN)
    {
        /* Loop over transaction list */
        for(txnIter=0;txnIter<msg->body.u.tl.num.val;txnIter++)
        {
            
            switch(msg->body.u.tl.txns[txnIter]->type.val) {
                case MGT_TXNREQ:
                {
                    MgMgcoTxnReq* txnReq; 
                    /*MgMgcoTransId transId; *//* XXX */
                    int axnIter;
                    txnReq = &(msg->body.u.tl.txns[txnIter]->u.req);

                    /* Loop over action list */
                    for (axnIter=0;axnIter<txnReq->al.num.val;axnIter++) {
                        MgMgcoActionReq *actnReq;
                        MgMgcoContextId ctxId;
                        int cmdIter;
                        
                        actnReq = txnReq->al.actns[axnIter];
                        ctxId = actnReq->cxtId; /* XXX */
                        
                        if (actnReq->pres.pres == NOTPRSNT) {
                            continue;
                        }
                        
                        /* Loop over command list */
                        for (cmdIter=0; cmdIter < (actnReq->cl.num.val); cmdIter++) {
                            MgMgcoCommandReq *cmdReq = actnReq->cl.cmds[cmdIter];
                            /*MgMgcoTermId *term_id = NULLP;*/
                            /* The reply we'll send */
                            MgMgcoCommand mgCmd;
			    memset(&mgCmd, 0, sizeof(mgCmd));
                            mgCmd.peerId = msg->lcl.id;
                            mgCmd.u.mgCmdInd[0] = cmdReq;
                            
                            
                            /* XXX Handle choose context before this */
                            
                            mgCmd.contextId = ctxId;
                            /*mgCmd.transId = transId;*/

                            mgCmd.cmdStatus.pres = PRSNT_NODEF;
                            
                            if(cmdIter == (actnReq->cl.num.val -1))
                            {
                                mgCmd.cmdStatus.val = CH_CMD_STATUS_END_OF_AXN;
                                if(axnIter == (txnReq->al.num.val-1))
                                {
                                    mgCmd.cmdStatus.val= CH_CMD_STATUS_END_OF_TXN;
                                } 
                            }
                            else
                            {
                                mgCmd.cmdStatus.val = CH_CMD_STATUS_PENDING;
                            }
                            
                            /* XXX handle props */
                            mgCmd.cmdType.pres = PRSNT_NODEF;
                            mgCmd.cmdType.val = CH_CMD_TYPE_REQ;
                            mgCmd.u.mgCmdReq[0] = cmdReq;
                            sng_mgco_send_cmd(suId, &mgCmd);
                            
                            
                            switch (cmdReq->cmd.type.val) {
                                case MGT_ADD:
                                {
                                    MgMgcoAmmReq *addReq = &cmdReq->cmd.u.add;
                                    int descId;
                                    for (descId = 0; descId < addReq->dl.num.val; descId++) {
                                        switch (addReq->dl.descs[descId]->type.val) {
                                            case MGT_MEDIADESC:
                                            {
                                                int mediaId;
                                                for (mediaId = 0; mediaId < addReq->dl.descs[descId]->u.media.num.val; mediaId++) {
                                                    MgMgcoMediaPar *mediaPar = addReq->dl.descs[descId]->u.media.parms[mediaId];
                                                    switch (mediaPar->type.val) {
                                                        case MGT_MEDIAPAR_LOCAL:
                                                        {
                                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_LOCAL");
                                                            break;
                                                        }
                                                        case MGT_MEDIAPAR_REMOTE:
                                                        {
                                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_REMOTE");
                                                            break;
                                                        }
                                                        
                                                        case MGT_MEDIAPAR_LOCCTL:
                                                        {
                                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_LOCCTL");
                                                            break;
                                                        }
                                                        case MGT_MEDIAPAR_TERMST:
                                                        {
                                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_TERMST");
                                                            break;
                                                        }
                                                        case MGT_MEDIAPAR_STRPAR:
                                                        {
//                                                            MgMgcoStreamDesc *mgStream = &mediaPar->u.stream;
//                                                            
//                                                            if (mgStream->sl.remote.pres.pres) {
//                                                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got remote stream media description:\n");
//                                                                mgco_print_sdp(&mgStream->sl.remote.sdp);
//                                                            }
//                                                            
//                                                            if (mgStream->sl.local.pres.pres) {
//                                                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got local stream media description:\n");
//                                                                mgco_print_sdp(&mgStream->sl.local.sdp);
//                                                            }
                                                            
                                                            
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                            case MGT_MODEMDESC:
                                            case MGT_MUXDESC:
                                            case MGT_REQEVTDESC:
                                            case MGT_EVBUFDESC:
                                            case MGT_SIGNALSDESC:
                                            case MGT_DIGMAPDESC:
                                            case MGT_AUDITDESC:
                                            case MGT_STATSDESC:
                                                break;
                                        }
                                    }
                                    
                                    
                                    
                                    break;
                                }
                                case MGT_MODIFY:
                                {
                                    /*MgMgcoAmmReq *addReq = &cmdReq->cmd.u.mod;*/
                                    break;
                                }
                                case MGT_MOVE:
                                {
                                    /*MgMgcoAmmReq *addReq = &cmdReq->cmd.u.move;*/
                                    break;
                                    
                                }
                                case MGT_SUB:
                                {
                                    /*MgMgcoSubAudReq *addReq = &cmdReq->cmd.u.sub;*/
                                }
                                case MGT_SVCCHG:
                                case MGT_NTFY:
                                case MGT_AUDITCAP:
                                case MGT_AUDITVAL:
                                    break;
                            }
                            
                        }
                    }
                    
                    break;
                }
                case MGT_TXNREPLY:
                {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MGT_TXNREPLY\n");
                    break;
                }
                default:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received unknown command %d in transaction\n", msg->body.u.tl.txns[txnIter]->type.val);
                    break;
            }
        }
    }
    
    switch_core_destroy_memory_pool(&pool);
}

/*****************************************************************************************************************************/
void handle_mgco_cmd_ind(Pst *pst, SuId suId, MgMgcoCommand* cmd)
{
	MgMgcoContextId  out_ctxt;
	U32 txn_id = 0x00;
	MgMgcoInd  *mgErr;
	MgStr      errTxt;
	MgMgcoContextId   ctxtId;
	MgMgcoContextId   *inc_context;
	MgMgcoTermIdLst*  termLst;
	MgMgcoTermId     *termId;
	int 		  count;
	int 		  err_code;
	megaco_profile_t* mg_profile;

    memset(&out_ctxt,0,sizeof(out_ctxt));

	inc_context = &cmd->contextId;
    memcpy(&out_ctxt, inc_context,sizeof(MgMgcoContextId));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: Received Command Type[%s] \n", __PRETTY_FUNCTION__, PRNT_MG_CMD_TYPE(cmd->cmdType.val));

    /*get mg profile associated with SuId */
    if(NULL == (mg_profile = megaco_get_profile_by_suId(suId))){
        goto error1;
    }

    /* first thing - restart ito timer */
    mg_restart_inactivity_timer(mg_profile);

	/* validate Transaction Id */
	if (NOTPRSNT != cmd->transId.pres){
		txn_id = cmd->transId.val;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: Transaction Id not present, rejecting\n", __PRETTY_FUNCTION__);	
		
		/*-- Send Error to MG Stack --*/
		MG_ZERO(&ctxtId, sizeof(MgMgcoContextId));
		ctxtId.type.pres = NOTPRSNT;
		ctxtId.val.pres  = NOTPRSNT;

		mg_util_set_txn_string(&errTxt, &txn_id);
		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		goto error;
	}

	/* Get the termination Id list from the command(Note: GCP_2_1 has termination list , else it will be termination Id)  */
	termLst = mg_get_term_id_list(cmd);
	if ((NULL == termLst) || (NOTPRSNT == termLst->num.pres)) {
		/* termination-id not present , error */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Termination-Id Not received..rejecting command \n");

		/*-- Send Error to MG Stack --*/
		MG_ZERO(&ctxtId, sizeof(MgMgcoContextId));
		ctxtId.type.pres = NOTPRSNT;
		ctxtId.val.pres  = NOTPRSNT;
		mg_util_set_txn_string(&errTxt, &txn_id);
		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		goto error;
	}

	termId  = termLst->terms[0];

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Termination-Id received..value[%s] type[%d] \n", termId->name.lcl.val, termId->type.val);

	/* Not sure - IF Stack fills term type properly..but adding code just to be sure ...*/
	if ((PRSNT_NODEF == termId->type.pres) &&
			(MGT_TERMID_OTHER == termId->type.val)) {
		/* Checking the $ in the pathname    */
		if ((PRSNT_NODEF == termId->name.pres.pres) &&
				(PRSNT_NODEF == termId->name.lcl.pres)) {
			for (count = 0; count < termId->name.lcl.len; count++) {
				if (termId->name.lcl.val[count] == '$') {
					termId->type.val = MGT_TERMID_CHOOSE;
					break;
				}

				if (termId->name.lcl.val[count] == '*') {
					termId->type.val = MGT_TERMID_ALL;
					break;
				}
			}
		}
	}

	/*If term type is other then check if that term is configured with us..for term type CHOOSE/ALL , no need to check */
	/* check is only if command is not AUDIT */
	if ((CH_CMD_TYPE_IND == cmd->cmdType.val) &&
			(MGT_TERMID_OTHER == termId->type.val) && 
			(MGT_AUDITVAL != cmd->u.mgCmdInd[0]->cmd.type.val)){
		if(SWITCH_STATUS_FALSE == mg_stack_termination_is_in_service(mg_profile, (char*)termId->name.lcl.val, termId->name.lcl.len)){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Termination[%s] not in service \n", (char*)termId->name.lcl.val);
			mg_util_set_term_string(&errTxt, termId);
			err_code = MGT_MGCO_RSP_CODE_UNKNOWN_TERM_ID;
			goto error;
		}
	}


	/* Validate Context - if context is specified then check if its present with us */
	MG_ZERO(&ctxtId, sizeof(MgMgcoContextId));
	memcpy(&ctxtId, inc_context, sizeof(MgMgcoContextId));

	if(NOTPRSNT == inc_context->type.pres){
		goto ctxt_error;

	}else if(MGT_CXTID_OTHER == inc_context->type.pres){

		if(NOTPRSNT != inc_context->val.pres){
#ifdef BIT_64
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Context specific request for contextId[%d]\n",inc_context->val.val);
#else
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Context specific request for contextId[%ld]\n",inc_context->val.val);
#endif
			/* check if context present with us */
			if(NULL == megaco_find_context_by_suid(suId, inc_context->val.val)){
				goto ctxt_error;
			}		
		}else{
			/* context id value not present - in case of type OTHER we should have context value */
			goto ctxt_error;
		}
	}
	

	/*mgAccEvntPrntMgMgcoCommand(cmd, stdout);*/

	
	switch(cmd->cmdType.val)
	{
		case CH_CMD_TYPE_IND:
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: Received Command indication for command[%s]\n",
						__PRETTY_FUNCTION__,PRNT_MG_CMD(cmd->u.mgCmdInd[0]->cmd.type.val));

				switch(cmd->u.mgCmdInd[0]->cmd.type.val)
				{
					case MGT_ADD:
						{
							handle_mg_add_cmd(mg_profile, cmd, &out_ctxt);
							/*mg_send_add_rsp(suId, cmd);*/
							break;
						}

					case MGT_MODIFY:
						{
							/*MgMgcoAmmReq *addReq = &cmdReq->cmd.u.mod;*/
							handle_mg_modify_cmd(mg_profile, cmd);
							/*mg_send_modify_rsp(suId, cmd);*/
							break;
						}
					case MGT_MOVE:
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MOVE Method Not Yet Supported\n");
							err_code = MGT_MGCO_RSP_CODE_UNSUPPORTED_CMD;
							mg_util_set_cmd_name_string(&errTxt, cmd);
							goto error;
						}
					case MGT_SUB:
						{
							/*MgMgcoSubAudReq *addReq = &cmdReq->cmd.u.sub;*/
							handle_mg_subtract_cmd(mg_profile, cmd);
							/*mg_send_subtract_rsp(suId, cmd);*/
							break;
						}
					case MGT_SVCCHG:
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Service-Change Method Not Yet Supported\n");
							break;
						}
					case MGT_NTFY:
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NOTIFY Method Not Yet Supported\n");
							break;
						}
					case MGT_AUDITCAP:
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Audit-Capability Method Not Yet Supported\n");
							err_code = MGT_MGCO_RSP_CODE_UNSUPPORTED_CMD;
							mg_util_set_cmd_name_string(&errTxt, cmd);
							goto error;
						}
					case MGT_AUDITVAL:
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received Audit-Value Method \n");
							handle_mg_audit_cmd(mg_profile, cmd);
							break;
						}
						break;
				}

				break;
			}
		case CH_CMD_TYPE_REQ:
			{
				break;
			}
		case CH_CMD_TYPE_RSP:
			{
				break;
			}
		case CH_CMD_TYPE_CFM:
			{
#ifdef BIT_64
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Received Command[%s] txn[%d] Response/Confirmation \n",
                        PRNT_MG_CMD(cmd->u.mgCmdCfm[0]->type.val), txn_id);
#else
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Received Command[%s] txn[%ld] Response/Confirmation \n",
                        PRNT_MG_CMD(cmd->u.mgCmdCfm[0]->type.val), txn_id);
#endif
                switch(cmd->u.mgCmdCfm[0]->type.val)
                {
                    case MGT_NTFY:
                        {
                            MgMgcoNtfyReply*  ntfy = &cmd->u.mgCmdCfm[0]->u.ntfy;
                            MgMgcoTermId*     term = NULL;
                            char              term_name[32]; 
                            memset(&term_name[0], 0x00,32);

                            strcpy(&term_name[0], "Invalid");

#ifdef GCP_VER_2_1   
                            if((NOTPRSNT != ntfy->termIdLst.num.pres) && 
                                    (0 != ntfy->termIdLst.num.val)){
                                term = ntfy->termIdLst.terms[0];
                            }
#else
                            term = &ntfy->termId;

#endif
                            if(NOTPRSNT != term->type.pres){
                                if(MGT_TERMID_ROOT == term->type.val){
                                    strcpy(&term_name[0],"ROOT");
                                }
                                else if(MGT_TERMID_OTHER == term->type.val){
                                    strcpy(&term_name[0], (char*)term->name.lcl.val);
                                }else if(MGT_TERMID_ALL == term->type.val){
                                    strcpy(&term_name[0],"ALL Termination"); 
                                }else if(MGT_TERMID_CHOOSE == term->type.val){
                                    strcpy(&term_name[0],"CHOOSE Termination"); 
                                }
                            }

                            if(NOTPRSNT != ntfy->pres.pres){
                                if((NOTPRSNT != ntfy->err.pres.pres) && 
                                        (NOTPRSNT != ntfy->err.code.pres)){
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, 
                                            "Received NOTIFY command response with ErroCode[%d] for Termination[%s] \n", 
                                            ntfy->err.code.val, &term_name[0]);
                                }
                                else{
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, 
                                            "Received Successful NOTIFY command response for Termination[%s] \n", &term_name[0]);
                                }
                            }

                            break;
                        }
                    case MGT_SVCCHG:
                        {
                            MgMgcoSvcChgReply*  svc = &cmd->u.mgCmdCfm[0]->u.svc;
                            MgMgcoTermId*     term = NULL;
                            char              term_name[32]; 
                            memset(&term_name[0], 0x00, 32);

                            strcpy(&term_name[0], "Invalid");

#ifdef GCP_VER_2_1   
                            if((NOTPRSNT != svc->termIdLst.num.pres) && 
                                    (0 != svc->termIdLst.num.val)){
                                term = svc->termIdLst.terms[0];
                            }
#else
                            term = &svc->termId;

#endif
                            if(NOTPRSNT != term->type.pres){
                                if(MGT_TERMID_ROOT == term->type.val){
                                    strcpy(&term_name[0],"ROOT");
                                }
                                else if(MGT_TERMID_OTHER == term->type.val){
                                    strcpy(&term_name[0], (char*)term->name.lcl.val);
                                }else if(MGT_TERMID_ALL == term->type.val){
                                    strcpy(&term_name[0],"ALL Termination"); 
                                }else if(MGT_TERMID_CHOOSE == term->type.val){
                                    strcpy(&term_name[0],"CHOOSE Termination"); 
                                }
                            }

                            if(NOTPRSNT != svc->pres.pres){ 

                                if((NOTPRSNT != svc->res.type.pres) && 
                                        (MGT_ERRDESC == svc->res.type.val)){
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, 
                                            "Received Service-Change command response with ErroCode[%d] for Termination[%s] \n", 
                                            svc->res.u.err.code.val, &term_name[0]);
                                }
                                else{
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, 
                                            "Received Successful Service-Change command response for Termination[%s] \n", &term_name[0]);
                                }
                            }

                            break;
                        }
                    default:
                        break;
                }
                break;
			}
		default:
#ifdef BIT_64
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid command type[%d]\n",cmd->cmdType.val);
#else
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid command type[%d]\n",cmd->cmdType.val);
#endif
			return;
	}

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "cmd->cmdStatus.val[%d]\n",cmd->cmdStatus.val);
    /* END OF TXN received - means last command in txn to process. 
     * Send response to peer */
    if(CH_CMD_TYPE_IND == cmd->cmdType.val){ 
    /*if(CH_CMD_STATUS_END_OF_TXN == cmd->cmdStatus.val)*/
        mg_send_end_of_axn(suId, &cmd->transId, &out_ctxt, &cmd->peerId);
    }

	return;

ctxt_error:
	err_code = MGT_MGCO_RSP_CODE_UNKNOWN_CTXT;

error:
	if (SWITCH_STATUS_SUCCESS == 
			mg_build_mgco_err_request(&mgErr, txn_id, &ctxtId, err_code, &errTxt)) {
		sng_mgco_send_err(suId, mgErr);
	}
    if(CH_CMD_STATUS_END_OF_TXN == cmd->cmdStatus.val){
        mg_send_end_of_axn(suId, &cmd->transId, &out_ctxt, &cmd->peerId);
    }
error1:
	mg_free_cmd(cmd);
	return;
}

/*****************************************************************************************************************************/
void handle_mgco_sta_ind(Pst *pst, SuId suId, MgMgtSta* sta)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s\n", __PRETTY_FUNCTION__);	/*TODO*/
}

/*****************************************************************************************************************************/

void handle_mgco_txn_sta_ind(Pst *pst, SuId suId, MgMgcoInd* txn_sta_ind)
{
	/*TODO*/
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s\n", __PRETTY_FUNCTION__);
   
    /*dump information*/
    mgAccEvntPrntMgMgcoInd(txn_sta_ind, stdout);
}

/*****************************************************************************************************************************/
void handle_mgco_cntrl_cfm(Pst *pst, SuId suId, MgMgtCntrl* cntrl, Reason reason) 
{
	/*TODO*/
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s\n", __PRETTY_FUNCTION__);
}

/*****************************************************************************************************************************/
void handle_mgco_audit_cfm(Pst *pst, SuId suId, MgMgtAudit* audit, Reason reason) 
{
	/*TODO*/
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s\n", __PRETTY_FUNCTION__);
}


/*****************************************************************************************************************************/

/*****************************************************************************************************************************/
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
