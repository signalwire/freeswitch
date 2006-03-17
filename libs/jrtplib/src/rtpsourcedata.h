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

#ifndef RTPSOURCEDATA_H

#define RTPSOURCEDATA_H

#include "rtpconfig.h"
#include "rtptimeutilities.h"
#include "rtppacket.h"
#include "rtcpsdesinfo.h"
#include "rtptypes.h"
#include "rtpsources.h"
#include <list>

class RTPAddress;

class RTCPSenderReportInfo
{
public:
	RTCPSenderReportInfo():ntptimestamp(0,0),receivetime(0,0)		{ hasinfo = false; rtptimestamp = 0; packetcount = 0; bytecount = 0; }
	void Set(const RTPNTPTime &ntptime,uint32_t rtptime,uint32_t pcount,
	         uint32_t bcount,const RTPTime &rcvtime)			{ ntptimestamp = ntptime; rtptimestamp = rtptime; packetcount = pcount; bytecount = bcount; receivetime = rcvtime; hasinfo = true; }
	
	bool HasInfo() const							{ return hasinfo; }
	RTPNTPTime GetNTPTimestamp() const					{ return ntptimestamp; }
	uint32_t GetRTPTimestamp() const					{ return rtptimestamp; }
	uint32_t GetPacketCount() const					{ return packetcount; }
	uint32_t GetByteCount() const						{ return bytecount; }
	RTPTime GetReceiveTime() const						{ return receivetime; }
private:
	bool hasinfo;
	RTPNTPTime ntptimestamp;
	uint32_t rtptimestamp;
	uint32_t packetcount;
	uint32_t bytecount;
	RTPTime receivetime;
};

class RTCPReceiverReportInfo
{
public:
	RTCPReceiverReportInfo():receivetime(0,0)				{ hasinfo = false; fractionlost = 0; packetslost = 0; exthighseqnr = 0; jitter = 0; lsr = 0; dlsr = 0; } 
	void Set(uint8_t fraclost,int32_t plost,uint32_t exthigh,
	         uint32_t jit,uint32_t l,uint32_t dl,const RTPTime &rcvtime) { fractionlost = ((double)fraclost)/256.0; packetslost = plost; exthighseqnr = exthigh; jitter = jit; lsr = l; dlsr = dl; receivetime = rcvtime; hasinfo = true; }
		
	bool HasInfo() const							{ return hasinfo; }
	double GetFractionLost() const						{ return fractionlost; }
	int32_t	GetPacketsLost() const						{ return packetslost; }
	uint32_t GetExtendedHighestSequenceNumber() const			{ return exthighseqnr; }
	uint32_t GetJitter() const						{ return jitter; }
	uint32_t GetLastSRTimestamp() const					{ return lsr; }
	uint32_t GetDelaySinceLastSR() const					{ return dlsr; }
	RTPTime GetReceiveTime() const						{ return receivetime; }
private:
	bool hasinfo;
	double fractionlost;
	int32_t packetslost;
	uint32_t exthighseqnr;
	uint32_t jitter;
	uint32_t lsr;
	uint32_t dlsr;
	RTPTime receivetime;
};

class RTPSourceStats
{
public:
	RTPSourceStats();
	void ProcessPacket(RTPPacket *pack,const RTPTime &receivetime,double tsunit,bool ownpacket,bool *accept,bool applyprobation,bool *onprobation);

	bool HasSentData() const						{ return sentdata; }
	uint32_t GetNumPacketsReceived() const					{ return packetsreceived; }
	uint32_t GetBaseSequenceNumber() const					{ return baseseqnr; }
	uint32_t GetExtendedHighestSequenceNumber() const			{ return exthighseqnr; }
	uint32_t GetJitter() const						{ return jitter; }

	int32_t GetNumPacketsReceivedInInterval() const				{ return numnewpackets; }
	uint32_t GetSavedExtendedSequenceNumber() const			{ return savedextseqnr; }
	void StartNewInterval()							{ numnewpackets = 0; savedextseqnr = exthighseqnr; }
	
	void SetLastMessageTime(const RTPTime &t)				{ lastmsgtime = t; }
	RTPTime GetLastMessageTime() const					{ return lastmsgtime; }
	void SetLastRTPPacketTime(const RTPTime &t)				{ lastrtptime = t; }
	RTPTime GetLastRTPPacketTime() const					{ return lastrtptime; }

	void SetLastNoteTime(const RTPTime &t)					{ lastnotetime = t; }
	RTPTime GetLastNoteTime() const						{ return lastnotetime; }
private:
	bool sentdata;
	uint32_t packetsreceived;
	uint32_t numcycles; // shifted left 16 bits
	uint32_t baseseqnr;
	uint32_t exthighseqnr,prevexthighseqnr;
	uint32_t jitter,prevtimestamp;
	double djitter;
	RTPTime prevpacktime;
	RTPTime lastmsgtime;
	RTPTime lastrtptime;
	RTPTime lastnotetime;
	uint32_t numnewpackets;
	uint32_t savedextseqnr;
#ifdef RTP_SUPPORT_PROBATION
	uint16_t prevseqnr;
	int probation;
	RTPSources::ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION
};
	
