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
void mg_restart_inactivity_timer(megaco_profile_t* profile)
{
    /* NOTE - For Restart - we are deleting existing task and adding it again  */
    if(profile->inact_tmr_task_id)
        switch_scheduler_del_task_id(profile->inact_tmr_task_id);

    if(profile->inact_tmr) {
        mg_activate_ito_timer(profile); 
    }
}

/*****************************************************************************************************************************/
static void mg_inactivity_timer_exp(switch_scheduler_task_t *task)
{
    megaco_profile_t* profile = (megaco_profile_t*) task->cmd_arg;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," mg_inactivity_timer_exp for profile[%s]\n", profile->name);
    mg_print_time();

    mg_send_ito_notify(profile);

    /* resetting task_id */
    profile->inact_tmr_task_id = 0x00;
}

/*****************************************************************************************************************************/
switch_status_t mg_activate_ito_timer(megaco_profile_t* profile)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Starting IT/ITO Timer \n");
    mg_print_time();

    profile->inact_tmr_task_id = switch_scheduler_add_task(switch_epoch_time_now(NULL)+profile->inact_tmr, mg_inactivity_timer_exp,"","media_gateway",0,profile,0);

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************************/
switch_status_t mg_is_ito_pkg_req(megaco_profile_t* mg_profile, MgMgcoCommand *cmd)
{
    int 		  descId = 0x00;
    MgMgcoAmmReq* desc = NULL;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"cmd->cmdType.val[%d]\n",cmd->cmdType.val);

    if(CH_CMD_TYPE_IND != cmd->cmdType.val)
        return SWITCH_STATUS_FALSE;

    if(MGT_MODIFY != cmd->u.mgCmdInd[0]->cmd.type.val)
        return SWITCH_STATUS_FALSE;

    desc = &cmd->u.mgCmdInd[0]->cmd.u.mod;

    if(NULL == desc){
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"No Valid descriptor found \n");
        return SWITCH_STATUS_FALSE;
    }

    if(NOTPRSNT == desc->dl.num.pres){
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"No descriptor found in-coming megaco request \n");
        return SWITCH_STATUS_SUCCESS;
    }


    for (descId = 0; descId < desc->dl.num.val; descId++) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"descriptors[%d] type in-coming megaco request \n", desc->dl.descs[descId]->type.val); 
        switch (desc->dl.descs[descId]->type.val) {
            case MGT_MEDIADESC:
                {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," Media descriptor on ROOT termination..Not Supporting now\n");
                    break;
                }

            case MGT_REQEVTDESC:
                {
                    MgMgcoReqEvtDesc* evts = &desc->dl.descs[descId]->u.evts; 
                    MgMgcoEvtPar     *reqEvtPar;
                    MgMgcoReqEvt     *evt;
                    int              numEvts = 0;
                    int              i;

                    /* As of now only handling ito package */

                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," Requested Event descriptor\n");

                    if (evts->el.num.pres)
                        numEvts = evts->el.num.val;

                    for (i = 0; i < numEvts; i++)
                    {
                        evt = evts->el.revts[i];
                        if (evt->pl.num.pres)
                        {
                            /* Check for the package */
                            if((MGT_PKG_KNOWN == evt->pkg.valType.val) &&
                                    (MGT_PKG_INACTTIMER != evt->pkg.u.val.val))
                            {
                                continue;
                            }
                            else
                            {
                                if((MGT_GEN_TYPE_KNOWN == evt->name.type.val) &&
                                   (MGT_PKG_ENUM_REQEVT_INACTTIMER_INACT_TIMOUT == 
                                        evt->name.u.val.val)){

                                    if((evt->pl.num.pres != NOTPRSNT) &&
                                            (evt->pl.num.val != 0)) {

                                        reqEvtPar = evt->pl.parms[0];

                                        if((NULL != reqEvtPar) &&
                                                (reqEvtPar->type.val == MGT_EVTPAR_OTHER)      &&
                                                (reqEvtPar->u.other.name.type.pres == PRSNT_NODEF) &&
                                                (reqEvtPar->u.other.name.type.val == MGT_GEN_TYPE_KNOWN) &&
                                                (reqEvtPar->u.other.name.u.val.pres == PRSNT_NODEF)  &&
                                                (reqEvtPar->u.other.name.u.val.val ==
                                                 MGT_PKG_ENUM_REQEVTOTHER_INACTTIMER_INACT_TIMOUT_MAX_IATIME)&&
                                                (reqEvtPar->u.other.val.type.pres == PRSNT_NODEF) &&
                                                (reqEvtPar->u.other.val.type.val == MGT_VALUE_EQUAL) &&
                                                (reqEvtPar->u.other.val.u.eq.type.pres == PRSNT_NODEF) &&
                                                (reqEvtPar->u.other.val.u.eq.type.val == MGT_VALTYPE_UINT32))
                                        {
#ifdef BIT_64
                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Received Inactivity timer value [%d]\n", 
                                                    reqEvtPar->u.other.val.u.eq.u.decInt.val); 
#else
                                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Received Inactivity timer value [%ld]\n", 
                                                    reqEvtPar->u.other.val.u.eq.u.decInt.val); 
#endif

                                            mg_profile->inact_tmr = reqEvtPar->u.other.val.u.eq.u.decInt.val/MG_INACTIVITY_TMR_RESOLUTION;

                                            if(0 == mg_profile->inact_tmr){
                                                /* value ZERO means MGC wantes  to disable ito timer */
                                                switch_scheduler_del_task_id(mg_profile->inact_tmr_task_id) ;
                                            } else {
                                                mg_activate_ito_timer(mg_profile);
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    break;
                }
            case MGT_SIGNALSDESC:
                {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," Signal descriptor on ROOT termination..Not Supporting now\n");
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
switch_status_t mg_prc_sig_desc(MgMgcoSignalsReq* req, megaco_profile_t* mg_profile, mg_termination_t* term)
{
    MgMgcoSigPar *p   = NULL;
    MgMgcoSigOther* o = NULL;
    int sig = 0x00;

	switch_assert(req);
	switch_assert(mg_profile);
	switch_assert(term);


    /* As of now only checking T.38 CED tone */

    /* Modify message will have following signal descriptor *
    *  Signals{ctyp/ANS{anstype=ans}} *
    */

    /* check for T.38 CED tone package i.e. MGT_PKG_CALL_TYP_DISCR */
    if((MGT_PKG_KNOWN == req->pkg.valType.val) &&
            (NOTPRSNT != req->pkg.u.val.pres) &&
            (MGT_PKG_CALL_TYP_DISCR == req->pkg.u.val.val) && 
            (NOTPRSNT != req->name.type.pres) && 
            (MGT_GEN_TYPE_KNOWN == req->name.type.val))
       {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Received Signal Descriptor : T.38 ctyp package\n");

        if((NOTPRSNT != req->pl.num.pres) && (NOTPRSNT != req->pl.num.val)){
            for(sig = 0x00; sig < req->pl.num.val; sig++){
                p = req->pl.parms[sig];

                if(NOTPRSNT == p->type.pres) continue;

                if((MGT_SIGPAR_OTHER == p->type.val) && 
                        (NULL != (o = &p->u.other)) &&
                        (NOTPRSNT != o->name.type.pres) && 
                        (MGT_GEN_TYPE_KNOWN == o->name.type.val) && 
                        (MGT_PKG_ENUM_SIGOTHERCALLTYPDISCRANSSIGANSTYP == o->name.u.val.val) && 
                        (MGT_VALUE_EQUAL == o->val.type.val) && 
                        (NOTPRSNT != o->val.u.eq.type.pres) && 
                        (MGT_VALTYPE_ENUM == o->val.u.eq.type.val) && 
                        (MGT_PKG_ENUM_SIGOTHERCALLTYPDISCRANSSIGANSTYPANS == o->val.u.eq.u.enume.val)){
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Signal Descriptor :  T.38 CED/ANS TONE \n");
                            /* apply CED/ANS tone to specify channel */
                }
            }
        }
    }


    return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************************/

/*
*
*       Fun: mg_prc_descriptors
*
*       Desc: this api will process the descriptors received from MG stack 
*
*
*/
switch_status_t mg_prc_descriptors(megaco_profile_t* mg_profile, MgMgcoCommand *cmd, mg_termination_t* term, CmMemListCp     *memCp)
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

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"cmd->cmdType.val[%d]\n",cmd->cmdType.val);

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
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Invalid cmd.type[%d] for descriptor processing \n",
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"No Valid descriptor found \n");
        return SWITCH_STATUS_FALSE;
    }

    //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"descriptors[%d] found in-coming megaco request \n", desc->dl.num.val);

    if(NOTPRSNT == desc->dl.num.pres){
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"No descriptor found in-coming megaco request \n");
        return SWITCH_STATUS_SUCCESS;
    }


    for (descId = 0; descId < desc->dl.num.val; descId++) {
        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"descriptors[%d] type in-coming megaco request \n", desc->dl.descs[descId]->type.val); 
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
                                                   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Format [%d]\n", fmt->val);
                                                }
                                            }
                                        }
                                    }

				    mgco_handle_incoming_sdp(&local->sdp, term, MG_SDP_LOCAL, mg_profile, memCp);

                                    break;
                                }

                            case MGT_MEDIAPAR_REMOTE:
                                {
                                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "MGT_MEDIAPAR_REMOTE");
                                    /* Matt - check remote descriptor processing */
                                    remote = &mediaPar->u.remote;
                                    sdp = remote->sdp.info[0];
                                    /* for Matt - same like local descriptor */
                                    mgco_handle_incoming_sdp(&remote->sdp, term, MG_SDP_REMOTE, mg_profile, memCp);
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
                                                       //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"MGT_LCLCTL_MODE - Mode value [%d]\n", lclParm->u.mode.val);
                                                        break;
                                                    }
                                                case MGT_LCLCTL_RESVAL:
                                                    {
                                                        /* Reserve Value */
                                                       //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"MGT_LCLCTL_RESVAL: Reserve Value[%d] \n", lclParm->u.resVal.val);
                                                        break;
                                                    }
                                                case MGT_LCLCTL_RESGRP:
                                                    {
                                                        /* Reserve group */
                                                       //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"MGT_LCLCTL_RESGRP: Reserve Group[%d]\n", lclParm->u.resGrp.val);
                                                        break;
                                                    }
                                                case MGT_LCLCTL_PROPPARM:
                                                    {
                                                        /* Properties (of a termination) */
                                                        /* Matt - See how we can apply this to a termination */
                                                       //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"MGT_LCLCTL_PROPPARM: \n");
                                                        break;
                                                    }
                                                default:
                                                   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Invalid local control descriptor type[%d]\n",lclParm->type.val);
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
                                                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"MGT_TERMST_PROPLST:\n");
                                                        break;
                                                    }
                                                case MGT_TERMST_EVTBUFCTL:
                                                    {
                                                        /* Event /buffer Control Properties */
                                                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE," MGT_TERMST_EVTBUFCTL: value[%d]\n", tsp->u.evtBufCtl.val);
                                                        break;
                                                    }
                                                case MGT_TERMST_SVCST:
                                                    {
                                                        /* Service State Properties */
                                                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE," MGT_TERMST_SVCST: value[%d]\n", tsp->u.svcState.val);
                                                        break;
                                                    }
                                                default:
                                                   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Invalid termination state descriptor type[%d]\n",tsp->type.val);
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
                                        mgco_handle_incoming_sdp(&mgStream->sl.remote.sdp, term, MG_SDP_LOCAL, mg_profile, memCp);
                                    }

                                    if (mgStream->sl.local.pres.pres) {
                                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got local stream media description:\n");
                                        mgco_handle_incoming_sdp(&mgStream->sl.local.sdp, term, MG_SDP_REMOTE, mg_profile, memCp);
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

                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG," Requested Event descriptor\n");
            
                    /* If we receive events from MGC , means clear any ongoing events */
                    /* as such we dont apply any events to term, so for us (as of now) clear events means clear active_events structure*/

                    if(NULL != term->active_events){
			mgUtlDelMgMgcoReqEvtDesc(term->active_events);
			MG_STACK_MEM_FREE(term->active_events, sizeof(MgMgcoReqEvtDesc));
                    }

		    MG_STACK_MEM_ALLOC(&term->active_events, sizeof(MgMgcoReqEvtDesc));

		    if(NULL == term->active_events){
			    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," term->active_events Memory Alloc failed \n");
			    return SWITCH_STATUS_FALSE;
		    }

                    /* copy requested event */
                    if(RFAILED == mgUtlCpyMgMgcoReqEvtDesc(term->active_events, evt, NULLP)){
		        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," copy new events to term->active_events failed \n");
			MG_STACK_MEM_FREE(term->active_events, sizeof(MgMgcoReqEvtDesc));
                        return SWITCH_STATUS_FALSE;
                    }

                    /* print Requested event descriptor */
                    /*mgAccEvntPrntMgMgcoReqEvtDesc(term->active_events, stdout);*/
                    
                    break;
                }
            case MGT_SIGNALSDESC:
                {
                    MgMgcoSignalsDesc* sig = &desc->dl.descs[descId]->u.sig; 
                    MgMgcoSignalsParm* param = NULL;
                    int i = 0x00;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG," Requested Signal descriptor\n");

                    if((NOTPRSNT != sig->pres.pres) && (NOTPRSNT != sig->num.pres) && (0 != sig->num.val)){

                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Total number of Signal descriptors[%d]\n", sig->num.val);

                        for(i=0; i< sig->num.val; i++){
                                param = sig->parms[i];

                                if(NOTPRSNT == param->type.pres) continue;

                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Signal Descriptor[%d] type[%s]\n",
                                        i, ((MGT_SIGSPAR_LST == param->type.val)?"MGT_SIGSPAR_LST":"MGT_SIGSPAR_REQ"));
                                
                                switch(param->type.val)
                                {
                                    case MGT_SIGSPAR_LST:
                                        {
                                            MgMgcoSignalsLst* lst = &param->u.lst;
                                            int sigId = 0x00;

                                            if((NOTPRSNT == lst->pl.num.pres) || (0 == lst->pl.num.val)) break;

                                            for(sigId = 0; sigId < lst->pl.num.val; sigId++){
                                                mg_prc_sig_desc(lst->pl.reqs[sigId], mg_profile, term);
                                            }

                                            break;
                                        }
                                    case MGT_SIGSPAR_REQ:
                                        {
                                            MgMgcoSignalsReq* req = &param->u.req;
                                            mg_prc_sig_desc(req, mg_profile, term);
                                            break;
                                        }
                                    default:
                                        break;
                                }
                        }
                    }

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
    /*MgMgcoStreamDesc*   inc_strm_desc;*/
    MgMgcoAudRetParm *desc;
    mg_context_t* mg_ctxt;
    int mediaId;
    MgMgcoLocalDesc   *local = NULL;
    /*CmSdpInfoSet      *psdp  = NULL;*/

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

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," ADD Request processing failed, Context ALL/NULL not allowed\n");

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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," megaco_choose_context failed \n"); 	
            mg_util_set_err_string(&errTxt, " Resource Failure ");
            err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
            goto error;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Allocated Context[%p] with context_id[%d]\n", (void*)mg_ctxt, mg_ctxt->context_id);

        /* fill Trillium Context structure with allocated context */
        MG_SET_VAL_PRES(new_ctxtId->type, MGT_CXTID_OTHER);
        MG_SET_VAL_PRES(new_ctxtId->val, mg_ctxt->context_id);
    }
    else {
	    /* context already present */
	    memcpy(new_ctxtId, &inc_cmd->contextId,sizeof(MgMgcoContextId));
	    mg_ctxt =  megaco_get_context(mg_profile, inc_cmd->contextId.val.val);
	    if(NULL == mg_ctxt){
#ifdef BIT_64
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				" megaco_get_context failed for context-id[%d]\n",  inc_cmd->contextId.val.val); 	
#else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				" megaco_get_context failed for context-id[%ld]\n",  inc_cmd->contextId.val.val); 	
#endif
		    mg_util_set_err_string(&errTxt, " Resource Failure ");
		    err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
		    goto error;
	    }
    }

    /********************************************************************/
    /* Allocate new RTP termination - If term type is CHOOSE */
    if ((NOTPRSNT != termId->type.pres)   &&
            (MGT_TERMID_CHOOSE == termId->type.val)){

        term = megaco_choose_termination(mg_profile, mg_profile->rtp_termination_id_prefix);

        if(NULL == term){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," megaco_choose_termination failed \n"); 	
            mg_util_set_err_string(&errTxt, " Resource Failure ");
            err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
            goto error;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Allocated Termination[%p] with term name[%s]\n", (void*)term, term->name);

        is_rtp = 0x01;

    /********************************************************************/
    }else{  /* Physical termination */
	    term = megaco_find_termination(mg_profile, (char*)termId->name.lcl.val);

	    if(NULL == term){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			" megaco_find_termination failed for term-id[%s] \n",(char*)termId->name.lcl.val); 	
		    mg_util_set_err_string(&errTxt, " Resource Failure ");
		    err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
		    goto error;
	    }


	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Allocated Termination[%p] with term name[%s]\n", (void*)term, term->name);
    }

    /********************************************************************/
    /* check if termination already is in call */

	    if(term->context){
		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Termination[%p : %s] "
					"already in context[%p -%d]..rejecting ADD \n", 
					(void*)term, term->name, (void*)term->context,term->context->context_id);
		    mg_util_set_err_string(&errTxt, " Term already is in call ");
		    err_code = MGT_MGCO_RSP_CODE_DUP_TERM_CTXT;
		    goto error;
	    }

