/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"
#include "media_gateway_stack.h"

struct megaco_globals megaco_globals;
static sng_mg_event_interface_t sng_event;

SWITCH_MODULE_LOAD_FUNCTION(mod_media_gateway_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_media_gateway_shutdown);
SWITCH_MODULE_DEFINITION(mod_media_gateway, mod_media_gateway_load, mod_media_gateway_shutdown, NULL);

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
		case SNG_LOGLEVEL_ERROR:    log_level = SWITCH_LOG_DEBUG;       break;
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
    
//   XXX check if we can use that for debug:
// cmUnpkCmSdpInfoSet(<#CmSdpInfoSet *param#>, <#Ptr ptr#>, <#CmIntfVer intfVer#>, <#Buffer *mBuf#>);
        
    
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
                    CmSdpAttr *a = s->attrSet.attr[mediaId];
                    
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

/*****************************************************************************************************************************/

void handle_mgco_txn_ind(Pst *pst, SuId suId, MgMgcoMsg* msg)
{
    size_t txnIter;
    
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
                            MgMgcoTermId *termId = NULLP;
                            
                            switch (cmdReq->cmd.type.val) {
                                case MGT_ADD:
                                {
                                    MgMgcoAmmReq *addReq = &cmdReq->cmd.u.add;
                                    MgMgcoTermId termId = addReq->termId;
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
                                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Textual SDP %s\n", mediaPar->u.local.sdpStr.pres ? "present" : "absent");
                                                            break;
                                                        }
                                                        case MGT_MEDIAPAR_REMOTE:
                                                        {
                                                            break;
                                                        }
                                                        
                                                        case MGT_MEDIAPAR_LOCCTL:
                                                        {
                                                            break;
                                                        }
                                                        case MGT_MEDIAPAR_TERMST:
                                                            break;
                                                        case MGT_MEDIAPAR_STRPAR:
                                                        {
                                                            MgMgcoStreamDesc *mgStream = &mediaPar->u.stream;
                                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Textual SDP %s\n", mgStream->sl.remote.sdpStr.pres ? "present" : "absent");
                                                            
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
                                    break;
                                }
                                case MGT_MODIFY:
                                {
                                    MgMgcoAmmReq *addReq = &cmdReq->cmd.u.mod;
                                    break;
                                }
                                case MGT_MOVE:
                                {
                                    MgMgcoAmmReq *addReq = &cmdReq->cmd.u.move;
                                    break;
                                    
                                }
                                case MGT_SUB:
                                {
                                    MgMgcoSubAudReq *addReq = &cmdReq->cmd.u.sub;
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
                    break;
                }
                default:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received unknown command %d in transaction\n", msg->body.u.tl.txns[txnIter]->type.val);
                    break;
            }
        }
    }
}

/*****************************************************************************************************************************/
void handle_mgco_cmd_ind(Pst *pst, SuId suId, MgMgcoCommand* cmd)
{
	/*TODO*/
}

/*****************************************************************************************************************************/
void handle_mgco_sta_ind(Pst *pst, SuId suId, MgMgtSta* sta)
{
	/*TODO*/
}

/*****************************************************************************************************************************/

void handle_mgco_txn_sta_ind(Pst *pst, SuId suId, MgMgcoInd* txn_sta_ind)
{
	/*TODO*/
}

/*****************************************************************************************************************************/
void handle_mgco_cntrl_cfm(Pst *pst, SuId suId, MgMgtCntrl* cntrl, Reason reason) 
{
	/*TODO*/
}

/*****************************************************************************************************************************/
void handle_mgco_audit_cfm(Pst *pst, SuId suId, MgMgtAudit* audit, Reason reason) 
{
	/*TODO*/
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
