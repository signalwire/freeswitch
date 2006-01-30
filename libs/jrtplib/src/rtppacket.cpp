/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2005 Jori Liesenborgs

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

#include "rtppacket.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

#ifdef RTPDEBUG
	#include <stdio.h>
#endif // RTPDEBUG

#include "rtpdebug.h"

void RTPPacket::Clear()
{
	hasextension = false;
	hasmarker = false;
	numcsrcs = 0;
	payloadtype = 0;
	extseqnr = 0;
	timestamp = 0;
	ssrc = 0;
	packet = 0;
	payload = 0; 
	packetlength = 0;
	payloadlength = 0;
	extid = 0;
	extension = 0;
	extensionlength = 0;
	error = 0;
	externalbuffer = false;
}

RTPPacket::RTPPacket(RTPRawPacket &rawpack) : receivetime(rawpack.GetReceiveTime())
{
	Clear();
	error = ParseRawPacket(rawpack);
}

RTPPacket::RTPPacket(u_int8_t payloadtype,const void *payloaddata,size_t payloadlen,u_int16_t seqnr,
		  u_int32_t timestamp,u_int32_t ssrc,bool gotmarker,u_int8_t numcsrcs,const u_int32_t *csrcs,
		  bool gotextension,u_int16_t extensionid,u_int16_t extensionlen_numwords,const void *extensiondata,
		  size_t maxpacksize /* = 0 */ ) : receivetime(0,0)
{
	Clear();
	error = BuildPacket(payloadtype,payloaddata,payloadlen,seqnr,timestamp,ssrc,gotmarker,numcsrcs,
	       	            csrcs,gotextension,extensionid,extensionlen_numwords,extensiondata,0,maxpacksize);
}

RTPPacket::RTPPacket(u_int8_t payloadtype,const void *payloaddata,size_t payloadlen,u_int16_t seqnr,
		  u_int32_t timestamp,u_int32_t ssrc,bool gotmarker,u_int8_t numcsrcs,const u_int32_t *csrcs,
		  bool gotextension,u_int16_t extensionid,u_int16_t extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t buffersize) : receivetime(0,0)
{
	Clear();
	if (buffer == 0)
		error = ERR_RTP_PACKET_EXTERNALBUFFERNULL;
	else if (buffersize <= 0)
		error = ERR_RTP_PACKET_ILLEGALBUFFERSIZE;
	else
		error = BuildPacket(payloadtype,payloaddata,payloadlen,seqnr,timestamp,ssrc,gotmarker,numcsrcs,
		                    csrcs,gotextension,extensionid,extensionlen_numwords,extensiondata,buffer,buffersize);
}

