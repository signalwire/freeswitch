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

switch_status_t mg_stack_free_mem(MgMgcoMsg* msg)
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
   MG_GETMEM(termId->name.lcl.val, termId->name.lcl.len, memCp, ret);
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
         sprintf((char*)&errTxt->val[errTxt->len], "%d", ctxtId->val.val);
         errTxt->len += cmStrlen((U8*)(&errTxt->val[errTxt->len]));
      }

      errTxt->val[errTxt->len] = '\"';
      errTxt->len += 1;
   }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s:" 
                     "info, error-text is: %s\n", __PRETTY_FUNCTION__,errTxt->val);
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
