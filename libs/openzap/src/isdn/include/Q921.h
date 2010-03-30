/*****************************************************************************

  FileName:     q921.h

  Description:  Contains headers of a Q.921 protocol.

  Note:         This header file is the only include file that should be 
                acessed by users of the Q.921 stack.

  Interface:    The Q.921 stack contains 2 layers. 

                -   One interface layer.
                -   One driver layer.

                The interface layer contains the interface functions required 
                for a layer 2 stack to be able to send and receive messages.

                The driver layer will simply feed bytes into the ship as
                required and queue messages received out from the ship.

                Q921TimeTick        The Q.921 like any other blackbox 
                                    modules contains no thread by it's own
                                    and must therefore be called regularly 
                                    by an external 'thread' to do maintenance
                                    etc.

                Q921Rx32            Receive message from layer 3. Called by
                                    the layer 3 stack to send a message.


				NOTE: The following are not yet implemented

                OnQ921Error         Function called every if an error is 
                                    detected.

                OnQ921Log           Function called if logging is active.


                <TODO> Maintenance/Configuration interface
				<TODO> Logging
				<TODO> DL_ message passing to layer 3
				<TODO> Timers
				<TODO> Api commands to tell 921 to stop and start for a trunk

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
 * - June,July 2008: Stefan Knoblich <s.knoblich@axsentis.de>:
 *     Add PTMP TEI management
 *     Add timers
 *     Add retransmit counters
 *     Add logging
 *     Various cleanups
 *
 ****************************************************************************/

#ifndef _Q921
#define _Q921

#define Q921MAXHDLCSPACE 3000
#define L2UCHAR		unsigned char		/* Min 8 bit			*/
#define L2USHORT	unsigned short		/* 16 bit			*/
#define L2INT		int			/* Min 16 bit signed		*/
#define L2ULONG		unsigned long		/* Min 32 bit			*/
#define L2TRUNK		Q921Data_t *

#define Q921_TEI_BCAST		127
#define Q921_TEI_MAX		Q921_TEI_BCAST

#define Q921_TEI_DYN_MIN	64
#define Q921_TEI_DYN_MAX	126


typedef enum			/* Network/User Mode		*/
{
	Q921_TE=0,		/*  0 : User Mode		*/
	Q921_NT=1		/*  1 : Network Mode		*/
} Q921NetUser_t;

typedef enum			/* Type of connection		*/
{
	Q921_PTP=0,		/* 0 : Point-To-Point		*/
	Q921_PTMP=1		/* 1 : Point-To-Multipoint	*/
} Q921NetType_t;

typedef enum
{
	Q921_LOG_NONE = -1,
	Q921_LOG_EMERG = 0,
	Q921_LOG_ALERT,
	Q921_LOG_CRIT,
	Q921_LOG_ERROR,
	Q921_LOG_WARNING,
	Q921_LOG_NOTICE,
	Q921_LOG_INFO,
	Q921_LOG_DEBUG
} Q921LogLevel_t;


/*
 * Messages for L2 <-> L3 communication
 */
typedef enum {
	Q921_DL_ESTABLISH = 0,
	Q921_DL_ESTABLISH_CONFIRM,
	Q921_DL_RELEASE,
	Q921_DL_RELEASE_CONFIRM,
	Q921_DL_DATA,
	Q921_DL_UNIT_DATA
} Q921DLMsg_t;

typedef int (*Q921Tx21CB_t) (void *, L2UCHAR *, L2INT);
typedef int (*Q921Tx23CB_t) (void *, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR *, L2INT);
typedef int (*Q921LogCB_t) (void *, Q921LogLevel_t, char *, L2INT);

struct Q921_Link;

typedef struct Q921Data
{
	L2INT initialized;

	L2UCHAR sapi;			/*!< User assigned SAPI */
	L2UCHAR tei;			/*!< User assigned TEI value */

	L2INT Q921HeaderSpace;
	Q921NetUser_t NetUser;
	Q921NetType_t NetType;

	struct Q921_Link *context;	/*!< per-TEI / link context space */

	/* timers */
	L2ULONG T202;			/*!< PTMP TE mode TEI retransmit timer */
	L2ULONG T200Timeout;
	L2ULONG T201Timeout;
	L2ULONG T202Timeout;
	L2ULONG T203Timeout;

	L2ULONG TM01Timeout;

	/* counters */
	L2ULONG N200Limit;		/*!< max retransmit */

	L2ULONG N202;			/*!< PTMP TE mode retransmit counter */
	L2ULONG N202Limit;		/*!< PTMP TE mode max retransmit */

	L2ULONG N201Limit;		/*!< max number of octets */
	L2ULONG k;			/*!< max number of unacknowledged I frames */

	/* callbacks and callback data pointers */
	Q921Tx21CB_t Q921Tx21Proc;
	Q921Tx23CB_t Q921Tx23Proc;
	void *PrivateData21;
	void *PrivateData23;

	/* logging */
	Q921LogLevel_t	loglevel;	/*!< trunk loglevel */
	Q921LogCB_t	Q921LogProc;	/*!< log callback procedure */
	void *PrivateDataLog;		/*!< private data pointer for log proc */

	/* tei mgmt */
	L2UCHAR tei_map[Q921_TEI_MAX];	/*!< */

	L2UCHAR HDLCInQueue[Q921MAXHDLCSPACE];	/*!< HDLC input queue */
} Q921Data_t;

/*
 * Public functions
 */
int Q921_InitTrunk(L2TRUNK trunk,
					L2UCHAR sapi,
					L2UCHAR tei,
					Q921NetUser_t NetUser,
					Q921NetType_t NetType,
					L2INT hsize,
					Q921Tx21CB_t cb21,
					Q921Tx23CB_t cb23,
					void *priv21,
					void *priv23);
int Q921Start(L2TRUNK trunk);
int Q921Stop(L2TRUNK trunk);

void Q921SetLogCB(L2TRUNK trunk, Q921LogCB_t func, void *priv);
void Q921SetLogLevel(L2TRUNK trunk, Q921LogLevel_t level);

int Q921Rx12(L2TRUNK trunk);
int Q921Rx32(L2TRUNK trunk, Q921DLMsg_t ind, L2UCHAR tei, L2UCHAR * Mes, L2INT Size);

int Q921QueueHDLCFrame(L2TRUNK trunk, L2UCHAR *b, L2INT size);

void Q921SetGetTimeCB(L2ULONG (*callback)(void));
void Q921TimerTick(L2TRUNK trunk);

#endif
