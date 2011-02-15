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
void handle_sng_log(uint8_t level, char *fmt,...);
void handle_sng_mtp1_alarm(Pst *pst, L1Mngmt *sta);
void handle_sng_mtp2_alarm(Pst *pst, SdMngmt *sta);
void handle_sng_mtp3_alarm(Pst *pst, SnMngmt *sta);
void handle_sng_isup_alarm(Pst *pst, SiMngmt *sta);
void handle_sng_cc_alarm(Pst *pst, CcMngmt *sta);
void handle_sng_relay_alarm(Pst *pst, RyMngmt *sta);

/******************************************************************************/

/* FUNCTIONS ******************************************************************/
void handle_sng_log(uint8_t level, char *fmt,...)
{
	char	*data;
	int		ret;
	va_list	ap;

	va_start(ap, fmt);
	ret = vasprintf(&data, fmt, ap);
	if (ret == -1) {
		return;
	}

	switch (level) {
	/**************************************************************************/
	case SNG_LOGLEVEL_DEBUG:
		ftdm_log(FTDM_LOG_DEBUG, "sng_ss7->%s", data);
		break;
	/**************************************************************************/
	case SNG_LOGLEVEL_WARN:
		ftdm_log(FTDM_LOG_WARNING, "sng_ss7->%s", data);
		break;
	/**************************************************************************/
	case SNG_LOGLEVEL_INFO:
		ftdm_log(FTDM_LOG_INFO, "sng_ss7->%s", data);
		break;
	/**************************************************************************/
	case SNG_LOGLEVEL_NOTICE:
		ftdm_log(FTDM_LOG_NOTICE, "sng_ss7->%s", data);
		break;
	/**************************************************************************/
	case SNG_LOGLEVEL_ERROR:
		ftdm_log(FTDM_LOG_ERROR, "sng_ss7->%s", data);
		break;
	/**************************************************************************/
	case SNG_LOGLEVEL_CRIT:
		/*printf("%s",data);*/
		ftdm_log(FTDM_LOG_CRIT, "sng_ss7->%s", data);
		break;
	/**************************************************************************/
	default:
		ftdm_log(FTDM_LOG_INFO, "sng_ss7->%s", data);
		break;
	/**************************************************************************/
	}

	return;
}

/******************************************************************************/
void handle_sng_mtp1_alarm(Pst *pst, L1Mngmt *sta)
{


}   /* handle_mtp1_alarm */

