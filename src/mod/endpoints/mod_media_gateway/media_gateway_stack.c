/*
* Copyright (c) 2012, Sangoma Technologies
* Kapil Gupta <kgupta@sangoma.com>
* All rights reserved.
* 
* <Insert license here>
*/

/* INCLUDES *******************************************************************/
#include "mod_media_gateway.h"
#include "media_gateway_stack.h"
/******************************************************************************/

/* DEFINES ********************************************************************/

/* FUNCTION PROTOTYPES ********************************************************/
int mgco_mg_gen_config(void);
int mgco_mu_gen_config(void);
int mgco_tucl_gen_config(void);
int mgco_mu_ssap_config(int idx);
int mgco_mg_tsap_config(megaco_profile_t* profile);
int mgco_mg_ssap_config(megaco_profile_t* profile);
int mgco_mg_peer_config(megaco_profile_t* profile);
int mgco_mg_tpt_server_config(megaco_profile_t* profile);
int mgco_tucl_sap_config(int idx);

int mgco_mg_tsap_bind_cntrl(int idx);
int mgco_mg_tsap_enable_cntrl(int idx);
int mgco_mg_ssap_cntrl(int idx);
int mgco_mu_ssap_cntrl(int idx);
int sng_mgco_tucl_shutdown();
int sng_mgco_mg_shutdown();
int sng_mgco_mg_ssap_stop(int sapId);
int sng_mgco_mg_tpt_server_stop(megaco_profile_t* profile);
int sng_mgco_mg_app_ssap_stop(int idx);
int mg_tucl_debug(int action);

switch_status_t sng_mgco_stack_gen_cfg();


sng_mg_transport_types_e  mg_get_tpt_type(megaco_profile_t* mg_cfg);
sng_mg_transport_types_e  mg_get_tpt_type_from_str(char* tpt_type);
sng_mg_encoding_types_e  mg_get_enc_type_from_str(char* enc_type);
sng_mg_protocol_types_e  mg_get_proto_type_from_str(char* proto_type);

/******************************************************************************/

/* FUNCTIONS ******************************************************************/

switch_status_t sng_mgco_init(sng_mg_event_interface_t* event)
{
	uint32_t major, minor, build;

	switch_assert(event);

	/* initalize sng_mg library */
	sng_mg_init_gen(event);

	/* print the version of the library being used */
	sng_mg_version(&major, &minor, &build);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Loaded LibSng-MEGACO %d.%d.%d\n", major, minor, build);

	/* start up the stack manager */
	if (sng_mg_init_sm()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Failed to start Stack Manager\n");
		return SWITCH_STATUS_FALSE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Started Stack Manager!\n");
	}

	if (sng_mg_init_tucl()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Failed to start TUCL\n");
		return SWITCH_STATUS_FALSE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Started TUCL!\n");
	}

	if (sng_mg_init_mg()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Failed to start MG\n");
		return SWITCH_STATUS_FALSE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Started MG!\n");
	}

	if (sng_mg_init_mu()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Failed to start MU\n");
		return SWITCH_STATUS_FALSE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Started MU!\n");
	}


	/* gen config for all the layers of megaco */
	return sng_mgco_stack_gen_cfg();
}

