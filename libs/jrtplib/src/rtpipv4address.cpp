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

#include "rtpipv4address.h"
#ifdef RTPDEBUG
	#include <stdio.h>
#endif // RTPDEBUG

#include "rtpdebug.h"

bool RTPIPv4Address::IsSameAddress(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != IPv4Address)
		return false;

	const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
	if (addr2->GetIP() == ip && addr2->GetPort() == port)
		return true;
	return false;
}

bool RTPIPv4Address::IsFromSameHost(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != IPv4Address)
		return false;
	
	const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
	if (addr2->GetIP() == ip)
		return true;
	return false;
}

RTPAddress *RTPIPv4Address::CreateCopy() const
{
	RTPIPv4Address *a = new RTPIPv4Address(ip,port);
	return a;
}

#ifdef RTPDEBUG
std::string RTPIPv4Address::GetAddressString() const
{
	char str[24];

	snprintf(str,24,"%d.%d.%d.%d:%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),
	                             (int)(ip&0xFF),(int)port);
	return std::string(str);
}
#endif // RTPDEBUG
