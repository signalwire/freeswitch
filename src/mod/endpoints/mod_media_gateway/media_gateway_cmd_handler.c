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
*       Fun: mg_prc_descriptors
*
*       Desc: this api will process the descriptors received from MG stack 
*
*
*/
switch_status_t mg_prc_descriptors(megaco_profile_t* mg_profile, MgMgcoCommand *cmd, mg_termination_t* term)
{
    CmSdpMedProtoFmts *format;
    TknU8 *fmt;
    CmSdpMedFmtRtpList      *fmt_list;
    MgMgcoTermStateDesc  *tstate;
    int 		  fmtCnt;
    int 		  i;
    int 		  descId = 0x00;
    int 		  j;
    MgMgcoLocalParm   *lclParm;
    CmSdpInfo 	  *sdp;
    MgMgcoLclCtlDesc  *locCtl;
    MgMgcoTermStateParm *tsp;
    MgMgcoAmmReq* desc = NULL;
    MgMgcoLocalDesc   *local; 
    MgMgcoRemoteDesc*  remote;

    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"cmd->cmdType.val[%d]\n",cmd->cmdType.val);

    switch (cmd->cmdType.val)
    {
        case CH_CMD_TYPE_IND:
            switch(cmd->u.mgCmdInd[0]->cmd.type.val)
            {
                case MGT_ADD:
                    {
                        desc = &cmd->u.mgCmdInd[0]->cmd.u.add;
                        break;
                    }
                case MGT_MOVE:
                    {
                        desc = &cmd->u.mgCmdInd[0]->cmd.u.move;
                        break;
                    }
                case MGT_MODIFY:
                    {
                        desc = &cmd->u.mgCmdInd[0]->cmd.u.mod;
                        break;
                    }
                default:
                    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Invalid cmd.type[%d] for descriptor processing \n",
                            cmd->u.mgCmdInd[0]->cmd.type.val);
                    return SWITCH_STATUS_FALSE;
            }
            break;
        default:
            {
                return SWITCH_STATUS_FALSE;
            }
    }

    if(NULL == desc){
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"No Valid descriptor found \n");
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"descriptors[%d] found in-coming megaco request \n", desc->dl.num.val);

    if(NOTPRSNT == desc->dl.num.pres){
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"No descriptor found in-coming megaco request \n");
        return SWITCH_STATUS_SUCCESS;
    }


    for (descId = 0; descId < desc->dl.num.val; descId++) {
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"descriptors[%d] type in-coming megaco request \n", desc->dl.descs[descId]->type.val); 
        switch (desc->dl.descs[descId]->type.val) {
            case MGT_MEDIADESC:
                {
                    int mediaId;
                    for (mediaId = 0; mediaId < desc->dl.descs[descId]->u.media.num.val; mediaId++) {
                        MgMgcoMediaPar *mediaPar = desc->dl.descs[descId]->u.media.parms[mediaId];
                        switch (mediaPar->type.val) {
                            case MGT_MEDIAPAR_LOCAL:
                                {
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_LOCAL");
                                    /* Matt - check local descriptor processing */
                                    local = &mediaPar->u.local;
                                    sdp = local->sdp.info[0];
                                    for (i = 0; i < sdp->mediaDescSet.numComp.val; i++) {
                                        /* sdp formats  */
                                        for (j = 0; j <
                                                sdp->mediaDescSet.mediaDesc[i]->field.par.numProtFmts.val; j++)
                                        {
                                            format = sdp->mediaDescSet.mediaDesc[i]->field.par.pflst[j];
                                            /* Matt - format has field for T38 also  */
                                            if ((format->protType.pres != NOTPRSNT) &&
                                                    (format->protType.val == CM_SDP_MEDIA_PROTO_RTP)) {

                                                /* protocol type RTP */
                                                fmt_list = &format->u.rtp;

                                                /* print format */
                                                for(fmtCnt = 0; fmtCnt <  fmt_list->num.val; fmtCnt++){
                                                    fmt = &fmt_list->fmts[i]->val;
                                                    if(fmt->pres == NOTPRSNT) continue;
                                                    printf("Format [%d]\n", fmt->val);
                                                }
                                            }
                                        }
                                    }

                                    break;
                                }

                            case MGT_MEDIAPAR_REMOTE:
                                {
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_REMOTE");
                                    /* Matt - check remote descriptor processing */
                                    remote = &mediaPar->u.remote;
                                    sdp = remote->sdp.info[0];
                                    /* for Matt - same like local descriptor */
                                    break;
                                }

                            case MGT_MEDIAPAR_LOCCTL:
                                {
                                    /* Matt - check Local Control descriptor processing */
                                    locCtl = &mediaPar->u.locCtl;
                                    for (i = 0; i < locCtl->num.val; i++){
                                        lclParm = locCtl->parms[i];
                                        if (PRSNT_NODEF == lclParm->type.pres){
                                            switch(lclParm->type.val)
                                            {
                                                case MGT_LCLCTL_MODE:
                                                    {
                                                        /* Mode Property */
                                                        printf("MGT_LCLCTL_MODE - Mode value [%d]\n", lclParm->u.mode.val);
                                                        break;
                                                    }
                                                case MGT_LCLCTL_RESVAL:
                                                    {
                                                        /* Reserve Value */
                                                        printf("MGT_LCLCTL_RESVAL: Reserve Value[%d] \n", lclParm->u.resVal.val);
                                                        break;
                                                    }
                                                case MGT_LCLCTL_RESGRP:
                                                    {
                                                        /* Reserve group */
                                                        printf("MGT_LCLCTL_RESGRP: Reserve Group[%d]\n", lclParm->u.resGrp.val);
                                                        break;
                                                    }
                                                case MGT_LCLCTL_PROPPARM:
                                                    {
                                                        /* Properties (of a termination) */
                                                        /* Matt - See how we can apply this to a termination */
                                                        printf("MGT_LCLCTL_PROPPARM: \n");
                                                        break;
                                                    }
                                                default:
                                                    printf("Invalid local control descriptor type[%d]\n",lclParm->type.val);
                                                    break;
                                            }
                                        }
                                    }

                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_LOCCTL");
                                    break;
                                }
                            case MGT_MEDIAPAR_TERMST:
                                {
                                    /* Matt - apply termination state descriptor */
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_TERMST");
                                    tstate = &mediaPar->u.tstate;	
                                    for (i = 0; i < tstate->numComp.val; i++)
                                    {
                                        /* Matt to see how to apply below descriptors to a termination */
                                        tsp = tstate->trmStPar[i];
                                        if (PRSNT_NODEF == tsp->type.pres) {
                                            switch(tsp->type.val)
                                            {
                                                case MGT_TERMST_PROPLST:
                                                    {
                                                        /* Matt to see how to apply properties to a termination */
                                                        /* Properties of a termination */
                                                        printf("MGT_TERMST_PROPLST:\n");
                                                        break;
                                                    }
                                                case MGT_TERMST_EVTBUFCTL:
                                                    {
                                                        /* Event /buffer Control Properties */
                                                        printf(" MGT_TERMST_EVTBUFCTL: value[%d]\n", tsp->u.evtBufCtl.val);
                                                        break;
                                                    }
                                                case MGT_TERMST_SVCST:
                                                    {
                                                        /* Service State Properties */
                                                        printf(" MGT_TERMST_SVCST: value[%d]\n", tsp->u.svcState.val);
                                                        break;
                                                    }
                                                default:
                                                    printf("Invalid termination state descriptor type[%d]\n",tsp->type.val);
                                                    break;
                                            }
                                        }
                                    }
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
                    break;
                }
            case MGT_REQEVTDESC:
                {
                    MgMgcoReqEvtDesc* evt = &desc->dl.descs[descId]->u.evts; 

                    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR," Requested Event descriptor\n");
            
                    /* If we receive events from MGC , means clear any ongoing events */
                    /* as such we dont apply any events to term, so for us (as of now) clear events means clear active_events structure*/

                    if(NULL != term->active_events){
                        mgUtlDelMgMgcoReqEvtDesc(term->active_events);
                        free(term->active_events);
                        term->active_events = NULL;
                    }

                    term->active_events = malloc(sizeof(*term->active_events));
                    
                    /* copy requested event */
                    if(RFAILED == mgUtlCpyMgMgcoReqEvtDesc(term->active_events, evt, NULLP)){
                        free(term->active_events);
                        term->active_events = NULL;
                        return SWITCH_STATUS_FALSE;
                    }

                    /* print Requested event descriptor */
                    mgAccEvntPrntMgMgcoReqEvtDesc(term->active_events, stdout);
                    
                    /* TODO - We can check for it/ito package*/

                    break;
                }
            case MGT_SIGNALSDESC:
                {
                    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR," Requested Signal descriptor\n");
                    break;
                }
            case MGT_MODEMDESC:
            case MGT_MUXDESC:
            case MGT_EVBUFDESC:
            case MGT_DIGMAPDESC:
            case MGT_AUDITDESC:
            case MGT_STATSDESC:
                break;

        }
    }

    return SWITCH_STATUS_SUCCESS;
}


