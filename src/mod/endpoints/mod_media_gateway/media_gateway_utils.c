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

	sMem.region = S_REG;
	sMem.pool = S_POOL;

	if ( _memSize <= 0 )
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Failed mg_stack_alloc_mem: invalid size\n"); 
		return SWITCH_STATUS_FALSE;
	}

	if ( ROK != cmAllocEvnt( _memSize, MG_MAXBLKSIZE, &sMem, _memPtr ) )
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Failed mg_stack_alloc_mem: cmAllocEvnt return failure for _memSize=%d\n",(int)_memSize); 
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Failed mg_stack_get_mem: invalid size\n"); 
		return SWITCH_STATUS_FALSE;
        }

        if ( !msg )
        {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Failed mg_stack_get_mem: invalid message\n"); 
		return SWITCH_STATUS_FALSE;
        }

        if ( cmGetMem( (Ptr)msg, _memSize, (Ptr*)_memPtr ) != ROK )
        {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Failed alloc_mg_stack_mem: get memory failed _memSize=%d\n", (int)_memSize );
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Failed mg_stack_get_mem: invalid message\n"); 
		return SWITCH_STATUS_FALSE;
        }

        cmFreeMem( (Ptr)msg );

        return SWITCH_STATUS_SUCCESS;
}


/*****************************************************************************************************************************/

/* To see if term is in service or not */
switch_status_t mg_stack_termination_is_in_service(megaco_profile_t* mg_profile, char* term_str,int len)
{
	mg_termination_t* term = NULL;
	term = megaco_find_termination(mg_profile, term_str);


	if(term && MG_TERM_RTP == term->type) {
		return SWITCH_STATUS_SUCCESS;
	}else if(term && (MG_TERM_TDM == term->type)){
		if(switch_test_flag(term, MG_IN_SERVICE)){
			return SWITCH_STATUS_SUCCESS;
		}else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " Term[%s] not in service\n", term_str); 
			return SWITCH_STATUS_FALSE;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Failed to find term for term string[%s]\n", term_str); 
		return SWITCH_STATUS_FALSE;
	}
}

/*****************************************************************************************************************************/

S16 mg_fill_mgco_termid ( MgMgcoTermId  *termId, char* term_str, int term_len, CmMemListCp   *memCp)
{
	char prnt_buf[128];
#ifdef GCP_ASN       
	Size              size;
#endif
	S16               ret = ROK;
	memset(&prnt_buf,0,sizeof(prnt_buf));

	termId->type.pres = PRSNT_NODEF;

	if(!strcmp(term_str,"ROOT")){
		termId->type.val  = MGT_TERMID_ROOT;
	} else {

		termId->type.val  = MGT_TERMID_OTHER;

		termId->name.dom.pres = NOTPRSNT;  
		termId->name.dom.len = 0x00;  

		termId->name.pres.pres = PRSNT_NODEF;

		termId->name.lcl.pres  = PRSNT_NODEF;
		termId->name.lcl.len  = cmStrlen((U8*)term_str);
		size = ((sizeof(U8)* term_len));
		MG_STACK_MEM_ALLOC(&termId->name.lcl.val, size);
		cmMemcpy((U8*)(termId->name.lcl.val),(CONSTANT U8*)term_str,termId->name.lcl.len);

		MG_MEM_COPY(&prnt_buf, termId->name.lcl.val, sizeof(U8) * termId->name.lcl.len);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
				"mg_fill_mgco_termid: Megaco termination name[%s], len[%d], input_term_str[%s], input_term_len[%d]\n",
				prnt_buf, termId->name.lcl.len, term_str,term_len);

		//MG_STACK_MEM_FREE(termId->name.lcl.val, size);
	}


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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: failed, Unsupported Command[%s]\n", __PRETTY_FUNCTION__, PRNT_MG_CMD(cmd_type));
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: failed, Unsupported Command[%s]\n", __PRETTY_FUNCTION__, PRNT_MG_CMD(cmd_type));
			} /* switch command type for reply */
			break;

		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: failed, Unsupported api_type[%s]!\n", __PRETTY_FUNCTION__, PRNT_MG_CMD_TYPE(api_type));
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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s:" 
			"info, error-text is: %s\n", __PRETTY_FUNCTION__,errTxt->val);

}

/*****************************************************************************************************************************/
void mg_util_set_err_string ( MgStr  *errTxt, char* str)
{
    MG_ZERO((errTxt->val), sizeof(errTxt->val));
    errTxt->len = strlen(str);
    strcpy((char*)&errTxt->val, str);
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
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s:" 
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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: Not expected command Type[%d]\n",
						__PRETTY_FUNCTION__,cmd->u.mgCmdInd[0]->cmd.type.val);

				break;
		}
	}
}

/*****************************************************************************************************************************/
void mgco_handle_sdp_attr_set(CmSdpAttrSet *s, mg_termination_t* term)
{
    int i=0x00;
    if (s->numComp.pres) {
        for (i = 0; i < s->numComp.val; i++) {
            CmSdpAttr *a = s->attr[i];

            if(NOTPRSNT == a->type.pres) continue;


            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Attribute Type[%d]\n",a->type.val);

            switch(a->type.val)
            {
                case CM_SDP_ATTR_GENERIC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_CAT:
                    {
                        break;
                    }

                case CM_SDP_ATTR_KEYWDS:
                    {
                        break;
                    }
                case CM_SDP_ATTR_TOOL:
                    {
                        break;
                    }
                case CM_SDP_ATTR_PTIME:
                    {
#ifdef BIT_64
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t PTIME  = %d \n", 
                                (NOTPRSNT != a->u.ptime.pres)?a->u.ptime.val:-1);
#else
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t PTIME  = %ld \n", 
                                (NOTPRSNT != a->u.ptime.pres)?a->u.ptime.val:-1);
#endif
			if(MG_TERM_RTP == term->type){
				term->u.rtp.ptime = a->u.ptime.val;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Updating ptime to [%d]\n", term->u.rtp.ptime);
			}
                        break;
                    }
                case CM_SDP_ATTR_RECVONLY:
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_ATTR_RECVONLY: \n");
                        break;
                    }
                case CM_SDP_ATTR_SENDRECV:
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_ATTR_SENDRECV: \n");
                        break;
                    }
                case CM_SDP_ATTR_SENDONLY:
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_ATTR_SENDONLY: \n");
                        break;
                    }
                case CM_SDP_ATTR_ORIENT:
                    {
                        break;
                    }
                case CM_SDP_ATTR_TYPE:
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_ATTR_TYPE: \n");
                        break;
                    }
                case CM_SDP_ATTR_CHARSET:
                    {
                        break;
                    }

                case CM_SDP_ATTR_SDPLANG:
                    {
                        break;
                    }

                case CM_SDP_ATTR_LANG:
                    {
                        break;
                    }
                case CM_SDP_ATTR_FRAMERATE:
                    {
                        break;
                    }
                case CM_SDP_ATTR_QUALITY:
                    {
                        break;
                    }
                case CM_SDP_ATTR_FMTP:
                    {
                        CmSdpAttrFmtp* f = &a->u.fmtp;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_ATTR_FMTP: \n");

                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t Format Type = %d \n",(NOTPRSNT != f->type.pres)?f->type.val:-1);

                        break;
                    }
                case CM_SDP_ATTR_RTPMAP:
                    {
                        CmSdpAttrRtpMap* r = &a->u.rtpmap;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_ATTR_RTPMAP: \n");

                        if(NOTPRSNT != r->pres.pres){

                            /* payload type */
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Payload Type = %d \n",
		                                 (NOTPRSNT != r->pay.type.pres)?r->pay.type.val:-1);

                            /* payload value */
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Payload Value = %d \n",
	        	                           (NOTPRSNT != r->pay.val.pres)?r->pay.val.val:-1);

                            /* encoding name */
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"\t Encoding Name value = %d \n",
	                                    (NOTPRSNT != r->enc.val.pres)?r->enc.val.val:-1);

			    if((NOTPRSNT != r->enc.val.pres ) && 
					    (CM_SDP_ENC_TELEPHONE_EVENT == r->enc.val.val)){
				
				    term->u.rtp.rfc2833_pt=r->pay.val.val;
				    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					" Updating rfc2833_pt to [%d] \n", term->u.rtp.rfc2833_pt);
			    }

                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"\t Encoding Name name = %s \n",
	                                    (NOTPRSNT != r->enc.name.pres)?
					   (char*)r->enc.name.val:"Not Present");

