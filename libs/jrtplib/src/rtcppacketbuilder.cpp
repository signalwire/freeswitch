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

#include "rtcppacketbuilder.h"
#include "rtpsources.h"
#include "rtppacketbuilder.h"
#include "rtcpscheduler.h"
#include "rtpsourcedata.h"
#include "rtcpcompoundpacketbuilder.h"

#include "rtpdebug.h"

RTCPPacketBuilder::RTCPPacketBuilder(RTPSources &s,RTPPacketBuilder &pb)
	: sources(s),rtppacketbuilder(pb),prevbuildtime(0,0),transmissiondelay(0,0)
{
	init = false;
#if (defined(WIN32) || defined(_WIN32_WCE))
	timeinit.Dummy();
#endif // WIN32 || _WIN32_WCE
}

RTCPPacketBuilder::~RTCPPacketBuilder()
{
	Destroy();
}

int RTCPPacketBuilder::Init(size_t maxpacksize,double tsunit,const void *cname,size_t cnamelen)
{
	if (init)
		return ERR_RTP_RTCPPACKETBUILDER_ALREADYINIT;
	if (maxpacksize < RTP_MINPACKETSIZE)
		return ERR_RTP_RTCPPACKETBUILDER_ILLEGALMAXPACKSIZE;
	if (tsunit < 0.0)
		return ERR_RTP_RTCPPACKETBUILDER_ILLEGALTIMESTAMPUNIT;

	if (cnamelen>255)
		cnamelen = 255;
	
	maxpacketsize = maxpacksize;
	timestampunit = tsunit;
	
	int status;
	
	if ((status = ownsdesinfo.SetCNAME((const uint8_t *)cname,cnamelen)) < 0)
		return status;
	
	ClearAllSourceFlags();
	
	interval_name = -1;
	interval_email = -1;
	interval_location = -1;
	interval_phone = -1;
	interval_tool = -1;
	interval_note = -1;

	sdesbuildcount = 0;
	transmissiondelay = RTPTime(0,0);

	firstpacket = true;
	processingsdes = false;
	init = true;
	return 0;
}

void RTCPPacketBuilder::Destroy()
{
	if (!init)
		return;
	ownsdesinfo.Clear();
	init = false;
}

