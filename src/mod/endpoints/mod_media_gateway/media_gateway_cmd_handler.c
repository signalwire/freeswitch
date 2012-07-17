/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"
#include "media_gateway_stack.h"

U32 outgoing_txn_id;

/*****************************************************************************************************************************/
const char *mg_service_change_reason[] = {
	"\"NOT USED\"",
	"\"900 ServiceRestored\"",
	"\"905 Termination taken out of service\"",
	0
};

/*****************************************************************************************************************************/

/*
*
*       Fun:  handle_mg_add_cmd
*
*       Desc: this api will handle the ADD request received from MG stack 
*
*
*/
switch_status_t handle_mg_add_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *inc_cmd)
{
	MgMgcoContextId  *ctxtId;
	int 		  descId;
	MgStr      	  errTxt;
	MgMgcoInd  	  *mgErr;
	MgMgcoTermId     *termId;
	MgMgcoTermIdLst*  termLst;
	int 		  err_code;
	MgMgcoAmmReq 	  *cmd = &inc_cmd->u.mgCmdInd[0]->cmd.u.add;
	U32 		   txn_id = inc_cmd->transId.val;

	/********************************************************************/
	ctxtId  = &inc_cmd->contextId;
	termLst = mg_get_term_id_list(inc_cmd);
	termId  = termLst->terms[0];

	/********************************************************************/
	/* Validating ADD request *******************************************/

	/*-- NULL Context & ALL Context not applicable for ADD request --*/
	if ((NOTPRSNT != ctxtId->type.pres)          &&
			((MGT_CXTID_ALL == ctxtId->type.val)     ||
			 (MGT_CXTID_NULL == ctxtId->type.val))) {

		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR," ADD Request processing failed, Context ALL/NULL not allowed\n");

		mg_util_set_ctxt_string(&errTxt, ctxtId);
		err_code = MGT_MGCO_RSP_CODE_PROT_ERROR;
		goto error;
	}

	/********************************************************************/
	/* Allocate context - if context type is CHOOSE */
	if ((NOTPRSNT != ctxtId->type.pres)  &&
			(MGT_CXTID_CHOOSE == ctxtId->type.val)){

		/* TODO - Matt */
	}

	/********************************************************************/
	/* Allocate new RTP termination - If term type is CHOOSE */
	if ((NOTPRSNT != termId->type.pres)   &&
			(MGT_TERMID_CHOOSE == termId->type.val)){

		/* TODO - Matt */
		/* allocate rtp term and associated the same to context */
	}

	/********************************************************************/


	for (descId = 0; descId < cmd->dl.num.val; descId++) {
		switch (cmd->dl.descs[descId]->type.val) {
			case MGT_MEDIADESC:
				{
					int mediaId;
					for (mediaId = 0; mediaId < cmd->dl.descs[descId]->u.media.num.val; mediaId++) {
						MgMgcoMediaPar *mediaPar = cmd->dl.descs[descId]->u.media.parms[mediaId];
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


	return SWITCH_STATUS_SUCCESS;	
error:
	if (SWITCH_STATUS_SUCCESS == 
			mg_build_mgco_err_request(&mgErr, txn_id, ctxtId, err_code, &errTxt)) {
		sng_mgco_send_err(mg_profile->idx, mgErr);
	}
	mg_free_cmd(cmd);
	return SWITCH_STATUS_FALSE;	
}

/*****************************************************************************************************************************/
/*
*
*       Fun:  mg_send_add_rsp
*
*       Desc: this api will send the ADD response based on ADD request received from MG stack 
*
* 	TODO - Dummy response , needs to have proper ADD response code
*/
switch_status_t mg_send_add_rsp(SuId suId, MgMgcoCommand *req)
{
	MgMgcoCommand  cmd;
	int ret = 0x00;
	MgMgcoTermId  *termId;

	memset(&cmd,0, sizeof(cmd));

	/*copy transaction-id*/
	memcpy(&cmd.transId, &req->transId,sizeof(MgMgcoTransId));

	/*copy context-id*/ /*TODO - in case of $ context should be generated by app, we should not simply copy incoming structure */
	memcpy(&cmd.contextId, &req->contextId,sizeof(MgMgcoContextId));

	/*copy peer identifier */
	memcpy(&cmd.peerId, &req->peerId,sizeof(TknU32));

	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&cmd.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
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
	/*mg_fill_mgco_termid(termId, (char*)"term1",&req->u.mgCmdRsp[0]->memCp);*/

	/* We will always send one command at a time..*/
	cmd.cmdStatus.pres = PRSNT_NODEF;
	cmd.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

	cmd.cmdType.pres = PRSNT_NODEF;
	cmd.cmdType.val  = CH_CMD_TYPE_RSP;

	ret = sng_mgco_send_cmd(suId, &cmd);

	return ret;
}

/*****************************************************************************************************************************/
/*
*
*       Fun:  mg_send_end_of_axn
*
*       Desc: this api will send the END_OF_AXN event to MG stack to indicate that application is done with processing 
*
*
*/
switch_status_t mg_send_end_of_axn(SuId suId, MgMgcoTransId* transId, MgMgcoContextId* ctxtId, TknU32* peerId)
{
	int 	       ret = 0x00;
	MgMgcoCtxt     ctxt;

	memset(&ctxt,0, sizeof(ctxt));
	memcpy(&ctxt.transId,transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, ctxtId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;

#ifdef BIT_64
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, 
			"mg_send_end_of_axn: Sending END_OF_AXN for transId[%d], peerId[%d], context[type = %s, value = %d]\n",
			transId->val, peerId->val, PRNT_MG_CTXT_TYPE(ctxtId->type.val), ctxtId->val.val);
#else
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, 
			"mg_send_end_of_axn: Sending END_OF_AXN for transId[%lu], peerId[%lu], context[type = %s, value = %lu]\n",
			transId->val, peerId->val, PRNT_MG_CTXT_TYPE(ctxtId->type.val), ctxtId->val.val);

#endif

	ret = sng_mgco_send_axn_req(suId, &ctxt);

	return SWITCH_STATUS_SUCCESS;	
}

/*****************************************************************************************************************************/
/*
*
*       Fun:  mg_build_mgco_err_request
*
*       Desc: this api will send the Error event to MG stack to indicate failure in application
*
*
*/

switch_status_t mg_build_mgco_err_request(MgMgcoInd  **errcmd,U32  trans_id, MgMgcoContextId   *ctxt_id, U32  err, MgStr  *errTxt)
{
	MgMgcoInd     *mgErr = NULL;   
	S16            ret;

	mgErr = NULLP;
	ret = ROK;

	/* Allocate for AG error */
	mg_stack_alloc_mem((Ptr*)&mgErr, sizeof(MgMgcoInd));
	if (NULL == mgErr) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " mg_build_mgco_err_request Failed : memory alloc \n"); 
		return SWITCH_STATUS_FALSE;
	}

	/* Set transaction Id in the error request */
	MG_SET_VAL_PRES(mgErr->transId, trans_id);

	/* Copy the context Id */
	MG_MEM_COPY(&mgErr->cntxtId, 
			ctxt_id, 
			sizeof(MgMgcoContextId));

	/* Set the peerId  */
	mgErr->peerId.pres = NOTPRSNT;

	/* Set the error code   */
	MG_SET_PRES(mgErr->err.pres.pres);                 
	MG_SET_PRES(mgErr->err.code.pres);                 
	MG_SET_VAL_PRES(mgErr->err.code, err);   

	if(errTxt->len)
	{
		MG_GETMEM(mgErr->err.text.val, (errTxt->len)*sizeof(U8), &mgErr->memCp, &ret);
		if (ROK != ret) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " mg_build_mgco_err_request Failed : memory alloc \n"); 
			return SWITCH_STATUS_FALSE;
		}
		mgErr->err.text.pres = PRSNT_NODEF;
		mgErr->err.text.len =  errTxt->len;
		MG_MEM_COPY(mgErr->err.text.val, errTxt->val, errTxt->len);
	}

	/* Set the output value  */
	*errcmd = mgErr;

	return SWITCH_STATUS_SUCCESS;
}


