/*****************************************************************************

  FileName:     q921.c

  Description:  Contains the implementation of a Q.921 protocol

  Created:      27.dec.2000/JVB

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are 
  met:

    * Redistributions of source code must retain the above copyright notice, 
	  this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, 
	  this list of conditions and the following disclaimer in the documentation 
	  and/or other materials provided with the distribution.
    * Neither the name of the Case Labs, Ltd nor the names of its contributors 
	  may be used to endorse or promote products derived from this software 
	  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
  POSSIBILITY OF SUCH DAMAGE.

*****************************************************************************/

/****************************************************************************
 * Changes:
 *
 * - June-August 2008: Stefan Knoblich <s.knoblich@axsentis.de>:
 *     Add PTMP TEI management (NT + TE mode)
 *     Add timers
 *     Add retransmit counters
 *     Add logging
 *     Various cleanups
 *     Queues, retransmission of I frames
 *     PTMP NT mode
 *
 *
 * TODO:
 *
 * - Cleanup queueing, test retransmission
 *
 * - Q921Start() /-Stop() TEI acquire + release
 *   (move everything related into these functions)
 *
 * - Q.921 '97 Appendix I (and maybe III, IV)
 *
 * - More complete Appendix II
 *
 * - Test PTP mode
 *
 * - PTMP NT mode (in progress)
 *
 * - NT mode TEI management: (ab)use T202 for TEI Check Request retransmission
 *
 * - General cleanup (move all non-public declarations into private header file)
 *
 * - Statistics, per-Frame type debug message filter
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "freetdm.h"
#include "Q921.h"
#include "Q921priv.h"
#include "mfifo.h"

#ifdef WIN32
#pragma warning(disable:4100 4244)
#endif

/******************************************************************************************************
 * Actual code below this line
 ******************************************************************************************************/


/**
 * Q921StateNames
 * \brief	Static array of state name / value mappings
 */
static struct Q921StateName {
	Q921State_t value;
	const char *name;
} Q921StateNames[10] = {
	{ Q921_STATE_STOPPED, "Stopped" },
	{ Q921_STATE_TEI_UNASSIGNED, "TEI Unassigned" },
	{ Q921_STATE_TEI_AWAITING, "TEI Awaiting Assignment" },
	{ Q921_STATE_TEI_ESTABLISH, "TEI Awaiting Establishment" },
	{ Q921_STATE_TEI_ASSIGNED, "TEI Assigned" },
	{ Q921_STATE_AWAITING_ESTABLISHMENT, "Awaiting Establishment" },
	{ Q921_STATE_AWAITING_RELEASE, "Awaiting Release" },
	{ Q921_STATE_MULTIPLE_FRAME_ESTABLISHED, "Multiple Frame Mode Established" },
	{ Q921_STATE_TIMER_RECOVERY, "Timer Recovery" },
	{ 0, 0 }
};

/**
 * Q921State2Name
 * \brief	Convert state value to name
 * \param[in]	state	the state value
 * \return	the state name or "Unknown"
 *
 * \author	Stefan Knoblich
 */
static const char *Q921State2Name(Q921State_t state)
{
	struct Q921StateName *p = Q921StateNames;

	while(p->name) {
		if(p->value == state)
			return p->name;
		p++;
	}

	return "Unknown";
}


/**
 * Q921SendEnquiry
 */
static int Q921SendEnquiry(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	/* send enquiry: begin */
	if(Q921_CHECK_FLAG(link, Q921_FLAG_RECV_BUSY)) {

		Q921SendRNR(trunk, trunk->sapi, Q921_COMMAND(trunk), tei, 1);
	}
	else {
		Q921SendRR(trunk, trunk->sapi, Q921_COMMAND(trunk), tei, 1);
	}

	/* clear acknowledge pending */
	Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);

	/* "Start" T200 */
	Q921T200TimerReset(trunk, tei);

	/* send enquiry: end */
	return 1;
}

/**
 * Q921SendEnquiryResponse
 */
static int Q921SendEnquiryResponse(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	/* send enquiry: begin */
	if(Q921_CHECK_FLAG(link, Q921_FLAG_RECV_BUSY)) {

		Q921SendRNR(trunk, trunk->sapi, Q921_RESPONSE(trunk), tei, 1);
	}
	else {
		Q921SendRR(trunk, trunk->sapi, Q921_RESPONSE(trunk), tei, 1);

		/* clear acknowledge pending */
		Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
	}
	/* send enquiry: end */
	return 1;
}

/**
 * Q921ResetExceptionConditions
 * \brief	Reset Q.921 Exception conditions procedure
 * \param	trunk	Q.921 data structure
 * \param	tei	TEI
 * \todo	Do something
 */
static void Q921ResetExceptionConditions(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	/* Clear peer receiver busy */
	Q921_CLEAR_FLAG(link, Q921_FLAG_PEER_RECV_BUSY);

	/* Clear reject exception */
	Q921_CLEAR_FLAG(link, Q921_FLAG_REJECT);

	/* Clear own receiver busy */
	Q921_CLEAR_FLAG(link, Q921_FLAG_RECV_BUSY);

	/* Clear acknowledge pending */
	Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);

	return;
}

/**
 * Q921EstablishDataLink
 * \brief	Q.921 Establish data link procedure
 * \param	trunk	Q.921 data structure
 * \param	tei	TEI
 * \return	always 1 (success)
 */
static int Q921EstablishDataLink(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	/* reset exception conditions */
	Q921ResetExceptionConditions(trunk, tei);

	/* RC = 0 */
	link->N200 = 0;

	/* Send SABME */
	Q921SendSABME(trunk, trunk->sapi, Q921_COMMAND(trunk), tei, 1);

	/* Restart T200, stop T203 */
	Q921T200TimerReset(trunk, tei);
	Q921T203TimerStop(trunk, tei);

	return 1;
}

/**
 * Q921NrErrorRecovery
 * \brief	NR(R) Error recovery procedure
 * \param	trunk	Q.921 data structure
 * \param	tei	TEI
 * \return	always 1 (success)
 */
static int Q921NrErrorRecovery(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	/* MDL Error indication (J) */

	/* Establish datalink */
	Q921EstablishDataLink(trunk, tei);

	/* Clear L3 initiated */
	Q921_CLEAR_FLAG(link, Q921_FLAG_L3_INITIATED);

	return 1;
}


/**
 * Q921InvokeRetransmission
 * \brief	I Frame retransmission procedure
 * \param	trunk	Q.921 data structure
 * \param	tei	TEI
 * \param	nr	N(R) for retransmission
 * \return	always 1 (success)
 */
static int Q921InvokeRetransmission(L2TRUNK trunk, L2UCHAR tei, L2UCHAR nr)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);
	L2UCHAR *mes;
	L2INT qpos, qnum, size = 0;

	qnum = MFIFOGetMesCount(link->IFrameResendQueue);
	qpos = qnum - 1;

	/*
	 * slightly different than what is shown in the spec
	 * (Q.921 '97 Annex B, Figure B.9, page 104)
	 * 
	 * what the above mentioned figure probably means is:
	 * "as long as V(S) != N(R), move the pointer marking
	 *  the first frame to start resending at to the previous
	 *  frame"
	 *
	 * if we actually implemented it as shown in the figure, we'd be
	 * resending frames in the wrong order (moving backwards in time)
	 * meaning we'd have to add an incoming queue to reorder the frames
	 *
	 */
	/*
	 * TODO: There's a "traditional" off-by-one error hidden in the original
	 *       mfifo implementation + it's late, i'm tired and being lazy,
	 *       so i'll probably have added another one :P
	 *
	 *       wow, the first while loop sucks and can be removed
	 */
	while(link->vs != nr && qpos > 0) {	/* ???? */
		/* V(S) = V(S) - 1 */
		Q921_DEC_COUNTER(link->vs);	/* huh? backwards? */

		/* next frame in queue (backtrack along I queue) ??? */
		qpos--;
	}

	/*
	 * being lazy and trying to avoid mod 128 math this way...
	 */
	if(link->vs != nr && !qpos) {
		/* fatal, we don't have enough history to resend all missing frames */
		/* TODO: how to handle this? */
	}

	/*
	 * resend frames in correct order (oldest missing frame first,
	 * contrary to what the spec figure shows)
	 */
	while(qpos < qnum) {
		/* Grab frame's buffer ptr and size from queue */
		mes = MFIFOGetMesPtrOffset(link->IFrameResendQueue, &size, qpos);
		if(mes) {
			/* requeue frame (TODO: check queue full condition) */
			MFIFOWriteMes(link->IFrameQueue, mes, size);

			/* set I frame queued */
		}

		qpos++;
	}

	return 1;
}


static int Q921AcknowledgePending(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
	case Q921_STATE_TIMER_RECOVERY:
		if(Q921_CHECK_FLAG(link, Q921_FLAG_ACK_PENDING)) {
			/* clear acknowledge pending */
			Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);

			/* send RR */
			Q921SendRR(trunk, trunk->sapi, Q921_COMMAND(trunk), tei, 0);

			return 1;
		}
		break;

	default:
		break;
	}

	return 0;
}

/*****************************************************************************

  Function:     Q921_InitTrunk

  Decription:   Initialize a Q.921 trunk so it is ready for use. This 
                function MUST be called before you call any other functions.

*****************************************************************************/
int Q921_InitTrunk(L2TRUNK trunk,
					L2UCHAR sapi,
					L2UCHAR tei,
					Q921NetUser_t NetUser,
					Q921NetType_t NetType,
					L2INT hsize,
					Q921Tx21CB_t cb21,
					Q921Tx23CB_t cb23,
					void *priv21,
					void *priv23)
{
	int numlinks = 0;

	trunk->sapi = sapi;
	trunk->tei = tei;
	trunk->NetUser = NetUser;
	trunk->NetType = NetType;
	trunk->Q921Tx21Proc = cb21;
	trunk->Q921Tx23Proc = cb23;
	trunk->PrivateData21 = priv21;
	trunk->PrivateData23 = priv23;
	trunk->Q921HeaderSpace = hsize;

	numlinks = Q921_IS_PTMP_NT(trunk) ? Q921_TEI_MAX : 1;

	if (trunk->initialized != INITIALIZED_MAGIC) {
		MFIFOCreate(trunk->HDLCInQueue, Q921MAXHDLCSPACE, 10);

		/*
		 * Allocate space for per-link context(s)
		 */
		trunk->context = ftdm_malloc(numlinks * sizeof(struct Q921_Link));
		if(!trunk->context)
			return -1;

		trunk->initialized = INITIALIZED_MAGIC;
	}

	/* timeout default values */
	trunk->T200Timeout = 1000;	/*   1 second  */
	trunk->T203Timeout = 10000;	/*  10 seconds */
	trunk->T202Timeout = 2000;	/*   2 seconds */
	trunk->T201Timeout = 200000;	/* 200 seconds */
	trunk->TM01Timeout = 10000;	/*  10 seconds */

	/* octet / retransmit counter default limits */
	trunk->N200Limit   = 3;		/*   3 retransmits */
	trunk->N201Limit   = 260;	/* 260 octets      */
	trunk->N202Limit   = 3;		/*   3 retransmits */
	trunk->k           = 7;		/*   7 outstanding ACKs */

	/* reset counters, timers, etc. */
	trunk->T202 = 0;
	trunk->N202 = 0;

	/* Reset per-link contexts */
	memset(trunk->context, 0, numlinks * sizeof(struct Q921_Link));

	/* clear tei map */
	memset(trunk->tei_map, 0, Q921_TEI_MAX + 1);

	if(Q921_IS_PTMP(trunk)) {
		/*
		 * We're either the Network side (NT, TEI = 0)
		 * or user-side equipment (TE) which will get it's TEI via
		 * dynamic assignment
		 */
		trunk->tei = 0;
	}

	return 0;
}


/**
 * Q921Tx21Proc
 * \brief	Submit frame to layer 1 (for sending)
 * \param[in]	trunk	Pointer to trunk struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 */
