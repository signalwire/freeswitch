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

#include "rtcpsdesinfo.h"

#include "rtpdebug.h"

void RTCPSDESInfo::Clear()
{
#ifdef RTP_SUPPORT_SDESPRIV
	std::list<SDESPrivateItem *>::const_iterator it;

	for (it = privitems.begin() ; it != privitems.end() ; ++it)
		delete *it;
	privitems.clear();
#endif // RTP_SUPPORT_SDESPRIV
}

#ifdef RTP_SUPPORT_SDESPRIV
int RTCPSDESInfo::SetPrivateValue(const uint8_t *prefix,size_t prefixlen,const uint8_t *value,size_t valuelen)
{
	std::list<SDESPrivateItem *>::const_iterator it;
	bool found;
	
	found = false;
	it = privitems.begin();
	while (!found && it != privitems.end())
	{
		uint8_t *p;
		size_t l;
		
		p = (*it)->GetPrefix(&l);
		if (l == prefixlen)
		{
			if (l <= 0)
				found = true;
			else if (memcmp(prefix,p,l) == 0)
				found = true;
			else
				++it;
		}
		else
			++it;
	}
	
	SDESPrivateItem *item;
	
	if (found) // replace the value for this entry
		item = *it;
	else // no entry for this prefix found... add it
	{
		if (privitems.size() >= RTP_MAXPRIVITEMS) // too many items present, just ignore it
			return ERR_RTP_SDES_MAXPRIVITEMS;
		
		int status;
		
		item = new SDESPrivateItem();
		if (item == 0)
			return ERR_RTP_OUTOFMEM;
		if ((status = item->SetPrefix(prefix,prefixlen)) < 0)
		{
			delete item;
			return status;
		}
		privitems.push_front(item);
	}
	return item->SetInfo(value,valuelen);
}

int RTCPSDESInfo::DeletePrivatePrefix(const uint8_t *prefix,size_t prefixlen)
{
	std::list<SDESPrivateItem *>::iterator it;
	bool found;
	
	found = false;
	it = privitems.begin();
	while (!found && it != privitems.end())
	{
		uint8_t *p;
		size_t l;
		
		p = (*it)->GetPrefix(&l);
		if (l == prefixlen)
		{
			if (l <= 0)
				found = true;
			else if (memcmp(prefix,p,l) == 0)
				found = true;
			else
				++it;
		}
		else
			++it;
	}
	if (!found)
		return ERR_RTP_SDES_PREFIXNOTFOUND;
	
	delete (*it);
	privitems.erase(it);
	return 0;
}

void RTCPSDESInfo::GotoFirstPrivateValue()
{
	curitem = privitems.begin();
}

bool RTCPSDESInfo::GetNextPrivateValue(uint8_t **prefix,size_t *prefixlen,uint8_t **value,size_t *valuelen)
{
	if (curitem == privitems.end())
		return false;
	*prefix = (*curitem)->GetPrefix(prefixlen);
	*value = (*curitem)->GetInfo(valuelen);
	++curitem;
	return true;
}

bool RTCPSDESInfo::GetPrivateValue(const uint8_t *prefix,size_t prefixlen,uint8_t **value,size_t *valuelen) const
{
	std::list<SDESPrivateItem *>::const_iterator it;
	bool found;
	
	found = false;
	it = privitems.begin();
	while (!found && it != privitems.end())
	{
		uint8_t *p;
		size_t l;
		
		p = (*it)->GetPrefix(&l);
		if (l == prefixlen)
		{
			if (l <= 0)
				found = true;
			else if (memcmp(prefix,p,l) == 0)
				found = true;
			else
				++it;
		}
		else
			++it;
	}
	if (found)
		*value = (*it)->GetInfo(valuelen);
	return found;
}
#endif // RTP_SUPPORT_SDESPRIV

