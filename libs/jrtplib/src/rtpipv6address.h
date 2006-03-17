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

#ifndef RTPIPV6ADDRESS_H

#define RTPIPV6ADDRESS_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtpaddress.h"
#include "rtptypes.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

class RTPIPv6Address : public RTPAddress
{
public:
	RTPIPv6Address():RTPAddress(IPv6Address)						{ for (int i = 0 ; i < 16 ; i++) ip.s6_addr[i] = 0; port = 0; }
	RTPIPv6Address(const uint8_t ip[16],uint16_t port = 0):RTPAddress(IPv6Address)	{ SetIP(ip); RTPIPv6Address::port = port; }
	RTPIPv6Address(in6_addr ip,uint16_t port = 0):RTPAddress(IPv6Address)			{ RTPIPv6Address::ip = ip; RTPIPv6Address::port = port; }
	~RTPIPv6Address()									{ }
	void SetIP(in6_addr ip)									{ RTPIPv6Address::ip = ip; }
	void SetIP(const uint8_t ip[16])							{ for (int i = 0 ; i < 16 ; i++) RTPIPv6Address::ip.s6_addr[i] = ip[i]; }
	void SetPort(uint16_t port)								{ RTPIPv6Address::port = port; }
	void GetIP(uint8_t ip[16]) const							{ for (int i = 0 ; i < 16 ; i++) ip[i] = RTPIPv6Address::ip.s6_addr[i]; }
	in6_addr GetIP() const									{ return ip; }
	uint16_t GetPort() const								{ return port; }

	RTPAddress *CreateCopy() const;
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;
#ifdef RTPDEBUG
	std::string GetAddressString() const;
#endif // RTPDEBUG
private:
	in6_addr ip;
	uint16_t port;
};

#endif // RTP_SUPPORT_IPV6

#endif // RTPIPV6ADDRESS_H