/*****************************************************************************************************************************/
switch_status_t handle_mg_audit_cmd( SuId suId, MgMgcoCommand *auditReq)
{
	MgMgcoContextId  *ctxtId;
	MgMgcoTermIdLst  *term_list;
	MgStr      	  errTxt;
	MgMgcoInd  	  *mgErr;
	MgMgcoTermId     *termId;
	MgMgcoSubAudReq  *audit;
	MgMgcoAuditDesc  *audit_desc;
	MgMgcoAuditItem  *audit_item;
	int 		  i;
	int 		  err_code;
	MgMgcoCommand     reply;
        MgMgcoAuditReply  *adtRep = NULLP;
        U16               numOfParms;
        MgMgcoMediaDesc*  media;
	MgMgcoCtxt     ctxt;
	switch_status_t  ret;
	uint8_t          wild = 0x00;

	memset(&reply, 0, sizeof(reply));

	audit 	   = &auditReq->u.mgCmdReq[0]->cmd.u.aval;
	wild 	   = auditReq->u.mgCmdReq[0]->wild.pres;

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"%s :: wild card request = %s \n",__FUNCTION__,(1==wild)?"TRUE":"FALSE");

	if(NOTPRSNT == audit->pres.pres){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Audit structure not present..rejecting \n");
		return SWITCH_STATUS_FALSE;
	}

	audit_desc = &audit->audit;

	if((NOTPRSNT == audit_desc->pres.pres) || ( NOTPRSNT == audit_desc->num.pres)){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Audit Descriptor not present.. Could be HeartBeat message\n");
		return mg_send_heartbeat_audit_rsp(suId, auditReq);
	}

	/* dump AUDIT message information */
	/*mgAccEvntPrntMgMgcoSubAudReq(auditReq,stdout);*/

	/*-- Get context id --*/
	ctxtId = &auditReq->contextId;

	/*-- Get termination list --*/
	term_list = mg_get_term_id_list(auditReq);
	termId    = term_list->terms[0];

     /*********************************************************************************************************************/
     /**************************** Validating Audit Request ***************************************************************/
     /*********************************************************************************************************************/
	/*-- Start with Context level checks --*/
	/*-- CHOOSE Context not allowed --*/
	if ((NOTPRSNT != ctxtId->type.pres)          &&
			(MGT_CXTID_CHOOSE == ctxtId->type.val)) {

		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,	
				"failed, Ctxt: CHOOSE not allowed in Audit Value\n");

		/* set correct error code */
		mg_util_set_ctxt_string(&errTxt,ctxtId);
		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		goto error;
	}
	/*--  CHOOSE Termination not allowed --*/
	else if ((NOTPRSNT != termId->type.pres)          &&
			(MGT_TERMID_CHOOSE == termId->type.val)) {

		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,	
				"failed, Term: CHOOSE not allowed in Audit Value\n");

		mg_util_set_term_string(&errTxt,termId);
		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		goto error;
	}

	/*-- For Audit Values, ROOT Termination is allowed with Context = ALL but not allowed with Other --*/
	/*-- Check whether the termination is present  in the given context --*/
	if (((NOTPRSNT != termId->type.pres) &&
				(MGT_TERMID_ROOT == termId->type.val))  &&
			((NOTPRSNT != ctxtId->type.pres)        &&
			 (MGT_CXTID_OTHER == ctxtId->type.val))) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,	
				"failed, Term: Invalid combination, ROOT Term with OTHER CONTEXT\n");

		mg_util_set_ctxt_string(&errTxt,ctxtId);
		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		goto error;
	}

     /*********************************************************************************************************************/
     /**************************** Preparing Response Structure ***********************************************************/
     /*********************************************************************************************************************/
	/*copy transaction-id*/
        memcpy(&reply.transId, &auditReq->transId,sizeof(MgMgcoTransId));
        /*copy context-id*/
        memcpy(&reply.contextId, &auditReq->contextId,sizeof(MgMgcoContextId));
        /*copy peer identifier */
        memcpy(&reply.peerId, &auditReq->peerId,sizeof(TknU32));

        /*fill response structue */
        if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&reply.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
                return ret;
        }

	reply.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.val = MGT_AUDITVAL;


	adtRep = &(reply.u.mgCmdRsp[0]->u.aval);

	adtRep->type.pres = PRSNT_NODEF;
	adtRep->type.val = MGT_TERMAUDIT;
	adtRep->u.other.pres.pres = PRSNT_NODEF;
	mgUtlAllocMgMgcoTermIdLst(&adtRep->u.other.termIdLst, term_list);

	/* NOW for each requested AUDIT descriptor we need to add entry to adtRep->u.other.audit.parms list */

     /*********************************************************************************************************************/
     /**************************** Processing Audit Request Descriptors **************************************************/
     /*********************************************************************************************************************/

	for (i = 0; i < audit_desc->num.val; i++) {

		audit_item = audit_desc->al[i];

		if (!audit_item) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,	
					"Audit Descriptor is NULL.. rejecting \n");
			return SWITCH_STATUS_FALSE; 
		}

		/*TODO - If we are not supporint AUDIT type then can send "MGT_MGCO_RSP_CODE_UNSUPPORTED_DESC" error to MG stack */
		if (NOTPRSNT != audit_item->auditItem.pres) {

			switch(audit_item->auditItem.val)
			{  
				case MGT_MEDIADESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing MEDIA \n");

						/* Grow the list of reply parameters */
						if (mgUtlGrowList((void ***)&adtRep->u.other.audit.parms, sizeof(MgMgcoAudRetParm),
									&adtRep->u.other.audit.num, &reply.u.mgCmdRsp[0]->memCp) != ROK)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
							return SWITCH_STATUS_FALSE;
						}

						numOfParms = adtRep->u.other.audit.num.val;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.pres = PRSNT_NODEF;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.val  = MGT_MEDIADESC;

						media = get_default_media_desc();
						if(!media){
							return SWITCH_STATUS_FALSE;
						}
						mgUtlCpyMgMgcoMediaDesc(&adtRep->u.other.audit.parms[numOfParms - 1]->u.media, media, &reply.u.mgCmdRsp[0]->memCp);

						break;
					}
				case MGT_MODEMDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing MODEM \n");
						break;
					}
				case MGT_MUXDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing MULTIPLEX \n");
						break;
					}
				case MGT_REQEVTDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing Events \n");
						break;
					}
				case MGT_SIGNALSDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing Signals \n");
						break;
					}
				case MGT_DIGMAPDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing Digit Maps \n");
						break;
					}
				case MGT_OBSEVTDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing Buffer Events \n");
						break;
					}
				case MGT_EVBUFDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing  Events Buffer \n");
						break;
					}
				case MGT_STATSDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing  Statistics \n");
						break;
					}
				case MGT_PKGSDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Auditing  Packages \n");
						/* Grow the list of reply parameters */
						if (mgUtlGrowList((void ***)&adtRep->u.other.audit.parms, sizeof(MgMgcoAudRetParm),
									&adtRep->u.other.audit.num, &reply.u.mgCmdRsp[0]->memCp) != ROK)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
							return SWITCH_STATUS_FALSE;
						}

						numOfParms = adtRep->u.other.audit.num.val;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.pres = PRSNT_NODEF;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.val  = MGT_PKGSDESC;

						if(SWITCH_STATUS_FALSE == mg_build_pkg_desc(&adtRep->u.other.audit.parms[numOfParms - 1]->u.pkgs)){
							return SWITCH_STATUS_FALSE;
						}

						break;
					}
				case MGT_INDAUD_TERMAUDDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Individual Term  Audit \n");
						break;
					}
				default:
					{
						switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Invalid Audit Descriptor[%d] request\n",audit_item->auditItem.val);
						err_code = MGT_MGCO_RSP_CODE_UNSUPPORTED_DESC;
						goto error;
					}
			}/*switch(audit_item->auditItem.val)*/
		}/*if (NOTPRSNT != audit_item->auditItem.pres)*/
	}/*for loop - audit_desc->num.val */

     /*********************************************************************************************************************/
     /**************************** Send Audit Command  Reply***************************************************************/
     /*********************************************************************************************************************/
	reply.cmdStatus.pres = PRSNT_NODEF;
	reply.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;
	reply.cmdType.pres = PRSNT_NODEF;
	reply.cmdType.val  = CH_CMD_TYPE_RSP;

	if(wild){                
		reply.u.mgCmdRsp[0]->wild.pres = PRSNT_NODEF;
	}


	/* send command reply */
	sng_mgco_send_cmd(suId, &reply);

	/* send indication to stack , so he can send response back to peer */
	memcpy(&ctxt.transId,&auditReq->transId,sizeof(MgMgcoTransId));
	memcpy(&ctxt.cntxtId, &auditReq->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &auditReq->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	sng_mgco_send_axn_req(suId, &ctxt);
	/***********************************************************************************************************************************/

	return SWITCH_STATUS_SUCCESS;

error:
	if (SWITCH_STATUS_SUCCESS == mg_build_mgco_err_request(&mgErr, auditReq->transId.val, ctxtId, err_code, &errTxt)) {
		sng_mgco_send_err(suId, mgErr);
	}

	/* deallocate the msg */
	mg_free_cmd(auditReq);
	return SWITCH_STATUS_FALSE; 
}

