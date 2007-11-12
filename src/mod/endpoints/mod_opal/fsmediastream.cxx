
#include "fsmediastream.h"

FSMediaStream::FSMediaStream(FSConnection & connection, const OpalMediaFormat &mediaFormat,
							 BOOL isSource, 
							 RTP_Session & rtpSession,
							 unsigned minAudioJitterDelay,
							 unsigned maxAudioJitterDelay)

	:OpalRTPMediaStream(connection, mediaFormat, isSource, rtpSession, minAudioJitterDelay, maxAudioJitterDelay), 
	fsconnection((FSConnection&) connection)
{
	
	if (isSource){
		PTRACE(3, "FSMediaStream: Created SOURCE Media Stream");
		//channel = fsconnection.writechannel;
	}else{
		//channel = fsconnection.readchannel;
		PTRACE(3, "FSMediaStream: Created SINK Media Stream");
	}
	
}

BOOL FSMediaStream::IsSynchronous() const
{
	return FALSE; //rtp mediastream
}


FSMediaStream::~FSMediaStream()
{
	
}



FSUDPMediaStream::FSUDPMediaStream(OpalConnection & connection, const OpalMediaFormat &mediaFormat, 
								   unsigned sessionID,
								   BOOL isSource,
								   OpalTransportUDP &transport)

	:OpalUDPMediaStream(connection, mediaFormat, sessionID, isSource, transport)
{
	if (isSource){
		PTRACE(3, "FSMediaStream: Created SOURCE Media Stream");
	}else{
		PTRACE(3, "FSMediaStream: Created SINK Media Stream");
	}
	
}

FSUDPMediaStream::~FSUDPMediaStream()
{

}
