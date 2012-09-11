/*
 * Copyright (c) 2012, Kapil Gupta <kgupta@sangoma.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Contributors: 
 *
 *
 */

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/
/* FUNCTION PROTOTYPES ********************************************************/
static int ftmod_tucl_gen_config(void);
static int ftmod_tucl_sap_config(int id);
static int ftmod_sctp_gen_config(void);
static int ftmod_cfg_sctp(void);
static int ftmod_sctp_config(int id);
static ftdm_status_t ftmod_sctp_sap_config(int id);
static ftdm_status_t ftmod_sctp_tsap_config(int id);
static int ftmod_m2ua_gen_config(void);
static int ftmod_m2ua_sctsap_config(int sct_sap_id, int sctp_id);
static int ftmod_m2ua_peer_config(int id);
static int ftmod_m2ua_peer_config1(int m2ua_inf_id, int peer_id);
static int ftmod_m2ua_cluster_config(int idx);
static int ftmod_m2ua_dlsap_config(int idx);
static int ftmod_nif_gen_config(void);
static int ftmod_nif_dlsap_config(int idx);
static int ftmod_sctp_tucl_tsap_bind(int idx);
static int ftmod_m2ua_sctp_sctsap_bind(int idx);
static int ftmod_open_endpoint(int idx);
static int ftmod_init_sctp_assoc(int peer_id);
static int ftmod_nif_m2ua_dlsap_bind(int id);
static int ftmod_nif_mtp2_dlsap_bind(int id);
static int ftmod_m2ua_debug(int action);
static int ftmod_tucl_debug(int action);
static int ftmod_sctp_debug(int action);

static int ftmod_ss7_sctp_shutdown(void);
static int ftmod_ss7_m2ua_shutdown(void);
static int ftmod_ss7_tucl_shutdown(void);


/******************************************************************************/
ftdm_status_t ftmod_ss7_m2ua_init(void) 
{
	/****************************************************************************************************/
	if (sng_isup_init_nif()) {
		ftdm_log (FTDM_LOG_ERROR , "Failed to start NIF\n");
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"Started NIF!\n");
	}
	/****************************************************************************************************/

	if (sng_isup_init_m2ua()) {
		ftdm_log (FTDM_LOG_ERROR ,"Failed to start M2UA\n");
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"Started M2UA!\n");
	}
	/****************************************************************************************************/

	if (sng_isup_init_sctp()) {
		ftdm_log (FTDM_LOG_ERROR ,"Failed to start SCTP\n");
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"Started SCTP!\n");
	}
	/****************************************************************************************************/

	if (sng_isup_init_tucl()) {
		ftdm_log (FTDM_LOG_ERROR ,"Failed to start TUCL\n");
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"Started TUCL!\n");
		sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_TUCL_PRESENT);
	}
	/****************************************************************************************************/

	if(ftmod_tucl_gen_config()){
		ftdm_log (FTDM_LOG_ERROR ,"TUCL GEN configuration: NOT OK\n");
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"TUCL GEN configuration: OK\n");
	}
	/****************************************************************************************************/
	if(ftmod_sctp_gen_config()){
		ftdm_log (FTDM_LOG_ERROR ,"SCTP GEN configuration: NOT OK\n");
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"SCTP GEN configuration: OK\n");
	}
	/****************************************************************************************************/
	if(ftmod_m2ua_gen_config()) {
		ftdm_log (FTDM_LOG_ERROR ,"M2UA General configuration: NOT OK\n");
		return FTDM_FAIL;
	}else {
		ftdm_log (FTDM_LOG_INFO ,"M2UA General configuration: OK\n");
	}
	/****************************************************************************************************/
	if(ftmod_nif_gen_config()){
		ftdm_log (FTDM_LOG_ERROR ,"NIF General configuration: NOT OK\n");
		return FTDM_FAIL;
	}else {
		ftdm_log (FTDM_LOG_INFO ,"NIF General configuration: OK\n");
	}
	/****************************************************************************************************/


	return FTDM_SUCCESS;
}

/******************************************************************************/
void ftmod_ss7_m2ua_free()
{
	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_M2UA_PRESENT)) {
		ftmod_ss7_m2ua_shutdown();
		sng_isup_free_m2ua();
	}
	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_SCTP_PRESENT)) {
		ftmod_ss7_sctp_shutdown();
		sng_isup_free_sctp();
	}
	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_TUCL_PRESENT)) {
		ftmod_ss7_tucl_shutdown();
		sng_isup_free_tucl();
	}
}

/******************************************************************************/
static int ftmod_ss7_tucl_shutdown()
{
	Pst pst;
	HiMngmt cntrl;  

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(HiMngmt));

	smPstInit(&pst);

	pst.dstEnt = ENTHI;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTHI;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STGEN;       /* General */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	cntrl.t.cntrl.action    = ASHUTDOWN;

	return (sng_cntrl_tucl (&pst, &cntrl));
}
/******************************************************************************/
static int ftmod_ss7_m2ua_shutdown()
{
	Pst pst;
	MwMgmt cntrl;  

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(MwMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTMW;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTMW;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STMWGEN;       /* General */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	cntrl.t.cntrl.action = ASHUTDOWN;

	return (sng_cntrl_m2ua (&pst, &cntrl));
}
/***********************************************************************************************************************/
static int ftmod_ss7_sctp_shutdown()
{
	Pst pst;
	SbMgmt cntrl;  

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(SbMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTSB;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTSB;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STSBGEN;       /* General */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	cntrl.t.cntrl.action = ASHUTDOWN;

	return (sng_cntrl_sctp (&pst, &cntrl));
}

/******************************************************************************/



ftdm_status_t ftmod_ss7_m2ua_cfg(void)
{
	int x=0;

	/* SCTP configuration */
	if(ftmod_cfg_sctp()){
		ftdm_log (FTDM_LOG_ERROR ,"SCTP Configuration : NOT OK\n");
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"SCTP Configuration : OK\n");
	}

	/****************************************************************************************************/
	/* M2UA SCTP SAP configurations */
	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].id !=0) && 
				(!(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags & SNGSS7_CONFIGURED))) {

			/****************************************************************************************************/
			/* M2UA PEER configurations */

			if(ftmod_m2ua_peer_config(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"M2UA PEER configuration for M2UA INTF[%d] : NOT OK\n", x);
				return FTDM_FAIL;
			}else {
				ftdm_log (FTDM_LOG_INFO ,"M2UA PEER configuration for M2UA INTF[%d] : OK\n", x);
			}
			/****************************************************************************************************/
			/* M2UA Cluster configurations */

			if(ftmod_m2ua_cluster_config(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"M2UA CLUSTER configuration for M2UA INTF[%d] : NOT OK\n", x);
				return FTDM_FAIL;
			}else {
				ftdm_log (FTDM_LOG_INFO ,"M2UA CLUSTER configuration for M2UA INTF[%d]: OK\n", x);
			}

			/****************************************************************************************************/

			/* Send the USAP (DLSAP) configuration request for M2UA layer; fill the number
			 * of saps required to be configured. Max is 3 */ 
			if(ftmod_m2ua_dlsap_config(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"M2UA DLSAP[%d] configuration: NOT OK\n", x);
				return FTDM_FAIL;
			}else {
				ftdm_log (FTDM_LOG_INFO ,"M2UA DLSAP[%d] configuration: OK\n", x);
			}
		} /* END - SNGSS7_CONFIGURED */
		g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags |= SNGSS7_CONFIGURED;
		x++;
	}/* END - M2UA Interfaces for loop*/
/****************************************************************************************************/
	/* NIF DLSAP */

	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if ((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].id !=0) && 
				(!(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].flags & SNGSS7_CONFIGURED))) {
			if(ftmod_nif_dlsap_config(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"NIF DLSAP[%d] configuration: NOT OK\n", x);
				return FTDM_FAIL;
			}else{
				ftdm_log (FTDM_LOG_INFO ,"NIF DLSAP[%d] configuration: OK\n", x);
			}
		}
		g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].flags |= SNGSS7_CONFIGURED;
		x++;
	}

	/* successfully started all the layers , not SET STARTED FLAGS */
	sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_NIF_STARTED);
	sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_M2UA_STARTED);
	sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_SCTP_STARTED);
	sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_TUCL_STARTED);


	return 0;
}

