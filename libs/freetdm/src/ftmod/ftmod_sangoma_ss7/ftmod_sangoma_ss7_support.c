/*
 * Copyright (c) 2009, Konrad Hammel <konrad@sangoma.com>
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

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
uint32_t sngss7_id;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
int check_for_state_change(ftdm_channel_t *ftdmchan);
int check_for_reset(sngss7_chan_data_t *sngss7_info);

unsigned long get_unique_id(void);

ftdm_status_t extract_chan_data(uint32_t circuit, sngss7_chan_data_t **sngss7_info, ftdm_channel_t **ftdmchan);

ftdm_status_t check_if_rx_grs_started(ftdm_span_t *ftdmspan);
ftdm_status_t check_if_rx_grs_processed(ftdm_span_t *ftdmspan);
ftdm_status_t check_if_rx_gra_started(ftdm_span_t *ftdmspan);
ftdm_status_t check_for_res_sus_flag(ftdm_span_t *ftdmspan);

ftdm_status_t process_span_ucic(ftdm_span_t *ftdmspan);

ftdm_status_t encode_subAddrIE_nsap(const char *subAddr, char *subAddrIE, int type);
ftdm_status_t encode_subAddrIE_nat(const char *subAddr, char *subAddrIE, int type);

int find_mtp2_error_type_in_map(const char *err_type);
int find_link_type_in_map(const char *linkType);
int find_switch_type_in_map(const char *switchType);
int find_ssf_type_in_map(const char *ssfType);
int find_cic_cntrl_in_map(const char *cntrlType);

ftdm_status_t check_status_of_all_isup_intf(void);
ftdm_status_t check_for_reconfig_flag(ftdm_span_t *ftdmspan);

void sngss7_send_signal(sngss7_chan_data_t *sngss7_info, ftdm_signal_event_t event_id);
void sngss7_set_sig_status(sngss7_chan_data_t *sngss7_info, ftdm_signaling_status_t status);
ftdm_status_t sngss7_add_var(sngss7_chan_data_t *ss7_info, const char* var, const char* val);
ftdm_status_t sngss7_add_raw_data(sngss7_chan_data_t *sngss7_info, uint8_t* data, ftdm_size_t data_len);
/******************************************************************************/

FTDM_ENUM_NAMES(CKT_FLAGS_NAMES, CKT_FLAGS_STRING)
FTDM_STR2ENUM(ftmod_ss7_ckt_state2flag, ftmod_ss7_ckt_flag2str, sng_ckt_flag_t, CKT_FLAGS_NAMES, 31)

FTDM_ENUM_NAMES(BLK_FLAGS_NAMES, BLK_FLAGS_STRING)
FTDM_STR2ENUM(ftmod_ss7_blk_state2flag, ftmod_ss7_blk_flag2str, sng_ckt_block_flag_t, BLK_FLAGS_NAMES, 31)

/* FUNCTIONS ******************************************************************/
static uint8_t get_trillium_val(ftdm2trillium_t *vals, uint8_t ftdm_val, uint8_t default_val);
static uint8_t get_ftdm_val(ftdm2trillium_t *vals, uint8_t trillium_val, uint8_t default_val);
ftdm_status_t four_char_to_hex(const char* in, uint16_t* out) ;
ftdm_status_t hex_to_four_char(uint16_t in, char* out);


ftdm_status_t hex_to_char(uint16_t in, char* out, int len);
ftdm_status_t char_to_hex(const char* in, uint16_t* out, int len);

/* Maps generic FreeTDM CPC codes to SS7 CPC codes */
ftdm2trillium_t cpc_codes[] = {
	{FTDM_CPC_UNKNOWN,			CAT_UNKNOWN},
	{FTDM_CPC_OPERATOR_FRENCH,	CAT_OPLANGFR},
	{FTDM_CPC_OPERATOR_ENGLISH,	CAT_OPLANGENG},
	{FTDM_CPC_OPERATOR_GERMAN,	CAT_OPLANGGER},
	{FTDM_CPC_OPERATOR_RUSSIAN,	CAT_OPLANGRUS},
	{FTDM_CPC_OPERATOR_SPANISH,	CAT_OPLANGSP},
	{FTDM_CPC_ORDINARY,			CAT_ORD},
	{FTDM_CPC_PRIORITY,			CAT_PRIOR},
	{FTDM_CPC_DATA,				CAT_DATA},
	{FTDM_CPC_TEST,				CAT_TEST},
	{FTDM_CPC_PAYPHONE,			CAT_PAYPHONE},
};

ftdm2trillium_t  bc_cap_codes[] = {
	{FTDM_BEARER_CAP_SPEECH,		ITC_SPEECH},	/* speech as per ATIS-1000113.3.2005 */
	{FTDM_BEARER_CAP_UNRESTRICTED,	ITC_UNRDIG},	/* unrestricted digital as per ATIS-1000113.3.2005 */
	{FTDM_BEARER_CAP_RESTRICTED,	ITC_UNRDIG},	/* Restricted Digital */
	{FTDM_BEARER_CAP_3_1KHZ_AUDIO,	ITC_A31KHZ},	/* 3.1kHz audio as per ATIS-1000113.3.2005 */
	{FTDM_BEARER_CAP_7KHZ_AUDIO,	ITC_A7KHZ},		/* 7Khz audio */
	{FTDM_BEARER_CAP_15KHZ_AUDIO,	ITC_A15KHZ},	/* 15Khz audio */
	{FTDM_BEARER_CAP_VIDEO,			ITC_VIDEO},		/* Video */
};

static uint8_t get_trillium_val(ftdm2trillium_t *vals, uint8_t ftdm_val, uint8_t default_val)
{
	ftdm2trillium_t *val = vals;
	while(val++) {
		if (val->ftdm_val == ftdm_val) {
			return val->trillium_val;
		}
	}
	return default_val;
}

static uint8_t get_ftdm_val(ftdm2trillium_t *vals, uint8_t trillium_val, uint8_t default_val)
{
	ftdm2trillium_t *val = vals;
	while(val++) {
		if (val->trillium_val == trillium_val) {
			return val->ftdm_val;
		}
	}
	return default_val;
}

ftdm_status_t copy_cgPtyNum_from_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *cgPtyNum)
{
	return FTDM_SUCCESS;
}

ftdm_status_t copy_cgPtyNum_to_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *cgPtyNum)
{
	const char *val = NULL;
	const char *clg_nadi = NULL;

	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;

	cgPtyNum->eh.pres		   = PRSNT_NODEF;
	
	cgPtyNum->natAddrInd.pres   = PRSNT_NODEF;
	cgPtyNum->natAddrInd.val = g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].clg_nadi;

	
	cgPtyNum->scrnInd.pres	  = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_screen_ind");
	if (!ftdm_strlen_zero(val)) {
		cgPtyNum->scrnInd.val	= atoi(val);
	} else {
		cgPtyNum->scrnInd.val	= caller_data->screen;
	}
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Calling Party Number Screening Ind %d\n", cgPtyNum->scrnInd.val);
	
	cgPtyNum->presRest.pres	 = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_pres_ind");
	if (!ftdm_strlen_zero(val)) {
		cgPtyNum->presRest.val	= atoi(val);
	} else {
		cgPtyNum->presRest.val	= caller_data->pres;
	}
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Calling Party Number Presentation Ind %d\n", cgPtyNum->presRest.val);

	cgPtyNum->numPlan.pres	  = PRSNT_NODEF;
	cgPtyNum->numPlan.val	   = 0x01;

	cgPtyNum->niInd.pres		= PRSNT_NODEF;
	cgPtyNum->niInd.val		 = 0x00;

	/* check if the user would like a custom NADI value for the calling Pty Num */
	clg_nadi = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_clg_nadi");
	if (!ftdm_strlen_zero(clg_nadi)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Calling NADI value \"%s\"\n", clg_nadi);
		cgPtyNum->natAddrInd.val = atoi(clg_nadi);
	}
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Calling Party Number NADI value %d\n", cgPtyNum->natAddrInd.val);

	return copy_tknStr_to_sngss7(caller_data->cid_num.digits, &cgPtyNum->addrSig, &cgPtyNum->oddEven);
}

ftdm_status_t copy_cdPtyNum_from_sngss7(ftdm_channel_t *ftdmchan, SiCdPtyNum *cdPtyNum)
{
	char var[FTDM_DIGITS_LIMIT];
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;

	if (cdPtyNum->eh.pres == PRSNT_NODEF &&
	    cdPtyNum->natAddrInd.pres 	== PRSNT_NODEF) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Called Party Number NADI %d\n", cdPtyNum->natAddrInd.val);
		sprintf(var, "%d", cdPtyNum->natAddrInd.val);
		sngss7_add_var(sngss7_info, "ss7_cld_nadi", var);
	}
		
	return FTDM_SUCCESS;
}


ftdm_status_t copy_cdPtyNum_to_sngss7(ftdm_channel_t *ftdmchan, SiCdPtyNum *cdPtyNum)
{
	const char	*val = NULL;
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;

	cdPtyNum->eh.pres		   = PRSNT_NODEF;

	cdPtyNum->natAddrInd.pres   = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_cld_nadi");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Called NADI value \"%s\"\n", val);
		cdPtyNum->natAddrInd.val	= atoi(val);
	} else {
		cdPtyNum->natAddrInd.val	= g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].cld_nadi;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "No user supplied NADI value found for CLD, using \"%d\"\n", cdPtyNum->natAddrInd.val);
	}

	cdPtyNum->numPlan.pres	  = PRSNT_NODEF;
	cdPtyNum->numPlan.val	   = 0x01;

	cdPtyNum->innInd.pres	   = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_cld_inn");
	if (!ftdm_strlen_zero(val)) {
		cdPtyNum->innInd.val		= atoi(val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Called INN value \"%s\"\n", val);
	} else {
		cdPtyNum->innInd.val		= 0x01;
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Called INN value, set to default value 0x01\n");
	}
	
	return copy_tknStr_to_sngss7(caller_data->dnis.digits, &cdPtyNum->addrSig, &cdPtyNum->oddEven);
}

ftdm_status_t copy_locPtyNum_from_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *locPtyNum)
{
	return FTDM_SUCCESS;
}

ftdm_status_t copy_locPtyNum_to_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *locPtyNum)
{
        const char *val = NULL;
        const char *loc_nadi = NULL;
	int pres_val = PRSNT_NODEF;

        sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
        ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;

	if (!strcasecmp(caller_data->loc.digits, "NULL")) {
		pres_val = NOTPRSNT;
		return FTDM_SUCCESS;
	}

        locPtyNum->eh.pres = pres_val;
        locPtyNum->natAddrInd.pres = pres_val;
        locPtyNum->natAddrInd.val = g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].loc_nadi;

        locPtyNum->scrnInd.pres = pres_val;
		val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_loc_screen_ind");
        if (!ftdm_strlen_zero(val)) {
			locPtyNum->scrnInd.val = atoi(val);
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Location Screening Ind %d\n", locPtyNum->scrnInd.val);
        } else {
			locPtyNum->scrnInd.val = caller_data->screen;
        }

        locPtyNum->presRest.pres = pres_val;
		val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_loc_pres_ind");
        if (!ftdm_strlen_zero(val)) {
			locPtyNum->presRest.val = atoi(val);
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Location Presentation Ind %d\n", locPtyNum->presRest.val);
        } else {
			locPtyNum->presRest.val = caller_data->pres;
        }

        locPtyNum->numPlan.pres	= pres_val;
        locPtyNum->numPlan.val = 0x01;

        locPtyNum->niInd.pres = pres_val;
        locPtyNum->niInd.val = 0x00;

		/* check if the user would like a custom NADI value for the Location Reference */
        loc_nadi = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_loc_nadi");
        if (!ftdm_strlen_zero(loc_nadi)) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Location Reference NADI value \"%s\"\n", loc_nadi);
			locPtyNum->natAddrInd.val = atoi(loc_nadi);
        } else {
			locPtyNum->natAddrInd.val = g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].loc_nadi;
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "No user supplied NADI value found for LOC, using \"%d\"\n", locPtyNum->natAddrInd.val);
	}

        return copy_tknStr_to_sngss7(caller_data->loc.digits, &locPtyNum->addrSig, &locPtyNum->oddEven);
}

ftdm_status_t copy_genNmb_to_sngss7(ftdm_channel_t *ftdmchan, SiGenNum *genNmb)
{	
	const char *val = NULL;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_gn_digits");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Generic Number qualifier \"%s\"\n", val);
		if (copy_tknStr_to_sngss7((char*)val, &genNmb->addrSig, &genNmb->oddEven) != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
	} else {
		return FTDM_SUCCESS;
	}
	
	genNmb->eh.pres = PRSNT_NODEF;
	genNmb->addrSig.pres = PRSNT_NODEF;
	
	genNmb->nmbQual.pres = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_gn_numqual");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Generic Number \"%s\"\n", val);
		genNmb->nmbQual.val	= atoi(val);
	} else {
		genNmb->nmbQual.val	= g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].gn_nmbqual;
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Generic Number \n");
	}
	genNmb->natAddrInd.pres = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_gn_nadi");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Generic Number \"nature of address\" \"%s\"\n", val);
		genNmb->natAddrInd.val	= atoi(val);
	} else {
		genNmb->natAddrInd.val	= g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].gn_nadi;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Generic Number \"nature of address\" \"%d\"\n", genNmb->natAddrInd.val);
	}
	genNmb->scrnInd.pres = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_gn_screen_ind");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Generic Number \"screening indicator\" \"%s\"\n", val);
		genNmb->scrnInd.val	= atoi(val);
	} else {
		genNmb->natAddrInd.val	= g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].gn_screen_ind;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Generic Number \"screening indicator\" \"%d\"\n", genNmb->natAddrInd.val);
	}
	genNmb->presRest.pres = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_gn_pres_ind");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Generic Number \"presentation indicator\" \"%s\"\n", val);
		genNmb->presRest.val	= atoi(val);
	} else {
		genNmb->presRest.val	= g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].gn_pres_ind;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Generic Number \"presentation indicator\" \"%d\"\n", genNmb->presRest.val);
	}
	genNmb->numPlan.pres = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_gn_npi");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Generic Number \"numbering plan\" \"%s\"\n", val);
		genNmb->numPlan.val	= atoi(val);
	} else {
	genNmb->numPlan.val	= g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].gn_npi;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Generic Number \"numbering plan\" \"%d\"\n", genNmb->numPlan.val);
	}
	genNmb->niInd.pres = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_gn_num_inc_ind");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Generic Number \"number incomplete indicator\" \"%s\"\n", val);
		genNmb->niInd.val	= atoi(val);
	} else {
		genNmb->niInd.val	= g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].gn_num_inc_ind;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Generic Number \"number incomplete indicator\" \"%d\"\n", genNmb->niInd.val);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t copy_genNmb_from_sngss7(ftdm_channel_t *ftdmchan, SiGenNum *genNmb)
{
	char val[64];
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;

	memset(val, 0, sizeof(val));

	if (genNmb->eh.pres != PRSNT_NODEF || genNmb->addrSig.pres != PRSNT_NODEF) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No Generic Number available\n");
		return FTDM_SUCCESS;
	}

	copy_tknStr_from_sngss7(genNmb->addrSig, val, genNmb->oddEven);

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generic Number:%s\n", val);
	sngss7_add_var(sngss7_info, "ss7_gn_digits", val);

	if (genNmb->nmbQual.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", genNmb->nmbQual.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generic Number \"number qualifier\" \"%s\"\n", val);
		sngss7_add_var(sngss7_info, "ss7_gn_numqual", val);
	}

	if (genNmb->natAddrInd.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", genNmb->natAddrInd.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generic Number \"nature of address\" \"%s\"\n", val);
		sngss7_add_var(sngss7_info, "ss7_gn_nadi", val);
	}

	if (genNmb->scrnInd.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", genNmb->scrnInd.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generic Number \"screening indicator\" \"%s\"\n", val);
		sngss7_add_var(sngss7_info, "ss7_gn_screen_ind", val);
	}
	
	if (genNmb->presRest.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", genNmb->presRest.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generic Number \"presentation indicator\" \"%s\"\n", val);
		sngss7_add_var(sngss7_info, "ss7_gn_pres_ind", val);
	}

	if (genNmb->numPlan.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", genNmb->numPlan.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generic Number \"numbering plan\" \"%s\"\n", val);
		sngss7_add_var(sngss7_info, "ss7_gn_npi", val);
	}

	if (genNmb->niInd.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", genNmb->niInd.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Generic Number \"number incomplete indicator\" \"%s\"\n", val);
		sngss7_add_var(sngss7_info, "ss7_gn_num_inc_ind", val);
	}
	
	return FTDM_SUCCESS;
}