/********************************************************************/

    ret = mg_prc_descriptors(mg_profile, inc_cmd, term, &inc_cmd->u.mgCmdInd[0]->memCp);

    /* IF there is any error , return */
    if(term->mg_error_code && (*term->mg_error_code == MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT)){
	    mg_util_set_err_string(&errTxt, " Unsupported Codec ");
	    err_code = MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT;
	    goto error;
    }
    /********************************************************************/
    /* associate physical termination to context  */

    if(SWITCH_STATUS_FALSE == megaco_context_add_termination(mg_ctxt, term)){
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"megaco_context_add_termination failed \n");
        mg_util_set_err_string(&errTxt, " Resource Failure ");
        err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
        goto error;
    }

    
    mg_print_t38_attributes(term);

    /********************************************************************/

    /* resp code -- begin */
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
            //mgUtlAllocMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.add.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.add.termIdLst);
	    mgUtlCpyMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.add.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.add.termIdLst, &rsp.u.mgCmdRsp[0]->memCp);

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
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
                return SWITCH_STATUS_FALSE;
            }

            out_termId = rsp.u.mgCmdRsp[0]->u.add.termIdLst.terms[rsp.u.mgCmdRsp[0]->u.add.termIdLst.num.val-1];
            mg_fill_mgco_termid(out_termId, (char*)term->name, strlen((char*)term->name), &rsp.u.mgCmdRsp[0]->memCp);
        }

	if(is_rtp){
		/* Whatever Media descriptor we have received, we can copy that and then
		 * whatever we want we can modify the fields */
		/* Kapil - TODO - will see if there is any problem of coping the
		 * descriptor */

		if (mgUtlGrowList((void ***)&rsp.u.mgCmdRsp[0]->u.add.audit.parms, sizeof(MgMgcoAudRetParm),
					&rsp.u.mgCmdRsp[0]->u.add.audit.num, &rsp.u.mgCmdRsp[0]->memCp) != ROK)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
			return SWITCH_STATUS_FALSE;
		}


		/* copy media descriptor */
		desc = rsp.u.mgCmdRsp[0]->u.add.audit.parms[rsp.u.mgCmdRsp[0]->u.add.audit.num.val-1];
		desc->type.pres = PRSNT_NODEF;
		desc->type.val = MGT_MEDIADESC;
		mgUtlCpyMgMgcoMediaDesc(&desc->u.media, inc_med_desc, &rsp.u.mgCmdRsp[0]->memCp);
		/* see if we have received local descriptor */
		if((NOTPRSNT != desc->u.media.num.pres) && 
				(0 != desc->u.media.num.val))
		{
			for(mediaId=0; mediaId<desc->u.media.num.val; mediaId++) {
				if(MGT_MEDIAPAR_LOCAL == desc->u.media.parms[mediaId]->type.val) {
					local  = &desc->u.media.parms[mediaId]->u.local;
				}
			}
		}


		/* only for RTP */
		if(SWITCH_STATUS_FALSE == mg_build_sdp(&desc->u.media, inc_med_desc, mg_profile, term, &rsp.u.mgCmdRsp[0]->memCp)) {
			if(term->mg_error_code && (*term->mg_error_code == MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT)){
				mg_util_set_err_string(&errTxt, " Unsupported Codec ");
				err_code = MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT;
				goto error;
			}
		}
	}


        /* We will always send one command at a time..*/
        rsp.cmdStatus.pres = PRSNT_NODEF;
        rsp.cmdStatus.val  = CH_CMD_STATUS_END_OF_CMD;

        rsp.cmdType.pres = PRSNT_NODEF;
        rsp.cmdType.val  = CH_CMD_TYPE_RSP;

        ret = sng_mgco_send_cmd( mg_profile->idx, &rsp);

	
	if(is_rtp){
		/* releasing memory allocated for term->lcl.val */
		MG_STACK_MEM_FREE(out_termId->name.lcl.val, ((sizeof(U8)* strlen(term->name))));
	}
    }

    /*************************************************************************************************************************/
    return ret;	

