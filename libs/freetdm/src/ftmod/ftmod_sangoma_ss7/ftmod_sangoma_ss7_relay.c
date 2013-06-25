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

/*static ftdm_status_t enable_all_ckts_for_relay(void);*/
static ftdm_status_t reconfig_all_ckts_for_relay(void);
static ftdm_status_t disable_all_ckts_for_relay(void);
static ftdm_status_t block_all_ckts_for_relay(uint32_t procId);
/* static ftdm_status_t unblock_all_ckts_for_relay(uint32_t procId); */
static ftdm_status_t disable_all_sigs_for_relay(uint32_t procId);
static ftdm_status_t disble_all_mtp2_sigs_for_relay(void);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
ftdm_status_t handle_relay_connect(RyMngmt *sta)
{
	sng_relay_t	*sng_relay = &g_ftdm_sngss7_data.cfg.relay[sta->t.usta.s.ryUpUsta.id];

	if (!sngss7_test_flag(sng_relay, SNGSS7_RELAY_INIT)) {
		/* SS7_INFO("Relay Channel %d initial connection UP\n", sng_relay->id); */
		sngss7_set_flag(sng_relay, SNGSS7_RELAY_INIT);
	}
	
	SS7_INFO("Relay Channel %d connection UP\n", sng_relay->id);
	if (sng_relay->type == LRY_CT_TCP_CLIENT) {
		if (reconfig_all_ckts_for_relay()) {
			SS7_ERROR("Failed to reconfigure ISUP Ckts!\n");
			/* we're done....this is very bad! */
		}
		return FTDM_SUCCESS;
	} else if (sng_relay->type == LRY_CT_TCP_SERVER) {
		ftmod_ss7_enable_grp_mtp3Link(sta->t.usta.s.ryUpUsta.id);
		/* unbloock the ckts on the client system */
		return FTDM_SUCCESS;
	}
	return FTDM_FAIL;
}

