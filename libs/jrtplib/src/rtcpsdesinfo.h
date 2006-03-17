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

#ifndef RTCPSDESINFO_H

#define RTCPSDESINFO_H

#include "rtpconfig.h"
#include "rtperrors.h"
#include "rtpdefines.h"
#include "rtptypes.h"
#include <string.h>
#include <list>

class RTCPSDESInfo
{
public:
	RTCPSDESInfo()								{ }
	virtual ~RTCPSDESInfo()							{ Clear(); }
	void Clear();

	int SetCNAME(const uint8_t *s,size_t l)				{ return SetNonPrivateItem(RTCP_SDES_ID_CNAME-1,s,l); }
	int SetName(const uint8_t *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_NAME-1,s,l); }
	int SetEMail(const uint8_t *s,size_t l)				{ return SetNonPrivateItem(RTCP_SDES_ID_EMAIL-1,s,l); }
	int SetPhone(const uint8_t *s,size_t l)				{ return SetNonPrivateItem(RTCP_SDES_ID_PHONE-1,s,l); }
	int SetLocation(const uint8_t *s,size_t l)				{ return SetNonPrivateItem(RTCP_SDES_ID_LOCATION-1,s,l); }
	int SetTool(const uint8_t *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_TOOL-1,s,l); }
	int SetNote(const uint8_t *s,size_t l)					{ return SetNonPrivateItem(RTCP_SDES_ID_NOTE-1,s,l); }

#ifdef RTP_SUPPORT_SDESPRIV
	int SetPrivateValue(const uint8_t *prefix,size_t prefixlen,const uint8_t *value,size_t valuelen);
	int DeletePrivatePrefix(const uint8_t *s,size_t len);
#endif // RTP_SUPPORT_SDESPRIV
	
	uint8_t *GetCNAME(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_CNAME-1,len); }
	uint8_t *GetName(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_NAME-1,len); }
	uint8_t *GetEMail(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_EMAIL-1,len); }
	uint8_t *GetPhone(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_PHONE-1,len); }
	uint8_t *GetLocation(size_t *len) const				{ return GetNonPrivateItem(RTCP_SDES_ID_LOCATION-1,len); }
	uint8_t *GetTool(size_t *len) const					{ return GetNonPrivateItem(RTCP_SDES_ID_TOOL-1,len); }
	uint8_t *GetNote(size_t *len) const 					{ return GetNonPrivateItem(RTCP_SDES_ID_NOTE-1,len); }

#ifdef RTP_SUPPORT_SDESPRIV
	void GotoFirstPrivateValue();
	bool GetNextPrivateValue(uint8_t **prefix,size_t *prefixlen,uint8_t **value,size_t *valuelen);
	bool GetPrivateValue(const uint8_t *prefix,size_t prefixlen,uint8_t **value,size_t *valuelen) const;
#endif // RTP_SUPPORT_SDESPRIV
private:
	int SetNonPrivateItem(int itemno,const uint8_t *s,size_t l)		{ if (l > RTCP_SDES_MAXITEMLENGTH) return ERR_RTP_SDES_LENGTHTOOBIG; return nonprivateitems[itemno].SetInfo(s,l); }
	uint8_t *GetNonPrivateItem(int itemno,size_t *len) const		{ return nonprivateitems[itemno].GetInfo(len); }

	class SDESItem
	{
	public:
		SDESItem() 							{ str = 0; length = 0; }
		~SDESItem() 							{ if (str) delete [] str; }
		uint8_t *GetInfo(size_t *len) const				{ *len = length; return str; }
		int SetInfo(const uint8_t *s,size_t len)			{ return SetString(&str,&length,s,len); }
	protected:
		static int SetString(uint8_t **dest,size_t *destlen,const uint8_t *s,size_t len)
		{
			if (len <= 0)
			{
				if (*dest)
					delete [] (*dest);
				*dest = 0;
				*destlen = 0;
			}
			else
			{
				len = (len>RTCP_SDES_MAXITEMLENGTH)?RTCP_SDES_MAXITEMLENGTH:len;
				uint8_t *str2 = new uint8_t[len];
				if (str2 == 0)
					return ERR_RTP_OUTOFMEM;
				memcpy(str2,s,len);
				*destlen = len;
				if (*dest)
					delete [] (*dest);
				*dest = str2;
			}
			return 0;
		}
	private:
		uint8_t *str;
		size_t length;
	};

	SDESItem nonprivateitems[RTCP_SDES_NUMITEMS_NONPRIVATE];

#ifdef RTP_SUPPORT_SDESPRIV
	class SDESPrivateItem : public SDESItem
	{
	public:
		SDESPrivateItem()						{ prefixlen = 0; prefix = 0; }
		~SDESPrivateItem()						{ if (prefix) delete [] prefix; }
		uint8_t *GetPrefix(size_t *len) const				{ *len = prefixlen; return prefix; }
		int SetPrefix(const uint8_t *s,size_t len)			{ return SetString(&prefix,&prefixlen,s,len); }
	private:
		uint8_t *prefix;
		size_t prefixlen;
	};

	std::list<SDESPrivateItem *> privitems;
	std::list<SDESPrivateItem *>::const_iterator curitem;
#endif // RTP_SUPPORT_SDESPRIV
};

#endif // RTCPSDESINFO_H

