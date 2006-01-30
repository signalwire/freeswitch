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

#ifndef RTPSESSION_H

#define RTPSESSION_H

#include "rtpconfig.h"
#include "rtplibraryversion.h"
#include "rtppacketbuilder.h"
#include "rtpsessionsources.h"
#include "rtptransmitter.h"
#include "rtpcollisionlist.h"
#include "rtcpscheduler.h"
#include "rtcppacketbuilder.h"
#include "rtptimeutilities.h"
#include <list>

#ifdef RTP_SUPPORT_THREAD
	#include <jmutex.h>	
#endif // RTP_SUPPORT_THREAD

class RTPTransmitter;
class RTPSessionParams;
class RTPTransmissionParams;
class RTPAddress;
class RTPSourceData;
class RTPPacket;
class RTPPollThread;
class RTPTransmissionInfo;
class RTCPCompoundPacket;
class RTCPPacket;
class RTCPAPPPacket;

class RTPSession
{
public:
	RTPSession(RTPTransmitter::TransmissionProtocol proto = RTPTransmitter::IPv4UDPProto);
	virtual ~RTPSession();
	
	int Create(const RTPSessionParams &sessparams,const RTPTransmissionParams *transparams = 0);
	void Destroy();
	void BYEDestroy(const RTPTime &maxwaittime,const void *reason,size_t reasonlength);
	bool IsActive();
	
	u_int32_t GetLocalSSRC();
	
	int AddDestination(const RTPAddress &addr);
	int DeleteDestination(const RTPAddress &addr);
	void ClearDestinations();

	bool SupportsMulticasting();
	int JoinMulticastGroup(const RTPAddress &addr);
	int LeaveMulticastGroup(const RTPAddress &addr);
	void LeaveAllMulticastGroups();

	int SendPacket(const void *data,size_t len);
	int SendPacket(const void *data,size_t len,
	                u_int8_t pt,bool mark,u_int32_t timestampinc);
	int SendPacketEx(const void *data,size_t len,
	                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords);
	int SendPacketEx(const void *data,size_t len,
	                  u_int8_t pt,bool mark,u_int32_t timestampinc,
	                  u_int16_t hdrextID,const void *hdrextdata,size_t numhdrextwords);
	int SetDefaultPayloadType(u_int8_t pt);
	int SetDefaultMark(bool m);
	int SetDefaultTimestampIncrement(u_int32_t timestampinc);
	int IncrementTimestamp(u_int32_t inc);
	int IncrementTimestampDefault();
	
	RTPTransmissionInfo *GetTransmissionInfo();
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0);
	int AbortWait();
	RTPTime GetRTCPDelay();

	// The following methods (GotoFirstSource till GetNextPacket) should
	// be called between calls to BeginDataAccess end EndDataAccess. This
	// makes sure that nasty things don't happen (e.g. when a background
	// thread is polling for data)
	int BeginDataAccess();
	bool GotoFirstSource();
	bool GotoNextSource();
	bool GotoPreviousSource();
	bool GotoFirstSourceWithData();
	bool GotoNextSourceWithData();
	bool GotoPreviousSourceWithData();
	RTPSourceData *GetCurrentSourceInfo();
	RTPSourceData *GetSourceInfo(u_int32_t ssrc);
	RTPPacket *GetNextPacket();
	int EndDataAccess();
	
	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	int AddToIgnoreList(const RTPAddress &addr);
	int DeleteFromIgnoreList(const RTPAddress &addr);
	void ClearIgnoreList();
	int AddToAcceptList(const RTPAddress &addr);
	int DeleteFromAcceptList(const RTPAddress &addr);
	void ClearAcceptList();
	
	int SetMaximumPacketSize(size_t s);
	int SetSessionBandwidth(double bw);
	int SetTimestampUnit(double u);
	
	void SetNameInterval(int count);
	void SetEMailInterval(int count);
	void SetLocationInterval(int count);
	void SetPhoneInterval(int count);
	void SetToolInterval(int count);
	void SetNoteInterval(int count);
	int SetLocalName(const void *s,size_t len);
	int SetLocalEMail(const void *s,size_t len);
	int SetLocalLocation(const void *s,size_t len);
	int SetLocalPhone(const void *s,size_t len);
	int SetLocalTool(const void *s,size_t len);
	int SetLocalNote(const void *s,size_t len);

#ifdef RTPDEBUG
	void DumpSources();
	void DumpTransmitter();
#endif // RTPDEBUG
protected:
	virtual RTPTransmitter *NewUserDefinedTransmitter()						{ return 0; }
	
	virtual void OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime,
	                         const RTPAddress *senderaddress) 					{ }
	virtual void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
	                                  const RTPAddress *senderaddress) 				{ }
	virtual void OnSSRCCollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp)	{ }
	virtual void OnCNAMECollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,
	                              const u_int8_t *cname,size_t cnamelength)				{ }
	virtual void OnNewSource(RTPSourceData *srcdat)			 				{ }
	virtual void OnRemoveSource(RTPSourceData *srcdat)						{ }
	virtual void OnTimeout(RTPSourceData *srcdat)							{ }
	virtual void OnBYETimeout(RTPSourceData *srcdat)						{ }
	virtual void OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,
	                         const RTPAddress *senderaddress)					{ }
	virtual void OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                 const RTPAddress *senderaddress)				{ }
	virtual void OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                   const RTPAddress *senderaddress)				{ }
	virtual void OnNoteTimeout(RTPSourceData *srcdat)						{ }
	virtual void OnBYEPacket(RTPSourceData *srcdat)							{ }
#ifdef RTP_SUPPORT_THREAD
	virtual void OnPollThreadError(int errcode)							{ }
	virtual void OnPollThreadStep()									{ }
#endif // RTP_SUPPORT_THREAD
private:
	int CreateCNAME(u_int8_t *buffer,size_t *bufferlength,bool resolve);
	int ProcessPolledData();
	int ProcessRTCPCompoundPacket(RTCPCompoundPacket &rtcpcomppack,RTPRawPacket *pack);
	
	RTPTransmitter *rtptrans;
	const RTPTransmitter::TransmissionProtocol protocol;	
	bool created;
	bool usingpollthread;
	bool acceptownpackets;
	bool useSR_BYEifpossible;
	size_t maxpacksize;
	double sessionbandwidth;
	double controlfragment;
	double sendermultiplier;
	double byemultiplier;
	double membermultiplier;
	double collisionmultiplier;
	double notemultiplier;

	RTPSessionSources sources;
	RTPPacketBuilder packetbuilder;
	RTCPScheduler rtcpsched;
	RTCPPacketBuilder rtcpbuilder;
	RTPCollisionList collisionlist;

	std::list<RTCPCompoundPacket *> byepackets;
	
#ifdef RTP_SUPPORT_THREAD
	RTPPollThread *pollthread;
	JMutex sourcesmutex,buildermutex,schedmutex;

	friend class RTPPollThread;
#endif // RTP_SUPPORT_THREAD
	friend class RTPSessionSources;
};

#endif // RTPSESSION_H