ftdm_status_t copy_redirgNum_to_sngss7(ftdm_channel_t *ftdmchan, SiRedirNum *redirgNum)
{
	const char* val = NULL;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;	
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdnis_digits");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Redirection Number\"%s\"\n", val);
		if (copy_tknStr_to_sngss7((char*)val, &redirgNum->addrSig, &redirgNum->oddEven) != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
	} else if (!ftdm_strlen_zero(caller_data->rdnis.digits)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Redirection Number\"%s\"\n", caller_data->rdnis.digits);
		if (copy_tknStr_to_sngss7(caller_data->rdnis.digits, &redirgNum->addrSig, &redirgNum->oddEven) != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
	} else {

		val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdnis_pres_ind");
		if (!ftdm_strlen_zero(val)) {
			redirgNum->presRest.val = atoi(val);
		} 
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number Address Presentation Restricted Ind:%d\n", redirgNum->presRest.val);

		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Redirection Number\n");
		return FTDM_SUCCESS;
	}
	
	redirgNum->eh.pres = PRSNT_NODEF;

	/* Nature of address indicator */
	redirgNum->natAddr.pres = PRSNT_NODEF;
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdnis_nadi");
	if (!ftdm_strlen_zero(val)) {
		redirgNum->natAddr.val = atoi(val);
	} else {		
		redirgNum->natAddr.val = g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].rdnis_nadi;
	}
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number NADI:%d\n", redirgNum->natAddr.val);

	/* Screening indicator */
	redirgNum->scrInd.pres = PRSNT_NODEF;
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdnis_screen_ind");
	if (!ftdm_strlen_zero(val)) {
		redirgNum->scrInd.val = atoi(val);
	} else {
		redirgNum->scrInd.val = FTDM_SCREENING_VERIFIED_PASSED;
	}
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number Screening Ind:%d\n", redirgNum->scrInd.val);
	
	/* Address presentation restricted ind */
	redirgNum->presRest.pres = PRSNT_NODEF;
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdnis_pres_ind");
	if (!ftdm_strlen_zero(val)) {
		redirgNum->presRest.val = atoi(val);
	} else {
		redirgNum->presRest.val =  FTDM_PRES_ALLOWED;
	}
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number Address Presentation Restricted Ind:%d\n", redirgNum->presRest.val);

	/* Numbering plan */
	redirgNum->numPlan.pres = PRSNT_NODEF;

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdnis_plan");
	if (!ftdm_strlen_zero(val)) {
		redirgNum->numPlan.val = atoi(val);
	} else {
		redirgNum->numPlan.val = caller_data->rdnis.plan; 
	}
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number Numbering plan:%d\n", redirgNum->numPlan.val);

	return copy_tknStr_to_sngss7(caller_data->rdnis.digits, &redirgNum->addrSig, &redirgNum->oddEven);
}

ftdm_status_t copy_redirgNum_from_sngss7(ftdm_channel_t *ftdmchan, SiRedirNum *redirgNum)
{
	char val[20];
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;

	if (redirgNum->eh.pres != PRSNT_NODEF || redirgNum->addrSig.pres != PRSNT_NODEF) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No Redirecting Number available\n");
		return FTDM_SUCCESS;
	}

	copy_tknStr_from_sngss7(redirgNum->addrSig, ftdmchan->caller_data.rdnis.digits, redirgNum->oddEven);

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number:%s\n", ftdmchan->caller_data.rdnis.digits);
	snprintf(val, sizeof(val), "%s", ftdmchan->caller_data.rdnis.digits);
	sngss7_add_var(sngss7_info, "ss7_rdnis_digits", val);
	

	if (redirgNum->natAddr.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirgNum->natAddr.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number NADI:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdnis_nadi", val);
		caller_data->rdnis.type = redirgNum->natAddr.val;
	}

	if (redirgNum->scrInd.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirgNum->scrInd.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number Screening Ind:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdnis_screen_ind", val);
	}

	if (redirgNum->presRest.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirgNum->presRest.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number Presentation Ind:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdnis_pres_ind", val);		
	}

	if (redirgNum->numPlan.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirgNum->numPlan.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirecting Number Numbering plan:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdnis_plan", val);
		caller_data->rdnis.plan = redirgNum->numPlan.val;
	}

	return FTDM_SUCCESS;
}

ftdm_status_t copy_redirgInfo_from_sngss7(ftdm_channel_t *ftdmchan, SiRedirInfo *redirInfo)
{
	char val[20];
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;

	if (redirInfo->eh.pres != PRSNT_NODEF ) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No Redirecting Information available\n");
		return FTDM_SUCCESS;
	}

	
	if (redirInfo->redirInd.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirInfo->redirInd.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirection Information - redirection indicator:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdinfo_indicator", val);
	}

	if (redirInfo->origRedirReas.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirInfo->origRedirReas.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirection Information - original redirection reason:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdinfo_orig", val);
	}

	if (redirInfo->redirCnt.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirInfo->redirCnt.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirection Information - redirection count:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdinfo_count", val);
	}

	if (redirInfo->redirReas.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", redirInfo->redirReas.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Redirection Information - redirection reason:%s\n", val);
		sngss7_add_var(sngss7_info, "ss7_rdinfo_reason", val);
	}
		
	return FTDM_SUCCESS;
}

ftdm_status_t copy_redirgInfo_to_sngss7(ftdm_channel_t *ftdmchan, SiRedirInfo *redirInfo)
{
	const char* val = NULL;
	int bProceed = 0;

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdinfo_indicator");
	if (!ftdm_strlen_zero(val)) {
		redirInfo->redirInd.val = atoi(val);
		redirInfo->redirInd.pres = 1;
		bProceed = 1;
	} else {		
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Redirection Information on Redirection Indicator\n");
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdinfo_orig");
	if (!ftdm_strlen_zero(val)) {
		redirInfo->origRedirReas.val = atoi(val);
		redirInfo->origRedirReas.pres = 1;
		bProceed = 1;
	} else {		
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Redirection Information on Original Reasons\n");
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdinfo_count");
	if (!ftdm_strlen_zero(val)) {
		redirInfo->redirCnt.val = atoi(val);
		redirInfo->redirCnt.pres= 1;
		bProceed = 1;
	} else {		
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Redirection Information on Redirection Count\n");
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_rdinfo_reason");
	if (!ftdm_strlen_zero(val)) {
		redirInfo->redirReas.val = atoi(val);
		redirInfo->redirReas.pres = 1;
		bProceed = 1;
	} else {		
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No user supplied Redirection Information on Redirection Reasons\n");
	}

	if( bProceed == 1 ) {
		redirInfo->eh.pres = PRSNT_NODEF;
	} else {
		redirInfo->eh.pres = NOTPRSNT;
	}

	return FTDM_SUCCESS;
}

ftdm_status_t copy_access_transport_from_sngss7(ftdm_channel_t *ftdmchan, SiAccTrnspt *accTrnspt)
{
	char val[3*((MF_SIZE_TKNSTRE + 7) & 0xff8)];
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;

	if (accTrnspt->eh.pres != PRSNT_NODEF || accTrnspt->infoElmts.pres !=PRSNT_NODEF) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No Access Transport IE available\n");
		return FTDM_SUCCESS;
	}

	ftdm_url_encode((const char*)accTrnspt->infoElmts.val, val, accTrnspt->infoElmts.len);
	sngss7_add_var (sngss7_info, "ss7_access_transport_urlenc", val);
	
	return FTDM_SUCCESS;
}
ftdm_status_t copy_access_transport_to_sngss7(ftdm_channel_t *ftdmchan, SiAccTrnspt *accTrnspt)
{
	const char *val = NULL;

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_access_transport_urlenc");
	if (ftdm_strlen_zero(val)) {
		accTrnspt->eh.pres = NOTPRSNT;
		accTrnspt->infoElmts.pres = NOTPRSNT;
	}
	else {
		char *val_dec = NULL;
		int val_len = strlen (val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found Access Transport IE encoded : %s\n", val);

		accTrnspt->eh.pres = PRSNT_NODEF;
		accTrnspt->infoElmts.pres = PRSNT_NODEF;

		val_dec = ftdm_strdup(val);
		ftdm_url_decode(val_dec, (ftdm_size_t*)&val_len);
		memcpy (accTrnspt->infoElmts.val, val_dec, val_len);
		accTrnspt->infoElmts.len = val_len;
		ftdm_safe_free(val_dec);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t copy_ocn_from_sngss7(ftdm_channel_t *ftdmchan, SiOrigCdNum *origCdNum)
{
	char val[20];
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;

	if (origCdNum->eh.pres != PRSNT_NODEF ) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No Original Called Number available\n");
		return FTDM_SUCCESS;
	}

	if (origCdNum->addrSig.pres == PRSNT_NODEF) {
		copy_tknStr_from_sngss7(origCdNum->addrSig, val, origCdNum->oddEven);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Original Called Number - Digits: %s\n", val);
		sngss7_add_var(sngss7_info, "ss7_ocn", val);
	}
	
	if (origCdNum->natAddr.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", origCdNum->natAddr.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Original Called Number - NADI: %s\n", val);
		sngss7_add_var(sngss7_info, "ss7_ocn_nadi", val);
	}
	
	if (origCdNum->numPlan.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", origCdNum->numPlan.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Original Called Number - Plan: %s\n", val);
		sngss7_add_var(sngss7_info, "ss7_ocn_plan", val);
	}

	if (origCdNum->presRest.pres == PRSNT_NODEF) {
		snprintf(val, sizeof(val), "%d", origCdNum->presRest.val);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Original Called Number - Presentation: %s\n", val);
		sngss7_add_var(sngss7_info, "ss7_ocn_pres", val);
	}

	return FTDM_SUCCESS;
}

ftdm_status_t copy_ocn_to_sngss7(ftdm_channel_t *ftdmchan, SiOrigCdNum *origCdNum) 
{
	const char *val = NULL;
	int bProceed = 0;

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_ocn");
	if (!ftdm_strlen_zero(val)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Original Called Number - Digits: %s\n", val);
		if (copy_tknStr_to_sngss7((char*)val, &origCdNum->addrSig, &origCdNum->oddEven) != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
		origCdNum->addrSig.pres = 1;
	} else {
		return FTDM_SUCCESS;
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_ocn_nadi");
	if (!ftdm_strlen_zero(val)) {
		origCdNum->natAddr.val = atoi(val);
		origCdNum->natAddr.pres = 1;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Original Called Number - NADI: %s\n", val);
		bProceed = 1;
	} else {        
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No  user supplied Original Called Number NADI value\n");
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_ocn_plan");
	if (!ftdm_strlen_zero(val)) {
		origCdNum->numPlan.val = atoi(val);
		origCdNum->numPlan.pres = 1;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Original Called Number - Plan: %s\n", val);
		bProceed = 1;
	} else {        
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No  user supplied Original Called Number Plan value\n");
	}

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_ocn_pres");
	if (!ftdm_strlen_zero(val)) {
		origCdNum->presRest.val = atoi(val);
		origCdNum->presRest.pres = 1;
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Original Called Number - Presentation: %s\n", val);
		bProceed = 1;
	} else {        
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No  user supplied Original Called Number Presentation value\n");
	}

	if( bProceed == 1 ) {
		origCdNum->eh.pres = PRSNT_NODEF;
	} else {
		origCdNum->eh.pres = NOTPRSNT;
	}
	
	return FTDM_SUCCESS;
}

ftdm_status_t copy_cgPtyCat_to_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyCat *cgPtyCat)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	
	cgPtyCat->eh.pres 			= PRSNT_NODEF;
	cgPtyCat->cgPtyCat.pres 	= PRSNT_NODEF;
	
	cgPtyCat->cgPtyCat.val = get_trillium_val(cpc_codes, caller_data->cpc, CAT_ORD);
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Calling Party Category:0x%x\n",cgPtyCat->cgPtyCat.val);
	return FTDM_SUCCESS;	
}

ftdm_status_t copy_cgPtyCat_from_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyCat *cgPtyCat)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	
	if (cgPtyCat->eh.pres == PRSNT_NODEF &&
		cgPtyCat->cgPtyCat.pres 	== PRSNT_NODEF) {
		
		caller_data->cpc = get_ftdm_val(cpc_codes, cgPtyCat->cgPtyCat.val, FTDM_CPC_UNKNOWN);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Calling Party Category:0x%x\n", cgPtyCat->cgPtyCat.val);
	}
	return FTDM_SUCCESS;
}


ftdm_status_t copy_accTrnspt_to_sngss7(ftdm_channel_t *ftdmchan, SiAccTrnspt *accTrnspt)
{
	const char			*clg_subAddr = NULL;
	const char			*cld_subAddr = NULL;
	char 				subAddrIE[MAX_SIZEOF_SUBADDR_IE];
	
	/* check if the user would like us to send a clg_sub-address */
	clg_subAddr = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_clg_subaddr");
	if (!ftdm_strlen_zero(clg_subAddr)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Calling Sub-Address value \"%s\"\n", clg_subAddr);
		
		/* clean out the subAddrIE */
		memset(subAddrIE, 0x0, sizeof(subAddrIE));

		/* check the first character in the sub-address to see what type of encoding to use */
		switch (clg_subAddr[0]) {
			case '0':						/* NSAP */
				encode_subAddrIE_nsap(&clg_subAddr[1], subAddrIE, SNG_CALLING);
				break;
				case '1':						/* national variant */
					encode_subAddrIE_nat(&clg_subAddr[1], subAddrIE, SNG_CALLING);
					break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Invalid Calling Sub-Address encoding requested: %c\n", clg_subAddr[0]);
				break;
		} /* switch (cld_subAddr[0]) */


		/* if subaddIE is still empty don't copy it in */
		if (subAddrIE[0] != '0') {
			/* check if the clg_subAddr has already been added */
			if (accTrnspt->eh.pres == PRSNT_NODEF) {
				/* append the subAddrIE */
				memcpy(&accTrnspt->infoElmts.val[accTrnspt->infoElmts.len], subAddrIE, (subAddrIE[1] + 2));
				accTrnspt->infoElmts.len		= accTrnspt->infoElmts.len +subAddrIE[1] + 2;
			} else {
				/* fill in from the beginning */
				accTrnspt->eh.pres			= PRSNT_NODEF;
				accTrnspt->infoElmts.pres	= PRSNT_NODEF;
				memcpy(accTrnspt->infoElmts.val, subAddrIE, (subAddrIE[1] + 2));
				accTrnspt->infoElmts.len		= subAddrIE[1] + 2;
			}
		}
	}	

	/* check if the user would like us to send a cld_sub-address */
	cld_subAddr = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_cld_subaddr");
	if ((cld_subAddr != NULL) && (*cld_subAddr)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Found user supplied Called Sub-Address value \"%s\"\n", cld_subAddr);
		
		/* clean out the subAddrIE */
		memset(subAddrIE, 0x0, sizeof(subAddrIE));

		/* check the first character in the sub-address to see what type of encoding to use */
		switch (cld_subAddr[0]) {
			case '0':						/* NSAP */
				encode_subAddrIE_nsap(&cld_subAddr[1], subAddrIE, SNG_CALLED);
				break;
				case '1':						/* national variant */
					encode_subAddrIE_nat(&cld_subAddr[1], subAddrIE, SNG_CALLED);
					break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Invalid Called Sub-Address encoding requested: %c\n", cld_subAddr[0]);
				break;
		} /* switch (cld_subAddr[0]) */

		/* if subaddIE is still empty don't copy it in */
		if (subAddrIE[0] != '0') {
			/* check if the cld_subAddr has already been added */
			if (accTrnspt->eh.pres == PRSNT_NODEF) {
				/* append the subAddrIE */
				memcpy(&accTrnspt->infoElmts.val[accTrnspt->infoElmts.len], subAddrIE, (subAddrIE[1] + 2));
				accTrnspt->infoElmts.len		= accTrnspt->infoElmts.len +subAddrIE[1] + 2;
			} else {
				/* fill in from the beginning */
				accTrnspt->eh.pres			= PRSNT_NODEF;
				accTrnspt->infoElmts.pres	= PRSNT_NODEF;
				memcpy(accTrnspt->infoElmts.val, subAddrIE, (subAddrIE[1] + 2));
				accTrnspt->infoElmts.len		= subAddrIE[1] + 2;
			}
		}
	} /* if ((cld_subAddr != NULL) && (*cld_subAddr)) */
	return FTDM_SUCCESS;
}

