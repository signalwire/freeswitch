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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <private/ftdm_core.h>
#include <libisdn/Q931.h>
#include <libisdn/Q921.h>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "ftmod_isdn.h"

#define LINE "--------------------------------------------------------------------------------"

/* helper macros */
#define FTDM_SPAN_IS_NT(x)	(((ftdm_isdn_data_t *)(x)->signal_data)->mode == Q921_NT)

#define DEFAULT_NATIONAL_PREFIX 	"0"
#define DEFAULT_INTERNATIONAL_PREFIX	"00"

/*****************************************************************************************
 * PCAP
 *          Based on Helmut Kuper's (<helmut.kuper@ewetel.de>) implementation,
 *          but using a different approach (needs a recent libpcap + wireshark)
 *****************************************************************************************/
#ifdef HAVE_PCAP
#include <arpa/inet.h>		/* htons() */
#include <pcap.h>

#define PCAP_SNAPLEN	1500

struct pcap_context {
	pcap_dumper_t		*dump;		/*!< pcap file handle  */
	pcap_t			*handle;	/*!< pcap lib context  */
	char			*filename;	/*!< capture file name */
};

static inline ftdm_status_t isdn_pcap_is_open(struct ftdm_isdn_data *isdn)
{
	return (isdn->pcap) ? 1 : 0;
}

static inline ftdm_status_t isdn_pcap_capture_both(struct ftdm_isdn_data *isdn)
{
	return ((isdn->flags & (FTDM_ISDN_CAPTURE | FTDM_ISDN_CAPTURE_L3ONLY)) == FTDM_ISDN_CAPTURE) ? 1 : 0;
}

static inline ftdm_status_t isdn_pcap_capture_l3only(struct ftdm_isdn_data *isdn)
{
	return ((isdn->flags & FTDM_ISDN_CAPTURE) && (isdn->flags & FTDM_ISDN_CAPTURE_L3ONLY)) ? 1 : 0;
}

static ftdm_status_t isdn_pcap_open(struct ftdm_isdn_data *isdn, char *filename)
{
	struct pcap_context *pcap = NULL;

	if (!isdn || ftdm_strlen_zero(filename))
		return FTDM_FAIL;

	pcap = malloc(sizeof(struct pcap_context));
	if (!pcap) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to allocate isdn pcap context\n");
		return FTDM_FAIL;
	}

	memset(pcap, 0, sizeof(struct pcap_context));

	pcap->filename = strdup(filename);

	pcap->handle = pcap_open_dead(DLT_LINUX_LAPD, PCAP_SNAPLEN);
	if (!pcap->handle) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to open pcap handle\n");
		goto error;
	}

	pcap->dump = pcap_dump_open(pcap->handle, pcap->filename);
	if (!pcap->dump) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to open capture file: '%s'\n", pcap_geterr(pcap->handle));
		goto error;
	}

	ftdm_log(FTDM_LOG_INFO, "Capture file '%s' opened\n", pcap->filename);

	isdn->pcap = pcap;

	return FTDM_SUCCESS;
error:
	if (pcap->handle)
		pcap_close(pcap->handle);
	if (pcap->filename)
		free(pcap->filename);

	free(pcap);

	return FTDM_FAIL;
}

static ftdm_status_t isdn_pcap_close(struct ftdm_isdn_data *isdn)
{
	struct pcap_context *pcap = NULL;
	long size;

	if (!isdn || !isdn->pcap)
		return FTDM_FAIL;

	pcap = isdn->pcap;

	isdn->flags &= ~(FTDM_ISDN_CAPTURE | FTDM_ISDN_CAPTURE_L3ONLY);
	isdn->pcap   = NULL;

	pcap_dump_flush(pcap->dump);

	size = pcap_dump_ftell(pcap->dump);
	ftdm_log(FTDM_LOG_INFO, "File '%s' captured %ld bytes of data\n", pcap->filename, size);

	pcap_dump_close(pcap->dump);
	pcap_close(pcap->handle);

	free(pcap->filename);
	free(pcap);

	return FTDM_SUCCESS;
}

static inline void isdn_pcap_start(struct ftdm_isdn_data *isdn)
{
	if (!isdn->pcap)
		return;

	isdn->flags |= FTDM_ISDN_CAPTURE;
}

static inline void isdn_pcap_stop(struct ftdm_isdn_data *isdn)
{
	isdn->flags &= ~FTDM_ISDN_CAPTURE;
}

#ifndef ETH_P_LAPD
#define ETH_P_LAPD 0x0030
#endif

struct isdn_sll_hdr {
	uint16_t slltype;
	uint16_t sllhatype;
	uint16_t slladdrlen;
	uint8_t  slladdr[8];
	uint16_t sllproto;
};

/* Fake Q.921 I-frame */
//static const char q921_fake_frame[] = { 0x00, 0x00, 0x00, 0x00 };

enum {
	ISDN_PCAP_INCOMING = 0,
	ISDN_PCAP_INCOMING_BCAST = 1,
	ISDN_PCAP_OUTGOING = 4,
};

static ftdm_status_t isdn_pcap_write(struct ftdm_isdn_data *isdn, unsigned char *buf, ftdm_ssize_t len, int direction)
{
	unsigned char frame[PCAP_SNAPLEN];
	struct pcap_context *pcap;
	struct isdn_sll_hdr *sll_hdr = (struct isdn_sll_hdr *)frame;
	struct pcap_pkthdr hdr;
	int offset = sizeof(struct isdn_sll_hdr);
	int nbytes;

	if (!isdn || !isdn->pcap || !buf || !len)
		return FTDM_FAIL;

	pcap = isdn->pcap;

	/* Update SLL header */
	sll_hdr->slltype    = htons(direction);
	sll_hdr->sllhatype  = 0;
	sll_hdr->slladdrlen = 1;
	sll_hdr->slladdr[0] = (isdn->mode == Q921_NT) ? 1 : 0;	/* TODO: NT/TE */
	sll_hdr->sllproto   = htons(ETH_P_LAPD);

#if 0
	/* Q.931-only mode: copy fake Q.921 header */
	if (isdn->flags & FTDM_ISDN_CAPTURE_L3ONLY) {
		/* copy fake q921 header */
		memcpy(frame + offset, q921_fake_frame, sizeof(q921_fake_frame));
		offset += sizeof(q921_fake_frame);
	}
#endif

	/* Copy data */
	nbytes = (len > (PCAP_SNAPLEN - offset)) ? (PCAP_SNAPLEN - offset) : len;
	memcpy(frame + offset, buf, nbytes);

	/* Update timestamp */
	memset(&hdr, 0, sizeof(struct pcap_pkthdr));
	gettimeofday(&hdr.ts, NULL);
	hdr.caplen = offset + nbytes;
	hdr.len    = hdr.caplen;

	/* Write packet */
	pcap_dump((unsigned char *)pcap->dump, &hdr, frame);

	return FTDM_SUCCESS;
}
#endif	/* HAVE_PCAP */


static L2ULONG ftdm_time_now(void)
{
	return (L2ULONG)ftdm_current_time_in_ms();
}

/**
 * \brief	Returns the signalling status on a channel
 * \param	ftdmchan	Channel to get status on
 * \param	status		Pointer to set signalling status
 * \return	Success or failure
 */
static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(isdn_get_channel_sig_status)
{
	*status = FTDM_SIG_STATE_DOWN;

	ftdm_isdn_data_t *isdn_data = ftdmchan->span->signal_data;
	if (ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		*status = FTDM_SIG_STATE_UP;
	}
	return FTDM_SUCCESS;
}

/**
 * \brief	Returns the signalling status on a span
 * \param	span	Span to get status on
 * \param	status	Pointer to set signalling status
 * \return	Success or failure
 */
static FIO_SPAN_GET_SIG_STATUS_FUNCTION(isdn_get_span_sig_status)
{
	*status = FTDM_SIG_STATE_DOWN;

	ftdm_isdn_data_t *isdn_data = span->signal_data;
	if (ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		*status = FTDM_SIG_STATE_UP;
	}
	return FTDM_SUCCESS;
}

/**
 * \brief	Create outgoing channel
 * \param	ftdmchan	Channel to create outgoing call on
 * \return	Success or failure
 */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(isdn_outgoing_call)
{
	return FTDM_SUCCESS;
}

/**
 * \brief	Create outgoing channel, let module select the channel to use
 * \param	span		Span to create outgoing call on
 * \param	caller_data
 * \return	Success or failure
 */