/****************************************************************************************************/
static int ftmod_tucl_gen_config(void)
{
	HiMngmt cfg;
	Pst             pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTHI;

	/* clear the configuration structure */
	memset(&cfg, 0, sizeof(cfg));

	/* fill in the post structure */
	smPstInit(&cfg.t.cfg.s.hiGen.lmPst);
	/*fill in the specific fields of the header */
	cfg.hdr.msgType                 		= TCFG;
	cfg.hdr.entId.ent               		= ENTHI;
	cfg.hdr.entId.inst              		= S_INST;
	cfg.hdr.elmId.elmnt     			= STGEN;

	cfg.t.cfg.s.hiGen.numSaps                       = HI_MAX_SAPS;          		/* number of SAPs */
	cfg.t.cfg.s.hiGen.numCons                       = HI_MAX_NUM_OF_CON;    		/* maximum num of connections */
	cfg.t.cfg.s.hiGen.numFdsPerSet                  = HI_MAX_NUM_OF_FD_PER_SET;     	/* maximum num of fds to use per set */
	cfg.t.cfg.s.hiGen.numFdBins                     = HI_MAX_NUM_OF_FD_HASH_BINS;   	/* for fd hash lists */
	cfg.t.cfg.s.hiGen.numClToAccept                 = HI_MAX_NUM_OF_CLIENT_TO_ACCEPT; 	/* clients to accept simultaneously */
	cfg.t.cfg.s.hiGen.permTsk                       = TRUE;                 		/* schedule as perm task or timer */
	cfg.t.cfg.s.hiGen.schdTmrVal                    = HI_MAX_SCHED_TMR_VALUE;               /* if !permTsk - probably ignored */
	cfg.t.cfg.s.hiGen.selTimeout                    = HI_MAX_SELECT_TIMEOUT_VALUE;          /* select() timeout */

	/* number of raw/UDP messages to read in one iteration */
	cfg.t.cfg.s.hiGen.numRawMsgsToRead              = HI_MAX_RAW_MSG_TO_READ;
	cfg.t.cfg.s.hiGen.numUdpMsgsToRead              = HI_MAX_UDP_MSG_TO_READ;

	/* thresholds for congestion on the memory pool */
	cfg.t.cfg.s.hiGen.poolStrtThr                   = HI_MEM_POOL_START_THRESHOLD;
	cfg.t.cfg.s.hiGen.poolDropThr                   = HI_MEM_POOL_DROP_THRESHOLD;
	cfg.t.cfg.s.hiGen.poolStopThr                   = HI_MEM_POOL_STOP_THRESHOLD;

	cfg.t.cfg.s.hiGen.timeRes                       = SI_PERIOD;        /* time resolution */

#ifdef HI_SPECIFY_GENSOCK_ADDR
	cfg.t.cfg.s.hiGen.ipv4GenSockAddr.address = CM_INET_INADDR_ANY;
	cfg.t.cfg.s.hiGen.ipv4GenSockAddr.port  = 0;                            /* DAVIDY - why 0? */
#ifdef IPV6_SUPPORTED
	cfg.t.cfg.s.hiGen.ipv6GenSockAddr.address = CM_INET_INADDR6_ANY;
	cfg.t.cfg.s.hiGen.ipv4GenSockAddr.port  = 0;
#endif
#endif

	return(sng_cfg_tucl(&pst, &cfg));
}
/****************************************************************************************************/

static int ftmod_tucl_sap_config(int id)
{
        HiMngmt cfg;
        Pst     pst;
        HiSapCfg  *pCfg;

	sng_sctp_link_t *k = &g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[id];

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTHI;

        /* clear the configuration structure */
        memset(&cfg, 0, sizeof(cfg));

	/*fill LM  post structure*/
	cfg.t.cfg.s.hiGen.lmPst.dstProcId   = SFndProcId();
	cfg.t.cfg.s.hiGen.lmPst.dstInst     = S_INST;

	cfg.t.cfg.s.hiGen.lmPst.dstProcId   = SFndProcId();
	cfg.t.cfg.s.hiGen.lmPst.dstEnt      = ENTSM;
	cfg.t.cfg.s.hiGen.lmPst.dstInst     = S_INST;

	cfg.t.cfg.s.hiGen.lmPst.prior       = PRIOR0;
	cfg.t.cfg.s.hiGen.lmPst.route       = RTESPEC;
	cfg.t.cfg.s.hiGen.lmPst.region      = S_REG;
	cfg.t.cfg.s.hiGen.lmPst.pool        = S_POOL;
	cfg.t.cfg.s.hiGen.lmPst.selector    = 0;


        /*fill in the specific fields of the header */
        cfg.hdr.msgType         = TCFG;
        cfg.hdr.entId.ent       = ENTHI;
        cfg.hdr.entId.inst      = 0;
        cfg.hdr.elmId.elmnt     = STTSAP;

        pCfg = &cfg.t.cfg.s.hiSap;

        pCfg->spId 	= k->id ; /* each SCTP link there will be one tucl sap */ 
        pCfg->uiSel 	= 0x00;  /*loosley coupled */
        pCfg->flcEnb = TRUE;
        pCfg->txqCongStrtLim = HI_SAP_TXN_QUEUE_CONG_START_LIMIT;
        pCfg->txqCongDropLim = HI_SAP_TXN_QUEUE_CONG_DROP_LIMIT;
        pCfg->txqCongStopLim = HI_SAP_TXN_QUEUE_CONG_STOP_LIMIT;
        pCfg->numBins = 10;

        pCfg->uiMemId.region = S_REG;
        pCfg->uiMemId.pool   = S_POOL;
        pCfg->uiPrior        = PRIOR0;
        pCfg->uiRoute        = RTESPEC;

        return(sng_cfg_tucl(&pst, &cfg));
}

/****************************************************************************************************/
 
static int ftmod_sctp_gen_config(void)
{
	SbMgmt  cfg;
	Pst             pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSB;

	/* clear the configuration structure */
	memset(&cfg, 0, sizeof(cfg));

	/* fill in the post structure */
	smPstInit(&cfg.t.cfg.s.genCfg.smPst);
	/*fill in the specific fields of the header */
	cfg.hdr.msgType                                         = TCFG;
	cfg.hdr.entId.ent                                       = ENTSB;
	cfg.hdr.entId.inst                                      = S_INST;
	cfg.hdr.elmId.elmnt                             	= STSBGEN;

#ifdef SB_IPV6_SUPPORTED
	/* U8          ipv6SrvcReqdFlg; */ /* IPV6 service required for sctp */
#endif

	cfg.t.cfg.s.genCfg.serviceType                          = HI_SRVC_RAW_SCTP;             	/* Usr packetized TCP Data */    /* TUCL transport protocol (IP/UDP) */
	cfg.t.cfg.s.genCfg.maxNmbSctSaps                        = SB_MAX_SCT_SAPS;                      /* max no. SCT SAPS */
	cfg.t.cfg.s.genCfg.maxNmbTSaps                          = SB_MAX_T_SAPS;                        /* max no. Transport SAPS */
	cfg.t.cfg.s.genCfg.maxNmbEndp                           = SB_MAX_NUM_OF_ENDPOINTS;              /* max no. endpoints */
	cfg.t.cfg.s.genCfg.maxNmbAssoc                          = SB_MAX_NUM_OF_ASSOC;                  /* max no. associations */
	cfg.t.cfg.s.genCfg.maxNmbDstAddr                        = SB_MAX_NUM_OF_DST_ADDR;               /* max no. dest. addresses */
	cfg.t.cfg.s.genCfg.maxNmbSrcAddr                        = SB_MAX_NUM_OF_SRC_ADDR;               /* max no. src. addresses */
	cfg.t.cfg.s.genCfg.maxNmbTxChunks                       = SB_MAX_NUM_OF_TX_CHUNKS;
	cfg.t.cfg.s.genCfg.maxNmbRxChunks                       = SB_MAX_NUM_OF_RX_CHUNKS;
	cfg.t.cfg.s.genCfg.maxNmbInStrms                        = SB_MAX_INC_STREAMS;
	cfg.t.cfg.s.genCfg.maxNmbOutStrms                       = SB_MAX_OUT_STREAMS;
	cfg.t.cfg.s.genCfg.initARwnd                            = SB_MAX_RWND_SIZE;
	cfg.t.cfg.s.genCfg.mtuInitial                           = SB_MTU_INITIAL;
	cfg.t.cfg.s.genCfg.mtuMinInitial                        = SB_MTU_MIN_INITIAL;
	cfg.t.cfg.s.genCfg.mtuMaxInitial                        = SB_MTU_MAX_INITIAL;
	cfg.t.cfg.s.genCfg.performMtu                           = FALSE;
	cfg.t.cfg.s.genCfg.timeRes                              = 1;
	sprintf((char*)cfg.t.cfg.s.genCfg.hostname, "www.sangoma.com"); /* DAVIDY - Fix this later, probably ignored */
	cfg.t.cfg.s.genCfg.useHstName                           = FALSE;      /* Flag whether hostname is to be used in INIT and INITACK msg */
	cfg.t.cfg.s.genCfg.reConfig.maxInitReTx         = 8;
	cfg.t.cfg.s.genCfg.reConfig.maxAssocReTx        = 10;
	cfg.t.cfg.s.genCfg.reConfig.maxPathReTx         = 10;
	cfg.t.cfg.s.genCfg.reConfig.altAcceptFlg        = TRUE;
	cfg.t.cfg.s.genCfg.reConfig.keyTm                       = 600; /* initial value for MD5 Key expiry timer */
	cfg.t.cfg.s.genCfg.reConfig.alpha                       = 12;
	cfg.t.cfg.s.genCfg.reConfig.beta                        = 25;
#ifdef SB_ECN
	cfg.t.cfg.s.genCfg.reConfig.ecnFlg                      = TRUE;
#endif

	return(sng_cfg_sctp(&pst, &cfg));
}

