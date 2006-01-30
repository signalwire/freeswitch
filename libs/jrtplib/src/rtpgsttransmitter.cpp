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

#include "rtpgsttransmitter.h"

#ifdef RTP_SUPPORT_GST

#include "rtprawpacket.h"
#include "rtpipv4address.h"
#include "rtptimeutilities.h"
#include <stdio.h>

#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#ifdef RTP_HAVE_SYS_FILIO
#include <sys/filio.h>
#endif // RTP_HAVE_SYS_FILIO

#define RTPIOCTL								ioctl

#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

//GST_DEBUG_CATEGORY (gst_rtp_bin_debug);
//#define GST_CAT_DEFAULT (gst_rtp_bin_debug)

#define RTPGSTv4TRANS_RTPRECEIVEBUFFER							32768
#define RTPGSTv4TRANS_RTCPRECEIVEBUFFER							32768
#define RTPGSTv4TRANS_RTPTRANSMITBUFFER							32768
#define RTPGSTv4TRANS_RTCPTRANSMITBUFFER						32768
#define RTPGSTv4TRANS_MAXPACKSIZE							65535
#define RTPGSTv4TRANS_IFREQBUFSIZE							8192

//#define RTPGSTv4TRANS_IS_MCASTADDR(x)							(((x)&0xF0000000) == 0xE0000000)

/*#define RTPGSTv4TRANS_MCASTMEMBERSHIP(socket,type,mcastip,status)	{\
										struct ip_mreq mreq;\
										\
										mreq.imr_multiaddr.s_addr = htonl(mcastip);\
										mreq.imr_interface.s_addr = htonl(bindIP);\
										status = setsockopt(socket,IPPROTO_IP,type,(const char *)&mreq,sizeof(struct ip_mreq));\
									}*/
#ifndef RTP_SUPPORT_INLINETEMPLATEPARAM
	int RTPGSTv4Trans_GetHashIndex_IPv4Dest(const RTPIPv4Destination &d)				{ return d.GetIP_HBO()%RTPGSTv4TRANS_HASHSIZE; }
	int RTPGSTv4Trans_GetHashIndex_u_int32_t(const u_int32_t &k)					{ return k%RTPGSTv4TRANS_HASHSIZE; }
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

RTPGSTv4Transmitter::RTPGSTv4Transmitter()
{
	created = false;
	init = false;
}

RTPGSTv4Transmitter::~RTPGSTv4Transmitter()
{
	Destroy();
}

int RTPGSTv4Transmitter::Init(bool tsafe)
{
	if (init)
		return ERR_RTP_GSTV4TRANS_ALREADYINIT;
	
#ifdef RTP_SUPPORT_THREAD
	threadsafe = tsafe;
	if (threadsafe)
	{
		int status;
		
		status = mainmutex.Init();
		if (status < 0)
			return ERR_RTP_GSTV4TRANS_CANTINITMUTEX;
		status = waitmutex.Init();
		if (status < 0)
			return ERR_RTP_GSTV4TRANS_CANTINITMUTEX;
	}
#else
	if (tsafe)
		return ERR_RTP_NOTHREADSUPPORT;
#endif // RTP_SUPPORT_THREAD

	init = true;
	return 0;
}

