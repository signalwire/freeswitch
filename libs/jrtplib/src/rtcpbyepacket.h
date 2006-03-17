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

#ifndef RTCPBYEPACKET_H

#define RTCPBYEPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#if ! (defined(WIN32) || defined (_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

class RTCPCompoundPacket;

class RTCPBYEPacket : public RTCPPacket
{
public:
	RTCPBYEPacket(uint8_t *data,size_t datalen);
	~RTCPBYEPacket()							{ }
	
	int GetSSRCCount() const;
	uint32_t GetSSRC(int index) const; // note: no check is performed to see if index is valid!
	bool HasReasonForLeaving() const;
	size_t GetReasonLength() const;
	uint8_t *GetReasonData();

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	size_t reasonoffset;
};
		      
inline int RTCPBYEPacket::GetSSRCCount() const
{
	if (!knownformat)
		return 0;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return (int)(hdr->count);
}

inline uint32_t RTCPBYEPacket::GetSSRC(int index) const
{
	if (!knownformat)
		return 0;
	uint32_t *ssrc = (uint32_t *)(data+sizeof(RTCPCommonHeader)+sizeof(uint32_t)*index);
	return ntohl(*ssrc);
}

inline bool RTCPBYEPacket::HasReasonForLeaving() const
{
	if (!knownformat)
		return false;
	if (reasonoffset == 0)
		return false;
	return true;
}

inline size_t RTCPBYEPacket::GetReasonLength() const
{
	if (!knownformat)
		return 0;
	if (reasonoffset == 0)
		return 0;
	uint8_t *reasonlen = (data+reasonoffset);
	return (size_t)(*reasonlen);
}

inline uint8_t *RTCPBYEPacket::GetReasonData()
{
	if (!knownformat)
		return 0;
	if (reasonoffset == 0)
		return 0;
	uint8_t *reasonlen = (data+reasonoffset);
	if ((*reasonlen) == 0)
		return 0;
	return (data+reasonoffset+1);	
}

#endif // RTCPBYEPACKET_H

