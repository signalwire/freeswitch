/*
 * Copyright (c) 2010, Sangoma Technologies 
 * David Yat Sin <davidy@sangoma.com>
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
 */

#include "ftmod_sangoma_isdn.h"

extern ftdm_sngisdn_data_t	g_sngisdn_data;

uint8_t sng_isdn_stack_switchtype(sngisdn_switchtype_t switchtype);

ftdm_status_t sngisdn_cfg_phy(ftdm_span_t *span);
ftdm_status_t sngisdn_cfg_q921(ftdm_span_t *span);
ftdm_status_t sngisdn_cfg_q931(ftdm_span_t *span);
ftdm_status_t sngisdn_cfg_cc(ftdm_span_t *span);

ftdm_status_t sngisdn_stack_cfg_phy_gen(void);
ftdm_status_t sngisdn_stack_cfg_q921_gen(void);
ftdm_status_t sngisdn_stack_cfg_q931_gen(void);
ftdm_status_t sngisdn_stack_cfg_cc_gen(void);


ftdm_status_t sngisdn_stack_cfg_phy_psap(ftdm_span_t *span);
ftdm_status_t sngisdn_stack_cfg_q921_msap(ftdm_span_t *span);
ftdm_status_t sngisdn_stack_cfg_q921_dlsap(ftdm_span_t *span, uint8_t management);
ftdm_status_t sngisdn_stack_cfg_q931_tsap(ftdm_span_t *span);
ftdm_status_t sngisdn_stack_cfg_q931_dlsap(ftdm_span_t *span);
ftdm_status_t sngisdn_stack_cfg_q931_lce(ftdm_span_t *span);

ftdm_status_t sngisdn_stack_cfg_cc_sap(ftdm_span_t *span);

ftdm_status_t sngisdn_start_gen_cfg(void)
{
	if (!g_sngisdn_data.gen_config_done) {
		g_sngisdn_data.gen_config_done = 1;
		ftdm_log(FTDM_LOG_DEBUG, "Starting general stack configuration\n");
		if(sngisdn_stack_cfg_phy_gen()!= FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT, "Failed general physical configuration\n");
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "General stack physical done\n");
	
		if(sngisdn_stack_cfg_q921_gen()!= FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT, "Failed general q921 configuration\n");
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "General stack q921 done\n");

		if(sngisdn_stack_cfg_q931_gen()!= FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT, "Failed general q921 configuration\n");
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "General stack q931 done\n");

		if(sngisdn_stack_cfg_cc_gen()!= FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT, "Failed general CC configuration\n");
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "General stack CC done\n");
		ftdm_log(FTDM_LOG_INFO, "General stack configuration done\n");
	}
	return FTDM_SUCCESS;
}


ftdm_status_t sngisdn_stack_cfg(ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	ftdm_log(FTDM_LOG_DEBUG, "Starting stack configuration for span:%s\n", span->name);

	if (signal_data->dchan) {
		if (sngisdn_stack_cfg_phy_psap(span) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "%s:phy_psap configuration failed\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:phy_psap configuration done\n", span->name);

		if (sngisdn_stack_cfg_q921_msap(span) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "%s:q921_msap configuration failed\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:q921_msap configuration done\n", span->name);

		if (sngisdn_stack_cfg_q921_dlsap(span, 0) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "%s:q921_dlsap configuration failed\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:q921_dlsap configuration done\n", span->name);

		if (span->trunk_type == FTDM_TRUNK_BRI_PTMP) {
			if (sngisdn_stack_cfg_q921_dlsap(span, 1) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "%s:q921_dlsap management configuration failed\n", span->name);
				return FTDM_FAIL;
			}
			ftdm_log(FTDM_LOG_DEBUG, "%s:q921_dlsap management configuration done\n", span->name);
		}
	}

	if (sngisdn_stack_cfg_q931_dlsap(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "%s:q931_dlsap configuration failed\n", span->name);
		return FTDM_FAIL;
	}
	ftdm_log(FTDM_LOG_DEBUG, "%s:q931_dlsap configuration done\n", span->name);

	if (signal_data->dchan) {
		if (sngisdn_stack_cfg_q931_lce(span) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "%s:q931_lce configuration failed\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:q931_lce configuration done\n", span->name);
	}

	if (!g_sngisdn_data.ccs[signal_data->cc_id].config_done) {
		g_sngisdn_data.ccs[signal_data->cc_id].config_done = 1;
		/* if BRI, need to configure dlsap_mgmt */
		if (sngisdn_stack_cfg_q931_tsap(span) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "%s:q931_tsap configuration failed\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:q931_tsap configuration done\n", span->name);

		if (sngisdn_stack_cfg_cc_sap(span) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "%s:cc_sap configuration failed\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:cc_sap configuration done\n", span->name);
	}

	ftdm_log(FTDM_LOG_INFO, "%s:stack configuration done\n", span->name);
	return FTDM_SUCCESS;
}



