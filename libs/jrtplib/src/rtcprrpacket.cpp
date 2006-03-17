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

#include "rtcprrpacket.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTCPRRPacket::RTCPRRPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(RR,data,datalength)
{
	knownformat = false;
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	size_t expectedlength;
	
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

	expectedlength = sizeof(RTCPCommonHeader)+sizeof(uint32_t);
	expectedlength += sizeof(RTCPReceiverReport)*((int)hdr->count);

	if (expectedlength != len)
		return;
	
	knownformat = true;
}

#ifdef RTPDEBUG
void RTCPRRPacket::Dump()
{
	RTCPPacket::Dump();
	if (!IsKnownFormat())
		std::cout << "    Unknown format" << std::endl;
	else
	{
		int num = GetReceptionReportCount();
		int i;

		std::cout << "    SSRC of sender:     " << GetSenderSSRC() << std::endl;
		for (i = 0 ; i < num ; i++)
		{
			std::cout << "    Report block " << i << std::endl;
			std::cout << "        SSRC:           " << GetSSRC(i) << std::endl;
			std::cout << "        Fraction lost:  " << (uint32_t)GetFractionLost(i) << std::endl;
			std::cout << "        Packets lost:   " << GetLostPacketCount(i) << std::endl;
			std::cout << "        Seq. nr.:       " << GetExtendedHighestSequenceNumber(i) << std::endl;
			std::cout << "        Jitter:         " << GetJitter(i) << std::endl;
			std::cout << "        LSR:            " << GetLSR(i) << std::endl;
			std::cout << "        DLSR:           " << GetDLSR(i) << std::endl;
		}
	}	
}
#endif // RTPDEBUG
