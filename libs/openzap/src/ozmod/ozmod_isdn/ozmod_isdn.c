/*
 * Copyright (c) 2007-2012, Anthony Minessale II
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

/**
 * Workaround for missing u_int / u_short types on solaris
 */
#if defined(HAVE_LIBPCAP) && defined(__SunOS)
#define __EXTENSIONS__
#endif

#include "openzap.h"
#include "Q931.h"
#include "Q921.h"
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "zap_isdn.h"

#define LINE "--------------------------------------------------------------------------------"
//#define IODEBUG

/* helper macros */
#define ZAP_SPAN_IS_BRI(x)	((x)->trunk_type == ZAP_TRUNK_BRI || (x)->trunk_type == ZAP_TRUNK_BRI_PTMP)
#define ZAP_SPAN_IS_NT(x)	(((zap_isdn_data_t *)(x)->signal_data)->mode == Q921_NT)


#ifdef HAVE_LIBPCAP
/*-------------------------------------------------------------------------*/
/*Q931ToPcap functions*/
#include <pcap.h>
#endif

#define SNAPLEN 1522
#define MAX_ETHER_PAYLOAD_SIZE 1500
#define MIN_ETHER_PAYLOAD_SIZE 42
#define SIZE_ETHERNET           18
#define VLANID_OFFSET           15
#define SIZE_IP                 20
#define SIZE_TCP                20
#define SIZE_TPKT               4
#define SIZE_ETHERNET_CRC       4
#define OVERHEAD                SIZE_ETHERNET+SIZE_IP+SIZE_TCP+SIZE_TPKT
#define MAX_Q931_SIZE           MAX_ETHER_PAYLOAD_SIZE-SIZE_IP-SIZE_TCP-SIZE_TPKT
#define TPKT_SIZE_OFFSET        SIZE_ETHERNET+SIZE_IP+SIZE_TCP+2
#define IP_SIZE_OFFSET          SIZE_ETHERNET+2
#define TCP_SEQ_OFFSET		SIZE_ETHERNET+SIZE_IP+4

#ifdef HAVE_LIBPCAP
/*Some globals*/
unsigned long           pcapfilesize = 0;
unsigned long		tcp_next_seq_no_send = 0;
unsigned long           tcp_next_seq_no_rec = 0;
pcap_dumper_t           *pcapfile    = NULL;
struct pcap_pkthdr      pcaphdr;
pcap_t                  *pcaphandle  = NULL;
char 			*pcapfn      = NULL;
int			do_q931ToPcap= 0;

/*Predefined Ethernet Frame with Q931-over-IP encapsulated - From remote TDM host to FreeSWITCH*/
L3UCHAR  recFrame[SNAPLEN]= {
                                /*IEEE 802.3 VLAN 802.1q Ethernet Frame Header*/
                                2,0,1,0xAA,0xAA,0xAA,2,0,1,0xBB,0xBB,0xBB,0x81,0,0xE0,0,0x08,0,
                                /*IPv4 Header (minimal size; no options)*/
                                0x45,0,0,44,0,0,0,0,64,6,0,0,2,2,2,2,1,1,1,1,
                                /*TCP-Header*/
                                0,0x66,0,0x66,0,0,0,0,0,0,0,0,0x50,0,0,1,0,0,0,0,
                                /*TPKT-Header RFC 1006*/
                                3,0,0,0
                            };

/*Predefined Ethernet Frame with Q931-over-IP encapsulated - Frome FreeSWITCH to remote TDM host*/
L3UCHAR  sendFrame[SNAPLEN]= {
                                /*IEEE 802.3 VLAN 802.1q Ethernet Frame Header*/
                                2,0,1,0xBB,0xBB,0xBB,2,0,1,0xAA,0xAA,0xAA,0x81,0,0xE0,0,0x08,0,
                                /*IPv4 Header (minimal size; no options)*/
                                0x45,0,0,44,0,0,0,0,64,6,0,0,1,1,1,1,2,2,2,2,
                                /*TCP-Header*/
                                0,0x66,0,0x66,0,0,0,0,0,0,0,0,0x50,0,0,1,0,0,0,0,
                                /*TPKT-Header RFC 1006*/
                                3,0,0,0
                             };

/**
 * \brief Opens a pcap file for capture
 * \return Success or failure
 */
static zap_status_t openPcapFile(void)
{
        if(!pcaphandle)
        {
                pcaphandle = pcap_open_dead(DLT_EN10MB, SNAPLEN);
                if (!pcaphandle)
                {
                        zap_log(ZAP_LOG_ERROR, "Can't open pcap session: (%s)\n", pcap_geterr(pcaphandle));
                        return ZAP_FAIL;
                }
        }

        if(!pcapfile){
                /* Open the dump file */
                if(!(pcapfile=pcap_dump_open(pcaphandle, pcapfn))){
                        zap_log(ZAP_LOG_ERROR, "Error opening output file (%s)\n", pcap_geterr(pcaphandle));
                        return ZAP_FAIL;
                }
        }
        else{
                zap_log(ZAP_LOG_WARNING, "Pcap file is already open!\n");
                return ZAP_FAIL;
        }

        zap_log(ZAP_LOG_DEBUG, "Pcap file '%s' successfully opened!\n", pcapfn);

        pcaphdr.ts.tv_sec  	= 0;
        pcaphdr.ts.tv_usec 	= 0;
	pcapfilesize       	= 24;	/*current pcap file header seems to be 24 bytes*/
	tcp_next_seq_no_send    = 0;
	tcp_next_seq_no_rec	= 0;

        return ZAP_SUCCESS;
}

/**
 * \brief Closes a pcap file
 * \return Success
 */
static zap_status_t closePcapFile(void)
{
	if (pcapfile) {
		pcap_dump_close(pcapfile);
		if (pcaphandle) pcap_close(pcaphandle);

		zap_log(ZAP_LOG_DEBUG, "Pcap file closed! File size is %lu bytes.\n", pcapfilesize);

		pcaphdr.ts.tv_sec 	= 0;
		pcaphdr.ts.tv_usec 	= 0;
		pcapfile		= NULL;
		pcaphandle 		= NULL;
		pcapfilesize		= 0;
		tcp_next_seq_no_send	= 0;
		tcp_next_seq_no_rec	= 0;
	}

	/*We have allways success with this? I think so*/
	return ZAP_SUCCESS;
}

/**
 * \brief Writes a Q931 packet to a pcap file
 * \return Success or failure
 */
static zap_status_t writeQ931PacketToPcap(L3UCHAR* q931buf, L3USHORT q931size, L3ULONG span_id, L3USHORT direction)
{
        L3UCHAR                 *frame		= NULL;
	struct timeval		ts;
	u_char			spanid		= (u_char)span_id;
	unsigned long 		*tcp_next_seq_no = NULL;

	spanid=span_id;
	
        /*The total length of the ethernet frame generated by this function has a min length of 66
        so we don't have to care about padding :)*/


        /*FS is sending the packet*/
        if(direction==0){
		frame=sendFrame;
		tcp_next_seq_no = &tcp_next_seq_no_send;
        } 
	/*FS is receiving the packet*/
	else{
		frame=recFrame;
		tcp_next_seq_no = &tcp_next_seq_no_rec;
	}

	/*Set spanid in VLAN-ID tag*/
        frame[VLANID_OFFSET]    = spanid;

        /*** Write sent packet ***/
        if(q931size > MAX_Q931_SIZE)
        {
                /*WARNING*/
		zap_log(ZAP_LOG_WARNING, "Q931 packet size is too big (%u)! Ignoring it!\n", q931size);
                return ZAP_FAIL;
        }

	/*Copy q931 buffer into frame*/
        memcpy(frame+OVERHEAD,q931buf,q931size);

	/*Store TCP sequence number in TCP header*/
	frame[TCP_SEQ_OFFSET]=(*tcp_next_seq_no>>24)&0xFF;
	frame[TCP_SEQ_OFFSET+1]=(*tcp_next_seq_no>>16)&0xFF;
	frame[TCP_SEQ_OFFSET+2]=(*tcp_next_seq_no>>8)&0xFF;
	frame[TCP_SEQ_OFFSET+3]=*tcp_next_seq_no & 0xFF;

        /*Store size of TPKT packet*/
        q931size+=4;
        frame[TPKT_SIZE_OFFSET]=(q931size>>8)&0xFF;
        frame[TPKT_SIZE_OFFSET+1]=q931size&0xFF;

	/*Calc next TCP sequence number*/
	*tcp_next_seq_no+=q931size;

        /*Store size of IP packet*/
        q931size+=SIZE_IP+SIZE_TCP;
        frame[IP_SIZE_OFFSET]=(q931size>>8)&0xFF;
        frame[IP_SIZE_OFFSET+1]=q931size&0xFF;

        pcaphdr.caplen = SIZE_ETHERNET+SIZE_ETHERNET_CRC+q931size;
        pcaphdr.len = pcaphdr.caplen;

        /* Set Timestamp */
        /* Get Time in ms. usecs would be better ...  */
        gettimeofday(&ts, NULL);
        /*Write it into packet header*/
        pcaphdr.ts.tv_sec = ts.tv_sec;
        pcaphdr.ts.tv_usec = ts.tv_usec;

	pcap_dump((u_char*)pcapfile, &pcaphdr, frame);
        pcap_dump_flush(pcapfile);

        /*Maintain pcap file size*/
        pcapfilesize+=pcaphdr.caplen;
        pcapfilesize+=sizeof(struct pcap_pkthdr);

	zap_log(ZAP_LOG_DEBUG, "Added %u bytes to pcap file. File size is now %lu, \n", q931size, pcapfilesize);

        return ZAP_SUCCESS;
}