error:
    if (SWITCH_STATUS_SUCCESS == 
            mg_build_mgco_err_request(&mgErr, txn_id, ctxtId, err_code, &errTxt)) {
        sng_mgco_send_err(mg_profile->idx, mgErr);
    }
    if(err_code != MGT_MGCO_RSP_CODE_DUP_TERM_CTXT){
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," ADD Request failed..releasing context/termination(if allocated) \n"); 
	    if(mg_ctxt){
		   /* we can call sub all from context api to release terminations..
		      as it could possible that phy term added to context but 
		     failure happened while adding rtp term, sub_all will release phy term also */
		    megaco_context_sub_all_termination(mg_ctxt);
		    megaco_release_context(mg_ctxt);
	    }
	    if(term){
		    megaco_termination_destroy(term);
	    }
    }
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
	MgMgcoAudRetParm *desc;
	MgMgcoMediaDesc*   inc_med_desc = NULL;
	MgMgcoLocalDesc   *local = NULL;
	int 		  err_code;
	int mediaId;
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				"Modify request processing failure,  CHOOSE Context should not present in Modify\n");

		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		mg_util_set_ctxt_string(&errTxt, ctxtId);
		goto error;
	}
	/***  CHOOSE Termination NOT ALLOWED **/
	else if ((NOTPRSNT != termId->type.pres)          &&
			(MGT_TERMID_CHOOSE == termId->type.val))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
				"Modify request is for ROOT termination \n");

		/* check if we have ito packg request */
		mg_is_ito_pkg_req(mg_profile, inc_cmd);

		/********************************************************************/
	} else if(MGT_TERMID_OTHER == termId->type.val){
		/********************************************************************/
#ifdef BIT_64
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
				"Modify request is for termination[%s] and context: type[%d], value[%d] \n", 
				termId->name.lcl.val, ctxtId->type.val, ctxtId->val.val);
