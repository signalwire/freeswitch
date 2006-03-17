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

#include "rtcpscheduler.h"
#include "rtpsources.h"
#include "rtpdefines.h"
#include "rtcppacket.h"
#include "rtppacket.h"
#include "rtcpcompoundpacket.h"
#include "rtpsourcedata.h"

#include "rtpdebug.h"

#define RTCPSCHED_MININTERVAL						1.0

RTCPSchedulerParams::RTCPSchedulerParams() : mininterval(RTCP_DEFAULTMININTERVAL)
{
	bandwidth = 1000; // TODO What is a good value here? 
	senderfraction = RTCP_DEFAULTSENDERFRACTION;
	usehalfatstartup = RTCP_DEFAULTHALFATSTARTUP;
	immediatebye = RTCP_DEFAULTIMMEDIATEBYE;
#if (defined(WIN32) || defined(_WIN32_WCE))
	timeinit.Dummy();
#endif // WIN32 || _WIN32_WCE
}

RTCPSchedulerParams::~RTCPSchedulerParams()
{
}

int RTCPSchedulerParams::SetRTCPBandwidth(double bw)
{
	if (bw < 0.0)
		return ERR_RTP_SCHEDPARAMS_INVALIDBANDWIDTH;
	bandwidth = bw;
	return 0;
}

int RTCPSchedulerParams::SetSenderBandwidthFraction(double fraction)
{
	if (fraction < 0.0 || fraction > 1.0)
		return ERR_RTP_SCHEDPARAMS_BADFRACTION;
	senderfraction = fraction;
	return 0;
}

int RTCPSchedulerParams::SetMinimumTransmissionInterval(const RTPTime &t)
{
	double t2 = t.GetDouble();

	if (t2 < RTCPSCHED_MININTERVAL)
		return ERR_RTP_SCHEDPARAMS_BADMINIMUMINTERVAL;

	mininterval = t;
	return 0;
}

RTCPScheduler::RTCPScheduler(RTPSources &s) : sources(s),nextrtcptime(0,0),prevrtcptime(0,0)
{
	Reset();
}

RTCPScheduler::~RTCPScheduler()
{
}

void RTCPScheduler::Reset()
{
	headeroverhead = 0; // user has to set this to an appropriate value
	hassentrtcp = false;
	firstcall = true;
	avgrtcppacksize = 1000; // TODO: what is a good value for this?
	byescheduled = false;
	sendbyenow = false;
}

void RTCPScheduler::AnalyseIncoming(RTCPCompoundPacket &rtcpcomppack)
{
	bool isbye = false;
	RTCPPacket *p;
	
	rtcpcomppack.GotoFirstPacket();
	while (!isbye && ((p = rtcpcomppack.GetNextPacket()) != 0))
	{
		if (p->GetPacketType() == RTCPPacket::BYE)
			isbye = true;
	}
	
	if (!isbye)
	{
		size_t packsize = headeroverhead+rtcpcomppack.GetCompoundPacketLength();
		avgrtcppacksize = (size_t)((1.0/16.0)*((double)packsize)+(15.0/16.0)*((double)avgrtcppacksize));
	}
	else
	{
		if (byescheduled)
		{
			size_t packsize = headeroverhead+rtcpcomppack.GetCompoundPacketLength();
			avgbyepacketsize = (size_t)((1.0/16.0)*((double)packsize)+(15.0/16.0)*((double)avgbyepacketsize));
			byemembers++;
		}
	}
}

void RTCPScheduler::AnalyseOutgoing(RTCPCompoundPacket &rtcpcomppack)
{
	bool isbye = false;
	RTCPPacket *p;
	
	rtcpcomppack.GotoFirstPacket();
	while (!isbye && ((p = rtcpcomppack.GetNextPacket()) != 0))
	{
		if (p->GetPacketType() == RTCPPacket::BYE)
			isbye = true;
	}
	
	if (!isbye)
	{
		size_t packsize = headeroverhead+rtcpcomppack.GetCompoundPacketLength();
		avgrtcppacksize = (size_t)((1.0/16.0)*((double)packsize)+(15.0/16.0)*((double)avgrtcppacksize));
	}

	hassentrtcp = true;
}

RTPTime RTCPScheduler::GetTransmissionDelay()
{
	if (firstcall)
	{
		firstcall = false;
		prevrtcptime = RTPTime::CurrentTime();
		pmembers = sources.GetActiveMemberCount();
		CalculateNextRTCPTime();
	}
	
	RTPTime curtime = RTPTime::CurrentTime();

	if (curtime > nextrtcptime) // packet should be sent
		return RTPTime(0,0);

	RTPTime diff = nextrtcptime;
	diff -= curtime;
	
	return diff;
}

bool RTCPScheduler::IsTime()
{
	if (firstcall)
	{
		firstcall = false;
		prevrtcptime = RTPTime::CurrentTime();
		pmembers = sources.GetActiveMemberCount();
		CalculateNextRTCPTime();
		return false;
	}

	RTPTime currenttime = RTPTime::CurrentTime();

	if (currenttime < nextrtcptime) // timer has not yet expired
		return false;

	RTPTime checktime(0,0);
	
	if (!byescheduled)
	{
		bool aresender = false;
		RTPSourceData *srcdat;
		
		if ((srcdat = sources.GetOwnSourceInfo()) != 0)
			aresender = srcdat->IsSender();
		
		checktime = CalculateTransmissionInterval(aresender);
	}
	else
		checktime = CalculateBYETransmissionInterval();
	
	checktime += prevrtcptime;
	
	if (checktime <= currenttime) // Okay
	{
		byescheduled = false;
		prevrtcptime = currenttime;
		pmembers = sources.GetActiveMemberCount();
		CalculateNextRTCPTime();
		return true;
	}
	
	nextrtcptime = checktime;
	pmembers = sources.GetActiveMemberCount();
	
	return false;
}

