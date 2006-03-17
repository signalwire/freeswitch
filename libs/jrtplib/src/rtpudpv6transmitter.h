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

#ifndef RTPUDPV6TRANSMITTER_H

#define RTPUDPV6TRANSMITTER_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtptransmitter.h"
#include "rtpipv6destination.h"
#include "rtphashtable.h"
#include "rtpkeyhashtable.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32
#include <string.h>
#include <list>

#ifdef RTP_SUPPORT_THREAD
	#include <jmutex.h>
#endif // RTP_SUPPORT_THREAD

#define RTPUDPV6TRANS_HASHSIZE									8317
#define RTPUDPV6TRANS_DEFAULTPORTBASE								5000

class RTPUDPv6TransmissionParams : public RTPTransmissionParams
{
public:
	RTPUDPv6TransmissionParams():RTPTransmissionParams(RTPTransmitter::IPv6UDPProto)	{ portbase = RTPUDPV6TRANS_DEFAULTPORTBASE; for (int i = 0 ; i < 16 ; i++) bindIP.s6_addr[i] = 0; multicastTTL = 1; mcastifidx = 0; }
	void SetBindIP(in6_addr ip)								{ bindIP = ip; }
	void SetMulticastInterfaceIndex(unsigned int idx)					{ mcastifidx = idx; }
	void SetPortbase(uint16_t pbase)							{ portbase = pbase; }
	void SetMulticastTTL(uint8_t mcastTTL)							{ multicastTTL = mcastTTL; }
	void SetLocalIPList(std::list<in6_addr> &iplist)					{ localIPs = iplist; } 
	void ClearLocalIPList()									{ localIPs.clear(); }
	in6_addr GetBindIP() const								{ return bindIP; }
	unsigned int GetMulticastInterfaceIndex() const						{ return mcastifidx; }
	uint16_t GetPortbase() const								{ return portbase; }
	uint8_t GetMulticastTTL() const							{ return multicastTTL; }
	const std::list<in6_addr> &GetLocalIPList() const					{ return localIPs; }
private:
	uint16_t portbase;
	in6_addr bindIP;
	unsigned int mcastifidx;
	std::list<in6_addr> localIPs;
	uint8_t multicastTTL;
};

class RTPUDPv6TransmissionInfo : public RTPTransmissionInfo
{
public:
	RTPUDPv6TransmissionInfo(std::list<in6_addr> iplist,jrtp_socket_t rtpsock,jrtp_socket_t rtcpsock) : RTPTransmissionInfo(RTPTransmitter::IPv6UDPProto) 
												{ localIPlist = iplist; rtpsocket = rtpsock; rtcpsocket = rtcpsock; }

	~RTPUDPv6TransmissionInfo()								{ }
	std::list<in6_addr> GetLocalIPList() const						{ return localIPlist; }
	jrtp_socket_t GetRTPSocket() const								{ return rtpsocket; }
	jrtp_socket_t GetRTCPSocket() const								{ return rtcpsocket; }
private:
	std::list<in6_addr> localIPlist;
	jrtp_socket_t rtpsocket,rtcpsocket;
};
		
#ifdef RTP_SUPPORT_INLINETEMPLATEPARAM
	inline int RTPUDPv6Trans_GetHashIndex_IPv6Dest(const RTPIPv6Destination &d)		{ in6_addr ip = d.GetIP(); return ((((uint32_t)ip.s6_addr[12])<<24)|(((uint32_t)ip.s6_addr[13])<<16)|(((uint32_t)ip.s6_addr[14])<<8)|((uint32_t)ip.s6_addr[15]))%RTPUDPV6TRANS_HASHSIZE; }
	inline int RTPUDPv6Trans_GetHashIndex_in6_addr(const in6_addr &ip)			{ return ((((uint32_t)ip.s6_addr[12])<<24)|(((uint32_t)ip.s6_addr[13])<<16)|(((uint32_t)ip.s6_addr[14])<<8)|((uint32_t)ip.s6_addr[15]))%RTPUDPV6TRANS_HASHSIZE; }
#else // No support for inline function as template parameter
	int RTPUDPv6Trans_GetHashIndex_IPv6Dest(const RTPIPv6Destination &d);
	int RTPUDPv6Trans_GetHashIndex_in6_addr(const in6_addr &ip);
#endif // RTP_SUPPORT_INLINETEMPLATEPARAM

#define RTPUDPV6TRANS_HEADERSIZE								(40+8)
	
class RTPUDPv6Transmitter : public RTPTransmitter
{
public:
	RTPUDPv6Transmitter();
	~RTPUDPv6Transmitter();

	jrtp_socket_t RTPUDPv6Transmitter::GetRTPSocket();
	jrtp_socket_t RTPUDPv6Transmitter::GetRTCPSocket();
	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();

	int GetLocalHostName(uint8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()								{ return RTPUDPV6TRANS_HEADERSIZE; }
	
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
	int ProcessAddAcceptIgnoreEntry(in6_addr ip,uint16_t port);
	int ProcessDeleteAcceptIgnoreEntry(in6_addr ip,uint16_t port);
#ifdef RTP_SUPPORT_IPV6MULTICAST
	bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV6MULTICAST
	bool ShouldAcceptData(in6_addr srcip,uint16_t srcport);
	void ClearAcceptIgnoreInfo();
	
	bool init;
	bool created;
	bool waitingfordata;
	jrtp_socket_t rtpsock,rtcpsock;
	in6_addr bindIP;
	unsigned int mcastifidx;
	std::list<in6_addr> localIPs;
	uint16_t portbase;
	uint8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	uint8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv6Destination,RTPUDPv6Trans_GetHashIndex_IPv6Dest,RTPUDPV6TRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV6MULTICAST
	RTPHashTable<const in6_addr,RTPUDPv6Trans_GetHashIndex_in6_addr,RTPUDPV6TRANS_HASHSIZE> multicastgroups;
#endif // RTP_SUPPORT_IPV6MULTICAST
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

	RTPKeyHashTable<const in6_addr,PortInfo*,RTPUDPv6Trans_GetHashIndex_in6_addr,RTPUDPV6TRANS_HASHSIZE> acceptignoreinfo;

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

#endif // RTP_SUPPORT_IPV6

#endif // RTPUDPV6TRANSMITTER_H

