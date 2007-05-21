/*****************************************************************************

  FileName:     q921.c

  Description:  Contains the implementation of a Q.921 protocol on top of the
                Comet Driver.

                Most of the work required to execute a Q.921 protocol is 
                taken care of by the Comet ship and it's driver. This layer
                will simply configure and make use of these features to 
                complete a Q.921 implementation.

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
#include "q921.h"
#include <stdlib.h>
#include "mfifo.h"

/*****************************************************************************
  Global Tables & Variables.
*****************************************************************************/
Q921Data Q921DevSpace[Q921MAXTRUNK];
int Q921HeaderSpace={0};

int (*Q921Tx21Proc)(int dev, unsigned char *, int)={NULL};
int (*Q921Tx23Proc)(int dev, unsigned char *, int)={NULL};

/*****************************************************************************

  Function:     Q921Init

  Decription:   Initialize the Q.921 stack so it is ready for use. This 
                function MUST be called as part of initializing the 
                application.

*****************************************************************************/
void Q921Init()
{
    int x;
    for(x=0; x<Q921MAXTRUNK;x++)
    {
        MFIFOCreate(Q921DevSpace[x].HDLCInQueue, Q921MAXHDLCSPACE, 10);
        Q921DevSpace[x].vr=0;
        Q921DevSpace[x].vs=0;
        Q921DevSpace[x].state=0;;
    }
}

void Q921SetHeaderSpace(int hspace)
{
    Q921HeaderSpace=hspace;
}

void Q921SetTx21CB(int (*callback)(int dev, unsigned char *, int))
{
    Q921Tx21Proc = callback;
}

void Q921SetTx23CB(int (*callback)(int dev, unsigned char *, int))
{
    Q921Tx23Proc = callback;
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
int Q921QueueHDLCFrame(int trunk, unsigned char *b, int size)
{
    return MFIFOWriteMes(Q921DevSpace[trunk].HDLCInQueue, b, size);
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
int Q921SendI(int trunk, unsigned char Sapi, char cr, unsigned char Tei, char pf, unsigned char *mes, int size)
{
    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = Q921DevSpace[trunk].vs<<1;
    mes[Q921HeaderSpace+3] = (Q921DevSpace[trunk].vr<<1) | (pf & 0x01);
    Q921DevSpace[trunk].vs++;

    return Q921Tx21Proc(trunk, mes, size);
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

int Q921SendRR(int trunk, int Sapi, int cr, int Tei, int pf)
{
    unsigned char mes[400];

    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = 0x01;
    mes[Q921HeaderSpace+3] = (Q921DevSpace[trunk].vr<<1) | (pf & 0x01);

    return Q921Tx21Proc(trunk, mes, Q921HeaderSpace+4);
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
int Q921SendRNR(int trunk, int Sapi, int cr, int Tei, int pf)
{
    unsigned char mes[400];

    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = 0x05;
    mes[Q921HeaderSpace+3] = (Q921DevSpace[trunk].vr<<1) | (pf & 0x01);

    return Q921Tx21Proc(trunk, mes, Q921HeaderSpace+4);
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
int Q921SendREJ(int trunk, int Sapi, int cr, int Tei, int pf)
{
    unsigned char mes[400];

    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = 0x09;
    mes[Q921HeaderSpace+3] = (Q921DevSpace[trunk].vr<<1) | (pf & 0x01);

    return Q921Tx21Proc(trunk, mes, Q921HeaderSpace+4);
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
int Q921SendSABME(int trunk, int Sapi, int cr, int Tei, int pf)
{
    unsigned char mes[400];

    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = 0x6f | ((pf<<4)&0x10);

    return Q921Tx21Proc(trunk, mes, Q921HeaderSpace+3);
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
int Q921SendDM(int trunk, int Sapi, int cr, int Tei, int pf)
{
    unsigned char mes[400];

    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = 0x0f | ((pf<<4)&0x10);

    return Q921Tx21Proc(trunk, mes, Q921HeaderSpace+3);
}

/*****************************************************************************

  Function:     Q921

  Description:

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P fiels octet 4

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
//int Q921SendUI(...)
//{
//}

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
int Q921SendDISC(int trunk, int Sapi, int cr, int Tei, int pf)
{
    unsigned char mes[400];

    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = 0x43 | ((pf<<4)&0x10);

    return Q921Tx21Proc(trunk, mes, Q921HeaderSpace+3);
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
int Q921SendUA(int trunk, int Sapi, int cr, int Tei, int pf)
{
    unsigned char mes[400];

    mes[Q921HeaderSpace+0] = (Sapi&0xfc) | ((cr<<1)&0x02);
    mes[Q921HeaderSpace+1] = (Tei<<1) | 0x01;
    mes[Q921HeaderSpace+2] = 0x63 | ((pf<<4)&0x10);

    return Q921Tx21Proc(trunk, mes, Q921HeaderSpace+3);
}

/*****************************************************************************

  Function:     Q921

  Description:

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P fiels octet 5

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
//int Q921SendFRMR(...)
//{
//}

/*****************************************************************************

  Function:     Q921

  Description:

  Parameters:   trunk       trunk #
                Sapi        Sapi
                cr          C/R field.
                Tei         Tei.
                pf          P fiels octet 5

  Return Value: 0 if failed, 1 if Send.

*****************************************************************************/
//int Q921SendXID(...)
//{
//}

int Q921ProcSABME(int trunk, unsigned char *mes, int size)
{
    Q921DevSpace[trunk].vr=0;
    Q921DevSpace[trunk].vs=0;

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
int Q921Rx12(long trunk)
{
    unsigned char *mes;
    int rs,size;     /* receive size & Q921 frame size*/
    unsigned char *smes = MFIFOGetMesPtr(Q921DevSpace[trunk].HDLCInQueue, &size);
    if(smes != NULL)
    {
        rs = size - Q921HeaderSpace;
        mes = &smes[Q921HeaderSpace];
        /* check for I frame */
        if((mes[2] & 0x01) == 0)
        {
            if(Q921Tx23Proc(trunk, smes, size-2)) /* -2 to clip away CRC */
            {
                Q921DevSpace[trunk].vr++;
                Q921SendRR(trunk, (mes[0]&0xfc)>>2, (mes[0]>>1)&0x01,mes[1]>>1, mes[3]&0x01);
            }
            else
            {
                /* todo: whatever*/
            }
        }

        /* check for RR */
        else if(mes[2] ==0x01)
        {
            /* todo: check if RR is responce to I */
            Q921SendRR(trunk, (mes[0]&0xfc)>>2, (mes[0]>>1)&0x01,mes[1]>>1, mes[2]&0x01);
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
//            Q921ErrorProc(trunk, Q921_UNKNOWNFRAME, mes, rs);
            /* todo: REJ or FRMR */
        }

        MFIFOKillNext(Q921DevSpace[trunk].HDLCInQueue);

        return 1;
    }
    return 0;
}