int RTPGSTv4Transmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams)
{
	struct sockaddr_in addr;
	int status;

	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if (created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_ALREADYCREATED;
	}
	
	// Obtain transmission parameters
	
	if (transparams == 0)
		params = new RTPGSTv4TransmissionParams;
	else
	{
		if (transparams->GetTransmissionProtocol() != RTPTransmitter::IPv4GSTProto)
			return ERR_RTP_GSTV4TRANS_ILLEGALPARAMETERS;
		params = (RTPGSTv4TransmissionParams *)transparams;
	}

	// Check if portbase is even
	//if (params->GetPortbase()%2 != 0)
	//{
	//	MAINMUTEX_UNLOCK
	//	return ERR_RTP_GSTV4TRANS_PORTBASENOTEVEN;
	//}

	// Try to obtain local IP addresses

	localIPs = params->GetLocalIPList();
	if (localIPs.empty()) // User did not provide list of local IP addresses, calculate them
	{
		int status;
		
		if ((status = CreateLocalIPList()) < 0)
		{
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

//#ifdef RTP_SUPPORT_IPV4MULTICAST
//	if (SetMulticastTTL(params->GetMulticastTTL()))
//		supportsmulticasting = true;
//	else
//		supportsmulticasting = false;
//#else // no multicast support enabled
	supportsmulticasting = false;
//#endif // RTP_SUPPORT_IPV4MULTICAST

	if (maximumpacketsize > RTPGSTv4TRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_SPECIFIEDSIZETOOBIG;
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

void RTPGSTv4Transmitter::Destroy()
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
	
	destinations.Clear();
#ifdef RTP_SUPPORT_IPV4MULTICAST
//	multicastgroups.Clear();
#endif // RTP_SUPPORT_IPV4MULTICAST
	FlushPackets();
	ClearAcceptIgnoreInfo();
	localIPs.clear();
	created = false;
    delete params;
	
	MAINMUTEX_UNLOCK
}

RTPTransmissionInfo *RTPGSTv4Transmitter::GetTransmissionInfo()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = new RTPGSTv4TransmissionInfo(localIPs, 
            params->GetGstRTPSrc(), params->GetGstRTPSrc(), params);
	MAINMUTEX_UNLOCK
	return tinf;
}

int RTPGSTv4Transmitter::GetLocalHostName(u_int8_t *buffer,size_t *bufferlength)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}

	if (localhostname == 0)
	{
		if (localIPs.empty())
		{
			MAINMUTEX_UNLOCK
			return ERR_RTP_GSTV4TRANS_NOLOCALIPS;
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

bool RTPGSTv4Transmitter::ComesFromThisTransmitter(const RTPAddress *addr)
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
			if (addr2->GetPort() == params->GetPortbase()) // check for RTP port
				v = true;
			else if (addr2->GetPort() == (params->GetPortbase()+1)) // check for RTCP port
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

int RTPGSTv4Transmitter::Poll()
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;

	int status;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	status = FakePoll(); // poll RTP socket
    params->SetCurrentData(NULL);
	MAINMUTEX_UNLOCK
	return status;
}

int RTPGSTv4Transmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
/*	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	fd_set fdset;
	struct timeval tv;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_ALREADYWAITING;
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
		return ERR_RTP_GSTV4TRANS_ERRORINSELECT;
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
#ifdef WIN32
		char buf[1];
		
		recv(abortdesc[0],buf,1,0);
#else 
		unsigned char buf[1];

		read(abortdesc[0],buf,1);
#endif // WIN32
	}
	
	MAINMUTEX_UNLOCK
	WAITMUTEX_UNLOCK
	return 0;*/
	return ERR_RTP_GSTV4TRANS_WAITNOTIMPLEMENTED;
}

int RTPGSTv4Transmitter::AbortWait()
{
/*	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (!waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTWAITING;
	}

	AbortWaitInternal();
	
	MAINMUTEX_UNLOCK
	return 0;*/
	return 0;
}

int RTPGSTv4Transmitter::SendRTPData(const void *data,size_t len)	
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
    GstNetBuffer *out_buf;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_SPECIFIEDSIZETOOBIG;
	}


	destinations.GotoFirstElement();
    // send to each destination
	while (destinations.HasCurrentElement())
	{
        // create GstNetBuffer from data
        out_buf = gst_netbuffer_new ();
        GST_BUFFER_DATA (out_buf) = (guint8*)data;
        GST_BUFFER_SIZE (out_buf) = len;
        gst_netaddress_set_ip4_address (&out_buf->to, destinations.GetCurrentElement().GetIP_NBO(),
                destinations.GetCurrentElement().GetRTPPort_NBO());
        GST_DEBUG("Sending RTP packet to ip %d port %d", destinations.GetCurrentElement().GetIP_NBO(), 
                destinations.GetCurrentElement().GetRTPPort_NBO());

        // push data
        gst_pad_push( params->GetGstRTPSrc(), GST_BUFFER(out_buf) );
		destinations.GotoNextElement();
	}
	
	rtppackcount++;
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPGSTv4Transmitter::SendRTCPData(const void *data,size_t len)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;

    // if it's NULL we dont want to transmit RTCPs
    if (params->GetGstRTCPSrc() == NULL)
    {
        return 0;
    }
    
	MAINMUTEX_LOCK

    GstNetBuffer *out_buf;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_SPECIFIEDSIZETOOBIG;
	}

	destinations.GotoFirstElement();
    // send to each destination
	while (destinations.HasCurrentElement())
	{
        // create GstNetBuffer from data
        out_buf = gst_netbuffer_new ();
        GST_BUFFER_DATA (out_buf) = (guint8 *)data;
        GST_BUFFER_SIZE (out_buf) = len;
        gst_netaddress_set_ip4_address (&out_buf->to, destinations.GetCurrentElement().GetIP_NBO(),
                destinations.GetCurrentElement().GetRTCPPort_NBO());
        GST_DEBUG("Sending RTCP packet");

        // push data
        gst_pad_push( params->GetGstRTCPSrc(), GST_BUFFER(out_buf) );
		destinations.GotoNextElement();
	}
	
	rtcppackcount++;
	MAINMUTEX_UNLOCK
	return 0;
}

