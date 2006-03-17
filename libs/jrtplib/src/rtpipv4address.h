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

#ifndef RTPIPV4ADDRESS_H

#define RTPIPV4ADDRESS_H

#include "rtpconfig.h"
#include "rtpaddress.h"
#include "rtptypes.h"

class RTPIPv4Address : public RTPAddress
{
public:
	RTPIPv4Address(uint32_t ip = 0, uint16_t port = 0):RTPAddress(IPv4Address) 		{ RTPIPv4Address::ip = ip; RTPIPv4Address::port = port; }
	RTPIPv4Address(const uint8_t ip[4],uint16_t port = 0):RTPAddress(IPv4Address)		{ RTPIPv4Address::ip = (uint32_t)ip[3]; RTPIPv4Address::ip |= (((uint32_t)ip[2])<<8); RTPIPv4Address::ip |= (((uint32_t)ip[1])<<16); RTPIPv4Address::ip |= (((uint32_t)ip[0])<<24); RTPIPv4Address::port = port; }
	~RTPIPv4Address()									{ }
	void SetIP(uint32_t ip)								{ RTPIPv4Address::ip = ip; }
	void SetIP(const uint8_t ip[4])							{ RTPIPv4Address::ip = (uint32_t)ip[3]; RTPIPv4Address::ip |= (((uint32_t)ip[2])<<8); RTPIPv4Address::ip |= (((uint32_t)ip[1])<<16); RTPIPv4Address::ip |= (((uint32_t)ip[0])<<24); }
	void SetPort(uint16_t port)								{ RTPIPv4Address::port = port; }
	uint32_t GetIP() const									{ return ip; }
	uint16_t GetPort() const								{ return port; }
	RTPAddress *CreateCopy() const;
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;
#ifdef RTPDEBUG
	std::string GetAddressString() const;
#endif // RTPDEBUG
private:
	uint32_t ip;
	uint16_t port;
};

#endif // RTPIPV4ADDRESS_H

