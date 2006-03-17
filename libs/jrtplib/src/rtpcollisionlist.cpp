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

#include "rtpcollisionlist.h"
#include "rtperrors.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTPCollisionList::RTPCollisionList()
{
#if (defined(WIN32) || defined(_WIN32_WCE))
	timeinit.Dummy();
#endif // WIN32 || _WIN32_WCE
}

void RTPCollisionList::Clear()
{
	std::list<AddressAndTime>::iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
		delete (*it).addr;
	addresslist.clear();
}

int RTPCollisionList::UpdateAddress(const RTPAddress *addr,const RTPTime &receivetime,bool *created)
{
	if (addr == 0)
		return ERR_RTP_COLLISIONLIST_BADADDRESS;
	
	std::list<AddressAndTime>::iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
	{
		if (((*it).addr)->IsSameAddress(addr))
		{
			(*it).recvtime = receivetime;
			*created = false;
			return 0;
		}
	}

	RTPAddress *newaddr = addr->CreateCopy();
	if (newaddr == 0)
		return ERR_RTP_OUTOFMEM;
	
	addresslist.push_back(AddressAndTime(newaddr,receivetime));
	*created = true;
	return 0;
}

bool RTPCollisionList::HasAddress(const RTPAddress *addr) const
{
	std::list<AddressAndTime>::const_iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
	{
		if (((*it).addr)->IsSameAddress(addr))
			return true;
	}

	return false;	
}

void RTPCollisionList::Timeout(const RTPTime &currenttime,const RTPTime &timeoutdelay)
{
	std::list<AddressAndTime>::iterator it;
	RTPTime checktime = currenttime;
	checktime -= timeoutdelay;
	
	it = addresslist.begin();
	while(it != addresslist.end())
	{
		if ((*it).recvtime < checktime) // timeout
		{
			delete (*it).addr;
			it = addresslist.erase(it);	
		}
		else
			it++;
	}
}

#ifdef RTPDEBUG
void RTPCollisionList::Dump()
{
	std::list<AddressAndTime>::const_iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
		std::cout << "Address: " << ((*it).addr)->GetAddressString() << "\tTime: " << (*it).recvtime.GetSeconds() << std::endl;
}
#endif // RTPDEBUG