int RTCPPacketBuilder::BuildNextPacket(RTCPCompoundPacket **pack)
{
	if (!init)
		return ERR_RTP_RTCPPACKETBUILDER_NOTINIT;

	RTCPCompoundPacketBuilder *rtcpcomppack;
	int status;
	bool sender = false;
	RTPSourceData *srcdat;
	
	*pack = 0;
	
	rtcpcomppack = new RTCPCompoundPacketBuilder();
	if (rtcpcomppack == 0)
		return ERR_RTP_OUTOFMEM;
	
	if ((status = rtcpcomppack->InitBuild(maxpacketsize)) < 0)
	{
		delete rtcpcomppack;
		return status;
	}
	
	if ((srcdat = sources.GetOwnSourceInfo()) != 0)
	{
		if (srcdat->IsSender())
			sender = true;
	}
	
	uint32_t ssrc = rtppacketbuilder.GetSSRC();
	RTPTime curtime = RTPTime::CurrentTime();

	if (sender)
	{
		RTPTime rtppacktime = rtppacketbuilder.GetPacketTime();
		uint32_t rtppacktimestamp = rtppacketbuilder.GetPacketTimestamp();
		uint32_t packcount = rtppacketbuilder.GetPacketCount();
		uint32_t octetcount = rtppacketbuilder.GetPayloadOctetCount();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		diff += transmissiondelay; // the sample being sampled at this very instant will need a larger timestamp
		
		uint32_t tsdiff = (uint32_t)((diff.GetDouble()/timestampunit)+0.5);
		uint32_t rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		if ((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0)
		{
			delete rtcpcomppack;
			if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}
	else
	{
		if ((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)
		{
			delete rtcpcomppack;
			if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}

	uint8_t *owncname;
	size_t owncnamelen;

	owncname = ownsdesinfo.GetCNAME(&owncnamelen);

	if ((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)
	{
		delete rtcpcomppack;
		if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
	}
	if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		delete rtcpcomppack;
		if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
	}

	if (!processingsdes)
	{
		int added,skipped;
		bool full,atendoflist;

		if ((status = FillInReportBlocks(rtcpcomppack,curtime,sources.GetTotalCount(),&full,&added,&skipped,&atendoflist)) < 0)
		{
			delete rtcpcomppack;
			return status;
		}
		
		if (full && added == 0)
		{
			delete rtcpcomppack;
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		}
	
		if (!full)
		{
			processingsdes = true;
			sdesbuildcount++;
			
			ClearAllSourceFlags();
	
			doname = false;
			doemail = false;
			doloc = false;
			dophone = false;
			dotool = false;
			donote = false;
			if (interval_name > 0 && ((sdesbuildcount%interval_name) == 0)) doname = true;
			if (interval_email > 0 && ((sdesbuildcount%interval_email) == 0)) doemail = true;
			if (interval_location > 0 && ((sdesbuildcount%interval_location) == 0)) doloc = true;
			if (interval_phone > 0 && ((sdesbuildcount%interval_phone) == 0)) dophone = true;
			if (interval_tool > 0 && ((sdesbuildcount%interval_tool) == 0)) dotool = true;
			if (interval_note > 0 && ((sdesbuildcount%interval_note) == 0)) donote = true;
			
			bool processedall;
			int itemcount;
			
			if ((status = FillInSDES(rtcpcomppack,&full,&processedall,&itemcount)) < 0)
			{
				delete rtcpcomppack;
				return status;
			}

			if (processedall)
			{
				processingsdes = false;
				ClearAllSDESFlags();
				if (!full && skipped > 0) 
				{
					// if the packet isn't full and we skipped some
				        // sources that we already got in a previous packet,
					// we can add some of them now
					
					bool atendoflist;
					 
					if ((status = FillInReportBlocks(rtcpcomppack,curtime,skipped,&full,&added,&skipped,&atendoflist)) < 0)
					{
						delete rtcpcomppack;
						return status;
					}
				}
			}
		}
	}
	else // previous sdes processing wasn't finished
	{
		bool processedall;
		int itemcount;
		bool full;
			
		if ((status = FillInSDES(rtcpcomppack,&full,&processedall,&itemcount)) < 0)
		{
			delete rtcpcomppack;
			return status;
		}

		if (itemcount == 0) // Big problem: packet size is too small to let any progress happen
		{
			delete rtcpcomppack;
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		}

		if (processedall)
		{
			processingsdes = false;
			ClearAllSDESFlags();
			if (!full) 
			{
				// if the packet isn't full and we skipped some
				// we can add some report blocks
				
				int added,skipped;
				bool atendoflist;

				if ((status = FillInReportBlocks(rtcpcomppack,curtime,sources.GetTotalCount(),&full,&added,&skipped,&atendoflist)) < 0)
				{
					delete rtcpcomppack;
					return status;
				}
				if (atendoflist) // filled in all possible sources
					ClearAllSourceFlags();
			}
		}
	}
		
	if ((status = rtcpcomppack->EndBuild()) < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	*pack = rtcpcomppack;
	firstpacket = false;
	prevbuildtime = curtime;
	return 0;
}

void RTCPPacketBuilder::ClearAllSourceFlags()
{
	if (sources.GotoFirstSource())
	{
		do
		{
			RTPSourceData *srcdat = sources.GetCurrentSourceInfo();
			srcdat->SetProcessedInRTCP(false);
		} while (sources.GotoNextSource());
	}
}

int RTCPPacketBuilder::FillInReportBlocks(RTCPCompoundPacketBuilder *rtcpcomppack,const RTPTime &curtime,int maxcount,bool *full,int *added,int *skipped,bool *atendoflist)
{
	RTPSourceData *srcdat;
	int addedcount = 0;
	int skippedcount = 0;
	bool done = false;
	bool filled = false;
	bool atend = false;
	int status;

	if (sources.GotoFirstSource())
	{
		do
		{
			bool shouldprocess = false;
			
			srcdat = sources.GetCurrentSourceInfo();
			if (!srcdat->IsOwnSSRC()) // don't send to ourselves
			{
				if (!srcdat->IsCSRC()) // p 35: no reports should go to CSRCs
				{
					if (srcdat->INF_HasSentData()) // if this isn't true, INF_GetLastRTPPacketTime() won't make any sense
					{
						if (firstpacket)
							shouldprocess = true;
						else
						{
							// p 35: only if rtp packets were received since the last RTP packet, a report block
							// should be added
							
							RTPTime lastrtptime = srcdat->INF_GetLastRTPPacketTime();
							
							if (lastrtptime > prevbuildtime)
								shouldprocess = true;
						}
					}
				}
			}

			if (shouldprocess)
			{
				if (srcdat->IsProcessedInRTCP()) // already covered this one
				{
					skippedcount++;
				}
				else
				{
					uint32_t rr_ssrc = srcdat->GetSSRC();
					uint32_t num = srcdat->INF_GetNumPacketsReceivedInInterval();
					uint32_t prevseq = srcdat->INF_GetSavedExtendedSequenceNumber();
					uint32_t curseq = srcdat->INF_GetExtendedHighestSequenceNumber();
					uint32_t expected = curseq-prevseq;
					uint8_t fraclost;
					
					if (expected < num) // got duplicates
						fraclost = 0;
					else
					{
						double lost = (double)(expected-num);
						double frac = lost/((double)expected);
						fraclost = (uint8_t)(frac*256.0);
					}

					expected = curseq-srcdat->INF_GetBaseSequenceNumber();
					num = srcdat->INF_GetNumPacketsReceived();

					uint32_t diff = expected-num;
					int32_t *packlost = (int32_t *)&diff;
					
					uint32_t jitter = srcdat->INF_GetJitter();
					uint32_t lsr;
					uint32_t dlsr; 	

					if (!srcdat->SR_HasInfo())
					{
						lsr = 0;
						dlsr = 0;
					}
					else
					{
						RTPNTPTime srtime = srcdat->SR_GetNTPTimestamp();
						uint32_t m = (srtime.GetMSW()&0xFFFF);
						uint32_t l = ((srtime.GetLSW()>>16)&0xFFFF);
						lsr = ((m<<16)|l);

						RTPTime diff = curtime;
						diff -= srcdat->SR_GetReceiveTime();
						double diff2 = diff.GetDouble();
						diff2 *= 65536.0;
						dlsr = (uint32_t)diff2;
					}

					status = rtcpcomppack->AddReportBlock(rr_ssrc,fraclost,*packlost,curseq,jitter,lsr,dlsr);
					if (status < 0)
					{
						if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
						{
							done = true;
							filled = true;
						}
						else
							return status;
					}
					else
					{
						addedcount++;
						if (addedcount >= maxcount)
						{
							done = true;
							if (!sources.GotoNextSource())
								atend = true;
						}
						srcdat->INF_StartNewInterval();
						srcdat->SetProcessedInRTCP(true);
					}
				}
			}

			if (!done)
			{
				if (!sources.GotoNextSource())
				{
					atend = true;
					done = true;
				}
			}

		} while (!done);
	}
	
	*added = addedcount;
	*skipped = skippedcount;
	*full = filled;
	
	if (!atend) // search for available sources
	{
		bool shouldprocess = false;
		
		do
		{	
			srcdat = sources.GetCurrentSourceInfo();
			if (!srcdat->IsOwnSSRC()) // don't send to ourselves
			{
				if (!srcdat->IsCSRC()) // p 35: no reports should go to CSRCs
				{
					if (srcdat->INF_HasSentData()) // if this isn't true, INF_GetLastRTPPacketTime() won't make any sense
					{
						if (firstpacket)
							shouldprocess = true;
						else
						{
							// p 35: only if rtp packets were received since the last RTP packet, a report block
							// should be added
							
							RTPTime lastrtptime = srcdat->INF_GetLastRTPPacketTime();
							
							if (lastrtptime > prevbuildtime)
								shouldprocess = true;
						}
					}
				}
			}
			
			if (shouldprocess)
			{
				if (srcdat->IsProcessedInRTCP())
					shouldprocess = false;
			}

			if (!shouldprocess)
			{
				if (!sources.GotoNextSource())
					atend = true;
			}
	
		} while (!atend && !shouldprocess);
	}	

	*atendoflist = atend;
	return 0;	
}

int RTCPPacketBuilder::FillInSDES(RTCPCompoundPacketBuilder *rtcpcomppack,bool *full,bool *processedall,int *added)
{
	int status;
	uint8_t *data;
	size_t datalen;
	
	*full = false;
	*processedall = false;
	*added = 0;

	// We don't need to add a SSRC for our own data, this is still set
	// from adding the CNAME
	if (doname)
	{
		if (!ownsdesinfo.ProcessedName())
		{
			data = ownsdesinfo.GetName(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::NAME,data,datalen)) < 0)
			{
				if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedName(true);
		}
	}
	if (doemail)
	{
		if (!ownsdesinfo.ProcessedEMail())
		{
			data = ownsdesinfo.GetEMail(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::EMAIL,data,datalen)) < 0)
			{
				if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedEMail(true);
		}
	}
	if (doloc)
	{
		if (!ownsdesinfo.ProcessedLocation())
		{
			data = ownsdesinfo.GetLocation(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::LOC,data,datalen)) < 0)
			{
				if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedLocation(true);
		}
	}
	if (dophone)
	{
		if (!ownsdesinfo.ProcessedPhone())
		{
			data = ownsdesinfo.GetPhone(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::PHONE,data,datalen)) < 0)
			{
				if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedPhone(true);
		}
	}
	if (dotool)
	{
		if (!ownsdesinfo.ProcessedTool())
		{
			data = ownsdesinfo.GetTool(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::TOOL,data,datalen)) < 0)
			{
				if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedTool(true);
		}
	}
	if (donote)
	{
		if (!ownsdesinfo.ProcessedNote())
		{
			data = ownsdesinfo.GetNote(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::NOTE,data,datalen)) < 0)
			{
				if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedNote(true);
		}
	}

	*processedall = true;
	return 0;
}

