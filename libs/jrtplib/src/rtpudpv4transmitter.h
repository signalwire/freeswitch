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
	RTPUDPv4TransmissionParams():RTPTransmissionParams(RTPTransmitter::IPv4UDPProto)	{ portbase = RTPUDPV4TRANS_DEFAULTPORTBASE; bindIP = 0; multicastTTL = 1; mcastifaceIP = 0; }
	void SetBindIP(uint32_t ip)								{ bindIP = ip; }
	void SetMulticastInterfaceIP(uint32_t ip)						{ mcastifaceIP = ip; }
	void SetPortbase(uint16_t pbase)							{ portbase = pbase; }
	void SetMulticastTTL(uint8_t mcastTTL)							{ multicastTTL = mcastTTL; }
	void SetLocalIPList(std::list<uint32_t> &iplist)					{ localIPs = iplist; } 
	void ClearLocalIPList()									{ localIPs.clear(); }
	uint32_t GetBindIP() const								{ return bindIP; }
	uint32_t GetMulticastInterfaceIP() const						{ return mcastifaceIP; }
	uint16_t GetPortbase() const								{ return portbase; }
	uint8_t GetMulticastTTL() const							{ return multicastTTL; }
	const std::list<uint32_t> &GetLocalIPList() const					{ return localIPs; }
private:
	uint16_t portbase;
	uint32_t bindIP, mcastifaceIP;
	std::list<uint32_t> localIPs;
	uint8_t multicastTTL;
};

class RTPUDPv4TransmissionInfo : public RTPTransmissionInfo
{
public:

	RTPUDPv4TransmissionInfo(std::list<uint32_t> iplist,jrtp_socket_t rtpsock,jrtp_socket_t rtcpsock) : RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto) 
		{ localIPlist = iplist; rtpsocket = rtpsock; rtcpsocket = rtcpsock; }

	~RTPUDPv4TransmissionInfo()								{ }
	std::list<uint32_t> GetLocalIPList() const						{ return localIPlist; }

private:
	std::list<uint32_t> localIPlist;
	jrtp_socket_t rtpsocket,rtcpsocket;
};
	
class RTPUDPv4Trans_GetHashIndex_IPv4Dest
{
public:
	static int GetIndex(const RTPIPv4Destination &d)							{ return d.GetIP_HBO()%RTPUDPV4TRANS_HASHSIZE; }
};

class RTPUDPv4Trans_GetHashIndex_uint32_t
{
public:
	static int GetIndex(const uint32_t &k)									{ return k%RTPUDPV4TRANS_HASHSIZE; }
};

#define RTPUDPV4TRANS_HEADERSIZE						(20+8)
	
class RTPUDPv4Transmitter : public RTPTransmitter
{
public:
	RTPUDPv4Transmitter();
	~RTPUDPv4Transmitter();

	jrtp_socket_t RTPUDPv4Transmitter::GetRTPSocket();
	jrtp_socket_t RTPUDPv4Transmitter::GetRTCPSocket();
	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();

	int GetLocalHostName(uint8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()							{ return RTPUDPV4TRANS_HEADERSIZE; }
	
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0);
	int AbortWait();
	
	int SendRTPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);

	void ResetPacketCount();
	uint32_t GetNumRTPPacketsSent();
	uint32_t GetNumRTCPPacketsSent();
				
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
	int ProcessAddAcceptIgnoreEntry(uint32_t ip,uint16_t port);
	int ProcessDeleteAcceptIgnoreEntry(uint32_t ip,uint16_t port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
	bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
	bool ShouldAcceptData(uint32_t srcip,uint16_t srcport);
	void ClearAcceptIgnoreInfo();
	
	bool init;
	bool created;
	bool waitingfordata;
	jrtp_socket_t rtpsock,rtcpsock;
	uint32_t bindIP, mcastifaceIP;
	std::list<uint32_t> localIPs;
	uint16_t portbase;
	uint8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	uint8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv4Destination,RTPUDPv4Trans_GetHashIndex_IPv4Dest,RTPUDPV4TRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
	RTPHashTable<const uint32_t,RTPUDPv4Trans_GetHashIndex_uint32_t,RTPUDPV4TRANS_HASHSIZE> multicastgroups;
#endif // RTP_SUPPORT_IPV4MULTICAST
	std::list<RTPRawPacket*> rawpacketlist;

	bool supportsmulticasting;
	size_t maxpacksize;

	class PortInfo
	{
	public:
		PortInfo() { all = false; }
		
		bool all;
		std::list<uint16_t> portlist;
	};

	RTPKeyHashTable<const uint32_t,PortInfo*,RTPUDPv4Trans_GetHashIndex_uint32_t,RTPUDPV4TRANS_HASHSIZE> acceptignoreinfo;

	// notification descriptors for AbortWait (0 is for reading, 1 for writing)
	jrtp_socket_t abortdesc[2];
	int CreateAbortDescriptors();
	void DestroyAbortDescriptors();
	void AbortWaitInternal();
#ifdef RTP_SUPPORT_THREAD
	JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD

	uint32_t rtppackcount,rtcppackcount;
};

#endif // RTPUDPV4TRANSMITTER_H