/*****************************************************************************************************************************/

/*
*
*       Fun:  handle_mg_add_cmd
*
*       Desc: this api will handle the ADD request received from MG stack 
*
*
*/
switch_status_t handle_mg_add_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *inc_cmd, MgMgcoContextId* new_ctxtId)
{
	switch_status_t ret;
    MgMgcoContextId  *ctxtId;
    MgStr      	  errTxt;
    MgMgcoInd  	  *mgErr;
    MgMgcoTermId     *termId;
    MgMgcoTermIdLst*  termLst;
    int 		  err_code;
    int 		  is_rtp = 0x00;
    MgMgcoAmmReq 	  *cmd = &inc_cmd->u.mgCmdInd[0]->cmd.u.add;
    U32 		   txn_id = inc_cmd->transId.val;
    mg_termination_t* term = NULL;
    MgMgcoMediaDesc*   inc_med_desc;
    MgMgcoAudRetParm *desc;
    mg_context_t* mg_ctxt;

    /* TODO - Kapil dummy line , will need to add with proper code */
    inc_med_desc = &cmd->dl.descs[0]->u.media;

    /********************************************************************/
    ctxtId  = &inc_cmd->contextId;
    termLst = mg_get_term_id_list(inc_cmd);
    termId  = termLst->terms[0];
    /* For Matt - termId->name.lcl.val - to get the termination id name */

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

        mg_ctxt = megaco_choose_context(mg_profile);

        if(NULL == mg_ctxt){
            mg_util_set_err_string(&errTxt, " Resource Failure ");
            err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
            goto error;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO," Allocated Context[%p] with context_id[%d]\n", (void*)mg_ctxt, mg_ctxt->context_id);

        /* fill Trillium Context structure with allocated context */
        MG_SET_VAL_PRES(new_ctxtId->type, MGT_CXTID_OTHER);
        MG_SET_VAL_PRES(new_ctxtId->val, mg_ctxt->context_id);
    }
    else {
        /* context already present */
        memcpy(new_ctxtId, &inc_cmd->contextId,sizeof(MgMgcoContextId));
    }

    /********************************************************************/
    /* Allocate new RTP termination - If term type is CHOOSE */
    if ((NOTPRSNT != termId->type.pres)   &&
            (MGT_TERMID_CHOOSE == termId->type.val)){

        term = megaco_choose_termination(mg_profile, "RTP"); /* TODO - RTP string has be configured one */

        if(NULL == term){
            mg_util_set_err_string(&errTxt, " Resource Failure ");
            err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
            goto error;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO," Allocated Termination[%p] with term name[%s]\n", (void*)term, term->name);

        is_rtp = 0x01;

        /* TODO - Matt */
        /* allocate rtp term and associated the same to context */
        /********************************************************************/
    }else{  /* Physical termination */
        
        /* get physical termination */
    }
    /********************************************************************/
    /* associate physical termination to context  */

    if(SWITCH_STATUS_FALSE == megaco_context_add_termination(mg_ctxt, term)){
        mg_util_set_err_string(&errTxt, " Resource Failure ");
        err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
        goto error;
    }

    /********************************************************************/

    ret = mg_prc_descriptors(mg_profile, inc_cmd, term);

    /* TODO - locally assigned SDP must be the part of termination...which we can use to fill responses*/

    /********************************************************************/

    /* Matt - to provide the response SDP structure which needs to fill in ADD command response */

    /* Matt - to indicate if there is any failure while processing add message */

    /* Kapil - to return error if there is any failure based on Matt's indication */

    /* Kapil - to fill the response structure and call the response API to send ADD response */

    /*************************************************************************************************************************/
    /* sample resp code -- begin */
    {
        MgMgcoCommand  rsp;
        int ret = 0x00;
        MgMgcoTermId  *out_termId;

        memset(&rsp,0, sizeof(rsp));

        /*copy transaction-id*/
        memcpy(&rsp.transId, &inc_cmd->transId,sizeof(MgMgcoTransId));

        /*copy context-id*/
        memcpy(&rsp.contextId, new_ctxtId,sizeof(MgMgcoContextId));

        /*copy peer identifier */
        memcpy(&rsp.peerId, &inc_cmd->peerId,sizeof(TknU32));

        /*fill response structue */
        if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&rsp.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
            return ret;
        }

        rsp.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->type.val  = MGT_ADD;
        rsp.u.mgCmdRsp[0]->u.add.pres.pres = PRSNT_NODEF;

        if(!is_rtp){
            /* IF ADD request is for Physical term then we can simply copy incoming
             * termination */
            mgUtlAllocMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.add.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.add.termIdLst);