#endif

/**
 * \brief Unloads pcap IO
 * \return Success or failure
 */
static ZIO_IO_UNLOAD_FUNCTION(close_pcap)
{
#ifdef HAVE_LIBPCAP
	return closePcapFile();
#else
	return ZAP_SUCCESS;
#endif
}

/*Q931ToPcap functions DONE*/
/*-------------------------------------------------------------------------*/

/**
 * \brief Gets current time
 * \return Current time (in ms)
 */
static L2ULONG zap_time_now(void)
{
	return (L2ULONG)zap_current_time_in_ms();
}

/**
 * \brief Initialises an ISDN channel (outgoing call)
 * \param zchan Channel to initiate call on
 * \return Success or failure
 */
static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(isdn_outgoing_call)
{
	zap_status_t status = ZAP_SUCCESS;
	zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DIALING);
	return status;
}

/**
 * \brief Requests an ISDN channel on a span (outgoing call)
 * \param span Span where to get a channel
 * \param chan_id Specific channel to get (0 for any)
 * \param direction Call direction (inbound, outbound)
 * \param caller_data Caller information
 * \param zchan Channel to initialise
 * \return Success or failure
 */
static ZIO_CHANNEL_REQUEST_FUNCTION(isdn_channel_request)
{
	Q931mes_Generic *gen = (Q931mes_Generic *) caller_data->raw_data;
	Q931ie_BearerCap BearerCap;
	Q931ie_ChanID ChanID = { 0 };
	Q931ie_CallingNum CallingNum;
	Q931ie_CallingNum *ptrCallingNum;
	Q931ie_CalledNum CalledNum;
	Q931ie_CalledNum *ptrCalledNum;
	Q931ie_Display Display, *ptrDisplay;
	Q931ie_HLComp HLComp;			/* High-Layer Compatibility IE */
	Q931ie_ProgInd Progress;		/* Progress Indicator IE */
	zap_status_t status = ZAP_FAIL;
	zap_isdn_data_t *isdn_data = span->signal_data;
	int sanity = 60000;
	int codec  = 0;

	/*
	 * get codec type
	 */
	zap_channel_command(span->channels[chan_id], ZAP_COMMAND_GET_NATIVE_CODEC, &codec);

	/*
	 * Q.931 Setup Message
	 */
	Q931InitMesGeneric(gen);
	gen->MesType = Q931mes_SETUP;
	gen->CRVFlag = 0;		/* outgoing call */

	/*
	 * Bearer Capability IE
	 */
	Q931InitIEBearerCap(&BearerCap);
	BearerCap.CodStand  = Q931_CODING_ITU;		/* ITU-T = 0, ISO/IEC = 1, National = 2, Network = 3 */
	BearerCap.ITC       = Q931_ITC_SPEECH;		/* Speech */
	BearerCap.TransMode = 0;			/* Circuit = 0, Packet = 1 */
	BearerCap.ITR       = Q931_ITR_64K;		/* 64k */
	BearerCap.Layer1Ident = 1;
	BearerCap.UIL1Prot = (codec == ZAP_CODEC_ALAW) ? Q931_UIL1P_G711A : Q931_UIL1P_G711U;	/* U-law = 2, A-law = 3 */
	gen->BearerCap = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &BearerCap);

	/*
	 * Channel ID IE
	 */
	Q931InitIEChanID(&ChanID);
	ChanID.IntType = ZAP_SPAN_IS_BRI(span) ? 0 : 1;		/* PRI = 1, BRI = 0 */

	if(!ZAP_SPAN_IS_NT(span)) {
		ChanID.PrefExcl = (isdn_data->opts & ZAP_ISDN_OPT_SUGGEST_CHANNEL) ? 0 : 1; /* 0 = preferred, 1 exclusive */
	} else {
		ChanID.PrefExcl = 1;	/* always exclusive in NT-mode */
	}

	if(ChanID.IntType) {
		ChanID.InfoChanSel = 1;				/* None = 0, See Slot = 1, Any = 3 */
		ChanID.ChanMapType = 3; 			/* B-Chan */
		ChanID.ChanSlot = (unsigned char)chan_id;
	} else {
		ChanID.InfoChanSel = (unsigned char)chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
	}
	gen->ChanID = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &ChanID);

	/*
	 * Progress IE
	 */
	Q931InitIEProgInd(&Progress);
	Progress.CodStand = Q931_CODING_ITU;	/* 0 = ITU */
	Progress.Location = 0;  /* 0 = User, 1 = Private Network */
	Progress.ProgDesc = 3;	/* 1 = Not end-to-end ISDN */
	gen->ProgInd = Q931AppendIE((L3UCHAR *)gen, (L3UCHAR *)&Progress);

	/*
	 * Display IE
	 */
	if (!(isdn_data->opts & ZAP_ISDN_OPT_OMIT_DISPLAY_IE)) {
		Q931InitIEDisplay(&Display);
		Display.Size = Display.Size + (unsigned char)strlen(caller_data->cid_name);
		gen->Display = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &Display);			
		ptrDisplay = Q931GetIEPtr(gen->Display, gen->buf);
		zap_copy_string((char *)ptrDisplay->Display, caller_data->cid_name, strlen(caller_data->cid_name)+1);
	}

	/*
	 * Calling Number IE
	 */
	Q931InitIECallingNum(&CallingNum);
	CallingNum.TypNum    = Q931_TON_UNKNOWN;
	CallingNum.NumPlanID = Q931_NUMPLAN_E164;
	CallingNum.PresInd   = Q931_PRES_ALLOWED;
	CallingNum.ScreenInd = Q931_SCREEN_USER_NOT_SCREENED;
	CallingNum.Size = CallingNum.Size + (unsigned char)strlen(caller_data->cid_num.digits);
	gen->CallingNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CallingNum);			
	ptrCallingNum = Q931GetIEPtr(gen->CallingNum, gen->buf);
	zap_copy_string((char *)ptrCallingNum->Digit, caller_data->cid_num.digits, strlen(caller_data->cid_num.digits)+1);


	/*
	 * Called number IE
	 */
	Q931InitIECalledNum(&CalledNum);
	CalledNum.TypNum    = Q931_TON_UNKNOWN;
	CalledNum.NumPlanID = Q931_NUMPLAN_E164;
	CalledNum.Size = CalledNum.Size + (unsigned char)strlen(caller_data->ani.digits);
	gen->CalledNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CalledNum);
	ptrCalledNum = Q931GetIEPtr(gen->CalledNum, gen->buf);
	zap_copy_string((char *)ptrCalledNum->Digit, caller_data->ani.digits, strlen(caller_data->ani.digits)+1);

	/*
	 * High-Layer Compatibility IE   (Note: Required for AVM FritzBox)
	 */
	Q931InitIEHLComp(&HLComp);
	HLComp.CodStand  = Q931_CODING_ITU;	/* ITU */
	HLComp.Interpret = 4;	/* only possible value */
	HLComp.PresMeth  = 1;   /* High-layer protocol profile */
	HLComp.HLCharID  = 1;	/* Telephony = 1, Fax G2+3 = 4, Fax G4 = 65 (Class I)/ 68 (Class II or III) */
	gen->HLComp = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &HLComp);

	caller_data->call_state = ZAP_CALLER_STATE_DIALING;
	Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
	
	isdn_data->outbound_crv[gen->CRV] = caller_data;
	//isdn_data->channels_local_crv[gen->CRV] = zchan;

	while(zap_running() && caller_data->call_state == ZAP_CALLER_STATE_DIALING) {
		zap_sleep(1);
		
		if (!--sanity) {
			caller_data->call_state = ZAP_CALLER_STATE_FAIL;
			break;
		}
	}
	isdn_data->outbound_crv[gen->CRV] = NULL;
	
	if (caller_data->call_state == ZAP_CALLER_STATE_SUCCESS) {
		zap_channel_t *new_chan = NULL;
		int fail = 1;
		
		new_chan = NULL;
		if (caller_data->chan_id < ZAP_MAX_CHANNELS_SPAN && caller_data->chan_id <= span->chan_count) {
			new_chan = span->channels[caller_data->chan_id];
		}

		if (new_chan && (status = zap_channel_open_chan(new_chan) == ZAP_SUCCESS)) {
			if (zap_test_flag(new_chan, ZAP_CHANNEL_INUSE) || new_chan->state != ZAP_CHANNEL_STATE_DOWN) {
				if (new_chan->state == ZAP_CHANNEL_STATE_DOWN || new_chan->state >= ZAP_CHANNEL_STATE_TERMINATING) {
					int x = 0;
					zap_log(ZAP_LOG_WARNING, "Channel %d:%d ~ %d:%d is already in use waiting for it to become available.\n");
					
					for (x = 0; x < 200; x++) {
						if (!zap_test_flag(new_chan, ZAP_CHANNEL_INUSE)) {
							break;
						}
						zap_sleep(5);
					}
				}
				if (zap_test_flag(new_chan, ZAP_CHANNEL_INUSE)) {
					zap_log(ZAP_LOG_ERROR, "Channel %d:%d ~ %d:%d is already in use.\n",
							new_chan->span_id,
							new_chan->chan_id,
							new_chan->physical_span_id,
							new_chan->physical_chan_id
							);
					new_chan = NULL;
				}
			}

			if (new_chan && new_chan->state == ZAP_CHANNEL_STATE_DOWN) {
				isdn_data->channels_local_crv[gen->CRV] = new_chan;
				memset(&new_chan->caller_data, 0, sizeof(new_chan->caller_data));
				zap_set_flag(new_chan, ZAP_CHANNEL_OUTBOUND);
				zap_set_state_locked(new_chan, ZAP_CHANNEL_STATE_DIALING);
				switch(gen->MesType) {
				case Q931mes_ALERTING:
					new_chan->init_state = ZAP_CHANNEL_STATE_PROGRESS_MEDIA;
					break;
				case Q931mes_CONNECT:
					new_chan->init_state = ZAP_CHANNEL_STATE_UP;
					break;
				default:
					new_chan->init_state = ZAP_CHANNEL_STATE_PROGRESS;
					break;
				}

				fail = 0;
			} 
		}
		
		if (!fail) {
			*zchan = new_chan;
			return ZAP_SUCCESS;
		} else {
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
			
			if (new_chan) {
				zap_log(ZAP_LOG_CRIT, "Channel is busy\n");
			} else {
				zap_log(ZAP_LOG_CRIT, "Failed to open channel for new setup message\n");
			}
		}
	}
	
	*zchan = NULL;
	return ZAP_FAIL;

}

