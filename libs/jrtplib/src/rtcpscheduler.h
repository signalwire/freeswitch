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

#ifndef RTCPSCHEDULER_H

#define RTCPSCHEDULER_H

#include "rtpconfig.h"
#include "rtptimeutilities.h"
#include "rtprandom.h"

class RTCPCompoundPacket;
class RTPPacket;
class RTPSources;

class RTCPSchedulerParams
{
public:
	RTCPSchedulerParams();
	~RTCPSchedulerParams();
	int SetRTCPBandwidth(double bw); // bandwidth is in bytes per second
	double GetRTCPBandwidth() const							{ return bandwidth; }
	int SetSenderBandwidthFraction(double fraction);
	double GetSenderBandwidthFraction() const					{ return senderfraction; }
	int SetMinimumTransmissionInterval(const RTPTime &t);
	RTPTime GetMinimumTransmissionInterval() const					{ return mininterval; }
	void SetUseHalfAtStartup(bool usehalf)						{ usehalfatstartup = usehalf; }
	bool GetUseHalfAtStartup() const						{ return usehalfatstartup; }
	void SetRequestImmediateBYE(bool v)						{ immediatebye = v; }
	bool GetRequestImmediateBYE() const						{ return immediatebye; }	
private:
	double bandwidth;
	double senderfraction;
	RTPTime mininterval;
	bool usehalfatstartup;
	bool immediatebye;
};

class RTCPScheduler
{
public:
	RTCPScheduler(RTPSources &sources);
	~RTCPScheduler();
	void Reset();

	void SetParameters(const RTCPSchedulerParams &params)						{ schedparams = params; }
	RTCPSchedulerParams GetParameters() const							{ return schedparams; }

	void SetHeaderOverhead(size_t numbytes)								{ headeroverhead = numbytes; }
	size_t GetHeaderOverhead() const								{ return headeroverhead; }

	void AnalyseIncoming(RTCPCompoundPacket &rtcpcomppack);
	void AnalyseOutgoing(RTCPCompoundPacket &rtcpcomppack);

	// is to be called when a source times out or when a bye packet was received
	void ActiveMemberDecrease();
	void ScheduleBYEPacket(size_t packetsize);

	RTPTime GetTransmissionDelay();
	
	// Returns true is it is time to send a packet and then recalculates the next rtcp time
	// So, if the function would be called immediately after it returned true, it would then
	// return false
	bool IsTime();

	RTPTime CalculateDeterministicInterval(bool sender = false);
private:
	void CalculateNextRTCPTime();
	void PerformReverseReconsideration();
	RTPTime CalculateBYETransmissionInterval();
	RTPTime CalculateTransmissionInterval(bool sender);
	
	RTPSources &sources;
	RTCPSchedulerParams schedparams;
	size_t headeroverhead;
	size_t avgrtcppacksize;
	bool hassentrtcp;
	bool firstcall;
	RTPTime nextrtcptime;
	RTPTime prevrtcptime;
	int pmembers;

	// for BYE packet scheduling
	bool byescheduled;
	int byemembers,pbyemembers;
	size_t avgbyepacketsize;
	bool sendbyenow;

	RTPRandom rtprand;
};

#endif // RTCPSCHEDULER_H

