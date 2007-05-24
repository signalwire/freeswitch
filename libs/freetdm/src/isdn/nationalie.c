/*****************************************************************************

  FileName:     nationalie.c

  Contents:     Information Element Pack/Unpack functions. 
  
                These functions will pack out a National ISDN message from the bit 
                packed original format into structs that are easier to process 
                and pack the same structs back into bit fields when sending
                messages out.

                The messages contains a short for each possible IE. The MSB 
                bit flags the precense of an IE, while the remaining bits 
                are the offset into a buffer to find the actual IE.

                Each IE are supported by 3 functions:

                nationalPie_XXX     Pack struct into Q.931 IE
                nationalUie_XXX     Unpack Q.931 IE into struct
                nationalInitIEXXX   Initialize IE (see nationalapi.c).

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Copyright (c) 2007, Michael Jerris. All rights reserved.
  email:mike@jerris.com  

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

#include "national.h"

/*****************************************************************************

  Macro:        Q931MoreIE

  Description:  Local helper macro detecting if there is more IE space left
                based on the 3 standard parameters Octet, Off and IESpace.
                This can be used to test if the IE is completed to avoid
                that the header of the next IE is interpreted as a part of
                the current IE.

*****************************************************************************/
#define Q931MoreIE() (Octet+Off - 2< IESize)

#define Q931IESizeTest(x)   {\
                            if(Octet + Off - 2!= IESize)\
                            {\
                                Q931SetError(pTrunk,x, Octet, Off);\
                                return x;\
                            }\
                            }

/*****************************************************************************

  Function:     nationalUie_GenericDigits

  Parameters:   pIE[OUT]        ptr to Information Element id.
                IBuf[IN]        ptr to a packed ie.
                OBuf[OUT]       ptr to buffer for Unpacked ie.
                IOff[IN\OUT]    Input buffer offset
                OOff[IN\OUT]    Output buffer offset


                Ibuf and OBuf points directly to buffers. The IOff and OOff
                must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT nationalUie_GenericDigits(Q931_TrunkInfo_t *pTrunk, ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
    nationalie_GenericDigits * pie = (nationalie_GenericDigits*)OBuf;
    L3INT Off = 0;
    L3INT Octet = 0;
    L3INT x=0;
    L3INT IESize;

    *pIE=0;

    /* Octet 1 */
    pie->IEId        = IBuf[Octet++];

    /* Octet 2 */
    IESize = IBuf[Octet++]; 

    Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
    *OOff = (*OOff) + sizeof(nationalie_GenericDigits) + x -1;
    
	pie->Size = (L3UCHAR)(sizeof(nationalie_GenericDigits) + x -1);

    return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:     nationalPie_GenericDigits

  Parameters:   IBuf[IN]        Ptr to struct.
                OBuf[OUT]        Ptr tp packed output buffer.
                Octet[IN/OUT]    Offset into OBuf.

  Return Value:    Error code, 0 = OK

*****************************************************************************/

L3INT nationalPie_GenericDigits(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	OBuf[(*Octet)++] = nationalie_GENERIC_DIGITS ;
    OBuf[(*Octet)++] = 2;

    return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:     nationalUie_CallingNum

  Parameters:   pIE[OUT]        ptr to Information Element id.
                IBuf[IN]        ptr to a packed ie.
                OBuf[OUT]       ptr to buffer for Unpacked ie.
                IOff[IN\OUT]    Input buffer offset
                OOff[IN\OUT]    Output buffer offset

                Ibuf and OBuf points directly to buffers. The IOff and OOff
                must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT nationalUie_CallingNum(Q931_TrunkInfo_t *pTrunk, ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
     Q931ie_CallingNum * pie = (Q931ie_CallingNum*)OBuf;
    L3INT Off = 0;
    L3INT Octet = 0;
    L3INT x;
    L3INT IESize;

    *pIE=0;

    /* Octet 1 */
    pie->IEId  = IBuf[Octet];
    Octet ++;

    /* Octet 2 */
    IESize = IBuf[Octet ++]; 

    /* Octet 3 */
    pie->TypNum = (IBuf[Octet+Off] >> 4) & 0x07;
    pie->NumPlanID = IBuf[Octet+Off] & 0x0f;
    
    /* Octet 3a */
    if((IBuf[Octet+Off] & 0x80) != 0)
    {
        Off++;
        pie->PresInd = (IBuf[Octet+Off] >> 5) & 0x03;
        pie->ScreenInd = IBuf[Octet+Off] & 0x03;
    }
    Octet++;

    /* Octet 4a*/
    x=0;
    do{
        pie->Digit[x] = IBuf[Octet+Off] & 0x7f;
        Off++;
        x++;
	}while(Q931MoreIE());

    Q931IESizeTest(Q931E_CALLINGNUM);

    Q931SetIE(*pIE, *OOff);

    *IOff = (*IOff) + Octet + Off;
    *OOff = (*OOff) + sizeof(Q931ie_CallingNum) + x - 1;
    pie->Size = (L3UCHAR)(sizeof(Q931ie_CallingNum) + x - 1);

    return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:     nationalPie_CallingNum

  Parameters:   IBuf[IN]        Ptr to struct.
                OBuf[OUT]        Ptr tp packed output buffer.
                Octet[IN/OUT]    Offset into OBuf.

  Return Value:    Error code, 0 = OK

*****************************************************************************/
L3INT nationalPie_CallingNum(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
    Q931ie_CallingSub * pIE = (Q931ie_CallingSub*)IBuf;
    L3INT rc=0;
    L3INT Beg=*Octet;
    L3INT li;
    L3INT sN = pIE->Size - sizeof(Q931ie_CalledSub) + 1;
    L3INT x;

    /* Octet 1 */
    OBuf[(*Octet)++] = Q931ie_CALLING_PARTY_SUBADDRESS;
    li=(*Octet)++;

    /* Octet 3 */
    OBuf[(*Octet)++] = 0x80 | (pIE->TypNum << 4) | (pIE->OddEvenInd << 3);
    
    /* Octet 3a */
    /*

        Details about the use and how to detect if Octet 3a is used
        are very weak. Temp left out.

        Octet 3 msb is set to 1 to indicate that this is not present

    */

    /* Octet 4 */
    for(x=0;x<sN;x++)
    {
        OBuf[(*Octet)++] = pIE->Digit[x];
    }

    OBuf[(*Octet)-1] |= 0x80; /* Terminate bit */

    OBuf[li] = (L3UCHAR)((*Octet)-Beg) - 2;
    return rc;
}
