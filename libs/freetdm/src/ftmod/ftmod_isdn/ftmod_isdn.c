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

/**
 * Workaround for missing u_int / u_short types on solaris
 */
#if defined(HAVE_LIBPCAP) && defined(__SunOS)
#define __EXTENSIONS__
#endif

#include "freetdm.h"
#include "Q931.h"
#include "Q921.h"
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "ftdm_isdn.h"

#define LINE "--------------------------------------------------------------------------------"
//#define IODEBUG

/* helper macros */
#define FTDM_SPAN_IS_BRI(x)	((x)->trunk_type == FTDM_TRUNK_BRI || (x)->trunk_type == FTDM_TRUNK_BRI_PTMP)
#define FTDM_SPAN_IS_NT(x)	(((ftdm_isdn_data_t *)(x)->signal_data)->mode == Q921_NT)


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
static ftdm_status_t openPcapFile(void)
{
        if(!pcaphandle)
        {
                pcaphandle = pcap_open_dead(DLT_EN10MB, SNAPLEN);
                if (!pcaphandle)
                {
                        ftdm_log(FTDM_LOG_ERROR, "Can't open pcap session: (%s)\n", pcap_geterr(pcaphandle));
                        return FTDM_FAIL;
                }
        }

        if(!pcapfile){
                /* Open the dump file */
                if(!(pcapfile=pcap_dump_open(pcaphandle, pcapfn))){
                        ftdm_log(FTDM_LOG_ERROR, "Error opening output file (%s)\n", pcap_geterr(pcaphandle));
                        return FTDM_FAIL;
                }
        }
        else{
                ftdm_log(FTDM_LOG_WARNING, "Pcap file is already open!\n");
                return FTDM_FAIL;
        }

        ftdm_log(FTDM_LOG_DEBUG, "Pcap file '%s' successfully opened!\n", pcapfn);

        pcaphdr.ts.tv_sec  	= 0;
        pcaphdr.ts.tv_usec 	= 0;
	pcapfilesize       	= 24;	/*current pcap file header seems to be 24 bytes*/
	tcp_next_seq_no_send    = 0;
	tcp_next_seq_no_rec	= 0;

        return FTDM_SUCCESS;
}

/**
 * \brief Closes a pcap file
 * \return Success
 */
static ftdm_status_t closePcapFile(void)
{
	if (pcapfile) {
		pcap_dump_close(pcapfile);
		if (pcaphandle) pcap_close(pcaphandle);

		ftdm_log(FTDM_LOG_DEBUG, "Pcap file closed! File size is %lu bytes.\n", pcapfilesize);

		pcaphdr.ts.tv_sec 	= 0;
		pcaphdr.ts.tv_usec 	= 0;
		pcapfile		= NULL;
		pcaphandle 		= NULL;
		pcapfilesize		= 0;
		tcp_next_seq_no_send	= 0;
		tcp_next_seq_no_rec	= 0;
	}

	/*We have allways success with this? I think so*/
	return FTDM_SUCCESS;
}

/**
 * \brief Writes a Q931 packet to a pcap file
 * \return Success or failure
 */
static ftdm_status_t writeQ931PacketToPcap(L3UCHAR* q931buf, L3USHORT q931size, L3ULONG span_id, L3USHORT direction)
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
		ftdm_log(FTDM_LOG_WARNING, "Q931 packet size is too big (%u)! Ignoring it!\n", q931size);
                return FTDM_FAIL;
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

	ftdm_log(FTDM_LOG_DEBUG, "Added %u bytes to pcap file. File size is now %lu, \n", q931size, pcapfilesize);

        return FTDM_SUCCESS;
}

#endif

/**
 * \brief Unloads pcap IO
 * \return Success or failure
 */
static FIO_IO_UNLOAD_FUNCTION(close_pcap)
{
#ifdef HAVE_LIBPCAP
	return closePcapFile();
#else
	return FTDM_SUCCESS;
#endif
}

/*Q931ToPcap functions DONE*/
/*-------------------------------------------------------------------------*/

/**
 * \brief Gets current time
 * \return Current time (in ms)
 */
static L2ULONG ftdm_time_now(void)
{
	return (L2ULONG)ftdm_current_time_in_ms();
}

/**
 * \brief Initialises an ISDN channel (outgoing call)
 * \param ftdmchan Channel to initiate call on
 * \return Success or failure
 */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(isdn_outgoing_call)
{
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DIALING);
	return status;
}

/**
 * \brief Requests an ISDN channel on a span (outgoing call)
 * \param span Span where to get a channel
 * \param chan_id Specific channel to get (0 for any)
 * \param direction Call direction (inbound, outbound)
 * \param caller_data Caller information
 * \param ftdmchan Channel to initialise
 * \return Success or failure
 */