#else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
				"Modify request is for termination[%s] and context: type[%d], value[%ld] \n", 
				termId->name.lcl.val, ctxtId->type.val, ctxtId->val.val);
#endif

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
#ifdef BIT_64
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							"Modify request Failed, context[%d] not found \n",ctxtId->val.val);
#else
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							"Modify request Failed, context[%ld] not found \n",ctxtId->val.val);
#endif
					mg_util_set_ctxt_string(&errTxt, ctxtId);
					err_code = MGT_MGCO_RSP_CODE_UNKNOWN_CTXT;
					goto error;
				}

				if(SWITCH_STATUS_FALSE == megaco_context_is_term_present(mg_ctxt, term)){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							"Modify request Failed, requested term not associated with any context \n");
					/* ERROR - termination didnt bind with requested context */
					mg_util_set_term_string(&errTxt,termId);
					err_code = MGT_MGCO_RSP_CODE_NO_TERM_CTXT;
					goto error;
				}

			}else if(MGT_CXTID_NULL == ctxtId->type.val) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						"Modify request is for NULL Context \n");
				/*TODO - NULL context...nothing to do now...jump to response to send +ve response */
				goto response;
			}else if(MGT_CXTID_ALL == ctxtId->type.val) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						"Modify request is for ALL Context \n");
				/*TODO - ALL context...nothing to do now...jump to response to send +ve response */
				goto response;
			}
		}

		/* Not sure if MODIFY can come with Context ALL with specified term */

		/********************************************************************/

		ret = mg_prc_descriptors(mg_profile, inc_cmd, term, &inc_cmd->u.mgCmdInd[0]->memCp);

		/* IF there is any error , return */
		if(term->mg_error_code && (*term->mg_error_code == MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT)){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
					"Modify request Failed, Unsupported Codec \n"); 
			mg_util_set_err_string(&errTxt, " Unsupported Codec ");
			err_code = MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT;
			goto error;
		}

		if(MG_TERM_RTP == term->type){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"MODIFY REQUEST - Updated RTP attributes:"
					" Media_Type(%s),local_addr[%s] local_port[%d] remote_addr[%s], remote_port[%d], ptime[%d] pt[%d], "
					" rfc2833_pt[%d] rate[%d], codec[%s], term_id[%d]\n",
					mg_media_type2str(term->u.rtp.media_type),
					((NULL != term->u.rtp.local_addr)?term->u.rtp.local_addr:NULL),
					term->u.rtp.local_port,
					((NULL != term->u.rtp.remote_addr)?term->u.rtp.remote_addr:NULL),
					term->u.rtp.remote_port,
					term->u.rtp.ptime,
					term->u.rtp.pt,
					term->u.rtp.rfc2833_pt,
					term->u.rtp.rate,
					((NULL != term->u.rtp.codec)?term->u.rtp.codec:NULL),
					term->u.rtp.term_id);
		}

		mg_print_t38_attributes(term);


		/* SDP updated to termination */
		if(SWITCH_STATUS_SUCCESS != megaco_activate_termination(term)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
					"Modify request Failed, Activation of termination failed \n"); 
			mg_util_set_err_string(&errTxt, " Resource Failure ");
			err_code = MGT_MGCO_RSP_CODE_RSRC_ERROR;
			goto error;
		}
	}

	/********************************************************************/

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

		mgUtlCpyMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.mod.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.mod.termIdLst, &rsp.u.mgCmdRsp[0]->memCp);

#ifdef GCP_VER_2_1
		termId = rsp.u.mgCmdRsp[0]->u.mod.termIdLst.terms[0];
#else
		termId = &(rsp.u.mgCmdRsp[0]->u.mod.termId);
