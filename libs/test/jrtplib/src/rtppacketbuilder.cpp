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

#include "rtppacketbuilder.h"
#include "rtperrors.h"
#include "rtppacket.h"
#include "rtpsources.h"
#include <time.h>
#include <stdlib.h>
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTPPacketBuilder::RTPPacketBuilder() : lastwallclocktime(0,0)
{
	init = false;
#if (defined(WIN32) || defined(_WIN32_WCE))
	timeinit.Dummy();
#endif // WIN32 || _WIN32_WCE
}

RTPPacketBuilder::~RTPPacketBuilder()
{
	Destroy();
}

int RTPPacketBuilder::Init(size_t max)
{
	if (init)
		return ERR_RTP_PACKBUILD_ALREADYINIT;
	if (max <= 0)
		return ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE;
	
	maxpacksize = max;
	buffer = new u_int8_t [max];
	if (buffer == 0)
		return ERR_RTP_OUTOFMEM;
	packetlength = 0;
	
	CreateNewSSRC();

	deftsset = false;
	defptset = false;
	defmarkset = false;
		
	numcsrcs = 0;
	
	init = true;
	return 0;
}

void RTPPacketBuilder::Destroy()
{
	if (!init)
		return;
	delete [] buffer;
	init = false;
}

int RTPPacketBuilder::SetMaximumPacketSize(size_t max)
{
	u_int8_t *newbuf;

	if (max <= 0)
		return ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE;
	newbuf = new u_int8_t[max];
	if (newbuf == 0)
		return ERR_RTP_OUTOFMEM;
	
	delete [] buffer;
	buffer = newbuf;
	maxpacksize = max;
	return 0;
}

int RTPPacketBuilder::AddCSRC(u_int32_t csrc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	if (numcsrcs >= RTP_MAXCSRCS)
		return ERR_RTP_PACKBUILD_CSRCLISTFULL;

	int i;
	
	for (i = 0 ; i < numcsrcs ; i++)
	{
		if (csrcs[i] == csrc)
			return ERR_RTP_PACKBUILD_CSRCALREADYINLIST;
	}
	csrcs[numcsrcs] = csrc;
	numcsrcs++;
	return 0;
}

int RTPPacketBuilder::DeleteCSRC(u_int32_t csrc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	
	int i = 0;
	bool found = false;

	while (!found && i < numcsrcs)
	{
		if (csrcs[i] == csrc)
			found = true;
		else
			i++;
	}

	if (!found)
		return ERR_RTP_PACKBUILD_CSRCNOTINLIST;
	
	// move the last csrc in the place of the deleted one
	numcsrcs--;
	if (numcsrcs > 0 && numcsrcs != i)
		csrcs[i] = csrcs[numcsrcs];
	return 0;
}

void RTPPacketBuilder::ClearCSRCList()
{
	if (!init)
		return;
	numcsrcs = 0;
}

u_int32_t RTPPacketBuilder::CreateNewSSRC()
{
	ssrc = rtprnd.GetRandom32();
	timestamp = rtprnd.GetRandom32();
	seqnr = rtprnd.GetRandom16();

	// p 38: the count SHOULD be reset if the sender changes its SSRC identifier
	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
}

u_int32_t RTPPacketBuilder::CreateNewSSRC(RTPSources &sources)
{
	bool found;
	
	do
	{
		ssrc = rtprnd.GetRandom32();
		found = sources.GotEntry(ssrc);
	} while (found);
	
	timestamp = rtprnd.GetRandom32();
	seqnr = rtprnd.GetRandom16();

	// p 38: the count SHOULD be reset if the sender changes its SSRC identifier
	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
}

int RTPPacketBuilder::BuildPacket(const void *data,size_t len)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	if (!defptset)
		return ERR_RTP_PACKBUILD_DEFAULTPAYLOADTYPENOTSET;
	if (!defmarkset)
		return ERR_RTP_PACKBUILD_DEFAULTMARKNOTSET;
	if (!deftsset)
		return ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET;
	return PrivateBuildPacket(data,len,defaultpayloadtype,defaultmark,defaulttimestampinc,false);
}

int RTPPacketBuilder::BuildPacket(const void *data,size_t len,
                u_int8_t pt,bool mark,u_int32_t timestampinc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	return PrivateBuildPacket(data,len,pt,mark,timestampinc,false);
}

int RTPPacketBuilder::BuildPacketEx(const void *data,size_t len,
                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	if (!defptset)
		return ERR_RTP_PACKBUILD_DEFAULTPAYLOADTYPENOTSET;
	if (!defmarkset)
		return ERR_RTP_PACKBUILD_DEFAULTMARKNOTSET;
	if (!deftsset)
		return ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET;
	return PrivateBuildPacket(data,len,defaultpayloadtype,defaultmark,defaulttimestampinc,true,hdrextID,hdrextdata,numhdrextwords);
}

int RTPPacketBuilder::BuildPacketEx(const void *data,size_t len,
                  u_int8_t pt,bool mark,u_int32_t timestampinc,
		  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	return PrivateBuildPacket(data,len,pt,mark,timestampinc,true,hdrextID,hdrextdata,numhdrextwords);

}

int RTPPacketBuilder::PrivateBuildPacket(const void *data,size_t len,
	                  u_int8_t pt,bool mark,u_int32_t timestampinc,bool gotextension,
	                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	RTPPacket p(pt,data,len,seqnr,timestamp,ssrc,mark,numcsrcs,csrcs,gotextension,hdrextID,
	            (u_int16_t)numhdrextwords,hdrextdata,buffer,maxpacksize);
	int status = p.GetCreationError();

	if (status < 0)
		return status;
	packetlength = p.GetPacketLength();

	if (numpackets == 0) // first packet
	{
		lastwallclocktime = RTPTime::CurrentTime();
		lastrtptimestamp = timestamp;
		prevrtptimestamp = timestamp;
	}
	else if (timestamp != prevrtptimestamp)
	{
		lastwallclocktime = RTPTime::CurrentTime();
		lastrtptimestamp = timestamp;
		prevrtptimestamp = timestamp;
	}
	
	numpayloadbytes += (u_int32_t)p.GetPayloadLength();
	numpackets++;
	timestamp += timestampinc;
	seqnr++;


	return 0;
}


