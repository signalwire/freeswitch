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
switch_status_t handle_mg_add_cmd(SuId suId, MgMgcoCommand *req, MgMgcoAmmReq *addReq);
switch_status_t mg_stack_free_mem(MgMgcoMsg* msg);
switch_status_t mg_stack_free_mem(MgMgcoMsg* msg);
switch_status_t mg_stack_alloc_mem( Ptr* _memPtr, Size _memSize );
switch_status_t mg_send_add_rsp(SuId suId, MgMgcoCommand *req);
S16 mg_fill_mgco_termid ( MgMgcoTermId  *termId, CONSTANT U8   *str, CmMemListCp   *memCp);

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

static void mgco_print_sdp(CmSdpInfoSet *sdp)
{
    int i;
    
    
    if (sdp->numComp.pres == NOTPRSNT) {
        return;
    }
    
    for (i = 0; i < sdp->numComp.val; i++) {
        CmSdpInfo *s = sdp->info[i];
        int mediaId;
        
        if (s->conn.addrType.pres && s->conn.addrType.val == CM_SDP_ADDR_TYPE_IPV4 &&
            s->conn.netType.type.val == CM_SDP_NET_TYPE_IN &&
            s->conn.u.ip4.addrType.val == CM_SDP_IPV4_IP_UNI) {

            if (s->conn.u.ip4.addrType.pres) {
                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Address: %d.%d.%d.%d\n",
                                   s->conn.u.ip4.u.uniIp.b[0].val,
                                   s->conn.u.ip4.u.uniIp.b[1].val,
                                   s->conn.u.ip4.u.uniIp.b[2].val,
                                   s->conn.u.ip4.u.uniIp.b[3].val);
            }
            if (s->attrSet.numComp.pres) {
                for (mediaId = 0; mediaId < s->attrSet.numComp.val; mediaId++) {
                    /*CmSdpAttr *a = s->attrSet.attr[mediaId];*/
                    
                }
            }

            if (s->mediaDescSet.numComp.pres) {
                for (mediaId = 0; mediaId < s->mediaDescSet.numComp.val; mediaId++) {
                    CmSdpMediaDesc *desc = s->mediaDescSet.mediaDesc[mediaId];
                    
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
}


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
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Media %p\n", a);
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
}

/*****************************************************************************************************************************/

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
                    MgMgcoTransId transId; /* XXX */
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
                            mgCmd.transId = transId;
                            mgCmd.u.mgCmdInd[0] = cmdReq;
                            
                            
                            /* XXX Handle choose context before this */
                            
                            mgCmd.contextId = ctxId;
                            mgCmd.transId = transId;

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
	uint32_t txn_id = 0x00;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: Received Command Type[%s] \n", __PRETTY_FUNCTION__, PRNT_MG_CMD_TYPE(cmd->cmdType.val));

	/* validate Transaction Id */
	if (NOTPRSNT != cmd->transId.pres)
		txn_id = cmd->transId.val;
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: Transaction Id not present, rejecting\n", __PRETTY_FUNCTION__);	
		/*TODO - can invoke "MgUiMgtMgcoErrReq" to report error to MEGACO stack */
		/* deallocate the msg */
		mg_free_cmd(cmd);
		return ;	
	}

	mgAccEvntPrntMgMgcoCommand(cmd, stdout);

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
							handle_mg_add_cmd(suId, cmd, &cmd->u.mgCmdInd[0]->cmd.u.add);
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
				break;
			}
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid command type[%d]\n",cmd->cmdType.val);
			return;
	}

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
    /*mgAccEvntPrntMgMgcoInd(txn_sta_ind, stdout);*/
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
/*
*
*       Fun:   mg_get_term_id_list
*
*       Desc:  Utility function to get MgMgcoTermIdLst structure
*              from MgMgcoCommand structure.
* 	       GCP_VER_2_1 - we will have term id list instead of single term id
*
*       Ret:   If success, return pointer to MgMgcoTermIdLst. 
*              If failure, return Null.
*
*       Notes: None
*
*/

