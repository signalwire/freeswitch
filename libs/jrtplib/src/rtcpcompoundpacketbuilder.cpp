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

#include "rtcpcompoundpacketbuilder.h"
#include "rtcpsrpacket.h"
#include "rtcprrpacket.h"
#include "rtcpsdespacket.h"
#include "rtcpbyepacket.h"
#include "rtcpapppacket.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

#include "rtpdebug.h"

RTCPCompoundPacketBuilder::RTCPCompoundPacketBuilder()
{
	byesize = 0;
	appsize = 0;
	maximumpacketsize = 0;
	buffer = 0;
	external = false;
	arebuilding = false;
}

RTCPCompoundPacketBuilder::~RTCPCompoundPacketBuilder()
{
	if (external)
		compoundpacket = 0; // make sure RTCPCompoundPacket doesn't delete the external buffer
	ClearBuildBuffers();
}

void RTCPCompoundPacketBuilder::ClearBuildBuffers()
{
	report.Clear();
	sdes.Clear();

	std::list<Buffer>::const_iterator it;
	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		if ((*it).packetdata)
			delete [] (*it).packetdata;
	
	for (it = apppackets.begin() ; it != apppackets.end() ; it++)
		if ((*it).packetdata)
			delete [] (*it).packetdata;

	byepackets.clear();
	apppackets.clear();
	byesize = 0;
	appsize = 0;
}

int RTCPCompoundPacketBuilder::InitBuild(size_t maxpacketsize)
{
	if (arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILDING;
	if (compoundpacket)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILT;

	if (maxpacketsize < RTP_MINPACKETSIZE)
		return ERR_RTP_RTCPCOMPPACKBUILDER_MAXPACKETSIZETOOSMALL;
	
	maximumpacketsize = maxpacketsize;
	buffer = 0;
	external = false;
	byesize = 0;
	appsize = 0;
	
	arebuilding = true;
	return 0;
}

int RTCPCompoundPacketBuilder::InitBuild(void *externalbuffer,size_t buffersize)
{
	if (arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILDING;
	if (compoundpacket)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILT;

	if (buffersize < RTP_MINPACKETSIZE)
		return ERR_RTP_RTCPCOMPPACKBUILDER_BUFFERSIZETOOSMALL;

	maximumpacketsize = buffersize;
	buffer = (uint8_t *)externalbuffer;
	external = true;
	byesize = 0;
	appsize = 0;

	arebuilding = true;
	return 0;
}

int RTCPCompoundPacketBuilder::StartSenderReport(uint32_t senderssrc,const RTPNTPTime &ntptimestamp,uint32_t rtptimestamp,
                                                 uint32_t packetcount,uint32_t octetcount)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;

	if (report.headerlength != 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYGOTREPORT;

	size_t totalsize = byesize+appsize+sdes.NeededBytes();
	size_t sizeleft = maximumpacketsize-totalsize;
	size_t neededsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t)+sizeof(RTCPSenderReport);
	
	if (neededsize > sizeleft)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;
	
	// fill in some things

	report.headerlength = sizeof(uint32_t)+sizeof(RTCPSenderReport);
	report.isSR = true;	
	
	uint32_t *ssrc = (uint32_t *)report.headerdata;
	*ssrc = htonl(senderssrc);

	RTCPSenderReport *sr = (RTCPSenderReport *)(report.headerdata + sizeof(uint32_t));
	sr->ntptime_msw = htonl(ntptimestamp.GetMSW());
	sr->ntptime_lsw = htonl(ntptimestamp.GetLSW());
	sr->rtptimestamp = htonl(rtptimestamp);
	sr->packetcount = htonl(packetcount);
	sr->octetcount = htonl(octetcount);

	return 0;
}

int RTCPCompoundPacketBuilder::StartReceiverReport(uint32_t senderssrc)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if (report.headerlength != 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYGOTREPORT;

	size_t totalsize = byesize+appsize+sdes.NeededBytes();
	size_t sizeleft = maximumpacketsize-totalsize;
	size_t neededsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t);
	
	if (neededsize > sizeleft)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;
	
	// fill in some things

	report.headerlength = sizeof(uint32_t);
	report.isSR = false;
	
	uint32_t *ssrc = (uint32_t *)report.headerdata;
	*ssrc = htonl(senderssrc);

	return 0;
}

