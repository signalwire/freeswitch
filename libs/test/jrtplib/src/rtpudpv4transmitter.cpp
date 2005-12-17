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

#include "rtpudpv4transmitter.h"
#include "rtprawpacket.h"
#include "rtpipv4address.h"
#include "rtptimeutilities.h"
#include <stdio.h>
#if (defined(WIN32) || defined(_WIN32_WCE))
	#define RTPSOCKERR								INVALID_SOCKET
	#define RTPCLOSE(x)								closesocket(x)
	#define RTPSOCKLENTYPE								int
	#define RTPIOCTL								ioctlsocket
#else // not Win32
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/ioctl.h>
	#include <net/if.h>
	#include <string.h>
	#include <netdb.h>
	#include <unistd.h>

	#ifdef RTP_HAVE_SYS_FILIO
		#include <sys/filio.h>
	#endif // RTP_HAVE_SYS_FILIO
	#ifdef RTP_HAVE_SYS_SOCKIO
		#include <sys/sockio.h>
	#endif // RTP_HAVE_SYS_SOCKIO
	#ifdef RTP_SUPPORT_IFADDRS
		#include <ifaddrs.h>
	#endif // RTP_SUPPORT_IFADDRS

	#define RTPSOCKERR								-1
	#define RTPCLOSE(x)								close(x)

	#ifdef RTP_SOCKLENTYPE_UINT
		#define RTPSOCKLENTYPE							unsigned int
	#else
		#define RTPSOCKLENTYPE							int
	#endif // RTP_SOCKLENTYPE_UINT

	#define RTPIOCTL								ioctl
#endif // WIN32
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

#ifndef _WIN32_WCE
	#define RTPUDPV4TRANS_RTPRECEIVEBUFFER							32768
	#define RTPUDPV4TRANS_RTCPRECEIVEBUFFER							32768
	#define RTPUDPV4TRANS_RTPTRANSMITBUFFER							32768
	#define RTPUDPV4TRANS_RTCPTRANSMITBUFFER						32768
	#define RTPUDPV4TRANS_MAXPACKSIZE							65535
	#define RTPUDPV4TRANS_IFREQBUFSIZE							8192
#else
	#define RTPUDPV4TRANS_RTPRECEIVEBUFFER							2048
	#define RTPUDPV4TRANS_RTCPRECEIVEBUFFER							2048
	#define RTPUDPV4TRANS_RTPTRANSMITBUFFER							2048
	#define RTPUDPV4TRANS_RTCPTRANSMITBUFFER						2048
	#define RTPUDPV4TRANS_MAXPACKSIZE							2048
	#define RTPUDPV4TRANS_IFREQBUFSIZE							2048
#endif // _WIN32_WCE

#define RTPUDPV4TRANS_IS_MCASTADDR(x)							(((x)&0xF0000000) == 0xE0000000)

#define RTPUDPV4TRANS_MCASTMEMBERSHIP(socket,type,mcastip,status)	{\
										struct ip_mreq mreq;\
										\
										mreq.imr_multiaddr.s_addr = htonl(mcastip);\
										mreq.imr_interface.s_addr = htonl(bindIP);\
										status = setsockopt(socket,IPPROTO_IP,type,(const char *)&mreq,sizeof(struct ip_mreq));\
									}
#ifndef RTP_SUPPORT_INLINETEMPLATEPARAM
	int RTPUDPv4Trans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d)				{ return d.GetIP_HBO()%RTPUDPV4TRANS_HASHSIZE; }
	int RTPUDPv4Trans_GetHashIndex_u_int32_t(const u_int32_t &k)					{ return k%RTPUDPV4TRANS_HASHSIZE; }
#endif // !RTP_SUPPORT_INLINETEMPLATEPARAM
	
#ifdef RTP_SUPPORT_THREAD
	#define MAINMUTEX_LOCK 		{ if (threadsafe) mainmutex.Lock(); }
	#define MAINMUTEX_UNLOCK	{ if (threadsafe) mainmutex.Unlock(); }
	#define WAITMUTEX_LOCK		{ if (threadsafe) waitmutex.Lock(); }
	#define WAITMUTEX_UNLOCK	{ if (threadsafe) waitmutex.Unlock(); }
#else
	#define MAINMUTEX_LOCK
	#define MAINMUTEX_UNLOCK
	#define WAITMUTEX_LOCK
	#define WAITMUTEX_UNLOCK
#endif // RTP_SUPPORT_THREAD

RTPUDPv4Transmitter::RTPUDPv4Transmitter()
{
	created = false;
	init = false;
#if (defined(WIN32) || defined(_WIN32_WCE))
	timeinit.Dummy();
#endif // WIN32 || _WIN32_WCE
}

RTPUDPv4Transmitter::~RTPUDPv4Transmitter()
{
	Destroy();
}