/****************************************************************************************************/
static int ftmod_cfg_sctp(void)
{
	int x=0;

	x = 1;
	while(x<MAX_SCTP_LINK){

		if((g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].id !=0) && 
				(!(g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].flags & SNGSS7_CONFIGURED))) {

			if (  ftmod_sctp_config(x) == FTDM_FAIL) {
				SS7_CRITICAL("SCTP %d configuration FAILED!\n", x);
				return FTDM_FAIL;
			} else {
				SS7_INFO("SCTP %d configuration DONE!\n", x);
			}
			g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].flags |= SNGSS7_CONFIGURED;
		}
		x++;
	}
	return FTDM_SUCCESS;
}

/****************************************************************************************************/
int ftmod_sctp_config(int id)
{ 
	if  (FTDM_SUCCESS != ftmod_sctp_tsap_config(id))
		return FTDM_FAIL;

	if  (FTDM_SUCCESS != ftmod_sctp_sap_config(id))
		return FTDM_FAIL;

	/* each sctp link there will be one tucl sap */
	if(ftmod_tucl_sap_config(id)){
		ftdm_log (FTDM_LOG_ERROR ,"TUCL SAP[%d] configuration: NOT OK\n", id);
		return FTDM_FAIL;
	} else {
		ftdm_log (FTDM_LOG_INFO ,"TUCL SAP[%d] configuration: OK\n", id);
	}

	return FTDM_SUCCESS;
}
/****************************************************************************************************/

ftdm_status_t ftmod_sctp_tsap_config(int id)
{
	Pst			pst;
	SbMgmt		cfg;
	SbTSapCfg	*c;	

	int 			i = 0;
	int			ret = -1;
	
	sng_sctp_link_t *k = &g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[id];

	smPstInit(&pst);
	pst.dstEnt = ENTSB;
	
	memset(&cfg, 0x0, sizeof(cfg));
	smHdrInit(&cfg.hdr);

	cfg.hdr.msgType			= TCFG;
	cfg.hdr.entId.ent		= ENTSB;
	cfg.hdr.entId.inst		= S_INST;
	cfg.hdr.elmId.elmnt		= STSBTSAP;
	cfg.hdr.elmId.elmntInst1 	= k->id;

	c = &cfg.t.cfg.s.tSapCfg;
	c->swtch			= LSB_SW_RFC_REL0;
	c->suId				= k->id;
	c->sel				= 0;
	c->ent				= ENTHI;
	c->inst				= S_INST;
	c->procId			= g_ftdm_sngss7_data.cfg.procId;
	c->memId.region			= S_REG;
	c->memId.pool			= S_POOL;
	c->prior			= PRIOR1;
	c->route			= RTESPEC;
	c->srcNAddrLst.nmb 		= k->numSrcAddr;
	for (i=0; i <= (k->numSrcAddr-1); i++) {
		c->srcNAddrLst.nAddr[i].type = CM_NETADDR_IPV4;		
		c->srcNAddrLst.nAddr[i].u.ipv4NetAddr = k->srcAddrList[i+1];
	}

	c->reConfig.spId		= k->id;
	c->reConfig.maxBndRetry 	= 3; 
	c->reConfig.tIntTmr 		= 200;
	
	ret = sng_cfg_sctp(&pst, &cfg);
	if (0 == ret) {
		SS7_INFO("SCTP TSAP [%d] configuration DONE!\n", id);
		return FTDM_SUCCESS;
	} else {
		SS7_CRITICAL("SCTP TSAP [%d] configuration FAILED!\n", id);
		return FTDM_FAIL;
	}
}

/****************************************************************************************************/

ftdm_status_t ftmod_sctp_sap_config(int id)
{
	Pst			pst;
	SbMgmt		cfg;
	SbSctSapCfg	*c;	
	
	int		ret = -1;
	sng_sctp_link_t *k = &g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[id];

	smPstInit(&pst);
	pst.dstEnt = ENTSB;
	
	memset(&cfg, 0x0, sizeof(cfg));
	smHdrInit(&cfg.hdr);

	cfg.hdr.msgType			= TCFG;
	cfg.hdr.entId.ent		= ENTSB;
	cfg.hdr.entId.inst		= S_INST;
	cfg.hdr.elmId.elmnt		= STSBSCTSAP;
	cfg.hdr.elmId.elmntInst1 	= k->id;

	c = &cfg.t.cfg.s.sctSapCfg;
	c->swtch 			= LSB_SW_RFC_REL0;
	c->spId				= k->id;	/* Service Provider SAP Id */
	c->sel				= 0;
	c->memId.region			= S_REG;
	c->memId.pool			= S_POOL;
	c->prior			= PRIOR1;
	c->route			= RTESPEC;

	/* Maximum time to wait before the SCTP layer must send a Selective Acknowledgement (SACK) message. Valid range is 1 -165535. */
	c->reConfig.maxAckDelayTm 	= 200; 
	/* Maximum number of messages to receive before the SCTP layer must send a SACK message. Valid range is 1 - 165535. */
	c->reConfig.maxAckDelayDg 	= 2;
	/* Initial value of the retransmission timer (RTO). The SCTP layer retransmits data after waiting for feedback during this time period. Valid range is 1 - 65535. */
	c->reConfig.rtoInitial 		= 3000;
	/* Minimum value used for the RTO. If the computed value of RTO is less than rtoMin, the computed value is rounded up to this value. */
	c->reConfig.rtoMin 		= 1000;
	/* Maxiumum value used for RTO. If the computed value of RTO is greater than rtoMax, the computed value is rounded down to this value. */
	c->reConfig.rtoMax 		= 10000;
	/* Default Freeze timer value */
	c->reConfig.freezeTm 		= 3000;
	/* Base cookie lifetime for the cookie in the Initiation Acknowledgement (INIT ACK) message. */
	c->reConfig.cookieLife 		= 60000;
	/* Default heartbeat interval timer. Valid range is 1 - 65535. */
	c->reConfig.intervalTm 		= 3000;
	/* Maximum burst value. Valid range is 1 - 65535. */
	c->reConfig.maxBurst 		= 4;
	/*Maximum number of heartbeats sent at each retransmission timeout (RTO). Valid range is 1 - 65535. */
	c->reConfig.maxHbBurst 		= 1;
	/*Shutdown guard timer value for graceful shutdowns. */
	c->reConfig.t5SdownGrdTm 	= 15000;
	/*	Action to take when the receiver's number of incoming streams is less than the sender's number of outgoing streams. Valid values are:
		TRUE = Accept incoming stream and continue association.
		FALSE = Abort the association.
	*/
	c->reConfig.negAbrtFlg 		= FALSE;
	/* 	Whether to enable or disable heartbeat by default. Valid values are:
		TRUE = Enable heartbeat.
		FALSE = Disable heartbeat.
	*/
	c->reConfig.hBeatEnable 	= TRUE;
	/* Flow control start threshold. When the number of messages in SCTPs message queue reaches this value, flow control starts. */
	c->reConfig.flcUpThr 		= 8;
	/* Flow control stop threshold. When the number of messages in SCTPs message queue reaches this value, flow control stops. */
	c->reConfig.flcLowThr 		= 6;

	c->reConfig.handleInitFlg 	= FALSE;

	ret = sng_cfg_sctp(&pst, &cfg);
	if (0 == ret) {
		SS7_INFO("SCTP SAP [%d] configuration DONE!\n", id);
		return FTDM_SUCCESS;
	} else {
		SS7_CRITICAL("SCTP SAP [%d] configuration FAILED!\n", id);
		return FTDM_FAIL;
	}
}