ftdm_status_t copy_natConInd_to_sngss7(ftdm_channel_t *ftdmchan, SiNatConInd *natConInd)
{
	/* copy down the nature of connection indicators */
	natConInd->eh.pres 				= PRSNT_NODEF;
	natConInd->satInd.pres 			= PRSNT_NODEF;
	natConInd->satInd.val 			= 0; /* no satellite circuit */
	natConInd->contChkInd.pres 		= PRSNT_NODEF;
	natConInd->contChkInd.val 		= CONTCHK_NOTREQ;
	natConInd->echoCntrlDevInd.pres	= PRSNT_NODEF;
	natConInd->echoCntrlDevInd.val 	= ECHOCDEV_INCL;
	return FTDM_SUCCESS;
}

ftdm_status_t four_char_to_hex(const char* in, uint16_t* out) 
{
	int i= 4; 
	char a, b, c, d;
	if (!in || 4>strlen(in)) {
		return FTDM_FAIL;
	}
	while(i)
	{
		switch((char)*(in+(4-i))) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':			
			if (i==4) {
				d = *(in+(4-i)) - 48;
			} else if (i==3) {
				c = *(in+(4-i)) - 48;
			} else if (i==2) {
				b = *(in+(4-i)) - 48;
			} else {
				a = *(in+(4-i)) - 48;
			}
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':		
			if (i==4) {
				d = *(in+(4-i)) - 55;
			} else if (i==3) {
				c = *(in+(4-i)) - 55;
			} else if (i==2) {
				b = *(in+(4-i)) - 55;
			} else {
				a = *(in+(4-i)) - 55;
			}
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':		
			if (i==4) {
				d = *(in+(4-i)) - 87;
			} else if (i==3) {
				c = *(in+(4-i)) - 87;
			} else if (i==2) {
				b = *(in+(4-i)) - 87;
			} else {
				a = *(in+(4-i)) - 87;
			}
			break;
		default:
			SS7_ERROR("Invalid character found when decoding hex string, %c!\n", *(in+(4-i)) );
			break;
		}
		i--;
	};

	*out |= d;
	*out = *out<<4;
	*out |= c;
	*out = *out<<4;
	*out |= b;
	*out = *out<<4;
	*out |= a;

	return FTDM_SUCCESS;
}

ftdm_status_t char_to_hex(const char* in, uint16_t* out, int len) 
{
	int i= len; 
	char *val = ftdm_malloc(len*sizeof(char));
	
	if (!val ||!in || len>strlen(in)) {
		return FTDM_FAIL;
	}
	
	while(i)
	{
		switch((char)*(in+(len-i))) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':	
			*(val+(len-i)) = *(in+(len-i)) - 48;
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':	
			*(val+(len-i)) = *(in+(len-i)) - 55;
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':		
			*(val+(len-i)) = *(in+(len-i)) - 87;
			break;
		default:
			SS7_ERROR("Invalid character found when decoding hex string, %c!\n", *(in+(len-i)) );
			break;
		}
		i--;
	};

	for (i=0; i<=len-1; i++) {
		*out = *out << 4;
		*out |= *(val+i);
	}

	return FTDM_SUCCESS;
}



ftdm_status_t hex_to_char(uint16_t in, char* out, int len) 
{
	char val=0;
	int mask = 0xf;
	int i=0;
	if (!out)  {
		return FTDM_SUCCESS;
	}

	for (i=len-1; i>=0; i--) {
		val = (in & (mask<<(4*i))) >> (4*i);
		sprintf (out+(len-1-i), "%x", val);
	}
	
	return FTDM_SUCCESS;
}
ftdm_status_t hex_to_four_char(uint16_t in, char* out) 
{
	char val=0;
	int mask = 0xf;
	int i=0;
	if (!out)  {
		return FTDM_SUCCESS;
	}

	for (i=3; i>=0; i--) {
		val = (in & (mask<<(4*i))) >> (4*i);
		sprintf (out+(3-i), "%x", val);
	}
	
	return FTDM_SUCCESS;
}

ftdm_status_t copy_NatureOfConnection_to_sngss7(ftdm_channel_t *ftdmchan, SiNatConInd *natConInd)
{
	const char *val = NULL;

	natConInd->eh.pres 				= PRSNT_NODEF;
	natConInd->satInd.pres 			= PRSNT_NODEF;
	natConInd->contChkInd.pres		= PRSNT_NODEF;;
	natConInd->echoCntrlDevInd.pres 	= PRSNT_NODEF;
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_nature_connection_hex");
	if (!ftdm_strlen_zero(val)) {
		uint16_t val_hex = 0;		
		if (char_to_hex (val, &val_hex, 2) == FTDM_FAIL) {
			SS7_ERROR ("Wrong value set in ss7_iam_nature_connection_hex variable. Please correct the error. Setting to default values.\n" );
		} else {
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "hex =  0x%x\n", val_hex);
			natConInd->satInd.val 			= (val_hex & 0x3);
			natConInd->contChkInd.val		= (val_hex & 0xc)>>2;
			natConInd->echoCntrlDevInd.val	= (val_hex & 0x10) >> 4;

			return FTDM_SUCCESS;
		}
	} 
	
	natConInd->satInd.val 			= 0;
	natConInd->contChkInd.val		= 0;
	natConInd->echoCntrlDevInd.val 	= 0;

	return FTDM_SUCCESS;
}

ftdm_status_t copy_NatureOfConnection_from_sngss7(ftdm_channel_t *ftdmchan, SiNatConInd *natConInd )
{
	char val[3];
	uint16_t val_hex = 0;
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;

	memset (val, 0, 3*sizeof(char));
	if (natConInd->eh.pres != PRSNT_NODEF ) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No nature of connection indicator IE available\n");
		return FTDM_SUCCESS;
	}

	val_hex |= natConInd->satInd.val;
	val_hex |= natConInd->contChkInd.val << 2;
	val_hex |= natConInd->echoCntrlDevInd.val <<4;
	hex_to_char(val_hex, val, 2) ;
	
	sngss7_add_var(sngss7_info, "ss7_iam_nature_connection_hex", val);
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Nature of connection indicator Hex: 0x%s\n", val);
	
	return FTDM_SUCCESS;
}

ftdm_status_t copy_fwdCallInd_hex_from_sngss7(ftdm_channel_t *ftdmchan, SiFwdCallInd *fwdCallInd)
{
	char val[5];
	uint16_t val_hex = 0;
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;

	memset (val, 0, 5*sizeof(char));
	if (fwdCallInd->eh.pres != PRSNT_NODEF ) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No forward call indicator IE available\n");
		return FTDM_SUCCESS;
	}

	val_hex |= fwdCallInd->natIntCallInd.val << 8; 
	val_hex |= (fwdCallInd->end2EndMethInd.val & 0x1) << 9;
	val_hex |= ((fwdCallInd->end2EndMethInd.val & 0x2)>>1) << 10;
	val_hex |= fwdCallInd->intInd.val << 11;
	val_hex |= fwdCallInd->end2EndInfoInd.val << 12;
	val_hex |= fwdCallInd->isdnUsrPrtInd.val << 13;
	val_hex |= (fwdCallInd->isdnUsrPrtPrfInd.val & 0x1) << 14;
	val_hex |= ((fwdCallInd->isdnUsrPrtPrfInd.val & 0x2)>>1) << 15;
	
	val_hex |= fwdCallInd->isdnAccInd.val;
	hex_to_four_char(val_hex, val) ;
	
	sngss7_add_var(sngss7_info, "ss7_iam_fwd_ind_hex", val);
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Forwad Call Indicator Hex: 0x%s\n", val);
	
	return FTDM_SUCCESS;
}

ftdm_status_t copy_fwdCallInd_to_sngss7(ftdm_channel_t *ftdmchan, SiFwdCallInd *fwdCallInd)
{
	const char *val = NULL;
	int acc_val = ISDNACC_ISDN;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;
	
	fwdCallInd->eh.pres 				= PRSNT_NODEF;
	fwdCallInd->natIntCallInd.pres 		= PRSNT_NODEF;
	fwdCallInd->end2EndMethInd.pres 	= PRSNT_NODEF;
	fwdCallInd->intInd.pres 			= PRSNT_NODEF;
	fwdCallInd->end2EndInfoInd.pres 	= PRSNT_NODEF;
	fwdCallInd->isdnUsrPrtInd.pres 		= PRSNT_NODEF;
	fwdCallInd->isdnUsrPrtPrfInd.pres 	= PRSNT_NODEF;
	fwdCallInd->isdnAccInd.pres 		= PRSNT_NODEF;
	fwdCallInd->sccpMethInd.pres 		= PRSNT_NODEF;
	fwdCallInd->sccpMethInd.val 		= SCCPMTH_NOIND;
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_fwd_ind_hex");
	if (!ftdm_strlen_zero(val)) {
		uint16_t val_hex = 0;
		if (four_char_to_hex (val, &val_hex) == FTDM_FAIL) {
			SS7_ERROR ("Wrong value set in iam_fwd_ind_HEX variable. Please correct the error. Setting to default values.\n" );
		} else {
			fwdCallInd->natIntCallInd.val 		= (val_hex & 0x100)>>8;
			fwdCallInd->end2EndMethInd.val 	= (val_hex & 0x600)>>9;
			fwdCallInd->intInd.val 			= (val_hex & 0x800)>>11;
			fwdCallInd->end2EndInfoInd.val 	= (val_hex & 0x1000)>>12;
			fwdCallInd->isdnUsrPrtInd.val 		= (val_hex & 0x2000)>>13;
			fwdCallInd->isdnUsrPrtPrfInd.val 	= (val_hex & 0xC000)>>14;
			fwdCallInd->isdnUsrPrtPrfInd.val 	= (fwdCallInd->isdnUsrPrtPrfInd.val==0x03)?0x0:fwdCallInd->isdnUsrPrtPrfInd.val;
			fwdCallInd->isdnAccInd.val 		= val_hex & 0x1;
			
			if ((g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].switchType == LSI_SW_ANS88) ||
				(g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].switchType == LSI_SW_ANS92) ||
				(g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].switchType == LSI_SW_ANS95)) {

				/* include only if we're running ANSI */
				fwdCallInd->transCallNInd.pres   = PRSNT_NODEF;
				fwdCallInd->transCallNInd.val    = 0x0;
			}

			return FTDM_SUCCESS;
		}
	} 

	fwdCallInd->natIntCallInd.val 		= 0x00;
	fwdCallInd->end2EndMethInd.val 	= E2EMTH_NOMETH;
	fwdCallInd->intInd.val 			= INTIND_NOINTW;
	fwdCallInd->end2EndInfoInd.val 	= E2EINF_NOINFO;
	fwdCallInd->isdnUsrPrtInd.val 		= ISUP_USED;
	fwdCallInd->isdnUsrPrtPrfInd.val 	= PREF_PREFAW;

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_fwd_ind_isdn_access_ind");
	if (ftdm_strlen_zero(val)) {
		/* Kept for backward compatibility */
		val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "iam_fwd_ind_isdn_access_ind");
	}

	if (!ftdm_strlen_zero(val)) {
		acc_val = (int)atoi(val);
	}

	fwdCallInd->isdnAccInd.val 		= acc_val;

	if ((g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].switchType == LSI_SW_ANS88) ||
		(g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].switchType == LSI_SW_ANS92) ||
		(g_ftdm_sngss7_data.cfg.isupCkt[sngss7_info->circuit->id].switchType == LSI_SW_ANS95)) {

		/* include only if we're running ANSI */
		fwdCallInd->transCallNInd.pres   = PRSNT_NODEF;
		fwdCallInd->transCallNInd.val    = 0x0;
	}

	return FTDM_SUCCESS;
}

ftdm_status_t copy_txMedReq_to_sngss7(ftdm_channel_t *ftdmchan, SiTxMedReq *txMedReq)
{
	txMedReq->eh.pres 		= PRSNT_NODEF;
	txMedReq->trMedReq.pres = PRSNT_NODEF;
	txMedReq->trMedReq.val 	= ftdmchan->caller_data.bearer_capability;

	return FTDM_SUCCESS;
}

