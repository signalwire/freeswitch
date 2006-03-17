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

#include "rtcppacket.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

#ifdef RTPDEBUG

void RTCPPacket::Dump()
{
	switch(packettype)
	{
	case SR:
		std::cout << "RTCP Sender Report      ";
		break;
	case RR:
		std::cout << "RTCP Receiver Report    ";
		break;
	case SDES:
		std::cout << "RTCP Source Description ";
		break;
	case APP:
		std::cout << "RTCP APP Packet         ";
		break;
	case BYE:
		std::cout << "RTCP Bye Packet         ";
		break;
	case Unknown:
		std::cout << "Unknown RTCP Packet     ";
		break;
	default:
		std::cout << "ERROR: Invalid packet type!" << std::endl;		
	}
	std::cout << "Length: " << datalen;
	std::cout << std::endl;
}

#endif // RTPDEBUG
