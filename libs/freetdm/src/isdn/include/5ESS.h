/******************************************************************************

  FileName:         5ESS.h

  Contents:         Header and definition for the AT&T 5ESS ISDN dialect. The 
                    header contains the following parts:

                    - Definition of codes
                    - Definition of information elements (5ESSie_).
                    - Definition of messages (5ESSmes_).
                    - Function prototypes.

  Description:      The AT&T 5ESS ISDN dialect here covers ????

  Related Files:    5ESS.h              AT&T 5ESS ISDN Definitions
                    5ESSie.c            AT&T 5ESS ISDN IE encoders/coders (not extant yet)
                                        See Q931ie.c for IE encoders/coders
                    5ESSStateTE.c       AT&T 5ESS ISDN TE State Engine
                    5ESSStateNT.c       AT&T 5ESS ISDN NT State Engine

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Copyright (c) 2007, Michael Jerris. All rights reserved.
  email:mike@jerris.com  
  
  Copyright (c) 2007, Michael S. Collins, All rights reserved.
  email:mcollins@fcnetwork.com

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
******************************************************************************/

#ifndef _5ESS_NL
#define _5ESS_NL

#include "Q931.h"

/*****************************************************************************

  Q.931 Message codes
  Only 5ESS specific message and ie types 
  here the rest are inherited from Q931.h
  
*****************************************************************************/


/*****************************************************************************

  Q.931 Message Pack/Unpack functions. Implemented in 5ESSmes.c
  Note: Because C variables may not begin with numeric digit, all identifiers
        are prefixed with "ATT5ESS" instead of a bare "5ESS"

*****************************************************************************/
L3INT ATT5ESSUmes_Setup(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT IOff, L3INT Size);
L3INT ATT5ESSPmes_Setup(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT ATT5ESSUmes_SetupAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT IOff, L3INT Size);
L3INT ATT5ESSPmes_SetupAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);

L3INT ATT5ESSUmes_0x07(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size);
L3INT ATT5ESSPmes_0x07(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT ATT5ESSUmes_0x0f(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size);
L3INT ATT5ESSPmes_0x0f(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);



/*****************************************************************************

  Q.931 Process Function Prototyping. Implemented in 5ESSStateTE.c

*****************************************************************************/
L3INT ATT5ESSProc0x0fTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom);
L3INT ATT5ESSProc0x07TE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom);


void ATT5ESSCreateTE(L3UCHAR i);
void ATT5ESSCreateNT(L3UCHAR i);

#endif /* _5ESS_NL */
