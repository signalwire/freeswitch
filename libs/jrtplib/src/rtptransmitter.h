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

#ifndef RTPTRANSMITTER_H

#define RTPTRANSMITTER_H

#include "rtpconfig.h"
#include "rtptypes.h"

class RTPRawPacket;
class RTPAddress;
class RTPTransmissionParams;
class RTPTime;
class RTPTransmissionInfo;

// Abstract class from which actual transmission components should be derived

class RTPTransmitter
{
public:
	enum TransmissionProtocol { IPv4UDPProto, IPv6UDPProto, IPv4GSTProto, UserDefinedProto };
	enum ReceiveMode { AcceptAll,AcceptSome,IgnoreSome };
protected:
	RTPTransmitter()															{ }
public:
	virtual ~RTPTransmitter()													{ }

	// The init function is there for initialization before any other threads
	// may access the object (e.g. initialization of mutexes)
	virtual int Init(bool threadsafe) = 0;
	virtual int Create(size_t maxpacksize,const RTPTransmissionParams *transparams) = 0;
	virtual void Destroy() = 0;

	// The user MUST delete the returned instance when it is no longer needed
	virtual RTPTransmissionInfo *GetTransmissionInfo() = 0;

	// If the buffersize ins't large enough, the transmitter must fill in the
	// required length in 'bufferlength'
	// If the size is ok, bufferlength is adjusted so that it indicates the
	// amount of bytes in the buffer that are part of the hostname.
	// The buffer is NOT null terminated!
	virtual int GetLocalHostName(u_int8_t *buffer,size_t *bufferlength) = 0;

	virtual bool ComesFromThisTransmitter(const RTPAddress *addr) = 0;
	virtual size_t GetHeaderOverhead() = 0;
	
	virtual int Poll() = 0;
	// If dataavailable is not NULL, it should be set to true if true if data was read
	// and to false otherwise
	virtual int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0) = 0;
	virtual int AbortWait() = 0;
	
	virtual int SendRTPData(const void *data,size_t len) = 0;	
	virtual int SendRTCPData(const void *data,size_t len) = 0;

	virtual void ResetPacketCount() = 0;
	virtual u_int32_t GetNumRTPPacketsSent() = 0;
	virtual u_int32_t GetNumRTCPPacketsSent() = 0;
	
	virtual int AddDestination(const RTPAddress &addr) = 0;
	virtual int DeleteDestination(const RTPAddress &addr) = 0;
	virtual void ClearDestinations() = 0;

	virtual bool SupportsMulticasting() = 0;
	virtual int JoinMulticastGroup(const RTPAddress &addr) = 0;
	virtual int LeaveMulticastGroup(const RTPAddress &addr) = 0;
	virtual void LeaveAllMulticastGroups() = 0;

	// Note: the list of addresses must be cleared when the receive mode is changed!
	virtual int SetReceiveMode(RTPTransmitter::ReceiveMode m) = 0;
	virtual int AddToIgnoreList(const RTPAddress &addr) = 0;
	virtual int DeleteFromIgnoreList(const RTPAddress &addr)= 0;
	virtual void ClearIgnoreList() = 0;
	virtual int AddToAcceptList(const RTPAddress &addr) = 0;
	virtual int DeleteFromAcceptList(const RTPAddress &addr) = 0;
	virtual void ClearAcceptList() = 0;
	virtual int SetMaximumPacketSize(size_t s) = 0;	
	
	virtual bool NewDataAvailable() = 0;
	virtual RTPRawPacket *GetNextPacket() = 0;
#ifdef RTPDEBUG
	virtual void Dump() = 0;
#endif // RTPDEBUG
};

// Abstract class from which actual transmission parameters should be derived

class RTPTransmissionParams
{
protected:
	RTPTransmissionParams(RTPTransmitter::TransmissionProtocol p)				{ protocol = p; }
public:
	virtual ~RTPTransmissionParams() { }
	RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const			{ return protocol; }
private:
	RTPTransmitter::TransmissionProtocol protocol;
};

class RTPTransmissionInfo
{
protected:
	RTPTransmissionInfo(RTPTransmitter::TransmissionProtocol p)				{ protocol = p; }
public:
	virtual ~RTPTransmissionInfo() { }
	RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const			{ return protocol; }
private:
	RTPTransmitter::TransmissionProtocol protocol;
};

#endif // RTPTRANSMITTER_H