#ifdef __TODO__
static FIO_CHANNEL_REQUEST_FUNCTION(isdn_channel_request)
{
#if 1 /* FIXME caller_data.raw_data does not exist anymore, see docs/variables.txt for more info */
	Q931mes_Generic empty_gen;
	Q931mes_Generic *gen = &empty_gen;
	
	memset(&empty_gen, 0, sizeof(empty_gen)) ;
#else
	Q931mes_Generic *gen = (Q931mes_Generic *) caller_data->raw_data;
#endif
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
	gen->BearerCap = Q931AppendIE(gen, (L3UCHAR *) &BearerCap);

	/*
	 * Channel ID IE
	 */
	Q931InitIEChanID(&ChanID);
	ChanID.IntType = FTDM_SPAN_IS_BRI(span) ? 0 : 1;		/* PRI = 1, BRI = 0 */

	if (!FTDM_SPAN_IS_NT(span)) {
		ChanID.PrefExcl = (isdn_data->opts & FTDM_ISDN_OPT_SUGGEST_CHANNEL) ? 0 : 1; /* 0 = preferred, 1 exclusive */
	} else {
		ChanID.PrefExcl = 1;	/* always exclusive in NT-mode */
	}

	if (ChanID.IntType) {
		ChanID.InfoChanSel = 1;				/* None = 0, See Slot = 1, Any = 3 */
		ChanID.ChanMapType = 3; 			/* B-Chan */
		ChanID.ChanSlot = (unsigned char)chan_id;
	} else {
		ChanID.InfoChanSel = (unsigned char)chan_id & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
	}
	gen->ChanID = Q931AppendIE(gen, (L3UCHAR *) &ChanID);

	/*
	 * Progress IE
	 */
	Q931InitIEProgInd(&Progress);
	Progress.CodStand = Q931_CODING_ITU;	/* 0 = ITU */
	Progress.Location = 0;  /* 0 = User, 1 = Private Network */
	Progress.ProgDesc = 3;	/* 1 = Not end-to-end ISDN */
	gen->ProgInd = Q931AppendIE(gen, (L3UCHAR *)&Progress);

	/*
	 * Display IE
	 */
	if (!(isdn_data->opts & FTDM_ISDN_OPT_OMIT_DISPLAY_IE) && FTDM_SPAN_IS_NT(span)) {
		Q931InitIEDisplay(&Display);
		Display.Size = Display.Size + (unsigned char)strlen(caller_data->cid_name);
		gen->Display = Q931AppendIE(gen, (L3UCHAR *) &Display);
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
	gen->CallingNum = Q931AppendIE(gen, (L3UCHAR *) &CallingNum);
	ptrCallingNum = Q931GetIEPtr(gen->CallingNum, gen->buf);
	ftdm_copy_string((char *)ptrCallingNum->Digit, caller_data->cid_num.digits, strlen(caller_data->cid_num.digits)+1);


	/*
	 * Called number IE
	 */
	Q931InitIECalledNum(&CalledNum);
	CalledNum.TypNum    = Q931_TON_UNKNOWN;
	CalledNum.NumPlanID = Q931_NUMPLAN_E164;
	CalledNum.Size = CalledNum.Size + (unsigned char)strlen(caller_data->ani.digits);
	gen->CalledNum = Q931AppendIE(gen, (L3UCHAR *) &CalledNum);
	ptrCalledNum = Q931GetIEPtr(gen->CalledNum, gen->buf);
	ftdm_copy_string((char *)ptrCalledNum->Digit, caller_data->ani.digits, strlen(caller_data->ani.digits)+1);

	/*
	 * High-Layer Compatibility IE   (Note: Required for AVM FritzBox)
	 */
	Q931InitIEHLComp(&HLComp);
	HLComp.CodStand  = Q931_CODING_ITU;	/* ITU */
	HLComp.Interpret = 4;	/* only possible value */
	HLComp.PresMeth  = 1;   /* High-layer protocol profile */
	HLComp.HLCharID  = 1;	/* Telephony = 1, Fax G2+3 = 4, Fax G4 = 65 (Class I)/ 68 (Class II or III) */
	gen->HLComp = Q931AppendIE(gen, (L3UCHAR *) &HLComp);

	caller_data->call_state = FTDM_CALLER_STATE_DIALING;
	Q931Rx43(&isdn_data->q931, gen, gen->Size);

	isdn_data->outbound_crv[gen->CRV] = caller_data;
	//isdn_data->channels_local_crv[gen->CRV] = ftdmchan;

	while (ftdm_running() && caller_data->call_state == FTDM_CALLER_STATE_DIALING) {
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
		if (caller_data->chan_id > 0 && caller_data->chan_id <= ftdm_span_get_chan_count(span)) {
			new_chan = ftdm_span_get_channel(span, caller_data->chan_id);
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
				struct Q931_Call *call = NULL;

				memset(&new_chan->caller_data, 0, sizeof(new_chan->caller_data));
				ftdm_set_flag(new_chan, FTDM_CHANNEL_OUTBOUND);
				ftdm_set_state_locked(new_chan, FTDM_CHANNEL_STATE_DIALING);

				call = Q931GetCallByCRV(&isdn_data->q931, gen->CRV);
				Q931CallSetPrivate(call, new_chan);

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
			gen->Cause = Q931AppendIE(gen, (L3UCHAR *) &cause);
			Q931Rx43(&isdn_data->q931, gen, gen->Size);

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
#endif /* __TODO__ */

static L3INT ftdm_isdn_931_err(void *pvt, L3INT id, L3INT p1, L3INT p2)
{
	ftdm_log(FTDM_LOG_ERROR, "ERROR: [%s] [%d] [%d]\n", q931_error_to_name(id), p1, p2);
	return 0;
}

/**
 * \brief	The new call event handler
 * \note	W000t!!! \o/  ;D
 * \todo	A lot
 */
static void ftdm_isdn_call_event(struct Q931_Call *call, struct Q931_CallEvent *event, void *priv)
{
	Q931_TrunkInfo_t *trunk = NULL;
	ftdm_isdn_data_t *isdn_data = NULL;
	ftdm_span_t *span = priv;

	assert(span);
	assert(call);
	assert(event);

	trunk = Q931CallGetTrunk(call);
	assert(trunk);

	isdn_data = span->signal_data;
	assert(isdn_data);

	if (Q931CallIsGlobal(call)) {
		/*
		 * Global event
		 */
		ftdm_log(FTDM_LOG_DEBUG, "Received global event from Q.931\n");
	} else {
		ftdm_channel_t *ftdmchan = NULL;
		ftdm_sigmsg_t sig;
		int call_crv = Q931CallGetCRV(call);
		int type;

		/*
		 * Call-specific event
		 */
		ftdm_log(FTDM_LOG_DEBUG, "Received call-specific event from Q.931 for call %d [%hu]\n", Q931CallGetCRV(call), Q931CallGetCRV(call));

		/*
		 * Try to get associated zap channel
		 * and init sigmsg struct if there is one
		 */
		ftdmchan = Q931CallGetPrivate(call);
		if (ftdmchan) {
			memset(&sig, 0, sizeof(ftdm_sigmsg_t));
			sig.chan_id = ftdmchan->chan_id;
			sig.span_id = ftdmchan->span_id;
			sig.channel = ftdmchan;
		}

		type = Q931CallEventGetType(event);

		if (type == Q931_EVENT_TYPE_CRV) {

			ftdm_log(FTDM_LOG_DEBUG, "\tCRV event\n");

			switch (Q931CallEventGetId(event)) {
			case Q931_EVENT_RELEASE_CRV:
				{
					/* WARNING contains old interface code, yuck! */
					if (!ftdmchan) {
						ftdm_log(FTDM_LOG_DEBUG, "Call %d [0x%x] not associated to zap channel\n", call_crv, call_crv);
						return;
					}

					if (ftdm_channel_get_state(ftdmchan) != FTDM_CHANNEL_STATE_DOWN &&
					    ftdm_channel_get_state(ftdmchan) != FTDM_CHANNEL_STATE_HANGUP_COMPLETE)
					{
						ftdm_log(FTDM_LOG_DEBUG, "Channel %d:%d not in DOWN state, cleaning up\n",
									ftdm_channel_get_span_id(ftdmchan),
									ftdm_channel_get_id(ftdmchan));

						/*
						 * Send hangup signal to mod_openzap
						 */
						if (!sig.channel->caller_data.hangup_cause) {
							sig.channel->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_CLEARING;
						}

						sig.event_id = FTDM_SIGEVENT_STOP;
						ftdm_span_send_signal(ftdm_channel_get_span(ftdmchan), &sig);

						/* Release zap channel */
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
					}
					return;
				}
				break;
			default:
				ftdm_log(FTDM_LOG_ERROR, "Unknown CRV event: %d\n", Q931CallEventGetId(event));
				return;
			}
		}
		else if (type == Q931_EVENT_TYPE_TIMER) {
			struct Q931_CallTimerEvent *timer_evt = Q931CallEventGetData(event);

			ftdm_log(FTDM_LOG_DEBUG, "\tTimer event\n");
			assert(timer_evt->id);

			switch (timer_evt->id) {
			case Q931_TIMER_T303:
				/*
				 * SETUP timeout
				 *
				 * TE-mode: Q931_EVENT_SETUP_CONFIRM (error)
				 * NT-mode: Q931_EVENT_RELEASE_INDICATION
				 */
				{
					/* WARNING contains old interface code, yuck! */
					if (!ftdmchan) {
						ftdm_log(FTDM_LOG_ERROR, "Call %d [0x%x] not associated to zap channel\n", call_crv, call_crv);
						return;
					}

					ftdm_log(FTDM_LOG_DEBUG, "Call setup failed on channel %d:%d\n",
								ftdm_channel_get_span_id(ftdmchan),
								ftdm_channel_get_id(ftdmchan));

					/*
					 * Send signal to mod_openzap
					 */
					sig.channel->caller_data.hangup_cause = FTDM_CAUSE_NETWORK_OUT_OF_ORDER;

					sig.event_id = FTDM_SIGEVENT_STOP;
					ftdm_span_send_signal(ftdm_channel_get_span(ftdmchan), &sig);

					/* Release zap channel */
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
					return;
				}
				break;

			default:
				ftdm_log(FTDM_LOG_ERROR, "Unhandled timer event %d\n", timer_evt->id);
			}
		}
		else if (type == Q931_EVENT_TYPE_MESSAGE) {
			struct Q931_CallMessageEvent *msg_evt = Q931CallEventGetData(event);

			ftdm_log(FTDM_LOG_DEBUG, "\tMessage event\n");
			assert(msg_evt);

			/*
			 * Slowly move stuff from the old event handler into this part...
			 */
			switch (Q931CallEventGetId(event)) {
			case Q931_EVENT_SETUP_CONFIRM:
			case Q931_EVENT_SETUP_COMPLETE_INDICATION:	/* CONNECT */
				{
					/* WARNING contains old interface code, yuck! */
					if (!ftdmchan) {
						ftdm_log(FTDM_LOG_ERROR, "Call %d [0x%x] not associated to zap channel\n", call_crv, call_crv);
						return;
					}
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
				}
				break;

			default:
				ftdm_log(FTDM_LOG_DEBUG, "Not yet handled message event %d\n", Q931CallEventGetId(event));
			}
		}
		else {
			ftdm_log(FTDM_LOG_ERROR, "Unknown event type %d\n", type);
		}
	}
}

/**
 * Copy callednum, readding prefix as needed
 */
static void __isdn_get_number(const char *digits, const int ton, char *buf, int size)
{
	int offset = 0;

	if (!digits || !buf || size <= 0)
		return;

	switch (ton) {
	case Q931_TON_NATIONAL:
		offset = strlen(DEFAULT_NATIONAL_PREFIX);
		memcpy(buf, DEFAULT_NATIONAL_PREFIX, offset);
		break;
	case Q931_TON_INTERNATIONAL:
		offset = strlen(DEFAULT_INTERNATIONAL_PREFIX);
		memcpy(buf, DEFAULT_INTERNATIONAL_PREFIX, offset);
		break;
	default:
		break;
	}

	strncpy(&buf[offset], digits, size - (offset + 1));
	buf[size - 1] = '\0';
}

#define isdn_get_number(num, buf) \
	__isdn_get_number((const char *)(num)->Digit, (num)->TypNum, (char *)buf, sizeof(buf))


/**
 * \brief	The old call event handler (err, call message handler)
 * \todo	This one must die!
 */
static L3INT ftdm_isdn_931_34(void *pvt, struct Q931_Call *call, Q931mes_Generic *msg, int mlen)
{
	Q931mes_Generic *gen = (Q931mes_Generic *) msg;
	ftdm_span_t *span = (ftdm_span_t *) pvt;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	ftdm_channel_t *ftdmchan = NULL;
	int chan_id = 0;
	int chan_hunt = 0;

	if (Q931IsIEPresent(gen->ChanID)) {
		Q931ie_ChanID *chanid = Q931GetIEPtr(gen->ChanID, gen->buf);

		if (chanid->IntType)
			chan_id = chanid->ChanSlot;
		else
			chan_id = chanid->InfoChanSel;

		/* "any" channel specified */
		if (chanid->InfoChanSel == 3) {
			chan_hunt++;
		}
	} else if (FTDM_SPAN_IS_NT(span)) {
		/* no channel ie */
		chan_hunt++;
	}

	assert(span != NULL);
	assert(isdn_data != NULL);

	/* ftdm channel is stored in call private */
	if (call) {
		ftdmchan = Q931CallGetPrivate(call);
		if (!ftdmchan) {
			ftdm_log(FTDM_LOG_DEBUG, "[s%d] No channel associated to call [%#x] private\n",
				ftdm_span_get_id(span), Q931CallGetCRV(call));
		}
	}

	ftdm_log(FTDM_LOG_DEBUG, "Yay I got an event! Type:[%02x] Size:[%d] CRV: %d (%#hx, CTX: %s)\n",
		gen->MesType, gen->Size, gen->CRV, gen->CRV, gen->CRVFlag ? "Terminator" : "Originator");

#ifdef __TODO__
	/*
	 * This code block is needed for isdn_channel_request()
	 * isdn_data->outbound_crv has been removed so another way to pass data around is required
	 */
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
			caller_data->call_state = FTDM_CALLER_STATE_SUCCESS;
			break;
		default:
			caller_data->call_state = FTDM_CALLER_STATE_FAIL;
			break;
		}

		return 0;
	}
#endif
	ftdm_log(FTDM_LOG_DEBUG, "ftdmchan %p (%d:%d) via CRV[%#hx]\n",
			ftdmchan,
			((ftdmchan) ? ftdm_channel_get_span_id(ftdmchan) : -1),
			((ftdmchan) ? ftdm_channel_get_id(ftdmchan) : -1),
			gen->CRV);

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
							ftdm_log(FTDM_LOG_DEBUG, "Channel %d:%d in service\n",
									ftdm_channel_get_span_id(ftdmchan),
									ftdm_channel_get_id(ftdmchan));
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
					ftdmchan = ftdm_span_get_channel(span, chan_id);
				}
				if (ftdmchan) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				} else {
					uint32_t i;

					for (i = 1; i < ftdm_span_get_chan_count(span); i++) {
						ftdmchan = ftdm_span_get_channel(span, chan_id);

						/* Skip channels that are down and D-Channels (#OpenZAP-39) */
						if (ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_DOWN ||
						    ftdm_channel_get_type(ftdmchan) == FTDM_CHAN_TYPE_DQ921)
							continue;

						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
					}
				}
			}
			break;
		case Q931mes_RELEASE:
		case Q931mes_RELEASE_COMPLETE:
			{
				const char *what = gen->MesType == Q931mes_RELEASE ? "Release" : "Release Complete";
				if (ftdmchan) {
					if (ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_TERMINATING ||
					    ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_HANGUP)
					{
						if (gen->MesType == Q931mes_RELEASE) {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
						} else {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
						}
					}
					else if (gen->MesType == Q931mes_RELEASE_COMPLETE && ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_DIALTONE) {
						/* Go DOWN */
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
					}
					else if ((gen->MesType == Q931mes_RELEASE && ftdm_channel_get_state(ftdmchan) <= FTDM_CHANNEL_STATE_UP) ||
						 (gen->MesType == Q931mes_RELEASE_COMPLETE && ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_DIALING)) {

						/*
						 * Don't keep inbound channels open if the remote side hangs up before we answered
						 */
						Q931ie_Cause *cause = Q931GetIEPtr(gen->Cause, gen->buf);
						ftdm_sigmsg_t sig;

						memset(&sig, 0, sizeof(sig));
						sig.span_id = ftdm_channel_get_span_id(ftdmchan);
						sig.chan_id = ftdm_channel_get_id(ftdmchan);
						sig.channel = ftdmchan;
						sig.channel->caller_data.hangup_cause = (cause) ? cause->Value : FTDM_CAUSE_NORMAL_UNSPECIFIED;

						sig.event_id = FTDM_SIGEVENT_STOP;
						ftdm_span_send_signal(span, &sig);

						ftdm_log(FTDM_LOG_DEBUG, "Received %s in state %s, requested hangup for channel %d:%d\n", what,
								ftdm_channel_get_state_str(ftdmchan),
								ftdm_channel_get_span_id(ftdmchan),
								ftdm_channel_get_id(ftdmchan));
					} else {
						ftdm_log(FTDM_LOG_DEBUG, "Ignoring %s on channel %d in state %s\n", what,
							 ftdm_channel_get_id(ftdmchan), ftdm_channel_get_state_str(ftdmchan));
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
#if 0	/* Handled by new event code */
			{
				if (ftdmchan) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);

#if 0	/* Auto-Ack is enabled, we actually don't need this */
					gen->MesType = Q931mes_CONNECT_ACKNOWLEDGE;
					gen->CRVFlag = 0;	/* outbound */
					Q931Rx43(&isdn_data->q931, gen, gen->Size);
#endif
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received Connect with no matching channel %d\n", chan_id);
				}
			}
#endif
			break;
		case Q931mes_SETUP:
			{
				Q931ie_CallingNum *callingnum = Q931GetIEPtr(gen->CallingNum, gen->buf);
				Q931ie_CalledNum *callednum = Q931GetIEPtr(gen->CalledNum, gen->buf);
				int overlap_dial = 0;
				int fail_cause = 0;
				int fail = 1;

				if (ftdmchan && ftdmchan == Q931CallGetPrivate(call)) {
					ftdm_log(FTDM_LOG_INFO, "Duplicate SETUP message(?) for Channel %d:%d ~ %d:%d in state %s [ignoring]\n",
									ftdm_channel_get_span_id(ftdmchan),
									ftdm_channel_get_id(ftdmchan),
									ftdm_channel_get_ph_span_id(ftdmchan),
									ftdm_channel_get_ph_id(ftdmchan),
									ftdm_channel_get_state_str(ftdmchan));
					break;
				}

				ftdmchan = NULL;
				/*
				 * Channel selection for incoming calls:
				 */
				if (FTDM_SPAN_IS_NT(span) && chan_hunt) {
					int x;

					/*
					 * In NT-mode with channel selection "any",
					 * try to find a free channel
					 */
					for (x = 1; x <= ftdm_span_get_chan_count(span); x++) {
						ftdm_channel_t *zc = ftdm_span_get_channel(span, x);

						if (!ftdm_test_flag(zc, FTDM_CHANNEL_INUSE) && ftdm_channel_get_state(zc) == FTDM_CHANNEL_STATE_DOWN) {
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
					if (chan_id > 0 && chan_id < FTDM_MAX_CHANNELS_SPAN && chan_id <= ftdm_span_get_chan_count(span)) {
						ftdmchan = ftdm_span_get_channel(span, chan_id);
					}
					else {
						/* invalid channel id */
						fail_cause = FTDM_CAUSE_CHANNEL_UNACCEPTABLE;

						ftdm_log(FTDM_LOG_ERROR, "Invalid channel selection in incoming call (none selected or out of bounds)\n");
					}
				}

				if (!callednum || ftdm_strlen_zero((char *)callednum->Digit)) {
					if (FTDM_SPAN_IS_NT(span)) {
						ftdm_log(FTDM_LOG_NOTICE, "No destination number found, assuming overlap dial\n");
						overlap_dial++;
					} else {
						ftdm_log(FTDM_LOG_ERROR, "No destination number found\n");
						ftdmchan = NULL;
					}
				}

				if (ftdmchan) {
					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE) || ftdm_channel_get_state(ftdmchan) != FTDM_CHANNEL_STATE_DOWN) {
						if (ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_DOWN || ftdm_channel_get_state(ftdmchan) >= FTDM_CHANNEL_STATE_TERMINATING)
						{
							int x = 0;
							ftdm_log(FTDM_LOG_WARNING, "Channel %d:%d ~ %d:%d is already in use waiting for it to become available.\n",
									ftdm_channel_get_span_id(ftdmchan),
									ftdm_channel_get_id(ftdmchan),
									ftdm_channel_get_ph_span_id(ftdmchan),
									ftdm_channel_get_ph_id(ftdmchan));

							for (x = 0; x < 200; x++) {
								if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE)) {
									break;
								}
								ftdm_sleep(5);
							}
						}
						if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE)) {
							ftdm_log(FTDM_LOG_ERROR, "Channel %d:%d ~ %d:%d is already in use.\n",
									ftdm_channel_get_span_id(ftdmchan),
									ftdm_channel_get_id(ftdmchan),
									ftdm_channel_get_ph_span_id(ftdmchan),
									ftdm_channel_get_ph_id(ftdmchan));
							ftdmchan = NULL;
						}
					}

					if (ftdmchan && ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_DOWN) {
						ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(ftdmchan);

						memset(&ftdmchan->caller_data, 0, sizeof(ftdmchan->caller_data));

						if (ftdmchan->call_data) {
							memset(ftdmchan->call_data, 0, sizeof(ftdm_isdn_bchan_data_t));
						}

						/* copy number readd prefix as needed */
						isdn_get_number(callingnum, caller_data->cid_num.digits);
						isdn_get_number(callingnum, caller_data->cid_name);
						isdn_get_number(callingnum, caller_data->ani.digits);

						if (!overlap_dial) {
							isdn_get_number(callednum, caller_data->dnis.digits);
						}

						ftdmchan->caller_data.call_reference = gen->CRV;
						Q931CallSetPrivate(call, ftdmchan);

						gen->CRVFlag = !(gen->CRVFlag);

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
					gen->Cause = Q931AppendIE(gen, (L3UCHAR *) &cause);
					Q931Rx43(&isdn_data->q931, gen, gen->Size);

					if (gen->CRV) {
						Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
					}

					if (ftdmchan) {
						ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Channel is busy\n");
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
					if (ChanID.IntType) {
						ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
						ChanID.ChanMapType = 3;		/* B-Chan */
						ChanID.ChanSlot = (unsigned char)ftdm_channel_get_id(ftdmchan);
					} else {
						ChanID.InfoChanSel = (unsigned char)ftdm_channel_get_id(ftdmchan) & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
					}
					gen->ChanID = Q931AppendIE(gen, (L3UCHAR *) &ChanID);

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
						gen->ProgInd = Q931AppendIE(gen, (L3UCHAR *) &progress);

						/*
						 * Send SETUP ACK
						 */
						gen->MesType = Q931mes_SETUP_ACKNOWLEDGE;
						gen->CRVFlag = 1;	/* inbound call */
						Q931Rx43(&isdn_data->q931, gen, gen->Size);

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
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Received CALL PROCEEDING message for channel\n");
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
				} else {
					ftdm_log(FTDM_LOG_CRIT, "Received CALL PROCEEDING with no matching channel %d\n", chan_id);
				}
			}
			break;
		case Q931mes_CONNECT_ACKNOWLEDGE:
			{
				if (ftdmchan) {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Received CONNECT_ACK message for channel\n");
				} else {
					ftdm_log(FTDM_LOG_DEBUG, "Received CONNECT_ACK with no matching channel %d\n", chan_id);
				}
			}
			break;

		case Q931mes_INFORMATION:
			{
				if (ftdmchan) {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Received INFORMATION message for channel\n");

					if (ftdm_channel_get_state(ftdmchan) == FTDM_CHANNEL_STATE_DIALTONE) {
						char digit = '\0';

						/*
						 * overlap dial digit indication
						 */
						if (Q931IsIEPresent(gen->CalledNum)) {
							ftdm_isdn_bchan_data_t *data = (ftdm_isdn_bchan_data_t *)ftdmchan->call_data;
							Q931ie_CalledNum *callednum = Q931GetIEPtr(gen->CalledNum, gen->buf);
							int pos;

							digit = callednum->Digit[strlen((char *)callednum->Digit) - 1];
							if (digit == '#') {
								callednum->Digit[strlen((char *)callednum->Digit) - 1] = '\0';
							}

							/* TODO: make this more safe with strncat() */
							pos = strlen(ftdmchan->caller_data.dnis.digits);
							strcat(&ftdmchan->caller_data.dnis.digits[pos], (char *)callednum->Digit);

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

		default:
			ftdm_log(FTDM_LOG_CRIT, "Received unhandled message %d (%#x)\n", (int)gen->MesType, (int)gen->MesType);
			break;
		}
	}

	return 0;
}

static int ftdm_isdn_921_23(void *pvt, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR *msg, L2INT mlen)
{
	ftdm_span_t *span = pvt;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	int ret, offset = (ind == Q921_DL_DATA) ? 4 : 3;
	char bb[4096] = "";

	switch(ind) {
	case Q921_DL_DATA:
	case Q921_DL_UNIT_DATA:
		print_hex_bytes(msg + offset, mlen - offset, bb, sizeof(bb));
		ftdm_log(FTDM_LOG_DEBUG, "READ %d\n%s\n%s\n\n", (int)mlen - offset, LINE, bb);
#ifdef HAVE_PCAP
		if (isdn_pcap_capture_l3only(isdn_data)) {
			isdn_pcap_write(isdn_data, msg, mlen, (ind == Q921_DL_UNIT_DATA) ? ISDN_PCAP_INCOMING_BCAST : ISDN_PCAP_INCOMING);
		}
#endif
	default:
		ret = Q931Rx23(&isdn_data->q931, ind, tei, msg, mlen);
		if (ret != 0)
			ftdm_log(FTDM_LOG_DEBUG, "931 parse error [%d] [%s]\n", ret, q931_error_to_name(ret));
		break;
	}

	return ((ret >= 0) ? 1 : 0);
}

static int ftdm_isdn_921_21(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	ftdm_span_t *span = (ftdm_span_t *) pvt;
	ftdm_size_t len = (ftdm_size_t) mlen;
	ftdm_isdn_data_t *isdn_data = span->signal_data;

	assert(span != NULL);

#ifdef HAVE_PCAP
	if (isdn_pcap_capture_both(isdn_data)) {
		isdn_pcap_write(isdn_data, msg, mlen, ISDN_PCAP_OUTGOING);
	}
#endif
	return ftdm_channel_write(isdn_data->dchan, msg, len, &len) == FTDM_SUCCESS ? 0 : -1;
}

static __inline__ void state_advance(ftdm_channel_t *ftdmchan)
{
	ftdm_span_t *span = ftdm_channel_get_span(ftdmchan);
	ftdm_isdn_data_t *isdn_data = NULL;
	ftdm_sigmsg_t sig;
	ftdm_status_t status;

	Q931mes_Generic empty_gen;
	Q931mes_Generic *gen = &empty_gen;
	struct Q931_Call *call = NULL;

	Q931InitMesGeneric(gen);

	isdn_data = span->signal_data;
	assert(isdn_data);

	call = Q931GetCallByCRV(&isdn_data->q931, ftdmchan->caller_data.call_reference);
	if (call) {
		gen->CRV     = Q931CallGetCRV(call);
		gen->CRVFlag = Q931CallGetDirection(call) == Q931_DIRECTION_INBOUND ? 1 : 0;
	}

	ftdm_log(FTDM_LOG_DEBUG, "%d:%d STATE [%s]\n",
			ftdm_channel_get_span_id(ftdmchan),
			ftdm_channel_get_id(ftdmchan),
			ftdm_channel_get_state_str(ftdmchan));

	memset(&sig, 0, sizeof(sig));
	sig.span_id = ftdm_channel_get_span_id(ftdmchan);
	sig.chan_id = ftdm_channel_get_id(ftdmchan);
	sig.channel = ftdmchan;

	/* Acknowledge channel state change */
	ftdm_channel_complete_state(ftdmchan);

	switch (ftdm_channel_get_state(ftdmchan)) {
	case FTDM_CHANNEL_STATE_DOWN:
		{
			if (gen->CRV) {
				Q931CallSetPrivate(call, NULL);
				Q931ReleaseCRV(&isdn_data->q931, gen->CRV);
			}
			ftdmchan->caller_data.call_reference = 0;
			ftdm_channel_close(&ftdmchan);
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS;
				if ((status = ftdm_span_send_signal(ftdm_channel_get_span(ftdmchan), &sig) != FTDM_SUCCESS)) {
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				gen->MesType = Q931mes_CALL_PROCEEDING;

				if (FTDM_SPAN_IS_NT(ftdm_channel_get_span(ftdmchan))) {
					Q931ie_ChanID ChanID;

					/*
					 * Set new Channel ID
					 */
					Q931InitIEChanID(&ChanID);
					ChanID.IntType = FTDM_SPAN_IS_BRI(ftdm_channel_get_span(ftdmchan)) ? 0 : 1;		/* PRI = 1, BRI = 0 */
					ChanID.PrefExcl = 1;	/* always exclusive in NT-mode */

					if (ChanID.IntType) {
						ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
						ChanID.ChanMapType = 3; 	/* B-Chan */
						ChanID.ChanSlot = (unsigned char)ftdm_channel_get_id(ftdmchan);
					} else {
						ChanID.InfoChanSel = (unsigned char)ftdm_channel_get_id(ftdmchan) & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
					}
					gen->ChanID = Q931AppendIE(gen, (L3UCHAR *) &ChanID);
				}

				Q931Rx43(&isdn_data->q931, gen, gen->Size);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DIALTONE:
		{
			ftdm_isdn_bchan_data_t *data = (ftdm_isdn_bchan_data_t *)ftdmchan->call_data;

			if (data) {
				data->digit_timeout = ftdm_time_now() + isdn_data->digit_timeout;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RING:
		{
			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_START;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RESTART:
		{
			ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_UNSPECIFIED;
			sig.event_id = FTDM_SIGEVENT_RESTART;
			status = ftdm_span_send_signal(span, &sig);
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS_MEDIA;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
					if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
						return;
					}
				}
				gen->MesType = Q931mes_ALERTING;
				Q931Rx43(&isdn_data->q931, gen, gen->Size);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_UP:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_UP;
				if ((status = ftdm_span_send_signal(span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
					if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
						return;
					}
				}

				gen->MesType = Q931mes_CONNECT;

				Q931Rx43(&isdn_data->q931, gen, gen->Size);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DIALING:
		if (!(isdn_data->opts & FTDM_ISDN_OPT_SUGGEST_CHANNEL)) {
			ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(ftdmchan);
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
			ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_NATIVE_CODEC, &codec);

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
			gen->BearerCap = Q931AppendIE(gen, (L3UCHAR *) &BearerCap);

			/*
			 * ChannelID IE
			 */
			Q931InitIEChanID(&ChanID);
			ChanID.IntType = FTDM_SPAN_IS_BRI(ftdm_channel_get_span(ftdmchan)) ? 0 : 1;	/* PRI = 1, BRI = 0 */
			ChanID.PrefExcl = FTDM_SPAN_IS_NT(ftdm_channel_get_span(ftdmchan)) ? 1 : 0;  /* Exclusive in NT-mode = 1, Preferred otherwise = 0 */
			if (ChanID.IntType) {
				ChanID.InfoChanSel = 1;		/* None = 0, See Slot = 1, Any = 3 */
				ChanID.ChanMapType = 3;		/* B-Chan */
				ChanID.ChanSlot = (unsigned char)ftdm_channel_get_id(ftdmchan);
			} else {
				ChanID.InfoChanSel = (unsigned char)ftdm_channel_get_id(ftdmchan) & 0x03;	/* None = 0, B1 = 1, B2 = 2, Any = 3 */
			}
			gen->ChanID = Q931AppendIE(gen, (L3UCHAR *) &ChanID);

			/*
			 * Progress IE
			 */
			Q931InitIEProgInd(&Progress);
			Progress.CodStand = Q931_CODING_ITU;	/* 0 = ITU */
			Progress.Location = 0;  /* 0 = User, 1 = Private Network */
			Progress.ProgDesc = 3;	/* 1 = Not end-to-end ISDN */
			gen->ProgInd = Q931AppendIE(gen, (L3UCHAR *)&Progress);

			/*
			 * Display IE
			 */
			if (!(isdn_data->opts & FTDM_ISDN_OPT_OMIT_DISPLAY_IE) && FTDM_SPAN_IS_NT(ftdm_channel_get_span(ftdmchan))) {
				Q931InitIEDisplay(&Display);
				Display.Size = Display.Size + (unsigned char)strlen(caller_data->cid_name);
				gen->Display = Q931AppendIE(gen, (L3UCHAR *) &Display);
				ptrDisplay = Q931GetIEPtr(gen->Display, gen->buf);
				ftdm_copy_string((char *)ptrDisplay->Display, caller_data->cid_name, strlen(caller_data->cid_name) + 1);
			}

			/*
			 * CallingNum IE
			 */
			Q931InitIECallingNum(&CallingNum);
			CallingNum.TypNum    = caller_data->ani.type;
			CallingNum.NumPlanID = Q931_NUMPLAN_E164;
			CallingNum.PresInd   = Q931_PRES_ALLOWED;
			CallingNum.ScreenInd = Q931_SCREEN_USER_NOT_SCREENED;
			CallingNum.Size = CallingNum.Size + (unsigned char)strlen(caller_data->cid_num.digits);
			gen->CallingNum = Q931AppendIE(gen, (L3UCHAR *) &CallingNum);
			ptrCallingNum = Q931GetIEPtr(gen->CallingNum, gen->buf);
			ftdm_copy_string((char *)ptrCallingNum->Digit, caller_data->cid_num.digits, strlen(caller_data->cid_num.digits) + 1);

			/*
			 * CalledNum IE
			 */
			Q931InitIECalledNum(&CalledNum);
			CalledNum.TypNum    = Q931_TON_UNKNOWN;
			CalledNum.NumPlanID = Q931_NUMPLAN_E164;
			CalledNum.Size = CalledNum.Size + (unsigned char)strlen(caller_data->ani.digits);
			gen->CalledNum = Q931AppendIE(gen, (L3UCHAR *) &CalledNum);
			ptrCalledNum = Q931GetIEPtr(gen->CalledNum, gen->buf);
			ftdm_copy_string((char *)ptrCalledNum->Digit, caller_data->ani.digits, strlen(caller_data->ani.digits) + 1);

			/*
			 * High-Layer Compatibility IE   (Note: Required for AVM FritzBox)
			 */
			Q931InitIEHLComp(&HLComp);
			HLComp.CodStand  = Q931_CODING_ITU;	/* ITU */
			HLComp.Interpret = 4;	/* only possible value */
			HLComp.PresMeth  = 1;   /* High-layer protocol profile */
			HLComp.HLCharID  = Q931_HLCHAR_TELEPHONY;	/* Telephony = 1, Fax G2+3 = 4, Fax G4 = 65 (Class I)/ 68 (Class II or III) */   /* TODO: make accessible from user layer */
			gen->HLComp = Q931AppendIE(gen, (L3UCHAR *) &HLComp);

			Q931Rx43(&isdn_data->q931, gen, gen->Size);

			/*
			 * Support code for the new event handling system
			 * Remove this as soon as we have the new api to set up calls
			 */
			if (gen->CRV) {
				call = Q931GetCallByCRV(&isdn_data->q931, gen->CRV);
				if (call) {
					ftdm_log(FTDM_LOG_DEBUG, "Storing reference to current span in call %d [0x%x]\n", gen->CRV, gen->CRV);

					Q931CallSetPrivate(call, ftdmchan);
					ftdmchan->caller_data.call_reference = gen->CRV;
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		{
			/* reply RELEASE with RELEASE_COMPLETE message */
			if (ftdm_channel_get_last_state(ftdmchan) == FTDM_CHANNEL_STATE_HANGUP) {
				gen->MesType = Q931mes_RELEASE_COMPLETE;

				Q931Rx43(&isdn_data->q931, gen, gen->Size);
			}
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP:
		{
			ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(ftdmchan);
			Q931ie_Cause cause;

			ftdm_log(FTDM_LOG_DEBUG, "Hangup: Call direction %s\n",
				ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? "Outbound" : "Inbound");

			cause.IEId = Q931ie_CAUSE;
			cause.Size = sizeof(Q931ie_Cause);
			cause.CodStand = Q931_CODING_ITU;	/* ITU */
			cause.Location = 1;	/* private network */
			cause.Recom    = 1;	/* */

			/*
			 * BRI PTMP needs special handling here...
			 * TODO: cleanup / refine (see above)
			 */
			if (ftdm_channel_get_last_state(ftdmchan) == FTDM_CHANNEL_STATE_RING) {
				/*
				 * inbound call [was: number unknown (= not found in routing state)]
				 * (in Q.931 spec terms: Reject request)
				 */
				if (!FTDM_SPAN_IS_NT(span)) {
					gen->MesType = Q931mes_RELEASE_COMPLETE;	/* TE mode: Reject call */
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
				} else {
					gen->MesType = Q931mes_DISCONNECT;		/* NT mode: Disconnect and wait */
					//ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
				}

				//cause.Value = (unsigned char) FTDM_CAUSE_UNALLOCATED;
				cause.Value = (unsigned char) caller_data->hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE(gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, gen, gen->Size);

				/* we're done, release channel */
				////ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
				//ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			}
			else if (ftdm_channel_get_last_state(ftdmchan) <= FTDM_CHANNEL_STATE_PROGRESS) {
				/*
				 * just release all unanswered calls [was: inbound call, remote side hung up before we answered]
				 */
				gen->MesType = Q931mes_RELEASE;

				cause.Value = (unsigned char) caller_data->hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE(gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, gen, gen->Size);

				/* this will be triggered by the RELEASE_COMPLETE reply */
				/* ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE); */
			}
			else {
				/*
				 * call connected, hangup
				 */
				gen->MesType = Q931mes_DISCONNECT;

				cause.Value = (unsigned char) caller_data->hangup_cause;
				*cause.Diag = '\0';
				gen->Cause = Q931AppendIE(gen, (L3UCHAR *) &cause);
				Q931Rx43(&isdn_data->q931, gen, gen->Size);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			ftdm_log(FTDM_LOG_DEBUG, "Terminating: Call direction %s\n",
				ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? "Outbound" : "Inbound");

			sig.event_id = FTDM_SIGEVENT_STOP;
			status = ftdm_span_send_signal(span, &sig);

			gen->MesType = Q931mes_RELEASE;
			Q931Rx43(&isdn_data->q931, gen, gen->Size);
		}
	default:
		break;
	}
}

static __inline__ void check_state(ftdm_span_t *span)
{
	if (ftdm_test_flag(span, FTDM_SPAN_STATE_CHANGE)) {
	        uint32_t j;

	        ftdm_clear_flag_locked(span, FTDM_SPAN_STATE_CHANGE);

	        for (j = 1; j <= ftdm_span_get_chan_count(span); j++) {
			ftdm_channel_t *chan = ftdm_span_get_channel(span, j);

			if (ftdm_test_flag(chan, FTDM_CHANNEL_STATE_CHANGE)) {
				ftdm_channel_lock(chan);
				ftdm_clear_flag(chan, FTDM_CHANNEL_STATE_CHANGE);
				state_advance(chan);
				ftdm_channel_unlock(chan);
			}
		}
	}
}


static __inline__ ftdm_status_t process_event(ftdm_span_t *span, ftdm_event_t *event)
{
	ftdm_alarm_flag_t alarmbits;
	ftdm_sigmsg_t sig;

	memset(&sig, 0, sizeof(sig));
	sig.span_id = ftdm_channel_get_span_id(event->channel);
	sig.chan_id = ftdm_channel_get_id(event->channel);
	sig.channel = event->channel;

	ftdm_log(FTDM_LOG_DEBUG, "EVENT [%s][%d:%d] STATE [%s]\n",
			ftdm_oob_event2str(event->enum_id),
			ftdm_channel_get_span_id(event->channel),
			ftdm_channel_get_id(event->channel),
			ftdm_channel_get_state_str(event->channel));

	switch (event->enum_id) {
	case FTDM_OOB_ALARM_TRAP:
		{
			sig.event_id = FTDM_OOB_ALARM_TRAP;
			if (ftdm_channel_get_state(event->channel) != FTDM_CHANNEL_STATE_DOWN) {
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_RESTART);
			}
			ftdm_set_flag(event->channel, FTDM_CHANNEL_SUSPENDED);
			ftdm_channel_get_alarms(event->channel, &alarmbits);
			ftdm_span_send_signal(span, &sig);

			ftdm_log(FTDM_LOG_WARNING, "channel %d:%d (%d:%d) has alarms [%s]\n",
					ftdm_channel_get_span_id(event->channel),
					ftdm_channel_get_id(event->channel),
					ftdm_channel_get_ph_span_id(event->channel),
					ftdm_channel_get_ph_id(event->channel),
					ftdm_channel_get_last_error(event->channel));
		}
		break;
	case FTDM_OOB_ALARM_CLEAR:
		{
			sig.event_id = FTDM_OOB_ALARM_CLEAR;
			ftdm_clear_flag(event->channel, FTDM_CHANNEL_SUSPENDED);
			ftdm_channel_get_alarms(event->channel, &alarmbits);
			ftdm_span_send_signal(span, &sig);
		}
		break;
#ifdef __BROKEN_BY_FREETDM_CONVERSION__
	case FTDM_OOB_DTMF:	/* Taken from ozmod_analog, minus the CALLWAITING state handling */
		{
			const char * digit_str = (const char *)event->data;

			if (digit_str) {
				fio_event_cb_t event_callback = NULL;

				ftdm_channel_queue_dtmf(event->channel, digit_str);
				if (span->event_callback) {
					event_callback = span->event_callback;
				} else if (event->channel->event_callback) {
					event_callback = event->channel->event_callback;
				}

				if (event_callback) {
					event->channel->event_header.channel = event->channel;
					event->channel->event_header.e_type = FTDM_EVENT_DTMF;
					event->channel->event_header.data = (void *)digit_str;
					event_callback(event->channel, &event->channel->event_header);
					event->channel->event_header.e_type = FTDM_EVENT_NONE;
					event->channel->event_header.data = NULL;
				}
				ftdm_safe_free(event->data);
			}
		}
		break;
#endif
	}

	return FTDM_SUCCESS;
}


static __inline__ void check_events(ftdm_span_t *span)
{
	ftdm_status_t status = ftdm_span_poll_event(span, 5, NULL);

	switch (status) {
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

static void *ftdm_isdn_tones_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	ftdm_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts = {{{{0}}}};;
	unsigned char frame[1024];
	int x, interval;

	ftdm_log(FTDM_LOG_DEBUG, "ISDN tones thread starting.\n");
	ftdm_set_flag(isdn_data, FTDM_ISDN_TONES_RUNNING);

	if (ftdm_buffer_create(&dt_buffer, 1024, 1024, 0) != FTDM_SUCCESS) {
		snprintf(isdn_data->dchan->last_error, sizeof(isdn_data->dchan->last_error), "memory error!");
		ftdm_log(FTDM_LOG_ERROR, "MEM ERROR\n");
		goto done;
	}
	ftdm_buffer_set_loops(dt_buffer, -1);

	/* get a tone generation friendly interval to avoid distortions */
	for (x = 1; x <= ftdm_span_get_chan_count(span); x++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, x);

		if (ftdm_channel_get_type(chan) != FTDM_CHAN_TYPE_DQ921) {
			ftdm_channel_command(chan, FTDM_COMMAND_GET_INTERVAL, &interval);
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
	while (ftdm_running() && ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		ftdm_wait_flag_t flags;
		ftdm_status_t status;
		int last_chan_state = 0;
		int gated = 0;
		L2ULONG now = ftdm_time_now();

		/*
		 * check b-channel states and generate & send tones if neccessary
		 */
		for (x = 1; x <= ftdm_span_get_chan_count(span); x++) {
			ftdm_channel_t *chan = ftdm_span_get_channel(span, x);
			ftdm_size_t len = sizeof(frame), rlen;
			ftdm_isdn_bchan_data_t *data = chan->call_data;

			if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_DQ921) {
				continue;
			}

			/*
			 * Generate tones based on current bchan state
			 * (Recycle buffer content if succeeding channels share the
			 *  same state, this saves some cpu cycles)
			 */
			switch (ftdm_channel_get_state(chan)) {
			case FTDM_CHANNEL_STATE_DIALTONE:
				{
					ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(chan);

					/* check overlap dial timeout first before generating tone */
					if (data && data->digit_timeout && data->digit_timeout <= now) {
						if (strlen(caller_data->dnis.digits) > 0) {
							ftdm_log(FTDM_LOG_DEBUG, "Overlap dial timeout, advancing to RING state\n");
							ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_RING);
						} else {
							/* no digits received, hangup */
							ftdm_log(FTDM_LOG_DEBUG, "Overlap dial timeout, no digits received, going to HANGUP state\n");
							caller_data->hangup_cause = FTDM_CAUSE_RECOVERY_ON_TIMER_EXPIRE;	/* TODO: probably wrong cause value */
							ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
						}
						data->digit_timeout = 0;
						continue;
					}

					if (last_chan_state != ftdm_channel_get_state(chan)) {
						ftdm_buffer_zero(dt_buffer);
						teletone_run(&ts, span->tone_map[FTDM_TONEMAP_DIAL]);
						last_chan_state = ftdm_channel_get_state(chan);
					}
				}
				break;

			case FTDM_CHANNEL_STATE_RING:
				{
					if (last_chan_state != ftdm_channel_get_state(chan)) {
						ftdm_buffer_zero(dt_buffer);
						teletone_run(&ts, span->tone_map[FTDM_TONEMAP_RING]);
						last_chan_state = ftdm_channel_get_state(chan);
					}
				}
				break;

			default:	/* Not in a tone generating state, go to next round */
				continue;
			}

			if (!ftdm_test_flag(chan, FTDM_CHANNEL_OPEN)) {
				if (ftdm_channel_open_chan(chan) != FTDM_SUCCESS) {
					ftdm_set_state_locked(chan, FTDM_CHANNEL_STATE_HANGUP);
					continue;
				}
				ftdm_log(FTDM_LOG_NOTICE, "Successfully opened channel %d:%d\n",
						ftdm_channel_get_span_id(chan),
						ftdm_channel_get_id(chan));
			}

			flags = FTDM_READ;

			status = ftdm_channel_wait(chan, &flags, (gated) ? 0 : interval);
			switch (status) {
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

			status = ftdm_channel_read(chan, frame, &len);
			if (status != FTDM_SUCCESS || len <= 0) {
				continue;
			}

			/*
			 * Teletone operates on SLIN data (2 bytes per sample).
			 * Convert the length of non-SLIN codecs, so we read
			 * the right amount of samples from the buffer.
			 */
			if (chan->effective_codec != FTDM_CODEC_SLIN) {
				len *= 2;
			}

			/* seek to current offset */
			ftdm_buffer_seek(dt_buffer, data->offset);

			/*
			 * ftdm_channel_read() can read up to sizeof(frame) bytes
			 * (in certain situations). Avoid overflowing the stack (and smashing dt_buffer)
			 * if the codec is not slin and we had to double the length.
			 */
			len  = ftdm_min(len, sizeof(frame));
			rlen = ftdm_buffer_read_loop(dt_buffer, frame, len);

			if (chan->effective_codec != FTDM_CODEC_SLIN) {
				fio_codec_t codec_func = NULL;

				if (chan->native_codec == FTDM_CODEC_ULAW) {
					codec_func = fio_slin2ulaw;
				} else if (chan->native_codec == FTDM_CODEC_ALAW) {
					codec_func = fio_slin2alaw;
				}

				/*
				 * Convert SLIN to native format (a-law/u-law),
				 * input size is 2 bytes per sample, output size
				 * (after conversion) is one byte per sample
				 * (= max. half the input size).
				 */
				if (codec_func) {
					status = codec_func(frame, sizeof(frame), &rlen);
				} else {
					snprintf(chan->last_error, sizeof(chan->last_error), "codec error!");
					goto done;
				}
			}

			if (ftdm_channel_write(chan, frame, sizeof(frame), &rlen) == FTDM_SUCCESS) {
				/*
				 * Advance offset in teletone buffer by amount
				 * of data actually written to channel.
				 */
				if (chan->effective_codec != FTDM_CODEC_SLIN) {
					data->offset += rlen << 1;	/* Teletone buffer is in SLIN (= rlen * 2) */
				} else {
					data->offset += rlen;
				}

				/* Limit offset to [0..Rate(Samples/s)-1] in SLIN (2 bytes per sample) units. */
				data->offset %= (ts.rate << 1);
			}
		}

		/*
		 * sleep a bit if there was nothing to do
		 */
		if (!gated) {
			ftdm_sleep(interval);
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
	Q931Start(&isdn_data->q931);

	while (ftdm_running() && ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		ftdm_wait_flag_t flags = FTDM_READ;
		ftdm_status_t status = ftdm_channel_wait(isdn_data->dchan, &flags, 100);

		Q921TimerTick(&isdn_data->q921);
		Q931TimerTick(&isdn_data->q931);
		check_state(span);
		check_events(span);

		/*
		 *
		 */
		switch (status) {
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
				if (flags & FTDM_READ) {
					len = sizeof(frame);
					if (ftdm_channel_read(isdn_data->dchan, frame, &len) != FTDM_SUCCESS) {
						ftdm_log_chan_msg(isdn_data->dchan, FTDM_LOG_ERROR, "Failed to read from D-Channel\n");
						continue;
					}
					if (len > 0) {
#ifdef HAVE_PCAP
						if (isdn_pcap_capture_both(isdn_data)) {
							isdn_pcap_write(isdn_data, frame, len, ISDN_PCAP_INCOMING);
						}
#endif
						Q921QueueHDLCFrame(&isdn_data->q921, frame, (int)len);
						Q921Rx12(&isdn_data->q921);

						/* Successful read, reset error counter */
						errs = 0;
					}
				} else {
					ftdm_log(FTDM_LOG_DEBUG, "No Read FLAG!\n");
				}
			}
			break;
		}
	}

done:
	ftdm_channel_close(&isdn_data->dchan);
	ftdm_clear_flag(isdn_data, FTDM_ISDN_RUNNING);

#ifdef WIN32
	timeEndPeriod(1);
#endif
#ifdef HAVE_PCAP
	if (isdn_pcap_is_open(isdn_data)) {
		isdn_pcap_close(isdn_data);
	}
#endif
	ftdm_log(FTDM_LOG_DEBUG, "ISDN thread ended.\n");
	return NULL;
}

static int q931_rx_32(void *pvt, Q921DLMsg_t ind, L3UCHAR tei, L3UCHAR *msg, L3INT mlen)
{
	ftdm_span_t *span = pvt;
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	int offset = 4;
	char bb[4096] = "";

	switch(ind) {
	case Q921_DL_UNIT_DATA:
		offset = 3;

	case Q921_DL_DATA:
		print_hex_bytes(msg + offset, mlen - offset, bb, sizeof(bb));
		ftdm_log(FTDM_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)mlen - offset, LINE, bb);
		break;

	default:
		break;
	}

#ifdef HAVE_PCAP
	if (isdn_pcap_capture_l3only(isdn_data)) {
		isdn_pcap_write(isdn_data, msg, mlen, ISDN_PCAP_OUTGOING);
	}
#endif
	return Q921Rx32(&isdn_data->q921, ind, tei, msg, mlen);
}

static int ftdm_isdn_q921_log(void *pvt, Q921LogLevel_t level, char *msg, L2INT size)
{
	ftdm_span_t *span = (ftdm_span_t *) pvt;

	ftdm_log("Span", "Q.921", span->span_id, (int)level, "%s", msg);
	return 0;
}

static L3INT ftdm_isdn_q931_log(void *pvt, Q931LogLevel_t level, const char *msg, L3INT size)
{
	ftdm_span_t *span = (ftdm_span_t *) pvt;

	ftdm_log("Span", "Q.931", span->span_id, (int)level, "%s", msg);
	return 0;
}

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
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_DOWN, FTDM_END}
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
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_DOWN, FTDM_END}
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
 * \brief	Stop signalling on span
 */
static ftdm_status_t ftdm_isdn_stop(ftdm_span_t *span)
{
	ftdm_isdn_data_t *isdn_data = span->signal_data;

	if (!ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_set_flag(isdn_data, FTDM_ISDN_STOP);

	while (ftdm_test_flag(isdn_data, FTDM_ISDN_RUNNING)) {
		ftdm_sleep(100);
	}

	while (ftdm_test_flag(isdn_data, FTDM_ISDN_TONES_RUNNING)) {
		ftdm_sleep(100);
	}

	return FTDM_SUCCESS;
}

/**
 * \brief	Start signalling on span
 */
static ftdm_status_t ftdm_isdn_start(ftdm_span_t *span)
{
	ftdm_isdn_data_t *isdn_data = span->signal_data;
	ftdm_status_t ret;

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


/*###################################################################*
 * (text) value parsing / translation
 *###################################################################*/

static int32_t parse_loglevel(const char *level)
{
	if (!level)
		return -1;

	if (!strcasecmp(level, "debug")) {
		return FTDM_LOG_LEVEL_DEBUG;
	} else if (!strcasecmp(level, "info")) {
		return FTDM_LOG_LEVEL_INFO;
	} else if (!strcasecmp(level, "notice")) {
		return FTDM_LOG_LEVEL_NOTICE;
	} else if (!strcasecmp(level, "warning")) {
		return FTDM_LOG_LEVEL_WARNING;
	} else if (!strcasecmp(level, "error")) {
		return FTDM_LOG_LEVEL_ERROR;
	} else if (!strcasecmp(level, "alert")) {
		return FTDM_LOG_LEVEL_ALERT;
	} else if (!strcasecmp(level, "crit")) {
		return FTDM_LOG_LEVEL_CRIT;
	} else if (!strcasecmp(level, "emerg")) {
		return FTDM_LOG_LEVEL_EMERG;
	} else {
		return -1;
	}
}

static int parse_opts(const char *in, uint32_t *flags)
{
	if (!in || !flags)
		return -1;

	if (strstr(in, "suggest_channel")) {
		*flags |= FTDM_ISDN_OPT_SUGGEST_CHANNEL;
	}
	if (strstr(in, "omit_display")) {
		*flags |= FTDM_ISDN_OPT_OMIT_DISPLAY_IE;
	}
	if (strstr(in, "disable_tones")) {
		*flags |= FTDM_ISDN_OPT_DISABLE_TONES;
	}

	return 0;
}

static int parse_dialect(const char *in, uint32_t *dialect)
{
	if (!in || !dialect)
		return -1;

#if __UNSUPPORTED__
	if (!strcasecmp(in, "national")) {
		*dialect = Q931_Dialect_National;
		return 0;
	}
	if (!strcasecmp(in, "dms")) {
		*dialect = Q931_Dialect_DMS;
		return 0;
	}
#endif
	if (!strcasecmp(in, "5ess")) {
		*dialect = Q931_Dialect_5ESS;
		return 0;
	}
	if (!strcasecmp(in, "dss1") || !strcasecmp(in, "euroisdn")) {
		*dialect = Q931_Dialect_DSS1;
		return 0;
	}
	if (!strcasecmp(in, "q931")) {
		*dialect = Q931_Dialect_Q931;
		return 0;
	}

	return -1;
}


/*###################################################################*
 * API commands
 *###################################################################*/

static const char isdn_api_usage[] =
#ifdef HAVE_PCAP
	"isdn capture <span> <start> <filename> [q931only]\n"
	"isdn capture <span> <stop|suspend|resume>\n"
#endif
	"isdn loglevel <span> <q921|q931|all> <loglevel>\n"
	"isdn dump <span> calls\n"
	"isdn help";


/**
 * isdn_api
 */
static FIO_API_FUNCTION(isdn_api)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (data) {
		mycmd = strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc || !strcasecmp(argv[0], "help")) {
		stream->write_function(stream, "%s\n", isdn_api_usage);
		goto done;
	}
	else if (!strcasecmp(argv[0], "dump")) {
		ftdm_isdn_data_t *isdn_data = NULL;
		ftdm_span_t *span = NULL;
		int span_id = 0;

		/* dump <span> calls */

		if (argc < 3) {
			stream->write_function(stream, "-ERR not enough arguments.\n");
			goto done;
		}

		span_id = atoi(argv[1]);

		if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS || ftdm_span_find(span_id, &span) == FTDM_SUCCESS) {
			isdn_data = span->signal_data;
		} else {
			stream->write_function(stream, "-ERR invalid span.\n");
			goto done;
		}

		if (!strcasecmp(argv[2], "calls")) {
			/* dump all calls to log */
			Q931DumpAllCalls(&isdn_data->q931);
			stream->write_function(stream, "+OK call information dumped to log\n");
			goto done;
		}
	}
	else if (!strcasecmp(argv[0], "loglevel")) {
		ftdm_isdn_data_t *isdn_data = NULL;
		ftdm_span_t *span = NULL;
		int span_id = 0;
		int layer   = 0;
		int level   = 0;

		/* loglevel <span> <q921|q931|all> [level] */

		if (argc < 3) {
			stream->write_function(stream, "-ERR not enough arguments.\n");
			goto done;
		}

		span_id = atoi(argv[1]);

		if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS || ftdm_span_find(span_id, &span) == FTDM_SUCCESS) {
			isdn_data = span->signal_data;
		} else {
			stream->write_function(stream, "-ERR invalid span.\n");
			goto done;
		}

		if (!strcasecmp(argv[2], "q921")) {
			layer = 0x01;
		} else if (!strcasecmp(argv[2], "q931")) {
			layer = 0x02;
		} else if (!strcasecmp(argv[2], "all")) {
			layer = 0x03;
		} else {
			stream->write_function(stream, "-ERR invalid layer\n");
			goto done;
		}

		if (argc > 3) {
			/* set loglevel  */
			if ((level = parse_loglevel(argv[3])) < 0) {
				stream->write_function(stream, "-ERR invalid loglevel\n");
				goto done;
			}

			if (layer & 0x01) {	/* q921 */
				Q921SetLogLevel(&isdn_data->q921, (Q921LogLevel_t)level);
			}
			if (layer & 0x02) {	/* q931 */
				Q931SetLogLevel(&isdn_data->q931, (Q931LogLevel_t)level);
			}
			stream->write_function(stream, "+OK loglevel set");
		} else {
			/* get loglevel */
			if (layer & 0x01) {
				stream->write_function(stream, "Q.921 loglevel: %s\n",
					Q921GetLogLevelName(&isdn_data->q921));
			}
			if (layer & 0x02) {
				stream->write_function(stream, "Q.931 loglevel: %s\n",
					Q931GetLogLevelName(&isdn_data->q931));
			}
			stream->write_function(stream, "+OK");
		}
		goto done;
	}
#ifdef HAVE_PCAP
	else if (!strcasecmp(argv[0], "capture")) {
		ftdm_isdn_data_t *isdn_data = NULL;
		ftdm_span_t *span = NULL;
		int span_id = 0;

		/* capture <span> <start> <filename> [q931only] */
		/* capture <span> <stop|suspend|resume> */

		if (argc < 3) {
			stream->write_function(stream, "-ERR not enough arguments.\n");
			goto done;
		}

		span_id = atoi(argv[1]);

		if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS || ftdm_span_find(span_id, &span) == FTDM_SUCCESS) {
			isdn_data = span->signal_data;
		} else {
			stream->write_function(stream, "-ERR invalid span.\n");
			goto done;
		}

		if (!strcasecmp(argv[2], "start")) {
			char *filename = NULL;

			if (argc < 4) {
				stream->write_function(stream, "-ERR not enough parameters.\n");
				goto done;
			}

			if (isdn_pcap_is_open(isdn_data)) {
				stream->write_function(stream, "-ERR capture is already running.\n");
				goto done;
			}

			filename = argv[3];

			if (isdn_pcap_open(isdn_data, filename) != FTDM_SUCCESS) {
				stream->write_function(stream, "-ERR failed to open capture file.\n");
				goto done;
			}

			if (argc > 4 && !strcasecmp(argv[4], "q931only")) {
				isdn_data->flags |= FTDM_ISDN_CAPTURE_L3ONLY;
			}
			isdn_pcap_start(isdn_data);

			stream->write_function(stream, "+OK capture started.\n");
			goto done;
		}
		else if (!strcasecmp(argv[2], "stop")) {

			if (!isdn_pcap_is_open(isdn_data)) {
				stream->write_function(stream, "-ERR capture is not running.\n");
				goto done;
			}

			isdn_pcap_stop(isdn_data);
			isdn_pcap_close(isdn_data);

			stream->write_function(stream, "+OK capture stopped.\n");
			goto done;
		}
		else if (!strcasecmp(argv[2], "suspend")) {

			if (!isdn_pcap_is_open(isdn_data)) {
				stream->write_function(stream, "-ERR capture is not running.\n");
				goto done;
			}
			isdn_pcap_stop(isdn_data);

			stream->write_function(stream, "+OK capture suspended.\n");
			goto done;
		}
		else if (!strcasecmp(argv[2], "resume")) {

			if (!isdn_pcap_is_open(isdn_data)) {
				stream->write_function(stream, "-ERR capture is not running.\n");
				goto done;
			}
			isdn_pcap_start(isdn_data);

			stream->write_function(stream, "+OK capture resumed.\n");
			goto done;
		}
		else {
			stream->write_function(stream, "-ERR wrong action.\n");
			goto done;
		}
	}
#endif
	else {
		stream->write_function(stream, "-ERR invalid command.\n");
	}
done:
	ftdm_safe_free(mycmd);

	return FTDM_SUCCESS;
}

static int parse_mode(const char *mode)
{
	if (!mode)
		return -1;

	if (!strcasecmp(mode, "user") || !strcasecmp(mode, "cpe")) {
		return Q931_TE;
	}
	if (!strcasecmp(mode, "net") || !strcasecmp(mode, "network")) {
		return Q931_NT;
	}

	return -1;
}

static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(isdn_configure_span)
{
	Q931Dialect_t dialect = Q931_Dialect_National;
	ftdm_channel_t *dchan = NULL;
	ftdm_isdn_data_t *isdn_data;
	int32_t digit_timeout = 0;
	const char *tonemap = "us";
	int dchan_count = 0, bchan_count = 0;
	int q921loglevel = -1;
	int q931loglevel = -1;
	uint32_t i;

	if (span->signal_type) {
		ftdm_log(FTDM_LOG_ERROR, "Span is already configured for signalling [%d]\n", span->signal_type);
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling [%d]", span->signal_type);
		return FTDM_FAIL;
	}

	if (ftdm_span_get_trunk_type(span) >= FTDM_TRUNK_NONE) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid trunk type '%s' defaulting to T1\n", ftdm_span_get_trunk_type_str(span));
		span->trunk_type = FTDM_TRUNK_T1;
	}

	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

		switch (ftdm_channel_get_type(chan)) {
		case FTDM_CHAN_TYPE_DQ921:
			if (dchan_count > 1) {
				ftdm_log(FTDM_LOG_ERROR, "Span has more than 1 D-Channel!\n");
				snprintf(span->last_error, sizeof(span->last_error), "Span has more than 1 D-Channel!");
				return FTDM_FAIL;
			}

			if (ftdm_channel_open(ftdm_span_get_id(span), i, &dchan) == FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_DEBUG, "opening d-channel #%d %d:%d\n", dchan_count,
					ftdm_channel_get_span_id(dchan), ftdm_channel_get_id(dchan));
				dchan->state = FTDM_CHANNEL_STATE_UP;
			}

			dchan_count++;
			break;

		case FTDM_CHAN_TYPE_B:
			bchan_count++;
			break;

		default:
			break;
		}
	}
	if (!dchan_count) {
		ftdm_log(FTDM_LOG_ERROR, "Span has no D-Channel!\n");
		snprintf(span->last_error, sizeof(span->last_error), "Span has no D-Channel!");
		return FTDM_FAIL;
	}
	if (!bchan_count) {
		ftdm_log(FTDM_LOG_ERROR, "Span has no B-Channels!\n");
		snprintf(span->last_error, sizeof(span->last_error), "Span has no B-Channels!");
		return FTDM_FAIL;
	}

	isdn_data = malloc(sizeof(*isdn_data));
	assert(isdn_data != NULL);

	memset(isdn_data, 0, sizeof(*isdn_data));
	dialect = Q931_Dialect_Q931;

	/* Use trunk_mode span parameter to set default */
	switch (ftdm_span_get_trunk_mode(span)) {
	case FTDM_TRUNK_MODE_NET:
		ftdm_log(FTDM_LOG_INFO, "Span '%s' [s%d] defaulting to NET mode\n",
			ftdm_span_get_name(span), ftdm_span_get_id(span));
		isdn_data->mode = Q931_NT;
		break;
	default:
		ftdm_log(FTDM_LOG_INFO, "Span '%s' [s%d] defaulting to USER mode\n",
			ftdm_span_get_name(span), ftdm_span_get_id(span));
		isdn_data->mode = Q931_TE;
		break;
	}

	for (i = 0; ftdm_parameters[i].var; i++) {
		const char *var = ftdm_parameters[i].var;
		const char *val = ftdm_parameters[i].val;

		if (ftdm_strlen_zero(var)) {
			ftdm_log(FTDM_LOG_WARNING, "Skipping variable with no name\n");
			continue;
		}

		if (ftdm_strlen_zero(val)) {
			ftdm_log(FTDM_LOG_ERROR, "Variable '%s' has no value\n", var);
			return FTDM_FAIL;
		}

		if (!strcasecmp(var, "mode")) {
			if ((isdn_data->mode = parse_mode(val)) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid/unknown mode '%s'\n", val);
				snprintf(span->last_error, sizeof(span->last_error), "Invalid/unknown mode [%s]!", val);
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "dialect")) {
			if (parse_dialect(val, &dialect) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid/unknown dialect '%s'\n", val);
				snprintf(span->last_error, sizeof(span->last_error), "Invalid/unknown dialect [%s]!", val);
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "opts")) {
			if (parse_opts(val, &isdn_data->opts) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid/unknown options '%s'\n", val);
				snprintf(span->last_error, sizeof(span->last_error), "Invalid/unknown options [%s]!", val);
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "tonemap")) {
			tonemap = (const char *)val;
		} else if (!strcasecmp(var, "digit_timeout")) {
			digit_timeout = atoi(val);
			if (digit_timeout < 3000 || digit_timeout > 30000) {
				ftdm_log(FTDM_LOG_WARNING, "Digit timeout %d ms outside of range (3000 - 30000 ms), using default (10000 ms)\n", digit_timeout);
				digit_timeout = DEFAULT_DIGIT_TIMEOUT;
			}
		} else if (!strcasecmp(var, "q921loglevel")) {
			if ((q921loglevel = parse_loglevel(val)) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid/unknown loglevel '%s'\n", val);
				snprintf(span->last_error, sizeof(span->last_error), "Invalid/unknown loglevel [%s]!", val);
				return FTDM_FAIL;
			}
		} else if (!strcasecmp(var, "q931loglevel")) {
			if ((q931loglevel = parse_loglevel(val)) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid/unknown loglevel '%s'\n", val);
				snprintf(span->last_error, sizeof(span->last_error), "Invalid/unknown loglevel [%s]!", val);
				return FTDM_FAIL;
			}
		} else {
			ftdm_log(FTDM_LOG_ERROR, "Unknown parameter '%s'\n", var);
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			return FTDM_FAIL;
		}
	}

	if (!digit_timeout) {
		digit_timeout = DEFAULT_DIGIT_TIMEOUT;
	}

	/* Check if modes match and log a message if they do not. Just to be on the safe side. */
	if (isdn_data->mode == Q931_TE && ftdm_span_get_trunk_mode(span) == FTDM_TRUNK_MODE_NET) {
		ftdm_log(FTDM_LOG_WARNING, "Span '%s' signalling set up for TE/CPE/USER mode, while port is running in NT/NET mode. You may want to check your 'trunk_mode' settings.\n",
			ftdm_span_get_name(span));
	}
	else if (isdn_data->mode == Q931_NT && ftdm_span_get_trunk_mode(span) == FTDM_TRUNK_MODE_CPE) {
		ftdm_log(FTDM_LOG_WARNING, "Span '%s' signalling set up for NT/NET mode, while port is running in TE/CPE/USER mode. You may want to check your 'trunk_mode' settings.\n",
			ftdm_span_get_name(span));
	}

	/* allocate per b-chan data */
	if (isdn_data->mode == Q931_NT) {
		ftdm_isdn_bchan_data_t *data;

		data = malloc(bchan_count * sizeof(ftdm_isdn_bchan_data_t));
		if (!data) {
			return FTDM_FAIL;
		}

		for (i = 1; i <= ftdm_span_get_chan_count(span); i++, data++) {
			ftdm_channel_t *chan = ftdm_span_get_channel(span, i);

			if (ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B) {
				chan->call_data = data;
				memset(data, 0, sizeof(ftdm_isdn_bchan_data_t));
			}
		}
	}

	isdn_data->dchan = dchan;
	isdn_data->digit_timeout = digit_timeout;

	Q921_InitTrunk(&isdn_data->q921,
				   0,
				   0,
				   isdn_data->mode,
				   (ftdm_span_get_trunk_type(span) == FTDM_TRUNK_BRI_PTMP) ? Q921_PTMP : Q921_PTP,
				   0,
				   ftdm_isdn_921_21,
				   (Q921Tx23CB_t)ftdm_isdn_921_23,
				   span,
				   span);

	Q921SetLogCB(&isdn_data->q921, &ftdm_isdn_q921_log, span);
	Q921SetLogLevel(&isdn_data->q921, (Q921LogLevel_t)q921loglevel);

	Q931InitTrunk(&isdn_data->q931,
					  dialect,
					  isdn_data->mode,
					  span->trunk_type,
					  ftdm_isdn_931_34,
					  (Q931Tx32CB_t)q931_rx_32,
					  ftdm_isdn_931_err,
					  span,
					  span);

	Q931SetLogCB(&isdn_data->q931, &ftdm_isdn_q931_log, span);
	Q931SetLogLevel(&isdn_data->q931, (Q931LogLevel_t)q931loglevel);

	/* Register new event hander CB */
	Q931SetCallEventCB(&isdn_data->q931, ftdm_isdn_call_event, span);

	/* TODO: hmm, maybe drop the "Trunk" prefix */
	Q931TrunkSetAutoRestartAck(&isdn_data->q931, 1);
	Q931TrunkSetAutoConnectAck(&isdn_data->q931, 1);
	Q931TrunkSetAutoServiceAck(&isdn_data->q931, 1);
	Q931TrunkSetStatusEnquiry(&isdn_data->q931, 0);

	span->state_map     = &isdn_state_map;
	span->signal_data   = isdn_data;
	span->signal_type   = FTDM_SIGTYPE_ISDN;
	span->signal_cb     = sig_cb;
	span->start         = ftdm_isdn_start;
	span->stop          = ftdm_isdn_stop;
	span->outgoing_call = isdn_outgoing_call;

	span->get_channel_sig_status = isdn_get_channel_sig_status;
	span->get_span_sig_status    = isdn_get_span_sig_status;

#ifdef __TODO__
	if ((isdn_data->opts & FTDM_ISDN_OPT_SUGGEST_CHANNEL)) {
		span->channel_request = isdn_channel_request;
		span->flags |= FTDM_SPAN_SUGGEST_CHAN_ID;
	}
#endif
	ftdm_span_load_tones(span, tonemap);

	return FTDM_SUCCESS;
}

/**
 * ISDN module io interface
 * \note	This is really ugly...
 */
static ftdm_io_interface_t isdn_interface = {
	.name = "isdn",
	.api  = isdn_api
};

/**
 * \brief	ISDN module io interface init callback
 */
static FIO_IO_LOAD_FUNCTION(isdn_io_load)
{
        assert(fio != NULL);

        *fio = &isdn_interface;

        return FTDM_SUCCESS;
}

/**
 * \brief	ISDN module load callback
 */
static FIO_SIG_LOAD_FUNCTION(isdn_load)
{
	Q931Initialize();

	Q921SetGetTimeCB(ftdm_time_now);
	Q931SetGetTimeCB(ftdm_time_now);

	return FTDM_SUCCESS;
}

/**
 * \brief	ISDN module shutdown callback
 */
static FIO_SIG_UNLOAD_FUNCTION(isdn_unload)
{
	return FTDM_SUCCESS;
};

ftdm_module_t ftdm_module = {
	.name          = "isdn",
	.io_load       = isdn_io_load,
	.io_unload     = NULL,
	.sig_load      = isdn_load,
	.sig_unload    = isdn_unload,
	.configure_span_signaling = isdn_configure_span
};

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