ftdm_status_t copy_usrServInfoA_to_sngss7(ftdm_channel_t *ftdmchan, SiUsrServInfo *usrServInfoA)
{
	int bProceed = 0;
	const char *val = NULL;
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_trans_cap");
	if (!ftdm_strlen_zero(val)) {
		int itc_type = 0;
		if (!strcasecmp(val, "SPEECH")) {
			itc_type = ITC_SPEECH;
		} else if (!strcasecmp(val, "UNRESTRICTED")) {
			itc_type = ITC_UNRDIG;
		} else if (!strcasecmp(val, "RESTRICTED")) {
			itc_type = ITC_RESDIG;
		} else if (!strcasecmp(val, "31KHZ")) {
			itc_type = ITC_A31KHZ;
		} else if (!strcasecmp(val, "7KHZ")) {
			itc_type = ITC_A7KHZ;
		} else if (!strcasecmp(val, "15KHZ")) {
			itc_type = ITC_A15KHZ;
		} else if (!strcasecmp(val, "VIDEO")) {
			itc_type = ITC_VIDEO;
		} else {
			itc_type = ITC_SPEECH;
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI transmission capability parameter is wrong : %s. Setting to default SPEECH. \n", val );
		}
		
		usrServInfoA->infoTranCap.pres	= PRSNT_NODEF;
		usrServInfoA->infoTranCap.val = get_trillium_val(bc_cap_codes, ftdmchan->caller_data.bearer_capability, itc_type);
		bProceed = 1;		
	} else {
		usrServInfoA->infoTranCap.pres	= NOTPRSNT;
	}

	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_code_standard");
	if (!ftdm_strlen_zero(val)) {		
		usrServInfoA->cdeStand.pres			= PRSNT_NODEF;
		usrServInfoA->cdeStand.val			= (int)atoi(val);	/* default is 0x0 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI coding standard = %d\n", usrServInfoA->cdeStand.val );
		bProceed = 1;
	} else {
		usrServInfoA->cdeStand.pres			= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_trans_mode");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->tranMode.pres			= PRSNT_NODEF;
		usrServInfoA->tranMode.val			= (int)atoi(val);				/* transfer mode, default is 0x0*/
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI transfer mode = %d\n", usrServInfoA->tranMode.val );
		bProceed = 1;
	} else {
		usrServInfoA->tranMode.pres			= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_trans_rate_0");	
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->infoTranRate0.pres		= PRSNT_NODEF;
		usrServInfoA->infoTranRate0.val		= (int)atoi(val);			/* default is 0x10, 64kbps origination to destination*/
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI trans rate 0 = %d\n", usrServInfoA->infoTranRate0.val );
		bProceed = 1;
	} else {
		usrServInfoA->infoTranRate0.pres		= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_trans_rate_1");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->infoTranRate1.pres		= PRSNT_NODEF;
		usrServInfoA->infoTranRate1.val		= (int)atoi(val);			/* 64kbps destination to origination, default is 0x10 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI trans rate 1 = %d\n", usrServInfoA->infoTranRate1.val );
		bProceed = 1;
	} else {
		usrServInfoA->infoTranRate1.pres		= NOTPRSNT;
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_layer1_ident");
	if (!ftdm_strlen_zero(val)) {		
		usrServInfoA->lyr1Ident.pres			= PRSNT_NODEF;
		usrServInfoA->lyr1Ident.val			= (int)atoi(val);		/*default value is 0x01 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI layer 1 indentification = %d\n", usrServInfoA->lyr1Ident.val );
		bProceed = 1;
	} else {
		usrServInfoA->lyr1Ident.pres			= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_layer1_prot");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->usrInfLyr1Prot.pres		= PRSNT_NODEF;
		usrServInfoA->usrInfLyr1Prot.val		= (int)atoi(val);		/*default value is 0x02 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI layer 1 protocol = %d\n", usrServInfoA->usrInfLyr1Prot.val );
		bProceed = 1;
	} else {
		usrServInfoA->usrInfLyr1Prot.pres		= NOTPRSNT;
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_layer2_ident");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->lyr2Ident.pres			= PRSNT_NODEF;
		usrServInfoA->lyr2Ident.val			= (int)atoi(val);		/*default value is 0x01 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI layer 2 indentification = %d\n", usrServInfoA->lyr2Ident.val );
		bProceed = 1;
	} else {
		usrServInfoA->lyr2Ident.pres			= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_layer2_prot");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->usrInfLyr2Prot.pres		= PRSNT_NODEF;
		usrServInfoA->usrInfLyr2Prot.val		= (int)atoi(val);		/*default value is 0x02 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI layer 2 protocol = %d\n", usrServInfoA->usrInfLyr2Prot.val );
		bProceed = 1;
	} else {
		usrServInfoA->usrInfLyr2Prot.pres		= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_layer3_ident");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->lyr3Ident.pres			= PRSNT_NODEF;
		usrServInfoA->lyr3Ident.val			= (int)atoi(val);		/*default value is 0x01 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI layer 3 indentification = %d\n", usrServInfoA->lyr3Ident.val );
		bProceed = 1;
	} else {
		usrServInfoA->lyr3Ident.pres			= NOTPRSNT;
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_layer3_prot");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->usrInfLyr3Prot.pres		= PRSNT_NODEF;
		usrServInfoA->usrInfLyr3Prot.val		= (int)atoi(val);		/*default value is 0x02 */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI layer 3 protocol = %d\n", usrServInfoA->usrInfLyr3Prot.val );
		bProceed = 1;
	} else {
		usrServInfoA->usrInfLyr3Prot.pres		= NOTPRSNT;
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_chan_struct");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->chanStruct.pres			= PRSNT_NODEF;
		usrServInfoA->chanStruct.val			= (int)atoi(val);                          /* default value is 0x1, 8kHz integrity */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI channel structure = %d\n", usrServInfoA->chanStruct.val );
		bProceed = 1;
	} else {
		usrServInfoA->chanStruct.pres			= NOTPRSNT;
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_config");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->config.pres				= PRSNT_NODEF;
		usrServInfoA->config.val				= (int)atoi(val);                          /* default value is 0x0, point to point configuration */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI configuration = %d\n", usrServInfoA->config.val );
		bProceed = 1;
	} else {
		usrServInfoA->config.pres				= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_establish");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->establish.pres			= PRSNT_NODEF;
		usrServInfoA->establish.val			= (int)atoi(val);                           /* default value is 0x0, on demand */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI establishment = %d\n", usrServInfoA->establish.val );
		bProceed = 1;
	} else {
		usrServInfoA->establish.pres			= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_symmetry");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->symmetry.pres			= PRSNT_NODEF;
		usrServInfoA->symmetry.val			= (int)atoi(val);                           /* default value is 0x0, bi-directional symmetric */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI symmetry = %d\n", usrServInfoA->symmetry.val );
		bProceed = 1;
	} else {
		usrServInfoA->symmetry.pres			= NOTPRSNT;	
	}
	
	val = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam_usi_rate_multiplier");
	if (!ftdm_strlen_zero(val)) {
		usrServInfoA->rateMultiplier.pres		= PRSNT_NODEF;
		usrServInfoA->rateMultiplier.val		= (int)atoi(val);                           /* default value is 0x1, 1x rate multipler */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "USI rate multipier = %d\n", usrServInfoA->rateMultiplier.val );
		bProceed = 1;
	} else {
		usrServInfoA->rateMultiplier.pres		= NOTPRSNT;
	}
	
	if (bProceed) {
		usrServInfoA->eh.pres				= PRSNT_NODEF;
	} else {
		usrServInfoA->eh.pres				= NOTPRSNT;
	}

	return FTDM_SUCCESS;
}



ftdm_status_t copy_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven)
{
	uint8_t i;
	uint8_t j;

	/* check if the token string is present */
	if (str.pres == 1) {
		j = 0;

		for (i = 0; i < str.len; i++) {
			sprintf(&ftdm[j], "%X", (str.val[i] & 0x0F));
			j++;
			sprintf(&ftdm[j], "%X", ((str.val[i] & 0xF0) >> 4));
			j++;
		}

		/* if the odd flag is up the last digit is a fake "0" */
		if ((oddEven.pres == 1) && (oddEven.val == 1)) {
			ftdm[j-1] = '\0';
		} else {
			ftdm[j] = '\0';
		}

		
	} else {
		SS7_ERROR("Asked to copy tknStr that is not present!\n");
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t append_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven)
{
	int i = 0;
	int j = 0;

	/* check if the token string is present */
	if (str.pres == 1) {
		/* find the length of the digits so far */
		j = strlen(ftdm);

		/* confirm that we found an acceptable length */
		if ( j > 25 ) {
			SS7_ERROR("string length exceeds maxium value...aborting append!\n");
			return FTDM_FAIL;
		} /* if ( j > 25 ) */

		/* copy in digits */
		for (i = 0; i < str.len; i++) {
			/* convert 4 bit integer to char and copy into lower nibblet*/
			sprintf(&ftdm[j], "%X", (str.val[i] & 0x0F));
			/* move along */
			j++;
			/* convert 4 bit integer to char and copy into upper nibblet */
			sprintf(&ftdm[j], "%X", ((str.val[i] & 0xF0) >> 4));
			/* move along */
			j++;
		} /* for (i = 0; i < str.len; i++) */

		/* if the odd flag is up the last digit is a fake "0" */
		if ((oddEven.pres == 1) && (oddEven.val == 1)) {
			ftdm[j-1] = '\0';
		} else {
			ftdm[j] = '\0';
		} /* if ((oddEven.pres == 1) && (oddEven.val == 1)) */
	} else {
		SS7_ERROR("Asked to copy tknStr that is not present!\n");
		return FTDM_FAIL;
	} /* if (str.pres == 1) */

	return FTDM_SUCCESS;
}


ftdm_status_t copy_tknStr_to_sngss7(char* val, TknStr *tknStr, TknU8 *oddEven)
{
	char tmp[2];
	int k = 0;
	int j = 0;
	uint8_t flag = 0;
	uint8_t odd = 0;

	uint8_t lower = 0x0;
	uint8_t upper = 0x0;

	tknStr->pres = PRSNT_NODEF;
	
	/* atoi will search through memory starting from the pointer it is given until
	* it finds the \0...since tmp is on the stack it will start going through the
	* possibly causing corruption.  Hard code a \0 to prevent this
	*/
	tmp[1] = '\0';

	while (1) {
		/* grab a digit from the ftdm digits */
		tmp[0] = val[k];

		/* check if the digit is a number and that is not null */
		while (!(isxdigit(tmp[0])) && (tmp[0] != '\0')) {
			if (tmp[0] == '*') {
				/* Could not find a spec that specifies this , but on customer system, * was transmitted as 0x0b */
				SS7_DEBUG("Replacing * with 0x0b");
				k++;
				tmp[0] = 0x0b;
			} else {
				SS7_INFO("Dropping invalid digit: %c\n", tmp[0]);
				/* move on to the next value */
				k++;
				tmp[0] = val[k];
			}
		} /* while(!(isdigit(tmp))) */

		/* check if tmp is null or a digit */
		if (tmp[0] != '\0') {
			/* push it into the lower nibble */
			lower = strtol(&tmp[0], (char **)NULL, 16);
			/* move to the next digit */
			k++;
			/* grab a digit from the ftdm digits */
			tmp[0] = val[k];

			/* check if the digit is a number and that is not null */
			while (!(isxdigit(tmp[0])) && (tmp[0] != '\0')) {
				SS7_INFO("Dropping invalid digit: %c\n", tmp[0]);
				k++;
				tmp[0] = val[k];
			} /* while(!(isdigit(tmp))) */

			/* check if tmp is null or a digit */
			if (tmp[0] != '\0') {
				/* push the digit into the upper nibble */
				upper = (strtol(&tmp[0], (char **)NULL, 16)) << 4;
			} else {
				/* there is no upper ... fill in 0 */
				upper = 0x0;
				/* throw the odd flag */
				odd = 1;
				/* throw the end flag */
				flag = 1;
			} /* if (tmp != '\0') */
		} else {
			/* keep the odd flag down */
			odd = 0;
			/* break right away since we don't need to write the digits */
			break;
		}

		/* push the digits into the trillium structure */
		tknStr->val[j] = upper | lower;

		/* increment the trillium pointer */
		j++;

		/* if the flag is up we're through all the digits */
		if (flag) break;

		/* move to the next digit */
		k++;
	} /* while(1) */
	
	tknStr->len = j;
	oddEven->pres = PRSNT_NODEF;
	oddEven->val = odd;
	return FTDM_SUCCESS;
}



/******************************************************************************/
int check_for_state_change(ftdm_channel_t *ftdmchan)
{

	/* check to see if there are any pending state changes on the channel and give them a sec to happen*/
	ftdm_wait_for_flag_cleared(ftdmchan, FTDM_CHANNEL_STATE_CHANGE, 500);

	/* check the flag to confirm it is clear now */

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
		/* the flag is still up...so we have a problem */
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "FTDM_CHANNEL_STATE_CHANGE flag set for over 500ms, channel state = %s\n",
									ftdm_channel_state2str (ftdmchan->state));

		return 1;
	}

	return 0;
}

/******************************************************************************/
ftdm_status_t extract_chan_data(uint32_t circuit, sngss7_chan_data_t **sngss7_info, ftdm_channel_t **ftdmchan)
{
	if (!g_ftdm_sngss7_data.cfg.isupCkt[circuit].obj) {
		SS7_ERROR("No ss7 info for circuit #%d\n", circuit);
		return FTDM_FAIL;
	}

	*sngss7_info = g_ftdm_sngss7_data.cfg.isupCkt[circuit].obj;

	if (!(*sngss7_info)->ftdmchan) {
		SS7_ERROR("No ftdmchan for circuit #%d\n", circuit);
		return FTDM_FAIL;
	}

	if (!(*sngss7_info)->ftdmchan->span) {
		SS7_CRITICAL("ftdmchan->span = NULL for circuit #%d\n",circuit);
		return FTDM_FAIL;
		
	}
	if (!(*sngss7_info)->ftdmchan->span->signal_data) {
		SS7_CRITICAL("ftdmchan->span->signal_data = NULL for circuit #%d\n",circuit);
		return FTDM_FAIL;
		
	}

	*ftdmchan = (*sngss7_info)->ftdmchan;
	return FTDM_SUCCESS;
}

/******************************************************************************/
int check_for_reset(sngss7_chan_data_t *sngss7_info)
{

	if (sngss7_test_ckt_flag(sngss7_info,FLAG_RESET_RX)) {
		return 1;
	}
	
	if (sngss7_test_ckt_flag(sngss7_info,FLAG_RESET_TX)) {
		return 1;
	}
	
	if (sngss7_test_ckt_flag(sngss7_info,FLAG_GRP_RESET_RX)) {
		return 1;
	}
	
	if (sngss7_test_ckt_flag(sngss7_info,FLAG_GRP_RESET_TX)) {
		return 1;
	}

	return 0;
	
}

/******************************************************************************/
unsigned long get_unique_id(void)
{
	int	procId = sng_get_procId(); 

	/* id values are between (procId * 1,000,000) and ((procId + 1) * 1,000,000) */ 
	if (sngss7_id < ((procId + 1) * 1000000) ) {
		sngss7_id++;
	} else {
		sngss7_id = procId * 1000000;
	}

	return(sngss7_id);
}

/******************************************************************************/
ftdm_status_t check_if_rx_grs_started(ftdm_span_t *ftdmspan)
{
	ftdm_iterator_t *iter = NULL;
	ftdm_iterator_t *curr = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	sngss7_chan_data_t *sngss7_info = NULL;
	sngss7_chan_data_t *cinfo = NULL;
	int i = 0;

	iter = ftdm_span_get_chan_iterator(ftdmspan, NULL);
	for (curr = iter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_channel_t *fchan = ftdm_iterator_current(curr);

		ftdm_channel_lock(fchan);
	
		cinfo = fchan->call_data;

		if (!cinfo->rx_grs.range) {
			ftdm_channel_unlock(fchan);
			continue;
		}

		SS7_INFO("Rx GRS (%d:%d)\n", 
				g_ftdm_sngss7_data.cfg.isupCkt[cinfo->rx_grs.circuit].cic, 
				(g_ftdm_sngss7_data.cfg.isupCkt[cinfo->rx_grs.circuit].cic + cinfo->rx_grs.range));

		for (i = cinfo->rx_grs.circuit; i < (cinfo->rx_grs.circuit + cinfo->rx_grs.range + 1); i++) {

			/* confirm this is a voice channel, otherwise we do nothing */ 
			if (g_ftdm_sngss7_data.cfg.isupCkt[i].type != SNG_CKT_VOICE) {
				continue;
			} 

			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", i);
				continue;
			}

			/* check if the GRP_RESET_RX flag is already up */
			if (sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX)) {
				/* we have already processed this channel...move along */
				continue;
			}

			/* lock the channel */
			ftdm_channel_lock(ftdmchan);

			/* clear up any pending state changes */
			while (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_sangoma_ss7_process_state_change (ftdmchan);
			}

			/* flag the channel as having received a reset */
			sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX);

			switch (ftdmchan->state) {
			/**************************************************************************/
			case FTDM_CHANNEL_STATE_RESTART:

				/* go to idle so that we can redo the restart state*/
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_IDLE);

				break;
			/**************************************************************************/
			default:

				/* set the state of the channel to restart...the rest is done by the chan monitor */
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				break;
			/**************************************************************************/
			}

			/* unlock the channel again before we exit */
			ftdm_channel_unlock(ftdmchan);

		}

		ftdm_channel_unlock(fchan);
	}

	ftdm_iterator_free(iter);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t check_if_rx_grs_processed(ftdm_span_t *ftdmspan)
{
	ftdm_iterator_t *iter = NULL;
	ftdm_iterator_t *curr = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	sngss7_chan_data_t *sngss7_info = NULL;
	sngss7_chan_data_t *cinfo = NULL;
	int i = 0, bn = 0;
	int byte = 0, bit = 0;
	int cic_start = 0, cic_end = 0, num_cics = 0;
	ftdm_bitmap_t *lockmap = 0;
	ftdm_size_t mapsize = 0;

	iter = ftdm_span_get_chan_iterator(ftdmspan, NULL);
	for (curr = iter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_channel_t *fchan = ftdm_iterator_current(curr);

		ftdm_channel_lock(fchan);

		cinfo = fchan->call_data;

		if (!cinfo->rx_grs.range) {

			ftdm_channel_unlock(fchan);

			continue;
		}

		cic_start = cinfo->rx_grs.circuit;
		cic_end = cinfo->rx_grs.circuit + cinfo->rx_grs.range;
		num_cics = cinfo->rx_grs.range + 1;
		mapsize = (num_cics / FTDM_BITMAP_NBITS) + 1;

		lockmap = ftdm_calloc(mapsize, sizeof(*lockmap));
		if (!lockmap) {
			ftdm_channel_unlock(fchan);
			return FTDM_ENOMEM;
		}

		/* check all the circuits in the range to see if they are done resetting */
		for (i = cic_start, bn = 0; i <= cic_end; i++, bn++) {

			/* confirm this is a voice channel, otherwise we do nothing */ 
			if (g_ftdm_sngss7_data.cfg.isupCkt[i].type != SNG_CKT_VOICE) {
				continue;
			}

			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", i);
				ftdm_assert(FTDM_FALSE, "Failed to extract channel data during GRS\n");
				continue;
			}

			/* lock the channel */
			ftdm_channel_lock(ftdmchan);
			ftdm_map_set_bit(lockmap, bn);

			/* check if there is a state change pending on the channel */
			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
				/* check the state to the GRP_RESET_RX_DN flag */
				if (!sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_DN)) {
					/* this channel is still resetting...do nothing */
					goto GRS_UNLOCK_ALL;
				} /* if (!sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_DN)) */
			} else {
				/* state change pending */
				goto GRS_UNLOCK_ALL;
			}

		}

		SS7_DEBUG("All circuits out of reset for GRS: circuit=%d, range=%d\n", cinfo->rx_grs.circuit, cinfo->rx_grs.range);
		for (i = cic_start; i <= cic_end; i++) {

			/* confirm this is a voice channel, otherwise we do nothing */ 
			if (g_ftdm_sngss7_data.cfg.isupCkt[i].type != SNG_CKT_VOICE) {
				continue;
			}

			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n",i);
				ftdm_assert(FTDM_FALSE, "Failed to extract channel data during GRS\n");
				continue;
			}

			/* throw the GRP reset flag complete flag */
			sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_CMPLT);

			/* move the channel to the down state */
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

			/* update the status map if the ckt is in blocked state */
			if ((sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX)) ||
				(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) ||
				(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX)) ||
				(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX))) {
			
				cinfo->rx_grs.status[byte] = (cinfo->rx_grs.status[byte] | (1 << bit));
			}

			/* update the bit and byte counter*/
			bit ++;
			if (bit == 8) {
				byte++;
				bit = 0;
			}

		}