int RTPUDPv4Transmitter::Init(bool tsafe)
{
	if (init)
		return ERR_RTP_UDPV4TRANS_ALREADYINIT;
	
#ifdef RTP_SUPPORT_THREAD
	threadsafe = tsafe;
	if (threadsafe)
	{
		int status;
		
		status = mainmutex.Init();
		if (status < 0)
			return ERR_RTP_UDPV4TRANS_CANTINITMUTEX;
		status = waitmutex.Init();
		if (status < 0)
			return ERR_RTP_UDPV4TRANS_CANTINITMUTEX;
	}
#else
	if (tsafe)
		return ERR_RTP_NOTHREADSUPPORT;
#endif // RTP_SUPPORT_THREAD

	init = true;
	return 0;
}

int RTPUDPv4Transmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams)
{
	const RTPUDPv4TransmissionParams *params,defaultparams;
	struct sockaddr_in addr;
	RTPSOCKLENTYPE size;
	int status;

	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if (created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_ALREADYCREATED;
	}
	
	// Obtain transmission parameters
	
	if (transparams == 0)
		params = &defaultparams;
	else
	{
		if (transparams->GetTransmissionProtocol() != RTPTransmitter::IPv4UDPProto)
		{
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_ILLEGALPARAMETERS;
		}
		params = (const RTPUDPv4TransmissionParams *)transparams;
	}

	// Check if portbase is even
	if (params->GetPortbase()%2 != 0)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_PORTBASENOTEVEN;
	}

	// create sockets
	
	rtpsock = socket(PF_INET,SOCK_DGRAM,0);
	if (rtpsock == RTPSOCKERR)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
	}
	rtcpsock = socket(PF_INET,SOCK_DGRAM,0);
	if (rtcpsock == RTPSOCKERR)
	{
		RTPCLOSE(rtpsock);
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
	}

	// set socket buffer sizes
	
	size = RTPUDPV4TRANS_RTPRECEIVEBUFFER;
	if (setsockopt(rtpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTSETRTPRECEIVEBUF;
	}
	size = RTPUDPV4TRANS_RTPTRANSMITBUFFER;
	if (setsockopt(rtpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTSETRTPTRANSMITBUF;
	}
	size = RTPUDPV4TRANS_RTCPRECEIVEBUFFER;
	if (setsockopt(rtcpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTSETRTCPRECEIVEBUF;
	}
	size = RTPUDPV4TRANS_RTCPTRANSMITBUFFER;
	if (setsockopt(rtcpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTSETRTCPTRANSMITBUF;
	}
	
	// bind sockets

	bindIP = params->GetBindIP();
	
	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(params->GetPortbase());
	addr.sin_addr.s_addr = htonl(bindIP);
	if (bind(rtpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTBINDRTPSOCKET;
	}
	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(params->GetPortbase()+1);
	addr.sin_addr.s_addr = htonl(bindIP);
	if (bind(rtcpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_CANTBINDRTCPSOCKET;
	}

	// Try to obtain local IP addresses

	localIPs = params->GetLocalIPList();
	if (localIPs.empty()) // User did not provide list of local IP addresses, calculate them
	{
		int status;
		
		if ((status = CreateLocalIPList()) < 0)
		{
			RTPCLOSE(rtpsock);
			RTPCLOSE(rtcpsock);
			MAINMUTEX_UNLOCK
			return status;
		}
#ifdef RTPDEBUG
		std::cout << "Found these local IP addresses:" << std::endl;
		
		std::list<u_int32_t>::const_iterator it;

		for (it = localIPs.begin() ; it != localIPs.end() ; it++)
		{
			RTPIPv4Address a(*it);

			std::cout << a.GetAddressString() << std::endl;
		}
#endif // RTPDEBUG
	}

#ifdef RTP_SUPPORT_IPV4MULTICAST
	if (SetMulticastTTL(params->GetMulticastTTL()))
		supportsmulticasting = true;
	else
		supportsmulticasting = false;
#else // no multicast support enabled
	supportsmulticasting = false;
#endif // RTP_SUPPORT_IPV4MULTICAST

	if ((status = CreateAbortDescriptors()) < 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return status;
	}
	
	if (maximumpacketsize > RTPUDPV4TRANS_MAXPACKSIZE)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		DestroyAbortDescriptors();
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}
	
	maxpacksize = maximumpacketsize;
	portbase = params->GetPortbase();
	multicastTTL = params->GetMulticastTTL();
	receivemode = RTPTransmitter::AcceptAll;

	localhostname = 0;
	localhostnamelength = 0;

	rtppackcount = 0;
	rtcppackcount = 0;
	
	waitingfordata = false;
	created = true;
	MAINMUTEX_UNLOCK
	return 0;
}

void RTPUDPv4Transmitter::Destroy()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK;
		return;
	}

	if (localhostname)
	{
		delete [] localhostname;
		localhostname = 0;
		localhostnamelength = 0;
	}
	
	RTPCLOSE(rtpsock);
	RTPCLOSE(rtcpsock);
	destinations.Clear();
#ifdef RTP_SUPPORT_IPV4MULTICAST
	multicastgroups.Clear();
#endif // RTP_SUPPORT_IPV4MULTICAST
	FlushPackets();
	ClearAcceptIgnoreInfo();
	localIPs.clear();
	created = false;
	
	if (waitingfordata)
	{
		AbortWaitInternal();
		DestroyAbortDescriptors();
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
		WAITMUTEX_UNLOCK
	}
	else
		DestroyAbortDescriptors();

	MAINMUTEX_UNLOCK
}

RTPTransmissionInfo *RTPUDPv4Transmitter::GetTransmissionInfo()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = new RTPUDPv4TransmissionInfo(localIPs,rtpsock,rtcpsock);
	MAINMUTEX_UNLOCK
	return tinf;
}

int RTPUDPv4Transmitter::GetLocalHostName(u_int8_t *buffer,size_t *bufferlength)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}

	if (localhostname == 0)
	{
		if (localIPs.empty())
		{
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_NOLOCALIPS;
		}
		
		std::list<u_int32_t>::const_iterator it;
		std::list<std::string> hostnames;
	
		for (it = localIPs.begin() ; it != localIPs.end() ; it++)
		{
			struct hostent *he;
			u_int8_t addr[4];
			u_int32_t ip = (*it);
	
			addr[0] = (u_int8_t)((ip>>24)&0xFF);
			addr[1] = (u_int8_t)((ip>>16)&0xFF);
			addr[2] = (u_int8_t)((ip>>8)&0xFF);
			addr[3] = (u_int8_t)(ip&0xFF);
			he = gethostbyaddr((char *)addr,4,AF_INET);
			if (he != 0)
			{
				std::string hname = std::string(he->h_name);
				hostnames.push_back(hname);
			}
		}
	
		bool found  = false;
		
		if (!hostnames.empty())	// try to select the most appropriate hostname
		{
			std::list<std::string>::const_iterator it;
			
			for (it = hostnames.begin() ; !found && it != hostnames.end() ; it++)
			{
				if ((*it).find('.') != std::string::npos)
				{
					found = true;
					localhostnamelength = (*it).length();
					localhostname = new u_int8_t [localhostnamelength+1];
					if (localhostname == 0)
					{
						MAINMUTEX_UNLOCK
						return ERR_RTP_OUTOFMEM;
					}
					memcpy(localhostname,(*it).c_str(),localhostnamelength);
					localhostname[localhostnamelength] = 0;
				}
			}
		}
	
		if (!found) // use an IP address
		{
			u_int32_t ip;
			int len;
			char str[256];
			
			it = localIPs.begin();
			ip = (*it);
			
			sprintf(str,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
			len = strlen(str);
	
			localhostnamelength = len;
			localhostname = new u_int8_t [localhostnamelength + 1];
			if (localhostname == 0)
			{
				MAINMUTEX_UNLOCK
				return ERR_RTP_OUTOFMEM;
			}
			memcpy(localhostname,str,localhostnamelength);
			localhostname[localhostnamelength] = 0;
		}
	}
	
	if ((*bufferlength) < localhostnamelength)
	{
		*bufferlength = localhostnamelength; // tell the application the required size of the buffer
		MAINMUTEX_UNLOCK
		return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
	}

	memcpy(buffer,localhostname,localhostnamelength);
	*bufferlength = localhostnamelength;
	
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPUDPv4Transmitter::ComesFromThisTransmitter(const RTPAddress *addr)
{
	if (!init)
		return false;

	if (addr == 0)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (created && addr->GetAddressType() == RTPAddress::IPv4Address)
	{	
		const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
		bool found = false;
		std::list<u_int32_t>::const_iterator it;
	
		it = localIPs.begin();
		while (!found && it != localIPs.end())
		{
			if (addr2->GetIP() == *it)
				found = true;
			else
				++it;
		}
	
		if (!found)
			v = false;
		else
		{
			if (addr2->GetPort() == portbase) // check for RTP port
				v = true;
			else if (addr2->GetPort() == (portbase+1)) // check for RTCP port
				v = true;
			else 
				v = false;
		}
	}
	else
		v = false;

	MAINMUTEX_UNLOCK
	return v;
}

int RTPUDPv4Transmitter::Poll()
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	int status;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	status = PollSocket(true); // poll RTP socket
	if (status >= 0)
		status = PollSocket(false); // poll RTCP socket
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	fd_set fdset;
	struct timeval tv;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_ALREADYWAITING;
	}
	
	FD_ZERO(&fdset);
	FD_SET(rtpsock,&fdset);
	FD_SET(rtcpsock,&fdset);
	FD_SET(abortdesc[0],&fdset);
	tv.tv_sec = delay.GetSeconds();
	tv.tv_usec = delay.GetMicroSeconds();
	
	waitingfordata = true;
	
	WAITMUTEX_LOCK
	MAINMUTEX_UNLOCK

	if (select(FD_SETSIZE,&fdset,0,0,&tv) < 0)
	{
		MAINMUTEX_LOCK
		waitingfordata = false;
		MAINMUTEX_UNLOCK
		WAITMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_ERRORINSELECT;
	}
	
	MAINMUTEX_LOCK
	waitingfordata = false;
	if (!created) // destroy called
	{
		MAINMUTEX_UNLOCK;
		WAITMUTEX_UNLOCK
		return 0;
	}
		
	// if aborted, read from abort buffer
	if (FD_ISSET(abortdesc[0],&fdset))
	{
#if (defined(WIN32) || defined(_WIN32_WCE))
		char buf[1];
		
		recv(abortdesc[0],buf,1,0);
#else 
		unsigned char buf[1];

		read(abortdesc[0],buf,1);
#endif // WIN32
	}

	if (dataavailable != 0)
	{
		if (FD_ISSET(rtpsock,&fdset) || FD_ISSET(rtcpsock,&fdset))
			*dataavailable = true;
		else
			*dataavailable = false;
	}	
	
	MAINMUTEX_UNLOCK
	WAITMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::AbortWait()
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (!waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTWAITING;
	}

	AbortWaitInternal();
	
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::SendRTPData(const void *data,size_t len)	
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	struct sockaddr_in saddr;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}
	
	memset(&saddr,0,sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	destinations.GotoFirstElement();
	while (destinations.HasCurrentElement())
	{
		saddr.sin_port = destinations.GetCurrentElement().GetRTPPort_NBO();
		saddr.sin_addr.s_addr = destinations.GetCurrentElement().GetIP_NBO();
		sendto(rtpsock,(const char *)data,len,0,(struct sockaddr *)&saddr,sizeof(struct sockaddr_in));
		destinations.GotoNextElement();
	}
	
	rtppackcount++;
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::SendRTCPData(const void *data,size_t len)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	struct sockaddr_in saddr;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}
	
	memset(&saddr,0,sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	destinations.GotoFirstElement();
	while (destinations.HasCurrentElement())
	{
		saddr.sin_port = destinations.GetCurrentElement().GetRTCPPort_NBO();
		saddr.sin_addr.s_addr = destinations.GetCurrentElement().GetIP_NBO();
		sendto(rtcpsock,(const char *)data,len,0,(struct sockaddr *)&saddr,sizeof(struct sockaddr_in));
		destinations.GotoNextElement();
	}
	
	rtcppackcount++;
	MAINMUTEX_UNLOCK
	return 0;
}

