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

#ifndef RTCPSRPACKET_H

#define RTCPSRPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtptimeutilities.h"
#include "rtpstructs.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

class RTCPCompoundPacket;

class RTCPSRPacket : public RTCPPacket
{
public:
	RTCPSRPacket(uint8_t *data,size_t datalength);
	~RTCPSRPacket()								{ }

	// Sender info
	
	uint32_t GetSenderSSRC() const;
	RTPNTPTime GetNTPTimestamp() const;
	uint32_t GetRTPTimestamp() const;
	uint32_t GetSenderPacketCount() const;
	uint32_t GetSenderOctetCount() const;

	// Reportblocks
	
	int GetReceptionReportCount() const;
	// Note: the validity of index is NOT checked!
	uint32_t GetSSRC(int index) const;
	uint8_t GetFractionLost(int index) const;
	int32_t GetLostPacketCount(int index) const;
	uint32_t GetExtendedHighestSequenceNumber(int index) const;
	uint32_t GetJitter(int index) const;
	uint32_t GetLSR(int index) const;
	uint32_t GetDLSR(int index) const;

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	RTCPReceiverReport *GotoReport(int index) const;
};

inline uint32_t RTCPSRPacket::GetSenderSSRC() const
{
	if (!knownformat)
		return 0;
	
	uint32_t *ssrcptr = (uint32_t *)(data+sizeof(RTCPCommonHeader));
	return ntohl(*ssrcptr);
}

inline RTPNTPTime RTCPSRPacket::GetNTPTimestamp() const
{
	if (!knownformat)
		return RTPNTPTime(0,0);

	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(uint32_t));
	return RTPNTPTime(ntohl(sr->ntptime_msw),ntohl(sr->ntptime_lsw));
}

inline uint32_t RTCPSRPacket::GetRTPTimestamp() const
{
	if (!knownformat)
		return 0;
	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(uint32_t));
	return ntohl(sr->rtptimestamp);
}

inline uint32_t RTCPSRPacket::GetSenderPacketCount() const
{
	if (!knownformat)
		return 0;
	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(uint32_t));
	return ntohl(sr->packetcount);
}
	
inline uint32_t RTCPSRPacket::GetSenderOctetCount() const
{
	if (!knownformat)
		return 0;
	RTCPSenderReport *sr = (RTCPSenderReport *)(data+sizeof(RTCPCommonHeader)+sizeof(uint32_t));
	return ntohl(sr->octetcount);
}

inline int RTCPSRPacket::GetReceptionReportCount() const
{
	if (!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
}

inline RTCPReceiverReport *RTCPSRPacket::GotoReport(int index) const
{
	RTCPReceiverReport *r = (RTCPReceiverReport *)(data+sizeof(RTCPCommonHeader)+sizeof(uint32_t)+sizeof(RTCPSenderReport)+index*sizeof(RTCPReceiverReport));
	return r;
}

inline uint32_t RTCPSRPacket::GetSSRC(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->ssrc);
}

inline uint8_t RTCPSRPacket::GetFractionLost(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return r->fractionlost;
}

inline int32_t RTCPSRPacket::GetLostPacketCount(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	uint32_t count = ((uint32_t)r->packetslost[2])|(((uint32_t)r->packetslost[1])<<8)|(((uint32_t)r->packetslost[0])<<16);
	if ((count&0x00800000) != 0) // test for negative number
		count |= 0xFF000000;
	int32_t *count2 = (int32_t *)(&count);
	return (*count2);
}

inline uint32_t RTCPSRPacket::GetExtendedHighestSequenceNumber(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->exthighseqnr);
}

inline uint32_t RTCPSRPacket::GetJitter(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->jitter);
}

inline uint32_t RTCPSRPacket::GetLSR(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->lsr);
}

inline uint32_t RTCPSRPacket::GetDLSR(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->dlsr);
}

#endif // RTCPSRPACKET_H