/******************************************************************************/
void handle_sng_mtp2_alarm(Pst *pst, SdMngmt *sta)
{
	char	buf[50];
	int		x = 1;

	memset(buf, '\0', sizeof(buf));

	switch (sta->t.usta.alarm.category) {
	/**************************************************************************/
	case (LCM_CATEGORY_PROTOCOL):
	case (LCM_CATEGORY_INTERFACE):

		switch (sta->t.usta.alarm.event) {
		/**********************************************************************/
		case (LSD_EVENT_ENTR_CONG):
		case (LSD_EVENT_EXIT_CONG):
		case (LSD_EVENT_PROT_ST_UP):
		case (LSD_EVENT_PROT_ST_DN):
		case (LSD_EVENT_LINK_ALIGNED):
		case (LSD_EVENT_REMOTE_CONG_START):
		case (LSD_EVENT_REMOTE_CONG_END):
		case (LSD_EVENT_RX_REMOTE_SIPO):

			/* find the name for the sap in question */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.mtp2Link[x].id != 0) {
				if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == sta->t.usta.evntParm[0]) {
					break;
				}
				x++;
			}

			if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == 0) {
				sprintf(buf, "[SAPID:%d]", sta->t.usta.evntParm[0]);
			} else {
				sprintf(buf, "[%s]", g_ftdm_sngss7_data.cfg.mtp2Link[x].name);
			}
					

			switch (sta->t.usta.alarm.cause) {
			/******************************************************************/
			case (LCM_CAUSE_UNKNOWN):
				ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s %s\n",
											buf,
											DECODE_LSD_EVENT(sta->t.usta.alarm.event));
				break;
			/******************************************************************/
			case (LCM_CAUSE_MGMT_INITIATED):
				ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s[MGMT] %s\n",
											buf,
											DECODE_LSD_EVENT(sta->t.usta.alarm.event));
				break;
			/******************************************************************/
			default:
				ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s %s (***unknown cause***)\n",
											buf,
											DECODE_LSD_EVENT(sta->t.usta.alarm.event));
				break;
			/******************************************************************/
			} /* switch (sta->t.usta.alarm.cause) */
			break;
		/**********************************************************************/
		case (LSD_EVENT_PROT_ERR):
			
			/* find the name for the sap in question */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.mtp2Link[x].id != 0) {
				if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == sta->t.usta.evntParm[0]) {
					break;
				}
				x++;
			}

			if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == 0) {
				sprintf(buf, "[SAPID:%d]", sta->t.usta.evntParm[0]);
			} else {
				sprintf(buf, "[%s]", g_ftdm_sngss7_data.cfg.mtp2Link[x].name);
			}

			ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s %s : %s\n",
										buf,
										DECODE_LSD_EVENT(sta->t.usta.alarm.event),
										DECODE_LSD_CAUSE(sta->t.usta.alarm.cause));
			break;
		/**********************************************************************/
		case (LSD_EVENT_ALIGN_LOST):
			
			/* find the name for the sap in question */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.mtp2Link[x].id != 0) {
				if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == sta->t.usta.evntParm[0]) {
					break;
				}
				x++;
			}

			if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == 0) {
				sprintf(buf, "[SAPID:%d]", sta->t.usta.evntParm[0]);
			} else {
				sprintf(buf, "[%s]", g_ftdm_sngss7_data.cfg.mtp2Link[x].name);
			}

			ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s %s : %s\n",
										buf,
										DECODE_LSD_EVENT(sta->t.usta.alarm.event),
										DECODE_DISC_REASON(sta->t.usta.evntParm[1]));
			break;
		/**********************************************************************/
		case (LSD_EVENT_RTB_FULL):
		case (LSD_EVENT_RTB_FULL_OVER):

			/* find the name for the sap in question */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.mtp2Link[x].id != 0) {
				if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == sta->t.usta.evntParm[0]) {
					break;
				}
				x++;
			}

			if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == 0) {
				sprintf(buf, "[SAPID:%d]", sta->t.usta.evntParm[0]);
			} else {
				sprintf(buf, "[%s]", g_ftdm_sngss7_data.cfg.mtp2Link[x].name);
			}

			ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s %s : RTB Queue Len(%d)|Oldest BSN(%d)|Tx Queue Len(%d)|Outstanding Frames(%d)\n",
										buf,
										DECODE_LSD_EVENT(sta->t.usta.alarm.event),
										sta->t.usta.evntParm[1],
										sta->t.usta.evntParm[2],
										sta->t.usta.evntParm[3],
										sta->t.usta.evntParm[4]);
			break;
		/**********************************************************************/
		case (LSD_EVENT_NEG_ACK):

			/* find the name for the sap in question */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.mtp2Link[x].id != 0) {
				if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == sta->t.usta.evntParm[0]) {
					break;
				}
				x++;
			}

			if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == 0) {
				sprintf(buf, "[SAPID:%d]", sta->t.usta.evntParm[0]);
			} else {
				sprintf(buf, "[%s]", g_ftdm_sngss7_data.cfg.mtp2Link[x].name);
			}

			ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s %s : RTB Queue Len(%d)\n",
										buf,
										DECODE_LSD_EVENT(sta->t.usta.alarm.event),
										sta->t.usta.evntParm[1]);
			break;
		/**********************************************************************/
		case (LSD_EVENT_DAT_CFM_SDT):

			/* find the name for the sap in question */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.mtp2Link[x].id != 0) {
				if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == sta->t.usta.evntParm[0]) {
					break;
				}
				x++;
			}

			if (g_ftdm_sngss7_data.cfg.mtp2Link[x].id == 0) {
				sprintf(buf, "[SAPID:%d]", sta->t.usta.evntParm[0]);
			} else {
				sprintf(buf, "[%s]", g_ftdm_sngss7_data.cfg.mtp2Link[x].name);
			}

			ftdm_log(FTDM_LOG_ERROR,"[MTP2]%s %s : %d\n",
										buf,
										DECODE_LSD_EVENT(sta->t.usta.alarm.event),
										DECODE_DISC_REASON(sta->t.usta.evntParm[1]));
			break;
		/**********************************************************************/
		case (LCM_EVENT_UI_INV_EVT):
		case (LCM_EVENT_LI_INV_EVT):
			ftdm_log(FTDM_LOG_ERROR,"[MTP2] %s(%d) : %s(%d) : Primitive (%d)\n",
										DECODE_LSD_EVENT(sta->t.usta.alarm.event),
										sta->t.usta.alarm.event,
										DECODE_LCM_CAUSE(sta->t.usta.alarm.cause),
										sta->t.usta.alarm.cause,
										sta->t.usta.evntParm[0]);
			break;
		/**********************************************************************/
		case (LCM_EVENT_INV_EVT):

			switch (sta->t.usta.alarm.cause) {
			/******************************************************************/
			case (LCM_CAUSE_UNKNOWN):
			case (LCM_CAUSE_SWVER_NAVAIL):
				ftdm_log(FTDM_LOG_ERROR,"[MTP2] %s : %s : Event (%d)\n",
											DECODE_LSD_EVENT(sta->t.usta.alarm.event),
											DECODE_LCM_CAUSE(sta->t.usta.alarm.cause),
											sta->t.usta.evntParm[0]);
				break;
			/******************************************************************/
			case (LCM_CAUSE_DECODE_ERR):
				ftdm_log(FTDM_LOG_ERROR,"[MTP2] %s : %s : Primitive (%d)|Version (%d)\n",
											DECODE_LSD_EVENT(sta->t.usta.alarm.event),
											DECODE_LCM_CAUSE(sta->t.usta.alarm.cause),
											sta->t.usta.evntParm[0],
											sta->t.usta.evntParm[1]);
				break;
			/******************************************************************/
			default:
				ftdm_log(FTDM_LOG_ERROR,"[MTP2] %s(%d) : %s(%d)\n",
											DECODE_LSD_EVENT(sta->t.usta.alarm.event),
											sta->t.usta.alarm.event,
											DECODE_LSD_CAUSE(sta->t.usta.alarm.cause),
											sta->t.usta.alarm.cause);
				break;
			/******************************************************************/
			} /* switch (sta->t.usta.alarm.cause) */
			break;
		/**********************************************************************/
		default:
			ftdm_log(FTDM_LOG_ERROR,"[MTP2] %s(%d) : %s(%d)\n",
										DECODE_LSD_EVENT(sta->t.usta.alarm.event),
										sta->t.usta.alarm.event,
										DECODE_LSD_CAUSE(sta->t.usta.alarm.cause),
										sta->t.usta.alarm.cause);
			break;
		/**********************************************************************/
		} /* switch (sta->t.usta.alarm.event) */
		break;
	/**************************************************************************/
	default:
		ftdm_log(FTDM_LOG_ERROR,"[MTP2] Unknown alarm category %d\n",
									sta->t.usta.alarm.category);
		break;
	/**************************************************************************/
	} /* switch(sta->t.usta.alarm.category) */

	return;
}   /* handle_mtp2_alarm */