static int Q921Tx21Proc(L2TRUNK trunk, L2UCHAR *Msg, L2INT size)
{
	Q921LogMesg(trunk, Q921_LOG_DEBUG, 0, Msg, size, "Sending frame");

	return trunk->Q921Tx21Proc(trunk->PrivateData21, Msg, size);
}


/**
 * Q921Tx23Proc
 * \brief	Submit frame to layer 3
 * \param[in]	trunk	Pointer to trunk struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 */
static int Q921Tx23Proc(L2TRUNK trunk, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR *Msg, L2INT size)
{
	return trunk->Q921Tx23Proc(trunk->PrivateData23, ind, tei, Msg, size);
}


/**
 * Q921LogProc
 * \brief	Used for logging, converts to string and submits to higher level log function via callback
 * \param[in]	trunk	Pointer to trunk struct
 * \param[in]	level	Q921 Loglevel
 * \param[in]	fmt	format of logmessage
 * \return	>= 0 on success, < 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921Log(L2TRUNK trunk, Q921LogLevel_t level, const char *fmt, ...)
{
	char  buf[Q921_LOGBUFSIZE];
	L2INT len;
	va_list ap;

	if(!trunk->Q921LogProc)
		return 0;

	if(trunk->loglevel < level)
		return 0;

	va_start(ap, fmt);

	len = vsnprintf(buf, sizeof(buf)-1, fmt, ap);
	if(len <= 0) {
		/* TODO: error handling */
		return -1;
	}
	if(len >= sizeof(buf))
		len = sizeof(buf) - 1;

	buf[len] = '\0';

	va_end(ap);

	return trunk->Q921LogProc(trunk->PrivateDataLog, level, buf, len);
}


static int print_hex(char *buf, int bsize, const unsigned char *in, const int len)
{
	static const char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	int offset = 0;
	int pos    = 0;
	int nr     = 0;

	buf[pos++] = '[';
	bsize -= 3;

	while((bsize - pos) > 0 && offset < len) {
		buf[pos++] = hex[(in[offset] & 0xF0) >> 4];
		buf[pos++] = hex[(in[offset++]   & 0x0F)];

		if(++nr == 32 && offset < len && (bsize - pos) > 3) {
			nr = 0;
			buf[pos++] = ']';
			buf[pos++] = '\n';
			buf[pos++] = '[';
		}
		else if(offset < len) {
			buf[pos++] = ' ';
		}
	}

	buf[pos++] = ']';
	buf[pos++] = '\n';
	buf[pos]   = '\0';

	return pos;
}

#define APPEND_MSG(buf, off, lef, fmt, ...) 			\
	len = snprintf(buf + off, lef, fmt, ##__VA_ARGS__);	\
	if(len > 0) { 						\
		off += len;					\
		lef -= len;					\
	} else {						\
		goto out;					\
	}

/**
 * Q921LogProcMesg
 * \brief	Used for logging, converts to string and submits to higher level log function via callback
 * \param[in]	trunk	Pointer to trunk struct
 * \param[in]	level	Q921 Loglevel
 * \param[in]	received	direction of the message (received = 1, sending = 0)
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \param[in]	fmt	format of logmessage
 * \return	>= 0 on success, < 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921LogMesg(L2TRUNK trunk, Q921LogLevel_t level, L2UCHAR received, L2UCHAR *mes, L2INT size, const char *fmt, ...)
{
	char  buf[Q921_LOGBUFSIZE];
	size_t len, left;
	va_list ap;

	if(!trunk->Q921LogProc)
		return 0;

	if(trunk->loglevel < level)
		return 0;

	if(!mes)
		return 0;

	memset(buf, 0, sizeof(buf));

	left = sizeof(buf) - 1;

	va_start(ap, fmt);

	len = vsnprintf(buf, left, fmt, ap);
	if(len > 0)
		left -= len;
	else {
		/* TODO: error handling */
		return -1;
	}

	va_end(ap);

	if(trunk->loglevel == Q921_LOG_DEBUG) {
		char pbuf[1024];
		size_t pleft, poffset;
		L2UCHAR sapi, tei, cr;
		L2UCHAR *pmes = mes + trunk->Q921HeaderSpace;
		struct Q921_Link *link;

		memset(pbuf, 0, sizeof(pbuf));

		pleft   = sizeof(pbuf);
		poffset = 0;

		/*
		 * Decode packet
		 */
		sapi = (pmes[0] & 0xfc) >> 2;
		cr   = (pmes[0] & 0x02) >> 1;
		tei  = (pmes[1] & 0xfe) >> 1;
		link  = Q921_LINK_CONTEXT(trunk, tei);

		/* make cr actually useful */
		cr   = (received) ? Q921_IS_COMMAND(trunk, cr) : Q921_IS_RESPONSE(trunk, cr);

		/* filter */
		if((pmes[2] & 0x01) == 0x00) {
 			;
		}
		else if((pmes[2] & 0x03) == 0x01) {
			; //return 0;
		}
		else if((pmes[2] & 0x03) == 0x03) {
			;
		}

		APPEND_MSG(pbuf, poffset, pleft, "\n----------------- Q.921 Packet [%s%s] ---------------\n", received ? "Incoming" : "Outgoing",
						(tei == link->tei || tei == Q921_TEI_BCAST) ? "" : ", Ignored" );

		/* common header */
		APPEND_MSG(pbuf, poffset, pleft, "    SAPI: %u, TEI: %u, C/R: %s (%d)\n\n", sapi, tei, (cr) ? "Command" : "Response", (mes[0] & 0x02) >> 1 );

		/*
		 * message specific
		 */
		if((pmes[2] & 0x01) == 0x00) {
			/*
			 * I frame
			 */
			L2UCHAR pf = pmes[3] & 0x01;	/* poll / final flag */
			L2UCHAR nr = pmes[3] >> 1;	/* receive sequence number */
			L2UCHAR ns = pmes[2] >> 1;	/* send sequence number */

			APPEND_MSG(pbuf, poffset, pleft, "    Type: I Frame\n          P/F: %d, N(S): %d, N(R): %d  [V(A): %d, V(R): %d, V(S): %d]\n", pf, ns, nr,
														link->va, link->vr, link->vs);

			/* Dump content of I Frames for foreign TEIs */
			if(tei != link->tei) {
				APPEND_MSG(pbuf, poffset, pleft, "    CONTENT:\n");

				len = print_hex(pbuf + poffset, (int)pleft, &pmes[4], size - (trunk->Q921HeaderSpace + 4));
				poffset += len;
				pleft   -= len;
			}
		}
		else if((pmes[2] & 0x03) == 0x01) {
			/*
			 * S frame
			 */
			L2UCHAR sv = (pmes[2] & 0x0c) >> 2;	/* supervisory format id */
			L2UCHAR pf =  pmes[3] & 0x01;		/* poll / final flag */
			L2UCHAR nr =  pmes[3] >> 1;		/* receive sequence number */
			const char *type;

			switch(sv) {
			case 0x00:	/* RR : Receive Ready */
				type = "RR (Receive Ready)";
				break;

			case 0x02:	/* RNR : Receive Not Ready */
				type = "RNR (Receiver Not Ready)";
				break;

			case 0x04:	/* REJ : Reject */
				type = "REJ (Reject)";
				break;

			default:	/* Invalid / Unknown */
				type = "Unknown";
				break;
			}

			APPEND_MSG(pbuf, poffset, pleft, "    Type: S Frame, SV: %s\n          P/F: %d, N(R): %d  [V(A): %d, V(R): %d, V(S): %d]\n", type, pf, nr,
														link->va, link->vr, link->vs);
		}
		else if((pmes[2] & 0x03) == 0x03) {
			/*
			 * U frame
			 */
			L2UCHAR m  = (pmes[2] & 0xe0) >> 3 | (pmes[2] & 0x0c) >> 2;	/* modifier function id */
			L2UCHAR pf = (pmes[2] & 0x10) >> 4;				/* poll / final flag */
			const char *type;

			switch(m) {
			case 0x00:
				type = "UI (Unnumbered Information)";
				break;

			case 0x03:
				type = "DM (Disconnected Mode)";
				break;

			case 0x08:
				type = "DISC (Disconnect)";
				break;

			case 0x0c:
				type = "UA (Unnumbered Acknowledgement)";
				break;

			case 0x0f:
				type = "SABME";
				break;

			case 0x11:
				type = "FRMR (Frame Reject)";
				break;

			case 0x17:
				type = "XID (Exchange Identification)";
				break;

			default:
				type = "Unknown";
			}


			APPEND_MSG(pbuf, poffset, pleft, "    Type: U Frame (%s)\n          P/F: %d\n", type, pf);

			if(m == 0x00) {
				switch(pmes[3]) {
				case Q921_LAYER_ENT_ID_TEI:
					type = "TEI Mgmt";
					break;

				case Q921_LAYER_ENT_ID_Q931:
					type = "Q.931";
					break;

				default:
					type = "Unknown";
				}

				if(pmes[3] == Q921_LAYER_ENT_ID_TEI) {
					const char *command = "";

					switch(pmes[6]) {
					case Q921_TEI_ID_REQUEST:
						command = "Request";
						break;
					case Q921_TEI_ID_VERIFY:
						command = "Verify";
						break;
					case Q921_TEI_ID_CHECKREQ:
						command = "Check req";
						break;
					case Q921_TEI_ID_CHECKRESP:
						command = "Check resp";
						break;
					case Q921_TEI_ID_REMOVE:
						command = "Remove";
						break;
					case Q921_TEI_ID_ASSIGNED:
						command = "Assign";
						break;
					case Q921_TEI_ID_DENIED:
						command = "Denied";
						break;
					}
					APPEND_MSG(pbuf, poffset, pleft, "    ENT ID: %d (%s), COMMAND: %d (%s), RI: %#x, AI: %d\n",
							 pmes[3], type, pmes[6], command, (int)((pmes[4] << 8) | pmes[5]), pmes[7] >> 1);
				}
				else {
					APPEND_MSG(pbuf, poffset, pleft, "    ENT ID: %d (%s), MESSAGE CONTENT:\n", pmes[3], type);

					len = print_hex(pbuf + poffset, (int)pleft, &pmes[3], size - (trunk->Q921HeaderSpace + 3));
					poffset += len;
					pleft   -= len;
				}
			}
		}	
		else {
			/*
			 * Unknown
			 */
			strncat(pbuf + poffset, "  -- unknown frame type --\n", pleft);

			len = (sizeof(pbuf) - poffset) - strlen(pbuf + poffset);
			if(len > 0) {
				poffset += len;
				pleft   -= len;
			} else
				goto out;
		}

		APPEND_MSG(pbuf, poffset, pleft, "\n    Q.921 state: \"%s\" (%d) [flags: %c%c%c%c]\n", Q921State2Name(link->state), link->state,
											Q921_CHECK_FLAG(link, Q921_FLAG_ACK_PENDING) ? 'A' : '-',
											Q921_CHECK_FLAG(link, Q921_FLAG_REJECT) ? 'R' : '-',
											Q921_CHECK_FLAG(link, Q921_FLAG_PEER_RECV_BUSY) ? 'P' : '-',
											Q921_CHECK_FLAG(link, Q921_FLAG_RECV_BUSY) ? 'B' : '-');

		strncat(pbuf + poffset, "----------------------------------------------\n\n", pleft);

		len = (sizeof(pbuf) - poffset) - strlen(pbuf + poffset);
		if(len > 0) {
			poffset += len;
			pleft   -= len;
		} else
			goto out;


		/* concat buffers together */
		len = strlen(pbuf);
		if(len <= left)
			strncat(buf, pbuf, left);
		else
			strncat(buf, "-- packet truncated --\n", left);
	}

out:
	buf[sizeof(buf) - 1] = '\0';

	return trunk->Q921LogProc(trunk->PrivateDataLog, level, buf, (int)strlen(buf));
}

