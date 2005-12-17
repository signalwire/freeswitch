/*

  This class allows for jrtp to send GstBuffers. Allows for integration of RTP 
  into gstreamer.
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

#ifndef RTPGSTV4TRANSMITTER_H

#define RTPGSTV4TRANSMITTER_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_GST

#include "rtptransmitter.h"
#include "rtpipv4destination.h"
#include "rtphashtable.h"
#include "rtpkeyhashtable.h"
#include <list>

#include <gst/gst.h>
#include <gst/net/gstnetbuffer.h>

#ifdef RTP_SUPPORT_THREAD
	#include <jmutex.h>
#endif // RTP_SUPPORT_THREAD

#define RTPGSTv4TRANS_HASHSIZE									8317
#define RTPGSTv4TRANS_DEFAULTPORTBASE								5000

class RTPGSTv4TransmissionParams : public RTPTransmissionParams
{
public:
	RTPGSTv4TransmissionParams():RTPTransmissionParams(RTPTransmitter::IPv4GSTProto)	{ portbase = RTPGSTv4TRANS_DEFAULTPORTBASE; bindIP = 0; multicastTTL = 1; rtpsrcpad = NULL, rtcpsrcpad = NULL; currentdata = NULL;}
	void SetBindIP(u_int32_t ip)								{ bindIP = ip; }
	void SetPortbase(u_int16_t pbase)							{ portbase = pbase; }
	void SetMulticastTTL(u_int8_t mcastTTL)							{ multicastTTL = mcastTTL; }
	void SetLocalIPList(std::list<u_int32_t> &iplist)					{ localIPs = iplist; } 
	void ClearLocalIPList()									{ localIPs.clear(); }
    void SetGstRTPSrc(GstPad *src)                          { rtpsrcpad = src; }
    void SetGstRTCPSrc(GstPad *src)                          { rtcpsrcpad = src; }
    void SetCurrentData(GstNetBuffer *data)                      { currentdata = data; }
    void SetCurrentDataType(bool type)                      { currentdatatype = type; }
	u_int32_t GetBindIP() const								{ return bindIP; }
	u_int16_t GetPortbase() const								{ return portbase; }
	u_int8_t GetMulticastTTL() const							{ return multicastTTL; }
	const std::list<u_int32_t> &GetLocalIPList() const					{ return localIPs; }
    GstPad* GetGstRTPSrc() const                         { return rtpsrcpad; }
    GstPad* GetGstRTCPSrc() const                          { return rtcpsrcpad; }
    GstNetBuffer* GetCurrentData() const                     { return currentdata; }
    bool GetCurrentDataType() const                     { return currentdatatype; }
private:
	u_int16_t portbase;
	u_int32_t bindIP;
	std::list<u_int32_t> localIPs;
	u_int8_t multicastTTL;
    GstPad *rtpsrcpad;
    GstPad *rtcpsrcpad;
    bool currentdatatype;
    GstNetBuffer* currentdata;
};

class RTPGSTv4TransmissionInfo : public RTPTransmissionInfo
{
public:
	RTPGSTv4TransmissionInfo(std::list<u_int32_t> iplist,
            GstPad* rtpsrc, GstPad* rtcpsrc, RTPGSTv4TransmissionParams *transparams) : 
        RTPTransmissionInfo(RTPTransmitter::IPv4GSTProto) 
    { localIPlist = iplist; rtpsrcpad = rtpsrc;
        rtcpsrcpad = rtcpsrc; params = transparams; } 

	~RTPGSTv4TransmissionInfo()								{ }
	std::list<u_int32_t> GetLocalIPList() const						{ return localIPlist; }
    GstPad* GetGstRTPSrc()                          { return rtpsrcpad; }
    GstPad* GetGstRTCPSrc()                          { return rtcpsrcpad; }
    RTPGSTv4TransmissionParams* GetTransParams()             { return params; }
private:
	std::list<u_int32_t> localIPlist;
    GstPad *rtpsrcpad;
    GstPad *rtcpsrcpad;
    RTPGSTv4TransmissionParams *params;
};
	
#ifdef RTP_SUPPORT_INLINETEMPLATEPARAM
	inline int RTPGSTv4Trans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d)				{ return d.GetIP_HBO()%RTPGSTv4TRANS_HASHSIZE; }
	inline int RTPGSTv4Trans_GetHashIndex_u_int32_t(const u_int32_t &k)					{ return k%RTPGSTv4TRANS_HASHSIZE; }
#else // No support for inline function as template parameter
	int RTPGSTv4Trans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d);
	int RTPGSTv4Trans_GetHashIndex_u_int32_t(const u_int32_t &k);
#endif // RTP_SUPPORT_INLINETEMPLATEPARAM

#define RTPGSTv4TRANS_HEADERSIZE						(20+8)
	
class RTPGSTv4Transmitter : public RTPTransmitter
{
public:
	RTPGSTv4Transmitter();
	~RTPGSTv4Transmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();

	int GetLocalHostName(u_int8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()							{ return RTPGSTv4TRANS_HEADERSIZE; }
	
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
	int FakePoll();
	int ProcessAddAcceptIgnoreEntry(u_int32_t ip,u_int16_t port);
	int ProcessDeleteAcceptIgnoreEntry(u_int32_t ip,u_int16_t port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
	bool SetMulticastTTL(u_int8_t ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
	bool ShouldAcceptData(u_int32_t srcip,u_int16_t srcport);
	void ClearAcceptIgnoreInfo();
	
    RTPGSTv4TransmissionParams *params;
	bool init;
	bool created;
	bool waitingfordata;
	std::list<u_int32_t> localIPs;
	u_int16_t portbase;
	u_int8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	u_int8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv4Destination,RTPGSTv4Trans_GetHashIndex_IPv4Dest,RTPGSTv4TRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
//	RTPHashTable<const u_int32_t,RTPGSTv4Trans_GetHashIndex_u_int32_t,RTPGSTv4TRANS_HASHSIZE> multicastgroups;
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

	RTPKeyHashTable<const u_int32_t,PortInfo*,RTPGSTv4Trans_GetHashIndex_u_int32_t,RTPGSTv4TRANS_HASHSIZE> acceptignoreinfo;

	int CreateAbortDescriptors();
	void DestroyAbortDescriptors();
	void AbortWaitInternal();
#ifdef RTP_SUPPORT_THREAD
	JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD

	u_int32_t rtppackcount,rtcppackcount;
};

#endif // RTP_SUPPORT_GST

#endif // RTPGSTv4TRANSMITTER_H

