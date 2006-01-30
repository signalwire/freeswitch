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

#include "rtpsession.h"
#include "rtperrors.h"
#include "rtppollthread.h"
#include "rtpudpv4transmitter.h"
#include "rtpudpv6transmitter.h"
#include "rtpgsttransmitter.h"
#include "rtpsessionparams.h"
#include "rtpdefines.h"
#include "rtprawpacket.h"
#include "rtppacket.h"
#include "rtptimeutilities.h"
#include "rtcpcompoundpacket.h"
#ifndef WIN32
	#include <unistd.h>
	#include <stdlib.h>
#else
	#include <winbase.h>
#endif // WIN32

#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

#ifdef RTP_SUPPORT_THREAD
	#define SOURCES_LOCK					{ if (usingpollthread) sourcesmutex.Lock(); }
	#define SOURCES_UNLOCK					{ if (usingpollthread) sourcesmutex.Unlock(); }
	#define BUILDER_LOCK					{ if (usingpollthread) buildermutex.Lock(); }
	#define BUILDER_UNLOCK					{ if (usingpollthread) buildermutex.Unlock(); }
	#define SCHED_LOCK					{ if (usingpollthread) schedmutex.Lock(); }
	#define SCHED_UNLOCK					{ if (usingpollthread) schedmutex.Unlock(); }
#else
	#define SOURCES_LOCK
	#define SOURCES_UNLOCK
	#define BUILDER_LOCK
	#define BUILDER_UNLOCK
	#define SCHED_LOCK
	#define SCHED_UNLOCK
#endif // RTP_SUPPORT_THREAD

RTPSession::RTPSession(RTPTransmitter::TransmissionProtocol proto /* = RTPTransmitter::IPv4UDPProto */ ) 
	: protocol(proto),sources(*this),rtcpsched(sources),rtcpbuilder(sources,packetbuilder)
{
	created = false;
#if (defined(WIN32) || defined(_WIN32_WCE))
	timeinit.Dummy();
#endif // WIN32 || _WIN32_WCE
}

RTPSession::~RTPSession()
{
	Destroy();
}

