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

#include "rtpsources.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#include "rtpinternalsourcedata.h"
#include "rtptimeutilities.h"
#include "rtpdefines.h"
#include "rtcpcompoundpacket.h"
#include "rtcppacket.h"
#include "rtcpapppacket.h"
#include "rtcpbyepacket.h"
#include "rtcpsdespacket.h"
#include "rtcpsrpacket.h"
#include "rtcprrpacket.h"
#include "rtptransmitter.h"

#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

#ifndef RTP_SUPPORT_INLINETEMPLATEPARAM
	int RTPSources_GetHashIndex(const uint32_t &ssrc)       { return ssrc%RTPSOURCES_HASHSIZE; }
#endif // !RTP_SUPPORT_INLINETEMPLATEPARAM
	
RTPSources::RTPSources(ProbationType probtype)
{
	totalcount = 0;
	sendercount = 0;
	activecount = 0;
	owndata = 0;
#ifdef RTP_SUPPORT_PROBATION
	probationtype = probtype;
#endif // RTP_SUPPORT_PROBATION
}

RTPSources::~RTPSources()
{
	Clear();
}

void RTPSources::Clear()
{
	ClearSourceList();
}

void RTPSources::ClearSourceList()
{
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *sourcedata;

		sourcedata = sourcelist.GetCurrentElement();
		delete sourcedata;
		sourcelist.GotoNextElement();
	}
	sourcelist.Clear();
	owndata = 0;
	totalcount = 0;
	sendercount = 0;
	activecount = 0;
}

int RTPSources::CreateOwnSSRC(uint32_t ssrc)
{
	if (owndata != 0)
		return ERR_RTP_SOURCES_ALREADYHAVEOWNSSRC;
	if (GotEntry(ssrc))
		return ERR_RTP_SOURCES_SSRCEXISTS;

	int status;
	bool created;
	
	status = ObtainSourceDataInstance(ssrc,&owndata,&created);
	if (status < 0)
	{
		owndata = 0; // just to make sure
		return status;
	}
	owndata->SetOwnSSRC();	
	owndata->SetRTPDataAddress(0);
	owndata->SetRTCPDataAddress(0);

	// we've created a validated ssrc, so we should increase activecount
	activecount++;

	OnNewSource(owndata);
	return 0;
}

int RTPSources::DeleteOwnSSRC()
{
	if (owndata == 0)
		return ERR_RTP_SOURCES_DONTHAVEOWNSSRC;

	uint32_t ssrc = owndata->GetSSRC();

	sourcelist.GotoElement(ssrc);
	sourcelist.DeleteCurrentElement();

	totalcount--;
	if (owndata->IsSender())
		sendercount--;
	if (owndata->IsActive())
		activecount--;

	OnRemoveSource(owndata);
	
	delete owndata;
	owndata = 0;
	return 0;
}

void RTPSources::SentRTPPacket()
{
	if (owndata == 0)
		return;

	bool prevsender = owndata->IsSender();
	
	owndata->SentRTPPacket();
	if (!prevsender && owndata->IsSender())
		sendercount++;
}

int RTPSources::ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *rtptrans,bool acceptownpackets)
{
	RTPTransmitter *transmitters[1];
	int num;
	
	transmitters[0] = rtptrans;
	if (rtptrans == 0)
		num = 0;
	else
		num = 1;
	return ProcessRawPacket(rawpack,transmitters,num,acceptownpackets);
}