ftdm_status_t sngisdn_stack_cfg_phy_gen(void)
{
	/*local variables*/
	L1Mngmt     cfg;    /*configuration structure*/
	Pst         pst;    /*post structure*/

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTL1;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTL1;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STGEN;

	stack_pst_init(&cfg.t.cfg.s.l1Gen.sm );
	cfg.t.cfg.s.l1Gen.sm.srcEnt     = ENTL1;
	cfg.t.cfg.s.l1Gen.sm.dstEnt     = ENTSM;

	cfg.t.cfg.s.l1Gen.nmbLnks       = MAX_L1_LINKS;
	cfg.t.cfg.s.l1Gen.poolTrUpper   = POOL_UP_TR;        /* upper pool threshold */
	cfg.t.cfg.s.l1Gen.poolTrLower   = POOL_LW_TR;        /* lower pool threshold */

	if (sng_isdn_phy_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_stack_cfg_phy_psap(ftdm_span_t *span)
{	
	L1Mngmt				cfg;
	Pst					pst;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTL1;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTL1;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STPSAP;

	cfg.hdr.elmId.elmntInst1    = signal_data->link_id;

	if (!signal_data->dchan) {
		ftdm_log(FTDM_LOG_ERROR, "%s:No d-channels specified\n", span->name);
		return FTDM_FAIL;
	}

	cfg.t.cfg.s.l1PSAP.sockfd = (int32_t)signal_data->dchan->sockfd;

	switch(span->trunk_type) {
		case FTDM_TRUNK_E1:
			cfg.t.cfg.s.l1PSAP.type = SNG_L1_TYPE_PRI;
			break;
		case FTDM_TRUNK_T1:
		case FTDM_TRUNK_J1:
			cfg.t.cfg.s.l1PSAP.type = SNG_L1_TYPE_PRI;
			break;
		case FTDM_TRUNK_BRI:
		case FTDM_TRUNK_BRI_PTMP:
			cfg.t.cfg.s.l1PSAP.type = SNG_L1_TYPE_BRI;
			break;
		default:
			ftdm_log(FTDM_LOG_ERROR, "%s:Unsupported trunk type %d\n", span->name, span->trunk_type);
			return FTDM_FAIL;
	}

	cfg.t.cfg.s.l1PSAP.spId		= signal_data->link_id;

	if (sng_isdn_phy_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}


ftdm_status_t sngisdn_stack_cfg_q921_gen(void)
{
	BdMngmt cfg;
	Pst     pst;

	/* initalize the post structure */
	stack_pst_init(&pst);
	/* insert the destination Entity */
	pst.dstEnt = ENTLD;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));
	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTLD;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STGEN;
	/* fill in the Gen Conf structures internal pst struct */

	stack_pst_init(&cfg.t.cfg.s.bdGen.sm);

	cfg.t.cfg.s.bdGen.sm.dstEnt    = ENTSM;         /* entity */
	
	cfg.t.cfg.s.bdGen.nmbPLnks = 	MAX_L1_LINKS+1;
	cfg.t.cfg.s.bdGen.nmbLDLnks =	MAX_L1_LINKS+1; /* Not used in LAPD */
	cfg.t.cfg.s.bdGen.nmbDLCs = MAX_L1_LINKS+1;
	cfg.t.cfg.s.bdGen.nmbDLCs = MAX_TEIS_PER_LINK*(MAX_L1_LINKS+1);
	cfg.t.cfg.s.bdGen.nmbASPLnks = MAX_L1_LINKS+1;

#ifdef LAPD_3_4
	cfg.t.cfg.s.bdGen.timeRes     = 100;      /* timer resolution = 1 sec */
#endif
	cfg.t.cfg.s.bdGen.poolTrUpper   = 2;        /* upper pool threshold */
	cfg.t.cfg.s.bdGen.poolTrLower   = 1;        /* lower pool threshold */

	if (sng_isdn_q921_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_stack_cfg_q921_msap(ftdm_span_t *span)
{
	BdMngmt cfg;
	Pst     pst;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	/* initalize the post structure */
	stack_pst_init(&pst);
	/* insert the destination Entity */
	pst.dstEnt = ENTLD;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));
	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTLD;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STMSAP;

	cfg.t.cfg.s.bdMSAP.lnkNmb      = signal_data->link_id;

	cfg.t.cfg.s.bdMSAP.maxOutsFrms = 24;            /* MAC window */
	cfg.t.cfg.s.bdMSAP.tQUpperTrs  = 32;           /* Tx Queue Upper Threshold */
	cfg.t.cfg.s.bdMSAP.tQLowerTrs  = 24;            /* Tx Queue Lower Threshold */
	cfg.t.cfg.s.bdMSAP.selector    = 0;       /* Selector 0 */
	/* TODO: check if bdMSAP parameters can be initialized by calling stack_pst_init */
	cfg.t.cfg.s.bdMSAP.mem.region  = S_REG;       /* Memory region */
	cfg.t.cfg.s.bdMSAP.mem.pool    = S_POOL;      /* Memory pool */
	cfg.t.cfg.s.bdMSAP.prior       = PRIOR0;            /* Priority */
	cfg.t.cfg.s.bdMSAP.route       = RTESPEC;            /* Route */
	cfg.t.cfg.s.bdMSAP.dstProcId   = SFndProcId(); /* destination proc id */
	cfg.t.cfg.s.bdMSAP.dstEnt      = ENTL1;        /* entity */
	cfg.t.cfg.s.bdMSAP.dstInst     = S_INST;      /* instance */
	cfg.t.cfg.s.bdMSAP.t201Tmr     = 1;            /* T201 - should be equal to t200Tmr */
	cfg.t.cfg.s.bdMSAP.t202Tmr     = 2;          /* T202 */
	cfg.t.cfg.s.bdMSAP.bndRetryCnt = 2;            /* bind retry counter */
	cfg.t.cfg.s.bdMSAP.tIntTmr     = 200;          /* bind retry timer */
	cfg.t.cfg.s.bdMSAP.n202        = 3;            /* N202 */
	cfg.t.cfg.s.bdMSAP.lowTei      = 64;         /* Lowest dynamic TEI */

	if (span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
		signal_data->signalling == SNGISDN_SIGNALING_NET) {
		cfg.t.cfg.s.bdMSAP.kpL1Up      = FALSE;        /* flag to keep l1 up or not */
	} else {
		cfg.t.cfg.s.bdMSAP.kpL1Up      = TRUE;        /* flag to keep l1 up or not */
	}

	switch(signal_data->switchtype) {
		case SNGISDN_SWITCH_NI2:
		case SNGISDN_SWITCH_5ESS:
		case SNGISDN_SWITCH_4ESS:
		case SNGISDN_SWITCH_DMS100:
			cfg.t.cfg.s.bdMSAP.type = SW_NI2;
			break;
		case SNGISDN_SWITCH_INSNET:
			cfg.t.cfg.s.bdMSAP.type = SW_CCITT;
			break;
		case SNGISDN_SWITCH_EUROISDN:
		case SNGISDN_SWITCH_QSIG:
			cfg.t.cfg.s.bdMSAP.type = SW_ETSI;
			break;
	}

	if (span->trunk_type == FTDM_TRUNK_BRI_PTMP) {
		cfg.t.cfg.s.bdMSAP.teiChkTmr   = 20;         /* Tei check timer */
	} else {
		cfg.t.cfg.s.bdMSAP.teiChkTmr   = 0;         /* Tei check timer */
	}

	if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
		cfg.t.cfg.s.bdMSAP.logInt      = 1;         /* logical interface = 0 = user, 1= network */
		cfg.t.cfg.s.bdMSAP.setUpArb    = PASSIVE;       /* set up arbitration */
	} else {
		cfg.t.cfg.s.bdMSAP.logInt      = 0;         /* logical interface = 0 = user, 1= network */
		cfg.t.cfg.s.bdMSAP.setUpArb    = ACTIVE;       /* set up arbitration */
	}

	/* Overwrite setUpArb value if user forced it */
	if (signal_data->setup_arb == SNGISDN_OPT_TRUE) {
		cfg.t.cfg.s.bdMSAP.setUpArb    = ACTIVE;
	} else if (signal_data->setup_arb == SNGISDN_OPT_FALSE) {
		cfg.t.cfg.s.bdMSAP.setUpArb    = PASSIVE;
	}

	if (sng_isdn_q921_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_stack_cfg_q921_dlsap(ftdm_span_t *span, uint8_t management)
{
	BdMngmt cfg;
	Pst     pst;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;
	/* initalize the post structure */
	stack_pst_init(&pst);
	/* insert the destination Entity */
	pst.dstEnt = ENTLD;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));
	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTLD;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STDLSAP;

	cfg.t.cfg.s.bdDLSAP.lnkNmb		= signal_data->link_id;

	cfg.t.cfg.s.bdDLSAP.n201		= 1028;          	/* n201 */
	if (span->trunk_type == FTDM_TRUNK_BRI_PTMP ||
		span->trunk_type == FTDM_TRUNK_BRI) {

		cfg.t.cfg.s.bdDLSAP.k			= 1;				/* Based on q.921 recommendations */
	} else {
		cfg.t.cfg.s.bdDLSAP.k			= 7;             	/* k */
	}

	cfg.t.cfg.s.bdDLSAP.n200		= 3;             	/* n200 */
	cfg.t.cfg.s.bdDLSAP.congTmr		= 300;           	/* congestion timer */
	cfg.t.cfg.s.bdDLSAP.t200Tmr		= 3;				/* t1 changed from 25 */
	cfg.t.cfg.s.bdDLSAP.t203Tmr		= 10;				/* t3 changed from 50 */
	cfg.t.cfg.s.bdDLSAP.mod			= 128;           	/* modulo */
	cfg.t.cfg.s.bdDLSAP.selector	= 0;				/* Selector 0 */
	cfg.t.cfg.s.bdDLSAP.mem.region	= S_REG;			/* Memory region */
	cfg.t.cfg.s.bdDLSAP.mem.pool	= S_POOL;			/* Memory pool */
	cfg.t.cfg.s.bdDLSAP.prior		= PRIOR0;			/* Priority */
	cfg.t.cfg.s.bdDLSAP.route		= RTESPEC;			/* Route */

	if (management) {
		cfg.t.cfg.s.bdDLSAP.sapi     = MNGMT_SAPI;
		cfg.t.cfg.s.bdDLSAP.teiAss     = NON_AUTOMATIC; /* static tei assignment */
		cfg.t.cfg.s.bdDLSAP.noOfDlc = 1;
		cfg.t.cfg.s.bdDLSAP.tei[0]  = 0x7f;
	} else {
		cfg.t.cfg.s.bdDLSAP.sapi  = Q930_SAPI;
		if (span->trunk_type == FTDM_TRUNK_BRI_PTMP) {
			if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
				cfg.t.cfg.s.bdDLSAP.teiAss  = AUTOMATIC;
				cfg.t.cfg.s.bdDLSAP.noOfDlc = 8;

				cfg.t.cfg.s.bdDLSAP.tei[0]  = 64;
				cfg.t.cfg.s.bdDLSAP.tei[1]  = 65;
				cfg.t.cfg.s.bdDLSAP.tei[2]  = 66;
				cfg.t.cfg.s.bdDLSAP.tei[3]  = 67;
				cfg.t.cfg.s.bdDLSAP.tei[4]  = 68;
				cfg.t.cfg.s.bdDLSAP.tei[5]  = 69;
				cfg.t.cfg.s.bdDLSAP.tei[6]  = 70;
				cfg.t.cfg.s.bdDLSAP.tei[7]  = 71;
			} else {
				cfg.t.cfg.s.bdDLSAP.teiAss  = AUTOMATIC;
				cfg.t.cfg.s.bdDLSAP.noOfDlc = 1;
			}
		} else {
			/* Point to point configs */
			cfg.t.cfg.s.bdDLSAP.teiAss    = NON_AUTOMATIC;
			cfg.t.cfg.s.bdDLSAP.noOfDlc   = 1;
			cfg.t.cfg.s.bdDLSAP.tei[0]    = signal_data->tei;
		}
	}

	if (sng_isdn_q921_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_stack_cfg_q931_gen(void)
{
	InMngmt cfg;
	Pst     pst;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTIN;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTIN;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STGEN;

	/* fill in the Gen Conf structures internal pst struct */
	stack_pst_init(&cfg.t.cfg.s.inGen.sm);

	cfg.t.cfg.s.inGen.nmbSaps = MAX_VARIANTS+1;			/* Total number of variants supported */

	cfg.t.cfg.s.inGen.nmbLnks = MAX_L1_LINKS+1;			/* number of Data Link SAPs */
	cfg.t.cfg.s.inGen.nmbSigLnks = MAX_L1_LINKS+1;

	/* number of CESs */
	cfg.t.cfg.s.inGen.nmbCes = (MAX_L1_LINKS+1)*MAX_NUM_CES_PER_LINK;
	/* number of global Call References can have 2 per channel when using HOLD/RESUME */
	cfg.t.cfg.s.inGen.nmbCalRef = MAX_NUM_CALLS;
	/* number of bearer channels */
	cfg.t.cfg.s.inGen.nmbBearer = NUM_E1_CHANNELS_PER_SPAN*(MAX_L1_LINKS+1);
	/* maximum number of routing entries */
	cfg.t.cfg.s.inGen.nmbRouts = 0;
	/* number of profiles */
	cfg.t.cfg.s.inGen.nmbProfiles = 0;
	/* upper pool threshold */
	cfg.t.cfg.s.inGen.poolTrUpper = INGEN_POOL_UP_TR;
	/* time resolution */
	cfg.t.cfg.s.inGen.timeRes = 100; /* timer resolution = 1 sec */

	cfg.t.cfg.s.inGen.sm.dstEnt = ENTSM;

	if (sng_isdn_q931_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

/* Link between CC and q931 */
ftdm_status_t sngisdn_stack_cfg_q931_tsap(ftdm_span_t *span)
{
	InMngmt cfg;
	Pst     pst;
	unsigned i;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;
	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTIN;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTIN;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STTSAP;

	cfg.t.cfg.s.inTSAP.sapId = signal_data->cc_id;

	cfg.t.cfg.s.inTSAP.prior = PRIOR0;
	cfg.t.cfg.s.inTSAP.route = RTESPEC;

	cfg.t.cfg.s.inTSAP.swtch = sng_isdn_stack_switchtype(signal_data->switchtype);
	cfg.t.cfg.s.inTSAP.useSubAdr = 0;						/* call routing on subaddress */
	cfg.t.cfg.s.inTSAP.adrPref = 0;							/* use of prefix for int'l calls */
	cfg.t.cfg.s.inTSAP.nmbPrefDig = 0;						/* number of digits used for prefix */

	for (i = 0; i < IN_MAXPREFDIG; i++)
		cfg.t.cfg.s.inTSAP.prefix[i] = 0;					/* address prefix */

	cfg.t.cfg.s.inTSAP.keyPad = 0;
	cfg.t.cfg.s.inTSAP.wcRout = 0;

	for (i = 0; i < ADRLEN; i++)
		cfg.t.cfg.s.inTSAP.wcMask[i] = 0;	/* address prefix */

	cfg.t.cfg.s.inTSAP.sidIns = FALSE;		/* SID insertion Flag */
	cfg.t.cfg.s.inTSAP.sid.length = 0;		/* SID */
	cfg.t.cfg.s.inTSAP.sidTon = 0;			/* SID Type of Number */
	cfg.t.cfg.s.inTSAP.sidNPlan = 0;		/* SID Numbering Plan */
	cfg.t.cfg.s.inTSAP.callId.len = 0;		/* Default Call Identity */
	cfg.t.cfg.s.inTSAP.minAdrDig = 0;		/* Minimum number of address digits */
	cfg.t.cfg.s.inTSAP.comptChck = FALSE;	/* Validate compatibility */
	cfg.t.cfg.s.inTSAP.nmbApplProf = 0;		/* Number of application profiles */
	cfg.t.cfg.s.inTSAP.profNmb[0] = 0;		/* Application profiles */
	cfg.t.cfg.s.inTSAP.mem.region = S_REG;
	cfg.t.cfg.s.inTSAP.mem.pool = S_POOL;
	cfg.t.cfg.s.inTSAP.selector = 0;


	if (sng_isdn_q931_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_stack_cfg_q931_dlsap(ftdm_span_t *span)
{
	InMngmt cfg;
	Pst     pst;

	unsigned i;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;
	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTIN;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTIN;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STDLSAP;

	cfg.hdr.response.selector=0;

	cfg.t.cfg.s.inDLSAP.sapId = signal_data->link_id;
	cfg.t.cfg.s.inDLSAP.spId = signal_data->link_id;

	cfg.t.cfg.s.inDLSAP.swtch = sng_isdn_stack_switchtype(signal_data->switchtype);

	cfg.t.cfg.s.inDLSAP.n201 = 1024;
	cfg.t.cfg.s.inDLSAP.nmbRst = 2;
	cfg.t.cfg.s.inDLSAP.tCbCfg = TRUE;

	cfg.t.cfg.s.inDLSAP.tCbId = signal_data->cc_id;
	
	if (signal_data->facility == SNGISDN_OPT_TRUE) {
		cfg.t.cfg.s.inDLSAP.facilityHandling = IN_FACILITY_STANDRD;
	} else {
		cfg.t.cfg.s.inDLSAP.facilityHandling = 0;
	}
	
	if (!signal_data->nfas.trunk) {
		cfg.t.cfg.s.inDLSAP.nfasInt = FALSE;
		cfg.t.cfg.s.inDLSAP.intId = 0;
		cfg.t.cfg.s.inDLSAP.sigInt = 0;
		cfg.t.cfg.s.inDLSAP.bupInt = 0;
		cfg.t.cfg.s.inDLSAP.nmbNfasInt = 0;
		cfg.t.cfg.s.inDLSAP.buIntPr = FALSE;

		for (i = 0; i < IN_MAX_NMB_INTRFS; i++)
			cfg.t.cfg.s.inDLSAP.ctldInt[i] = IN_INT_NOT_CFGD;

	} else {
		cfg.t.cfg.s.inDLSAP.nfasInt = TRUE;
		cfg.t.cfg.s.inDLSAP.intId = signal_data->nfas.interface_id;

		for (i = 0; i < IN_MAX_NMB_INTRFS; i++)
			cfg.t.cfg.s.inDLSAP.ctldInt[i] = IN_INT_NOT_CFGD;

		switch (signal_data->nfas.sigchan) {
			case SNGISDN_NFAS_DCHAN_PRIMARY:
				cfg.t.cfg.s.inDLSAP.sigInt = signal_data->nfas.trunk->dchan->link_id;
				cfg.t.cfg.s.inDLSAP.nmbNfasInt = signal_data->nfas.trunk->num_spans;

				if (signal_data->nfas.trunk->backup) {
					cfg.t.cfg.s.inDLSAP.buIntPr = TRUE;
					cfg.t.cfg.s.inDLSAP.bupInt = signal_data->nfas.trunk->backup->link_id;
				} else {
					cfg.t.cfg.s.inDLSAP.buIntPr = FALSE;
				}

				for (i = 0; i < MAX_SPANS_PER_NFAS_LINK; i++) {
					if (signal_data->nfas.trunk->spans[i]) {
						cfg.t.cfg.s.inDLSAP.ctldInt[i] = signal_data->nfas.trunk->spans[i]->link_id;
					}
				}
				
				break;
			case SNGISDN_NFAS_DCHAN_BACKUP:
				cfg.t.cfg.s.inDLSAP.sigInt = signal_data->nfas.trunk->dchan->link_id;
				cfg.t.cfg.s.inDLSAP.nmbNfasInt = signal_data->nfas.trunk->num_spans;

				if (signal_data->nfas.trunk->backup) {
					cfg.t.cfg.s.inDLSAP.buIntPr = TRUE;
					cfg.t.cfg.s.inDLSAP.bupInt = signal_data->nfas.trunk->backup->link_id;
				} else {
					cfg.t.cfg.s.inDLSAP.buIntPr = FALSE;
				}

				for (i = 0; i < MAX_SPANS_PER_NFAS_LINK; i++) {
					if (signal_data->nfas.trunk->spans[i]) {
						cfg.t.cfg.s.inDLSAP.ctldInt[i] = signal_data->nfas.trunk->spans[i]->link_id;
					}
				}
				
				break;
			case SNGISDN_NFAS_DCHAN_NONE:
				cfg.t.cfg.s.inDLSAP.sigInt = signal_data->nfas.trunk->dchan->link_id;
				cfg.t.cfg.s.inDLSAP.nmbNfasInt = 0;

				break;
		}
	}

	cfg.t.cfg.s.inDLSAP.numRstInd = 255;
	cfg.t.cfg.s.inDLSAP.relOpt = TRUE;
#ifdef ISDN_SRV
	cfg.t.cfg.s.inDLSAP.bcas = FALSE;
	cfg.t.cfg.s.inDLSAP.maxBSrvCnt = 2;
	cfg.t.cfg.s.inDLSAP.maxDSrvCnt = 2;
#endif /* ISDN_SRV */

	if (signal_data->switchtype == SNGISDN_SWITCH_QSIG) {
		cfg.t.cfg.s.inDLSAP.intType = SYM_USER;
		cfg.t.cfg.s.inDLSAP.clrGlr = TRUE;			/* in case of glare, clear local call */
		cfg.t.cfg.s.inDLSAP.statEnqOpt = FALSE;
		cfg.t.cfg.s.inDLSAP.rstOpt = FALSE;
	} else {
		if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
			cfg.t.cfg.s.inDLSAP.intType = NETWORK;
			cfg.t.cfg.s.inDLSAP.clrGlr = FALSE;			/* in case of glare, do not clear local call */
			cfg.t.cfg.s.inDLSAP.statEnqOpt = TRUE;

			if (signal_data->ftdm_span->trunk_type == FTDM_TRUNK_BRI_PTMP) {
				cfg.t.cfg.s.inDLSAP.rstOpt = FALSE;
			} else {
				cfg.t.cfg.s.inDLSAP.rstOpt = TRUE;
			}
		} else {
			cfg.t.cfg.s.inDLSAP.intType = USER;
			cfg.t.cfg.s.inDLSAP.clrGlr = TRUE;			/* in case of glare, clear local call */
			cfg.t.cfg.s.inDLSAP.statEnqOpt = FALSE;
			cfg.t.cfg.s.inDLSAP.rstOpt = FALSE;
		}
	}

	if (signal_data->switchtype == SNGISDN_SWITCH_QSIG ||
	    signal_data->switchtype == SNGISDN_SWITCH_5ESS) {

		cfg.t.cfg.s.inDLSAP.ackOpt = TRUE;
	} else if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
		cfg.t.cfg.s.inDLSAP.ackOpt = TRUE;
	} else {
		cfg.t.cfg.s.inDLSAP.ackOpt = FALSE;
	}

	if (signal_data->send_connect_ack != SNGISDN_OPT_DEFAULT) {
		if (signal_data->send_connect_ack == SNGISDN_OPT_TRUE) {
			cfg.t.cfg.s.inDLSAP.ackOpt = TRUE;
		} else {
			cfg.t.cfg.s.inDLSAP.ackOpt = FALSE;
		}
	}

	/* Override the restart options if user selected that option */
	if (signal_data->restart_opt != SNGISDN_OPT_DEFAULT) {
		if (signal_data->restart_opt == SNGISDN_OPT_TRUE) {
			cfg.t.cfg.s.inDLSAP.rstOpt = TRUE;
		} else {
			cfg.t.cfg.s.inDLSAP.rstOpt = FALSE;
		}
	}
	
	for (i = 0; i < IN_MAXBCHNL; i++)
	{
		cfg.t.cfg.s.inDLSAP.bProf[i].profNmb = 0;
		cfg.t.cfg.s.inDLSAP.bProf[i].valid = FALSE;
		cfg.t.cfg.s.inDLSAP.bProf[i].state = IN_PROV_AVAIL;
	}

	if (span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
		signal_data->signalling == SNGISDN_SIGNALING_NET) {
		cfg.t.cfg.s.inDLSAP.nmbCes = MAX_NUM_CES_PER_LINK;
	} else {
		cfg.t.cfg.s.inDLSAP.nmbCes=1;
	}
	
	cfg.t.cfg.s.inDLSAP.useSubAdr = 0;       /* call routing on subaddress */
#ifdef SANGOMA_ISDN_CHAN_ID_INVERT_BIT
	if (signal_data->switchtype == SNGISDN_SWITCH_DMS100 &&
		g_sngisdn_data.chan_id_invert_extend_bit == SNGISDN_OPT_TRUE) {
		/* Since this feature is not standard, we modified Trillium to check 
		the useSubAdr field and remove the extended bit if this is set, this
		is a global configuration and once set, applies to all spans configured
		as DMS 100 */
		cfg.t.cfg.s.inDLSAP.useSubAdr = PRSNT_NODEF;
	}
#endif
	cfg.t.cfg.s.inDLSAP.adrPref = 0;         /* use of prefix for international calls */
	cfg.t.cfg.s.inDLSAP.nmbPrefDig = 0;      /* number of digits used for prefix */
	for (i = 0; i < IN_MAXPREFDIG; i++)
		cfg.t.cfg.s.inDLSAP.prefix[i] = 0;       /* address prefix */
	cfg.t.cfg.s.inDLSAP.keyPad = 0;
	cfg.t.cfg.s.inDLSAP.wcRout = 0;
	for (i = 0; i < ADRLEN; i++)
		cfg.t.cfg.s.inDLSAP.wcMask[i] = 0;       /* address prefix */

	cfg.t.cfg.s.inDLSAP.sidIns = FALSE;         /* SID insertion flag */
	cfg.t.cfg.s.inDLSAP.sid.length = 0;  				/* SID */
	cfg.t.cfg.s.inDLSAP.sidTon = 0;             /* SID Type of Number */
	cfg.t.cfg.s.inDLSAP.sidNPlan = 0;           /* SID Numbering Plan */
	cfg.t.cfg.s.inDLSAP.sidPresInd = FALSE;         /* SID Presentation Indicator */
	cfg.t.cfg.s.inDLSAP.minAdrDig = 0;          /* minimum number of address digits */
	cfg.t.cfg.s.inDLSAP.srvOpt = FALSE;
	cfg.t.cfg.s.inDLSAP.callId.len = 0;         /* default call id */
	cfg.t.cfg.s.inDLSAP.redirSubsc = FALSE;      /* subscription to call redirection */
	cfg.t.cfg.s.inDLSAP.redirAdr.eh.pres = NOTPRSNT; /* redirAdr Numbering Plan */
	cfg.t.cfg.s.inDLSAP.forwSubsc = FALSE;        /* programmed forwarding subscription */
	cfg.t.cfg.s.inDLSAP.cndSubsc = TRUE;         /* calling adddress delivery service subscription */
	
	cfg.t.cfg.s.inDLSAP.tmr.t301.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t301.val = 180;
	if (signal_data->timer_t301 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t301.val = signal_data->timer_t301;
	}

	/* It looks like ETSI is the only variant that supports Overlap */
	if (signal_data->switchtype == SNGISDN_SWITCH_EUROISDN) {
		cfg.t.cfg.s.inDLSAP.tmr.t302.enb = TRUE;
		cfg.t.cfg.s.inDLSAP.tmr.t302.val = 15;
	} else {
		cfg.t.cfg.s.inDLSAP.tmr.t302.enb = FALSE;
		cfg.t.cfg.s.inDLSAP.tmr.t302.val = 0;
	}

	if (signal_data->timer_t302 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t302.val = signal_data->timer_t302;
	}

	cfg.t.cfg.s.inDLSAP.tmr.t303.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t303.val = 4;

	if (signal_data->timer_t303 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t303.val = signal_data->timer_t303;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t304.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t304.val = 30;

	if (signal_data->timer_t304 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t304.val = signal_data->timer_t304;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t305.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t305.val = 30;

	if (signal_data->timer_t305 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t305.val = signal_data->timer_t305;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t306.enb = FALSE;
	cfg.t.cfg.s.inDLSAP.tmr.t306.val = 35;

	if (signal_data->timer_t306 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t306.val = signal_data->timer_t306;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t307.enb = FALSE;
	cfg.t.cfg.s.inDLSAP.tmr.t307.val = 35;

	if (signal_data->timer_t307 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t307.val = signal_data->timer_t307;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t308.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t308.val = 4;
	cfg.t.cfg.s.inDLSAP.tmr.t310.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t310.val = 120;

	if (signal_data->timer_t308 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t308.val = signal_data->timer_t308;
	}

	if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
		cfg.t.cfg.s.inDLSAP.tmr.t312.enb = TRUE;
		cfg.t.cfg.s.inDLSAP.tmr.t312.val = cfg.t.cfg.s.inDLSAP.tmr.t303.val+2;
	} else {
		cfg.t.cfg.s.inDLSAP.tmr.t312.enb = FALSE;
	}

	if (signal_data->timer_t310 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t310.val = signal_data->timer_t310;
	}

	if (signal_data->timer_t312 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t312.val = signal_data->timer_t312;
	}

	cfg.t.cfg.s.inDLSAP.tmr.t313.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t313.val = 4;

	if (signal_data->timer_t313 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t313.val = signal_data->timer_t313;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t316.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t316.val = 120;

	if (signal_data->timer_t316 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t316.val = signal_data->timer_t316;
	}

	cfg.t.cfg.s.inDLSAP.tmr.t316c.enb = FALSE;
	cfg.t.cfg.s.inDLSAP.tmr.t316c.val = 35;
	
	cfg.t.cfg.s.inDLSAP.tmr.t318.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t318.val = 4;

	if (signal_data->timer_t318 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t318.val = signal_data->timer_t318;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t319.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t319.val = 4;

	if (signal_data->timer_t319 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t319.val = signal_data->timer_t319;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t322.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.t322.val = 4;

	if (signal_data->timer_t322 > 0) {
		cfg.t.cfg.s.inDLSAP.tmr.t322.val = signal_data->timer_t322;
	}
	
	cfg.t.cfg.s.inDLSAP.tmr.t332.enb = FALSE;
	cfg.t.cfg.s.inDLSAP.tmr.t332.val = 35;

	cfg.t.cfg.s.inDLSAP.tmr.tRst.enb = TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.tRst.val = 8;

	cfg.t.cfg.s.inDLSAP.tmr.tAns.enb = FALSE;  /* non-standard timer */
	cfg.t.cfg.s.inDLSAP.tmr.t396.enb = FALSE;  /* non-standard timer */
	cfg.t.cfg.s.inDLSAP.tmr.t397.enb = TRUE;  /* non-standard timer */
	cfg.t.cfg.s.inDLSAP.tmr.tProg.enb= TRUE;
	cfg.t.cfg.s.inDLSAP.tmr.tProg.val= 35;
#ifdef NI2
#ifdef NI2_TREST
	cfg.t.cfg.s.inDLSAP.tmr.tRest.enb= FALSE;
	cfg.t.cfg.s.inDLSAP.tmr.tRest.val= 35;    /* tRest timer for NI2 */
#endif /* NI2_TREST */
#endif /* NI2 */

	cfg.t.cfg.s.inDLSAP.dstEnt = ENTLD;
	cfg.t.cfg.s.inDLSAP.dstInst = S_INST;
	cfg.t.cfg.s.inDLSAP.dstProcId = SFndProcId();
	cfg.t.cfg.s.inDLSAP.prior = PRIOR0;
	cfg.t.cfg.s.inDLSAP.route = RTESPEC;
	cfg.t.cfg.s.inDLSAP.selector = 0;
	cfg.t.cfg.s.inDLSAP.mem.region = S_REG;
	cfg.t.cfg.s.inDLSAP.mem.pool = S_POOL;

	switch (span->trunk_type) {
		case FTDM_TRUNK_E1:
			cfg.t.cfg.s.inDLSAP.dChannelNum = 16;
			cfg.t.cfg.s.inDLSAP.nmbBearChan = NUM_E1_CHANNELS_PER_SPAN;
			cfg.t.cfg.s.inDLSAP.firstBChanNum = 0;
			cfg.t.cfg.s.inDLSAP.callRefLen = 2;
			cfg.t.cfg.s.inDLSAP.teiAlloc = IN_STATIC;
			cfg.t.cfg.s.inDLSAP.intCfg = IN_INTCFG_PTPT;
			break;
		case FTDM_TRUNK_T1:
		case FTDM_TRUNK_J1:
			/* if NFAS, could be 0 if no signalling */
			cfg.t.cfg.s.inDLSAP.callRefLen = 2;
			cfg.t.cfg.s.inDLSAP.teiAlloc = IN_STATIC;
			cfg.t.cfg.s.inDLSAP.intCfg = IN_INTCFG_PTPT;
			cfg.t.cfg.s.inDLSAP.firstBChanNum = 1;
			
			if (signal_data->nfas.trunk) {
				if (signal_data->nfas.sigchan ==  SNGISDN_NFAS_DCHAN_PRIMARY ||
					signal_data->nfas.sigchan ==  SNGISDN_NFAS_DCHAN_BACKUP) {

					cfg.t.cfg.s.inDLSAP.dChannelNum = 24;
					cfg.t.cfg.s.inDLSAP.nmbBearChan = NUM_T1_CHANNELS_PER_SPAN - 1;
				} else {
					cfg.t.cfg.s.inDLSAP.dChannelNum = 0;
					cfg.t.cfg.s.inDLSAP.nmbBearChan = NUM_T1_CHANNELS_PER_SPAN;
				}
			} else {
				cfg.t.cfg.s.inDLSAP.dChannelNum = 24;
				cfg.t.cfg.s.inDLSAP.nmbBearChan = NUM_T1_CHANNELS_PER_SPAN;
				cfg.t.cfg.s.inDLSAP.firstBChanNum = 1;
			}
			break;
		case FTDM_TRUNK_BRI:
			cfg.t.cfg.s.inDLSAP.dChannelNum = 0; /* Unused for BRI */
			cfg.t.cfg.s.inDLSAP.nmbBearChan = NUM_BRI_CHANNELS_PER_SPAN;
			cfg.t.cfg.s.inDLSAP.firstBChanNum = 1;
			cfg.t.cfg.s.inDLSAP.callRefLen = 1;			
			cfg.t.cfg.s.inDLSAP.teiAlloc = IN_STATIC;
			cfg.t.cfg.s.inDLSAP.intCfg = IN_INTCFG_PTPT;
			break;
		case FTDM_TRUNK_BRI_PTMP:
			cfg.t.cfg.s.inDLSAP.dChannelNum = 0; /* Unused for BRI */
			cfg.t.cfg.s.inDLSAP.nmbBearChan = NUM_BRI_CHANNELS_PER_SPAN;
			cfg.t.cfg.s.inDLSAP.firstBChanNum = 1;
			cfg.t.cfg.s.inDLSAP.callRefLen = 1;
			cfg.t.cfg.s.inDLSAP.teiAlloc = IN_DYNAMIC;
			cfg.t.cfg.s.inDLSAP.intCfg = IN_INTCFG_MULTI;
			break;
		default:
			ftdm_log(FTDM_LOG_ERROR, "%s: Unsupported trunk_type\n", span->name);
			return FTDM_FAIL;
	}

	if (sng_isdn_q931_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_stack_cfg_q931_lce(ftdm_span_t *span)
{
	InMngmt cfg;
	Pst     pst;
	uint8_t	i;
	uint8_t numCes=1;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;
	if (span->trunk_type == FTDM_TRUNK_BRI_PTMP && signal_data->signalling == SNGISDN_SIGNALING_NET) {
		numCes = 8;
	}
	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTIN;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTIN;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STDLC;

	cfg.hdr.response.selector=0;

	cfg.t.cfg.s.inLCe.sapId = signal_data->link_id;

	if (span->trunk_type == FTDM_TRUNK_BRI_PTMP) {
		/* Stack will send Restart CFM's each time link is established (TEI negotiated),
                   and we do not want thi s event */
		cfg.t.cfg.s.inLCe.lnkUpDwnInd = FALSE;
	} else {
		cfg.t.cfg.s.inLCe.lnkUpDwnInd = TRUE;
	}

	if (FTDM_SPAN_IS_BRI(span)) {
		/* tCon Timer causes unwanted hangup on BRI links
			where the Q.921 link goes into disconnected
			state when idle. */

		cfg.t.cfg.s.inLCe.tCon.enb = FALSE;
		cfg.t.cfg.s.inLCe.tCon.val = 0;
	} else {
		cfg.t.cfg.s.inLCe.tCon.enb = TRUE;
		cfg.t.cfg.s.inLCe.tCon.val = 35;
	}
	
	cfg.t.cfg.s.inLCe.tDisc.enb = TRUE;
	cfg.t.cfg.s.inLCe.tDisc.val = 35;
	cfg.t.cfg.s.inLCe.t314.enb = FALSE; /* if segmentation enabled, set to TRUE */
	cfg.t.cfg.s.inLCe.t314.val = 35;

	if (signal_data->nfas.trunk) {
		cfg.t.cfg.s.inLCe.t332i.enb = TRUE;
		cfg.t.cfg.s.inLCe.t332i.val = 35;
	} else {
		cfg.t.cfg.s.inLCe.t332i.enb = FALSE;
		cfg.t.cfg.s.inLCe.t332i.val = 35;
	}

#if (ISDN_NI1 || ISDN_NT || ISDN_ATT)
	cfg.t.cfg.s.inLCe.tSpid.enb = TRUE;
	cfg.t.cfg.s.inLCe.tSpid.val = 5;

	/* In case we want to support BRI - NORTH America, we will need to configure 8 spid's per CES */
	cfg.t.cfg.s.inLCe.spid.pres = NOTPRSNT;
	cfg.t.cfg.s.inLCe.spid.len = 0;
#endif
	cfg.t.cfg.s.inLCe.tRstAck.enb = TRUE;
	cfg.t.cfg.s.inLCe.tRstAck.val = 10;

	cfg.t.cfg.s.inLCe.usid = 0;
	cfg.t.cfg.s.inLCe.tid = 0;

	for(i=0;i<numCes;i++) {
		cfg.t.cfg.s.inLCe.ces = i;
		if (sng_isdn_q931_config(&pst, &cfg)) {
			return FTDM_FAIL;
		}
	}

	return FTDM_SUCCESS;
}


ftdm_status_t sngisdn_stack_cfg_cc_gen(void)
{
	CcMngmt cfg;
	Pst     pst;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTCC;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTCC;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STGEN;

	/* fill in the Gen Conf structures internal pst struct */
	stack_pst_init(&cfg.t.cfg.s.ccGenCfg.smPst);
	cfg.t.cfg.s.ccGenCfg.smPst.dstEnt = ENTSM;

	cfg.t.cfg.s.ccGenCfg.poolTrUpper = 2;
	cfg.t.cfg.s.ccGenCfg.poolTrLower = 1;

	cfg.t.cfg.s.ccGenCfg.nmbSaps	 = MAX_VARIANTS+1; /* Set to number of variants + 1 */

	if (sng_isdn_cc_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}


ftdm_status_t sngisdn_stack_cfg_cc_sap(ftdm_span_t *span)
{
	CcMngmt cfg;
	Pst     pst;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTCC;

	/*clear the configuration structure*/
	memset(&cfg, 0, sizeof(cfg));

	/*fill in some general sections of the header*/
	stack_hdr_init(&cfg.hdr);

	/*fill in the specific fields of the header*/
	cfg.hdr.msgType     = TCFG;
	cfg.hdr.entId.ent   = ENTCC;
	cfg.hdr.entId.inst  = S_INST;
	cfg.hdr.elmId.elmnt = STTSAP;

	cfg.t.cfg.s.ccISAP.pst.srcProcId	= SFndProcId();
    cfg.t.cfg.s.ccISAP.pst.srcEnt		= ENTCC;
    cfg.t.cfg.s.ccISAP.pst.srcInst		= S_INST;
	cfg.t.cfg.s.ccISAP.pst.dstEnt		= ENTIN;
	cfg.t.cfg.s.ccISAP.pst.dstInst		= S_INST;
	cfg.t.cfg.s.ccISAP.pst.dstProcId	= SFndProcId();

	cfg.t.cfg.s.ccISAP.pst.prior		= PRIOR0;
	cfg.t.cfg.s.ccISAP.pst.route		= RTESPEC;
	cfg.t.cfg.s.ccISAP.pst.region		= S_REG;
	cfg.t.cfg.s.ccISAP.pst.pool			= S_POOL;
	cfg.t.cfg.s.ccISAP.pst.selector		= 0;

	cfg.t.cfg.s.ccISAP.suId				= signal_data->cc_id;
	cfg.t.cfg.s.ccISAP.spId				= signal_data->cc_id;

	cfg.t.cfg.s.ccISAP.swtch			= sng_isdn_stack_switchtype(signal_data->switchtype);
	cfg.t.cfg.s.ccISAP.sapType			= SNG_SAP_TYPE_ISDN;

	if (sng_isdn_cc_config(&pst, &cfg)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

/* TODO: see if we can move this to inside the library */
void stack_pst_init(Pst *pst)
{
	memset(pst, 0, sizeof(Pst));
		/*fill in the post structure*/
	pst->dstProcId   = SFndProcId();
	pst->dstInst     = S_INST;

	pst->srcProcId   = SFndProcId();
	pst->srcEnt      = ENTSM;
	pst->srcInst     = S_INST;

	pst->prior       = PRIOR0;
	pst->route       = RTESPEC;
	pst->region      = S_REG;
	pst->pool        = S_POOL;
	pst->selector    = 0;
	return;
}



void stack_hdr_init(Header *hdr)
{
	hdr->msgType                = 0;
	hdr->msgLen                 = 0;
	hdr->entId.ent              = 0;
	hdr->entId.inst             = 0;
	hdr->elmId.elmnt            = 0;
	hdr->elmId.elmntInst1       = 0;
	hdr->elmId.elmntInst2       = 0;
	hdr->elmId.elmntInst3       = 0;
	hdr->seqNmb                 = 0;
	hdr->version                = 0;
	hdr->response.prior         = PRIOR0;
	hdr->response.route         = RTESPEC;
	hdr->response.mem.region    = S_REG;
	hdr->response.mem.pool      = S_POOL;
	hdr->transId                = 0;
	hdr->response.selector      = 0;
	return;
}

uint8_t sng_isdn_stack_switchtype(sngisdn_switchtype_t switchtype)
{
	switch (switchtype) {
		case SNGISDN_SWITCH_NI2:
			return SW_NI2;
		case SNGISDN_SWITCH_5ESS:
			return SW_ATT5EP;
		case SNGISDN_SWITCH_4ESS:
			return SW_ATT4E;
		case SNGISDN_SWITCH_DMS100:
			return SW_NTDMS100P;
		case SNGISDN_SWITCH_EUROISDN:
			return SW_ETSI;
		case SNGISDN_SWITCH_QSIG:
			return SW_QSIG;
		case SNGISDN_SWITCH_INSNET:
			return SW_INSNET;
		case SNGISDN_SWITCH_INVALID:
			ftdm_log(FTDM_LOG_ERROR, "Invalid switchtype: %d\n", switchtype);
			break;
	}
	return 0;
}

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
