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
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
void ft_to_sngss7_iam(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_acm(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_anm(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_rel(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_rlc(ftdm_channel_t * ftdmchan);

void ft_to_sngss7_rsc(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_rsca(ftdm_channel_t * ftdmchan);

void ft_to_sngss7_blo(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_bla(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_ubl(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_uba(ftdm_channel_t * ftdmchan);

void ft_to_sngss7_lpa(ftdm_channel_t * ftdmchan);

void ft_to_sngss7_gra(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_grs(ftdm_channel_t * ftdmchan);

void ft_to_sngss7_cgb(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_cgu(ftdm_channel_t * ftdmchan);

void ft_to_sngss7_cgba(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_cgua(ftdm_channel_t * ftdmchan);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
void ft_to_sngss7_iam (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;;
	const char			*clg_nadi = NULL;
	const char			*cld_nadi = NULL;
	const char			*clg_subAddr = NULL;
	const char			*cld_subAddr = NULL;
	char 				subAddrIE[MAX_SIZEOF_SUBADDR_IE];
	SiConEvnt 			iam;
	
	sngss7_info->suInstId 	= get_unique_id ();
	sngss7_info->spInstId 	= 0;
	sngss7_info->spId 		= 1;
	
	memset (&iam, 0x0, sizeof (iam));
	
	/* copy down the nature of connection indicators */
	iam.natConInd.eh.pres 				= PRSNT_NODEF;
	iam.natConInd.satInd.pres 			= PRSNT_NODEF;
	iam.natConInd.satInd.val 			= 0; /* no satellite circuit */
	iam.natConInd.contChkInd.pres 		= PRSNT_NODEF;
	iam.natConInd.contChkInd.val 		= CONTCHK_NOTREQ;
	iam.natConInd.echoCntrlDevInd.pres	= PRSNT_NODEF;
	iam.natConInd.echoCntrlDevInd.val 	= ECHOCDEV_INCL;
	
	/* copy down the forward call indicators */
	iam.fwdCallInd.eh.pres 				= PRSNT_NODEF;
	iam.fwdCallInd.natIntCallInd.pres 	= PRSNT_NODEF;
	iam.fwdCallInd.natIntCallInd.val 	= 0x00;
	iam.fwdCallInd.end2EndMethInd.pres 	= PRSNT_NODEF;
	iam.fwdCallInd.end2EndMethInd.val 	= E2EMTH_NOMETH;
	iam.fwdCallInd.intInd.pres 			= PRSNT_NODEF;
	iam.fwdCallInd.intInd.val 			= INTIND_NOINTW;
	iam.fwdCallInd.end2EndInfoInd.pres 	= PRSNT_NODEF;
	iam.fwdCallInd.end2EndInfoInd.val 	= E2EINF_NOINFO;
	iam.fwdCallInd.isdnUsrPrtInd.pres 	= PRSNT_NODEF;
	iam.fwdCallInd.isdnUsrPrtInd.val 	= ISUP_USED;
	iam.fwdCallInd.isdnUsrPrtPrfInd.pres = PRSNT_NODEF;
	iam.fwdCallInd.isdnUsrPrtPrfInd.val = PREF_PREFAW;
	iam.fwdCallInd.isdnAccInd.pres 		= PRSNT_NODEF;
	iam.fwdCallInd.isdnAccInd.val 		= ISDNACC_ISDN;
	iam.fwdCallInd.sccpMethInd.pres 	= PRSNT_NODEF;
	iam.fwdCallInd.sccpMethInd.val 		= SCCPMTH_NOIND;
	
	/* copy down the calling number information */
	iam.cgPtyCat.eh.pres 				= PRSNT_NODEF;
	iam.cgPtyCat.cgPtyCat.pres 			= PRSNT_NODEF;
	iam.cgPtyCat.cgPtyCat.val 			= CAT_ORD;	/* ordinary suscriber */
	
	/* copy down the transmission medium requirements */
	iam.txMedReq.eh.pres 				= PRSNT_NODEF;
	iam.txMedReq.trMedReq.pres 			= PRSNT_NODEF;
	iam.txMedReq.trMedReq.val 			= ftdmchan->caller_data.bearer_capability;

	if ((g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId].switchType == LSI_SW_ANS88) ||
		(g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId].switchType == LSI_SW_ANS92) ||
		(g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId].switchType == LSI_SW_ANS95)) {

		/* include only if we're running ANSI */
		iam.fwdCallInd.transCallNInd.pres   = PRSNT_NODEF;
		iam.fwdCallInd.transCallNInd.val    = 0x0;

		iam.usrServInfoA.eh.pres				= PRSNT_NODEF;

		iam.usrServInfoA.infoTranCap.pres		= PRSNT_NODEF;
		switch (ftdmchan->caller_data.bearer_capability) {
		/**********************************************************************/
		case (FTDM_BEARER_CAP_SPEECH):
			iam.usrServInfoA.infoTranCap.val	= 0x0;			/* speech as per ATIS-1000113.3.2005 */
			break;
		/**********************************************************************/
		case (FTDM_BEARER_CAP_64K_UNRESTRICTED):
			iam.usrServInfoA.infoTranCap.val	= 0x8;			/* unrestricted digital as per ATIS-1000113.3.2005 */
			break;
		/**********************************************************************/
		case (FTDM_BEARER_CAP_3_1KHZ_AUDIO):
			iam.usrServInfoA.infoTranCap.val	= 0x10;			/* 3.1kHz audio as per ATIS-1000113.3.2005 */
			break;
		/**********************************************************************/
		default:
			SS7_ERROR_CHAN(ftdmchan, "Unknown Bearer capability falling back to speech%s\n", " ");
			iam.usrServInfoA.infoTranCap.val	= 0x0;			/* speech as per ATIS-1000113.3.2005 */
			break;
		/**********************************************************************/
		} /* switch (ftdmchan->caller_data.bearer_capability) */

		iam.usrServInfoA.cdeStand.pres			= PRSNT_NODEF;
		iam.usrServInfoA.cdeStand.val			= 0x0;				/* ITU-T standardized coding */
		iam.usrServInfoA.tranMode.pres			= PRSNT_NODEF;
		iam.usrServInfoA.tranMode.val			= 0x0;				/* circuit mode */
		iam.usrServInfoA.infoTranRate0.pres		= PRSNT_NODEF;
		iam.usrServInfoA.infoTranRate0.val		= 0x10;				/* 64kbps origination to destination */
		iam.usrServInfoA.infoTranRate1.pres		= PRSNT_NODEF;
		iam.usrServInfoA.infoTranRate1.val		= 0x10;				/* 64kbps destination to origination */
		iam.usrServInfoA.chanStruct.pres		= PRSNT_NODEF;
		iam.usrServInfoA.chanStruct.val			= 0x1;				/* 8kHz integrity */
		iam.usrServInfoA.config.pres			= PRSNT_NODEF;
		iam.usrServInfoA.config.val				= 0x0;				/* point to point configuration */
		iam.usrServInfoA.establish.pres			= PRSNT_NODEF;
		iam.usrServInfoA.establish.val			= 0x0;				/* on demand */
		iam.usrServInfoA.symmetry.pres			= PRSNT_NODEF;
		iam.usrServInfoA.symmetry.val			= 0x0;				/* bi-directional symmetric */
		iam.usrServInfoA.usrInfLyr1Prot.pres	= PRSNT_NODEF;
		iam.usrServInfoA.usrInfLyr1Prot.val		= 0x2;				/* G.711 ulaw */
		iam.usrServInfoA.rateMultiplier.pres	= PRSNT_NODEF;
		iam.usrServInfoA.rateMultiplier.val		= 0x1;				/* 1x rate multipler */
	} /* if ANSI */
	
	/* copy down the called number information */
	copy_cdPtyNum_to_sngss7 (&ftdmchan->caller_data, &iam.cdPtyNum);
	
	/* copy down the calling number information */
	
	copy_cgPtyNum_to_sngss7 (&ftdmchan->caller_data, &iam.cgPtyNum);

	/* check if the user would like a custom NADI value for the calling Pty Num */
	clg_nadi = ftdm_channel_get_var(ftdmchan, "ss7_clg_nadi");
	if ((clg_nadi != NULL) && (*clg_nadi)) {
		SS7_DEBUG_CHAN(ftdmchan,"Found user supplied Calling NADI value \"%s\"\n", clg_nadi);
		iam.cgPtyNum.natAddrInd.val	= atoi(clg_nadi);
	} else {
		iam.cgPtyNum.natAddrInd.val	= g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId].clg_nadi;
		SS7_DEBUG_CHAN(ftdmchan,"No user supplied NADI value found for CLG, using \"%d\"\n", iam.cgPtyNum.natAddrInd.val);
	}

	cld_nadi = ftdm_channel_get_var(ftdmchan, "ss7_cld_nadi");
	if ((cld_nadi != NULL) && (*cld_nadi)) {
		SS7_DEBUG_CHAN(ftdmchan,"Found user supplied Called NADI value \"%s\"\n", cld_nadi);
		iam.cdPtyNum.natAddrInd.val	= atoi(cld_nadi);
	} else {
		iam.cdPtyNum.natAddrInd.val	= g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId].cld_nadi;
		SS7_DEBUG_CHAN(ftdmchan,"No user supplied NADI value found for CLD, using \"%d\"\n", iam.cdPtyNum.natAddrInd.val);
	}

	/* check if the user would like us to send a clg_sub-address */
	clg_subAddr = ftdm_channel_get_var(ftdmchan, "ss7_clg_subaddr");
	if ((clg_subAddr != NULL) && (*clg_subAddr)) {
		SS7_DEBUG_CHAN(ftdmchan,"Found user supplied Calling Sub-Address value \"%s\"\n", clg_subAddr);
		
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
			SS7_ERROR_CHAN(ftdmchan,"Invalid Calling Sub-Address encoding requested: %c\n", clg_subAddr[0]);
			break;
		} /* switch (cld_subAddr[0]) */


		/* if subaddIE is still empty don't copy it in */
		if (subAddrIE[0] != '0') {
			/* check if the clg_subAddr has already been added */
			if (iam.accTrnspt.eh.pres == PRSNT_NODEF) {
				/* append the subAddrIE */
				memcpy(&iam.accTrnspt.infoElmts.val[iam.accTrnspt.infoElmts.len], subAddrIE, (subAddrIE[1] + 2));
				iam.accTrnspt.infoElmts.len		= iam.accTrnspt.infoElmts.len +subAddrIE[1] + 2;
			} else {
				/* fill in from the beginning */
				iam.accTrnspt.eh.pres			= PRSNT_NODEF;
				iam.accTrnspt.infoElmts.pres	= PRSNT_NODEF;
				memcpy(iam.accTrnspt.infoElmts.val, subAddrIE, (subAddrIE[1] + 2));
				iam.accTrnspt.infoElmts.len		= subAddrIE[1] + 2;
			} /* if (iam.accTrnspt.eh.pres */
		} /* if (subAddrIE[0] != '0') */
	}

	/* check if the user would like us to send a cld_sub-address */
	cld_subAddr = ftdm_channel_get_var(ftdmchan, "ss7_cld_subaddr");
	if ((cld_subAddr != NULL) && (*cld_subAddr)) {
		SS7_DEBUG_CHAN(ftdmchan,"Found user supplied Called Sub-Address value \"%s\"\n", cld_subAddr);
		
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
			SS7_ERROR_CHAN(ftdmchan,"Invalid Called Sub-Address encoding requested: %c\n", cld_subAddr[0]);
			break;
		} /* switch (cld_subAddr[0]) */

		/* if subaddIE is still empty don't copy it in */
		if (subAddrIE[0] != '0') {
			/* check if the cld_subAddr has already been added */
			if (iam.accTrnspt.eh.pres == PRSNT_NODEF) {
				/* append the subAddrIE */
				memcpy(&iam.accTrnspt.infoElmts.val[iam.accTrnspt.infoElmts.len], subAddrIE, (subAddrIE[1] + 2));
				iam.accTrnspt.infoElmts.len		= iam.accTrnspt.infoElmts.len +subAddrIE[1] + 2;
			} else {
				/* fill in from the beginning */
				iam.accTrnspt.eh.pres			= PRSNT_NODEF;
				iam.accTrnspt.infoElmts.pres	= PRSNT_NODEF;
				memcpy(iam.accTrnspt.infoElmts.val, subAddrIE, (subAddrIE[1] + 2));
				iam.accTrnspt.infoElmts.len		= subAddrIE[1] + 2;
			} /* if (iam.accTrnspt.eh.pres */
		} /* if (subAddrIE[0] != '0') */
	} /* if ((cld_subAddr != NULL) && (*cld_subAddr)) */




	sng_cc_con_request (sngss7_info->spId,
						sngss7_info->suInstId,
						sngss7_info->spInstId,
						sngss7_info->circuit->id, 
						&iam, 
						0);

	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx IAM clg = \"%s\" (NADI=%d), cld = \"%s\" (NADI=%d)\n",
							sngss7_info->circuit->cic,
							ftdmchan->caller_data.cid_num.digits,
							iam.cgPtyNum.natAddrInd.val,
							ftdmchan->caller_data.dnis.digits,
							iam.cdPtyNum.natAddrInd.val);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_acm (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	SiCnStEvnt acm;
	
	memset (&acm, 0x0, sizeof (acm));
	
	/* fill in the needed information for the ACM */
	acm.bckCallInd.eh.pres 				= PRSNT_NODEF;
	acm.bckCallInd.chrgInd.pres			= PRSNT_NODEF;
	acm.bckCallInd.chrgInd.val			= CHRG_CHRG;
	acm.bckCallInd.cadPtyStatInd.pres	= PRSNT_NODEF;
	acm.bckCallInd.cadPtyStatInd.val	= 0x01;
	acm.bckCallInd.cadPtyCatInd.pres	= PRSNT_NODEF;
	acm.bckCallInd.cadPtyCatInd.val		= CADCAT_ORDSUBS;
	acm.bckCallInd.end2EndMethInd.pres	= PRSNT_NODEF;
	acm.bckCallInd.end2EndMethInd.val	= E2EMTH_NOMETH;
	acm.bckCallInd.intInd.pres			= PRSNT_NODEF;
	acm.bckCallInd.intInd.val 			= INTIND_NOINTW;
	acm.bckCallInd.end2EndInfoInd.pres	= PRSNT_NODEF;
	acm.bckCallInd.end2EndInfoInd.val	= E2EINF_NOINFO;
	acm.bckCallInd.isdnUsrPrtInd.pres	= PRSNT_NODEF;
	acm.bckCallInd.isdnUsrPrtInd.val	= ISUP_USED;
	acm.bckCallInd.holdInd.pres			= PRSNT_NODEF;
	acm.bckCallInd.holdInd.val			= HOLD_NOTREQD;
	acm.bckCallInd.isdnAccInd.pres		= PRSNT_NODEF;
	acm.bckCallInd.isdnAccInd.val		= ISDNACC_NONISDN;
	acm.bckCallInd.echoCtrlDevInd.pres	= PRSNT_NODEF;
	acm.bckCallInd.echoCtrlDevInd.val	= 0x1;	/* ec device present */
	acm.bckCallInd.sccpMethInd.pres		= PRSNT_NODEF;
	acm.bckCallInd.sccpMethInd.val		= SCCPMTH_NOIND;
	
	/* send the ACM request to LibSngSS7 */
	sng_cc_con_status  (1,
						sngss7_info->suInstId,
						sngss7_info->spInstId,
						sngss7_info->circuit->id, 
						&acm, 
						ADDRCMPLT);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx ACM\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_anm (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	SiConEvnt anm;
	
	memset (&anm, 0x0, sizeof (anm));
	
	/* send the ANM request to LibSngSS7 */
	sng_cc_con_response(1,
						sngss7_info->suInstId,
						sngss7_info->spInstId,
						sngss7_info->circuit->id, 
						&anm, 
						5);

  SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx ANM\n", sngss7_info->circuit->cic);

  SS7_FUNC_TRACE_EXIT (__FUNCTION__);
  return;
}

/******************************************************************************/
void ft_to_sngss7_rel (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	SiRelEvnt rel;
	
	memset (&rel, 0x0, sizeof (rel));
	
	rel.causeDgn.eh.pres = PRSNT_NODEF;
	rel.causeDgn.location.pres = PRSNT_NODEF;
	rel.causeDgn.location.val = 0x01;
	rel.causeDgn.cdeStand.pres = PRSNT_NODEF;
	rel.causeDgn.cdeStand.val = 0x00;
	rel.causeDgn.recommend.pres = NOTPRSNT;
	rel.causeDgn.causeVal.pres = PRSNT_NODEF;
	rel.causeDgn.causeVal.val = (uint8_t) ftdmchan->caller_data.hangup_cause;
	rel.causeDgn.dgnVal.pres = NOTPRSNT;
	
	/* send the REL request to LibSngSS7 */
	sng_cc_rel_request (1,
						sngss7_info->suInstId,
						sngss7_info->spInstId, 
						sngss7_info->circuit->id, 
						&rel);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx REL cause=%d \n",
							sngss7_info->circuit->cic,
							ftdmchan->caller_data.hangup_cause );
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_rlc (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	SiRelEvnt rlc;
	
	memset (&rlc, 0x0, sizeof (rlc));
	
	/* send the RLC request to LibSngSS7 */
	sng_cc_rel_response (1,
						sngss7_info->suInstId,
						sngss7_info->spInstId, 
						sngss7_info->circuit->id, 
						&rlc);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx RLC\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_rsc (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	sng_cc_sta_request (1,
						sngss7_info->suInstId,
						sngss7_info->spInstId,
						sngss7_info->circuit->id,
						sngss7_info->globalFlg, 
						SIT_STA_CIRRESREQ, 
						NULL);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx RSC\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_rsca (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	sng_cc_sta_request (1,
						sngss7_info->suInstId,
						sngss7_info->spInstId,
						sngss7_info->circuit->id,
						sngss7_info->globalFlg, 
						SIT_STA_CIRRESRSP, 
						NULL);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx RSC-RLC\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
  return;
}

/******************************************************************************/
void ft_to_sngss7_blo (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	sng_cc_sta_request (1,
						0,
						0,
						sngss7_info->circuit->id,
						sngss7_info->globalFlg, 
						SIT_STA_CIRBLOREQ, 
						NULL);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx BLO\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_bla (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	sng_cc_sta_request (1,
						0,
						0,
						sngss7_info->circuit->id,
						sngss7_info->globalFlg, 
						SIT_STA_CIRBLORSP, 
						NULL);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx BLA\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void
ft_to_sngss7_ubl (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	sng_cc_sta_request (1,
						0,
						0,
						sngss7_info->circuit->id,
						sngss7_info->globalFlg, 
						SIT_STA_CIRUBLREQ, 
						NULL);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx UBL\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_uba (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	sng_cc_sta_request (1,
						0,
						0,
						sngss7_info->circuit->id,
						sngss7_info->globalFlg, 
						SIT_STA_CIRUBLRSP, 
						NULL);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx UBA\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_lpa (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	sng_cc_sta_request (1,
						sngss7_info->suInstId,
						sngss7_info->spInstId,
						sngss7_info->circuit->id,
						sngss7_info->globalFlg, 
						SIT_STA_LOOPBACKACK, 
						NULL);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx LPA\n", sngss7_info->circuit->cic);
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
return;
}

/******************************************************************************/
void ft_to_sngss7_gra (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_span_data_t *sngss7_span = ftdmchan->span->mod_data;
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	SiStaEvnt	gra;
	
	/* clean out the gra struct */
	memset (&gra, 0x0, sizeof (gra));

	gra.rangStat.eh.pres = PRSNT_NODEF;

	/* fill in the range */	
	gra.rangStat.range.pres = PRSNT_NODEF;
	gra.rangStat.range.val = sngss7_span->rx_grs.range;

	/* fill in the status */
	gra.rangStat.status.pres = PRSNT_NODEF;
	gra.rangStat.status.len = ((sngss7_span->rx_grs.range + 1) >> 3) + (((sngss7_span->rx_grs.range + 1) & 0x07) ? 1 : 0); 
	
	/* the status field should be 1 if blocked for maintenace reasons 
	* and 0 is not blocked....since we memset the struct nothing to do
	*/
	
	/* send the GRA to LibSng-SS7 */
	sng_cc_sta_request (1,
						0,
						0,
						sngss7_span->rx_grs.circuit,
						0,
						SIT_STA_GRSRSP,
						&gra);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx GRA (%d:%d)\n",
							sngss7_info->circuit->cic,
							sngss7_info->circuit->cic,
							(sngss7_info->circuit->cic + sngss7_span->rx_grs.range));
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_grs (ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_span_data_t 	*sngss7_span = ftdmchan->span->mod_data;
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	
	SiStaEvnt grs;
	
	memset (&grs, 0x0, sizeof(grs));
	
	grs.rangStat.eh.pres	= PRSNT_NODEF;
	grs.rangStat.range.pres	= PRSNT_NODEF;
	grs.rangStat.range.val	= sngss7_span->tx_grs.range;
	
	sng_cc_sta_request (1,
						0,
						0,
						sngss7_span->tx_grs.circuit,
						0,
						SIT_STA_GRSREQ,
						&grs);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx GRS (%d:%d)\n",
							sngss7_info->circuit->cic,
							sngss7_info->circuit->cic,
							(sngss7_info->circuit->cic + sngss7_span->tx_grs.range));
	
	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
return;
}

/******************************************************************************/
void ft_to_sngss7_cgba(ftdm_channel_t * ftdmchan)
{	
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_span_data_t 	*sngss7_span = ftdmchan->span->mod_data;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;
	int					x = 0;
	
	SiStaEvnt cgba;

	memset (&cgba, 0x0, sizeof(cgba));

	/* fill in the circuit group supervisory message */
	cgba.cgsmti.eh.pres = PRSNT_NODEF;
	cgba.cgsmti.typeInd.pres = PRSNT_NODEF;
	cgba.cgsmti.typeInd.val = sngss7_span->rx_cgb.type;

	cgba.rangStat.eh.pres = PRSNT_NODEF;
	/* fill in the range */	
	cgba.rangStat.range.pres = PRSNT_NODEF;
	cgba.rangStat.range.val = sngss7_span->rx_cgb.range;
	/* fill in the status */
	cgba.rangStat.status.pres = PRSNT_NODEF;
	cgba.rangStat.status.len = ((sngss7_span->rx_cgb.range + 1) >> 3) + (((sngss7_span->rx_cgb.range + 1) & 0x07) ? 1 : 0);
	for(x = 0; x < cgba.rangStat.status.len; x++){
		cgba.rangStat.status.val[x] = sngss7_span->rx_cgb.status[x];
	}

	sng_cc_sta_request (1,
						0,
						0,
						sngss7_span->rx_cgb.circuit,
						0,
						SIT_STA_CGBRSP,
						&cgba);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx CGBA (%d:%d)\n",
							sngss7_info->circuit->cic,
							sngss7_info->circuit->cic,
							(sngss7_info->circuit->cic + sngss7_span->rx_cgb.range));

	/* clean out the saved data */
	memset(&sngss7_span->rx_cgb, 0x0, sizeof(sngss7_group_data_t));

	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_cgua(ftdm_channel_t * ftdmchan)
{	
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);
	
	sngss7_span_data_t 	*sngss7_span = ftdmchan->span->mod_data;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;
	int					x = 0;
	
	SiStaEvnt cgua;

	memset (&cgua, 0x0, sizeof(cgua));

	/* fill in the circuit group supervisory message */
	cgua.cgsmti.eh.pres = PRSNT_NODEF;
	cgua.cgsmti.typeInd.pres = PRSNT_NODEF;
	cgua.cgsmti.typeInd.val = sngss7_span->rx_cgu.type;

	cgua.rangStat.eh.pres = PRSNT_NODEF;
	/* fill in the range */	
	cgua.rangStat.range.pres = PRSNT_NODEF;
	cgua.rangStat.range.val = sngss7_span->rx_cgu.range;
	/* fill in the status */
	cgua.rangStat.status.pres = PRSNT_NODEF;
	cgua.rangStat.status.len = ((sngss7_span->rx_cgu.range + 1) >> 3) + (((sngss7_span->rx_cgu.range + 1) & 0x07) ? 1 : 0);
	for(x = 0; x < cgua.rangStat.status.len; x++){
		cgua.rangStat.status.val[x] = sngss7_span->rx_cgu.status[x];
	}

	sng_cc_sta_request (1,
						0,
						0,
						sngss7_span->rx_cgu.circuit,
						0,
						SIT_STA_CGURSP,
						&cgua);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx CGUA (%d:%d)\n",
							sngss7_info->circuit->cic,
							sngss7_info->circuit->cic,
							(sngss7_info->circuit->cic + sngss7_span->rx_cgu.range));

	/* clean out the saved data */
	memset(&sngss7_span->rx_cgu, 0x0, sizeof(sngss7_group_data_t));

	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_cgb(ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);

	sngss7_span_data_t 	*sngss7_span = ftdmchan->span->mod_data;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;
	SiStaEvnt 			cgb;
	int					x = 0;


	memset (&cgb, 0x0, sizeof(cgb));

	/* fill in the circuit group supervisory message */
	cgb.cgsmti.eh.pres			= PRSNT_NODEF;
	cgb.cgsmti.typeInd.pres		= PRSNT_NODEF;
	cgb.cgsmti.typeInd.val		= sngss7_span->tx_cgb.type;

	/* fill in the range */	
	cgb.rangStat.eh.pres 		= PRSNT_NODEF;
	cgb.rangStat.range.pres		= PRSNT_NODEF;
	cgb.rangStat.range.val 		= sngss7_span->tx_cgb.range;

	/* fill in the status */
	cgb.rangStat.status.pres	= PRSNT_NODEF;
	cgb.rangStat.status.len 	= ((sngss7_span->tx_cgb.range + 1) >> 3) + (((sngss7_span->tx_cgb.range + 1) & 0x07) ? 1 : 0);
	for(x = 0; x < cgb.rangStat.status.len; x++){
		cgb.rangStat.status.val[x] = sngss7_span->tx_cgb.status[x];
	}

	sng_cc_sta_request (1,
						0,
						0,
						sngss7_span->tx_cgb.circuit,
						0,
						SIT_STA_CGBREQ,
						&cgb);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx CGB (%d:%d)\n",
							sngss7_info->circuit->cic,
							sngss7_info->circuit->cic,
							(sngss7_info->circuit->cic + sngss7_span->tx_cgb.range));

	/* clean out the saved data */
	memset(&sngss7_span->tx_cgb, 0x0, sizeof(sngss7_group_data_t));

	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
}

/******************************************************************************/
void ft_to_sngss7_cgu(ftdm_channel_t * ftdmchan)
{
	SS7_FUNC_TRACE_ENTER (__FUNCTION__);

	sngss7_span_data_t 	*sngss7_span = ftdmchan->span->mod_data;
	sngss7_chan_data_t	*sngss7_info = ftdmchan->call_data;
	SiStaEvnt 			cgu;
	int					x = 0;


	memset (&cgu, 0x0, sizeof(cgu));

	/* fill in the circuit group supervisory message */
	cgu.cgsmti.eh.pres			= PRSNT_NODEF;
	cgu.cgsmti.typeInd.pres		= PRSNT_NODEF;
	cgu.cgsmti.typeInd.val		= sngss7_span->tx_cgu.type;

	/* fill in the range */	
	cgu.rangStat.eh.pres 		= PRSNT_NODEF;
	cgu.rangStat.range.pres		= PRSNT_NODEF;
	cgu.rangStat.range.val 		= sngss7_span->tx_cgu.range;

	/* fill in the status */
	cgu.rangStat.status.pres	= PRSNT_NODEF;
	cgu.rangStat.status.len 	= ((sngss7_span->tx_cgu.range + 1) >> 3) + (((sngss7_span->tx_cgu.range + 1) & 0x07) ? 1 : 0);
	for(x = 0; x < cgu.rangStat.status.len; x++){
		cgu.rangStat.status.val[x] = sngss7_span->tx_cgu.status[x];
	}

	sng_cc_sta_request (1,
						0,
						0,
						sngss7_span->tx_cgu.circuit,
						0,
						SIT_STA_CGUREQ,
						&cgu);
	
	SS7_INFO_CHAN(ftdmchan,"[CIC:%d]Tx CGU (%d:%d)\n",
							sngss7_info->circuit->cic,
							sngss7_info->circuit->cic,
							(sngss7_info->circuit->cic + sngss7_span->tx_cgu.range));

	/* clean out the saved data */
	memset(&sngss7_span->tx_cgu, 0x0, sizeof(sngss7_group_data_t));

	SS7_FUNC_TRACE_EXIT (__FUNCTION__);
	return;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
/******************************************************************************/
