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

#ifndef RTPIPV6DESTINATION_H

#define RTPIPV6DESTINATION_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtptypes.h"
#include <string.h>
#ifndef WIN32
	#include <netinet/in.h>
#endif // WIN32
#ifdef RTPDEBUG
	#include <stdio.h>
	#include <string>
#endif // RTPDEBUG

class RTPIPv6Destination
{
public:
	RTPIPv6Destination(in6_addr ip,uint16_t portbase)	 			{ RTPIPv6Destination::ip = ip; rtpport_nbo = htons(portbase); rtcpport_nbo = htons(portbase+1); }
	in6_addr GetIP() const								{ return ip; }
	uint16_t GetRTPPort_NBO() const						{ return rtpport_nbo; }
	uint16_t GetRTCPPort_NBO() const						{ return rtcpport_nbo; }
	bool operator==(const RTPIPv6Destination &src) const				{ if (src.rtpport_nbo == rtpport_nbo && (memcmp(&(src.ip),&ip,sizeof(in6_addr)) == 0)) return true; return false; } // NOTE: I only check IP and portbase
#ifdef RTPDEBUG
	std::string GetDestinationString() const;
#endif // RTPDEBUG
private:
	in6_addr ip;
	uint16_t rtpport_nbo,rtcpport_nbo;
};

#ifdef RTPDEBUG
inline std::string RTPIPv6Destination::GetDestinationString() const
{
	uint16_t ip16[8];
	char str[48];
	uint16_t portbase = ntohs(rtpport_nbo);
	int i,j;
	for (i = 0,j = 0 ; j < 8 ; j++,i += 2)	{ ip16[j] = (((uint16_t)ip.s6_addr[i])<<8); ip16[j] |= ((uint16_t)ip.s6_addr[i+1]); }
	snprintf(str,48,"%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X/%d",(int)ip16[0],(int)ip16[1],(int)ip16[2],(int)ip16[3],(int)ip16[4],(int)ip16[5],(int)ip16[6],(int)ip16[7],(int)portbase);
	return std::string(str);
}
#endif // RTPDEBUG

#endif // RTP_SUPPORT_IPV6

#endif // RTPIPV6DESTINATION_H