#ifdef GCP_VER_2_1
            out_termId = rsp.u.mgCmdRsp[0]->u.add.termIdLst.terms[0];
#else
            out_termId = &(rsp.u.mgCmdRsp[0]->u.add.termId);
#endif
        }else{
            /* ADD request is for RTP term we need to create termination */ 

            /* Grow the list of reply parameters */
            if (mgUtlGrowList((void ***)&rsp.u.mgCmdRsp[0]->u.add.termIdLst.terms, sizeof(MgMgcoTermId),
                        &rsp.u.mgCmdRsp[0]->u.add.termIdLst.num, &rsp.u.mgCmdRsp[0]->memCp) != ROK)
            {
                switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
                return SWITCH_STATUS_FALSE;
            }

            out_termId = rsp.u.mgCmdRsp[0]->u.add.termIdLst.terms[rsp.u.mgCmdRsp[0]->u.add.termIdLst.num.val-1];
            mg_fill_mgco_termid(out_termId, (char*)term->name, strlen((char*)term->name), &rsp.u.mgCmdRsp[0]->memCp);
        }

        /* Whatever Media descriptor we have received, we can copy that and then
         * whatever we want we can modify the fields */
        /* Kapil - TODO - will see if there is any problem of coping the
         * descriptor */

        if (mgUtlGrowList((void ***)&rsp.u.mgCmdRsp[0]->u.add.audit.parms, sizeof(MgMgcoAudRetParm),
                    &rsp.u.mgCmdRsp[0]->u.add.audit.num, &rsp.u.mgCmdRsp[0]->memCp) != ROK)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
            return SWITCH_STATUS_FALSE;
        }

        /* copy media descriptor */

        desc = rsp.u.mgCmdRsp[0]->u.add.audit.parms[rsp.u.mgCmdRsp[0]->u.add.audit.num.val-1];
        desc->type.pres = PRSNT_NODEF;
        desc->type.val = MGT_MEDIADESC;
        mgUtlCpyMgMgcoMediaDesc(&desc->u.media, inc_med_desc, &rsp.u.mgCmdRsp[0]->memCp);