void RTCPScheduler::CalculateNextRTCPTime()
{
	bool aresender = false;
	RTPSourceData *srcdat;
	
	if ((srcdat = sources.GetOwnSourceInfo()) != 0)
		aresender = srcdat->IsSender();
	
	nextrtcptime = RTPTime::CurrentTime();	
	nextrtcptime += CalculateTransmissionInterval(aresender);
}

RTPTime RTCPScheduler::CalculateDeterministicInterval(bool sender /* = false */)
{
	int numsenders = sources.GetSenderCount();
	int numtotal = sources.GetActiveMemberCount();

	// Try to avoid division by zero:
	if (numtotal == 0)
		numtotal++;

	double sfraction = ((double)numsenders)/((double)numtotal);
	double C,n;

	if (sfraction <= schedparams.GetSenderBandwidthFraction())
	{
		if (sender)
		{
			C = ((double)avgrtcppacksize)/(schedparams.GetSenderBandwidthFraction()*schedparams.GetRTCPBandwidth());
			n = (double)numsenders;
		}
		else
		{
			C = ((double)avgrtcppacksize)/((1.0-schedparams.GetSenderBandwidthFraction())*schedparams.GetRTCPBandwidth());
			n = (double)(numtotal-numsenders);
		}
	}
	else
	{
		C = ((double)avgrtcppacksize)/schedparams.GetRTCPBandwidth();
		n = (double)numtotal;
	}
	
	RTPTime Tmin = schedparams.GetMinimumTransmissionInterval();
	double tmin = Tmin.GetDouble();
	
	if (!hassentrtcp && schedparams.GetUseHalfAtStartup())
		tmin /= 2.0;

	double ntimesC = n*C;
	double Td = (tmin>ntimesC)?tmin:ntimesC;

	return RTPTime(Td);
}

RTPTime RTCPScheduler::CalculateTransmissionInterval(bool sender)
{
	RTPTime Td = CalculateDeterministicInterval(sender);
	double td,mul,T;

	td = Td.GetDouble();
	mul = rtprand.GetRandomDouble()+0.5; // gives random value between 0.5 and 1.5
	T = (td*mul)/1.21828; // see RFC 3550 p 30

	return RTPTime(T);
}

void RTCPScheduler::PerformReverseReconsideration()
{
	if (firstcall)
		return;
	
	double diff1,diff2;
	int members = sources.GetActiveMemberCount();
	
	RTPTime tc = RTPTime::CurrentTime();
	RTPTime tn_min_tc = nextrtcptime;
	tn_min_tc -= tc;
	RTPTime tc_min_tp = tc;
	tc_min_tp -= prevrtcptime;
	
	if (pmembers == 0) // avoid division by zero
		pmembers++;
	
	diff1 = (((double)members)/((double)pmembers))*tn_min_tc.GetDouble();
	diff2 = (((double)members)/((double)pmembers))*tc_min_tp.GetDouble();

	nextrtcptime = tc;
	prevrtcptime = tc;
	nextrtcptime += RTPTime(diff1);
	prevrtcptime -= RTPTime(diff2);
	
	pmembers = members;
}

void RTCPScheduler::ScheduleBYEPacket(size_t packetsize)
{
	if (byescheduled)
		return;
	
	if (firstcall)
	{
		firstcall = false;
		pmembers = sources.GetActiveMemberCount();
	}

	byescheduled = true;
	avgbyepacketsize = packetsize+headeroverhead;

	// For now, we will always use the BYE backoff algorithm as described in rfc 3550 p 33
	
	byemembers = 1;
	pbyemembers = 1;

	if (schedparams.GetRequestImmediateBYE() && sources.GetActiveMemberCount() < 50) // p 34 (top)
		sendbyenow = true;
	else
		sendbyenow = false;
	
	prevrtcptime = RTPTime::CurrentTime();
	nextrtcptime = prevrtcptime;
	nextrtcptime += CalculateBYETransmissionInterval();
}

void RTCPScheduler::ActiveMemberDecrease()
{
	if (sources.GetActiveMemberCount() < pmembers)
		PerformReverseReconsideration();
}

RTPTime RTCPScheduler::CalculateBYETransmissionInterval()
{
	if (!byescheduled)
		return RTPTime(0,0);
	
	if (sendbyenow)
		return RTPTime(0,0);
	
	double C,n;

	C = ((double)avgbyepacketsize)/((1.0-schedparams.GetSenderBandwidthFraction())*schedparams.GetRTCPBandwidth());
	n = (double)byemembers;
	
	RTPTime Tmin = schedparams.GetMinimumTransmissionInterval();
	double tmin = Tmin.GetDouble();
	
	if (schedparams.GetUseHalfAtStartup())
		tmin /= 2.0;

	double ntimesC = n*C;
	double Td = (tmin>ntimesC)?tmin:ntimesC;

	double mul = rtprand.GetRandomDouble()+0.5; // gives random value between 0.5 and 1.5
	double T = (Td*mul)/1.21828; // see RFC 3550 p 30
	
	return RTPTime(T);
}