#endif
		if((MGT_TERMID_ROOT != termId->type.val) && 
			(term && (MG_TERM_RTP == term->type) &&
				 ((NOTPRSNT != inc_cmd->u.mgCmdInd[0]->cmd.u.mod.dl.num.pres) && 
				 (0 != inc_cmd->u.mgCmdInd[0]->cmd.u.mod.dl.num.val)))) {
			/* Whatever Media descriptor we have received, we can copy that and then
			 * whatever we want we can modify the fields */
			/* Kapil - TODO - will see if there is any problem of coping the
			 * descriptor */

			inc_med_desc = &inc_cmd->u.mgCmdInd[0]->cmd.u.mod.dl.descs[0]->u.media;

			if (mgUtlGrowList((void ***)&rsp.u.mgCmdRsp[0]->u.mod.audit.parms, sizeof(MgMgcoAudRetParm),
						&rsp.u.mgCmdRsp[0]->u.mod.audit.num, &rsp.u.mgCmdRsp[0]->memCp) != ROK)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
				return SWITCH_STATUS_FALSE;
			}

			/* copy media descriptor */
			desc = rsp.u.mgCmdRsp[0]->u.mod.audit.parms[rsp.u.mgCmdRsp[0]->u.mod.audit.num.val-1];
			desc->type.pres = PRSNT_NODEF;
			desc->type.val = MGT_MEDIADESC;
			mgUtlCpyMgMgcoMediaDesc(&desc->u.media, inc_med_desc, &rsp.u.mgCmdRsp[0]->memCp);
			/* see if we have received local descriptor */
			if((NOTPRSNT != desc->u.media.num.pres) && 
					(0 != desc->u.media.num.val))
			{
				for(mediaId=0; mediaId<desc->u.media.num.val; mediaId++) {
					if(MGT_MEDIAPAR_LOCAL == desc->u.media.parms[mediaId]->type.val) {
						local  = &desc->u.media.parms[mediaId]->u.local;
					}
				}
			}

			/* only for RTP */
			if(SWITCH_STATUS_FALSE == mg_build_sdp(&desc->u.media, inc_med_desc, mg_profile, term, &rsp.u.mgCmdRsp[0]->memCp)) {
				if(term->mg_error_code && (*term->mg_error_code == MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT)){
					mg_util_set_err_string(&errTxt, " Unsupported Codec ");
					err_code = MGT_MGCP_RSP_CODE_INCONSISTENT_LCL_OPT;
					goto error;
				}
			}
		}

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

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," Modify Request failed..releasing context/termination \n"); 
	if(mg_ctxt){
		megaco_release_context(mg_ctxt);
	}
	if(term){
		megaco_termination_destroy(term);
	}
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," SUB Request processing failed, Context Not Present\n");
        mg_util_set_ctxt_string(&errTxt, ctxtId);
        err_code = MGT_MGCO_RSP_CODE_PROT_ERROR;
        goto error;
    }

    if(NOTPRSNT == termId->type.pres){
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," SUB Request processing failed, Termination Not Present\n");
        mg_util_set_term_string(&errTxt,termId);
        err_code = MGT_MGCO_RSP_CODE_PROT_ERROR;
        goto error;
    }

    /*-- NULL Context & CHOOSE Context not applicable for SUB request --*/
    if ((MGT_CXTID_CHOOSE == ctxtId->type.val)     ||
             (MGT_CXTID_NULL == ctxtId->type.val)) {

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," SUB Request processing failed, Context CHOOSE/NULL not allowed\n");

        mg_util_set_ctxt_string(&errTxt, ctxtId);
        err_code = MGT_MGCO_RSP_CODE_PROT_ERROR;
        goto error;
    }
    /* ROOT Termination & CHOOSE Termination not allowed */
    else if ((MGT_TERMID_ROOT == termId->type.val)     ||
             (MGT_TERMID_CHOOSE == termId->type.val)) {

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," SUB Request processing failed, Termination ROOT/CHOOSE not allowed\n");

        mg_util_set_term_string(&errTxt,termId);
        err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
        goto error;
    }
    /************************************************************************************************************************************************************/

    if (MGT_CXTID_OTHER == ctxtId->type.val){

#ifdef BIT_64
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," SUB Request for Context[%d] \n", ctxtId->val.val);
#else
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," SUB Request for Context[%ld] \n",  ctxtId->val.val);
#endif

        /*find context based on received context-id */
        mg_ctxt = megaco_get_context(mg_profile, ctxtId->val.val);
        if(NULL == mg_ctxt){
            mg_util_set_ctxt_string(&errTxt, ctxtId);
            err_code = MGT_MGCO_RSP_CODE_UNKNOWN_CTXT;
            goto error;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Found Context[%p] for context_id[%d]\n", (void*)mg_ctxt, mg_ctxt->context_id);        
        if(MGT_TERMID_ALL == termId->type.val){

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," SUB Request for context[%d] with ALL termination   \n", mg_ctxt->context_id);

            /* remove terminations from context */
            megaco_context_sub_all_termination(mg_ctxt);

        }else if(MGT_TERMID_OTHER == termId->type.val){

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," SUB Request for termination[%s]  \n", (char*)termId->name.lcl.val);

            term = megaco_find_termination(mg_profile, (char*)termId->name.lcl.val);

            if(SWITCH_STATUS_FALSE == megaco_context_is_term_present(mg_ctxt, term)){
		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				    "Subtract request Failed, termination no associated with any context \n"); 
                /* ERROR - termination didnt bind with requested context */
                mg_util_set_term_string(&errTxt,termId);
                err_code = MGT_MGCO_RSP_CODE_NO_TERM_CTXT;
                goto error;
            }

            if(NULL == term){
		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				    "Subtract request Failed, no termination found for input term string[%s] \n", (char*)termId->name.lcl.val); 
                mg_util_set_term_string(&errTxt,termId);
                err_code = MGT_MGCO_RSP_CODE_UNKNOWN_TERM_ID;
                goto error;
            }

            /* remove termination from context */
            megaco_context_sub_termination(mg_ctxt, term);
        }

        if ((NULL == mg_ctxt->terminations[0]) && (NULL == mg_ctxt->terminations[1])) {
            /* release context*/
            megaco_release_context(mg_ctxt);
        }
    }

    /************************************************************************************************************************************************************/
    /* CONTEXT = ALL */

    if (MGT_CXTID_ALL == ctxtId->type.val){

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," SUB Request for ALL context \n");

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

        //mgUtlAllocMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.add.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.add.termIdLst);
	mgUtlCpyMgMgcoTermIdLst(&rsp.u.mgCmdRsp[0]->u.add.termIdLst, &inc_cmd->u.mgCmdReq[0]->cmd.u.add.termIdLst, &rsp.u.mgCmdRsp[0]->memCp);

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
    return SWITCH_STATUS_FALSE;	
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

