/*****************************************************************************

  FileName:     Q921priv.h

  Description:  Private declarations

  Created:      08.Aug.2008/STKN

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Copyright (c) 2008, Stefan Knoblich, axsentis GmbH. All rights reserved.
  email:s.knoblich@axsentis.de  

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
#ifndef _Q921_PRIV_H_
#define _Q921_PRIV_H_

#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif
#if (_MSC_VER >= 1400)			/* VC8+ */
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif
#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif
#ifndef strncasecmp
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

typedef enum			/* Q.921 States */
{
	Q921_STATE_STOPPED = 0,			/* Trunk stopped */
	Q921_STATE_TEI_UNASSIGNED = 1,		/* TEI unassigned */
	Q921_STATE_TEI_AWAITING,		/* Assign awaiting TEI */
	Q921_STATE_TEI_ESTABLISH,		/* Establish awaiting TEI */
	Q921_STATE_TEI_ASSIGNED,		/* TEI assigned */
	Q921_STATE_AWAITING_ESTABLISHMENT,	/* Awaiting establishment */
	Q921_STATE_AWAITING_RELEASE,		/* Awaiting release */
	Q921_STATE_MULTIPLE_FRAME_ESTABLISHED,	/* Multiple frame established */
	Q921_STATE_TIMER_RECOVERY		/* Timer recovery */
} Q921State_t;

/*
 * Flags
 */
enum Q921_Flags {
	Q921_FLAG_L3_INITIATED = (1 << 0),

	Q921_FLAG_UI_FRAME_QUEUED = (1 << 1),
	Q921_FLAG_I_FRAME_QUEUED  = (1 << 2),

	Q921_FLAG_ACK_PENDING = (1 << 3),
	Q921_FLAG_REJECT      = (1 << 4),

	Q921_FLAG_RECV_BUSY      = (1 << 5),
	Q921_FLAG_PEER_RECV_BUSY = (1 << 6)
};

#define Q921_SET_FLAG(x, f)	((x)->flags |= f)
#define Q921_CHECK_FLAG(x, f)	((x)->flags & f)
#define Q921_CLEAR_FLAG(x, f)	((x)->flags &= ~f)


/*
 * dynamic TEI handling
 */
#define Q921_SAPI_TEI		63	/* SAPI for all TEI Messages */
#define Q921_LAYER_ENT_ID_TEI	0x0f	/* UN Layer Management Entity ID for TEI Mgmt */
#define Q921_LAYER_ENT_ID_Q931	0x08	/* Q.931 Layer Management Entity ID */


typedef enum {
	Q921_TEI_ID_REQUEST = 1,
	Q921_TEI_ID_ASSIGNED,
	Q921_TEI_ID_DENIED,
	Q921_TEI_ID_CHECKREQ,
	Q921_TEI_ID_CHECKRESP,
	Q921_TEI_ID_REMOVE,
	Q921_TEI_ID_VERIFY
} Q921TeiMessageType_t;


/**
 * Per-Datalink context
 */
struct Q921_Link {
	L2UCHAR tei;		/*!< This endpoint's TEI */

	L2UCHAR va;
	L2UCHAR vs;
	L2UCHAR vr;

	L2INT flags;
	Q921State_t state;

	L2ULONG N202;		/*!< PTMP TE mode retransmit counter */
	L2ULONG N200;		/*!< retransmit counter (per-TEI in PTMP NT mode) */

	L2ULONG TM01;		/*!< Datalink inactivity disconnect timer */

	L2ULONG T200;
	L2ULONG T201;		/*!< PTMP NT mode timer */
	L2ULONG T203;

	L2USHORT ri;		/*!< random id for TEI request mgmt */

	/* I + UI Frame queue */
	L2UCHAR UIFrameQueue[Q921MAXHDLCSPACE];
	L2UCHAR  IFrameQueue[Q921MAXHDLCSPACE];
	L2UCHAR  IFrameResendQueue[Q921MAXHDLCSPACE];
};


#define Q921_LINK_CONTEXT(tr, tei) \
	(Q921_IS_PTMP_NT(tr) && tei != Q921_TEI_BCAST) ? ((struct Q921_Link *)&(tr)->context[tei]) : (tr)->context

