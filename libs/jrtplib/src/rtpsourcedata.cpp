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

#include "rtpsourcedata.h"
#include "rtpdefines.h"
#include "rtpaddress.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32

#ifdef RTPDEBUG
	#include <iostream>
	#include <string>
#endif // RTPDEBUG

#include "rtpdebug.h"

#define ACCEPTPACKETCODE									\
		*accept = true;									\
												\
		sentdata = true;								\
		packetsreceived++;								\
		numnewpackets++;								\
												\
		if (pack->GetExtendedSequenceNumber() == 0)					\
		{										\
			baseseqnr = 0x0000FFFF;							\
			numcycles = 0x00010000;							\
		}										\
		else										\
			baseseqnr = pack->GetExtendedSequenceNumber() - 1;			\
												\
		exthighseqnr = baseseqnr + 1;							\
		prevpacktime = receivetime;							\
		prevexthighseqnr = baseseqnr;							\
		savedextseqnr = baseseqnr;							\
												\
		pack->SetExtendedSequenceNumber(exthighseqnr);					\
												\
		prevtimestamp = pack->GetTimestamp();						\
		lastmsgtime = prevpacktime;							\
		if (!ownpacket) /* for own packet, this value is set on an outgoing packet */	\
			lastrtptime = prevpacktime;

void RTPSourceStats::ProcessPacket(RTPPacket *pack,const RTPTime &receivetime,double tsunit,
                                   bool ownpacket,bool *accept,bool applyprobation,bool *onprobation)
{
	// Note that the sequence number in the RTP packet is still just the
	// 16 bit number contained in the RTP header

	*onprobation = false;
	
	if (!sentdata) // no valid packets received yet
	{
#ifdef RTP_SUPPORT_PROBATION
		if (applyprobation)
		{
			bool acceptpack = false;

			if (probation)  
			{	
				uint16_t pseq;
				uint32_t pseq2;
	
				pseq = prevseqnr;
				pseq++;
				pseq2 = (uint32_t)pseq;
				if (pseq2 == pack->GetExtendedSequenceNumber()) // ok, its the next expected packet
				{
					prevseqnr = (uint16_t)pack->GetExtendedSequenceNumber();
					probation--;	
					if (probation == 0) // probation over
						acceptpack = true;
					else
						*onprobation = true;
				}
				else // not next packet
				{
					probation = RTP_PROBATIONCOUNT;
					prevseqnr = (uint16_t)pack->GetExtendedSequenceNumber();
					*onprobation = true;
				}
			}
			else // first packet received with this SSRC ID, start probation
			{
				probation = RTP_PROBATIONCOUNT;
				prevseqnr = (uint16_t)pack->GetExtendedSequenceNumber();	
				*onprobation = true;
			}
	
			if (acceptpack)
			{
				ACCEPTPACKETCODE
			}
			else
			{
				*accept = false;
				lastmsgtime = receivetime;
			}
		}
		else // No probation
		{
			ACCEPTPACKETCODE
		}
#else // No compiled-in probation support

		ACCEPTPACKETCODE

#endif // RTP_SUPPORT_PROBATION
	}
	else // already got packets
	{
		uint16_t maxseq16;
		uint32_t extseqnr;

		// Adjust max extended sequence number and set extende seq nr of packet

		*accept = true;
		packetsreceived++;
		numnewpackets++;

		maxseq16 = (uint16_t)(exthighseqnr&0x0000FFFF);
		if (pack->GetExtendedSequenceNumber() >= maxseq16)
		{
			extseqnr = numcycles+pack->GetExtendedSequenceNumber();
			exthighseqnr = extseqnr;
		}
		else
		{
			uint16_t dif1,dif2;

			dif1 = ((uint16_t)pack->GetExtendedSequenceNumber());
			dif1 -= maxseq16;
			dif2 = maxseq16;
			dif2 -= ((uint16_t)pack->GetExtendedSequenceNumber());
			if (dif1 < dif2)
			{
				numcycles += 0x00010000;
				extseqnr = numcycles+pack->GetExtendedSequenceNumber();
				exthighseqnr = extseqnr;
			}
			else
				extseqnr = numcycles+pack->GetExtendedSequenceNumber();
		}

		pack->SetExtendedSequenceNumber(extseqnr);

		// Calculate jitter

		if (tsunit > 0)
		{
			RTPTime curtime = receivetime;
			double diffts1,diffts2,diff;

			curtime -= prevpacktime;
			diffts1 = curtime.GetDouble()/tsunit;	
			diffts2 = (double)pack->GetTimestamp() - (double)prevtimestamp;
			diff = diffts1 - diffts2;
			if (diff < 0)
				diff = -diff;
			diff -= djitter;
			diff /= 16.0;
			djitter += diff;
			jitter = (uint32_t)djitter;
		}
		else
		{
			djitter = 0;
			jitter = 0;
		}

		prevpacktime = receivetime;
		prevtimestamp = pack->GetTimestamp();
		lastmsgtime = prevpacktime;
		if (!ownpacket) // for own packet, this value is set on an outgoing packet
			lastrtptime = prevpacktime;
	}
}

