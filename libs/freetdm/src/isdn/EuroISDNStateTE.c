/*****************************************************************************

  FileName:		EuroISDNStateTE.c

  Contents:		EuroISDN State Engine for TE (User Mode).

			The controlling state engine for Q.931 is the state engine
			on the NT side. The state engine on the TE side is a slave 
			of this. The TE side maintain it's own states as described in
			ITU-T Q931, but will in	raise conditions be overridden by 
			the NT side.

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

#include "Q931.h"

/*
  EuroISDN is a sub-set of Q.931. Q.931 is very generic as it embrase a lot,
  while EuroISDN is more exact and make decitions on some of the 
  'implementation options' in the original standard. EuroISDN will 
  however run smoothly under the generic space, so these functions are more 
  for show 
*/
#if 0
static void EuroISDNCreateTE(L3UCHAR i)
{
	Q931CreateTE(i);
}
#endif