int RTPSources::ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *rtptrans[],int numtrans,bool acceptownpackets)
{
	int status;
	
	if (rawpack->IsRTP()) // RTP packet
	{
		RTPPacket *rtppack;
		
		// First, we'll see if the packet can be parsed
		rtppack = new RTPPacket(*rawpack);
		if (rtppack == 0)
			return ERR_RTP_OUTOFMEM;
		if ((status = rtppack->GetCreationError()) < 0)
		{
			if (status == ERR_RTP_PACKET_INVALIDPACKET)
			{
				delete rtppack;
				rtppack = 0;
			}
			else
			{
				delete rtppack;
				return status;
			}
		}
				
		// Check if the packet was valid
		if (rtppack != 0)
		{
			bool stored = false;
			bool ownpacket = false;
			int i;
			const RTPAddress *senderaddress = rawpack->GetSenderAddress();

			for (i = 0 ; !ownpacket && i < numtrans ; i++)
			{
				if (rtptrans[i]->ComesFromThisTransmitter(senderaddress))
					ownpacket = true;
			}
			
			// Check if the packet is our own.
			if (ownpacket)
			{
				// Now it depends on the user's preference
				// what to do with this packet:
				if (acceptownpackets)
				{
					// sender addres for own packets has to be NULL!
					if ((status = ProcessRTPPacket(rtppack,rawpack->GetReceiveTime(),0,&stored)) < 0)
					{
						if (!stored)
							delete rtppack;
						return status;
					}
				}
			}
			else 
			{
				if ((status = ProcessRTPPacket(rtppack,rawpack->GetReceiveTime(),senderaddress,&stored)) < 0)
				{
					if (!stored)
						delete rtppack;
					return status;
				}
			}
			if (!stored)
				delete rtppack;
		}
	}
	else // RTCP packet
	{
		RTCPCompoundPacket rtcpcomppack(*rawpack);
		bool valid = false;
		
		if ((status = rtcpcomppack.GetCreationError()) < 0)
		{
			if (status != ERR_RTP_RTCPCOMPOUND_INVALIDPACKET)
				return status;
		}
		else
			valid = true;


		if (valid)
		{
			bool ownpacket = false;
			int i;
			const RTPAddress *senderaddress = rawpack->GetSenderAddress();

			for (i = 0 ; !ownpacket && i < numtrans ; i++)
			{
				if (rtptrans[i]->ComesFromThisTransmitter(senderaddress))
					ownpacket = true;
			}

			// First check if it's a packet of this session.
			if (ownpacket)
			{
				if (acceptownpackets)
				{
					// sender address for own packets has to be NULL
					status = ProcessRTCPCompoundPacket(&rtcpcomppack,rawpack->GetReceiveTime(),0);
					if (status < 0)
						return status;
				}
			}
			else // not our own packet
			{
				status = ProcessRTCPCompoundPacket(&rtcpcomppack,rawpack->GetReceiveTime(),rawpack->GetSenderAddress());
				if (status < 0)
					return status;
			}
		}
	}
	
	return 0;
}

int RTPSources::ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,const RTPAddress *senderaddress,bool *stored)
{
	uint32_t ssrc;
	RTPInternalSourceData *srcdat;
	int status;
	bool created;

	OnRTPPacket(rtppack,receivetime,senderaddress);

	*stored = false;
	
	ssrc = rtppack->GetSSRC();
	if ((status = ObtainSourceDataInstance(ssrc,&srcdat,&created)) < 0)
		return status;

	if (created)
	{
		if ((status = srcdat->SetRTPDataAddress(senderaddress)) < 0)
			return status;
	}
	else // got a previously existing source
	{
		if (CheckCollision(srcdat,senderaddress,true))
			return 0; // ignore packet on collision
	}
	
	bool prevsender = srcdat->IsSender();
	bool prevactive = srcdat->IsActive();
	
	// The packet comes from a valid source, we can process it further now
	// The following function should delete rtppack itself if something goes
	// wrong
	if ((status = srcdat->ProcessRTPPacket(rtppack,receivetime,stored)) < 0)
		return status;

	if (!prevsender && srcdat->IsSender())
		sendercount++;
	if (!prevactive && srcdat->IsActive())
		activecount++;

	if (created)
		OnNewSource(srcdat);

	if (srcdat->IsValidated()) // process the CSRCs
	{
		RTPInternalSourceData *csrcdat;
		bool createdcsrc;

		int num = rtppack->GetCSRCCount();
		int i;

		for (i = 0 ; i < num ; i++)
		{
			if ((status = ObtainSourceDataInstance(rtppack->GetCSRC(i),&csrcdat,&createdcsrc)) < 0)
				return status;
			if (createdcsrc)
			{
				csrcdat->SetCSRC();
				if (csrcdat->IsActive())
					activecount++;
				OnNewSource(csrcdat);
			}
			else // already found an entry, possibly because of RTCP data
			{
				if (!CheckCollision(csrcdat,senderaddress,true))
					csrcdat->SetCSRC();
			}
		}
	}
	
	return 0;
}