void RTPUDPv4Transmitter::ResetPacketCount()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (created)
	{
		rtppackcount = 0;
		rtcppackcount = 0;	
	}
	MAINMUTEX_UNLOCK	
}

u_int32_t RTPUDPv4Transmitter::GetNumRTPPacketsSent()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK
	
	u_int32_t num;

	if (!created)
		num = 0;
	else
		num = rtppackcount;

	MAINMUTEX_UNLOCK

	return num;
}

u_int32_t RTPUDPv4Transmitter::GetNumRTCPPacketsSent()
{
	if (!init)
		return 0;
	
	MAINMUTEX_LOCK
	
	u_int32_t num;

	if (!created)
		num = 0;
	else
		num = rtcppackcount;

	MAINMUTEX_UNLOCK

	return num;
}
	
int RTPUDPv4Transmitter::AddDestination(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	
	RTPIPv4Address &address = (RTPIPv4Address &)addr;
	RTPIPv4Destination dest(address.GetIP(),address.GetPort());
	int status = destinations.AddElement(dest);

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteDestination(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	
	RTPIPv4Address &address = (RTPIPv4Address &)addr;	
	RTPIPv4Destination dest(address.GetIP(),address.GetPort());
	int status = destinations.DeleteElement(dest);
	
	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearDestinations()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created)
		destinations.Clear();
	MAINMUTEX_UNLOCK
}