/**
 * \brief Handler for Q931 error
 * \param pvt Private structure (span?)
 * \param id Error number
 * \param p1 ??
 * \param p2 ??
 * \return 0
 */
static L3INT zap_isdn_931_err(void *pvt, L3INT id, L3INT p1, L3INT p2)
{
	zap_log(ZAP_LOG_ERROR, "ERROR: [%s] [%d] [%d]\n", q931_error_to_name(id), p1, p2);
	return 0;
}

/**
 * \brief Handler for Q931 event message
 * \param pvt Span to handle
 * \param msg Message string
 * \param mlen Message string length
 * \return 0
 */
static L3INT zap_isdn_931_34(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	zap_span_t *span = (zap_span_t *) pvt;
	zap_isdn_data_t *isdn_data = span->signal_data;
	Q931mes_Generic *gen = (Q931mes_Generic *) msg;
	uint32_t chan_id = 0;
	int chan_hunt = 0;
	zap_channel_t *zchan = NULL;
	zap_caller_data_t *caller_data = NULL;

	if (Q931IsIEPresent(gen->ChanID)) {
		Q931ie_ChanID *chanid = Q931GetIEPtr(gen->ChanID, gen->buf);

		if(chanid->IntType)
			chan_id = chanid->ChanSlot;
		else
			chan_id = chanid->InfoChanSel;

		/* "any" channel specified */
		if(chanid->InfoChanSel == 3) {
			chan_hunt++;
		}
	} else if (ZAP_SPAN_IS_NT(span)) {
		/* no channel ie */
		chan_hunt++;
	}

	assert(span != NULL);
	assert(isdn_data != NULL);
	
	zap_log(ZAP_LOG_DEBUG, "Yay I got an event! Type:[%02x] Size:[%d] CRV: %d (%#hx, CTX: %s)\n", gen->MesType, gen->Size, gen->CRV, gen->CRV, gen->CRVFlag ? "Terminator" : "Originator");

	if (gen->CRVFlag && (caller_data = isdn_data->outbound_crv[gen->CRV])) {
		if (chan_id) {
			caller_data->chan_id = chan_id;
		}

		switch(gen->MesType) {
		case Q931mes_STATUS:
		case Q931mes_CALL_PROCEEDING:
			break;
		case Q931mes_ALERTING:
		case Q931mes_PROGRESS:
		case Q931mes_CONNECT:
			{
				caller_data->call_state = ZAP_CALLER_STATE_SUCCESS;
			}
			break;
		default:
			caller_data->call_state = ZAP_CALLER_STATE_FAIL;
			break;
		}
	
		return 0;
	}

	if (gen->CRVFlag) {
		zchan = isdn_data->channels_local_crv[gen->CRV];
	} else {
		zchan = isdn_data->channels_remote_crv[gen->CRV];
	}

	zap_log(ZAP_LOG_DEBUG, "zchan %x (%d:%d) source isdn_data->channels_%s_crv[%#hx]\n", zchan, zchan ? zchan->span_id : -1, zchan ? zchan->chan_id : -1, gen->CRVFlag ? "local" : "remote", gen->CRV);


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
					zchan = span->channels[chan_id];
				}
				if (zchan) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
				} else {
					uint32_t i;
					for (i = 1; i < span->chan_count; i++) {
						zap_set_state_locked((span->channels[i]), ZAP_CHANNEL_STATE_RESTART);
					}
				}
			}
			break;
		case Q931mes_RELEASE:
		case Q931mes_RELEASE_COMPLETE:
			{
				const char *what = gen->MesType == Q931mes_RELEASE ? "Release" : "Release Complete";
				if (zchan) {
					if (zchan->state == ZAP_CHANNEL_STATE_TERMINATING || zchan->state == ZAP_CHANNEL_STATE_HANGUP) {
						if (gen->MesType == Q931mes_RELEASE) {
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE);
						} else {
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
						}
					}
					else if((gen->MesType == Q931mes_RELEASE && zchan->state <= ZAP_CHANNEL_STATE_UP) ||
						(gen->MesType == Q931mes_RELEASE_COMPLETE && zchan->state == ZAP_CHANNEL_STATE_DIALING)) {

						/*
						 * Don't keep inbound channels open if the remote side hangs up before we answered
						 */
						Q931ie_Cause *cause = Q931GetIEPtr(gen->Cause, gen->buf);
						zap_sigmsg_t sig;
						zap_status_t status;

						memset(&sig, 0, sizeof(sig));
						sig.chan_id = zchan->chan_id;
						sig.span_id = zchan->span_id;
						sig.channel = zchan;
						sig.channel->caller_data.hangup_cause = (cause) ? cause->Value : ZAP_CAUSE_NORMAL_UNSPECIFIED;

						sig.event_id = ZAP_SIGEVENT_STOP;
						status = zap_span_send_signal(zchan->span, &sig);

						zap_log(ZAP_LOG_DEBUG, "Received %s in state %s, requested hangup for channel %d:%d\n", what, zap_channel_state2str(zchan->state), zchan->span_id, chan_id);
					}
					else {
						zap_log(ZAP_LOG_DEBUG, "Ignoring %s on channel %d\n", what, chan_id);
					}
				} else {
					zap_log(ZAP_LOG_CRIT, "Received %s with no matching channel %d\n", what, chan_id);
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

					gen->MesType = Q931mes_CONNECT_ACKNOWLEDGE;
					gen->CRVFlag = 0;	/* outbound */
					Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
				} else {
					zap_log(ZAP_LOG_CRIT, "Received Connect with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_SETUP:
			{
				Q931ie_CallingNum *callingnum = Q931GetIEPtr(gen->CallingNum, gen->buf);
				Q931ie_CalledNum *callednum = Q931GetIEPtr(gen->CalledNum, gen->buf);
				int fail = 1;
				int fail_cause = 0;
				int overlap_dial = 0;
				uint32_t cplen = mlen;

				if(zchan && zchan == isdn_data->channels_remote_crv[gen->CRV]) {
					zap_log(ZAP_LOG_INFO, "Duplicate SETUP message(?) for Channel %d:%d ~ %d:%d in state %s [ignoring]\n",
									zchan->span_id,
									zchan->chan_id,
									zchan->physical_span_id,
									zchan->physical_chan_id,
									zap_channel_state2str(zchan->state));
					break;
				}
				
				zchan = NULL;
				/*
				 * Channel selection for incoming calls:
				 */
				if (ZAP_SPAN_IS_NT(span) && chan_hunt) {
					uint32_t x;

					/*
					 * In NT-mode with channel selection "any",
					 * try to find a free channel
					 */
					for (x = 1; x <= span->chan_count; x++) {
						zap_channel_t *zc = span->channels[x];

						if (!zap_test_flag(zc, ZAP_CHANNEL_INUSE) && zc->state == ZAP_CHANNEL_STATE_DOWN) {
							zchan = zc;
							break;
						}
					}
				}
				else if (!ZAP_SPAN_IS_NT(span) && chan_hunt) {
					/*
					 * In TE-mode this ("any") is invalid
					 */
					fail_cause = ZAP_CAUSE_CHANNEL_UNACCEPTABLE;

					zap_log(ZAP_LOG_ERROR, "Invalid channel selection in incoming call (network side didn't specify a channel)\n");
				}
				else {
					/*
					 * Otherwise simply try to select the channel we've been told
					 *
					 * TODO: NT mode is abled to select a different channel if the one chosen
					 *       by the TE side is already in use
					 */
					if (chan_id > 0 && chan_id < ZAP_MAX_CHANNELS_SPAN && chan_id <= span->chan_count) {
						zchan = span->channels[chan_id];
					}
					else {
						/* invalid channel id */
						fail_cause = ZAP_CAUSE_CHANNEL_UNACCEPTABLE;

						zap_log(ZAP_LOG_ERROR, "Invalid channel selection in incoming call (none selected or out of bounds)\n");
					}
				}

				if (!callednum || !strlen((char *)callednum->Digit)) {
					if (ZAP_SPAN_IS_NT(span)) {
						zap_log(ZAP_LOG_NOTICE, "No destination number found, assuming overlap dial\n");
						overlap_dial++;
					}
					else {
						zap_log(ZAP_LOG_ERROR, "No destination number found\n");
						zchan = NULL;
					}
				}

				if (zchan) {
					if (zap_test_flag(zchan, ZAP_CHANNEL_INUSE) || zchan->state != ZAP_CHANNEL_STATE_DOWN) {
						if (zchan->state == ZAP_CHANNEL_STATE_DOWN || zchan->state >= ZAP_CHANNEL_STATE_TERMINATING) {
							int x = 0;
							zap_log(ZAP_LOG_WARNING, "Channel %d:%d ~ %d:%d is already in use waiting for it to become available.\n",
									zchan->span_id,
									zchan->chan_id,
									zchan->physical_span_id,
									zchan->physical_chan_id);

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

						if (zchan->mod_data) {
							memset(zchan->mod_data, 0, sizeof(zap_isdn_bchan_data_t));
						}

						zap_set_string(zchan->caller_data.cid_num.digits, (char *)callingnum->Digit);
						zap_set_string(zchan->caller_data.cid_name, (char *)callingnum->Digit);
						zap_set_string(zchan->caller_data.ani.digits, (char *)callingnum->Digit);
						if (!overlap_dial) {
							zap_set_string(zchan->caller_data.dnis.digits, (char *)callednum->Digit);
						}

						zchan->caller_data.CRV = gen->CRV;
						if (cplen > sizeof(zchan->caller_data.raw_data)) {
							cplen = sizeof(zchan->caller_data.raw_data);
						}
						gen->CRVFlag = !(gen->CRVFlag);
						memcpy(zchan->caller_data.raw_data, msg, cplen);
						zchan->caller_data.raw_data_len = cplen;
						fail = 0;
					} 
				} 

				if (fail) {
					Q931ie_Cause cause;
					gen->MesType = Q931mes_DISCONNECT;
					gen->CRVFlag = 1;	/* inbound call */
					cause.IEId = Q931ie_CAUSE;
					cause.Size = sizeof(Q931ie_Cause);
					cause.CodStand = Q931_CODING_ITU;
					cause.Location = 1;
					cause.Recom = 1;
					//should we be casting here.. or do we need to translate value?
					cause.Value = (unsigned char)((fail_cause) ? fail_cause : ZAP_CAUSE_WRONG_CALL_STATE);
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
					
				} else {
					Q931ie_ChanID ChanID;

					/*
					 * Update Channel ID IE
					 */
					Q931InitIEChanID(&ChanID);
					ChanID.IntType = ZAP_SPAN_IS_BRI(zchan->span) ? 0 : 1;	/* PRI = 1, BRI = 0 */
					ChanID.PrefExcl = ZAP_SPAN_IS_NT(zchan->span) ? 1 : 0;  /* Exclusive in NT-mode = 1, Preferred otherwise = 0 */
					if(ChanID.IntType) {
						ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
						ChanID.ChanMapType = 3;		/* B-Chan */
						ChanID.ChanSlot = (unsigned char)zchan->chan_id;
					} else {
						ChanID.InfoChanSel = (unsigned char)zchan->chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
					}
					gen->ChanID = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &ChanID);

					if (overlap_dial) {
						Q931ie_ProgInd progress;

						/*
						 * Setup Progress indicator
						 */
						progress.IEId = Q931ie_PROGRESS_INDICATOR;
						progress.Size = sizeof(Q931ie_ProgInd);
						progress.CodStand = Q931_CODING_ITU;	/* ITU */ 
						progress.Location = 1;	/* private network serving the local user */
						progress.ProgDesc = 8;	/* call is not end-to-end isdn = 1, in-band information available = 8 */
						gen->ProgInd = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &progress);

						/*
						 * Send SETUP ACK
						 */
						gen->MesType = Q931mes_SETUP_ACKNOWLEDGE;
						gen->CRVFlag = 1;	/* inbound call */
						Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);

						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DIALTONE);
					} else {
						/*
						 * Advance to RING state
						 */
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
					}
				}
			}
			break;

		case Q931mes_CALL_PROCEEDING:
			{
				if (zchan) {
					zap_log(ZAP_LOG_CRIT, "Received CALL PROCEEDING message for channel %d\n", chan_id);
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS);
				} else {
					zap_log(ZAP_LOG_CRIT, "Received CALL PROCEEDING with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_CONNECT_ACKNOWLEDGE:
			{
				if (zchan) {
					zap_log(ZAP_LOG_DEBUG, "Received CONNECT_ACK message for channel %d\n", chan_id);
				} else {
					zap_log(ZAP_LOG_DEBUG, "Received CONNECT_ACK with no matching channel %d\n", chan_id);
				}
			}
			break;

		case Q931mes_INFORMATION:
			{
				if (zchan) {
					zap_log(ZAP_LOG_CRIT, "Received INFORMATION message for channel %d\n", zchan->chan_id);

					if (zchan->state == ZAP_CHANNEL_STATE_DIALTONE) {
						char digit = '\0';

						/*
						 * overlap dial digit indication
						 */
						if (Q931IsIEPresent(gen->CalledNum)) {
							zap_isdn_bchan_data_t *data = (zap_isdn_bchan_data_t *)zchan->mod_data;
							Q931ie_CalledNum *callednum = Q931GetIEPtr(gen->CalledNum, gen->buf);
							int pos;

							digit = callednum->Digit[strlen((char *)callednum->Digit) - 1];
							if (digit == '#') {
								callednum->Digit[strlen((char *)callednum->Digit) - 1] = '\0';
							}

							/* TODO: make this more safe with strncat() */
							pos = (int)strlen(zchan->caller_data.dnis.digits);
							strcat(&zchan->caller_data.dnis.digits[pos],    (char *)callednum->Digit);

							/* update timer */
							data->digit_timeout = zap_time_now() + isdn_data->digit_timeout;

							zap_log(ZAP_LOG_DEBUG, "Received new overlap digit (%s), destination number: %s\n", callednum->Digit, zchan->caller_data.dnis.digits);
						}

						if (Q931IsIEPresent(gen->SendComplete) || digit == '#') {
							zap_log(ZAP_LOG_DEBUG, "Leaving overlap dial mode\n");

							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
						}
					}
				} else {
					zap_log(ZAP_LOG_CRIT, "Received INFORMATION message with no matching channel\n");
				}
			}
			break;

		case Q931mes_STATUS_ENQUIRY:
			{
				/*
				 * !! HACK ALERT !!
				 *
				 * Map OpenZAP channel states to Q.931 states
				 */
				Q931ie_CallState state;
				Q931ie_Cause cause;

				gen->MesType = Q931mes_STATUS;
				gen->CRVFlag = gen->CRVFlag ? 0 : 1;

				state.CodStand  = Q931_CODING_ITU;	/* ITU-T */
				state.CallState = Q931_U0;		/* Default: Null */

				cause.IEId = Q931ie_CAUSE;
				cause.Size = sizeof(Q931ie_Cause);
				cause.CodStand = Q931_CODING_ITU;	/* ITU */
				cause.Location = 1;	/* private network */
				cause.Recom    = 1;	/* */
				*cause.Diag    = '\0';

				if(zchan) {
					switch(zchan->state) {
					case ZAP_CHANNEL_STATE_UP:
						state.CallState = Q931_U10;	/* Active */
						break;
					case ZAP_CHANNEL_STATE_RING:
						state.CallState = Q931_U6;	/* Call present */
						break;
					case ZAP_CHANNEL_STATE_DIALING:
						state.CallState = Q931_U1;	/* Call initiated */
						break;
					case ZAP_CHANNEL_STATE_DIALTONE:
						state.CallState = Q931_U25;	/* Overlap receiving */
						break;

					/* TODO: map missing states */

					default:
						state.CallState = Q931_U0;
					}

					cause.Value = 30;	/* response to STATUS ENQUIRY */
				} else {
					cause.Value = 98;	/* */
				}

				gen->CallState = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &state);
				gen->Cause     = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
			}
			break;

		default:
			zap_log(ZAP_LOG_CRIT, "Received unhandled message %d (%#x)\n", (int)gen->MesType, (int)gen->MesType);
			break;
		}
	}

	return 0;
}