#if 0
        {

            MgMgcoStreamDesc *stream;
            MgMgcoLocalDesc   *local; 
            MgMgcoRemoteDesc*  remote;
            CmSdpInfoSet *psdp;
            CmSdpInfoSet *prsdp;
            char* ipAddress = "192.168.1.1";

            /* Most probably we need to add local descriptor */

            /* TODO - considering only one descriptor*/
            stream = &desc->u.media.parms[0]->u.stream;
            stream->pres.pres = PRSNT_NODEF;
            stream->sl.pres.pres = PRSNT_NODEF;
            local  = &stream->sl.local;
            remote = &stream->sl.remote;

            if(!local->pres.pres) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " Local stream media not present adding it \n");

                /* allocating mem for local descriptor */
                if (mgUtlGrowList((void ***)&desc->u.media.parms, sizeof(MgMgcoMediaPar),
                            &desc->u.media.num, &rsp.u.mgCmdRsp[0]->memCp) != ROK)
                {
                    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
                    return SWITCH_STATUS_FALSE;
                }

                desc->u.media.parms[desc->u.media.num.val-1]->type.pres = PRSNT_NODEF;
                desc->u.media.parms[desc->u.media.num.val-1]->type.val = MGT_MEDIAPAR_STRPAR;


                stream = &desc->u.media.parms[desc->u.media.num.val-1]->u.stream;
                stream->pres.pres = PRSNT_NODEF;
                stream->pres.val = 0x01;
                stream->sl.pres.pres = PRSNT_NODEF;
                stream->sl.pres.val = 0x01;
                local  = &stream->sl.local;
                remote = &stream->sl.remote;

                memcpy(&stream->streamId, &desc->u.media.parms[0]->u.stream.streamId, sizeof(MgMgcoStreamId));

                local->pres.pres = PRSNT_NODEF;
                psdp = &(local->sdp);
                prsdp = &(remote->sdp);

                if (mgUtlGrowList((void ***)&psdp->info, sizeof(CmSdpInfo),
                            &psdp->numComp, &rsp.u.mgCmdRsp[0]->memCp) != ROK)
                {
                    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
                    return SWITCH_STATUS_FALSE;
                }

                psdp->info[0]->pres.pres = PRSNT_NODEF;

                /* fill version */
                /*memcpy(&psdp->info[0]->ver, &prsdp->info[0]->ver, sizeof(TknU16)); */
                MG_INIT_TOKEN_VALUE(&(psdp->info[0]->ver),1);

                /* fill orig */
                MG_SET_TKNSTROSXL(psdp->info[0]->orig.orig.usrName, 1, "-",
                        &rsp.u.mgCmdRsp[0]->memCp);
                MG_SET_TKNSTROSXL(psdp->info[0]->orig.orig.sessId, 1, "0",
                        &rsp.u.mgCmdRsp[0]->memCp);
                MG_SET_TKNSTROSXL(psdp->info[0]->orig.orig.sessVer, 1, "0",
                        &rsp.u.mgCmdRsp[0]->memCp);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.netType.type),
                        CM_SDP_NET_TYPE_IN);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.addrType), 
                        CM_SDP_ADDR_TYPE_IPV4);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.u.ip4.addrType), 
                        CM_SDP_IPV4_IP_UNI);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.u.ip4.addrType),
                        CM_SDP_IPV4_IP_UNI);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.u.ip4.u.ip.b[0]),
                        ipAddress[0]);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.u.ip4.u.ip.b[1]),
                        ipAddress[1]);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.u.ip4.u.ip.b[2]),
                        ipAddress[2]);
                MG_SET_VAL_PRES( (psdp->info[0]->orig.orig.sdpAddr.u.ip4.u.ip.b[3]),
                        ipAddress[3]);

                /* fill session name */
                /*memcpy(&psdp->info[0]->sessName, &prsdp->info[0]->sessName, sizeof(TknStrOSXL));*/
                MG_SET_TKNSTROSXL(psdp->info[0]->sessName, 1, "-",&rsp.u.mgCmdRsp[0]->memCp);


                /* fill info */
                MG_SET_VAL_PRES( (psdp->info[0]->conn.u.ip4.u.uniIp.b[0]), ipAddress[0]);
                MG_SET_VAL_PRES( (psdp->info[0]->conn.u.ip4.u.uniIp.b[1]), ipAddress[1]);
                MG_SET_VAL_PRES( (psdp->info[0]->conn.u.ip4.u.uniIp.b[2]), ipAddress[2]);
                MG_SET_VAL_PRES( (psdp->info[0]->conn.u.ip4.u.uniIp.b[3]), ipAddress[3]);

                /* Fill the SDP Connection Info */
                /* "c=" line - ipaddress */
                /*memcpy(&psdp->info[0]->conn, &prsdp->info[0]->conn, sizeof(CmSdpConn));*/

                /* fill media descriptors */
                {
                    CmSdpMediaDescSet* med = &psdp->info[0]->mediaDescSet;
                    CmSdpMediaDesc*    media;

                    if (mgUtlGrowList((void ***)&med->mediaDesc, sizeof(CmSdpMediaDesc),
                                &med->numComp, &rsp.u.mgCmdRsp[0]->memCp) != ROK)
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
                        return SWITCH_STATUS_FALSE;
                    }

                    media = med->mediaDesc[0];

                    MG_INIT_TOKEN_VALUE(&(media->pres),1);

                    /* Fill CmSdpMediaField */
                    MG_INIT_TOKEN_VALUE(&(media->field.pres),1);
                    MG_INIT_TOKEN_VALUE(&(media->field.mediaType),CM_SDP_MEDIA_AUDIO);

                    MG_INIT_TOKEN_VALUE(&(media->field.id.type),CM_SDP_VCID_PORT);
                    MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.type),CM_SDP_PORT_INT);
                    MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.pres),1);
                    MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.port.type),
                            CM_SDP_SPEC);
                    MG_INIT_TOKEN_VALUE(&(media->field.id.u.port.u.portInt.port.val), 2904);


                    MG_INIT_TOKEN_VALUE(&(media->field.par.numProtFmts),1);
                    MG_GETMEM((media->field.par.pflst),1*sizeof(CmSdpMedProtoFmts*),&rsp.u.mgCmdRsp[0]->memCp, ret);
                    MG_GETMEM((media->field.par.pflst[0]),sizeof(CmSdpMedProtoFmts),&rsp.u.mgCmdRsp[0]->memCp, ret);
                    MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[0]->prot.type),
                            CM_SDP_MEDIA_PROTO_RTP)
                        MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[0]->prot.u.subtype.type),
                                CM_SDP_PROTO_RTP_AVP);
                    MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[0]->protType),
                            CM_SDP_MEDIA_PROTO_RTP);


                    MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[0]->u.rtp.num),1);

                    MG_GETMEM((media->field.par.pflst[0]->u.rtp.fmts),
                            1*sizeof(CmSdpU8OrNil *), &rsp.u.mgCmdRsp[0]->memCp, ret);

                    MG_GETMEM((media->field.par.pflst[0]->u.rtp.fmts[0]),
                            sizeof(CmSdpU8OrNil), &rsp.u.mgCmdRsp[0]->memCp, ret);

                    MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[0]->u.rtp.fmts[0]->type),
                            CM_SDP_SPEC);

                    MG_INIT_TOKEN_VALUE(&(media->field.par.pflst[0]->u.rtp.fmts[0]->val),
                            4);

                    /* Fill attribute if reqd */
                    {
                        MG_INIT_TOKEN_VALUE(&(media->attrSet.numComp),1);
                        MG_GETMEM((media->attrSet.attr),sizeof(CmSdpAttr*),&rsp.u.mgCmdRsp[0]->memCp, ret);
                        MG_GETMEM((media->attrSet.attr[0]),sizeof(CmSdpAttr),&rsp.u.mgCmdRsp[0]->memCp, ret);
                        MG_INIT_TOKEN_VALUE(&(media->attrSet.attr[0]->type),CM_SDP_ATTR_PTIME);
                        media->attrSet.attr[0]->u.ptime.pres = PRSNT_NODEF;
                        media->attrSet.attr[0]->u.ptime.val = 30;
                    }

                }

                printf("ret[%d]\n",ret);

            }
            else {
                printf("!local->pres.pres false \n");
            }
        }
#endif



        /* We will always send one command at a time..*/
        rsp.cmdStatus.pres = PRSNT_NODEF;
        rsp.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

        rsp.cmdType.pres = PRSNT_NODEF;
        rsp.cmdType.val  = CH_CMD_TYPE_RSP;

        ret = sng_mgco_send_cmd( mg_profile->idx, &rsp);

        return ret;

    }
    /* sample resp code -- end */
    /*************************************************************************************************************************/


    return SWITCH_STATUS_SUCCESS;	
error:
    if (SWITCH_STATUS_SUCCESS == 
            mg_build_mgco_err_request(&mgErr, txn_id, ctxtId, err_code, &errTxt)) {
        sng_mgco_send_err(mg_profile->idx, mgErr);
    }
    mg_free_cmd(inc_cmd);
    return SWITCH_STATUS_FALSE;	
}

