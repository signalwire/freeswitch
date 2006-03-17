#include "rtpsession.h"
#include "rtpsessionparams.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtptimeutilities.h"
#include "rtppacket.h"
#include <stdlib.h>
#include <iostream>

int main(void)
{
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // WIN32
		
	RTPSession session;
	
	RTPSessionParams sessionparams;
	sessionparams.SetOwnTimestampUnit(1.0/8000.0);
			
	RTPUDPv4TransmissionParams transparams;
	transparams.SetPortbase(8000);
			
	int status = session.Create(sessionparams,&transparams);
	if (status < 0)
	{
		std::cerr << RTPGetErrorString(status) << std::endl;
		exit(-1);
	}
	
	uint8_t localip[]={127,0,0,1};
	RTPIPv4Address addr(localip,9000);
	
	status = session.AddDestination(addr);
	if (status < 0)
	{
		std::cerr << RTPGetErrorString(status) << std::endl;
		exit(-1);
	}
	
	session.SetDefaultPayloadType(96);
	session.SetDefaultMark(false);
	session.SetDefaultTimestampIncrement(160);
	
	uint8_t silencebuffer[160];
	for (int i = 0 ; i < 160 ; i++)
		silencebuffer[i] = 128;

	RTPTime delay(0.020);
	RTPTime starttime = RTPTime::CurrentTime();
	
	bool done = false;
	while (!done)
	{
		status = session.SendPacket(silencebuffer,160);
		if (status < 0)
		{
			std::cerr << RTPGetErrorString(status) << std::endl;
			exit(-1);
		}
		
		session.BeginDataAccess();
		if (session.GotoFirstSource())
		{
			do
			{
				RTPPacket *packet = session.GetNextPacket();
				if (packet)
				{
					std::cout << "Got packet with " 
					          << "extended sequence number " 
					          << packet->GetExtendedSequenceNumber() 
					          << " from SSRC " << packet->GetSSRC() 
					          << std::endl;
					delete packet;
				}
			} while (session.GotoNextSource());
		}
		session.EndDataAccess();
			
		RTPTime::Wait(delay);
		
		RTPTime t = RTPTime::CurrentTime();
		t -= starttime;
		if (t > RTPTime(60.0))
			done = true;
	}
	
	delay = RTPTime(10.0);
	session.BYEDestroy(delay,"Time's up",9);
	
#ifdef WIN32
	WSACleanup();
#endif // WIN32
	return 0;
}