int RTCPCompoundPacketBuilder::AddReportBlock(uint32_t ssrc,uint8_t fractionlost,int32_t packetslost,uint32_t exthighestseq,
	                                      uint32_t jitter,uint32_t lsr,uint32_t dlsr)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if (report.headerlength == 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_REPORTNOTSTARTED;

	size_t totalothersize = byesize+appsize+sdes.NeededBytes();
	size_t reportsizewithextrablock = report.NeededBytesWithExtraReportBlock();
	
	if ((totalothersize+reportsizewithextrablock) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	uint8_t *buf = new uint8_t[sizeof(RTCPReceiverReport)];
	if (buf == 0)
		return ERR_RTP_OUTOFMEM;
	
	RTCPReceiverReport *rr = (RTCPReceiverReport *)buf;
	uint32_t *packlost = (uint32_t *)&packetslost;
	uint32_t packlost2 = (*packlost);
		
	rr->ssrc = htonl(ssrc);
	rr->fractionlost = fractionlost;
	rr->packetslost[2] = (uint8_t)(packlost2&0xFF);
	rr->packetslost[1] = (uint8_t)((packlost2>>8)&0xFF);
	rr->packetslost[0] = (uint8_t)((packlost2>>16)&0xFF);
	rr->exthighseqnr = htonl(exthighestseq);
	rr->jitter = htonl(jitter);
	rr->lsr = htonl(lsr);
	rr->dlsr = htonl(dlsr);

	report.reportblocks.push_back(Buffer(buf,sizeof(RTCPReceiverReport)));
	return 0;
}

int RTCPCompoundPacketBuilder::AddSDESSource(uint32_t ssrc)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;

	size_t totalotherbytes = byesize+appsize+report.NeededBytes();
	size_t sdessizewithextrasource = sdes.NeededBytesWithExtraSource();

	if ((totalotherbytes + sdessizewithextrasource) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	int status;

	if ((status = sdes.AddSSRC(ssrc)) < 0)
		return status;
	return 0;
}

int RTCPCompoundPacketBuilder::AddSDESNormalItem(RTCPSDESPacket::ItemType t,const void *itemdata,uint8_t itemlength)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if (sdes.sdessources.empty())
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE;

	uint8_t itemid;
	
	switch(t)
	{
	case RTCPSDESPacket::CNAME:
		itemid = RTCP_SDES_ID_CNAME;
		break;
	case RTCPSDESPacket::NAME:
		itemid = RTCP_SDES_ID_NAME;
		break;
	case RTCPSDESPacket::EMAIL:
		itemid = RTCP_SDES_ID_EMAIL;
		break;
	case RTCPSDESPacket::PHONE:
		itemid = RTCP_SDES_ID_PHONE;
		break;
	case RTCPSDESPacket::LOC:
		itemid = RTCP_SDES_ID_LOCATION;
		break;
	case RTCPSDESPacket::TOOL:
		itemid = RTCP_SDES_ID_TOOL;
		break;
	case RTCPSDESPacket::NOTE:
		itemid = RTCP_SDES_ID_NOTE;
		break;
	default:
		return ERR_RTP_RTCPCOMPPACKBUILDER_INVALIDITEMTYPE;
	}

	size_t totalotherbytes = byesize+appsize+report.NeededBytes();
	size_t sdessizewithextraitem = sdes.NeededBytesWithExtraItem(itemlength);

	if ((sdessizewithextraitem+totalotherbytes) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	uint8_t *buf;
	size_t len;

	buf = new uint8_t[sizeof(RTCPSDESHeader)+(size_t)itemlength];
	if (buf == 0)
		return ERR_RTP_OUTOFMEM;
	len = sizeof(RTCPSDESHeader)+(size_t)itemlength;

	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(buf);

	sdeshdr->id = itemid;
	sdeshdr->length = itemlength;
	if (itemlength != 0)
		memcpy((buf + sizeof(RTCPSDESHeader)),itemdata,(size_t)itemlength);

	sdes.AddItem(buf,len);
	return 0;
}

#ifdef RTP_SUPPORT_SDESPRIV
int RTCPCompoundPacketBuilder::AddSDESPrivateItem(const void *prefixdata,uint8_t prefixlength,const void *valuedata,
                                                  uint8_t valuelength)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if (sdes.sdessources.empty())
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE;

	size_t itemlength = ((size_t)prefixlength)+1+((size_t)valuelength);
	if (itemlength > 255)
		return ERR_RTP_RTCPCOMPPACKBUILDER_TOTALITEMLENGTHTOOBIG;
	
	size_t totalotherbytes = byesize+appsize+report.NeededBytes();
	size_t sdessizewithextraitem = sdes.NeededBytesWithExtraItem(itemlength);

	if ((sdessizewithextraitem+totalotherbytes) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	uint8_t *buf;
	size_t len;

	buf = new uint8_t[sizeof(RTCPSDESHeader)+itemlength];
	if (buf == 0)
		return ERR_RTP_OUTOFMEM;
	len = sizeof(RTCPSDESHeader)+(size_t)itemlength;

	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(buf);

	sdeshdr->id = RTCP_SDES_ID_PRIVATE;
	sdeshdr->length = itemlength;
	
	buf[sizeof(RTCPSDESHeader)] = prefixlength;
	if (prefixlength != 0)
		memcpy((buf+sizeof(RTCPSDESHeader)+1),prefixdata,(size_t)prefixlength);
	if (valuelength != 0)
		memcpy((buf+sizeof(RTCPSDESHeader)+1+(size_t)prefixlength),valuedata,(size_t)valuelength);

	sdes.AddItem(buf,len);
	return 0;
}
#endif // RTP_SUPPORT_SDESPRIV

int RTCPCompoundPacketBuilder::AddBYEPacket(uint32_t *ssrcs,uint8_t numssrcs,const void *reasondata,uint8_t reasonlength)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;

	if (numssrcs > 31)
		return ERR_RTP_RTCPCOMPPACKBUILDER_TOOMANYSSRCS;
	
	size_t packsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t)*((size_t)numssrcs);
	size_t zerobytes = 0;
	
	if (reasonlength > 0)
	{
		packsize += 1; // 1 byte for the length;
		packsize += (size_t)reasonlength;

		size_t r = (packsize&0x03);
		if (r != 0)
		{
			zerobytes = 4-r;
			packsize += zerobytes;
		}
	}

	size_t totalotherbytes = appsize+byesize+sdes.NeededBytes()+report.NeededBytes();

	if ((totalotherbytes + packsize) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	uint8_t *buf;
	size_t numwords;
	
	buf = new uint8_t[packsize];
	if (buf == 0)
		return ERR_RTP_OUTOFMEM;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)buf;

	hdr->version = 2;
	hdr->padding = 0;
	hdr->count = numssrcs;
	
	numwords = packsize/sizeof(uint32_t);
	hdr->length = htons((uint16_t)(numwords-1));
	hdr->packettype = RTP_RTCPTYPE_BYE;
	
	uint32_t *sources = (uint32_t *)(buf+sizeof(RTCPCommonHeader));
	uint8_t srcindex;
	
	for (srcindex = 0 ; srcindex < numssrcs ; srcindex++)
		sources[srcindex] = htonl(ssrcs[srcindex]);

	if (reasonlength != 0)
	{
		size_t offset = sizeof(RTCPCommonHeader)+((size_t)numssrcs)*sizeof(uint32_t);

		buf[offset] = reasonlength;
		memcpy((buf+offset+1),reasondata,(size_t)reasonlength);
		for (size_t i = 0 ; i < zerobytes ; i++)
			buf[packsize-1-i] = 0;
	}

	byepackets.push_back(Buffer(buf,packsize));
	byesize += packsize;
	
	return 0;
}