#define Q921_TRUNK_CONTEXT(tr) \
	(tr)->context

#define Q921_LOGBUFSIZE		2000
#define INITIALIZED_MAGIC	42

/*
 * Helper macros
 */
#define Q921_INC_COUNTER(x)		(x = (x + 1) % 128)
#define Q921_DEC_COUNTER(x)		(x = (x) ? (x - 1) : 127)

#define Q921_UFRAME_HEADER_SIZE		3
#define Q921_UFRAME_DATA_OFFSET(tr)	((tr)->Q921HeaderSpace + Q921_UFRAME_HEADER_SIZE)

#define Q921_SFRAME_HEADER_SIZE		4
#define Q921_SFRAME_DATA_OFFSET(tr)	((tr)->Q921HeaderSpace + Q921_SFRAME_HEADER_SIZE)

#define Q921_IFRAME_HEADER_SIZE		4
#define Q921_IFRAME_DATA_OFFSET(tr)	((tr)->Q921HeaderSpace + Q921_IFRAME_HEADER_SIZE)

#define Q921_IS_TE(x)			((x)->NetUser == Q921_TE)
#define Q921_IS_NT(x)			((x)->NetUser == Q921_NT)

#define Q921_IS_STOPPED(tr)		((tr)->state == Q921_STATE_STOPPED)

/* TODO: rework this one */
#define Q921_IS_READY(tr)		((tr)->state >= Q921_STATE_TEI_ASSIGNED)

#define Q921_IS_PTMP(x)			((x)->NetType == Q921_PTMP)
#define Q921_IS_PTMP_TE(x)		((x)->NetType == Q921_PTMP && (x)->NetUser == Q921_TE)
#define Q921_IS_PTMP_NT(x)		((x)->NetType == Q921_PTMP && (x)->NetUser == Q921_NT)

#define Q921_IS_PTP(x)			((x)->NetType == Q921_PTP)
#define Q921_IS_PTP_TE(x)		((x)->NetType == Q921_PTP && (x)->NetUser == Q921_TE)
#define Q921_IS_PTP_NT(x)		((x)->NetType == Q921_PTP && (x)->NetUser == Q921_NT)

/* Make life a little easier */
#define Q921_COMMAND(x)			((x)->NetUser == Q921_TE ? 0 : 1)
#define Q921_RESPONSE(x)		((x)->NetUser == Q921_TE ? 1 : 0)

#define Q921_IS_COMMAND(tr, x)		((x) == (Q921_IS_TE(tr) ? 1 : 0))
#define Q921_IS_RESPONSE(tr, x)		((x) == (Q921_IS_TE(tr) ? 0 : 1))


/*******************************************************************************
 * Private functions
 *******************************************************************************/

/*
 * L1 / L2 Interface
 */
static int Q921Tx21Proc(L2TRUNK trunk, L2UCHAR *Msg, L2INT size);
static int Q921Tx23Proc(L2TRUNK trunk, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR *Msg, L2INT size);


/*
 * Timers
 */
static L2ULONG Q921GetTime(void);

static void Q921T200TimerStart(L2TRUNK trunk, L2UCHAR tei);
static void Q921T200TimerStop(L2TRUNK trunk, L2UCHAR tei);
static void Q921T200TimerReset(L2TRUNK trunk, L2UCHAR tei);
static void Q921T200TimerExpire(L2TRUNK trunk, L2UCHAR tei);

static void Q921T201TimerStart(L2TRUNK trunk, L2UCHAR tei);
static void Q921T201TimerStop(L2TRUNK trunk, L2UCHAR tei);
/* static void Q921T201TimerReset(L2TRUNK trunk, L2UCHAR tei); - Unused for now */
static void Q921T201TimerExpire(L2TRUNK trunk, L2UCHAR tei);

static void Q921T202TimerStart(L2TRUNK trunk);
static void Q921T202TimerStop(L2TRUNK trunk);
static void Q921T202TimerReset(L2TRUNK trunk);
static void Q921T202TimerExpire(L2TRUNK trunk);

