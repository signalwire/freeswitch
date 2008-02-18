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
#include "Q921.h"
#include <stdlib.h>
#include <stdio.h>
#include "mfifo.h"

int Q921SendRR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf);

/*****************************************************************************

  Function:     Q921_InitTrunk

  Decription:   Initialize a Q.921 trunk so it is ready for use. This 
                function MUST be called before you call any other functions.

*****************************************************************************/
void Q921_InitTrunk(L2TRUNK trunk,
					L2UCHAR sapi,
					L2UCHAR tei,
					Q921NetUser_t NetUser,
					L2INT hsize,
					Q921TxCB_t cb21,
					Q921TxCB_t cb23,
					void *priv21,
					void *priv23)
{
	if (trunk->initialized != INITIALIZED_MAGIC) {
		MFIFOCreate(trunk->HDLCInQueue, Q921MAXHDLCSPACE, 10);
		trunk->initialized = INITIALIZED_MAGIC;
	}
	trunk->va = 0;
	trunk->vs = 0;	
	trunk->vr = 0;
	trunk->state = 0;
	trunk->sapi = sapi;
	trunk->tei = tei;
	trunk->NetUser = NetUser;
	trunk->Q921Tx21Proc = cb21;
	trunk->Q921Tx23Proc = cb23;
	trunk->PrivateData21 = priv21;
	trunk->PrivateData23 = priv23;
	trunk->Q921HeaderSpace = hsize;
	trunk->T200 = 0;
	trunk->T203 = 0;
	trunk->T200Timeout = 1000;
	trunk->T203Timeout = 10000;
}

int Q921Tx21Proc(L2TRUNK trunk, L2UCHAR *Msg, L2INT size)
{
	return trunk->Q921Tx21Proc(trunk->PrivateData21, Msg, size);
}

int Q921Tx23Proc(L2TRUNK trunk, L2UCHAR *Msg, L2INT size)
{
	return trunk->Q921Tx23Proc(trunk->PrivateData23, Msg, size);

}

/*****************************************************************************

  Function:     Q921TimeTick

  Description:  Called periodically from an external source to allow the 
                stack to process and maintain it's own timers.

  Return Value: none

*****************************************************************************/
L2ULONG (*Q921GetTimeProc) (void) = NULL; /* callback for func reading time in ms */
L2ULONG tLast = {0};

L2ULONG Q921GetTime(void)
{
    L2ULONG tNow = 0;
    if(Q921GetTimeProc != NULL)
    {
        tNow = Q921GetTimeProc();
        if(tNow < tLast)    /* wrapped */
        {
			/* TODO */
        }
		tLast = tNow;
    }
    return tNow;
}

void Q921T200TimerStart(L2TRUNK trunk)
{
	if (!trunk->T200) {
		trunk->T200 = Q921GetTime() + trunk->T200Timeout;
	}
}

void Q921T200TimerStop(L2TRUNK trunk)
{
	trunk->T200 = 0;
}

void Q921T200TimerReset(L2TRUNK trunk)
{
	Q921T200TimerStop(trunk);
	Q921T200TimerStart(trunk);
}

void Q921T203TimerStart(L2TRUNK trunk)
{
	if (!trunk->T203) {
		trunk->T203 = Q921GetTime() + trunk->T203Timeout;
	}
}

void Q921T203TimerStop(L2TRUNK trunk)
{
	trunk->T203 = 0;
}

void Q921T203TimerReset(L2TRUNK trunk)
{
	Q921T203TimerStop(trunk);
	Q921T203TimerStart(trunk);
}

void Q921T200TimerExpire(L2TRUNK trunk)
{
	(void)trunk;
}

void Q921T203TimerExpire(L2TRUNK trunk)
{
	Q921T203TimerReset(trunk);
	Q921SendRR(trunk, trunk->sapi, trunk->NetUser == Q921_TE ? 0 : 1, trunk->tei, 1);
}

