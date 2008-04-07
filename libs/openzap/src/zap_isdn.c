/*
 * Copyright (c) 2007, Anthony Minessale II
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

#include "openzap.h"
#include "zap_isdn.h"
#include "Q931.h"
#include "Q921.h"
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define LINE "--------------------------------------------------------------------------------"
//#define IODEBUG

static L2ULONG zap_time_now(void)
{
	return (L2ULONG)zap_current_time_in_ms();
}

static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(isdn_outgoing_call)
{
	zap_status_t status = ZAP_SUCCESS;
	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DIALING);
	return status;
}

static L3INT zap_isdn_931_err(void *pvt, L3INT id, L3INT p1, L3INT p2)
{
	zap_log(ZAP_LOG_ERROR, "ERROR: [%s] [%d] [%d]\n", q931_error_to_name(id), p1, p2);
	return 0;
}

static L3INT zap_isdn_931_34(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	zap_span_t *span = (zap_span_t *) pvt;
	zap_isdn_data_t *isdn_data = span->signal_data;
	Q931mes_Generic *gen = (Q931mes_Generic *) msg;
	int chan_id = 0;
	zap_channel_t *zchan = NULL;
	
	if (Q931IsIEPresent(gen->ChanID)) {
		Q931ie_ChanID *chanid = Q931GetIEPtr(gen->ChanID, gen->buf);
		chan_id = chanid->ChanSlot;
	}

	assert(span != NULL);
	assert(isdn_data != NULL);
	
#if 0
	if (chan_id) {
		zchan = &span->channels[chan_id];
	}
#endif

	if (gen->CRVFlag) {
		zchan = isdn_data->channels_local_crv[gen->CRV];
	} else {
		zchan = isdn_data->channels_remote_crv[gen->CRV];
	}


	zap_log(ZAP_LOG_DEBUG, "Yay I got an event! Type:[%02x] Size:[%d]\n", gen->MesType, gen->Size);

	if (gen->ProtDisc == 3) {
		switch(gen->MesType) {
		case Q931mes_SERVICE:
			{
				Q931ie_ChangeStatus *changestatus = Q931GetIEPtr(gen->ChangeStatus, gen->buf);
				if (zchan) {
					switch (changestatus->NewStatus) {
					case 0: /* change status to "in service" */
						{
							zap_clear_flag_locked(zchan, ZAP_CHANNEL_SUSPENDED);
							zap_log(ZAP_LOG_DEBUG, "Channel %d:%d in service\n", zchan->span_id, zchan->chan_id);
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
						}
						break;
					case 1: 
						{ /* change status to "maintenance" */
							zap_set_flag_locked(zchan, ZAP_CHANNEL_SUSPENDED);
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_SUSPENDED);
						}
						break;
					case 2:
						{ /* change status to "out of service" */
							zap_set_flag_locked(zchan, ZAP_CHANNEL_SUSPENDED);
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_SUSPENDED);
						}
						break;
					default: /* unknown */
						{
							break;
						}
					}
				}
			}
			break;
		default:
			break;
		}
	} else {
		switch(gen->MesType) {
		case Q931mes_RESTART:
			{
				if (chan_id) {
					zchan = &span->channels[chan_id];
				}
				if (zchan) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
				} else {
					uint32_t i;
					for (i = 0; i < span->chan_count; i++) {
						zap_set_state_locked((&span->channels[i]), ZAP_CHANNEL_STATE_RESTART);
					}
				}
			}
			break;
		case Q931mes_RELEASE:
		case Q931mes_RELEASE_COMPLETE:
			{
				if (zchan) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
				} else {
					zap_log(ZAP_LOG_CRIT, "Received Release Complete with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_DISCONNECT:
			{
				if (zchan) {
					Q931ie_Cause *cause = Q931GetIEPtr(gen->Cause, gen->buf);
					zchan->caller_data.hangup_cause = cause->Value;
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_TERMINATING);
				} else {
					zap_log(ZAP_LOG_CRIT, "Received Disconnect with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_ALERTING:
			{
				if (zchan) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS_MEDIA);
				} else {
					zap_log(ZAP_LOG_CRIT, "Received Alerting with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_PROGRESS:
			{
				if (zchan) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS);
				} else {
					zap_log(ZAP_LOG_CRIT, "Received Progress with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_CONNECT:
			{
				if (zchan) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
				} else {
					zap_log(ZAP_LOG_CRIT, "Received Connect with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_SETUP:
			{
				
				Q931ie_CallingNum *callingnum = Q931GetIEPtr(gen->CallingNum, gen->buf);
				Q931ie_CalledNum *callednum = Q931GetIEPtr(gen->CalledNum, gen->buf);
				zap_status_t status;
				int fail = 1;
				uint32_t cplen = mlen;
				
				zchan = NULL;
				if (chan_id < ZAP_MAX_CHANNELS_SPAN && chan_id <= span->chan_count) {
					zchan = &span->channels[chan_id];
				}

				if (zchan && (status = zap_channel_open_chan(zchan) == ZAP_SUCCESS)) {
					if (zap_test_flag(zchan, ZAP_CHANNEL_INUSE) || zchan->state != ZAP_CHANNEL_STATE_DOWN) {
						if (zchan->state == ZAP_CHANNEL_STATE_DOWN || zchan->state >= ZAP_CHANNEL_STATE_TERMINATING) {
							int x = 0;
							zap_log(ZAP_LOG_WARNING, "Channel %d:%d ~ %d:%d is already in use waiting for it to become available.\n");

							for (x = 0; x < 200; x++) {
								if (!zap_test_flag(zchan, ZAP_CHANNEL_INUSE)) {
									break;
								}
								zap_sleep(5);
							}
						}
						if (zap_test_flag(zchan, ZAP_CHANNEL_INUSE)) {
							zap_log(ZAP_LOG_ERROR, "Channel %d:%d ~ %d:%d is already in use.\n",
									zchan->span_id,
									zchan->chan_id,
									zchan->physical_span_id,
									zchan->physical_chan_id
									);
							zchan = NULL;
						}
					}

					if (zchan && zchan->state == ZAP_CHANNEL_STATE_DOWN) {
						isdn_data->channels_remote_crv[gen->CRV] = zchan;
						memset(&zchan->caller_data, 0, sizeof(zchan->caller_data));

						zap_set_string(zchan->caller_data.cid_num.digits, (char *)callingnum->Digit);
						zap_set_string(zchan->caller_data.cid_name, (char *)callingnum->Digit);
						zap_set_string(zchan->caller_data.ani.digits, (char *)callingnum->Digit);
						zap_set_string(zchan->caller_data.dnis.digits, (char *)callednum->Digit);

						zchan->caller_data.CRV = gen->CRV;
						if (cplen > sizeof(zchan->caller_data.raw_data)) {
							cplen = sizeof(zchan->caller_data.raw_data);
						}
						gen->CRVFlag = !(gen->CRVFlag);
						memcpy(zchan->caller_data.raw_data, msg, cplen);
						zchan->caller_data.raw_data_len = cplen;
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
						fail = 0;
					} 
				} 

				if (fail) {
					Q931ie_Cause cause;
					gen->MesType = Q931mes_DISCONNECT;
					cause.IEId = Q931ie_CAUSE;
					cause.Size = sizeof(Q931ie_Cause);
					cause.CodStand  = 0;
					cause.Location = 1;
					cause.Recom = 1;
					//should we be casting here.. or do we need to translate value?
					cause.Value = (unsigned char) ZAP_CAUSE_WRONG_CALL_STATE;
					*cause.Diag = '\0';
					gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
					Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);

					if (gen->CRV) {
						Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
					}

					if (zchan) {
						zap_log(ZAP_LOG_CRIT, "Channel is busy\n");
					} else {
						zap_log(ZAP_LOG_CRIT, "Failed to open channel for new setup message\n");
					}
					
				}
				
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

static int zap_isdn_921_23(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	int ret;
	char bb[4096] = "";
	print_hex_bytes(msg+4, mlen-2, bb, sizeof(bb));
	zap_log(ZAP_LOG_DEBUG, "READ %d\n%s\n%s\n\n", (int)mlen-2, LINE, bb);

	ret = Q931Rx23(pvt, msg, mlen);
	if (ret != 0)
		zap_log(ZAP_LOG_DEBUG, "931 parse error [%d] [%s]\n", ret, q931_error_to_name(ret));
	return ((ret >= 0) ? 1 : 0);
}

static int zap_isdn_921_21(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	zap_span_t *span = (zap_span_t *) pvt;
	zap_size_t len = (zap_size_t) mlen;
	zap_isdn_data_t *isdn_data = span->signal_data;

#ifdef IODEBUG
	char bb[4096] = "";
	print_hex_bytes(msg, len, bb, sizeof(bb));
	print_bits(msg, (int)len, bb, sizeof(bb), ZAP_ENDIAN_LITTLE, 0);
	zap_log(ZAP_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)len, LINE, bb);

#endif

	assert(span != NULL);
	return zap_channel_write(isdn_data->dchan, msg, len, &len) == ZAP_SUCCESS ? 0 : -1;
}

static __inline__ void state_advance(zap_channel_t *zchan)
{
	Q931mes_Generic *gen = (Q931mes_Generic *) zchan->caller_data.raw_data;
	zap_isdn_data_t *isdn_data = zchan->span->signal_data;
	zap_sigmsg_t sig;
	zap_status_t status;

	zap_log(ZAP_LOG_ERROR, "%d:%d STATE [%s]\n", 
			zchan->span_id, zchan->chan_id, zap_channel_state2str(zchan->state));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = zchan->chan_id;
	sig.span_id = zchan->span_id;
	sig.channel = zchan;

	switch (zchan->state) {
	case ZAP_CHANNEL_STATE_DOWN:
		{
			if (gen->CRV) {
				Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
			}
			zap_channel_done(zchan);
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS;
				if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				gen->MesType = Q931mes_PROGRESS;
				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RING:
		{
			if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_START;
				if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			}

		}
		break;
	case ZAP_CHANNEL_STATE_RESTART:
		{
			if (zchan->last_state != ZAP_CHANNEL_STATE_HANGUP && zchan->last_state != ZAP_CHANNEL_STATE_DOWN) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
			} else {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS_MEDIA;
				if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				gen->MesType = Q931mes_ALERTING;
				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_UP;
				if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				gen->MesType = Q931mes_CONNECT;
				gen->BearerCap = 0;
				Q931Rx43(&isdn_data->q931, (void *)gen, zchan->caller_data.raw_data_len);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_DIALING:
		{
			Q931ie_BearerCap BearerCap;
			Q931ie_ChanID ChanID;
			Q931ie_CallingNum CallingNum;
			Q931ie_CallingNum *ptrCallingNum;
			Q931ie_CalledNum CalledNum;
			Q931ie_CalledNum *ptrCalledNum;
			Q931ie_Display Display, *ptrDisplay;
			
			Q931InitIEBearerCap(&BearerCap);
			Q931InitIEChanID(&ChanID);
			Q931InitIECallingNum(&CallingNum);
			Q931InitIECalledNum(&CalledNum);
			Q931InitIEDisplay(&Display);

			Q931InitMesGeneric(gen);
			gen->MesType = Q931mes_SETUP;

			BearerCap.CodStand = 0; /* ITU-T = 0, ISO/IEC = 1, National = 2, Network = 3 */
			BearerCap.ITC = 0; /* Speech */
			BearerCap.TransMode = 0; /* Circuit = 0, Packet = 1 */
			BearerCap.ITR = 16; /* 64k */
			BearerCap.Layer1Ident = 1;
			BearerCap.UIL1Prot = 2; /* U-law (a-law = 3)*/
#if 0
			BearerCap.SyncAsync = ;
			BearerCap.Negot = ;
			BearerCap.UserRate = ;
			BearerCap.InterRate = ;
			BearerCap.NIConTx = ;
			BearerCap.FlowCtlTx = ;
			BearerCap.HDR = ;
			BearerCap.MultiFrame = ;
			BearerCap.Mode = ;
			BearerCap.LLInegot = ;
			BearerCap.Assignor = ;
			BearerCap.InBandNeg = ;
			BearerCap.NumStopBits = ;
			BearerCap.NumDataBits = ;
			BearerCap.Parity = ;
			BearerCap.DuplexMode = ;
			BearerCap.ModemType = ;
			BearerCap.Layer2Ident = ;
			BearerCap.UIL2Prot = ;
			BearerCap.Layer3Ident = ;
			BearerCap.UIL3Prot = ;
			BearerCap.AL3Info1 = ;
			BearerCap.AL3Info2 = ;
#endif

			gen->BearerCap = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &BearerCap);

			//is cast right here?
			ChanID.IntType = 1; /* PRI = 1, BRI = 0 */
			ChanID.InfoChanSel = 1;
			ChanID.ChanMapType = 3; /* B-Chan */
			ChanID.ChanSlot = (unsigned char)zchan->chan_id;
			gen->ChanID = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &ChanID);
			
			Display.Size = Display.Size + (unsigned char)strlen(zchan->caller_data.cid_name);
			gen->Display = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &Display);			
			ptrDisplay = Q931GetIEPtr(gen->Display, gen->buf);
			zap_copy_string((char *)ptrDisplay->Display, zchan->caller_data.cid_name, strlen(zchan->caller_data.cid_name)+1);

			/* TypNum: Type of number		*/
			/* Bits 7 6 5					*/
			/* 000	Unknown					*/
			/* 001	International number	*/
			/* 010	National number			*/
			/* 011	Network Specific number */
			/* 100	Subscriber mumber		*/
			/* 110	Abbreviated number		*/
			/* 111	Reserved for extension	*/
			/* All other values are reserved */
			CallingNum.TypNum = 2;

			/* NumPlanID									*/
			/* Bits 4 3 2 1									*/
			/* 0000	Unknown									*/
			/* 0001	ISDN/telephony numbering plan (E.164)	*/
			/* 0011	Data numbering plan (X.121)				*/
			/* 0100	Telex numbering plan (F.69)				*/
			/* 1000	National standard numbering plan		*/
			/* 1001	Private numbering plan					*/
			/* 1111	Reserved for extension					*/
			/* All other valures are reserved				*/
			CallingNum.NumPlanID = 1;

			/* Presentation indicator		*/
			/* Bits 7 6						*/
			/* 00	Presenation Allowed		*/
			/* 01	Presentation Restricted	*/
			/* 10	Number not available due to internetworking	*/
			/* 11	Reserved				*/
			CallingNum.PresInd = 0;

			/* Screening Indicator						*/
			/* Bits 2 1									*/
			/* 00	User-provided, not screened			*/
			/* 01	User-provided, verified and passed	*/
			/* 10	User-provided, verified and failed	*/
			/* 11	Network provided					*/
			CallingNum.ScreenInd = 0;
			CallingNum.Size = CallingNum.Size + (unsigned char)strlen(zchan->caller_data.cid_num.digits);
			gen->CallingNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CallingNum);			
			ptrCallingNum = Q931GetIEPtr(gen->CallingNum, gen->buf);
			zap_copy_string((char *)ptrCallingNum->Digit, zchan->caller_data.cid_num.digits, strlen(zchan->caller_data.cid_num.digits)+1);

			CalledNum.TypNum = 2;
			CalledNum.NumPlanID = 1;
			CalledNum.Size = CalledNum.Size + (unsigned char)strlen(zchan->caller_data.ani.digits);
			gen->CalledNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CalledNum);
			ptrCalledNum = Q931GetIEPtr(gen->CalledNum, gen->buf);
			zap_copy_string((char *)ptrCalledNum->Digit, zchan->caller_data.ani.digits, strlen(zchan->caller_data.ani.digits)+1);

			Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
			isdn_data->channels_local_crv[gen->CRV] = zchan;
		}
		break;
	case ZAP_CHANNEL_STATE_HANGUP:
		{
			Q931ie_Cause cause;
			gen->MesType = Q931mes_DISCONNECT;
			cause.IEId = Q931ie_CAUSE;
			cause.Size = sizeof(Q931ie_Cause);
			cause.CodStand  = 0;
			cause.Location = 1;
			cause.Recom = 1;
			//should we be casting here.. or do we need to translate value?
			cause.Value = (unsigned char) zchan->caller_data.hangup_cause;
			*cause.Diag = '\0';
			gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
			Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
		}
		break;
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = ZAP_SIGEVENT_STOP;
			status = isdn_data->sig_cb(&sig);
			gen->MesType = Q931mes_RELEASE;
			Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
		}
	default:
		break;
	}
}

static __inline__ void check_state(zap_span_t *span)
{
    if (zap_test_flag(span, ZAP_SPAN_STATE_CHANGE)) {
        uint32_t j;
        zap_clear_flag_locked(span, ZAP_SPAN_STATE_CHANGE);
        for(j = 1; j <= span->chan_count; j++) {
            if (zap_test_flag((&span->channels[j]), ZAP_CHANNEL_STATE_CHANGE)) {
                zap_clear_flag_locked((&span->channels[j]), ZAP_CHANNEL_STATE_CHANGE);
                state_advance(&span->channels[j]);
                zap_channel_complete_state(&span->channels[j]);
            }
        }
    }
}


static __inline__ zap_status_t process_event(zap_span_t *span, zap_event_t *event)
{
	zap_sigmsg_t sig;
	zap_isdn_data_t *isdn_data = span->signal_data;

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = event->channel->chan_id;
	sig.span_id = event->channel->span_id;
	sig.channel = event->channel;

	zap_log(ZAP_LOG_DEBUG, "EVENT [%s][%d:%d] STATE [%s]\n", 
			zap_oob_event2str(event->enum_id), event->channel->span_id, event->channel->chan_id, zap_channel_state2str(event->channel->state));

	switch(event->enum_id) {
	case ZAP_OOB_ALARM_TRAP:
		{
			sig.event_id = ZAP_OOB_ALARM_TRAP;
			if (event->channel->state != ZAP_CHANNEL_STATE_DOWN) {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_RESTART);
			}
			zap_set_flag(event->channel, ZAP_CHANNEL_SUSPENDED);
			zap_channel_get_alarms(event->channel);
			isdn_data->sig_cb(&sig);
			zap_log(ZAP_LOG_WARNING, "channel %d:%d (%d:%d) has alarms [%s]\n", 
					event->channel->span_id, event->channel->chan_id, 
					event->channel->physical_span_id, event->channel->physical_chan_id, 
					event->channel->last_error);
		}
		break;
	case ZAP_OOB_ALARM_CLEAR:
		{
			sig.event_id = ZAP_OOB_ALARM_CLEAR;
			zap_clear_flag(event->channel, ZAP_CHANNEL_SUSPENDED);
			zap_channel_get_alarms(event->channel);
			isdn_data->sig_cb(&sig);
		}
		break;
	}

	return ZAP_SUCCESS;
}


static __inline__ void check_events(zap_span_t *span)
{
	zap_status_t status;

	status = zap_span_poll_event(span, 5);

	switch(status) {
	case ZAP_SUCCESS:
		{
			zap_event_t *event;
			while (zap_span_next_event(span, &event) == ZAP_SUCCESS) {
				if (event->enum_id == ZAP_OOB_NOOP) {
					continue;
				}
				if (process_event(span, event) != ZAP_SUCCESS) {
					break;
				}
			}
		}
		break;
	case ZAP_FAIL:
		{
			zap_log(ZAP_LOG_DEBUG, "Event Failure! %d\n", zap_running());
		}
		break;
	default:
		break;
	}
}

static void *zap_isdn_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_isdn_data_t *isdn_data = span->signal_data;
	unsigned char buf[1024];
	zap_size_t len = sizeof(buf);
	int errs = 0;

#ifdef WIN32
    timeBeginPeriod(1);
#endif

	zap_log(ZAP_LOG_DEBUG, "ISDN thread starting.\n");

	Q921Start(&isdn_data->q921);

	while(zap_running() && zap_test_flag(isdn_data, ZAP_ISDN_RUNNING)) {
		zap_wait_flag_t flags = ZAP_READ;
		zap_status_t status = zap_channel_wait(isdn_data->dchan, &flags, 100);

		Q921TimerTick(&isdn_data->q921);
		check_state(span);
		check_events(span);

		switch(status) {
		case ZAP_FAIL:
			{
				zap_log(ZAP_LOG_ERROR, "D-Chan Read Error!\n");
				snprintf(span->last_error, sizeof(span->last_error), "D-Chan Read Error!");
				if (++errs == 10) {
					goto done;
				}
			}
			break;
		case ZAP_TIMEOUT:
			{
				/*zap_log(ZAP_LOG_DEBUG, "Timeout!\n");*/
				/*Q931TimeTick(isdn_data->q931, L3ULONG ms);*/
				errs = 0;
			}
			break;
		default:
			{
				errs = 0;
				if (flags & ZAP_READ) {
					len = sizeof(buf);
					if (zap_channel_read(isdn_data->dchan, buf, &len) == ZAP_SUCCESS) {
#ifdef IODEBUG
						char bb[4096] = "";
						print_hex_bytes(buf, len, bb, sizeof(bb));

						print_bits(buf, (int)len, bb, sizeof(bb), ZAP_ENDIAN_LITTLE, 0);
						zap_log(ZAP_LOG_DEBUG, "READ %d\n%s\n%s\n\n", (int)len, LINE, bb);
#endif
						
						Q921QueueHDLCFrame(&isdn_data->q921, buf, (int)len);
						Q921Rx12(&isdn_data->q921);
					}
				} else {
					zap_log(ZAP_LOG_DEBUG, "No Read FLAG!\n");
				}
			}
			break;
		}

	}
	
 done:

	zap_channel_close(&isdn_data->dchans[0]);
	zap_channel_close(&isdn_data->dchans[1]);
	zap_clear_flag(isdn_data, ZAP_ISDN_RUNNING);

#ifdef WIN32
    timeEndPeriod(1);
#endif

	zap_log(ZAP_LOG_DEBUG, "ISDN thread ended.\n");
	return NULL;
}

zap_status_t zap_isdn_init(void)
{
	Q931Initialize();

	Q921SetGetTimeCB(zap_time_now);
	
	return ZAP_SUCCESS;
}

zap_status_t zap_isdn_start(zap_span_t *span)
{
	zap_isdn_data_t *isdn_data = span->signal_data;
	zap_set_flag(isdn_data, ZAP_ISDN_RUNNING);
	return zap_thread_create_detached(zap_isdn_run, span);
}

static int q931_rx_32(void *pvt,L3UCHAR *msg, L3INT mlen)
{
	int ret;
	char bb[4096] = "";
	ret = Q921Rx32(pvt, msg, mlen);
	print_hex_bytes(msg, mlen, bb, sizeof(bb));
	zap_log(ZAP_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)mlen, LINE, bb);
	return ret;
}

