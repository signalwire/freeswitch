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

/* FUNCTION PROTOTYPES ********************************************************/

/******************************************************************************/

/* FUNCTIONS ******************************************************************/

void handle_mg_alarm(Pst *pst, MgMngmt *usta)
{
	U16 ret;
	int len = 0x00;
	char prBuf[3048];

	memset(&prBuf[0], 0, sizeof(prBuf));

	len = len + sprintf(prBuf+len,"MG Status Indication: received for sapId[%d] with Category = %d, Event = %d, Cause = %d \n",
			usta->t.usta.alarmInfo.sapId, usta->t.usta.alarm.category, usta->t.usta.alarm.event, 
			usta->t.usta.alarm.cause);

	len = len + sprintf(prBuf+len, "Category ( ");

	switch (usta->t.usta.alarm.category)
	{
		case LCM_CATEGORY_PROTOCOL:
			{
				len = len + sprintf(prBuf+len, "protocol related ");
				break;
			}
		case LCM_CATEGORY_INTERFACE:
			{
				len = len + sprintf(prBuf+len, "interface related ");
				break;
			}
		case LCM_CATEGORY_INTERNAL:
			{
				len = len + sprintf(prBuf+len, "internal ");
				break;
			}
		case LCM_CATEGORY_RESOURCE:
			{
				len = len + sprintf(prBuf+len, "system resources related ");
				break;
			}
		case LCM_CATEGORY_PSF_FTHA:
			{
				len = len + sprintf(prBuf+len, "fault tolerance / high availability PSF related ");
				break;
			}
		case LCM_CATEGORY_LYR_SPECIFIC:
			{
				len = len + sprintf(prBuf+len, "MGCP related ");
				break;
			}
		default:
			{
				len = len + sprintf(prBuf+len, "unknown: %d", (int)(usta->t.usta.alarm.category));
				break;
			}
	}
	len = len + sprintf(prBuf+len, ") ");

	len = len + sprintf(prBuf+len, " Event ( ");
	switch (usta->t.usta.alarm.event)
	{
        case LMG_EVENT_ALL_MGC_FAILED:
            {
				//mgco_process_mgc_failure(usta->t.usta.alarmInfo.sapId);
                len = len + sprintf(prBuf+len, "ALL MGC Failed ");
                break;
            }
        case LMG_EVENT_MGC_FAILED:
            {
                len = len + sprintf(prBuf+len, "MGC Failed ");
                break;
            }
		case LMG_EVENT_TSAP_RECVRY_SUCCESS:
			{
				len = len + sprintf(prBuf+len, "TSAP recovery success");
				break;
			}
		case LMG_EVENT_TSAP_RECVRY_FAILED:
			{
				len = len + sprintf(prBuf+len, "TSAP recovery failed");
				break;
			}
		case LCM_EVENT_UI_INV_EVT:
			{
				len = len + sprintf(prBuf+len, "upper interface invalid event");
				break;
			}
		case LCM_EVENT_LI_INV_EVT:
			{
				len = len + sprintf(prBuf+len, "lower interface invalid event");
				break;
			}
		case LCM_EVENT_PI_INV_EVT:
			{
				len = len + sprintf(prBuf+len, "peer interface invalid event");
				break;
			}
		case LCM_EVENT_INV_EVT:
			{
				len = len + sprintf(prBuf+len, "general invalid event");
				break;
			}
		case LCM_EVENT_INV_STATE:
			{
				len = len + sprintf(prBuf+len, "invalid internal state");
				break;
			}
		case LCM_EVENT_INV_TMR_EVT:
			{
				len = len + sprintf(prBuf+len, "invalid timer event");
				break;
			}
		case LCM_EVENT_MI_INV_EVT:
			{
				len = len + sprintf(prBuf+len, "management interface invalid event");
				break;
			}
		case LCM_EVENT_BND_FAIL:
			{
				len = len + sprintf(prBuf+len, "bind failure");
				break;
			}
		case LCM_EVENT_NAK:
			{
				len = len + sprintf(prBuf+len, "destination nack");
				break;
			}
		case LCM_EVENT_TIMEOUT:
			{
				len = len + sprintf(prBuf+len, "timeout");
				break;
			}
		case LCM_EVENT_BND_OK:
			{
				len = len + sprintf(prBuf+len, "bind ok");
				break;
			}
		case LCM_EVENT_SMEM_ALLOC_FAIL:
			{
				len = len + sprintf(prBuf+len, "static memory allocation failed");
				break;
			}
		case LCM_EVENT_DMEM_ALLOC_FAIL:
			{
				len = len + sprintf(prBuf+len, "dynamic mmemory allocation failed");
				break;
			}
		case LCM_EVENT_LYR_SPECIFIC:
			{
				len = len + sprintf(prBuf+len, "MGCP specific");
				break;
			}
		default:
			{
				len = len + sprintf(prBuf+len, "unknown event %d", (int)(usta->t.usta.alarm.event));
				break;
			}
		case LMG_EVENT_HIT_BNDCFM:
			{
				len = len + sprintf(prBuf+len, "HIT bind confirm");
				break;
			}
		case LMG_EVENT_HIT_CONCFM:
			{
				len = len + sprintf(prBuf+len, "HIT connect confirm");
				break;
			}
		case LMG_EVENT_HIT_DISCIND:
			{
				len = len + sprintf(prBuf+len, "HIT disconnect indication");
				break;
			}
		case LMG_EVENT_HIT_UDATIND:
			{
				len = len + sprintf(prBuf+len, "HIT unit data indication");
				break;
			}
		case LMG_EVENT_MGT_BNDREQ:
			{
				len = len + sprintf(prBuf+len, "MGT bind request");
				break;
			}
		case LMG_EVENT_PEER_CFG_FAIL:
			{
				len = len + sprintf(prBuf+len, "Peer Configuration Failed");
				break;
			}
		case LMG_EVENT_MGT_UBNDREQ:
			{
				len = len + sprintf(prBuf+len, "MGT unbind request");
				break;
			}
		case LMG_EVENT_MGT_MGCPTXNREQ:
			{
				len = len + sprintf(prBuf+len, "MGT MGCP transaction request");
				break;
			}
		case LMG_EVENT_MGT_MGCPTXNIND:
			{
				len = len + sprintf(prBuf+len, "MGT MGCP transaction indication");
				break;
			}

		case LMG_EVENT_PEER_ENABLED:
			{
				len = len + sprintf(prBuf+len, "gateway enabled");

				/* gateway enabled now we can send termination service change  for all terminations */
				mgco_init_ins_service_change( usta->t.usta.alarmInfo.sapId );
				break;
			}
		case LMG_EVENT_PEER_DISCOVERED:
			{
				len = len + sprintf(prBuf+len, "gateway discovered , notified entity");
				break;
			}
		case LMG_EVENT_PEER_REMOVED:
			{
				len = len + sprintf(prBuf+len, "gateway removed");
				break;
			}
		case LMG_EVENT_RES_CONG_ON:
			{
				len = len + sprintf(prBuf+len, "resource congestion ON");
				break;
			}
		case LMG_EVENT_RES_CONG_OFF:
			{
				len = len + sprintf(prBuf+len, "resource congestion OFF");
				break;
			}
		case LMG_EVENT_TPTSRV:
			{
				len = len + sprintf(prBuf+len, "transport service");
				break;
			}
		case LMG_EVENT_SSAP_ENABLED:
			{
				len = len + sprintf(prBuf+len, "SSAP enabled");
				break;
			}
		case LMG_EVENT_NS_NOT_RESPONDING:
			{
				len = len + sprintf(prBuf+len, "name server not responding");
				break;
			}
		case LMG_EVENT_TPT_FAILED:
			{
				len = len + sprintf(prBuf+len, "transport failure");
				break;
			}
	}

	len = len + sprintf(prBuf+len, " ) ");

	len = len + sprintf(prBuf+len, " cause ( ");
	switch (usta->t.usta.alarm.cause)
	{
		case LCM_CAUSE_UNKNOWN:
			{
				len = len + sprintf(prBuf+len, "unknown");
				break;
			}
		case LCM_CAUSE_OUT_OF_RANGE:
			{
				len = len + sprintf(prBuf+len, "out of range");
				break;
			}
		case LCM_CAUSE_INV_SAP:
			{
				len = len + sprintf(prBuf+len, "NULL/unknown sap");
				break;
			}
		case LCM_CAUSE_INV_SPID:
			{
				len = len + sprintf(prBuf+len, "invalid service provider");
				break;
			}
		case LCM_CAUSE_INV_SUID:
			{
				len = len + sprintf(prBuf+len, "invalid service user");
				break;
			}
		case LCM_CAUSE_INV_NETWORK_MSG:
			{
				len = len + sprintf(prBuf+len, "invalid network message");
				break;
			}
		case LCM_CAUSE_DECODE_ERR:
			{
				len = len + sprintf(prBuf+len, "message decoding problem");
				break;
			}
		case LCM_CAUSE_USER_INITIATED:
			{
				len = len + sprintf(prBuf+len, "user initiated");
				break;
			}
		case LCM_CAUSE_MGMT_INITIATED:
			{
				len = len + sprintf(prBuf+len, "management initiated");
				break;
			}
		case LCM_CAUSE_INV_STATE: /* cause and event! */
			{
				len = len + sprintf(prBuf+len, "invalid state");
				break;
			}
		case LCM_CAUSE_TMR_EXPIRED: /* cause and event! */
			{
				len = len + sprintf(prBuf+len, "timer expired");
				break;
			}
		case LCM_CAUSE_INV_MSG_LENGTH:
			{
				len = len + sprintf(prBuf+len, "invalid message length");
				break;
			}
		case LCM_CAUSE_PROT_NOT_ACTIVE:
			{
				len = len + sprintf(prBuf+len, "protocol layer not active");
				break;
			}
		case LCM_CAUSE_INV_PAR_VAL:
			{
				len = len + sprintf(prBuf+len, "invalid parameter value");
				break;
			}
		case LCM_CAUSE_NEG_CFM:
			{
				len = len + sprintf(prBuf+len, "negative confirmation");
				break;
			}
		case LCM_CAUSE_MEM_ALLOC_FAIL:
			{
				len = len + sprintf(prBuf+len, "memory allocation failure");
				break;
			}
		case LCM_CAUSE_HASH_FAIL:
			{
				len = len + sprintf(prBuf+len, "hashing failure");
				break;
			}
		case LCM_CAUSE_LYR_SPECIFIC:
			{
				len = len + sprintf(prBuf+len, "MGCP specific");
				break;
			}
		default:
			{
				len = len + sprintf(prBuf+len, "unknown %d", (int)(usta->t.usta.alarm.cause));
				break;
			}
		case LMG_CAUSE_TPT_FAILURE: /* make up your mind - cause or event? */
			{
				len = len + sprintf(prBuf+len, "transport failure");
				break;
			}
		case LMG_CAUSE_NS_NOT_RESPONDING:
			{
				len = len + sprintf(prBuf+len, "name server not responding");
				break;
			}
	}
	len = len + sprintf(prBuf+len, "  ) ");

	len = len + sprintf(prBuf+len, "  Alarm parameters ( ");
	ret = smmgGetAlarmInfoField(&usta->t.usta);
	switch (ret)
	{
		case SMMG_UNKNOWNFIELD:
			{
				len = len + sprintf(prBuf+len, "invalid ");

				break;
			}
		case SMMG_PEERINFO:
			{
				/* 
				 * Invoke the new function for printing the MgPeerInfo &
				 * delete all print code here 
				 */
				smmgPrntPeerInfo(&(usta->t.usta.alarmInfo.u.peerInfo));
				break;
			}
		case SMMG_SAPID:
			{
				len = len + sprintf(prBuf+len, "SAP ID %d\n", (int)(usta->t.usta.alarmInfo.u.sapId));
				break;
			}
		case SMMG_MEM:
			{
				len = len + sprintf(prBuf+len, "memory region %d pool %d\n",
						(int)(usta->t.usta.alarmInfo.u.mem.region),
						(int)(usta->t.usta.alarmInfo.u.mem.pool));

				break;
			}
		case SMMG_SRVSTA:
			{
				smmgPrntSrvSta(&usta->t.usta.alarmInfo.u.srvSta);
				break;
			}
		case SMMG_PEERSTA:
			{
				smmgPrntPeerSta(&usta->t.usta.alarmInfo.u.peerSta);
				break;
			}
		case SMMG_SSAPSTA:
			{
				smmgPrntSsapSta(&usta->t.usta.alarmInfo.u.ssapSta);
				break;
			}
		case SMMG_PARID:
			{
				len = len + sprintf(prBuf+len,  "parameter type: ");
				switch (usta->t.usta.alarmInfo.u.parId.parType)
				{
					case LMG_PAR_TPTADDR: len = len + sprintf(prBuf+len, "transport address"); break;
					case LMG_PAR_MBUF:    len = len + sprintf(prBuf+len, "message buffer"); break;
					case LMG_PAR_CHOICE:  len = len + sprintf(prBuf+len, "choice"); break;
					case LMG_PAR_SPID:    len = len + sprintf(prBuf+len, "spId"); break;
					default:              len = len + sprintf(prBuf+len, "unknown"); break;
				}

				len = len + sprintf(prBuf+len, ", value %d\n", 
						(int)(usta->t.usta.alarmInfo.u.parId.u.sapId));

				break;
			}
		case SMMG_NOT_APPL:
			{
				len = len + sprintf(prBuf+len, "not applicable\n");
				break;
			}

			/*TODO*/
	}
	len = len + sprintf(prBuf+len, "  ) ");
	len = len + sprintf(prBuf+len, "  \n ");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s \n", prBuf);
}