void Q921TimerTick(L2TRUNK trunk)
{
	L2ULONG tNow = Q921GetTime();
	if (trunk->T200 && tNow > trunk->T200) {
		Q921T200TimerExpire(trunk);
	}
	if (trunk->T203 && tNow > trunk->T203) {
		Q921T203TimerExpire(trunk);		
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

/*****************************************************************************

  Function:     Q921SendI

  Description:  Compose and Send I Frame to layer. Will receive an I frame
                with space for L2 header and fill out that header before
                it call Q921Tx21Proc.

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P fiels octet 5
                mes         ptr to I frame message.
                size        size of message in bytes.

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
int Q921SendI(L2TRUNK trunk, L2UCHAR Sapi, char cr, L2UCHAR Tei, char pf, L2UCHAR *mes, L2INT size)
{
    mes[trunk->Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[trunk->Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[trunk->Q921HeaderSpace+2] = trunk->vs<<1;
    mes[trunk->Q921HeaderSpace+3] = (trunk->vr<<1) | (pf & 0x01);
    trunk->vs++;

    return Q921Tx21Proc(trunk, mes, size);
}

int Q921Rx32(L2TRUNK trunk, L2UCHAR * Mes, L2INT Size)
{
	return Q921SendI(trunk, 
					trunk->sapi, 
					trunk->NetUser == Q921_TE ? 0 : 1,
					trunk->tei, 
					0, 
					Mes, 
					Size);
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

int Q921SendRR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
    L2UCHAR mes[400];

    mes[trunk->Q921HeaderSpace+0] = (L2UCHAR)((Sapi&0xfc) | ((cr<<1)&0x02));
    mes[trunk->Q921HeaderSpace+1] = (L2UCHAR)((Tei<<1) | 0x01);
    mes[trunk->Q921HeaderSpace+2] = (L2UCHAR)0x01;
    mes[trunk->Q921HeaderSpace+3] = (L2UCHAR)((trunk->vr<<1) | (pf & 0x01));

    return Q921Tx21Proc(trunk, mes, trunk->Q921HeaderSpace+4);
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
int Q921SendRNR(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
    L2UCHAR mes[400];

    mes[trunk->Q921HeaderSpace+0] = (L2UCHAR)((Sapi&0xfc) | ((cr<<1)&0x02));
    mes[trunk->Q921HeaderSpace+1] = (L2UCHAR)((Tei<<1) | 0x01);
    mes[trunk->Q921HeaderSpace+2] = (L2UCHAR)0x05;
    mes[trunk->Q921HeaderSpace+3] = (L2UCHAR)((trunk->vr<<1) | (pf & 0x01));

    return Q921Tx21Proc(trunk, mes, trunk->Q921HeaderSpace+4);
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
int Q921SendREJ(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
    L2UCHAR mes[400];

    mes[trunk->Q921HeaderSpace+0] = (L2UCHAR)((Sapi&0xfc) | ((cr<<1)&0x02));
    mes[trunk->Q921HeaderSpace+1] = (L2UCHAR)((Tei<<1) | 0x01);
    mes[trunk->Q921HeaderSpace+2] = (L2UCHAR)0x09;
    mes[trunk->Q921HeaderSpace+3] = (L2UCHAR)((trunk->vr<<1) | (pf & 0x01));

    return Q921Tx21Proc(trunk, mes, trunk->Q921HeaderSpace+4);
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
int Q921SendSABME(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
    L2UCHAR mes[400];

    mes[trunk->Q921HeaderSpace+0] = (L2UCHAR)((Sapi&0xfc) | ((cr<<1)&0x02));
    mes[trunk->Q921HeaderSpace+1] = (L2UCHAR)((Tei<<1) | 0x01);
    mes[trunk->Q921HeaderSpace+2] = (L2UCHAR)(0x6f | ((pf<<4)&0x10));

    return Q921Tx21Proc(trunk, mes, trunk->Q921HeaderSpace+3);
}

int Q921Start(L2TRUNK trunk)
{
	return Q921SendSABME(trunk, 
					trunk->sapi, 
					trunk->NetUser == Q921_TE ? 0 : 1,
					trunk->tei, 
					1);
}

/*****************************************************************************

  Function:     Q921SendDM

  Description:  Comose and Send DM (Disconnected Mode)

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          F fiels octet 4

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
int Q921SendDM(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
    L2UCHAR mes[400];

    mes[trunk->Q921HeaderSpace+0] = (L2UCHAR)((Sapi&0xfc) | ((cr<<1)&0x02));
    mes[trunk->Q921HeaderSpace+1] = (L2UCHAR)((Tei<<1) | 0x01);
    mes[trunk->Q921HeaderSpace+2] = (L2UCHAR)(0x0f | ((pf<<4)&0x10));

    return Q921Tx21Proc(trunk, mes, trunk->Q921HeaderSpace+3);
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
int Q921SendDISC(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
    L2UCHAR mes[400];

    mes[trunk->Q921HeaderSpace+0] = (L2UCHAR)((Sapi&0xfc) | ((cr<<1)&0x02));
    mes[trunk->Q921HeaderSpace+1] = (L2UCHAR)((Tei<<1) | 0x01);
    mes[trunk->Q921HeaderSpace+2] = (L2UCHAR)(0x43 | ((pf<<4)&0x10));

    return Q921Tx21Proc(trunk, mes, trunk->Q921HeaderSpace+3);
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
int Q921SendUA(L2TRUNK trunk, int Sapi, int cr, int Tei, int pf)
{
    L2UCHAR mes[400];

    mes[trunk->Q921HeaderSpace+0] = (L2UCHAR)((Sapi&0xfc) | ((cr<<1)&0x02));
    mes[trunk->Q921HeaderSpace+1] = (L2UCHAR)((Tei<<1) | 0x01);
    mes[trunk->Q921HeaderSpace+2] = (L2UCHAR)(0x63 | ((pf<<4)&0x10));

    return Q921Tx21Proc(trunk, mes, trunk->Q921HeaderSpace+3);
}

int Q921ProcSABME(L2TRUNK trunk, L2UCHAR *mes, L2INT size)
{
	/* TODO:  Do we need these paramaters? */
	(void)mes;
	(void)size;

    trunk->vr=0;
    trunk->vs=0;
	trunk->va=0;

    return 1;
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
    L2UCHAR *mes;
    L2INT rs,size;     /* receive size & Q921 frame size*/
    L2UCHAR *smes = MFIFOGetMesPtr(trunk->HDLCInQueue, &size);
	L2UCHAR nr;

    if(smes != NULL)
    {
        rs = size - trunk->Q921HeaderSpace;
        mes = &smes[trunk->Q921HeaderSpace];

		if ((mes[2] & 3) != 3) {
			/* we have an S or I frame */
			/* if nr is between va and vs, update our va counter */
			nr = (mes[3] >> 1);
			if (nr >= trunk->va && nr <= trunk->vs) {
				trunk->va = nr;
			}
		}

        /* check for I frame */
        if((mes[2] & 0x01) == 0)
        {

			/* we increment before we "really" know its good so that if we send in the callback, we use the right nr */
			trunk->vr++;
            if(Q921Tx23Proc(trunk, smes, size-2)) /* -2 to clip away CRC */
            {
				Q921SendRR(trunk, (mes[0]&0xfc)>>2, (mes[0]>>1)&0x01, mes[1]>>1, mes[3]&0x01);
			}
            else
            {
                /* todo: whatever*/
				/* trunk->vr--; */
				/* for now, lets just respond for easier debugging on errors */
				Q921SendRR(trunk, (mes[0]&0xfc)>>2, (mes[0]>>1)&0x01, mes[1]>>1, mes[3]&0x01);
           }
        }

        /* check for RR */
        else if(mes[2] ==0x01)
		{
			if (((mes[0]>>1)&0x01) == (trunk->NetUser == Q921_TE ? 1 : 0)) { /* if this is a command */
				Q921SendRR(trunk, (mes[0]&0xfc)>>2, (mes[0]>>1)&0x01, mes[1]>>1, mes[2]&0x01);
			}

        }
		
        /* check for RNR */
        /* check for REJ */
        /* check for SABME */
        else if((mes[2] & 0xef) == 0x6f)
        {
            Q921ProcSABME(trunk, mes, rs);
            Q921SendUA(trunk, (mes[0]&0xfc)>>2, (mes[0]>>1)&0x01,mes[1]>>1, (mes[2]&0x10)>>4);
        }

        /* check for DM */
        /* check for UI */
        /* check for DISC */
        /* check for UA */
        /* check for FRMR */
        /* check for XID */

        else
        {
            /* what the ? Issue an error */
			/* Q921ErrorProc(trunk, Q921_UNKNOWNFRAME, mes, rs); */
            /* todo: REJ or FRMR */
        }

        MFIFOKillNext(trunk->HDLCInQueue);

        return 1;
    }
    return 0;
}