void RTCPPacketBuilder::ClearAllSDESFlags()
{
	ownsdesinfo.ClearFlags();
}
	
int RTCPPacketBuilder::BuildBYEPacket(RTCPCompoundPacket **pack,const void *reason,size_t reasonlength,bool useSRifpossible)
{
	if (!init)
		return ERR_RTP_RTCPPACKETBUILDER_NOTINIT;

	RTCPCompoundPacketBuilder *rtcpcomppack;
	int status;
	
	if (reasonlength > 255)
		reasonlength = 255;
	
	*pack = 0;
	
	rtcpcomppack = new RTCPCompoundPacketBuilder();
	if (rtcpcomppack == 0)
		return ERR_RTP_OUTOFMEM;
	
	if ((status = rtcpcomppack->InitBuild(maxpacketsize)) < 0)
	{
		delete rtcpcomppack;
		return status;
	}
	
	uint32_t ssrc = rtppacketbuilder.GetSSRC();
	bool useSR = false;
	
	if (useSRifpossible)
	{
		RTPSourceData *srcdat;
		
		if ((srcdat = sources.GetOwnSourceInfo()) != 0)
		{
			if (srcdat->IsSender())
				useSR = true;
		}
	}
			
	if (useSR)
	{
		RTPTime curtime = RTPTime::CurrentTime();
		RTPTime rtppacktime = rtppacketbuilder.GetPacketTime();
		uint32_t rtppacktimestamp = rtppacketbuilder.GetPacketTimestamp();
		uint32_t packcount = rtppacketbuilder.GetPacketCount();
		uint32_t octetcount = rtppacketbuilder.GetPayloadOctetCount();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		
		uint32_t tsdiff = (uint32_t)((diff.GetDouble()/timestampunit)+0.5);
		uint32_t rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		if ((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0)
		{
			delete rtcpcomppack;
			if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}
	else
	{
		if ((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)
		{
			delete rtcpcomppack;
			if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
				return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
			return status;
		}
	}

	uint8_t *owncname;
	size_t owncnamelen;

	owncname = ownsdesinfo.GetCNAME(&owncnamelen);

	if ((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)
	{
		delete rtcpcomppack;
		if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
	}
	if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		delete rtcpcomppack;
		if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
	}

	uint32_t ssrcs[1];

	ssrcs[0] = ssrc;
	
	if ((status = rtcpcomppack->AddBYEPacket(ssrcs,1,(const uint8_t *)reason,reasonlength)) < 0)
	{
		delete rtcpcomppack;
		if (status == ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT)
			return ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON;
		return status;
	}
	
	if ((status = rtcpcomppack->EndBuild()) < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	*pack = rtcpcomppack;
	return 0;
}

