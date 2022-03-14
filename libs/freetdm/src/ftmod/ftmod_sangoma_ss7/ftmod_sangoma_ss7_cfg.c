/*
 * Copyright (c) 2009|Konrad Hammel <konrad@sangoma.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms|with or without
 * modification|are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice|this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice|this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES|INCLUDING|BUT NOT
 * LIMITED TO|THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT|INDIRECT|INCIDENTAL|SPECIAL,
 * EXEMPLARY|OR CONSEQUENTIAL DAMAGES (INCLUDING|BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE|DATA|OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY|WHETHER IN CONTRACT|STRICT LIABILITY|OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE|EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
int ft_to_sngss7_cfg_all(void);

int ftmod_ss7_relay_gen_config(void);
int ftmod_ss7_mtp1_gen_config(void);
int ftmod_ss7_mtp2_gen_config(void);
int ftmod_ss7_mtp3_gen_config(void);
int ftmod_ss7_isup_gen_config(void);
int ftmod_ss7_cc_gen_config(void);

int ftmod_ss7_mtp1_psap_config(int id);

int ftmod_ss7_mtp2_dlsap_config(int id);

int ftmod_ss7_mtp3_dlsap_config(int id);
int ftmod_ss7_mtp3_nsap_config(int id);
int ftmod_ss7_mtp3_linkset_config(int id);
int ftmod_ss7_mtp3_route_config(int id);

int ftmod_ss7_isup_nsap_config(int id);
int ftmod_ss7_isup_intf_config(int id);
int ftmod_ss7_isup_ckt_config(int id);
int ftmod_ss7_isup_isap_config(int id);

int ftmod_ss7_cc_isap_config(int id);

int ftmod_ss7_relay_chan_config(int id);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
int  ft_to_sngss7_cfg_all(void)
{
	int x = 0;
	int ret = 0;

	/* check if we have done gen_config already */
	if (g_ftdm_sngss7_data.gen_config == SNG_GEN_CFG_STATUS_INIT) {
		/* update the global gen_config so we don't do it again */
		g_ftdm_sngss7_data.gen_config = SNG_GEN_CFG_STATUS_PENDING;

		/* start of by checking if the license and sig file are valid */
		if (sng_validate_license(g_ftdm_sngss7_data.cfg.license,
								 g_ftdm_sngss7_data.cfg.signature)) {

			SS7_CRITICAL("License verification failed..ending!\n");
			return 1;
		}

		/* if the procId is not 0 then we are using relay mode */
		if (g_ftdm_sngss7_data.cfg.procId != 0) {
			/* set the desired procID value */
			sng_set_procId((uint16_t)g_ftdm_sngss7_data.cfg.procId);
		}
			
		/* start up the stack manager */
		if (sng_isup_init_sm()) {
			SS7_CRITICAL("Failed to start Stack Manager\n");
			return 1;
		} else {
			SS7_INFO("Started Stack Manager!\n");
			sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_SM_STARTED);
		}

		/* check if the configuration had a Relay Channel */
		if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_RY_PRESENT)) { 
			/* start up the relay task */
			if (sng_isup_init_relay()) {
				SS7_CRITICAL("Failed to start Relay\n");
				return 1;
			} else {
				SS7_INFO("Started Relay!\n");
				sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_RY_STARTED);
			}

			/* run general configuration on the relay task */
			if (ftmod_ss7_relay_gen_config()) {
				SS7_CRITICAL("Relay General configuration FAILED!\n");
				return 1;
			} else {
				SS7_INFO("Relay General configuration DONE\n");
			}

		} /* if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_RY)) */

		if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_CC_PRESENT)) {
			if (sng_isup_init_cc()) {
				SS7_CRITICAL("Failed to start Call-Control\n");
				return 1;
			} else {
				SS7_INFO("Started Call-Control!\n");
				sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_CC_STARTED);
			}
			if (ftmod_ss7_cc_gen_config()) {
				SS7_CRITICAL("CC General configuration FAILED!\n");
				return 1;
			} else {
				SS7_INFO("CC General configuration DONE\n");
			}
			if (ftmod_ss7_cc_isap_config(1)) {
				SS7_CRITICAL("CC ISAP configuration FAILED!\n");
				return 1;
			} else {
				SS7_INFO("CC ISAP configuration DONE!\n");
			}
		} /* if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_CC)) */

		if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_ISUP_PRESENT)) {
			if (sng_isup_init_isup()) {
				SS7_CRITICAL("Failed to start ISUP\n");
				return 1;
			} else {
				SS7_INFO("Started ISUP!\n");
				sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_ISUP_STARTED);
			}	
			if (ftmod_ss7_isup_gen_config()) {
				SS7_CRITICAL("ISUP General configuration FAILED!\n");
				return 1;
			} else {
				SS7_INFO("ISUP General configuration DONE\n");
			}
		} /* if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_ISUP)) */

		if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP3_PRESENT)) {
			if (sng_isup_init_mtp3()) {
				SS7_CRITICAL("Failed to start MTP3\n");
				return 1;
			} else {
				SS7_INFO("Started MTP3!\n");
				sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP3_STARTED);
			}

			if (ftmod_ss7_mtp3_gen_config()) {
				SS7_CRITICAL("MTP3 General configuration FAILED!\n");
				return 1;
			} else {
				SS7_INFO("MTP3 General configuration DONE\n");
			}
		} /* if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP3)) */

		if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP2_PRESENT)) {
			if (sng_isup_init_mtp2()) {
				SS7_CRITICAL("Failed to start MTP2\n");
				return 1;
			} else {
				SS7_INFO("Started MTP2!\n");
				sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP2_STARTED);
			}
			if (sng_isup_init_mtp1()) {
				SS7_CRITICAL("Failed to start MTP1\n");
				return 1;
			} else {
				SS7_INFO("Started MTP1!\n");
				sngss7_set_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP1_STARTED);
			}
			if (ftmod_ss7_mtp1_gen_config()) {
				SS7_CRITICAL("MTP1 General configuration FAILED!\n");
				return 1;
			} else {
				SS7_INFO("MTP1 General configuration DONE\n");
			}
			if (ftmod_ss7_mtp2_gen_config()) {
				SS7_CRITICAL("MTP2 General configuration FAILED!\n");
				return 1;
			} else {
				SS7_INFO("MTP2 General configuration DONE\n");
			}
		} /* if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP2)) */

		if(SNG_SS7_OPR_MODE_M2UA_SG == g_ftdm_operating_mode){
			if(FTDM_SUCCESS != ftmod_ss7_m2ua_init()){
				ftdm_log (FTDM_LOG_ERROR, "ftmod_ss7_m2ua_init FAILED \n");
				return FTDM_FAIL;
			}
		}

		g_ftdm_sngss7_data.gen_config = SNG_GEN_CFG_STATUS_DONE;

	} /* if (!(g_ftdm_sngss7_data.gen_config)) */


	if (g_ftdm_sngss7_data.gen_config != SNG_GEN_CFG_STATUS_DONE) {
			SS7_CRITICAL("General configuration FAILED!\n");
			return 1;
	}

	x = 1;
	while (x < (MAX_MTP_LINKS)) {
		/* check if this link has been configured already */
		if ((g_ftdm_sngss7_data.cfg.mtp1Link[x].id != 0) &&
			(!(g_ftdm_sngss7_data.cfg.mtp1Link[x].flags & SNGSS7_CONFIGURED))) {

			/* configure mtp1 */
			if (ftmod_ss7_mtp1_psap_config(x)) {
				SS7_CRITICAL("MTP1 PSAP %d configuration FAILED!\n", x);
				return 1;;
			} else {
				SS7_INFO("MTP1 PSAP %d configuration DONE!\n", x);
			}

			/* set the SNGSS7_CONFIGURED flag */
			g_ftdm_sngss7_data.cfg.mtp1Link[x].flags |= SNGSS7_CONFIGURED;
		}
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	x = 1;
	while (x < (MAX_MTP_LINKS)) {
		/* check if this link has been configured already */
		if ((g_ftdm_sngss7_data.cfg.mtp2Link[x].id != 0) &&
			(!(g_ftdm_sngss7_data.cfg.mtp2Link[x].flags & SNGSS7_CONFIGURED))) {

			/* configure mtp2 */
			if (ftmod_ss7_mtp2_dlsap_config(x)) {
				SS7_CRITICAL("MTP2 DLSAP %d configuration FAILED!\n",x);
				return 1;;
			} else {
				SS7_INFO("MTP2 DLSAP %d configuration DONE!\n", x);
			}

			/* set the SNGSS7_CONFIGURED flag */
			g_ftdm_sngss7_data.cfg.mtp2Link[x].flags |= SNGSS7_CONFIGURED;
		}
		x++;
	} /* while (x < (MAX_MTP_LINKS+1)) */

	/* no configs above mtp2 for relay */
	if (g_ftdm_sngss7_data.cfg.procId == 1) {
		x = 1;
			while (x < (MAX_MTP_LINKS)) {
				/* check if this link has been configured already */
				if ((g_ftdm_sngss7_data.cfg.mtp3Link[x].id != 0) &&
						(!(g_ftdm_sngss7_data.cfg.mtp3Link[x].flags & SNGSS7_CONFIGURED))) {

					/* configure mtp3 */
					if (ftmod_ss7_mtp3_dlsap_config(x)) {
						SS7_CRITICAL("MTP3 DLSAP %d configuration FAILED!\n", x);
						return 1;;
					} else {
						SS7_INFO("MTP3 DLSAP %d configuration DONE!\n", x);
					}

					/* set the SNGSS7_CONFIGURED flag */
					g_ftdm_sngss7_data.cfg.mtp3Link[x].flags |= SNGSS7_CONFIGURED;
				}

				x++;
			} /* while (x < (MAX_MTP_LINKS+1)) */

		/* in M2UA_SG mode there will not be any MTP3 layer */
			if(SNG_SS7_OPR_MODE_M2UA_SG != g_ftdm_operating_mode){
				x = 1;
				while (x < (MAX_NSAPS)) {
					/* check if this link has been configured already */
					if ((g_ftdm_sngss7_data.cfg.nsap[x].id != 0) &&
							(!(g_ftdm_sngss7_data.cfg.nsap[x].flags & SNGSS7_CONFIGURED))) {

						ret = ftmod_ss7_mtp3_nsap_config(x);
						if (ret) {
							SS7_CRITICAL("MTP3 NSAP %d configuration FAILED!(%s)\n", x, DECODE_LCM_REASON(ret));
							return 1;
						} else {
							SS7_INFO("MTP3 NSAP %d configuration DONE!\n", x);
						}

						ret = ftmod_ss7_isup_nsap_config(x);
						if (ret) {
							SS7_CRITICAL("ISUP NSAP %d configuration FAILED!(%s)\n", x, DECODE_LCM_REASON(ret));
							return 1;
						} else {
							SS7_INFO("ISUP NSAP %d configuration DONE!\n", x);
						}

						/* set the SNGSS7_CONFIGURED flag */
						g_ftdm_sngss7_data.cfg.nsap[x].flags |= SNGSS7_CONFIGURED;
					} /* if !SNGSS7_CONFIGURED */

					x++;
				} /* while (x < (MAX_NSAPS)) */
			}

		/* in M2UA_SG mode there will not be any MTP3 layer */
		if(SNG_SS7_OPR_MODE_M2UA_SG != g_ftdm_operating_mode){
			x = 1;
			while (x < (MAX_MTP_LINKSETS+1)) {
				/* check if this link has been configured already */
				if ((g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id != 0) &&
						(!(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].flags & SNGSS7_CONFIGURED))) {

					if (ftmod_ss7_mtp3_linkset_config(x)) {
						SS7_CRITICAL("MTP3 LINKSET %d configuration FAILED!\n", x);
						return 1;
					} else {
						SS7_INFO("MTP3 LINKSET %d configuration DONE!\n", x);
					}

					/* set the SNGSS7_CONFIGURED flag */
					g_ftdm_sngss7_data.cfg.mtpLinkSet[x].flags |= SNGSS7_CONFIGURED;
				} /* if !SNGSS7_CONFIGURED */

				x++;
			} /* while (x < (MAX_MTP_LINKSETS+1)) */
		}

		/* in M2UA_SG mode there will not be any MTP3 layer */
		if(SNG_SS7_OPR_MODE_M2UA_SG != g_ftdm_operating_mode){
			x = 1;
			while (x < (MAX_MTP_ROUTES+1)) {
				/* check if this link has been configured already */
				if ((g_ftdm_sngss7_data.cfg.mtpRoute[x].id != 0) &&
						(!(g_ftdm_sngss7_data.cfg.mtpRoute[x].flags & SNGSS7_CONFIGURED))) {

					if (ftmod_ss7_mtp3_route_config(x)) {
						SS7_CRITICAL("MTP3 ROUTE %d configuration FAILED!\n", x);
						return 1;
					} else {
						SS7_INFO("MTP3 ROUTE %d configuration DONE!\n",x);
					}

					/* set the SNGSS7_CONFIGURED flag */
					g_ftdm_sngss7_data.cfg.mtpRoute[x].flags |= SNGSS7_CONFIGURED;
				} /* if !SNGSS7_CONFIGURED */

				x++;
			} /* while (x < (MAX_MTP_ROUTES+1)) */
		}

		x = 1;
		while (x < (MAX_ISAPS)) {
			/* check if this link has been configured already */
			if ((g_ftdm_sngss7_data.cfg.isap[x].id != 0) &&
				(!(g_ftdm_sngss7_data.cfg.isap[x].flags & SNGSS7_CONFIGURED))) {
				
				if (ftmod_ss7_isup_isap_config(x)) {
					SS7_CRITICAL("ISUP ISAP %d configuration FAILED!\n", x);
					return 1;
				} else {
					SS7_INFO("ISUP ISAP %d configuration DONE!\n", x);
				}

				/* set the SNGSS7_CONFIGURED flag */
				g_ftdm_sngss7_data.cfg.isap[x].flags |= SNGSS7_CONFIGURED;
			} /* if !SNGSS7_CONFIGURED */
			
			x++;
		} /* while (x < (MAX_ISAPS)) */

		if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_ISUP_STARTED)) {
			x = 1;
			while (x < (MAX_ISUP_INFS)) {
				/* check if this link has been configured already */
				if ((g_ftdm_sngss7_data.cfg.isupIntf[x].id != 0) &&
					(!(g_ftdm_sngss7_data.cfg.isupIntf[x].flags & SNGSS7_CONFIGURED))) {
		
					if (ftmod_ss7_isup_intf_config(x)) {
						SS7_CRITICAL("ISUP INTF %d configuration FAILED!\n", x);
						return 1;
					} else {
						SS7_INFO("ISUP INTF %d configuration DONE!\n", x);
						/* set the interface to paused */
						sngss7_set_flag(&g_ftdm_sngss7_data.cfg.isupIntf[x], SNGSS7_PAUSED);
					}
		
					/* set the SNGSS7_CONFIGURED flag */
					g_ftdm_sngss7_data.cfg.isupIntf[x].flags |= SNGSS7_CONFIGURED;
				} /* if !SNGSS7_CONFIGURED */
				
				x++;
			} /* while (x < (MAX_ISUP_INFS)) */
		} /* if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_ISUP)) */

		x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
		while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {

			if (g_ftdm_sngss7_data.cfg.procId > 1) {
				break;
			}

			/* check if this link has been configured already */
			if ((g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) &&
				(!(g_ftdm_sngss7_data.cfg.isupCkt[x].flags & SNGSS7_CONFIGURED))) {

				if (ftmod_ss7_isup_ckt_config(x)) {
					SS7_CRITICAL("ISUP CKT %d configuration FAILED!\n", x);
					return 1;
				} else {
					SS7_INFO("ISUP CKT %d configuration DONE!\n", x);
				}

				/* set the SNGSS7_CONFIGURED flag */
				g_ftdm_sngss7_data.cfg.isupCkt[x].flags |= SNGSS7_CONFIGURED;
			} /* if !SNGSS7_CONFIGURED */
			
			x++;
		} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */
	}

	/* go through all the relays channels and configure it */
	x = 1;
	while (x < (MAX_RELAY_CHANNELS)) {
		/* check if this relay channel has been configured already */
		if ((g_ftdm_sngss7_data.cfg.relay[x].id != 0) &&
			(!(g_ftdm_sngss7_data.cfg.relay[x].flags & SNGSS7_CONFIGURED))) {

			/* send the specific configuration */
			if (ftmod_ss7_relay_chan_config(x)) {
				SS7_CRITICAL("Relay Channel %d configuration FAILED!\n", x);
				return 1;
			} else {
				SS7_INFO("Relay Channel %d configuration DONE!\n", x);
			}

			/* set the SNGSS7_CONFIGURED flag */
			g_ftdm_sngss7_data.cfg.relay[x].flags |= SNGSS7_CONFIGURED;
		} /* if !SNGSS7_CONFIGURED */
		x++;
	} /* while (x < (MAX_RELAY_CHANNELS)) */


	if(SNG_SS7_OPR_MODE_M2UA_SG == g_ftdm_operating_mode){
		return ftmod_ss7_m2ua_cfg();
	}
	
	return 0;
}

