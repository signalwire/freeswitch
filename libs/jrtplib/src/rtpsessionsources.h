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

#ifndef RTPSESSIONSOURCES_H

#define RTPSESSIONSOURCES_H

#include "rtpconfig.h"
#include "rtpsources.h"

class RTPSession;

class RTPSessionSources : public RTPSources
{
public:
	RTPSessionSources(RTPSession &sess) : rtpsession(sess) 					{ owncollision = false; }
	~RTPSessionSources()									{ }
	void ClearOwnCollisionFlag()								{ owncollision = false; }
	bool DetectedOwnCollision() const							{ return owncollision; }
private:
	void OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime,
	                 const RTPAddress *senderaddress);
	void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
	                          const RTPAddress *senderaddress);
	void OnSSRCCollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp);
	void OnCNAMECollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,
	                              const uint8_t *cname,size_t cnamelength);
	void OnNewSource(RTPSourceData *srcdat);
	void OnRemoveSource(RTPSourceData *srcdat);
	void OnTimeout(RTPSourceData *srcdat);
	void OnBYETimeout(RTPSourceData *srcdat);
	void OnBYEPacket(RTPSourceData *srcdat);
	void OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,
	                 const RTPAddress *senderaddress);
	void OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                         const RTPAddress *senderaddress);
	void OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                           const RTPAddress *senderaddress);
	void OnNoteTimeout(RTPSourceData *srcdat);
	
	RTPSession &rtpsession;
	bool owncollision;
};

#endif // RTPSESSIONSOURCES_H