void RTPGSTv4Transmitter::ResetPacketCount()
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

u_int32_t RTPGSTv4Transmitter::GetNumRTPPacketsSent()
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

u_int32_t RTPGSTv4Transmitter::GetNumRTCPPacketsSent()
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
	
int RTPGSTv4Transmitter::AddDestination(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	
	RTPIPv4Address &address = (RTPIPv4Address &)addr;
	RTPIPv4Destination dest(address.GetIP(),address.GetPort());
	int status = destinations.AddElement(dest);

	MAINMUTEX_UNLOCK
	return status;
}

int RTPGSTv4Transmitter::DeleteDestination(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	
	RTPIPv4Address &address = (RTPIPv4Address &)addr;	
	RTPIPv4Destination dest(address.GetIP(),address.GetPort());
	int status = destinations.DeleteElement(dest);
	
	MAINMUTEX_UNLOCK
	return status;
}

void RTPGSTv4Transmitter::ClearDestinations()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created)
		destinations.Clear();
	MAINMUTEX_UNLOCK
}

bool RTPGSTv4Transmitter::SupportsMulticasting()
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

int RTPGSTv4Transmitter::JoinMulticastGroup(const RTPAddress &addr)
{
// hrrm wonder how will manage to get multicast info thru to the UDPSINK
/*	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	u_int32_t mcastIP = address.GetIP();
	
	if (!RTPGSTv4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTAMULTICASTADDRESS;
	}
	
	status = multicastgroups.AddElement(mcastIP);
	if (status >= 0)
	{
		RTPGSTv4TRANS_MCASTMEMBERSHIP(rtpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
		if (status != 0)
		{
			multicastgroups.DeleteElement(mcastIP);
			MAINMUTEX_UNLOCK
			return ERR_RTP_GSTV4TRANS_COULDNTJOINMULTICASTGROUP;
		}
		RTPGSTv4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
		if (status != 0)
		{
			RTPGSTv4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			multicastgroups.DeleteElement(mcastIP);
			MAINMUTEX_UNLOCK
			return ERR_RTP_GSTV4TRANS_COULDNTJOINMULTICASTGROUP;
		}
	}
	MAINMUTEX_UNLOCK	
	return status;*/
	return ERR_RTP_GSTV4TRANS_NOMULTICASTSUPPORT;
}

int RTPGSTv4Transmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
    /*
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	u_int32_t mcastIP = address.GetIP();
	
	if (!RTPGSTv4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTAMULTICASTADDRESS;
	}
	
	status = multicastgroups.DeleteElement(mcastIP);
	if (status >= 0)
	{	
		RTPGSTv4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
		RTPGSTv4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
		status = 0;
	}
	
	MAINMUTEX_UNLOCK
	return status;
    */
	return ERR_RTP_GSTV4TRANS_NOMULTICASTSUPPORT;
}

void RTPGSTv4Transmitter::LeaveAllMulticastGroups()
{
/*	if (!init)
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
			RTPGSTv4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			RTPGSTv4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			multicastgroups.GotoNextElement();
		}
		multicastgroups.Clear();
	}
	MAINMUTEX_UNLOCK*/
}

#else // no multicast support

int RTPGSTv4Transmitter::JoinMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_GSTV4TRANS_NOMULTICASTSUPPORT;
}

int RTPGSTv4Transmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_GSTV4TRANS_NOMULTICASTSUPPORT;
}