bool RTPUDPv4Transmitter::SupportsMulticasting()
{
	if (!init)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (!created)
		v = false;
	else
		v = supportsmulticasting;

	MAINMUTEX_UNLOCK
	return v;
}

#ifdef RTP_SUPPORT_IPV4MULTICAST

int RTPUDPv4Transmitter::JoinMulticastGroup(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	u_int32_t mcastIP = address.GetIP();
	
	if (!RTPUDPV4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTAMULTICASTADDRESS;
	}
	
	status = multicastgroups.AddElement(mcastIP);
	if (status >= 0)
	{
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
		if (status != 0)
		{
			multicastgroups.DeleteElement(mcastIP);
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP;
		}
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
		if (status != 0)
		{
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			multicastgroups.DeleteElement(mcastIP);
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP;
		}
	}
	MAINMUTEX_UNLOCK	
	return status;
}

int RTPUDPv4Transmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	u_int32_t mcastIP = address.GetIP();
	
	if (!RTPUDPV4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTAMULTICASTADDRESS;
	}
	
	status = multicastgroups.DeleteElement(mcastIP);
	if (status >= 0)
	{	
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
		status = 0;
	}
	
	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::LeaveAllMulticastGroups()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created)
	{
		multicastgroups.GotoFirstElement();
		while (multicastgroups.HasCurrentElement())
		{
			u_int32_t mcastIP;
			int status = 0;

			mcastIP = multicastgroups.GetCurrentElement();
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			multicastgroups.GotoNextElement();
		}
		multicastgroups.Clear();
	}
	MAINMUTEX_UNLOCK
}