/*****************************************************************************

  Function:     Q921TimeTick

  Description:  Called periodically from an external source to allow the 
                stack to process and maintain it's own timers.

  Return Value: none

*****************************************************************************/
static L2ULONG (*Q921GetTimeProc) (void) = NULL; /* callback for func reading time in ms */
static L2ULONG tLast = {0};

static L2ULONG Q921GetTime(void)
{
	L2ULONG tNow = 0;

	if(Q921GetTimeProc)
	{
		tNow = Q921GetTimeProc();
		if(tNow < tLast)	/* wrapped */
		{
			/* TODO */
		}
		tLast = tNow;
	}
	return tNow;
}

/*
 * T200 handling (per-TEI in PTMP NT mode, tei=0 otherwise)
 */
static void Q921T200TimerStart(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	if (!link->T200) {
		link->T200 = Q921GetTime() + trunk->T200Timeout;

		Q921Log(trunk, Q921_LOG_DEBUG, "T200 (timeout: %d msecs) started for TEI %d\n", trunk->T200Timeout, tei);
	}
}

static void Q921T200TimerStop(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->T200 = 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "T200 stopped for TEI %d\n", tei);
}

static void Q921T200TimerReset(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->T200 = Q921GetTime() + trunk->T200Timeout;

	Q921Log(trunk, Q921_LOG_DEBUG, "T200 (timeout: %d msecs) restarted for TEI %d\n", trunk->T200Timeout, tei);
}

/*
 * T203 handling (per-TEI in PTMP NT mode, tei=0 otherwise)
 */
static void Q921T203TimerStart(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	if (!link->T203) {
		link->T203 = Q921GetTime() + trunk->T203Timeout;

		Q921Log(trunk, Q921_LOG_DEBUG, "T203 (timeout: %d msecs) started for TEI %d\n", trunk->T203Timeout, tei);
	}
}

static void Q921T203TimerStop(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->T203 = 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "T203 stopped for TEI %d\n", tei);
}

static void Q921T203TimerReset(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->T203 = Q921GetTime() + trunk->T203Timeout;

	Q921Log(trunk, Q921_LOG_DEBUG, "T203 (timeout: %d msecs) restarted for TEI %d\n", trunk->T203Timeout, tei);
}

/*
 * T202 handling (TEI message timeout, TE mode only)
 */
static void Q921T202TimerStart(L2TRUNK trunk)
{
	if (!trunk->T202) {
		trunk->T202 = Q921GetTime() + trunk->T202Timeout;

		Q921Log(trunk, Q921_LOG_DEBUG, "T202 (timeout: %d msecs) started\n", trunk->T202Timeout);
	}
}

static void Q921T202TimerStop(L2TRUNK trunk)
{
	trunk->T202 = 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "T202 stopped\n");
}

static void Q921T202TimerReset(L2TRUNK trunk)
{
	trunk->T202 = Q921GetTime() + trunk->T202Timeout;

	Q921Log(trunk, Q921_LOG_DEBUG, "T202 (timeout: %d msecs) restarted\n", trunk->T202Timeout);
}

/*
 * T201 handling (TEI management (NT side), per-TEI)
 */
static void Q921T201TimerStart(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	if (!link->T201) {
		link->T201 = Q921GetTime() + trunk->T201Timeout;

		Q921Log(trunk, Q921_LOG_DEBUG, "T201 (timeout: %d msecs) started for TEI %d\n", trunk->T201Timeout, tei);
	}	
}

static void Q921T201TimerStop(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->T201 = 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "T201 stopped for TEI %d\n", tei);
}

#ifdef __UNUSED_FOR_NOW__
static void Q921T201TimerReset(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->T201 = Q921GetTime() + trunk->T201Timeout;

	Q921Log(trunk, Q921_LOG_DEBUG, "T201 (timeout: %d msecs) restarted for TEI %d\n", trunk->T201Timeout, tei);
}
#endif

/*
 * TM01 handling (Datalink inactivity shutdown timer)
 */
static void Q921TM01TimerStart(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	if (!link->TM01) {
		link->TM01 = Q921GetTime() + trunk->TM01Timeout;

		Q921Log(trunk, Q921_LOG_DEBUG, "TM01 (timeout: %d msecs) started for TEI %d\n", trunk->TM01Timeout, tei);
	}
}

#ifdef __UNUSED_FOR_NOW__
static void Q921TM01TimerStop(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->TM01 = 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "TM01 stopped for TEI %d\n", tei);
}
#endif

static void Q921TM01TimerReset(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	link->TM01 = Q921GetTime() + trunk->TM01Timeout;

	Q921Log(trunk, Q921_LOG_DEBUG, "TM01 (timeout: %d msecs) restarted for TEI %d\n", trunk->TM01Timeout, tei);
}

/*
 * Expiry callbacks
 */
static void Q921T200TimerExpire(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link   = Q921_LINK_CONTEXT(trunk, tei);
	struct Q921_Link *trlink = Q921_TRUNK_CONTEXT(trunk);

	Q921Log(trunk, Q921_LOG_DEBUG, "T200 expired for TEI %d (trunk TEI %d)\n", tei, trlink->tei);

	/* Stop timer first */
	Q921T200TimerStop(trunk, tei);

	switch(link->state) {
	case Q921_STATE_AWAITING_ESTABLISHMENT:
		if(link->N200 >= trunk->N200Limit) {
			/* Discard I queue */
			MFIFOClear(link->IFrameQueue);

			/* MDL-Error indication (G) */
			Q921Log(trunk, Q921_LOG_ERROR, "Failed to establish Q.921 link in %d retries\n", link->N200);

			/* DL-Release indication */
			Q921Tx23Proc(trunk, Q921_DL_RELEASE, tei, NULL, 0);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_TEI_ASSIGNED, tei);
		} else {
			/* Increment retry counter */
			link->N200++;

			/* Send SABME */
			Q921SendSABME(trunk,
					trunk->sapi,
					Q921_COMMAND(trunk),
					tei,
					1);

			/* Start T200 */
			Q921T200TimerStart(trunk, tei);
		}
		break;

	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		link->N200 = 0;

		if(!Q921_CHECK_FLAG(link, Q921_FLAG_PEER_RECV_BUSY)) {
			/* get last transmitted I frame */

			/* V(S) = V(S) - 1 */
			Q921_DEC_COUNTER(link->vs);

			/* retransmit I frame */

			/* V(S) = V(S) + 1 (done by Q921SendI() ) */
			//Q921_INC_COUNTER(link->vs);

			/* clear acknowledge pending */
			Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);

			/* Start T200 */
			Q921T200TimerStart(trunk, tei);
		} else {
			/* transmit enquiry */
			Q921SendEnquiry(trunk, tei);
		}

		/* increment counter */
		link->N200++;

		/* change state (no action) */
		Q921ChangeState(trunk, Q921_STATE_TIMER_RECOVERY, tei);
		break;

	case Q921_STATE_TIMER_RECOVERY:
		if(link->N200 == trunk->N200Limit) {
			/* MDL Error indication (I) */

			/* Establish data link */
			Q921EstablishDataLink(trunk, tei);

			/* Clear L3 initiated */
			Q921_CLEAR_FLAG(link, Q921_FLAG_L3_INITIATED);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		} else {
			if(link->vs == link->va) {
				/* transmit enquiry */
				Q921SendEnquiry(trunk, tei);

			} else if(!Q921_CHECK_FLAG(link, Q921_FLAG_PEER_RECV_BUSY)) {
				/* get last transmitted frame */

				/* V(S) = V(S) - 1 */
				Q921_DEC_COUNTER(link->vs);

				/* retrans frame */

				/* V(S) = V(S) + 1 (done by Q921SendI() ) */
				//Q921_INC_COUNTER(link->vs);

				/* clear acknowledge pending */
				Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);

				/* Start T200 */
				Q921T200TimerStart(trunk, tei);
			}

			/* increment counter */
			link->N200++;

			/* no state change */
		}
		break;

	default:
		break;
	}
}

static void Q921T203TimerExpire(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link   = Q921_LINK_CONTEXT(trunk, tei);
	struct Q921_Link *trlink = Q921_TRUNK_CONTEXT(trunk);

	Q921Log(trunk, Q921_LOG_DEBUG, "T203 expired for TEI %d (trunk TEI %d)\n", tei, trlink->tei);

	/* Stop Timer first */
	Q921T203TimerStop(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		/* Send Enquiry */
		Q921SendEnquiry(trunk, tei);

		/* RC = 0 */
		link->N200 = 0;

		/* no state change */
		break;

	default:
		break;
	}
}

static void Q921T202TimerExpire(L2TRUNK trunk)
{
	struct Q921_Link *link = Q921_TRUNK_CONTEXT(trunk);

	Q921T202TimerReset(trunk);

	Q921Log(trunk, Q921_LOG_DEBUG, "T202 expired for Q.921 trunk with TEI %d\n", link->tei);

	/* todo: implement resend counter */

	switch(link->state) {
	case Q921_STATE_TEI_ASSIGNED:	/* Tei identity verify timeout */
		Q921TeiSendVerifyRequest(trunk);
		break;

	default:			/* Tei assignment request timeout (TODO: refine) */

		if(trunk->N202 >= trunk->N202Limit) {
			/* Too many retransmits, reset counter, stop timer and handle case (TODO) */
			trunk->N202 = 0;

			Q921T202TimerStop(trunk);

			return;
		}
		Q921TeiSendAssignRequest(trunk);

		trunk->N202++;
	}
}

static void Q921T201TimerExpire(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link   = Q921_LINK_CONTEXT(trunk, tei);
	struct Q921_Link *trlink = Q921_TRUNK_CONTEXT(trunk);

	Q921Log(trunk, Q921_LOG_DEBUG, "T201 expired for TEI %d (trunk TEI: %d)\n", tei, trlink->tei);

	Q921T201TimerStop(trunk, tei);

	/* NOTE: abusing N202 for this */
	if(link->N202 < trunk->N202Limit) {
		/* send check request */
		Q921TeiSendCheckRequest(trunk, tei);

		/* increment counter */
		link->N202++;
	} else {
		/* put context in STOPPED state */
		Q921ChangeState(trunk, Q921_STATE_STOPPED, tei);

		/* NOTE: should we clear the link too? */
		memset(link, 0, sizeof(struct Q921_Link));

		/* mark TEI free */
		trunk->tei_map[tei] = 0;
	}
}

#ifdef __UNUSED_FOR_NOW__
static void Q921TM01TimerExpire(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link   = Q921_LINK_CONTEXT(trunk, tei);
	struct Q921_Link *trlink = Q921_TRUNK_CONTEXT(trunk);

	Q921Log(trunk, Q921_LOG_DEBUG, "TM01 expired for TEI %d (trunk TEI: %d)\n", tei, trlink->tei);

	/* Restart TM01 */
	Q921TM01TimerReset(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
	case Q921_STATE_TIMER_RECOVERY:
/*
 * NT-only, needs more support from L3
 */
#if 0
		/* No activity, shutdown link */
		Q921SendDISC(trunk, trunk->sapi, Q921_COMMAND(trunk), tei, 1);

		/* clear I queue */
		MFIFOClear(link->IFrameQueue);

		/* change state */
		Q921ChangeState(trunk, Q921_STATE_AWAITING_RELEASE, tei);
#endif
		break;

	default:
		break;
	}
}
#endif

/*
 * Timer Tick function
 */
void Q921TimerTick(L2TRUNK trunk)
{
	struct Q921_Link *link;
	L2ULONG tNow = Q921GetTime();
	int numlinks = Q921_IS_PTMP_NT(trunk) ? Q921_TEI_MAX : 1;
	int x;

	for(x = 0; x <= numlinks; x++) {
		link = Q921_LINK_CONTEXT(trunk, x);

		/* TODO: check if TEI is assigned and skip check if not (speedup!) */
		if(link->state == Q921_STATE_STOPPED)
			continue;

		if (link->T200 && tNow > link->T200) {
			Q921T200TimerExpire(trunk, link->tei);
		}
		if (link->T203 && tNow > link->T203) {
			Q921T203TimerExpire(trunk, link->tei);		
		}

		if(Q921_IS_PTMP_NT(trunk) && link->tei) {
			if (link->T201 && tNow > link->T201) {
				Q921T201TimerExpire(trunk, link->tei);
			}
		}

		if(!Q921_IS_PTMP_NT(trunk)) {
			if (trunk->T202 && tNow > trunk->T202) {
				Q921T202TimerExpire(trunk);
			}
		}

		/* Send enqueued I frame, if available */
		Q921SendQueuedIFrame(trunk, link->tei);

		/* Send ack if pending */
		Q921AcknowledgePending(trunk, link->tei);
	}

}