RTPSourceData::RTPSourceData(uint32_t s) : byetime(0,0)
{
	ssrc = s;
	issender = false;
	iscsrc = false;
	timestampunit = -1;
	receivedbye = false;
	byereason = 0;
	byereasonlen = 0;
	rtpaddr = 0;
	rtcpaddr = 0;
	ownssrc = false;
	validated = false;
	processedinrtcp = false;			
	isrtpaddrset = false;
	isrtcpaddrset = false;
}

RTPSourceData::~RTPSourceData()
{
	FlushPackets();
	if (byereason)
		delete [] byereason;
	if (rtpaddr)
		delete rtpaddr;
	if (rtcpaddr)
		delete rtcpaddr;
}

double RTPSourceData::INF_GetEstimatedTimestampUnit() const
{
	if (!SRprevinf.HasInfo())
		return -1.0;
	
	RTPTime t1 = RTPTime(SRinf.GetNTPTimestamp());
	RTPTime t2 = RTPTime(SRprevinf.GetNTPTimestamp());
	if ((t1.GetSeconds() == 0 && t1.GetMicroSeconds() == 0) ||
	    (t2.GetSeconds() == 0 && t2.GetMicroSeconds() == 0)) // one of the times couldn't be calculated
		return -1.0;

	if (t1 < t2)
		return -1.0;

	t1 -= t2; // get the time difference
	
	uint32_t tsdiff = SRinf.GetRTPTimestamp()-SRprevinf.GetRTPTimestamp();
	
	return (t1.GetDouble()/((double)tsdiff));
}

RTPTime RTPSourceData::INF_GetRoundtripTime() const
{
	if (!RRinf.HasInfo())
		return RTPTime(0,0);
	if (RRinf.GetDelaySinceLastSR() == 0 && RRinf.GetLastSRTimestamp() == 0)
		return RTPTime(0,0);

	RTPNTPTime recvtime = RRinf.GetReceiveTime().GetNTPTime();
	uint32_t rtt = ((recvtime.GetMSW()&0xFFFF)<<16)|((recvtime.GetLSW()>>16)&0xFFFF);
	rtt -= RRinf.GetLastSRTimestamp();
	rtt -= RRinf.GetDelaySinceLastSR();

	double drtt = (((double)rtt)/65536.0);
	return RTPTime(drtt);
}

