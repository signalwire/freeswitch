/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2006 Jori Liesenborgs

  Contact: jori@lumumba.uhasselt.be

  This library was developed at the "Expertisecentrum Digitale Media"
  (http://www.edm.uhasselt.be), a research center of the Hasselt University
  (http://www.uhasselt.be). The library is based upon work done for 
  my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include "rtcpbyepacket.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTCPBYEPacket::RTCPBYEPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(BYE,data,datalength)
{
	knownformat = false;
	reasonoffset = 0;	
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	
	hdr = (RTCPCommonHeader *)data;
	if (hdr->padding)
	{
		uint8_t padcount = data[datalength-1];
		if ((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if (((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
	}
	
	size_t ssrclen = ((size_t)(hdr->count))*sizeof(uint32_t) + sizeof(RTCPCommonHeader);
	if (ssrclen > len)
		return;
	if (ssrclen < len) // there's probably a reason for leaving
	{
		uint8_t *reasonlength = (data+ssrclen);
		size_t reaslen = (size_t)(*reasonlength);
		if (reaslen > (len-ssrclen-1))
			return;
		reasonoffset = ssrclen;
	}
	knownformat = true;
}

#ifdef RTPDEBUG
void RTCPBYEPacket::Dump()
{
	RTCPPacket::Dump();
	if (!IsKnownFormat())
	{
		std::cout << "    Unknown format" << std::endl;
		return;	
	}

	int num = GetSSRCCount();
	int i;

	for (i = 0 ; i < num ; i++)
		std::cout << "    SSRC: " << GetSSRC(i) << std::endl;
	if (HasReasonForLeaving())
	{
		char str[1024];
		memcpy(str,GetReasonData(),GetReasonLength());
		str[GetReasonLength()] = 0;
		std::cout << "    Reason: " << str << std::endl;
	}
}
#endif // RTPDEBUG