/*****************************************************************************************************************************/

/*
*
*       Fun:  handle_mg_modify_cmd
*
*       Desc: this api will handle the Modify request received from MG stack 
*
*
*/
switch_status_t handle_mg_modify_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *inc_cmd)
{
    mg_context_t* mg_ctxt = NULL;
	MgMgcoContextId  *ctxtId;
	MgStr      	  errTxt;
	MgMgcoInd  	  *mgErr;
	MgMgcoTermId     *termId;
	MgMgcoTermIdLst*  termLst;
    mg_termination_t* term = NULL;
    switch_status_t  ret;
	int 		  err_code;
	/*MgMgcoAmmReq 	  *cmd = &inc_cmd->u.mgCmdInd[0]->cmd.u.mod;*/
	U32 		   txn_id = inc_cmd->transId.val;

	/********************************************************************/
	ctxtId  = &inc_cmd->contextId;
	termLst = mg_get_term_id_list(inc_cmd);
	termId  = termLst->terms[0];
	/* For Matt - termId->name.lcl.val - to get the termination id name */

	/********************************************************************/
	/* Validation *******************************************/
	/********************************************************************/

	/*** CHOOSE Context NOT ALLOWED ***/
	if ((NOTPRSNT != ctxtId->type.pres)          &&
			(MGT_CXTID_CHOOSE == ctxtId->type.val))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,
				"Modify request processing failure,  CHOOSE Context should not present in Modify\n");

		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		mg_util_set_ctxt_string(&errTxt, ctxtId);
		goto error;
	}
	/***  CHOOSE Termination NOT ALLOWED **/
	else if ((NOTPRSNT != termId->type.pres)          &&
			(MGT_TERMID_CHOOSE == termId->type.val))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,
				"Modify request processing failure,  CHOOSE Termination should not present in Modify\n");

		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		mg_util_set_term_string(&errTxt,termId);
		goto error;
	}

	/********************************************************************/
	/* context id presence check is already being done, we are here it means context-id requested in megaco message is present */ 
	/********************************************************************/

    /* MGT_TERMID_ROOT = If ROOT term then there will not be any conext */

    if(MGT_TERMID_ROOT == termId->type.val){
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,
                "Modify request is for ROOT termination \n");
        /* TODO */

	/********************************************************************/
    } else if(MGT_TERMID_OTHER == termId->type.val){
        /********************************************************************/
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,
                "Modify request is for termination[%s] and context: type[%d], value[%d] \n", 
                termId->name.lcl.val, ctxtId->type.val, ctxtId->val.val);

        term = megaco_find_termination(mg_profile, (char*)termId->name.lcl.val);

        if(NULL == term){
            mg_util_set_term_string(&errTxt,termId);
            err_code = MGT_MGCO_RSP_CODE_UNKNOWN_TERM_ID;
            goto error;
        }

        /* termination specified...context should also be specified */

        /* check if we have terminations in the context */

        if (NOTPRSNT != ctxtId->type.pres) {
            if(MGT_CXTID_OTHER == ctxtId->type.val) {
                /*find context based on received context-id */
                mg_ctxt = megaco_get_context(mg_profile, ctxtId->val.val);
                if(NULL == mg_ctxt){
                    mg_util_set_ctxt_string(&errTxt, ctxtId);
                    err_code = MGT_MGCO_RSP_CODE_UNKNOWN_CTXT;
                    goto error;
                }

                if(SWITCH_STATUS_FALSE == megaco_context_is_term_present(mg_ctxt, term)){
                    /* ERROR - termination didnt bind with requested context */
                    mg_util_set_term_string(&errTxt,termId);
                    err_code = MGT_MGCO_RSP_CODE_NO_TERM_CTXT;
                    goto error;
                }

            }else if(MGT_CXTID_NULL == ctxtId->type.val) {
                switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,
                        "Modify request is for NULL Context \n");
                /*TODO - NULL context...nothing to do now...jump to response to send +ve response */
                goto response;
            }else if(MGT_CXTID_ALL == ctxtId->type.val) {
                switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO,
                        "Modify request is for ALL Context \n");
                /*TODO - ALL context...nothing to do now...jump to response to send +ve response */
                goto response;
            }
        }

        /* Not sure if MODIFY can come with Context ALL with specified term */

        /********************************************************************/

        ret = mg_prc_descriptors(mg_profile, inc_cmd, term);

    }

	/********************************************************************/

	/* Matt - to provide the response SDP structure which needs to fill in Modify command response */

	/* Matt - to indicate if there is any failure while processing add message */

	/* Kapil - to return error if there is any failure based on Matt's indication */

	/* Kapil - to fill the response structure and call the response API to send Modify response */

response:
    { /* send response */

        MgMgcoCommand  rsp;
        int ret = 0x00;

        memset(&rsp,0, sizeof(rsp));

        /*copy transaction-id*/
        memcpy(&rsp.transId, &inc_cmd->transId,sizeof(MgMgcoTransId));

        /*copy context-id*/ /*TODO - in case of $ context should be generated by app, we should not simply copy incoming structure */
        memcpy(&rsp.contextId, &inc_cmd->contextId,sizeof(MgMgcoContextId));

        /*copy peer identifier */
        memcpy(&rsp.peerId, &inc_cmd->peerId,sizeof(TknU32));

        /*fill response structue */
        if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&rsp.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
            return ret;
        }

        rsp.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->type.val = MGT_MODIFY;
        rsp.u.mgCmdRsp[0]->u.mod.pres.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->u.mod.termIdLst.num.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->u.mod.termIdLst.num.val  = 1;

        mgUtlAllocMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.mod.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.mod.termIdLst);

#ifdef GCP_VER_2_1
        termId = rsp.u.mgCmdRsp[0]->u.mod.termIdLst.terms[0];
#else
        termId = &(rsp.u.mgCmdRsp[0]->u.mod.termId);
#endif
        /*mg_fill_mgco_termid(termId, (char*)"term1",&req->u.mgCmdRsp[0]->memCp);*/

        /* We will always send one command at a time..*/
        rsp.cmdStatus.pres = PRSNT_NODEF;
        rsp.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

        rsp.cmdType.pres = PRSNT_NODEF;
        rsp.cmdType.val  = CH_CMD_TYPE_RSP;


        ret = sng_mgco_send_cmd(mg_profile->idx, &rsp);

    }


	return SWITCH_STATUS_SUCCESS;	
