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

#ifndef RTPRAWPACKET_H

#define RTPRAWPACKET_H

#include "rtpconfig.h"
#include "rtptimeutilities.h"
#include "rtpaddress.h"
#include "rtptypes.h"

class RTPRawPacket
{
public:	
	RTPRawPacket(uint8_t *data,size_t datalen,RTPAddress *address,RTPTime &recvtime,bool rtp);
	~RTPRawPacket();
	
	uint8_t *GetData()						{ return packetdata; }
	size_t GetDataLength() const					{ return packetdatalength; }
	RTPTime GetReceiveTime() const					{ return receivetime; }
	const RTPAddress *GetSenderAddress() const			{ return senderaddress; }
	bool IsRTP() const						{ return isrtp; }
	void ZeroData()							{ packetdata = 0; packetdatalength = 0; }
private:
	uint8_t *packetdata;
	size_t packetdatalength;
	RTPTime receivetime;
	RTPAddress *senderaddress;
	bool isrtp;
};

inline RTPRawPacket::RTPRawPacket(uint8_t *data,size_t datalen,RTPAddress *address,RTPTime &recvtime,bool rtp):receivetime(recvtime)
{
	packetdata = data;
	packetdatalength = datalen;
	senderaddress = address;
	isrtp = rtp;
}

inline RTPRawPacket::~RTPRawPacket()
{
	if (packetdata)
		delete [] packetdata;
	if (senderaddress)
		delete senderaddress;
}

#endif // RTPRAWPACKET_H