#ifdef BIT_64
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Clock Rate = %d \n",
		                                    (NOTPRSNT != r->clk.pres)?r->clk.val:-1);
#else
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Clock Rate = %ld \n",
		                                    (NOTPRSNT != r->clk.pres)?r->clk.val:-1);
#endif

                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Encoding Parameters = %s \n",
		                                    (NOTPRSNT != r->parms.pres)?
						    (char*)r->parms.val:"Not Present");
                        }
                        break;
                    }
                case CM_SDP_ATTR_INACTIVE:
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_ATTR_INACTIVE: \n");
                        break;
                    }
                case CM_SDP_ATTR_CONTROL:
                    {
                        break;
                    }
                case CM_SDP_ATTR_RANGE:
                    {
                        break;
                    }
                case CM_SDP_ATTR_ETAG:
                    {
                        break;
                    }
                case CM_SDP_ATTR_ATMMAP:
                    {
                        break;
                    }
                case CM_SDP_ATTR_EECID:
                    {
                        break;
                    }
                case CM_SDP_ATTR_AALTYPE:
                    {
                        break;
                    }
                case CM_SDP_ATTR_SILENCESUPP:
                    {
                        break;
                    }
                case CM_SDP_ATTR_ECAN:
                    {
                        break;
                    }
                case CM_SDP_ATTR_GC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_PROFILEDESC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_VSEL:
                    {
                        break;
                    }
                case CM_SDP_ATTR_DSEL:
                    {
                        break;
                    }
                case CM_SDP_ATTR_FSEL:
                    {
                        break;
                    }
                case CM_SDP_ATTR_CAPABILITY:
                    {
                        break;
                    }
                case CM_SDP_ATTR_QOSCLASS:
                    {
                        break;
                    }
                case CM_SDP_ATTR_BCOB:
                    {
                        break;
                    }
                case CM_SDP_ATTR_STC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_UPCC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_ATMQOSPARMS:
                    {
                        break;
                    }
                case CM_SDP_ATTR_AAL2QOSFPARMS:
                    {
                        break;
                    }
                case CM_SDP_ATTR_AAL2QOSBPARMS:
                    {
                        break;
                    }
                case CM_SDP_ATTR_ATMTRFCDESC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_AAL2FTRFCDESC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_AAL2BTRFCDESC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_ABRPARMS:
                    {
                        break;
                    }
                case CM_SDP_ATTR_CLKREC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_FEC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_PRTFL:
                    {
                        break;
                    }
                case CM_SDP_ATTR_BEARERTYPE:
                    {
                        break;
                    }
                case CM_SDP_ATTR_STRUCTURE:
                    {
                        break;
                    }
                case CM_SDP_ATTR_SBC:
                    {
                        break;
                    }
                case CM_SDP_ATTR_CPSSDUSIZE:
                    {
                        break;
                    }
                case CM_SDP_ATTR_AAL2CPS:
                    {
                        break;
                    }
                case CM_SDP_ATTR_ANYCAST:
                    {
                        break;
                    }
                case CM_SDP_ATTR_WTP:
                    {
                        break;
                    }
                case CM_SDP_ATTR_CACHE:
                    {
                        break;
                    }
                case CM_SDP_ATTR_CHAIN:
                    {
                        break;
                    }
                case CM_SDP_ATTR_PHONECONTEXT:
                    {
                        break;
                    }
                case CM_SDP_ATTR_CLIR:
                    {
                        break;
                    }
                case CM_SDP_ATTR_DIRECTION:
                    {
                        break;
                    }
                case CM_SDP_ATTR_MAXPTIME:
                    {
                        break;
                    }
                case CM_SDP_ATTR_T38_FAX:
		    {
			    CmSdpAttrT38Fax* f = &a->u.fax;
			    if(NOTPRSNT == f->type.pres) {
				    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						    "CM_SDP_ATTR_T38_FAX: TYPE not present \n");
				    break;
			    }

			    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					    "\t CM_SDP_ATTR_T38_FAX: type=%d\n", f->type.val);

			    if(NULL == term->u.rtp.t38_options){
				    term->u.rtp.t38_options = 
					    switch_core_alloc(term->pool, sizeof *term->u.rtp.t38_options);
			    }
			    switch(f->type.val)
			    {
				    case CM_SDP_ATTR_T38_FAX_VER:
					    {
						    term->u.rtp.t38_options->T38FaxVersion = f->u.num.val;
						    break;
					    }
				    case CM_SDP_ATTR_T38_MAX_BIT_RATE:
					    {
						    term->u.rtp.t38_options->T38MaxBitRate = f->u.num.val;
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_FILL_BIT_RMVL:
					    {
						    term->u.rtp.t38_options->T38FaxFillBitRemoval = f->u.num.val;
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_TRNS_MMR:
					    {
						    term->u.rtp.t38_options->T38FaxTranscodingMMR = f->u.num.val;
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_TRNS_JBIG:
					    {
						    term->u.rtp.t38_options->T38FaxTranscodingJBIG = f->u.num.val;
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_RATE_MNGMNT:
					    {
						    switch(f->u.val.val)
						    {
							    case CM_SDP_ATTR_T38_FAX_RATE_MNG_LOC_TCF:
								    {
									    term->u.rtp.t38_options->T38FaxRateManagement = 
										    switch_core_strdup(term->pool,"localTCF") ;
									    break;
								    }
							    case CM_SDP_ATTR_T38_FAX_RATE_MNG_TRANSF_TCF:
								    {
									    term->u.rtp.t38_options->T38FaxRateManagement = 
										    switch_core_strdup(term->pool,"transferredTCF") ;
									    break;
								    }
						    }
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_MAX_BFR:
					    {
						    term->u.rtp.t38_options->T38FaxMaxBuffer = f->u.num.val;
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_MAX_DATAGRAM:
					    {
						    term->u.rtp.t38_options->T38FaxMaxDatagram = f->u.num.val;
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_UDP_EC:
					    {
						    switch(f->u.val.val)
						    {
							    case CM_SDP_ATTR_T38_FAX_UDP_EC_UDP_FEC:
								    {
									    term->u.rtp.t38_options->T38FaxUdpEC = 
										    switch_core_strdup(term->pool,"t38UDPNoEC");
									    break;
								    }
							    case CM_SDP_ATTR_T38_FAX_UDP_EC_UDP_RED:
								    {
									    term->u.rtp.t38_options->T38FaxUdpEC = 
										    switch_core_strdup(term->pool,"t38UDPRedundancy") ;
									    break;
								    }
						    }
						    break;
					    }
				    case CM_SDP_ATTR_T38_FAX_UNKNOWN:
					    {
					     if(f->u.unknown.name.pres){
					       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					       "T38: Attribute : name[len=%d, value=%s] \n",
					         f->u.unknown.name.len,(char*)f->u.unknown.name.val);
						}
						if(f->u.unknown.val.pres){
					         switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						 "T38: Attribute : value[len=%d, value=%s] \n",
						   f->u.unknown.val.len,(char*)f->u.unknown.val.val);
						  }
						    break;
					    }

				    default:
					    break;
			    }


#if 0
			term->u.rtp.t38_options->T38FaxVersion = 0x01;
			term->u.rtp.t38_options->T38MaxBitRate = 14400;
			term->u.rtp.t38_options->T38FaxRateManagement = 
				switch_core_strdup(term->pool,"transferredTCF") ;
			//term->u.rtp.t38_options->T38FaxMaxBuffer = ;
			//term->u.rtp.t38_options->T38FaxMaxDatagram = ;
			term->u.rtp.t38_options->T38FaxUdpEC = 
				switch_core_strdup(term->pool,"t38UDPRedundancy") ;
			//term->u.rtp.t38_options->T38VendorInfo = 
#endif

			break;
		    }
                default:
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not supported Type[%d]\n",a->type.val);
                    break;
            }
        }
    }else{
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "a-line not present \n");
    }
}