error:
	if (SWITCH_STATUS_SUCCESS == 
			mg_build_mgco_err_request(&mgErr, txn_id, ctxtId, err_code, &errTxt)) {
		sng_mgco_send_err(mg_profile->idx, mgErr);
	}
	mg_free_cmd(inc_cmd);
	return SWITCH_STATUS_FALSE;	
}

/*****************************************************************************************************************************/

/*
*
*       Fun:  handle_mg_subtract_cmd
*
*       Desc: this api will handle the Subtract request received from MG stack 
*
*
*/
switch_status_t handle_mg_subtract_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *inc_cmd)
{
    MgMgcoContextId  *ctxtId;
    MgStr      	  errTxt;
    MgMgcoInd  	  *mgErr;
    MgMgcoTermId     *termId;
    MgMgcoTermIdLst*  termLst;
    int 		  err_code;
    /*MgMgcoAmmReq 	  *cmd = &inc_cmd->u.mgCmdInd[0]->cmd.u.add;*/
    U32 		   txn_id = inc_cmd->transId.val;
    mg_context_t* mg_ctxt = NULL;
    mg_termination_t* term = NULL;
    uint8_t        wild = 0x00;

    wild = inc_cmd->u.mgCmdReq[0]->wild.pres;


    /************************************************************************************************************************************************************/
    ctxtId  = &inc_cmd->contextId;
    termLst = mg_get_term_id_list(inc_cmd);
    termId  = termLst->terms[0];

    /************************************************************************************************************************************************************/
    /* Validating Subtract request *******************************************/

    if(NOTPRSNT == ctxtId->type.pres){
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR," SUB Request processing failed, Context Not Present\n");
        mg_util_set_ctxt_string(&errTxt, ctxtId);
        err_code = MGT_MGCO_RSP_CODE_PROT_ERROR;
        goto error;
    }

    if(NOTPRSNT == termId->type.pres){
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR," SUB Request processing failed, Termination Not Present\n");
        mg_util_set_term_string(&errTxt,termId);
        err_code = MGT_MGCO_RSP_CODE_PROT_ERROR;
        goto error;
    }

    /*-- NULL Context & CHOOSE Context not applicable for SUB request --*/
    if ((MGT_CXTID_CHOOSE == ctxtId->type.val)     ||
             (MGT_CXTID_NULL == ctxtId->type.val)) {

        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR," SUB Request processing failed, Context CHOOSE/NULL not allowed\n");

        mg_util_set_ctxt_string(&errTxt, ctxtId);
        err_code = MGT_MGCO_RSP_CODE_PROT_ERROR;
        goto error;
    }
    /* ROOT Termination & CHOOSE Termination not allowed */
    else if ((MGT_TERMID_ROOT == termId->type.val)     ||
             (MGT_TERMID_CHOOSE == termId->type.val)) {

        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR," SUB Request processing failed, Termination ROOT/CHOOSE not allowed\n");

        mg_util_set_term_string(&errTxt,termId);
        err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
        goto error;
    }
    /************************************************************************************************************************************************************/

    if (MGT_CXTID_OTHER == ctxtId->type.val){

        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO," SUB Request for Context[%d] \n", ctxtId->val.val);

        /*find context based on received context-id */
        mg_ctxt = megaco_get_context(mg_profile, ctxtId->val.val);
        if(NULL == mg_ctxt){
            mg_util_set_ctxt_string(&errTxt, ctxtId);
            err_code = MGT_MGCO_RSP_CODE_UNKNOWN_CTXT;
            goto error;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO," Found Context[%p] for context_id[%d]\n", (void*)mg_ctxt, mg_ctxt->context_id);        
        if(MGT_TERMID_ALL == termId->type.val){

            switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO," SUB Request for context[%d] with ALL termination   \n", mg_ctxt->context_id);

            /* remove terminations from context */
            megaco_context_sub_all_termination(mg_ctxt);

        }else if(MGT_TERMID_OTHER == termId->type.val){

            switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO," SUB Request for termination[%s]  \n", (char*)termId->name.lcl.val);

            term = megaco_find_termination(mg_profile, (char*)termId->name.lcl.val);

            if(SWITCH_STATUS_FALSE == megaco_context_is_term_present(mg_ctxt, term)){
                /* ERROR - termination didnt bind with requested context */
                mg_util_set_term_string(&errTxt,termId);
                err_code = MGT_MGCO_RSP_CODE_NO_TERM_CTXT;
                goto error;
            }

            if(NULL == term){
                mg_util_set_term_string(&errTxt,termId);
                err_code = MGT_MGCO_RSP_CODE_UNKNOWN_TERM_ID;
                goto error;
            }

            /* remove termination from context */
            megaco_context_sub_termination(mg_ctxt, term);
        }

        /* release context*/
        megaco_release_context(mg_ctxt);
    }

    /************************************************************************************************************************************************************/
    /* CONTEXT = ALL */

    if (MGT_CXTID_ALL == ctxtId->type.val){

        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO," SUB Request for ALL context \n");

        /* TODO */

        /* As of now sending +ve response */
        goto response;
    }

    
    /************************************************************************************************************************************************************/


response:
    /************************************************************************************************************************************************************/
    /* resp code -- begin */
    {
        MgMgcoCommand  rsp;
        int ret = 0x00;
        MgMgcoTermId  *out_termId;

        memset(&rsp,0, sizeof(rsp));

        /*copy transaction-id*/
        memcpy(&rsp.transId, &inc_cmd->transId, sizeof(MgMgcoTransId));

        /*copy context-id*/
        memcpy(&rsp.contextId, &inc_cmd->contextId, sizeof(MgMgcoContextId));

        /*copy peer identifier */
        memcpy(&rsp.peerId, &inc_cmd->peerId,sizeof(TknU32));

        /*fill response structue */
        if(SWITCH_STATUS_FALSE == (ret = mg_stack_alloc_mem((Ptr*)&rsp.u.mgCmdRsp[0],sizeof(MgMgcoCmdReply)))){
            return ret;
        }

        rsp.u.mgCmdRsp[0]->pres.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->type.pres = PRSNT_NODEF;
        rsp.u.mgCmdRsp[0]->type.val  = MGT_SUB;
        rsp.u.mgCmdRsp[0]->u.sub.pres.pres = PRSNT_NODEF;

        if(wild){
            rsp.u.mgCmdRsp[0]->wild.pres = PRSNT_NODEF;
        }

        mgUtlAllocMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.add.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.add.termIdLst);