void Q921SetGetTimeCB(L2ULONG (*callback)(void))
{
    Q921GetTimeProc = callback;
}

/*****************************************************************************

  Function:     Q921QueueHDLCFrame

  Description:  Called to receive and queue an incoming HDLC frame. Will
                queue this in Q921HDLCInQueue. The called must either call
                Q921Rx12 directly afterwards or signal Q921Rx12 to be called
                later. Q921Rx12 will read from the same queue and process
                the frame.

                This function assumes that the message contains header 
                space. This is removed for internal Q921 processing, but 
                must be keept for I frames.

  Parameters:   trunk   trunk #
                b       ptr to frame;
                size    size of frame in bytes

*****************************************************************************/
int Q921QueueHDLCFrame(L2TRUNK trunk, L2UCHAR *b, L2INT size)
{
    return MFIFOWriteMes(trunk->HDLCInQueue, b, size);
}

/**
 * Q921EnqueueI
 * \brief	Put I frame into transmit queue
 *
 */
static int Q921EnqueueI(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR *mes, L2INT size)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, Tei);

	/* I frame header */
	mes[trunk->Q921HeaderSpace+0] = ((Sapi << 2) & 0xfc) | ((cr << 1) & 0x02);
	mes[trunk->Q921HeaderSpace+1] = (Tei << 1) | 0x01;
	mes[trunk->Q921HeaderSpace+2] = 0x00;
	mes[trunk->Q921HeaderSpace+3] = (pf & 0x01);

	Q921Log(trunk, Q921_LOG_DEBUG, "Enqueueing I frame for TEI %d [%d]\n", link->tei, Tei);

	/* transmit queue, (TODO: check for full condition!) */
	MFIFOWriteMes(link->IFrameQueue, mes, size);

	/* try to send queued frame */
	Q921SendQueuedIFrame(trunk, link->tei);

	return 1;
}

/**
 * Q921SendQueuedIFrame
 * \brief	Try to transmit queued I frame (if available)
 */
static int Q921SendQueuedIFrame(L2TRUNK trunk, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	L2INT size = 0;
	L2UCHAR *mes;

	if(MFIFOGetMesCount(link->IFrameQueue) == 0) {
		return 0;
	}

	/* Link ready? */
	if(link->state != Q921_STATE_MULTIPLE_FRAME_ESTABLISHED) {
		return 0;
	}

	/* peer receiver busy? */
	if(Q921_CHECK_FLAG(link, Q921_FLAG_PEER_RECV_BUSY)) {
		return 0;
	}

	/* V(S) = V(A) + k? */
	if(link->vs == ((link->va + trunk->k) % 128)) {
		Q921Log(trunk, Q921_LOG_WARNING, "Maximum number (%d) of outstanding I frames reached for TEI %d\n", trunk->k, tei);
		return 0;
	}

	mes = MFIFOGetMesPtr(link->IFrameQueue, &size);
	if(mes) {
		/* Fill in + update counter values */
		mes[trunk->Q921HeaderSpace+2]  = link->vs << 1;
		mes[trunk->Q921HeaderSpace+3] |= link->vr << 1;

		if(MFIFOGetMesCount(link->IFrameQueue) == 0) {
			/* clear I frame queued */
		}

		/* Send I frame */
		Q921Tx21Proc(trunk, mes, size);

		/* V(S) = V(S) + 1 */
		Q921_INC_COUNTER(link->vs);

		/* clear acknowledge pending */
		Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);

		/* T200 running? */
		if(!link->T200) {
			/* Stop T203, Start T200 */
			Q921T200TimerStart(trunk, tei);
			Q921T203TimerStop(trunk, tei);
		}

		/* put frame into resend queue */
		MFIFOWriteMesOverwrite(link->IFrameResendQueue, mes, size);

		/* dequeue frame */
		MFIFOKillNext(link->IFrameQueue);

		/* Restart TM01 */
		if(Q921_IS_NT(trunk)) {
			Q921TM01TimerReset(trunk, tei);
		}

		/* no state change */
		return 1;
	}

	return 0;
}

/**
 * Q921SendS
 * \brief	Prepare and send S frame
 */
static int Q921SendS(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR sv, L2UCHAR *mes, L2INT size)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, Tei);

	if(!Q921_IS_READY(link)) {
		/* don't even bother trying */
		Q921Log(trunk, Q921_LOG_DEBUG, "Link not ready, discarding S frame for TEI %d\n", Tei);
		return 0;
	}

	/* S frame header */
	mes[trunk->Q921HeaderSpace+0] = ((Sapi << 2) & 0xfc) | ((cr << 1) & 0x02);
	mes[trunk->Q921HeaderSpace+1] = (Tei << 1) | 0x01;
	mes[trunk->Q921HeaderSpace+2] = ((sv << 2) & 0x0c) | 0x01;
	mes[trunk->Q921HeaderSpace+3] = (link->vr << 1) | (pf & 0x01);

	return Q921Tx21Proc(trunk, mes, size);
}


/**
 * Q921SendU
 * \brief	Prepare and send U frame
 */
static int Q921SendU(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR m, L2UCHAR *mes, L2INT size)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, Tei);

	/* U frame header */
	mes[trunk->Q921HeaderSpace+0] = ((Sapi << 2) & 0xfc) | ((cr << 1) & 0x02);
	mes[trunk->Q921HeaderSpace+1] = (Tei << 1) | 0x01;
	mes[trunk->Q921HeaderSpace+2] = ((m << 3) & 0xe0) | ((pf << 4) & 0x10) | ((m << 2) & 0x0c) | 0x03;

	/* link not ready? enqueue non-TEI-mgmt UI (DL-UNIT DATA) frames */
	if(m == 0x00 && Sapi != Q921_SAPI_TEI && link->state < Q921_STATE_TEI_ASSIGNED) {

		/* write frame to queue */
		MFIFOWriteMes(link->UIFrameQueue, mes, size);

		Q921Log(trunk, Q921_LOG_DEBUG, "Link not ready, UI Frame of size %d bytes queued for TEI %d\n", size, Tei);
		return 1;
	}

	return Q921Tx21Proc(trunk, mes, size);
}

/**
 * TODO: NT mode handling? Need a way to get Link context from Q.931
 */
int Q921Rx32(L2TRUNK trunk, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR * Mes, L2INT Size)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);	/* TODO: need real link tei for NT mode */
	L2INT res = 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "Got frame from Q.931, type: %d, tei: %d, size: %d\n", ind, tei, Size);

	switch(ind) {
	case Q921_DL_ESTABLISH:
		/*
		 * Hmm...
		 */
		switch(link->state) {
		case Q921_STATE_TEI_ASSIGNED:
			if(!Q921_IS_NT(trunk)) {
				/* establish data link */
				Q921EstablishDataLink(trunk, link->tei);

				/* Set layer 3 initiated */
				Q921_SET_FLAG(link, Q921_FLAG_L3_INITIATED);

				/* change state (no action) */
				Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, link->tei);
			}
			break;

		case Q921_STATE_AWAITING_ESTABLISHMENT:
			if(!Q921_IS_NT(trunk)) {
				/* Discard I queue */
				MFIFOClear(link->IFrameQueue);

				/* Set layer 3 initiated */
				Q921_SET_FLAG(link, Q921_FLAG_L3_INITIATED);
			}
			break;

		case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		case Q921_STATE_TIMER_RECOVERY:
			if(!Q921_IS_NT(trunk)) {
				/* Discard I queue */
				MFIFOClear(link->IFrameQueue);

				/* establish data link */
				Q921EstablishDataLink(trunk, link->tei);

				/* Set layer 3 initiated */
				Q921_SET_FLAG(link, Q921_FLAG_L3_INITIATED);

				/* change state (no action) */
				Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, link->tei);
			}
			break;

		default:
			break;
		}
		break;

	case Q921_DL_RELEASE:
		switch(link->state) {
		case Q921_STATE_TEI_ASSIGNED:
			/* send DL-RELEASE confirm */
			Q921Tx23Proc(trunk, Q921_DL_RELEASE, tei, NULL, 0);
			break;

		case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		case Q921_STATE_TIMER_RECOVERY:
			if(!Q921_IS_NT(trunk)) {
				/* Discard I queue */
				MFIFOClear(link->IFrameQueue);

				/* RC = 0 */
				link->N200 = 0;

				/* send DISC command */
				Q921SendDISC(trunk, trunk->sapi, Q921_COMMAND(trunk), link->tei, 1);

				/* Stop T203, restart T200 */
				if(link->state == Q921_STATE_MULTIPLE_FRAME_ESTABLISHED) {
					Q921T203TimerStop(trunk, link->tei);
				}
				Q921T200TimerReset(trunk, link->tei);

				/* change state */
				Q921ChangeState(trunk, Q921_STATE_AWAITING_RELEASE, link->tei);
			}
			break;

		default:
			break;
		}
		break;

	case Q921_DL_DATA:	/* DL-DATA request */
		res = Q921EnqueueI(trunk, 
				trunk->sapi, 
				Q921_COMMAND(trunk),
				link->tei,
				0, 
				Mes, 
				Size);

		if(link->state < Q921_STATE_MULTIPLE_FRAME_ESTABLISHED) {
			/* Treat as implicit DL-ESTABLISH request */

			/* establish data link */
			Q921EstablishDataLink(trunk, link->tei);

			/* Set layer 3 initiated */
			Q921_SET_FLAG(link, Q921_FLAG_L3_INITIATED);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, link->tei);
		}
		break;

	case Q921_DL_UNIT_DATA:		/* DL-UNIT DATA request */
		res = Q921SendUN(trunk,
				trunk->sapi,
				Q921_COMMAND(trunk),
				Q921_TEI_BCAST,
				0,
				Mes,
				Size);
		/* NOTE: Let the other side initiate link establishment */
		break;

	default:
		break;
	}

	return res;
}
/*****************************************************************************

  Function:     Q921SendRR

  Description:  Compose and send Receive Ready.

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P/F fiels octet 5

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/

static int Q921SendRR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
	L2UCHAR mes[25];

	return Q921SendS(trunk, Sapi, cr, Tei, pf, 0x00, mes, trunk->Q921HeaderSpace+4);
}

/*****************************************************************************

  Function:     Q921SendRNR

  Description:  Compose and send Receive Nor Ready

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P/F fiels octet 5

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
static int Q921SendRNR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
	L2UCHAR mes[25];

	return Q921SendS(trunk, Sapi, cr, Tei, pf, 0x01, mes, trunk->Q921HeaderSpace+4);
}

/*****************************************************************************

  Function:     Q921SendREJ

  Description:  Compose and Send Reject.

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P/F fiels octet 5

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
static int Q921SendREJ(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
	L2UCHAR mes[25];

	return Q921SendS(trunk, Sapi, cr, Tei, pf, 0x03, mes, trunk->Q921HeaderSpace+4);
}

/*****************************************************************************

  Function:     Q921SendSABME

  Description:  Compose and send SABME

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P fiels octet 4

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
static int Q921SendSABME(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
	L2UCHAR mes[25];

	return Q921SendU(trunk, Sapi, cr, Tei, pf, 0x0f, mes, trunk->Q921HeaderSpace+3);
}


/**
 * Q921Start
 * \brief	Start trunk
 * \param[in]	trunk	pointer to Q921 data struct
 * \return	> 0 on success; <= 0 on error
 */