/*****************************************************************************************************************************/
void handle_tucl_alarm(Pst *pst, HiMngmt *sta)
{
	/* To print the general information */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Recieved a status indication from TUCL layer \n\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " Category = %d , event = %d , cause = %d\n", 
			sta->t.usta.alarm.category, 
			sta->t.usta.alarm.event, sta->t.usta.alarm.cause);

	switch(sta->t.usta.alarm.event)
	{
		case LCM_EVENT_INV_EVT: 
			{ 
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," [HI_USTA]: LCM_EVENT_INV_EVT with type (%d)\n\n",
						sta->t.usta.info.type);
				break;
			}
		case LHI_EVENT_BNDREQ:
			{ 
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," [HI_USTA]: LHI_EVENT_BNDREQ with type (%d) spId (%d)\n\n",
						sta->t.usta.info.type, sta->t.usta.info.spId);
				break;
			}
		case LHI_EVENT_SERVOPENREQ:
		case LHI_EVENT_DATREQ:
		case LHI_EVENT_UDATREQ:
		case LHI_EVENT_CONREQ:
		case LHI_EVENT_DISCREQ:
#if(defined(HI_TLS) && defined(HI_TCP_TLS)) 
		case LHI_EVENT_TLS_ESTREQ:
#endif
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO," [HI_USTA]: partype (%d) type(%d)\n\n",
						sta->t.usta.info.inf.parType, sta->t.usta.info.type);
				break;
			}
		case LCM_EVENT_DMEM_ALLOC_FAIL:
		case LCM_EVENT_SMEM_ALLOC_FAIL:
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " [HI_USTA]: MEM_ALLOC_FAIL with region(%d) pool (%d) type(%d)\n\n",
						sta->t.usta.info.inf.mem.region, sta->t.usta.info.inf.mem.pool,
						sta->t.usta.info.type);
				break;
			}
		default:
			break;
	}

}   /* handle_sng_tucl_alarm */
/******************************************************************************/

