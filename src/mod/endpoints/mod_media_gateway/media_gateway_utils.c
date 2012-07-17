/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"
#include "media_gateway_stack.h"


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
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed mg_stack_alloc_mem: cmAllocEvnt return failure for _memSize=%d\n",(int)_memSize); 
		return SWITCH_STATUS_FALSE;
	}

	// Note: memset done inside stack api

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************************/

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
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed alloc_mg_stack_mem: get memory failed _memSize=%d\n", (int)_memSize );
		return SWITCH_STATUS_FALSE;
        }

        // Note: memset done inside stack api

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************************/

switch_status_t mg_stack_free_mem(void* msg)
{
        if ( !msg )
        {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, " Failed mg_stack_get_mem: invalid message\n"); 
		return SWITCH_STATUS_FALSE;
        }

        cmFreeMem( (Ptr)msg );

        return SWITCH_STATUS_SUCCESS;
}


/*****************************************************************************************************************************/

/* TODO - Matt - to see if term is in service or not */
switch_status_t mg_stack_termination_is_in_service(char* term_str,int len)
{
        return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************************/

S16 mg_fill_mgco_termid ( MgMgcoTermId  *termId, char* term_str, int term_len, CmMemListCp   *memCp)
{
#ifdef GCP_ASN       
	Size              size;
#endif
	S16               ret = ROK;

	termId->type.pres = PRSNT_NODEF;
	termId->type.val  = MGT_TERMID_OTHER;

	termId->name.dom.pres = NOTPRSNT;  
	termId->name.dom.len = 0x00;  

	termId->name.pres.pres = PRSNT_NODEF;
	termId->name.lcl.pres = PRSNT_NODEF;
	termId->name.lcl.len = term_len;
	/*MG_GETMEM(termId->name.lcl.val, termId->name.lcl.len , memCp, ret);*/
	ret = mg_stack_alloc_mem((Ptr*)&termId->name.lcl.val,term_len);

	printf("termId->name.lcl.val[%p]\n",termId->name.lcl.val);

	if( ret != ROK)
		RETVALUE(ret);          

	/*cmMemcpy((U8*)(termId->name.lcl.val), (CONSTANT U8*)term_str,termId->name.lcl.len);*/
	strncpy((char*)(termId->name.lcl.val), term_str, termId->name.lcl.len);
	termId->name.lcl.val[termId->name.lcl.len] = '\0';

	printf("mg_fill_mgco_termid: name.lcl.val[%s], len[%d], term_str[%s], term_len[%d]\n",termId->name.lcl.val, termId->name.lcl.len, term_str,term_len);
	      

#ifdef GCP_ASN
	if((termId->type.val == MGT_TERMID_ALL) ||
			(termId->type.val == MGT_TERMID_CHOOSE)){
		/* Remove comment to fill other term ID 
		   termId->wildcard.num.pres = NOTPRSNT; */ 
		/* Remove comment to fill wilcard term ID */
		termId->wildcard.num.pres = PRSNT_NODEF;
		termId->wildcard.num.val = 1;
		size = ((sizeof(MgMgcoWildcardField*)));
		MG_GETMEM((termId->wildcard.wildcard),size,memCp, ret);
		if( ret != ROK)
			RETVALUE(ret);

		MG_GETMEM( ((termId->wildcard.wildcard)[0]),sizeof(MgMgcoWildcardField),
				memCp, ret);
		if( ret != ROK)
			RETVALUE(ret);

		termId->wildcard.wildcard[0]->pres = PRSNT_NODEF;
		termId->wildcard.wildcard[0]->len = 1;
		termId->wildcard.wildcard[0]->val[0] = 0x55;

	}else{
		   termId->wildcard.num.pres = NOTPRSNT;
	}
#endif /* GCP_ASN */

	RETVALUE(ROK);
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

void mg_util_set_txn_string(MgStr  *errTxt, U32 *txnId)
{
	MG_ZERO(errTxt->val, sizeof(errTxt->val));
	errTxt->len = 0;

	errTxt->val[errTxt->len] = '\"';
	errTxt->len += 1;

	if (MG_TXN_INVALID == txnId )
	{
		MG_MEM_COPY((&errTxt->val[errTxt->len]), "TransactionId=0", 15);
		errTxt->len += 15;
	}           

	errTxt->val[errTxt->len] = '\"';
	errTxt->len += 1;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s:" 
			"info, error-text is: %s\n", __PRETTY_FUNCTION__,errTxt->val);

}

/*****************************************************************************************************************************/
void mg_util_set_ctxt_string ( MgStr  *errTxt, MgMgcoContextId     *ctxtId)
{
	MG_ZERO((errTxt->val), sizeof(errTxt->val));
	errTxt->len = 0;
	if(ctxtId->type.pres != NOTPRSNT)
	{
		errTxt->val[errTxt->len] = '\"';
		errTxt->len += 1;
		if(ctxtId->type.val == MGT_CXTID_NULL)
		{
			errTxt->val[errTxt->len] = '-';
			errTxt->len    += 1;
		}
		else if(ctxtId->type.val == MGT_CXTID_ALL)
		{
			errTxt->val[errTxt->len] = '*';
			errTxt->len    += 1;
		}
		else if(ctxtId->type.val == MGT_CXTID_CHOOSE)
		{
			errTxt->val[errTxt->len] = '$';
			errTxt->len    += 1;
		}
		else if((ctxtId->type.val == MGT_CXTID_OTHER) && (ctxtId->val.pres != NOTPRSNT))
		{
#ifdef BIT_64
			sprintf((char*)&errTxt->val[errTxt->len], "%d", ctxtId->val.val);
#else
			sprintf((char*)&errTxt->val[errTxt->len], "%lu", ctxtId->val.val);
#endif
			errTxt->len += cmStrlen((U8*)(&errTxt->val[errTxt->len]));
		}

		errTxt->val[errTxt->len] = '\"';
		errTxt->len += 1;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s:" 
			"info, error-text is: %s\n", __PRETTY_FUNCTION__,errTxt->val);
}

/*****************************************************************************************************************************/

void mg_util_set_cmd_name_string (MgStr *errTxt, MgMgcoCommand       *cmd)
{
	MG_ZERO((errTxt->val), sizeof(errTxt->val));
	errTxt->len = 0;

	if ((!cmd) && (!cmd->u.mgCmdInd[0])) {
		switch(cmd->u.mgCmdInd[0]->cmd.type.val)
		{
			case MGT_AUDITCAP:
				errTxt->val[0]='\"';
				errTxt->val[1]='A';
				errTxt->val[2]='u';
				errTxt->val[3]='d';
				errTxt->val[4]='i';
				errTxt->val[5]='t';
				errTxt->val[6]='C';
				errTxt->val[7]='a';
				errTxt->val[8]='p';
				errTxt->val[9]='a';
				errTxt->val[10]='b';
				errTxt->val[11]='i';
				errTxt->val[12]='l';
				errTxt->val[13]='i';
				errTxt->val[14]='t';
				errTxt->val[15]='y';
				errTxt->val[16]='\"';
				errTxt->len = 17;
				break;

			case MGT_MOVE:
				errTxt->val[0]='\"';
				errTxt->val[1]='M';
				errTxt->val[2]='o';
				errTxt->val[3]='v';
				errTxt->val[4]='e';
				errTxt->val[5]='\"';
				errTxt->len = 6;
				break;

			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s: Not expected command Type[%d]\n",
						__PRETTY_FUNCTION__,cmd->u.mgCmdInd[0]->cmd.type.val);

				break;
		}
	}
}