#ifdef GCP_VER_2_1
        out_termId = rsp.u.mgCmdRsp[0]->u.add.termIdLst.terms[0];
#else
        out_termId = &(rsp.u.mgCmdRsp[0]->u.add.termId);
#endif

        /* We will always send one command at a time..*/
        rsp.cmdStatus.pres = PRSNT_NODEF;
        rsp.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

        rsp.cmdType.pres = PRSNT_NODEF;
        rsp.cmdType.val  = CH_CMD_TYPE_RSP;

        ret = sng_mgco_send_cmd( mg_profile->idx, &rsp);

        return ret;

    }
    /* sample resp code -- end */
    /************************************************************************************************************************************************************/


    return SWITCH_STATUS_SUCCESS;	
error:
    if (SWITCH_STATUS_SUCCESS == 
            mg_build_mgco_err_request(&mgErr, txn_id, ctxtId, err_code, &errTxt)) {
        sng_mgco_send_err(mg_profile->idx, mgErr);
    }
    mg_free_cmd(inc_cmd);
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
/* Note : API to send Service Change when termination is coming up(in-service) */
/* INPUT : MG Profile structure and termination name */
/* wild flag will tell if service change request needs to be in W-SC format as we can have W-SC=A01* or SC=A01* */
switch_status_t mg_send_ins_service_change(megaco_profile_t* mg_profile, const char* term_name, int wild)
{
	switch_assert(term_name);
	switch_assert(mg_profile);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
			  "Sending In-Service Service Change for termination[%s] configured in mg profile[%s], suId[%d]\n", 
			   term_name, mg_profile->name, mg_profile->idx);

	return mg_send_service_change(mg_profile->idx, term_name, MGT_SVCCHGMETH_RESTART, MG_SVC_REASON_900_RESTORED, wild);
}

/*****************************************************************************************************************************/
/* Note : API to send Service Change when termination is going down (out-of-service) */
/* INPUT : MG Profile structure and termination name */
/* wild flag will tell if service change request needs to be in W-SC format as we can have W-SC=A01* or SC=A01* */
switch_status_t mg_send_oos_service_change(megaco_profile_t* mg_profile, const char* term_name, int wild)
{
	switch_assert(term_name);
	switch_assert(mg_profile);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
			  "Sending Out-Of-Service Service Change for termination[%s] configured in mg profile[%s], suId[%d]\n", 
			   term_name, mg_profile->name, mg_profile->idx);

	return mg_send_service_change(mg_profile->idx, term_name, MGT_SVCCHGMETH_FORCED, MG_SVC_REASON_905_TERM_OOS, wild);
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
/* API to send In-Activity Timeout NOTIFY to MGC */
switch_status_t  mg_send_ito_notify(megaco_profile_t* mg_profile )
{
    MgMgcoObsEvt *oevt;

	switch_assert(mg_profile);

    mg_stack_alloc_mem((Ptr*)&oevt, sizeof(MgMgcoObsEvt));

    oevt->pres.pres = PRSNT_NODEF;

    mg_get_time_stamp(&oevt->time);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.pkgType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.valType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.u.val), MGT_PKG_INACTTIMER);

    MG_INIT_TOKEN_VALUE(&(oevt->name.type),MGT_GEN_TYPE_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->name.u.val),(U8)MGT_PKG_ENUM_REQEVT_INACTTIMER_INACT_TIMOUT);

    oevt->pl.num.pres = PRSNT_NODEF;
    oevt->pl.num.val = 0x00;

    return mg_send_notify(mg_profile, "ROOT", oevt);
}
/*****************************************************************************************************************************/
/* API to send DTMF Digits Notification */

switch_status_t  mg_send_dtmf_notify(megaco_profile_t* mg_profile, const char* term_name, char* digits, int num_of_collected_digits )
{
    MgMgcoObsEvt *oevt;
    MgMgcoEvtPar* param;
    int           ascii = 0x00;
    int           cnt = 0x00;

	switch_assert(term_name);
	switch_assert(mg_profile);
	switch_assert(digits);

    if(0 == num_of_collected_digits ){
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"num_of_collected_digits cannt be ZERO \n");
        return SWITCH_STATUS_FALSE;
    }

    mg_stack_alloc_mem((Ptr*)&oevt, sizeof(MgMgcoObsEvt));

    oevt->pres.pres = PRSNT_NODEF;

    mg_get_time_stamp(&oevt->time);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.pkgType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.valType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.u.val), MGT_PKG_EXT_DTMF);

    MG_INIT_TOKEN_VALUE(&(oevt->name.type),MGT_GEN_TYPE_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->name.u.val),(U8)MGT_PKG_ENUM_REQEVT_EXT_DTMF_EXT_CE);

    if (mgUtlGrowList((void ***)&oevt->pl.parms, sizeof(MgMgcoEvtPar), &oevt->pl.num, NULL) != ROK) {
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

    param = oevt->pl.parms[0];

    MG_INIT_TOKEN_VALUE(&(param->type),(U8)MGT_EVTPAR_OTHER);

    MG_INIT_TOKEN_VALUE(&(param->u.other.name.type),MGT_GEN_TYPE_KNOWN);
    MG_INIT_TOKEN_VALUE(&(param->u.other.name.u.val),MGT_PKG_ENUM_OBSEVTOTHER_EXT_DTMF_EXT_CE_DGT_STR);

    MG_INIT_TOKEN_VALUE(&(param->u.other.val.type),MGT_VALUE_EQUAL);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.type),MGT_VALTYPE_OCTSTRXL);

    mg_stack_alloc_mem((Ptr*)&param->u.other.val.u.eq.u.osxl.val, num_of_collected_digits+2);

    param->u.other.val.u.eq.u.osxl.pres = 0x01; 
    param->u.other.val.u.eq.u.osxl.len = num_of_collected_digits+2;

    param->u.other.val.u.eq.u.osxl.val[0] = '\"';

    /* copy collected DTMF digits */
    if(ascii)
    {
        for(cnt = 1; cnt< num_of_collected_digits; cnt++){
            /* convert values to ascii */
            if(digits[cnt-1] <= 9){
                param->u.other.val.u.eq.u.osxl.val[cnt] = digits[cnt-1] + '0';
            }else{
                /* 10 for decimal equivalent of A */
                param->u.other.val.u.eq.u.osxl.val[cnt] = digits[cnt-1] + 'A' - 10;
            }
        }
    } else {
        /* If incoming digits are already in ascii format .. simply copy */
        for(cnt = 1; cnt< num_of_collected_digits; cnt++){
                param->u.other.val.u.eq.u.osxl.val[cnt] = digits[cnt-1];
        }
    }


    param->u.other.val.u.eq.u.osxl.val[num_of_collected_digits+1] = '\"';

    if (mgUtlGrowList((void ***)&oevt->pl.parms, sizeof(MgMgcoEvtPar), &oevt->pl.num, NULL) != ROK) {
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

    param = oevt->pl.parms[1];


    /* Set method */
    MG_INIT_TOKEN_VALUE(&(param->type),MGT_EVTPAR_OTHER);
    MG_INIT_TOKEN_VALUE(&(param->u.other.name.type),MGT_GEN_TYPE_KNOWN);
    MG_INIT_TOKEN_VALUE(&(param->u.other.name.u.val),MGT_PKG_ENUM_OBSEVTOTHER_EXT_DTMF_EXT_CE_TERM_METH);

    MG_INIT_TOKEN_VALUE(&(param->u.other.val.type),MGT_VALUE_EQUAL);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.type),MGT_VALTYPE_ENUM);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHER_EXT_DTMF_EXT_CE_TERM_METHFM);

    return mg_send_notify(mg_profile, term_name, oevt);
}

