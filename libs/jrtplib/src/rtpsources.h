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

#ifndef RTPSOURCES_H

#define RTPSOURCES_H

#include "rtpconfig.h"
#include "rtpkeyhashtable.h"
#include "rtcpsdespacket.h"
#include "rtptypes.h"

#define RTPSOURCES_HASHSIZE							8317

#ifdef RTP_SUPPORT_INLINETEMPLATEPARAM
	inline int RTPSources_GetHashIndex(const uint32_t &ssrc)	{ return ssrc%RTPSOURCES_HASHSIZE; }
#else // can't use inline function as template parameter
	int RTPSources_GetHashIndex(const uint32_t &ssrc);
#endif // RTP_SUPPORT_INLINETEMPLATEPARAM
	
class RTPNTPTime;
class RTPTransmitter;
class RTCPAPPPacket;
class RTPInternalSourceData;
class RTPRawPacket;
class RTPPacket;
class RTPTime;
class RTPAddress;
class RTPSourceData;

class RTPSources
{
public:
	enum ProbationType { NoProbation, ProbationDiscard, ProbationStore };
	
	RTPSources(ProbationType = ProbationStore);
	virtual ~RTPSources();
	void Clear();
#ifdef RTP_SUPPORT_PROBATION
	void SetProbationType(ProbationType probtype)							{ probationtype = probtype; }
#endif // RTP_SUPPORT_PROBATION

	int CreateOwnSSRC(uint32_t ssrc);
	int DeleteOwnSSRC();
	void SentRTPPacket();

	int ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *trans,bool acceptownpackets);
	int ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *trans[],int numtrans,bool acceptownpackets);

	// Note: if the packet originated from our own session, senderaddress has to be NULL
	int ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,const RTPAddress *senderaddress,bool *stored);
	int ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack,const RTPTime &receivetime,
	                              const RTPAddress *senderaddress);
	
	int ProcessRTCPSenderInfo(uint32_t ssrc,const RTPNTPTime &ntptime,uint32_t rtptime,
	                          uint32_t packetcount,uint32_t octetcount,const RTPTime &receivetime,
				  const RTPAddress *senderaddress);
	int ProcessRTCPReportBlock(uint32_t ssrc,uint8_t fractionlost,int32_t lostpackets,
	                           uint32_t exthighseqnr,uint32_t jitter,uint32_t lsr,
				   uint32_t dlsr,const RTPTime &receivetime,const RTPAddress *senderaddress);
	int ProcessSDESNormalItem(uint32_t ssrc,RTCPSDESPacket::ItemType t,size_t itemlength,
	                          const void *itemdata,const RTPTime &receivetime,const RTPAddress *senderaddress);
#ifdef RTP_SUPPORT_SDESPRIV
	int ProcessSDESPrivateItem(uint32_t ssrc,size_t prefixlen,const void *prefixdata,
	                           size_t valuelen,const void *valuedata,const RTPTime &receivetime,
				   const RTPAddress *senderaddress);
#endif //RTP_SUPPORT_SDESPRIV
	int ProcessBYE(uint32_t ssrc,size_t reasonlength,const void *reasondata,const RTPTime &receivetime,
	               const RTPAddress *senderaddress);

	// If no specific info was sent to us, but we did receive a packet from a SSRC, the following
	// function can be used to update the time at which we last heard something from the SSRC.
	// This way, premature timeouts can be avoided. 
	int UpdateReceiveTime(uint32_t ssrc,const RTPTime &receivetime,const RTPAddress *senderaddress);
	
	bool GotoFirstSource();
	bool GotoNextSource();
	bool GotoPreviousSource();
	bool GotoFirstSourceWithData();
	bool GotoNextSourceWithData();
	bool GotoPreviousSourceWithData();
	RTPSourceData *GetCurrentSourceInfo();
	RTPSourceData *GetSourceInfo(uint32_t ssrc);
	RTPPacket *GetNextPacket();
	bool GotEntry(uint32_t ssrc);
	RTPSourceData *GetOwnSourceInfo()								{ return (RTPSourceData *)owndata; }

	void Timeout(const RTPTime &curtime,const RTPTime &timeoutdelay);
	void SenderTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay);
	void BYETimeout(const RTPTime &curtime,const RTPTime &timeoutdelay);
	void NoteTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay);
	void MultipleTimeouts(const RTPTime &curtime,const RTPTime &sendertimeout,
			      const RTPTime &byetimeout,const RTPTime &generaltimeout,
			      const RTPTime &notetimeout);

	int GetSenderCount() const									{ return sendercount; }
	int GetTotalCount() const									{ return totalcount; }
	int GetActiveMemberCount() const								{ return activecount; } 
#ifdef RTPDEBUG
	void Dump();
	void SafeCountTotal();
	void SafeCountSenders();
	void SafeCountActive();
#endif // RTPDEBUG
protected:
	virtual void OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime,
	                         const RTPAddress *senderaddress) 					{ }
	virtual void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
	                                  const RTPAddress *senderaddress) 				{ }
	virtual void OnSSRCCollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp)  { }
	virtual void OnCNAMECollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,
	                              const uint8_t *cname,size_t cnamelength)				{ }
	virtual void OnNewSource(RTPSourceData *srcdat)			 				{ }
	virtual void OnRemoveSource(RTPSourceData *srcdat)						{ }
	virtual void OnTimeout(RTPSourceData *srcdat)							{ }
	virtual void OnBYETimeout(RTPSourceData *srcdat)						{ }
	virtual void OnBYEPacket(RTPSourceData *srcdat)							{ }
	virtual void OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,
	                         const RTPAddress *senderaddress)					{ }
	virtual void OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                 const RTPAddress *senderaddress)				{ }
	virtual void OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                   const RTPAddress *senderaddress)				{ }
	virtual void OnNoteTimeout(RTPSourceData *srcdat)						{ }
private:
	void ClearSourceList();
	int ObtainSourceDataInstance(uint32_t ssrc,RTPInternalSourceData **srcdat,bool *created);
	int GetRTCPSourceData(uint32_t ssrc,const RTPAddress *senderaddress,RTPInternalSourceData **srcdat,bool *newsource);
	bool CheckCollision(RTPInternalSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp);
	
	RTPKeyHashTable<const uint32_t,RTPInternalSourceData*,RTPSources_GetHashIndex,RTPSOURCES_HASHSIZE> sourcelist;
	
	int sendercount;
	int totalcount;
	int activecount;

#ifdef RTP_SUPPORT_PROBATION
	ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION

	RTPInternalSourceData *owndata;
};

#endif // RTPSOURCES_H

