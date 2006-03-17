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

#ifndef RTPADDRESS_H

#define RTPADDRESS_H

#include "rtpconfig.h"
#include <string>

class RTPAddress
{
public:
	enum AddressType { IPv4Address, IPv6Address, UserDefinedAddress }; 
	AddressType GetAddressType() const				{ return addresstype; }

	virtual RTPAddress *CreateCopy() const = 0;

	// note: these functions should be able to handle a NULL argument
	virtual bool IsSameAddress(const RTPAddress *addr) const = 0;
	virtual bool IsFromSameHost(const RTPAddress *addr) const  = 0;

#ifdef RTPDEBUG
	virtual std::string GetAddressString() const = 0;
#endif // RTPDEBUG
	
	virtual ~RTPAddress()						{ }
protected:
	RTPAddress(const AddressType t) : addresstype(t) { } // only allow subclasses to be created
private:
	const AddressType addresstype;
};

#endif // RTPADDRESS_H