int RTPPacket::ParseRawPacket(RTPRawPacket &rawpack)
{
	u_int8_t *packetbytes;
	size_t packetlen;
	u_int8_t payloadtype;
	RTPHeader *rtpheader;
	bool marker;
	int csrccount;
	bool hasextension;
	int payloadoffset,payloadlength;
	int numpadbytes;
	RTPExtensionHeader *rtpextheader;
	u_int16_t exthdrlen;
	
	if (!rawpack.IsRTP()) // If we didn't receive it on the RTP port, we'll ignore it
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	// The length should be at least the size of the RTP header
	packetlen = rawpack.GetDataLength();
	if (packetlen < sizeof(RTPHeader))
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	packetbytes = (u_int8_t *)rawpack.GetData();
	rtpheader = (RTPHeader *)packetbytes;
	
	// The version number should be correct
	if (rtpheader->version != RTP_VERSION)
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	// We'll check if this is possibly a RTCP packet. For this to be possible
	// the marker bit and payload type combined should be either an SR or RR
	// identifier
	marker = (rtpheader->marker == 0)?false:true;
	payloadtype = rtpheader->payloadtype;
	if (marker)
	{
		if (payloadtype == (RTP_RTCPTYPE_SR & 127)) // don't check high bit (this was the marker!!)
			return ERR_RTP_PACKET_INVALIDPACKET;
		if (payloadtype == (RTP_RTCPTYPE_RR & 127))
			return ERR_RTP_PACKET_INVALIDPACKET;
	}

	csrccount = rtpheader->csrccount;
	payloadoffset = sizeof(RTPHeader)+(int)(csrccount*sizeof(u_int32_t));
	
	if (rtpheader->padding) // adjust payload length to take padding into account
	{
		numpadbytes = (int)packetbytes[packetlen-1]; // last byte contains number of padding bytes
		if (numpadbytes <= 0)
			return ERR_RTP_PACKET_INVALIDPACKET;
	}
	else
		numpadbytes = 0;

	hasextension = (rtpheader->extension == 0)?false:true;
	if (hasextension) // got header extension
	{
		rtpextheader = (RTPExtensionHeader *)(packetbytes+payloadoffset);
		payloadoffset += sizeof(RTPExtensionHeader);
		exthdrlen = ntohs(rtpextheader->length);
		payloadoffset += ((int)exthdrlen)*sizeof(u_int32_t);
	}
	else
	{
		rtpextheader = 0;
		exthdrlen = 0;
	}	
	
	payloadlength = packetlen-numpadbytes-payloadoffset;
	if (payloadlength < 0)
		return ERR_RTP_PACKET_INVALIDPACKET;

	// Now, we've got a valid packet, so we can create a new instance of RTPPacket
	// and fill in the members
	
	RTPPacket::hasextension = hasextension;
	if (hasextension)
	{
		RTPPacket::extid = ntohs(rtpextheader->id);
		RTPPacket::extensionlength = ((int)ntohs(rtpextheader->length))*sizeof(u_int32_t);
		RTPPacket::extension = ((u_int8_t *)rtpextheader)+sizeof(RTPExtensionHeader);
	}

	RTPPacket::hasmarker = marker;
	RTPPacket::numcsrcs = csrccount;
	RTPPacket::payloadtype = payloadtype;
	
	// Note: we don't fill in the EXTENDED sequence number here, since we
	// don't have information about the source here. We just fill in the low
	// 16 bits
	RTPPacket::extseqnr = (u_int32_t)ntohs(rtpheader->sequencenumber);

	RTPPacket::timestamp = ntohl(rtpheader->timestamp);
	RTPPacket::ssrc = ntohl(rtpheader->ssrc);
	RTPPacket::packet = packetbytes;
	RTPPacket::payload = packetbytes+payloadoffset;
	RTPPacket::packetlength = packetlen;
	RTPPacket::payloadlength = payloadlength;

	// We'll zero the data of the raw packet, since we're using it here now!
	rawpack.ZeroData();

	return 0;
}

u_int32_t RTPPacket::GetCSRC(int num) const
{
	if (num >= numcsrcs)
		return 0;

	u_int8_t *csrcpos;
	u_int32_t *csrcval_nbo;
	u_int32_t csrcval_hbo;
	
	csrcpos = packet+sizeof(RTPHeader)+num*sizeof(u_int32_t);
	csrcval_nbo = (u_int32_t *)csrcpos;
	csrcval_hbo = ntohl(*csrcval_nbo);
	return csrcval_hbo;
}