/**
 * \brief Handler for Q921 read event
 * \param pvt Span were message is coming from
 * \param ind Q921 indication
 * \param tei Terminal Endpoint Identifier
 * \param msg Message string
 * \param mlen Message string length
 * \return 0 on success, 1 on failure
 */
static int zap_isdn_921_23(void *pvt, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR *msg, L2INT mlen)
{
	int ret, offset = (ind == Q921_DL_DATA) ? 4 : 3;
	char bb[4096] = "";

	switch(ind) {
	case Q921_DL_DATA:
	case Q921_DL_UNIT_DATA:
		print_hex_bytes(msg + offset, mlen - offset, bb, sizeof(bb));
#ifdef HAVE_LIBPCAP
		/*Q931ToPcap*/
                if(do_q931ToPcap==1){
			zap_span_t *span = (zap_span_t *) pvt;
                        if(writeQ931PacketToPcap(msg + offset, mlen - offset, span->span_id, 1) != ZAP_SUCCESS){
                                zap_log(ZAP_LOG_WARNING, "Couldn't write Q931 buffer to pcap file!\n");
                        }
                }
                /*Q931ToPcap done*/
#endif
		zap_log(ZAP_LOG_DEBUG, "READ %d\n%s\n%s\n\n\n", (int)mlen - offset, LINE, bb);
	
	default:
		ret = Q931Rx23(pvt, ind, tei, msg, mlen);
		if (ret != 0)
			zap_log(ZAP_LOG_DEBUG, "931 parse error [%d] [%s]\n", ret, q931_error_to_name(ret));
		break;
	}

	return ((ret >= 0) ? 1 : 0);
}