int RTPSession::Create(const RTPSessionParams &sessparams,const RTPTransmissionParams *transparams /* = 0 */)
{
	int status;
	
	if (created)
		return ERR_RTP_SESSION_ALREADYCREATED;

	usingpollthread = sessparams.IsUsingPollThread();
	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	
	// Check max packet size
	
	if ((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
		
	// Initialize the transmission component
	
	rtptrans = 0;
	switch(protocol)
	{
	case RTPTransmitter::IPv4UDPProto:
		rtptrans = new RTPUDPv4Transmitter();
		break;
#ifdef RTP_SUPPORT_IPV6
	case RTPTransmitter::IPv6UDPProto:
		rtptrans = new RTPUDPv6Transmitter();
		break;
#endif // RTP_SUPPORT_IPV6
#ifdef RTP_SUPPORT_GST
	case RTPTransmitter::IPv4GSTProto:
		rtptrans = new RTPGSTv4Transmitter();
		break;
#endif // RTP_SUPPORT_GST
	case RTPTransmitter::UserDefinedProto:
		rtptrans = NewUserDefinedTransmitter();
		if (rtptrans == 0)
			return ERR_RTP_SESSION_USERDEFINEDTRANSMITTERNULL;
		break;
	default:
		return ERR_RTP_SESSION_UNSUPPORTEDTRANSMISSIONPROTOCOL;
	}
	
	if (rtptrans == 0)
		return ERR_RTP_OUTOFMEM;
	if ((status = rtptrans->Init(usingpollthread)) < 0)
	{
		delete rtptrans;
		return status;
	}
	if ((status = rtptrans->Create(maxpacksize,transparams)) < 0)
	{
		delete rtptrans;
		return status;
	}

	// Initialize packet builder
	
	if ((status = packetbuilder.Init(maxpacksize)) < 0)
	{
		delete rtptrans;
		return status;
	}

#ifdef RTP_SUPPORT_PROBATION

	// Set probation type
	sources.SetProbationType(sessparams.GetProbationType());

#endif // RTP_SUPPORT_PROBATION

	// Add our own ssrc to the source table
	
	if ((status = sources.CreateOwnSSRC(packetbuilder.GetSSRC())) < 0)
	{
		packetbuilder.Destroy();
		delete rtptrans;
		return status;
	}

	// Set the initial receive mode
	
	if ((status = rtptrans->SetReceiveMode(sessparams.GetReceiveMode())) < 0)
	{
		packetbuilder.Destroy();
		sources.Clear();
		delete rtptrans;
		return status;
	}

	// Init the RTCP packet builder
	
	double timestampunit = sessparams.GetOwnTimestampUnit();
	u_int8_t buf[1024];
	size_t buflen = 1024;
	
	if ((status = CreateCNAME(buf,&buflen,sessparams.GetResolveLocalHostname())) < 0)
	{
		packetbuilder.Destroy();
		sources.Clear();
		delete rtptrans;
		return status;
	}
	
	if ((status = rtcpbuilder.Init(maxpacksize,timestampunit,buf,buflen)) < 0)
	{
		packetbuilder.Destroy();
		sources.Clear();
		delete rtptrans;
		return status;
	}

	// Set scheduler parameters
	
	rtcpsched.Reset();
	rtcpsched.SetHeaderOverhead(rtptrans->GetHeaderOverhead());

	RTCPSchedulerParams schedparams;

	sessionbandwidth = sessparams.GetSessionBandwidth();
	controlfragment = sessparams.GetControlTrafficFraction();
	
	if ((status = schedparams.SetRTCPBandwidth(sessionbandwidth*controlfragment)) < 0)
	{
		delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	if ((status = schedparams.SetSenderBandwidthFraction(sessparams.GetSenderControlBandwidthFraction())) < 0)
	{
		delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	if ((status = schedparams.SetMinimumTransmissionInterval(sessparams.GetMinimumRTCPTransmissionInterval())) < 0)
	{
		delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	schedparams.SetUseHalfAtStartup(sessparams.GetUseHalfRTCPIntervalAtStartup());
	schedparams.SetRequestImmediateBYE(sessparams.GetRequestImmediateBYE());
	
	rtcpsched.SetParameters(schedparams);

	// copy other parameters
	
	acceptownpackets = sessparams.AcceptOwnPackets();
	membermultiplier = sessparams.GetSourceTimeoutMultiplier();
	sendermultiplier = sessparams.GetSenderTimeoutMultiplier();
	byemultiplier = sessparams.GetBYETimeoutMultiplier();
	collisionmultiplier = sessparams.GetCollisionTimeoutMultiplier();
	notemultiplier = sessparams.GetNoteTimeoutMultiplier();

	// Do thread stuff if necessary
	
#ifdef RTP_SUPPORT_THREAD
	pollthread = 0;
	if (usingpollthread)
	{
		if (!sourcesmutex.IsInitialized())	
		{
			if (sourcesmutex.Init() < 0)
			{
				delete rtptrans;
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
			}
		}
		if (!buildermutex.IsInitialized())
		{
			if (buildermutex.Init() < 0)
			{
				delete rtptrans;
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
			}
		}
		if (!schedmutex.IsInitialized())
		{
			if (schedmutex.Init() < 0)
			{
				delete rtptrans;
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
			}
		}
		
		pollthread = new RTPPollThread(*this,rtcpsched);
		if (pollthread == 0)
		{
			delete rtptrans;
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return ERR_RTP_OUTOFMEM;
		}
		if ((status = pollthread->Start(rtptrans)) < 0)
		{
			delete rtptrans;
			delete pollthread;
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return status;
		}
	}
#endif // RTP_SUPPORT_THREAD	
	
	created = true;
	return 0;
}

void RTPSession::Destroy()
{
	if (!created)
		return;

#ifdef RTP_SUPPORT_THREAD
	if (pollthread)
		delete pollthread;
#endif // RTP_SUPPORT_THREAD
	
	delete rtptrans;
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	std::list<RTCPCompoundPacket *>::const_iterator it;

	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		delete (*it);
	byepackets.clear();
	
	created = false;
}

void RTPSession::BYEDestroy(const RTPTime &maxwaittime,const void *reason,size_t reasonlength)
{
	if (!created)
		return;

	// first, stop the thread so we have full control over all components
	
#ifdef RTP_SUPPORT_THREAD
	if (pollthread)
		delete pollthread;
#endif // RTP_SUPPORT_THREAD

	RTPTime stoptime = RTPTime::CurrentTime();
	stoptime += maxwaittime;

	// add bye packet to the list if we've sent data

	RTCPCompoundPacket *pack;

	if (rtptrans->GetNumRTPPacketsSent() != 0 || rtptrans->GetNumRTCPPacketsSent() != 0)
	{
		int status;
		
		reasonlength = (reasonlength>RTCP_BYE_MAXREASONLENGTH)?RTCP_BYE_MAXREASONLENGTH:reasonlength;
	       	status = rtcpbuilder.BuildBYEPacket(&pack,reason,reasonlength,useSR_BYEifpossible);
		if (status >= 0)
		{
			byepackets.push_back(pack);
	
			if (byepackets.size() == 1)
				rtcpsched.ScheduleBYEPacket(pack->GetCompoundPacketLength());
		}
	}
	
	if (!byepackets.empty())
	{
		bool done = false;
		
		while (!done)
		{
			RTPTime curtime = RTPTime::CurrentTime();
			
			if (curtime >= stoptime)
				done = true;
		
			if (rtcpsched.IsTime())
			{
				pack = *(byepackets.begin());
				byepackets.pop_front();
			
				rtptrans->SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength());
				delete pack;
				if (!byepackets.empty()) // more bye packets to send, schedule them
					rtcpsched.ScheduleBYEPacket((*(byepackets.begin()))->GetCompoundPacketLength());
				else
					done = true;
			}
			if (!done)
				RTPTime::Wait(RTPTime(0,100000));
		}
	}
	
	delete rtptrans;
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	// clear rest of bye packets
	std::list<RTCPCompoundPacket *>::const_iterator it;

	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		delete (*it);
	byepackets.clear();
	
	created = false;
}

bool RTPSession::IsActive()
{
	return created;
}

u_int32_t RTPSession::GetLocalSSRC()
{
	if (!created)
		return 0;
	
	u_int32_t ssrc;

	BUILDER_LOCK
	ssrc = packetbuilder.GetSSRC();
	BUILDER_UNLOCK
	return ssrc;
}

int RTPSession::AddDestination(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->AddDestination(addr);
}

int RTPSession::DeleteDestination(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->DeleteDestination(addr);
}

void RTPSession::ClearDestinations()
{
	if (!created)
		return;
	rtptrans->ClearDestinations();
}

bool RTPSession::SupportsMulticasting()
{
	if (!created)
		return false;
	return rtptrans->SupportsMulticasting();
}

int RTPSession::JoinMulticastGroup(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->JoinMulticastGroup(addr);
}

int RTPSession::LeaveMulticastGroup(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->LeaveMulticastGroup(addr);
}

void RTPSession::LeaveAllMulticastGroups()
{
	if (!created)
		return;
	rtptrans->LeaveAllMulticastGroups();
}

int RTPSession::SendPacket(const void *data,size_t len)
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacket(data,len)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = rtptrans->SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::SendPacket(const void *data,size_t len,
                u_int8_t pt,bool mark,u_int32_t timestampinc)
{
	int status;

	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	
	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacket(data,len,pt,mark,timestampinc)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = rtptrans->SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK
	
	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::SendPacketEx(const void *data,size_t len,
                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacketEx(data,len,hdrextID,hdrextdata,numhdrextwords)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = rtptrans->SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::SendPacketEx(const void *data,size_t len,
                  u_int8_t pt,bool mark,u_int32_t timestampinc,
                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	
	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacketEx(data,len,pt,mark,timestampinc,hdrextID,hdrextdata,numhdrextwords)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = rtptrans->SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::SetDefaultPayloadType(u_int8_t pt)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	
	int status;
	
	BUILDER_LOCK
	status = packetbuilder.SetDefaultPayloadType(pt);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetDefaultMark(bool m)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = packetbuilder.SetDefaultMark(m);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetDefaultTimestampIncrement(u_int32_t timestampinc)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = packetbuilder.SetDefaultTimestampIncrement(timestampinc);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::IncrementTimestamp(u_int32_t inc)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = packetbuilder.IncrementTimestamp(inc);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::IncrementTimestampDefault()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	
	BUILDER_LOCK
	status = packetbuilder.IncrementTimestampDefault();
	BUILDER_UNLOCK
	return status;
}

RTPTransmissionInfo *RTPSession::GetTransmissionInfo()
{
	if (!created)
		return 0;
	return rtptrans->GetTransmissionInfo();
}

int RTPSession::Poll()
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	if (usingpollthread)
		return ERR_RTP_SESSION_USINGPOLLTHREAD;
	if ((status = rtptrans->Poll()) < 0)
		return status;
	return ProcessPolledData();
}

int RTPSession::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	if (usingpollthread)
		return ERR_RTP_SESSION_USINGPOLLTHREAD;
	return rtptrans->WaitForIncomingData(delay,dataavailable);
}

int RTPSession::AbortWait()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	if (usingpollthread)
		return ERR_RTP_SESSION_USINGPOLLTHREAD;
	return rtptrans->AbortWait();
}

RTPTime RTPSession::GetRTCPDelay()
{
	if (!created)
		return RTPTime(0,0);
	if (usingpollthread)
		return RTPTime(0,0);

	SOURCES_LOCK
	SCHED_LOCK
	RTPTime t = rtcpsched.GetTransmissionDelay();
	SCHED_UNLOCK
	SOURCES_UNLOCK
	return t;
}

int RTPSession::BeginDataAccess()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	SOURCES_LOCK
	return 0;
}

bool RTPSession::GotoFirstSource()
{
	if (!created)
		return false;
	return sources.GotoFirstSource();
}

bool RTPSession::GotoNextSource()
{
	if (!created)
		return false;
	return sources.GotoNextSource();
}

bool RTPSession::GotoPreviousSource()
{
	if (!created)
		return false;
	return sources.GotoPreviousSource();
}

bool RTPSession::GotoFirstSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoFirstSourceWithData();
}

bool RTPSession::GotoNextSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoNextSourceWithData();
}