zap_status_t zap_isdn_configure_span(zap_span_t *span, Q921NetUser_t mode, Q931Dialect_t dialect, zio_signal_cb_t sig_cb)
{
	uint32_t i, x = 0;
	zap_channel_t *dchans[2] = {0};
	zap_isdn_data_t *isdn_data;
	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling [%d].", span->signal_type);
		return ZAP_FAIL;
	}

	if (span->trunk_type >= ZAP_TRUNK_NONE) {
		snprintf(span->last_error, sizeof(span->last_error), "Unknown trunk type!");
		return ZAP_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i].type == ZAP_CHAN_TYPE_DQ921) {
			if (x > 1) {
				snprintf(span->last_error, sizeof(span->last_error), "Span has more than 2 D-Channels!");
				return ZAP_FAIL;
			} else {
				if (zap_channel_open(span->span_id, i, &dchans[x]) == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_DEBUG, "opening d-channel #%d %d:%d\n", x, dchans[x]->span_id, dchans[x]->chan_id);
					x++;
				}
			}
		}
	}

	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "Span has no D-Channels!");
		return ZAP_FAIL;
	}

	
	isdn_data = malloc(sizeof(*isdn_data));
	assert(isdn_data != NULL);
	memset(isdn_data, 0, sizeof(*isdn_data));
	
	isdn_data->sig_cb = sig_cb;
	isdn_data->dchans[0] = dchans[0];
	isdn_data->dchans[1] = dchans[1];
	isdn_data->dchan = isdn_data->dchans[0];
	
	Q921_InitTrunk(&isdn_data->q921,
				   0,
				   0,
				   mode,
				   0,
				   zap_isdn_921_21,
				   (Q921TxCB_t)zap_isdn_921_23,
				   span,
				   &isdn_data->q931);
	
	Q931Api_InitTrunk(&isdn_data->q931,
					  dialect,
					  mode,
					  span->trunk_type,
					  zap_isdn_931_34,
					  (Q931TxCB_t)q931_rx_32,
					  zap_isdn_931_err,
					  &isdn_data->q921,
					  span);

	isdn_data->q931.autoRestartAck = 1;
	isdn_data->q931.autoConnectAck = 1;
	isdn_data->q931.autoServiceAck = 1;
	span->signal_data = isdn_data;
	span->signal_type = ZAP_SIGTYPE_ISDN;
	span->outgoing_call = isdn_outgoing_call;
	
	return ZAP_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