/**
 * \brief Handler for Q921 write event
 * \param pvt Span were message is coming from
 * \param msg Message string
 * \param mlen Message string length
 * \return 0 on success, -1 on failure
 */
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

/**
 * \brief Handler for channel state change
 * \param zchan Channel to handle
 */
static __inline__ void state_advance(zap_channel_t *zchan)
{
	Q931mes_Generic *gen = (Q931mes_Generic *) zchan->caller_data.raw_data;
	zap_isdn_data_t *isdn_data = zchan->span->signal_data;
	zap_sigmsg_t sig;
	zap_status_t status;

	zap_log(ZAP_LOG_DEBUG, "%d:%d STATE [%s]\n", 
			zchan->span_id, zchan->chan_id, zap_channel_state2str(zchan->state));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = zchan->chan_id;
	sig.span_id = zchan->span_id;
	sig.channel = zchan;

	switch (zchan->state) {
	case ZAP_CHANNEL_STATE_DOWN:
		{
			if (gen->CRV) {
				if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
					isdn_data->channels_local_crv[gen->CRV] = NULL;
				} else {
					isdn_data->channels_remote_crv[gen->CRV] = NULL;
				}
				Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
			}
			zap_channel_done(zchan);
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				gen->MesType = Q931mes_CALL_PROCEEDING;
				gen->CRVFlag = 1;	/* inbound */

				if (ZAP_SPAN_IS_NT(zchan->span)) {
					Q931ie_ChanID ChanID;

					/*
					 * Set new Channel ID
					 */
					Q931InitIEChanID(&ChanID);
					ChanID.IntType = ZAP_SPAN_IS_BRI(zchan->span) ? 0 : 1;		/* PRI = 1, BRI = 0 */
					ChanID.PrefExcl = 1;	/* always exclusive in NT-mode */

					if(ChanID.IntType) {
						ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
						ChanID.ChanMapType = 3; 	/* B-Chan */
						ChanID.ChanSlot = (unsigned char)zchan->chan_id;
					} else {
						ChanID.InfoChanSel = (unsigned char)zchan->chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
					}
					gen->ChanID = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &ChanID);
				}

				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_DIALTONE:
		{
			zap_isdn_bchan_data_t *data = (zap_isdn_bchan_data_t *)zchan->mod_data;

			if (data) {
				data->digit_timeout = zap_time_now() + isdn_data->digit_timeout;
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RING:
		{
			if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_START;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RESTART:
		{
			zchan->caller_data.hangup_cause = ZAP_CAUSE_NORMAL_UNSPECIFIED;
			sig.event_id = ZAP_SIGEVENT_RESTART;
			status = zap_span_send_signal(zchan->span, &sig);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS_MEDIA;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
					if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
						return;
					}
				}
				gen->MesType = Q931mes_ALERTING;
				gen->CRVFlag = 1;	/* inbound call */
				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_UP;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
					if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
						return;
					}
				}
				gen->MesType = Q931mes_CONNECT;
				gen->BearerCap = 0;
				gen->CRVFlag = 1;	/* inbound call */
				Q931Rx43(&isdn_data->q931, (void *)gen, zchan->caller_data.raw_data_len);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_DIALING:
		if (!(isdn_data->opts & ZAP_ISDN_OPT_SUGGEST_CHANNEL)) {
			Q931ie_BearerCap BearerCap;
			Q931ie_ChanID ChanID;
			Q931ie_CallingNum CallingNum;
			Q931ie_CallingNum *ptrCallingNum;
			Q931ie_CalledNum CalledNum;
			Q931ie_CalledNum *ptrCalledNum;
			Q931ie_Display Display, *ptrDisplay;
			Q931ie_HLComp HLComp;			/* High-Layer Compatibility IE */
			Q931ie_ProgInd Progress;		/* Progress Indicator IE */
			int codec  = 0;

			/*
			 * get codec type
			 */
			zap_channel_command(zchan->span->channels[zchan->chan_id], ZAP_COMMAND_GET_NATIVE_CODEC, &codec);

			/*
			 * Q.931 Setup Message
			 */ 
			Q931InitMesGeneric(gen);
			gen->MesType = Q931mes_SETUP;
			gen->CRVFlag = 0;		/* outbound(?) */

			/*
			 * Bearer Capability IE
			 */
			Q931InitIEBearerCap(&BearerCap);
			BearerCap.CodStand  = Q931_CODING_ITU;	/* ITU-T = 0, ISO/IEC = 1, National = 2, Network = 3 */
			BearerCap.ITC       = Q931_ITC_SPEECH;	/* Speech */
			BearerCap.TransMode = 0;		/* Circuit = 0, Packet = 1 */
			BearerCap.ITR       = Q931_ITR_64K;	/* 64k = 16, Packet mode = 0 */
			BearerCap.Layer1Ident = 1;
			BearerCap.UIL1Prot = (codec == ZAP_CODEC_ALAW) ? 3 : 2;	/* U-law = 2, A-law = 3 */
			gen->BearerCap = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &BearerCap);

			/*
			 * ChannelID IE
			 */
			Q931InitIEChanID(&ChanID);
			ChanID.IntType = ZAP_SPAN_IS_BRI(zchan->span) ? 0 : 1;	/* PRI = 1, BRI = 0 */
			ChanID.PrefExcl = ZAP_SPAN_IS_NT(zchan->span) ? 1 : 0;  /* Exclusive in NT-mode = 1, Preferred otherwise = 0 */
			if(ChanID.IntType) {
				ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
				ChanID.ChanMapType = 3;		/* B-Chan */
				ChanID.ChanSlot = (unsigned char)zchan->chan_id;
			} else {
				ChanID.InfoChanSel = (unsigned char)zchan->chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
			}
			gen->ChanID = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &ChanID);

			/*
			 * Progress IE
			 */
			Q931InitIEProgInd(&Progress);
			Progress.CodStand = Q931_CODING_ITU;	/* 0 = ITU */
			Progress.Location = 0;  /* 0 = User, 1 = Private Network */
			Progress.ProgDesc = 3;	/* 1 = Not end-to-end ISDN */
			gen->ProgInd = Q931AppendIE((L3UCHAR *)gen, (L3UCHAR *)&Progress);

			/*
			 * Display IE
			 */
			if (!(isdn_data->opts & ZAP_ISDN_OPT_OMIT_DISPLAY_IE)) {
				Q931InitIEDisplay(&Display);
				Display.Size = Display.Size + (unsigned char)strlen(zchan->caller_data.cid_name);
				gen->Display = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &Display);
				ptrDisplay = Q931GetIEPtr(gen->Display, gen->buf);
				zap_copy_string((char *)ptrDisplay->Display, zchan->caller_data.cid_name, strlen(zchan->caller_data.cid_name)+1);
			}

			/*
			 * CallingNum IE
			 */ 
			Q931InitIECallingNum(&CallingNum);
			CallingNum.TypNum    = zchan->caller_data.ani.type;
			CallingNum.NumPlanID = Q931_NUMPLAN_E164;
			CallingNum.PresInd   = Q931_PRES_ALLOWED;
			CallingNum.ScreenInd = Q931_SCREEN_USER_NOT_SCREENED;
			CallingNum.Size = CallingNum.Size + (unsigned char)strlen(zchan->caller_data.cid_num.digits);
			gen->CallingNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CallingNum);
			ptrCallingNum = Q931GetIEPtr(gen->CallingNum, gen->buf);
			zap_copy_string((char *)ptrCallingNum->Digit, zchan->caller_data.cid_num.digits, strlen(zchan->caller_data.cid_num.digits)+1);

			/*
			 * CalledNum IE
			 */
			Q931InitIECalledNum(&CalledNum);
			CalledNum.TypNum    = Q931_TON_UNKNOWN;
			CalledNum.NumPlanID = Q931_NUMPLAN_E164;
			CalledNum.Size = CalledNum.Size + (unsigned char)strlen(zchan->caller_data.ani.digits);
			gen->CalledNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CalledNum);
			ptrCalledNum = Q931GetIEPtr(gen->CalledNum, gen->buf);
			zap_copy_string((char *)ptrCalledNum->Digit, zchan->caller_data.ani.digits, strlen(zchan->caller_data.ani.digits)+1);

			/*
			 * High-Layer Compatibility IE   (Note: Required for AVM FritzBox)
			 */
			Q931InitIEHLComp(&HLComp);
			HLComp.CodStand  = Q931_CODING_ITU;	/* ITU */
			HLComp.Interpret = 4;	/* only possible value */
			HLComp.PresMeth  = 1;   /* High-layer protocol profile */
			HLComp.HLCharID  = Q931_HLCHAR_TELEPHONY;	/* Telephony = 1, Fax G2+3 = 4, Fax G4 = 65 (Class I)/ 68 (Class II or III) */   /* TODO: make accessible from user layer */
			gen->HLComp = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &HLComp);

			Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
			isdn_data->channels_local_crv[gen->CRV] = zchan;
		}
		break;
	case ZAP_CHANNEL_STATE_HANGUP_COMPLETE:
		{
			/* reply RELEASE with RELEASE_COMPLETE message */
			if(zchan->last_state == ZAP_CHANNEL_STATE_HANGUP) {
				gen->MesType = Q931mes_RELEASE_COMPLETE;

				Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
			}
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case ZAP_CHANNEL_STATE_HANGUP:
		{
			Q931ie_Cause cause;

			zap_log(ZAP_LOG_DEBUG, "Hangup: Direction %s\n", zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND) ? "Outbound" : "Inbound");

			gen->CRVFlag = zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND) ? 0 : 1;

			cause.IEId = Q931ie_CAUSE;
			cause.Size = sizeof(Q931ie_Cause);
			cause.CodStand = Q931_CODING_ITU;	/* ITU */
			cause.Location = 1;	/* private network */
			cause.Recom    = 1;	/* */

			/*
			 * BRI PTMP needs special handling here...
			 * TODO: cleanup / refine (see above)
			 */
			if (zchan->last_state == ZAP_CHANNEL_STATE_RING) {
				/*
				 * inbound call [was: number unknown (= not found in routing state)]
				 * (in Q.931 spec terms: Reject request)
				 */
				gen->MesType = Q931mes_RELEASE_COMPLETE;

				//cause.Value = (unsigned char) ZAP_CAUSE_UNALLOCATED;
				cause.Value = (unsigned char) zchan->caller_data.hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);

				/* we're done, release channel */
				//zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE);
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			}
			else if (zchan->last_state <= ZAP_CHANNEL_STATE_PROGRESS) {
				/*
				 * just release all unanswered calls [was: inbound call, remote side hung up before we answered]
				 */
				gen->MesType = Q931mes_RELEASE;

				cause.Value = (unsigned char) zchan->caller_data.hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);

				/* this will be triggered by the RELEASE_COMPLETE reply */
				/* zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE); */
			}
			else {
				/*
				 * call connected, hangup
				 */
				gen->MesType = Q931mes_DISCONNECT;

				cause.Value = (unsigned char) zchan->caller_data.hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			zap_log(ZAP_LOG_DEBUG, "Terminating: Direction %s\n", zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND) ? "Outbound" : "Inbound");

			sig.event_id = ZAP_SIGEVENT_STOP;
			status = zap_span_send_signal(zchan->span, &sig);
			gen->MesType = Q931mes_RELEASE;
			gen->CRVFlag = zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND) ? 0 : 1;
			Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
		}
	default:
		break;
	}
}