int RTPSources::ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTCPPacket *rtcppack;
	int status;
	bool gotownssrc = ((owndata == 0)?false:true);
	uint32_t ownssrc = ((owndata != 0)?owndata->GetSSRC():0);
	
	OnRTCPCompoundPacket(rtcpcomppack,receivetime,senderaddress);
	
	rtcpcomppack->GotoFirstPacket();	
	while ((rtcppack = rtcpcomppack->GetNextPacket()) != 0)
	{
		if (rtcppack->IsKnownFormat())
		{
			switch (rtcppack->GetPacketType())
			{
			case RTCPPacket::SR:
				{
					RTCPSRPacket *p = (RTCPSRPacket *)rtcppack;
					uint32_t senderssrc = p->GetSenderSSRC();
					
					status = ProcessRTCPSenderInfo(senderssrc,p->GetNTPTimestamp(),p->GetRTPTimestamp(),
						                       p->GetSenderPacketCount(),p->GetSenderOctetCount(),
								       receivetime,senderaddress);
					if (status < 0)
						return status;
					
					bool gotinfo = false;
					if (gotownssrc)
					{
						int i;
						int num = p->GetReceptionReportCount();
						for (i = 0 ; i < num ; i++)
						{
							if (p->GetSSRC(i) == ownssrc) // data is meant for us
							{
								gotinfo = true;
								status = ProcessRTCPReportBlock(senderssrc,p->GetFractionLost(i),p->GetLostPacketCount(i),
										                        p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
													p->GetDLSR(i),receivetime,senderaddress);
								if (status < 0)
									return status;
							}
						}
					}
					if (!gotinfo)
					{
						status = UpdateReceiveTime(senderssrc,receivetime,senderaddress);
						if (status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::RR:
				{
					RTCPRRPacket *p = (RTCPRRPacket *)rtcppack;
					uint32_t senderssrc = p->GetSenderSSRC();
					
					bool gotinfo = false;

					if (gotownssrc)
					{
						int i;
						int num = p->GetReceptionReportCount();
						for (i = 0 ; i < num ; i++)
						{
							if (p->GetSSRC(i) == ownssrc)
							{
								gotinfo = true;
								status = ProcessRTCPReportBlock(senderssrc,p->GetFractionLost(i),p->GetLostPacketCount(i),
										                        p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
													p->GetDLSR(i),receivetime,senderaddress);
								if (status < 0)
									return status;
							}
						}
					}
					if (!gotinfo)
					{
						status = UpdateReceiveTime(senderssrc,receivetime,senderaddress);
						if (status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::SDES:
				{
					RTCPSDESPacket *p = (RTCPSDESPacket *)rtcppack;
					
					if (p->GotoFirstChunk())
					{
						do
						{
							uint32_t sdesssrc = p->GetChunkSSRC();
							bool updated = false;
							if (p->GotoFirstItem())
							{
								do
								{
									RTCPSDESPacket::ItemType t;
				
									if ((t = p->GetItemType()) != RTCPSDESPacket::PRIV)
									{
										updated = true;
										status = ProcessSDESNormalItem(sdesssrc,t,p->GetItemLength(),p->GetItemData(),receivetime,senderaddress);
										if (status < 0)
											return status;
									}
#ifdef RTP_SUPPORT_SDESPRIV
									else
									{
										updated = true;
										status = ProcessSDESPrivateItem(sdesssrc,p->GetPRIVPrefixLength(),p->GetPRIVPrefixData(),p->GetPRIVValueLength(),
												                        p->GetPRIVValueData(),receivetime,senderaddress);
										if (status < 0)
											return status;
									}
#endif // RTP_SUPPORT_SDESPRIV
								} while (p->GotoNextItem());
							}
							if (!updated)
							{
								status = UpdateReceiveTime(sdesssrc,receivetime,senderaddress);
								if (status < 0)
									return status;
							}
						} while (p->GotoNextChunk());
					}
				}
				break;
			case RTCPPacket::BYE:
				{
					RTCPBYEPacket *p = (RTCPBYEPacket *)rtcppack;
					int i;
					int num = p->GetSSRCCount();

					for (i = 0 ; i < num ; i++)
					{
						uint32_t byessrc = p->GetSSRC(i);
						status = ProcessBYE(byessrc,p->GetReasonLength(),p->GetReasonData(),receivetime,senderaddress);
						if (status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::APP:
				{
					RTCPAPPPacket *p = (RTCPAPPPacket *)rtcppack;

					OnAPPPacket(p,receivetime,senderaddress);
				}
				break; 
			case RTCPPacket::Unknown:
			default:
				{
					OnUnknownPacketType(rtcppack,receivetime,senderaddress);
				}
				break;
			}
		}
		else
		{
			OnUnknownPacketFormat(rtcppack,receivetime,senderaddress);
		}
	}

	return 0;
}

bool RTPSources::GotoFirstSource()
{
	sourcelist.GotoFirstElement();
	if (sourcelist.HasCurrentElement())
		return true;
	return false;
}

bool RTPSources::GotoNextSource()
{
	sourcelist.GotoNextElement();
	if (sourcelist.HasCurrentElement())
		return true;
	return false;
}

bool RTPSources::GotoPreviousSource()
{
	sourcelist.GotoPreviousElement();
	if (sourcelist.HasCurrentElement())
		return true;
	return false;
}

bool RTPSources::GotoFirstSourceWithData()
{
	bool found = false;
	
	sourcelist.GotoFirstElement();
	while (!found && sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat;

		srcdat = sourcelist.GetCurrentElement();
		if (srcdat->HasData())
			found = true;
		else
			sourcelist.GotoNextElement();
	}
			
	return found;
}

bool RTPSources::GotoNextSourceWithData()
{
	bool found = false;
	
	sourcelist.GotoNextElement();
	while (!found && sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat;

		srcdat = sourcelist.GetCurrentElement();
		if (srcdat->HasData())
			found = true;
		else
			sourcelist.GotoNextElement();
	}
			
	return found;
}

bool RTPSources::GotoPreviousSourceWithData()
{
	bool found = false;
	
	sourcelist.GotoPreviousElement();
	while (!found && sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat;

		srcdat = sourcelist.GetCurrentElement();
		if (srcdat->HasData())
			found = true;
		else
			sourcelist.GotoNextElement();
	}
			
	return found;
}

RTPSourceData *RTPSources::GetCurrentSourceInfo()
{
	if (!sourcelist.HasCurrentElement())
		return 0;
	return sourcelist.GetCurrentElement();
}

RTPSourceData *RTPSources::GetSourceInfo(uint32_t ssrc)
{
	if (sourcelist.GotoElement(ssrc) < 0)
		return 0;
	if (!sourcelist.HasCurrentElement())
		return 0;
	return sourcelist.GetCurrentElement();
}

bool RTPSources::GotEntry(uint32_t ssrc)
{
	return sourcelist.HasElement(ssrc);
}

RTPPacket *RTPSources::GetNextPacket()
{
	if (!sourcelist.HasCurrentElement())
		return 0;
	
	RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
	RTPPacket *pack = srcdat->GetNextPacket();
	return pack;
}

int RTPSources::ProcessRTCPSenderInfo(uint32_t ssrc,const RTPNTPTime &ntptime,uint32_t rtptime,
                          uint32_t packetcount,uint32_t octetcount,const RTPTime &receivetime,
			  const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;
	
	srcdat->ProcessSenderInfo(ntptime,rtptime,packetcount,octetcount,receivetime);
	
	// Call the callback
	if (created)
		OnNewSource(srcdat);

	return 0;
}

int RTPSources::ProcessRTCPReportBlock(uint32_t ssrc,uint8_t fractionlost,int32_t lostpackets,
                           uint32_t exthighseqnr,uint32_t jitter,uint32_t lsr,
			   uint32_t dlsr,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;
	
	srcdat->ProcessReportBlock(fractionlost,lostpackets,exthighseqnr,jitter,lsr,dlsr,receivetime);

	// Call the callback
	if (created)
		OnNewSource(srcdat);
			
	return 0;
}

int RTPSources::ProcessSDESNormalItem(uint32_t ssrc,RTCPSDESPacket::ItemType t,size_t itemlength,
                          const void *itemdata,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created,cnamecollis;
	int status;
	uint8_t id;
	bool prevactive;

	switch(t)
	{
	case RTCPSDESPacket::CNAME:
		id = RTCP_SDES_ID_CNAME;
		break;
	case RTCPSDESPacket::NAME:
		id = RTCP_SDES_ID_NAME;
		break;
	case RTCPSDESPacket::EMAIL:
		id = RTCP_SDES_ID_EMAIL;
		break;
	case RTCPSDESPacket::PHONE:
		id = RTCP_SDES_ID_PHONE;
		break;
	case RTCPSDESPacket::LOC:
		id = RTCP_SDES_ID_LOCATION;
		break;
	case RTCPSDESPacket::TOOL:
		id = RTCP_SDES_ID_TOOL;
		break;
	case RTCPSDESPacket::NOTE:
		id = RTCP_SDES_ID_NOTE;
		break;
	default:
		return ERR_RTP_SOURCES_ILLEGALSDESTYPE;
	}	
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;

	prevactive = srcdat->IsActive();
	status = srcdat->ProcessSDESItem(id,(const uint8_t *)itemdata,itemlength,receivetime,&cnamecollis);
	if (!prevactive && srcdat->IsActive())
		activecount++;
	
	// Call the callback
	if (created)
		OnNewSource(srcdat);
	if (cnamecollis)
		OnCNAMECollision(srcdat,senderaddress,(const uint8_t *)itemdata,itemlength);
	
	return status;
}

#ifdef RTP_SUPPORT_SDESPRIV
int RTPSources::ProcessSDESPrivateItem(uint32_t ssrc,size_t prefixlen,const void *prefixdata,
                           size_t valuelen,const void *valuedata,const RTPTime &receivetime,
			   const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;

	status = srcdat->ProcessPrivateSDESItem((const uint8_t *)prefixdata,prefixlen,(const uint8_t *)valuedata,valuelen,receivetime);
	// Call the callback
	if (created)
		OnNewSource(srcdat);
	return status;
}
#endif //RTP_SUPPORT_SDESPRIV

int RTPSources::ProcessBYE(uint32_t ssrc,size_t reasonlength,const void *reasondata,
		           const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	bool prevactive;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;

	// we'll ignore BYE packets for our own ssrc
	if (srcdat == owndata)
		return 0;
	
	prevactive = srcdat->IsActive();
	srcdat->ProcessBYEPacket((const uint8_t *)reasondata,reasonlength,receivetime);
	if (prevactive && !srcdat->IsActive())
		activecount--;
	
	// Call the callback
	if (created)
		OnNewSource(srcdat);
	OnBYEPacket(srcdat);
	return 0;
}

int RTPSources::ObtainSourceDataInstance(uint32_t ssrc,RTPInternalSourceData **srcdat,bool *created)
{
	RTPInternalSourceData *srcdat2;
	int status;
	
	if (sourcelist.GotoElement(ssrc) < 0) // No entry for this source
	{
#ifdef RTP_SUPPORT_PROBATION
		srcdat2 = new RTPInternalSourceData(ssrc,probationtype);
#else
		srcdat2 = new RTPInternalSourceData(ssrc,RTPSources::NoProbation);
#endif // RTP_SUPPORT_PROBATION
		if (srcdat2 == 0)
			return ERR_RTP_OUTOFMEM;
		if ((status = sourcelist.AddElement(ssrc,srcdat2)) < 0)
		{
			delete srcdat2;
			return status;
		}
		*srcdat = srcdat2;
		*created = true;
		totalcount++;
	}
	else
	{
		*srcdat = sourcelist.GetCurrentElement();
		*created = false;
	}
	return 0;
}

	
int RTPSources::GetRTCPSourceData(uint32_t ssrc,const RTPAddress *senderaddress,
		                  RTPInternalSourceData **srcdat2,bool *newsource)
{
	int status;
	bool created;
	RTPInternalSourceData *srcdat;
	
	*srcdat2 = 0;
	
	if ((status = ObtainSourceDataInstance(ssrc,&srcdat,&created)) < 0)
		return status;
	
	if (created)
	{
		if ((status = srcdat->SetRTCPDataAddress(senderaddress)) < 0)
			return status;
	}
	else // got a previously existing source
	{
		if (CheckCollision(srcdat,senderaddress,false))
			return 0; // ignore packet on collision
	}
	
	*srcdat2 = srcdat;
	*newsource = created;

	return 0;
}

int RTPSources::UpdateReceiveTime(uint32_t ssrc,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;
	
	// We got valid SSRC info
	srcdat->UpdateMessageTime(receivetime);
	
	// Call the callback
	if (created)
		OnNewSource(srcdat);

	return 0;
}

void RTPSources::Timeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		RTPTime lastmsgtime = srcdat->INF_GetLastMessageTime();

		// we don't want to time out ourselves
		if ((srcdat != owndata) && (lastmsgtime < checktime)) // timeout
		{
			
			totalcount--;
			if (srcdat->IsSender())
				sendercount--;
			if (srcdat->IsActive())
				activecount--;
			
			sourcelist.DeleteCurrentElement();

			OnTimeout(srcdat);
			OnRemoveSource(srcdat);
			delete srcdat;
		}
		else
		{
			newtotalcount++;
			if (srcdat->IsSender())
				newsendercount++;
			if (srcdat->IsActive())
				newactivecount++;
			sourcelist.GotoNextElement();
		}
	}
	
#ifdef RTPDEBUG
	if (newtotalcount != totalcount)
	{
		std::cout << "New total count " << newtotalcount << " doesnt match old total count " << totalcount << std::endl;
		SafeCountTotal();
	}
	if (newsendercount != sendercount)
	{
		std::cout << "New sender count " << newsendercount << " doesnt match old sender count " << sendercount << std::endl;
		SafeCountSenders();
	}
	if (newactivecount != activecount)
	{
		std::cout << "New active count " << newactivecount << " doesnt match old active count " << activecount << std::endl;
		SafeCountActive();
	}
#endif // RTPDEBUG
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

void RTPSources::SenderTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();

		newtotalcount++;
		if (srcdat->IsActive())
			newactivecount++;

		if (srcdat->IsSender())
		{
			RTPTime lastrtppacktime = srcdat->INF_GetLastRTPPacketTime();

			if (lastrtppacktime < checktime) // timeout
			{
				srcdat->ClearSenderFlag();
				sendercount--;
			}
			else
				newsendercount++;
		}
		sourcelist.GotoNextElement();
	}
	
#ifdef RTPDEBUG
	if (newtotalcount != totalcount)
	{
		std::cout << "New total count " << newtotalcount << " doesnt match old total count " << totalcount << std::endl;
		SafeCountTotal();
	}
	if (newsendercount != sendercount)
	{
		std::cout << "New sender count " << newsendercount << " doesnt match old sender count " << sendercount << std::endl;
		SafeCountSenders();
	}
	if (newactivecount != activecount)
	{
		std::cout << "New active count " << newactivecount << " doesnt match old active count " << activecount << std::endl;
		SafeCountActive();
	}
#endif // RTPDEBUG
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

void RTPSources::BYETimeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		
		if (srcdat->ReceivedBYE())
		{
			RTPTime byetime = srcdat->GetBYETime();

			if ((srcdat != owndata) && (checktime > byetime))
			{
				totalcount--;
				if (srcdat->IsSender())
					sendercount--;
				if (srcdat->IsActive())
					activecount--;
				sourcelist.DeleteCurrentElement();
				OnBYETimeout(srcdat);
				OnRemoveSource(srcdat);
				delete srcdat;
			}
			else
			{
				newtotalcount++;
				if (srcdat->IsSender())
					newsendercount++;
				if (srcdat->IsActive())
					newactivecount++;
				sourcelist.GotoNextElement();
			}
		}
		else
		{
			newtotalcount++;
			if (srcdat->IsSender())
				newsendercount++;
			if (srcdat->IsActive())
				newactivecount++;
			sourcelist.GotoNextElement();
		}
	}
	
#ifdef RTPDEBUG
	if (newtotalcount != totalcount)
	{
		std::cout << "New total count " << newtotalcount << " doesnt match old total count " << totalcount << std::endl;
		SafeCountTotal();
	}
	if (newsendercount != sendercount)
	{
		std::cout << "New sender count " << newsendercount << " doesnt match old sender count " << sendercount << std::endl;
		SafeCountSenders();
	}
	if (newactivecount != activecount)
	{
		std::cout << "New active count " << newactivecount << " doesnt match old active count " << activecount << std::endl;
		SafeCountActive();
	}
#endif // RTPDEBUG
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

void RTPSources::NoteTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		uint8_t *note;
		size_t notelen;

		note = srcdat->SDES_GetNote(&notelen);
		if (notelen != 0) // Note has been set
		{
			RTPTime notetime = srcdat->INF_GetLastSDESNoteTime();
			
			if (checktime > notetime)
			{
				srcdat->ClearNote();
				OnNoteTimeout(srcdat);
			}
		}
		
		newtotalcount++;
		if (srcdat->IsSender())
			newsendercount++;
		if (srcdat->IsActive())
			newactivecount++;
		sourcelist.GotoNextElement();
	}
	
#ifdef RTPDEBUG
	if (newtotalcount != totalcount)
	{
		std::cout << "New total count " << newtotalcount << " doesnt match old total count " << totalcount << std::endl;
		SafeCountTotal();
	}
	if (newsendercount != sendercount)
	{
		std::cout << "New sender count " << newsendercount << " doesnt match old sender count " << sendercount << std::endl;
		SafeCountSenders();
	}
	if (newactivecount != activecount)
	{
		std::cout << "New active count " << newactivecount << " doesnt match old active count " << activecount << std::endl;
		SafeCountActive();
	}
#endif // RTPDEBUG
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;

}
	
void RTPSources::MultipleTimeouts(const RTPTime &curtime,const RTPTime &sendertimeout,const RTPTime &byetimeout,const RTPTime &generaltimeout,const RTPTime &notetimeout)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime senderchecktime = curtime;
	RTPTime byechecktime = curtime;
	RTPTime generaltchecktime = curtime;
	RTPTime notechecktime = curtime;
	senderchecktime -= sendertimeout;
	byechecktime -= byetimeout;
	generaltchecktime -= generaltimeout;
	notechecktime -= notetimeout;
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		bool deleted,issender,isactive;
		bool byetimeout,normaltimeout,notetimeout;
		uint8_t *note;
		size_t notelen;
		
		issender = srcdat->IsSender();
		isactive = srcdat->IsActive();
		deleted = false;
		byetimeout = false;
		normaltimeout = false;
		notetimeout = false;

		note = srcdat->SDES_GetNote(&notelen);
		if (notelen != 0) // Note has been set
		{
			RTPTime notetime = srcdat->INF_GetLastSDESNoteTime();
			
			if (notechecktime > notetime)
			{
				notetimeout = true;
				srcdat->ClearNote();
			}
		}

		if (srcdat->ReceivedBYE())
		{
			RTPTime byetime = srcdat->GetBYETime();

			if ((srcdat != owndata) && (byechecktime > byetime))
			{
				sourcelist.DeleteCurrentElement();
				deleted = true;
				byetimeout = true;
			}
		}

		if (!deleted)
		{
			RTPTime lastmsgtime = srcdat->INF_GetLastMessageTime();

			if ((srcdat != owndata) && (lastmsgtime < generaltchecktime))
			{
				sourcelist.DeleteCurrentElement();
				deleted = true;
				normaltimeout = true;
			}
		}
		
		if (!deleted)
		{
			newtotalcount++;
			
			if (issender)
			{
				RTPTime lastrtppacktime = srcdat->INF_GetLastRTPPacketTime();

				if (lastrtppacktime < senderchecktime)
				{
					srcdat->ClearSenderFlag();
					sendercount--;
				}
				else
					newsendercount++;
			}

			if (isactive)
				newactivecount++;

			if (notetimeout)
				OnNoteTimeout(srcdat);

			sourcelist.GotoNextElement();
		}
		else // deleted entry
		{
			if (issender)
				sendercount--;
			if (isactive)
				activecount--;
			totalcount--;

			if (byetimeout)
				OnBYETimeout(srcdat);
			if (normaltimeout)
				OnTimeout(srcdat);
			delete srcdat;
		}
	}	
	
#ifdef RTPDEBUG
	if (newtotalcount != totalcount)
	{
		SafeCountTotal();
		std::cout << "New total count " << newtotalcount << " doesnt match old total count " << totalcount << std::endl;
	}
	if (newsendercount != sendercount)
	{
		SafeCountSenders();
		std::cout << "New sender count " << newsendercount << " doesnt match old sender count " << sendercount << std::endl;
	}
	if (newactivecount != activecount)
	{
		std::cout << "New active count " << newactivecount << " doesnt match old active count " << activecount << std::endl;
		SafeCountActive();
	}
#endif // RTPDEBUG
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

#ifdef RTPDEBUG
void RTPSources::Dump()
{
	std::cout << "Total count:  " << totalcount << std::endl;
	std::cout << "Sender count: " << sendercount << std::endl;
	std::cout << "Active count: " << activecount << std::endl;
	if (GotoFirstSource())
	{
		do
		{
			RTPSourceData *s;
			s = GetCurrentSourceInfo();
			s->Dump();
			std::cout << std::endl;
		} while (GotoNextSource());
	}
}

void RTPSources::SafeCountTotal()
{
	int count = 0;
	
	if (GotoFirstSource())
	{
		do
		{
			count++;	
		} while (GotoNextSource());
	}
	std::cout << "Actual total count: " << count << std::endl;
}

void RTPSources::SafeCountSenders()
{
	int count = 0;
	
	if (GotoFirstSource())
	{
		do
		{
			RTPSourceData *s;
			s = GetCurrentSourceInfo();
			if (s->IsSender())
				count++;	
		} while (GotoNextSource());
	}
	std::cout << "Actual sender count: " << count << std::endl;
}

void RTPSources::SafeCountActive()
{
	int count = 0;
	
	if (GotoFirstSource())
	{
		do
		{
			RTPSourceData *s;
			s = GetCurrentSourceInfo();
			if (s->IsActive())
				count++;	
		} while (GotoNextSource());
	}
	std::cout << "Actual active count: " << count << std::endl;
}

#endif // RTPDEBUG

bool RTPSources::CheckCollision(RTPInternalSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp)
{
	bool isset,otherisset;
	const RTPAddress *addr,*otheraddr;
	
	if (isrtp)
	{
		isset = srcdat->IsRTPAddressSet();
		addr = srcdat->GetRTPDataAddress();
		otherisset = srcdat->IsRTCPAddressSet();
		otheraddr = srcdat->GetRTCPDataAddress();
	}
	else
	{
		isset = srcdat->IsRTCPAddressSet();
		addr = srcdat->GetRTCPDataAddress();
		otherisset = srcdat->IsRTPAddressSet();
		otheraddr = srcdat->GetRTPDataAddress();
	}

	if (!isset)
	{
		if (otherisset) // got other address, can check if it comes from same host
		{
			if (otheraddr == 0) // other came from our own session
			{
				if (senderaddress != 0)
				{
					OnSSRCCollision(srcdat,senderaddress,isrtp);
					return true;
				}

				// Ok, store it

				if (isrtp)
					srcdat->SetRTPDataAddress(senderaddress);
				else
					srcdat->SetRTCPDataAddress(senderaddress);
			}
			else
			{
				if (!otheraddr->IsFromSameHost(senderaddress))
				{
					OnSSRCCollision(srcdat,senderaddress,isrtp);
					return true;
				}

				// Ok, comes from same host, store the address

				if (isrtp)
					srcdat->SetRTPDataAddress(senderaddress);
				else
					srcdat->SetRTCPDataAddress(senderaddress);
			}
		}
		else // no other address, store this one
		{
			if (isrtp)
				srcdat->SetRTPDataAddress(senderaddress);
			else
				srcdat->SetRTCPDataAddress(senderaddress);
		}
	}
	else // already got an address
	{
		if (addr == 0)
		{
			if (senderaddress != 0)
			{
				OnSSRCCollision(srcdat,senderaddress,isrtp);
				return true;
			}
		}
		else
		{
			if (!addr->IsSameAddress(senderaddress))
			{
				OnSSRCCollision(srcdat,senderaddress,isrtp);
				return true;
			}
		}
	}
	
	return false;
}