#else // no multicast support

int RTPUDPv4Transmitter::JoinMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_UDPV4TRANS_NOMULTICASTSUPPORT;
}

int RTPUDPv4Transmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_UDPV4TRANS_NOMULTICASTSUPPORT;
}

void RTPUDPv4Transmitter::LeaveAllMulticastGroups()
{
}

#endif // RTP_SUPPORT_IPV4MULTICAST

int RTPUDPv4Transmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (m != receivemode)
	{
		receivemode = m;
		acceptignoreinfo.Clear();
	}
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::AddToIgnoreList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessAddAcceptIgnoreEntry(address.GetIP(),address.GetPort());
	
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteFromIgnoreList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;	
	status = ProcessDeleteAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearIgnoreList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::IgnoreSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv4Transmitter::AddToAcceptList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessAddAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteFromAcceptList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessDeleteAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearAcceptList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::AcceptSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv4Transmitter::SetMaximumPacketSize(size_t s)	
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (s > RTPUDPV4TRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPUDPv4Transmitter::NewDataAvailable()
{
	if (!init)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (!created)
		v = false;
	else
	{
		if (rawpacketlist.empty())
			v = false;
		else
			v = true;
	}
	
	MAINMUTEX_UNLOCK
	return v;
}

RTPRawPacket *RTPUDPv4Transmitter::GetNextPacket()
{
	if (!init)
		return 0;
	
	MAINMUTEX_LOCK
	
	RTPRawPacket *p;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return 0;
	}
	if (rawpacketlist.empty())
	{
		MAINMUTEX_UNLOCK
		return 0;
	}

	p = *(rawpacketlist.begin());
	rawpacketlist.pop_front();

	MAINMUTEX_UNLOCK
	return p;
}

// Here the private functions start...

#ifdef RTP_SUPPORT_IPV4MULTICAST
bool RTPUDPv4Transmitter::SetMulticastTTL(u_int8_t ttl)
{
	int ttl2,status;

	ttl2 = (int)ttl;
	status = setsockopt(rtpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;
	status = setsockopt(rtcpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;
	return true;
}
#endif // RTP_SUPPORT_IPV4MULTICAST

void RTPUDPv4Transmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = rawpacketlist.begin() ; it != rawpacketlist.end() ; ++it)
		delete (*it);
	rawpacketlist.clear();
}

int RTPUDPv4Transmitter::PollSocket(bool rtp)
{
	RTPSOCKLENTYPE fromlen;
	int recvlen;
	char packetbuffer[RTPUDPV4TRANS_MAXPACKSIZE];
#if (defined(WIN32) || defined(_WIN32_WCE))
	SOCKET sock;
	unsigned long len;
#else 
	size_t len;
	int sock;
#endif // WIN32
	struct sockaddr_in srcaddr;
	
	if (rtp)
		sock = rtpsock;
	else
		sock = rtcpsock;
	
	len = 0;
	RTPIOCTL(sock,FIONREAD,&len);
	if (len <= 0)
		return 0;

	while (len > 0)
	{
		RTPTime curtime = RTPTime::CurrentTime();
		fromlen = sizeof(struct sockaddr_in);
		recvlen = recvfrom(sock,packetbuffer,(int)len,0,(struct sockaddr *)&srcaddr,&fromlen);
		if (recvlen > 0)
		{
			bool acceptdata;

			// got data, process it
			if (receivemode == RTPTransmitter::AcceptAll)
				acceptdata = true;
			else
				acceptdata = ShouldAcceptData(ntohl(srcaddr.sin_addr.s_addr),htons(srcaddr.sin_port));
			
			if (acceptdata)
			{
				RTPRawPacket *pack;
				RTPIPv4Address *addr;
				u_int8_t *datacopy;

				addr = new RTPIPv4Address(ntohl(srcaddr.sin_addr.s_addr),ntohs(srcaddr.sin_port));
				if (addr == 0)
					return ERR_RTP_OUTOFMEM;
				datacopy = new u_int8_t[recvlen];
				if (datacopy == 0)
				{
					delete addr;
					return ERR_RTP_OUTOFMEM;
				}
				memcpy(datacopy,packetbuffer,recvlen);
				pack = new RTPRawPacket(datacopy,recvlen,addr,curtime,rtp);

				if (pack == 0)
				{
					delete addr;
					delete [] datacopy;
					return ERR_RTP_OUTOFMEM;
				}
				rawpacketlist.push_back(pack);	
			}
		}
		len = 0;
		RTPIOCTL(sock,FIONREAD,&len);
	}
	return 0;
}