bool RTPSession::GotoPreviousSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoPreviousSourceWithData();
}

RTPSourceData *RTPSession::GetCurrentSourceInfo()
{
	if (!created)
		return 0;
	return sources.GetCurrentSourceInfo();
}

RTPSourceData *RTPSession::GetSourceInfo(u_int32_t ssrc)
{
	if (!created)
		return 0;
	return sources.GetSourceInfo(ssrc);
}

RTPPacket *RTPSession::GetNextPacket()
{
	if (!created)
		return 0;
	return sources.GetNextPacket();
}

int RTPSession::EndDataAccess()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->SetReceiveMode(m);
}

int RTPSession::AddToIgnoreList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->AddToIgnoreList(addr);
}

int RTPSession::DeleteFromIgnoreList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->DeleteFromIgnoreList(addr);
}

void RTPSession::ClearIgnoreList()
{
	if (!created)
		return;
	rtptrans->ClearIgnoreList();
}

int RTPSession::AddToAcceptList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->AddToAcceptList(addr);
}

int RTPSession::DeleteFromAcceptList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->DeleteFromAcceptList(addr);
}

void RTPSession::ClearAcceptList()
{
	if (!created)
		return;
	rtptrans->ClearAcceptList();
}

int RTPSession::SetMaximumPacketSize(size_t s)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	if (s < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
	
	int status;

	if ((status = rtptrans->SetMaximumPacketSize(s)) < 0)
		return status;

	BUILDER_LOCK
	if ((status = packetbuilder.SetMaximumPacketSize(s)) < 0)
	{
		BUILDER_UNLOCK
		// restore previous max packet size
		rtptrans->SetMaximumPacketSize(maxpacksize);
		return status;
	}
	if ((status = rtcpbuilder.SetMaximumPacketSize(s)) < 0)
	{
		// restore previous max packet size
		packetbuilder.SetMaximumPacketSize(maxpacksize);
		BUILDER_UNLOCK
		rtptrans->SetMaximumPacketSize(maxpacksize);
		return status;
	}
	BUILDER_UNLOCK
	maxpacksize = s;
	return 0;
}