/**********************************************************************************************/
/* M2UA - General configuration */
static int ftmod_m2ua_gen_config(void)
{
	Pst    pst; 
	MwMgmt cfg;

	memset((U8 *)&cfg, 0, sizeof(MwMgmt));
	memset((U8 *)&pst, 0, sizeof(Pst));

	smPstInit(&pst);

	pst.dstEnt = ENTMW;

	/* prepare header */
	cfg.hdr.msgType     = TCFG;           /* message type */
	cfg.hdr.entId.ent   = ENTMW;          /* entity */
	cfg.hdr.entId.inst  = 0;              /* instance */
	cfg.hdr.elmId.elmnt = STMWGEN;        /* General */
	cfg.hdr.transId     = 0;     /* transaction identifier */

	cfg.hdr.response.selector    = 0;
	cfg.hdr.response.prior       = PRIOR0;
	cfg.hdr.response.route       = RTESPEC;
	cfg.hdr.response.mem.region  = S_REG;
	cfg.hdr.response.mem.pool    = S_POOL;



	cfg.t.cfg.s.genCfg.nodeType          = LMW_TYPE_SGP; /* NodeType ==  SGP or ASP  */
	cfg.t.cfg.s.genCfg.maxNmbIntf        = MW_MAX_NUM_OF_INTF;
	cfg.t.cfg.s.genCfg.maxNmbCluster     = MW_MAX_NUM_OF_CLUSTER;
	cfg.t.cfg.s.genCfg.maxNmbPeer        = MW_MAX_NUM_OF_PEER;
	cfg.t.cfg.s.genCfg.maxNmbSctSap      = MW_MAX_NUM_OF_SCT_SAPS;
	cfg.t.cfg.s.genCfg.timeRes           = 1;              /* timer resolution */
	cfg.t.cfg.s.genCfg.maxClusterQSize   = MW_MAX_CLUSTER_Q_SIZE;
	cfg.t.cfg.s.genCfg.maxIntfQSize      = MW_MAX_INTF_Q_SIZE;

#ifdef LCMWMILMW  
	cfg.t.cfg.s.genCfg.reConfig.smPst.selector  = 0;     /* selector */
#else /* LCSBMILSB */
	cfg.t.cfg.s.genCfg.reConfig.smPst.selector  = 1;     /* selector */
#endif /* LCSBMILSB */

	cfg.t.cfg.s.genCfg.reConfig.smPst.region    = S_REG;   /* region */
	cfg.t.cfg.s.genCfg.reConfig.smPst.pool      = S_POOL;     /* pool */
	cfg.t.cfg.s.genCfg.reConfig.smPst.prior     = PRIOR0;        /* priority */
	cfg.t.cfg.s.genCfg.reConfig.smPst.route     = RTESPEC;       /* route */

	cfg.t.cfg.s.genCfg.reConfig.smPst.dstEnt    = ENTSM;         /* dst entity */
	cfg.t.cfg.s.genCfg.reConfig.smPst.dstInst   = S_INST;             /* dst inst */
	cfg.t.cfg.s.genCfg.reConfig.smPst.dstProcId = SFndProcId();  /* src proc id */

	cfg.t.cfg.s.genCfg.reConfig.smPst.srcEnt    = ENTMW;         /* src entity */
	cfg.t.cfg.s.genCfg.reConfig.smPst.srcInst   = S_INST;             /* src inst */
	cfg.t.cfg.s.genCfg.reConfig.smPst.srcProcId = SFndProcId();  /* src proc id */

	cfg.t.cfg.s.genCfg.reConfig.tmrFlcPoll.enb = TRUE;            /* SCTP Flc Poll timer */
	cfg.t.cfg.s.genCfg.reConfig.tmrFlcPoll.val = 10;

#ifdef MWASP
	cfg.t.cfg.s.genCfg.reConfig.tmrAspm.enb    = TRUE;         /* ASPM  timer */
	cfg.t.cfg.s.genCfg.reConfig.tmrAspm.val    = 10;
	cfg.t.cfg.s.genCfg.reConfig.tmrHeartBeat.enb  = TRUE;       /* Heartbeat timer */
	cfg.t.cfg.s.genCfg.reConfig.tmrHeartBeat.val  = 10;
#endif

#ifdef MWSG
	cfg.t.cfg.s.genCfg.reConfig.tmrAsPend.enb  = TRUE;   /* AS-PENDING timer */
	cfg.t.cfg.s.genCfg.reConfig.tmrAsPend.val  = 10;
	cfg.t.cfg.s.genCfg.reConfig.tmrCongPoll.enb = TRUE;  /* SS7 Congestion poll timer */
	cfg.t.cfg.s.genCfg.reConfig.tmrCongPoll.val = 10;
	cfg.t.cfg.s.genCfg.reConfig.tmrHeartBeat.enb  = FALSE;       /* HBtimer only at ASP */
#endif
	cfg.t.cfg.s.genCfg.reConfig.aspmRetry = 5;

	return (sng_cfg_m2ua (&pst, &cfg));
}   

/**********************************************************************************************/
static int ftmod_m2ua_peer_config(int id)
{
	int x = 0;
	int peer_id = 0;
	sng_m2ua_cfg_t* 	    m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[id];
	sng_m2ua_cluster_cfg_t*     clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[m2ua->clusterId];
	sng_m2ua_peer_cfg_t* 	    peer  = NULL;

	if((clust->flags & SNGSS7_CONFIGURED)){
		ftdm_log (FTDM_LOG_INFO, " ftmod_m2ua_peer_config: Cluster [%s] is already configured \n", clust->name);
		return 0x00;
	}

	/*NOTE : SCTSAP is based on per source address , so if we have same Cluster / peer shared across many <m2ua_interface> then 
	 * we dont have do configuration for each time */

	/* loop through peer list from cluster to configure SCTSAP */

	for(x = 0; x < clust->numOfPeers;x++){
		peer_id = clust->peerIdLst[x];
		peer = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[peer_id];
		if(ftmod_m2ua_sctsap_config(id, peer->sctpId)){
			ftdm_log (FTDM_LOG_ERROR, " ftmod_m2ua_sctsap_config: M2UA SCTSAP for M2UA Intf Id[%d] config FAILED \n", id);
			return 0x01;
		}else{
			ftdm_log (FTDM_LOG_INFO, " ftmod_m2ua_sctsap_config: M2UA SCTSAP for M2UA Intf Id[%d] config SUCCESS \n", id);
		}
		if(ftmod_m2ua_peer_config1(id, peer_id)){
			ftdm_log (FTDM_LOG_ERROR, " ftmod_m2ua_peer_config1: M2UA Peer[%d] configuration for M2UA Intf Id[%d] config FAILED \n", peer_id, id);
			return 0x01;
		}else{
			ftdm_log (FTDM_LOG_INFO, " ftmod_m2ua_peer_config1: M2UA Peer[%d] configuration for M2UA Intf Id[%d] config SUCCESS \n", peer_id, id);
		}

		clust->sct_sap_id = id;

		/* set configured flag for cluster and peer */
		clust->flags |= SNGSS7_CONFIGURED;
		peer->flags |= SNGSS7_CONFIGURED;
	}

	return 0x0;;
}