int RTCPCompoundPacketBuilder::AddAPPPacket(uint8_t subtype,uint32_t ssrc,const uint8_t name[4],const void *appdata,size_t appdatalen)
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if (subtype > 31)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALSUBTYPE;
	if ((appdatalen%4) != 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALAPPDATALENGTH;

	size_t appdatawords = appdatalen/4;

	if ((appdatawords+2) > 65535)
		return ERR_RTP_RTCPCOMPPACKBUILDER_APPDATALENTOOBIG;
	
	size_t packsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2+appdatalen;
	size_t totalotherbytes = appsize+byesize+sdes.NeededBytes()+report.NeededBytes();

	if ((totalotherbytes + packsize) > maximumpacketsize)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT;

	uint8_t *buf;
	
	buf = new uint8_t[packsize];
	if (buf == 0)
		return ERR_RTP_OUTOFMEM;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)buf;

	hdr->version = 2;
	hdr->padding = 0;
	hdr->count = subtype;
	
	hdr->length = htons((uint16_t)(appdatawords+2));
	hdr->packettype = RTP_RTCPTYPE_APP;
	
	uint32_t *source = (uint32_t *)(buf+sizeof(RTCPCommonHeader));
	*source = htonl(ssrc);

	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+0] = name[0];
	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+1] = name[1];
	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+2] = name[2];
	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+3] = name[3];

	if (appdatalen > 0)
		memcpy((buf+sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2),appdata,appdatalen);

	apppackets.push_back(Buffer(buf,packsize));
	appsize += packsize;
	
	return 0;
}