/**
 * \brief Checks current state on a span
 * \param span Span to check status on
 */
static __inline__ void check_state(zap_span_t *span)
{
    if (zap_test_flag(span, ZAP_SPAN_STATE_CHANGE)) {
        uint32_t j;
        zap_clear_flag_locked(span, ZAP_SPAN_STATE_CHANGE);
        for(j = 1; j <= span->chan_count; j++) {
            if (zap_test_flag((span->channels[j]), ZAP_CHANNEL_STATE_CHANGE)) {
				zap_mutex_lock(span->channels[j]->mutex);
                zap_clear_flag((span->channels[j]), ZAP_CHANNEL_STATE_CHANGE);
                state_advance(span->channels[j]);
                zap_channel_complete_state(span->channels[j]);
				zap_mutex_unlock(span->channels[j]->mutex);
            }
        }
    }
}

/**
 * \brief Processes Openzap event on a span
 * \param span Span to process event on
 * \param event Event to process
 * \return Success or failure
 */
static __inline__ zap_status_t process_event(zap_span_t *span, zap_event_t *event)
{
	zap_log(ZAP_LOG_DEBUG, "EVENT [%s][%d:%d] STATE [%s]\n", 
			zap_oob_event2str(event->enum_id), event->channel->span_id, event->channel->chan_id, zap_channel_state2str(event->channel->state));

	switch(event->enum_id) {
	case ZAP_OOB_ALARM_TRAP:
		{
			if (event->channel->state != ZAP_CHANNEL_STATE_DOWN) {
				if (event->channel->type == ZAP_CHAN_TYPE_B) {
					zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_RESTART);
				}
			}
			

			zap_set_flag(event->channel, ZAP_CHANNEL_SUSPENDED);

			
			zap_channel_get_alarms(event->channel);
			zap_log(ZAP_LOG_WARNING, "channel %d:%d (%d:%d) has alarms! [%s]\n", 
					event->channel->span_id, event->channel->chan_id, 
					event->channel->physical_span_id, event->channel->physical_chan_id, 
					event->channel->last_error);
		}
		break;
	case ZAP_OOB_ALARM_CLEAR:
		{
			
			zap_log(ZAP_LOG_WARNING, "channel %d:%d (%d:%d) alarms Cleared!\n", event->channel->span_id, event->channel->chan_id,
					event->channel->physical_span_id, event->channel->physical_chan_id);

			zap_clear_flag(event->channel, ZAP_CHANNEL_SUSPENDED);
			zap_channel_get_alarms(event->channel);
		}
		break;
	}

	return ZAP_SUCCESS;
}

/**
 * \brief Checks for events on a span
 * \param span Span to check for events
 */
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

/**
 * \brief Retrieves tone generation output to be sent
 * \param ts Teletone generator
 * \param map Tone map
 * \return -1 on error, 0 on success
 */
static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	zap_buffer_t *dt_buffer = ts->user_data;
	int wrote;

	if (!dt_buffer) {
		return -1;
	}
	wrote = teletone_mux_tones(ts, map);
	zap_buffer_write(dt_buffer, ts->buffer, wrote * 2);
	return 0;
}

/**
 * \brief Main thread function for tone generation on a span
 * \param me Current thread
 * \param obj Span to generate tones on
 */
static void *zap_isdn_tones_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_isdn_data_t *isdn_data = span->signal_data;
	zap_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts = {{{{0}}}};
	unsigned char frame[1024];
	uint32_t x;
	int interval = 0;
	int offset = 0;

	zap_log(ZAP_LOG_DEBUG, "ISDN tones thread starting.\n");
	zap_set_flag(isdn_data, ZAP_ISDN_TONES_RUNNING);

	if (zap_buffer_create(&dt_buffer, 1024, 1024, 0) != ZAP_SUCCESS) {
		snprintf(isdn_data->dchan->last_error, sizeof(isdn_data->dchan->last_error), "memory error!");
		zap_log(ZAP_LOG_ERROR, "MEM ERROR\n");
		goto done;
	}
	zap_buffer_set_loops(dt_buffer, -1);

	/* get a tone generation friendly interval to avoid distortions */
	for (x = 1; x <= span->chan_count; x++) {
		if (span->channels[x]->type != ZAP_CHAN_TYPE_DQ921) {
			zap_channel_command(span->channels[x], ZAP_COMMAND_GET_INTERVAL, &interval);
			break;
		}
	}
	if (!interval) {
		interval = 20;
	}
	zap_log(ZAP_LOG_NOTICE, "Tone generating interval %d\n", interval);

	/* init teletone */
	teletone_init_session(&ts, 0, teletone_handler, dt_buffer);
	ts.rate     = 8000;
	ts.duration = ts.rate;

	/* main loop */
	while(zap_running() && zap_test_flag(isdn_data, ZAP_ISDN_TONES_RUNNING) && !zap_test_flag(isdn_data, ZAP_ISDN_STOP)) {
		zap_wait_flag_t flags;
		zap_status_t status;
		int last_chan_state = 0;
		int gated = 0;
		L2ULONG now = zap_time_now();

		/*
		 * check b-channel states and generate & send tones if neccessary
		 */
		for (x = 1; x <= span->chan_count; x++) {
			zap_channel_t *zchan = span->channels[x];
			zap_size_t len = sizeof(frame), rlen;

			if (zchan->type == ZAP_CHAN_TYPE_DQ921) {
				continue;
			}

			/*
			 * Generate tones based on current bchan state
			 * (Recycle buffer content if succeeding channels share the
			 *  same state, this saves some cpu cycles)
			 */
			switch (zchan->state) {
			case ZAP_CHANNEL_STATE_DIALTONE:
				{
					zap_isdn_bchan_data_t *data = (zap_isdn_bchan_data_t *)zchan->mod_data;

					/* check overlap dial timeout first before generating tone */
					if (data && data->digit_timeout && data->digit_timeout <= now) {
						if (strlen(zchan->caller_data.dnis.digits) > 0) {
							zap_log(ZAP_LOG_DEBUG, "Overlap dial timeout, advancing to RING state\n");
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
						} else {
							/* no digits received, hangup */
							zap_log(ZAP_LOG_DEBUG, "Overlap dial timeout, no digits received, going to HANGUP state\n");
							zchan->caller_data.hangup_cause = ZAP_CAUSE_RECOVERY_ON_TIMER_EXPIRE;	/* TODO: probably wrong cause value */
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
						}
						data->digit_timeout = 0;
						continue;
					}

					if (last_chan_state != zchan->state) {
						zap_buffer_zero(dt_buffer);
						teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_DIAL]);
						last_chan_state = zchan->state;
					}
				}
				break;

			case ZAP_CHANNEL_STATE_RING:
				{
					if (last_chan_state != zchan->state) {
						zap_buffer_zero(dt_buffer);
						teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_RING]);
						last_chan_state = zchan->state;
					}
				}
				break;

			default:	/* Not in a tone generating state, go to next round */
				continue;
			}

			if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
				if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
					continue;
				}
				zap_log(ZAP_LOG_NOTICE, "Successfully opened channel %d:%d\n", zchan->span_id, zchan->chan_id);
			}

			flags = ZAP_READ;

			status = zap_channel_wait(zchan, &flags, (gated) ? 0 : interval);
			switch(status) {
			case ZAP_FAIL:
				continue;

			case ZAP_TIMEOUT:
				gated = 1;
				continue;

			default:
				if (!(flags & ZAP_READ)) {
					continue;
				}
			}
			gated = 1;

			status = zap_channel_read(zchan, frame, &len);
			if (status != ZAP_SUCCESS || len <= 0) {
				continue;
			}

			if (zchan->effective_codec != ZAP_CODEC_SLIN) {
				len *= 2;
			}

			/* seek to current offset */
			zap_buffer_seek(dt_buffer, offset);

			rlen = zap_buffer_read_loop(dt_buffer, frame, len);

			if (zchan->effective_codec != ZAP_CODEC_SLIN) {
				zio_codec_t codec_func = NULL;

				if (zchan->native_codec == ZAP_CODEC_ULAW) {
					codec_func = zio_slin2ulaw;
				} else if (zchan->native_codec == ZAP_CODEC_ALAW) {
					codec_func = zio_slin2alaw;
				}

				if (codec_func) {
					status = codec_func(frame, sizeof(frame), &rlen);
				} else {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "codec error!");
					goto done;
				}
			}
			zap_channel_write(zchan, frame, sizeof(frame), &rlen);
		}

		/*
		 * sleep a bit if there was nothing to do
		 */
		if (!gated) {
			zap_sleep(interval);
		}

		offset += (ts.rate / (1000 / interval)) << 1;
		if (offset >= ts.rate) {
			offset = 0;
		}
	}