static int ftmod_m2ua_sctsap_config(int sct_sap_id, int sctp_id)
{
   int    i;
   int    ret;
   Pst    pst; 
   MwMgmt cfg;
   MwMgmt cfm;
   sng_sctp_link_t *sctp = &g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[sctp_id];



   memset((U8 *)&cfg, 0, sizeof(MwMgmt));
   memset((U8 *)&cfm, 0, sizeof(MwMgmt));
   memset((U8 *)&pst, 0, sizeof(Pst));

   /* check is sct_sap is already configured */
   if(!ftmod_m2ua_ssta_req(STMWSCTSAP, sct_sap_id, &cfm )){
	   ftdm_log (FTDM_LOG_INFO, " ftmod_m2ua_sctsap_config: SCT SAP [%s] is already configured \n", sctp->name);
	   return 0x00;
   }

   if(LCM_REASON_INVALID_SAP == cfm.cfm.reason){
	   ftdm_log (FTDM_LOG_INFO, " ftmod_m2ua_sctsap_config: SCT SAP [%s] is not configured..configuring now \n", sctp->name);
   }

   smPstInit(&pst);

   pst.dstEnt = ENTMW;

   /* prepare header */
   cfg.hdr.msgType     = TCFG;           /* message type */
   cfg.hdr.entId.ent   = ENTMW;          /* entity */
   cfg.hdr.entId.inst  = 0;              /* instance */
   cfg.hdr.elmId.elmnt = STMWSCTSAP;     /* SCTSAP */
   cfg.hdr.transId     = 0;     /* transaction identifier */

    cfg.hdr.response.selector    = 0;
   cfg.hdr.response.prior       = PRIOR0;
   cfg.hdr.response.route       = RTESPEC;
   cfg.hdr.response.mem.region  = S_REG;
   cfg.hdr.response.mem.pool    = S_POOL;

   cfg.t.cfg.s.sctSapCfg.reConfig.selector     = 0;

   /* service user SAP ID */
   cfg.t.cfg.s.sctSapCfg.suId                   = sct_sap_id;
   /* service provider ID   */
   cfg.t.cfg.s.sctSapCfg.spId                   = sctp_id;
   /* source port number */
   cfg.t.cfg.s.sctSapCfg.srcPort                = sctp->port;
   /* interface address */
   /*For multiple IP address support */
#ifdef SCT_ENDP_MULTI_IPADDR
   cfg.t.cfg.s.sctSapCfg.srcAddrLst.nmb  	= sctp->numSrcAddr;
   for (i=0; i <= (sctp->numSrcAddr-1); i++) {
	   cfg.t.cfg.s.sctSapCfg.srcAddrLst.nAddr[i].type = CM_NETADDR_IPV4;
	   cfg.t.cfg.s.sctSapCfg.srcAddrLst.nAddr[i].u.ipv4NetAddr = sctp->srcAddrList[i+1];
   }
#else
   /* for single ip support ,src address will always be one */
   cfg.t.cfg.s.sctSapCfg.intfAddr.type          = CM_NETADDR_IPV4;
   cfg.t.cfg.s.sctSapCfg.intfAddr.u.ipv4NetAddr = sctp->srcAddrList[1];
#endif

   /* lower SAP primitive timer */
   cfg.t.cfg.s.sctSapCfg.reConfig.tmrPrim.enb   = TRUE;
   cfg.t.cfg.s.sctSapCfg.reConfig.tmrPrim.val   = 10;
   /* Association primitive timer */
   cfg.t.cfg.s.sctSapCfg.reConfig.tmrAssoc.enb   = TRUE;
   cfg.t.cfg.s.sctSapCfg.reConfig.tmrAssoc.val   = 10;
   /* maxnumber of retries */
   cfg.t.cfg.s.sctSapCfg.reConfig.nmbMaxPrimRetry  = 5;
   /* Life Time of Packets  */
   cfg.t.cfg.s.sctSapCfg.reConfig.lifeTime  = 200;
   /* priority */
   cfg.t.cfg.s.sctSapCfg.reConfig.prior       =  PRIOR0;
   /* route */
   cfg.t.cfg.s.sctSapCfg.reConfig.route       =  RTESPEC;
   cfg.t.cfg.s.sctSapCfg.reConfig.ent         =  ENTSB;
   cfg.t.cfg.s.sctSapCfg.reConfig.inst        =  0;
   cfg.t.cfg.s.sctSapCfg.reConfig.procId      =  SFndProcId();
   /* memory region and pool ID */
   cfg.t.cfg.s.sctSapCfg.reConfig.mem.region    = S_REG;
   cfg.t.cfg.s.sctSapCfg.reConfig.mem.pool      = S_POOL;

     if (0 == (ret = sng_cfg_m2ua (&pst, &cfg))){
		sctp->flags |= SNGSS7_CONFIGURED;
     }

     return ret;
}

/****************************************************************************************************/

/* M2UA - Peer configuration */
static int ftmod_m2ua_peer_config1(int m2ua_inf_id, int peer_id)
{
   int    i;
   Pst    pst;
   MwMgmt cfg;
   sng_m2ua_peer_cfg_t* peer  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[peer_id];
   sng_sctp_link_t 	*sctp = &g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[peer->sctpId];

   memset((U8 *)&cfg, 0, sizeof(MwMgmt));
   memset((U8 *)&pst, 0, sizeof(Pst));

   smPstInit(&pst);

   pst.dstEnt = ENTMW;

   /* prepare header */
   cfg.hdr.msgType     = TCFG;           /* message type */
   cfg.hdr.entId.ent   = ENTMW;          /* entity */
   cfg.hdr.entId.inst  = 0;              /* instance */
   cfg.hdr.elmId.elmnt = STMWPEER;       /* Peer */
   cfg.hdr.transId     = 0;     /* transaction identifier */

   cfg.hdr.response.selector    = 0;
   cfg.hdr.response.prior       = PRIOR0;
   cfg.hdr.response.route       = RTESPEC;
   cfg.hdr.response.mem.region  = S_REG;
   cfg.hdr.response.mem.pool    = S_POOL;



   cfg.t.cfg.s.peerCfg.peerId 		= peer->id;               /* peer id */
   cfg.t.cfg.s.peerCfg.aspIdFlag 	= peer->aspIdFlag;        /* aspId flag */
#ifdef MWASP
   cfg.t.cfg.s.peerCfg.selfAspId 	= peer->selfAspId;  	  /* aspId */
#endif
   cfg.t.cfg.s.peerCfg.assocCfg.suId    = peer->sctpId; 	  /* SCTSAP ID */
   cfg.t.cfg.s.peerCfg.assocCfg.dstAddrLst.nmb = peer->numDestAddr;
   for (i=0; i <= (peer->numDestAddr); i++) {
	   cfg.t.cfg.s.peerCfg.assocCfg.dstAddrLst.nAddr[i].type = CM_NETADDR_IPV4;
	   cfg.t.cfg.s.peerCfg.assocCfg.dstAddrLst.nAddr[i].u.ipv4NetAddr = peer->destAddrList[i]; 
   }
#ifdef MW_CFG_DSTPORT
   cfg.t.cfg.s.peerCfg.assocCfg.dstPort = peer->port; /* Port on which M2UA runs */
#endif
   cfg.t.cfg.s.peerCfg.assocCfg.srcAddrLst.nmb = sctp->numSrcAddr; /* source address list */
   for (i=0; i <= (sctp->numSrcAddr-1); i++) {
	   cfg.t.cfg.s.peerCfg.assocCfg.srcAddrLst.nAddr[i].type =  CM_NETADDR_IPV4;
	   cfg.t.cfg.s.peerCfg.assocCfg.srcAddrLst.nAddr[i].u.ipv4NetAddr = sctp->srcAddrList[i+1]; 
   }

   cfg.t.cfg.s.peerCfg.assocCfg.priDstAddr.type = CM_NETADDR_IPV4;
   cfg.t.cfg.s.peerCfg.assocCfg.priDstAddr.u.ipv4NetAddr = cfg.t.cfg.s.peerCfg.assocCfg.dstAddrLst.nAddr[0].u.ipv4NetAddr;

   cfg.t.cfg.s.peerCfg.assocCfg.locOutStrms = peer->locOutStrms;
#ifdef SCT3
   cfg.t.cfg.s.peerCfg.assocCfg.tos = 0;
#endif

     return (sng_cfg_m2ua (&pst, &cfg));
}
/**********************************************************************************************/
/* M2UA - Cluster configuration */
static int ftmod_m2ua_cluster_config(int id)
{
   int i;
   Pst    pst; 
   MwMgmt cfg;
   sng_m2ua_cfg_t* 	    m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[id];
   sng_m2ua_cluster_cfg_t*  clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[m2ua->clusterId];

   memset((U8 *)&cfg, 0, sizeof(MwMgmt));
   memset((U8 *)&pst, 0, sizeof(Pst));

   smPstInit(&pst);

   pst.dstEnt = ENTMW;

   /* prepare header */
   cfg.hdr.msgType     = TCFG;           /* message type */
   cfg.hdr.entId.ent   = ENTMW;          /* entity */
   cfg.hdr.entId.inst  = 0;              /* instance */
   cfg.hdr.elmId.elmnt = STMWCLUSTER;    /* Cluster */
   cfg.hdr.transId     = 0;     /* transaction identifier */

   cfg.hdr.response.selector    = 0;
   cfg.hdr.response.prior       = PRIOR0;
   cfg.hdr.response.route       = RTESPEC;
   cfg.hdr.response.mem.region  = S_REG;
   cfg.hdr.response.mem.pool    = S_POOL;


   cfg.t.cfg.s.clusterCfg.clusterId 	= clust->id;
   cfg.t.cfg.s.clusterCfg.trfMode   	= clust->trfMode;
   cfg.t.cfg.s.clusterCfg.loadshareMode = clust->loadShareAlgo;
   cfg.t.cfg.s.clusterCfg.reConfig.nmbPeer = clust->numOfPeers;
   for(i=0; i<(clust->numOfPeers);i++) {
	   cfg.t.cfg.s.clusterCfg.reConfig.peer[i] = clust->peerIdLst[i];
   }

     return (sng_cfg_m2ua (&pst, &cfg));
}

/**********************************************************************************************/