int Q921Start(L2TRUNK trunk)
{
	int x, numlinks = Q921_IS_PTMP_NT(trunk) ? Q921_TEI_MAX : 1;
	struct Q921_Link *link = Q921_TRUNK_CONTEXT(trunk);

	if(trunk->initialized != INITIALIZED_MAGIC)
		return 0;

	memset(trunk->context, 0, numlinks * sizeof(struct Q921_Link));

	/* Common init part */
	for(x = 0; x <= numlinks; x++) {
		link = Q921_LINK_CONTEXT(trunk, x);

		link->state = Q921_STATE_TEI_UNASSIGNED;
		link->tei   = 0;

		/* Initialize per-TEI I + UI queues */
		MFIFOCreate(link->UIFrameQueue, Q921MAXHDLCSPACE, 10);
		MFIFOCreate(link->IFrameQueue,  Q921MAXHDLCSPACE, 10);
		MFIFOCreate(link->IFrameResendQueue, Q921MAXHDLCSPACE, 10);
	}

	if(Q921_IS_PTMP_TE(trunk)) {
		link->state = Q921_STATE_TEI_UNASSIGNED;
		link->tei   = 0;
	}
	else if(Q921_IS_PTMP_NT(trunk)) {
		link = Q921_TRUNK_CONTEXT(trunk);

		link->state = Q921_STATE_TEI_ASSIGNED;
		link->tei   = trunk->tei;

		/* clear tei map */
		memset(trunk->tei_map, 0, Q921_TEI_MAX + 1);
	}
	else {
		link->state = Q921_STATE_TEI_ASSIGNED;
		link->tei   = trunk->tei;
	}

	Q921Log(trunk, Q921_LOG_DEBUG, "Starting trunk %p (sapi: %d, tei: %d, mode: %s %s)\n",
				 trunk,
				 trunk->sapi,
				 link->tei,
				 Q921_IS_PTMP(trunk) ? "PTMP" : "PTP",
				 Q921_IS_TE(trunk) ? "TE" : "NT");

	if(Q921_IS_PTP(trunk)) {
		Q921Log(trunk, Q921_LOG_DEBUG, "Sending SABME\n");

		return Q921SendSABME(trunk, 
					trunk->sapi, 
					Q921_COMMAND(trunk),
					link->tei, 
					1);

	} else if(Q921_IS_PTMP_NT(trunk)) {

		Q921Log(trunk, Q921_LOG_DEBUG, "Revoking all TEIs\n");

		return Q921TeiSendRemoveRequest(trunk, Q921_TEI_BCAST);	/* Revoke all TEIs in use */
	} else {

		Q921Log(trunk, Q921_LOG_DEBUG, "Requesting TEI\n");

		return Q921TeiSendAssignRequest(trunk);
	}
}


/**
 * Q921Stop
 * \brief	Stop trunk
 * \param[in]	trunk	pointer to Q921 data struct
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
int Q921Stop(L2TRUNK trunk)
{
	struct Q921_Link *link;
	int x, numlinks;

	if(!trunk)
		return -1;

	link = Q921_TRUNK_CONTEXT(trunk);
	numlinks = Q921_IS_PTMP_NT(trunk) ? Q921_TEI_MAX : 1;

	if(Q921_IS_STOPPED(link))
		return 0;

	/* Release TEI */
	if(Q921_IS_PTMP_TE(trunk)) {
		/* send verify request */
		Q921TeiSendVerifyRequest(trunk);

		/* drop TEI */
		link->tei  = 0;
	}

	/* Stop timers, stop link, flush queues */
	for(x = 0; x <= numlinks; x++) {
		Q921T200TimerStop(trunk, x);
		Q921T203TimerStop(trunk, x);
		Q921T201TimerStop(trunk, x);

		/* Change state (no action) */
		Q921ChangeState(trunk, Q921_STATE_STOPPED, x);

		/* Flush per-tei I/UI queues */
		MFIFOClear(link->UIFrameQueue);
		MFIFOClear(link->IFrameQueue);
		MFIFOClear(link->IFrameResendQueue);
	}
	Q921T202TimerStop(trunk);

	/* Flush HDLC queue */
	MFIFOClear(trunk->HDLCInQueue);

	return 0;
}


/*****************************************************************************

  Function:     Q921SendDM

  Description:  Compose and Send DM (Disconnected Mode)

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          F fiels octet 4

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
static int Q921SendDM(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
	L2UCHAR mes[25];

	return Q921SendU(trunk, Sapi, cr, Tei, pf, 0x03, mes, trunk->Q921HeaderSpace+3);
}

/*****************************************************************************

  Function:     Q921SendDISC

  Description:  Compose and Send Disconnect

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P fiels octet 4

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
static int Q921SendDISC(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
	L2UCHAR mes[25];

	return Q921SendU(trunk, Sapi, cr, Tei, pf, 0x08, mes, trunk->Q921HeaderSpace+3);
}

/*****************************************************************************

  Function:     Q921SendUA

  Description:  Compose and Send UA

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          F fiels octet 4

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
static int Q921SendUA(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
	L2UCHAR mes[25];

	return Q921SendU(trunk, Sapi, cr, Tei, pf, 0x0c, mes, trunk->Q921HeaderSpace+3);
}

static int Q921SendUN(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf, L2UCHAR *mes, L2INT size)
{
	return Q921SendU(trunk, Sapi, cr, Tei, pf, 0x00, mes, size+trunk->Q921HeaderSpace+3);
}


/**
 * Q921ProcSABME
 * \brief	Handle incoming SABME
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success, <= 0 on error
 */
static int Q921ProcSABME(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR pf = (mes[2] & 0x10) >> 4;				/* poll / final flag */
	L2UCHAR tei = (mes[1] & 0xfe) >> 1;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_TEI_ASSIGNED:
		/* send UA */
		Q921SendUA(trunk,
				trunk->sapi,
				Q921_RESPONSE(trunk),	/* or command? */
				tei, pf);

		/* clear counters */
		link->vr=0;
		link->vs=0;
		link->va=0;

		/* TODO: send DL-Establish indication to Q.931 */
		Q921Tx23Proc(trunk, Q921_DL_ESTABLISH, tei, NULL, 0);

		/* start T203 */
		Q921T203TimerStart(trunk, tei);

		/* change state (no action) */
		Q921ChangeState(trunk, Q921_STATE_MULTIPLE_FRAME_ESTABLISHED, tei);
		break;

	case Q921_STATE_AWAITING_ESTABLISHMENT:
		/* send UA */
		Q921SendUA(trunk,
				trunk->sapi,
				Q921_RESPONSE(trunk),
				tei, pf);

		/* no state change */
		break;

	case Q921_STATE_AWAITING_RELEASE:
		/* send DM */
		Q921SendDM(trunk,
				trunk->sapi,
				Q921_RESPONSE(trunk),
				tei, pf);

		/* no state change */
		break;

	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
	case Q921_STATE_TIMER_RECOVERY:
		/* send UA */
		Q921SendUA(trunk,
				trunk->sapi,
				Q921_RESPONSE(trunk),
				tei, pf);

		/* clear exception conditions */
		Q921ResetExceptionConditions(trunk, tei);

		/* send MDL-Error indication */

		/* V(S) == V(A) ? */
		if(link->vs != link->va) {
			/* clear I queue */
			MFIFOClear(link->IFrameQueue);

			/* DL-Establish indication */
			Q921Tx23Proc(trunk, Q921_DL_ESTABLISH, tei, NULL, 0);
		}

		/* clear counters */
		link->vr=0;
		link->vs=0;
		link->va=0;

		/* Stop T200, start T203 */
		Q921T200TimerStop(trunk, tei);
		Q921T203TimerStart(trunk, tei);

		/* state change only if in TIMER_RECOVERY state */
		if(link->state == Q921_STATE_TIMER_RECOVERY)
			Q921ChangeState(trunk, Q921_STATE_MULTIPLE_FRAME_ESTABLISHED, tei);
		break;

	default:
		break;
	}

	return 1;
}


/**
 * Q921ProcDM
 * \brief	Handle incoming DM
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success, <= 0 on error
 */
static int Q921ProcDM(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR pf = (mes[2] & 0x10) >> 4;				/* poll / final flag */
	L2UCHAR tei = (mes[1] & 0xfe) >> 1;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_TEI_ASSIGNED:
		if(!pf) {
			/* to next state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	case Q921_STATE_AWAITING_ESTABLISHMENT:
	case Q921_STATE_AWAITING_RELEASE:
		if(pf) {
			if(link->state == Q921_STATE_AWAITING_ESTABLISHMENT) {
				/* Discard I queue */
				MFIFOClear(link->IFrameQueue);
			}

			/* Send DL-Release indication to Q.931 */
			Q921Tx23Proc(trunk, Q921_DL_RELEASE, tei, NULL, 0);

			/* Stop T200 */
			Q921T200TimerStop(trunk, tei);

			/* Change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_TEI_ASSIGNED, tei);
		}
		break;

	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		if(pf) {
			/* MDL-Error indication (B) */

			/* no state change */
		} else {
			/* MDL-Error indication (E) */

			/* establish data link */
			Q921EstablishDataLink(trunk, tei);

			/* clear L3 initiated */
			Q921_CLEAR_FLAG(link, Q921_FLAG_L3_INITIATED);

			/* change state (no action?) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	case Q921_STATE_TIMER_RECOVERY:
		if(pf) {
			/* MDL Error indication (B) */
		} else {
			/* MDL Error indication (E) */
		}

		/* establish data link */
		Q921EstablishDataLink(trunk, tei);

		/* clear layer 3 initiated */
		Q921_CLEAR_FLAG(link, Q921_FLAG_L3_INITIATED);

		/* change state */
		Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		break;

	default:
		break;
	}

	return 1;
}

/**
 * Q921ProcUA
 * \brief	Handle incoming UA
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success, <= 0 on error
 */
static int Q921ProcUA(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR pf = (mes[2] & 0x10) >> 4;				/* poll / final flag */
	L2UCHAR tei = (mes[1] & 0xfe) >> 1;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_TEI_ASSIGNED:
	case Q921_STATE_TIMER_RECOVERY:
		/* MDL Error indication (C, D) */
		Q921Log(trunk, Q921_LOG_ERROR, "Received UA frame in invalid state\n");
		break;

	case Q921_STATE_AWAITING_ESTABLISHMENT:
		if(pf) {
			/* TODO: other fancy stuff (see docs) */
			if(Q921_CHECK_FLAG(link, Q921_FLAG_L3_INITIATED)) {	/* layer3 initiated */
				link->vr = 0;

				/* DL-Establish confirm */
				Q921Tx23Proc(trunk, Q921_DL_ESTABLISH_CONFIRM, tei, NULL, 0);

			} else if(link->vs != link->va) {

				/* discard I queue */
				MFIFOClear(link->IFrameQueue);

				/* DL-Establish indication */
				Q921Tx23Proc(trunk, Q921_DL_ESTABLISH, tei, NULL, 0);
			}

			/* Stop T200, start T203 */
			Q921T200TimerStop(trunk, tei);
			Q921T203TimerStart(trunk, tei);

			link->vs = 0;
			link->va = 0;

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_MULTIPLE_FRAME_ESTABLISHED, tei);
		} else {
			/* MDL Error indication (C, D) */
			Q921Log(trunk, Q921_LOG_ERROR, "Received UA frame is not a response to a request\n");

			/* no state change */
		}
		break;

	case Q921_STATE_AWAITING_RELEASE:
		if(pf) {
			/* DL Release confirm */
			Q921Tx23Proc(trunk, Q921_DL_RELEASE_CONFIRM, tei, NULL, 0);

			/* Stop T200 */
			Q921T200TimerStop(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_TEI_ASSIGNED, tei);
		} else {
			/* MDL Error indication (D) */
			Q921Log(trunk, Q921_LOG_ERROR, "Received UA frame is not a response to a request\n");

			/* no state change */
		}
		break;

	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		/* MDL Error indication (C, D) */
		Q921Log(trunk, Q921_LOG_ERROR, "Received UA frame in invalid state\n");

		/* no state change */
		break;

	default:
		break;
	}

	return 1;
}


