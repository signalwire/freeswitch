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

#ifndef RTPPACKETBUILDER_H

#define RTPPACKETBUILDER_H

#include "rtpconfig.h"
#include "rtperrors.h"
#include "rtpdefines.h"
#include "rtprandom.h"
#include "rtptimeutilities.h"
#include "rtptypes.h"

class RTPSources;

class RTPPacketBuilder
{
public:
	RTPPacketBuilder();
	~RTPPacketBuilder();
	int Init(size_t maxpacksize);
	void Destroy();
	uint32_t GetPacketCount()					{ if (!init) return 0; return numpackets; }
	uint32_t GetPayloadOctetCount()				{ if (!init) return 0; return numpayloadbytes; }
	int SetMaximumPacketSize(size_t maxpacksize);

	int AddCSRC(uint32_t csrc);
	int DeleteCSRC(uint32_t csrc);
	void ClearCSRCList();	
	
	int BuildPacket(const void *data,size_t len);
	int BuildPacket(const void *data,size_t len,
	                uint8_t pt,bool mark,uint32_t timestampinc);
	int BuildPacket(const void *data,size_t len,
	                uint8_t pt,bool mark,uint32_t timestampinc, uint32_t mseq);
	int BuildPacketEx(const void *data,size_t len,
	                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords);
	int BuildPacketEx(const void *data,size_t len,
	                  uint8_t pt,bool mark,uint32_t timestampinc,
	                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords);
	uint8_t *GetPacket()						{ if (!init) return 0; return buffer; }
	size_t GetPacketLength()					{ if (!init) return 0; return packetlength; }
	
	int SetDefaultPayloadType(uint8_t pt);
	int SetDefaultMark(bool m);
	int SetDefaultTimestampIncrement(uint32_t timestampinc);
	int IncrementTimestamp(uint32_t inc);
	int IncrementTimestampDefault();
	
	uint32_t CreateNewSSRC();
	uint32_t CreateNewSSRC(RTPSources &sources);
	uint32_t GetSSRC() const					{ if (!init) return 0; return ssrc; }
	uint32_t GetTimestamp() const					{ if (!init) return 0; return timestamp; }
	uint16_t GetSequenceNumber() const				{ if (!init) return 0; return seqnr; }

	// note: these are not necessarily from the last packet!
	RTPTime GetPacketTime() const					{ if (!init) return RTPTime(0,0); return lastwallclocktime; }
	uint32_t GetPacketTimestamp() const				{ if (!init) return 0; return lastrtptimestamp; }
private:
	int PrivateBuildPacket(const void *data,size_t len,
	                  uint8_t pt,bool mark,uint32_t timestampinc,bool gotextension,
	                  uint16_t hdrextID = 0,const void *hdrextdata = 0,size_t numhdrextwords = 0,  uint32_t mseq = 0);

	RTPRandom rtprnd;	
	size_t maxpacksize;
	uint8_t *buffer;
	size_t packetlength;
	
	uint32_t numpayloadbytes;
	uint32_t numpackets;
	bool init;

	uint32_t ssrc;
	uint32_t timestamp;
	uint16_t seqnr;

	uint32_t defaulttimestampinc;
	uint8_t defaultpayloadtype;
	bool defaultmark;

	bool deftsset,defptset,defmarkset;

	uint32_t csrcs[RTP_MAXCSRCS];
	int numcsrcs;

	RTPTime lastwallclocktime;
	uint32_t lastrtptimestamp;
	uint32_t prevrtptimestamp;
};

inline int RTPPacketBuilder::SetDefaultPayloadType(uint8_t pt)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	defptset = true;
	defaultpayloadtype = pt;
	return 0;
}

inline int RTPPacketBuilder::SetDefaultMark(bool m)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	defmarkset = true;
	defaultmark = m;
	return 0;
}

inline int RTPPacketBuilder::SetDefaultTimestampIncrement(uint32_t timestampinc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	deftsset = true;
	defaulttimestampinc = timestampinc;
	return 0;
}

inline int RTPPacketBuilder::IncrementTimestamp(uint32_t inc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	timestamp += inc;
	return 0;
}

inline int RTPPacketBuilder::IncrementTimestampDefault()
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	if (!deftsset)
		return ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET;
	timestamp += defaulttimestampinc;
	return 0;
}

#endif // RTPPACKETBUILDER_H