/*****************************************************************************************************************************/
void mgco_print_sdp(CmSdpInfoSet *sdp)
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

/*****************************************************************************************************************************/
void mg_util_set_term_string ( MgStr  *errTxt, MgMgcoTermId   *termId) 
{
	MG_ZERO((errTxt->val), sizeof(errTxt->val));
	errTxt->len = 0;

	if(termId->type.pres != NOTPRSNT)
	{
		errTxt->val[errTxt->len] = '\"';
		errTxt->len += 1;

		if(termId->type.val == MGT_TERMID_ROOT)
		{
			MG_MEM_COPY((&errTxt->val[errTxt->len]), "ROOT", 4);
			errTxt->len += 4;
		}
		else if(termId->type.val == MGT_TERMID_ALL)
		{
			errTxt->val[errTxt->len] = '*';
			errTxt->len    += 1;
		}
		else if(termId->type.val == MGT_TERMID_CHOOSE)
		{
			errTxt->val[errTxt->len] = '$';
			errTxt->len    += 1;
		}
		else if((termId->type.val == MGT_TERMID_OTHER) && (termId->name.pres.pres != NOTPRSNT))
		{
			if(termId->name.lcl.pres != NOTPRSNT)
			{
				MG_MEM_COPY(&(errTxt->val[errTxt->len]), termId->name.lcl.val, sizeof(U8) * termId->name.lcl.len);
				errTxt->len += termId->name.lcl.len;
			}
			if(termId->name.dom.pres != NOTPRSNT)
			{
				MG_MEM_COPY(&(errTxt->val[errTxt->len]),
						termId->name.dom.val, sizeof(U8) * termId->name.dom.len);
				errTxt->len += termId->name.dom.len;
			}
		}
		errTxt->val[errTxt->len] = '\"';
		errTxt->len += 1;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s:" 
			"info, error-text is: %s\n", __PRETTY_FUNCTION__,errTxt->val);
}
/*****************************************************************************************************************************/
MgMgcoMediaDesc* get_default_media_desc()
{
	MgMgcoMediaDesc   *media = NULL;
	MgMgcoMediaPar    *mediaPar = NULL;
	MgMgcoTermStateParm *trmStPar = NULL;

	mg_stack_alloc_mem((Ptr)&media, sizeof(MgMgcoMediaDesc));

	if (!media) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		return NULL;	
	}
	media->num.pres = PRSNT_NODEF;
	media->num.val = 1;
	mg_stack_alloc_mem((Ptr)&mediaPar, sizeof(MgMgcoMediaPar));

	if (!mediaPar) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem(media);
		return NULL;	
	}
	mg_stack_alloc_mem((Ptr)&media->parms, sizeof(MgMgcoMediaPar *));

	if (!media->parms) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem((void*)mediaPar);
		mg_stack_free_mem((void*)media);
		return NULL;	
	}
	mediaPar->type.pres = PRSNT_NODEF;
	mediaPar->type.val = MGT_MEDIAPAR_TERMST;
	mediaPar->u.tstate.numComp.pres = PRSNT_NODEF;
	mediaPar->u.tstate.numComp.val = 1;
	mg_stack_alloc_mem((Ptr)&trmStPar, sizeof(MgMgcoTermStateParm));

	if (!trmStPar) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem((void*)mediaPar);
		mg_stack_free_mem((void*)media->parms);
		mg_stack_free_mem((void*)media);
		return NULL;	
	}
	mg_stack_alloc_mem((Ptr)&mediaPar->u.tstate.trmStPar, sizeof(MgMgcoTermStateParm *));
	if (!mediaPar->u.tstate.trmStPar) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem((void*)trmStPar);
		mg_stack_free_mem((void*)mediaPar);
		mg_stack_free_mem((void*)media->parms);
		mg_stack_free_mem((void*)media);
		return NULL;	
	}
	trmStPar->type.pres = PRSNT_NODEF;
	trmStPar->type.val = MGT_TERMST_SVCST;
	trmStPar->u.svcState.pres = PRSNT_NODEF;
	/*TODO - ADD CHECK if term is in svc or not */
	trmStPar->u.svcState.val = MGT_SVCST_INSVC;

	mediaPar->u.tstate.trmStPar[0] = trmStPar; 
	media->parms[0] = mediaPar;

	return media;
}
/*****************************************************************************************************************************/