/*****************************************************************************************************************************/

/* Note : API to send NOTIFY Message */
/* INPUT : 
*           mg_profile          - MG profile structure pointer
*	        term_name 			- Termination Name(as specified for MEGACO )
*	        oevt                - Observed Event structure pointer
*/
switch_status_t  mg_send_notify(megaco_profile_t* mg_profile, const char* term_name, MgMgcoObsEvt* oevt)
{
    switch_status_t ret;
    MgMgcoCommand   request;
    MgMgcoTermId*   termId;
    mg_termination_t* term = NULL;
    MgMgcoObsEvtDesc  *obs_desc = NULL;
    MgMgcoRequestId    reqId;               

    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "Sending Notify Message for termination[%s] !\n", term_name);

    MG_ZERO(&request, sizeof(request));
    MG_ZERO(&reqId, sizeof(reqId));

    if(strcmp(term_name, "ROOT")){
        /* Not ROOT term then --- */
        term = megaco_find_termination(mg_profile, (char*)term_name);

        if(!term){
            switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "No termination configured for given name[%s] !\n", term_name);
            return SWITCH_STATUS_FALSE;
        }

        if(NULL == term->active_events){
            switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "No Active events observed on given termination[%s] !\n", term_name);
            /* return SWITCH_STATUS_FALSE; */
            /*TODO - ideally we should return ...
             * as of now not returning .. if we dont have active signals then
             * setting default request id and sending notification to MGC */
            MG_SET_DEF_REQID(&reqId);
        }else{
            MG_MEM_COPY(&reqId, &term->active_events->reqId, sizeof(MgMgcoRequestId));
        }
    }else{
            MG_SET_DEF_REQID(&reqId);
    }
    


    if(SWITCH_STATUS_FALSE == (ret = mg_create_mgco_command(&request, CH_CMD_TYPE_REQ, MGT_NTFY))){
        return SWITCH_STATUS_FALSE;
    }

    if (mgUtlGrowList((void ***)&request.u.mgCmdReq[0]->cmd.u.ntfy.obs.el.evts, sizeof(MgMgcoObsEvtLst),
                &request.u.mgCmdReq[0]->cmd.u.ntfy.obs.el.num, &request.u.mgCmdReq[0]->memCp) != ROK) {
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

    obs_desc = &request.u.mgCmdReq[0]->cmd.u.ntfy.obs;

    MG_INIT_TOKEN_VALUE(&(obs_desc->pres), PRSNT_NODEF);

    /* copy request id */
    MG_MEM_COPY(&obs_desc->reqId, &reqId, sizeof(MgMgcoRequestId));

    /* copy observe event */
    /*MG_MEM_COPY(obs_desc->el.evts[0], oevt, sizeof(MgMgcoObsEvt));*/

    obs_desc->el.evts[0] = oevt;


    /*fill txn id */
    request.transId.pres = PRSNT_NODEF;
    request.transId.val  = get_txn_id();

    request.contextId.type.pres = PRSNT_NODEF;
    request.contextId.type.val  = MGT_CXTID_NULL;

    request.cmdStatus.pres = PRSNT_NODEF;
    request.cmdStatus.val = CH_CMD_STATUS_END_OF_TXN;

    request.cmdType.pres = PRSNT_NODEF;
    request.cmdType.val  = CH_CMD_TYPE_REQ;

    /* fill termination */
    if (mgUtlGrowList((void ***)&request.u.mgCmdReq[0]->cmd.u.ntfy.termIdLst.terms, sizeof(MgMgcoTermIdLst),
                &request.u.mgCmdReq[0]->cmd.u.ntfy.termIdLst.num, &request.u.mgCmdReq[0]->memCp) != ROK)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

#ifdef GCP_VER_2_1
    termId = request.u.mgCmdReq[0]->cmd.u.ntfy.termIdLst.terms[0];
#else
    termId = &(request.u.mgCmdReq[0]->cmd.u.ntfy.termId);
#endif

    mg_fill_mgco_termid(termId, (char*)term_name ,strlen(term_name), &request.u.mgCmdReq[0]->memCp);

    sng_mgco_send_cmd(mg_profile->idx, &request);

    return SWITCH_STATUS_SUCCESS;
}
/*****************************************************************************************************************************/