void mgco_handle_sdp_c_line(CmSdpConn *s, mg_termination_t* term, mgco_sdp_types_e sdp_type)
{
    char ipadd[32];
    memset(ipadd, 0, sizeof(ipadd));

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "********** SDP connection line ****** \n");

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t Net Type = %d \n", 
            (NOTPRSNT != s->netType.type.pres)?s->netType.type.val:-1);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t Address Type = %d \n", 
            (NOTPRSNT != s->addrType.pres)?s->addrType.val:-1);

    if (s->addrType.pres && s->addrType.val == CM_SDP_ADDR_TYPE_IPV4 &&
            s->netType.type.val == CM_SDP_NET_TYPE_IN &&
            s->u.ip4.addrType.val == CM_SDP_IPV4_IP_UNI) {

        if (s->u.ip4.addrType.pres) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Address: %d.%d.%d.%d\n",
                    s->u.ip4.u.uniIp.b[0].val,
                    s->u.ip4.u.uniIp.b[1].val,
                    s->u.ip4.u.uniIp.b[2].val,
                    s->u.ip4.u.uniIp.b[3].val);

	    if(MG_SDP_REMOTE == sdp_type) {
		    sprintf(ipadd,"%d.%d.%d.%d",
				    s->u.ip4.u.uniIp.b[0].val,
				    s->u.ip4.u.uniIp.b[1].val,
				    s->u.ip4.u.uniIp.b[2].val,
				    s->u.ip4.u.uniIp.b[3].val);
		    /* update remote ip */
		    if(MG_TERM_RTP == term->type){
			    term->u.rtp.remote_addr = switch_core_strdup(term->pool,ipadd); 
			    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Update remote ip to [%s]\n", term->u.rtp.remote_addr);
		    }
	    }
	}
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "**************** \n");
}

void mgco_print_CmSdpU8OrNil(CmSdpU8OrNil* p)
{

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"CmSdpU8OrNil: Type = %d \n", (NOTPRSNT != p->type.pres)?p->type.val:-1); 
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"CmSdpU8OrNil: Value = %d \n", (NOTPRSNT != p->val.pres)?p->val.val:-1); 
}

const char* mg_get_codec_name(megaco_profile_t* mg_profile, int iana_code)
{
	int i = 0x00;      
	const switch_codec_implementation_t *codecs[16];
	char *codec_prefs[16] = { 0 };
	char *szcodec_prefs;
	int codec_count;
    const char *name = NULL;

	szcodec_prefs = strdup(mg_profile->codec_prefs);
	codec_count = switch_split(szcodec_prefs, ',', codec_prefs);

	/* Get the list of codecs, by preference */
	switch_loadable_module_get_codecs_sorted(codecs, switch_arraylen(codecs), codec_prefs, switch_arraylen(codec_prefs));
	/* see if received codec is present in our codec supported list */
	for (i = 0; codecs[i] && i < codec_count; i++) {
		if(iana_code == codecs[i]->ianacode){
			name = codecs[i]->iananame;
            break;
		}
	}

	return name;
}

void mgco_handle_sdp_media_param(CmSdpMedPar *s, mg_termination_t* term, mgco_sdp_types_e sdp_type, megaco_profile_t* mg_profile, CmSdpAttrSet  *attrSet, CmMemListCp  *memCp)
{
    int i=0x00;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "***** Media Parameter *********** \n");
    if (s->numProtFmts.pres) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Media Formats = %d \n", s->numProtFmts.val);
        for (i = 0; i < s->numProtFmts.val; i++) {
            CmSdpMedProtoFmts *a = s->pflst[i];

            /*Prot*/
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
				" Proto Type = %d \n", (NOTPRSNT != a->prot.type.pres)?a->prot.type.val:-1);
            switch(a->prot.type.val)
            {
                case CM_SDP_MEDIA_PROTO_UNKNOWN:
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " Proto Type Unknown , name = %s \n", 
                                (NOTPRSNT != a->prot.u.name.pres)?(char*)a->prot.u.name.val:"Not Present ");
                        break;
                    }
                case CM_SDP_MEDIA_PROTO_RTP:
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " Proto Type RTP , subtype = %d \n", 
                                (NOTPRSNT != a->prot.u.subtype.type.pres)?a->prot.u.subtype.type.val: -1);
                        break;
                    }
		case CM_SDP_MEDIA_PROTO_UDPTL:
		    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					" Proto Type T38 -UDPTL , subtype = %d \n", 
					(NOTPRSNT != a->prot.u.subtype.type.pres)?a->prot.u.subtype.type.val: -1);
		    }
            }

            /*repeated from "prot" field */
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " Proto Type  = %d \n", 
                    (NOTPRSNT != a->protType.pres)?a->protType.val: -1);

            switch(a->protType.val)
            {
                case CM_SDP_MEDIA_PROTO_RTP:
                    {
                        CmSdpMedFmtRtpList* r = &a->u.rtp;
                        int i = 0x00;
			term->u.rtp.media_type = MGM_AUDIO;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " CM_SDP_MEDIA_PROTO_RTP: \n"); 
                        if(NOTPRSNT != r->num.pres){
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						" Number of Formats[%d] \n", r->num.val); 

                            for(i=0;i<r->num.val;i++){
                                mgco_print_CmSdpU8OrNil(r->fmts[i]);

				if(MG_RTP_AVP_PROFILE_A_LAW == r->fmts[i]->val.val){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " MG_RTP_AVP_PROFILE_A_LAW: \n"); 
				}else if(MG_RTP_AVP_PROFILE_U_LAW == r->fmts[i]->val.val){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " MG_RTP_AVP_PROFILE_U_LAW: \n"); 
				}
                            }
                        }

			/* Ideally remote descriptor should have supported codec..but just in case calling remove un-supported codecs api */
			mg_rem_unsupported_codecs(mg_profile, term , r, attrSet, memCp);

			
			/* now whatever we have , that will be suported one */
			if((NOTPRSNT != r->num.pres) && (0 != r->num.val) && (NULL != r->fmts[0])){
					const char* name =  mg_get_codec_name(mg_profile, r->fmts[0]->val.val);
					if(name){
						if(MG_TERM_RTP == term->type){
							term->u.rtp.codec = name;
							term->u.rtp.pt = r->fmts[0]->val.val;
							/* Set default value of sampling rate depends on codec */
							switch(megaco_codec_parse(name))
							{
								case MEGACO_CODEC_PCMA:
									{
										term->u.rtp.rate = 8000; 
										break;
									}
								case MEGACO_CODEC_PCMU:
									{
										/* TODO - proper values */
										term->u.rtp.rate = 8000; 
										break;
									}
								case MEGACO_CODEC_G729:
									{
										/* TODO - proper values */
										term->u.rtp.rate = 8000; 
										break;
									}
								case MEGACO_CODEC_G723_1:
									{
										/* TODO - proper values */
										term->u.rtp.rate = 8000; 
										break;
									}
								case MEGACO_CODEC_ILBC:
									{
										/* TODO - proper values */
										term->u.rtp.rate = 8000; 
										break;
									}
								default:
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
											" not updating sampling rate \n"); 
									break;
							}

							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
									" Updating codec to[%d], name[%s] \n", 
									r->fmts[0]->val.val, name); 
									
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									"Updating pt to [%d], rate to[%d]\n", 
									term->u.rtp.pt, term->u.rtp.rate);
						}
					}else{
						/* ERROR */
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
									" NO Codec Name found against iana[%d] \n", r->fmts[0]->val.val); 
					}
			}

                        break;
                    }

                case CM_SDP_MEDIA_PROTO_UDPTL:
                    {
                        CmSdpMedFmtUdptlList* t = &a->u.t38;
			int i = 0;
		
			if(NOTPRSNT == t->num.pres) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				" CM_SDP_MEDIA_PROTO_UDPTL: no format defines..\n"); 
			}

			term->u.rtp.media_type = MGM_IMAGE;

                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					" CM_SDP_MEDIA_PROTO_UDPTL: formats[%d]\n", t->num.val); 

			for(i=0; i< t->num.val;i++)			
			{
				CmSdpT38Fmt* f = t->fmts[i];
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						" f->knownFmt.pres=%d, f->knownFmt.val=%d\n",
						  f->knownFmt.pres,f->knownFmt.val);
				if(f->unknownFmt.pres){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
							"f->unknownFmt.val[%s]\n",f->unknownFmt.val);
				}
			}

                        break;
                    }
                default:
                    break;
            }
        }
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "**************** \n");
}

