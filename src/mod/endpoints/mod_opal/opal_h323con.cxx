
#include "opal_h323.h"
#include "opal_h323con.h"


FSExternalRTPChannel::FSExternalRTPChannel(H323Connection & connection,        ///<  Connection to endpoint for channel
											const H323Capability & capability,  ///<  Capability channel is using
											Directions direction,               ///<  Direction of channel
											unsigned sessionID)              ///<  Session ID for channel)

	: H323_ExternalRTPChannel(connection, capability, direction, sessionID)
{
	PTRACE(3,"Creating External RTP Channel");
	
	PIPSocket::Address addr("10.0.0.1");
    WORD port = 41004;
    SetExternalAddress(H323TransportAddress(addr, port), H323TransportAddress(addr, port+1));

      // get the payload code
	PTRACE(3, "Channel Capability " << capability.GetFormatName());
    OpalMediaFormat format(capability.GetFormatName());
    payloadCode = format.GetPayloadType();

	Start();
}

BOOL FSExternalRTPChannel::Start()
{
	 if (!H323_ExternalRTPChannel::Start())
        return FALSE;
	
	 PIPSocket::Address addr;
     WORD port;
     GetRemoteAddress(addr, port);
	PTRACE(3, "External RTP Channel Started ::Start");
	
	return TRUE;
}


FSH323Connection::FSH323Connection(OpalCall &call,FSH323EndPoint &endpoint,
						 const PString & token,const PString &alias,
						 const H323TransportAddress & address,unsigned options)
		
	:H323Connection(call,endpoint,token,alias,address,options)
{


}

FSH323Connection::~FSH323Connection()
{

}

OpalMediaFormatList FSH323Connection::GetMediaFormats()
{
	OpalMediaFormatList mediaformats;
	
	mediaformats+=OpalPCM16;
	mediaformats+=OpalG711uLaw;
	mediaformats+=OpalG711ALaw; 
	mediaformats+=OpalG711_ULAW_64K;
	mediaformats+=OpalG711_ALAW_64K;
	mediaformats+=OpalGSM0610;
	mediaformats+=OpalPCM16_16KHZ;
	mediaformats+=OPAL_G729;
	mediaformats+=OPAL_G729A;
	mediaformats+=OPAL_G729B;
	mediaformats+=OPAL_G729AB;
	PTRACE(3, "RTP Channel Callback !");
	
	return mediaformats;
}

/*
H323Channel* FSH323Connection::CreateRealTimeLogicalChannel(const H323Capability & capability,
															H323Channel::Directions dir,
															unsigned sessionID,
															const H245_H2250LogicalChannelParameters * param,
															RTP_QOS * rtpqos)
{
	
	PTRACE(3, "RTP Channel Callback !");
	//h323 external rtp can be set from here
	return new FSExternalRTPChannel(*this, capability, dir, sessionID);	
}
*/