/* M2UA - DLSAP configuration */
static int ftmod_m2ua_dlsap_config(int id)
{
   Pst    pst; 
   MwMgmt cfg;
   sng_m2ua_cfg_t* 	    m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[id];

   memset((U8 *)&cfg, 0, sizeof(MwMgmt));
   memset((U8 *)&pst, 0, sizeof(Pst));

   smPstInit(&pst);

   pst.dstEnt = ENTMW;

   /* prepare header */
   cfg.hdr.msgType     = TCFG;           /* message type */
   cfg.hdr.entId.ent   = ENTMW;          /* entity */
   cfg.hdr.entId.inst  = 0;              /* instance */
   cfg.hdr.elmId.elmnt = STMWDLSAP;      /* DLSAP */
   cfg.hdr.transId     = 0;     /* transaction identifier */

   cfg.hdr.response.selector    = 0;
   cfg.hdr.response.prior       = PRIOR0;
   cfg.hdr.response.route       = RTESPEC;
   cfg.hdr.response.mem.region  = S_REG;
   cfg.hdr.response.mem.pool    = S_POOL;


   cfg.t.cfg.s.dlSapCfg.lnkNmb 	= id; /* SapId */
   cfg.t.cfg.s.dlSapCfg.intfId.type = LMW_INTFID_INT;
   cfg.t.cfg.s.dlSapCfg.intfId.id.intId = m2ua->iid;
   
   cfg.t.cfg.s.dlSapCfg.swtch = LMW_SAP_ITU;

   cfg.t.cfg.s.dlSapCfg.reConfig.clusterId =  m2ua->clusterId;
   cfg.t.cfg.s.dlSapCfg.reConfig.selector  =  0; /* Loosely couple mode */
   /* memory region and pool id*/
   cfg.t.cfg.s.dlSapCfg.reConfig.mem.region  =  S_REG;
   cfg.t.cfg.s.dlSapCfg.reConfig.mem.pool    =  S_POOL;
   /* priority */
   cfg.t.cfg.s.dlSapCfg.reConfig.prior       =  PRIOR0;
   /* route */
   cfg.t.cfg.s.dlSapCfg.reConfig.route       =  RTESPEC;

     return (sng_cfg_m2ua (&pst, &cfg));

}
/*****************************************************************************/
/* NIF - General configuration */
static int ftmod_nif_gen_config(void)
{
   Pst    pst; 
   NwMgmt cfg;

   memset((U8 *)&cfg, 0, sizeof(NwMgmt));
   memset((U8 *)&pst, 0, sizeof(Pst));

   smPstInit(&pst);

   pst.dstEnt = ENTNW;

   /* prepare header */
   cfg.hdr.msgType     = TCFG;           /* message type */
   cfg.hdr.entId.ent   = ENTNW;          /* entity */
   cfg.hdr.entId.inst  = 0;              /* instance */
   cfg.hdr.elmId.elmnt = STNWGEN;      /* DLSAP */
   cfg.hdr.transId     = 0;     /* transaction identifier */

   cfg.hdr.response.selector    = 0;
   cfg.hdr.response.prior       = PRIOR0;
   cfg.hdr.response.route       = RTESPEC;
   cfg.hdr.response.mem.region  = S_REG;
   cfg.hdr.response.mem.pool    = S_POOL;

   cfg.t.cfg.s.genCfg.maxNmbDlSap       = NW_MAX_NUM_OF_DLSAPS;
   cfg.t.cfg.s.genCfg.timeRes           = 1;    /* timer resolution */

   cfg.t.cfg.s.genCfg.reConfig.maxNmbRetry    = NW_MAX_NUM_OF_RETRY;
   cfg.t.cfg.s.genCfg.reConfig.tmrRetry.enb =   TRUE;     /* SS7 Congestion poll timer */
   cfg.t.cfg.s.genCfg.reConfig.tmrRetry.val =   NW_RETRY_TMR_VALUE;

#ifdef LCNWMILNW  
   cfg.t.cfg.s.genCfg.reConfig.smPst.selector  = 0;     /* selector */
#else /* LCSBMILSB */
   cfg.t.cfg.s.genCfg.reConfig.smPst.selector  = 1;     /* selector */
#endif /* LCSBMILSB */

   cfg.t.cfg.s.genCfg.reConfig.smPst.region    = S_REG;   /* region */
   cfg.t.cfg.s.genCfg.reConfig.smPst.pool      = S_POOL;     /* pool */
   cfg.t.cfg.s.genCfg.reConfig.smPst.prior     = PRIOR0;        /* priority */
   cfg.t.cfg.s.genCfg.reConfig.smPst.route     = RTESPEC;       /* route */

   cfg.t.cfg.s.genCfg.reConfig.smPst.dstEnt    = ENTSM;         /* dst entity */
   cfg.t.cfg.s.genCfg.reConfig.smPst.dstInst   = 0;             /* dst inst */
   cfg.t.cfg.s.genCfg.reConfig.smPst.dstProcId = SFndProcId();  /* src proc id */

   cfg.t.cfg.s.genCfg.reConfig.smPst.srcEnt    = ENTNW;         /* src entity */
   cfg.t.cfg.s.genCfg.reConfig.smPst.srcInst   = 0;             /* src inst */
   cfg.t.cfg.s.genCfg.reConfig.smPst.srcProcId = SFndProcId();  /* src proc id */

     return (sng_cfg_nif (&pst, &cfg));

}

/*****************************************************************************/

/* NIF - DLSAP configuration */
static int ftmod_nif_dlsap_config(int id)
{
   Pst    pst; 
   NwMgmt cfg;
   sng_nif_cfg_t* nif = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[id];

   memset((U8 *)&cfg, 0, sizeof(NwMgmt));
   memset((U8 *)&pst, 0, sizeof(Pst));

   smPstInit(&pst);

   pst.dstEnt = ENTNW;

   /* prepare header */
   cfg.hdr.msgType     = TCFG;           /* message type */
   cfg.hdr.entId.ent   = ENTNW;          /* entity */
   cfg.hdr.entId.inst  = 0;              /* instance */
   cfg.hdr.elmId.elmnt = STNWDLSAP;      /* DLSAP */
   cfg.hdr.transId     = 0;     /* transaction identifier */

   cfg.hdr.response.selector    = 0;
   cfg.hdr.response.prior       = PRIOR0;
   cfg.hdr.response.route       = RTESPEC;
   cfg.hdr.response.mem.region  = S_REG;
   cfg.hdr.response.mem.pool    = S_POOL;
   cfg.t.cfg.s.dlSapCfg.suId 	    = nif->id;           
   cfg.t.cfg.s.dlSapCfg.m2uaLnkNmb  = nif->m2uaLnkNmb; 
   cfg.t.cfg.s.dlSapCfg.mtp2LnkNmb  = nif->mtp2LnkNmb;
        
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.selector   = 0;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.region     = S_REG;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.pool       = S_POOL;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.route      = RTESPEC;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.prior      = PRIOR0;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.srcEnt     = ENTNW;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.srcInst    = 0;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.srcProcId  = SFndProcId();
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.dstEnt     = ENTMW;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.dstInst    = 0;
   cfg.t.cfg.s.dlSapCfg.reConfig.m2uaPst.dstProcId  = SFndProcId();

   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.selector   = 0;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.region     = S_REG;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.pool       = S_POOL;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.route      = RTESPEC;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.prior      = PRIOR0;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.srcEnt     = ENTNW;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.srcInst    = 0;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.srcProcId  = SFndProcId();
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.dstEnt     = ENTSD;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.dstInst    = 0;
   cfg.t.cfg.s.dlSapCfg.reConfig.mtp2Pst.dstProcId  = SFndProcId();

     return (sng_cfg_nif (&pst, &cfg));
}   

/*****************************************************************************/
uint32_t iptoul(const char *ip)
{
        char i,*tmp;
        int strl;
        char strIp[16];
        unsigned long val=0, cvt;
        if (!ip)
                return 0;

        memset(strIp, 0, sizeof(char)*16);
        strl = strlen(ip);
        strncpy(strIp, ip, strl>=15?15:strl);


        tmp=strtok(strIp, ".");
        for (i=0;i<4;i++)
        {
                sscanf(tmp, "%lu", &cvt);
                val <<= 8;
                val |= (unsigned char)cvt;
                tmp=strtok(NULL,".");
        }
        return (uint32_t)val;
}
/***********************************************************************************************************************/
void ftmod_ss7_enable_m2ua_sg_logging(void){

	/* Enable DEBUGs*/
	ftmod_sctp_debug(AENA);
	ftmod_m2ua_debug(AENA);
	ftmod_tucl_debug(AENA);
}

/***********************************************************************************************************************/
void ftmod_ss7_disable_m2ua_sg_logging(void){

	/* DISABLE DEBUGs*/
	ftmod_sctp_debug(ADISIMM);
	ftmod_m2ua_debug(ADISIMM);
	ftmod_tucl_debug(ADISIMM);
}

/***********************************************************************************************************************/
int ftmod_ss7_m2ua_start(void){
	int x=0;

/***********************************************************************************************************************/
	x = 1;
	while(x<MAX_SCTP_LINK){
		if((g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].id !=0) && 
				(!(g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].flags & SNGSS7_ACTIVE))) {

			/* Send a control request to bind the TSAP between SCTP and TUCL */
			if(ftmod_sctp_tucl_tsap_bind(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"\nControl request to bind TSAP[%d] of SCTP and TUCL : NOT OK\n", x);
				return 1;
			} else {
				ftdm_log (FTDM_LOG_INFO ,"\nControl request to bind TSAP[%d] of SCTP and TUCL: OK\n", x);
			}
			g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[x].flags |= SNGSS7_ACTIVE;
		}
		x++;
	}