GRS_UNLOCK_ALL:
		for (i = cic_start, bn = 0; i <= cic_end; i++, bn++) {
			/* confirm this is a voice channel, otherwise we do nothing */ 
			if (g_ftdm_sngss7_data.cfg.isupCkt[i].type != SNG_CKT_VOICE) {
				continue;
			}

			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", i);
				ftdm_assert(FTDM_FALSE, "Failed to extract channel data during GRS\n");
				continue;
			}
			if (ftdm_map_test_bit(lockmap, bn)) {
				/* unlock the channel */
				ftdm_channel_unlock(ftdmchan);
				ftdm_map_clear_bit(lockmap, bn);
			}
		}

		ftdm_safe_free(lockmap);

		ftdm_channel_unlock(fchan);
	}

	ftdm_iterator_free(iter);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t check_if_rx_gra_started(ftdm_span_t *ftdmspan)
{
	ftdm_iterator_t *iter = NULL;
	ftdm_iterator_t *curr = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	sngss7_chan_data_t *sngss7_info = NULL;
	sngss7_chan_data_t *cinfo = NULL;
	int i = 0;

	iter = ftdm_span_get_chan_iterator(ftdmspan, NULL);

	for (curr = iter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_channel_t *fchan = ftdm_iterator_current(curr);
		ftdm_channel_lock(fchan);

		cinfo = fchan->call_data;

		if (!cinfo->rx_gra.range) {

			ftdm_channel_unlock(fchan);

			continue;
		}

		SS7_INFO("Rx GRA (%d:%d)\n", 
				g_ftdm_sngss7_data.cfg.isupCkt[cinfo->rx_gra.circuit].cic, 
				(g_ftdm_sngss7_data.cfg.isupCkt[cinfo->rx_gra.circuit].cic + cinfo->rx_gra.range));

		for (i = cinfo->rx_gra.circuit; i < (cinfo->rx_gra.circuit + cinfo->rx_gra.range + 1); i++) {

			/* confirm this is a voice channel, otherwise we do nothing */ 
			if (g_ftdm_sngss7_data.cfg.isupCkt[i].type != SNG_CKT_VOICE) {
				continue;
			} 

			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", i);
				continue;
			}

			/* check if the channel is already processing the GRA */
			if (sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX_RSP)) {
				/* move along */
				continue;
			}

			/* lock the channel */
			ftdm_channel_lock(ftdmchan);

			/* clear up any pending state changes */
			while (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_sangoma_ss7_process_state_change (ftdmchan);
			}

			switch (ftdmchan->state) {
			/**********************************************************************/
			case FTDM_CHANNEL_STATE_RESTART:
				
				/* throw the FLAG_RESET_TX_RSP to indicate we have acknowledgement from the remote side */
				sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX_RSP);

				/* go to DOWN */
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

				break;
			/**********************************************************************/
			case FTDM_CHANNEL_STATE_DOWN:

				/* do nothing, just drop the message */
				SS7_DEBUG("Receveived GRA in down state, dropping\n");

				break;
			/**********************************************************************/
			case FTDM_CHANNEL_STATE_TERMINATING:
			case FTDM_CHANNEL_STATE_HANGUP:
			case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
				
				/* throw the FLAG_RESET_TX_RSP to indicate we have acknowledgement from the remote side */
				sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX_RSP);

				break;
			/**********************************************************************/
			default:
				/* ITU Q764-2.9.5.1.c -> release the circuit */
				if (cinfo->rx_gra.cause != 0) {
					ftdmchan->caller_data.hangup_cause = cinfo->rx_gra.cause;
				} else {
					ftdmchan->caller_data.hangup_cause = 98;	/* Message not compatiable with call state */
				}

				/* go to terminating to hang up the call */
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
				break;
			/**********************************************************************/
			}

			ftdm_channel_unlock(ftdmchan);
		}

		ftdm_channel_unlock(fchan);
	}

	ftdm_iterator_free(iter);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t check_for_res_sus_flag(ftdm_span_t *ftdmspan)
{
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_sigmsg_t 		sigev;
	int 				x;

	for (x = 1; x < (ftdmspan->chan_count + 1); x++) {

		/* extract the channel structure and sngss7 channel data */
		ftdmchan = ftdmspan->channels[x];
		
		/* if the call data is NULL move on */
		if (ftdmchan->call_data == NULL) continue;

		sngss7_info = ftdmchan->call_data;

		/* lock the channel */
		ftdm_mutex_lock(ftdmchan->mutex);

		memset (&sigev, 0, sizeof (sigev));

		sigev.chan_id = ftdmchan->chan_id;
		sigev.span_id = ftdmchan->span_id;
		sigev.channel = ftdmchan;

		/* if we have the PAUSED flag and the sig status is still UP */
		if ((sngss7_test_ckt_flag(sngss7_info, FLAG_INFID_PAUSED)) &&
			(ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP))) {

			/* clear up any pending state changes */
			while (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_sangoma_ss7_process_state_change (ftdmchan);
			}
			
			/* throw the channel into SUSPENDED to process the flag */
			/* after doing this once the sig status will be down */
			ftdm_set_state (ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
		}

		/* if the RESUME flag is up go to SUSPENDED to process the flag */
		/* after doing this the flag will be cleared */
		if (sngss7_test_ckt_flag(sngss7_info, FLAG_INFID_RESUME)) {

			/* clear up any pending state changes */
			while (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_sangoma_ss7_process_state_change (ftdmchan);
			}

			/* got SUSPENDED state to clear the flag */
			ftdm_set_state (ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
		}

		/* unlock the channel */
		ftdm_mutex_unlock(ftdmchan->mutex);

	} /* for (x = 1; x < (span->chan_count + 1); x++) */

	/* signal the core that sig events are queued for processing */
	ftdm_span_trigger_signals(ftdmspan);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t process_span_ucic(ftdm_span_t *ftdmspan)
{
	ftdm_iterator_t *iter = NULL;
	ftdm_iterator_t *curr = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	sngss7_chan_data_t *sngss7_info = NULL;
	sngss7_chan_data_t *cinfo = NULL;
	sngss7_span_data_t *sngss7_span = (sngss7_span_data_t *)ftdmspan->signal_data;
	int i = 0;

	iter = ftdm_span_get_chan_iterator(ftdmspan, NULL);
	curr = iter;
	for (curr = iter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_channel_t *fchan = ftdm_iterator_current(curr);

		ftdm_channel_lock(fchan);

		cinfo = fchan->call_data;

		if (!cinfo->ucic.range) {

			ftdm_channel_unlock(fchan);

			continue;
		}

		for (i = cinfo->ucic.circuit; i < (cinfo->ucic.circuit + cinfo->ucic.range + 1); i++) {

			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", i);
				continue;
			}

			/* lock the channel */
			ftdm_channel_lock(ftdmchan);

			SS7_INFO_CHAN(ftdmchan, "[CIC:%d]Rx Span UCIC\n", sngss7_info->circuit->cic);

			/* clear up any pending state changes */
			while (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_sangoma_ss7_process_state_change (ftdmchan);
			}

			/* throw the ckt block flag */
			sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_BLOCK);

			/* set the channel to suspended state */
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

			/* unlock the channel again before we exit */
			ftdm_channel_unlock(ftdmchan);
		}
		/* clear out the ucic data since we're done with it */
		memset(&cinfo->ucic, 0, sizeof(cinfo->ucic));

		ftdm_channel_unlock(fchan);
	}

	ftdm_clear_flag(sngss7_span, SNGSS7_UCIC_PENDING);

	ftdm_iterator_free(iter);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t clear_rx_grs_flags(sngss7_chan_data_t *sngss7_info)
{
	/* clear all the flags related to an incoming GRS */
	sngss7_clear_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX);
	sngss7_clear_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_DN);
	sngss7_clear_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_CMPLT);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t clear_rx_grs_data(sngss7_chan_data_t *sngss7_info)
{
	ftdm_iterator_t *iter = NULL;
	ftdm_iterator_t *curr = NULL;
	sngss7_chan_data_t *cinfo = NULL;
	ftdm_channel_t *ftdmchan = sngss7_info->ftdmchan;
	sngss7_span_data_t *sngss7_span = (sngss7_span_data_t *)ftdmchan->span->signal_data;

	memset(&sngss7_info->rx_grs, 0, sizeof(sngss7_info->rx_grs));

	iter = ftdm_span_get_chan_iterator(ftdmchan->span, NULL);
	curr = iter;
	for (curr = iter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_channel_t *fchan = ftdm_iterator_current(curr);

		ftdm_channel_lock(fchan);

		cinfo = fchan->call_data;
		if (cinfo->rx_grs.range) {
			/* there is still another grs pending, do not clear the SNGSS7_RX_GRS_PENDING flag yet */
			ftdm_channel_unlock(fchan);
			goto done;
		}

		ftdm_channel_unlock(fchan);
	}

	/* if we're here is because there is no other grs going on now in this span */
	ftdm_clear_flag(sngss7_span, SNGSS7_RX_GRS_PENDING);

done:
	ftdm_iterator_free(iter);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t clear_rx_gra_data(sngss7_chan_data_t *sngss7_info)
{
	ftdm_iterator_t *iter = NULL;
	ftdm_iterator_t *curr = NULL;
	sngss7_chan_data_t *cinfo = NULL;
	ftdm_channel_t *ftdmchan = sngss7_info->ftdmchan;
	sngss7_span_data_t *sngss7_span = ftdmchan->span->signal_data;

	/* clear the rx_grs data fields */
	memset(&sngss7_info->rx_gra, 0, sizeof(sngss7_info->rx_gra));

	iter = ftdm_span_get_chan_iterator(ftdmchan->span, NULL);
	curr = iter;
	for (curr = iter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_channel_t *fchan = ftdm_iterator_current(curr);

		ftdm_channel_lock(fchan);

		cinfo = fchan->call_data;
		if (cinfo->rx_gra.range) {
			/* there is still another gra pending, do not clear the SNGSS7_RX_GRA_PENDING flag yet */
			ftdm_channel_unlock(fchan);
			goto done;
		}

		ftdm_channel_unlock(fchan);
	}

	/* if we're here is because there is no other gra pending in this span */
	ftdm_clear_flag(sngss7_span, SNGSS7_RX_GRA_PENDING);

done:

	ftdm_iterator_free(iter);

	return FTDM_SUCCESS;
}
/******************************************************************************/
ftdm_status_t clear_tx_grs_flags(sngss7_chan_data_t *sngss7_info)
{
	/* clear all the flags related to an outgoing GRS */
	sngss7_clear_ckt_flag(sngss7_info, FLAG_GRP_RESET_BASE);
	sngss7_clear_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX);
	sngss7_clear_ckt_flag(sngss7_info, FLAG_GRP_RESET_SENT);
	sngss7_clear_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX_RSP);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t clear_tx_grs_data(sngss7_chan_data_t *sngss7_info)
{
	/* clear everything up */
	memset(&sngss7_info->tx_grs, 0, sizeof(sngss7_info->tx_grs));
	return FTDM_SUCCESS;
}

/******************************************************************************/