int RTPSession::SetSessionBandwidth(double bw)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	SCHED_LOCK
	RTCPSchedulerParams p = rtcpsched.GetParameters();
	status = p.SetRTCPBandwidth(bw*controlfragment);
	if (status >= 0)
	{
		rtcpsched.SetParameters(p);
		sessionbandwidth = bw;
	}
	SCHED_UNLOCK
	return status;
}

int RTPSession::SetTimestampUnit(double u)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = rtcpbuilder.SetTimestampUnit(u);
	BUILDER_UNLOCK
	return status;
}

void RTPSession::SetNameInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetNameInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetEMailInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetEMailInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetLocationInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetLocationInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetPhoneInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetPhoneInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetToolInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetToolInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetNoteInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetNoteInterval(count);
	BUILDER_UNLOCK
}

int RTPSession::SetLocalName(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalName(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalEMail(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalEMail(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalLocation(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalLocation(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalPhone(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalPhone(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalTool(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalTool(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalNote(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalNote(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::ProcessPolledData()
{
	RTPRawPacket *rawpack;
	int status;
	
	SOURCES_LOCK
	while ((rawpack = rtptrans->GetNextPacket()) != 0)
	{
		sources.ClearOwnCollisionFlag();

		// since our sources instance also uses the scheduler (analysis of incoming packets)
		// we'll lock it
		SCHED_LOCK
		if ((status = sources.ProcessRawPacket(rawpack,rtptrans,acceptownpackets)) < 0)
		{
			SCHED_UNLOCK
			SOURCES_UNLOCK
			delete rawpack;
			return status;
		}
		SCHED_UNLOCK
				
		if (sources.DetectedOwnCollision()) // collision handling!
		{
			bool created;
			
			if ((status = collisionlist.UpdateAddress(rawpack->GetSenderAddress(),rawpack->GetReceiveTime(),&created)) < 0)
			{
				SOURCES_UNLOCK
				delete rawpack;
				return status;
			}

			if (created) // first time we've encountered this address, send bye packet and
			{            // change our own SSRC
				if (rtptrans->GetNumRTPPacketsSent() != 0 || rtptrans->GetNumRTCPPacketsSent() != 0)
				{
					// Only send BYE packet if we've actually sent data using this
					// SSRC
					
					RTCPCompoundPacket *rtcpcomppack;

					BUILDER_LOCK
					if ((status = rtcpbuilder.BuildBYEPacket(&rtcpcomppack,0,0,useSR_BYEifpossible)) < 0)
					{
						BUILDER_UNLOCK
						SOURCES_UNLOCK
						delete rawpack;
						return status;
					}
					BUILDER_UNLOCK

					byepackets.push_back(rtcpcomppack);
					if (byepackets.size() == 1) // was the first packet, schedule a BYE packet (otherwise there's already one scheduled)
					{
						SCHED_LOCK
						rtcpsched.ScheduleBYEPacket(rtcpcomppack->GetCompoundPacketLength());
						SCHED_UNLOCK
					}
				}
				// bye packet is built and scheduled, now change our SSRC
				// and reset the packet count in the transmitter
				
				BUILDER_LOCK
				u_int32_t newssrc = packetbuilder.CreateNewSSRC(sources);
				BUILDER_UNLOCK
					
				rtptrans->ResetPacketCount();

				// remove old entry in source table and add new one

				if ((status = sources.DeleteOwnSSRC()) < 0)
				{
					SOURCES_UNLOCK
					delete rawpack;
					return status;
				}
				if ((status = sources.CreateOwnSSRC(newssrc)) < 0)
				{
					SOURCES_UNLOCK
					delete rawpack;
					return status;
				}
			}
		}
		
		delete rawpack;
	}

	SCHED_LOCK
	RTPTime d = rtcpsched.CalculateDeterministicInterval(false);
	SCHED_UNLOCK
	
	RTPTime t = RTPTime::CurrentTime();
	double Td = d.GetDouble();
	RTPTime sendertimeout = RTPTime(Td*sendermultiplier);
	RTPTime generaltimeout = RTPTime(Td*membermultiplier);
	RTPTime byetimeout = RTPTime(Td*byemultiplier);
	RTPTime colltimeout = RTPTime(Td*collisionmultiplier);
	RTPTime notetimeout = RTPTime(Td*notemultiplier);
	
	sources.MultipleTimeouts(t,sendertimeout,byetimeout,generaltimeout,notetimeout);
	collisionlist.Timeout(t,colltimeout);
	
	// We'll check if it's time for RTCP stuff

	SCHED_LOCK
	bool istime = rtcpsched.IsTime();
	SCHED_UNLOCK
	
	if (istime)
	{
		RTCPCompoundPacket *pack;
	
		// we'll check if there's a bye packet to send, or just a normal packet

		if (byepackets.empty())
		{
			BUILDER_LOCK
			if ((status = rtcpbuilder.BuildNextPacket(&pack)) < 0)
			{
				BUILDER_UNLOCK
				SOURCES_UNLOCK
				return status;
			}
			BUILDER_UNLOCK
			if ((status = rtptrans->SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0)
			{
				SOURCES_UNLOCK
				delete pack;
				return status;
			}
		}
		else
		{
			pack = *(byepackets.begin());
			byepackets.pop_front();
			
			if ((status = rtptrans->SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0)
			{
				SOURCES_UNLOCK
				delete pack;
				return status;
			}
			
			if (!byepackets.empty()) // more bye packets to send, schedule them
			{
				SCHED_LOCK
				rtcpsched.ScheduleBYEPacket((*(byepackets.begin()))->GetCompoundPacketLength());
				SCHED_UNLOCK
			}
		}
		
		SCHED_LOCK
		rtcpsched.AnalyseOutgoing(*pack);
		SCHED_UNLOCK

		delete pack;
	}
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::CreateCNAME(u_int8_t *buffer,size_t *bufferlength,bool resolve)
{
#ifndef WIN32
	bool gotlogin = true;
#ifdef RTP_SUPPORT_GETLOGINR
	buffer[0] = 0;
	if (getlogin_r((char *)buffer,*bufferlength) != 0)
		gotlogin = false;
	else
	{
		if (buffer[0] == 0)
			gotlogin = false;
	}
	
	if (!gotlogin) // try regular getlogin
	{
		char *loginname = getlogin();
		if (loginname == 0)
			gotlogin = false;
		else
			strncpy((char *)buffer,loginname,*bufferlength);
	}
#else
	char *loginname = getlogin();
	if (loginname == 0)
		gotlogin = false;
	else
		strncpy((char *)buffer,loginname,*bufferlength);
#endif // RTP_SUPPORT_GETLOGINR
	if (!gotlogin)
	{
		char *logname = getenv("LOGNAME");
		if (logname == 0)
			return ERR_RTP_SESSION_CANTGETLOGINNAME;
		strncpy((char *)buffer,logname,*bufferlength);
	}
#else // Win32 version

#ifndef _WIN32_WCE
	DWORD len = *bufferlength;
	if (!GetUserName((LPTSTR)buffer,&len))
		strcpy((char *)buffer,"unknown");
#else 
	strcpy((char *)buffer,"unknown");
#endif // _WIN32_WCE
	
#endif // WIN32
	buffer[*bufferlength-1] = 0;

	size_t offset = strlen((const char *)buffer);
	if (offset < (*bufferlength-1))
		buffer[offset] = (u_int8_t)'@';
	offset++;

	size_t buflen2 = *bufferlength-offset;
	int status;
	
	if (resolve)
	{
		if ((status = rtptrans->GetLocalHostName(buffer+offset,&buflen2)) < 0)
			return status;
		*bufferlength = buflen2+offset;
	}
	else
	{
		char hostname[1024];
		
		strcpy(hostname,"localhost"); // just in case gethostname fails
		gethostname(hostname,1024);
		strncpy((char *)(buffer+offset),hostname,buflen2);
		*bufferlength = offset+strlen(hostname);
	}
	if (*bufferlength > RTCP_SDES_MAXITEMLENGTH)
		*bufferlength = RTCP_SDES_MAXITEMLENGTH;
	return 0;
}

#ifdef RTPDEBUG
void RTPSession::DumpSources()
{
	BeginDataAccess();
	std::cout << "----------------------------------------------------------------" << std::endl;
	sources.Dump();
	EndDataAccess();
}

void RTPSession::DumpTransmitter()
{
	if (created)
		rtptrans->Dump();
}
#endif // RTPDEBUG