/******************************************************************************/
void handle_sng_mtp3_alarm(Pst *pst, SnMngmt *sta)
{
	char	buf[50];
	int		x = 1;

	memset(buf, '\0', sizeof(buf));

	switch (sta->hdr.elmId.elmnt) {
	/**************************************************************************/
	case (STDLSAP):

			/* find the name for the sap in question */
			x = 1;
			while (g_ftdm_sngss7_data.cfg.mtp3Link[x].id != 0) {
				if (g_ftdm_sngss7_data.cfg.mtp3Link[x].id == sta->hdr.elmId.elmntInst1) {
					break;
				}
				x++;
			}

			if (g_ftdm_sngss7_data.cfg.mtp3Link[x].id == 0) {
				sprintf(buf, "[SAPID:%d]", sta->hdr.elmId.elmntInst1);
			} else {
				sprintf(buf, "[%s]", g_ftdm_sngss7_data.cfg.mtp3Link[x].name);
			}

		switch (sta->t.usta.alarm.event) {
		/**********************************************************************/
		case (LSN_EVENT_INV_OPC_OTHER_END):
			
			ftdm_log(FTDM_LOG_ERROR,"[MTP3]%s %s : %s : OPC(0x%X%X%X%X)\n",
										buf,
										DECODE_LSN_EVENT(sta->t.usta.alarm.event),
										DECODE_LSN_CAUSE(sta->t.usta.alarm.cause),
										sta->t.usta.evntParm[3],
										sta->t.usta.evntParm[2],
										sta->t.usta.evntParm[1],
										sta->t.usta.evntParm[0]);
			break;
		/**********************************************************************/
		case (LSN_EVENT_INV_SLC_OTHER_END):
			ftdm_log(FTDM_LOG_ERROR,"[MTP3]%s %s : %s : SLC(%d)\n",
										buf,
										DECODE_LSN_EVENT(sta->t.usta.alarm.event),
										DECODE_LSN_CAUSE(sta->t.usta.alarm.cause),
										sta->t.usta.evntParm[0]);
			break;
		/**********************************************************************/
		default:
			ftdm_log(FTDM_LOG_ERROR,"[MTP3]%s %s(%d) : %s(%d)\n",
										buf,
										DECODE_LSN_EVENT(sta->t.usta.alarm.event),
										sta->t.usta.alarm.event,
										DECODE_LSN_CAUSE(sta->t.usta.alarm.cause),
										sta->t.usta.alarm.cause);
			break;
		/**********************************************************************/
		} /* sta->t.usta.alarm.event */
		break;
	/**************************************************************************/
	case (STNSAP):
		ftdm_log(FTDM_LOG_ERROR,"[MTP3][SAPID:%d] %s : %s\n",
									sta->hdr.elmId.elmntInst1,
									DECODE_LSN_EVENT(sta->t.usta.alarm.event),
									DECODE_LSN_CAUSE(sta->t.usta.alarm.cause));
		break;
	/**************************************************************************/
	case (STLNKSET):
		ftdm_log(FTDM_LOG_ERROR,"[MTP3][LNKSET:%d] %s : %s\n",
									sta->hdr.elmId.elmntInst1,
									DECODE_LSN_EVENT(sta->t.usta.alarm.event),
									DECODE_LSN_CAUSE(sta->t.usta.alarm.cause));
		break;
	/**************************************************************************/
	case (STROUT):
		switch (sta->t.usta.alarm.event) {
		/**********************************************************************/
		case (LSN_EVENT_RX_TRANSFER_MSG):
			switch (sta->t.usta.evntParm[5]) {
			/******************************************************************/
			case (0x23):
				ftdm_log(FTDM_LOG_INFO,"[MTP3] Rx SNM TFC\n");
				break;
			/******************************************************************/
			case (0x34):
				ftdm_log(FTDM_LOG_INFO,"[MTP3] Rx SNM TFR\n");
				break;
			/******************************************************************/
			case (0x54):
				ftdm_log(FTDM_LOG_INFO,"[MTP3] Rx SNM TFA\n");
				break;
			/******************************************************************/
			case (0x14):
				ftdm_log(FTDM_LOG_INFO,"[MTP3] Rx SNM TFP\n");
				break;
			/******************************************************************/
			case (0x24):
				ftdm_log(FTDM_LOG_INFO,"[MTP3] Rx SNM TFP (cluster)\n");
				break;
			/******************************************************************/
			case (0x64):
				ftdm_log(FTDM_LOG_INFO,"[MTP3] Rx SNM TFA (cluster)\n");
				break;
			/******************************************************************/
			case (0x44):
				ftdm_log(FTDM_LOG_INFO,"[MTP3] Rx SNM TFR (cluster)\n");
				break;
			/******************************************************************/
			} /* switch (sta->t.usta.evntParm[5]) */
			break;
		/**********************************************************************/
		default:
			ftdm_log(FTDM_LOG_ERROR,"[MTP3][DPC:0x%X%X%X%X] %s : %s\n",
										sta->t.usta.evntParm[0],
										sta->t.usta.evntParm[1],
										sta->t.usta.evntParm[2],
										sta->t.usta.evntParm[3],
										DECODE_LSN_EVENT(sta->t.usta.alarm.event),
										DECODE_LSN_CAUSE(sta->t.usta.alarm.cause));
			break;
		/**********************************************************************/
		} /* switch (sta->t.usta.alarm.event) */
		break;
	/**************************************************************************/
	default:
		ftdm_log(FTDM_LOG_ERROR,"[MTP3] %s(%d) : %s(%d)\n",
									DECODE_LSN_EVENT(sta->t.usta.alarm.event),
									sta->t.usta.alarm.event,
									DECODE_LSN_CAUSE(sta->t.usta.alarm.cause),
									sta->t.usta.alarm.cause);
		break;
	/**************************************************************************/
	} /* switch (sta->hdr.elmId.elmnt) */

	return;
}   /* handle_mtp3_alarm */