/**
 * Q921ProcDISC
 * \brief	Handle incoming DISC
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success, <= 0 on error
 */
static int Q921ProcDISC(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR pf  = (mes[2] & 0x10) >> 4;				/* poll / final flag */
	L2UCHAR tei = (mes[1] & 0xfe) >> 1;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_TEI_ASSIGNED:
	case Q921_STATE_AWAITING_ESTABLISHMENT:
		/* Send DM */
		Q921SendDM(trunk,
				trunk->sapi,
				Q921_RESPONSE(trunk),
				tei, pf);

		/* no state change */
		break;

	case Q921_STATE_AWAITING_RELEASE:
		Q921SendUA(trunk,
				trunk->sapi,
				Q921_RESPONSE(trunk),
				tei, pf);

		/* no state change */
		break;

	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
	case Q921_STATE_TIMER_RECOVERY:
		/* Discard I queue */
		MFIFOClear(link->IFrameQueue);

		/* send UA */
		Q921SendUA(trunk,
				trunk->sapi,
				Q921_RESPONSE(trunk),
				tei, pf);
		
		/* DL Release indication */
		Q921Tx23Proc(trunk, Q921_DL_RELEASE, tei, NULL, 0);

		/* Stop T200 */
		Q921T200TimerStop(trunk, tei);

		if(link->state == Q921_STATE_MULTIPLE_FRAME_ESTABLISHED) {
			/* Stop T203 */
			Q921T203TimerStop(trunk, tei);
		}

		/* change state (no action) */
		Q921ChangeState(trunk, Q921_STATE_TEI_ASSIGNED, tei);
		break;

	default:
		Q921Log(trunk, Q921_LOG_ERROR, "Invalid DISC received in state \"%s\" (%d)", Q921State2Name(link->state), link->state);
		break;
	}

	return 1;
}


/**
 * Q921ProcRR
 * \brief	Handle incoming RR
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success, <= 0 on error
 */
