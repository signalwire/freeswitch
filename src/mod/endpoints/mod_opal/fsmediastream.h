
#ifndef __FREESWITCH_OPAL_MEDIASTREAM__
#define __FREESWITCH_OPAL_MEDIASTREAM__


#include <ptlib.h>
#include <opal/buildopts.h>
#include <opal/connection.h>
#include <opal/mediastrm.h>
#include <switch.h>
#include "fscon.h"

class FSConnection;

class FSMediaStream : public OpalRTPMediaStream
{
	PCLASSINFO(FSMediaStream, OpalRTPMediaStream);
	public:
			FSMediaStream(FSConnection & connection, const OpalMediaFormat &mediaFormat,
						  BOOL isSource, 
						  RTP_Session & rtpSession,
						  unsigned minAudioJitterDelay,
						  unsigned maxAudioJitterDelay);
	
	
			~FSMediaStream();
			//virtual BOOL ReadData(BYTE* data,PINDEX size,PINDEX& length);
			//virtual BOOL WriteData(const BYTE* data,PINDEX length, PINDEX& written);			
			virtual BOOL IsSynchronous() const;
	
		FSConnection &fsconnection;
	protected:
		PAdaptiveDelay channelDelay;
		BOOL	isSource;
	 	PQueueChannel *channel;
};

class FSUDPMediaStream : public OpalUDPMediaStream
{
	PCLASSINFO(FSUDPMediaStream, OpalUDPMediaStream);
	public:
			FSUDPMediaStream(OpalConnection & connection, const OpalMediaFormat &mediaFormat,
						  unsigned sessionID,
						  BOOL isSource,
						  OpalTransportUDP &transport);
	
			~FSUDPMediaStream();
			
};

#endif// __FREESWITCH_OPAL_CONNECTION__