int RTPPacket::BuildPacket(u_int8_t payloadtype,const void *payloaddata,size_t payloadlen,u_int16_t seqnr,
		  u_int32_t timestamp,u_int32_t ssrc,bool gotmarker,u_int8_t numcsrcs,const u_int32_t *csrcs,
		  bool gotextension,u_int16_t extensionid,u_int16_t extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t maxsize)
{
	if (numcsrcs > RTP_MAXCSRCS)
		return ERR_RTP_PACKET_TOOMANYCSRCS;

	if (payloadtype > 127) // high bit should not be used
		return ERR_RTP_PACKET_BADPAYLOADTYPE;
	if (payloadtype == 72 || payloadtype == 73) // could cause confusion with rtcp types
		return ERR_RTP_PACKET_BADPAYLOADTYPE;
	
	packetlength = sizeof(RTPHeader);
	packetlength += sizeof(u_int32_t)*((size_t)numcsrcs);
	if (gotextension)
	{
		packetlength += sizeof(RTPExtensionHeader);
		packetlength += sizeof(u_int32_t)*((size_t)extensionlen_numwords);
	}
	packetlength += payloadlen;

	if (maxsize > 0 && packetlength > maxsize)
	{
		packetlength = 0;
		return ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE;
	}

	// Ok, now we'll just fill in...
	
	RTPHeader *rtphdr;
	
	if (buffer == 0)
	{
		packet = new u_int8_t [packetlength];
		if (packet == 0)
		{
			packetlength = 0;
			return ERR_RTP_OUTOFMEM;
		}
		externalbuffer = false;
	}
	else
	{
		packet = (u_int8_t *)buffer;
		externalbuffer = true;
	}
	
	RTPPacket::hasmarker = gotmarker;
	RTPPacket::hasextension = gotextension;
	RTPPacket::numcsrcs = numcsrcs;
	RTPPacket::payloadtype = payloadtype;
	RTPPacket::extseqnr = (u_int32_t)seqnr;
	RTPPacket::timestamp = timestamp;
	RTPPacket::ssrc = ssrc;
	RTPPacket::payloadlength = payloadlen;
	RTPPacket::extid = extensionid;
	RTPPacket::extensionlength = ((size_t)extensionlen_numwords)*sizeof(u_int32_t);
	
	rtphdr = (RTPHeader *)packet;
	rtphdr->version = RTP_VERSION;
	rtphdr->padding = 0;
	if (gotmarker)
		rtphdr->marker = 1;
	else
		rtphdr->marker = 0;
	if (gotextension)
		rtphdr->extension = 1;
	else
		rtphdr->extension = 0;
	rtphdr->csrccount = numcsrcs;
	rtphdr->payloadtype = payloadtype&127; // make sure high bit isn't set
	rtphdr->sequencenumber = htons(seqnr);
	rtphdr->timestamp = htonl(timestamp);
	rtphdr->ssrc = htonl(ssrc);
	
	u_int32_t *curcsrc;
	int i;

	curcsrc = (u_int32_t *)(packet+sizeof(RTPHeader));
	for (i = 0 ; i < numcsrcs ; i++,curcsrc++)
		*curcsrc = htonl(csrcs[i]);

	payload = packet+sizeof(RTPHeader)+((size_t)numcsrcs)*sizeof(u_int32_t); 
	if (gotextension)
	{
		RTPExtensionHeader *rtpexthdr = (RTPExtensionHeader *)payload;

		rtpexthdr->id = htons(extensionid);
		rtpexthdr->length = htons((u_int16_t)extensionlen_numwords);
		
		payload += sizeof(RTPExtensionHeader);
		memcpy(payload,extensiondata,RTPPacket::extensionlength);
		
		payload += RTPPacket::extensionlength;
	}
	memcpy(payload,payloaddata,payloadlen);
	return 0;
}

#ifdef RTPDEBUG	
void RTPPacket::Dump()
{
	int i;
	
	printf("Payload type:                %d\n",(int)GetPayloadType());
	printf("Extended sequence number:    0x%08x\n",GetExtendedSequenceNumber());
	printf("Timestamp:                   0x%08x\n",GetTimestamp());
	printf("SSRC:                        0x%08x\n",GetSSRC());
	printf("Marker:                      %s\n",HasMarker()?"yes":"no");
	printf("CSRC count:                  %d\n",GetCSRCCount());
	for (i = 0 ; i < GetCSRCCount() ; i++)
		printf("    CSRC[%02d]:                0x%08x\n",i,GetCSRC(i));
	printf("Payload:                     %s\n",GetPayloadData());
	printf("Payload length:              %d\n",GetPayloadLength());
	printf("Packet length:               %d\n",GetPacketLength());
	printf("Extension:                   %s\n",HasExtension()?"yes":"no");
	if (HasExtension())
	{
		printf("    Extension ID:            0x%04x\n",GetExtensionID());
		printf("    Extension data:          %s\n",GetExtensionData());
		printf("    Extension length:        %d\n",GetExtensionLength());
	}
}
#endif // RTPDEBUG

