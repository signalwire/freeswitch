
#ifndef __OPAL_H323_CONNECTION__
#define __OPAL_H323_CONNECTION__

#include "opal_h323.h"
#include "fsep.h"
#include "fsmanager.h"

class FSH323EndPoint;


class FSExternalRTPChannel : public H323_ExternalRTPChannel
{
	PCLASSINFO(FSExternalRTPChannel, H323_ExternalRTPChannel);
	public:
		FSExternalRTPChannel(H323Connection & connection,        ///<  Connection to endpoint for channel
							const H323Capability & capability,  ///<  Capability channel is using
							Directions direction,               ///<  Direction of channel
							unsigned sessionID );                 ///<  Session ID for channel

		virtual BOOL Start();
	
	protected:
    	BYTE payloadCode;
};


class FSH323Connection : public H323Connection
{
	PCLASSINFO(FSH323Connection, H323Connection);
	public:
		FSH323Connection(OpalCall &call, FSH323EndPoint &endpoint,
						 const PString & token,const PString &alias,
						 const H323TransportAddress & address,unsigned options = 0);
		
		//External H323 RTP
		/*
		H323Channel* CreateRealTimeLogicalChannel(const H323Capability & capability,
                                     H323Channel::Directions dir,
                                     unsigned sessionID,
                                     const H245_H2250LogicalChannelParameters * param,
									 RTP_QOS * rtpqos);
		*/
		
		virtual OpalMediaFormatList GetMediaFormats();
	
		~FSH323Connection();

};

#endif // __FREESWITCH_H323_CONNECTION__