#ifdef RTPDEBUG
void RTPSourceData::Dump()
{
	std::cout << "Source data for SSRC:     " << ssrc << std::endl;
	std::cout << "    Active:               " << ((IsActive())?"Yes":"No") << std::endl;
	std::cout << "    Sender:               " << ((issender)?"Yes":"No") << std::endl;
	std::cout << "    CSRC:                 " << ((iscsrc)?"Yes":"No") << std::endl;
	std::cout << "    Received bye:         " << ((receivedbye)?"Yes":"No") << std::endl;
	std::cout << "    ProcessedInRTCP:      " << ((processedinrtcp)?"Yes":"No") << std::endl;
	std::cout << "    Timestamp unit:       " << timestampunit << std::endl;
	std::cout << "    RTP address:          ";
	if (!isrtpaddrset)
		std::cout << "Not set" << std::endl;
	else
	{
		if (rtpaddr == 0)
			std::cout << "Own session" << std::endl;
		else
			std::cout << rtpaddr->GetAddressString() << std::endl;
	}
	std::cout << "    RTCP address:         ";
	if (!isrtcpaddrset)
		std::cout << "Not set" << std::endl;
	else
	{
		if (rtcpaddr == 0)
			std::cout << "Own session" << std::endl;
		else
			std::cout << rtcpaddr->GetAddressString() << std::endl;
	}
	if (SRinf.HasInfo())
	{
		if (!SRprevinf.HasInfo())
		{
			std::cout << "    SR Info:" << std::endl;
			std::cout << "        NTP timestamp:    " << SRinf.GetNTPTimestamp().GetMSW() << ":" << SRinf.GetNTPTimestamp().GetLSW() << std::endl;
			std::cout << "        RTP timestamp:    " << SRinf.GetRTPTimestamp() << std::endl;
			std::cout << "        Packet count:     " << SRinf.GetPacketCount() << std::endl;
			std::cout << "        Octet count:      " << SRinf.GetByteCount() << std::endl;
			std::cout << "        Receive time:     " << SRinf.GetReceiveTime().GetSeconds() << std::endl;
		}	
		else
		{
			std::cout << "    SR Info:" << std::endl;
			std::cout << "        NTP timestamp:    " << SRinf.GetNTPTimestamp().GetMSW() << ":" << SRinf.GetNTPTimestamp().GetLSW()
				  << " (" << SRprevinf.GetNTPTimestamp().GetMSW() << ":" << SRprevinf.GetNTPTimestamp().GetLSW() << ")" << std::endl;
			std::cout << "        RTP timestamp:    " << SRinf.GetRTPTimestamp()
			          << " (" << SRprevinf.GetRTPTimestamp() << ")" << std::endl;
			std::cout << "        Packet count:     " << SRinf.GetPacketCount()
			          << " (" << SRprevinf.GetPacketCount() << ")" << std::endl;
			std::cout << "        Octet count:      " << SRinf.GetByteCount() 
			          << " (" << SRprevinf.GetByteCount() <<")" << std::endl;
			std::cout << "        Receive time:     " << SRinf.GetReceiveTime().GetSeconds()
			          << " (" << SRprevinf.GetReceiveTime().GetSeconds() << ")" << std::endl;
		}
	}
	if (RRinf.HasInfo())
	{
		if (!RRprevinf.HasInfo())
		{
			std::cout << "    RR Info:" << std::endl;
			std::cout << "        Fraction lost:    " << RRinf.GetFractionLost() << std::endl;
			std::cout << "        Packets lost:     " << RRinf.GetPacketsLost() << std::endl;
			std::cout << "        Ext.High.Seq:     " << RRinf.GetExtendedHighestSequenceNumber() << std::endl;
			std::cout << "        Jitter:           " << RRinf.GetJitter() << std::endl;
			std::cout << "        LSR:              " << RRinf.GetLastSRTimestamp() << std::endl;
			std::cout << "        DLSR:             " << RRinf.GetDelaySinceLastSR() << std::endl;
			std::cout << "        Receive time:     " << RRinf.GetReceiveTime().GetSeconds() << std::endl;
		}
		else
		{
			std::cout << "    RR Info:" << std::endl;
			std::cout << "        Fraction lost:    " << RRinf.GetFractionLost() 
				  << " (" << RRprevinf.GetFractionLost() << ")" << std::endl;
			std::cout << "        Packets lost:     " << RRinf.GetPacketsLost() 
			          << " (" << RRprevinf.GetPacketsLost() << ")" << std::endl;
			std::cout << "        Ext.High.Seq:     " << RRinf.GetExtendedHighestSequenceNumber() 
			          << " (" << RRprevinf.GetExtendedHighestSequenceNumber() << ")" << std::endl;
			std::cout << "        Jitter:           " << RRinf.GetJitter() 
			          << " (" << RRprevinf.GetJitter() << ")" << std::endl;
			std::cout << "        LSR:              " << RRinf.GetLastSRTimestamp() 
			          << " (" << RRprevinf.GetLastSRTimestamp() << ")" << std::endl;
			std::cout << "        DLSR:             " << RRinf.GetDelaySinceLastSR() 
			          << " (" << RRprevinf.GetDelaySinceLastSR() << ")" << std::endl;
			std::cout << "        Receive time:     " << RRinf.GetReceiveTime().GetSeconds() 
			          << " (" << RRprevinf.GetReceiveTime().GetSeconds() <<")" << std::endl;
		}
	}
	std::cout << "    Stats:" << std::endl;
	std::cout << "        Sent data:        " << ((stats.HasSentData())?"Yes":"No") << std::endl;
	std::cout << "        Packets received: " << stats.GetNumPacketsReceived() << std::endl;
	std::cout << "        Seq. base:        " << stats.GetBaseSequenceNumber() << std::endl;
	std::cout << "        Ext.High.Seq:     " << stats.GetExtendedHighestSequenceNumber() << std::endl;
	std::cout << "        Jitter:           " << stats.GetJitter() << std::endl;
	std::cout << "        New packets:      " << stats.GetNumPacketsReceivedInInterval() << std::endl;	
	std::cout << "        Saved seq. nr.:   " << stats.GetSavedExtendedSequenceNumber() << std::endl;	
	std::cout << "        RTT:              " << INF_GetRoundtripTime().GetDouble() << " seconds" << std::endl;
	if (INF_GetEstimatedTimestampUnit() > 0)
		std::cout << "        Estimated:        " << (1.0/INF_GetEstimatedTimestampUnit()) << " samples per second" << std::endl;
	std::cout << "    SDES Info:" << std::endl;

	size_t len;
	char str[1024];
	uint8_t *val;
	
	if ((val = SDESinf.GetCNAME(&len)) != 0)
	{
		memcpy(str,val,len);
		str[len] = 0;
		std::cout << "        CNAME:            " << std::string(str) << std::endl;
	}
	if ((val = SDESinf.GetName(&len)) != 0)
	{
		memcpy(str,val,len);
		str[len] = 0;
		std::cout << "        Name:             " << std::string(str) << std::endl;
	}
	if ((val = SDESinf.GetEMail(&len)) != 0)
	{
		memcpy(str,val,len);
		str[len] = 0;
		std::cout << "        EMail:            " << std::string(str) << std::endl;
	}
	if ((val = SDESinf.GetPhone(&len)) != 0)
	{
		memcpy(str,val,len);
		str[len] = 0;
		std::cout << "        phone:            " << std::string(str) << std::endl;
	}
	if ((val = SDESinf.GetLocation(&len)) != 0)
	{
		memcpy(str,val,len);
		str[len] = 0;
		std::cout << "        Location:         " << std::string(str) << std::endl;
	}
	if ((val = SDESinf.GetTool(&len)) != 0)
	{
		memcpy(str,val,len);
		str[len] = 0;
		std::cout << "        Tool:             " << std::string(str) << std::endl;
	}	
	if ((val = SDESinf.GetNote(&len)) != 0)
	{
		memcpy(str,val,len);
		str[len] = 0;
		std::cout << "        Note:             " << std::string(str) << std::endl;
	}
#ifdef RTP_SUPPORT_SDESPRIV
	SDESinf.GotoFirstPrivateValue();
	uint8_t *pref;
	size_t preflen;
	while (SDESinf.GetNextPrivateValue(&pref,&preflen,&val,&len))
	{
		char prefstr[1024];
		memcpy(prefstr,pref,preflen);
		memcpy(str,val,len);
		prefstr[preflen] = 0;
		str[len] = 0;
		std::cout << "        Private:          " << std::string(prefstr) << ":" << std::string(str) << std::endl;
	}
#endif // RTP_SUPPORT_SDESPRIV
	if (byereason)
	{
		memcpy(str,byereason,byereasonlen);
		str[byereasonlen] = 0;
		std::cout << "    BYE Reason:           " << std::string(str) << std::endl;
	}
}

#endif // RTPDEBUG