void RTPGSTv4Transmitter::LeaveAllMulticastGroups()
{
}

#endif // RTP_SUPPORT_IPV4MULTICAST

int RTPGSTv4Transmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (m != receivemode)
	{
		receivemode = m;
		acceptignoreinfo.Clear();
	}
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPGSTv4Transmitter::AddToIgnoreList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessAddAcceptIgnoreEntry(address.GetIP(),address.GetPort());
	
	MAINMUTEX_UNLOCK
	return status;
}

int RTPGSTv4Transmitter::DeleteFromIgnoreList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;	
	status = ProcessDeleteAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPGSTv4Transmitter::ClearIgnoreList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::IgnoreSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPGSTv4Transmitter::AddToAcceptList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessAddAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

int RTPGSTv4Transmitter::DeleteFromAcceptList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_DIFFERENTRECEIVEMODE;
	}
	
	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessDeleteAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPGSTv4Transmitter::ClearAcceptList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::AcceptSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPGSTv4Transmitter::SetMaximumPacketSize(size_t s)	
{
	if (!init)
		return ERR_RTP_GSTV4TRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_NOTCREATED;
	}
	if (s > RTPGSTv4TRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_GSTV4TRANS_SPECIFIEDSIZETOOBIG;
	}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPGSTv4Transmitter::NewDataAvailable()
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

RTPRawPacket *RTPGSTv4Transmitter::GetNextPacket()
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
bool RTPGSTv4Transmitter::SetMulticastTTL(u_int8_t ttl)
{
/*	int ttl2,status;

	ttl2 = (int)ttl;
	status = setsockopt(rtpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;
	status = setsockopt(rtcpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;
	return true;*/
}
#endif // RTP_SUPPORT_IPV4MULTICAST

void RTPGSTv4Transmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = rawpacketlist.begin() ; it != rawpacketlist.end() ; ++it)
		delete (*it);
	rawpacketlist.clear();
}

int RTPGSTv4Transmitter::FakePoll()
{
    int recvlen;
    GstNetBuffer *packetbuffer;

    RTPTime curtime = RTPTime::CurrentTime();

    packetbuffer = params->GetCurrentData();
    bool rtp = params->GetCurrentDataType();
    // lets make sure we got something
    if (packetbuffer == NULL )
    {
        //GST_DEBUG("No packetbuffer to poll");
        return 0;
    }
    // should be a netbuffer
    if (GST_IS_NETBUFFER (packetbuffer))
    {
        guint16 senderport;
        guint32 senderaddr;
        // let's get the address from the netbuffer
        gst_netaddress_get_ip4_address (&packetbuffer->from, &senderaddr, &senderport);
        RTPIPv4Address *addr = new RTPIPv4Address(ntohl(senderaddr), ntohs(senderport));
        if (addr == 0)
        {
            return ERR_RTP_OUTOFMEM;
        }
        GST_DEBUG("Got netbuffer, saving src info %d %d", addr->GetIP(), addr->GetPort());
        // ok we got the src addr, now this should be the actual packet
        recvlen = GST_BUFFER_SIZE(packetbuffer);
        if (GST_BUFFER_DATA(packetbuffer) && recvlen > 0)
        {
            GST_DEBUG("Got packet %d", recvlen);
            bool acceptdata;

            // got data, process it
            if (receivemode == RTPTransmitter::AcceptAll)
                acceptdata = true;
            else
                acceptdata = ShouldAcceptData(addr->GetIP(),addr->GetPort());

            if (acceptdata)
            {
                GST_DEBUG("Adding packet to queue");
                RTPRawPacket *pack;
                u_int8_t *datacopy;

                datacopy = new u_int8_t[recvlen];
                if (datacopy == 0)
                {
                    delete addr;
                    return ERR_RTP_OUTOFMEM;
                }
                memcpy(datacopy, GST_BUFFER_DATA(packetbuffer),recvlen);
                pack = new RTPRawPacket(datacopy,recvlen,addr,curtime,rtp);

                GST_DEBUG("lenght %d is RTP? %d", pack->GetDataLength(), pack->IsRTP());
                if (pack == 0)
                {
                    delete addr;
                    delete [] datacopy;
                    return ERR_RTP_OUTOFMEM;
                }
                rawpacketlist.push_back(pack);	
            }
        }
    } else {
        return ERR_RTP_GSTV4TRANS_NOTNETBUFFER;
    }
    return 0;
}

