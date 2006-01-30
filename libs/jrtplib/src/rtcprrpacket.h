/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2005 Jori Liesenborgs

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

#ifndef RTCPRRPACKET_H

#define RTCPRRPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

class RTCPCompoundPacket;

class RTCPRRPacket : public RTCPPacket
{
public:
	RTCPRRPacket(u_int8_t *data,size_t datalen);
	~RTCPRRPacket()								{ }
	
	// Sender info
	
	u_int32_t GetSenderSSRC() const;
	
	// Reportblocks

	int GetReceptionReportCount() const;
	// Note: the validity of index is NOT checked!
	u_int32_t GetSSRC(int index) const;
	u_int8_t GetFractionLost(int index) const;
	int32_t GetLostPacketCount(int index) const;
	u_int32_t GetExtendedHighestSequenceNumber(int index) const;
	u_int32_t GetJitter(int index) const;
	u_int32_t GetLSR(int index) const;
	u_int32_t GetDLSR(int index) const;

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	RTCPReceiverReport *GotoReport(int index) const;
};

inline u_int32_t RTCPRRPacket::GetSenderSSRC() const
{
	if (!knownformat)
		return 0;
	
	u_int32_t *ssrcptr = (u_int32_t *)(data+sizeof(RTCPCommonHeader));
	return ntohl(*ssrcptr);
}
inline int RTCPRRPacket::GetReceptionReportCount() const
{
	if (!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
}

inline RTCPReceiverReport *RTCPRRPacket::GotoReport(int index) const
{
	RTCPReceiverReport *r = (RTCPReceiverReport *)(data+sizeof(RTCPCommonHeader)+sizeof(u_int32_t)+index*sizeof(RTCPReceiverReport));
	return r;
}

inline u_int32_t RTCPRRPacket::GetSSRC(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->ssrc);
}

inline u_int8_t RTCPRRPacket::GetFractionLost(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return r->fractionlost;
}

inline int32_t RTCPRRPacket::GetLostPacketCount(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	u_int32_t count = ((u_int32_t)r->packetslost[2])|(((u_int32_t)r->packetslost[1])<<8)|(((u_int32_t)r->packetslost[0])<<16);
	if ((count&0x00800000) != 0) // test for negative number
		count |= 0xFF000000;
	int32_t *count2 = (int32_t *)(&count);
	return (*count2);
}

inline u_int32_t RTCPRRPacket::GetExtendedHighestSequenceNumber(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->exthighseqnr);
}

inline u_int32_t RTCPRRPacket::GetJitter(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->jitter);
}

inline u_int32_t RTCPRRPacket::GetLSR(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->lsr);
}

inline u_int32_t RTCPRRPacket::GetDLSR(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->dlsr);
}

#endif // RTCPRRPACKET_H

