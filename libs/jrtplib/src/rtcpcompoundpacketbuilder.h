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

#ifndef RTCPCOMPOUNDPACKETBUILDER_H

#define RTCPCOMPOUNDPACKETBUILDER_H

#include "rtpconfig.h"
#include "rtcpcompoundpacket.h"
#include "rtptimeutilities.h"
#include "rtcpsdespacket.h"
#include "rtperrors.h"
#include <list>

class RTCPCompoundPacketBuilder : public RTCPCompoundPacket
{
public:
	RTCPCompoundPacketBuilder();
	~RTCPCompoundPacketBuilder();

	int InitBuild(size_t maxpacketsize);
	int InitBuild(void *externalbuffer,size_t buffersize);
	
	int StartSenderReport(uint32_t senderssrc,const RTPNTPTime &ntptimestamp,uint32_t rtptimestamp,
	                    uint32_t packetcount,uint32_t octetcount);
	int StartReceiverReport(uint32_t senderssrc);
	int AddReportBlock(uint32_t ssrc,uint8_t fractionlost,int32_t packetslost,uint32_t exthighestseq,
	                   uint32_t jitter,uint32_t lsr,uint32_t dlsr);
	
	int AddSDESSource(uint32_t ssrc);
	int AddSDESNormalItem(RTCPSDESPacket::ItemType t,const void *itemdata,uint8_t itemlength);
#ifdef RTP_SUPPORT_SDESPRIV
	int AddSDESPrivateItem(const void *prefixdata,uint8_t prefixlength,const void *valuedata,
	                       uint8_t valuelength);
#endif // RTP_SUPPORT_SDESPRIV

	int AddBYEPacket(uint32_t *ssrcs,uint8_t numssrcs,const void *reasondata,uint8_t reasonlength);

	// note: appdatalen must be a multiple of 4 (32 bits) !
	int AddAPPPacket(uint8_t subtype,uint32_t ssrc,const uint8_t name[4],const void *appdata,size_t appdatalen);

	int EndBuild();
private:
	class Buffer
	{
	public:
		Buffer():packetdata(0),packetlength(0) { }
		Buffer(uint8_t *data,size_t len):packetdata(data),packetlength(len) { }			
		
		uint8_t *packetdata;
		size_t packetlength;
	};

	class Report
	{
	public:
		Report() { headerdata = (uint8_t *)headerdata32; isSR = false; headerlength = 0; }
		~Report() { Clear(); }

		void Clear()
		{
			std::list<Buffer>::const_iterator it; 
			for (it = reportblocks.begin() ; it != reportblocks.end() ; it++) 
				if ((*it).packetdata) 
					delete [] (*it).packetdata; 
			reportblocks.clear();
			isSR = false;
			headerlength = 0;
		}

		size_t NeededBytes() 
		{ 
			size_t x,n,d,r; 
			n = reportblocks.size(); 
			if (n == 0)
			{
				if (headerlength == 0)
					return 0;
				x = sizeof(RTCPCommonHeader)+headerlength;
			}
			else
			{
				x = n*sizeof(RTCPReceiverReport);
				d = n/31; // max 31 reportblocks per report
				r = n%31;
				if (r != 0)
					d++;
				x += d*(sizeof(RTCPCommonHeader)+sizeof(uint32_t)); /* header and SSRC */
				if (isSR)
					x += sizeof(RTCPSenderReport);
			}
			return x;
		}			

		size_t NeededBytesWithExtraReportBlock()
		{
			size_t x,n,d,r; 
			n = reportblocks.size() + 1; // +1 for the extra block
			x = n*sizeof(RTCPReceiverReport);
			d = n/31; // max 31 reportblocks per report
			r = n%31;
			if (r != 0)
				d++;
			x += d*(sizeof(RTCPCommonHeader)+sizeof(uint32_t)); /* header and SSRC */
			if (isSR)
				x += sizeof(RTCPSenderReport);
			return x;
		}
		
		bool isSR;

		uint8_t *headerdata;
		uint32_t headerdata32[(sizeof(uint32_t)+sizeof(RTCPSenderReport))/sizeof(uint32_t)]; // either for ssrc and sender info or just ssrc
		size_t headerlength;
		std::list<Buffer> reportblocks;
	};