void mgco_handle_incoming_sdp(CmSdpInfoSet *sdp, mg_termination_t* term, mgco_sdp_types_e sdp_type, megaco_profile_t* mg_profile, CmMemListCp     *memCp)
{
	int i;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					" Parsing %s SDP for Term[%s] \n", 
					(MG_SDP_LOCAL== sdp_type)?"MG_SDP_LOCAL":"MG_SDP_REMOTE", 
					(MG_TERM_RTP == term->type)?"MG_TERM_RTP":"MG_TERM_TDM");

	if (sdp->numComp.pres == NOTPRSNT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
				  " No %s SDP present \n", 
				(MG_SDP_LOCAL== sdp_type)?"MG_SDP_LOCAL":"MG_SDP_REMOTE");
		return;
	}

	for (i = 0; i < sdp->numComp.val; i++) {
		CmSdpInfo *s = sdp->info[i];
		int mediaId;

		/************************************************************************************************************************/
		/* info presence check */
		if(NOTPRSNT == s->pres.pres) continue;

		/************************************************************************************************************************/
		/* Version */
		if(NOTPRSNT != s->ver.pres) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					" SDP Version = %d \n", s->ver.val);
		}

		/************************************************************************************************************************/
		/* Orig */
		if(NOTPRSNT != s->orig.pres.pres) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"********** SDP orig line ****** \n \t Type = %d \n", 
					s->orig.type.val);

			if(NOTPRSNT != s->orig.orig.pres.pres) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t User Name = %s \n", 
						(NOTPRSNT != s->orig.orig.usrName.pres)?
						(char*)s->orig.orig.usrName.val:"Not Present");

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Session Id = %s \n", 
						(NOTPRSNT != s->orig.orig.sessId.pres)?
						(char*)s->orig.orig.sessId.val:"Not Present");

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Session Version = %s \n", 
						(NOTPRSNT != s->orig.orig.sessVer.pres)?
						(char*)s->orig.orig.sessVer.val:"Not Present");

				/* sdpAddr */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t Net Type = %d \n", 
						(NOTPRSNT != s->orig.orig.sdpAddr.netType.type.pres)?
						s->orig.orig.sdpAddr.netType.type.val:-1);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						 "\t Address Type = %d \n", 
						(NOTPRSNT != s->orig.orig.sdpAddr.addrType.pres)
						?s->orig.orig.sdpAddr.addrType.val:-1);

				/* print IPV4 address */
				if (s->orig.orig.sdpAddr.addrType.pres && 
					s->orig.orig.sdpAddr.addrType.val == CM_SDP_ADDR_TYPE_IPV4 &&
						s->orig.orig.sdpAddr.netType.type.val == CM_SDP_NET_TYPE_IN &&
						s->orig.orig.sdpAddr.u.ip4.addrType.val == CM_SDP_IPV4_IP_UNI) 
				    {

					if (s->orig.orig.sdpAddr.u.ip4.addrType.pres) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
								"Address: %d.%d.%d.%d\n",
								s->orig.orig.sdpAddr.u.ip4.u.ip.b[0].val,
								s->orig.orig.sdpAddr.u.ip4.u.ip.b[1].val,
								s->orig.orig.sdpAddr.u.ip4.u.ip.b[2].val,
								s->orig.orig.sdpAddr.u.ip4.u.ip.b[3].val);
					}

				}else{
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
								"\t O-line not present \n"); 
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "********** ****** \n");
			}
		} else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t O-line not present \n"); 
		}