int RTPUDPv4Transmitter::ProcessAddAcceptIgnoreEntry(u_int32_t ip,u_int16_t port)
{
	acceptignoreinfo.GotoElement(ip);
	if (acceptignoreinfo.HasCurrentElement()) // An entry for this IP address already exists
	{
		PortInfo *portinf = acceptignoreinfo.GetCurrentElement();
		
		if (port == 0) // select all ports
		{
			portinf->all = true;
			portinf->portlist.clear();
		}
		else if (!portinf->all)
		{
			std::list<u_int16_t>::const_iterator it,begin,end;

			begin = portinf->portlist.begin();
			end = portinf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == port) // already in list
					return 0;
			}
			portinf->portlist.push_front(port);
		}
	}
	else // got to create an entry for this IP address
	{
		PortInfo *portinf;
		int status;
		
		portinf = new PortInfo();
		if (port == 0) // select all ports
			portinf->all = true;
		else
			portinf->portlist.push_front(port);
		
		status = acceptignoreinfo.AddElement(ip,portinf);
		if (status < 0)
		{
			delete portinf;
			return status;
		}
	}

	return 0;
}

void RTPUDPv4Transmitter::ClearAcceptIgnoreInfo()
{
	acceptignoreinfo.GotoFirstElement();
	while (acceptignoreinfo.HasCurrentElement())
	{
		PortInfo *inf;

		inf = acceptignoreinfo.GetCurrentElement();
		delete inf;
		acceptignoreinfo.GotoNextElement();
	}
	acceptignoreinfo.Clear();
}
	
int RTPUDPv4Transmitter::ProcessDeleteAcceptIgnoreEntry(u_int32_t ip,u_int16_t port)
{
	acceptignoreinfo.GotoElement(ip);
	if (!acceptignoreinfo.HasCurrentElement())
		return ERR_RTP_UDPV4TRANS_NOSUCHENTRY;
	
	PortInfo *inf;

	inf = acceptignoreinfo.GetCurrentElement();
	if (port == 0) // delete all entries
	{
		inf->all = false;
		inf->portlist.clear();
	}
	else // a specific port was selected
	{
		if (inf->all) // currently, all ports are selected. Add the one to remove to the list
		{
			// we have to check if the list doesn't contain the port already
			std::list<u_int16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == port) // already in list: this means we already deleted the entry
					return ERR_RTP_UDPV4TRANS_NOSUCHENTRY;
			}
			inf->portlist.push_front(port);
		}
		else // check if we can find the port in the list
		{
			std::list<u_int16_t>::iterator it,begin,end;
			
			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; ++it)
			{
				if (*it == port) // found it!
				{
					inf->portlist.erase(it);
					return 0;
				}
			}
			// didn't find it
			return ERR_RTP_UDPV4TRANS_NOSUCHENTRY;			
		}
	}
	return 0;
}

bool RTPUDPv4Transmitter::ShouldAcceptData(u_int32_t srcip,u_int16_t srcport)
{
	if (receivemode == RTPTransmitter::AcceptSome)
	{
		PortInfo *inf;

		acceptignoreinfo.GotoElement(srcip);
		if (!acceptignoreinfo.HasCurrentElement())
			return false;
		
		inf = acceptignoreinfo.GetCurrentElement();
		if (!inf->all) // only accept the ones in the list
		{
			std::list<u_int16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return true;
			}
			return false;
		}
		else // accept all, except the ones in the list
		{
			std::list<u_int16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return false;
			}
			return true;
		}
	}
	else // IgnoreSome
	{
		PortInfo *inf;

		acceptignoreinfo.GotoElement(srcip);
		if (!acceptignoreinfo.HasCurrentElement())
			return true;
		
		inf = acceptignoreinfo.GetCurrentElement();
		if (!inf->all) // ignore the ports in the list
		{
			std::list<u_int16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return false;
			}
			return true;
		}
		else // ignore all, except the ones in the list
		{
			std::list<u_int16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return true;
			}
			return false;
		}
	}
	return true;
}

#if (defined(WIN32) || defined(_WIN32_WCE))