/***********************************************************************************************************************/
	/* Send a control request to bind the SCTSAP between SCTP and M2UA */
	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].id !=0) && 
				(!(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags & SNGSS7_ACTIVE))) {
			if(ftmod_m2ua_sctp_sctsap_bind(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"Control request to bind SCTSAP[%d] of M2UA and SCTP : NOT OK\n", x);
				return 1;
			} else {
				ftdm_log (FTDM_LOG_INFO ,"Control request to bind SCTSAP[%d] of M2UA and SCTP: OK\n", x);
			}
			g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].flags |= SNGSS7_ACTIVE;
		}
		x++;
	}/* END - M2UA Interfaces while loop*/
/***********************************************************************************************************************/

	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if ((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].id !=0) && 
				(!(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].flags & SNGSS7_ACTIVE))) {
			/* Send a control request to bind the DLSAP between NIF, M2UA and MTP-2 */
			if(ftmod_nif_m2ua_dlsap_bind(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"Control request to bind DLSAP[%d] between NIF and M2UA: NOT OK\n", x);
				return 1;
			}else {
				ftdm_log (FTDM_LOG_INFO ,"Control request to bind DLSAP[%d] between NIF and M2UA : OK\n", x);
			}
			if(ftmod_nif_mtp2_dlsap_bind(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"Control request to bind DLSAP[%d] between NIF and MTP2: NOT OK\n", x);
				return 1;
			}else {
				ftdm_log (FTDM_LOG_INFO ,"Control request to bind DLSAP[%d] between NIF and MTP2 : OK\n", x);
			}
			g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[x].flags |= SNGSS7_ACTIVE;
		}
		x++;
	}/* END - NIF Interfaces for loop*/

/***********************************************************************************************************************/

	x = 1;
	while(x<MW_MAX_NUM_OF_INTF){
		if ((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].id !=0) && 
				(!(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].end_point_opened))) {
			/* Send a control request to open endpoint */
			if(ftmod_open_endpoint(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"ftmod_open_endpoint FAIL  \n");
				return 1;
			}else {
				ftdm_log (FTDM_LOG_INFO ,"ftmod_open_endpoint SUCCESS  \n");
			}
			g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[x].end_point_opened = 0x01;
		}
		x++;
	}

/***********************************************************************************************************************/
	sleep(2);

	x = 1;
	while (x < (MW_MAX_NUM_OF_PEER)) {
		if ((g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[x].id !=0) &&
				(!(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[x].flags & SNGSS7_M2UA_INIT_ASSOC_DONE)) && 
				(g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[x].init_sctp_assoc)) {
			if(ftmod_init_sctp_assoc(x)) {
				ftdm_log (FTDM_LOG_ERROR ,"ftmod_init_sctp_assoc FAIL for peerId[%d] \n", x);
				return 1;
			}else {
				ftdm_log (FTDM_LOG_INFO ,"ftmod_init_sctp_assoc SUCCESS for peerId[%d] \n", x);
			}
			g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_peer[x].flags |= SNGSS7_M2UA_INIT_ASSOC_DONE;
		}
		x++;
	}


	

	return 0;
}
/***********************************************************************************************************************/