#if 0
#ifdef BIT_64
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
			"mg_send_end_of_axn: Sending END_OF_AXN for transId[%d], peerId[%d], context[type = %s, value = %d]\n",
			transId->val, peerId->val, PRNT_MG_CTXT_TYPE(ctxtId->type.val), ctxtId->val.val);
#else
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
			"mg_send_end_of_axn: Sending END_OF_AXN for transId[%lu], peerId[%lu], context[type = %s, value = %lu]\n",
			transId->val, peerId->val, PRNT_MG_CTXT_TYPE(ctxtId->type.val), ctxtId->val.val);

#endif
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

#ifdef BIT_64
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
			"Sending Error Request with erro code[%d] for trans_id[%d]\n",trans_id, err);
#else
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
			"Sending Error Request with erro code[%ld] for trans_id[%ld]\n",trans_id, err);
#endif

	/* Allocate for AG error */
	mg_stack_alloc_mem((Ptr*)&mgErr, sizeof(MgMgcoInd));
	if (NULL == mgErr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mg_build_mgco_err_request Failed : memory alloc \n"); 
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mg_build_mgco_err_request Failed : memory alloc \n"); 
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
switch_status_t handle_mg_audit_cmd( megaco_profile_t* mg_profile, MgMgcoCommand *auditReq)
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
	memset(&ctxt, 0, sizeof(ctxt));

	audit 	   = &auditReq->u.mgCmdReq[0]->cmd.u.aval;
	wild 	   = auditReq->u.mgCmdReq[0]->wild.pres;

	if(NOTPRSNT == audit->pres.pres){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Audit structure not present..rejecting \n");
		return SWITCH_STATUS_FALSE;
	}

	audit_desc = &audit->audit;

	if((NOTPRSNT == audit_desc->pres.pres) || ( NOTPRSNT == audit_desc->num.pres)){
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Audit Descriptor not present.. Could be HeartBeat message\n");
		return mg_send_heartbeat_audit_rsp(mg_profile->idx, auditReq);
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

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,	
				"failed, Ctxt: CHOOSE not allowed in Audit Value\n");

		/* set correct error code */
		mg_util_set_ctxt_string(&errTxt,ctxtId);
		err_code = MGT_MGCO_RSP_CODE_INVLD_IDENTIFIER;
		goto error;
	}
	/*--  CHOOSE Termination not allowed --*/
	else if ((NOTPRSNT != termId->type.pres)          &&
			(MGT_TERMID_CHOOSE == termId->type.val)) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,	
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

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,	
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
	
	mgUtlCpyMgMgcoTermIdLst(&adtRep->u.other.termIdLst, term_list, &reply.u.mgCmdRsp[0]->memCp);

	/* NOW for each requested AUDIT descriptor we need to add entry to adtRep->u.other.audit.parms list */

     /*********************************************************************************************************************/
     /**************************** Processing Audit Request Descriptors **************************************************/
     /*********************************************************************************************************************/

	for (i = 0; i < audit_desc->num.val; i++) {

		audit_item = audit_desc->al[i];

		if (!audit_item) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,	
					"Audit Descriptor is NULL.. rejecting \n");
			return SWITCH_STATUS_FALSE; 
		}

		/*TODO - If we are not supporint AUDIT type then can send "MGT_MGCO_RSP_CODE_UNSUPPORTED_DESC" error to MG stack */
		if (NOTPRSNT != audit_item->auditItem.pres) {

			switch(audit_item->auditItem.val)
			{  
				case MGT_MEDIADESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing MEDIA \n");

						/* Grow the list of reply parameters */
						if (mgUtlGrowList((void ***)&adtRep->u.other.audit.parms, sizeof(MgMgcoAudRetParm),
									&adtRep->u.other.audit.num, &reply.u.mgCmdRsp[0]->memCp) != ROK)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
							return SWITCH_STATUS_FALSE;
						}

						numOfParms = adtRep->u.other.audit.num.val;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.pres = PRSNT_NODEF;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.val  = MGT_MEDIADESC;

						media = get_default_media_desc(mg_profile, termId, &reply.u.mgCmdRsp[0]->memCp);
						if(!media){
							return SWITCH_STATUS_FALSE;
						}
						mgUtlCpyMgMgcoMediaDesc(&adtRep->u.other.audit.parms[numOfParms - 1]->u.media, media, &reply.u.mgCmdRsp[0]->memCp);

						break;
					}
				case MGT_MODEMDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing MODEM \n");
						break;
					}
				case MGT_MUXDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing MULTIPLEX \n");
						break;
					}
				case MGT_REQEVTDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing Events \n");
						break;
					}
				case MGT_SIGNALSDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing Signals \n");
						break;
					}
				case MGT_DIGMAPDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing Digit Maps \n");
						break;
					}
				case MGT_OBSEVTDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing Buffer Events \n");
						break;
					}
				case MGT_EVBUFDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing  Events Buffer \n");
						break;
					}
				case MGT_STATSDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing  Statistics \n");
						break;
					}
				case MGT_PKGSDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Auditing  Packages \n");
						/* Grow the list of reply parameters */
						if (mgUtlGrowList((void ***)&adtRep->u.other.audit.parms, sizeof(MgMgcoAudRetParm),
									&adtRep->u.other.audit.num, &reply.u.mgCmdRsp[0]->memCp) != ROK)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
							return SWITCH_STATUS_FALSE;
						}

						numOfParms = adtRep->u.other.audit.num.val;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.pres = PRSNT_NODEF;
						adtRep->u.other.audit.parms[numOfParms - 1]->type.val  = MGT_PKGSDESC;

						if(SWITCH_STATUS_FALSE == mg_build_pkg_desc(&adtRep->u.other.audit.parms[numOfParms - 1]->u.pkgs, &reply.u.mgCmdRsp[0]->memCp)){
							return SWITCH_STATUS_FALSE;
						}

						break;
					}
				case MGT_INDAUD_TERMAUDDESC:
					{  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Individual Term  Audit \n");
						break;
					}
				default:
					{
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Invalid Audit Descriptor[%d] request\n",audit_item->auditItem.val);
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
	sng_mgco_send_cmd(mg_profile->idx, &reply);

	/* send indication to stack , so he can send response back to peer */
	memcpy(&ctxt.transId,&auditReq->transId,sizeof(MgMgcoTransId));
	memcpy(&ctxt.cntxtId, &auditReq->contextId,sizeof(MgMgcoContextId));
	memcpy(&ctxt.peerId, &auditReq->peerId,sizeof(TknU32));
	ctxt.cmdStatus.pres = PRSNT_NODEF;
	ctxt.cmdStatus.val  = CH_CMD_STATUS_END_OF_AXN;
	sng_mgco_send_axn_req(mg_profile->idx, &ctxt);
	/***********************************************************************************************************************************/

	return SWITCH_STATUS_SUCCESS;

error:
	if (SWITCH_STATUS_SUCCESS == mg_build_mgco_err_request(&mgErr, auditReq->transId.val, ctxtId, err_code, &errTxt)) {
		sng_mgco_send_err(mg_profile->idx, mgErr);
	}

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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Audit structure not present..rejecting \n");
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

	mgUtlCpyMgMgcoTermIdLst(&adtRep->u.other.termIdLst, term_list, &reply.u.mgCmdRsp[0]->memCp);


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
U32 get_txn_id(){
	outgoing_txn_id++;
	return outgoing_txn_id;
}
/*****************************************************************************************************************************/
switch_status_t mg_is_peer_active(megaco_profile_t* profile)
{
	if((profile) && (0x01 == profile->peer_active)){
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}
/*****************************************************************************************************************************/
switch_status_t mg_send_term_service_change(char *span_name, char *chan_number, mg_term_states_e term_state)
{
	mg_termination_t* term = NULL;
	switch_status_t  ret = SWITCH_STATUS_SUCCESS;

	switch_assert(span_name);
	switch_assert(chan_number);

	term = 	megaco_term_locate_by_span_chan_id(span_name, chan_number);

	if(!term || !term->profile){
		return SWITCH_STATUS_FALSE;
	}

    if(SWITCH_STATUS_FALSE == mg_is_peer_active(term->profile))
    {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "profile: %s peer not yet enabled..\n", term->profile->name);
		return SWITCH_STATUS_FALSE;
	}

	switch(term_state)
	{
		case MG_TERM_SERVICE_STATE_IN_SERVICE:
			{
				if(switch_test_flag(term, MG_OUT_OF_SERVICE)){
					/* set INS flag...clear oos flag */
					switch_clear_flag(term, MG_OUT_OF_SERVICE);
					switch_set_flag(term, MG_IN_SERVICE);
					ret = mg_send_ins_service_change(term->profile, term->name, 0x00 );
				}
				break;
			}
		case MG_TERM_SERVICE_STATE_OUT_OF_SERVICE:
			{
				if(switch_test_flag(term, MG_IN_SERVICE)){
					/* set OOS flag...clear ins flag */
					switch_clear_flag(term, MG_IN_SERVICE);
					switch_set_flag(term, MG_OUT_OF_SERVICE);
					ret = mg_send_oos_service_change(term->profile, term->name, 0x00 );
				}
				break;
			}
		default:
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," Invalid term_state[%d]\n", term_state);
				return SWITCH_STATUS_FALSE;
			}
	}

	return ret;
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
	MgMgcoTermId*   termId;
	switch_status_t ret;
	MgMgcoCommand   request;
	MgMgcoSvcChgReq      *svc;

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

	if(SWITCH_STATUS_FALSE == (ret = mg_fill_svc_change(&svc->parm, method, mg_service_change_reason[reason], &request.u.mgCmdReq[0]->memCp))){
		return ret;
	}

	if (mgUtlGrowList((void ***)&svc->termIdLst.terms, sizeof(MgMgcoTermIdLst),
				&svc->termIdLst.num, &request.u.mgCmdReq[0]->memCp) != ROK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
		goto err;	
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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Sending %s Service Change for termId[%s] with reason[%s], len[%d]\n",
			((1==wild)?"WildCard":"Non Wild Card"), term_name, svc->parm.reason.val, svc->parm.reason.len);

	sng_mgco_send_cmd(suId, &request);

	/* releasing memory allocated for term->lcl.val */
	MG_STACK_MEM_FREE(termId->name.lcl.val, ((sizeof(U8)* strlen(term_name))));

	return SWITCH_STATUS_SUCCESS;