static FIO_CHANNEL_REQUEST_FUNCTION(isdn_channel_request)
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
	ftdm_status_t status = FTDM_FAIL;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	int sanity = 60000;
	int codec  = 0;

	/*
	 * get codec type
	 */
	ftdm_channel_command(span->channels[chan_id], FTDM_COMMAND_GET_NATIVE_CODEC, &codec);

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
	BearerCap.UIL1Prot = (codec == FTDM_CODEC_ALAW) ? Q931_UIL1P_G711A : Q931_UIL1P_G711U;	/* U-law = 2, A-law = 3 */
	gen->BearerCap = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &BearerCap);

	/*
	 * Channel ID IE
	 */
	Q931InitIEChanID(&ChanID);
	ChanID.IntType = FTDM_SPAN_IS_BRI(span) ? 0 : 1;		/* PRI = 1, BRI = 0 */

	if(!FTDM_SPAN_IS_NT(span)) {
		ChanID.PrefExcl = (isdn_data->opts & FTDM_ISDN_OPT_SUGGEST_CHANNEL) ? 0 : 1; /* 0 = preferred, 1 exclusive */
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
	if (!(isdn_data->opts & FTDM_ISDN_OPT_OMIT_DISPLAY_IE)) {
		Q931InitIEDisplay(&Display);
		Display.Size = Display.Size + (unsigned char)strlen(caller_data->cid_name);
		gen->Display = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &Display);			
		ptrDisplay = Q931GetIEPtr(gen->Display, gen->buf);
		ftdm_copy_string((char *)ptrDisplay->Display, caller_data->cid_name, strlen(caller_data->cid_name)+1);
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
	ftdm_copy_string((char *)ptrCallingNum->Digit, caller_data->cid_num.digits, strlen(caller_data->cid_num.digits)+1);


	/*
	 * Called number IE
	 */
	Q931InitIECalledNum(&CalledNum);
	CalledNum.TypNum    = Q931_TON_UNKNOWN;
	CalledNum.NumPlanID = Q931_NUMPLAN_E164;
	CalledNum.Size = CalledNum.Size + (unsigned char)strlen(caller_data->dnis.digits);
	gen->CalledNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CalledNum);
	ptrCalledNum = Q931GetIEPtr(gen->CalledNum, gen->buf);
	ftdm_copy_string((char *)ptrCalledNum->Digit, caller_data->dnis.digits, strlen(caller_data->dnis.digits)+1);

	/*
	 * High-Layer Compatibility IE   (Note: Required for AVM FritzBox)
	 */
	Q931InitIEHLComp(&HLComp);
	HLComp.CodStand  = Q931_CODING_ITU;	/* ITU */
	HLComp.Interpret = 4;	/* only possible value */
	HLComp.PresMeth  = 1;   /* High-layer protocol profile */
	HLComp.HLCharID  = 1;	/* Telephony = 1, Fax G2+3 = 4, Fax G4 = 65 (Class I)/ 68 (Class II or III) */
	gen->HLComp = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &HLComp);

	caller_data->call_state = FTDM_CALLER_STATE_DIALING;
	Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
	
	isdn_data->outbound_crv[gen->CRV] = caller_data;
	//isdn_data->channels_local_crv[gen->CRV] = ftdmchan;

	while(ftdm_running() && caller_data->call_state == FTDM_CALLER_STATE_DIALING) {
		ftdm_sleep(1);
		
		if (!--sanity) {
			caller_data->call_state = FTDM_CALLER_STATE_FAIL;
			break;
		}
	}
	isdn_data->outbound_crv[gen->CRV] = NULL;
	
	if (caller_data->call_state == FTDM_CALLER_STATE_SUCCESS) {
		ftdm_channel_t *new_chan = NULL;
		int fail = 1;
		
		new_chan = NULL;
		if (caller_data->chan_id < FTDM_MAX_CHANNELS_SPAN && caller_data->chan_id <= span->chan_count) {
			new_chan = span->channels[caller_data->chan_id];
		}

		if (new_chan && (status = ftdm_channel_open_chan(new_chan) == FTDM_SUCCESS)) {
			if (ftdm_test_flag(new_chan, FTDM_CHANNEL_INUSE) || new_chan->state != FTDM_CHANNEL_STATE_DOWN) {
				if (new_chan->state == FTDM_CHANNEL_STATE_DOWN || new_chan->state >= FTDM_CHANNEL_STATE_TERMINATING) {
					int x = 0;
					ftdm_log(FTDM_LOG_WARNING, "Channel %d:%d ~ %d:%d is already in use waiting for it to become available.\n");
					
					for (x = 0; x < 200; x++) {
						if (!ftdm_test_flag(new_chan, FTDM_CHANNEL_INUSE)) {
							break;
						}
						ftdm_sleep(5);
					}
				}
				if (ftdm_test_flag(new_chan, FTDM_CHANNEL_INUSE)) {
					ftdm_log(FTDM_LOG_ERROR, "Channel %d:%d ~ %d:%d is already in use.\n",
							new_chan->span_id,
							new_chan->chan_id,
							new_chan->physical_span_id,
							new_chan->physical_chan_id
							);
					new_chan = NULL;
				}
			}

			if (new_chan && new_chan->state == FTDM_CHANNEL_STATE_DOWN) {
				isdn_data->channels_local_crv[gen->CRV] = new_chan;
				memset(&new_chan->caller_data, 0, sizeof(new_chan->caller_data));
				ftdm_set_flag(new_chan, FTDM_CHANNEL_OUTBOUND);
				ftdm_set_state_locked(new_chan, FTDM_CHANNEL_STATE_DIALING);
				switch(gen->MesType) {
				case Q931mes_ALERTING:
					new_chan->init_state = FTDM_CHANNEL_STATE_PROGRESS_MEDIA;
					break;
				case Q931mes_CONNECT:
					new_chan->init_state = FTDM_CHANNEL_STATE_UP;
					break;
				default:
					new_chan->init_state = FTDM_CHANNEL_STATE_PROGRESS;
					break;
				}

				fail = 0;
			} 
		}
		
		if (!fail) {
			*ftdmchan = new_chan;
			return FTDM_SUCCESS;
		} else {
			Q931ie_Cause cause;
			gen->MesType = Q931mes_DISCONNECT;
			cause.IEId = Q931ie_CAUSE;
			cause.Size = sizeof(Q931ie_Cause);
			cause.CodStand  = 0;
			cause.Location = 1;
			cause.Recom = 1;
			//should we be casting here.. or do we need to translate value?
			cause.Value = (unsigned char) FTDM_CAUSE_WRONG_CALL_STATE;
			*cause.Diag = '\0';
			gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
			Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);

			if (gen->CRV) {
				Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
			}
			
			if (new_chan) {
				ftdm_log(FTDM_LOG_CRIT, "Channel is busy\n");
			} else {
				ftdm_log(FTDM_LOG_CRIT, "Failed to open channel for new setup message\n");
			}
		}
	}
	
	*ftdmchan = NULL;
	return FTDM_FAIL;

}

/**
 * \brief Handler for Q931 error
 * \param pvt Private structure (span?)
 * \param id Error number
 * \param p1 ??
 * \param p2 ??
 * \return 0
 */
static L3INT ftdm_isdn_931_err(void *pvt, L3INT id, L3INT p1, L3INT p2)
{
	ftdm_log(FTDM_LOG_ERROR, "ERROR: [%s] [%d] [%d]\n", q931_error_to_name(id), p1, p2);
	return 0;
}

/**
 * \brief Handler for Q931 event message
 * \param pvt Span to handle
 * \param msg Message string
 * \param mlen Message string length
 * \return 0
 */