/******************************************************************************/
void handle_sng_isup_alarm(Pst *pst, SiMngmt *sta)
{
	char	msg[250];
	char	tmp[25];
	char	*p = NULL;
	int		x = 0;

	/* initalize the msg variable to NULLs */
	memset(&msg[0], '\0', sizeof(&msg));

	/* if the event is REMOTE/LOCAL we don't need to print these */
	if ((sta->t.usta.alarm.event == LSI_EVENT_REMOTE) ||
		(sta->t.usta.alarm.event == LSI_EVENT_LOCAL)) {
		return;
	}


	/* point p to the first spot in msg */
	p = &msg[0];

	p = strcat(p, "[ISUP]");

	/* go through the dgnVals */
	for (x = 0; x < 5; x++) {
		switch (sta->t.usta.dgn.dgnVal[x].type) {
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_NONE):
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_EVENT):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[EVENT:%d]",sta->t.usta.dgn.dgnVal[x].t.event);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_SPID):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[SPID:%d]",sta->t.usta.dgn.dgnVal[x].t.spId);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_SUID):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[SUID:%d]",sta->t.usta.dgn.dgnVal[x].t.suId);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_SPINSTID):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[SPINSTID:%d]", (int)sta->t.usta.dgn.dgnVal[x].t.spInstId);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_SUINSTID):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[SUINSTID:%d]", (int)sta->t.usta.dgn.dgnVal[x].t.suInstId);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_CIRCUIT):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[CKT:%d]", (int)sta->t.usta.dgn.dgnVal[x].t.cirId);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_CIC):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[CIC:%d]", (int)sta->t.usta.dgn.dgnVal[x].t.cic);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_INTF):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[INTF:%d]", (int)sta->t.usta.dgn.dgnVal[x].t.intfId);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_DPC):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[DPC:%d]", (int)sta->t.usta.dgn.dgnVal[x].t.dpc);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_ADDRS):
#if 0
			/*
			 *typedef struct addrs
			 *{
			 *U8 length;
			 *U8 strg[ADRLEN];
			 *} Addrs;
			 */
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[ADDRS:%d]",sta->t.usta.dgn.dgnVal[x].t.);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
#endif
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_SWTCH):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[SWTCH:%d]",sta->t.usta.dgn.dgnVal[x].t.swtch);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_RANGE):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[RANGE:0x%X]",sta->t.usta.dgn.dgnVal[x].t.range);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_STATUS_OCTS):
#if 0
			/*
			 *typedef struct addrs
			 *{
			 *U8 length;
			 *U8 strg[ADRLEN];
			 *} Addrs;
			 */
			/* init tmp with NULLs */
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[STATUS_OCT:0x%X]",sta->t.usta.dgn.dgnVal[x].t.);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
#endif
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_VER):
#ifdef SI_RUG
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[VER:%d]",sta->t.usta.dgn.dgnVal[x].t.intfVer);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
#endif
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_TIMER):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[TIMER:0x%X]",sta->t.usta.dgn.dgnVal[x].t.tmrInfo);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_MSGTYPE):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[MSGTYPE:%d]",sta->t.usta.dgn.dgnVal[x].t.msgType);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		case (LSI_USTA_DGNVAL_STATE):
			/* init tmp with NULLs */
			memset(&tmp[0], '\0', sizeof(&tmp));

			/* fill in the dgn val to tmp */
			sprintf(&tmp[0], "[STATE:%d]",sta->t.usta.dgn.dgnVal[x].t.state);

			/* concat tmp to msg */
			p = strcat(p, &tmp[0]);
			break;
		/**********************************************************************/
		default:
			break;
		/**********************************************************************/
		} /* switch (sta->t.usta.dgn.dgnVal[x].t.type) */
	} /* for  (x = 0; x < 5; x++) */
		
	ftdm_log(FTDM_LOG_ERROR,"%s %s : %s\n",
								msg,
								DECODE_LSI_EVENT(sta->t.usta.alarm.event),
								DECODE_LSI_CAUSE(sta->t.usta.alarm.cause));

	return;

}   /* handle_isup_alarm */