/******************************************************************************/
ftdm_status_t clear_rx_rsc_flags(sngss7_chan_data_t *sngss7_info)
{
	/* clear all the flags related to an incoming RSC */
	sngss7_clear_ckt_flag(sngss7_info, FLAG_RESET_RX);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t clear_tx_rsc_flags(sngss7_chan_data_t *sngss7_info)
{
	/* clear all the flags related to an outgoing RSC */
	sngss7_clear_ckt_flag(sngss7_info, FLAG_RESET_TX);
	sngss7_clear_ckt_flag(sngss7_info, FLAG_RESET_SENT);
	sngss7_clear_ckt_flag(sngss7_info, FLAG_RESET_TX_RSP);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t encode_subAddrIE_nsap(const char *subAddr, char *subAddrIE, int type)
{
	/* Q931 4.5.9 
	 * 8	7	6	5	4	3	2	1	(octet)
	 *
	 * 0	1	1	1	0	0	0	1	(spare 8) ( IE id 1-7)
	 * X	X	X	X	X	X	X	X	(length of IE contents)
	 * 1	0	0	0	Z	0	0	0	(ext 8) (NSAP type 5-7) (odd/even 4) (spare 1-3)
	 * X	X	X	X	X	X	X	X	(sub address encoded in ia5)
	 */

	int	x = 0;
	int p = 0;
	int len = 0;
	char tmp[2];

	/* initalize the second element of tmp to \0 so that atoi doesn't go to far */
	tmp[1]='\0';

	/* set octet 1 aka IE id */
	p = 0;
	switch(type) {
	/**************************************************************************/
	case SNG_CALLED:						/* called party sub address */
		subAddrIE[p] = 0x71;
		break;
	/**************************************************************************/
	case SNG_CALLING:						/* calling party sub address */
		subAddrIE[p] = 0x6d;
		break;
	/**************************************************************************/
	default:								/* not good */
		SS7_ERROR("Sub-Address type is invalid: %d\n", type);
		return FTDM_FAIL;
		break;
	/**************************************************************************/
	} /* switch(type) */

	/* set octet 3 aka type and o/e */
	p = 2;
	subAddrIE[p] = 0x80;

	/* set the subAddrIE pointer octet 4 */
	p = 3;

	/* loop through all digits in subAddr and insert them into subAddrIE */
	while (subAddr[x] != '\0') {

		/* grab a character */
		tmp[0] = subAddr[x];

		/* confirm it is a digit */
		if (!isdigit(tmp[0])) {
			SS7_INFO("Dropping invalid digit: %c\n", tmp[0]);
			/* move to the next character in subAddr */
			x++;

			/* restart the loop */
			continue;
		}

		/* convert the character to IA5 encoding and write into subAddrIE */
		subAddrIE[p] = atoi(&tmp[0]);	/* lower nibble is the digit */
		subAddrIE[p] |= 0x3 << 4;		/* upper nibble is 0x3 */

		/* increment address length counter */
		len++;

		/* increment the subAddrIE pointer */
		p++;

		/* move to the next character in subAddr */
		x++;

	} /* while (subAddr[x] != '\0') */

	/* set octet 2 aka length of subaddr */
	p = 1;
	subAddrIE[p] = len + 1;


	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t encode_subAddrIE_nat(const char *subAddr, char *subAddrIE, int type)
{
	/* Q931 4.5.9 
	 * 8	7	6	5	4	3	2	1	(octet)
	 *
	 * 0	1	1	1	0	0	0	1	(spare 8) ( IE id 1-7)
	 * X	X	X	X	X	X	X	X	(length of IE contents)
	 * 1	0	0	0	Z	0	0	0	(ext 8) (NSAP type 5-7) (odd/even 4) (spare 1-3)
	 * X	X	X	X	X	X	X	X	(sub address encoded in ia5)
	 */

	int		x = 0;
	int 	p = 0;
	int 	len = 0;
	char 	tmp[2];
	int 	flag = 0;
	int 	odd = 0;
	uint8_t	lower = 0x0;
	uint8_t upper = 0x0;

	/* initalize the second element of tmp to \0 so that atoi doesn't go to far */
	tmp[1]='\0';

	/* set octet 1 aka IE id */
	p = 0;
	switch(type) {
	/**************************************************************************/
	case SNG_CALLED:						/* called party sub address */
		subAddrIE[p] = 0x71;
		break;
	/**************************************************************************/
	case SNG_CALLING:						/* calling party sub address */
		subAddrIE[p] = 0x6d;
		break;
	/**************************************************************************/
	default:								/* not good */
		SS7_ERROR("Sub-Address type is invalid: %d\n", type);
		return FTDM_FAIL;
		break;
	/**************************************************************************/
	} /* switch(type) */

	/* set the subAddrIE pointer octet 4 */
	p = 3;

	/* loop through all digits in subAddr and insert them into subAddrIE */
	while (1) {

		/* grab a character */
		tmp[0] = subAddr[x];

		/* confirm it is a hex digit */
		while ((!isxdigit(tmp[0])) && (tmp[0] != '\0')) {
			if (tmp[0] == '*') {
				/* Could not find a spec that specifies this, but on customer system, * was transmitted as 0x0b */
				SS7_DEBUG("Replacing * with 0x0b");
				x++;
				tmp[0] = 0x0b;
			} else {
				SS7_INFO("Dropping invalid digit: %c\n", tmp[0]);
				/* move to the next character in subAddr */
				x++;
				tmp[0] = subAddr[x];
			}
		}

		/* check if tmp is null or a digit */
		if (tmp[0] != '\0') {
			/* push it into the lower nibble using strtol to allow a-f chars */
			lower = strtol(&tmp[0], (char **)NULL, 16);
			/* move to the next digit */
			x++;
			/* grab a digit from the ftdm digits */
			tmp[0] = subAddr[x];

			/* check if the digit is a hex digit and that is not null */
			while (!(isxdigit(tmp[0])) && (tmp[0] != '\0')) {
				SS7_INFO("Dropping invalid digit: %c\n", tmp[0]);
				x++;
				tmp[0] = subAddr[x];
			} /* while(!(isdigit(tmp))) */

			/* check if tmp is null or a digit */
			if (tmp[0] != '\0') {
				/* push the digit into the upper nibble using strtol to allow a-f chars */
				upper = (strtol(&tmp[0], (char **)NULL, 16)) << 4;
			} else {
				/* there is no upper ... fill in spare */
				upper = 0x00;
				/* throw the odd flag since we need to buffer */
				odd = 1;
				/* throw the end flag */
				flag = 1;
			} /* if (tmp != '\0') */
		} else {
			/* keep the odd flag down */
			odd = 0;

			/* throw the flag */
			flag = 1;

			/* bounce out right away */
			break;
		}

		/* fill in the octet */
		subAddrIE[p] = upper | lower;

		/* increment address length counter */
		len++;

		/* if the flag is we're through all the digits */
		if (flag) break;

		/* increment the subAddrIE pointer */
		p++;

		/* move to the next character in subAddr */
		x++;

	} /* while (subAddr[x] != '\0') */

	/* set octet 2 aka length of subaddr */
	p = 1;
	subAddrIE[p] = len + 1;

	/* set octet 3 aka type and o/e */
	p = 2;
	subAddrIE[p] = 0xa0 | (odd << 3);


	return FTDM_SUCCESS;
}

/******************************************************************************/
int find_mtp2_error_type_in_map(const char *err_type)
{
	int i = 0;

	while (sng_mtp2_error_type_map[i].init == 1) {
		/* check if string matches the sng_type name */ 
		if (!strcasecmp(err_type, sng_mtp2_error_type_map[i].sng_type)) {
			/* we've found a match break from the loop */
			break;
		} else {
			/* move on to the next on */
			i++;
		}
	} /* while (sng_mtp2_error_type_map[i].init == 1) */

	/* check how we exited the loop */
	if (sng_mtp2_error_type_map[i].init == 0) {
		return -1;
	} else {
		return i;
	} /* if (sng_mtp2_error_type_map[i].init == 0) */
}

/******************************************************************************/
int find_link_type_in_map(const char *linkType)
{
	int i = 0;

	while (sng_link_type_map[i].init == 1) {
		/* check if string matches the sng_type name */ 
		if (!strcasecmp(linkType, sng_link_type_map[i].sng_type)) {
			/* we've found a match break from the loop */
			break;
		} else {
			/* move on to the next on */
			i++;
		}
	} /* while (sng_link_type_map[i].init == 1) */

	/* check how we exited the loop */
	if (sng_link_type_map[i].init == 0) {
		return -1;
	} else {
		return i;
	} /* if (sng_link_type_map[i].init == 0) */
}

/******************************************************************************/
int find_switch_type_in_map(const char *switchType)
{
	int i = 0;

	while (sng_switch_type_map[i].init == 1) {
		/* check if string matches the sng_type name */ 
		if (!strcasecmp(switchType, sng_switch_type_map[i].sng_type)) {
			/* we've found a match break from the loop */
			break;
		} else {
			/* move on to the next on */
			i++;
		}
	} /* while (sng_switch_type_map[i].init == 1) */

	/* check how we exited the loop */
	if (sng_switch_type_map[i].init == 0) {
		return -1;
	} else {
		return i;
	} /* if (sng_switch_type_map[i].init == 0) */
}

/******************************************************************************/
int find_ssf_type_in_map(const char *ssfType)
{
	int i = 0;

	while (sng_ssf_type_map[i].init == 1) {
		/* check if string matches the sng_type name */ 
		if (!strcasecmp(ssfType, sng_ssf_type_map[i].sng_type)) {
			/* we've found a match break from the loop */
			break;
		} else {
			/* move on to the next on */
			i++;
		}
	} /* while (sng_ssf_type_map[i].init == 1) */

	/* check how we exited the loop */
	if (sng_ssf_type_map[i].init == 0) {
		return -1;
	} else {
		return i;
	} /* if (sng_ssf_type_map[i].init == 0) */
}

/******************************************************************************/
int find_cic_cntrl_in_map(const char *cntrlType)
{
	int i = 0;

	while (sng_cic_cntrl_type_map[i].init == 1) {
		/* check if string matches the sng_type name */ 
		if (!strcasecmp(cntrlType, sng_cic_cntrl_type_map[i].sng_type)) {
			/* we've found a match break from the loop */
			break;
		} else {
			/* move on to the next on */
			i++;
		}
	} /* while (sng_cic_cntrl_type_map[i].init == 1) */

	/* check how we exited the loop */
	if (sng_cic_cntrl_type_map[i].init == 0) {
		return -1;
	} else {
		return i;
	} /* if (sng_cic_cntrl_type_map[i].init == 0) */
}

/******************************************************************************/
ftdm_status_t check_status_of_all_isup_intf(void)
{
	sng_isup_inf_t		*sngss7_intf = NULL;
	uint8_t				status = 0xff;
	int					x;

	/* go through all the isupIntfs and ask the stack to give their current state */
	x = 1;
	for (x = 1; x < (MAX_ISUP_INFS); x++) {
	/**************************************************************************/

		if (g_ftdm_sngss7_data.cfg.isupIntf[x].id == 0) continue;

		sngss7_intf = &g_ftdm_sngss7_data.cfg.isupIntf[x];

		if (ftmod_ss7_isup_intf_sta(sngss7_intf->id, &status)) {
			SS7_ERROR("Failed to get status of ISUP intf %d\n", sngss7_intf->id);
			sngss7_set_flag(sngss7_intf, SNGSS7_PAUSED);
			continue;
		}

		switch (status){
		/**********************************************************************/
		case (SI_INTF_AVAIL):
			SS7_DEBUG("State of ISUP intf %d = AVAIL\n", sngss7_intf->id); 

			/* check the current state for interface that we know */
			if (sngss7_test_flag(sngss7_intf, SNGSS7_PAUSED)) {
				/* we thing the intf is paused...put into resume */ 
				sngss7_clear_flag(sngss7_intf, SNGSS7_PAUSED);
			} else {
				/* nothing to since we already know that interface is active */
			}
			break;
		/**********************************************************************/
		case (SI_INTF_UNAVAIL):
			SS7_DEBUG("State of ISUP intf %d = UNAVAIL\n", sngss7_intf->id); 
			/* check the current state for interface that we know */
			if (sngss7_test_flag(sngss7_intf, SNGSS7_PAUSED)) {
				/* nothing to since we already know that interface is active */ 
			} else {
				/* put the interface into pause */
				sngss7_set_flag(sngss7_intf, SNGSS7_PAUSED);
			}
			break;
		/**********************************************************************/
		case (SI_INTF_CONG1):
			SS7_DEBUG("State of ISUP intf %d = Congestion 1\n", sngss7_intf->id);
			break;
		/**********************************************************************/
		case (SI_INTF_CONG2):
			SS7_DEBUG("State of ISUP intf %d = Congestion 2\n", sngss7_intf->id);
			break;
		/**********************************************************************/
		case (SI_INTF_CONG3):
			SS7_DEBUG("State of ISUP intf %d = Congestion 3\n", sngss7_intf->id);
			break;
		/**********************************************************************/
		default:
			/* should do something here to handle the possiblity of an unknown case */
			SS7_ERROR("Unknown ISUP intf Status code (%d) for Intf = %d\n", status, sngss7_intf->id);
			break;
		/**********************************************************************/
		} /* switch (status) */

	/**************************************************************************/
	} /* for (x = 1; x < MAX_ISUP_INFS); i++) */

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t sngss7_add_var(sngss7_chan_data_t *sngss7_info, const char* var, const char* val)
{
	char	*t_name = 0;
	char	*t_val = 0;

	/* confirm the user has sent us a value */
	if (!var || !val) {
		return FTDM_FAIL;
	}

	if (!sngss7_info->variables) {
		/* initialize on first use */
		sngss7_info->variables = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
		ftdm_assert_return(sngss7_info->variables, FTDM_FAIL, "Failed to create hash table\n");
	}

	t_name = ftdm_strdup(var);
	t_val = ftdm_strdup(val);

	hashtable_insert(sngss7_info->variables, t_name, t_val, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_FLAG_FREE_VALUE);

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t sngss7_add_raw_data(sngss7_chan_data_t *sngss7_info, uint8_t* data, ftdm_size_t data_len)
{
	ftdm_assert_return(!sngss7_info->raw_data, FTDM_FAIL, "Overwriting existing raw data\n");
	
	sngss7_info->raw_data = ftdm_calloc(1, data_len);
	ftdm_assert_return(sngss7_info->raw_data, FTDM_FAIL, "Failed to allocate raw data\n");

	memcpy(sngss7_info->raw_data, data, data_len);
	sngss7_info->raw_data_len = data_len;
	return FTDM_SUCCESS;
}

/******************************************************************************/
void sngss7_send_signal(sngss7_chan_data_t *sngss7_info, ftdm_signal_event_t event_id)
{
	ftdm_sigmsg_t	sigev;
	ftdm_channel_t	*ftdmchan = sngss7_info->ftdmchan;

	memset(&sigev, 0, sizeof(sigev));

	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;
	sigev.event_id = event_id;

	if (sngss7_info->variables) {
		/*
		* variables now belongs to the ftdm core, and
		* will be cleared after sigev is processed by user. Set
		* local pointer to NULL so we do not attempt to
		* destroy it */
		sigev.variables = sngss7_info->variables;
		sngss7_info->variables = NULL;
	}

	if (sngss7_info->raw_data) {
		/*
		* raw_data now belongs to the ftdm core, and
		* will be cleared after sigev is processed by user. Set
		* local pointer to NULL so we do not attempt to
		* destroy it */
		
		sigev.raw.data = sngss7_info->raw_data;
		sigev.raw.len = sngss7_info->raw_data_len;
		
		sngss7_info->raw_data = NULL;
		sngss7_info->raw_data_len = 0;
	}
	ftdm_span_send_signal(ftdmchan->span, &sigev);
}

/******************************************************************************/
void sngss7_set_sig_status(sngss7_chan_data_t *sngss7_info, ftdm_signaling_status_t status)
{
	ftdm_sigmsg_t	sig;
	ftdm_channel_t	*ftdmchan = sngss7_info->ftdmchan;

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Signalling link status changed to %s\n",
								ftdm_signaling_status2str(status));
	
	memset(&sig, 0, sizeof(sig));

	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;
	sig.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
	sig.ev_data.sigstatus.status = status;

	if (ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR,  "Failed to change channel status to %s\n",
									ftdm_signaling_status2str(status));
	}
	return;
}

#if 0
ftdm_status_t check_for_invalid_states(ftdm_channel_t *ftmchan)
{
	sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

	if (!sngss7_info) {
			SS7_WARN_CHAN(ftdmchan, "Found ftdmchan with no sig module data!%s\n", " ");
			return FTDM_FAIL;
	}	
		
	if (sngss7_test_flag(sngss7_intf, SNGSS7_PAUSED)) {
		return FTDM_SUCCESS;
	}

	switch (ftdmchan->state) {
	case UP:
	case DOWN:	
		return FTDM_SUCCESS;

	default:
		if ((ftdm_current_time_in_ms() - ftdmchan->last_state_change_time) > 30000) {
			SS7_WARN_CHAN(ftdmchan, "Circuite in state=%s too long - resetting!%s\n", 
								ftdm_channel_state2str(ftdmchan->state));

			ftdm_channel_lock(ftdmchan);
				
			if (sngss7_channel_status_clear(sngss7_info)) {
				sngss7_tx_reset_restart(sngss7_info);

				if (ftdmchan->state == FTDM_CHANNEL_STATE_RESTART) { 
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
				} else {
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				}
			} else {

			}	


			
			ftdm_channel_unlock(ftdmchan);
		}	
	}	

	return FTDM_SUCCESS;
}
#endif
	

/******************************************************************************/
ftdm_status_t check_for_reconfig_flag(ftdm_span_t *ftdmspan)
{
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_chan_data_t	*sngss7_info = NULL;
	sng_isup_inf_t		*sngss7_intf = NULL;
	uint8_t				state;
	uint8_t				bits_ab = 0;
	uint8_t				bits_cd = 0;	
	uint8_t				bits_ef = 0;
	int 				x;
	int					ret;
	ret=0;

	for (x = 1; x < (ftdmspan->chan_count + 1); x++) {
	/**************************************************************************/
		/* extract the channel structure and sngss7 channel data */
		ftdmchan = ftdmspan->channels[x];
		
		/* if the call data is NULL move on */
		if (ftdmchan->call_data == NULL) {
			SS7_WARN_CHAN(ftdmchan, "Found ftdmchan with no sig module data!%s\n", " ");
			continue;
		}

		/* grab the private data */
		sngss7_info = ftdmchan->call_data;

		/* check the reconfig flag */
		if (sngss7_test_ckt_flag(sngss7_info, FLAG_CKT_RECONFIG)) {
			/* confirm the state of all isup interfaces*/
			check_status_of_all_isup_intf();

			sngss7_intf = &g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId];

			/* check if the interface is paused or resumed */
			if (sngss7_test_flag(sngss7_intf, SNGSS7_PAUSED)) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Circuit set to PAUSED %s\n"," ");
				/* throw the pause flag */
				sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_RESUME);
				sngss7_set_ckt_flag(sngss7_info, FLAG_INFID_PAUSED);
			} else {
				ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Circuit set to RESUMED %s\n"," ");
				/* throw the resume flag */
				sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_PAUSED);
				sngss7_set_ckt_flag(sngss7_info, FLAG_INFID_RESUME);
			}

			/* query for the status of the ckt */
			if (ftmod_ss7_isup_ckt_sta(sngss7_info->circuit->id, &state)) {
				/* NC: Circuit statistic failed: does not exist. Must re-configure circuit
				       Reset the circuit CONFIGURED flag so that RESUME will reconfigure
				       this circuit. */
				sngss7_info->circuit->flags &= ~SNGSS7_CONFIGURED;
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR,"Failed to read isup ckt = %d status\n", sngss7_info->circuit->id);
				continue;
			}

			/* extract the bit sections */
			bits_ab = (state & (SNG_BIT_A + SNG_BIT_B)) >> 0;
			bits_cd = (state & (SNG_BIT_C + SNG_BIT_D)) >> 2;
			bits_ef = (state & (SNG_BIT_E + SNG_BIT_F)) >> 4;
					
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Circuit state=0x%X ab=0x%X cd=0x%X ef=0x%X\n",state,bits_ab,bits_cd,bits_ef);

			if (bits_cd == 0x0) {
				/* check if circuit is UCIC or transient */
				if (bits_ab == 0x3) {
					SS7_INFO("ISUP CKT %d re-configuration pending!\n", x);
					sngss7_info->circuit->flags &= ~SNGSS7_CONFIGURED;
					SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

					/* NC: The code below should be deleted. Its here for hitorical
					       reason. The RESUME code will reconfigure the channel since
					       the CONFIGURED flag has been reset */
#if 0
					/* bit a and bit b are set, unequipped */
					ret = ftmod_ss7_isup_ckt_config(sngss7_info->circuit->id);
					if (ret) {
						SS7_CRITICAL("ISUP CKT %d re-configuration FAILED!\n",x);
					} else {
						SS7_INFO("ISUP CKT %d re-configuration DONE!\n", x);
					}

					/* reset the circuit to sync states */
					ftdm_mutex_lock(ftdmchan->mutex);
			
					/* flag the circuit as active */
					sngss7_set_flag(sngss7_info->circuit, SNGSS7_ACTIVE);

					/* throw the channel into reset */
					sngss7_set_ckt_flag(sngss7_info, FLAG_RESET_TX);

					/* throw the channel to suspend */
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
			
					/* unlock the channel */
					ftdm_mutex_unlock(ftdmchan->mutex);
#endif

				} else { /* if (bits_ab == 0x3) */
					/* The stack status is not blocked.  However this is possible if
					   the circuit state was UP. So even though Master sent out the BLO
					   the status command is not showing it.  
					   
					   As a kudge. We will try to send out an UBL even though the status
					   indicates that there is no BLO.  */
					if (!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) {
						sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_UNBLK_TX);

						/* set the channel to suspended state */
						SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					}
				}
			} else {
				/* check the maintenance block status in bits A and B */
				switch (bits_ab) {
				/**************************************************************************/
				case (0):
					/* no maintenace block...do nothing */
					break;
				/**************************************************************************/
				case (1):
					/* The stack status is Blocked.  Check if the block was sent
					   by user via console.  If the block was not sent by user then, it 
					   was sent out by Master due to relay down.  
					   Therefore send out the unblock to clear it */
					if (!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) {
						sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_UNBLK_TX);

						/* set the channel to suspended state */
						SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					}

					/* Only locally blocked, thus remove a remote block */
					sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX);
					sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX_DN);

					break;
				/**************************************************************************/
				case (2):
					/* remotely blocked */
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX);
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX_DN);

					/* set the channel to suspended state */
					SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					break;
				/**************************************************************************/
				case (3):
					/* both locally and remotely blocked */
					if (!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) {
						sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_UNBLK_TX);
					}
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX);
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX_DN);

					/* set the channel to suspended state */
					SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					break;
				/**************************************************************************/
				default:
					break;
				/**************************************************************************/
				} /* switch (bits_ab) */
			
				/* check the hardware block status in bits e and f */
				switch (bits_ef) {
				/**************************************************************************/
				case (0):
					/* no maintenace block...do nothing */
					break;
				/**************************************************************************/
				case (1):
					/* locally blocked */
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX);

					/* set the channel to suspended state */
					SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					break;
				/**************************************************************************/
				case (2):
					/* remotely blocked */
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX);

					/* set the channel to suspended state */
					SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					break;
				/**************************************************************************/
				case (3):
					/* both locally and remotely blocked */
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX);
					sngss7_set_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX);

					/* set the channel to suspended state */
					SS7_STATE_CHANGE(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					break;
				/**************************************************************************/
				default:
					break;
				/**************************************************************************/
				} /* switch (bits_ef) */
			}

			/* clear the re-config flag ... no matter what */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_CKT_RECONFIG);

		} 
	} /* for (x = 1; x < (span->chan_count + 1); x++) */

	return FTDM_SUCCESS;
}