static L3INT ftdm_isdn_931_34(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	ftdm_span_t *span = (ftdm_span_t *) pvt;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	Q931mes_Generic *gen = (Q931mes_Generic *) msg;
	uint32_t chan_id = 0;
	int chan_hunt = 0;
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_caller_data_t *caller_data = NULL;

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
	} else if (FTDM_SPAN_IS_NT(span)) {
		/* no channel ie */
		chan_hunt++;
	}

	assert(span != NULL);
	assert(isdn_data != NULL);
	
	ftdm_log(FTDM_LOG_DEBUG, "Yay I got an event! Type:[%02x] Size:[%d] CRV: %d (%#hx, CTX: %s)\n", gen->MesType, gen->Size, gen->CRV, gen->CRV, gen->CRVFlag ? "Terminator" : "Originator");

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
				caller_data->call_state = FTDM_CALLER_STATE_SUCCESS;
			}
			break;
		default:
			caller_data->call_state = FTDM_CALLER_STATE_FAIL;
			break;
		}
	
		return 0;
	}

	if (gen->CRVFlag) {
		ftdmchan = isdn_data->channels_local_crv[gen->CRV];
	} else {
		ftdmchan = isdn_data->channels_remote_crv[gen->CRV];
	}

	ftdm_log(FTDM_LOG_DEBUG, "ftdmchan %x (%d:%d) source isdn_data->channels_%s_crv[%#hx]\n", ftdmchan, ftdmchan ? ftdmchan->span_id : -1, ftdmchan ? ftdmchan->chan_id : -1, gen->CRVFlag ? "local" : "remote", gen->CRV);


	if (gen->ProtDisc == 3) {
		switch(gen->MesType) {
		case Q931mes_SERVICE:
			{
				Q931ie_ChangeStatus *changestatus = Q931GetIEPtr(gen->ChangeStatus, gen->buf);
				if (ftdmchan) {
					switch (changestatus->NewStatus) {
					case 0: /* change status to "in service" */
						{
							ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_SUSPENDED);
							ftdm_log(FTDM_LOG_DEBUG, "Channel %d:%d in service\n", ftdmchan->span_id, ftdmchan->chan_id);
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
						}
						break;
					case 1: 
						{ /* change status to "maintenance" */
							ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_SUSPENDED);
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
						}
						break;
					case 2:
						{ /* change status to "out of service" */
							ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_SUSPENDED);
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
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
					ftdmchan = span->channels[chan_id];
				}
				if (ftdmchan) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				} else {
					uint32_t i;
					for (i = 1; i < span->chan_count; i++) {
						ftdm_set_state_locked((span->channels[i]), FTDM_CHANNEL_STATE_RESTART);
					}
				}
			}
			break;
		case Q931mes_RELEASE:
		case Q931mes_RELEASE_COMPLETE:
			{
				const char *what = gen->MesType == Q931mes_RELEASE ? "Release" : "Release Complete";
				if (ftdmchan) {
					if (ftdmchan->state == FTDM_CHANNEL_STATE_TERMINATING || ftdmchan->state == FTDM_CHANNEL_STATE_HANGUP) {
						if (gen->MesType == Q931mes_RELEASE) {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
						} else {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
						}
					}
					else if((gen->MesType == Q931mes_RELEASE && ftdmchan->state <= FTDM_CHANNEL_STATE_UP) ||
						(gen->MesType == Q931mes_RELEASE_COMPLETE && ftdmchan->state == FTDM_CHANNEL_STATE_DIALING)) {

						/*
						 * Don't keep inbound channels open if the remote side hangs up before we answered
						 */
						Q931ie_Cause *cause = Q931GetIEPtr(gen->Cause, gen->buf);
						ftdm_sigmsg_t sig;
						ftdm_status_t status;

						memset(&sig, 0, sizeof(sig));
						sig.chan_id = ftdmchan->chan_id;
						sig.span_id = ftdmchan->span_id;
						sig.channel = ftdmchan;
						sig.channel->caller_data.hangup_cause = (cause) ? cause->Value : FTDM_CAUSE_NORMAL_UNSPECIFIED;

						sig.event_id = FTDM_SIGEVENT_STOP;
						status = ftdm_span_send_signal(ftdmchan->span, &sig);

						ftdm_log(FTDM_LOG_DEBUG, "Received %s in state %s, requested hangup for channel %d:%d\n", what, ftdm_channel_state2str(ftdmchan->state), ftdmchan->span_id, chan_id);
					}
					else {
						ftdm_log(FTDM_LOG_DEBUG, "Ignoring %s on channel %d\n", what, chan_id);
					}
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received %s with no matching channel %d\n", what, chan_id);
				}
			}
			break;
		case Q931mes_DISCONNECT:
			{
				if (ftdmchan) {
					Q931ie_Cause *cause = Q931GetIEPtr(gen->Cause, gen->buf);
					ftdmchan->caller_data.hangup_cause = cause->Value;
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received Disconnect with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_ALERTING:
			{
				if (ftdmchan) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received Alerting with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_PROGRESS:
			{
				if (ftdmchan) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received Progress with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_CONNECT:
			{
				if (ftdmchan) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);

					gen->MesType = Q931mes_CONNECT_ACKNOWLEDGE;
					gen->CRVFlag = 0;	/* outbound */
					Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received Connect with no matching channel %d\n", chan_id);
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

				if(ftdmchan && ftdmchan == isdn_data->channels_remote_crv[gen->CRV]) {
					ftdm_log(FTDM_LOG_INFO, "Duplicate SETUP message(?) for Channel %d:%d ~ %d:%d in state %s [ignoring]\n",
									ftdmchan->span_id,
									ftdmchan->chan_id,
									ftdmchan->physical_span_id,
									ftdmchan->physical_chan_id,
									ftdm_channel_state2str(ftdmchan->state));
					break;
				}
				
				ftdmchan = NULL;
				/*
				 * Channel selection for incoming calls:
				 */
				if (FTDM_SPAN_IS_NT(span) && chan_hunt) {
					uint32_t x;

					/*
					 * In NT-mode with channel selection "any",
					 * try to find a free channel
					 */
					for (x = 1; x <= span->chan_count; x++) {
						ftdm_channel_t *zc = span->channels[x];

						if (!ftdm_test_flag(zc, FTDM_CHANNEL_INUSE) && zc->state == FTDM_CHANNEL_STATE_DOWN) {
							ftdmchan = zc;
							break;
						}
					}
				}
				else if (!FTDM_SPAN_IS_NT(span) && chan_hunt) {
					/*
					 * In TE-mode this ("any") is invalid
					 */
					fail_cause = FTDM_CAUSE_CHANNEL_UNACCEPTABLE;

					ftdm_log(FTDM_LOG_ERROR, "Invalid channel selection in incoming call (network side didn't specify a channel)\n");
				}
				else {
					/*
					 * Otherwise simply try to select the channel we've been told
					 *
					 * TODO: NT mode is abled to select a different channel if the one chosen
					 *       by the TE side is already in use
					 */
					if (chan_id > 0 && chan_id < FTDM_MAX_CHANNELS_SPAN && chan_id <= span->chan_count) {
						ftdmchan = span->channels[chan_id];
					}
					else {
						/* invalid channel id */
						fail_cause = FTDM_CAUSE_CHANNEL_UNACCEPTABLE;

						ftdm_log(FTDM_LOG_ERROR, "Invalid channel selection in incoming call (none selected or out of bounds)\n");
					}
				}

				if (!callednum || !strlen((char *)callednum->Digit)) {
					if (FTDM_SPAN_IS_NT(span)) {
						ftdm_log(FTDM_LOG_NOTICE, "No destination number found, assuming overlap dial\n");
						overlap_dial++;
					}
					else {
						ftdm_log(FTDM_LOG_ERROR, "No destination number found\n");
						ftdmchan = NULL;
					}
				}

				if (ftdmchan) {
					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE) || ftdmchan->state != FTDM_CHANNEL_STATE_DOWN) {
						if (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN || ftdmchan->state >= FTDM_CHANNEL_STATE_TERMINATING) {
							int x = 0;
							ftdm_log(FTDM_LOG_WARNING, "Channel %d:%d ~ %d:%d is already in use waiting for it to become available.\n",
									ftdmchan->span_id,
									ftdmchan->chan_id,
									ftdmchan->physical_span_id,
									ftdmchan->physical_chan_id);

							for (x = 0; x < 200; x++) {
								if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE)) {
									break;
								}
								ftdm_sleep(5);
							}
						}
						if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE)) {
							ftdm_log(FTDM_LOG_ERROR, "Channel %d:%d ~ %d:%d is already in use.\n",
									ftdmchan->span_id,
									ftdmchan->chan_id,
									ftdmchan->physical_span_id,
									ftdmchan->physical_chan_id
									);
							ftdmchan = NULL;
						}
					}

					if (ftdmchan && ftdmchan->state == FTDM_CHANNEL_STATE_DOWN) {
						isdn_data->channels_remote_crv[gen->CRV] = ftdmchan;
						memset(&ftdmchan->caller_data, 0, sizeof(ftdmchan->caller_data));

						if (ftdmchan->mod_data) {
							memset(ftdmchan->mod_data, 0, sizeof(ftdm_isdn_bchan_data_t));
						}

						ftdm_set_string(ftdmchan->caller_data.cid_num.digits, (char *)callingnum->Digit);
						ftdm_set_string(ftdmchan->caller_data.cid_name, (char *)callingnum->Digit);
						ftdm_set_string(ftdmchan->caller_data.ani.digits, (char *)callingnum->Digit);
						if (!overlap_dial) {
							ftdm_set_string(ftdmchan->caller_data.dnis.digits, (char *)callednum->Digit);
						}

						ftdmchan->caller_data.CRV = gen->CRV;
						if (cplen > sizeof(ftdmchan->caller_data.raw_data)) {
							cplen = sizeof(ftdmchan->caller_data.raw_data);
						}
						gen->CRVFlag = !(gen->CRVFlag);
						memcpy(ftdmchan->caller_data.raw_data, msg, cplen);
						ftdmchan->caller_data.raw_data_len = cplen;
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
					cause.Value = (unsigned char)((fail_cause) ? fail_cause : FTDM_CAUSE_WRONG_CALL_STATE);
					*cause.Diag = '\0';
					gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
					Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);

					if (gen->CRV) {
						Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
					}

					if (ftdmchan) {
						ftdm_log(FTDM_LOG_CRIT, "Channel is busy\n");
					} else {
						ftdm_log(FTDM_LOG_CRIT, "Failed to open channel for new setup message\n");
					}
					
				} else {
					Q931ie_ChanID ChanID;

					/*
					 * Update Channel ID IE
					 */
					Q931InitIEChanID(&ChanID);
					ChanID.IntType = FTDM_SPAN_IS_BRI(ftdmchan->span) ? 0 : 1;	/* PRI = 1, BRI = 0 */
					ChanID.PrefExcl = FTDM_SPAN_IS_NT(ftdmchan->span) ? 1 : 0;  /* Exclusive in NT-mode = 1, Preferred otherwise = 0 */
					if(ChanID.IntType) {
						ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
						ChanID.ChanMapType = 3;		/* B-Chan */
						ChanID.ChanSlot = (unsigned char)ftdmchan->chan_id;
					} else {
						ChanID.InfoChanSel = (unsigned char)ftdmchan->chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
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

						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DIALTONE);
					} else {
						/*
						 * Advance to RING state
						 */
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
					}
				}
			}
			break;

		case Q931mes_CALL_PROCEEDING:
			{
				if (ftdmchan) {
					ftdm_log(FTDM_LOG_CRIT, "Received CALL PROCEEDING message for channel %d\n", chan_id);
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received CALL PROCEEDING with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_CONNECT_ACKNOWLEDGE:
			{
				if (ftdmchan) {
					ftdm_log(FTDM_LOG_DEBUG, "Received CONNECT_ACK message for channel %d\n", chan_id);
				} else {
					ftdm_log(FTDM_LOG_DEBUG, "Received CONNECT_ACK with no matching channel %d\n", chan_id);
				}
			}
			break;

		case Q931mes_INFORMATION:
			{
				if (ftdmchan) {
					ftdm_log(FTDM_LOG_CRIT, "Received INFORMATION message for channel %d\n", ftdmchan->chan_id);

					if (ftdmchan->state == FTDM_CHANNEL_STATE_DIALTONE) {
						char digit = '\0';

						/*
						 * overlap dial digit indication
						 */
						if (Q931IsIEPresent(gen->CalledNum)) {
							ftdm_isdn_bchan_data_t *data = (ftdm_isdn_bchan_data_t *)ftdmchan->mod_data;
							Q931ie_CalledNum *callednum = Q931GetIEPtr(gen->CalledNum, gen->buf);
							int pos;

							digit = callednum->Digit[strlen((char *)callednum->Digit) - 1];
							if (digit == '#') {
								callednum->Digit[strlen((char *)callednum->Digit) - 1] = '\0';
							}

							/* TODO: make this more safe with strncat() */
							pos = (int)strlen(ftdmchan->caller_data.dnis.digits);
							strcat(&ftdmchan->caller_data.dnis.digits[pos],    (char *)callednum->Digit);

							/* update timer */
							data->digit_timeout = ftdm_time_now() + isdn_data->digit_timeout;

							ftdm_log(FTDM_LOG_DEBUG, "Received new overlap digit (%s), destination number: %s\n", callednum->Digit, ftdmchan->caller_data.dnis.digits);
						}

						if (Q931IsIEPresent(gen->SendComplete) || digit == '#') {
							ftdm_log(FTDM_LOG_DEBUG, "Leaving overlap dial mode\n");

							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
						}
					}
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received INFORMATION message with no matching channel\n");
				}
			}
			break;

		case Q931mes_STATUS_ENQUIRY:
			{
				/*
				 * !! HACK ALERT !!
				 *
				 * Map FreeTDM channel states to Q.931 states
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

				if(ftdmchan) {
					switch(ftdmchan->state) {
					case FTDM_CHANNEL_STATE_UP:
						state.CallState = Q931_U10;	/* Active */
						break;
					case FTDM_CHANNEL_STATE_RING:
						state.CallState = Q931_U6;	/* Call present */
						break;
					case FTDM_CHANNEL_STATE_DIALING:
						state.CallState = Q931_U1;	/* Call initiated */
						break;
					case FTDM_CHANNEL_STATE_DIALTONE:
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
			ftdm_log(FTDM_LOG_CRIT, "Received unhandled message %d (%#x)\n", (int)gen->MesType, (int)gen->MesType);
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
static int ftdm_isdn_921_23(void *pvt, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR *msg, L2INT mlen)
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
			ftdm_span_t *span = (ftdm_span_t *) pvt;
                        if(writeQ931PacketToPcap(msg + offset, mlen - offset, span->span_id, 1) != FTDM_SUCCESS){
                                ftdm_log(FTDM_LOG_WARNING, "Couldn't write Q931 buffer to pcap file!\n");
                        }
                }
                /*Q931ToPcap done*/
#endif
		ftdm_log(FTDM_LOG_DEBUG, "READ %d\n%s\n%s\n\n\n", (int)mlen - offset, LINE, bb);
	
	default:
		ret = Q931Rx23(pvt, ind, tei, msg, mlen);
		if (ret != 0)
			ftdm_log(FTDM_LOG_DEBUG, "931 parse error [%d] [%s]\n", ret, q931_error_to_name(ret));
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
static int ftdm_isdn_921_21(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	ftdm_span_t *span = (ftdm_span_t *) pvt;
	ftdm_size_t len = (ftdm_size_t) mlen;
	ftdm_isdn_data_t *isdn_data = span->signal_data;

#ifdef IODEBUG
	char bb[4096] = "";
	print_hex_bytes(msg, len, bb, sizeof(bb));
	print_bits(msg, (int)len, bb, sizeof(bb), FTDM_ENDIAN_LITTLE, 0);
	ftdm_log(FTDM_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)len, LINE, bb);

#endif

	assert(span != NULL);
	return ftdm_channel_write(isdn_data->dchan, msg, len, &len) == FTDM_SUCCESS ? 0 : -1;
}

/**
 * \brief Handler for channel state change
 * \param ftdmchan Channel to handle
 */
static __inline__ void state_advance(ftdm_channel_t *ftdmchan)
{
	Q931mes_Generic *gen = (Q931mes_Generic *) ftdmchan->caller_data.raw_data;
	ftdm_isdn_data_t *isdn_data = ftdmchan->span->signal_data;
	ftdm_sigmsg_t sig;
	ftdm_status_t status;

	ftdm_log(FTDM_LOG_DEBUG, "%d:%d STATE [%s]\n", 
			ftdmchan->span_id, ftdmchan->chan_id, ftdm_channel_state2str(ftdmchan->state));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;

	switch (ftdmchan->state) {
	case FTDM_CHANNEL_STATE_DOWN:
		{
			if (gen->CRV) {
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
					isdn_data->channels_local_crv[gen->CRV] = NULL;
				} else {
					isdn_data->channels_remote_crv[gen->CRV] = NULL;
				}
				Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
			}
			ftdm_channel_done(ftdmchan);
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				gen->MesType = Q931mes_CALL_PROCEEDING;
				gen->CRVFlag = 1;	/* inbound */

				if (FTDM_SPAN_IS_NT(ftdmchan->span)) {
					Q931ie_ChanID ChanID;

					/*
					 * Set new Channel ID
					 */
					Q931InitIEChanID(&ChanID);
					ChanID.IntType = FTDM_SPAN_IS_BRI(ftdmchan->span) ? 0 : 1;		/* PRI = 1, BRI = 0 */
					ChanID.PrefExcl = 1;	/* always exclusive in NT-mode */

					if(ChanID.IntType) {
						ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
						ChanID.ChanMapType = 3; 	/* B-Chan */
						ChanID.ChanSlot = (unsigned char)ftdmchan->chan_id;
					} else {
						ChanID.InfoChanSel = (unsigned char)ftdmchan->chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
					}
					gen->ChanID = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &ChanID);
				}

				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DIALTONE:
		{
			ftdm_isdn_bchan_data_t *data = (ftdm_isdn_bchan_data_t *)ftdmchan->mod_data;

			if (data) {
				data->digit_timeout = ftdm_time_now() + isdn_data->digit_timeout;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RING:
		{
			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_START;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RESTART:
		{
			ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_UNSPECIFIED;
			sig.event_id = FTDM_SIGEVENT_RESTART;
			status = ftdm_span_send_signal(ftdmchan->span, &sig);
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS_MEDIA;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
					if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
						return;
					}
				}
				gen->MesType = Q931mes_ALERTING;
				gen->CRVFlag = 1;	/* inbound call */
				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_UP:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_UP;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
					if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
						return;
					}
				}
				gen->MesType = Q931mes_CONNECT;
				gen->BearerCap = 0;
				gen->CRVFlag = 1;	/* inbound call */
				Q931Rx43(&isdn_data->q931, (void *)gen, ftdmchan->caller_data.raw_data_len);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DIALING:
		if (!(isdn_data->opts & FTDM_ISDN_OPT_SUGGEST_CHANNEL)) {
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
			ftdm_channel_command(ftdmchan->span->channels[ftdmchan->chan_id], FTDM_COMMAND_GET_NATIVE_CODEC, &codec);

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
			BearerCap.UIL1Prot = (codec == FTDM_CODEC_ALAW) ? 3 : 2;	/* U-law = 2, A-law = 3 */
			gen->BearerCap = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &BearerCap);

			/*
			 * ChannelID IE
			 */
			Q931InitIEChanID(&ChanID);
			ChanID.IntType = FTDM_SPAN_IS_BRI(ftdmchan->span) ? 0 : 1;	/* PRI = 1, BRI = 0 */
			ChanID.PrefExcl = FTDM_SPAN_IS_NT(ftdmchan->span) ? 1 : 0;  /* Exclusive in NT-mode = 1, Preferred otherwise = 0 */
			if(ChanID.IntType) {
				ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
				ChanID.ChanMapType = 3;		/* B-Chan */
				ChanID.ChanSlot = (unsigned char)ftdmchan->chan_id;
			} else {
				ChanID.InfoChanSel = (unsigned char)ftdmchan->chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
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
			if (!(isdn_data->opts & FTDM_ISDN_OPT_OMIT_DISPLAY_IE)) {
				Q931InitIEDisplay(&Display);
				Display.Size = Display.Size + (unsigned char)strlen(ftdmchan->caller_data.cid_name);
				gen->Display = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &Display);
				ptrDisplay = Q931GetIEPtr(gen->Display, gen->buf);
				ftdm_copy_string((char *)ptrDisplay->Display, ftdmchan->caller_data.cid_name, strlen(ftdmchan->caller_data.cid_name)+1);
			}

			/*
			 * CallingNum IE
			 */ 
			Q931InitIECallingNum(&CallingNum);
			CallingNum.TypNum    = ftdmchan->caller_data.ani.type;
			CallingNum.NumPlanID = Q931_NUMPLAN_E164;
			CallingNum.PresInd   = Q931_PRES_ALLOWED;
			CallingNum.ScreenInd = Q931_SCREEN_USER_NOT_SCREENED;
			CallingNum.Size = CallingNum.Size + (unsigned char)strlen(ftdmchan->caller_data.cid_num.digits);
			gen->CallingNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CallingNum);
			ptrCallingNum = Q931GetIEPtr(gen->CallingNum, gen->buf);
			ftdm_copy_string((char *)ptrCallingNum->Digit, ftdmchan->caller_data.cid_num.digits, strlen(ftdmchan->caller_data.cid_num.digits)+1);

			/*
			 * CalledNum IE
			 */
			Q931InitIECalledNum(&CalledNum);
			CalledNum.TypNum    = ftdmchan->caller_data.dnis.type;
			CalledNum.NumPlanID = Q931_NUMPLAN_E164;
			CalledNum.Size = CalledNum.Size + (unsigned char)strlen(ftdmchan->caller_data.dnis.digits);
			gen->CalledNum = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &CalledNum);
			ptrCalledNum = Q931GetIEPtr(gen->CalledNum, gen->buf);
			ftdm_copy_string((char *)ptrCalledNum->Digit, ftdmchan->caller_data.dnis.digits, strlen(ftdmchan->caller_data.dnis.digits)+1);

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
			isdn_data->channels_local_crv[gen->CRV] = ftdmchan;
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		{
			/* reply RELEASE with RELEASE_COMPLETE message */
			if(ftdmchan->last_state == FTDM_CHANNEL_STATE_HANGUP) {
				gen->MesType = Q931mes_RELEASE_COMPLETE;

				Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
			}
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP:
		{
			Q931ie_Cause cause;

			ftdm_log(FTDM_LOG_DEBUG, "Hangup: Direction %s\n", ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? "Outbound" : "Inbound");

			gen->CRVFlag = ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? 0 : 1;

			cause.IEId = Q931ie_CAUSE;
			cause.Size = sizeof(Q931ie_Cause);
			cause.CodStand = Q931_CODING_ITU;	/* ITU */
			cause.Location = 1;	/* private network */
			cause.Recom    = 1;	/* */

			/*
			 * BRI PTMP needs special handling here...
			 * TODO: cleanup / refine (see above)
			 */
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_RING) {
				/*
				 * inbound call [was: number unknown (= not found in routing state)]
				 * (in Q.931 spec terms: Reject request)
				 */
				gen->MesType = Q931mes_RELEASE_COMPLETE;

				//cause.Value = (unsigned char) FTDM_CAUSE_UNALLOCATED;
				cause.Value = (unsigned char) ftdmchan->caller_data.hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);

				/* we're done, release channel */
				//ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			}
			else if (ftdmchan->last_state <= FTDM_CHANNEL_STATE_PROGRESS) {
				/*
				 * just release all unanswered calls [was: inbound call, remote side hung up before we answered]
				 */
				gen->MesType = Q931mes_RELEASE;

				cause.Value = (unsigned char) ftdmchan->caller_data.hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, (void *)gen, gen->Size);

				/* this will be triggered by the RELEASE_COMPLETE reply */
				/* ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE); */
			}
			else {
				/*
				 * call connected, hangup
				 */
				gen->MesType = Q931mes_DISCONNECT;

				cause.Value = (unsigned char) ftdmchan->caller_data.hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, (L3UCHAR *) gen, gen->Size);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			ftdm_log(FTDM_LOG_DEBUG, "Terminating: Direction %s\n", ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? "Outbound" : "Inbound");

			sig.event_id = FTDM_SIGEVENT_STOP;
			status = ftdm_span_send_signal(ftdmchan->span, &sig);
			gen->MesType = Q931mes_RELEASE;
			gen->CRVFlag = ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? 0 : 1;
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
static __inline__ void check_state(ftdm_span_t *span)
{
    if (ftdm_test_flag(span, FTDM_SPAN_STATE_CHANGE)) {
        uint32_t j;
        ftdm_clear_flag_locked(span, FTDM_SPAN_STATE_CHANGE);
        for(j = 1; j <= span->chan_count; j++) {
            if (ftdm_test_flag((span->channels[j]), FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_mutex_lock(span->channels[j]->mutex);
                ftdm_clear_flag((span->channels[j]), FTDM_CHANNEL_STATE_CHANGE);
                state_advance(span->channels[j]);
                ftdm_channel_complete_state(span->channels[j]);
				ftdm_mutex_unlock(span->channels[j]->mutex);
            }
        }
    }
}

/**
 * \brief Processes FreeTDM event on a span
 * \param span Span to process event on
 * \param event Event to process
 * \return Success or failure
 */
static __inline__ ftdm_status_t process_event(ftdm_span_t *span, ftdm_event_t *event)
{
	ftdm_log(FTDM_LOG_DEBUG, "EVENT [%s][%d:%d] STATE [%s]\n", 
			ftdm_oob_event2str(event->enum_id), event->channel->span_id, event->channel->chan_id, ftdm_channel_state2str(event->channel->state));

	switch(event->enum_id) {
	case FTDM_OOB_ALARM_TRAP:
		{
			if (event->channel->state != FTDM_CHANNEL_STATE_DOWN) {
				if (event->channel->type == FTDM_CHAN_TYPE_B) {
					ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_RESTART);
				}
			}
			

			ftdm_set_flag(event->channel, FTDM_CHANNEL_SUSPENDED);

			
			ftdm_channel_get_alarms(event->channel);
			ftdm_log(FTDM_LOG_WARNING, "channel %d:%d (%d:%d) has alarms! [%s]\n", 
					event->channel->span_id, event->channel->chan_id, 
					event->channel->physical_span_id, event->channel->physical_chan_id, 
					event->channel->last_error);
		}
		break;
	case FTDM_OOB_ALARM_CLEAR:
		{
			
			ftdm_log(FTDM_LOG_WARNING, "channel %d:%d (%d:%d) alarms Cleared!\n", event->channel->span_id, event->channel->chan_id,
					event->channel->physical_span_id, event->channel->physical_chan_id);

			ftdm_clear_flag(event->channel, FTDM_CHANNEL_SUSPENDED);
			ftdm_channel_get_alarms(event->channel);
		}
		break;
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Checks for events on a span
 * \param span Span to check for events
 */
static __inline__ void check_events(ftdm_span_t *span)
{
	ftdm_status_t status;

	status = ftdm_span_poll_event(span, 5);

	switch(status) {
	case FTDM_SUCCESS:
		{
			ftdm_event_t *event;
			while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS) {
				if (event->enum_id == FTDM_OOB_NOOP) {
					continue;
				}
				if (process_event(span, event) != FTDM_SUCCESS) {
					break;
				}
			}
		}
		break;
	case FTDM_FAIL:
		{
			ftdm_log(FTDM_LOG_DEBUG, "Event Failure! %d\n", ftdm_running());
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
	ftdm_buffer_t *dt_buffer = ts->user_data;
	int wrote;

	if (!dt_buffer) {
		return -1;
	}
	wrote = teletone_mux_tones(ts, map);
	ftdm_buffer_write(dt_buffer, ts->buffer, wrote * 2);
	return 0;
}

/**
 * \brief Main thread function for tone generation on a span
 * \param me Current thread
 * \param obj Span to generate tones on
 */
static void *ftdm_isdn_tones_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	ftdm_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts = {{{{0}}}};
	unsigned char frame[1024];
	uint32_t x;
	int interval = 0;
	int offset = 0;

	ftdm_log(FTDM_LOG_DEBUG, "ISDN tones thread starting.\n");
	ftdm_set_flag(isdn_data, FTDM_ISDN_TONES_RUNNING);

	if (ftdm_buffer_create(&dt_buffer, 1024, 1024, 0) != FTDM_SUCCESS) {
		snprintf(isdn_data->dchan->last_error, sizeof(isdn_data->dchan->last_error), "memory error!");
		ftdm_log(FTDM_LOG_ERROR, "MEM ERROR\n");
		goto done;
	}
	ftdm_buffer_set_loops(dt_buffer, -1);

	/* get a tone generation friendly interval to avoid distortions */
	for (x = 1; x <= span->chan_count; x++) {
		if (span->channels[x]->type != FTDM_CHAN_TYPE_DQ921) {
			ftdm_channel_command(span->channels[x], FTDM_COMMAND_GET_INTERVAL, &interval);
			break;
		}
	}
	if (!interval) {
		interval = 20;
	}
	ftdm_log(FTDM_LOG_NOTICE, "Tone generating interval %d\n", interval);

	/* init teletone */
	teletone_init_session(&ts, 0, teletone_handler, dt_buffer);
	ts.rate     = 8000;
	ts.duration = ts.rate;

	/* main loop */
	while(ftdm_running() && ftdm_test_flag(isdn_data, FTDM_ISDN_TONES_RUNNING) && !ftdm_test_flag(isdn_data, FTDM_ISDN_STOP)) {
		ftdm_wait_flag_t flags;
		ftdm_status_t status;
		int last_chan_state = 0;
		int gated = 0;
		L2ULONG now = ftdm_time_now();

		/*
		 * check b-channel states and generate & send tones if neccessary
		 */
		for (x = 1; x <= span->chan_count; x++) {
			ftdm_channel_t *ftdmchan = span->channels[x];
			ftdm_size_t len = sizeof(frame), rlen;

			if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
				continue;
			}

			/*
			 * Generate tones based on current bchan state
			 * (Recycle buffer content if succeeding channels share the
			 *  same state, this saves some cpu cycles)
			 */
			switch (ftdmchan->state) {
			case FTDM_CHANNEL_STATE_DIALTONE:
				{
					ftdm_isdn_bchan_data_t *data = (ftdm_isdn_bchan_data_t *)ftdmchan->mod_data;

					/* check overlap dial timeout first before generating tone */
					if (data && data->digit_timeout && data->digit_timeout <= now) {
						if (strlen(ftdmchan->caller_data.dnis.digits) > 0) {
							ftdm_log(FTDM_LOG_DEBUG, "Overlap dial timeout, advancing to RING state\n");
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
						} else {
							/* no digits received, hangup */
							ftdm_log(FTDM_LOG_DEBUG, "Overlap dial timeout, no digits received, going to HANGUP state\n");
							ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_RECOVERY_ON_TIMER_EXPIRE;	/* TODO: probably wrong cause value */
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
						}
						data->digit_timeout = 0;
						continue;
					}

					if (last_chan_state != ftdmchan->state) {
						ftdm_buffer_zero(dt_buffer);
						teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_DIAL]);
						last_chan_state = ftdmchan->state;
					}
				}
				break;

			case FTDM_CHANNEL_STATE_RING:
				{
					if (last_chan_state != ftdmchan->state) {
						ftdm_buffer_zero(dt_buffer);
						teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_RING]);
						last_chan_state = ftdmchan->state;
					}
				}
				break;

			default:	/* Not in a tone generating state, go to next round */
				continue;
			}

			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
				if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
					continue;
				}
				ftdm_log(FTDM_LOG_NOTICE, "Successfully opened channel %d:%d\n", ftdmchan->span_id, ftdmchan->chan_id);
			}

			flags = FTDM_READ;

			status = ftdm_channel_wait(ftdmchan, &flags, (gated) ? 0 : interval);
			switch(status) {
			case FTDM_FAIL:
				continue;

			case FTDM_TIMEOUT:
				gated = 1;
				continue;

			default:
				if (!(flags & FTDM_READ)) {
					continue;
				}
			}
			gated = 1;

			status = ftdm_channel_read(ftdmchan, frame, &len);
			if (status != FTDM_SUCCESS || len <= 0) {
				continue;
			}

			if (ftdmchan->effective_codec != FTDM_CODEC_SLIN) {
				len *= 2;
			}

			/* seek to current offset */
			ftdm_buffer_seek(dt_buffer, offset);

			rlen = ftdm_buffer_read_loop(dt_buffer, frame, len);

			if (ftdmchan->effective_codec != FTDM_CODEC_SLIN) {
				fio_codec_t codec_func = NULL;

				if (ftdmchan->native_codec == FTDM_CODEC_ULAW) {
					codec_func = fio_slin2ulaw;
				} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW) {
					codec_func = fio_slin2alaw;
				}

				if (codec_func) {
					status = codec_func(frame, sizeof(frame), &rlen);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
					goto done;
				}
			}
			ftdm_channel_write(ftdmchan, frame, sizeof(frame), &rlen);
		}

		/*
		 * sleep a bit if there was nothing to do
		 */
		if (!gated) {
			ftdm_sleep(interval);
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
		ftdm_buffer_destroy(&dt_buffer);
	}

	ftdm_log(FTDM_LOG_DEBUG, "ISDN tone thread ended.\n");
	ftdm_clear_flag(isdn_data, FTDM_ISDN_TONES_RUNNING);

	return NULL;
}