inline RTPSourceStats::RTPSourceStats():prevpacktime(0,0),lastmsgtime(0,0),lastrtptime(0,0),lastnotetime(0,0)
{ 
	sentdata = false; 
	packetsreceived = 0; 
	baseseqnr = 0; 
	exthighseqnr = 0; 
	prevexthighseqnr = 0; 
	jitter = 0; 
	numcycles = 0;
	numnewpackets = 0;
	prevtimestamp = 0;
	djitter = 0;
	savedextseqnr = 0;
#ifdef RTP_SUPPORT_PROBATION
	probation = 0; 
	prevseqnr = 0; 
#endif // RTP_SUPPORT_PROBATION
}

class RTPSourceData
{
protected:
	RTPSourceData(uint32_t ssrc);
	virtual ~RTPSourceData();
public:
	RTPPacket *GetNextPacket();
	void FlushPackets();
	bool HasData() const							{ if (!validated) return false; return packetlist.empty()?false:true; }
	uint32_t GetSSRC() const						{ return ssrc; }
	bool IsOwnSSRC() const							{ return ownssrc; }
	bool IsCSRC() const							{ return iscsrc; }
	bool IsSender() const							{ return issender; }
	bool IsValidated() const						{ return validated; }
	bool IsActive() const							{ if (!validated) return false; if (receivedbye) return false; return true; }

	void SetProcessedInRTCP(bool v)						{ processedinrtcp = v; }
	bool IsProcessedInRTCP() const						{ return processedinrtcp; }
	
	bool IsRTPAddressSet() const						{ return isrtpaddrset; }
	bool IsRTCPAddressSet() const						{ return isrtcpaddrset; }
	const RTPAddress *GetRTPDataAddress() const				{ return rtpaddr; }
	const RTPAddress *GetRTCPDataAddress() const				{ return rtcpaddr; }
	
	bool ReceivedBYE() const						{ return receivedbye; }
	uint8_t *GetBYEReason(size_t *len) const				{ *len = byereasonlen; return byereason; }
	RTPTime GetBYETime() const						{ return byetime; }
		
	void SetTimestampUnit(double tsu)					{ timestampunit = tsu; }
	double GetTimestampUnit() const						{ return timestampunit; }

	// Here is the info received in the last RTCP SR packet from this source
	bool SR_HasInfo() const							{ return SRinf.HasInfo(); }
	RTPNTPTime SR_GetNTPTimestamp() const					{ return SRinf.GetNTPTimestamp(); }
	uint32_t SR_GetRTPTimestamp() const					{ return SRinf.GetRTPTimestamp(); }
	uint32_t SR_GetPacketCount() const					{ return SRinf.GetPacketCount(); }
	uint32_t SR_GetByteCount() const					{ return SRinf.GetByteCount(); }
	RTPTime SR_GetReceiveTime() const					{ return SRinf.GetReceiveTime(); }
	
	// Here is the info received in the previous RTCP SR packet from this source
	bool SR_Prev_HasInfo() const						{ return SRprevinf.HasInfo(); }
	RTPNTPTime SR_Prev_GetNTPTimestamp() const				{ return SRprevinf.GetNTPTimestamp(); }
	uint32_t SR_Prev_GetRTPTimestamp() const				{ return SRprevinf.GetRTPTimestamp(); }
	uint32_t SR_Prev_GetPacketCount() const				{ return SRprevinf.GetPacketCount(); }
	uint32_t SR_Prev_GetByteCount() const					{ return SRprevinf.GetByteCount(); }
	RTPTime SR_Prev_GetReceiveTime() const					{ return SRprevinf.GetReceiveTime(); }

	// Here is the info received in the last RTCP RR packet from this source
	bool RR_HasInfo() const							{ return RRinf.HasInfo(); }
	double RR_GetFractionLost() const					{ return RRinf.GetFractionLost(); }
	int32_t	RR_GetPacketsLost() const					{ return RRinf.GetPacketsLost(); }
	uint32_t RR_GetExtendedHighestSequenceNumber() const			{ return RRinf.GetExtendedHighestSequenceNumber(); }
	uint32_t RR_GetJitter() const						{ return RRinf.GetJitter(); }
	uint32_t RR_GetLastSRTimestamp() const					{ return RRinf.GetLastSRTimestamp(); }
	uint32_t RR_GetDelaySinceLastSR() const				{ return RRinf.GetDelaySinceLastSR(); }
	RTPTime RR_GetReceiveTime() const					{ return RRinf.GetReceiveTime(); }
	
