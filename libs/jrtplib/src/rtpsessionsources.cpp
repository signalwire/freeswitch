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

#include "rtpsessionsources.h"
#include "rtpsession.h"
#include "rtpsourcedata.h"

#include "rtpdebug.h"

void RTPSessionSources::OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	rtpsession.OnRTPPacket(pack,receivetime,senderaddress);
}

void RTPSessionSources::OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	if (senderaddress != 0) // don't analyse own RTCP packets again (they're already analysed on their way out)
		rtpsession.rtcpsched.AnalyseIncoming(*pack);
	rtpsession.OnRTCPCompoundPacket(pack,receivetime,senderaddress);
}

void RTPSessionSources::OnSSRCCollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp)
{
	if (srcdat->IsOwnSSRC())
		owncollision = true;
	rtpsession.OnSSRCCollision(srcdat,senderaddress,isrtp);
}

void RTPSessionSources::OnCNAMECollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,const uint8_t *cname,size_t cnamelength)
{
	rtpsession.OnCNAMECollision(srcdat,senderaddress,cname,cnamelength);
}

void RTPSessionSources::OnNewSource(RTPSourceData *srcdat)
{
	rtpsession.OnNewSource(srcdat);
}

void RTPSessionSources::OnRemoveSource(RTPSourceData *srcdat)
{
	rtpsession.OnRemoveSource(srcdat);
}

void RTPSessionSources::OnTimeout(RTPSourceData *srcdat)
{
	rtpsession.rtcpsched.ActiveMemberDecrease();
	rtpsession.OnTimeout(srcdat);
}

void RTPSessionSources::OnBYETimeout(RTPSourceData *srcdat)
{
	rtpsession.OnBYETimeout(srcdat);
}

void RTPSessionSources::OnBYEPacket(RTPSourceData *srcdat)
{
	rtpsession.rtcpsched.ActiveMemberDecrease();
	rtpsession.OnBYEPacket(srcdat);
}

void RTPSessionSources::OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	rtpsession.OnAPPPacket(apppacket,receivetime,senderaddress);
}

void RTPSessionSources::OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime, const RTPAddress *senderaddress)
{
	rtpsession.OnUnknownPacketType(rtcppack,receivetime,senderaddress);
}

void RTPSessionSources::OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	rtpsession.OnUnknownPacketFormat(rtcppack,receivetime,senderaddress);
}

void RTPSessionSources::OnNoteTimeout(RTPSourceData *srcdat)
{
	rtpsession.OnNoteTimeout(srcdat);
}