/*****************************************************************************************************************************/
switch_status_t mg_send_heartbeat_audit_rsp( SuId suId, MgMgcoCommand *auditReq)
{
	MgMgcoCtxt     ctxt;
	switch_status_t  ret;
	MgMgcoCommand    reply;
	MgMgcoTermIdLst  *term_list;
	MgMgcoTermId     *termId;
	MgMgcoSubAudReq  *audit;
	MgMgcoAuditReply  *adtRep = NULLP;

	memset(&reply, 0, sizeof(reply));
	audit 	   = &auditReq->u.mgCmdReq[0]->cmd.u.aval;

	if(NOTPRSNT == audit->pres.pres){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Audit structure not present..rejecting \n");
		return SWITCH_STATUS_FALSE;
	}

	/*-- Get termination list --*/
	term_list = mg_get_term_id_list(auditReq);
	termId    = term_list->terms[0];


	/*copy transaction-id*/
	memcpy(&reply.transId, &auditReq->transId,sizeof(MgMgcoTransId));
	/*copy context-id*/
	memcpy(&reply.contextId, &auditReq->contextId,sizeof(MgMgcoContextId));
	/*copy peer identifier */
	memcpy(&reply.peerId, &auditReq->peerId,sizeof(TknU32));

	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&reply.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
	}

	reply.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.val = MGT_AUDITVAL;


	adtRep = &(reply.u.mgCmdRsp[0]->u.aval);

	adtRep->type.pres = PRSNT_NODEF;
	adtRep->type.val = MGT_TERMAUDIT;
	adtRep->u.other.pres.pres = PRSNT_NODEF;
	adtRep->u.other.audit.num.pres = 0x00;
	mgUtlAllocMgMgcoTermIdLst(&adtRep->u.other.termIdLst, term_list);


	/* We will always send one command at a time..*/
	reply.cmdStatus.pres = PRSNT_NODEF;
	reply.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

	reply.cmdType.pres = PRSNT_NODEF;
	reply.cmdType.val  = CH_CMD_TYPE_RSP;

	ret = sng_mgco_send_cmd(suId, &reply);

	/*will send once all audit done*/
	memcpy(&ctxt.transId,&auditReq->transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, &auditReq->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &auditReq->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	ret = sng_mgco_send_axn_req(suId, &ctxt);

	return ret;
}