switch_status_t  mg_fill_svc_change(MgMgcoSvcChgPar  *srvPar, uint8_t  method, const char  *reason)
{
	MG_SET_TKN_VAL_PRES(&srvPar->pres, 0, PRSNT_NODEF);
	MG_SET_TKN_VAL_PRES(&srvPar->meth.pres, 0, PRSNT_NODEF);
	MG_SET_TKN_VAL_PRES(&srvPar->meth.type, method, PRSNT_NODEF);

	/* Set the reason */
	srvPar->reason.pres = PRSNT_NODEF;
	srvPar->reason.len  = cmStrlen((const U8 *)reason);

	mg_stack_alloc_mem((Ptr*)&srvPar->reason.val, srvPar->reason.len);
	if (NULL == srvPar->reason.val)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "failed, memory alloc\n");
		return SWITCH_STATUS_FALSE;
	}

	strncpy((char*)srvPar->reason.val,
			(const char *)reason,
			srvPar->reason.len);

	srvPar->reason.val[srvPar->reason.len] = '\0';

	mg_get_time_stamp(&srvPar->time);

	printf("reason[%s], len[%d]\n",srvPar->reason.val, srvPar->reason.len);


	return SWITCH_STATUS_SUCCESS;
} 
/*****************************************************************************************************************************/