static int Q921ProcRR(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR cr = (mes[0] & 0x02) >> 1;
	L2UCHAR pf =  mes[3] & 0x01;		/* poll / final flag */
	L2UCHAR nr = (mes[3] >> 1);
//	L2UCHAR	sapi = (mes[0] & 0xfc) >> 2;
	L2UCHAR	tei  = (mes[1] & 0xfe) >> 1;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		/* clear receiver peer busy */
		Q921_CLEAR_FLAG(link, Q921_FLAG_PEER_RECV_BUSY);

		if (Q921_IS_COMMAND(trunk, cr)) { /* if this is a command */
			if(pf) {
				/* Enquiry response */
				Q921SendEnquiryResponse(trunk, tei);
			}
		} else {
			if(pf) {
				/* MDL Error indication */
			}
		}

		/* */
		if(link->va <= nr && nr <= link->vs) {

			if(nr == link->vs) {
				/* V(A) = N(R) */
				link->va = nr;

				/* Stop T200, restart T203 */
				Q921T200TimerStop(trunk, tei);
				Q921T203TimerReset(trunk, tei);

			} else if(nr == link->va) {

				/* do nothing */

			} else {
				/* V(A) = N(R) */
				link->va = nr;

				/* Restart T200 */
				Q921T200TimerReset(trunk, tei);
			}
			/* no state change */

		} else {
			/* N(R) Error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	case Q921_STATE_TIMER_RECOVERY:
		/* clear receiver peer busy */
		Q921_CLEAR_FLAG(link, Q921_FLAG_PEER_RECV_BUSY);

		/* command + P? */
		if(Q921_IS_COMMAND(trunk, cr) && pf) {
			/* Enquiry response */
			Q921SendEnquiryResponse(trunk, tei);
		}

		/* */
		if(link->va <= nr && nr <= link->vs) {
			/* V(A) = N(R) */
			link->va = nr;

			if(!Q921_IS_COMMAND(trunk, cr) && pf) {
				/* Stop T200, start T203 */
				Q921T200TimerStop(trunk, tei);
				Q921T203TimerStart(trunk, tei);

				/* Invoke retransmission */
				Q921InvokeRetransmission(trunk, tei, nr);

				/* change state (no action) */
				Q921ChangeState(trunk, Q921_STATE_MULTIPLE_FRAME_ESTABLISHED, tei);
			}
			/* no state change otherwise */
		} else {
			/* N(R) Error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	default:
		break;
	}
	return 1;
}


/**
 * Q921ProcREJ
 * \brief	Handle incoming REJ
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success, <= 0 on error
 */
static int Q921ProcREJ(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR cr = (mes[0] & 0x02) >> 1;
	L2UCHAR pf =  mes[3] & 0x01;		/* poll / final flag */
	L2UCHAR nr = (mes[3] >> 1);
//	L2UCHAR	sapi = (mes[0] & 0xfc) >> 2;
	L2UCHAR	tei  = (mes[1] & 0xfe) >> 1;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		/* clear receiver peer busy */
		Q921_CLEAR_FLAG(link, Q921_FLAG_PEER_RECV_BUSY);

		/* command? */
		if(Q921_IS_COMMAND(trunk, cr)) {
			if(pf) {
				/* Enquiry response */
				Q921SendEnquiryResponse(trunk, tei);
			}
		} else {
			if(pf) {
				/* MDL Error indication (A) */
			}
		}

		/* */
		if(link->va <= nr && nr <= link->vs) {

			/* V(A) = N(R) */
			link->va = nr;

			/* Stop T200, start T203 */
			Q921T200TimerStop(trunk, tei);
			Q921T203TimerStart(trunk, tei);

			/* Invoke retransmission of frame >N(R)  (?) */
			Q921InvokeRetransmission(trunk, tei, nr);

			/* no state change */
		} else {
			/* N(R) Error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	case Q921_STATE_TIMER_RECOVERY:
		/* clear receiver peer busy */
		Q921_CLEAR_FLAG(link, Q921_FLAG_PEER_RECV_BUSY);

		/* command + P ? */
		if(Q921_IS_COMMAND(trunk, cr) && pf) {
			/* Enquiry response */
			Q921SendEnquiryResponse(trunk, tei);
		}

		/* */
		if(link->va <= nr && nr <= link->vs) {

			/* V(A) = N(R) */
			link->va = nr;

			if(!Q921_IS_COMMAND(trunk, cr) && pf) {
				/* Stop T200, start T203 */
				Q921T200TimerStop(trunk, tei);
				Q921T203TimerStart(trunk, tei);

				/* Invoke retransmission */
				Q921InvokeRetransmission(trunk, tei, nr);

				/* change state (no action) */
				Q921ChangeState(trunk, Q921_STATE_MULTIPLE_FRAME_ESTABLISHED, tei);
			}
			/* no state change otherwise */
		} else {
			/* N(R) Error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	default:
		break;
	}

	return 1;
}


/**
 * Q921ProcRNR
 * \brief	Handle incoming RNR
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success, <= 0 on error
 */
static int Q921ProcRNR(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR cr = (mes[0] & 0x02) >> 1;
	L2UCHAR pf =  mes[3] & 0x01;		/* poll / final flag */
	L2UCHAR nr = (mes[3] >> 1);
//	L2UCHAR	sapi = (mes[0] & 0xfc) >> 2;
	L2UCHAR	tei  = (mes[1] & 0xfe) >> 1;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		/* set peer receiver busy */
		Q921_SET_FLAG(link, Q921_FLAG_PEER_RECV_BUSY);

		/* command? */
		if(Q921_IS_COMMAND(trunk, cr)) {
			if(pf) {
				/* Enquiry response */
				Q921SendEnquiryResponse(trunk, tei);
			}
		} else {
			if(pf) {
				/* MDL Error indication (A) */
			}
		}

		/* */
		if(link->va <= nr && nr <= link->vs) {

			/* V(A) = N(R) */
			link->va = nr;

			/* Stop T203, restart T200 */
			Q921T200TimerReset(trunk, tei);
			Q921T203TimerStop(trunk, tei);

			/* no state change */
		} else {
			/* N(R) Error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	case Q921_STATE_TIMER_RECOVERY:
		/* set peer receiver busy */
		Q921_SET_FLAG(link, Q921_FLAG_PEER_RECV_BUSY);

		/* command + P? */
		if(Q921_IS_COMMAND(trunk, cr) && pf) {
			/* Enquiry response */
			Q921SendEnquiryResponse(trunk, tei);
		}

		/* */
		if(link->va <= nr && nr <= link->vs) {

			/* V(A) = N(R) */
			link->va = nr;

			if(!Q921_IS_COMMAND(trunk, cr) && pf) {
				/* Restart T200 */
				Q921T200TimerReset(trunk, tei);

				/* Invoke retransmission */
				Q921InvokeRetransmission(trunk, tei, nr);

				/* change state (no action) */
				Q921ChangeState(trunk, Q921_STATE_MULTIPLE_FRAME_ESTABLISHED, tei);
			}
			/* no state change otherwise */
		} else {
			/* N(R) Error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	default:
		break;
	}

	return 1;
}

#if 0
static int Q921SetReceiverBusy(L2TRUNK trunk)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		if(!Q921_CHECK_FLAG(link, Q921_FLAG_RECV_BUSY)) {
			/* set own receiver busy */
			Q921_SET_FLAG(link, Q921_FLAG_RECV_BUSY);

			/* send RR response */
			Q921SendRR(trunk, trunk->sapi, Q921_RESPONSE(trunk), link->tei, 0);

			/* clear ack pending */
			Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
		}
		break;

	case Q921_STATE_TIMER_RECOVERY:
		if(!Q921_CHECK_FLAG(link, Q921_FLAG_RECV_BUSY)) {
			/* set own receiver busy */
			Q921_SET_FLAG(link, Q921_FLAG_RECV_BUSY);

			/* send RNR response */
			Q921SendRNR(trunk, trunk->sapi, Q921_RESPONSE(trunk), link->tei, 0);

			/* clear ack pending */
			Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
		}
		break;

	default:
		break;
	}

	return 0;
}

static int Q921ClearReceiverBusy(L2TRUNK trunk)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
	case Q921_STATE_TIMER_RECOVERY:
		if(Q921_CHECK_FLAG(link, Q921_FLAG_RECV_BUSY)) {
			/* clear own receiver busy */
			Q921_CLEAR_FLAG(link, Q921_FLAG_RECV_BUSY);

			/* send RNR response */
			Q921SendRNR(trunk, trunk->sapi, Q921_RESPONSE(trunk), link->tei, 0);

			/* clear ack pending */
			Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
		}
		break;

	default:
		break;
	}

	return 0;
}
#endif

static int Q921ProcIFrame(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	/* common fields: get sapi, tei and cr */
//	L2UCHAR sapi = (mes[0] & 0xfc) >> 2;
//	L2UCHAR cr   = (mes[0] & 0x02) >> 1;
	L2UCHAR tei  = (mes[1] & 0xfe) >> 1;
	L2UCHAR pf   =  mes[3] & 0x01;		/* poll / final flag */
	L2UCHAR nr   =  mes[3] >> 1;		/* receive sequence number */
	L2UCHAR ns   =  mes[2] >> 1;		/* send sequence number */
	L2UCHAR discard = 0;
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

	/* Ignore I frames in earlier states */
	if(link->state < Q921_STATE_MULTIPLE_FRAME_ESTABLISHED) {
		Q921Log(trunk, Q921_LOG_NOTICE, "I frame in invalid state ignored\n");
		return 0;
	}

	/* Receiver busy? */
	if(Q921_CHECK_FLAG(link, Q921_FLAG_RECV_BUSY)) {
		/* discard information */
		discard = 1;

		if(pf) {
			/* send RNR Response */
			Q921SendRNR(trunk, trunk->sapi, Q921_RESPONSE(trunk), tei, 1);

			/* Clear ack pending */
			Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
		}
	}
	else {
		if(ns != link->vr) {
			/* discard information */
			discard = 1;

			if(Q921_CHECK_FLAG(link, Q921_FLAG_REJECT) && pf) {

				/* Send RR response */
				Q921SendRR(trunk, trunk->sapi, Q921_RESPONSE(trunk), tei, 1);

				/* clear ack pending */
				Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
			}
			else if(!Q921_CHECK_FLAG(link, Q921_FLAG_REJECT)){

				/* set reject exception */
				Q921_SET_FLAG(link, Q921_FLAG_REJECT);

				/* Send REJ response */
				Q921SendREJ(trunk, trunk->sapi, Q921_RESPONSE(trunk), tei, pf);

				/* clear ack pending */
				Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
			}
		}
		else {
			/* V(R) = V(R) + 1 */
			Q921_INC_COUNTER(link->vr);

			/* clear reject exception */
			Q921_CLEAR_FLAG(link, Q921_FLAG_REJECT);

			/* DL-Data indication */
			Q921Tx23Proc(trunk, Q921_DL_DATA, tei, mes, size);

			if(pf) {
				/* Send RR response */
				Q921SendRR(trunk, trunk->sapi, Q921_RESPONSE(trunk), tei, 1);

				/* clear ack pending */
				Q921_CLEAR_FLAG(link, Q921_FLAG_ACK_PENDING);
			}
			else if(!Q921_CHECK_FLAG(link, Q921_FLAG_ACK_PENDING)) {
				/* ack pending */

				/* Send RR response */
				Q921SendRR(trunk, trunk->sapi, Q921_RESPONSE(trunk), tei, 0);
				
				/* set ack pending*/
				Q921_SET_FLAG(link, Q921_FLAG_ACK_PENDING);
			}
		}
	}


	switch(link->state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		if(link->va <= nr && nr <= link->vs) {
			if(Q921_CHECK_FLAG(link, Q921_FLAG_PEER_RECV_BUSY)) {
				link->va = nr;
			}
			else if(nr == link->vs) {
				/* V(A) = N(R) */
				link->va = nr;

				/* stop t200, restart t203 */
				Q921T200TimerStop(trunk, tei);
				Q921T203TimerReset(trunk, tei);
			}
			else if(nr != link->va) {
				/* V(A) = N(R) */
				link->va = nr;

				/* restart T200 */
				Q921T200TimerReset(trunk, tei);
			}

			/* Restart TM01 */
			if(Q921_IS_NT(trunk)) {
				Q921TM01TimerReset(trunk, tei);
			}
		}
		else {
			/* N(R) error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	case Q921_STATE_TIMER_RECOVERY:
		if(link->va <= nr && nr <= link->vs) {
			/* V(A) = N(R) */
			link->va = nr;

			/* Restart TM01 */
			if(Q921_IS_NT(trunk)) {
				Q921TM01TimerReset(trunk, tei);
			}
		}
		else {
			/* N(R) error recovery */
			Q921NrErrorRecovery(trunk, tei);

			/* change state (no action) */
			Q921ChangeState(trunk, Q921_STATE_AWAITING_ESTABLISHMENT, tei);
		}
		break;

	default:
		break;
	}

	return 0;
}


static int Q921ProcSFrame(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR sv = (mes[2] & 0x0c) >> 2;	/* supervisory format id */
	//L2UCHAR pf = mes[3] & 0x01;		/* poll / final flag */
	//L2UCHAR nr = mes[3] >> 1;		/* receive sequence number */
	L2INT res = -1;

	switch(sv) {
	case 0x00:	/* RR : Receive Ready */
		res = Q921ProcRR(trunk, mes, size);
		break;

	case 0x02:	/* RNR : Receive Not Ready */
		res = Q921ProcRNR(trunk, mes, size);
		break;

	case 0x04:	/* REJ : Reject */
		res = Q921ProcREJ(trunk, mes, size);
		break;

	default:	/* Invalid / Unknown */
		Q921Log(trunk, Q921_LOG_ERROR, "Invalid S frame type %d\n", sv);
		break;
	}

	return res;
}



static int Q921ProcUFrame(L2TRUNK trunk, L2UCHAR *mes, L2INT size) 
{
	L2UCHAR m  = (mes[2] & 0xe0) >> 3 | (mes[2] & 0x0c) >> 2;	/* modifier function id */
//	L2UCHAR pf = (mes[2] & 0x10) >> 4;				/* poll / final flag */
	L2INT res = -1;

	switch(m) {
	case 0x00:	/* UN : Unnumbered Information */
		if(mes[3] == Q921_LAYER_ENT_ID_TEI)
		{
			if(!Q921_IS_PTMP(trunk)) {
				/* wtf? nice try */
				return res;
			}

			switch(mes[6]) {
			case Q921_TEI_ID_REQUEST:	/* (TE ->) NT */
				res = Q921TeiProcAssignRequest(trunk, mes, size);
				break;

			case Q921_TEI_ID_ASSIGNED:	/* (NT ->) TE */
			case Q921_TEI_ID_DENIED:
				res = Q921TeiProcAssignResponse(trunk, mes, size);
				break;

			case Q921_TEI_ID_CHECKREQ:	/* (NT ->) TE */
				res = Q921TeiProcCheckRequest(trunk, mes, size);
				break;

			case Q921_TEI_ID_CHECKRESP:	/* (TE ->) NT */
				res = Q921TeiProcCheckResponse(trunk, mes, size);
				break;

			case Q921_TEI_ID_REMOVE:	/* (NT ->) TE */
				res = Q921TeiProcRemoveRequest(trunk, mes, size);
				break;

			case Q921_TEI_ID_VERIFY:	/* (TE ->) NT */
				res = Q921TeiProcVerifyRequest(trunk, mes, size);
				break;

			default:			/* Invalid / Unknown */
				Q921Log(trunk, Q921_LOG_ERROR, "Invalid UN message from TEI management/endpoint\n");
				break;
			}
		}
		else if(mes[3] == Q921_LAYER_ENT_ID_Q931) {

			Q921Log(trunk, Q921_LOG_DEBUG, "UI Frame for Layer 3 received\n");

			res = Q921Tx23Proc(trunk, Q921_DL_UNIT_DATA, 0, mes, size);
		}
		break;

	case 0x03:	/* DM : Disconnect Mode */
		res = Q921ProcDM(trunk, mes, size);
		break;

	case 0x08:	/* DISC : Disconnect */
		res = Q921ProcDISC(trunk, mes, size);
		break;

	case 0x0c:	/* UA : Unnumbered Acknowledgement */
		res = Q921ProcUA(trunk, mes, size);
		break;

	case 0x0f:	/* SABME  : Set Asynchronous Balanced Mode Extend */
		res = Q921ProcSABME(trunk, mes, size);
		break;

	case 0x11:	/* FRMR : Frame Reject */
	case 0x17:	/* XID : Exchange Identification */
		res = 0;
		break;

	default:	/* Unknown / Invalid */
		Q921Log(trunk, Q921_LOG_ERROR, "Invalid U frame type: %d\n", m);
		break;
	}

	return res;
}


/*****************************************************************************

  Function:     Q921Rx12

  Description:  Called to process a message frame from layer 1. Will 
                identify the message and call the proper 'processor' for
                layer 2 messages and forward I frames to the layer 3 entity.

                Q921Rx12 will check the input fifo for a message, and if a 
                message exist process one message before it exits. The caller
                must either call Q921Rx12 polling or keep track on # 
                messages in the queue.

  Parameters:   trunk       trunk #.

  Return Value: # messages processed (always 1 or 0).

*****************************************************************************/
int Q921Rx12(L2TRUNK trunk)
{
	L2INT size;     /* receive size & Q921 frame size*/
	L2UCHAR *smes = MFIFOGetMesPtr(trunk->HDLCInQueue, &size);

	if(smes)
	{
		struct Q921_Link *link;
		L2UCHAR sapi, tei;
		L2UCHAR *mes;
		L2INT rs;

		rs  = size - trunk->Q921HeaderSpace;
		mes = &smes[trunk->Q921HeaderSpace];

		Q921LogMesg(trunk, Q921_LOG_DEBUG, 1, mes, rs, "New packet received (%d bytes)", rs);

		/* common fields: get sapi, tei and cr */
		sapi = (mes[0] & 0xfc) >> 2;
		tei  = (mes[1] & 0xfe) >> 1;
		link  = Q921_LINK_CONTEXT(trunk, tei);

		if(Q921_IS_PTMP_TE(trunk) && (
			 (link->state >= Q921_STATE_TEI_ASSIGNED && tei != link->tei && tei != Q921_TEI_BCAST) ||			/* Assigned TEI: Only BCAST and directed */
			 (link->state == Q921_STATE_TEI_UNASSIGNED && tei != Q921_TEI_BCAST)))					/* No assigned TEI: Only BCAST */
		{
			/* Ignore Messages with foreign TEIs */
			goto out;
		}

		if((mes[2] & 0x01) == 0x00) {		/* I frame */
			Q921ProcIFrame(trunk, mes, rs);
		}
		else if((mes[2] & 0x03) == 0x01) {	/* S frame */
			Q921ProcSFrame(trunk, mes, rs);
		}
		else if((mes[2] & 0x03) == 0x03) {	/* U frame */
			Q921ProcUFrame(trunk, mes, rs);
		}
		else {
			Q921Log(trunk, Q921_LOG_ERROR, "Invalid frame type: %d\n", (int)(mes[2] & 0x03));
			/* TODO: send FRMR or REJ */
		}

out:
		MFIFOKillNext(trunk->HDLCInQueue);

		return 1;
	}

	return 0;
}

/*
 * Misc
 */
/**
 * Q921SetLogCB
 * \brief	Set logging callback
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	func	pointer to logging callback function
 * \param[in]	priv	pointer to private data
 *
 * \author	Stefan Knoblich
 */
void Q921SetLogCB(L2TRUNK trunk, Q921LogCB_t func, void *priv)
{
	if(!trunk)
		return;

	trunk->Q921LogProc = func;
	trunk->PrivateDataLog = priv;
}

/**
 * Q921SetLogLevel
 * \brief	Set loglevel of Q.921 logging functions
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	level	new loglevel
 *
 * \author	Stefan Knoblich
 */
void Q921SetLogLevel(L2TRUNK trunk, Q921LogLevel_t level)
{
	if(!trunk)
		return;

    if (level < Q921_LOG_NONE) {
        level = Q921_LOG_NONE;
    } else if (level > Q921_LOG_DEBUG) {
        level = Q921_LOG_DEBUG;
    }

	trunk->loglevel = level;
}


/**
 * Q921ChangeState
 * \brief	Change state, invoke neccessary actions
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	state	state to change to
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921ChangeState(L2TRUNK trunk, Q921State_t state, L2UCHAR tei)
{
	struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);
	Q921State_t oldstate = link->state;
	int res = 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "Changing state from \"%s\" (%d) to \"%s\" (%d) for TEI %d\n",
				Q921State2Name(oldstate), oldstate,
				Q921State2Name(state), state,
				tei);

	/*
	 * generic actions (depending on the target state only)
	 */
	switch(state) {
	case Q921_STATE_MULTIPLE_FRAME_ESTABLISHED:
		/* Start TM01 */
		if(Q921_IS_NT(trunk)) {
			Q921TM01TimerStart(trunk, tei);
		}
		break;

	default:
		break;
	}

	/*
	 * actions that depend on type of the old -> new state transition
	 */
	switch(oldstate) {
	case Q921_STATE_STOPPED:

		switch(state) {
		case Q921_STATE_TEI_UNASSIGNED:
			if(Q921_IS_PTMP_TE(trunk)) {
				res = Q921TeiSendAssignRequest(trunk);
			}
			break;

		case Q921_STATE_TEI_ASSIGNED:
			if(Q921_IS_PTMP_NT(trunk)) {
				res = Q921TeiSendRemoveRequest(trunk, Q921_TEI_BCAST);
			}
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}

	link->state = state;

	Q921Log(trunk, Q921_LOG_DEBUG, "Q921ChangeState() returns %d, new state is \"%s\" (%d) for TEI %d\n", res, Q921State2Name(state), state, tei);

	return res;
}

/*
 * TEI Management functions
 * \note	All TEI-mgmt UN frames are sent with cr = command!
 */
static int Q921TeiSend(L2TRUNK trunk, L2UCHAR type, L2USHORT ri, L2UCHAR ai)
{
	L2UCHAR mes[10];
	L2UCHAR offset = Q921_UFRAME_DATA_OFFSET(trunk);

	mes[offset++] = Q921_LAYER_ENT_ID_TEI;	/* layer management entity identifier */
	mes[offset++] = (ri & 0xff00) >> 8;	/* reference number upper part */
	mes[offset++] =  ri & 0xff;		/* reference number lower part */
	mes[offset++] = type;			/* message type: Identity Request */
	mes[offset++] = ai << 1 | 0x01;		/* action indicator: TEI */

	return Q921SendU(trunk, Q921_SAPI_TEI, Q921_COMMAND(trunk), Q921_TEI_BCAST, 0, 0x00, mes, offset);
}


/**
 * Q921TeiSendAssignRequest
 * \brief	Ask for new TEI (TE mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \return	> 0 on success, <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiSendAssignRequest(L2TRUNK trunk)
{
	struct Q921_Link *link = Q921_TRUNK_CONTEXT(trunk);
	L2INT res;

	if (!Q921_IS_PTMP_TE(trunk))	/* only ptmp te mode*/
		return 0;

#ifndef WIN32
		link->ri = (L2USHORT)(random() % 0xffff);
#else
		link->ri = (L2USHORT)(rand() % 0xffff); //todo
#endif

	/* send TEI assign request */
	res = Q921TeiSend(trunk, Q921_TEI_ID_REQUEST, link->ri, Q921_TEI_BCAST);

	/* start T202 */
	Q921T202TimerStart(trunk);

	return res;
}


/**
 * Q921TeiProcessAssignResponse
 * \brief	Process assign response (TE mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiProcAssignResponse(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	struct Q921_Link *link = Q921_TRUNK_CONTEXT(trunk);
	L2UCHAR offset = Q921_UFRAME_DATA_OFFSET(trunk);
	L2USHORT ri = 0;

	if (!Q921_IS_PTMP_TE(trunk))	/* PTMP TE only */
		return 0;

	ri = (mes[offset + 1] << 8) | mes[offset + 2];

	if(ri != link->ri) {
		/* hmmm ..., not our response i guess */
		return 0;
	}

	switch(mes[offset + 3]) {
	case Q921_TEI_ID_ASSIGNED:
		/* Yay, use the new TEI and change state to assigned */
		link->tei      = mes[offset + 4] >> 1;

		Q921Log(trunk, Q921_LOG_DEBUG, "Assigned TEI %d, setting state to TEI_ASSIGNED\n", link->tei);

		Q921ChangeState(trunk, Q921_STATE_TEI_ASSIGNED, link->tei);
		break;

	case Q921_TEI_ID_DENIED:
		/* oops, what to do now? */
		if ((mes[offset + 4] >> 1) == Q921_TEI_BCAST) {
			/* No more free TEIs? this is bad */

			//Q921TeiSendVerifyRequest(trunk, Q921_TEI_BCAST); /* TODO: does this work ?? */
		} else {
			/* other reason, this is fatal, shutdown link */
		}

		Q921Log(trunk, Q921_LOG_DEBUG, "TEI assignment has been denied, reason: %s\n",
			 ((mes[offset +4] >> 1) == Q921_TEI_BCAST) ? "No free TEIs available" : "Unknown");

		Q921ChangeState(trunk, Q921_STATE_TEI_UNASSIGNED, link->tei);
		break;

	default:
		return 0;
	}

	/* stop T202 */
	Q921T202TimerStop(trunk);

	return 1;
}


/**
 * Q921TeiSendVerifyRequest
 * \brief	Verify TEI (TE mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiSendVerifyRequest(L2TRUNK trunk)
{
	struct Q921_Link *link = Q921_TRUNK_CONTEXT(trunk);
	L2INT res;

	if (!Q921_IS_PTMP_TE(trunk))	/* only ptmp te mode*/
		return 0;

	/* Request running? */
	if (trunk->T202)
		return 0;

	/* Send TEI verify request */
	res = Q921TeiSend(trunk, Q921_TEI_ID_VERIFY, link->ri, link->tei);

	/* start T202 */
	Q921T202TimerStart(trunk);

	return res;
}


/**
 * Q921TeiProcCheckRequest
 * \brief	Process Check Request (TE mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiProcCheckRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	struct Q921_Link *link = Q921_TRUNK_CONTEXT(trunk);
	L2UCHAR offset = Q921_UFRAME_DATA_OFFSET(trunk);
	L2UCHAR tei = (mes[offset + 4] >> 1);		/* action indicator => tei */
	L2INT res = 0;

	if (!Q921_IS_PTMP_TE(trunk))	/* ptmp te mode only */
		return 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "Received TEI Check request for TEI %d\n", tei);

	if (tei == Q921_TEI_BCAST || tei == link->tei) {
		/*
		 * Broadcast TEI check or for our assigned TEI
		 */

		/* send TEI check reponse */
		res = Q921TeiSend(trunk, Q921_TEI_ID_CHECKRESP, link->ri, link->tei);

		Q921T202TimerStop(trunk);
	}

	return res;
}