	// Here is the info received in the last RTCP RR packet from this source
	bool RR_Prev_HasInfo() const						{ return RRprevinf.HasInfo(); }
	double RR_Prev_GetFractionLost() const					{ return RRprevinf.GetFractionLost(); }
	int32_t	RR_Prev_GetPacketsLost() const					{ return RRprevinf.GetPacketsLost(); }
	uint32_t RR_Prev_GetExtendedHighestSequenceNumber() const		{ return RRprevinf.GetExtendedHighestSequenceNumber(); }
	uint32_t RR_Prev_GetJitter() const					{ return RRprevinf.GetJitter(); }
	uint32_t RR_Prev_GetLastSRTimestamp() const				{ return RRprevinf.GetLastSRTimestamp(); }
	uint32_t RR_Prev_GetDelaySinceLastSR() const				{ return RRprevinf.GetDelaySinceLastSR(); }
	RTPTime RR_Prev_GetReceiveTime() const					{ return RRprevinf.GetReceiveTime(); }

	// Here is info which is used when sending RTCP packets to this source
	bool INF_HasSentData() const						{ return stats.HasSentData(); }
	int32_t INF_GetNumPacketsReceived() const				{ return stats.GetNumPacketsReceived(); }
	uint32_t INF_GetBaseSequenceNumber() const				{ return stats.GetBaseSequenceNumber(); }
	uint32_t INF_GetExtendedHighestSequenceNumber() const			{ return stats.GetExtendedHighestSequenceNumber(); }
	uint32_t INF_GetJitter() const						{ return stats.GetJitter(); }
	RTPTime INF_GetLastMessageTime() const					{ return stats.GetLastMessageTime(); }
	RTPTime INF_GetLastRTPPacketTime() const				{ return stats.GetLastRTPPacketTime(); }
	double INF_GetEstimatedTimestampUnit() const;
	uint32_t INF_GetNumPacketsReceivedInInterval() const			{ return stats.GetNumPacketsReceivedInInterval(); }
	uint32_t INF_GetSavedExtendedSequenceNumber() const			{ return stats.GetSavedExtendedSequenceNumber(); }
	void INF_StartNewInterval()						{ stats.StartNewInterval(); }
	RTPTime INF_GetRoundtripTime() const;
	RTPTime INF_GetLastSDESNoteTime() const					{ return stats.GetLastNoteTime(); }
	
	uint8_t *SDES_GetCNAME(size_t *len) const				{ return SDESinf.GetCNAME(len); }
	uint8_t *SDES_GetName(size_t *len) const				{ return SDESinf.GetName(len); }
	uint8_t *SDES_GetEMail(size_t *len) const				{ return SDESinf.GetEMail(len); }
	uint8_t *SDES_GetPhone(size_t *len) const				{ return SDESinf.GetPhone(len); }
	uint8_t *SDES_GetLocation(size_t *len) const				{ return SDESinf.GetLocation(len); }
	uint8_t *SDES_GetTool(size_t *len) const				{ return SDESinf.GetTool(len); }
	uint8_t *SDES_GetNote(size_t *len) const				{ return SDESinf.GetNote(len); }
	
#ifdef RTP_SUPPORT_SDESPRIV
	void SDES_GotoFirstPrivateValue()										{ SDESinf.GotoFirstPrivateValue(); }
	bool SDES_GetNextPrivateValue(uint8_t **prefix,size_t *prefixlen,uint8_t **value,size_t *valuelen) 		{ return SDESinf.GetNextPrivateValue(prefix,prefixlen,value,valuelen); }
	bool SDES_GetPrivateValue(uint8_t *prefix,size_t prefixlen,uint8_t **value,size_t *valuelen) const 		{ return SDESinf.GetPrivateValue(prefix,prefixlen,value,valuelen); }
#endif // RTP_SUPPORT_SDESPRIV

#ifdef RTPDEBUG
	virtual void Dump();
#endif // RTPDEBUG
protected:
	std::list<RTPPacket *> packetlist;

	uint32_t ssrc;
	bool ownssrc;
	bool iscsrc;
	double timestampunit;
	bool receivedbye;
	bool validated;
	bool processedinrtcp;
	bool issender;
	
	RTCPSenderReportInfo SRinf,SRprevinf;
	RTCPReceiverReportInfo RRinf,RRprevinf;
	RTPSourceStats stats;
	RTCPSDESInfo SDESinf;
	
	bool isrtpaddrset,isrtcpaddrset;
	RTPAddress *rtpaddr,*rtcpaddr;
	
	RTPTime byetime;
	uint8_t *byereason;
	size_t byereasonlen;
};

inline RTPPacket *RTPSourceData::GetNextPacket()
{
	if (!validated)
		return 0;

	RTPPacket *p;

	if (packetlist.empty())
		return 0;
	p = *(packetlist.begin());
	packetlist.pop_front();
	return p;
}

inline void RTPSourceData::FlushPackets()
{
	std::list<RTPPacket *>::const_iterator it;

	for (it = packetlist.begin() ; it != packetlist.end() ; ++it)
		delete (*it);
	packetlist.clear();
}
#endif // RTPSOURCEDATA_H