/************************************************************************************************************************/
		/* Session Name (s = line) */

		if(NOTPRSNT != s->sessName.pres) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					   "\t Session Name = %s \n", s->sessName.val); 
		} else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t s-line not present \n"); 
		}

		/************************************************************************************************************************/
		/* Session Info(i= line) */

		if(NOTPRSNT != s->info.pres) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Session Info = %s \n", s->info.val); 
		} else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t i-line not present \n"); 
		}

		/************************************************************************************************************************/
		/* Session Uri */

		if(NOTPRSNT != s->uri.pres) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t Session Uri = %s \n", s->uri.val); 
		} else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t uri not present \n"); 
		}

		/************************************************************************************************************************/
		/* E-Mail */
		/* TODO */


		/************************************************************************************************************************/
		/* Phone */
		/* TODO */


		/************************************************************************************************************************/
		/* connection line */

		mgco_handle_sdp_c_line(&s->conn, term, sdp_type);
		/************************************************************************************************************************/
		/* Bandwidth */
		/* TODO */

		/************************************************************************************************************************/
		/* SDP Time (t= line)*/

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "*** t-line **************** \n");
		if(NOTPRSNT != s->sdpTime.pres.pres) {
			if(NOTPRSNT != s->sdpTime.sdpOpTimeSet.numComp.pres) {
				int i = 0x00;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"SDP op time present with total component[%d]\n", 
						s->sdpTime.sdpOpTimeSet.numComp.val);
				for (i = 0;i<s->sdpTime.sdpOpTimeSet.numComp.val;i++){
					CmSdpOpTime* t = s->sdpTime.sdpOpTimeSet.sdpOpTime[i];
					if(NOTPRSNT == t->pres.pres) continue;

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
							"\t Start Time = %s \n", 
							(NOTPRSNT != t->startTime.pres)?
							(char*)t->startTime.val:"Not Present");

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
							"\t Stop Time = %s \n", 
							(NOTPRSNT != t->stopTime.pres)?
							(char*)t->stopTime.val:"Not Present");

					/*repeat time repFieldSet */

					if(NOTPRSNT != t->repFieldSet.numComp.pres) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
								"SDP repeat time present with total component[%d]\n", 
								t->repFieldSet.numComp.val);

						/*TODO - print repeat fields */
					}else{
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
								 "SDP repeat time not present \n");
					}
				} /* sdpOpTimeSet.numComp for loop -- end */
			}else{/*sdpOpTimeSet.numComp.pres if -- end */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SDP op time not present \n");
			}

			/*TODO - zoneAdjSet */
		}else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "t-line not present \n");
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "**************** \n");


		/************************************************************************************************************************/
		/* key type (k= line)*/

		if(NOTPRSNT != s->keyType.pres.pres) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t Key Type = %d \n", 
					(NOTPRSNT != s->keyType.keyType.pres)?s->keyType.keyType.val:-1);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t Key Data = %s \n", 
					(NOTPRSNT != s->keyType.key_data.pres)?
					(char*)s->keyType.key_data.val:"Not Present");
		}else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "k-line not present \n");
		}

		/************************************************************************************************************************/
		/* Attribute Set */

		mgco_handle_sdp_attr_set(&s->attrSet, term);

		/************************************************************************************************************************/
		/* Media Descriptor Set */

		if (s->mediaDescSet.numComp.pres) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"****** Media Descriptor Set present with numComp[%d]\n", 
					s->mediaDescSet.numComp.val);
			for (mediaId = 0; mediaId < s->mediaDescSet.numComp.val; mediaId++) {
				CmSdpMediaDesc *desc = s->mediaDescSet.mediaDesc[mediaId];

				if(NOTPRSNT == desc->pres.pres) continue;

				/* Media Field */
				{
					CmSdpMediaField* f = &desc->field; 
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"\t Media Type = %d \n",
					(NOTPRSNT != f->mediaType.pres)?f->mediaType.val:-1);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"\t Media  = %s \n",(NOTPRSNT != f->media.pres)?
					(char*)f->media.val:"Not Present");
					
					/* Channel ID */
					if(NOTPRSNT != f->id.type.pres){
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"\t VcId Type = %d \n", f->id.type.val);
						switch(f->id.type.val){
							case CM_SDP_VCID_PORT:
								{
									CmSdpPort         *p = &f->id.u.port;
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CM_SDP_VCID_PORT:\n");
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t SDP port type = %d \n", (NOTPRSNT != p->type.pres)?p->type.val:-1);
									switch(p->type.val)
									{
										case CM_SDP_PORT_INT:
											{
												switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
														  "\t CM_SDP_PORT_INT: SDP port = %d  type = %d \n", 
														 p->u.portInt.port.val.val, p->u.portInt.port.type.val);
												if(MG_SDP_REMOTE == sdp_type) {
												/* update remote information */
													if(MG_TERM_RTP == term->type){
														term->u.rtp.remote_port 
															= p->u.portInt.port.val.val;
														switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Updating remote port to [%d]\n", term->u.rtp.remote_port);
													}
												}
												break;
											}
										case CM_SDP_PORT_VPCID:
											{
												switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t CM_SDP_PORT_VPCID: \n"); 
												break;
											}
										default:
											break;
									}
									break;
								}
							default:
								break;
						}
					}
					mgco_handle_sdp_media_param(&f->par, term, sdp_type, mg_profile, &desc->attrSet, memCp);
				}

				/*info */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
	"\t Info  = %s \n",(NOTPRSNT != desc->info.pres)?(char*)desc->info.val:"Not Present");

	/*connection set */
				{
					int cnt=0x00;
					if(NOTPRSNT != desc->connSet.numComp.pres){
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\t Number of Connection component[%d]\n",desc->connSet.numComp.val); 
						for(cnt=0;cnt<desc->connSet.numComp.val;cnt++){
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "************************\n");
							mgco_handle_sdp_c_line(desc->connSet.connSet[cnt], term, sdp_type);
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "************************\n");
						}
					}
				}

				/* attribute set */
				mgco_handle_sdp_attr_set(&desc->attrSet, term);


				if (desc->field.mediaType.val == CM_SDP_MEDIA_AUDIO &&
						desc->field.id.type.val ==  CM_SDP_VCID_PORT &&
						desc->field.id.u.port.type.val == CM_SDP_PORT_INT &&
						desc->field.id.u.port.u.portInt.port.type.val == CM_SDP_SPEC) {
					int port = desc->field.id.u.port.u.portInt.port.val.val;

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Port: %d\n", port);
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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s:" 
			"info, error-text is: %s\n", __PRETTY_FUNCTION__,errTxt->val);
}
/*****************************************************************************************************************************/
MgMgcoMediaDesc* get_default_media_desc(megaco_profile_t* mg_profile, MgMgcoTermId* termId, CmMemListCp   *memCp)
{
	MgMgcoMediaDesc   *media = NULL;
	MgMgcoMediaPar    *mediaPar = NULL;
	MgMgcoTermStateParm *trmStPar = NULL;
	S16 ret = ROK;

        MG_GETMEM(media, sizeof(MgMgcoMediaDesc) , memCp, ret);

	if (!media) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		return NULL;	
	}
	media->num.pres = PRSNT_NODEF;
	media->num.val = 1;

        MG_GETMEM(mediaPar, sizeof(MgMgcoMediaPar) , memCp, ret);

	if (!mediaPar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem(media);
		return NULL;	
	}
        MG_GETMEM(media->parms, sizeof(MgMgcoMediaPar*) , memCp, ret);

	if (!media->parms) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem((void*)mediaPar);
		mg_stack_free_mem((void*)media);
		return NULL;	
	}
	mediaPar->type.pres = PRSNT_NODEF;
	mediaPar->type.val = MGT_MEDIAPAR_TERMST;
	mediaPar->u.tstate.numComp.pres = PRSNT_NODEF;
	mediaPar->u.tstate.numComp.val = 1;

        MG_GETMEM(trmStPar, sizeof(MgMgcoTermStateParm) , memCp, ret);

	if (!trmStPar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem((void*)mediaPar);
		mg_stack_free_mem((void*)media->parms);
		mg_stack_free_mem((void*)media);
		return NULL;	
	}
        MG_GETMEM(mediaPar->u.tstate.trmStPar, sizeof(MgMgcoTermStateParm *) , memCp, ret);

	if (!mediaPar->u.tstate.trmStPar) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed, memory alloc\n"); 
		mg_stack_free_mem((void*)trmStPar);
		mg_stack_free_mem((void*)mediaPar);
		mg_stack_free_mem((void*)media->parms);
		mg_stack_free_mem((void*)media);
		return NULL;	
	}
	trmStPar->type.pres = PRSNT_NODEF;
	trmStPar->type.val = MGT_TERMST_SVCST;
	trmStPar->u.svcState.pres = PRSNT_NODEF;

	if ((NOTPRSNT != termId->type.pres)   &&
			(MGT_TERMID_ROOT == termId->type.val)){
		trmStPar->u.svcState.val = MGT_SVCST_INSVC;
	}else{
		/*not root termination */
		mg_termination_t* term = NULL;
		term = megaco_find_termination(mg_profile, (char*)termId->name.lcl.val);
		if(term && switch_test_flag(term, MG_OUT_OF_SERVICE)){
			trmStPar->u.svcState.val = MGT_SVCST_OUTOFSVC;
		}else{
			trmStPar->u.svcState.val = MGT_SVCST_INSVC;
		}
	}

	mediaPar->u.tstate.trmStPar[0] = trmStPar; 
	media->parms[0] = mediaPar;

	return media;
}
/*****************************************************************************************************************************/