/******************************************************************************/
ftdm_status_t handle_relay_disconnect_on_error(RyMngmt *sta)
{
	SS7_DEBUG("SS7 relay disconnect on error\n");

	/* check which procId is in error, if it is 1, disable the ckts */
	if (sta->t.usta.s.ryErrUsta.errPid == 1 ) {
		/* we've lost the server, bring the sig status down on all ckts */
		disable_all_ckts_for_relay();
		
		/* we've lost the server, bring down the mtp2 links */
		disble_all_mtp2_sigs_for_relay();
	}

	/* check if the channel is a server, means we just lost a MGW */
	if (g_ftdm_sngss7_data.cfg.relay[sta->t.usta.s.ryErrUsta.errPid].type == LRY_CT_TCP_SERVER) {
		/* we've lost the client, bring down all the ckts for this procId */
		block_all_ckts_for_relay(sta->t.usta.s.ryErrUsta.errPid);
		
		/* we've lost the client, bring down all mtp3 links for this procId */
		disable_all_sigs_for_relay(sta->t.usta.s.ryErrUsta.errPid);
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_relay_disconnect_on_down(RyMngmt *sta)
{

	SS7_DEBUG("SS7 relay disconnect on down\n");

	/* check if the channel is a server, means we just lost a MGW */
	if (g_ftdm_sngss7_data.cfg.relay[sta->t.usta.s.ryUpUsta.id].type == LRY_CT_TCP_SERVER) {
		block_all_ckts_for_relay(sta->t.usta.s.ryUpUsta.id);

		disable_all_sigs_for_relay(sta->t.usta.s.ryUpUsta.id);
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t disable_all_ckts_for_relay(void)
{
	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	int					x;	

	SS7_INFO("Disabling all ckts becuase of Relay loss\n");

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
	/**********************************************************************/
		/* make sure this is voice channel */
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
	
			/* get the ftdmchan and ss7_chan_data from the circuit */
			if (extract_chan_data(g_ftdm_sngss7_data.cfg.isupCkt[x].id, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", g_ftdm_sngss7_data.cfg.isupCkt[x].id);
				x++;
				continue;
			}

			/* throw the relay_down flag */
			sngss7_set_ckt_flag(sngss7_info, FLAG_RELAY_DOWN);

			/* throw the channel infId status flags to PAUSED ... they will be executed next process cycle */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_RESUME);
			sngss7_set_ckt_flag(sngss7_info, FLAG_INFID_PAUSED);
		} /* if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) */

		/* move along */
		x++;
	/**********************************************************************/
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */

	return FTDM_SUCCESS;
}
#if 0
/******************************************************************************/
ftdm_status_t enable_all_ckts_for_relay(void)
{
	sngss7_chan_data_t	*sngss7_info = NULL;
	sng_isup_inf_t		*sngIntf = NULL;
	ftdm_channel_t		*ftdmchan = NULL;
	int					x;

	SS7_INFO("Enabling all ckts becuase of Relay connection\n");

	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
	/**********************************************************************/
		/* make sure this is voice channel */
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
	
			/* get the ftdmchan and ss7_chan_data from the circuit */
			if (extract_chan_data(g_ftdm_sngss7_data.cfg.isupCkt[x].id, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", g_ftdm_sngss7_data.cfg.isupCkt[x].id);
				x++;
				continue;
			}

			/* bring the relay_down flag down */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_RELAY_DOWN);

			sngIntf = &g_ftdm_sngss7_data.cfg.isupIntf[g_ftdm_sngss7_data.cfg.isupCkt[x].infId];

			/* check if the interface is paused or resumed */
			if (sngss7_test_flag(sngIntf, SNGSS7_PAUSED)) {
				/* don't bring the channel resume flag up...the interface is down */
				SS7_DEBUG_CHAN(ftdmchan, "ISUP interface (%d) set to paused, not resuming channel\n", sngIntf->id);
			} else {
				SS7_DEBUG_CHAN(ftdmchan, "ISUP interface (%d) set to resume, resuming channel\n", sngIntf->id);
				/* throw the channel infId status flags to PAUSED ... they will be executed next process cycle */
				sngss7_set_ckt_flag(sngss7_info, FLAG_INFID_RESUME);
				sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_PAUSED);
			}
		} /* if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) */

		/* move along */
		x++;
	/**********************************************************************/
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */

	return FTDM_SUCCESS;
}
#endif
/******************************************************************************/
ftdm_status_t reconfig_all_ckts_for_relay(void)
{
	int x;
	sngss7_chan_data_t	*sngss7_info = NULL;

	/* go through all the circuits on our ProcId */
	x = (g_ftdm_sngss7_data.cfg.procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
	/**************************************************************************/
		if ( g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			/* grab the private data structure */
			sngss7_info = g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			
			/* mark the circuit for re-configuration */
			sngss7_set_ckt_flag(sngss7_info, FLAG_CKT_RECONFIG);

			/* clear the relay flag */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_RELAY_DOWN);
		}

		/* move to the next circuit */
		x++;
	/**************************************************************************/
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t block_all_ckts_for_relay(uint32_t procId)
{
	int x;
	int ret;

	/* we just lost connection to this procId, send out a block for all these circuits */
	x = (procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
	/**************************************************************************/
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			/* send a block request via stack manager */
			ret = ftmod_ss7_block_isup_ckt_nowait(g_ftdm_sngss7_data.cfg.isupCkt[x].id);
			if (ret) {
				SS7_INFO("Successfully BLOcked CIC:%d(ckt:%d) due to Relay failure\n", 
							g_ftdm_sngss7_data.cfg.isupCkt[x].cic,
							g_ftdm_sngss7_data.cfg.isupCkt[x].id);
			} else {
				SS7_ERROR("Failed to BLOck CIC:%d(ckt:%d) due to Relay failure\n",
							g_ftdm_sngss7_data.cfg.isupCkt[x].cic,
							g_ftdm_sngss7_data.cfg.isupCkt[x].id);
			}
	
		} /* if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) */

		/* move along */
		x++;
	/**************************************************************************/
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t disable_all_sigs_for_relay(uint32_t procId)
{
	SS7_INFO("Disalbing all sig links on ProcID = %d\n", procId);

	ftmod_ss7_disable_grp_mtp3Link(procId);
	
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t disble_all_mtp2_sigs_for_relay(void)
{
	/* check if there is a local mtp2 link*/
	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP2_STARTED)) {
		SS7_INFO("Disalbing all mtp2 sig links on local system\n");

		ftmod_ss7_disable_grp_mtp2Link(1);
	}

	return FTDM_SUCCESS;

}

#if 0
/******************************************************************************/
static ftdm_status_t unblock_all_ckts_for_relay(uint32_t procId)
{
	int x;
	int ret;

	/* we just got connection to this procId, send out a unblock for all these circuits
	 * since we blocked them when we lost the connection	
 	 */
	x = (procId * 1000) + 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) {
	/**************************************************************************/
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {

			/* send a block request via stack manager */
			ret = ftmod_ss7_unblock_isup_ckt(g_ftdm_sngss7_data.cfg.isupCkt[x].id);
			if (ret) {
				SS7_INFO("Successfully unblocked CIC:%d(ckt:%d) due to Relay connection\n", 
							g_ftdm_sngss7_data.cfg.isupCkt[x].cic,
							g_ftdm_sngss7_data.cfg.isupCkt[x].id);
			} else {
				SS7_ERROR("Failed to unblock CIC:%d(ckt:%d) due to Relay connection\n",
							g_ftdm_sngss7_data.cfg.isupCkt[x].cic,
							g_ftdm_sngss7_data.cfg.isupCkt[x].id);
			}
	
		} /* if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) */

		/* move along */
		x++;
	/**************************************************************************/
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0) */

	return FTDM_SUCCESS;
}
#endif


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
