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
                                    deteceted.

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

#ifndef _Q921
#define _Q921

#define Q921MAXHDLCSPACE 3000
#define L2UCHAR		unsigned char		/* Min 8 bit						*/
#define L2INT       int                 /* Min 16 bit signed                */
#define L2ULONG		unsigned long		/* Min 32 bit						*/
#define L2TRUNK		Q921Data_t *

typedef enum					/* Network/User Mode.                   */
{
	Q921_TE=0,                  /*  0 : User Mode                       */
    Q921_NT=1                   /*  1 : Network Mode                    */
} Q921NetUser_t;

typedef struct Q921Data Q921Data_t;
typedef int (*Q921TxCB_t) (void *, L2UCHAR *, L2INT);

#define INITIALIZED_MAGIC 42
struct Q921Data
{
    L2UCHAR HDLCInQueue[Q921MAXHDLCSPACE];
	L2INT initialized;
	L2UCHAR va;
    L2UCHAR vs;
    L2UCHAR vr;
    L2INT state;
	L2UCHAR sapi;
	L2UCHAR tei;
	Q921NetUser_t NetUser;
	L2ULONG T200;
	L2ULONG T203;
	L2ULONG T200Timeout;
	L2ULONG T203Timeout;
	Q921TxCB_t Q921Tx21Proc;
	Q921TxCB_t Q921Tx23Proc;
	void *PrivateData21;
	void *PrivateData23;
	L2INT Q921HeaderSpace;

};

void Q921_InitTrunk(L2TRUNK trunk,
					L2UCHAR sapi,
					L2UCHAR tei,
					Q921NetUser_t NetUser,
					L2INT hsize,
					Q921TxCB_t cb21,
					Q921TxCB_t cb23,
					void *priv21,
					void *priv23);
int Q921QueueHDLCFrame(L2TRUNK trunk, L2UCHAR *b, L2INT size);
int Q921Rx12(L2TRUNK trunk);
int Q921Rx32(L2TRUNK trunk, L2UCHAR * Mes, L2INT Size);
int Q921Start(L2TRUNK trunk);
void Q921SetGetTimeCB(L2ULONG (*callback)(void));
void Q921TimerTick(L2TRUNK trunk);

int Q921Tx21Proc(L2TRUNK trunk, L2UCHAR *Msg, L2INT size);
int Q921Tx23Proc(L2TRUNK trunk, L2UCHAR *Msg, L2INT size);
extern L2ULONG tLast;
L2ULONG Q921GetTime(void);
void Q921T200TimerStart(L2TRUNK trunk);
void Q921T200TimerStop(L2TRUNK trunk);
void Q921T200TimerReset(L2TRUNK trunk);
void Q921T203TimerStart(L2TRUNK trunk);
void Q921T203TimerStop(L2TRUNK trunk);
void Q921T203TimerReset(L2TRUNK trunk);
void Q921T200TimerExpire(L2TRUNK trunk);
void Q921T203TimerExpire(L2TRUNK trunk);
int Q921SendI(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR *mes, L2INT size);
int Q921SendRNR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
int Q921SendREJ(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
int Q921SendSABME(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
int Q921SendDM(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
int Q921SendDISC(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
int Q921SendUA(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);
int Q921ProcSABME(L2TRUNK trunk, L2UCHAR *mes, L2INT size);

#endif

