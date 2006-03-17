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

#ifndef RTPDEFINES_H

#define RTPDEFINES_H

#define RTP_VERSION							2
#define RTP_MAXCSRCS							15
#define RTP_MINPACKETSIZE						600
#define RTP_DEFAULTPACKETSIZE						1400
#define RTP_PROBATIONCOUNT						2
#define RTP_MAXPRIVITEMS						256
#define RTP_SENDERTIMEOUTMULTIPLIER					2
#define RTP_BYETIMEOUTMULTIPLIER					1
#define RTP_MEMBERTIMEOUTMULTIPLIER					5
#define RTP_COLLISIONTIMEOUTMULTIPLIER					10
#define RTP_NOTETTIMEOUTMULTIPLIER					25
#define RTP_DEFAULTSESSIONBANDWIDTH					10000.0

#define RTP_RTCPTYPE_SR							200
#define RTP_RTCPTYPE_RR							201
#define RTP_RTCPTYPE_SDES						202
#define RTP_RTCPTYPE_BYE						203
#define RTP_RTCPTYPE_APP						204

#define RTCP_SDES_ID_CNAME						1
#define RTCP_SDES_ID_NAME						2
#define RTCP_SDES_ID_EMAIL						3
#define RTCP_SDES_ID_PHONE						4
#define RTCP_SDES_ID_LOCATION						5
#define RTCP_SDES_ID_TOOL						6
#define RTCP_SDES_ID_NOTE						7
#define RTCP_SDES_ID_PRIVATE						8
#define RTCP_SDES_NUMITEMS_NONPRIVATE					7
#define RTCP_SDES_MAXITEMLENGTH						255

#define RTCP_BYE_MAXREASONLENGTH					255
#define RTCP_DEFAULTMININTERVAL						5.0	
#define RTCP_DEFAULTBANDWIDTHFRACTION					0.05
#define RTCP_DEFAULTSENDERFRACTION					0.25
#define RTCP_DEFAULTHALFATSTARTUP					true
#define RTCP_DEFAULTIMMEDIATEBYE					true
#define RTCP_DEFAULTSRBYE						true

#endif // RTPDEFINES_H