switch_status_t  mg_fill_svc_change(MgMgcoSvcChgPar  *srvPar, uint8_t  method, const char  *reason, CmMemListCp   *memCp)
{
	S16               ret = ROK;
	MG_SET_TKN_VAL_PRES(&srvPar->pres, 0, PRSNT_NODEF);
	MG_SET_TKN_VAL_PRES(&srvPar->meth.pres, 0, PRSNT_NODEF);
	MG_SET_TKN_VAL_PRES(&srvPar->meth.type, method, PRSNT_NODEF);

	/* Set the reason */
	srvPar->reason.pres = PRSNT_NODEF;
	srvPar->reason.len  = cmStrlen((const U8 *)reason);

        MG_GETMEM(srvPar->reason.val, srvPar->reason.len , memCp, ret);
	if (NULL == srvPar->reason.val)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed, memory alloc\n");
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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"mg_get_time_stamp: time(%s)\n", dmBuf);
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"mg_create_mgco_command: failed, memory alloc\n");
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"mg_create_mgco_command: failed, memory alloc\n");
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"mg_create_mgco_command: failed, invalid Cmd type[%d]\n",apiType); 
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
switch_status_t  mg_util_build_obs_evt_desc (MgMgcoObsEvt *obs_event, MgMgcoRequestId *request_id, MgMgcoObsEvtDesc **ptr_obs_desc)
{
   MgMgcoObsEvtDesc *obs_desc = NULL;
 
   /* Check for valid request Id, if not then fill default value */
   if (NOTPRSNT == request_id->type.pres)
   {
      MG_SET_DEF_REQID(request_id);
   }
 
   mg_stack_alloc_mem((Ptr*)&obs_desc, sizeof(MgMgcoObsEvtDesc));
   if (NULL == obs_desc)
   {
       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate MgMgcoObsEvtDesc!\n");
       return SWITCH_STATUS_FALSE;
   }

   obs_desc->pres.pres = PRSNT_NODEF;
   MG_MEM_COPY(&obs_desc->reqId, request_id, sizeof(MgMgcoRequestId));
   obs_desc->el.num.pres = PRSNT_NODEF;
   obs_desc->el.num.val = 1;

   mg_stack_alloc_mem((Ptr*)&obs_desc->el.evts, sizeof(MgMgcoObsEvt*));
   if (NULL == obs_desc->el.evts)
   {
       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate MgMgcoObsEvt!\n");
       return SWITCH_STATUS_FALSE;
   }

   MG_MEM_COPY(obs_desc->el.evts[0], obs_event, sizeof(obs_event));

   *ptr_obs_desc = obs_desc;

   return SWITCH_STATUS_SUCCESS;
}
/*****************************************************************************************************************************/
void mg_print_time()
{
    time_t now;
    time(&now);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Current Time = %s", ctime(&now));
}
/*****************************************************************************************************************************/
switch_status_t mg_add_local_descriptor(MgMgcoMediaDesc* media, megaco_profile_t* mg_profile, mg_termination_t* term,CmMemListCp     *memCp)
{
	char* ipAddress[4];
	MgMgcoLocalDesc   *local; 
	CmSdpInfoSet      *psdp  = NULL;
	char		   * dup = NULL;
	switch_status_t    ret = SWITCH_STATUS_SUCCESS;
	CmSdpMediaDescSet* med = NULL;

	switch_assert(media);
	switch_assert(mg_profile);
	switch_assert(term);

	dup = strdup((char*)term->u.rtp.local_addr);
	switch_split(dup,'.',ipAddress);

	/* allocating mem for local descriptor */
	if (mgUtlGrowList((void ***)&media->parms, sizeof(MgMgcoMediaPar), &media->num, memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
		return SWITCH_STATUS_FALSE;
	}

	media->parms[media->num.val-1]->type.pres = PRSNT_NODEF;
	media->parms[media->num.val-1]->type.val = MGT_MEDIAPAR_LOCAL;

	local  = &media->parms[media->num.val-1]->u.local;

	local->pres.pres = PRSNT_NODEF;

	psdp = &(local->sdp);

	if (mgUtlGrowList((void ***)&psdp->info, sizeof(CmSdpInfo), &psdp->numComp, memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
		return SWITCH_STATUS_FALSE;
	}

	psdp->info[psdp->numComp.val-1]->pres.pres = PRSNT_NODEF;

	/* fill version */
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->ver),1);

	/* fill orig */
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->orig.pres), 1);
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->orig.type), CM_SDP_SPEC);
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->orig.orig.pres), 1);

	MG_SET_TKNSTROSXL(psdp->info[psdp->numComp.val-1]->orig.orig.usrName, 1, "-",
			memCp);	
	MG_SET_TKNSTROSXL(psdp->info[psdp->numComp.val-1]->orig.orig.sessId, 1, "0",
			memCp);
	MG_SET_TKNSTROSXL(psdp->info[psdp->numComp.val-1]->orig.orig.sessVer, 1, "0",
			memCp);	
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.netType.type),
			CM_SDP_NET_TYPE_IN);
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.addrType), 
			CM_SDP_ADDR_TYPE_IPV4);
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.addrType), 
			CM_SDP_IPV4_IP_UNI);
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.addrType),
			CM_SDP_IPV4_IP_UNI);
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[0]),
			atoi(ipAddress[0]));
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[1]),
			atoi(ipAddress[1]));
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[2]),
			atoi(ipAddress[2]));
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[3]),
			atoi(ipAddress[3]));

	/* fill session name */
	MG_SET_TKNSTROSXL(psdp->info[psdp->numComp.val-1]->sessName, 8, "SANGOMA", memCp);


	/* Fill the SDP Connection Info */
	/* "c=" line - ipaddress */
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->conn.netType.type),CM_SDP_NET_TYPE_IN);
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->conn.addrType), CM_SDP_ADDR_TYPE_IPV4);
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->conn.u.ip4.addrType), CM_SDP_IPV4_IP_UNI);

	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[0]), atoi(ipAddress[0]));
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[1]), atoi(ipAddress[1]));
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[2]), atoi(ipAddress[2]));
	MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[3]), atoi(ipAddress[3]));


	/* t= line */
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->sdpTime.pres),1);
#if 0
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->sdpTime.sdpOpTimeSet.numComp),0);
	MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->sdpTime.zoneAdjSet.numComp),0);
#endif

	med = &psdp->info[psdp->numComp.val-1]->mediaDescSet;
	ret = mg_add_lcl_media(med, mg_profile, term, memCp);

	return ret;
}
/*****************************************************************************************************************************/
switch_status_t mg_add_supported_media_codec(CmSdpMediaDesc* media, megaco_profile_t* mg_profile, mg_termination_t* term, CmMemListCp     *memCp)
{
	const switch_codec_implementation_t *codecs[16];
	char *codec_prefs[16] = { 0 };
	char *szcodec_prefs;
	int codec_count;
	int i;
	int fmt= 0x00;

	switch_assert(media);
	switch_assert(mg_profile);
	switch_assert(term);
	switch_assert(memCp);

	szcodec_prefs = strdup(mg_profile->codec_prefs);
	codec_count = switch_split(szcodec_prefs, ',', codec_prefs);

	/* Get the list of codecs, by preference */
	switch_loadable_module_get_codecs_sorted(codecs, switch_arraylen(codecs), codec_prefs, switch_arraylen(codec_prefs));
	for (i = 0; codecs[i] && i < codec_count; i++) {
		int pt = codecs[i]->ianacode;
		const char *name = codecs[i]->iananame;

		printf("Preference %d is %s/%d\n", i, name, pt);


		if (mgUtlGrowList((void ***)&media->field.par.pflst[media->field.par.numProtFmts.val-1]->u.rtp.fmts, sizeof(CmSdpU8OrNil),
					&media->field.par.pflst[media->field.par.numProtFmts.val-1]->u.rtp.num, memCp) != ROK)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
			return SWITCH_STATUS_FALSE;
		}
		fmt = media->field.par.pflst[media->field.par.numProtFmts.val-1]->u.rtp.num.val-1;

		MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[media->field.par.numProtFmts.val-1]->u.rtp.fmts[fmt]->type), CM_SDP_SPEC);

		MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[media->field.par.numProtFmts.val-1]->u.rtp.fmts[fmt]->val), pt);

		/* add associated attributes */
		{
			if (mgUtlGrowList((void ***)&media->attrSet.attr, sizeof(CmSdpAttr), &media->attrSet.numComp, memCp) != ROK)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
				return SWITCH_STATUS_FALSE;
			}

			MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->type),CM_SDP_ATTR_RTPMAP);
			MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->u.rtpmap.pres), 1);
			MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->u.rtpmap.pay.type), CM_SDP_SPEC);
			MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->u.rtpmap.pay.val), pt);
			MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->u.rtpmap.enc.val), CM_SDP_ENC_UNKNOWN);
			MG_SET_TKNSTROSXL((media->attrSet.attr[media->attrSet.numComp.val-1]->u.rtpmap.enc.name), strlen(name), name, memCp);
			MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->u.rtpmap.clk), codecs[i]->samples_per_second);
			/* encoding parameter not required to fill */
		}

	}
	free(szcodec_prefs);
	return SWITCH_STATUS_SUCCESS;
}
/*****************************************************************************************************************************/
switch_status_t mg_add_lcl_media(CmSdpMediaDescSet* med, megaco_profile_t* mg_profile, mg_termination_t* term, CmMemListCp     *memCp)
{
	CmSdpMediaDesc*    media;

	switch_assert(med);
	switch_assert(mg_profile);
	switch_assert(term);
	switch_assert(memCp);

	if (mgUtlGrowList((void ***)&med->mediaDesc, sizeof(CmSdpMediaDesc),
				&med->numComp, memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
		return SWITCH_STATUS_FALSE;
	}

	media = med->mediaDesc[med->numComp.val-1];

	MG_INIT_TOKEN_VALUE(&(media->pres),1);

	/* Fill CmSdpMediaField */
	MG_INIT_TOKEN_VALUE(&(media->field.pres),1);
	MG_INIT_TOKEN_VALUE(&(media->field.mediaType),CM_SDP_MEDIA_AUDIO);

	MG_INIT_TOKEN_VALUE(&(media->field.id.type),CM_SDP_VCID_PORT);
	MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.type),CM_SDP_PORT_INT);
	MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.pres),1);
	MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.port.type), CM_SDP_SPEC);
	MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.port.val), term->u.rtp.local_port);

	if (mgUtlGrowList((void ***)&media->field.par.pflst, sizeof(CmSdpMedProtoFmts),
				&media->field.par.numProtFmts, memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
		return SWITCH_STATUS_FALSE;
	}

	/* CmSdpMedProtoFmts */
	MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[media->field.par.numProtFmts.val-1]->prot.type), CM_SDP_MEDIA_PROTO_RTP);
	MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[media->field.par.numProtFmts.val-1]->prot.u.subtype.type), CM_SDP_PROTO_RTP_AVP);
	MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[media->field.par.numProtFmts.val-1]->protType), CM_SDP_MEDIA_PROTO_RTP);


	/***************************************************************************************************************************************************************/
	/* Fill ptime attribute */
	{
		if (mgUtlGrowList((void ***)&media->attrSet.attr, sizeof(CmSdpAttr), &media->attrSet.numComp, memCp) != ROK)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
			return SWITCH_STATUS_FALSE;
		}

		MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->type),CM_SDP_ATTR_PTIME);
		MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[media->attrSet.numComp.val-1]->u.ptime), term->u.rtp.ptime);
	}
	/***************************************************************************************************************************************************************/
	/* fill codec info */
	mg_add_supported_media_codec(media, mg_profile, term, memCp);
			
	return SWITCH_STATUS_SUCCESS;
}
/***************************************************************************************************************************************************************/