int RTPGSTv4Transmitter::ProcessAddAcceptIgnoreEntry(u_int32_t ip,u_int16_t port)
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

void RTPGSTv4Transmitter::ClearAcceptIgnoreInfo()
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
	
int RTPGSTv4Transmitter::ProcessDeleteAcceptIgnoreEntry(u_int32_t ip,u_int16_t port)
{
	acceptignoreinfo.GotoElement(ip);
	if (!acceptignoreinfo.HasCurrentElement())
		return ERR_RTP_GSTV4TRANS_NOSUCHENTRY;
	
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
					return ERR_RTP_GSTV4TRANS_NOSUCHENTRY;
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
			return ERR_RTP_GSTV4TRANS_NOSUCHENTRY;			
		}
	}
	return 0;
}

bool RTPGSTv4Transmitter::ShouldAcceptData(u_int32_t srcip,u_int16_t srcport)
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

#ifdef WIN32

int RTPGSTv4Transmitter::CreateAbortDescriptors()
{
    // no need for these no more
/*
	SOCKET listensock;
	int size;
	struct sockaddr_in addr;

	listensock = socket(PF_INET,SOCK_STREAM,0);
	if (listensock == RTPSOCKERR)
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	
	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(listensock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	if (getsockname(listensock,(struct sockaddr*)&addr,&size) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	unsigned short connectport = ntohs(addr.sin_port);

	abortdesc[0] = socket(PF_INET,SOCK_STREAM,0);
	if (abortdesc[0] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(abortdesc[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	if (listen(listensock,1) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(connectport);
	
	if (connect(abortdesc[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	abortdesc[1] = accept(listensock,(struct sockaddr *)&addr,&size);
	if (abortdesc[1] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS;
	}

	// okay, got the connection, close the listening socket

	RTPCLOSE(listensock);
	return 0;*/
}

void RTPGSTv4Transmitter::DestroyAbortDescriptors()
{
//	RTPCLOSE(abortdesc[0]);
//	RTPCLOSE(abortdesc[1]);
}

#else // in a non winsock environment we can use pipes

int RTPGSTv4Transmitter::CreateAbortDescriptors()
{
//	if (pipe(abortdesc) < 0)
//		return ERR_RTP_GSTV4TRANS_CANTCREATEPIPE;
//	return 0;
}

void RTPGSTv4Transmitter::DestroyAbortDescriptors()
{
//	close(abortdesc[0]);
//	close(abortdesc[1]);
}

#endif // WIN32

int RTPGSTv4Transmitter::CreateLocalIPList()
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

//#ifdef WIN32

bool RTPGSTv4Transmitter::GetLocalIPList_Interfaces()
{
	// REMINDER: got to find out how to do this
	return false;
}
/*
#else // use ioctl

bool RTPGSTv4Transmitter::GetLocalIPList_Interfaces()
{
	int status;
	char buffer[RTPGSTv4TRANS_IFREQBUFSIZE];
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr *sa;
	char *startptr,*endptr;
	int remlen;
	
	ifc.ifc_len = RTPGSTv4TRANS_IFREQBUFSIZE;
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

#endif // WIN32
*/
void RTPGSTv4Transmitter::GetLocalIPList_DNS()
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

void RTPGSTv4Transmitter::AbortWaitInternal()
{
/*#ifdef WIN32
	send(abortdesc[1],"*",1,0);
#else
	write(abortdesc[1],"*",1);
#endif // WIN32*/
}

void RTPGSTv4Transmitter::AddLoopbackAddress()
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
void RTPGSTv4Transmitter::Dump()
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
			
			std::cout << "Portbase:                       " << params->GetPortbase() << std::endl;
			std::cout << "Local IP addresses:" << std::endl;
			for (it = localIPs.begin() ; it != localIPs.end() ; it++)
			{
				ip = (*it);
				sprintf(str,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
				std::cout << "    " << str << std::endl;
			}
//			std::cout << "Multicast TTL:                  " << (int)multicastTTL << std::endl;
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
/*			std::cout << "List of multicast groups:       ";
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
				std::cout << "Empty" << std::endl;*/
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

#endif // RTP_SUPPORT_GST