ftdm_status_t sngss7_bufferzero_iam(SiConEvnt *siConEvnt)
{
	if (siConEvnt->natConInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->natConInd, 0, sizeof(siConEvnt->natConInd));	
	if (siConEvnt->fwdCallInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->fwdCallInd, 0, sizeof(siConEvnt->fwdCallInd));
	if (siConEvnt->cgPtyCat.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cgPtyCat, 0, sizeof(siConEvnt->cgPtyCat));
	if (siConEvnt->txMedReq.eh.pres != PRSNT_NODEF) memset(&siConEvnt->txMedReq, 0, sizeof(siConEvnt->txMedReq));
#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL)	
	if (siConEvnt->usrServInfoA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->usrServInfoA, 0, sizeof(siConEvnt->usrServInfoA));
#endif	
	if (siConEvnt->cdPtyNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cdPtyNum, 0, sizeof(siConEvnt->cdPtyNum));
#if TNS_ANSI
#if (SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	if (siConEvnt->tranNetSel1.eh.pres != PRSNT_NODEF) memset(&siConEvnt->tranNetSel1, 0, sizeof(siConEvnt->tranNetSel1));
#endif
#endif
	if (siConEvnt->tranNetSel.eh.pres != PRSNT_NODEF) memset(&siConEvnt->tranNetSel, 0, sizeof(siConEvnt->tranNetSel));
#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL || SS7_CHINA)
	if (siConEvnt->callRefA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->callRefA, 0, sizeof(siConEvnt->callRefA));
#endif
	if (siConEvnt->callRef.eh.pres != PRSNT_NODEF) memset(&siConEvnt->callRef, 0, sizeof(siConEvnt->callRef));
	if (siConEvnt->cgPtyNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cgPtyNum, 0, sizeof(siConEvnt->cgPtyNum));
#if SS7_BELL
	if (siConEvnt->cgPtyNumB.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cgPtyNumB, 0, sizeof(siConEvnt->cgPtyNumB));
#endif
	if (siConEvnt->opFwdCalInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->opFwdCalInd, 0, sizeof(siConEvnt->opFwdCalInd));
#if (SS7_Q767 || SS7_RUSSIA || SS7_NTT)	
	if (siConEvnt->opFwdCalIndQ.eh.pres != PRSNT_NODEF) memset(&siConEvnt->opFwdCalIndQ, 0, sizeof(siConEvnt->opFwdCalIndQ));
#endif
#if SS7_Q767IT
	if (siConEvnt->fwdVad.eh.pres != PRSNT_NODEF) memset(&siConEvnt->fwdVad, 0, sizeof(siConEvnt->fwdVad));
#endif
#if SS7_ANS88
	if (siConEvnt->opFwdCalIndA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->opFwdCalIndA, 0, sizeof(siConEvnt->opFwdCalIndA));
#endif	
	if (siConEvnt->redirgNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->redirgNum, 0, sizeof(siConEvnt->redirgNum));
	if (siConEvnt->redirInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->redirInfo, 0, sizeof(siConEvnt->redirInfo));
	if (siConEvnt->cugIntCode.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cugIntCode, 0, sizeof(siConEvnt->cugIntCode));
#if SS7_ANS88
	if (siConEvnt->cugIntCodeA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cugIntCodeA, 0, sizeof(siConEvnt->cugIntCodeA));
#endif
#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL || SS7_CHINA)
	if (siConEvnt->connReqA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->connReqA, 0, sizeof(siConEvnt->connReqA));
#endif
#if SS7_ANS88
	if (siConEvnt->usr2UsrInfoA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->usr2UsrInfoA, 0, sizeof(siConEvnt->usr2UsrInfoA));
#endif
	if (siConEvnt->connReq.eh.pres != PRSNT_NODEF) memset(&siConEvnt->connReq, 0, sizeof(siConEvnt->connReq));
	if (siConEvnt->origCdNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->origCdNum, 0, sizeof(siConEvnt->origCdNum));
	if (siConEvnt->usr2UsrInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->usr2UsrInfo, 0, sizeof(siConEvnt->usr2UsrInfo));
	if (siConEvnt->accTrnspt.eh.pres != PRSNT_NODEF) memset(&siConEvnt->accTrnspt, 0, sizeof(siConEvnt->accTrnspt));
	if (siConEvnt->echoControl.eh.pres != PRSNT_NODEF) memset(&siConEvnt->echoControl, 0, sizeof(siConEvnt->echoControl));
#if SS7_ANS88
	if (siConEvnt->redirInfoA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->redirInfoA, 0, sizeof(siConEvnt->redirInfoA));
#endif
#if (SS7_ANS88 || SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	if (siConEvnt->chargeNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->chargeNum, 0, sizeof(siConEvnt->chargeNum));
	if (siConEvnt->origLineInf.eh.pres != PRSNT_NODEF) memset(&siConEvnt->origLineInf, 0, sizeof(siConEvnt->origLineInf));
#endif
	if (siConEvnt->usrServInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->usrServInfo, 0, sizeof(siConEvnt->usrServInfo));
	if (siConEvnt->usr2UsrInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->usr2UsrInd, 0, sizeof(siConEvnt->usr2UsrInd));
	if (siConEvnt->propDly.eh.pres != PRSNT_NODEF) memset(&siConEvnt->propDly, 0, sizeof(siConEvnt->propDly));
	if (siConEvnt->usrServInfo1.eh.pres != PRSNT_NODEF) memset(&siConEvnt->usrServInfo1, 0, sizeof(siConEvnt->usrServInfo1));
	if (siConEvnt->netFac.eh.pres != PRSNT_NODEF) memset(&siConEvnt->netFac, 0, sizeof(siConEvnt->netFac));
#ifdef SS7_CHINA	
	if (siConEvnt->orgPteCdeA.eh.pres != PRSNT_NODEF) memset(&siConEvnt->orgPteCdeA, 0, sizeof(siConEvnt->orgPteCdeA));
#endif	
	if (siConEvnt->orgPteCde.eh.pres != PRSNT_NODEF) memset(&siConEvnt->orgPteCde, 0, sizeof(siConEvnt->orgPteCde));
	if (siConEvnt->genDigits.eh.pres != PRSNT_NODEF) memset(&siConEvnt->genDigits, 0, sizeof(siConEvnt->genDigits));
	if (siConEvnt->genDigitsR.eh.pres != PRSNT_NODEF) memset(&siConEvnt->genDigitsR, 0, sizeof(siConEvnt->genDigitsR));
	if (siConEvnt->usrTSrvInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->usrTSrvInfo, 0, sizeof(siConEvnt->usrTSrvInfo));
	if (siConEvnt->remotOper.eh.pres != PRSNT_NODEF) memset(&siConEvnt->remotOper, 0, sizeof(siConEvnt->remotOper));
	if (siConEvnt->parmCom.eh.pres != PRSNT_NODEF) memset(&siConEvnt->parmCom, 0, sizeof(siConEvnt->parmCom));
#if (SS7_ANS92 || SS7_ANS95)
	if (siConEvnt->servCode.eh.pres != PRSNT_NODEF) memset(&siConEvnt->servCode, 0, sizeof(siConEvnt->servCode));
#endif
#if SS7_ANS92
	if (siConEvnt->serviceAct1.eh.pres != PRSNT_NODEF) memset(&siConEvnt->serviceAct1, 0, sizeof(siConEvnt->serviceAct1));
#endif
#if SS7_CHINA
	if (siConEvnt->serviceAct2.eh.pres != PRSNT_NODEF) memset(&siConEvnt->serviceAct2, 0, sizeof(siConEvnt->serviceAct2));
#endif
	if (siConEvnt->serviceAct2.eh.pres != PRSNT_NODEF) memset(&siConEvnt->serviceAct2, 0, sizeof(siConEvnt->serviceAct2));
	if (siConEvnt->serviceAct.eh.pres != PRSNT_NODEF) memset(&siConEvnt->serviceAct, 0, sizeof(siConEvnt->serviceAct));
	if (siConEvnt->mlppPrec.eh.pres != PRSNT_NODEF) memset(&siConEvnt->mlppPrec, 0, sizeof(siConEvnt->mlppPrec));	
#if (defined(SIT_PARAMETER) || defined(TDS_ROLL_UPGRADE_SUPPORT))
	if (siConEvnt->txMedUsPr.eh.pres != PRSNT_NODEF) memset(&siConEvnt->txMedUsPr, 0, sizeof(siConEvnt->txMedUsPr));
#endif
	if (siConEvnt->bckCallInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->bckCallInd, 0, sizeof(siConEvnt->bckCallInd));
	if (siConEvnt->cgPtyNum1.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cgPtyNum1, 0, sizeof(siConEvnt->cgPtyNum1));
	if (siConEvnt->optBckCalInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->optBckCalInd, 0, sizeof(siConEvnt->optBckCalInd));
#if (SS7_Q767 || SS7_RUSSIA || SS7_NTT)
	if (siConEvnt->optBckCalIndQ.eh.pres != PRSNT_NODEF) memset(&siConEvnt->optBckCalIndQ, 0, sizeof(siConEvnt->optBckCalIndQ));
#endif
	if (siConEvnt->connNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->connNum, 0, sizeof(siConEvnt->connNum));
#if (defined(SIT_PARAMETER) || defined(TDS_ROLL_UPGRADE_SUPPORT))
	if (siConEvnt->connNum2.eh.pres != PRSNT_NODEF) memset(&siConEvnt->connNum2, 0, sizeof(siConEvnt->connNum2));
#endif
	if (siConEvnt->accDelInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->accDelInfo, 0, sizeof(siConEvnt->accDelInfo));
	if (siConEvnt->notifInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->notifInd, 0, sizeof(siConEvnt->notifInd));
	if (siConEvnt->notifIndR2.eh.pres != PRSNT_NODEF) memset(&siConEvnt->notifIndR2, 0, sizeof(siConEvnt->notifIndR2));
	if (siConEvnt->cllHstry.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cllHstry, 0, sizeof(siConEvnt->cllHstry));
	if (siConEvnt->genNmb.eh.pres != PRSNT_NODEF) memset(&siConEvnt->genNmb, 0, sizeof(siConEvnt->genNmb));
	if (siConEvnt->genNmbR.eh.pres != PRSNT_NODEF) memset(&siConEvnt->genNmbR, 0, sizeof(siConEvnt->genNmbR));
	if (siConEvnt->redirNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->redirNum, 0, sizeof(siConEvnt->redirNum));
	if (siConEvnt->redirRstr.eh.pres != PRSNT_NODEF) memset(&siConEvnt->redirRstr, 0, sizeof(siConEvnt->redirRstr));

#if SS7_Q767IT
	if (siConEvnt->backVad.eh.pres != PRSNT_NODEF) memset(&siConEvnt->backVad, 0, sizeof(siConEvnt->backVad));
#endif
#if SS7_SINGTEL
	if (siConEvnt->cgPtyNumS.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cgPtyNumS, 0, sizeof(siConEvnt->cgPtyNumS));
#endif
#if (SS7_ANS92 || SS7_ANS95 || SS7_BELL)
	if (siConEvnt->businessGrp.eh.pres != PRSNT_NODEF) memset(&siConEvnt->businessGrp, 0, sizeof(siConEvnt->businessGrp));
	if (siConEvnt->infoInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->infoInd, 0, sizeof(siConEvnt->infoInd));
	if (siConEvnt->carrierId.eh.pres != PRSNT_NODEF) memset(&siConEvnt->carrierId, 0, sizeof(siConEvnt->carrierId));
	if (siConEvnt->carSelInf.eh.pres != PRSNT_NODEF) memset(&siConEvnt->carSelInf, 0, sizeof(siConEvnt->carSelInf));
	if (siConEvnt->egress.eh.pres != PRSNT_NODEF) memset(&siConEvnt->egress, 0, sizeof(siConEvnt->egress));
	if (siConEvnt->genAddr.eh.pres != PRSNT_NODEF) memset(&siConEvnt->genAddr, 0, sizeof(siConEvnt->genAddr));
	if (siConEvnt->genAddrR.eh.pres != PRSNT_NODEF) memset(&siConEvnt->genAddrR, 0, sizeof(siConEvnt->genAddrR));
	if (siConEvnt->infoReqInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->infoReqInd, 0, sizeof(siConEvnt->infoReqInd));
	if (siConEvnt->jurisInf.eh.pres != PRSNT_NODEF) memset(&siConEvnt->jurisInf, 0, sizeof(siConEvnt->jurisInf));
	if (siConEvnt->netTransport.eh.pres != PRSNT_NODEF) memset(&siConEvnt->netTransport, 0, sizeof(siConEvnt->netTransport));
	if (siConEvnt->specProcReq.eh.pres != PRSNT_NODEF) memset(&siConEvnt->specProcReq, 0, sizeof(siConEvnt->specProcReq));
	if (siConEvnt->transReq.eh.pres != PRSNT_NODEF) memset(&siConEvnt->transReq, 0, sizeof(siConEvnt->transReq));
#endif
#if (defined(SIT_PARAMETER) || defined(TDS_ROLL_UPGRADE_SUPPORT))
#if (SS7_ANS92 || SS7_ANS95)
	if (siConEvnt->notifInd1.eh.pres != PRSNT_NODEF) memset(&siConEvnt->notifInd1, 0, sizeof(siConEvnt->notifInd1));
	if (siConEvnt->notifIndR1.eh.pres != PRSNT_NODEF) memset(&siConEvnt->notifIndR1, 0, sizeof(siConEvnt->notifIndR1));
#endif /* SS7_ANS92 */
#endif /* SIT_PARAMETER || TDS_ROLL_UPGRADE_SUPPORT */
#if (SS7_BELL || SS7_ANS95)
	if (siConEvnt->genName.eh.pres != PRSNT_NODEF) memset(&siConEvnt->genName, 0, sizeof(siConEvnt->genName));
#endif
#if (SS7_ITU97 || SS7_RUSS2000 || SS7_ITU2000 || SS7_ETSIV3 || \
	SS7_BELL || SS7_ANS95 || SS7_INDIA || SS7_UK || SS7_NZL || SS7_KZ)
	if (siConEvnt->hopCounter.eh.pres != PRSNT_NODEF) memset(&siConEvnt->hopCounter, 0, sizeof(siConEvnt->hopCounter));