static int ftmod_open_endpoint(int id)
{
	int ret = 0x00;
	Pst pst;
	MwMgmt cntrl;  
	sng_m2ua_cfg_t* m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[id];
	sng_m2ua_cluster_cfg_t*     clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[m2ua->clusterId];

	if(clust->flags & SNGSS7_M2UA_EP_OPENED) {
		ftdm_log (FTDM_LOG_INFO ," END-POINT already opened\n");
		return ret;
	}

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(MwMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTMW;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTMW;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STMWSCTSAP;       /* General */
	cntrl.hdr.transId     = 1;     /* transaction identifier */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;


	cntrl.t.cntrl.action = AMWENDPOPEN;
	cntrl.t.cntrl.s.suId = m2ua->id; /* M2UA sct sap Id */

	
	if(0 == (ret = sng_cntrl_m2ua (&pst, &cntrl))){
		clust->flags |= SNGSS7_M2UA_EP_OPENED;
	}
	return ret;

}

/***********************************************************************************************************************/
static int ftmod_init_sctp_assoc(int peer_id)
{

        Pst pst;
        MwMgmt cntrl;

        memset((U8 *)&pst, 0, sizeof(Pst));
        memset((U8 *)&cntrl, 0, sizeof(MwMgmt));

        smPstInit(&pst);

        pst.dstEnt = ENTMW;

        /* prepare header */
        cntrl.hdr.msgType     = TCNTRL;         /* message type */
        cntrl.hdr.entId.ent   = ENTMW;          /* entity */
        cntrl.hdr.entId.inst  = 0;              /* instance */
        cntrl.hdr.elmId.elmnt = STMWPEER;       /* General */
        cntrl.hdr.transId     = 1;     /* transaction identifier */

        cntrl.hdr.response.selector    = 0;
        cntrl.hdr.response.prior       = PRIOR0;
        cntrl.hdr.response.route       = RTESPEC;
        cntrl.hdr.response.mem.region  = S_REG;
        cntrl.hdr.response.mem.pool    = S_POOL;


        cntrl.t.cntrl.action = AMWESTABLISH;
        /*cntrl.t.cntrl.s.suId = 1;*/

        cntrl.t.cntrl.s.peerId = (MwPeerId) peer_id;

        return (sng_cntrl_m2ua (&pst, &cntrl));
}

/***********************************************************************************************************************/
static int ftmod_sctp_tucl_tsap_bind(int id)
{
  Pst pst;
  SbMgmt cntrl;  
  sng_sctp_link_t *k = &g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[id];

  memset((U8 *)&pst, 0, sizeof(Pst));
  memset((U8 *)&cntrl, 0, sizeof(SbMgmt));

  smPstInit(&pst);

  pst.dstEnt = ENTSB;

  /* prepare header */
   cntrl.hdr.msgType     = TCNTRL;         /* message type */
   cntrl.hdr.entId.ent   = ENTSB;          /* entity */
   cntrl.hdr.entId.inst  = 0;              /* instance */
   cntrl.hdr.elmId.elmnt = STSBTSAP;       /* General */
   cntrl.hdr.transId     = 1;     /* transaction identifier */

   cntrl.hdr.response.selector    = 0;

   cntrl.hdr.response.prior       = PRIOR0;
   cntrl.hdr.response.route       = RTESPEC;
   cntrl.hdr.response.mem.region  = S_REG;
   cntrl.hdr.response.mem.pool    = S_POOL;

   cntrl.t.cntrl.action = ABND_ENA;
   cntrl.t.cntrl.sapId  = k->id;  /* SCT sap id configured at SCTP layer */

     return (sng_cntrl_sctp (&pst, &cntrl));
}  
/***********************************************************************************************************************/

static int ftmod_m2ua_sctp_sctsap_bind(int id)
{
  int ret = 0x00;
  Pst pst;
  MwMgmt cntrl;  
  sng_m2ua_cfg_t* m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[id];
  sng_m2ua_cluster_cfg_t*     clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[m2ua->clusterId];

  if(clust->flags & SNGSS7_ACTIVE) {
	  ftdm_log (FTDM_LOG_INFO ," SCT-SAP is already enabled\n");
	  return ret;
  }


  memset((U8 *)&pst, 0, sizeof(Pst));
  memset((U8 *)&cntrl, 0, sizeof(MwMgmt));

  smPstInit(&pst);

  pst.dstEnt = ENTMW;

  /* prepare header */
  cntrl.hdr.msgType     = TCNTRL;         /* message type */
   cntrl.hdr.entId.ent   = ENTMW;          /* entity */
   cntrl.hdr.entId.inst  = 0;              /* instance */
   cntrl.hdr.elmId.elmnt = STMWSCTSAP;       /* General */
   cntrl.hdr.transId     = 1;     /* transaction identifier */

   cntrl.hdr.response.selector    = 0;
   cntrl.hdr.response.prior       = PRIOR0;
   cntrl.hdr.response.route       = RTESPEC;
   cntrl.hdr.response.mem.region  = S_REG;
   cntrl.hdr.response.mem.pool    = S_POOL;

   cntrl.t.cntrl.action = ABND;
   cntrl.t.cntrl.s.suId = m2ua->id;

   if(0 == (ret = sng_cntrl_m2ua (&pst, &cntrl))){
	   clust->flags |= SNGSS7_ACTIVE;
   }
   return ret;
}   
/***********************************************************************************************************************/
static int ftmod_nif_m2ua_dlsap_bind(int id)
{
  Pst pst;
  NwMgmt cntrl;  
  sng_nif_cfg_t* nif = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[id];

  memset((U8 *)&pst, 0, sizeof(Pst));
  memset((U8 *)&cntrl, 0, sizeof(NwMgmt));

  smPstInit(&pst);

  pst.dstEnt = ENTNW;

  /* prepare header */
  cntrl.hdr.msgType     = TCNTRL;         /* message type */
   cntrl.hdr.entId.ent   = ENTNW;          /* entity */
   cntrl.hdr.entId.inst  = 0;              /* instance */
   cntrl.hdr.elmId.elmnt = STNWDLSAP;       /* General */
   cntrl.hdr.transId     = 1;     /* transaction identifier */

   cntrl.hdr.response.selector    = 0;
   cntrl.hdr.response.prior       = PRIOR0;
   cntrl.hdr.response.route       = RTESPEC;
   cntrl.hdr.response.mem.region  = S_REG;
   cntrl.hdr.response.mem.pool    = S_POOL;

   cntrl.t.cntrl.action = ABND;
   cntrl.t.cntrl.suId = nif->id;      /* NIF DL sap Id */
   cntrl.t.cntrl.entity = ENTMW; /* M2UA */

     return (sng_cntrl_nif (&pst, &cntrl));

}   

/***********************************************************************************************************************/
static int ftmod_nif_mtp2_dlsap_bind(int id)
{
  Pst pst;
  NwMgmt cntrl;  
  sng_nif_cfg_t* nif = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[id];

  memset((U8 *)&pst, 0, sizeof(Pst));
  memset((U8 *)&cntrl, 0, sizeof(NwMgmt));

  smPstInit(&pst);

  pst.dstEnt = ENTNW;

  /* prepare header */
  cntrl.hdr.msgType     = TCNTRL;         /* message type */
   cntrl.hdr.entId.ent   = ENTNW;          /* entity */
   cntrl.hdr.entId.inst  = 0;              /* instance */
   cntrl.hdr.elmId.elmnt = STNWDLSAP;       /* General */
   cntrl.hdr.transId     = 1;     /* transaction identifier */

   cntrl.hdr.response.selector    = 0;
   cntrl.hdr.response.prior       = PRIOR0;
   cntrl.hdr.response.route       = RTESPEC;
   cntrl.hdr.response.mem.region  = S_REG;
   cntrl.hdr.response.mem.pool    = S_POOL;

   cntrl.t.cntrl.action = ABND;
   cntrl.t.cntrl.suId = nif->id;      /* NIF DL sap Id */
   cntrl.t.cntrl.entity = ENTSD;      /* MTP2 */

     return (sng_cntrl_nif (&pst, &cntrl));

}

/***********************************************************************************************************************/
static int ftmod_sctp_debug(int action)
{
	Pst pst;
	SbMgmt cntrl;  

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(SbMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTSB;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTSB;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STSBGEN;       /* General */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	cntrl.t.cntrl.action = action;
	cntrl.t.cntrl.subAction = SADBG;
	cntrl.t.cntrl.dbgMask   = 0xFFFF;

	return (sng_cntrl_sctp (&pst, &cntrl));
}
/***********************************************************************************************************************/

static int ftmod_m2ua_debug(int action)
{
	Pst pst;
	MwMgmt cntrl;  

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(MwMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTMW;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTMW;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STMWGEN;       /* General */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	cntrl.t.cntrl.action = action;
	cntrl.t.cntrl.subAction = SADBG;
	cntrl.t.cntrl.s.dbgMask   = 0xFFFF;

	return (sng_cntrl_m2ua (&pst, &cntrl));
}
/***********************************************************************************************************************/
static int ftmod_tucl_debug(int action)
{
	Pst pst;
	HiMngmt cntrl;  

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(HiMngmt));

	smPstInit(&pst);

	pst.dstEnt = ENTHI;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTHI;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STGEN;       /* General */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	cntrl.t.cntrl.action    = action;
	cntrl.t.cntrl.subAction = SADBG;
	cntrl.t.cntrl.ctlType.hiDbg.dbgMask = 0xFFFF;

	return (sng_cntrl_tucl (&pst, &cntrl));
}
/***********************************************************************************************************************/

/***********************************************************************************************************************/
int ftmod_sctp_ssta_req(int elemt, int id, SbMgmt* cfm)
{
	SbMgmt ssta; 
	Pst pst;
	sng_sctp_link_t *k = &g_ftdm_sngss7_data.cfg.sctpCfg.linkCfg[id];

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&ssta, 0, sizeof(SbMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTSB;

	/* prepare header */
	ssta.hdr.msgType     = TSSTA;         /* message type */
	ssta.hdr.entId.ent   = ENTSB;          /* entity */
	ssta.hdr.entId.inst  = 0;              /* instance */
	ssta.hdr.elmId.elmnt = elemt;  		/* STSBGEN */ /* Others are STSBTSAP, STSBSCTSAP, STSBASSOC, STSBDTA, STSBTMR */ 
	ssta.hdr.transId     = 1;     /* transaction identifier */

	ssta.hdr.response.selector    = 0;
	ssta.hdr.response.prior       = PRIOR0;
	ssta.hdr.response.route       = RTESPEC;
	ssta.hdr.response.mem.region  = S_REG;
	ssta.hdr.response.mem.pool    = S_POOL;

	if((ssta.hdr.elmId.elmnt == STSBSCTSAP) || (ssta.hdr.elmId.elmnt == STSBTSAP))
	{
		ssta.t.ssta.sapId = k->id; /* SapId */
	}
        if(ssta.hdr.elmId.elmnt == STSBASSOC)
        {
		/*TODO - how to get assoc Id*/
                ssta.t.ssta.s.assocSta.assocId = 0; /* association id */
        }
	return(sng_sta_sctp(&pst,&ssta,cfm));
}

int ftmod_m2ua_ssta_req(int elemt, int id, MwMgmt* cfm)
{
	MwMgmt ssta; 
	Pst pst;
	sng_m2ua_cfg_t* 	 m2ua  = NULL; 
	sng_m2ua_cluster_cfg_t*  clust = NULL; 

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&ssta, 0, sizeof(MwMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTMW;

	/* prepare header */
	ssta.hdr.msgType     = TSSTA;         /* message type */
	ssta.hdr.entId.ent   = ENTMW;          /* entity */
	ssta.hdr.entId.inst  = 0;              /* instance */
	ssta.hdr.elmId.elmnt = elemt; 	      /*STMWGEN */ /* Others are STMWSCTSAP, STMWCLUSTER, STMWPEER,STMWSID, STMWDLSAP */
	ssta.hdr.transId     = 1;     /* transaction identifier */

	ssta.hdr.response.selector    = 0;
	ssta.hdr.response.prior       = PRIOR0;
	ssta.hdr.response.route       = RTESPEC;
	ssta.hdr.response.mem.region  = S_REG;
	ssta.hdr.response.mem.pool    = S_POOL;

       switch(ssta.hdr.elmId.elmnt)
       {
          case STMWSCTSAP:
                {
		   m2ua  = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua[id];
		   clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[m2ua->clusterId];
                   ssta.t.ssta.id.suId = clust->sct_sap_id ; /* lower sap Id */            
                   break;
                }       
          case STMWDLSAP:
                {
                   ssta.t.ssta.id.lnkNmb = id ; /* upper sap Id */            
                   break;
                }
          case STMWPEER:
                {
                   ssta.t.ssta.id.peerId = id ; /* peer Id */            
                   break;
                }
          case STMWCLUSTER:
                {
		   clust = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.m2ua_clus[id];
                   ssta.t.ssta.id.clusterId = clust->id ; /* cluster Id */            
                   break;
                }
           default:
                   break;
        }

	return(sng_sta_m2ua(&pst,&ssta,cfm));
}

int ftmod_nif_ssta_req(int elemt, int id, NwMgmt* cfm)
{
	NwMgmt ssta; 
	Pst pst;
	sng_nif_cfg_t* nif = &g_ftdm_sngss7_data.cfg.g_m2ua_cfg.nif[id];

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&ssta, 0, sizeof(NwMgmt));

	smPstInit(&pst);

	pst.dstEnt = ENTNW;

	/* prepare header */
	ssta.hdr.msgType     = TSSTA;         /* message type */
	ssta.hdr.entId.ent   = ENTNW;          /* entity */
	ssta.hdr.entId.inst  = 0;              /* instance */
	ssta.hdr.elmId.elmnt = elemt;

	ssta.hdr.response.selector    = 0;
	ssta.hdr.response.prior       = PRIOR0;
	ssta.hdr.response.route       = RTESPEC;
	ssta.hdr.response.mem.region  = S_REG;
	ssta.hdr.response.mem.pool    = S_POOL;
	ssta.t.ssta.suId = nif->id; /*  Lower sapId */

	return(sng_sta_nif(&pst,&ssta,cfm));
}