done:
	if (ts.buffer) {
		teletone_destroy_session(&ts);
	}

	if (dt_buffer) {
		zap_buffer_destroy(&dt_buffer);
	}

	zap_log(ZAP_LOG_DEBUG, "ISDN tone thread ended.\n");
	zap_clear_flag(isdn_data, ZAP_ISDN_TONES_RUNNING);

	return NULL;
}

/**
 * \brief Main thread function for an ISDN span
 * \param me Current thread
 * \param obj Span to monitor
 */
static void *zap_isdn_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_isdn_data_t *isdn_data = span->signal_data;
	unsigned char frame[1024];
	zap_size_t len = sizeof(frame);
	int errs = 0;

#ifdef WIN32
    timeBeginPeriod(1);
#endif

	zap_log(ZAP_LOG_DEBUG, "ISDN thread starting.\n");
	zap_set_flag(isdn_data, ZAP_ISDN_RUNNING);

	Q921Start(&isdn_data->q921);

	while(zap_running() && zap_test_flag(isdn_data, ZAP_ISDN_RUNNING) && !zap_test_flag(isdn_data, ZAP_ISDN_STOP)) {
		zap_wait_flag_t flags = ZAP_READ;
		zap_status_t status = zap_channel_wait(isdn_data->dchan, &flags, 100);

		Q921TimerTick(&isdn_data->q921);
		Q931TimerTick(&isdn_data->q931);
		check_state(span);
		check_events(span);

		/*
		 *
		 */
		switch(status) {
		case ZAP_FAIL:
			{
				zap_log(ZAP_LOG_ERROR, "D-Chan Read Error!\n");
				snprintf(span->last_error, sizeof(span->last_error), "D-Chan Read Error!");
				if (++errs == 10) {
					isdn_data->dchan->state = ZAP_CHANNEL_STATE_UP;
					goto done;
				}
			}
			break;
		case ZAP_TIMEOUT:
			{
				errs = 0;
			}
			break;
		default:
			{
				errs = 0;
				if (flags & ZAP_READ) {

					if (zap_test_flag(isdn_data->dchan, ZAP_CHANNEL_SUSPENDED)) {
						zap_clear_flag_all(span, ZAP_CHANNEL_SUSPENDED);
					}
					len = sizeof(frame);
					if (zap_channel_read(isdn_data->dchan, frame, &len) == ZAP_SUCCESS) {
#ifdef IODEBUG
						char bb[4096] = "";
						print_hex_bytes(frame, len, bb, sizeof(bb));

						print_bits(frame, (int)len, bb, sizeof(bb), ZAP_ENDIAN_LITTLE, 0);
						zap_log(ZAP_LOG_DEBUG, "READ %d\n%s\n%s\n\n", (int)len, LINE, bb);
#endif

						Q921QueueHDLCFrame(&isdn_data->q921, frame, (int)len);
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

/**
 * \brief Openzap ISDN signaling module initialisation
 * \return Success
 */
static ZIO_SIG_LOAD_FUNCTION(zap_isdn_init)
{
	Q931Initialize();

	Q921SetGetTimeCB(zap_time_now);
	Q931SetGetTimeCB(zap_time_now);

	return ZAP_SUCCESS;
}

/**
 * \brief Receives a Q931 indication message
 * \param pvt Span were message is coming from
 * \param ind Q931 indication
 * \param tei Terminal Endpoint Identifier
 * \param msg Message string
 * \param mlen Message string length
 * \return 0 on success
 */
static int q931_rx_32(void *pvt, Q921DLMsg_t ind, L3UCHAR tei, L3UCHAR *msg, L3INT mlen)
{
	int offset = 4;
	char bb[4096] = "";

	switch(ind) {
	case Q921_DL_UNIT_DATA:
		offset = 3;

	case Q921_DL_DATA:
		print_hex_bytes(msg + offset, mlen - offset, bb, sizeof(bb));
#ifdef HAVE_LIBPCAP
		/*Q931ToPcap*/
		if(do_q931ToPcap==1){
			zap_span_t *span = (zap_span_t *) pvt;
			if(writeQ931PacketToPcap(msg + offset, mlen - offset, span->span_id, 0) != ZAP_SUCCESS){
				zap_log(ZAP_LOG_WARNING, "Couldn't write Q931 buffer to pcap file!\n");	
			}
		}
		/*Q931ToPcap done*/
#endif
		zap_log(ZAP_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)mlen - offset, LINE, bb);
		break;

	default:
		break;
	}

	return Q921Rx32(pvt, ind, tei, msg, mlen);
}

/**
 * \brief Logs Q921 message
 * \param pvt Span were message is coming from
 * \param level Q921 log level
 * \param msg Message string
 * \param size Message string length
 * \return 0
 */
static int zap_isdn_q921_log(void *pvt, Q921LogLevel_t level, char *msg, L2INT size)
{
	zap_span_t *span = (zap_span_t *) pvt;

	zap_log("Span", "Q.921", span->span_id, (int)level, "%s", msg);
	return 0;
}

/**
 * \brief Logs Q931 message
 * \param pvt Span were message is coming from
 * \param level Q931 log level
 * \param msg Message string
 * \param size Message string length
 * \return 0
 */
static L3INT zap_isdn_q931_log(void *pvt, Q931LogLevel_t level, char *msg, L3INT size)
{
	zap_span_t *span = (zap_span_t *) pvt;

	zap_log("Span", "Q.931", span->span_id, (int)level, "%s", msg);
	return 0;
}
/**
 * \brief ISDN state map
 */
static zap_state_map_t isdn_state_map = {
	{
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_ANY_STATE},
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
			{ZAP_CHANNEL_STATE_DIALING, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DIALING, ZAP_END},
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_UP, ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_UP, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_DOWN, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_UP, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END}
		},

		/****************************************/
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_ANY_STATE},
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
			{ZAP_CHANNEL_STATE_DIALTONE, ZAP_CHANNEL_STATE_RING, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DIALTONE, ZAP_END},
			{ZAP_CHANNEL_STATE_RING, ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_UP, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, 
			 ZAP_CHANNEL_STATE_CANCEL, ZAP_CHANNEL_STATE_UP, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_UP, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
		},
		

	}
};

/**
 * \brief Stops an ISDN span
 * \param span Span to halt
 * \return Success
 *
 * Sets a stop flag and waits for the threads to end
 */
static zap_status_t zap_isdn_stop(zap_span_t *span)
{
	zap_isdn_data_t *isdn_data = span->signal_data;

	if (!zap_test_flag(isdn_data, ZAP_ISDN_RUNNING)) {
		return ZAP_FAIL;
	}
		
	zap_set_flag(isdn_data, ZAP_ISDN_STOP);
	
	while(zap_test_flag(isdn_data, ZAP_ISDN_RUNNING)) {
		zap_sleep(100);
	}

	while(zap_test_flag(isdn_data, ZAP_ISDN_TONES_RUNNING)) {
		zap_sleep(100);
	}

	return ZAP_SUCCESS;
	
}

