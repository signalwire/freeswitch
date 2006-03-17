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

#ifndef RTPSESSIONPARAMS_H

#define RTPSESSIONPARAMS_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtptransmitter.h"
#include "rtptimeutilities.h"
#include "rtpsources.h"

class RTPSessionParams
{
public:
	RTPSessionParams();

	int SetUsePollThread(bool usethread);
	bool IsUsingPollThread() const						{ return usepollthread; }
	void SetMaximumPacketSize(size_t max)					{ maxpacksize = max; }
	size_t GetMaximumPacketSize() const					{ return maxpacksize; }
	void SetAcceptOwnPackets(bool accept)					{ acceptown = accept; }
	bool AcceptOwnPackets() const						{ return acceptown; }
	void SetReceiveMode(RTPTransmitter::ReceiveMode recvmode)		{ receivemode = recvmode; }
	RTPTransmitter::ReceiveMode GetReceiveMode() const			{ return receivemode; }
	void SetOwnTimestampUnit(double tsunit)					{ owntsunit = tsunit; }
	double GetOwnTimestampUnit() const					{ return owntsunit; }
	void SetResolveLocalHostname(bool v)					{ resolvehostname = v; }
	bool GetResolveLocalHostname() const					{ return resolvehostname; }
#ifdef RTP_SUPPORT_PROBATION
	void SetProbationType(RTPSources::ProbationType probtype)		{ probationtype = probtype; }
	RTPSources::ProbationType GetProbationType() const			{ return probationtype; }
#endif // RTP_SUPPORT_PROBATION

	void SetSessionBandwidth(double sessbw)					{ sessionbandwidth = sessbw; }
	double GetSessionBandwidth() const					{ return sessionbandwidth; }
	void SetControlTrafficFraction(double frac)				{ controlfrac = frac; }
	double GetControlTrafficFraction() const				{ return controlfrac; }
	void SetSenderControlBandwidthFraction(double frac)			{ senderfrac = frac; }
	double GetSenderControlBandwidthFraction() const			{ return senderfrac; }
	void SetMinimumRTCPTransmissionInterval(const RTPTime &t)		{ mininterval = t; }
	RTPTime GetMinimumRTCPTransmissionInterval() const			{ return mininterval; }
	void SetUseHalfRTCPIntervalAtStartup(bool usehalf)			{ usehalfatstartup = usehalf; }
	bool GetUseHalfRTCPIntervalAtStartup() const				{ return usehalfatstartup; }
	void SetRequestImmediateBYE(bool v) 					{ immediatebye = v; }
	bool GetRequestImmediateBYE() const					{ return immediatebye; }
	void SetSenderReportForBYE(bool v)					{ SR_BYE = v; }
	bool GetSenderReportForBYE() const					{ return SR_BYE; }

	void SetSenderTimeoutMultiplier(double m)				{ sendermultiplier = m; }
	double GetSenderTimeoutMultiplier() const				{ return sendermultiplier; }
	void SetSourceTimeoutMultiplier(double m)				{ generaltimeoutmultiplier = m; }
	double GetSourceTimeoutMultiplier() const				{ return generaltimeoutmultiplier; }
	void SetBYETimeoutMultiplier(double m)					{ byetimeoutmultiplier = m; }
	double GetBYETimeoutMultiplier() const					{ return byetimeoutmultiplier; }
	void SetCollisionTimeoutMultiplier(double m)				{ collisionmultiplier = m; }
	double GetCollisionTimeoutMultiplier() const				{ return collisionmultiplier; }
	void SetNoteTimeoutMultiplier(double m)					{ notemultiplier = m; }
	double GetNoteTimeoutMultiplier() const					{ return notemultiplier; }
private:
	bool acceptown;
	bool usepollthread;
	int maxpacksize;
	double owntsunit;
	RTPTransmitter::ReceiveMode receivemode;
	bool resolvehostname;
#ifdef RTP_SUPPORT_PROBATION
	RTPSources::ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION
	
	double sessionbandwidth;
	double controlfrac;
	double senderfrac;
	RTPTime mininterval;
	bool usehalfatstartup;
	bool immediatebye;
	bool SR_BYE;

	double sendermultiplier;
	double generaltimeoutmultiplier;
	double byetimeoutmultiplier;
	double collisionmultiplier;
	double notemultiplier;
};

#endif // RTPSESSIONPARAMS_H

