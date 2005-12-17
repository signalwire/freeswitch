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

#ifndef RTPSTRUCTS_H

#define RTPSTRUCTS_H

#include "rtpconfig.h"
#include "rtptypes.h"

struct RTPHeader
{
#ifdef RTP_BIG_ENDIAN
	u_int8_t version:2;
	u_int8_t padding:1;
	u_int8_t extension:1;
	u_int8_t csrccount:4;
	
	u_int8_t marker:1;
	u_int8_t payloadtype:7;
#else // little endian
	u_int8_t csrccount:4;
	u_int8_t extension:1;
	u_int8_t padding:1;
	u_int8_t version:2;
	
	u_int8_t payloadtype:7;
	u_int8_t marker:1;
#endif // RTP_BIG_ENDIAN
	
	u_int16_t sequencenumber;
	u_int32_t timestamp;
	u_int32_t ssrc;
};

struct RTPExtensionHeader
{
	u_int16_t id;
	u_int16_t length;
};

struct RTPSourceIdentifier
{
	u_int32_t ssrc;
};

struct RTCPCommonHeader
{
#ifdef RTP_BIG_ENDIAN
	u_int8_t version:2;
	u_int8_t padding:1;
	u_int8_t count:5;
#else // little endian
	u_int8_t count:5;
	u_int8_t padding:1;
	u_int8_t version:2;
#endif // RTP_BIG_ENDIAN

	u_int8_t packettype;
	u_int16_t length;
};

struct RTCPSenderReport
{
	u_int32_t ntptime_msw;
	u_int32_t ntptime_lsw;
	u_int32_t rtptimestamp;
	u_int32_t packetcount;
	u_int32_t octetcount;
};

struct RTCPReceiverReport
{
	u_int32_t ssrc; // Identifies about which SSRC's data this report is...
	u_int8_t fractionlost;
	u_int8_t packetslost[3];
	u_int32_t exthighseqnr;
	u_int32_t jitter;
	u_int32_t lsr;
	u_int32_t dlsr;
};

struct RTCPSDESHeader
{
	u_int8_t id;
	u_int8_t length;
};

#endif // RTPSTRUCTS