void mg_get_time_stamp(MgMgcoTimeStamp *timeStamp)
{
	DateTime dt;           
	Txt      dmBuf[16];   
	U32 	    usec;

	usec = 0;

	/*-- Get system date and time via Trillium stack API --*/
	SGetRefDateTimeAdj(0, 0, &dt, &usec);

	/*-- Now fill the time and date in the target --*/
	MG_ZERO(&dmBuf[0], 16);

	sprintf(dmBuf, "%04d%02d%02d",
			(S16)(dt.year) + 1900, (S16)(dt.month), (S16)(dt.day));
	cmMemcpy((U8*) &timeStamp->date.val[0], (U8*) &dmBuf[0], 8);

	MG_ZERO(&dmBuf[0], 16);
	sprintf(dmBuf, "%02d%02d%02d%02d",
			(S16)(dt.hour), (S16)(dt.min), (S16)(dt.sec), (S16)(usec/10000));
	cmMemcpy((U8*) &timeStamp->time.val[0], (U8*) &dmBuf[0], 8);

	/*-- Setup the other stuff --*/
	timeStamp->pres.pres = PRSNT_NODEF;
	timeStamp->date.pres = PRSNT_NODEF;
	timeStamp->date.len  = 8;
	timeStamp->time.pres = PRSNT_NODEF;
	timeStamp->time.len  = 8;

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"mg_get_time_stamp: time(%s)\n", dmBuf);
}
/*****************************************************************************************************************************/
switch_status_t  mg_create_mgco_command(MgMgcoCommand  *cmd, uint8_t apiType, uint8_t cmdType)
{
	MgMgcoCommandReq     *cmdReq;
	MgMgcoCmdReply       *cmdRep;
	switch_status_t        ret;

	cmdReq = NULL;
	cmdRep = NULL;

	cmMemset((U8 *)cmd, 0, sizeof(MgMgcoCommand));

	MG_SET_VAL_PRES(cmd->cmdType, apiType);

	/* Allocate the event structure */
	switch(apiType)
	{
		/* For command Request */
		case CH_CMD_TYPE_REQ:
			{
				if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&cmd->u.mgCmdReq[0],sizeof(MgMgcoCommandReq)))){
					return ret;
				}

				if (NULL == cmd->u.mgCmdReq[0]) {
					switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"mg_create_mgco_command: failed, memory alloc\n");
					return SWITCH_STATUS_FALSE;
				}

				cmdReq = cmd->u.mgCmdReq[0];
				cmdReq->pres.pres = PRSNT_NODEF;
				cmdReq->cmd.type.pres = PRSNT_NODEF;
				cmdReq->cmd.type.val = cmdType;
				switch (cmdType)
				{
					case MGT_SVCCHG:
						cmdReq->cmd.u.svc.pres.pres = PRSNT_NODEF;
						break;

					case MGT_NTFY:
						cmdReq->cmd.u.ntfy.pres.pres = PRSNT_NODEF;
						break;
				} /* switch cmdType  */
				break;         
			}

			/* For command Response */
		case CH_CMD_TYPE_RSP:
			{
				if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&cmd->u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
					return ret;
				}

				if (NULL == cmd->u.mgCmdRsp[0]) {
					switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"mg_create_mgco_command: failed, memory alloc\n");
					return SWITCH_STATUS_FALSE;
				}
				cmdRep = cmd->u.mgCmdRsp[0]; 
				cmdRep->pres.pres = PRSNT_NODEF;
				cmdRep->type.pres = PRSNT_NODEF;
				cmdRep->type.val = cmdType;
				switch (cmdType)
				{
					case MGT_ADD:
						cmdRep->u.add.pres.pres = PRSNT_NODEF;
						break; 

					case MGT_MOVE:
						cmdRep->u.move.pres.pres = PRSNT_NODEF;
						break; 

					case MGT_MODIFY:
						cmdRep->u.mod.pres.pres = PRSNT_NODEF;
						break; 

					case MGT_SUB:  
						cmdRep->u.sub.pres.pres = PRSNT_NODEF;
						break; 

					case MGT_SVCCHG:  
						cmdRep->u.svc.pres.pres =  PRSNT_NODEF;
						break; 

					case MGT_AUDITVAL:  
						cmdRep->u.aval.type.pres =  PRSNT_NODEF;
						break; 
					case MGT_AUDITCAP:  
						cmdRep->u.acap.type.pres =  PRSNT_NODEF;
						break; 

				} /* switch cmdType  */
				break;         
			}

		default:
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"mg_create_mgco_command: failed, invalid Cmd type[%d]\n",apiType); 
			return SWITCH_STATUS_FALSE;
	} /* switch -apiType */

	return SWITCH_STATUS_SUCCESS;
}
/*****************************************************************************************************************************/

void mg_fill_null_context(MgMgcoContextId* ctxt)
{
	MG_SET_TKN_VAL_PRES(&ctxt->type, MGT_CXTID_NULL, PRSNT_NODEF);
}	
/*****************************************************************************************************************************/
