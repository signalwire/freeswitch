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

#ifndef RTCPSDESPACKET_H

#define RTCPSDESPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

class RTCPCompoundPacket;

class RTCPSDESPacket : public RTCPPacket
{
public:
	enum ItemType { None,CNAME,NAME,EMAIL,PHONE,LOC,TOOL,NOTE,PRIV,Unknown };
	
	RTCPSDESPacket(uint8_t *data,size_t datalen);
	~RTCPSDESPacket()							{ }

	int GetChunkCount() const;
	
	bool GotoFirstChunk();
	bool GotoNextChunk();

	uint32_t GetChunkSSRC() const;
	bool GotoFirstItem();
	bool GotoNextItem();

	ItemType GetItemType() const;
	size_t GetItemLength() const;
	uint8_t *GetItemData();

#ifdef RTP_SUPPORT_SDESPRIV
	size_t GetPRIVPrefixLength() const;
	uint8_t *GetPRIVPrefixData();
	size_t GetPRIVValueLength() const;
	uint8_t *GetPRIVValueData();
#endif // RTP_SUPPORT_SDESPRIV

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	uint8_t *currentchunk;
	int curchunknum;
	size_t itemoffset;
};

inline int RTCPSDESPacket::GetChunkCount() const
{
	if (!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
}

inline bool RTCPSDESPacket::GotoFirstChunk()
{
	if (GetChunkCount() == 0)
	{
		currentchunk = 0;
		return false;
	}
	currentchunk = data+sizeof(RTCPCommonHeader);
	curchunknum = 1;
	itemoffset = sizeof(uint32_t);
	return true;
}

inline bool RTCPSDESPacket::GotoNextChunk()
{
	if (!knownformat)
		return false;
	if (currentchunk == 0)
		return false;
	if (curchunknum == GetChunkCount())
		return false;
	
	size_t offset = sizeof(uint32_t);
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+sizeof(uint32_t));
	
	while (sdeshdr->id != 0)
	{
		offset += sizeof(RTCPSDESHeader);
		offset += (size_t)(sdeshdr->length);
		sdeshdr = (RTCPSDESHeader *)(currentchunk+offset);
	}
	offset++; // for the zero byte
	if ((offset&0x03) != 0)
		offset += (4-(offset&0x03));
	currentchunk += offset;
	curchunknum++;
	itemoffset = sizeof(uint32_t);
	return true;
}

inline uint32_t RTCPSDESPacket::GetChunkSSRC() const
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	uint32_t *ssrc = (uint32_t *)currentchunk;
	return ntohl(*ssrc);
}

inline bool RTCPSDESPacket::GotoFirstItem()
{
	if (!knownformat)
		return false;
	if (currentchunk == 0)
		return false;
	itemoffset = sizeof(uint32_t);
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id == 0)
		return false;
	return true;
}

inline bool RTCPSDESPacket::GotoNextItem()
{
	if (!knownformat)
		return false;
	if (currentchunk == 0)
		return false;
	
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id == 0)
		return false;
	
	size_t offset = itemoffset;
	offset += sizeof(RTCPSDESHeader);
	offset += (size_t)(sdeshdr->length);
	sdeshdr = (RTCPSDESHeader *)(currentchunk+offset);
	if (sdeshdr->id == 0)
		return false;
	itemoffset = offset;
	return true;
}

inline RTCPSDESPacket::ItemType RTCPSDESPacket::GetItemType() const
{
	if (!knownformat)
		return None;
	if (currentchunk == 0)
		return None;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	switch (sdeshdr->id)
	{
	case 0:
		return None;
	case RTCP_SDES_ID_CNAME:
		return CNAME;
	case RTCP_SDES_ID_NAME:
		return NAME;
	case RTCP_SDES_ID_EMAIL:
		return EMAIL;
	case RTCP_SDES_ID_PHONE:
		return PHONE;
	case RTCP_SDES_ID_LOCATION:
		return LOC;
	case RTCP_SDES_ID_TOOL:
		return TOOL;
	case RTCP_SDES_ID_NOTE:
		return NOTE;
	case RTCP_SDES_ID_PRIVATE:
		return PRIV;
	default:
		return Unknown;
	}
	return Unknown;
}

inline size_t RTCPSDESPacket::GetItemLength() const
{
	if (!knownformat)
		return None;
	if (currentchunk == 0)
		return None;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id == 0)
		return 0;
	return (size_t)(sdeshdr->length);
}

inline uint8_t *RTCPSDESPacket::GetItemData()
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id == 0)
		return 0;
	return (currentchunk+itemoffset+sizeof(RTCPSDESHeader));
}

#ifdef RTP_SUPPORT_SDESPRIV
inline size_t RTCPSDESPacket::GetPRIVPrefixLength() const
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id != RTCP_SDES_ID_PRIVATE)
		return 0;
	if (sdeshdr->length == 0)
		return 0;
	uint8_t *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if (prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	return prefixlength;
}

inline uint8_t *RTCPSDESPacket::GetPRIVPrefixData()
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id != RTCP_SDES_ID_PRIVATE)
		return 0;
	if (sdeshdr->length == 0)
		return 0;
	uint8_t *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if (prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	if (prefixlength == 0)
		return 0;
	return (currentchunk+itemoffset+sizeof(RTCPSDESHeader)+1);
}

inline size_t RTCPSDESPacket::GetPRIVValueLength() const
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id != RTCP_SDES_ID_PRIVATE)
		return 0;
	if (sdeshdr->length == 0)
		return 0;
	uint8_t *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if (prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	return ((size_t)(sdeshdr->length))-prefixlength-1;
}

inline uint8_t *RTCPSDESPacket::GetPRIVValueData()
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->id != RTCP_SDES_ID_PRIVATE)
		return 0;
	if (sdeshdr->length == 0)
		return 0;
	uint8_t *preflen = currentchunk+itemoffset+sizeof(RTCPSDESHeader);
	size_t prefixlength = (size_t)(*preflen);
	if (prefixlength > (size_t)((sdeshdr->length)-1))
		return 0;
	size_t valuelen = ((size_t)(sdeshdr->length))-prefixlength-1;
	if (valuelen == 0)
		return 0;
	return (currentchunk+itemoffset+sizeof(RTCPSDESHeader)+1+prefixlength);
}

#endif // RTP_SUPPORT_SDESPRIV

#endif // RTCPSDESPACKET_H