err:
	mgUtlDelMgMgcoSvcChgPar(&svc->parm);
	return SWITCH_STATUS_FALSE;
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
/* API to send T.38 CNG tone Notification */
switch_status_t  mg_send_t38_fax_con_change_notify(megaco_profile_t* mg_profile, const char* term_name, char* state)
{
    MgMgcoObsEvt *oevt;
    MgMgcoEvtPar* param;

    switch_assert(term_name);
    switch_assert(mg_profile);
    switch_assert(state);

    mg_stack_alloc_mem((Ptr*)&oevt, sizeof(MgMgcoObsEvt));

    oevt->pres.pres = PRSNT_NODEF;

    mg_get_time_stamp(&oevt->time);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.pkgType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.valType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.u.val), MGT_PKG_FAX);

    MG_INIT_TOKEN_VALUE(&(oevt->name.type),MGT_GEN_TYPE_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->name.u.val),(U8)MGT_PKG_ENUM_REQEVTIPFAXFAXCONNSTATECHNG);

    if (mgUtlGrowList((void ***)&oevt->pl.parms, sizeof(MgMgcoEvtPar), &oevt->pl.num, NULL) != ROK) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

    param = oevt->pl.parms[0];

    MG_INIT_TOKEN_VALUE(&(param->type),(U8)MGT_EVTPAR_OTHER);

    MG_INIT_TOKEN_VALUE(&(param->u.other.name.type),MGT_GEN_TYPE_KNOWN);
    MG_INIT_TOKEN_VALUE(&(param->u.other.name.u.val), MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNG);

    MG_INIT_TOKEN_VALUE(&(param->u.other.val.type),MGT_VALUE_EQUAL);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.type),MGT_VALTYPE_ENUM);
    if(!strcasecmp(state,"connected")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGCONNECTED);
    } else if(!strcasecmp(state,"negotiating")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGNEGOTIATING);
    } else if(!strcasecmp(state,"disconnect")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGDISCONNECT);
    } else if(!strcasecmp(state,"prepare")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGPREPARE);
    } else if(!strcasecmp(state,"TrainR")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGTRAINR);
    } else if(!strcasecmp(state,"TrainT")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGTRAINT);
    } else if(!strcasecmp(state,"EOP")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGEOP);
    } else if(!strcasecmp(state,"ProcInterrupt")){
	    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERIPFAXFAXCONNSTATECHNGFAXCONNSTATECHNGPROCINTR);
    }else{
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Invalid input state[%s] param\n", state);
	    return SWITCH_STATUS_FALSE;
    }

    return mg_send_notify(mg_profile, term_name, oevt);
}
switch_status_t  mg_send_t38_v21flag_notify(megaco_profile_t* mg_profile, const char* term_name)
{
    MgMgcoObsEvt *oevt;
    MgMgcoEvtPar* param;

	switch_assert(term_name);
	switch_assert(mg_profile);

    mg_stack_alloc_mem((Ptr*)&oevt, sizeof(MgMgcoObsEvt));

    oevt->pres.pres = PRSNT_NODEF;

    mg_get_time_stamp(&oevt->time);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.pkgType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.valType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.u.val), MGT_PKG_CALL_TYP_DISCR);

    MG_INIT_TOKEN_VALUE(&(oevt->name.type),MGT_GEN_TYPE_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->name.u.val),(U8)MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYPCNG);

    if (mgUtlGrowList((void ***)&oevt->pl.parms, sizeof(MgMgcoEvtPar), &oevt->pl.num, NULL) != ROK) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

    param = oevt->pl.parms[0];

    MG_INIT_TOKEN_VALUE(&(param->type),(U8)MGT_EVTPAR_OTHER);

    MG_INIT_TOKEN_VALUE(&(param->u.other.name.type),MGT_GEN_TYPE_KNOWN);
    MG_INIT_TOKEN_VALUE(&(param->u.other.name.u.val), MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYP);

    MG_INIT_TOKEN_VALUE(&(param->u.other.val.type),MGT_VALUE_EQUAL);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.type),MGT_VALTYPE_ENUM);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYPV21FLG);


    return mg_send_notify(mg_profile, term_name, oevt);
}

