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

#ifndef RTPINTERNALSOURCEDATA_H

#define RTPINTERNALSOURCEDATA_H

#include "rtpconfig.h"
#include "rtpsourcedata.h"
#include "rtpaddress.h"
#include "rtptimeutilities.h"
#include "rtpsources.h"

class RTPInternalSourceData : public RTPSourceData
{
public:
	RTPInternalSourceData(uint32_t ssrc, RTPSources::ProbationType probtype);
	~RTPInternalSourceData();

	int ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,bool *stored);
	void ProcessSenderInfo(const RTPNTPTime &ntptime,uint32_t rtptime,uint32_t packetcount,
	                       uint32_t octetcount,const RTPTime &receivetime)				{ SRprevinf = SRinf; SRinf.Set(ntptime,rtptime,packetcount,octetcount,receivetime); stats.SetLastMessageTime(receivetime); }
	void ProcessReportBlock(uint8_t fractionlost,int32_t lostpackets,uint32_t exthighseqnr,
	                        uint32_t jitter,uint32_t lsr,uint32_t dlsr,
				const RTPTime &receivetime)						{ RRprevinf = RRinf; RRinf.Set(fractionlost,lostpackets,exthighseqnr,jitter,lsr,dlsr,receivetime); stats.SetLastMessageTime(receivetime); }
	void UpdateMessageTime(const RTPTime &receivetime)						{ stats.SetLastMessageTime(receivetime); }
	int ProcessSDESItem(uint8_t id,const uint8_t *data,size_t itemlen,const RTPTime &receivetime,bool *cnamecollis);
#ifdef RTP_SUPPORT_SDESPRIV
	int ProcessPrivateSDESItem(const uint8_t *prefix,size_t prefixlen,const uint8_t *value,size_t valuelen,const RTPTime &receivetime);
#endif // RTP_SUPPORT_SDESPRIV
	int ProcessBYEPacket(const uint8_t *reason,size_t reasonlen,const RTPTime &receivetime);
		
	int SetRTPDataAddress(const RTPAddress *a);
	int SetRTCPDataAddress(const RTPAddress *a);

	void ClearSenderFlag()										{ issender = false; }
	void SentRTPPacket()										{ if (!ownssrc) return; RTPTime t = RTPTime::CurrentTime(); issender = true; stats.SetLastRTPPacketTime(t); stats.SetLastMessageTime(t); }
	void SetOwnSSRC()										{ ownssrc = true; validated = true; }
	void SetCSRC()											{ validated = true; iscsrc = true; }
	void ClearNote()										{ SDESinf.SetNote(0,0); }
	
#ifdef RTP_SUPPORT_PROBATION
private:
	RTPSources::ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION
};

inline int RTPInternalSourceData::SetRTPDataAddress(const RTPAddress *a)
{
	if (a == 0)
	{
		if (rtpaddr)
		{
			delete rtpaddr;
			rtpaddr = 0;
		}
	}
	else
	{
		RTPAddress *newaddr = a->CreateCopy();
		if (newaddr == 0)
			return ERR_RTP_OUTOFMEM;
		
		if (rtpaddr && a != rtpaddr)
			delete rtpaddr;
		rtpaddr = newaddr;
	}
	isrtpaddrset = true;
	return 0;
}

inline int RTPInternalSourceData::SetRTCPDataAddress(const RTPAddress *a)
{
	if (a == 0)
	{
		if (rtcpaddr)
		{
			delete rtcpaddr;
			rtcpaddr = 0;
		}
	}
	else
	{
		RTPAddress *newaddr = a->CreateCopy();
		if (newaddr == 0)
			return ERR_RTP_OUTOFMEM;
		
		if (rtcpaddr && a != rtcpaddr)
			delete rtcpaddr;
		rtcpaddr = newaddr;
	}
	isrtcpaddrset = true;
	return 0;
}
	
#endif // RTPINTERNALSOURCEDATA_H