/**
 * \brief Starts an ISDN span
 * \param span Span to halt
 * \return Success or failure
 *
 * Launches a thread to monitor the span and a thread to generate tones on the span
 */
static zap_status_t zap_isdn_start(zap_span_t *span)
{
	zap_status_t ret;
	zap_isdn_data_t *isdn_data = span->signal_data;

	if (zap_test_flag(isdn_data, ZAP_ISDN_RUNNING)) {
		return ZAP_FAIL;
	}

	zap_clear_flag(isdn_data, ZAP_ISDN_STOP);
	ret = zap_thread_create_detached(zap_isdn_run, span);

	if (ret != ZAP_SUCCESS) {
		return ret;
	}

	if (ZAP_SPAN_IS_NT(span) && !(isdn_data->opts & ZAP_ISDN_OPT_DISABLE_TONES)) {
		ret = zap_thread_create_detached(zap_isdn_tones_run, span);
	}
	return ret;
}

/**
 * \brief Parses an option string to flags
 * \param in String to parse for configuration options
 * \return Flags
 */
static uint32_t parse_opts(const char *in)
{
	uint32_t flags = 0;
	
	if (!in) {
		return 0;
	}
	
	if (strstr(in, "suggest_channel")) {
		flags |= ZAP_ISDN_OPT_SUGGEST_CHANNEL;
	}

	if (strstr(in, "omit_display")) {
		flags |= ZAP_ISDN_OPT_OMIT_DISPLAY_IE;
	}

	if (strstr(in, "disable_tones")) {
		flags |= ZAP_ISDN_OPT_DISABLE_TONES;
	}

	return flags;
}

/**
 * \brief Initialises an ISDN span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ap List of configuration variables
 * \return Success or failure
 */
static ZIO_SIG_CONFIGURE_FUNCTION(zap_isdn_configure_span)
{
	uint32_t i, x = 0;
	zap_channel_t *dchans[2] = {0};
	zap_isdn_data_t *isdn_data;
	const char *tonemap = "us";
	char *var, *val;
	Q931Dialect_t dialect = Q931_Dialect_National;
	int32_t digit_timeout = 0;
	int q921loglevel = -1;
	int q931loglevel = -1;
#ifdef HAVE_LIBPCAP
	int q931topcap   = -1; 	/*Q931ToPcap*/
	int openPcap = 0; 	/*Flag: open Pcap file please*/
#endif

	if (span->signal_type) {
#ifdef HAVE_LIBPCAP
		/*Q931ToPcap: Get the content of the q931topcap and pcapfilename args given by mod_openzap */
		while((var = va_arg(ap, char *))) {
			if (!strcasecmp(var, "q931topcap")) {
				q931topcap = va_arg(ap, int);
				if(q931topcap==1) {
					/*PCAP on*/;
					openPcap=1;
				} else if (q931topcap==0) {
					/*PCAP off*/
					if (closePcapFile() != ZAP_SUCCESS) return ZAP_FAIL;
					do_q931ToPcap=0;
					return ZAP_SUCCESS;
				}
			}
			if (!strcasecmp(var, "pcapfilename")) {
				/*Put filename into global var*/
				pcapfn = va_arg(ap, char*);
			}
		}
		/*We know now, that user wants to enable Q931ToPcap and what file name he wants, so open it please*/
		if(openPcap==1){
			if(openPcapFile() != ZAP_SUCCESS) return ZAP_FAIL;
			do_q931ToPcap=1;
			return ZAP_SUCCESS;
		}
		/*Q931ToPcap done*/
#endif
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling [%d].", span->signal_type);
		return ZAP_FAIL;
	}

	if (span->trunk_type >= ZAP_TRUNK_NONE) {
		zap_log(ZAP_LOG_WARNING, "Invalid trunk type '%s' defaulting to T1.\n", zap_trunk_type2str(span->trunk_type));
		span->trunk_type = ZAP_TRUNK_T1;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->type == ZAP_CHAN_TYPE_DQ921) {
			if (x > 1) {
				snprintf(span->last_error, sizeof(span->last_error), "Span has more than 2 D-Channels!");
				return ZAP_FAIL;
			} else {
				if (zap_channel_open(span->span_id, i, &dchans[x]) == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_DEBUG, "opening d-channel #%d %d:%d\n", x, dchans[x]->span_id, dchans[x]->chan_id);
					dchans[x]->state = ZAP_CHANNEL_STATE_UP;
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
	
	isdn_data->mode = Q931_TE;
	dialect = Q931_Dialect_National;
	
	while((var = va_arg(ap, char *))) {
		if (!strcasecmp(var, "mode")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			isdn_data->mode = strcasecmp(val, "net") ? Q931_TE : Q931_NT;
		} else if (!strcasecmp(var, "dialect")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			dialect = q931_str2Q931Dialect_type(val);
			if (dialect == Q931_Dialect_Count) {
				return ZAP_FAIL;
			}
		} else if (!strcasecmp(var, "opts")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			isdn_data->opts = parse_opts(val);
		} else if (!strcasecmp(var, "tonemap")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			tonemap = (const char *)val;
		} else if (!strcasecmp(var, "digit_timeout")) {
			int *optp;
			if (!(optp = va_arg(ap, int *))) {
				break;
			}
			digit_timeout = *optp;
		} else if (!strcasecmp(var, "q921loglevel")) {
			q921loglevel = va_arg(ap, int);
			if (q921loglevel < Q921_LOG_NONE) {
				q921loglevel = Q921_LOG_NONE;
			} else if (q921loglevel > Q921_LOG_DEBUG) {
				q921loglevel = Q921_LOG_DEBUG;
			}
		} else if (!strcasecmp(var, "q931loglevel")) {
			q931loglevel = va_arg(ap, int);
			if (q931loglevel < Q931_LOG_NONE) {
				q931loglevel = Q931_LOG_NONE;
			} else if (q931loglevel > Q931_LOG_DEBUG) {
				q931loglevel = Q931_LOG_DEBUG;
			}
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			return ZAP_FAIL;
		}
	}


	if (!digit_timeout) {
		digit_timeout = DEFAULT_DIGIT_TIMEOUT;
	}
	else if (digit_timeout < 3000 || digit_timeout > 30000) {
		zap_log(ZAP_LOG_WARNING, "Digit timeout %d ms outside of range (3000 - 30000 ms), using default (10000 ms)\n", digit_timeout);
		digit_timeout = DEFAULT_DIGIT_TIMEOUT;
	}

	/* allocate per b-chan data */
	if (isdn_data->mode == Q931_NT) {
		zap_isdn_bchan_data_t *data;

		data = malloc((span->chan_count - 1) * sizeof(zap_isdn_bchan_data_t));
		if (!data) {
			return ZAP_FAIL;
		}

		for (i = 1; i <= span->chan_count; i++, data++) {
			if (span->channels[i]->type == ZAP_CHAN_TYPE_B) {
				span->channels[i]->mod_data = data;
				memset(data, 0, sizeof(zap_isdn_bchan_data_t));
			}
		}
	}
					
	span->start = zap_isdn_start;
	span->stop = zap_isdn_stop;
	span->signal_cb = sig_cb;
	isdn_data->dchans[0] = dchans[0];
	isdn_data->dchans[1] = dchans[1];
	isdn_data->dchan = isdn_data->dchans[0];
	isdn_data->digit_timeout = digit_timeout;
	
	Q921_InitTrunk(&isdn_data->q921,
				   0,
				   0,
				   isdn_data->mode,
				   span->trunk_type == ZAP_TRUNK_BRI_PTMP ? Q921_PTMP : Q921_PTP,
				   0,
				   zap_isdn_921_21,
				   (Q921Tx23CB_t)zap_isdn_921_23,
				   span,
				   &isdn_data->q931);

	Q921SetLogCB(&isdn_data->q921, &zap_isdn_q921_log, isdn_data);
	Q921SetLogLevel(&isdn_data->q921, (Q921LogLevel_t)q921loglevel);
	
	Q931Api_InitTrunk(&isdn_data->q931,
					  dialect,
					  isdn_data->mode,
					  span->trunk_type,
					  zap_isdn_931_34,
					  (Q931Tx32CB_t)q931_rx_32,
					  zap_isdn_931_err,
					  &isdn_data->q921,
					  span);

	Q931SetLogCB(&isdn_data->q931, &zap_isdn_q931_log, isdn_data);
	Q931SetLogLevel(&isdn_data->q931, (Q931LogLevel_t)q931loglevel);

	isdn_data->q931.autoRestartAck = 1;
	isdn_data->q931.autoConnectAck = 0;
	isdn_data->q931.autoServiceAck = 1;
	span->signal_data = isdn_data;
	span->signal_type = ZAP_SIGTYPE_ISDN;
	span->outgoing_call = isdn_outgoing_call;

	if ((isdn_data->opts & ZAP_ISDN_OPT_SUGGEST_CHANNEL)) {
		span->channel_request = isdn_channel_request;
		span->suggest_chan_id = 1;
	}
	span->state_map = &isdn_state_map;

	zap_span_load_tones(span, tonemap);

	return ZAP_SUCCESS;
}

/**
 * \brief Openzap ISDN signaling module definition
 */
EX_DECLARE_DATA zap_module_t zap_module = {
	"isdn",
	NULL,
	close_pcap,
	zap_isdn_init,
	zap_isdn_configure_span,
	NULL
};



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