switch_status_t  mg_send_t38_ans_notify(megaco_profile_t* mg_profile, const char* term_name)
{
    MgMgcoObsEvt *oevt;
    MgMgcoEvtPar* param;

	switch_assert(term_name);
	switch_assert(mg_profile);

    mg_stack_alloc_mem((Ptr*)&oevt, sizeof(MgMgcoObsEvt));

    oevt->pres.pres = PRSNT_NODEF;

    mg_get_time_stamp(&oevt->time);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.pkgType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.valType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.u.val), MGT_PKG_CALL_TYP_DISCR);

    MG_INIT_TOKEN_VALUE(&(oevt->name.type),MGT_GEN_TYPE_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->name.u.val),(U8)MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYPCNG);

    if (mgUtlGrowList((void ***)&oevt->pl.parms, sizeof(MgMgcoEvtPar), &oevt->pl.num, NULL) != ROK) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

    param = oevt->pl.parms[0];

    MG_INIT_TOKEN_VALUE(&(param->type),(U8)MGT_EVTPAR_OTHER);

    MG_INIT_TOKEN_VALUE(&(param->u.other.name.type),MGT_GEN_TYPE_KNOWN);
    MG_INIT_TOKEN_VALUE(&(param->u.other.name.u.val), MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYP);

    MG_INIT_TOKEN_VALUE(&(param->u.other.val.type),MGT_VALUE_EQUAL);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.type),MGT_VALTYPE_ENUM);
    //MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYPANSBAR);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYPANSAM);


    return mg_send_notify(mg_profile, term_name, oevt);
}

switch_status_t  mg_send_t38_cng_notify(megaco_profile_t* mg_profile, const char* term_name)
{
    MgMgcoObsEvt *oevt;
    MgMgcoEvtPar* param;

    switch_assert(term_name);
    switch_assert(mg_profile);


    mg_stack_alloc_mem((Ptr*)&oevt, sizeof(MgMgcoObsEvt));

    oevt->pres.pres = PRSNT_NODEF;

    mg_get_time_stamp(&oevt->time);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.pkgType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.valType), MGT_PKG_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->pkg.u.val), MGT_PKG_CALL_TYP_DISCR);

    MG_INIT_TOKEN_VALUE(&(oevt->name.type),MGT_GEN_TYPE_KNOWN);

    MG_INIT_TOKEN_VALUE(&(oevt->name.u.val),(U8)MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYPCNG);

    if (mgUtlGrowList((void ***)&oevt->pl.parms, sizeof(MgMgcoEvtPar), &oevt->pl.num, NULL) != ROK) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

    param = oevt->pl.parms[0];

    MG_INIT_TOKEN_VALUE(&(param->type),(U8)MGT_EVTPAR_OTHER);

    MG_INIT_TOKEN_VALUE(&(param->u.other.name.type),MGT_GEN_TYPE_KNOWN);
    MG_INIT_TOKEN_VALUE(&(param->u.other.name.u.val), MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYP);

    MG_INIT_TOKEN_VALUE(&(param->u.other.val.type),MGT_VALUE_EQUAL);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.type),MGT_VALTYPE_ENUM);
    MG_INIT_TOKEN_VALUE(&(param->u.other.val.u.eq.u.enume),MGT_PKG_ENUM_OBSEVTOTHERCALLTYPDISCRDISCTONEDETDISCTONETYPCNG);

     return mg_send_notify(mg_profile, term_name, oevt);
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"num_of_collected_digits cannt be ZERO \n");
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
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

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Sending Notify Message for termination[%s] !\n", term_name);

    MG_ZERO(&request, sizeof(request));
    MG_ZERO(&reqId, sizeof(reqId));

    if(strcmp(term_name, "ROOT")){
        /* Not ROOT term then --- */
        term = megaco_find_termination(mg_profile, (char*)term_name);

        if(!term){
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No termination configured for given name[%s] !\n", term_name);
            return SWITCH_STATUS_FALSE;
        }

        if(NULL == term->active_events){
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No Active events observed on given termination[%s] !\n", term_name);
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
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
    if(term && term->context){
	printf("Temrination is in context, adding context-id[%d]\n",term->context->context_id);
	    request.contextId.type.val  = MGT_CXTID_OTHER;
	    request.contextId.val.pres  = PRSNT_NODEF;
	    request.contextId.val.val  = term->context->context_id;
    } else{
	    request.contextId.type.val  = MGT_CXTID_NULL;
    }
    request.cmdStatus.pres = PRSNT_NODEF;
    request.cmdStatus.val = CH_CMD_STATUS_END_OF_TXN;

    request.cmdType.pres = PRSNT_NODEF;
    request.cmdType.val  = CH_CMD_TYPE_REQ;

    /* fill termination */
    if (mgUtlGrowList((void ***)&request.u.mgCmdReq[0]->cmd.u.ntfy.termIdLst.terms, sizeof(MgMgcoTermIdLst),
                &request.u.mgCmdReq[0]->cmd.u.ntfy.termIdLst.num, &request.u.mgCmdReq[0]->memCp) != ROK)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Grow List failed\n");
        return SWITCH_STATUS_FALSE;
    }

#ifdef GCP_VER_2_1
    termId = request.u.mgCmdReq[0]->cmd.u.ntfy.termIdLst.terms[0];
#else
    termId = &(request.u.mgCmdReq[0]->cmd.u.ntfy.termId);
#endif

    mg_fill_mgco_termid(termId, (char*)term_name ,strlen(term_name), &request.u.mgCmdReq[0]->memCp);

    sng_mgco_send_cmd(mg_profile->idx, &request);

    /* releasing memory allocated for term->lcl.val */
    MG_STACK_MEM_FREE(termId->name.lcl.val, ((sizeof(U8)* strlen(term_name))));

    return SWITCH_STATUS_SUCCESS;
}
/*****************************************************************************************************************************/

