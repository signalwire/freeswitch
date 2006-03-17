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

#include "rtcpapppacket.h"
#ifdef RTPDEBUG
	#include <iostream>
	#include <string>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTCPAPPPacket::RTCPAPPPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(APP,data,datalength)
{
	knownformat = false;
	
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
	
	if (len < (sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2))
		return;
	len -= (sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2);
	appdatalen = len;
	knownformat = true;
}

#ifdef RTPDEBUG
void RTCPAPPPacket::Dump()
{
	RTCPPacket::Dump();
	if (!IsKnownFormat())
	{
		std::cout << "    Unknown format!" << std::endl;
	}
	else
	{
		std::cout << "    SSRC:   " << GetSSRC() << std::endl;
		
		char str[5];
		memcpy(str,GetName(),4);
		str[4] = 0;
		std::cout << "    Name:   " << std::string(str).c_str() << std::endl;
		std::cout << "    Length: " << GetAPPDataLength() << std::endl;
	}
}
#endif // RTPDEBUG