switch_status_t mg_rem_unsupported_codecs (megaco_profile_t* mg_profile, mg_termination_t* term, CmSdpMedFmtRtpList  *fmtList, CmSdpAttrSet  *attrSet, CmMemListCp     *memCp)
{
	int i = 0x00;      
	int id = 0x00;      
	int j = 0x00;         
	int a = 0x00;         
	CmSdpU8OrNil *fmt = NULL;    
	int foundCodec = 0x00;
	const switch_codec_implementation_t *codecs[16];
	char *codec_prefs[16] = { 0 };
	char *szcodec_prefs;
	int codec_count;
	CmSdpAttr      *attr = NULL;
	CmSdpAttrRtpMap   *rtp = NULL;

	/* Check if code list is present */
	if (!fmtList ||  (NOTPRSNT == fmtList->num.pres))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "codec List Not present\n");
		return SWITCH_STATUS_FALSE;
	}

	szcodec_prefs = strdup(mg_profile->codec_prefs);
	codec_count = switch_split(szcodec_prefs, ',', codec_prefs);

	/* Get the list of codecs, by preference */
	switch_loadable_module_get_codecs_sorted(codecs, switch_arraylen(codecs), codec_prefs, switch_arraylen(codec_prefs));


	/* codec type is specified one */

	/* loop through coddec list and remove un-supported codec */ 
	for(i = 0; i <  fmtList->num.val; i++)
	{
		foundCodec = 0x00;
		fmt = fmtList->fmts[i];

		if((NOTPRSNT == fmt->type.pres) || (NOTPRSNT == fmt->val.pres)) continue;

		if(CM_SDP_SPEC != fmt->type.val) continue;  /* TODO - need to see for other cases like CM_SDP_NIL/CM_SDP_CHOICE etc not sure as of now */ 


		/* see if received codec is present in our codec supported list */
		for (id = 0; codecs[id] && id < codec_count; id++) {
			int pt = codecs[id]->ianacode;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
				"Matching recv codec[%d] with supported codec[%d] \n", fmt->val.val, pt);
			//const char *name = codecs[id]->iananame;
			/* anything > 96 is dymanic we should skip codec match */
			if(fmt->val.val >= 96) {foundCodec = 0x01; break;}
			if(pt == fmt->val.val){
				foundCodec = 0x01;
				break;
			}
		}

		/* IF codec not found in list, remove it */
		if(!foundCodec) {

			for(j = i; j <  fmtList->num.val - 1; j++)
			{
				fmtList->fmts[j] = fmtList->fmts[j +1];
			}
			mgUtlShrinkList((Void ***)&fmtList->fmts, sizeof(CmSdpU8OrNil), &fmtList->num, memCp);
			i-- ;

			/* remove associated a= , if present */
			if((NOTPRSNT != attrSet->numComp.pres) && (0 != attrSet->numComp.val)){
				for(a = 0; a < attrSet->numComp.val; a++) {
					attr = attrSet->attr[a];

					if(CM_SDP_ATTR_RTPMAP != attr->type.val) continue; /* as of now only checking RTPMAP */

					rtp = &attr->u.rtpmap;

					if((NOTPRSNT != rtp->pres.pres) && (fmt->val.val == rtp->pay.val.val)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "a line found against codec[%d]..Removing a line  \n", fmt->val.val);

						/* mgUtlShrinkList API will delete last node from list, hence suffling list nodes */
						for(j = a; j <  attrSet->numComp.val - 1; j++)
						{
							attrSet->attr[j] = attrSet->attr[j +1];
						}
						mgUtlShrinkList((Void ***)&attrSet->attr, sizeof(CmSdpAttr), &attrSet->numComp, memCp);
					}
				}
			}
		}
	}

	if(0 == fmtList->num.val) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No Supported codec found in offer, Rejecting request \n");
		term->mg_error_code = switch_core_alloc(term->pool, sizeof(term->mg_error_code));
		*term->mg_error_code = MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT;
		
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}



/*****************************************************************************************************************************/

switch_status_t mg_build_sdp(MgMgcoMediaDesc* out, MgMgcoMediaDesc* inc, megaco_profile_t* mg_profile, mg_termination_t* term, CmMemListCp     *memCp)
{
	CmSdpU8OrNil 	   *fmt = NULL;    
	CmSdpInfoSet       *psdp  = NULL;
	char* 		   ipAddress[4];
	int 		   i = 0x00;
	int 		   j = 0x00;
	int 		   choose_codec = 0x00;
	int 		   k = 0x00;
	MgMgcoLocalDesc   *local = NULL; 
	MgMgcoLocalDesc   *remote = NULL; 
	int		   fresh_sdp = 0x00;
	char* 		   dup = NULL; 
	CmSdpMedProtoFmts *format=NULL;


	switch_assert(out);
	switch_assert(inc);
	switch_assert(mg_profile);
	switch_assert(term);

	dup = strdup((char*)term->u.rtp.local_addr);
	switch_split(dup,'.',ipAddress);


	if((NOTPRSNT == inc->num.pres) || (0 == inc->num.val)){
		fresh_sdp = 0x01;
	}

	/* if its fresh sdp then add only local descriptor */
	if(fresh_sdp) {
		mg_add_local_descriptor(out, mg_profile, term, memCp);
	} else {
		/* incoming has sdp, so copy that sdp and overwrite only local sdp */
		mgUtlCpyMgMgcoMediaDesc(out, inc, memCp);

		/* now see if we have local descriptor, then pick up that and modify the fields  */

		if((NOTPRSNT != out->num.pres) && (0 != out->num.val))
		{
			for(i=0; i<out->num.val; i++) {
				if(MGT_MEDIAPAR_LOCAL == out->parms[i]->type.val) {
					local  = &out->parms[i]->u.local;
				} else if(MGT_MEDIAPAR_STRPAR == out->parms[i]->type.val){
					MgMgcoStreamDesc *stream = &out->parms[i]->u.stream;
					if((NOTPRSNT != stream->sl.pres.pres) && (NOTPRSNT !=  stream->sl.local.pres.pres)){
						local  = &stream->sl.local;
					}
				}else if(MGT_MEDIAPAR_REMOTE == out->parms[i]->type.val) {
					remote  = &out->parms[i]->u.remote;
				}
			}
		}
	}


	if(!local || (NOTPRSNT == local->sdp.numComp.pres) || (0 == local->sdp.numComp.val)){
		/* local sdp is not part of media descriptor, then add local sdp*/
		mg_add_local_descriptor(out, mg_profile, term, memCp);
	}else{
		/* local sdp is present..  now go over the local descriptor and modify fields */
			psdp = &(local->sdp);

			for(i=0; i< psdp->numComp.val; i++) {
/**********************************************************************************************************************************/
				/* version - let it be same, if present else use version 1 */
				if(NOTPRSNT == psdp->info[psdp->numComp.val-1]->ver.pres) {
					MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->ver),1);
				}
/**********************************************************************************************************************************/
					/* orig (o- line) fill with our info */
MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->orig.pres), 1);
MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->orig.type), CM_SDP_SPEC);
MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->orig.orig.pres), 1);

