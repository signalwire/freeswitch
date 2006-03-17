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

#ifndef RTPPACKET_H

#define RTPPACKET_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtptimeutilities.h"

class RTPRawPacket;

class RTPPacket
{
public:
	// If successfull, the data is moved from the raw packet to the RTPPacket instance
	RTPPacket(RTPRawPacket &rawpack);

	// if maxpacksize == 0, it is ignored
	RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  size_t maxpacksize = 0);
	// pretty much the same function, except that here the data is placed in an external buffer
	RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t buffersize);

	virtual ~RTPPacket()							{ if (packet && !externalbuffer) delete [] packet; }
	int GetCreationError() const						{ return error; }

	bool HasExtension() const						{ return hasextension; }
	bool HasMarker() const							{ return hasmarker; }
	
	int GetCSRCCount() const						{ return numcsrcs; }
	uint32_t GetCSRC(int num) const;
	
	uint8_t GetPayloadType() const						{ return payloadtype; }

	// On reception, this is actually a 16 bit value. The high 16 bits
	// are filled in when the packet is processed in the source
	// table
	uint32_t GetExtendedSequenceNumber() const				{ return extseqnr; }
	uint16_t GetSequenceNumber() const					{ return (uint16_t)(extseqnr&0x0000FFFF); }
	void SetExtendedSequenceNumber(uint32_t seq)				{ extseqnr = seq; }

	uint32_t GetTimestamp() const						{ return timestamp; }
	uint32_t GetSSRC() const						{ return ssrc; }

	uint8_t *GetPacketData() const						{ return packet; }
	uint8_t *GetPayloadData() const					{ return payload; }
	size_t GetPacketLength() const						{ return packetlength; }
	size_t GetPayloadLength() const						{ return payloadlength; }
	
	uint16_t GetExtensionID() const					{ return extid; }
	uint8_t *GetExtensionData() const					{ return extension; }
	size_t GetExtensionLength() const					{ return extensionlength; }
#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG

	// If parsed from a raw packet, the receive time is also copied.
	// This function can be used to retrieve it.
	RTPTime GetReceiveTime() const						{ return receivetime; }
private:
	void Clear();
	int ParseRawPacket(RTPRawPacket &rawpack);
	int BuildPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
	                uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
	                bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
	                void *buffer,size_t maxsize);

	int error;
	
	bool hasextension,hasmarker;
	int numcsrcs;

	uint8_t payloadtype;
	uint32_t extseqnr,timestamp,ssrc;
	uint8_t *packet,*payload;
	size_t packetlength,payloadlength;

	uint16_t extid;
	uint8_t *extension;
	size_t extensionlength;

	bool externalbuffer;

	RTPTime receivetime;
};

#endif // RTPPACKET_H