/******************************************************************************/
void handle_sng_cc_alarm(Pst *pst, CcMngmt *sta)
{

	return;
}   /* handle_cc_alarm */

/******************************************************************************/
void handle_sng_relay_alarm(Pst *pst, RyMngmt *sta)
{


	switch (sta->hdr.elmId.elmnt) {
	/**************************************************************************/
	case (LRY_USTA_ERR): /* ERROR */
		ftdm_log(FTDM_LOG_ERROR,"[RELAY] Error: tx procId %d: err procId %d: channel %d: seq %s: reason %s\n",
												sta->t.usta.s.ryErrUsta.sendPid,
												sta->t.usta.s.ryErrUsta.errPid,
												sta->t.usta.s.ryErrUsta.id,
												DECODE_LRY_SEQ(sta->t.usta.s.ryErrUsta.sequence),
												DECODE_LRY_REASON(sta->t.usta.s.ryErrUsta.reason));

		/* process the event */
		switch (sta->t.usta.s.ryErrUsta.reason) {
		/**********************************************************************/
		case (LRYRSNMGMTREQ):
			/* do nothing since this is a shutdown */
			break;
		/**********************************************************************/
		default:
			/* handle the error */
			handle_relay_disconnect_on_error(sta);
			break;
		/**********************************************************************/
		} /* switch (sta->t.usta.s.ryErrUsta.reason) */

		break;
	/**************************************************************************/
	case (LRY_USTA_CNG): /* Congestion */
		ftdm_log(FTDM_LOG_ERROR,"[RELAY] Congestion: tx procId %d: rem procId %d: channel %d: %s\n",
												sta->t.usta.s.ryCongUsta.sendPid,
												sta->t.usta.s.ryCongUsta.remPid,
												sta->t.usta.s.ryCongUsta.id,
												DECODE_LRY_CONG_FLAGS(sta->t.usta.s.ryCongUsta.flags));
		break;
	/**************************************************************************/
	case (LRY_USTA_UP): /* channel up */
		ftdm_log(FTDM_LOG_ERROR,"[RELAY] Channel UP: tx procId %d: channel %d\n",
												sta->t.usta.s.ryUpUsta.sendPid,
												sta->t.usta.s.ryUpUsta.id);

		/* process the event */
		handle_relay_connect(sta);

		break;
	/**************************************************************************/
	case (LRY_USTA_DN): /* channel down */
		ftdm_log(FTDM_LOG_ERROR,"[RELAY] Channel DOWN: tx procId %d: channel %d\n",
												sta->t.usta.s.ryUpUsta.sendPid,
												sta->t.usta.s.ryUpUsta.id);

		/* process the event */
		handle_relay_disconnect_on_down(sta);

		break;
	/**************************************************************************/
	default:
		ftdm_log(FTDM_LOG_ERROR,"Unknown Relay Alram\n");
		break;
	/**************************************************************************/
	} /* switch (sta->hdr.elmId.elmnt) */

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