/*****************************************************************************************************************/
switch_status_t sng_mgco_stack_shutdown()
{
	/* disable MG logging */
	mg_disable_logging();

	/* shutdown MG */
	sng_mgco_mg_shutdown();

	/* shutdown TUCL */
	sng_mgco_tucl_shutdown();

	/* free MEGACO Application */
	sng_mg_free_mu();

	/* free MEGACO */
	sng_mg_free_mg();

	/* free TUCL */
	sng_mg_free_tucl();

	/* free SM */
	sng_mg_free_sm();

	/* free gen */
	sng_mg_free_gen();

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************/
switch_status_t sng_mgco_stack_gen_cfg()
{
	if(mgco_mg_gen_config()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"MG Gen Config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"MG Gen Config SUCCESS \n");	
	}

	if(mgco_mu_gen_config()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"MU(MG-Application) Gen Config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"MU(MG-Application) Gen Config SUCCESS \n");	
	}

	if(mgco_tucl_gen_config()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," TUCL Gen Config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," TUCL Gen Config SUCCESS \n");	
	}

	return SWITCH_STATUS_SUCCESS;
}


/*****************************************************************************************************************/

switch_status_t sng_mgco_cfg(megaco_profile_t* profile)
{
	int idx   = 0x00;

	switch_assert(profile);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Starting MG configuration for idx[%d] against profilename[%s]\n", profile->idx, profile->name);

	idx = profile->idx;

	if(mgco_tucl_sap_config(idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," mgco_tucl_sap_config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," mgco_tucl_sap_config SUCCESS \n");	
	}


	if(mgco_mu_ssap_config(idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," mgco_mu_ssap_config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," mgco_mu_ssap_config SUCCESS \n");	
	}

	if(mgco_mg_tsap_config(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," mgco_mg_tsap_config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," mgco_mg_tsap_config SUCCESS \n");	
	}

	if(mgco_mg_ssap_config(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mgco_mg_ssap_config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mgco_mg_ssap_config SUCCESS \n");	
	}

	if(mgco_mg_peer_config(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mgco_mg_peer_config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mgco_mg_peer_config SUCCESS \n");	
	}

	if(mgco_mg_tpt_server_config(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mgco_mg_tpt_server_config FAILED \n");	
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mgco_mg_tpt_server_config SUCCESS \n");	
	}

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************/

switch_status_t sng_mgco_start(megaco_profile_t* profile )
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Starting MG stack for idx[%d] against profilename[%s]\n", profile->idx, profile->name);

	if(mgco_mu_ssap_cntrl(profile->idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mgco_mu_ssap_cntrl FAILED \n");
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mgco_mu_ssap_cntrl SUCCESS \n");
	}

	if(mgco_mg_tsap_bind_cntrl(profile->idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mgco_mg_tsap_bind_cntrl FAILED \n");
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mgco_mg_tsap_bind_cntrl SUCCESS \n");
	}

	if(mgco_mg_ssap_cntrl(profile->idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mgco_mg_ssap_cntrl FAILED \n");
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mgco_mg_ssap_cntrl SUCCESS \n");
	}

	if(mgco_mg_tsap_enable_cntrl(profile->idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " mgco_mg_tsap_enable_cntrl FAILED \n");
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mgco_mg_tsap_enable_cntrl SUCCESS \n");
	}

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************/

switch_status_t sng_mgco_stop(megaco_profile_t* profile )
{
	int idx = 0x00;

	switch_assert(profile);

	idx = profile->idx;	

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," Stopping MG stack for idx[%d] against profilename[%s]\n", idx, profile->name);

	/* MG STOP is as good as deleting that perticular mg(virtual mg instance) data from megaco stack */
	/* currently we are not supporting enable/disable MG stack */

	if(sng_mgco_mg_ssap_stop(idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " sng_mgco_mg_ssap_stop FAILED \n");
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " sng_mgco_mg_ssap_stop SUCCESS \n");
	}

	if(sng_mgco_mg_tpt_server_stop(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " sng_mgco_mg_tpt_server_stop FAILED \n");
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " sng_mgco_mg_tpt_server_stop SUCCESS \n");
	}

	if(sng_mgco_mg_app_ssap_stop(idx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " sng_mgco_mg_app_ssap_stop FAILED \n");
		return SWITCH_STATUS_FALSE;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " sng_mgco_mg_app_ssap_stop SUCCESS \n");
	}

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************************************************/
int sng_mgco_mg_app_ssap_stop(int idx)
{
        MuMngmt         mgMngmt;
        Pst             pst;              /* Post for layer manager */
        MuCntrl         *cntrl;

        memset(&mgMngmt, 0, sizeof(mgMngmt));

        cntrl = &(mgMngmt.t.cntrl);

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTMU;

        /*fill in the specific fields of the header */
        mgMngmt.hdr.msgType         = TCNTRL;
        mgMngmt.hdr.entId.ent       = ENTMG;
        mgMngmt.hdr.entId.inst      = S_INST;
        mgMngmt.hdr.elmId.elmnt     = STSSAP;
        mgMngmt.hdr.elmId.elmntInst1     = idx;

        cntrl->action       = ADEL;
        cntrl->subAction    = SAELMNT;

        return(sng_cntrl_mu(&pst, &mgMngmt));
}
/*****************************************************************************************************************/

int sng_mgco_mg_ssap_stop(int sapId)
{
	Pst pst;
	MgMngmt cntrl;

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(MgCntrl));

	smPstInit(&pst);

	pst.dstEnt = ENTMG;

	/* prepare header */
	cntrl.hdr.msgType     = TCNTRL;         /* message type */
	cntrl.hdr.entId.ent   = ENTMG;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = STSSAP;       /* SSAP */
	cntrl.hdr.elmId.elmntInst1 = sapId;      /* sap id */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	cntrl.t.cntrl.action    	= ADEL;
	cntrl.t.cntrl.subAction    	= SAELMNT;
	cntrl.t.cntrl.spId 		= sapId;
	return (sng_cntrl_mg (&pst, &cntrl));
}

/*****************************************************************************************************************/
int sng_mgco_mg_tpt_server_stop(megaco_profile_t* mg_cfg)
{
        MgMngmt         mgMngmt;
        Pst             pst;              /* Post for layer manager */
        MgCntrl         *cntrl;
        MgTptCntrl *tptCntrl = &mgMngmt.t.cntrl.s.tptCntrl;
        CmInetIpAddr   ipAddr = 0;

        cntrl = &(mgMngmt.t.cntrl);

        memset(&mgMngmt, 0, sizeof(mgMngmt));

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTMG;

	tptCntrl->transportType = mg_get_tpt_type(mg_cfg); 
        
	tptCntrl->serverAddr.type =  CM_INET_IPV4ADDR_TYPE;
	tptCntrl->serverAddr.u.ipv4TptAddr.port = atoi(mg_cfg->port);
	if(ROK == cmInetAddr((S8*)mg_cfg->my_ipaddr, &ipAddr))
	{
		tptCntrl->serverAddr.u.ipv4TptAddr.address = ntohl(ipAddr);
	}

        /*fill in the specific fields of the header */
        mgMngmt.hdr.msgType         = TCNTRL;
        mgMngmt.hdr.entId.ent       = ENTMG;
        mgMngmt.hdr.entId.inst      = S_INST;
        mgMngmt.hdr.elmId.elmnt     = STSERVER;

        cntrl->action       = ADEL;
        cntrl->subAction    = SAELMNT;

        return(sng_cntrl_mg(&pst, &mgMngmt));
}
/*****************************************************************************************************************/

int mgco_mg_tsap_bind_cntrl(int idx)
{
        MgMngmt         mgMngmt;
        Pst             pst;              /* Post for layer manager */
        MgCntrl         *cntrl;

        memset(&mgMngmt, 0, sizeof(mgMngmt));

        cntrl = &(mgMngmt.t.cntrl);

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTMG;

        /*fill in the specific fields of the header */
        mgMngmt.hdr.msgType         = TCNTRL;
        mgMngmt.hdr.entId.ent       = ENTMG;
        mgMngmt.hdr.entId.inst      = S_INST;
        mgMngmt.hdr.elmId.elmnt     = STTSAP;

        cntrl->action       = ABND_ENA;
        cntrl->subAction    = SAELMNT;
        cntrl->spId 	    = idx; 

        return(sng_cntrl_mg(&pst, &mgMngmt));
}

/*****************************************************************************************************************/

int mgco_mg_tsap_enable_cntrl(int idx)
{
        MgMngmt         mgMngmt;
        Pst             pst;              /* Post for layer manager */
        MgCntrl         *cntrl;

        memset(&mgMngmt, 0, sizeof(mgMngmt));

        cntrl = &(mgMngmt.t.cntrl);

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTMG;

        /*fill in the specific fields of the header */
        mgMngmt.hdr.msgType         = TCNTRL;
        mgMngmt.hdr.entId.ent       = ENTMG;
        mgMngmt.hdr.entId.inst      = S_INST;
        mgMngmt.hdr.elmId.elmnt     = STTSAP;

        cntrl->action       = AENA;
        cntrl->subAction    = SAELMNT;
        cntrl->spId 	    = idx;

        return(sng_cntrl_mg(&pst, &mgMngmt));
}

/*****************************************************************************************************************/

#if 0
int mgco_mg_tpt_server(int idx)
{
        MgMngmt         mgMngmt;
        Pst             pst;              /* Post for layer manager */
        MgCntrl         *cntrl;
        MgTptCntrl *tptCntrl = &mgMngmt.t.cntrl.s.tptCntrl;
        CmInetIpAddr   ipAddr = 0;
	sng_mg_cfg_t* mg_cfg  = &megaco_globals.g_mg_cfg.mg_cfg[idx];

        cntrl = &(mgMngmt.t.cntrl);

        memset(&mgMngmt, 0, sizeof(mgMngmt));

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTMG;

	tptCntrl->transportType = GET_TPT_TYPE(idx);
        
	tptCntrl->serverAddr.type =  CM_INET_IPV4ADDR_TYPE;
	tptCntrl->serverAddr.u.ipv4TptAddr.port = mg_cfg->port;
	if(ROK == cmInetAddr((S8*)mg_cfg->my_ipaddr, &ipAddr))
	{
		tptCntrl->serverAddr.u.ipv4TptAddr.address = ntohl(ipAddr);
	}

        /*fill in the specific fields of the header */
        mgMngmt.hdr.msgType         = TCNTRL;
        mgMngmt.hdr.entId.ent       = ENTMG;
        mgMngmt.hdr.entId.inst      = S_INST;
        mgMngmt.hdr.elmId.elmnt     = STSERVER;

        cntrl->action       = AENA;
        cntrl->subAction    = SAELMNT;
        cntrl->spId = (SpId)0x01;

        return(sng_cntrl_mg(&pst, &mgMngmt));
}
#endif

/*****************************************************************************************************************/

int mgco_mu_ssap_cntrl(int idx)
{
        MuMngmt         mgMngmt;
        Pst             pst;              /* Post for layer manager */
        MuCntrl         *cntrl;

        memset(&mgMngmt, 0, sizeof(mgMngmt));

        cntrl = &(mgMngmt.t.cntrl);

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTMU;

        /*fill in the specific fields of the header */
        mgMngmt.hdr.msgType         = TCNTRL;
        mgMngmt.hdr.entId.ent       = ENTMG;
        mgMngmt.hdr.entId.inst      = S_INST;
        mgMngmt.hdr.elmId.elmnt     = STSSAP;
        mgMngmt.hdr.elmId.elmntInst1     = idx;

        cntrl->action       = ABND_ENA;
        cntrl->subAction    = SAELMNT;

        return(sng_cntrl_mu(&pst, &mgMngmt));
}

/*****************************************************************************************************************/

int mgco_mg_ssap_cntrl(int idx)
{
        MgMngmt         mgMngmt;
        Pst             pst;              /* Post for layer manager */
        MgCntrl         *cntrl;

        memset(&mgMngmt, 0, sizeof(mgMngmt));

        cntrl = &(mgMngmt.t.cntrl);

        /* initalize the post structure */
        smPstInit(&pst);

        /* insert the destination Entity */
        pst.dstEnt = ENTMG;

        /*fill in the specific fields of the header */
        mgMngmt.hdr.msgType         = TCNTRL;
        mgMngmt.hdr.entId.ent       = ENTMG;
        mgMngmt.hdr.entId.inst      = S_INST;
        mgMngmt.hdr.elmId.elmnt     = STSSAP;

        cntrl->action       = AENA;
        cntrl->subAction    = SAELMNT;
        cntrl->spId = (SpId)idx;

        return(sng_cntrl_mg(&pst, &mgMngmt));
}
/******************************************************************************/
                                                                                                       
int mg_enable_logging()
{
	MgMngmt    mgMngmt;
	Pst          pst;              /* Post for layer manager */
	MgCntrl*    cntrl;

	memset(&mgMngmt, 0, sizeof(mgMngmt));
	cntrl = &mgMngmt.t.cntrl;

	mg_tucl_debug(AENA);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMG;
	mgMngmt.hdr.msgType         = TCFG;
	mgMngmt.hdr.entId.ent       = ENTMG;
	mgMngmt.hdr.entId.inst      = S_INST;
	mgMngmt.hdr.elmId.elmnt     = STGEN;

	cntrl->action  		        = AENA;
	cntrl->subAction                = SADBG;
	cntrl->s.dbg.genDbgMask    = 0xfffffdff;

	return(sng_cntrl_mg(&pst, &mgMngmt));
}

/******************************************************************************/
int mg_disable_logging()
{
	MgMngmt    mgMngmt;
	Pst          pst;              /* Post for layer manager */
	MgCntrl*    cntrl;

	memset(&mgMngmt, 0, sizeof(mgMngmt));
	cntrl = &mgMngmt.t.cntrl;

	mg_tucl_debug(ADISIMM);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMG;
	mgMngmt.hdr.msgType         = TCFG;
	mgMngmt.hdr.entId.ent       = ENTMG;
	mgMngmt.hdr.entId.inst      = S_INST;
	mgMngmt.hdr.elmId.elmnt     = STGEN;

	cntrl->action  		        = ADISIMM;
	cntrl->subAction                = SADBG;
	cntrl->s.dbg.genDbgMask    = 0xfffffdff;

	return(sng_cntrl_mg(&pst, &mgMngmt));
}

/******************************************************************************/
int mg_tucl_debug(int action)
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

/******************************************************************************/
int mgco_tucl_gen_config(void)
{
	HiMngmt cfg;
	Pst     pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTHI;

	/* clear the configuration structure */
	memset(&cfg, 0, sizeof(cfg));

	/* fill in the post structure */
	set_dest_sm_pst(&cfg.t.cfg.s.hiGen.lmPst);
	/*fill in the specific fields of the header */
	cfg.hdr.msgType         = TCFG;
	cfg.hdr.entId.ent       = ENTHI;
	cfg.hdr.entId.inst      = S_INST;
	cfg.hdr.elmId.elmnt     = STGEN;

	cfg.t.cfg.s.hiGen.numSaps               = HI_MAX_SAPS;            		/* number of SAPs */
	cfg.t.cfg.s.hiGen.numCons               = HI_MAX_NUM_OF_CON;          		/* maximum num of connections */
	cfg.t.cfg.s.hiGen.numFdsPerSet          = HI_MAX_NUM_OF_FD_PER_SET;     	/* maximum num of fds to use per set */
	cfg.t.cfg.s.hiGen.numFdBins             = HI_MAX_NUM_OF_FD_HASH_BINS;   	/* for fd hash lists */
	cfg.t.cfg.s.hiGen.numClToAccept         = HI_MAX_NUM_OF_CLIENT_TO_ACCEPT; 	/* clients to accept simultaneously */
	cfg.t.cfg.s.hiGen.permTsk               = TRUE;         		 	/* schedule as perm task or timer */
	cfg.t.cfg.s.hiGen.schdTmrVal            = HI_MAX_SCHED_TMR_VALUE;            	/* if !permTsk - probably ignored */
	cfg.t.cfg.s.hiGen.selTimeout            = HI_MAX_SELECT_TIMEOUT_VALUE;     	/* select() timeout */

	/* number of raw/UDP messages to read in one iteration */
	cfg.t.cfg.s.hiGen.numRawMsgsToRead      = HI_MAX_RAW_MSG_TO_READ;
	cfg.t.cfg.s.hiGen.numUdpMsgsToRead      = HI_MAX_UDP_MSG_TO_READ;

	/* thresholds for congestion on the memory pool */
	cfg.t.cfg.s.hiGen.poolStrtThr           = HI_MEM_POOL_START_THRESHOLD;
	cfg.t.cfg.s.hiGen.poolDropThr           = HI_MEM_POOL_DROP_THRESHOLD;
	cfg.t.cfg.s.hiGen.poolStopThr           = HI_MEM_POOL_STOP_THRESHOLD;

	cfg.t.cfg.s.hiGen.timeRes               = HI_PERIOD;        /* time resolution */

#ifdef HI_SPECIFY_GENSOCK_ADDR
	cfg.t.cfg.s.hiGen.ipv4GenSockAddr.address = CM_INET_INADDR_ANY;
	cfg.t.cfg.s.hiGen.ipv4GenSockAddr.port  = 0;                /* DAVIDY - why 0? */
#ifdef IPV6_SUPPORTED
	cfg.t.cfg.s.hiGen.ipv6GenSockAddr.address = CM_INET_INADDR6_ANY;
	cfg.t.cfg.s.hiGen.ipv4GenSockAddr.port  = 0;
#endif
#endif

	return(sng_cfg_tucl(&pst, &cfg));
}

/******************************************************************************/

int mgco_tucl_sap_config(int idx)
{
	HiMngmt cfg;
	Pst     pst;
	HiSapCfg  *pCfg;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTHI;

	/* clear the configuration structure */
	memset(&cfg, 0, sizeof(cfg));

	/* fill in the post structure */
	set_dest_sm_pst(&cfg.t.cfg.s.hiGen.lmPst);
	/*fill in the specific fields of the header */
	cfg.hdr.msgType         = TCFG;
	cfg.hdr.entId.ent       = ENTHI;
	cfg.hdr.entId.inst      = S_INST;
	cfg.hdr.elmId.elmnt     = STTSAP;

	pCfg = &cfg.t.cfg.s.hiSap;

	pCfg->spId 		= idx; 
	pCfg->uiSel 		= 0x00;  /*loosley coupled */
	pCfg->flcEnb 		= TRUE;
	pCfg->txqCongStrtLim 	= HI_SAP_TXN_QUEUE_CONG_START_LIMIT;
	pCfg->txqCongDropLim 	= HI_SAP_TXN_QUEUE_CONG_DROP_LIMIT;
	pCfg->txqCongStopLim 	= HI_SAP_TXN_QUEUE_CONG_STOP_LIMIT;
	pCfg->numBins 		= 10;

	pCfg->uiMemId.region 	= S_REG;
	pCfg->uiMemId.pool   	= S_POOL;
	pCfg->uiPrior 	     	= PRIOR0;
	pCfg->uiRoute        	= RTESPEC;

	return(sng_cfg_tucl(&pst, &cfg));
}

/******************************************************************************/

int mgco_mg_gen_config(void)
{
	MgMngmt    mgMngmt;
	MgGenCfg    *cfg;
	Pst          pst;              /* Post for layer manager */

	memset(&mgMngmt, 0, sizeof(mgMngmt));
	cfg   = &(mgMngmt.t.cfg.c.genCfg);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMG;

	/* fill in the post structure */
	set_dest_sm_pst(&mgMngmt.t.cfg.c.genCfg.lmPst);
	/*fill in the specific fields of the header */
	mgMngmt.hdr.msgType         = TCFG;
	mgMngmt.hdr.entId.ent       = ENTMG;
	mgMngmt.hdr.entId.inst      = S_INST;
	mgMngmt.hdr.elmId.elmnt     = STGEN;


	/*----------- Fill General Configuration Parameters ---------*/
	cfg->maxSSaps = (U16)MG_MAX_SSAPS;
	cfg->maxTSaps = (U32)MG_MAX_TSAPS;
	cfg->maxServers = (U16)MG_MAX_SERVERS;
	cfg->maxConn = (U32)10;
	cfg->maxTxn = (U16)MG_MAX_OUTSTANDING_TRANSACTIONS;
	cfg->maxPeer = (U32)MG_MAX_PEER;
	cfg->resThUpper = (Status)7;
	cfg->resThLower = (Status)3;
#if (defined(GCP_MGCP) || defined(TDS_ROLL_UPGRADE_SUPPORT))   
	cfg->timeResTTL = (Ticks)10000;
#endif

	cfg->timeRes = (Ticks)10;
	cfg->reCfg.rspAckEnb = MG_LMG_GET_RSPACK_MGCO;
	cfg->numBlks = (U32)MG_NUM_BLK;
	cfg->maxBlkSize = (Size)MG_MAXBLKSIZE;
	cfg->numBinsTxnIdHl = (U16)149;
	cfg->numBinsNameHl = (U16)149;
	cfg->entType = LMG_ENT_GW;
	cfg->numBinsTptSrvrHl = (U16)149;
	cfg->indicateRetx = TRUE;  /* Assume environment to be lossy */
	cfg->resOrder = LMG_RES_IPV4; /* IPV4 only */

#ifdef CM_ABNF_MT_LIB
	cfg->firstInst = 1;
	cfg->edEncTmr.enb = FALSE;
	cfg->edEncTmr.val = (U16)50;
	cfg->edDecTmr.enb = TRUE;
	cfg->edDecTmr.val = (U16)50;
	cfg->noEDInst = 1;
#endif /* CM_ABNF_MT_LIB */

	cfg->entType  = LMG_ENT_GW; 

#ifdef GCP_CH
	cfg->numBinsPeerCmdHl = 20;
	cfg->numBinsTransReqHl = 50;
	cfg->numBinsTransIndRspCmdHl = 50;
#endif /* GCP_CH */

#ifdef GCP_MG
	cfg->maxMgCmdTimeOut.enb =TRUE;
	cfg->maxMgCmdTimeOut.val =20;
#endif /* GCP_MG */

#ifdef GCP_MG
	cfg->maxMgCmdTimeOut.enb =TRUE;
	cfg->maxMgCmdTimeOut.val =20;
#endif /* GCP_MG */

#ifdef GCP_MGC
	cfg->maxMgcCmdTimeOut.enb =TRUE;
	cfg->maxMgcCmdTimeOut.val =20;
#endif /* GCP_MG */

#if (defined(GCP_MGCO) && (defined GCP_VER_2_1))
	cfg->reCfg.segRspTmr.enb = TRUE;
	cfg->reCfg.segRspTmr.val = (U16)50;
	cfg->reCfg.segRspAckTmr.enb = TRUE;
	cfg->reCfg.segRspAckTmr.val = (U16)25;
#endif

#ifdef GCP_PKG_MGCO_ROOT
	cfg->limit.pres.pres = PRSNT_NODEF;
	cfg->limit.mgcOriginatedPendingLimit = 20000;
	cfg->limit.mgOriginatedPendingLimit  = 20000;
#endif /* GCP_PKG_MGCO_ROOT */

	return(sng_cfg_mg(&pst, &mgMngmt));
}

/******************************************************************************/

int mgco_mu_gen_config(void)
{
	MuMngmt      mgmt;
	MuGenCfg    *cfg;
	Pst          pst;              /* Post for layer manager */

	memset(&mgmt, 0, sizeof(mgmt));
	cfg   = &(mgmt.t.cfg);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMU;

	/*fill in the specific fields of the header */
	mgmt.hdr.msgType         = TCFG;
	mgmt.hdr.entId.ent       = ENTMU;
	mgmt.hdr.entId.inst      = S_INST;
	mgmt.hdr.elmId.elmnt     = STGEN;

	return(sng_cfg_mu(&pst, &mgmt));
}

/******************************************************************************/

int mgco_mu_ssap_config(int idx)
{
	MuMngmt      mgmt;
	MuSAP_t      *cfg;
	Pst          pst;              /* Post for layer manager */

	memset(&mgmt, 0, sizeof(mgmt));
	cfg   = &(mgmt.t.sapCfg);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMU;

	/*fill in the specific fields of the header */
	mgmt.hdr.msgType         = TCFG;
	mgmt.hdr.entId.ent       = ENTMU;
	mgmt.hdr.entId.inst      = S_INST;
	mgmt.hdr.elmId.elmnt     = STSSAP;

	/* fill lower layer i.e. MG PST */ 
	cfg->ssapId 		= idx; 
	cfg->spId 		= idx;

	cfg->mem.region 	= S_REG; 
	cfg->mem.pool 	        = S_POOL;
	cfg->dstProcId          = SFndProcId();
	cfg->dstEnt             = ENTMG; 
	cfg->dstInst            = S_INST; 
	cfg->dstPrior     	= PRIOR0; 
	cfg->dstRoute     	= RTESPEC; 
	cfg->selector       	= 0x00; /* Loosely coupled */ 

	return(sng_cfg_mu(&pst, &mgmt));
}

/******************************************************************************/

int mgco_mg_ssap_config(megaco_profile_t* profile)
{
	MgMngmt       mgMngmt;
	MgSSAPCfg    *pCfg;
	Pst          pst;              /* Post for layer manager */
	CmInetIpAddr ipAddr;
	int len = 0x00;

	memset(&mgMngmt, 0, sizeof(mgMngmt));
	pCfg   = &(mgMngmt.t.cfg.c.sSAPCfg);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMG;

	/*fill in the specific fields of the header */
	mgMngmt.hdr.msgType         = TCFG;
	mgMngmt.hdr.entId.ent       = ENTMG;
	mgMngmt.hdr.entId.inst      = S_INST;
	mgMngmt.hdr.elmId.elmnt     = STSSAP;

	/* FILL SAP config */

	pCfg->sSAPId 		= profile->idx;  		/* SSAP ID */ 
	pCfg->sel 		= 0x00 ; 				/* Loosely coupled */ 
	pCfg->memId.region 	= S_REG; 
	pCfg->memId.pool 	= S_POOL;
	pCfg->prior 		= PRIOR0; 
	pCfg->route 		= RTESPEC;

	pCfg->protocol 		= mg_get_proto_type_from_str(profile->protocol_type);

	pCfg->startTxnNum = 50;
	pCfg->endTxnNum   = 60;

	pCfg->initReg = TRUE;
	pCfg->mwdTimer = (U16)10;

	pCfg->minMgcoVersion = LMG_VER_PROF_MGCO_H248_1_0;
	switch(profile->protocol_version)
	{
		case 1:
			pCfg->maxMgcoVersion = LMG_VER_PROF_MGCO_H248_1_0;
			break;
		case 2:
			pCfg->maxMgcoVersion = LMG_VER_PROF_MGCO_H248_2_0;
			break;
		case 3:
			pCfg->maxMgcoVersion = LMG_VER_PROF_MGCO_H248_3_0;
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Protocol version[%d] \n",profile->protocol_version);
			return SWITCH_STATUS_FALSE;
	}

	pCfg->chEnabled = 0x01;

	pCfg->userInfo.pres.pres = PRSNT_NODEF;
	pCfg->userInfo.id.pres 	 = NOTPRSNT;
	pCfg->userInfo.mid.pres = PRSNT_NODEF;
	pCfg->userInfo.dname.namePres.pres = PRSNT_NODEF;

	pCfg->userInfo.mid.len = (U8)strlen((char*)profile->mid);
	strncpy((char*)pCfg->userInfo.mid.val, (char*)profile->mid, MAX_MID_LEN);

	len = (U32)strlen((char*)profile->my_domain);
	memcpy( (U8*)(pCfg->userInfo.dname.name),
			(CONSTANT U8*)(profile->my_domain), len );
	pCfg->userInfo.dname.name[len] = '\0';

	pCfg->userInfo.dname.netAddr.type = CM_TPTADDR_IPV4;
	memset(&ipAddr,'\0',sizeof(ipAddr));
	if(ROK == cmInetAddr((S8*)profile->my_ipaddr,&ipAddr))
	{
		pCfg->userInfo.dname.netAddr.u.ipv4NetAddr = ntohl(ipAddr);
	}

	pCfg->reCfg.initRetxTmr.enb = TRUE;
	pCfg->reCfg.initRetxTmr.val = MG_INIT_RTT;
	pCfg->reCfg.provRspTmr.enb = TRUE;
	pCfg->reCfg.provRspTmr.val = (U16)50; /* In timer resolution */
	pCfg->reCfg.provRspDelay = 2;
	pCfg->reCfg.atMostOnceTmr.enb = TRUE;
	pCfg->reCfg.atMostOnceTmr.val = (U16)30;

	return(sng_cfg_mg(&pst, &mgMngmt));
}

/******************************************************************************/

int mgco_mg_tsap_config(megaco_profile_t* profile)
{
	MgMngmt    mgMngmt;
	MgTSAPCfg    *cfg;
	Pst          pst; 

	memset(&mgMngmt, 0, sizeof(mgMngmt));
	cfg   = &(mgMngmt.t.cfg.c.tSAPCfg);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMG;

	/*fill in the specific fields of the header */
	mgMngmt.hdr.msgType         = TCFG;
	mgMngmt.hdr.entId.ent       = ENTMG;
	mgMngmt.hdr.entId.inst      = S_INST;
	mgMngmt.hdr.elmId.elmnt     = STTSAP;

	/* FILL TSAP config */
	cfg->tSAPId 	= profile->idx;
	cfg->spId   	= profile->idx;
	cfg->provType 	= LMG_PROV_TYPE_TUCL; 

	/* FILL TUCL Information */
	cfg->memId.region = S_REG; 
	cfg->memId.pool   = S_POOL; 
	cfg->dstProcId    = SFndProcId();
	cfg->dstEnt       = ENTHI; 
	cfg->dstInst      = S_INST; 
	cfg->dstPrior     = PRIOR0; 
	cfg->dstRoute     = RTESPEC; 
	cfg->dstSel       = 0x00; /* Loosely coupled */ 
	cfg->bndTmrCfg.enb = TRUE;
	cfg->bndTmrCfg.val = 5; /* 5 seconds */


	/* Disable DNS as of now */
	cfg->reCfg.idleTmr.enb = FALSE;
        cfg->reCfg.dnsCfg.dnsAccess = LMG_DNS_DISABLED;
        cfg->reCfg.dnsCfg.dnsAddr.type = CM_TPTADDR_IPV4;
        cfg->reCfg.dnsCfg.dnsAddr.u.ipv4TptAddr.port = (U16)53;
        cfg->reCfg.dnsCfg.dnsAddr.u.ipv4TptAddr.address = (CmInetIpAddr)MG_DNS_IP;

        cfg->reCfg.dnsCfg.dnsRslvTmr.enb = FALSE;
        cfg->reCfg.dnsCfg.dnsRslvTmr.val = 60; /* 60 sec */
        cfg->reCfg.dnsCfg.maxRetxCnt = 4;

        cfg->reCfg.tMax = 1000;
        cfg->reCfg.tptParam.type = CM_TPTPARAM_SOCK;
        cfg->reCfg.tptParam.u.sockParam.listenQSize = 5;
        cfg->reCfg.tptParam.u.sockParam.numOpts = 0;


	return(sng_cfg_mg(&pst, &mgMngmt));
}

/******************************************************************************/

int mgco_mg_peer_config(megaco_profile_t* mg_cfg)
{
	MgMngmt    	mgMngmt;
	MgGcpEntCfg    *cfg;
	Pst          	pst;              /* Post for layer manager */
	U32            peerIdx = 0;
	CmInetIpAddr   ipAddr = 0;
	mg_peer_profile_t*  mg_peer =  NULL;

	memset(&mgMngmt, 0, sizeof(mgMngmt));
	cfg   = &(mgMngmt.t.cfg.c.mgGcpEntCfg);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMG;

	/*fill in the specific fields of the header */
	mgMngmt.hdr.msgType         = TCFG;
	mgMngmt.hdr.entId.ent       = ENTMG;
	mgMngmt.hdr.entId.inst      = S_INST;
	mgMngmt.hdr.elmId.elmnt     = STGCPENT;

	cfg->numPeer 			= mg_cfg->total_peers;
	for(peerIdx =0; peerIdx < mg_cfg->total_peers; peerIdx++){

		mg_peer = megaco_peer_profile_locate(mg_cfg->peer_list[peerIdx]);

		cfg->peerCfg[peerIdx].sSAPId 	= mg_cfg->idx;        /* SSAP ID */;
		cfg->peerCfg[peerIdx].port 	= atoi(mg_peer->port);
		cfg->peerCfg[peerIdx].tsapId 	= mg_cfg->idx;

		cfg->peerCfg[peerIdx].mtuSize = MG_MAX_MTU_SIZE;

		cfg->peerCfg[peerIdx].peerAddrTbl.count = 1;
		cfg->peerCfg[peerIdx].peerAddrTbl.netAddr[0].type =
			CM_NETADDR_IPV4;

		if(ROK == cmInetAddr((S8*)&mg_peer->ipaddr[0],&ipAddr))
		{
			cfg->peerCfg[peerIdx].peerAddrTbl.netAddr[0].u.ipv4NetAddr = ntohl(ipAddr);
		}
		else
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "cmInetAddr failed \n");
			cfg->peerCfg[peerIdx].peerAddrTbl.count = 0;
		}

#ifdef GCP_MG
		cfg->peerCfg[peerIdx].transportType  = mg_get_tpt_type_from_str(mg_peer->transport_type);
		cfg->peerCfg[peerIdx].encodingScheme = mg_get_enc_type_from_str(mg_peer->encoding_type);
		cfg->peerCfg[peerIdx].mgcPriority = peerIdx;
		cfg->peerCfg[peerIdx].useAHScheme = FALSE;
		cfg->peerCfg[peerIdx].mid.pres = PRSNT_NODEF;
		cfg->peerCfg[peerIdx].mid.len = strlen((char*)mg_peer->mid);
		cmMemcpy((U8 *)cfg->peerCfg[peerIdx].mid.val, 
				(CONSTANT U8*)(char*)mg_peer->mid, 
				cfg->peerCfg[peerIdx].mid.len);

#endif /* GCP_MG */
	}

	return(sng_cfg_mg(&pst, &mgMngmt));
}

/******************************************************************************/

int mgco_mg_tpt_server_config(megaco_profile_t* mg_cfg)
{
	MgMngmt    	mgMngmt;
	MgTptSrvrCfg    *cfg;
	Pst          	pst;              /* Post for layer manager */
	CmInetIpAddr   ipAddr = 0;
	int srvIdx = 0;
	mg_peer_profile_t*  mg_peer = megaco_peer_profile_locate(mg_cfg->peer_list[0]);

	memset(&mgMngmt, 0, sizeof(mgMngmt));
	cfg   = &(mgMngmt.t.cfg.c.tptSrvrCfg);

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTMG;

	/*fill in the specific fields of the header */
	mgMngmt.hdr.msgType         = TCFG;
	mgMngmt.hdr.entId.ent       = ENTMG;
	mgMngmt.hdr.entId.inst      = S_INST;
	mgMngmt.hdr.elmId.elmnt     = STSERVER;

	cfg->count = 1;
	cfg->srvr[srvIdx].isDefault = TRUE;
	cfg->srvr[srvIdx].sSAPId = mg_cfg->idx;
	cfg->srvr[srvIdx].tSAPId = mg_cfg->idx; 
	cfg->srvr[srvIdx].protocol = mg_get_proto_type_from_str(mg_cfg->protocol_type);
	cfg->srvr[srvIdx].transportType = mg_get_tpt_type(mg_cfg); 
	cfg->srvr[srvIdx].encodingScheme = mg_get_enc_type_from_str(mg_peer->encoding_type);

	cfg->srvr[srvIdx].tptParam.type = CM_TPTPARAM_SOCK;
	cfg->srvr[srvIdx].tptParam.u.sockParam.listenQSize = 5;
	cfg->srvr[srvIdx].tptParam.u.sockParam.numOpts = 0;
	cfg->srvr[srvIdx].lclTptAddr.type = CM_TPTADDR_IPV4;
	cfg->srvr[srvIdx].lclTptAddr.u.ipv4TptAddr.port = atoi(mg_cfg->port);
	if(ROK == cmInetAddr((S8*)mg_cfg->my_ipaddr, &ipAddr))
	{
		cfg->srvr[srvIdx].lclTptAddr.u.ipv4TptAddr.address = ntohl(ipAddr);
	}

	return(sng_cfg_mg(&pst, &mgMngmt));
}

/******************************************************************************/
int sng_mgco_tucl_shutdown()
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
int sng_mgco_mg_shutdown()
{
        Pst pst;
        MgMngmt cntrl;

        memset((U8 *)&pst, 0, sizeof(Pst));
        memset((U8 *)&cntrl, 0, sizeof(MgCntrl));

        smPstInit(&pst);

        pst.dstEnt = ENTMG;

        /* prepare header */
        cntrl.hdr.msgType     = TCNTRL;         /* message type */
        cntrl.hdr.entId.ent   = ENTMG;          /* entity */
        cntrl.hdr.entId.inst  = 0;              /* instance */
        cntrl.hdr.elmId.elmnt = STGEN;       /* General */

        cntrl.hdr.response.selector    = 0;
        cntrl.hdr.response.prior       = PRIOR0;
        cntrl.hdr.response.route       = RTESPEC;
        cntrl.hdr.response.mem.region  = S_REG;
        cntrl.hdr.response.mem.pool    = S_POOL;

        cntrl.t.cntrl.action    = ASHUTDOWN;
        cntrl.t.cntrl.subAction    = SAELMNT;

        return (sng_cntrl_mg (&pst, &cntrl));
}
/******************************************************************************/
int sng_mgco_mg_get_status(int elemId, MgMngmt* cfm,  megaco_profile_t* mg_cfg, mg_peer_profile_t* mg_peer)
{
	Pst pst;
	MgMngmt cntrl;
	CmInetIpAddr   ipAddr = 0;

	memset((U8 *)&pst, 0, sizeof(Pst));
	memset((U8 *)&cntrl, 0, sizeof(MgCntrl));

	smPstInit(&pst);

	pst.dstEnt = ENTMG;

	/* prepare header */
	/*cntrl.hdr.msgType     = TCNTRL;  */       /* message type */
	cntrl.hdr.entId.ent   = ENTMG;          /* entity */
	cntrl.hdr.entId.inst  = 0;              /* instance */
	cntrl.hdr.elmId.elmnt = elemId;       /* General */

	cntrl.hdr.response.selector    = 0;
	cntrl.hdr.response.prior       = PRIOR0;
	cntrl.hdr.response.route       = RTESPEC;
	cntrl.hdr.response.mem.region  = S_REG;
	cntrl.hdr.response.mem.pool    = S_POOL;

	switch(elemId)
	{
		case STGCPENT:
			{
				cntrl.t.ssta.s.mgPeerSta.peerId.pres = PRSNT_NODEF;
				cntrl.t.ssta.s.mgPeerSta.peerId.val  = mg_cfg->idx;

				cntrl.t.ssta.s.mgPeerSta.mid.pres = PRSNT_NODEF;
				cntrl.t.ssta.s.mgPeerSta.mid.len  = strlen((char*)mg_peer->mid);
				cmMemcpy((U8 *)cntrl.t.ssta.s.mgPeerSta.mid.val, 
						(CONSTANT U8*)(char*)mg_peer->mid, 
					 	cntrl.t.ssta.s.mgPeerSta.mid.len);	
				break;
			}
		case STSSAP:
			{
				cntrl.t.ssta.s.mgSSAPSta.sapId = mg_cfg->idx; 
				break;
			}
		case STTSAP:
			{
				cntrl.t.ssta.s.mgTSAPSta.tSapId = mg_cfg->idx;
				break;
			}
		case STSERVER:
			{
				cntrl.t.ssta.s.mgTptSrvSta.tptAddr.type =  CM_INET_IPV4ADDR_TYPE;
				cntrl.t.ssta.s.mgTptSrvSta.tptAddr.u.ipv4TptAddr.port = atoi(mg_cfg->port); 
				if(ROK == cmInetAddr((S8*)mg_cfg->my_ipaddr, &ipAddr))
				{
					cntrl.t.ssta.s.mgTptSrvSta.tptAddr.u.ipv4TptAddr.address = ntohl(ipAddr);
				}


				break;
			}
		default:
			break;
	}

	return (sng_sta_mg (&pst, &cntrl, cfm));
}
 
/**********************************************************************************************************************************/
sng_mg_transport_types_e mg_get_tpt_type(megaco_profile_t* mg_profile)
{
	mg_peer_profile_t* 	mg_peer_profile = NULL; 

	if(NULL == mg_profile){
		return SNG_MG_TPT_NONE;
	}

	if(!mg_profile->total_peers){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " No peer configured against profilename[%s]\n",mg_profile->name);
		return SNG_MG_TPT_NONE;
	}

	if( NULL == (mg_peer_profile = megaco_peer_profile_locate(mg_profile->peer_list[0]))){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Not able to get peer_profile node based on profilename[%s]\n",mg_profile->peer_list[0]);
		return SNG_MG_TPT_NONE;
	}

	return mg_get_tpt_type_from_str(mg_peer_profile->transport_type);
}
/**********************************************************************************************************************************/

sng_mg_transport_types_e  mg_get_tpt_type_from_str(char* tpt_type)
{
	if(!tpt_type) return SNG_MG_TPT_NONE;

	if(!strcasecmp(tpt_type, "UDP")){
		return SNG_MG_TPT_UDP;
	}else if(!strcasecmp(tpt_type,"TCP")){
		return SNG_MG_TPT_TCP;
	}else if(!strcasecmp(tpt_type,"STCP")){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STCP Transport for H.248 Protocol Not Yet Supported \n");
		return SNG_MG_TPT_SCTP;
	}else{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Protocol Value[%s] \n",tpt_type);
		return SNG_MG_TPT_NONE;
	}

	return SNG_MG_TPT_NONE;
}
/**********************************************************************************************************************************/

sng_mg_encoding_types_e  mg_get_enc_type_from_str(char* enc_type)
{
	if(!enc_type) return SNG_MG_ENCODING_NONE;

	if(!strcasecmp(enc_type, "TEXT")){
		return SNG_MG_ENCODING_TEXT;
	} else if(!strcasecmp(enc_type, "BINARY")){
		return SNG_MG_ENCODING_BINARY;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Encoding Type[%s] \n", enc_type);
		return SNG_MG_ENCODING_NONE;
	}

	return SNG_MG_ENCODING_NONE;
}

/**********************************************************************************************************************************/

sng_mg_protocol_types_e  mg_get_proto_type_from_str(char* proto_type)
{
	if(!proto_type) return SNG_MG_NONE;

	if(!strcasecmp(proto_type,"MEGACO")) {
		return SNG_MG_MEGACO;
	}else if(!strcasecmp(proto_type,"MGCP")){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MGCP Protocol Not Yet Supported \n");
		return SNG_MG_MGCP;
	}else{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Protocol Value[%s] \n",proto_type);
		return SNG_MG_NONE;
	}

	return SNG_MG_NONE;
}


/**********************************************************************************************************************************/