/**
 * Q921TeiProcRemoveRequest
 * \brief	Process remove Request (TE mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiProcRemoveRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	struct Q921_Link *link = Q921_TRUNK_CONTEXT(trunk);
	L2UCHAR offset = Q921_UFRAME_DATA_OFFSET(trunk);
	L2UCHAR tei = (mes[offset + 4] >> 1);		/* action indicator => tei */
	L2INT res = 0;

	if (!Q921_IS_PTMP_TE(trunk))	/* ptmp te mode only */
		return 0;

	Q921Log(trunk, Q921_LOG_DEBUG, "Received TEI Remove request for TEI %d\n", tei);

	if (tei == Q921_TEI_BCAST || tei == link->tei) {
		/*
		 * Broadcast TEI remove or for our assigned TEI
		 */

		/* reset tei */
		link->tei  = 0;

		/* change state (no action) */
		Q921ChangeState(trunk, Q921_STATE_TEI_UNASSIGNED, link->tei);

		/* TODO: hmm, request new one ? */
		res = Q921TeiSendAssignRequest(trunk);
	}
	return res;
}


/**
 * Q921TeiProcAssignRequest
 * \brief	Process assign request from peer (NT mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiProcAssignRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR offset = Q921_UFRAME_DATA_OFFSET(trunk);
	L2USHORT ri = 0;
	L2UCHAR tei = 0;

	if (!Q921_IS_PTMP_NT(trunk))	/* PTMP NT only */
		return 0;

	ri  = (mes[offset + 1] << 8) | mes[offset + 2];
	tei =  mes[offset + 4] >> 1;

	if(tei == Q921_TEI_BCAST) {
		int x;

		/* dynamically allocate TEI */
		for(x = Q921_TEI_DYN_MIN, tei = 0; x <= Q921_TEI_MAX; x++) {
			if(!trunk->tei_map[x]) {
				tei = x;
				break;
			}
		}
	}
	else if(!(tei > 0 && tei < Q921_TEI_DYN_MIN)) {
		/* reject TEIs that are not in the static area */
		Q921TeiSendDenyResponse(trunk, 0, ri);

		return 0;
	}

	if(!tei) {
		/* no free TEI found */
		Q921TeiSendDenyResponse(trunk, Q921_TEI_BCAST, ri);
	}
	else {
		struct Q921_Link *link = Q921_LINK_CONTEXT(trunk, tei);

		/* mark used */
		trunk->tei_map[tei] = 1;

		/* assign tei */
		link->tei = tei;

		/* put context in TEI ASSIGNED state */
		Q921ChangeState(trunk, Q921_STATE_TEI_ASSIGNED, tei);

		/* send assign response */
		Q921TeiSendAssignedResponse(trunk, tei, ri);

		/* Start T201 */
		Q921T201TimerStart(trunk, tei);
	}
	return 0;
}

/**
 * Q921TeiSendCheckRequest
 * \brief	Send check request (NT mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	tei	TEI to check
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiSendCheckRequest(L2TRUNK trunk, L2UCHAR tei)
{
	L2INT res = 0;

	if (!Q921_IS_PTMP_NT(trunk))	/* PTMP NT only */
		return 0;

	/* send TEI check request */
	res = Q921TeiSend(trunk, Q921_TEI_ID_CHECKREQ, 0, tei);

	/* (Re-)Start T201 timer */
	Q921T201TimerStart(trunk, tei);

	return res;
}

/**
 * Q921TeiProcCheckResponse
 * \brief	Process Check Response (NT mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiProcCheckResponse(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	struct Q921_Link *link;
	L2UCHAR offset = Q921_UFRAME_DATA_OFFSET(trunk);
	L2USHORT ri = 0;
	L2UCHAR tei = 0;

	if (!Q921_IS_PTMP_NT(trunk))	/* PTMP NT mode only */
		return 0;

	ri  = (mes[offset + 1] << 8) | mes[offset + 2];
	tei =  mes[offset + 4] >> 1;

	/* restart T201 */
	Q921T201TimerStop(trunk, tei);

	/* reset counter */
	link       = Q921_LINK_CONTEXT(trunk, tei);
	link->N202 = 0;

	if(!(tei > 0 && tei < Q921_TEI_MAX) || !trunk->tei_map[tei]) {
		/* TODO: Should we send a DISC first? */

		/* TEI not assigned? Invalid TEI? */
		Q921TeiSendRemoveRequest(trunk, tei);

		/* change state */
		Q921ChangeState(trunk, Q921_STATE_STOPPED, tei);

		/* clear */
		memset(link, 0, sizeof(struct Q921_Link));
	} else {
		/* Start T201 */
		Q921T201TimerStart(trunk, tei);
	}

	return 0;
}


/**
 * Q921TeiProcVerifyRequest
 * \brief	Process Verify Request (NT mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	mes	pointer to message buffer
 * \param[in]	size	size of message
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiProcVerifyRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	L2UCHAR resp[25];
	L2UCHAR offset = Q921_UFRAME_DATA_OFFSET(trunk);
	L2UCHAR tei = 0;

	if (!Q921_IS_PTMP_NT(trunk))	/* PTMP NT mode only */
		return 0;

	tei = mes[offset + 4] >> 1;

	/* todo: handle response... verify assigned TEI */
	resp[offset + 0] = 0;

	return 0;
}

/**
 * Q921TeiSendDenyResponse
 * \brief	Send Deny Response (NT mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiSendDenyResponse(L2TRUNK trunk, L2UCHAR tei, L2USHORT ri)
{
	if (!Q921_IS_PTMP_NT(trunk))	/* PTMP NT only */
		return 0;

	return Q921TeiSend(trunk, Q921_TEI_ID_DENIED, ri, tei);
}


/**
 * Q921TeiSendAssignedResponse
 * \brief	Send Assigned Response (NT mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	tei	TEI to assign
 * \param[in]	ri	RI of request
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiSendAssignedResponse(L2TRUNK trunk, L2UCHAR tei, L2USHORT ri)
{
	if (!Q921_IS_PTMP_NT(trunk))	/* PTMP NT only */
		return 0;

	return Q921TeiSend(trunk, Q921_TEI_ID_ASSIGNED, ri, tei);
}

/**
 * Q921TeiSendRemoveRequest
 * \brief	Send Remove Request (NT mode only)
 * \param[in]	trunk	pointer to Q921 data struct
 * \param[in]	tei	TEI to remove
 * \return	> 0 on success; <= 0 on error
 *
 * \author	Stefan Knoblich
 */
static int Q921TeiSendRemoveRequest(L2TRUNK trunk, L2UCHAR tei)
{
	if (!Q921_IS_PTMP_NT(trunk))	/* PTMP NT only */
		return 0;

	return Q921TeiSend(trunk, Q921_TEI_ID_REMOVE, 0, tei);
}