	class SDESSource
	{
	public:
		SDESSource(uint32_t s) : ssrc(s),totalitemsize(0) { }
		~SDESSource()
		{
			std::list<Buffer>::const_iterator it;
			for (it = items.begin() ; it != items.end() ; it++)
				if ((*it).packetdata)
					delete [] (*it).packetdata;
			items.clear();
		}

		size_t NeededBytes()
		{
			size_t x,r;
			x = totalitemsize + 1; // +1 for the 0 byte which terminates the item list
			r = x%sizeof(uint32_t);
			if (r != 0)
				x += (sizeof(uint32_t)-r); // make sure it ends on a 32 bit boundary
			x += sizeof(uint32_t); // for ssrc
			return x;
		}

		size_t NeededBytesWithExtraItem(uint8_t itemdatalength)
		{
			size_t x,r;
			x = totalitemsize + sizeof(RTCPSDESHeader) + (size_t)itemdatalength + 1;
			r = x%sizeof(uint32_t);
			if (r != 0)
				x += (sizeof(uint32_t)-r); // make sure it ends on a 32 bit boundary
			x += sizeof(uint32_t); // for ssrc
			return x;
		}
		
		void AddItem(uint8_t *buf,size_t len)
		{
			Buffer b(buf,len);
			totalitemsize += len;
			items.push_back(b);	
		}
		
		uint32_t ssrc;
		std::list<Buffer> items;
	private:
		size_t totalitemsize;
	};
	
	class SDES
	{
	public:
		SDES() { sdesit = sdessources.end(); }
		~SDES() { Clear(); }

		void Clear()
		{
			std::list<SDESSource *>::const_iterator it;

			for (it = sdessources.begin() ; it != sdessources.end() ; it++)
				delete (*it);
			sdessources.clear();
		}

		int AddSSRC(uint32_t ssrc)
		{
			SDESSource *s = new SDESSource(ssrc);
			if (s == 0)
				return ERR_RTP_OUTOFMEM;
			sdessources.push_back(s);
			sdesit = sdessources.end();
			sdesit--;
			return 0;
		}

		int AddItem(uint8_t *buf,size_t len)
		{
			if (sdessources.empty())
				return ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE;
			(*sdesit)->AddItem(buf,len);
			return 0;
		}

		size_t NeededBytes()
		{
			std::list<SDESSource *>::const_iterator it;
			size_t x = 0;
			size_t n,d,r;
			
			if (sdessources.empty())
				return 0;
			
			for (it = sdessources.begin() ; it != sdessources.end() ; it++)
				x += (*it)->NeededBytes();
			n = sdessources.size();
			d = n/31;
			r = n%31;
			if (r != 0)
				d++;
			x += d*sizeof(RTCPCommonHeader);
			return x;
		}
		
		size_t NeededBytesWithExtraItem(uint8_t itemdatalength)
		{
			std::list<SDESSource *>::const_iterator it;
			size_t x = 0;
			size_t n,d,r;
			
			if (sdessources.empty())
				return 0;
			
			for (it = sdessources.begin() ; it != sdesit ; it++)
				x += (*it)->NeededBytes();
			x += (*sdesit)->NeededBytesWithExtraItem(itemdatalength);
			n = sdessources.size();
			d = n/31;
			r = n%31;
			if (r != 0)
				d++;
			x += d*sizeof(RTCPCommonHeader);
			return x;
		}

		size_t NeededBytesWithExtraSource()
		{
			std::list<SDESSource *>::const_iterator it;
			size_t x = 0;
			size_t n,d,r;
			
			if (sdessources.empty())
				return 0;
			
			for (it = sdessources.begin() ; it != sdessources.end() ; it++)
				x += (*it)->NeededBytes();
			
			// for the extra source we'll need at least 8 bytes (ssrc and four 0 bytes)
			x += sizeof(uint32_t)*2;
			
			n = sdessources.size() + 1; // also, the number of sources will increase
			d = n/31;
			r = n%31;
			if (r != 0)
				d++;
			x += d*sizeof(RTCPCommonHeader);
			return x;
		}
		
		std::list<SDESSource *> sdessources;
	private:
		std::list<SDESSource *>::const_iterator sdesit;
	};

	size_t maximumpacketsize;
	uint8_t *buffer;
	bool external;
	bool arebuilding;
	
	Report report;
	SDES sdes;

	std::list<Buffer> byepackets;
	size_t byesize;
	
	std::list<Buffer> apppackets;
	size_t appsize;
	
	void ClearBuildBuffers();
};

#endif // RTCPCOMPOUNDPACKETBUILDER_H

