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


#ifndef RTCPCOMPOUNDPACKET_H

#define RTCPCOMPOUNDPACKET_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include <list>

class RTPRawPacket;
class RTCPPacket;

class RTCPCompoundPacket
{
public:
	RTCPCompoundPacket(RTPRawPacket &rawpack);
protected:
	RTCPCompoundPacket(); // this is for the compoundpacket builder
public:
	virtual ~RTCPCompoundPacket();
	int GetCreationError()								{ return error; }

	uint8_t *GetCompoundPacketData()						{ return compoundpacket; }
	size_t GetCompoundPacketLength()						{ return compoundpacketlength; }

	void GotoFirstPacket()								{ rtcppackit = rtcppacklist.begin(); }

	// Note: the individual RTCPPackets may NOT be deleted!
	RTCPPacket *GetNextPacket()							{ if (rtcppackit == rtcppacklist.end()) return 0; RTCPPacket *p = *rtcppackit; rtcppackit++; return p; }

#ifdef RTPDEBUG
	void Dump();	
#endif // RTPDEBUG
protected:
	void ClearPacketList();
	
	int error;

	uint8_t *compoundpacket;
	size_t compoundpacketlength;
	
	std::list<RTCPPacket *> rtcppacklist;
	std::list<RTCPPacket *>::const_iterator rtcppackit;
};

#endif // RTCPCOMPOUNDPACKET_H