/**
 * \brief Main thread function for an ISDN span
 * \param me Current thread
 * \param obj Span to monitor
 */
static void *ftdm_isdn_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	unsigned char frame[1024];
	ftdm_size_t len = sizeof(frame);
	int errs = 0;

#ifdef WIN32
    timeBeginPeriod(1);
#endif

	ftdm_log(FTDM_LOG_DEBUG, "ISDN thread starting.\n");
	ftdm_set_flag(isdn_data, FTDM_ISDN_RUNNING);

	Q921Start(&isdn_data->q921);

	while(ftdm_running() && ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING) && !ftdm_test_flag(isdn_data, FTDM_ISDN_STOP)) {
		ftdm_wait_flag_t flags = FTDM_READ;
		ftdm_status_t status = ftdm_channel_wait(isdn_data->dchan, &flags, 100);

		Q921TimerTick(&isdn_data->q921);
		Q931TimerTick(&isdn_data->q931);
		check_state(span);
		check_events(span);

		/*
		 *
		 */
		switch(status) {
		case FTDM_FAIL:
			{
				ftdm_log(FTDM_LOG_ERROR, "D-Chan Read Error!\n");
				snprintf(span->last_error, sizeof(span->last_error), "D-Chan Read Error!");
				if (++errs == 10) {
					isdn_data->dchan->state = FTDM_CHANNEL_STATE_UP;
					goto done;
				}
			}
			break;
		case FTDM_TIMEOUT:
			{
				errs = 0;
			}
			break;
		default:
			{
				errs = 0;
				if (flags & FTDM_READ) {

					if (ftdm_test_flag(isdn_data->dchan, FTDM_CHANNEL_SUSPENDED)) {
						ftdm_clear_flag_all(span, FTDM_CHANNEL_SUSPENDED);
					}
					len = sizeof(frame);
					if (ftdm_channel_read(isdn_data->dchan, frame, &len) == FTDM_SUCCESS) {
#ifdef IODEBUG
						char bb[4096] = "";
						print_hex_bytes(frame, len, bb, sizeof(bb));

						print_bits(frame, (int)len, bb, sizeof(bb), FTDM_ENDIAN_LITTLE, 0);
						ftdm_log(FTDM_LOG_DEBUG, "READ %d\n%s\n%s\n\n", (int)len, LINE, bb);
#endif

						Q921QueueHDLCFrame(&isdn_data->q921, frame, (int)len);
						Q921Rx12(&isdn_data->q921);
					}
				} else {
					ftdm_log(FTDM_LOG_DEBUG, "No Read FLAG!\n");
				}
			}
			break;
		}
	}
	
 done:
	ftdm_channel_close(&isdn_data->dchans[0]);
	ftdm_channel_close(&isdn_data->dchans[1]);
	ftdm_clear_flag(isdn_data, FTDM_ISDN_RUNNING);

