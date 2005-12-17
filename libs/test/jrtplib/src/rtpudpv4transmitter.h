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

#ifndef RTPUDPV4TRANSMITTER_H

#define RTPUDPV4TRANSMITTER_H

#include "rtpconfig.h"
#include "rtptransmitter.h"
#include "rtpipv4destination.h"
#include "rtphashtable.h"
#include "rtpkeyhashtable.h"
#include <list>

#ifdef RTP_SUPPORT_THREAD
	#include <jmutex.h>
#endif // RTP_SUPPORT_THREAD

#define RTPUDPV4TRANS_HASHSIZE									8317
#define RTPUDPV4TRANS_DEFAULTPORTBASE								5000

class RTPUDPv4TransmissionParams : public RTPTransmissionParams
{
public:
	RTPUDPv4TransmissionParams():RTPTransmissionParams(RTPTransmitter::IPv4UDPProto)	{ portbase = RTPUDPV4TRANS_DEFAULTPORTBASE; bindIP = 0; multicastTTL = 1; }
	void SetBindIP(u_int32_t ip)								{ bindIP = ip; }
	void SetPortbase(u_int16_t pbase)							{ portbase = pbase; }
	void SetMulticastTTL(u_int8_t mcastTTL)							{ multicastTTL = mcastTTL; }
	void SetLocalIPList(std::list<u_int32_t> &iplist)					{ localIPs = iplist; } 
	void ClearLocalIPList()									{ localIPs.clear(); }
	u_int32_t GetBindIP() const								{ return bindIP; }
	u_int16_t GetPortbase() const								{ return portbase; }
	u_int8_t GetMulticastTTL() const							{ return multicastTTL; }
	const std::list<u_int32_t> &GetLocalIPList() const					{ return localIPs; }
private:
	u_int16_t portbase;
	u_int32_t bindIP;
	std::list<u_int32_t> localIPs;
	u_int8_t multicastTTL;
};

class RTPUDPv4TransmissionInfo : public RTPTransmissionInfo
{
public:
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	RTPUDPv4TransmissionInfo(std::list<u_int32_t> iplist,int rtpsock,int rtcpsock) : RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto) 
#else
	RTPUDPv4TransmissionInfo(std::list<u_int32_t> iplist,SOCKET rtpsock,SOCKET rtcpsock) : RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto) 
#endif  // WIN32
												{ localIPlist = iplist; rtpsocket = rtpsock; rtcpsocket = rtcpsock; }

	~RTPUDPv4TransmissionInfo()								{ }
	std::list<u_int32_t> GetLocalIPList() const						{ return localIPlist; }
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	int GetRTPSocket() const								{ return rtpsocket; }
	int GetRTCPSocket() const								{ return rtcpsocket; }
#else
	SOCKET GetRTPSocket() const								{ return rtpsocket; }
	SOCKET GetRTCPSocket() const								{ return rtcpsocket; }
#endif // WIN32
private:
	std::list<u_int32_t> localIPlist;
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	int rtpsocket,rtcpsocket;
#else
	SOCKET rtpsocket,rtcpsocket;
#endif // WIN32
};
	
#ifdef RTP_SUPPORT_INLINETEMPLATEPARAM
	inline int RTPUDPv4Trans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d)				{ return d.GetIP_HBO()%RTPUDPV4TRANS_HASHSIZE; }
	inline int RTPUDPv4Trans_GetHashIndex_u_int32_t(const u_int32_t &k)					{ return k%RTPUDPV4TRANS_HASHSIZE; }
#else // No support for inline function as template parameter
	int RTPUDPv4Trans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d);
	int RTPUDPv4Trans_GetHashIndex_u_int32_t(const u_int32_t &k);
#endif // RTP_SUPPORT_INLINETEMPLATEPARAM

#define RTPUDPV4TRANS_HEADERSIZE						(20+8)
	
class RTPUDPv4Transmitter : public RTPTransmitter
{
public:
	RTPUDPv4Transmitter();
	~RTPUDPv4Transmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();

	int GetLocalHostName(u_int8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()							{ return RTPUDPV4TRANS_HEADERSIZE; }
	
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0);
	int AbortWait();
	
	int SendRTPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);

	void ResetPacketCount();
	u_int32_t GetNumRTPPacketsSent();
	u_int32_t GetNumRTCPPacketsSent();
				
	int AddDestination(const RTPAddress &addr);
	int DeleteDestination(const RTPAddress &addr);
	void ClearDestinations();

	bool SupportsMulticasting();
	int JoinMulticastGroup(const RTPAddress &addr);
	int LeaveMulticastGroup(const RTPAddress &addr);
	void LeaveAllMulticastGroups();

	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	int AddToIgnoreList(const RTPAddress &addr);
	int DeleteFromIgnoreList(const RTPAddress &addr);
	void ClearIgnoreList();
	int AddToAcceptList(const RTPAddress &addr);
	int DeleteFromAcceptList(const RTPAddress &addr);
	void ClearAcceptList();
	int SetMaximumPacketSize(size_t s);	
	
	bool NewDataAvailable();
	RTPRawPacket *GetNextPacket();
#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	int CreateLocalIPList();
	bool GetLocalIPList_Interfaces();
	void GetLocalIPList_DNS();
	void AddLoopbackAddress();
	void FlushPackets();
	int PollSocket(bool rtp);
	int ProcessAddAcceptIgnoreEntry(u_int32_t ip,u_int16_t port);
	int ProcessDeleteAcceptIgnoreEntry(u_int32_t ip,u_int16_t port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
	bool SetMulticastTTL(u_int8_t ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
	bool ShouldAcceptData(u_int32_t srcip,u_int16_t srcport);
	void ClearAcceptIgnoreInfo();
	
	bool init;
	bool created;
	bool waitingfordata;
#if (defined(WIN32) || defined(_WIN32_WCE))
	SOCKET rtpsock,rtcpsock;
#else // not using winsock
	int rtpsock,rtcpsock;
#endif // WIN32
	u_int32_t bindIP;
	std::list<u_int32_t> localIPs;
	u_int16_t portbase;
	u_int8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	u_int8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv4Destination,RTPUDPv4Trans_GetHashIndex_IPv4Dest,RTPUDPV4TRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
	RTPHashTable<const u_int32_t,RTPUDPv4Trans_GetHashIndex_u_int32_t,RTPUDPV4TRANS_HASHSIZE> multicastgroups;
#endif // RTP_SUPPORT_IPV4MULTICAST
	std::list<RTPRawPacket*> rawpacketlist;

	bool supportsmulticasting;
	size_t maxpacksize;

	class PortInfo
	{
	public:
		PortInfo() { all = false; }
		
		bool all;
		std::list<u_int16_t> portlist;
	};

	RTPKeyHashTable<const u_int32_t,PortInfo*,RTPUDPv4Trans_GetHashIndex_u_int32_t,RTPUDPV4TRANS_HASHSIZE> acceptignoreinfo;

	// notification descriptors for AbortWait (0 is for reading, 1 for writing)
#if (defined(WIN32) || defined(_WIN32_WCE))
	SOCKET abortdesc[2];
#else
	int abortdesc[2];
#endif // WIN32
	int CreateAbortDescriptors();
	void DestroyAbortDescriptors();
	void AbortWaitInternal();
#ifdef RTP_SUPPORT_THREAD
	JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD

	u_int32_t rtppackcount,rtcppackcount;
};

#endif // RTPUDPV4TRANSMITTER_H