/******************************************************************************/
int ftmod_ss7_relay_gen_config(void)
{
	RyMngmt	cfg;	/*configuration structure*/
	Pst		pst;	/*post structure*/
	
	/* initalize the post structure */
	smPstInit(&pst);
	
	/* insert the destination Entity */
	pst.dstEnt = ENTRY;
	
	/* clear the configuration structure */
	memset(&cfg, 0x0, sizeof(RyMngmt));
	
	/* fill in some general sections of the header */
	smHdrInit(&cfg.hdr);

	/* fill in the post structure */
	smPstInit( &cfg.t.cfg.s.ryGenCfg.lmPst );
	
	/*fill in the specific fields of the header */
	cfg.hdr.msgType						= TCFG;
	cfg.hdr.entId.ent 					= ENTRY;
	cfg.hdr.entId.inst					= S_INST;
	cfg.hdr.elmId.elmnt 				= STGEN;

	cfg.t.cfg.s.ryGenCfg.lmPst.srcEnt	= ENTRY;
	cfg.t.cfg.s.ryGenCfg.lmPst.dstEnt	= ENTSM;

	cfg.t.cfg.s.ryGenCfg.nmbChan		= 10;
	cfg.t.cfg.s.ryGenCfg.tmrRes			= RY_PERIOD;
	cfg.t.cfg.s.ryGenCfg.usta			= 1;
	

	return(sng_cfg_relay(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp1_gen_config(void)
{
	L1Mngmt	cfg;	/*configuration structure*/
	Pst		pst;	/*post structure*/
	
	/* initalize the post structure */
	smPstInit(&pst);
	
	/* insert the destination Entity */
	pst.dstEnt = ENTL1;
	
	/* clear the configuration structure */
	memset(&cfg, 0x0, sizeof(L1Mngmt));
	
	/* fill in some general sections of the header */
	smHdrInit(&cfg.hdr);

	/* fill in the post structure */
	smPstInit( &cfg.t.cfg.s.l1Gen.sm );
	
	/*fill in the specific fields of the header */
	cfg.hdr.msgType					= TCFG;
	cfg.hdr.entId.ent 				= ENTL1;
	cfg.hdr.entId.inst				= S_INST;
	cfg.hdr.elmId.elmnt 			= STGEN;

	cfg.t.cfg.s.l1Gen.sm.srcEnt		= ENTL1;
	cfg.t.cfg.s.l1Gen.sm.dstEnt		= ENTSM;
	
	cfg.t.cfg.s.l1Gen.nmbLnks		= MAX_L1_LINKS;
	cfg.t.cfg.s.l1Gen.poolTrUpper	= POOL_UP_TR;		/* upper pool threshold */
	cfg.t.cfg.s.l1Gen.poolTrLower	= POOL_LW_TR;		/* lower pool threshold */

	return(sng_cfg_mtp1(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp2_gen_config(void)
{
	SdMngmt	cfg;
	Pst		pst;
	
	/* initalize the post structure */
	smPstInit(&pst);
	
	/* insert the destination Entity */
	pst.dstEnt = ENTSD;
	
	/* clear the configuration structure */
	memset(&cfg, 0x0, sizeof(SdMngmt));
	
	/* fill in some general sections of the header */
	smHdrInit(&cfg.hdr);

	/* fill in the post structure */
	smPstInit( &cfg.t.cfg.s.sdGen.sm );
	
	/* fill in the specific fields of the header */
	cfg.hdr.msgType					= TCFG;
	cfg.hdr.entId.ent				= ENTSD;
	cfg.hdr.entId.inst				= S_INST;
	cfg.hdr.elmId.elmnt				= STGEN;
	
	cfg.t.cfg.s.sdGen.sm.srcEnt		= ENTSD;
	cfg.t.cfg.s.sdGen.sm.dstEnt		= ENTSM;
	
	cfg.t.cfg.s.sdGen.nmbLnks		= MAX_SD_LINKS;
	cfg.t.cfg.s.sdGen.poolTrUpper	= POOL_UP_TR;
	cfg.t.cfg.s.sdGen.poolTrLower	= POOL_LW_TR;

	return(sng_cfg_mtp2(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp3_gen_config(void)
{
	SnMngmt	cfg;
	Pst		pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SnMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/* fill in the post structure */
	smPstInit(&cfg.t.cfg.s.snGen.sm);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType					= TCFG;
	cfg.hdr.entId.ent 				= ENTSN;
	cfg.hdr.entId.inst				= S_INST;
	cfg.hdr.elmId.elmnt 			= STGEN;

	cfg.t.cfg.s.snGen.sm.srcEnt		= ENTSN;
	cfg.t.cfg.s.snGen.sm.dstEnt		= ENTSM;


	cfg.t.cfg.s.snGen.typeSP		= LSN_TYPE_SP;		/* type of signalling postatic int */
	cfg.t.cfg.s.snGen.spCode1		= 0;				/* our DPC for CCITT version */
#if (SS7_ANS92 || SS7_ANS88 || SS7_ANS96 || SS7_CHINA || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snGen.spCode2		= 0;				/* our DPC for ANSI or CHINA version */
#endif
	cfg.t.cfg.s.snGen.ssfValid		= TRUE;				/* ssf validation required */
	cfg.t.cfg.s.snGen.nmbDLSap		= MAX_SN_LINKS;		/* number of MTP Data Link SAPs */
	cfg.t.cfg.s.snGen.nmbNSap		= MAX_SN_ROUTES;	/* number of Upper Layer Saps */
	cfg.t.cfg.s.snGen.nmbRouts		= MAX_SN_ROUTES;	/* maximum number of routing entries */
	cfg.t.cfg.s.snGen.nmbLnkSets	= MAX_SN_LINKSETS;	/* number of link sets */
	cfg.t.cfg.s.snGen.nmbRteInst	= MAX_SN_ROUTES*16;	/* number of simultaneous Rte instances */
	cfg.t.cfg.s.snGen.cbTimeRes		= SN_CB_PERIOD;		/* link time resolution */
	cfg.t.cfg.s.snGen.spTimeRes		= SN_SP_PERIOD;		/* general time resolution */
	cfg.t.cfg.s.snGen.rteTimeRes	= SN_RTE_PERIOD;	/* route time resolution */
	cfg.t.cfg.s.snGen.extCmbndLnkst	= FALSE;			/* enbale extended combined linkset feature */

#if (defined(LSNV3) || defined(SN_MULTIPLE_NETWORK_RESTART))

#else
	cfg.t.cfg.s.snGen.rstReq		= LSN_NO_RST;		/* restarting procedure required */
	cfg.t.cfg.s.snGen.tfrReq		= FALSE;			/* TFR procedure required or not */
	cfg.t.cfg.s.snGen.tmr.t15.enb	= TRUE;				/* t15 - waiting to start route set congestion test */
	cfg.t.cfg.s.snGen.tmr.t15.val	= 30;
	cfg.t.cfg.s.snGen.tmr.t16.enb	= TRUE;				/* t16 - waiting for route set congestion status update */
	cfg.t.cfg.s.snGen.tmr.t16.val	= 20;
	cfg.t.cfg.s.snGen.tmr.t18.enb	= TRUE;				/* t18 - waiting for links to become available */
	cfg.t.cfg.s.snGen.tmr.t18.val	= 200;
	cfg.t.cfg.s.snGen.tmr.t19.enb	= TRUE;				/* t19 - waiting to receive all traffic restart allowed */
	cfg.t.cfg.s.snGen.tmr.t19.val	= 690;
	cfg.t.cfg.s.snGen.tmr.t21.enb	= TRUE;				/* t21 - waiting to restart traffic routed through adjacent SP */
	cfg.t.cfg.s.snGen.tmr.t21.val	= 650;
# if (SS7_ANS92 || SS7_ANS88 || SS7_ANS96 || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snGen.tmr.t26.enb		= TRUE;				/* t26 - waiting to repeat traffic restart waiting message for ANSI */
	cfg.t.cfg.s.snGen.tmr.t26.val		= 600;
# endif
#endif

#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS96)
	cfg.t.cfg.s.snGen.mopc			= FALSE;
#endif

	return(sng_cfg_mtp3(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_isup_gen_config(void)
{
	SiMngmt	 cfg;
	Pst		 pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSI;

	/* clear the configuration structure */
	memset(&cfg, 0x0, sizeof(SiMngmt));

	/* fill in some general sections of the header */
	smHdrInit(&cfg.hdr);

	/* fill in the post structure */
	smPstInit( &cfg.t.cfg.s.siGen.sm );

	/*fill in the specific fields of the header */
	cfg.hdr.msgType	 					= TCFG;
	cfg.hdr.entId.ent  					= ENTSI;
	cfg.hdr.entId.inst 					= S_INST;
	cfg.hdr.elmId.elmnt 				= STGEN;

	cfg.t.cfg.s.siGen.sm.srcEnt			= ENTSI;
	cfg.t.cfg.s.siGen.sm.dstEnt			= ENTSM;

	cfg.t.cfg.s.siGen.nmbSaps			= MAX_CC_INTERFACE;		/* Number of ISUP Saps */
	cfg.t.cfg.s.siGen.nmbNSaps			= MAX_SN_INTERFACE;		/* Number of Network Saps */
	cfg.t.cfg.s.siGen.nmbCir			= MAX_SI_CIRCUITS;		/* Number of circuits */
	cfg.t.cfg.s.siGen.nmbIntf			= MAX_SI_INTERFACES;	/* Number of interfaces */
	cfg.t.cfg.s.siGen.nmbCirGrp			= MAX_SI_CIR_GRP;		/* Max number of circuit groups */
	cfg.t.cfg.s.siGen.nmbCalRef			= MAX_SI_CALL_REF;		/* Number of Call References */
	cfg.t.cfg.s.siGen.timeRes			= SI_PERIOD;			/* time resolution */
	cfg.t.cfg.s.siGen.sccpSup			= FALSE;				/* SCCP support	*/
	cfg.t.cfg.s.siGen.handleTTBinCC		= FALSE;				/* Flag used for controlling TTB feature */
	cfg.t.cfg.s.siGen.mapCPCandFCI		= TRUE;					/* Flag used for controlling TTB feature */
#if (LSIV3 || LSIV4 || LSIV5)
	cfg.t.cfg.s.siGen.lnkSelOpt			= SI_LINK_SELECTION;	/* link selector option */
#endif  
	cfg.t.cfg.s.siGen.poolTrUpper		= POOL_UP_TR;			/* upper pool threshold */
	cfg.t.cfg.s.siGen.poolTrLower		= POOL_LW_TR;			/* lower pool threshold */
	cfg.t.cfg.s.siGen.cirGrTmr.t18.enb	= TRUE;					/* t18 timer - group blocking sent */
	cfg.t.cfg.s.siGen.cirGrTmr.t18.val	= 300;
	cfg.t.cfg.s.siGen.cirGrTmr.t19.enb	= TRUE;					/* t19 timer - initial group blocking sent */
	cfg.t.cfg.s.siGen.cirGrTmr.t19.val	= 3000;
	cfg.t.cfg.s.siGen.cirGrTmr.t20.enb	= TRUE;					/* t20 timer - group unblocking sent */
	cfg.t.cfg.s.siGen.cirGrTmr.t20.val	= 300;
	cfg.t.cfg.s.siGen.cirGrTmr.t21.enb	= TRUE;					/* t21 timer - initial grp unblocking sent */
	cfg.t.cfg.s.siGen.cirGrTmr.t21.val	= 3000;
	cfg.t.cfg.s.siGen.cirGrTmr.t22.enb	= TRUE;					/* t22 timer - group reset sent */
	cfg.t.cfg.s.siGen.cirGrTmr.t22.val	= 300;
	cfg.t.cfg.s.siGen.cirGrTmr.t23.enb	= TRUE;					/* t23 timer - initial group reset sent	*/
	cfg.t.cfg.s.siGen.cirGrTmr.t23.val	= 3000;
#ifndef SS7_UK
	cfg.t.cfg.s.siGen.cirGrTmr.t28.enb 	= TRUE;					/* t28 timer - circuit group query sent */
	cfg.t.cfg.s.siGen.cirGrTmr.t28.val 	= 100;
#endif
#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	cfg.t.cfg.s.siGen.cirGrTmr.tFGR.enb = TRUE;					/* first group received timer */
	cfg.t.cfg.s.siGen.cirGrTmr.tFGR.val = 50;
#endif
#if CGPN_CHK
	cfg.t.cfg.s.siGen.cgPtyNumGenCfg	= TRUE;					/* Calling party number general config flag */
#endif
#ifdef SI_SUPPRESS_CFN
	cfg.t.cfg.s.siGen.suppressCfn		= TRUE;					/* Flag used for 'suppress CFN' feature */
#endif

	return(sng_cfg_isup(&pst, &cfg));

}

/******************************************************************************/
int ftmod_ss7_cc_gen_config(void)
{
	CcMngmt	cfg;
	Pst		 pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTCC;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(CcMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/* fill in the post structure */
	smPstInit( &cfg.t.cfg.s.ccGen.sm );

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType	 				= TCFG;
	cfg.hdr.entId.ent  				= ENTCC;
	cfg.hdr.entId.inst 				= S_INST;
	cfg.hdr.elmId.elmnt 			= STGEN;

	cfg.t.cfg.s.ccGen.sm.srcEnt		= ENTCC;
	cfg.t.cfg.s.ccGen.sm.dstEnt		= ENTSM;

	cfg.t.cfg.s.ccGen.poolTrUpper	= POOL_UP_TR;		/* upper pool threshold */
	cfg.t.cfg.s.ccGen.poolTrLower	= POOL_LW_TR;		/* lower pool threshold */

	return(sng_cfg_cc(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp1_psap_config(int id)
{
	L1Mngmt			cfg;
	Pst				pst;
	sng_mtp1_link_t	*k = &g_ftdm_sngss7_data.cfg.mtp1Link[id];
	
	/* initalize the post structure */
	smPstInit(&pst);
	
	/* insert the destination Entity */
	pst.dstEnt = ENTL1;
	
	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(L1Mngmt));
	
	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);
	
	/*fill in the specific fields of the header*/
	cfg.hdr.msgType				= TCFG;
	cfg.hdr.entId.ent 			= ENTL1;
	cfg.hdr.entId.inst			= S_INST;
	cfg.hdr.elmId.elmnt 		= STPSAP;
	
	cfg.hdr.elmId.elmntInst1	= k->id;
	
	cfg.t.cfg.s.l1PSAP.span		= k->span;
	cfg.t.cfg.s.l1PSAP.chan		= k->chan;
	cfg.t.cfg.s.l1PSAP.spId		= k->id;

	return(sng_cfg_mtp1(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp2_dlsap_config(int id)
{
	SdMngmt	cfg;
	Pst		pst;
	sng_mtp2_link_t	*k = &g_ftdm_sngss7_data.cfg.mtp2Link[id];

	/* initalize the post structure */
	smPstInit( &pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSD;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SdMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType						= TCFG;
	cfg.hdr.entId.ent 					= ENTSD;
	cfg.hdr.entId.inst					= S_INST;
	cfg.hdr.elmId.elmnt 				= STDLSAP;

	cfg.hdr.elmId.elmntInst1 			= k->id;

	cfg.t.cfg.s.sdDLSAP.mem.region		= S_REG;					/* memory region */
	cfg.t.cfg.s.sdDLSAP.mem.pool		= S_POOL;					/* memory pool */
	cfg.t.cfg.s.sdDLSAP.swtch			= k->linkType;			/* protocol type */
	cfg.t.cfg.s.sdDLSAP.priorDl			= PRIOR0;					/* priority for data link layer */
	cfg.t.cfg.s.sdDLSAP.routeDl			= RTESPEC;					/* route for data link layer */
	cfg.t.cfg.s.sdDLSAP.selectorDl		= 0;						/* upper interface selector */
	if (k->mtp1ProcId > 0) {
		cfg.t.cfg.s.sdDLSAP.dstProcId	= k->mtp1ProcId;		/* the procid of MAC/L1/MTP1 */
	} else {
		cfg.t.cfg.s.sdDLSAP.dstProcId	= SFndProcId();				/* the procid of MAC/L1/MTP1 */
	}
	cfg.t.cfg.s.sdDLSAP.dstProcId		= SFndProcId();				/* the procid of MAC/L1/MTP1 */
	cfg.t.cfg.s.sdDLSAP.entMac			= ENTL1;					/* entity for MAC */
	cfg.t.cfg.s.sdDLSAP.instMac			= S_INST;					/* instance for MAC */
	cfg.t.cfg.s.sdDLSAP.priorMac		= PRIOR0;					/* priority for MAC layer */
	cfg.t.cfg.s.sdDLSAP.routeMac		= RTESPEC;					/* route for MAC layer */
	cfg.t.cfg.s.sdDLSAP.selectorMac		= 0;						/* lower interface selector */
	cfg.t.cfg.s.sdDLSAP.memMac.region	= S_REG;					/* memory region and pool id for MAC */
	cfg.t.cfg.s.sdDLSAP.memMac.pool		= S_POOL;
	cfg.t.cfg.s.sdDLSAP.maxOutsFrms		= MAX_SD_OUTSTANDING;		/* maximum outstanding frames */
	cfg.t.cfg.s.sdDLSAP.errType			= k->errorType;
	cfg.t.cfg.s.sdDLSAP.t1.enb			= TRUE;						/* timer 1 - Alignment Ready Timer */
	cfg.t.cfg.s.sdDLSAP.t1.val			= k->t1;
	cfg.t.cfg.s.sdDLSAP.t2.enb			= TRUE;						/* timer 2 - Not Aligned Timer */
	cfg.t.cfg.s.sdDLSAP.t2.val			= k->t2;
	cfg.t.cfg.s.sdDLSAP.t3.enb			= TRUE;						/* timer 3 - Aligned Timer */
	cfg.t.cfg.s.sdDLSAP.t3.val			= k->t3;
	cfg.t.cfg.s.sdDLSAP.t5.enb			= TRUE;						/* timer 5 - Sending SIB timer */
	cfg.t.cfg.s.sdDLSAP.t5.val			= k->t5;
	cfg.t.cfg.s.sdDLSAP.t6.enb			= TRUE;						/* timer 6 - Remote Congestion Timer */
	cfg.t.cfg.s.sdDLSAP.t6.val			= k->t6;
	cfg.t.cfg.s.sdDLSAP.t7.enb			= TRUE;						/* timer 7 - Excessive delay of acknowledgement timer */
	cfg.t.cfg.s.sdDLSAP.t7.val			= k->t7;
	cfg.t.cfg.s.sdDLSAP.provEmrgcy		= k->t4e;				/* emergency proving period */
	cfg.t.cfg.s.sdDLSAP.provNormal		= k->t4n;				/* normal proving period */
	cfg.t.cfg.s.sdDLSAP.lssuLen			= k->lssuLength;			/* one or two byte LSSU length */
	cfg.t.cfg.s.sdDLSAP.maxFrmLen		= MAX_SD_FRAME_LEN;			/* max frame length for MSU */
	cfg.t.cfg.s.sdDLSAP.congDisc		= FALSE;					/* congestion discard TRUE or FALSE */
	cfg.t.cfg.s.sdDLSAP.sdT				= MAX_SD_SUERM;				/* SUERM error rate threshold */
	cfg.t.cfg.s.sdDLSAP.sdTie			= MAX_SD_AERM_EMERGENCY;	/* AERM emergency error rate threshold */
	cfg.t.cfg.s.sdDLSAP.sdTin			= MAX_SD_AERM_NORMAL;		/* AERM normal error rate threshold */
	cfg.t.cfg.s.sdDLSAP.sdN1			= MAX_SD_MSU_RETRANS;		/* maximum number of MSUs for retransmission */
	cfg.t.cfg.s.sdDLSAP.sdN2			= MAX_SD_OCTETS_RETRANS;	/* maximum number of MSU octets for retrans */
	cfg.t.cfg.s.sdDLSAP.sdCp			= MAX_SD_ALIGN_ATTEMPTS;	/* maximum number of alignment attempts */
	cfg.t.cfg.s.sdDLSAP.spIdSE			= k->mtp1Id;				/* service provider id */
	cfg.t.cfg.s.sdDLSAP.sdtFlcStartTr	= 256;						/* SDT interface flow control start thresh */
	cfg.t.cfg.s.sdDLSAP.sdtFlcEndTr		= 512;						/* SDT interface flow control end thresh */

#ifdef SD_HSL
	cfg.t.cfg.s.sdDLSAP.sapType			=;							/* Indcates whether link is HSL or LSL */
	cfg.t.cfg.s.sdDLSAP.sapFormat		=;							/* The extened sequence no to be used or not */
	cfg.t.cfg.s.sdDLSAP.t8.enb			=;							/* timer 8 configuration structure */
	cfg.t.cfg.s.sdDLSAP.sdTe			=;							/* EIM threshold */
	cfg.t.cfg.s.sdDLSAP.sdUe			=;							/* increment constant */
	cfg.t.cfg.s.sdDLSAP.sdDe			=;							/* decrement constant */
#endif /* HIGH_SPEED_SIGNALING_SUPPORT */

#if (SS7_TTC || SS7_NTT)
	cfg.t.cfg.s.sdDLSAP.numRtb			=;							/* outstanding number of messages in RTB */
	cfg.t.cfg.s.sdDLSAP.tf				=;							/* FISU transmission interval */
	cfg.t.cfg.s.sdDLSAP.tfv				=;							/* FISU transmission interval during verification */
	cfg.t.cfg.s.sdDLSAP.to				=;							/* SIO transmission interval */
	cfg.t.cfg.s.sdDLSAP.ta				=;							/* SIE transmission interval */
	cfg.t.cfg.s.sdDLSAP.ts				=;							/* SIOS transmission interval */
	cfg.t.cfg.s.sdDLSAP.tso				=;							/* SIOS transmission duration when out of service */
	cfg.t.cfg.s.sdDLSAP.te				=;							/* SU normalization time */
#endif /* (SS7_TTC || SS7_NTT) */

#if (SS7_NTT)		/* NTTT - Q.703 */
	cfg.t.cfg.s.sdDLSAP.repMsuNack		=;							/* Nack on receipt of repeated MSU */
	cfg.t.cfg.s.sdDLSAP.invFibIgnore	=;							/* invalid FIB ignore or bring the link down */
	cfg.t.cfg.s.sdDLSAP.invBsnIgnore	=;							/* invalid BSN ignore or bring the link down */
	cfg.t.cfg.s.sdDLSAP.congAbatOnNack	=;							/* congestion abatement on nack or only on acks */
#endif /* (SS7_NTT) */

#ifdef TDS_ROLL_UPGRADE_SUPPORT 
	cfg.t.cfg.s.sdDLSAP.hlremIntfValid	= FALSE;					/* Upper Sap Version number valid ? */
	cfg.t.cfg.s.sdDLSAP.remIntfVer		= SDTIFVER;					/* remote version info */
#endif /*RUG*/

	return(sng_cfg_mtp2(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp3_dlsap_config(int id)
{
	Pst				pst;
	SnMngmt			cfg;
	sng_mtp3_link_t	*k = &g_ftdm_sngss7_data.cfg.mtp3Link[id];


	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SnMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType						= TCFG;
	cfg.hdr.entId.ent 					= ENTSN;
	cfg.hdr.entId.inst					= S_INST;
	cfg.hdr.elmId.elmnt 				= STDLSAP;

	cfg.hdr.elmId.elmntInst1 			= k->id;

	cfg.t.cfg.s.snDLSAP.lnkSetId		= k->linkSetId;			/* link set ID */
	cfg.t.cfg.s.snDLSAP.opc				= k->spc;				/* Originating Postatic int Code */
	cfg.t.cfg.s.snDLSAP.adjDpc			= k->apc;				/* Adlacent Destination Postatic int Code */
	cfg.t.cfg.s.snDLSAP.lnkPrior		= 0;					/* link priority within the link set */
	cfg.t.cfg.s.snDLSAP.msgSize			= MAX_SN_MSG_SIZE;		/* message length */
	cfg.t.cfg.s.snDLSAP.msgPrior		= 0;					/* management message priority */
	cfg.t.cfg.s.snDLSAP.lnkType			= k->linkType;			/* link type ANSI, ITU, BICI or CHINA */
	cfg.t.cfg.s.snDLSAP.upSwtch			= k->switchType;		/* user part switch type */
	cfg.t.cfg.s.snDLSAP.maxSLTtry		= MAX_SLTM_RETRIES;		/* maximun times to retry SLTM */
	cfg.t.cfg.s.snDLSAP.p0QLen			= 32;					/* size of the priority 0 Q */
	cfg.t.cfg.s.snDLSAP.p1QLen			= 32;					/* size of the priority 1 Q */
	cfg.t.cfg.s.snDLSAP.p2QLen			= 32;					/* size of the priority 2 Q */
	cfg.t.cfg.s.snDLSAP.p3QLen			= 32;					/* size of the priority 3 Q */
	cfg.t.cfg.s.snDLSAP.discPrior		= 0;					/* discard priority */
#ifndef SDT2
	cfg.t.cfg.s.snDLSAP.maxCredit		= MAX_SN_CREDIT;		/* max credit */
#endif /* SDT2 */
	cfg.t.cfg.s.snDLSAP.lnkId			= 0;					/* signalling link allocation procedure identity */
	cfg.t.cfg.s.snDLSAP.lnkTstSLC		= k->slc;				/* link selection code for link test */
	cfg.t.cfg.s.snDLSAP.tstLen			= 7;					/* link test pattern length */
	cfg.t.cfg.s.snDLSAP.tst[0]			= 'S';					/* link test pattern */
	cfg.t.cfg.s.snDLSAP.tst[1]			= 'A';					/* link test pattern */
	cfg.t.cfg.s.snDLSAP.tst[2]			= 'N';					/* link test pattern */
	cfg.t.cfg.s.snDLSAP.tst[3]			= 'G';					/* link test pattern */
	cfg.t.cfg.s.snDLSAP.tst[4]			= 'O';					/* link test pattern */
	cfg.t.cfg.s.snDLSAP.tst[5]			= 'M';					/* link test pattern */
	cfg.t.cfg.s.snDLSAP.tst[6]			= 'A';					/* link test pattern */
	cfg.t.cfg.s.snDLSAP.ssf				= k->ssf;				/* sub service field */ 
	cfg.t.cfg.s.snDLSAP.dstProcId		= k->mtp2ProcId;		/* destination processor id */
	cfg.t.cfg.s.snDLSAP.dstEnt			= ENTSD;				/* entity */
	cfg.t.cfg.s.snDLSAP.dstInst			= S_INST;				/* instance */
	cfg.t.cfg.s.snDLSAP.prior			= PRIOR0;				/* priority */
	cfg.t.cfg.s.snDLSAP.route			= RTESPEC;				/* route */
	cfg.t.cfg.s.snDLSAP.selector		= 0;					/* lower layer selector */
	cfg.t.cfg.s.snDLSAP.mem.region		= S_REG;				/* memory region id */
	cfg.t.cfg.s.snDLSAP.mem.pool		= S_POOL;				/* memory pool id */
	cfg.t.cfg.s.snDLSAP.spId			= k->mtp2Id;			/* service provider id */

	switch (k->linkType) {
	/**************************************************************************/
	case (LSN_SW_ANS):
	case (LSN_SW_ANS96):
	case (LSN_SW_CHINA):
		cfg.t.cfg.s.snDLSAP.dpcLen		= DPC24;				/* dpc length 24 bits */
		cfg.t.cfg.s.snDLSAP.l2Type		= LSN_MTP2_56KBPS;		/* layer 2 type - 56kbps MTP2 link, 1.536Mbps MTP2 link or QSAAL link */
		cfg.t.cfg.s.snDLSAP.isCLink		= FALSE;				/* identifies if the link is a C type link.Required to check if sls has to be rotated.*/
		break;
	/**************************************************************************/
	case (LSN_SW_ITU):
		cfg.t.cfg.s.snDLSAP.dpcLen		= DPC14;				/* dpc length 14 bits */
		break;
	/**************************************************************************/
	default:
		cfg.t.cfg.s.snDLSAP.dpcLen		= DPC14;				/* dpc length 14 bits */
		break;
	/**************************************************************************/
	} /* switch (k->linkType) */

	switch (k->linkType) {
	/**************************************************************************/
	case (LSN_SW_ANS):
	case (LSN_SW_ANS96):
		cfg.t.cfg.s.snDLSAP.flushContFlag	= TRUE;			/* flush continue handling */
		break;
	/**************************************************************************/
	case (LSN_SW_ITU):
	case (LSN_SW_CHINA):
		cfg.t.cfg.s.snDLSAP.flushContFlag	= FALSE;			/* flush continue handling */
		break;
	/**************************************************************************/
	default:
		cfg.t.cfg.s.snDLSAP.flushContFlag	= FALSE;			/* flush continue handling */
		break;
	/**************************************************************************/
	} /* switch (k->linkType) */

	cfg.t.cfg.s.snDLSAP.tmr.t1.enb		= TRUE;					/* t1 - delay to avoid missequencing on changeover */
	cfg.t.cfg.s.snDLSAP.tmr.t1.val		= k->t1;
	cfg.t.cfg.s.snDLSAP.tmr.t2.enb		= TRUE;					/* t2 - waiting for changeover ack */
	cfg.t.cfg.s.snDLSAP.tmr.t2.val		= k->t2;
	cfg.t.cfg.s.snDLSAP.tmr.t3.enb		= TRUE;					/* t3 - delay to avoid missequencing on changeback */
	cfg.t.cfg.s.snDLSAP.tmr.t3.val		= k->t3;
	cfg.t.cfg.s.snDLSAP.tmr.t4.enb		= TRUE;					/* t4 - waiting for first changeback ack */
	cfg.t.cfg.s.snDLSAP.tmr.t4.val		= k->t4;
	cfg.t.cfg.s.snDLSAP.tmr.t5.enb		= TRUE;					/* t5 - waiting for second changeback ack */
	cfg.t.cfg.s.snDLSAP.tmr.t5.val		= k->t5;
	cfg.t.cfg.s.snDLSAP.tmr.t7.enb		= TRUE;					/* t7 - waiting for link connection ack */
	cfg.t.cfg.s.snDLSAP.tmr.t7.val		= k->t7;
	cfg.t.cfg.s.snDLSAP.tmr.t12.enb		= TRUE;					/* t12 - waiting for uninhibit ack */
	cfg.t.cfg.s.snDLSAP.tmr.t12.val		= k->t12;
	cfg.t.cfg.s.snDLSAP.tmr.t13.enb		= TRUE;					/* t13 - waiting for forced uninhibit */
	cfg.t.cfg.s.snDLSAP.tmr.t13.val		= k->t13;
	cfg.t.cfg.s.snDLSAP.tmr.t14.enb		= TRUE;					/* t14 - waiting for inhibition ack */
	cfg.t.cfg.s.snDLSAP.tmr.t14.val		= k->t14;
	cfg.t.cfg.s.snDLSAP.tmr.t17.enb		= TRUE;					/* t17 - delay to avoid oscillation of initial alignment failure */
	cfg.t.cfg.s.snDLSAP.tmr.t17.val		= k->t17;
	cfg.t.cfg.s.snDLSAP.tmr.t22.enb		= TRUE;					/* t22 - local inhibit test timer */
	cfg.t.cfg.s.snDLSAP.tmr.t22.val		= k->t22;
	cfg.t.cfg.s.snDLSAP.tmr.t23.enb		= TRUE;					/* t23 - remote inhibit test timer */
	cfg.t.cfg.s.snDLSAP.tmr.t23.val		= k->t23;
	cfg.t.cfg.s.snDLSAP.tmr.t24.enb		= TRUE;					/* t24 - stabilizing timer */
	cfg.t.cfg.s.snDLSAP.tmr.t24.val		= k->t24;
	cfg.t.cfg.s.snDLSAP.tmr.t31.enb		= TRUE;					/* t31 - BSN requested timer */
	cfg.t.cfg.s.snDLSAP.tmr.t31.val		= k->t31;
	cfg.t.cfg.s.snDLSAP.tmr.t32.enb		= TRUE;					/* t32 - SLT timer */
	cfg.t.cfg.s.snDLSAP.tmr.t32.val		= k->t32;
	cfg.t.cfg.s.snDLSAP.tmr.t33.enb		= TRUE;					/* t33 - connecting timer */
	cfg.t.cfg.s.snDLSAP.tmr.t33.val		= k->t33;
	cfg.t.cfg.s.snDLSAP.tmr.t34.enb		= TRUE;					/* t34 - periodic signalling link test timer */
	cfg.t.cfg.s.snDLSAP.tmr.t34.val		= k->t34;
#if (SS7_ANS92 || SS7_ANS88 || SS7_ANS96 || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snDLSAP.tmr.t35.enb		= TRUE;					/* t35 - false link congestion timer, same as t31 of ANSI'96*/
	cfg.t.cfg.s.snDLSAP.tmr.t35.val		= k->t35;
	cfg.t.cfg.s.snDLSAP.tmr.t36.enb		= TRUE;					/* t36 - false link congestion timer, same as t33 of ANSI'96*/
	cfg.t.cfg.s.snDLSAP.tmr.t36.val		= k->t36;	
	cfg.t.cfg.s.snDLSAP.tmr.t37.enb		= TRUE;					/* t37 - false link congestion timer, same as t34 of ANSI'96*/
	cfg.t.cfg.s.snDLSAP.tmr.t37.val		= k->t37;
	cfg.t.cfg.s.snDLSAP.tmr.tCraft.enb	= TRUE;					/* link referral craft timer - T19 in ANSI */
	cfg.t.cfg.s.snDLSAP.tmr.tCraft.val	= k->tcraft;
#endif
#ifdef SDT2
	cfg.t.cfg.s.snDLSAP.tmr.tFlc.enb	= TRUE;					/* flow control timer */
	cfg.t.cfg.s.snDLSAP.tmr.tFlc.val	= 300;
	cfg.t.cfg.s.snDLSAP.tmr.tBnd.enb	= TRUE;					/* bind request timer */
	cfg.t.cfg.s.snDLSAP.tmr.tBnd.val	= 20;
#endif /* SDT2 */
#ifdef TDS_ROLL_UPGRADE_SUPPORT
	cfg.t.cfg.s.snDLSAP.remIntfValid	= FALSE;				/* remote interface version is valid */
	cfg.t.cfg.s.snDLSAP.remIntfVer		= SNTIFVER;				/* remote interface version */
#endif

	return(sng_cfg_mtp3(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp3_nsap_config(int id)
{
	Pst			pst;
	SnMngmt		cfg;
	sng_nsap_t	*k = &g_ftdm_sngss7_data.cfg.nsap[id];

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SnMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType					= TCFG;
	cfg.hdr.entId.ent 				= ENTSN;
	cfg.hdr.entId.inst				= S_INST;
	cfg.hdr.elmId.elmnt 			= STNSAP;

	cfg.hdr.elmId.elmntInst1 		= k->spId;

	cfg.t.cfg.s.snNSAP.ssf			= k->ssf;			/* sub service field */
	cfg.t.cfg.s.snNSAP.lnkType		= k->linkType;		/* link type -ANSI, ITU, CHINA or BICI */
	cfg.t.cfg.s.snNSAP.upSwtch		= k->switchType;	/* user part switch type */
	cfg.t.cfg.s.snNSAP.selector		= 0;				/* upper layer selector */
	cfg.t.cfg.s.snNSAP.mem.region	= S_REG;			/* memory region id */
	cfg.t.cfg.s.snNSAP.mem.pool		= S_POOL;			/* memory pool id */
	cfg.t.cfg.s.snNSAP.prior		= PRIOR0;			/* priority */
	cfg.t.cfg.s.snNSAP.route		= RTESPEC;			/* route */
#if( SS7_ANS92 || SS7_ANS88 || SS7_ANS96 || SS7_CHINA )
	cfg.t.cfg.s.snNSAP.dpcLen		= DPC24;			/* dpc length 14 or 24 bits */
#else
	cfg.t.cfg.s.snNSAP.dpcLen		= DPC14;			/* dpc length 14 or 24 bits */
#endif
#if (defined(SN_SG) || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snNSAP.usrParts		= ;					/* user parts configured on self postatic int code on IP side */ 
#endif
#ifdef TDS_ROLL_UPGRADE_SUPPORT
	cfg.t.cfg.s.snNSAP.remIntfValid	= FALSE;			/* remote interface version is valid */
	cfg.t.cfg.s.snNSAP.remIntfVer	= SNTIFVER;			/* remote interface version */
#endif

	return(sng_cfg_mtp3(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp3_linkset_config(int id)
{
	Pst				pst;
	SnMngmt			cfg;
	int				c;
	sng_link_set_t	*k = &g_ftdm_sngss7_data.cfg.mtpLinkSet[id];

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SnMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType	 					= TCFG;
	cfg.hdr.entId.ent  					= ENTSN;
	cfg.hdr.entId.inst 					= S_INST;
	cfg.hdr.elmId.elmnt 				= STLNKSET;		 /* link set */

	cfg.hdr.elmId.elmntInst1 			= k->id;

	cfg.t.cfg.s.snLnkSet.lnkSetId		= k->id;			/* link set ID */
	cfg.t.cfg.s.snLnkSet.lnkSetType		= k->linkType;		/* link type */
	cfg.t.cfg.s.snLnkSet.adjDpc			= k->apc;			/* adjacent DPC */
	cfg.t.cfg.s.snLnkSet.nmbActLnkReqd	= k->minActive;		/* minimum number of active links */
	cfg.t.cfg.s.snLnkSet.nmbCmbLnkSet	= k->numLinks;				/* number of combined link sets */
	for(c = 0; c < k->numLinks;c++) {
		cfg.t.cfg.s.snLnkSet.cmbLnkSet[c].cmbLnkSetId = k->links[c];
		cfg.t.cfg.s.snLnkSet.cmbLnkSet[c].lnkSetPrior = 0;
	}


	return(sng_cfg_mtp3(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_mtp3_route_config(int id)
{
	Pst			pst;
	SnMngmt		cfg;
	sng_route_t	*k = &g_ftdm_sngss7_data.cfg.mtpRoute[id];

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SnMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType	 					= TCFG;
	cfg.hdr.entId.ent  					= ENTSN;
	cfg.hdr.entId.inst 					= S_INST;
	cfg.hdr.elmId.elmnt 				= STROUT;

	cfg.hdr.elmId.elmntInst1 			= k->id;

	cfg.t.cfg.s.snRout.dpc				= k->dpc;					/* destination postatic int code */
	cfg.t.cfg.s.snRout.spType			= LSN_TYPE_SP;				/* signalling postatic int type */
	cfg.t.cfg.s.snRout.swtchType		= k->linkType;				/* switch type */
	cfg.t.cfg.s.snRout.upSwtch			= k->switchType;			/* user part switch type */
	cfg.t.cfg.s.snRout.cmbLnkSetId		= k->cmbLinkSetId;			/* combined link set ID */
	if (k->dir == SNG_RTE_UP) {
		cfg.t.cfg.s.snRout.dir		 	= LSN_RTE_UP;				/* direction */
	} else {
		cfg.t.cfg.s.snRout.dir		 	= LSN_RTE_DN;				/* direction */
	}
	cfg.t.cfg.s.snRout.rteToAdjSp		= 0;						/* flag indicating this route to adjacent SP */ 
	cfg.t.cfg.s.snRout.ssf				= k->ssf;					/* sub service field */
	cfg.t.cfg.s.snRout.brdcastFlg		= TRUE;						/* flag indicating to have TFP broadcast */
	if (cfg.t.cfg.s.snRout.swtchType == LSN_SW_ITU) {
		cfg.t.cfg.s.snRout.rstReq		= LSN_ITU92_RST;			/* Restart type supported by the node */
	} else if ((cfg.t.cfg.s.snRout.swtchType == LSN_SW_ANS) ||
			   (cfg.t.cfg.s.snRout.swtchType == LSN_SW_ANS96)) {
		cfg.t.cfg.s.snRout.rstReq		= LSN_ANS_RST;			/* Restart type supported by the node */
	} else {
		cfg.t.cfg.s.snRout.rstReq		= LSN_NO_RST;			/* Restart type supported by the node */
	}
	if ((cfg.t.cfg.s.snRout.swtchType == LSN_SW_ITU) || 
		(cfg.t.cfg.s.snRout.swtchType == LSN_SW_CHINA) || 
		(cfg.t.cfg.s.snRout.swtchType == LSN_SW_BICI)) {
		cfg.t.cfg.s.snRout.slsRange		= LSN_ITU_SLS_RANGE;		/* max value of SLS for this DPC */
	} else {
		cfg.t.cfg.s.snRout.slsRange		= LSN_ANSI_5BIT_SLS_RANGE;	/* max value of SLS for this DPC */
	}
	cfg.t.cfg.s.snRout.lsetSel			= 0x1;						/* linkset selection bit in SLS for STP */
	cfg.t.cfg.s.snRout.multiMsgPrior	= TRUE;					/* TRUE if multiple cong priorities of messages */
	cfg.t.cfg.s.snRout.rctReq			= TRUE;					/* route set congestion test required or not */
	cfg.t.cfg.s.snRout.slsLnk			= FALSE;
#ifdef LSNV2
# if (SS7_NTT || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snRout.destSpec			=;							/* destination specfication A or B*/ 
# endif  
#endif  
#if (defined(LSNV3) || defined(SN_MULTIPLE_NETWORK_RESTART))
	cfg.t.cfg.s.snRout.tfrReq			=;							/* TFR procedure required or not */
#endif
	cfg.t.cfg.s.snRout.tmr.t6.enb		= TRUE;
	cfg.t.cfg.s.snRout.tmr.t6.val		= k->t6;
	cfg.t.cfg.s.snRout.tmr.t8.enb		= TRUE;						/* t8 - transfer prohibited inhibition timer */
	cfg.t.cfg.s.snRout.tmr.t8.val		= k->t8;
	cfg.t.cfg.s.snRout.tmr.t10.enb		= TRUE;						/* t10 - waiting to repeat route set test */
	cfg.t.cfg.s.snRout.tmr.t10.val		= k->t10;
	cfg.t.cfg.s.snRout.tmr.t11.enb		= TRUE;						/* t11 - transfer restrict timer */
	cfg.t.cfg.s.snRout.tmr.t11.val		= k->t11;
	cfg.t.cfg.s.snRout.tmr.t19.enb		= TRUE;						/* t19 - TRA sent timer */
	cfg.t.cfg.s.snRout.tmr.t19.val		= k->t19;
	cfg.t.cfg.s.snRout.tmr.t21.enb		= TRUE;						/* t21 - waiting to restart traffic routed through adjacent SP */
	cfg.t.cfg.s.snRout.tmr.t21.val		= k->t21;

#if (defined(LSNV3) || defined(SN_MULTIPLE_NETWORK_RESTART))
	cfg.t.cfg.s.snRout.tmr.t15.enb		= TRUE;						/* t15 - waiting to start route set congestion test */
	cfg.t.cfg.s.snRout.tmr.t15.val		= k->t15;
	cfg.t.cfg.s.snRout.tmr.t16.enb		= TRUE;						/* t16 - waiting for route set congestion status update */
	cfg.t.cfg.s.snRout.tmr.t16.val		= k->t16;
	cfg.t.cfg.s.snRout.tmr.t18.enb		= TRUE;						/* t18 - transfer prohibited inhibition timer */
	cfg.t.cfg.s.snRout.tmr.t18.val		= k->t18;
# if (SS7_ANS92 || SS7_ANS88 || SS7_ANS96 || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snRout.tmr.t25.enb		= TRUE;						/* t25 - waiting to traffic resatrt allowed message for ANSI */
	cfg.t.cfg.s.snRout.tmr.t25.val		= k->t25;
	cfg.t.cfg.s.snRout.tmr.t26.enb		= TRUE;				 		/* t26 - waiting to repeat traffic restart waiting message for ANSI */
	cfg.t.cfg.s.snRout.tmr.t26.val		= k->t26;
# endif
#endif
#if (SS7_TTC || SS7_NTT || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snRout.tmr.tc.enb		= TRUE;						/* tc - Waiting for congestion abatement */
	cfg.t.cfg.s.snRout.tmr.tc.val		= k->tc;
#endif
#if (defined(SN_SG) || defined(TDS_ROLL_UPGRADE_SUPPORT))
	cfg.t.cfg.s.snRout.tmr.tQry.enb		= TRUE;						/* Periodic query timer over the NIF */
	cfg.t.cfg.s.snRout.tmr.tQry.val		= k->tqry;
#endif

	return(sng_cfg_mtp3(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_isup_nsap_config(int id)
{
	SiMngmt	 cfg;
	Pst		 pst;
	sng_nsap_t	*k = &g_ftdm_sngss7_data.cfg.nsap[id];

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSI;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SiMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType					= TCFG;
	cfg.hdr.entId.ent				= ENTSI;
	cfg.hdr.entId.inst				= S_INST;
	cfg.hdr.elmId.elmnt 			= STNSAP;

	cfg.hdr.elmId.elmntInst1 		= k->id;

#if (SI_LMINT3 || SMSI_LMINT3)
	cfg.t.cfg.s.siNSap.nsapId		= k->id;			/* Id of the NSAP being configured */
#endif
	cfg.t.cfg.s.siNSap.nwId			= k->nwId;			/* Network Id */
	cfg.t.cfg.s.siNSap.spId			= k->spId;			/* service providor id */
	cfg.t.cfg.s.siNSap.ssf			= k->ssf;			/* sub service field */
	cfg.t.cfg.s.siNSap.dstEnt		= ENTSN;			/* entity */
	cfg.t.cfg.s.siNSap.dstInst		= S_INST;			/* instance */
	cfg.t.cfg.s.siNSap.prior		= PRIOR0;			/* priority */
	cfg.t.cfg.s.siNSap.route		= RTESPEC;			/* route */
	cfg.t.cfg.s.siNSap.dstProcId	= SFndProcId();		/* destination processor id */
	cfg.t.cfg.s.siNSap.sapType		= SAP_MTP;			/* sap type */
	cfg.t.cfg.s.siNSap.selector		= 0;				/* selector */
	cfg.t.cfg.s.siNSap.tINT.enb		= TRUE;				/* interface (Bind Confirm) timer */
	cfg.t.cfg.s.siNSap.tINT.val		= 50;
	cfg.t.cfg.s.siNSap.mem.region	= S_REG;			/* memory region & pool id */
	cfg.t.cfg.s.siNSap.mem.pool		= S_POOL;

#ifdef TDS_ROLL_UPGRADE_SUPPORT
	cfg.t.cfg.s.siNSap.remIntfValid = FALSE;	/* remote interface version is valid */
	cfg.t.cfg.s.siNSap.remIntfVer;			 /* remote interface version */
#endif

	return(sng_cfg_isup(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_isup_intf_config(int id)
{
	SiMngmt	 cfg;
	Pst		 pst;
	sng_isup_inf_t *k = &g_ftdm_sngss7_data.cfg.isupIntf[id];

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSI;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SiMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType	 							= TCFG;
	cfg.hdr.entId.ent  							= ENTSI;
	cfg.hdr.entId.inst 							= S_INST;
	cfg.hdr.elmId.elmnt 						= SI_STINTF;

	cfg.hdr.elmId.elmntInst1 					= k->id;

	cfg.t.cfg.s.siIntfCb.intfId					= k->id;				/* Interface id */
	cfg.t.cfg.s.siIntfCb.nwId					= k->nwId;				/* Network Id */
	cfg.t.cfg.s.siIntfCb.sapId					= k->isap;				/* Id of the Upper ISUP SAP */
	cfg.t.cfg.s.siIntfCb.opc					= k->spc;				/* physical originating postatic int code */
	cfg.t.cfg.s.siIntfCb.phyDpc					= k->dpc;				/* physical destination postatic int code */
	cfg.t.cfg.s.siIntfCb.swtch					= k->switchType;		/* Protocol Switch */
	cfg.t.cfg.s.siIntfCb.ssf					= k->ssf;				/* subsystem service information */
	cfg.t.cfg.s.siIntfCb.pauseActn				= SI_PAUSE_CLRTRAN;		/* call clearing behavior upon rx. PAUSE */
	cfg.t.cfg.s.siIntfCb.dpcCbTmr.t4.enb		= TRUE;					/* t4 timer - user part test sent */
	cfg.t.cfg.s.siIntfCb.dpcCbTmr.t4.val		= k->t4;
	cfg.t.cfg.s.siIntfCb.dpcCbTmr.tPAUSE.enb	= TRUE;					/* waiting for PAUSE to be effective */
	cfg.t.cfg.s.siIntfCb.dpcCbTmr.tPAUSE.val	= k->tpause;
	cfg.t.cfg.s.siIntfCb.dpcCbTmr.tSTAENQ.enb	= TRUE;					/* status enquiry timer */
	cfg.t.cfg.s.siIntfCb.dpcCbTmr.tSTAENQ.val	= k->tstaenq;
#if SS7_ANS95
	cfg.t.cfg.s.siIntfCb.availTest				= FALSE;				/* circuit validation test */
#endif
#if (SS7_ITU97 || SS7_ETSIV3 || SS7_UK || SS7_NZL || SS7_ITU2000 || SS7_KZ)
	cfg.t.cfg.s.siIntfCb.checkTable				= LSI_CHKTBLE_MRATE;	/* Validation flag for Table 3 p1/p2 Q.763 */
#endif
#if (SS7_ANS95 || SS7_ITU97 || SS7_ETSIV3 || SS7_UK || SS7_NZL || SS7_ITU2000 || SS7_KZ)
	switch (k->switchType) {
	case LSI_SW_TST:
	case LSI_SW_ITU:
	case LSI_SW_ITU97:
	case LSI_SW_ITU2000:
	case LSI_SW_ETSI:
	case LSI_SW_ETSIV3:
	case LSI_SW_RUSSIA:
	case LSI_SW_RUSS2000:
	case LSI_SW_INDIA:
	case LSI_SW_CHINA:
		cfg.t.cfg.s.siIntfCb.trunkType			= TRUE;					/* truck type E1(TRUE)/T1(FALSE) at intf */
		break;
	case LSI_SW_ANS88:
	case LSI_SW_ANS92:
	case LSI_SW_ANS95:
	case LSI_SW_BELL:
		cfg.t.cfg.s.siIntfCb.trunkType			= FALSE;				/* truck type E1(TRUE)/T1(FALSE) at intf */
		break;
	}

#endif
#if (LSIV4 || LSIV5)
	cfg.t.cfg.s.siIntfCb.lnkSelOpt				= LSI_LINSEK_CIC;		/* link select option */
# if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	cfg.t.cfg.s.siIntfCb.lnkSelBits				= LSI_LNKSEL_8BITS;		/* number of bits for link selection */
# endif
#endif

	return(sng_cfg_isup(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_isup_ckt_config(int id)
{
	SiMngmt			 cfg;
	Pst				 pst;
	U32				 tmp_flag;
	sng_isup_ckt_t	*k = &g_ftdm_sngss7_data.cfg.isupCkt[id];

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSI;

	/* check the for the correct ProcId and make sure it goes to the right system */
	if (g_ftdm_sngss7_data.cfg.procId != 1) {
		pst.dstProcId = 1;
	}

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SiMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType						= TCFG;
	cfg.hdr.entId.ent					= ENTSI;
	cfg.hdr.entId.inst					= S_INST;
	cfg.hdr.elmId.elmnt					= STICIR;

	cfg.hdr.elmId.elmntInst1			= k->id;

	cfg.t.cfg.s.siCir.cirId				= k->id;			/* circuit id code						 */
	cfg.t.cfg.s.siCir.cic				= k->cic;			/* cic									 */
	cfg.t.cfg.s.siCir.intfId			= k->infId;			/* interface id							 */
	cfg.t.cfg.s.siCir.typeCntrl			= k->typeCntrl;		/* type of control						 */
	cfg.t.cfg.s.siCir.contReq			= FALSE;		 	/* continuity check required				*/
#if (SI_218_COMP || SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	cfg.t.cfg.s.siCir.firstCic			= 1;					/* First cic in the circuit group		  */
	cfg.t.cfg.s.siCir.numCir			= 24;					/* Number of circuits in the circuit group */
	cfg.t.cfg.s.siCir.nonSS7Con			= TRUE;				/* connecting to non SS7 network			*/
	cfg.t.cfg.s.siCir.outTrkGrpN.length	= 0;					/* outgoing trunk group number (For EXM)	*/
	cfg.t.cfg.s.siCir.cvrTrkClli.length	= 0;					/* Trunk Group number (For CVR validation) */
	cfg.t.cfg.s.siCir.clli.length		= 0;					/* common language location identifier	 */
#endif
	cfg.t.cfg.s.siCir.cirTmr.t3.enb		= TRUE;				/* t3 timer - overload received			*/
	cfg.t.cfg.s.siCir.cirTmr.t3.val		= k->t3;
	cfg.t.cfg.s.siCir.cirTmr.t12.enb	= TRUE;				/* t12 timer - blocking sent				*/
	cfg.t.cfg.s.siCir.cirTmr.t12.val	= k->t12;
	cfg.t.cfg.s.siCir.cirTmr.t13.enb	= TRUE;				/* t13 timer - initial blocking sent		*/
	cfg.t.cfg.s.siCir.cirTmr.t13.val	= k->t13;
	cfg.t.cfg.s.siCir.cirTmr.t14.enb	= TRUE;				/* t14 timer - unblocking sent			 */
	cfg.t.cfg.s.siCir.cirTmr.t14.val	= k->t14;
	cfg.t.cfg.s.siCir.cirTmr.t15.enb	= TRUE;				/* t15 timer - initial unblocking sent	 */
	cfg.t.cfg.s.siCir.cirTmr.t15.val	= k->t15;
	cfg.t.cfg.s.siCir.cirTmr.t16.enb	= TRUE;				/* t16 timer - reset sent				  */
	cfg.t.cfg.s.siCir.cirTmr.t16.val	= k->t16;
	cfg.t.cfg.s.siCir.cirTmr.t17.enb	= TRUE;				/* t17 timer - initial reset sent		  */
	cfg.t.cfg.s.siCir.cirTmr.t17.val	= k->t17;
#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	cfg.t.cfg.s.siCir.cirTmr.tVal.enb	= TRUE;				/* circuit validation timer				 */
	cfg.t.cfg.s.siCir.cirTmr.tVal.val	= k->tval;
#endif
#if (SS7_ANS95 || SS7_ITU97 || SS7_ETSIV3 || SS7_UK)
	tmp_flag = 0x0;
	/* bit 0 - 4 is the timeslot on the span for this circuit */
	tmp_flag = ( 1 );

	/* bit 5 -> can this timeslot be used for contigous M-rate call */
	tmp_flag |= !(0x20);

	/* bit 6 -> does this timeslot support contigous M-rate call */
	tmp_flag |= !(0x40);

	cfg.t.cfg.s.siCir.slotId		= tmp_flag ;		 /* physical slot id bit wise flag		  */
	cfg.t.cfg.s.siCir.ctrlMult		= 0;				/* Controller for multirate calls		  */
#endif

	tmp_flag = 0x0;
	/* bit 0 -> ANSI international support or national support */
	tmp_flag = k->ssf;

	/* bit 1 -> confusion message on */
	tmp_flag |= LSI_CIRFLG_CFN_ON;

	/*bit 2-3 -> circuit group carrier information */
	tmp_flag |= LSI_CFCI_ANALDIG;

	/*bit 4-5 -> alarm carrier */
	tmp_flag |= LSI_CFAC_UNKNOWN;

	/*bit 6-7 -> continuity check requirement*/
	tmp_flag |= LSI_CFCO_NONE;

	cfg.t.cfg.s.siCir.cirFlg 		= tmp_flag;		 /* Flag indicating diff cfg options for ckt */

	return(sng_cfg_isup(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_isup_isap_config(int id)
{
	SiMngmt		 cfg;
	Pst			 pst;
	sng_isap_t	*k = &g_ftdm_sngss7_data.cfg.isap[id];

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSI;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(SiMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType							= TCFG;
	cfg.hdr.entId.ent						= ENTSI;
	cfg.hdr.entId.inst 						= S_INST;
	cfg.hdr.elmId.elmnt 					= STISAP;

	cfg.hdr.elmId.elmntInst1 				= k->id;

#if (SI_LMINT3 || SMSI_LMINT3)
	cfg.t.cfg.s.siSap.sapId			 		= k->id;					/* Id of the SAP being configured */
#endif
	cfg.t.cfg.s.siSap.swtch					= k->switchType;			/* Protocol Switch */
	cfg.t.cfg.s.siSap.ssf					= k->ssf;					/* Sub service field */
	cfg.t.cfg.s.siSap.sidIns				= FALSE;					/* SID insertion Flag */
	cfg.t.cfg.s.siSap.sidVer				= FALSE;					/* SID verification Flag */
	if ( cfg.t.cfg.s.siSap.sidIns == TRUE ) {							/* SID */
		#if 0
		cfg.t.cfg.s.siSap.sid				=;
		cfg.t.cfg.s.siSap.natAddrInd		=;							/* SID Nature of Addres Indicator */
		cfg.t.cfg.s.siSap.sidNPlan			=;							/* SID Numbering Plan */
		cfg.t.cfg.s.siSap.sidPresInd		=;							/* default presentation indicator */
		cfg.t.cfg.s.siSap.incSidPresRes		=;							/* Presentation Restriction of incoming SID */
		cfg.t.cfg.s.siSap.sidPresRes		=;							/* Presentation Restriction */
		#endif
	} else {
		cfg.t.cfg.s.siSap.sid.length	= 0;
		/*cfg.t.cfg.s.siSap.sid.strg[0]  =*/
		cfg.t.cfg.s.siSap.natAddrInd		= ADDR_NOTPRSNT;			/* SID Nature of Addres Indicator */
		cfg.t.cfg.s.siSap.sidNPlan			= NP_ISDN;					/* SID Numbering Plan */
		cfg.t.cfg.s.siSap.sidPresInd		= FALSE;					/* default presentation indicator */
		cfg.t.cfg.s.siSap.incSidPresRes		= FALSE;					/* Presentation Restriction of incoming SID */
		cfg.t.cfg.s.siSap.sidPresRes		= 0;						/* Presentation Restriction */
	}
	cfg.t.cfg.s.siSap.reqOpt				= FALSE;					/* Request option */
	cfg.t.cfg.s.siSap.allCallMod			= TRUE;						/* call modification allowed flag */
	cfg.t.cfg.s.siSap.maxLenU2U				= MAX_SI_USER_2_USER_LEN;	/* Max length of user to user messages */
	cfg.t.cfg.s.siSap.passOnFlag			= TRUE;						/* flag for passing unknown par/msg */
	cfg.t.cfg.s.siSap.relLocation			= ILOC_PRIVNETLU;			/* release location indicator in cause val */
	cfg.t.cfg.s.siSap.prior					= PRIOR0;					/* priority */
	cfg.t.cfg.s.siSap.route					= RTESPEC;					/* route */
	cfg.t.cfg.s.siSap.selector				= 0;						/* selector */
	cfg.t.cfg.s.siSap.mem.region			= S_REG;					/* memory region & pool id */
	cfg.t.cfg.s.siSap.mem.pool				= S_POOL;					/* memory region & pool id */

	cfg.t.cfg.s.siSap.tmr.t1.enb			= TRUE;						/* t1 timer - release sent				 */
	cfg.t.cfg.s.siSap.tmr.t1.val			= k->t1;
	cfg.t.cfg.s.siSap.tmr.t2.enb			= TRUE;						/* t2 timer - suspend received			 */
	cfg.t.cfg.s.siSap.tmr.t2.val			= k->t2;
	cfg.t.cfg.s.siSap.tmr.t5.enb			= TRUE;						/* t5 timer - initial release sent		 */
	cfg.t.cfg.s.siSap.tmr.t5.val			= k->t5;
	cfg.t.cfg.s.siSap.tmr.t6.enb			= TRUE;						/* t6 timer - suspend received			 */
	cfg.t.cfg.s.siSap.tmr.t6.val			= k->t6;
	cfg.t.cfg.s.siSap.tmr.t7.enb			= TRUE;						/* t7 timer - latest address sent		  */
	cfg.t.cfg.s.siSap.tmr.t7.val			= k->t7;
	cfg.t.cfg.s.siSap.tmr.t8.enb			= TRUE;						/* t8 timer - initial address received	 */
	cfg.t.cfg.s.siSap.tmr.t8.val			= k->t8;
	cfg.t.cfg.s.siSap.tmr.t9.enb			= TRUE;						/* t9 timer - latest address sent after ACM */
	cfg.t.cfg.s.siSap.tmr.t9.val			= k->t9;
	cfg.t.cfg.s.siSap.tmr.t27.enb			= TRUE;						/* t27 timer - wait. for continuity recheck */
	cfg.t.cfg.s.siSap.tmr.t27.val			= k->t27;
	cfg.t.cfg.s.siSap.tmr.t31.enb			= TRUE;						/* t31 timer - call reference frozen period */
	cfg.t.cfg.s.siSap.tmr.t31.val			= k->t31;
	cfg.t.cfg.s.siSap.tmr.t33.enb			= TRUE;						/* t33 timer - INR sent					 */
	cfg.t.cfg.s.siSap.tmr.t33.val			= k->t33;
	cfg.t.cfg.s.siSap.tmr.t34.enb			= TRUE;						/* t34 timer - wait. for continuity after recheck */
	cfg.t.cfg.s.siSap.tmr.t34.val			= k->t34;
	cfg.t.cfg.s.siSap.tmr.t36.enb			= TRUE;						/* waiting SGM							 */
	cfg.t.cfg.s.siSap.tmr.t36.val			= k->t36;
	cfg.t.cfg.s.siSap.tmr.tCCR.enb			= TRUE;						/* tCCR timer - continuity recheck timer	*/
	cfg.t.cfg.s.siSap.tmr.tCCR.val			= k->tccr;
	cfg.t.cfg.s.siSap.tmr.tRELRSP.enb		= TRUE;						/* waiting for release response			 */
	cfg.t.cfg.s.siSap.tmr.tRELRSP.val		= k->trelrsp;
	cfg.t.cfg.s.siSap.tmr.tFNLRELRSP.enb	= TRUE;						/* waiting for final release response	  */
	cfg.t.cfg.s.siSap.tmr.tFNLRELRSP.val	= k->tfnlrelrsp;
#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	cfg.t.cfg.s.siSap.tmr.tEx.enb			= TRUE;						/* tEx timer - Exit to be sent			 */
	cfg.t.cfg.s.siSap.tmr.tEx.val			= k->tex;
	cfg.t.cfg.s.siSap.tmr.tCCRt.enb			= TRUE;						/* tCCR timer (o/g side) - continuity recheck timer */
	cfg.t.cfg.s.siSap.tmr.tCCRt.val			= k->tccrt;
#endif
#if (SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	cfg.t.cfg.s.siSap.tmr.tCRM.enb			= TRUE;						/* circuit reservation message timer		*/
	cfg.t.cfg.s.siSap.tmr.tCRM.val			= k->tcrm;
	cfg.t.cfg.s.siSap.tmr.tCRA.enb			= TRUE;						/* circuit reservation ack. timer		 */
	cfg.t.cfg.s.siSap.tmr.tCRA.val			= k->tcra;
#endif
#if (SS7_ETSI || SS7_ITU97 || SS7_ETSIV3 || SS7_UK || SS7_NZL || SS7_KZ)
	cfg.t.cfg.s.siSap.tmr.tECT.enb			= TRUE;						/* Explicit Call Transfer - waiting for loop prvnt rsp */
	cfg.t.cfg.s.siSap.tmr.tECT.val			= k->tect;
#endif

#ifdef TDS_ROLL_UPGRADE_SUPPORT
	cfg.t.cfg.s.siSap.remIntfValid			= FALSE;					/* remote interface version is valid */
	cfg.t.cfg.s.siSap.remIntfVer			=;							/* remote interface version */
#endif

	return(sng_cfg_isup(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_cc_isap_config(int dstProcId)
{
	CcMngmt	 cfg;
	Pst		 pst;


	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTCC;

	/*clear the configuration structure*/
	memset(&cfg, 0x0, sizeof(CcMngmt));

	/*fill in some general sections of the header*/
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType						= TCFG;
	cfg.hdr.entId.ent					= ENTCC;
	cfg.hdr.entId.inst					= S_INST;
	cfg.hdr.elmId.elmnt					= STISAP;

	cfg.hdr.elmId.elmntInst1 			= 1;

	cfg.t.cfg.s.ccISAP.suId			 	= 1;
	cfg.t.cfg.s.ccISAP.spId			 	= 1;
	cfg.t.cfg.s.ccISAP.pst.dstProcId	= dstProcId;
	cfg.t.cfg.s.ccISAP.pst.dstEnt		= ENTSI;
	cfg.t.cfg.s.ccISAP.pst.dstInst		= S_INST;
	cfg.t.cfg.s.ccISAP.pst.srcProcId	= SFndProcId();
	cfg.t.cfg.s.ccISAP.pst.srcEnt		= ENTCC;
	cfg.t.cfg.s.ccISAP.pst.srcInst		= S_INST;
	cfg.t.cfg.s.ccISAP.pst.prior		= PRIOR0;
	cfg.t.cfg.s.ccISAP.pst.route		= RTESPEC;
	cfg.t.cfg.s.ccISAP.pst.region		= S_REG;
	cfg.t.cfg.s.ccISAP.pst.pool			= S_POOL;
	cfg.t.cfg.s.ccISAP.pst.selector	 	= 0;

	return(sng_cfg_cc(&pst, &cfg));
}

/******************************************************************************/
int ftmod_ss7_relay_chan_config(int id)
{
	RyMngmt	cfg;	/*configuration structure*/
	Pst		pst;	/*post structure*/
	sng_relay_t		*k = &g_ftdm_sngss7_data.cfg.relay[id];
	
	/* initalize the post structure */
	smPstInit(&pst);
	
	/* insert the destination Entity */
	pst.dstEnt = ENTRY;
	
	/* clear the configuration structure */
	memset(&cfg, 0x0, sizeof(RyMngmt));
	
	/* fill in some general sections of the header */
	smHdrInit(&cfg.hdr);

	/*fill in the specific fields of the header */
	cfg.hdr.msgType							= TCFG;
	cfg.hdr.entId.ent 						= ENTRY;
	cfg.hdr.entId.inst						= S_INST;
	cfg.hdr.elmId.elmnt 					= STCHCFG;

	cfg.hdr.elmId.elmntInst1 				= k->id;

	cfg.t.cfg.s.ryChanCfg.id				= k->id;					/* channel id */
	cfg.t.cfg.s.ryChanCfg.type				= k->type;					/* channel type */
/*	cfg.t.cfg.s.ryChanCfg.msInd				=;*/						/* master/slave indicator */
	if (k->type == LRY_CT_TCP_LISTEN) {
		cfg.t.cfg.s.ryChanCfg.low			= 0;						/* low proc id for channel */
		cfg.t.cfg.s.ryChanCfg.high			= 0;						/* high proc id for channel */
	} else {
		cfg.t.cfg.s.ryChanCfg.low			= k->procId;				/* low proc id for channel */
		cfg.t.cfg.s.ryChanCfg.high			= k->procId;				/* high proc id for channel */
	}
	cfg.t.cfg.s.ryChanCfg.nmbScanQ			= MAX_RELAY_NMBSCAN;		/* number of times to scan the queue */
	cfg.t.cfg.s.ryChanCfg.flags				= LRY_FLG_INTR;				/* flags */
	cfg.t.cfg.s.ryChanCfg.congThrsh			= MAX_RELAY_CONGTHRSH;		/* congestion threshold */
	cfg.t.cfg.s.ryChanCfg.dropThrsh			= 0;						/* drop threshold */
	cfg.t.cfg.s.ryChanCfg.contThrsh			= MAX_RELAY_CONGTHRSH + 1;	/* continue threshold */
	cfg.t.cfg.s.ryChanCfg.kaTxTmr.enb		= 1;						/* keep alive transmit timer config */
	cfg.t.cfg.s.ryChanCfg.kaTxTmr.val		= RY_TX_KP_ALIVE_TMR;
	cfg.t.cfg.s.ryChanCfg.kaRxTmr.enb		= 1;						/* keep alive receive timer config */
	cfg.t.cfg.s.ryChanCfg.kaRxTmr.val		= RY_RX_KP_ALIVE_TMR;
	cfg.t.cfg.s.ryChanCfg.btTmr.enb			= 1;						/* boot timer */
	cfg.t.cfg.s.ryChanCfg.btTmr.val			= RY_BT_TMR;
	cfg.t.cfg.s.ryChanCfg.region			= S_REG;					/* Relay region */
	cfg.t.cfg.s.ryChanCfg.pool				= S_POOL;					/* Relay pool */
#if (RY_ENBUDPSOCK || RY_ENBTCPSOCK) 
	cfg.t.cfg.s.ryChanCfg.listenPortNo		= k->port;					/* Listen Port of Rx Relay Channel*/
	strncpy(cfg.t.cfg.s.ryChanCfg.transmittoHostName, k->hostname, (size_t)RY_REMHOSTNAME_SIZE);
	cfg.t.cfg.s.ryChanCfg.transmittoPortNo	= k->port;					/* TransmitTo PortId for Tx Relay Channel */
	cfg.t.cfg.s.ryChanCfg.targetProcId		= k->procId;				/* procId of the node present in the other end of this channel                     */
# ifdef LRY1
	cfg.t.cfg.s.ryChanCfg.sockParam			=;   /* Socket Parameters */
# endif /* LRY1 */
# ifdef LRYV2
	cfg.t.cfg.s.ryChanCfg.selfHostName[RY_REMHOSTNAME_SIZE];
# endif /* LRY2 */
#endif /* RY_ENBUDPSOCK || RY_ENBTCPSOCK */

	return(sng_cfg_relay(&pst, &cfg));
}

/******************************************************************************/
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
/******************************************************************************/