#ifdef WIN32
    timeEndPeriod(1);
#endif

	ftdm_log(FTDM_LOG_DEBUG, "ISDN thread ended.\n");
	return NULL;
}

/**
 * \brief FreeTDM ISDN signaling module initialisation
 * \return Success
 */
static FIO_SIG_LOAD_FUNCTION(ftdm_isdn_init)
{
	Q931Initialize();

	Q921SetGetTimeCB(ftdm_time_now);
	Q931SetGetTimeCB(ftdm_time_now);

	return FTDM_SUCCESS;
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
			ftdm_span_t *span = (ftdm_span_t *) pvt;
			if(writeQ931PacketToPcap(msg + offset, mlen - offset, span->span_id, 0) != FTDM_SUCCESS){
				ftdm_log(FTDM_LOG_WARNING, "Couldn't write Q931 buffer to pcap file!\n");	
			}
		}
		/*Q931ToPcap done*/
#endif
		ftdm_log(FTDM_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)mlen - offset, LINE, bb);
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
static int ftdm_isdn_q921_log(void *pvt, Q921LogLevel_t level, char *msg, L2INT size)
{
	ftdm_span_t *span = (ftdm_span_t *) pvt;

	ftdm_log("Span", "Q.921", span->span_id, (int)level, "%s", msg);
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
static L3INT ftdm_isdn_q931_log(void *pvt, Q931LogLevel_t level, char *msg, L3INT size)
{
	ftdm_span_t *span = (ftdm_span_t *) pvt;

	ftdm_log("Span", "Q.931", span->span_id, (int)level, "%s", msg);
	return 0;
}
/**
 * \brief ISDN state map
 */
static ftdm_state_map_t isdn_state_map = {
	{
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
			{FTDM_CHANNEL_STATE_DIALING, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DIALING, FTDM_END},
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},

		/****************************************/
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
			{FTDM_CHANNEL_STATE_DIALTONE, FTDM_CHANNEL_STATE_RING, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DIALTONE, FTDM_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, 
			 FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_UP, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
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
static ftdm_status_t ftdm_isdn_stop(ftdm_span_t *span)
{
	ftdm_isdn_data_t *isdn_data = span->signal_data;

	if (!ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		return FTDM_FAIL;
	}
		
	ftdm_set_flag(isdn_data, FTDM_ISDN_STOP);
	
	while(ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		ftdm_sleep(100);
	}

	while(ftdm_test_flag(isdn_data, FTDM_ISDN_TONES_RUNNING)) {
		ftdm_sleep(100);
	}

	return FTDM_SUCCESS;
	
}

/**
 * \brief Starts an ISDN span
 * \param span Span to halt
 * \return Success or failure
 *
 * Launches a thread to monitor the span and a thread to generate tones on the span
 */
static ftdm_status_t ftdm_isdn_start(ftdm_span_t *span)
{
	ftdm_status_t ret;
	ftdm_isdn_data_t *isdn_data = span->signal_data;

	if (ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_clear_flag(isdn_data, FTDM_ISDN_STOP);
	ret = ftdm_thread_create_detached(ftdm_isdn_run, span);

	if (ret != FTDM_SUCCESS) {
		return ret;
	}

	if (FTDM_SPAN_IS_NT(span) && !(isdn_data->opts & FTDM_ISDN_OPT_DISABLE_TONES)) {
		ret = ftdm_thread_create_detached(ftdm_isdn_tones_run, span);
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
		flags |= FTDM_ISDN_OPT_SUGGEST_CHANNEL;
	}

	if (strstr(in, "omit_display")) {
		flags |= FTDM_ISDN_OPT_OMIT_DISPLAY_IE;
	}

	if (strstr(in, "disable_tones")) {
		flags |= FTDM_ISDN_OPT_DISABLE_TONES;
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
static FIO_SIG_CONFIGURE_FUNCTION(ftdm_isdn_configure_span)
{
	uint32_t i, x = 0;
	ftdm_channel_t *dchans[2] = {0};
	ftdm_isdn_data_t *isdn_data;
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
		/*Q931ToPcap: Get the content of the q931topcap and pcapfilename args given by mod_freetdm */
		while((var = va_arg(ap, char *))) {
			if (!strcasecmp(var, "q931topcap")) {
				q931topcap = va_arg(ap, int);
				if(q931topcap==1) {
					/*PCAP on*/;
					openPcap=1;
				} else if (q931topcap==0) {
					/*PCAP off*/
					if (closePcapFile() != FTDM_SUCCESS) return FTDM_FAIL;
					do_q931ToPcap=0;
					return FTDM_SUCCESS;
				}
			}
			if (!strcasecmp(var, "pcapfilename")) {
				/*Put filename into global var*/
				pcapfn = va_arg(ap, char*);
			}
		}
		/*We know now, that user wants to enable Q931ToPcap and what file name he wants, so open it please*/
		if(openPcap==1){
			if(openPcapFile() != FTDM_SUCCESS) return FTDM_FAIL;
			do_q931ToPcap=1;
			return FTDM_SUCCESS;
		}
		/*Q931ToPcap done*/
#endif
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling [%d].", span->signal_type);
		return FTDM_FAIL;
	}

	if (span->trunk_type >= FTDM_TRUNK_NONE) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid trunk type '%s' defaulting to T1.\n", ftdm_trunk_type2str(span->trunk_type));
		span->trunk_type = FTDM_TRUNK_T1;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->type == FTDM_CHAN_TYPE_DQ921) {
			if (x > 1) {
				snprintf(span->last_error, sizeof(span->last_error), "Span has more than 2 D-Channels!");
				return FTDM_FAIL;
			} else {
				if (ftdm_channel_open(span->span_id, i, &dchans[x]) == FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_DEBUG, "opening d-channel #%d %d:%d\n", x, dchans[x]->span_id, dchans[x]->chan_id);
					dchans[x]->state = FTDM_CHANNEL_STATE_UP;
					x++;
				}
			}
		}
	}

	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "Span has no D-Channels!");
		return FTDM_FAIL;
	}

	isdn_data = ftdm_malloc(sizeof(*isdn_data));
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
				return FTDM_FAIL;
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
			return FTDM_FAIL;
		}
	}


	if (!digit_timeout) {
		digit_timeout = DEFAULT_DIGIT_TIMEOUT;
	}
	else if (digit_timeout < 3000 || digit_timeout > 30000) {
		ftdm_log(FTDM_LOG_WARNING, "Digit timeout %d ms outside of range (3000 - 30000 ms), using default (10000 ms)\n", digit_timeout);
		digit_timeout = DEFAULT_DIGIT_TIMEOUT;
	}

	/* allocate per b-chan data */
	if (isdn_data->mode == Q931_NT) {
		ftdm_isdn_bchan_data_t *data;

		data = ftdm_malloc((span->chan_count - 1) * sizeof(ftdm_isdn_bchan_data_t));
		if (!data) {
			return FTDM_FAIL;
		}

		for (i = 1; i <= span->chan_count; i++, data++) {
			if (span->channels[i]->type == FTDM_CHAN_TYPE_B) {
				span->channels[i]->mod_data = data;
				memset(data, 0, sizeof(ftdm_isdn_bchan_data_t));
			}
		}
	}
					
	span->start = ftdm_isdn_start;
	span->stop = ftdm_isdn_stop;
	span->signal_cb = sig_cb;
	isdn_data->dchans[0] = dchans[0];
	isdn_data->dchans[1] = dchans[1];
	isdn_data->dchan = isdn_data->dchans[0];
	isdn_data->digit_timeout = digit_timeout;
	
	Q921_InitTrunk(&isdn_data->q921,
				   0,
				   0,
				   isdn_data->mode,
				   span->trunk_type == FTDM_TRUNK_BRI_PTMP ? Q921_PTMP : Q921_PTP,
				   0,
				   ftdm_isdn_921_21,
				   (Q921Tx23CB_t)ftdm_isdn_921_23,
				   span,
				   &isdn_data->q931);

	Q921SetLogCB(&isdn_data->q921, &ftdm_isdn_q921_log, isdn_data);
	Q921SetLogLevel(&isdn_data->q921, (Q921LogLevel_t)q921loglevel);
	
	Q931Api_InitTrunk(&isdn_data->q931,
					  dialect,
					  isdn_data->mode,
					  span->trunk_type,
					  ftdm_isdn_931_34,
					  (Q931Tx32CB_t)q931_rx_32,
					  ftdm_isdn_931_err,
					  &isdn_data->q921,
					  span);

	Q931SetLogCB(&isdn_data->q931, &ftdm_isdn_q931_log, isdn_data);
	Q931SetLogLevel(&isdn_data->q931, (Q931LogLevel_t)q931loglevel);

	isdn_data->q931.autoRestartAck = 1;
	isdn_data->q931.autoConnectAck = 0;
	isdn_data->q931.autoServiceAck = 1;
	span->signal_data = isdn_data;
	span->signal_type = FTDM_SIGTYPE_ISDN;
	span->outgoing_call = isdn_outgoing_call;

	if ((isdn_data->opts & FTDM_ISDN_OPT_SUGGEST_CHANNEL)) {
		span->channel_request = isdn_channel_request;
		ftdm_set_flag(span, FTDM_SPAN_SUGGEST_CHAN_ID);
	}
	span->state_map = &isdn_state_map;

	ftdm_span_load_tones(span, tonemap);

	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM ISDN signaling module definition
 */
EX_DECLARE_DATA ftdm_module_t ftdm_module = {
	"isdn",
	NULL,
	close_pcap,
	ftdm_isdn_init,
	ftdm_isdn_configure_span,
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