int RTCPCompoundPacketBuilder::EndBuild()
{
	if (!arebuilding)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING;
	if (report.headerlength == 0)
		return ERR_RTP_RTCPCOMPPACKBUILDER_NOREPORTPRESENT;
	
	uint8_t *buf;
	size_t len;
	
	len = appsize+byesize+report.NeededBytes()+sdes.NeededBytes();
	
	if (!external)
	{
		buf = new uint8_t[len];
		if (buf == 0)
			return ERR_RTP_OUTOFMEM;
	}
	else
		buf = buffer;
	
	uint8_t *curbuf = buf;
	RTCPPacket *p;

	// first, we'll add all report info
	
	{
		bool firstpacket = true;
		bool done = false;
		std::list<Buffer>::const_iterator it = report.reportblocks.begin();
		do
		{
			RTCPCommonHeader *hdr = (RTCPCommonHeader *)curbuf;
			size_t offset;
			
			hdr->version = 2;
			hdr->padding = 0;

			if (firstpacket && report.isSR)
			{
				hdr->packettype = RTP_RTCPTYPE_SR;
				memcpy((curbuf+sizeof(RTCPCommonHeader)),report.headerdata,report.headerlength);
				offset = sizeof(RTCPCommonHeader)+report.headerlength;
			}
			else
			{
				hdr->packettype = RTP_RTCPTYPE_RR;
				memcpy((curbuf+sizeof(RTCPCommonHeader)),report.headerdata,sizeof(uint32_t));
				offset = sizeof(RTCPCommonHeader)+sizeof(uint32_t);
			}
			firstpacket = false;
			
			uint8_t count = 0;

			while (it != report.reportblocks.end() && count < 31)
			{
				memcpy(curbuf+offset,(*it).packetdata,(*it).packetlength);
				offset += (*it).packetlength;
				count++;
				it++;
			}

			size_t numwords = offset/sizeof(uint32_t);

			hdr->length = htons((uint16_t)(numwords-1));
			hdr->count = count;

			// add entry in parent's list
			if (hdr->packettype == RTP_RTCPTYPE_SR)
				p = new RTCPSRPacket(curbuf,offset);
			else
				p = new RTCPRRPacket(curbuf,offset);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
			}
			rtcppacklist.push_back(p);

			curbuf += offset;
			if (it == report.reportblocks.end())
				done = true;
		} while (!done);
	}
		
	// then, we'll add the sdes info

	if (!sdes.sdessources.empty())
	{
		bool done = false;
		std::list<SDESSource *>::const_iterator sourceit = sdes.sdessources.begin();
		
		do
		{
			RTCPCommonHeader *hdr = (RTCPCommonHeader *)curbuf;
			size_t offset = sizeof(RTCPCommonHeader);
			
			hdr->version = 2;
			hdr->padding = 0;
			hdr->packettype = RTP_RTCPTYPE_SDES;

			uint8_t sourcecount = 0;
			
			while (sourceit != sdes.sdessources.end() && sourcecount < 31)
			{
				uint32_t *ssrc = (uint32_t *)(curbuf+offset);
				*ssrc = htonl((*sourceit)->ssrc);
				offset += sizeof(uint32_t);
				
				std::list<Buffer>::const_iterator itemit,itemend;

				itemit = (*sourceit)->items.begin();
				itemend = (*sourceit)->items.end();
				while (itemit != itemend)
				{
					memcpy(curbuf+offset,(*itemit).packetdata,(*itemit).packetlength);
					offset += (*itemit).packetlength;
					itemit++;
				}

				curbuf[offset] = 0; // end of item list;
				offset++;

				size_t r = offset&0x03;
				if (r != 0) // align to 32 bit boundary
				{
					size_t num = 4-r;
					size_t i;

					for (i = 0 ; i < num ; i++)
						curbuf[offset+i] = 0;
					offset += num;
				}
				
				sourceit++;
				sourcecount++;
			}

			size_t numwords = offset/4;
			
			hdr->count = sourcecount;
			hdr->length = htons((uint16_t)(numwords-1));

			p = new RTCPSDESPacket(curbuf,offset);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
			}
			rtcppacklist.push_back(p);
			
			curbuf += offset;
			if (sourceit == sdes.sdessources.end())
				done = true;
		} while (!done);
	}
	
	// adding the app data
	
	{
		std::list<Buffer>::const_iterator it;

		for (it = apppackets.begin() ; it != apppackets.end() ; it++)
		{
			memcpy(curbuf,(*it).packetdata,(*it).packetlength);
			
			p = new RTCPAPPPacket(curbuf,(*it).packetlength);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
			}
			rtcppacklist.push_back(p);
	
			curbuf += (*it).packetlength;
		}
	}
	
	// adding bye packets
	
	{
		std::list<Buffer>::const_iterator it;

		for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		{
			memcpy(curbuf,(*it).packetdata,(*it).packetlength);
			
			p = new RTCPBYEPacket(curbuf,(*it).packetlength);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return ERR_RTP_OUTOFMEM;
			}
			rtcppacklist.push_back(p);
	
			curbuf += (*it).packetlength;
		}
	}
	
	compoundpacket = buf;
	compoundpacketlength = len;
	arebuilding = false;
	ClearBuildBuffers();
	return 0;
}

