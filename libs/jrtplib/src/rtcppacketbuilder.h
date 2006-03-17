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

#ifndef RTCPPACKETBUILDER_H

#define RTCPPACKETBUILDER_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtperrors.h"
#include "rtcpsdesinfo.h"
#include "rtptimeutilities.h"

class RTPSources;
class RTPPacketBuilder;
class RTCPScheduler;
class RTCPCompoundPacket;
class RTCPCompoundPacketBuilder;

class RTCPPacketBuilder
{
public:
	RTCPPacketBuilder(RTPSources &sources,RTPPacketBuilder &rtppackbuilder);
	~RTCPPacketBuilder();

	int Init(size_t maxpacksize,double timestampunit,const void *cname,size_t cnamelen);
	void Destroy();

	int SetTimestampUnit(double tsunit)						{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; if (tsunit < 0) return ERR_RTP_RTCPPACKETBUILDER_ILLEGALTIMESTAMPUNIT; timestampunit = tsunit; return 0; }
	int SetMaximumPacketSize(size_t maxpacksize)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; if (maxpacksize < RTP_MINPACKETSIZE) return ERR_RTP_RTCPPACKETBUILDER_ILLEGALMAXPACKSIZE; maxpacketsize = maxpacksize; return 0; }
	int SetPreTransmissionDelay(const RTPTime &delay)				{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; transmissiondelay = delay; return 0; }
	
	int BuildNextPacket(RTCPCompoundPacket **pack);
	int BuildBYEPacket(RTCPCompoundPacket **pack,const void *reason,size_t reasonlength,bool useSRifpossible = true);

	void SetNameInterval(int count)							{ if (!init) return; interval_name = count; }
	void SetEMailInterval(int count)						{ if (!init) return; interval_email = count; }
	void SetLocationInterval(int count)						{ if (!init) return; interval_location = count; }
	void SetPhoneInterval(int count)						{ if (!init) return; interval_phone = count; }
	void SetToolInterval(int count)							{ if (!init) return; interval_tool = count; }
	void SetNoteInterval(int count)							{ if (!init) return; interval_note = count; }
	int SetLocalName(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetName((const uint8_t *)s,len); }
	int SetLocalEMail(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetEMail((const uint8_t *)s,len); }
	int SetLocalLocation(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetLocation((const uint8_t *)s,len); }
	int SetLocalPhone(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetPhone((const uint8_t *)s,len); }
	int SetLocalTool(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetTool((const uint8_t *)s,len); }
	int SetLocalNote(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetNote((const uint8_t *)s,len); }
private:
	void ClearAllSourceFlags();
	int FillInReportBlocks(RTCPCompoundPacketBuilder *pack,const RTPTime &curtime,int maxcount,bool *full,int *added,int *skipped,bool *atendoflist);
	int FillInSDES(RTCPCompoundPacketBuilder *pack,bool *full,bool *processedall,int *added);
	void ClearAllSDESFlags();
	
	RTPSources &sources;
	RTPPacketBuilder &rtppacketbuilder;
	
	bool init;
	size_t maxpacketsize;
	double timestampunit;
	bool firstpacket;
	RTPTime prevbuildtime,transmissiondelay;

	class RTCPSDESInfoInternal : public RTCPSDESInfo
	{
	public:
		RTCPSDESInfoInternal() 			{ ClearFlags(); }
		void ClearFlags()			{ pname = false; pemail = false; plocation = false; pphone = false; ptool = false; pnote = false; }
		bool ProcessedName() const 		{ return pname; }
		bool ProcessedEMail() const		{ return pemail; }
		bool ProcessedLocation() const		{ return plocation; }
		bool ProcessedPhone() const		{ return pphone; }
		bool ProcessedTool() const		{ return ptool; }
		bool ProcessedNote() const		{ return pnote; }
		void SetProcessedName(bool v)		{ pname = v; }
		void SetProcessedEMail(bool v)		{ pemail = v; }
		void SetProcessedLocation(bool v)	{ plocation  = v; }
		void SetProcessedPhone(bool v)		{ pphone = v; }
		void SetProcessedTool(bool v)		{ ptool = v; }
		void SetProcessedNote(bool v)		{ pnote = v; }
	private:
		bool pname,pemail,plocation,pphone,ptool,pnote;
	};
	
	RTCPSDESInfoInternal ownsdesinfo;
	int interval_name,interval_email,interval_location;
	int interval_phone,interval_tool,interval_note;
	bool doname,doemail,doloc,dophone,dotool,donote;
	bool processingsdes;

	int sdesbuildcount;
};

#endif // RTCPPACKETBUILDER_H