MG_SET_TKNSTROSXL(psdp->info[psdp->numComp.val-1]->orig.orig.usrName, 1, "-", memCp);
MG_SET_TKNSTROSXL(psdp->info[psdp->numComp.val-1]->orig.orig.sessId, 1, "0", memCp);
MG_SET_TKNSTROSXL(psdp->info[psdp->numComp.val-1]->orig.orig.sessVer, 1, "0", memCp);
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.netType.type),
CM_SDP_NET_TYPE_IN);
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.addrType), 
							CM_SDP_ADDR_TYPE_IPV4);
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.addrType), 
							CM_SDP_IPV4_IP_UNI);
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.addrType),
							CM_SDP_IPV4_IP_UNI);
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[0]),
							atoi(ipAddress[0]));
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[1]),
							atoi(ipAddress[1]));
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[2]),
							atoi(ipAddress[2]));
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->orig.orig.sdpAddr.u.ip4.u.ip.b[3]),
							atoi(ipAddress[3]));

/**********************************************************************************************************************************/
					/* session-name , let it be like this if present, else skip it */
/**********************************************************************************************************************************/
					/* "c=" line - ipaddress */
MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->conn.netType.type),CM_SDP_NET_TYPE_IN);
MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->conn.addrType), CM_SDP_ADDR_TYPE_IPV4);
MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->conn.u.ip4.addrType), CM_SDP_IPV4_IP_UNI);

MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[0]), atoi(ipAddress[0]));
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[1]), atoi(ipAddress[1]));
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[2]), atoi(ipAddress[2]));
MG_SET_VAL_PRES( (psdp->info[psdp->numComp.val-1]->conn.u.ip4.u.uniIp.b[3]), atoi(ipAddress[3]));

/**********************************************************************************************************************************/
/* t= line */
MG_INIT_TOKEN_VALUE(&(psdp->info[psdp->numComp.val-1]->sdpTime.pres),1);
/**********************************************************************************************************************************/
/* fill media descriptors */
{
CmSdpMediaDescSet* med = &psdp->info[psdp->numComp.val-1]->mediaDescSet;
CmSdpMediaDesc*    media;

if((NOTPRSNT == med->numComp.pres) || (0 == med->numComp.val)){
	mg_add_lcl_media(med, mg_profile, term, memCp);
}else{
  for(j =0;j < med->numComp.val; j++){
     media = med->mediaDesc[j];
     /* check for choose port and fill the port */
     if(NOTPRSNT != media->field.id.type.pres){
	     //if(CM_SDP_VCID_CHOOSE == media->field.id.type.val)
		{
		     MG_INIT_TOKEN_VALUE(&(media->field.id.type),CM_SDP_VCID_PORT);
		     MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.type),CM_SDP_PORT_INT);
		     MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.pres),1);
		     MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.port.type), CM_SDP_SPEC);
		     MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.port.val), term->u.rtp.local_port);

	     }
     }

 /* check for codec */
if((NOTPRSNT == media->field.par.numProtFmts.pres) || 
		(0 == media->field.par.numProtFmts.val)){
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
			"No codec specified in incoming local descriptor \n");
	mg_add_supported_media_codec(media, mg_profile, term, memCp );	
}else{
/* check for media format/codec  */
 for(k =0;k < media->field.par.numProtFmts.val; k++){
format = media->field.par.pflst[k];
if ((NOTPRSNT != format->protType.pres) &&
   (CM_SDP_MEDIA_PROTO_RTP == format->protType.val))
{
  if((NOTPRSNT != format->u.rtp.num.pres)
			&&(0 != format->u.rtp.num.val))
  {
    /* If the codec type is CHOOSE then we need to fill our list */
    for(i = 0; i <  format->u.rtp.num.val; i++) {
	fmt = format->u.rtp.fmts[i];
	if((NOTPRSNT == fmt->type.pres) || (NOTPRSNT == fmt->val.pres)) continue;
	if(CM_SDP_CHOOSE == fmt->type.val){
		choose_codec = 0x1;
	}
  }
    if(choose_codec){
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
			"CHOOSE codec is requested fill out supported codecs \n");

	/* delete existing rtp format list..TODO find better way  */
	for(i = 0; i <  format->u.rtp.num.val; i++)
	{
		mgUtlShrinkList((Void ***)&format->u.rtp.fmts, sizeof(CmSdpU8OrNil), &format->u.rtp.num, memCp);
	}
	/* If the codec type is CHOOSE then we need to fill our list */
	mg_add_supported_media_codec(media, mg_profile, term, memCp);
    } else if (!choose_codec && 
		(SWITCH_STATUS_FALSE == 
		mg_rem_unsupported_codecs(mg_profile, term , &format->u.rtp, &media->attrSet, memCp)))
    {
	    return SWITCH_STATUS_FALSE;
    }
  }
}
}
}
}
}
}
/**********************************************************************************************************************************/
}


	}
	return SWITCH_STATUS_SUCCESS;
}
/*****************************************************************************************************************************/
void mg_print_t38_attributes(mg_termination_t* term)
{
	switch_assert(term);

	if((MG_TERM_RTP == term->type) && (term->u.rtp.t38_options)){
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, 
				"switch_t38_options_t for termination[%s]\n", term->name);
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxVersion[%d]\n",
				term->u.rtp.t38_options->T38FaxVersion); 
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38MaxBitRate[%d]\n",
				term->u.rtp.t38_options->T38MaxBitRate); 
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxFillBitRemoval[%d]\n",
				term->u.rtp.t38_options->T38FaxFillBitRemoval); 
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxTranscodingMMR[%d]\n",
				term->u.rtp.t38_options->T38FaxTranscodingMMR); 
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxTranscodingJBIG[%d]\n",
				term->u.rtp.t38_options->T38FaxTranscodingJBIG); 
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxRateManagement[%s]\n",
				(NULL != term->u.rtp.t38_options->T38FaxRateManagement)?
				term->u.rtp.t38_options->T38FaxRateManagement:"NULL");
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxMaxBuffer[%d]\n",
				term->u.rtp.t38_options->T38FaxMaxBuffer); 
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxMaxDatagram[%d]\n",
				term->u.rtp.t38_options->T38FaxMaxDatagram); 
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38FaxUdpEC[%s]\n",
				(NULL != term->u.rtp.t38_options->T38FaxUdpEC)?
				term->u.rtp.t38_options->T38FaxUdpEC:"NULL");
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,"T38VendorInfo[%s]\n",
				(NULL != term->u.rtp.t38_options->T38VendorInfo)?
				term->u.rtp.t38_options->T38VendorInfo:"NULL");
	}

}