MgMgcoTermIdLst *mg_get_term_id_list(MgMgcoCommand *cmd)
{
   uint8_t           cmd_type	= MGT_NONE;
   uint8_t           api_type 	= CM_CMD_TYPE_NONE;
   MgMgcoTermIdLst *    term_id 	= NULL;


   /*-- mgCmdInd type represents the data structure for both
    *   incoming and outgoing requests, hence we can get the
    *   command type from there itself --*/
   cmd_type = cmd->u.mgCmdInd[0]->cmd.type.val;

   /*-- Find apiType --*/
   api_type = cmd->cmdType.val;

   switch (api_type)
   {
      case CH_CMD_TYPE_REQ:
      case CH_CMD_TYPE_IND:
         /* Based on Command Type, get to the TermId structure */
         switch (cmd_type)
         {
            case MGT_ADD:
               if (cmd->u.mgCmdInd[0]->cmd.u.add.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.add.termIdLst;
               break;

            case MGT_MOVE:
               if (cmd->u.mgCmdInd[0]->cmd.u.move.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.move.termIdLst;
               break;

            case MGT_MODIFY:
               if (cmd->u.mgCmdInd[0]->cmd.u.mod.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.mod.termIdLst;
               break;

            case MGT_SUB:
               if (cmd->u.mgCmdInd[0]->cmd.u.sub.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.sub.termIdLst;
               break;

            case MGT_AUDITCAP:
               if (cmd->u.mgCmdInd[0]->cmd.u.acap.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.acap.termIdLst;
               break;

            case MGT_AUDITVAL:
               if (cmd->u.mgCmdInd[0]->cmd.u.aval.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.aval.termIdLst;
               break;

            case MGT_NTFY:
               if (cmd->u.mgCmdInd[0]->cmd.u.ntfy.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.ntfy.termIdLst;
               break;

            case MGT_SVCCHG:
               if (cmd->u.mgCmdInd[0]->cmd.u.svc.pres.pres)
                  term_id = &cmd->u.mgCmdInd[0]->cmd.u.svc.termIdLst;
               break;

            default:
                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: failed, Unsupported Command[%s]\n", __PRETTY_FUNCTION__, PRNT_MG_CMD(cmd_type));
               break;
         }
         break;

      case CH_CMD_TYPE_RSP:
      case CH_CMD_TYPE_CFM:

         cmd_type = cmd->u.mgCmdRsp[0]->type.val;

         switch (cmd_type)
         {
            case MGT_ADD:
               if (cmd->u.mgCmdRsp[0]->u.add.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.add.termIdLst;
               break;

            case MGT_MOVE:
               if (cmd->u.mgCmdRsp[0]->u.move.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.move.termIdLst;
               break;

            case MGT_MODIFY:
               if (cmd->u.mgCmdRsp[0]->u.mod.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.mod.termIdLst;
               break;

            case MGT_SUB:
               if (cmd->u.mgCmdRsp[0]->u.sub.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.sub.termIdLst;
               break;

            case MGT_SVCCHG:
               if (cmd->u.mgCmdRsp[0]->u.svc.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.svc.termIdLst;
               break;

            case MGT_AUDITVAL:
               if (cmd->u.mgCmdRsp[0]->u.aval.u.other.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.aval.u.other.termIdLst;
               break;

            case MGT_AUDITCAP:
               if (cmd->u.mgCmdRsp[0]->u.acap.u.other.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.acap.u.other.termIdLst;
               break;

            case MGT_NTFY:
               if (cmd->u.mgCmdRsp[0]->u.ntfy.pres.pres)
                  term_id = &cmd->u.mgCmdRsp[0]->u.ntfy.termIdLst;
               break;

            default:
                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: failed, Unsupported Command[%s]\n", __PRETTY_FUNCTION__, PRNT_MG_CMD(cmd_type));
         } /* switch command type for reply */
         break;

      default:
           switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: failed, Unsupported api_type[%s]!\n", __PRETTY_FUNCTION__, PRNT_MG_CMD_TYPE(api_type));
         break;
   } /* switch -api_type */

   return (term_id);
}

/*****************************************************************************************************************************/

switch_status_t handle_mg_add_cmd(SuId suId, MgMgcoCommand *req, MgMgcoAmmReq *addReq)
{
    int descId;
    mg_context_t *ctx = NULL;
    
    MgMgcoCommand  cmd;
	int ret = 0x00;
	MgMgcoTermId  *termId;
	MgMgcoCtxt     ctxt;
    
	memset(&cmd,0, sizeof(cmd));
    
	/*copy transaction-id*/
	memcpy(&cmd.transId, &req->transId, sizeof(MgMgcoTransId));
    
    if (req->contextId.type.val == MGT_CXTID_CHOOSE) {
        ctx = megaco_choose_context(NULL);
    } else if (req->contextId.type.val == MGT_CXTID_OTHER) {
        ctx = megaco_get_context(NULL, req->contextId.val.val);
    }
    
    
	/*copy context-id*/ /*TODO - in case of $ context should be generated by app, we should not simply copy incoming structure */
	memcpy(&cmd.contextId, &req->contextId, sizeof(MgMgcoContextId));
    
	/*copy peer identifier */
	memcpy(&cmd.peerId, &req->peerId, sizeof(TknU32));
    
	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&cmd.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
	}


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
									MgMgcoStreamDesc *mgStream = &mediaPar->u.stream;

									if (mgStream->sl.remote.pres.pres) {
										switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got remote stream media description:\n");
										mgco_print_sdp(&mgStream->sl.remote.sdp);
									}

									if (mgStream->sl.local.pres.pres) {
										switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got local stream media description:\n");
										mgco_print_sdp(&mgStream->sl.local.sdp);
									}

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
    
    cmd.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->type.val = MGT_ADD;
	cmd.u.mgCmdRsp[0]->u.add.pres.pres = PRSNT_NODEF;
    
    
	cmd.u.mgCmdRsp[0]->u.add.termIdLst.num.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->u.add.termIdLst.num.val  = 1;
    
	mgUtlAllocMgMgcoTermIdLst(&cmd.u.mgCmdRsp[0]->u.add.termIdLst, &req->u.mgCmdReq[0]->cmd.u.add.termIdLst);
    
#ifdef GCP_VER_2_1
	termId = cmd.u.mgCmdRsp[0]->u.add.termIdLst.terms[0];
#else
	termId = &(cmd.u.mgCmdRsp[0]->u.add.termId);
#endif
    /* FIXME */
	mg_fill_mgco_termid(termId, (CONSTANT U8*)"term1",&req->u.mgCmdRsp[0]->memCp);
    
	/* We will always send one command at a time..*/
	cmd.cmdStatus.pres = PRSNT_NODEF;
	cmd.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;
    
	cmd.cmdType.pres = PRSNT_NODEF;
	cmd.cmdType.val  = CH_CMD_TYPE_RSP;
    
    
	ret = sng_mgco_send_cmd(suId, &cmd);
    
	memcpy(&ctxt.transId,&req->transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, &cmd.contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &req->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;

	return sng_mgco_send_axn_req(suId, &ctxt);;	
}

/*****************************************************************************************************************************/
switch_status_t mg_stack_alloc_mem( Ptr* _memPtr, Size _memSize )
{
	Mem sMem;

	sMem.region = 0;
	sMem.pool = 0;

	if ( _memSize <= 0 )
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed mg_stack_alloc_mem: invalid size\n"); 
		return SWITCH_STATUS_FALSE;
	}

	if ( ROK != cmAllocEvnt( _memSize, MG_MAXBLKSIZE, &sMem, _memPtr ) )
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed mg_stack_alloc_mem: cmAllocEvnt return failure for _memSize=%d\n", (int)_memSize); 
		return SWITCH_STATUS_FALSE;
	}

	// Note: memset done inside stack api

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mg_stack_get_mem(MgMgcoMsg* msg, Ptr* _memPtr, Size _memSize )
{
        if ( _memSize <= 0 )
        {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed mg_stack_get_mem: invalid size\n"); 
		return SWITCH_STATUS_FALSE;
        }

        if ( !msg )
        {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed mg_stack_get_mem: invalid message\n"); 
		return SWITCH_STATUS_FALSE;
        }

        if ( cmGetMem( (Ptr)msg, _memSize, (Ptr*)_memPtr ) != ROK )
        {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed alloc_mg_stack_mem: get memory failed _memSize=%d\n", _memSize );
		return SWITCH_STATUS_FALSE;
        }

        // Note: memset done inside stack api

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t mg_stack_free_mem(MgMgcoMsg* msg)
{
        if ( !msg )
        {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed mg_stack_free_mem: invalid message\n"); 
		return SWITCH_STATUS_FALSE;
        }

        cmFreeMem( (Ptr)msg );

        return SWITCH_STATUS_SUCCESS;
}

#define MG_ACC_GETMEM(_ptr,_len,_memCp,_ret)                              \
{                                                                         \
        ret = cmGetMem((_memCp), (_len), (Ptr *)&(_ptr));                 \
        if( ret == ROK)                                                   \
          cmMemset((U8 *)(_ptr), (U8)0,  (PTR)(_len));                    \
}


S16 mg_fill_mgco_termid ( MgMgcoTermId  *termId, CONSTANT U8   *str, CmMemListCp   *memCp)
{
#ifdef GCP_ASN       
   Size              size;
#endif
   S16               ret = ROK;

   termId->name.pres.pres = PRSNT_NODEF;
   /* mg011.105: Bug fixes */
   termId->name.lcl.pres = PRSNT_NODEF;
   termId->name.lcl.len = cmStrlen((CONSTANT U8*)str);
   MG_ACC_GETMEM((termId->name.lcl.val), termId->name.lcl.len, memCp, ret);
   if( ret != ROK)
      RETVALUE(ret);          

   cmMemcpy((U8*)(termId->name.lcl.val), (CONSTANT U8*)str,termId->name.lcl.len);

#ifdef GCP_ASN          
   /* Remove comment to fill other term ID 
   termId->wildcard.num.pres = NOTPRSNT; */
   /* Remove comment to fill wilcard term ID */
   termId->wildcard.num.pres = PRSNT_NODEF;    
   termId->wildcard.num.val = 1;
   size = ((sizeof(MgMgcoWildcardField*)));
   MG_ACC_GETMEM((termId->wildcard.wildcard),size,memCp, ret);
   if( ret != ROK)
      RETVALUE(ret);

   MG_ACC_GETMEM( ((termId->wildcard.wildcard)[0]),sizeof(MgMgcoWildcardField),
                  memCp, ret);
   if( ret != ROK)
      RETVALUE(ret);

   termId->wildcard.wildcard[0]->pres = PRSNT_NODEF;
   termId->wildcard.wildcard[0]->len = 1;
   termId->wildcard.wildcard[0]->val[0] = 0x55;
#endif /* GCP_ASN */

   RETVALUE(ROK);
}


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