#endif
#if (SS7_BELL || SS7_ITU2000 || SS7_KZ)
	if (siConEvnt->redirCap.eh.pres != PRSNT_NODEF) memset(&siConEvnt->redirCap, 0, sizeof(siConEvnt->redirCap));
	if (siConEvnt->redirCntr.eh.pres != PRSNT_NODEF) memset(&siConEvnt->redirCntr, 0, sizeof(siConEvnt->redirCntr));
#endif
#if (SS7_ETSI || SS7_FTZ)
	if (siConEvnt->ccbsParam.eh.pres != PRSNT_NODEF) memset(&siConEvnt->ccbsParam, 0, sizeof(siConEvnt->ccbsParam));
	if (siConEvnt->freePhParam.eh.pres != PRSNT_NODEF) memset(&siConEvnt->freePhParam, 0, sizeof(siConEvnt->freePhParam));
#endif
#ifdef SS7_FTZ
	if (siConEvnt->naPaFF.eh.pres != PRSNT_NODEF) memset(&siConEvnt->naPaFF, 0, sizeof(siConEvnt->naPaFF));
	if (siConEvnt->naPaFE.eh.pres != PRSNT_NODEF) memset(&siConEvnt->naPaFE, 0, sizeof(siConEvnt->naPaFE));
	if (siConEvnt->naPaSSP.eh.pres != PRSNT_NODEF) memset(&siConEvnt->naPaSSP, 0, sizeof(siConEvnt->naPaSSP));
	if (siConEvnt->naPaCdPNO.eh.pres != PRSNT_NODEF) memset(&siConEvnt->naPaCdPNO, 0, sizeof(siConEvnt->naPaCdPNO));
	if (siConEvnt->naPaSPV.eh.pres != PRSNT_NODEF) memset(&siConEvnt->naPaSPV, 0, sizeof(siConEvnt->naPaSPV));
	if (siConEvnt->naPaUKK.eh.pres != PRSNT_NODEF) memset(&siConEvnt->naPaUKK, 0, sizeof(siConEvnt->naPaUKK));
#endif
#if SS7_NTT
	if (siConEvnt->msgAreaInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->msgAreaInfo, 0, sizeof(siConEvnt->msgAreaInfo));
	if (siConEvnt->subsNumber.eh.pres != PRSNT_NODEF) memset(&siConEvnt->subsNumber, 0, sizeof(siConEvnt->subsNumber));
	if (siConEvnt->rpCllngNo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->rpCllngNo, 0, sizeof(siConEvnt->rpCllngNo));
	if (siConEvnt->supplUserType.eh.pres != PRSNT_NODEF) memset(&siConEvnt->supplUserType, 0, sizeof(siConEvnt->supplUserType));
	if (siConEvnt->carrierInfoTrans.eh.pres != PRSNT_NODEF) memset(&siConEvnt->carrierInfoTrans, 0, sizeof(siConEvnt->carrierInfoTrans));
	if (siConEvnt->nwFuncType.eh.pres != PRSNT_NODEF) memset(&siConEvnt->nwFuncType, 0, sizeof(siConEvnt->nwFuncType));
#endif
#if SS7_ANS95
	if (siConEvnt->optrServicesInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->optrServicesInfo, 0, sizeof(siConEvnt->optrServicesInfo));
#endif
#if (SS7_ANS95 || SS7_ITU97  || SS7_RUSS2000|| SS7_ITU2000 || SS7_NZL || SS7_KZ)
	if (siConEvnt->cirAsgnMap.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cirAsgnMap, 0, sizeof(siConEvnt->cirAsgnMap));
#endif
#if (SS7_ITU97 || SS7_RUSS2000 || SS7_ITU2000 || SS7_ETSIV3 || \
	SS7_INDIA || SS7_UK || SS7_NZL || SS7_KZ)
	if (siConEvnt->displayInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->displayInfo, 0, sizeof(siConEvnt->displayInfo));
	if (siConEvnt->confTrtInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->confTrtInd, 0, sizeof(siConEvnt->confTrtInd));
	if (siConEvnt->netMgmtControls.eh.pres != PRSNT_NODEF) memset(&siConEvnt->netMgmtControls, 0, sizeof(siConEvnt->netMgmtControls));
	if (siConEvnt->correlationId.eh.pres != PRSNT_NODEF) memset(&siConEvnt->correlationId, 0, sizeof(siConEvnt->correlationId));
	if (siConEvnt->callDivTrtInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->callDivTrtInd, 0, sizeof(siConEvnt->callDivTrtInd));
	if (siConEvnt->callInNmb.eh.pres != PRSNT_NODEF) memset(&siConEvnt->callInNmb, 0, sizeof(siConEvnt->callInNmb));
	if (siConEvnt->callOfferTrtInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->callOfferTrtInd, 0, sizeof(siConEvnt->callOfferTrtInd));
	if (siConEvnt->scfId.eh.pres != PRSNT_NODEF) memset(&siConEvnt->scfId, 0, sizeof(siConEvnt->scfId));
	if (siConEvnt->uidCapInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->uidCapInd, 0, sizeof(siConEvnt->uidCapInd));
	if (siConEvnt->collCallReq.eh.pres != PRSNT_NODEF) memset(&siConEvnt->collCallReq, 0, sizeof(siConEvnt->collCallReq));
	if (siConEvnt->ccss.eh.pres != PRSNT_NODEF) memset(&siConEvnt->ccss, 0, sizeof(siConEvnt->ccss));
#endif
#if (SS7_ITU97 || SS7_RUSS2000 || SS7_ITU2000 || SS7_UK || SS7_NZL || SS7_KZ)
	if (siConEvnt->backGVNS.eh.pres != PRSNT_NODEF) memset(&siConEvnt->backGVNS, 0, sizeof(siConEvnt->backGVNS));
	if (siConEvnt->forwardGVNS.eh.pres != PRSNT_NODEF) memset(&siConEvnt->forwardGVNS, 0, sizeof(siConEvnt->forwardGVNS));
#endif
#if (SS7_ETSIV3 || SS7_ITU97 || SS7_RUSS2000 || SS7_ITU2000 || \
	SS7_INDIA || SS7_UK || SS7_NZL || SS7_KZ)
	if (siConEvnt->appTransParam.eh.pres != PRSNT_NODEF) memset(&siConEvnt->appTransParam, 0, sizeof(siConEvnt->appTransParam));
#endif
#if (SS7_ITU2000 || SS7_UK || SS7_NZL || SS7_KZ)
	if (siConEvnt->htrInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->htrInfo, 0, sizeof(siConEvnt->htrInfo));
	if (siConEvnt->pivotCap.eh.pres != PRSNT_NODEF) memset(&siConEvnt->pivotCap, 0, sizeof(siConEvnt->pivotCap));
	if (siConEvnt->cadDirNmb.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cadDirNmb, 0, sizeof(siConEvnt->cadDirNmb));
	if (siConEvnt->origCallInNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->origCallInNum, 0, sizeof(siConEvnt->origCallInNum));
	if (siConEvnt->calgGeoLoc.eh.pres != PRSNT_NODEF) memset(&siConEvnt->calgGeoLoc, 0, sizeof(siConEvnt->calgGeoLoc));
	if (siConEvnt->netRoutNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->netRoutNum, 0, sizeof(siConEvnt->netRoutNum));
	if (siConEvnt->qonRelCap.eh.pres != PRSNT_NODEF) memset(&siConEvnt->qonRelCap, 0, sizeof(siConEvnt->qonRelCap));
	if (siConEvnt->pivotCntr.eh.pres != PRSNT_NODEF) memset(&siConEvnt->pivotCntr, 0, sizeof(siConEvnt->pivotCntr));
	if (siConEvnt->pivotRtgFwInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->pivotRtgFwInfo, 0, sizeof(siConEvnt->pivotRtgFwInfo));
	if (siConEvnt->pivotRtgBkInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->pivotRtgBkInfo, 0, sizeof(siConEvnt->pivotRtgBkInfo));
	if (siConEvnt->numPortFwdInfo.eh.pres != PRSNT_NODEF) memset(&siConEvnt->numPortFwdInfo, 0, sizeof(siConEvnt->numPortFwdInfo));
#endif
#ifdef SS7_UK
	if (siConEvnt->natFwdCalInd.eh.pres != PRSNT_NODEF) memset(&siConEvnt->natFwdCalInd, 0, sizeof(siConEvnt->natFwdCalInd));
	if (siConEvnt->presntNum.eh.pres != PRSNT_NODEF) memset(&siConEvnt->presntNum, 0, sizeof(siConEvnt->presntNum));
	if (siConEvnt->lstDvrtLineId.eh.pres != PRSNT_NODEF) memset(&siConEvnt->lstDvrtLineId, 0, sizeof(siConEvnt->lstDvrtLineId));
	if (siConEvnt->pcgLineId.eh.pres != PRSNT_NODEF) memset(&siConEvnt->pcgLineId, 0, sizeof(siConEvnt->pcgLineId));
	if (siConEvnt->natFwdCalIndLnk.eh.pres != PRSNT_NODEF) memset(&siConEvnt->natFwdCalIndLnk, 0, sizeof(siConEvnt->natFwdCalIndLnk));
	if (siConEvnt->cdBascSrvcMrk.eh.pres != PRSNT_NODEF) memset(&siConEvnt->cdBascSrvcMrk, 0, sizeof(siConEvnt->cdBascSrvcMrk));
#endif /* SS7_UK */
#if (defined(CGPN_CHK))
	if (siConEvnt->causeDgnCgPnChk.eh.pres != PRSNT_NODEF) memset(&siConEvnt->causeDgnCgPnChk, 0, sizeof(siConEvnt->causeDgnCgPnChk));
#endif
	return FTDM_SUCCESS;
}

ftdm_status_t sngss7_save_iam(ftdm_channel_t *ftdmchan, SiConEvnt *siConEvnt)
{
#ifndef HAVE_ZLIB
	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Cannot perform transparent IAM because zlib is missing\n");
	return FTDM_FAIL;
#else
	unsigned ret_val = FTDM_FAIL;
	char *compressed_iam = NULL;
	char *url_encoded_iam = NULL;
	uLongf len = sizeof(*siConEvnt);	
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;

	/* By default, Trillium does not memset their whole structure to zero for
	 * performance. But we want to memset all the IE's that are not present to
	 * optimize compressed size */
	sngss7_bufferzero_iam(siConEvnt);
	
	compressed_iam = ftdm_malloc(sizeof(*siConEvnt));
	if (!compressed_iam) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to allocate buffer for compressed_iam\n");
		goto done;
	}

	/* Compress IAM structure to minimize buffer size */
	ret_val = compress((Bytef *)compressed_iam, &len, (const Bytef *)siConEvnt, (uLong)sizeof(*siConEvnt));
	if (ret_val != Z_OK) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Failed to compress IAM (error:%d)\n", ret_val);
		ret_val = FTDM_FAIL;
		goto done;
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Compressed IAM size:%lu\n", len);

	/* Worst case: size will triple after url encode */
	url_encoded_iam = ftdm_malloc(3*sizeof(*siConEvnt));
	if (!url_encoded_iam) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to allocated buffer for url_encoded_iam\n");
		ret_val = FTDM_FAIL;
		goto done;
	}
	memset(url_encoded_iam, 0, 2*sizeof(*siConEvnt));
	
	/* URL encode buffer to that its safe to store it in a string */
	ftdm_url_encode((const char*)compressed_iam, url_encoded_iam, len);

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "IAM variable length:%"FTDM_SIZE_FMT"\n", strlen(url_encoded_iam));

	if (strlen(url_encoded_iam) > sngss7_info->circuit->transparent_iam_max_size) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "IAM variable length exceeds max size (len:%d max:%d) \n", 
			strlen(url_encoded_iam), sngss7_info->circuit->transparent_iam_max_size);
		ret_val = FTDM_FAIL;
		goto done;
	}
	
	sngss7_add_var(sngss7_info, "ss7_iam", url_encoded_iam);
done:	
	ftdm_safe_free(compressed_iam);
	ftdm_safe_free(url_encoded_iam);
	return ret_val;
#endif
}

ftdm_status_t sngss7_retrieve_iam(ftdm_channel_t *ftdmchan, SiConEvnt *siConEvnt)
{
#ifndef HAVE_ZLIB
	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Cannot perform transparent IAM because zlib is missing\n");
	return FTDM_FAIL;
#else	
	uLongf len = 3*sizeof(*siConEvnt); /* worst case: URL encoded buffer is 3x length of buffer */
	char *val = NULL;
	unsigned ret_val = FTDM_FAIL;	
	void *uncompressed_buffer = NULL;
	char *url_encoded_iam = NULL;
	ftdm_size_t url_encoded_iam_len;

	val = (char*)ftdm_usrmsg_get_var(ftdmchan->usrmsg, "ss7_iam");
	if (ftdm_strlen_zero(val)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "No Transparent IAM info available\n");
		return FTDM_FAIL;
	}

	url_encoded_iam = ftdm_strdup(val);

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "IAM variable length:%"FTDM_SIZE_FMT"\n", strlen(val));
	ftdm_url_decode(url_encoded_iam, &url_encoded_iam_len);
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Compressed IAM size:%"FTDM_SIZE_FMT"\n", url_encoded_iam_len);

	uncompressed_buffer = ftdm_malloc(sizeof(*siConEvnt));
	ftdm_assert_return(uncompressed_buffer, FTDM_FAIL, "Failed to allocate buffer for uncompressed buffer\n");

	ret_val = uncompress(uncompressed_buffer, &len, (const Bytef *)url_encoded_iam, (uLong)url_encoded_iam_len);
	if (ret_val != Z_OK) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Failed to uncompress IAM (error:%d)\n", ret_val);
		goto done;
	}

	if (len != sizeof(*siConEvnt)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Incompatible IAM structure size (expected:%"FTDM_SIZE_FMT" size:%"FTDM_SIZE_FMT")\n", sizeof(*siConEvnt), strlen(uncompressed_buffer));
		goto done;
	}

	memcpy(siConEvnt, uncompressed_buffer, sizeof(*siConEvnt));
	ret_val = FTDM_SUCCESS;

done:
	ftdm_safe_free(uncompressed_buffer);
	ftdm_safe_free(url_encoded_iam);	
	return ret_val;
#endif
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