int RTPUDPv4Transmitter::CreateAbortDescriptors()
{
	SOCKET listensock;
	int size;
	struct sockaddr_in addr;

	listensock = socket(PF_INET,SOCK_STREAM,0);
	if (listensock == RTPSOCKERR)
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	
	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(listensock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	if (getsockname(listensock,(struct sockaddr*)&addr,&size) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	unsigned short connectport = ntohs(addr.sin_port);

	abortdesc[0] = socket(PF_INET,SOCK_STREAM,0);
	if (abortdesc[0] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(abortdesc[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	if (listen(listensock,1) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(connectport);
	
	if (connect(abortdesc[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	abortdesc[1] = accept(listensock,(struct sockaddr *)&addr,&size);
	if (abortdesc[1] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	// okay, got the connection, close the listening socket

	RTPCLOSE(listensock);
	return 0;
}

void RTPUDPv4Transmitter::DestroyAbortDescriptors()
{
	RTPCLOSE(abortdesc[0]);
	RTPCLOSE(abortdesc[1]);
}

#else // in a non winsock environment we can use pipes

int RTPUDPv4Transmitter::CreateAbortDescriptors()
{
	if (pipe(abortdesc) < 0)
		return ERR_RTP_UDPV4TRANS_CANTCREATEPIPE;
	return 0;
}

void RTPUDPv4Transmitter::DestroyAbortDescriptors()
{
	close(abortdesc[0]);
	close(abortdesc[1]);
}

#endif // WIN32

int RTPUDPv4Transmitter::CreateLocalIPList()
{
	 // first try to obtain the list from the network interface info

	if (!GetLocalIPList_Interfaces())
	{
		// If this fails, we'll have to depend on DNS info
		GetLocalIPList_DNS();
	}
	AddLoopbackAddress();
	return 0;
}

#if (defined(WIN32) || defined(_WIN32_WCE))

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces()
{
	unsigned char buffer[RTPUDPV4TRANS_IFREQBUFSIZE];
	DWORD outputsize;
	DWORD numaddresses,i;
	SOCKET_ADDRESS_LIST *addrlist;

	if (WSAIoctl(rtpsock,SIO_ADDRESS_LIST_QUERY,NULL,0,&buffer,RTPUDPV4TRANS_IFREQBUFSIZE,&outputsize,NULL,NULL))
		return false;
	
	addrlist = (SOCKET_ADDRESS_LIST *)buffer;
	numaddresses = addrlist->iAddressCount;
	for (i = 0 ; i < numaddresses ; i++)
	{
		SOCKET_ADDRESS *sockaddr = &(addrlist->Address[i]);
		if (sockaddr->iSockaddrLength == sizeof(struct sockaddr_in)) // IPv4 address
		{
			struct sockaddr_in *addr = (struct sockaddr_in *)sockaddr->lpSockaddr;

			localIPs.push_back(ntohl(addr->sin_addr.s_addr));
		}
	}

	if (localIPs.empty())
		return false;

	return true;
}

#else // use either getifaddrs or ioctl

#ifdef RTP_SUPPORT_IFADDRS

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces()
{
	struct ifaddrs *addrs,*tmp;
	
	getifaddrs(&addrs);
	tmp = addrs;
	
	while (tmp != 0)
	{
		if (tmp->ifa_addr->sa_family == AF_INET)
		{
			struct sockaddr_in *inaddr = (struct sockaddr_in *)tmp->ifa_addr;
			localIPs.push_back(ntohl(inaddr->sin_addr.s_addr));
		}
		tmp = tmp->ifa_next;
	}
	
	freeifaddrs(addrs);
	
	if (localIPs.empty())
		return false;
	return true;
}

#else // user ioctl

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces()
{
	int status;
	char buffer[RTPUDPV4TRANS_IFREQBUFSIZE];
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr *sa;
	char *startptr,*endptr;
	int remlen;
	
	ifc.ifc_len = RTPUDPV4TRANS_IFREQBUFSIZE;
	ifc.ifc_buf = buffer;
	status = ioctl(rtpsock,SIOCGIFCONF,&ifc);
	if (status < 0)
		return false;
	
	startptr = (char *)ifc.ifc_req;
	endptr = startptr + ifc.ifc_len;
	remlen = ifc.ifc_len;
	while((startptr < endptr) && remlen >= (int)sizeof(struct ifreq))
	{
		ifr = (struct ifreq *)startptr;
		sa = &(ifr->ifr_addr);
#ifdef RTP_HAVE_SOCKADDR_LEN
		if (sa->sa_len <= sizeof(struct sockaddr))
		{
			if (sa->sa_len == sizeof(struct sockaddr_in) && sa->sa_family == PF_INET)
			{
				u_int32_t ip;
				struct sockaddr_in *addr = (struct sockaddr_in *)sa;
				
				ip = ntohl(addr->sin_addr.s_addr);
				localIPs.push_back(ip);
			}
			remlen -= sizeof(struct ifreq);
			startptr += sizeof(struct ifreq);
		}
		else
		{
			int l = sa->sa_len-sizeof(struct sockaddr)+sizeof(struct ifreq);
			
			remlen -= l;
			startptr += l;
		}
#else // don't have sa_len in struct sockaddr
		if (sa->sa_family == PF_INET)
		{
			u_int32_t ip;
			struct sockaddr_in *addr = (struct sockaddr_in *)sa;
		
			ip = ntohl(addr->sin_addr.s_addr);
			localIPs.push_back(ip);
		}
		remlen -= sizeof(struct ifreq);
		startptr += sizeof(struct ifreq);
	
#endif // RTP_HAVE_SOCKADDR_LEN
	}

	if (localIPs.empty())
		return false;
	return true;
}

#endif // RTP_SUPPORT_IFADDRS

#endif // WIN32

void RTPUDPv4Transmitter::GetLocalIPList_DNS()
{
	struct hostent *he;
	char name[1024];
	u_int32_t ip;
	bool done;
	int i,j;

	gethostname(name,1023);
	name[1023] = 0;
	he = gethostbyname(name);
	if (he == 0)
		return;
	
	ip = 0;
	i = 0;
	done = false;
	while (!done)
	{
		if (he->h_addr_list[i] == NULL)
			done = true;
		else
		{
			ip = 0;
			for (j = 0 ; j < 4 ; j++)
				ip |= ((u_int32_t)((unsigned char)he->h_addr_list[i][j])<<((3-j)*8));
			localIPs.push_back(ip);
			i++;
		}
	}
}

void RTPUDPv4Transmitter::AbortWaitInternal()
{
#if (defined(WIN32) || defined(_WIN32_WCE))
	send(abortdesc[1],"*",1,0);
#else
	write(abortdesc[1],"*",1);
#endif // WIN32
}

void RTPUDPv4Transmitter::AddLoopbackAddress()
{
	u_int32_t loopbackaddr = (((u_int32_t)127)<<24)|((u_int32_t)1);
	std::list<u_int32_t>::const_iterator it;
	bool found = false;
	
	for (it = localIPs.begin() ; !found && it != localIPs.end() ; it++)
	{
		if (*it == loopbackaddr)
			found = true;
	}

	if (!found)
		localIPs.push_back(loopbackaddr);
}

#ifdef RTPDEBUG
void RTPUDPv4Transmitter::Dump()
{
	if (!init)
		std::cout << "Not initialized" << std::endl;
	else
	{
		MAINMUTEX_LOCK
	
		if (!created)
			std::cout << "Not created" << std::endl;
		else
		{
			char str[1024];
			u_int32_t ip;
			std::list<u_int32_t>::const_iterator it;
			
			std::cout << "Portbase:                       " << portbase << std::endl;
			std::cout << "RTP socket descriptor:          " << rtpsock << std::endl;
			std::cout << "RTCP socket descriptor:         " << rtcpsock << std::endl;
			ip = bindIP;
			sprintf(str,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
			std::cout << "Bind IP address:                " << str << std::endl;
			std::cout << "Local IP addresses:" << std::endl;
			for (it = localIPs.begin() ; it != localIPs.end() ; it++)
			{
				ip = (*it);
				sprintf(str,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
				std::cout << "    " << str << std::endl;
			}
			std::cout << "Multicast TTL:                  " << (int)multicastTTL << std::endl;
			std::cout << "Receive mode:                   ";
			switch (receivemode)
			{
			case RTPTransmitter::AcceptAll:
				std::cout << "Accept all";
				break;
			case RTPTransmitter::AcceptSome:
				std::cout << "Accept some";
				break;
			case RTPTransmitter::IgnoreSome:
				std::cout << "Ignore some";
			}
			std::cout << std::endl;
			if (receivemode != RTPTransmitter::AcceptAll)
			{
				acceptignoreinfo.GotoFirstElement();
				while(acceptignoreinfo.HasCurrentElement())
				{
					ip = acceptignoreinfo.GetCurrentKey();
					sprintf(str,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
					PortInfo *pinfo = acceptignoreinfo.GetCurrentElement();
					std::cout << "    " << str << ": ";
					if (pinfo->all)
					{
						std::cout << "All ports";
						if (!pinfo->portlist.empty())
							std::cout << ", except ";
					}
					
					std::list<u_int16_t>::const_iterator it;
					
					for (it = pinfo->portlist.begin() ; it != pinfo->portlist.end() ; )
					{
						std::cout << (*it);
						it++;
						if (it != pinfo->portlist.end())
							std::cout << ", ";
					}
					std::cout << std::endl;
				}
			}
			
			std::cout << "Local host name:                ";
			if (localhostname == 0)
				std::cout << "Not set";
			else
				std::cout << localhostname;
			std::cout << std::endl;

			std::cout << "List of destinations:           ";
			destinations.GotoFirstElement();
			if (destinations.HasCurrentElement())
			{
				std::cout << std::endl;
				do
				{
					std::cout << "    " << destinations.GetCurrentElement().GetDestinationString() << std::endl;
					destinations.GotoNextElement();
				} while (destinations.HasCurrentElement());
			}
			else
				std::cout << "Empty" << std::endl;
		
			std::cout << "Supports multicasting:          " << ((supportsmulticasting)?"Yes":"No") << std::endl;
#ifdef RTP_SUPPORT_IPV4MULTICAST
			std::cout << "List of multicast groups:       ";
			multicastgroups.GotoFirstElement();
			if (multicastgroups.HasCurrentElement())
			{
				std::cout << std::endl;
				do
				{
					ip = multicastgroups.GetCurrentElement();
					sprintf(str,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
					std::cout << "    " << str << std::endl;
					multicastgroups.GotoNextElement();
				} while (multicastgroups.HasCurrentElement());
			}
			else
				std::cout << "Empty" << std::endl;
#endif // RTP_SUPPORT_IPV4MULTICAST
			
			std::cout << "Number of raw packets in queue: " << rawpacketlist.size() << std::endl;
			std::cout << "Maximum allowed packet size:    " << maxpacksize << std::endl;
			std::cout << "RTP packet count:               " << rtppackcount << std::endl;
			std::cout << "RTCP packet count:              " << rtcppackcount << std::endl;
		}
		
		MAINMUTEX_UNLOCK
	}
}
#endif // RTPDEBUG

