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
	u_int32_t GetPacketCount()					{ if (!init) return 0; return numpackets; }
	u_int32_t GetPayloadOctetCount()				{ if (!init) return 0; return numpayloadbytes; }
	int SetMaximumPacketSize(size_t maxpacksize);

	int AddCSRC(u_int32_t csrc);
	int DeleteCSRC(u_int32_t csrc);
	void ClearCSRCList();	
	
	int BuildPacket(const void *data,size_t len);
	int BuildPacket(const void *data,size_t len,
	                u_int8_t pt,bool mark,u_int32_t timestampinc);
	int BuildPacketEx(const void *data,size_t len,
	                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords);
	int BuildPacketEx(const void *data,size_t len,
	                  u_int8_t pt,bool mark,u_int32_t timestampinc,
	                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords);
	u_int8_t *GetPacket()						{ if (!init) return 0; return buffer; }
	size_t GetPacketLength()					{ if (!init) return 0; return packetlength; }
	
	int SetDefaultPayloadType(u_int8_t pt);
	int SetDefaultMark(bool m);
	int SetDefaultTimestampIncrement(u_int32_t timestampinc);
	int IncrementTimestamp(u_int32_t inc);
	int IncrementTimestampDefault();
	
	u_int32_t CreateNewSSRC();
	u_int32_t CreateNewSSRC(RTPSources &sources);
	u_int32_t GetSSRC() const					{ if (!init) return 0; return ssrc; }
	u_int32_t GetTimestamp() const					{ if (!init) return 0; return timestamp; }
	u_int16_t GetSequenceNumber() const				{ if (!init) return 0; return seqnr; }

	// note: these are not necessarily from the last packet!
	RTPTime GetPacketTime() const					{ if (!init) return RTPTime(0,0); return lastwallclocktime; }
	u_int32_t GetPacketTimestamp() const				{ if (!init) return 0; return lastrtptimestamp; }
private:
	int PrivateBuildPacket(const void *data,size_t len,
	                  u_int8_t pt,bool mark,u_int32_t timestampinc,bool gotextension,
	                  u_int16_t hdrextID = 0,const void *hdrextdata = 0,size_t numhdrextwords = 0);

	RTPRandom rtprnd;	
	size_t maxpacksize;
	u_int8_t *buffer;
	size_t packetlength;
	
	u_int32_t numpayloadbytes;
	u_int32_t numpackets;
	bool init;

	u_int32_t ssrc;
	u_int32_t timestamp;
	u_int16_t seqnr;

	u_int32_t defaulttimestampinc;
	u_int8_t defaultpayloadtype;
	bool defaultmark;

	bool deftsset,defptset,defmarkset;

	u_int32_t csrcs[RTP_MAXCSRCS];
	int numcsrcs;

	RTPTime lastwallclocktime;
	u_int32_t lastrtptimestamp;
	u_int32_t prevrtptimestamp;
};

inline int RTPPacketBuilder::SetDefaultPayloadType(u_int8_t pt)
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

inline int RTPPacketBuilder::SetDefaultTimestampIncrement(u_int32_t timestampinc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	deftsset = true;
	defaulttimestampinc = timestampinc;
	return 0;
}

inline int RTPPacketBuilder::IncrementTimestamp(u_int32_t inc)
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