static void Q921T203TimerStart(L2TRUNK trunk, L2UCHAR tei);
static void Q921T203TimerStop(L2TRUNK trunk, L2UCHAR tei);
static void Q921T203TimerReset(L2TRUNK trunk, L2UCHAR tei);
static void Q921T203TimerExpire(L2TRUNK trunk, L2UCHAR tei);

static void Q921TM01TimerStart(L2TRUNK trunk, L2UCHAR tei);
/* static void Q921TM01TimerStop(L2TRUNK trunk, L2UCHAR tei); - Unused for now */
static void Q921TM01TimerReset(L2TRUNK trunk, L2UCHAR tei);
/* static void Q921TM01TimerExpire(L2TRUNK trunk, L2UCHAR tei); - Unused for now */

/*
 * Frame encoding
 */
static int Q921SendS(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR sv, L2UCHAR *mes, L2INT size);
static int Q921SendU(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR m, L2UCHAR *mes, L2INT size);

static int Q921SendRNR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
static int Q921SendREJ(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
static int Q921SendSABME(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
static int Q921SendDM(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
static int Q921SendDISC(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
static int Q921SendUA(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
static int Q921SendUN(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf, L2UCHAR *mes, L2INT size);
static int Q921SendRR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);

/*
 * Frame decoding
 */
static int Q921ProcIFrame(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcSFrame(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcUFrame(L2TRUNK trunk, L2UCHAR *mes, L2INT size);

static int Q921ProcSABME(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcDM(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcUA(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcDISC(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcRR(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcRNR(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921ProcREJ(L2TRUNK trunk, L2UCHAR *mes, L2INT size);


/*
 * (Common) procedures defined in the Q.921 SDL
 */
static int Q921SendEnquiry(L2TRUNK trunk, L2UCHAR tei);
static int Q921SendEnquiryResponse(L2TRUNK trunk, L2UCHAR tei);
static void Q921ResetExceptionConditions(L2TRUNK trunk, L2UCHAR tei);
static int Q921EstablishDataLink(L2TRUNK trunk, L2UCHAR tei);
static int Q921NrErrorRecovery(L2TRUNK trunk, L2UCHAR tei);
static int Q921InvokeRetransmission(L2TRUNK trunk, L2UCHAR tei, L2UCHAR nr);
static int Q921AcknowledgePending(L2TRUNK trunk, L2UCHAR tei);
/*
static int Q921SetReceiverBusy(L2TRUNK trunk);
static int Q921ClearReceiverBusy(L2TRUNK trunk);
*/

/*
 * Queueing
 */
static int Q921SendQueuedIFrame(L2TRUNK trunk, L2UCHAR tei);
static int Q921EnqueueI(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR *mes, L2INT size);

/*
 * TEI management
 */
static int Q921TeiSendAssignRequest(L2TRUNK trunk);
static int Q921TeiProcAssignResponse(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921TeiSendVerifyRequest(L2TRUNK trunk);
static int Q921TeiProcCheckRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921TeiProcRemoveRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921TeiProcAssignRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921TeiProcCheckResponse(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921TeiProcVerifyRequest(L2TRUNK trunk, L2UCHAR *mes, L2INT size);
static int Q921TeiSendRemoveRequest(L2TRUNK trunk, L2UCHAR tei);
static int Q921TeiSendDenyResponse(L2TRUNK trunk, L2UCHAR tei, L2USHORT ri);
static int Q921TeiSendAssignedResponse(L2TRUNK trunk, L2UCHAR tei, L2USHORT ri);
static int Q921TeiSendCheckRequest(L2TRUNK trunk, L2UCHAR tei);

/*
 * Logging
 */
static int Q921Log(L2TRUNK trunk, Q921LogLevel_t level, const char *fmt, ...);
static int Q921LogMesg(L2TRUNK trunk, Q921LogLevel_t level, L2UCHAR received, L2UCHAR *mes, L2INT size, const char *fmt, ...);

/*
 * State handling
 */
static int Q921ChangeState(L2TRUNK trunk, Q921State_t state, L2UCHAR tei);

#endif
