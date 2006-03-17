/*

  This class allows for jrtp to process packets without sending them out 
  anywhere.
  The incoming messages are handed in to jrtp through the TransmissionParams 
  and can be retreived from jrtp through the normal polling mecanisms.
  The outgoing RTP/RTCP packets are given to jrtp through the normal
  session->SendPacket() and those packets are handed back out to the
  client through a callback function (packet_ready_cb).
  
  example usage : Allows for integration of RTP into gstreamer.
  
  Copyright (c) 2005 Philippe Khalaf <burger@speedy.org>
  
  This file is a part of JRTPLIB
  Copyright (c) 1999-2004 Jori Liesenborgs

  Contact: jori@lumumba.luc.ac.be

  This library was developed at the "Expertisecentrum Digitale Media"
  (http://www.edm.luc.ac.be), a research center of the "Limburgs Universitair
  Centrum" (http://www.luc.ac.be). The library is based upon work done for 
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

#ifndef RTPFAKETRANSMITTER_H

#define RTPFAKETRANSMITTER_H

#include "rtpconfig.h"

#include "rtptransmitter.h"
#include "rtpipv4destination.h"
#include "rtphashtable.h"
#include "rtpkeyhashtable.h"
#include <list>

#ifdef RTP_SUPPORT_THREAD
	#include <jmutex.h>
#endif // RTP_SUPPORT_THREAD

#define RTPFAKETRANS_HASHSIZE									8317
#define RTPFAKETRANS_DEFAULTPORTBASE								5000

// Definition of a callback that is called when a packet is ready for sending
// params (*data, data_len, dest_addr, dest_port, rtp [1 if rtp, 0 if rtcp])
typedef void(*packet_ready_cb)(uint8_t*, uint16_t, uint32_t, uint16_t, int rtp);

class RTPFakeTransmissionParams : public RTPTransmissionParams
{
public:
	RTPFakeTransmissionParams():RTPTransmissionParams(RTPTransmitter::UserDefinedProto)	{ portbase = RTPFAKETRANS_DEFAULTPORTBASE; bindIP = 0; multicastTTL = 1; currentdata = NULL;}
	void SetBindIP(uint32_t ip)								{ bindIP = ip; }
	void SetPortbase(uint16_t pbase)							{ portbase = pbase; }
	void SetMulticastTTL(uint8_t mcastTTL)							{ multicastTTL = mcastTTL; }
	void SetLocalIPList(std::list<uint32_t> &iplist)					{ localIPs = iplist; } 
	void ClearLocalIPList()									{ localIPs.clear(); }
    void SetCurrentData(uint8_t *data)                      { currentdata = data; }
    void SetCurrentDataLen(uint16_t len)                   { currentdatalen = len; }
    void SetCurrentDataAddr(uint32_t addr)                 { currentdataaddr = addr; }
    void SetCurrentDataPort(uint16_t port)                 { currentdataport = port; }
    void SetCurrentDataType(bool type)                      { currentdatatype = type; }
    void SetPacketReadyCB(packet_ready_cb cb)                 { packetreadycb = cb; };
	uint32_t GetBindIP() const								{ return bindIP; }
	uint16_t GetPortbase() const								{ return portbase; }
	uint8_t GetMulticastTTL() const							{ return multicastTTL; }
	const std::list<uint32_t> &GetLocalIPList() const					{ return localIPs; }
    uint8_t* GetCurrentData() const                     { return currentdata; }
    uint16_t GetCurrentDataLen() const                     { return currentdatalen; }
    uint32_t GetCurrentDataAddr() const                { return currentdataaddr; }
    uint16_t GetCurrentDataPort() const                 { return currentdataport; }
    bool GetCurrentDataType() const                     { return currentdatatype; }
    packet_ready_cb GetPacketReadyCB() const             { return packetreadycb; }
private:
	uint16_t portbase;
	uint32_t bindIP;
	std::list<uint32_t> localIPs;
	uint8_t multicastTTL;
    uint8_t* currentdata;
    uint16_t currentdatalen;
    uint32_t currentdataaddr;
    uint16_t currentdataport;
    bool currentdatatype;
    packet_ready_cb packetreadycb;
};

class RTPFakeTransmissionInfo : public RTPTransmissionInfo
{
public:
	RTPFakeTransmissionInfo(std::list<uint32_t> iplist,
            RTPFakeTransmissionParams *transparams) : 
        RTPTransmissionInfo(RTPTransmitter::UserDefinedProto) 
    { localIPlist = iplist; params = transparams; } 

	~RTPFakeTransmissionInfo()								{ }
	std::list<uint32_t> GetLocalIPList() const						{ return localIPlist; }
    RTPFakeTransmissionParams* GetTransParams()             { return params; }
private:
	std::list<uint32_t> localIPlist;
    RTPFakeTransmissionParams *params;
};
	
#ifdef RTP_SUPPORT_INLINETEMPLATEPARAM
	inline int RTPFakeTrans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d)				{ return d.GetIP_HBO()%RTPFAKETRANS_HASHSIZE; }
	inline int RTPFakeTrans_GetHashIndex_uint32_t(const uint32_t &k)					{ return k%RTPFAKETRANS_HASHSIZE; }
#else // No support for inline function as template parameter
	int RTPFakeTrans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d);
	int RTPFakeTrans_GetHashIndex_uint32_t(const uint32_t &k);
#endif // RTP_SUPPORT_INLINETEMPLATEPARAM

#define RTPFAKETRANS_HEADERSIZE						(20+8)
	
class RTPFakeTransmitter : public RTPTransmitter
{
public:
	RTPFakeTransmitter();
	~RTPFakeTransmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();

	int GetLocalHostName(uint8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()							{ return RTPFAKETRANS_HEADERSIZE; }
	
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
	int FakePoll();
	int ProcessAddAcceptIgnoreEntry(uint32_t ip,uint16_t port);
	int ProcessDeleteAcceptIgnoreEntry(uint32_t ip,uint16_t port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
	bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
	bool ShouldAcceptData(uint32_t srcip,uint16_t srcport);
	void ClearAcceptIgnoreInfo();
	
    RTPFakeTransmissionParams *params;
	bool init;
	bool created;
	bool waitingfordata;
	std::list<uint32_t> localIPs;
	uint16_t portbase;
	uint8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	uint8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv4Destination,RTPFakeTrans_GetHashIndex_IPv4Dest,RTPFAKETRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
//	RTPHashTable<const uint32_t,RTPFakeTrans_GetHashIndex_uint32_t,RTPFAKETRANS_HASHSIZE> multicastgroups;
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

	RTPKeyHashTable<const uint32_t,PortInfo*,RTPFakeTrans_GetHashIndex_uint32_t,RTPFAKETRANS_HASHSIZE> acceptignoreinfo;

	int CreateAbortDescriptors();
	void DestroyAbortDescriptors();
	void AbortWaitInternal();
#ifdef RTP_SUPPORT_THREAD
	JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD

	uint32_t rtppackcount,rtcppackcount;
};

#endif // RTPFakeTRANSMITTER_H

