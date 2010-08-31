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
int ft_to_sngss7_activate_all(void);

static int ftmod_ss7_enable_isap(int suId);
static int ftmod_ss7_enable_nsap(int suId);
static int ftmod_ss7_enable_mtpLinkSet(int lnkSetId);

int ftmod_ss7_inhibit_mtplink(uint32_t id);
int ftmod_ss7_uninhibit_mtplink(uint32_t id);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
int ft_to_sngss7_activate_all(void)
{
	int x;

	x = 1;
	while (g_ftdm_sngss7_data.cfg.isap[x].id != 0) {
		/* check if this link has already been actived */
		if (!(g_ftdm_sngss7_data.cfg.isap[x].flags & ACTIVE)) {

			if (ftmod_ss7_enable_isap(x)) {	
				SS7_CRITICAL("ISAP %d Enable: NOT OK\n", x);
				SS7_ASSERT;
			} else {
				SS7_INFO("ISAP %d Enable: OK\n", x);
			}

			/* set the ACTIVE flag */
			g_ftdm_sngss7_data.cfg.isap[x].flags |= ACTIVE;
		} /* if !ACTIVE */
		
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.isap[x].id != 0) */

	x = 1;
	while (g_ftdm_sngss7_data.cfg.nsap[x].id != 0) {
		/* check if this link has already been actived */
		if (!(g_ftdm_sngss7_data.cfg.nsap[x].flags & ACTIVE)) {

			if (ftmod_ss7_enable_nsap(x)) {	
				SS7_CRITICAL("NSAP %d Enable: NOT OK\n", x);
				SS7_ASSERT;
			} else {
				SS7_INFO("NSAP %d Enable: OK\n", x);
			}

			/* set the ACTIVE flag */
			g_ftdm_sngss7_data.cfg.nsap[x].flags |= ACTIVE;
		} /* if !ACTIVE */
		
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.nsap[x].id != 0) */

	x = 1;
	while (g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id != 0) {
		/* check if this link has already been actived */
		if (!(g_ftdm_sngss7_data.cfg.mtpLinkSet[x].flags & ACTIVE)) {

			if (ftmod_ss7_enable_mtpLinkSet(x)) {	
				SS7_CRITICAL("LinkSet \"%s\" Enable: NOT OK\n", g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name);
				SS7_ASSERT;
			} else {
				SS7_INFO("LinkSet \"%s\" Enable: OK\n", g_ftdm_sngss7_data.cfg.mtpLinkSet[x].name);
			}

			/* set the ACTIVE flag */
			g_ftdm_sngss7_data.cfg.mtpLinkSet[x].flags |= ACTIVE;
		} /* if !ACTIVE */
		
		x++;
	} /* while (g_ftdm_sngss7_data.cfg.mtpLinkSet[x].id != 0) */

	return 0;
}

/******************************************************************************/
static int ftmod_ss7_enable_isap(int suId)
{
	CcMngmt cntrl;
	Pst pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTCC;

	/* initalize the control structure */
	memset(&cntrl, 0x0, sizeof(CcMngmt));

	/* initalize the control header */
	smHdrInit(&cntrl.hdr);

	cntrl.hdr.msgType			= TCNTRL;		/* this is a control request */
	cntrl.hdr.entId.ent			= ENTCC;
	cntrl.hdr.entId.inst		= S_INST;
	cntrl.hdr.elmId.elmnt		= STISAP;

	cntrl.hdr.elmId.elmntInst1	= suId;			/* this is the SAP to bind */

	cntrl.t.cntrl.action		= ABND_ENA;		/* bind and activate */
	cntrl.t.cntrl.subAction		= SAELMNT;		/* specificed element */

	return (sng_cntrl_cc(&pst, &cntrl));
}

/******************************************************************************/
static int ftmod_ss7_enable_nsap(int suId)
{
	SiMngmt cntrl;
	Pst pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSI;

	/* initalize the control structure */
	memset(&cntrl, 0x0, sizeof(SiMngmt));

	/* initalize the control header */
	smHdrInit(&cntrl.hdr);

	cntrl.hdr.msgType			= TCNTRL;	   /* this is a control request */
	cntrl.hdr.entId.ent			= ENTSI;
	cntrl.hdr.entId.inst		= S_INST;
	cntrl.hdr.elmId.elmnt		= STNSAP;

	cntrl.t.cntrl.s.siElmnt.elmntId.sapId				= suId; 
	cntrl.t.cntrl.s.siElmnt.elmntParam.nsap.nsapType	= SAP_MTP; 


	cntrl.t.cntrl.action		= ABND_ENA;		/* bind and activate */
	cntrl.t.cntrl.subAction		= SAELMNT;		/* specificed element */

	return (sng_cntrl_isup(&pst, &cntrl));
}

/******************************************************************************/
static int ftmod_ss7_enable_mtpLinkSet(int lnkSetId)
{
	SnMngmt cntrl;
	Pst pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/* initalize the control structure */
	memset(&cntrl, 0x0, sizeof(SnMngmt));

	/* initalize the control header */
	smHdrInit(&cntrl.hdr);

	cntrl.hdr.msgType			= TCNTRL;		/* this is a control request */
	cntrl.hdr.entId.ent			= ENTSN;
	cntrl.hdr.entId.inst		= S_INST;
	cntrl.hdr.elmId.elmnt		= STLNKSET;
	cntrl.hdr.elmId.elmntInst1	= lnkSetId;		/* this is the linkset to bind */

	cntrl.t.cntrl.action		= ABND_ENA;		/* bind and activate */
	cntrl.t.cntrl.subAction		= SAELMNT;		/* specificed element */

	return (sng_cntrl_mtp3(&pst, &cntrl));
}

/******************************************************************************/
int ftmod_ss7_inhibit_mtplink(uint32_t id)
{
	SnMngmt cntrl;
	Pst pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/* initalize the control structure */
	memset(&cntrl, 0x0, sizeof(SnMngmt));

	/* initalize the control header */
	smHdrInit(&cntrl.hdr);

	cntrl.hdr.msgType			= TCNTRL;	/* this is a control request */
	cntrl.hdr.entId.ent			= ENTSN;
	cntrl.hdr.entId.inst		= S_INST;
	cntrl.hdr.elmId.elmnt		= STDLSAP;
	cntrl.hdr.elmId.elmntInst1	= id;		/* the DSLAP to inhibit  */

	cntrl.t.cntrl.action		= AINH;		/* Inhibit */
	cntrl.t.cntrl.subAction		= SAELMNT;	/* specificed element */

	return (sng_cntrl_mtp3(&pst, &cntrl));
}

/******************************************************************************/
int ftmod_ss7_uninhibit_mtplink(uint32_t id)
{
	SnMngmt cntrl;
	Pst pst;

	/* initalize the post structure */
	smPstInit(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTSN;

	/* initalize the control structure */
	memset(&cntrl, 0x0, sizeof(SnMngmt));

	/* initalize the control header */
	smHdrInit(&cntrl.hdr);

	cntrl.hdr.msgType			= TCNTRL;	/* this is a control request */
	cntrl.hdr.entId.ent			= ENTSN;
	cntrl.hdr.entId.inst		= S_INST;
	cntrl.hdr.elmId.elmnt		= STDLSAP;
	cntrl.hdr.elmId.elmntInst1	= id;		/* the DSLAP to inhibit  */

	cntrl.t.cntrl.action		= AUNINH;		/* Inhibit */
	cntrl.t.cntrl.subAction		= SAELMNT;	/* specificed element */

	return (sng_cntrl_mtp3(&pst, &cntrl));
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

