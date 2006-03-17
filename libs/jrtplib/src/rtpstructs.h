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

#ifndef RTPSTRUCTS_H

#define RTPSTRUCTS_H

#include "rtpconfig.h"
#include "rtptypes.h"

struct RTPHeader
{
#ifdef RTP_BIG_ENDIAN
	uint8_t version:2;
	uint8_t padding:1;
	uint8_t extension:1;
	uint8_t csrccount:4;
	
	uint8_t marker:1;
	uint8_t payloadtype:7;
#else // little endian
	uint8_t csrccount:4;
	uint8_t extension:1;
	uint8_t padding:1;
	uint8_t version:2;
	
	uint8_t payloadtype:7;
	uint8_t marker:1;
#endif // RTP_BIG_ENDIAN
	
	uint16_t sequencenumber;
	uint32_t timestamp;
	uint32_t ssrc;
};

struct RTPExtensionHeader
{
	uint16_t id;
	uint16_t length;
};

struct RTPSourceIdentifier
{
	uint32_t ssrc;
};

struct RTCPCommonHeader
{
#ifdef RTP_BIG_ENDIAN
	uint8_t version:2;
	uint8_t padding:1;
	uint8_t count:5;
#else // little endian
	uint8_t count:5;
	uint8_t padding:1;
	uint8_t version:2;
#endif // RTP_BIG_ENDIAN

	uint8_t packettype;
	uint16_t length;
};

struct RTCPSenderReport
{
	uint32_t ntptime_msw;
	uint32_t ntptime_lsw;
	uint32_t rtptimestamp;
	uint32_t packetcount;
	uint32_t octetcount;
};

struct RTCPReceiverReport
{
	uint32_t ssrc; // Identifies about which SSRC's data this report is...
	uint8_t fractionlost;
	uint8_t packetslost[3];
	uint32_t exthighseqnr;
	uint32_t jitter;
	uint32_t lsr;
	uint32_t dlsr;
};

struct RTCPSDESHeader
{
	uint8_t id;
	uint8_t length;
};

#endif // RTPSTRUCTS