/*****************************************************************************************************************************/
#if 0
/* Kapil - Not using any more */
switch_status_t handle_media_audit( SuId suId, MgMgcoCommand *auditReq)
{
	switch_status_t  ret;
	MgMgcoCommand    reply;
	MgMgcoTermIdLst  *term_list;
	MgMgcoTermId     *termId;
	MgMgcoSubAudReq  *audit;
	MgMgcoAuditDesc  *audit_desc;
	MgMgcoAuditReply  *adtRep = NULLP;
	U16                    numOfParms;
	MgMgcoMediaDesc* media;


	memset(&reply, 0, sizeof(reply));
	audit 	   = &auditReq->u.mgCmdReq[0]->cmd.u.aval;

	if(NOTPRSNT == audit->pres.pres){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Audit structure not present..rejecting \n");
		return SWITCH_STATUS_FALSE;
	}

	audit_desc = &audit->audit;

	if((NOTPRSNT == audit_desc->pres.pres) || ( NOTPRSNT == audit_desc->num.pres)){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Audit Descriptor not present..rejecting \n");
		return SWITCH_STATUS_FALSE;
	}

	/* dump AUDIT message information */
	/*mgAccEvntPrntMgMgcoSubAudReq(auditReq,stdout);*/

	/*-- Get termination list --*/
	term_list = mg_get_term_id_list(auditReq);
	termId    = term_list->terms[0];


	/*copy transaction-id*/
	memcpy(&reply.transId, &auditReq->transId,sizeof(MgMgcoTransId));
	/*copy context-id*/
	memcpy(&reply.contextId, &auditReq->contextId,sizeof(MgMgcoContextId));
	/*copy peer identifier */
	memcpy(&reply.peerId, &auditReq->peerId,sizeof(TknU32));

	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&reply.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
	}

	reply.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.val = MGT_AUDITVAL;


	adtRep = &(reply.u.mgCmdRsp[0]->u.aval);

	adtRep->type.pres = PRSNT_NODEF;
	adtRep->type.val = MGT_TERMAUDIT;
	adtRep->u.other.pres.pres = PRSNT_NODEF;
	mgUtlAllocMgMgcoTermIdLst(&adtRep->u.other.termIdLst, term_list);

	/* Grow the list of reply parameters */
	if (mgUtlGrowList((void ***)&adtRep->u.other.audit.parms, sizeof(MgMgcoAudRetParm),
				&adtRep->u.other.audit.num, &reply.u.mgCmdRsp[0]->memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
		return SWITCH_STATUS_FALSE;
	}

	numOfParms = adtRep->u.other.audit.num.val;
	adtRep->u.other.audit.parms[numOfParms - 1]->type.pres = PRSNT_NODEF;
	adtRep->u.other.audit.parms[numOfParms - 1]->type.val  = MGT_MEDIADESC;

	media = get_default_media_desc();
	if(!media){
		return SWITCH_STATUS_FALSE;
	}
	mgUtlCpyMgMgcoMediaDesc(&adtRep->u.other.audit.parms[numOfParms - 1]->u.media, media, &reply.u.mgCmdRsp[0]->memCp);

	/* We will always send one command at a time..*/
	reply.cmdStatus.pres = PRSNT_NODEF;
	reply.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

	reply.cmdType.pres = PRSNT_NODEF;
	reply.cmdType.val  = CH_CMD_TYPE_RSP;


	ret = sng_mgco_send_cmd(suId, &reply);

#if 0
	/*will send once all audit done*/
	memcpy(&ctxt.transId,&auditReq->transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, &auditReq->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &auditReq->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	ret = sng_mgco_send_axn_req(suId, &ctxt);
#endif

	return ret;



}

/*****************************************************************************************************************************/
switch_status_t handle_pkg_audit( SuId suId, MgMgcoCommand *auditReq)
{
	switch_status_t  ret;
	MgMgcoCommand    reply;
	MgMgcoTermIdLst  *term_list;
	MgMgcoTermId     *termId;
	MgMgcoSubAudReq  *audit;
	MgMgcoAuditDesc  *audit_desc;
	MgMgcoAuditReply  *adtRep = NULLP;
	U16                    numOfParms;

	memset(&reply, 0, sizeof(reply));
	audit 	   = &auditReq->u.mgCmdReq[0]->cmd.u.aval;

	if(NOTPRSNT == audit->pres.pres){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Audit structure not present..rejecting \n");
		return SWITCH_STATUS_FALSE;
	}

	audit_desc = &audit->audit;

	if((NOTPRSNT == audit_desc->pres.pres) || ( NOTPRSNT == audit_desc->num.pres)){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Audit Descriptor not present..rejecting \n");
		return SWITCH_STATUS_FALSE;
	}

	/* dump AUDIT message information */
	/*mgAccEvntPrntMgMgcoSubAudReq(auditReq,stdout);*/

	/*-- Get termination list --*/
	term_list = mg_get_term_id_list(auditReq);
	termId    = term_list->terms[0];


	/*copy transaction-id*/
	memcpy(&reply.transId, &auditReq->transId,sizeof(MgMgcoTransId));
	/*copy context-id*/
	memcpy(&reply.contextId, &auditReq->contextId,sizeof(MgMgcoContextId));
	/*copy peer identifier */
	memcpy(&reply.peerId, &auditReq->peerId,sizeof(TknU32));

	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&reply.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
	}

	reply.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	reply.u.mgCmdRsp[0]->type.val = MGT_AUDITVAL;


	adtRep = &(reply.u.mgCmdRsp[0]->u.aval);

	adtRep->type.pres = PRSNT_NODEF;
	adtRep->type.val = MGT_TERMAUDIT;
	adtRep->u.other.pres.pres = PRSNT_NODEF;
	mgUtlAllocMgMgcoTermIdLst(&adtRep->u.other.termIdLst, term_list);

	/* Grow the list of reply parameters */
	if (mgUtlGrowList((void ***)&adtRep->u.other.audit.parms, sizeof(MgMgcoAudRetParm),
				&adtRep->u.other.audit.num, &reply.u.mgCmdRsp[0]->memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
		return SWITCH_STATUS_FALSE;
	}

	numOfParms = adtRep->u.other.audit.num.val;
	adtRep->u.other.audit.parms[numOfParms - 1]->type.pres = PRSNT_NODEF;
	adtRep->u.other.audit.parms[numOfParms - 1]->type.val  = MGT_PKGSDESC;

	if(SWITCH_STATUS_FALSE == mg_build_pkg_desc(&adtRep->u.other.audit.parms[numOfParms - 1]->u.pkgs)){
		return SWITCH_STATUS_FALSE;
	}

	/* We will always send one command at a time..*/
	reply.cmdStatus.pres = PRSNT_NODEF;
	reply.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

	reply.cmdType.pres = PRSNT_NODEF;
	reply.cmdType.val  = CH_CMD_TYPE_RSP;


	ret = sng_mgco_send_cmd(suId, &reply);

#if 0
	/*will send once all audit done*/
	memcpy(&ctxt.transId,&auditReq->transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, &auditReq->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &auditReq->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	ret = sng_mgco_send_axn_req(suId, &ctxt);
#endif

	return ret;



}
#endif
/*****************************************************************************************************************************/
#if 0
/* Kapil - Not using any more */
switch_status_t mg_send_audit_rsp(SuId suId, MgMgcoCommand *req)
{
	MgMgcoCommand  cmd;
	int ret = 0x00;
	MgMgcoTermId  *termId;
	MgMgcoCtxt     ctxt;
	MgMgcoAuditReply  *adtRep = NULLP;

	memset(&cmd,0, sizeof(cmd));

	/*copy transaction-id*/
	memcpy(&cmd.transId, &req->transId,sizeof(MgMgcoTransId));

	/*copy context-id*/ /*TODO - in case of $ context should be generated by app, we should not simply copy incoming structure */
	memcpy(&cmd.contextId, &req->contextId,sizeof(MgMgcoContextId));

	/*copy peer identifier */
	memcpy(&cmd.peerId, &req->peerId,sizeof(TknU32));

	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&cmd.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
	}

	cmd.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->wild.pres = NOTPRSNT;
	cmd.u.mgCmdRsp[0]->type.val = MGT_AUDITVAL;

	adtRep = &(cmd.u.mgCmdRsp[0]->u.aval);

	/* Set type as Cxt Audit */
	MG_INIT_TOKEN_VALUE(&(adtRep->type), MGT_CXTAUDIT);
	/* Set no of Terminations to 1 */
	MG_INIT_TOKEN_VALUE(&(adtRep->u.cxt.num), 1);



	cmd.u.mgCmdRsp[0]->u.aval.u.cxt.num.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->u.aval.u.cxt.num.val  = 1;

	mgUtlAllocMgMgcoTermIdLst(&cmd.u.mgCmdRsp[0]->u.aval.u.cxt, &req->u.mgCmdReq[0]->cmd.u.aval.termIdLst);

#ifdef GCP_VER_2_1
	termId = cmd.u.mgCmdRsp[0]->u.add.termIdLst.terms[0];
#else
	termId = &(cmd.u.mgCmdRsp[0]->u.add.termId);
#endif
	mg_fill_mgco_termid(termId, (CONSTANT U8*)"term1",&req->u.mgCmdRsp[0]->memCp);

	/* We will always send one command at a time..*/
	cmd.cmdStatus.pres = PRSNT_NODEF;
	cmd.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

	cmd.cmdType.pres = PRSNT_NODEF;
	cmd.cmdType.val  = CH_CMD_TYPE_RSP;


	ret = sng_mgco_send_cmd(suId, &cmd);

	memcpy(&ctxt.transId,&req->transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, &req->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &req->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	ret = sng_mgco_send_axn_req(suId, &ctxt);

	return ret;
}
#endif

/*****************************************************************************************************************************/
switch_status_t mg_send_modify_rsp(SuId suId, MgMgcoCommand *req)
{
	MgMgcoCommand  cmd;
	int ret = 0x00;
	MgMgcoTermId  *termId;
	MgMgcoCtxt     ctxt;

	memset(&cmd,0, sizeof(cmd));

	/*copy transaction-id*/
	memcpy(&cmd.transId, &req->transId,sizeof(MgMgcoTransId));

	/*copy context-id*/ /*TODO - in case of $ context should be generated by app, we should not simply copy incoming structure */
	memcpy(&cmd.contextId, &req->contextId,sizeof(MgMgcoContextId));

	/*copy peer identifier */
	memcpy(&cmd.peerId, &req->peerId,sizeof(TknU32));

	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&cmd.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
	}

	cmd.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->type.val = MGT_MODIFY;
	cmd.u.mgCmdRsp[0]->u.mod.pres.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->u.mod.termIdLst.num.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->u.mod.termIdLst.num.val  = 1;

	mgUtlAllocMgMgcoTermIdLst(&cmd.u.mgCmdRsp[0]->u.mod.termIdLst, &req->u.mgCmdReq[0]->cmd.u.mod.termIdLst);

#ifdef GCP_VER_2_1
	termId = cmd.u.mgCmdRsp[0]->u.mod.termIdLst.terms[0];
#else
	termId = &(cmd.u.mgCmdRsp[0]->u.mod.termId);
#endif
	/*mg_fill_mgco_termid(termId, (char*)"term1",&req->u.mgCmdRsp[0]->memCp);*/

	/* We will always send one command at a time..*/
	cmd.cmdStatus.pres = PRSNT_NODEF;
	cmd.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

	cmd.cmdType.pres = PRSNT_NODEF;
	cmd.cmdType.val  = CH_CMD_TYPE_RSP;


	ret = sng_mgco_send_cmd(suId, &cmd);

	memcpy(&ctxt.transId,&req->transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, &req->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &req->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	ret = sng_mgco_send_axn_req(suId, &ctxt);

	return ret;
}

/*****************************************************************************************************************************/
/*****************************************************************************************************************************/
switch_status_t mg_send_subtract_rsp(SuId suId, MgMgcoCommand *req)
{
	MgMgcoCommand  cmd;
	int ret = 0x00;
	MgMgcoTermId  *termId;
	MgMgcoCtxt     ctxt;
	uint8_t        wild = 0x00;

	memset(&cmd,0, sizeof(cmd));

	wild = req->u.mgCmdReq[0]->wild.pres;

	/*copy transaction-id*/
	memcpy(&cmd.transId, &req->transId,sizeof(MgMgcoTransId));

	/*copy context-id*/ /*TODO - in case of $ context should be generated by app, we should not simply copy incoming structure */
	memcpy(&cmd.contextId, &req->contextId,sizeof(MgMgcoContextId));

	/*copy peer identifier */
	memcpy(&cmd.peerId, &req->peerId,sizeof(TknU32));

	/*fill response structue */
	if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&cmd.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
		return ret;
	}

	if(wild){
		cmd.u.mgCmdRsp[0]->wild.pres = PRSNT_NODEF;
	}

	cmd.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->type.val = MGT_SUB;
	cmd.u.mgCmdRsp[0]->u.sub.pres.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->u.sub.termIdLst.num.pres = PRSNT_NODEF;
	cmd.u.mgCmdRsp[0]->u.sub.termIdLst.num.val  = 1;

	mgUtlAllocMgMgcoTermIdLst(&cmd.u.mgCmdRsp[0]->u.sub.termIdLst, &req->u.mgCmdReq[0]->cmd.u.sub.termIdLst);

#ifdef GCP_VER_2_1
	termId = cmd.u.mgCmdRsp[0]->u.sub.termIdLst.terms[0];
#else
	termId = &(cmd.u.mgCmdRsp[0]->u.sub.termId);
#endif
	/*mg_fill_mgco_termid(termId, (char *)"term1",&req->u.mgCmdRsp[0]->memCp);*/

	/* We will always send one command at a time..*/
	cmd.cmdStatus.pres = PRSNT_NODEF;
	cmd.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

	cmd.cmdType.pres = PRSNT_NODEF;
	cmd.cmdType.val  = CH_CMD_TYPE_RSP;


	ret = sng_mgco_send_cmd(suId, &cmd);

	memcpy(&ctxt.transId,&req->transId,sizeof(MgMgcoTransId)); 
	memcpy(&ctxt.cntxtId, &req->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &req->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	ret = sng_mgco_send_axn_req(suId, &ctxt);

	return ret;
}
/*****************************************************************************************************************************/
U32 get_txn_id(){
	outgoing_txn_id++;
	return outgoing_txn_id;
}
/*****************************************************************************************************************************/
/* Note : API to send Service Change */
/* INPUT : 
*	method 			- Service change method type (can be MGT_SVCCHGMETH_RESTART/MGT_SVCCHGMETH_FORCED (please refer to sng_ss7/cm/mgt.h for more values))
*	MgServiceChangeReason_e - Service Change reason 	
*	SuId 			- Service User ID for MG SAP - it will be same like mg_profile_t->idx (refer to media_gateway_xml.c->mg_sap_id)
*	term_name 		- String format defined termination name
*/
switch_status_t  mg_send_service_change(SuId suId, const char* term_name, uint8_t method, MgServiceChangeReason_e reason,uint8_t wild) 
{
	MgMgcoSvcChgPar srvPar;
	MgMgcoTermId*   termId;
	switch_status_t ret;
	MgMgcoCommand   request;
	MgMgcoSvcChgReq      *svc;

	MG_ZERO(&srvPar, sizeof(MgMgcoSvcChgPar));
	MG_ZERO(&request, sizeof(request));

	

	if(SWITCH_STATUS_FALSE == (ret = mg_create_mgco_command(&request, CH_CMD_TYPE_REQ, MGT_SVCCHG))){
		goto err;
	}

	/*fill txn id */
	request.transId.pres = PRSNT_NODEF;
	request.transId.val  = get_txn_id();

	request.contextId.type.pres = PRSNT_NODEF;
	request.contextId.type.val = MGT_CXTID_NULL;

#if 0
	/* TODO - fill of below fields */
#ifdef GCP_MGCO
#ifdef GCP_VER_2_1
	MgMgcoSegNum          segNum;
	MgMgcoSegCmpl         segCmpl;
#endif
#endif  /* GCP_MGCO */
#endif
	request.cmdStatus.pres = PRSNT_NODEF;
	request.cmdStatus.val = CH_CMD_STATUS_END_OF_TXN;

	request.cmdType.pres = PRSNT_NODEF;
	request.cmdType.val  = CH_CMD_TYPE_REQ;

	svc = &request.u.mgCmdReq[0]->cmd.u.svc;

	if(SWITCH_STATUS_FALSE == (ret = mg_fill_svc_change(&svc->parm, method, mg_service_change_reason[reason]))){
		return ret;
	}

	/*mgUtlCpyMgMgcoSvcChgPar(&svc->parm, &srvPar, &request.u.mgCmdReq[0]->memCp);*/

	if (mgUtlGrowList((void ***)&svc->termIdLst.terms, sizeof(MgMgcoTermIdLst),
				&svc->termIdLst.num, &request.u.mgCmdReq[0]->memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
		return SWITCH_STATUS_FALSE;
	}

#ifdef GCP_VER_2_1
	termId = svc->termIdLst.terms[0];
#else
	termId = &(svc->termId);
#endif


	mg_fill_mgco_termid(termId, (char*)term_name ,strlen(term_name), &request.u.mgCmdReq[0]->memCp);

	if(wild){
		request.u.mgCmdReq[0]->wild.pres = PRSNT_NODEF;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"Sending %s Service Change for termId[%s] with reason[%s], len[%d]\n",
			((1==wild)?"WildCard":"Non Wild Card"), term_name, svc->parm.reason.val, svc->parm.reason.len);

	sng_mgco_send_cmd(suId, &request);

	return SWITCH_STATUS_SUCCESS;

err:
	mgUtlDelMgMgcoSvcChgPar(&srvPar);
	return ret;
}
/*****************************************************************************************************************************/
